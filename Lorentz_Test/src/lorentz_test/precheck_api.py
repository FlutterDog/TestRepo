"""FastAPI routes for station readiness checks."""

from __future__ import annotations

from fastapi import APIRouter, HTTPException, status

from lorentz_test.models.precheck import LcpPrecheckResult
from lorentz_test.models.tests import LcpHelloRequest
from lorentz_test.services.lcp_precheck import run_lcp_precheck
from lorentz_test.services.test_run_controller import HardwareBusyError, hardware_operation_guard
from lorentz_test.station_store import station_store

router = APIRouter()


@router.post("/api/tests/lcp/precheck", response_model=LcpPrecheckResult)
def test_lcp_precheck(request: LcpHelloRequest) -> LcpPrecheckResult:
    try:
        station = station_store.load()
        with hardware_operation_guard.hold("station-precheck"):
            return run_lcp_precheck(request, station)
    except HardwareBusyError as exc:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=f"Аппаратные ресурсы заняты операцией: {exc.owner}",
        ) from exc
    except RuntimeError as exc:
        raise HTTPException(status_code=500, detail=str(exc)) from exc
