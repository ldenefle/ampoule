import time
import logging

import serial

logger = logging.getLogger(__name__)

# TODO: Creation should be more dynamic here
PING = bytearray([0, 2, 8, 1])
PONG = bytearray([0, 4, 8, 2, 16, 1])

def test_protocol_should_pong_a_ping(port: serial.Serial):
    port.write(PING)

    resp = port.read(len(PONG))

    assert resp == PONG

def test_protocol_should_handle_partial_packets(port: serial.Serial):
    # Transmits partial packets
    port.write(PING[:2])
    port.flush()
    port.write(PING[2:])

    resp = port.read(len(PONG))

    assert resp == PONG

def test_protocol_should_recover_after_lost_packet(port: serial.Serial):
    # Transmits partial packets
    port.write(PING[:2])

    resp = port.read(len(PONG))

    assert len(resp) == 0

    # Wait a bit for recovery
    time.sleep(0.1)

    port.write(PING)

    resp = port.read(len(PONG))

    assert resp == PONG

