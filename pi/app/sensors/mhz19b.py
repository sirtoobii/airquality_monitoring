import time
import serial


class MHZ19B:
    """
    Interface the MHZ19B Co2 sensor.

    GPIO connections:
    - 2: VIN 5V
    - 6: GND
    - 8 (GPIO 14, TXD): RX
    - 10 (GPIO 15, RXD): TX

    """

    def __init__(self, serial_device: str):
        self.serial_device = serial_device

    def _send_serial_command(self, cmd: bytes, n_resp_bytes: int = 0):
        handle = serial.Serial(self.serial_device, baudrate=9600, timeout=1.0, bytesize=serial.EIGHTBITS,
                               parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE)
        time.sleep(0.01)
        try:
            handle.write(cmd)
            time.sleep(0.01)
            if n_resp_bytes > 0:
                _resp = handle.read(9)
                if len(_resp) > 0:
                    if self.checksum(_resp) == _resp[7]:
                        return _resp
                    else:
                        raise IOError("MHZ19B: Checksum missmatch")
                else:
                    raise IOError("MHZ19B: No data read")
        finally:
            handle.close()

    def read_co2_concentration(self):
        cmd = bytearray([0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79])
        _resp = self._send_serial_command(cmd, 9)
        if _resp is not None:
            return _resp[2] * 256 + _resp[3]

    def zero_point_calibration(self):
        cmd = bytearray([0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78])
        self._send_serial_command(cmd)

    @staticmethod
    def checksum(data: bytes):
        _sum = 0
        for byte in data[1:]:
            _sum += byte
        _sum = 0xFF - _sum
        return (_sum + 1) & 0xFF


if __name__ == '__main__':
    z = MHZ19B(serial_device="/dev/serial0")
    print(z.read_co2_concentration())
