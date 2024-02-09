from time import sleep

import smbus2
from smbus2 import SMBus, i2c_msg

TEMP_RES_14bit = 0x00
TEMP_RES_13bit = 0x80
TEMP_RES_12bit = 0x01
TEMP_RES_11bit = 0x81


def bytes_to_int(bytes):
    result = 0
    for b in bytes:
        result = result * 256 + int(b)
    return result


def calculateCRC(input):
    crc = 0x0
    for i in range(0, 2):
        crc = crc ^ input[i]
        for j in range(8, 0, -1):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x31
            else:
                crc = crc << 1
    crc = crc & 0x0000FF
    return crc


def checkCRC(result):
    for i in range(2, len(result), 3):
        data = []
        data.append(result[i - 2])
        data.append(result[i - 1])

        crc = result[i]

        if crc == calculateCRC(data):
            crc_result = True
        else:
            crc_result = False
    return crc_result


class SHT20:
    """
    Connect to SHT20 humidity and temperature sensor using i2c

    - 2: VCC 3.3V-5V (1)
    - 6: GND (3)
    - 3: SDA (2)
    - 5: SDL (4)

    Pin numbering on sensor looking from the top, left to right: 1,2,3,4

    Taken from: https://github.com/feyzikesim/sht20, License MIT
    """
    SHT20_ADDR = 0x40

    TRIG_TEMP_HOLD = 0xE3
    TRIG_HUMID_HOLD = 0xE5
    TRIG_TEMP_NOHOLD = 0xF3
    TRIG_HUMID_NOHOLD = 0xF5
    WRITE_USER_REG = 0xE6
    READ_USER_REG = 0xE7
    SOFT_RESET = 0xFE

    END_OF_BATTERY = 0x40
    ENABLE_HEATER = 0x04
    DISABLE_OTP_RELOAD = 0x02

    def __init__(self, bus: smbus2.SMBus, resolution: int = TEMP_RES_14bit):
        self.bus = bus
        self.sht20_init(resolution)

    def sht20_init(self, inits):
        self.bus.write_byte(self.SHT20_ADDR, self.SOFT_RESET)
        sleep(0.2)

        userReg = self.bus.read_byte_data(self.SHT20_ADDR, self.READ_USER_REG)
        userReg &= 0x38  # we must not change "Reserved bits"
        userReg |= inits + self.DISABLE_OTP_RELOAD  # add our settings + always disable OTP

        self.bus.write_byte_data(self.SHT20_ADDR, self.WRITE_USER_REG, userReg)

    def start_temp_measurement(self):
        self.bus.write_byte(self.SHT20_ADDR, self.TRIG_TEMP_NOHOLD)
        sleep(0.1)

    def start_humid_measurement(self):
        self.bus.write_byte(self.SHT20_ADDR, self.TRIG_HUMID_NOHOLD)
        sleep(0.1)

    def read_temp(self):
        temp_list = []

        self.start_temp_measurement()

        readTemp = i2c_msg.read(self.SHT20_ADDR, 3)

        self.bus.i2c_rdwr(readTemp)
        for i in range(readTemp.len):
            temp_list.append(bytes_to_int(readTemp.buf[i]))

        if checkCRC(temp_list):
            temp_list[1] &= 0xFC
            temp = (((temp_list[0] * pow(2, 8) + temp_list[1]) * 175.72) / pow(2, 16)) - 46.85
            return temp
        else:
            raise IOError("SHT20: Checksum mismatch in temperature result")

    def read_humid(self):
        humid_list = []

        self.start_humid_measurement()

        readHumid = i2c_msg.read(self.SHT20_ADDR, 3)
        self.bus.i2c_rdwr(readHumid)

        for i in range(readHumid.len):
            humid_list.append(bytes_to_int(readHumid.buf[i]))

        if checkCRC(humid_list):
            humid_list[1] &= 0xFC
            humid = (((humid_list[0] * pow(2, 8) + humid_list[1]) * 125) / pow(2, 16)) - 6
            return humid
        else:
            raise IOError("SHT20: Checksum mismatch in humidity result")

    def read_all(self):
        """
        Read all sensor data
        :return: 2-tuple containing temperature and humidity
        """
        return self.read_temp(), self.read_humid()

    def device_reset(self):
        self.bus.write_byte(self.SHT20_ADDR, self.SOFT_RESET)
        sleep(1)