from flask import Flask, request
from redis import Redis
import json

app = Flask(__name__)
redis = Redis()

@app.route('/', methods=['GET'])
def main():
    return 'Welcome to Rack-Controller!'

@app.route('/fan', methods=['POST'])
def set_fan():
    identifier = request.args.get('id')
    speed = request.args.get('speed')
    default = request.args.get('default')

    errors = []

    if identifier is None:
        errors.append("Missing Parameter: id (integer)")
    elif not identifier.isnumeric():
        errors.append("Invalid Parameter: id (integer)")

    if speed is None:
        errors.append("Missing Parameter: speed (between 0 and 255)")
    elif not speed.isnumeric() or int(speed) < 0 or int(speed) > 255:
        errors.append("Invalid Parameter: speed (between 0 and 255)")

    has_errors = len(errors) > 0

    response = json.loads('{"acknowledged": ' + str(not has_errors).lower() + ' }')
    if has_errors:
        response.update({"errors": errors})
        return response, 400

    task = {"device": "fan",
            "default": default is not None,
            "identifier": int(identifier),
            "speed": int(speed)}

    redis.rpush('queue:tasks', json.dumps(task))
    response.update({"task": task})
    return response, 200


@app.route('/servo', methods=['POST'])
def set_servo():
    identifier = request.args.get('id')
    position = request.args.get('position')
    default = request.args.get('default')
    minimum = request.args.get('min')
    maximum = request.args.get('max')

    errors = []

    if identifier is None:
        errors.append("Missing Parameter: id (integer)")
    elif not identifier.isnumeric():
        errors.append("Invalid Parameter: id (integer)")

    if position is not None:
        if not position.isnumeric() or int(position) < 0 or int(position) > 255:
            errors.append("Invalid Parameter: position (between 0 and 255)")

    if minimum is not None:
        if not minimum.isnumeric() or int(minimum) < 0 or int(minimum) > 255:
            errors.append("Invalid Parameter: minimum (between 0 and 255)")

    if maximum is not None:
        if not maximum.isnumeric() or int(maximum) < 0 or int(maximum) > 255:
            errors.append("Invalid Parameter: maximum (between 0 and 255)")

    if position is None and minimum is None and maximum is None:
        errors.append("Missing Parameter: position, minimum, or maximum.")

    has_errors = len(errors) > 0

    response = json.loads('{"acknowledged": ' + str(not has_errors).lower() + ' }')
    if has_errors:
        response.update({"errors": errors})
        return response, 400

    task = {"device": "servo",
            "default": default is not None,
            "identifier": int(identifier)}

    if position is not None:
        task.update({"position": int(position)})
    if minimum is not None:
        task.update({"minimum": int(minimum)})
    if maximum is not None:
        task.update({"maximum": int(maximum)})

    redis.rpush('queue:tasks', json.dumps(task))
    response.update({"task": task})
    return response, 200

@app.route('/sensor', methods=['POST'])
def set_sensor():
    identifier = request.args.get('id')
    enable = request.args.get('enable')

    errors = []

    if identifier is None:
        errors.append("Missing Parameter: id (integer)")
    elif not identifier.isnumeric():
        errors.append("Invalid Parameter: id (integer)")

    if enable is None:
        errors.append("Missing Parameter: enable ('true' or 'false')")
    elif enable != "true" and enable != "false":
        errors.append("Invalid Parameter: enable ('true' or 'false')")

    has_errors = len(errors) > 0

    response = json.loads('{"acknowledged": ' + str(not has_errors).lower() + ' }')
    if has_errors:
        response.update({"errors": errors})
        return response, 400

    task = {"device": "servo",
            "identifier": int(identifier),
            "enable": enable == "true"}

    redis.rpush('queue:tasks', json.dumps(task))
    response.update({"task": task})
    return response, 200

@app.route('/board', methods=['POST'])
def set_board():
    restart = request.args.get('restart')
    factory_reset = request.args.get('factory')

    errors = []

    if restart is None and factory_reset is None:
        errors.append("Missing Parameter: restart or factory_reset")

    has_errors = len(errors) > 0

    response = json.loads('{"acknowledged": ' + str(not has_errors).lower() + ' }')
    if has_errors:
        response.update({"errors": errors})
        return response, 400

    task = {"device": "board",
            "restart": restart is not None,
            "factory": factory_reset is not None}

    redis.rpush('queue:tasks', json.dumps(task))
    response.update({"task": task})
    return response, 200