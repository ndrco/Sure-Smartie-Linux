import serial
import time

PORT = "/dev/ttyUSB1"
BAUD = 9600

def line_packet(row: int, text: str) -> bytes:
    # row: 1..4
    text = text[:20].ljust(20)
    return bytes([0xFE, 0x47, 0x01, row]) + text.encode("ascii", errors="replace")

with serial.Serial(PORT, BAUD, timeout=1) as ser:
    time.sleep(0.2)

    # backlight on
    ser.write(bytes([0xFE, 0x42, 0x00]))

    # contrast: 0x01..0xFE
    ser.write(bytes([0xFE, 0x50, 0x80]))

    # brightness: 0x01..0xFE
    ser.write(bytes([0xFE, 0x98, 0xC0]))

    ser.write(line_packet(1, "SURE LCD on Linux"))
    ser.write(line_packet(2, "ttyUSB1 @ 9600"))
    ser.write(line_packet(3, "Protocol: native"))
    ser.write(line_packet(4, "No MtxOrb weirdness"))
