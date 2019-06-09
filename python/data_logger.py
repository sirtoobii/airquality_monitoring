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
measurement = "first_sensor"
# location will be used as a grouping tag later
location = "living_room"


def write_to_db(temp:float, presure:float, co2:int, hum:int):
  #iso = time.ctime()
  print(temp, presure, co2, hum)
  data = [
        {
          "measurement": measurement,
              "tags": {
                  "location": location,
              },
              "time": datetime.datetime.utcnow().isoformat(),
              "fields": {
                  "temperature" : temp,
                  "humidity": hum,
                  "presure": presure,
                  "co2":co2
              }
          }
        ]
  client.write_points(data)
