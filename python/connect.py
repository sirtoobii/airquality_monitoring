import struct
import re
import bluepy.btle as btle
from data_logger import write_to_db
# callback class
class MyDelegate(btle.DefaultDelegate):
    def __init__(self):
        btle.DefaultDelegate.__init__(self)

    def handleNotification(self, cHandle, data):
        try:
            pattern = r't(\d{2}.\d)p\s*(\d{3,4})c(\d{3,4})h(\d{1,4})'
            str_data = data.decode()
            matched_data = re.findall(pattern, str_data)
            temp = float(matched_data[0][0])
            presure = float(matched_data[0][1])
            co2 = int(matched_data[0][2])
            hum = int(matched_data[0][3])
            write_to_db(temp, presure, co2, hum)
            for data in matched_data:
                print(data)
        except IndexError:
           print("Got data with errors. Got: {}".format(data.decode()))


if __name__ == '__main__':
    try:
        # connect to device
        per = btle.Peripheral("d0:5f:b8:03:7d:3a",iface=0)

        try:
            # set callback for notifications
            per.setDelegate(MyDelegate())

            # enable notification
            setup_data = b"\x01\x00"
            notify = per.getCharacteristics(uuid='0000ffe1-0000-1000-8000-00805f9b34fb')[0]
            notify_handle = notify.getHandle() + 1
            per.writeCharacteristic(notify_handle, setup_data, withResponse=True)

            # wait for answer
            while True:
                if per.waitForNotifications(1.0):
                    continue
        finally:
            per.disconnect()
    except KeyboardInterrupt:
        print("Exiting....")
        per.disconnect()
