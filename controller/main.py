from redis import Redis
import prometheus_client as prom
from serial import Serial
import json
import time

temperature_gauge = prom.Gauge('rack_sensor_temperature', 'Temperature from DHT22 sensors', ['sensor_id'])
humidity_gauge = prom.Gauge('rack_sensor_humidity', 'Humidity from DHT22 sensors', ['sensor_id'])
redis = Redis()


def run_task(task_json):
    task_action = task_json['action']
    print("Starting Task: " + task_action)

def initialize(port):
    prom.start_http_server(9999)
    print("Open port")
    serial = Serial(port=port, baudrate=9600)
    time.sleep(2)
    while True:
        # Check if new data on serial
        if(serial.in_waiting > 0):
            data_str = serial.readline()
            print(data_str)

        # Check if new task is available
        queued_task = redis.lpop("queue:tasks")
        if queued_task is not None:
            run_task(json.loads(queued_task))
        time.sleep(0.01)