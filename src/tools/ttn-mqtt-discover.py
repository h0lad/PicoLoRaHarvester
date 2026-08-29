#!/usr/bin/env python3
"""Connect to The Things Network MQTT broker and dump the message structure.

Subscribes to every topic (#) and pretty-prints each received message so the
payload structure of the uplinks can be determined. Also dumps the raw frm_payload
bytes and, if a `decoded_payload` is present, its structure.
"""

import argparse
import json
import os
import ssl
import sys

import paho.mqtt.client as mqtt

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def load_env_file(path):
    """Load KEY=VALUE pairs from an .env file into os.environ (no override)."""
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            if key and key not in os.environ:
                os.environ[key] = value


def on_connect(client, userdata, flags, reason_code, properties):
    if reason_code.is_failure:
        print(f"[ERROR] connection failed, rc={reason_code}", flush=True)
        sys.exit(1)
    print("[OK] connected to TTN MQTT, subscribing to '#'", flush=True)
    client.subscribe("#", qos=1)


def on_message(client, userdata, msg):
    print("=" * 80, flush=True)
    print(f"TOPIC: {msg.topic}", flush=True)
    try:
        payload = msg.payload.decode("utf-8")
        data = json.loads(payload)
        print(json.dumps(data, indent=2, sort_keys=True), flush=True)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        print(f"[raw/non-json] {type(exc).__name__}: {exc}", flush=True)
        print(f"RAW BYTES ({len(msg.payload)}):", flush=True)
        print(msg.payload.hex(), flush=True)


def main():
    load_env_file(os.path.join(SCRIPT_DIR, "secrets.env"))

    parser = argparse.ArgumentParser(description="Discover TTN MQTT payload structure")
    parser.add_argument("--host", default=os.environ.get("TTN_HOST", "eu1.cloud.thethings.network"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("TTN_PORT", "8883")))
    parser.add_argument("--username", default=os.environ.get("TTN_USERNAME", "test-application-11111@ttn"))
    parser.add_argument("--password", default=os.environ.get("TTN_PASSWORD", ""))
    parser.add_argument("--topic", default=os.environ.get("TTN_TOPIC", "#"))
    args = parser.parse_args()

    client = mqtt.Client(client_id="ttn-discovery", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)
    client.username_pw_set(args.username, args.password)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(args.host, args.port, keepalive=60)
    print("listening... (Ctrl-C to stop)", flush=True)
    client.loop_forever()


if __name__ == "__main__":
    main()
