"""Atomic station configuration persistence."""

from __future__ import annotations

import json
import os
from pathlib import Path

from lorentz_test.models.station import StationConfig
from lorentz_test.paths import data_dir


class StationConfigStore:
    def __init__(self, path: Path | None = None) -> None:
        self.path = path or data_dir() / "station.json"

    def load(self) -> StationConfig:
        if not self.path.exists():
            return StationConfig()
        try:
            return StationConfig.model_validate_json(self.path.read_text(encoding="utf-8"))
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            raise RuntimeError(f"cannot read station configuration: {exc}") from exc

    def save(self, config: StationConfig) -> StationConfig:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(self.path.suffix + ".tmp")
        payload = config.model_dump_json(indent=2)
        try:
            temporary.write_text(payload + "\n", encoding="utf-8")
            os.replace(temporary, self.path)
        except OSError as exc:
            temporary.unlink(missing_ok=True)
            raise RuntimeError(f"cannot save station configuration: {exc}") from exc
        return config


station_store = StationConfigStore()
