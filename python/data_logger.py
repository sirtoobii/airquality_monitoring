import sys
import datetime
from influxdb import InfluxDBClient

# Configure InfluxDB connection variables
host = "localhost" # My Ubuntu NUC
port = 8086 # default port
user = "grafana" # the user/password created for the pi, with write access
password = "grafana_pw" 
dbname = "sensor_data" # the database we created earlier

# Create the InfluxDB client object
client = InfluxDBClient(host, port, user, password, dbname)

# think of measurement as a SQL table, it's not...but...
measurement = "second_sensor"
# location will be used as a grouping tag later
location = "living_room"


def write_to_db(co2:int, temp:float, presure:float, tvo2:int):
  #iso = time.ctime()
  print(co2, temp, presure, tvo2)
  data = [
        {
          "measurement": measurement,
              "tags": {
                  "location": location,
              },
              "time": datetime.datetime.utcnow().isoformat(),
              "fields": {
                  "temperature" : temp,
                  "tvo2": tvo2,
                  "presure": presure,
                  "co2":co2
              }
          }
        ]
  client.write_points(data)
