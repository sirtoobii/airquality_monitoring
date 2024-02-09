# Raspberry PI variant

The same project but implemented on a Raspberry PI instead of an Arduino. 

## Raspberry PI setup

- Enable I2C: `raspi-config->Interface Options->I2C`
- Enable Serial port: `raspi-config->Interface Options->Serial Port` (login shell: no) (serial port hardware: yes)
- Wiring: Check comments in each sensor implementation
- 
Setup Python3 environment (inside `/pi` directory)

```shell
# Setup venv
python3 -m venv venv

# activate venv
source venv/bin/activate

# install requirements
pip install -r requirements.txt

# copy and edit .env file according your local environment
cp .env.dist .env

# add your user to "dialout" group to access the serial and i2c interface as non root user
usermod -aG dialout <user_name>
```
The corresponding systemd service (to be placed in `/usr/lib/systemd/system/raspi-airquality.service`)
```text
[Unit]
Description=Air-quality Monitoring
After=network.target
Wants=network-online.target

[Service]
Restart=always
Type=simple
Type=simple
ExecStart=/home/<user_name>/airquality_monitoring/pi/venv/bin/python3 /home/<user_name>/airquality_monitoring/pi/app/main.py
User=<user_name>
Group=<user_group>

[Install]
WantedBy=multi-user.target 
```

You probably also want to enable the service on boot: `systemctl enable raspi-airquality.service`

## Influx and Grafana

I highly recommend using this docker-compose setup: [https://github.com/sirtoobii/docker-compose-influxdb-grafana](https://github.com/sirtoobii/docker-compose-influxdb-grafana). The [included grafana
dashboard config](../grafana/Dashboard_config_influx_2_0.json) should work right away with this setup.