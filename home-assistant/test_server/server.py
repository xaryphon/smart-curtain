from flask import Flask, jsonify, request
from math import copysign
from random import random

app = Flask(__name__)

target = 0
position = 0
lux_target = 5000
mode_auto = False

def direction() -> int:
    diff = target - position
    if diff < 0:
        return -1
    elif diff > 0:
        return 1
    else:
        return 0


def status_blob() -> dict:
    blob = { 'position': position, 'direction': direction() }
    print(blob)
    return blob


def update_position():
    global position
    position += direction()


@app.route('/status', methods=['GET'])
def get_curtain_status():
    update_position()
    return jsonify(status_blob())


@app.route('/open', methods=['POST'])
def open_curtains():
    global target
    target = 100
    return jsonify(status_blob())


@app.route('/close', methods=['POST'])
def close_curtains():
    global target
    target = 0
    return jsonify(status_blob())


@app.route('/set_position/<int:position>', methods=['POST'])
def move_curtains(position):
    global target
    target = position
    return jsonify(status_blob())

@app.route('/als', methods=['GET'])
def get_als():
    return jsonify({'value': 100_000})

@app.route('/target', methods=['GET'])
def get_target():
    return jsonify({'value': lux_target})

@app.route('/target/<float:value>', methods=['POST'])
def set_target(value):
    global lux_target
    lux_target = value
    return jsonify({'value': lux_target})

@app.route('/mode', methods=['GET'])
def get_mode():
    return jsonify({'mode': 'automatic' if mode_auto else 'manual'})

@app.route('/mode/automatic', methods=['POST'])
def enable_automatic_mode():
    global mode_auto
    mode_auto = True
    return jsonify({'mode': 'automatic'})

@app.route('/mode/manual', methods=['POST'])
def disable_automatic_mode():
    global mode_auto
    mode_auto = False
    return jsonify({'mode': 'manual'})

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
