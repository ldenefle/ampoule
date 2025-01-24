import pytest
import time
import logging

from twister_harness import DeviceAdapter
import serial

logger = logging.getLogger(__name__)

# TODO: Creation should be more dynamic here
PING = bytearray([0, 2, 8, 1])
PONG = bytearray([0, 2, 8, 2])

@pytest.fixture()
def port(dut: DeviceAdapter):
    # Don't use the dut as it manipulates the data
    dut.close()

    # Grab the serial port and run with it with 1s timeout
    dut.base_timeout = 1
    dut._connect_device()
    yield dut._serial_connection

def test_ping_should_pong(port: serial.Serial):
    port.write(PING)

    resp = port.read(len(PONG))

    assert resp == PONG

