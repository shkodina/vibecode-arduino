from __future__ import annotations

import asyncio
from typing import Any

import httpx


WC_STATUS_KEYS = frozenset(
    {"playing", "sd_ok", "volume", "ip", "motion", "file", "directory"}
)


def looks_like_wc_sounds(status: Any) -> bool:
    if not isinstance(status, dict):
        return False
    return WC_STATUS_KEYS.issubset(status.keys())


async def _probe(
    client: httpx.AsyncClient,
    host: str,
    port: int,
    timeout: float,
) -> dict[str, Any] | None:
    url = f"http://{host}:{port}/api/status"
    try:
        response = await client.get(url, timeout=timeout)
        if response.status_code != 200:
            return None
        data = response.json()
    except Exception:
        return None
    if not looks_like_wc_sounds(data):
        return None
    return {"host": host, "port": port, "status": data}


async def scan_modules(
    hosts: list[str] | tuple[str, ...],
    port: int = 80,
    timeout_seconds: float = 0.4,
    concurrency: int = 64,
    client: httpx.AsyncClient | None = None,
) -> list[dict[str, Any]]:
    semaphore = asyncio.Semaphore(max(1, concurrency))

    async def limited(http: httpx.AsyncClient, host: str) -> dict[str, Any] | None:
        async with semaphore:
            return await _probe(http, host, port, timeout_seconds)

    own_client = client is None
    http = client or httpx.AsyncClient()
    try:
        results = await asyncio.gather(*(limited(http, host) for host in hosts))
    finally:
        if own_client:
            await http.aclose()
    found = [item for item in results if item]
    found.sort(key=lambda item: tuple(int(part) for part in item["host"].split(".")))
    return found


async def check_known_modules(
    modules: list[tuple[str, int]] | tuple[tuple[str, int], ...],
    timeout_seconds: float = 0.4,
    concurrency: int = 32,
    client: httpx.AsyncClient | None = None,
) -> list[dict[str, Any]]:
    semaphore = asyncio.Semaphore(max(1, concurrency))

    async def one(http: httpx.AsyncClient, host: str, port: int) -> dict[str, Any]:
        async with semaphore:
            found = await _probe(http, host, port, timeout_seconds)
        if found:
            return {**found, "online": True}
        return {"host": host, "port": port, "status": None, "online": False}

    own_client = client is None
    http = client or httpx.AsyncClient()
    try:
        results = await asyncio.gather(*(one(http, host, port) for host, port in modules))
    finally:
        if own_client:
            await http.aclose()
    return list(results)
