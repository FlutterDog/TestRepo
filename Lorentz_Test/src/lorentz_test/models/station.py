"""Station configuration model."""

from __future__ import annotations

from enum import Enum
from ipaddress import IPv4Address

from pydantic import BaseModel, ConfigDict, Field, model_validator


class PostTestAction(str, Enum):
    RESTORE_ORIGINAL = "restore_original"
    CLEAR_TO_RUNTIME_DEFAULTS = "clear_to_runtime_defaults"
    KEEP_TEST_CONFIGURATION = "keep_test_configuration"


class StationConfig(BaseModel):
    """Persistent settings of one Lorentz test station."""

    model_config = ConfigDict(extra="forbid", str_strip_whitespace=True)

    station_name: str = Field(default="Lorentz Test Station", min_length=1, max_length=80)
    lcp_port: str | None = None
    s1_endpoint: str | None = None
    s2_endpoint: str | None = None
    s3_endpoint: str | None = None
    s4_endpoint: str | None = None
    hmi_endpoint: str | None = None
    x2x_endpoint: str | None = None
    eth1_ip: IPv4Address = IPv4Address("192.168.1.1")
    eth2_ip: IPv4Address = IPv4Address("192.168.1.2")
    eth1_source_ip: IPv4Address | None = None
    eth2_source_ip: IPv4Address | None = None
    eth1_test_enabled: bool = True
    eth2_test_enabled: bool = True
    shared_hmi_x2x_adapter: bool = True
    post_test_action: PostTestAction = PostTestAction.CLEAR_TO_RUNTIME_DEFAULTS
    expected_firmware_version: str = Field(default="1.02.0", min_length=1, max_length=32)
    serial_baudrate: int = Field(default=115200, ge=300, le=1_000_000)
    serial_timeout_seconds: float = Field(default=2.0, gt=0.05, le=30.0)

    @model_validator(mode="after")
    def validate_endpoint_ownership(self) -> "StationConfig":
        endpoints: list[tuple[str, str]] = []
        for name in ("lcp_port", "s1_endpoint", "s2_endpoint", "s3_endpoint", "s4_endpoint"):
            value = getattr(self, name)
            if value:
                endpoints.append((name, value.casefold()))

        hmi = self.hmi_endpoint.casefold() if self.hmi_endpoint else None
        x2x = self.x2x_endpoint.casefold() if self.x2x_endpoint else None
        shared_same_endpoint = bool(
            self.shared_hmi_x2x_adapter
            and hmi is not None
            and x2x is not None
            and hmi == x2x
        )

        if hmi:
            endpoints.append(("hmi_endpoint", hmi))
        if x2x and not shared_same_endpoint:
            endpoints.append(("x2x_endpoint", x2x))

        owners: dict[str, str] = {}
        for name, endpoint in endpoints:
            previous = owners.get(endpoint)
            if previous is not None:
                raise ValueError(f"endpoint {endpoint!r} is assigned to both {previous} and {name}")
            owners[endpoint] = name
        return self
