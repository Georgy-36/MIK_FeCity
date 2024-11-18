# app.py
from flask import Flask, render_template, request, jsonify
from bleak import BleakClient, BleakScanner
import asyncio
import json
import os
import threading
import nest_asyncio

nest_asyncio.apply()

app = Flask(__name__)

# Global variables
client = None
device = None
connected_device_name = None
discovered_devices = {}
loop = asyncio.new_event_loop()

# Load config
def load_config():
    config_path = 'config.json'
    if not os.path.exists(config_path):
        devices = {
            "00001": {
                "name": "ESP32_BLE",
                "service_uuid": "4fafc201-1fb5-459e-8fcc-c5c9c331914b",
                "characteristic_uuid": "beb5483e-36e1-4688-b7f5-ea07361b26a8",
                "request_uuid": "c0de1234-5678-9abc-def0-123456789abc"
            }
        }
        with open(config_path, 'w') as f:
            json.dump(devices, f, indent=4)
    else:
        with open(config_path, 'r') as f:
            devices = json.load(f)
    return devices

devices = load_config()

def run_async(coro):
    return loop.run_until_complete(coro)

@app.route('/')
def index():
    return render_template('index.html', devices=devices)

@app.route('/connect', methods=['POST'])
def connect():
    global client, device, connected_device_name
    selected_device_id = request.form.get('device')
    if not selected_device_id:
        return jsonify({"status": "error", "message": "No device selected"})

    selected_device_info = devices[selected_device_id]
    selected_device_name = selected_device_info["name"]

    def connect_ble():
        global client, device, connected_device_name
        try:
            device = run_async(BleakScanner.find_device_by_name(selected_device_name, timeout=5.0))
            if device is None:
                return {"status": "error", "message": f"{selected_device_name} not found"}

            client = BleakClient(device)
            run_async(client.connect())

            if client.is_connected:
                connected_device_name = device.name
                return {"status": "success", "message": f"Connected to {device.name}"}
            else:
                raise Exception("Failed to establish connection")

        except Exception as e:
            return {"status": "error", "message": f"Connection failed: {str(e)}"}

    result = connect_ble()
    return jsonify(result)

@app.route('/disconnect', methods=['POST'])
def disconnect():
    global client, device, connected_device_name
    if client and client.is_connected:
        run_async(client.disconnect())
        client = None
        device = None
        connected_device_name = None
        return jsonify({"status": "success", "message": "Disconnected"})
    else:
        return jsonify({"status": "error", "message": "Not connected"})

@app.route('/read', methods=['POST'])
def read():
    if not client or not client.is_connected:
        return jsonify({"status": "error", "message": "Not connected"})

    def read_ble():
        try:
            request_uuid = devices[request.form.get('device')]["request_uuid"]
            characteristic_uuid = devices[request.form.get('device')]["characteristic_uuid"]

            run_async(client.write_gatt_char(request_uuid, bytearray("get", "utf-8")))
            run_async(asyncio.sleep(1.0))

            value = run_async(client.read_gatt_char(characteristic_uuid))
            decoded_value = value.decode('utf-8').strip()

            if not decoded_value:
                return {"status": "error", "message": "No data received from ESP32"}

            data = json.loads(decoded_value)
            return {"status": "success", "data": data}

        except Exception as e:
            return {"status": "error", "message": f"Error in read: {str(e)}"}

    result = read_ble()
    return jsonify(result)

@app.route('/write', methods=['POST'])
def write():
    if not client or not client.is_connected:
        return jsonify({"status": "error", "message": "Not connected"})

    def write_ble():
        try:
            data = {
                "CargoID": request.form.get('cargo_id'),
                "Longitude": request.form.get('longitude'),
                "Latitude": request.form.get('latitude'),
                "Address": request.form.get('address'),
                "FullName": request.form.get('full_name'),
                "Price": request.form.get('price'),
                "Weight": request.form.get('weight'),
                "Description": request.form.get('description')
            }

            # Perform validation here (omitted for brevity)

            json_data = json.dumps(data)
            characteristic_uuid = devices[request.form.get('device')]["characteristic_uuid"]
            run_async(client.write_gatt_char(characteristic_uuid, json_data.encode('utf-8')))
            return {"status": "success", "message": "Data sent successfully"}

        except Exception as e:
            return {"status": "error", "message": f"Error writing: {str(e)}"}

    result = write_ble()
    return jsonify(result)

@app.route('/check_connection', methods=['POST'])
def check_connection():
    global client, connected_device_name
    if client and client.is_connected:
        return jsonify({"status": "success", "message": f"Connected to {connected_device_name}"})
    else:
        return jsonify({"status": "error", "message": "Not connected"})


def run_loop_in_thread(loop):
    asyncio.set_event_loop(loop)
    loop.run_forever()

if __name__ == '__main__':
    threading.Thread(target=run_loop_in_thread, args=(loop,), daemon=True).start()
    app.run(debug=True, use_reloader=False)