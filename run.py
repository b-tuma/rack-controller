from webapp import api
from controller import main
import os
import time
import threading

if __name__ == '__main__':
    os.system('service redis-server start')
    time.sleep(1)
    main_thread = threading.Thread(target=main.initialize, args=("/dev/serial/by-id/usb-Teensyduino_USB_Serial_1327270-if00",))
    flask_thread = threading.Thread(target=api.app.run, args=("0.0.0.0",))
    main_thread.start()
    time.sleep(1)
    flask_thread.start()
    time.sleep(100)