import os
import time
from datetime import datetime

from sensors.sht20 import SHT20
from sensors.bmp180 import BMP180
from sensors.mhz19b import MHZ19B
from smbus2 import SMBus
from influxdb_client import InfluxDBClient
import logging

from dotenv import load_dotenv

load_dotenv()

READ_FREQ_SEC = 10
logging.basicConfig(level=logging.INFO, format="[%(levelname)s | %(filename)s:%(lineno)s]: %(message)s")

if __name__ == '__main__':
    first_read_ok = False
    influx_url = os.getenv("INFLUX_URL")
    influx_org = os.getenv("INFLUX_ORG")
    influx_token = os.getenv("INFLUX_ACCESS_TOKEN")
    influx_bucket = os.getenv("INFLUX_BUCKET")
    client = InfluxDBClient(url=influx_url, token=influx_token, org=influx_org)
    write_api = client.write_api()
    logging.info("Starting up...")

    bus = SMBus(1)
    b = BMP180(bus=bus)
    s = SHT20(bus=bus)
    z = MHZ19B(serial_device="/dev/serial0")

    try:
        while True:
            try:
                temperature1, pressure = b.read_data()
                temperature2, humidity = s.read_all()
                co2 = z.read_co2_concentration()
                if co2 is not None:
                    write_api.write(bucket=influx_bucket, org=influx_org, record={
                        "measurement": "air_quality",
                        "time": datetime.utcnow().isoformat(),
                        "fields": {
                            "temperature": (temperature1 + temperature2) / 2,
                            "humidity": humidity,
                            "pressure": pressure,
                            "co2": co2
                        }
                    })
                    if not first_read_ok:
                        logging.info("Read all sensors successfully. Continue operation...")
                        first_read_ok = True
            except IOError as ie:
                logging.error(f"Error while reading sensor data: {ie}")
            except Exception as e:
                logging.error(f"Unhandled exception: {e}")

            time.sleep(READ_FREQ_SEC)
    except KeyboardInterrupt:
        logging.info("Keyboard interrupt: Bye")
