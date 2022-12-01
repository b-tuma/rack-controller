from redis import Redis
import prometheus_client as prom
from serial import Serial
import json
import time
from functools import reduce
import operator

temperature_gauge = prom.Gauge('rack_sensor_temperature', 'Temperature from DHT22 sensors', ['sensor_id'])
humidity_gauge = prom.Gauge('rack_sensor_humidity', 'Humidity from DHT22 sensors', ['sensor_id'])
redis = Redis()


def nmea_checksum(sentence: str):
    calculated_checksum = reduce(operator.xor, (ord(s) for s in sentence), 0)
    return "%0.2X" % calculated_checksum

def validate_nmea(sentence: str):
    try:
        sentence = sentence[sentence.find('$')+1:]
        sentence = sentence[:sentence.find('*')+3]
        nmea = sentence.split('*', 1)
        calculated_checksum = nmea_checksum(nmea[0])
        if calculated_checksum == nmea[1]:
            return nmea[0]
        else:
            return ""
    except:
        return ""



def parse_serial_input(sentence: str):
    nmea_pieces = sentence.split(',')
    if nmea_pieces[0] == 'SENSOR':
        if len(nmea_pieces) == 4:
            temperature_gauge.labels(sensor_id=nmea_pieces[1]).set(float(nmea_pieces[2]))
            humidity_gauge.labels(sensor_id=nmea_pieces[1]).set(float(nmea_pieces[3]))
    else:
        print(nmea_pieces)

def initialize(port):
    prom.start_http_server(9999)
    print("Open port")
    serial = Serial(port=port, baudrate=115200)
    time.sleep(2)
    while True:
        # Check if new data on serial
        if(serial.in_waiting > 0):
            data_str = validate_nmea(str(serial.readline(), 'ascii'))
            if data_str != "":
                parse_serial_input(data_str)

        # Check if new task is available
        queued_task = redis.lpop("queue:tasks")
        if queued_task is not None:
            task_json = json.loads(queued_task)
            task_device = task_json['device']
            if task_device == 'fan':
                identifier = task_json['identifier']
                speed = task_json['speed']
                default = bool(task_json['default'])
                nmea_message = f"FAN,{'CONFIG' if default else 'SET'},{str(identifier)},{str(speed)}"
                full_message = f"${nmea_message}*{nmea_checksum(nmea_message)}\r\n"
                serial.write(full_message.encode('ascii'))


        time.sleep(0.01)