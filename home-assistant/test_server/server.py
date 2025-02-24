from flask import Flask, jsonify, request
from math import copysign

app = Flask(__name__)

target = 0
position = 0

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

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000)
