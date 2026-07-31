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


def test_same_hmi_x2x_endpoint_is_allowed_only_in_shared_mode() -> None:
    shared = StationConfig(
        hmi_endpoint="COM7",
        x2x_endpoint="com7",
        shared_hmi_x2x_adapter=True,
    )
    assert shared.hmi_endpoint == "COM7"

    with pytest.raises(ValueError):
        StationConfig(
            hmi_endpoint="COM7",
            x2x_endpoint="com7",
            shared_hmi_x2x_adapter=False,
        )


def test_x2x_still_conflicts_with_other_fixture_ports_in_shared_mode() -> None:
    with pytest.raises(ValueError):
        StationConfig(
            s1_endpoint="COM6",
            x2x_endpoint="com6",
            shared_hmi_x2x_adapter=True,
        )
