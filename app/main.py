from redis import Redis
import prometheus_client as prom
from serial import Serial
import json
import time
from functools import reduce
import operator
from webapp import api
import os
import threading

port = os.getenv("SERIAL_DEV", "/dev/serial/by-id/usb-Teensyduino_USB_Serial_1327270-if00")
baudrate = int(os.getenv("SERIAL_BAUD", "115200"))


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
        print("Received Message: " + str(nmea_pieces))

def fan_control(task):
    identifier = task['identifier']
    speed = task['speed']
    default = bool(task['default'])

    nmea_message = f"FAN,{'CONFIG' if default else 'SET'},{str(identifier)},{str(speed)}"
    full_message = f"${nmea_message}*{nmea_checksum(nmea_message)}\r\n"
    return full_message

def servo_control(task):
    identifier = task['identifier']
    position = task['position']
    minimum = task['minimum']
    maximum = task['maximum']
    default = bool(task['default'])

    nmea_message = None
    if default:
        if position is not None and minimum is not None and maximum is not None:
            nmea_message = f"SERVO,CONFIG,{str(identifier)},{str(position)},{str(minimum)},{str(maximum)}"
    else:
        if position is not None:
            nmea_message = f"SERVO,SET,{str(identifier)},{str(position)}"
    if nmea_message is None:
        return None

    full_message = f"${nmea_message}*{nmea_checksum(nmea_message)}\r\n"
    return full_message

def sensor_control(task):
    identifier = task['identifier']
    enable = int(task['enable'])
    default = bool(task['default'])
    if not default:
        return None
    
    nmea_message = f"SENSOR,CONFIG,{str(identifier)},{str(enable)}"
    full_message = f"${nmea_message}*{nmea_checksum(nmea_message)}\r\n"
    return full_message

def board_control(task):
    factory_reset = bool(task['factory'])
    restart = bool(task['restart'])
    if factory_reset:
        nmea_message = "CONTROLLER,FACTORY"
    elif restart:
        nmea_message = "CONTROLLER,RESTART"
    full_message = f"${nmea_message}*{nmea_checksum(nmea_message)}\r\n"
    return full_message
    

if __name__ == '__main__':
    print("Starting up redis...")
    os.system('service redis-server start')
    time.sleep(2)
    redis = Redis()
    if not redis.ping():
        raise Exception("Redis failed to start.")

    print("Starting up metrics endpoint...")
    temperature_gauge = prom.Gauge('rack_sensor_temperature', 'Temperature from DHT22 sensors', ['sensor_id'])
    humidity_gauge = prom.Gauge('rack_sensor_humidity', 'Humidity from DHT22 sensors', ['sensor_id'])
    prom.start_http_server(9999)
    time.sleep(2)

    print("Starting up Flask...")
    flask_thread = threading.Thread(target=api.app.run, args=("0.0.0.0",))
    flask_thread.start()
    time.sleep(2)

    print("Starting up serial port...")
    serial = Serial(port=port, baudrate=baudrate)
    time.sleep(2)
    while serial.is_open:
        # Check if new data on serial
        while serial.in_waiting > 0:
            data_str = validate_nmea(str(serial.readline(), 'ascii'))
            if data_str != "":
                parse_serial_input(data_str)

        # Check if new task is available
        queued_task = redis.lpop("queue:tasks")
        if queued_task is not None:
            task_json = json.loads(queued_task)
            task_device = task_json['device']
            message = None
            match task_device:
                case 'fan':
                    message = fan_control(task_json)
                case 'servo':
                    message = servo_control(task_json)
                case 'sensor':
                    message = sensor_control(task_json)
                case 'board':
                    message = board_control(task_json)
                case _:
                    print(f"Invalid device: {task_device}.")
            if message is not None:
                print("Send message: " + message)
                serial.write(message.encode('ascii'))

        time.sleep(1)