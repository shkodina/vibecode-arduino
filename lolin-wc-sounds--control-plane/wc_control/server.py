from __future__ import annotations

from pathlib import Path
from typing import Any

import httpx
import uvicorn
from fastapi import FastAPI, File, HTTPException, Query, Request, UploadFile
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles

from wc_control.names import module_filename
from wc_control.scanner import check_known_modules, scan_modules
from wc_control.settings import Settings, default_config_path, load_settings, save_discovered_modules
from wc_control.wavutil import analyze_wav, convert_to_module_wav

ROOT = Path(__file__).resolve().parent.parent
STATIC = ROOT / "static"

MODULE_TIMEOUT = httpx.Timeout(15.0)
UPLOAD_TIMEOUT = httpx.Timeout(connect=8.0, read=180.0, write=180.0, pool=8.0)


def _require_host(settings: Settings, host: str) -> None:
    if not settings.host_allowed(host):
        raise HTTPException(status_code=403, detail="адрес вне пула scan.subnet")


async def _forward(
    host: str,
    port: int,
    method: str,
    path: str,
    *,
    params: dict[str, Any] | None = None,
    content: bytes | None = None,
    json_body: Any = None,
    headers: dict[str, str] | None = None,
    files: dict | None = None,
    timeout: httpx.Timeout | float | None = None,
) -> Response:
    url = f"http://{host}:{port}{path}"
    try:
        async with httpx.AsyncClient(timeout=timeout or MODULE_TIMEOUT) as client:
            response = await client.request(
                method,
                url,
                params=params,
                content=content,
                json=json_body,
                headers=headers,
                files=files,
            )
    except httpx.TimeoutException as exc:
        raise HTTPException(
            status_code=502,
            detail="модуль не ответил вовремя при загрузке. ESP8266 пишет SD медленно и может уйти в watchdog reboot — подождите ~10 с и проверьте, появился ли файл",
        ) from exc
    except httpx.RequestError as exc:
        raise HTTPException(status_code=502, detail=f"модуль не отвечает: {exc}") from exc
    media = response.headers.get("content-type", "text/plain; charset=utf-8")
    return Response(content=response.content, status_code=response.status_code, media_type=media)


def create_app(settings: Settings | None = None) -> FastAPI:
    settings = settings or load_settings(default_config_path())
    known: list[tuple[str, int]] = list(settings.known_modules)
    app = FastAPI(title="WC Sounds Control Plane", docs_url=None, redoc_url=None)

    @app.get("/")
    async def index() -> FileResponse:
        return FileResponse(STATIC / "index.html")

    @app.get("/api/meta")
    async def meta() -> dict[str, Any]:
        return {
            "subnet": settings.scan_subnet,
            "scan_port": settings.scan_port,
            "host_count": len(settings.hosts),
            "known_count": len(known),
            "listen": f"http://localhost:{settings.listen_port}",
        }

    @app.get("/api/known")
    async def known_modules() -> dict[str, Any]:
        rows = await check_known_modules(
            known,
            timeout_seconds=settings.timeout_seconds,
            concurrency=settings.concurrency,
        )
        return {"modules": rows}

    @app.post("/api/scan")
    async def scan() -> dict[str, Any]:
        modules = await scan_modules(
            settings.hosts,
            port=settings.scan_port,
            timeout_seconds=settings.timeout_seconds,
            concurrency=settings.concurrency,
        )
        for item in modules:
            item["online"] = True
        if settings.path is not None:
            save_discovered_modules(settings.path, modules)
        known[:] = [(item["host"], int(item["port"])) for item in modules]
        return {"modules": modules, "scanned": len(settings.hosts), "saved": len(known)}

    @app.get("/api/modules/{host}/status")
    async def module_status(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "GET", "/api/status")

    @app.post("/api/modules/{host}/play")
    async def module_play(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "POST", "/api/play")

    @app.post("/api/modules/{host}/stop")
    async def module_stop(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "POST", "/api/stop")

    @app.post("/api/modules/{host}/volume")
    async def module_volume(host: str, value: int = Query(...)) -> Response:
        _require_host(settings, host)
        return await _forward(
            host, settings.scan_port, "POST", "/api/volume", params={"value": value}
        )

    @app.get("/api/modules/{host}/config")
    async def module_get_config(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "GET", "/api/config")

    @app.post("/api/modules/{host}/config")
    async def module_post_config(host: str, request: Request) -> Response:
        _require_host(settings, host)
        body = await request.body()
        return await _forward(
            host,
            settings.scan_port,
            "POST",
            "/api/config",
            content=body,
            headers={"Content-Type": "application/json"},
        )

    @app.post("/api/modules/{host}/reload")
    async def module_reload(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "POST", "/api/reload")

    @app.post("/api/modules/{host}/reboot")
    async def module_reboot(host: str) -> Response:
        _require_host(settings, host)
        return await _forward(host, settings.scan_port, "POST", "/api/reboot")

    @app.get("/api/modules/{host}/files")
    async def module_files(host: str, path: str = "/") -> Response:
        _require_host(settings, host)
        return await _forward(
            host, settings.scan_port, "GET", "/api/files", params={"path": path}
        )

    @app.post("/api/modules/{host}/mkdir")
    async def module_mkdir(host: str, path: str = Query(...)) -> Response:
        _require_host(settings, host)
        return await _forward(
            host, settings.scan_port, "POST", "/api/mkdir", params={"path": path}
        )

    @app.delete("/api/modules/{host}/delete")
    async def module_delete(host: str, path: str = Query(...)) -> Response:
        _require_host(settings, host)
        return await _forward(
            host, settings.scan_port, "DELETE", "/api/delete", params={"path": path}
        )

    @app.post("/api/modules/{host}/upload")
    async def module_upload(
        host: str,
        path: str = Query(...),
        convert: bool = Query(False),
        file: UploadFile = File(...),
    ) -> Response:
        _require_host(settings, host)
        data = await file.read()
        if convert:
            info = analyze_wav(data)
            if info.needs_conversion:
                try:
                    data = convert_to_module_wav(data)
                except RuntimeError as exc:
                    raise HTTPException(status_code=400, detail=str(exc)) from exc
        filename = Path(path).name or "sound.wav"
        return await _forward(
            host,
            settings.scan_port,
            "POST",
            "/api/upload",
            params={"path": path},
            files={"file": (filename, data, "audio/wav")},
            timeout=UPLOAD_TIMEOUT,
        )

    @app.post("/api/analyze")
    async def analyze(file: UploadFile = File(...)) -> dict[str, Any]:
        data = await file.read()
        payload = analyze_wav(data).as_dict()
        payload["name"] = file.filename
        payload["module_filename"] = module_filename(file.filename)
        payload["size"] = len(data)
        return payload

    @app.post("/api/convert")
    async def convert(file: UploadFile = File(...)) -> Response:
        data = await file.read()
        try:
            converted = convert_to_module_wav(data)
        except RuntimeError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        name = module_filename(file.filename)
        return Response(
            content=converted,
            media_type="audio/wav",
            headers={"Content-Disposition": f'attachment; filename="{name}"'},
        )

    if STATIC.is_dir():
        app.mount("/static", StaticFiles(directory=STATIC), name="static")
    return app


def main() -> None:
    settings = load_settings(default_config_path())
    url = f"http://localhost:{settings.listen_port}"
    banner = (
        "\n"
        "  WC Sounds Control Plane\n"
        f"  {url}\n"
        "  (кликните ссылку или скопируйте в браузер)\n"
    )
    print(banner, flush=True)
    uvicorn.run(
        create_app(settings),
        host=settings.listen_host,
        port=settings.listen_port,
        log_level="info",
    )


if __name__ == "__main__":
    main()
