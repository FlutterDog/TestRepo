from lorentz_test.protocols.modbus_rtu_slave import (
    append_crc,
    build_response,
    frame_crc_valid,
    modbus_crc16,
)


def test_crc_matches_standard_modbus_vector() -> None:
    request = bytes.fromhex("010300000002")
    assert modbus_crc16(request) == 0x0BC4
    assert append_crc(request) == bytes.fromhex("010300000002C40B")


def test_fc03_response_contains_requested_registers() -> None:
    request = append_crc(bytes.fromhex("010300000002"))
    response, parsed, error = build_response(
        request,
        slave_address=1,
        register_provider=lambda address, count: [0x1101, 0x1102]
        if (address, count) == (0, 2)
        else None,
    )
    assert error is None
    assert parsed is not None
    assert parsed.address == 0
    assert parsed.count_or_value == 2
    assert response is not None
    assert response[:-2] == bytes.fromhex("01030411011102")
    assert frame_crc_valid(response)


def test_illegal_address_returns_modbus_exception() -> None:
    request = append_crc(bytes.fromhex("010303520001"))
    response, _, error = build_response(
        request,
        slave_address=1,
        register_provider=lambda _address, _count: None,
    )
    assert error == "illegal data address"
    assert response is not None
    assert response[0:3] == bytes((1, 0x83, 0x02))
    assert frame_crc_valid(response)


def test_bad_crc_is_rejected() -> None:
    response, parsed, error = build_response(
        bytes.fromhex("0103000000020000"),
        slave_address=1,
        register_provider=lambda _address, _count: [0, 0],
    )
    assert response is None
    assert parsed is None
    assert error == "CRC mismatch"
