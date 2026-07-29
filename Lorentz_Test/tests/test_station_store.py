from pathlib import Path

import pytest

from lorentz_test.models.station import StationConfig
from lorentz_test.station_store import StationConfigStore


def test_station_round_trip(tmp_path: Path) -> None:
    store = StationConfigStore(tmp_path / "station.json")
    expected = StationConfig(station_name="Station A", lcp_port="COM5")
    store.save(expected)
    assert store.load() == expected


def test_duplicate_ports_are_rejected() -> None:
    with pytest.raises(ValueError):
        StationConfig(lcp_port="COM5", s1_endpoint="com5")
