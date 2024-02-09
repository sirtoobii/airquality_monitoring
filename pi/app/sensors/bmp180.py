import struct

import smbus2
import time


class BMP180:
    """
    Communicate with the BMP180 sensor using i2c

    - 2: VCC 3.3V-5V
    - 6: GND
    - 3: SDA
    - 5: SDL

    Inspired by: https://github.com/bediger4000/station/tree/main
    """
    I2C_ADDRESS = 0x77

    def __init__(self, bus: smbus2.SMBus):
        self.bus = bus

    def read_data(self):
        """
        Read sensor data
        :return: 2-tuple containing temperature and pressure
        """
        try:
            # Read calibration data
            data = self.bus.read_i2c_block_data(self.I2C_ADDRESS, 0xAA, 22)
            (AC1, AC2, AC3, AC4, AC5, AC6, B1, B2, MB, MC, MD) = struct.unpack(
                ">hhhHHHhhhhh", bytearray(data)
            )

            # read uncompensated temperature value
            self.bus.write_byte_data(self.I2C_ADDRESS, 0xF4, 0x2E)
            # wait 5ms
            time.sleep(0.005)
            data = self.bus.read_i2c_block_data(self.I2C_ADDRESS, 0xF6, 2)
            (UT,) = struct.unpack(">h", bytearray(data))

            # read uncompensated pressure
            self.bus.write_byte_data(self.I2C_ADDRESS, 0xF4, 0x74)
            time.sleep(0.005)
            data = self.bus.read_i2c_block_data(self.I2C_ADDRESS, 0xF6, 3)
            # Convert the data
            UP = ((data[0] * 65536) + (data[1] * 256) + data[2]) / 128

            # Calculate true temperature
            X1 = (UT - AC6) * AC5 / 32768.0
            X2 = (MC * 2048.0) / (X1 + MD)
            B5 = X1 + X2
            T = ((B5 + 8.0) / 16.0) / 10.0

            # Calculate true pressure
            B6 = B5 - 4000
            X1 = (B2 * (B6 * B6 / 4096.0)) / 2048.0
            X2 = AC2 * B6 / 2048.0
            X3 = X1 + X2
            B3 = (((AC1 * 4 + X3) * 2) + 2) / 4.0
            X1 = AC3 * B6 / 8192.0
            X2 = (B1 * (B6 * B6 / 2048.0)) / 65536.0
            X3 = ((X1 + X2) + 2) / 4.0
            B4 = AC4 * (X3 + 32768) / 32768.0
            B7 = ((UP - B3) * 25000.0)
            if B7 < 2147483648:
                pressure = (B7 * 2) / B4
            else:
                pressure = (B7 / B4) * 2
            X1 = (pressure / 256.0) * (pressure / 256.0)
            X1 = (X1 * 3038.0) / 65536.0
            X2 = ((-7357) * pressure) / 65536.0
            pressure = (pressure + (X1 + X2 + 3791) / 16.0) / 100

            return T, pressure
        except Exception as e:
            raise IOError(f"BMP180: Something went wrong while reading data: {e}")
