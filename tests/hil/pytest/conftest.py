import pytest
from twister_harness import DeviceAdapter

@pytest.fixture()
def port(dut: DeviceAdapter):
    # Don't use the dut as it manipulates the data
    dut.close()

    # Grab the serial port and run with it with 1s timeout
    dut.base_timeout = 1
    dut._connect_device()
    yield dut._serial_connection

    dut.close()
