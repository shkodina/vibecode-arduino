from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import ipaddress
import re
import yaml


class SettingsError(Exception):
    pass


@dataclass(frozen=True)
class Settings:
    listen_host: str
    listen_port: int
    scan_subnet: str
    scan_port: int
    timeout_seconds: float
    concurrency: int
    hosts: tuple[str, ...]
    known_modules: tuple[tuple[str, int], ...] = ()
    path: Path | None = None

    def host_allowed(self, host: str) -> bool:
        try:
            ip = ipaddress.ip_address(host)
        except ValueError:
            return False
        if str(ip) in self.hosts:
            return True
        return any(str(ip) == known for known, _port in self.known_modules)


def _hosts_from_subnet(subnet: str) -> tuple[str, ...]:
    network = ipaddress.ip_network(subnet, strict=False)
    return tuple(str(ip) for ip in network.hosts())


def load_settings(path: Path | str) -> Settings:
    cfg_path = Path(path)
    if not cfg_path.is_file():
        raise SettingsError(f"нет файла конфига: {cfg_path}")
    raw = yaml.safe_load(cfg_path.read_text(encoding="utf-8")) or {}
    listen = raw.get("listen") or {}
    scan = raw.get("scan") or {}
    subnet = str(scan.get("subnet") or "").strip()
    if not subnet:
        raise SettingsError("в конфиге нужен scan.subnet")
    extra = scan.get("hosts") or []
    hosts = list(_hosts_from_subnet(subnet))
    for item in extra:
        ip = str(item).strip()
        if ip and ip not in hosts:
            hosts.append(ip)
    scan_port = int(scan.get("port") or 80)
    known: list[tuple[str, int]] = []
    for item in raw.get("modules") or []:
        if isinstance(item, str) and item.strip():
            known.append((item.strip(), scan_port))
        elif isinstance(item, dict) and item.get("host"):
            known.append((str(item["host"]).strip(), int(item.get("port") or scan_port)))
    return Settings(
        listen_host=str(listen.get("host") or "127.0.0.1"),
        listen_port=int(listen.get("port") or 8000),
        scan_subnet=subnet,
        scan_port=scan_port,
        timeout_seconds=float(scan.get("timeout_seconds") or 0.4),
        concurrency=int(scan.get("concurrency") or 64),
        hosts=tuple(hosts),
        known_modules=tuple(known),
        path=cfg_path,
    )


def default_config_path() -> Path:
    return Path(__file__).resolve().parent.parent / "config.yaml"


def save_discovered_modules(path: Path | str, modules: list[dict]) -> None:
    cfg_path = Path(path)
    original = cfg_path.read_text(encoding="utf-8") if cfg_path.is_file() else ""
    entries = []
    seen: set[str] = set()
    for item in modules:
        host = str(item.get("host") or "").strip()
        if not host or host in seen:
            continue
        seen.add(host)
        entries.append({"host": host, "port": int(item.get("port") or 80)})
    if entries:
        lines = ["modules:"]
        for item in entries:
            lines.append(f"  - host: {item['host']}")
            lines.append(f"    port: {item['port']}")
        block = "\n".join(lines) + "\n"
    else:
        block = "modules: []\n"
    pattern = re.compile(r"(?m)^modules:.*(?:\n[ \t].+)*\n?")
    if pattern.search(original):
        text = pattern.sub(block.rstrip() + "\n", original, count=1)
    else:
        text = original.rstrip() + "\n\n" + block
        if not text.endswith("\n"):
            text += "\n"
    cfg_path.write_text(text, encoding="utf-8")

