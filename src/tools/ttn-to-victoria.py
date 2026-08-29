#!/usr/bin/env python3
"""Forward The Things Network MQTT uplinks to VictoriaMetrics.

Subscribes to the TTN MQTT broker (eu1.cloud.thethings.network:8883, TLS) and
writes the decoded telemetry of each uplink into VictoriaMetrics using the
InfluxDB line protocol on the /write endpoint.

The device firmware sends a 15 byte bit-packed uplink
(src/app/inc/telemetry.h, telemetry_encode()).

If a TTN payload formatter (decoder) is configured, the decoded_payload already
carries the values under the same names; both cases are handled.
"""

import argparse
import base64
import gzip
import json
import os
import ssl
import struct
import sys
import threading
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
import requests

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


def parse_iso_ns(value):
    """Parse an ISO-8601 timestamp (with optional Z) into nanoseconds since epoch."""
    if not value:
        return None
    if value.endswith("Z"):
        value = value[:-1] + "+00:00"
    dt = datetime.fromisoformat(value)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=timezone.utc)
    return int(dt.timestamp() * 1_000_000_000)


def decode_binary_payload(raw: bytes) -> dict:
    """Decode the 15 byte bit-packed uplink into a flat field dict."""
    if len(raw) < 15:
        raise ValueError(f"payload too short: {len(raw)} bytes (expected 15)")

    bits = int.from_bytes(raw[:15], "big")
    pos = 15 * 8

    def take(nbits):
        nonlocal pos
        pos -= nbits
        return (bits >> pos) & ((1 << nbits) - 1)

    def take_signed(nbits):
        value = take(nbits)
        sign = 1 << (nbits - 1)
        return (value ^ sign) - sign

    current_min_ua = take(15)
    current_avg_ua = take(15)
    current_max_ua = take(15)
    voltage_min_mv = take(13)
    voltage_avg_mv = take(13)
    voltage_max_mv = take(13)
    temperature_avg_cd = take_signed(12)
    humidity_avg_permille = take(10)
    mcu_temperature_cd = take_signed(12)

    return {
        "current_min_ua": current_min_ua,
        "current_avg_ua": current_avg_ua,
        "current_max_ua": current_max_ua,
        "voltage_min_mv": voltage_min_mv,
        "voltage_avg_mv": voltage_avg_mv,
        "voltage_max_mv": voltage_max_mv,
        "temperature_avg_c": temperature_avg_cd / 10.0,
        "humidity_avg_pct": humidity_avg_permille / 10.0,
        "mcu_temperature_c": mcu_temperature_cd / 10.0,
    }


def parse_duration_s(value):
    """Parse a TTN duration string like '0.905216s' into a float (seconds)."""
    if not value:
        return None
    value = str(value).strip()
    if value.endswith("s"):
        value = value[:-1]
    try:
        return float(value)
    except ValueError:
        return None


def extract_radio(uplink: dict):
    """Extract LoRa radio settings and TX metadata from an uplink message.

    Returns (tags, fields). `tags` holds the compact data_rate label, `fields`
    holds the numeric/setting values (spreading factor, bandwidth, airtime, ...).
    """
    tags = {}
    fields = {}

    settings = uplink.get("settings") or {}
    dr = (settings.get("data_rate") or {}).get("lora") or {}
    sf = dr.get("spreading_factor")
    bw = dr.get("bandwidth")
    cr = dr.get("coding_rate")
    freq = settings.get("frequency")

    if sf is not None:
        fields["spreading_factor"] = sf
    if bw is not None:
        fields["bandwidth_hz"] = bw
    if cr is not None:
        fields["coding_rate"] = cr
    if freq is not None:
        try:
            fields["frequency_hz"] = int(freq)
        except ValueError:
            pass

    if sf is not None and bw is not None:
        tags["data_rate"] = f"SF{sf}BW{bw // 1000}"

    airtime = parse_duration_s(uplink.get("consumed_airtime"))
    if airtime is not None:
        fields["airtime_s"] = airtime

    if uplink.get("f_port") is not None:
        fields["f_port"] = uplink["f_port"]
    if uplink.get("f_cnt") is not None:
        fields["f_cnt"] = uplink["f_cnt"]

    return tags, fields


def extract_rx_metadata(uplink: dict):
    """Extract per-gateway signal levels from the uplink rx_metadata.

    Returns a list of (gateway_id, fields) tuples with rssi/snr/frequency_offset.
    """
    points = []
    for rx in uplink.get("rx_metadata") or []:
        gateway_id = (rx.get("gateway_ids") or {}).get("gateway_id")
        if not gateway_id:
            continue
        fields = {}
        if rx.get("rssi") is not None:
            fields["rssi"] = rx["rssi"]
        if rx.get("snr") is not None:
            fields["snr"] = rx["snr"]
        if rx.get("frequency_offset") is not None:
            try:
                fields["frequency_offset"] = int(rx["frequency_offset"])
            except ValueError:
                fields["frequency_offset"] = float(rx["frequency_offset"])
        if fields:
            points.append((gateway_id, fields))
    return points


def flatten_decoded_payload(decoded: dict) -> dict:
    """Flatten the nested decoded_payload structure into a flat field dict."""
    fields = {}
    current = decoded.get("current")
    if isinstance(current, dict):
        if "min_ua" in current:
            fields["current_min_ua"] = current["min_ua"]
        if "avg_ua" in current:
            fields["current_avg_ua"] = current["avg_ua"]
        if "max_ua" in current:
            fields["current_max_ua"] = current["max_ua"]

    voltage = decoded.get("voltage")
    if isinstance(voltage, dict):
        if "min_mv" in voltage:
            fields["voltage_min_mv"] = voltage["min_mv"]
        if "avg_mv" in voltage:
            fields["voltage_avg_mv"] = voltage["avg_mv"]
        if "max_mv" in voltage:
            fields["voltage_max_mv"] = voltage["max_mv"]

    for key in ("temperature_avg_c", "humidity_avg_pct", "mcu_temperature_c"):
        if key in decoded:
            fields[key] = decoded[key]

    return fields


class VictoriaSink:
    """Buffers line-protocol points and posts them to VictoriaMetrics /write."""

    def __init__(self, url, database, username, password, verify_tls,
                 batch_size=64, timeout=30, flush_interval=30.0,
                 max_buffer_lines=10000, max_retry_backoff=300.0):
        self.base_url = url.rstrip("/")
        self.database = database
        self.auth = (username, password) if username else None
        self.verify_tls = verify_tls
        self.batch_size = batch_size
        self.timeout = timeout
        self.flush_interval = flush_interval
        self.max_buffer_lines = max_buffer_lines
        self.max_retry_backoff = max_retry_backoff
        self.buffer = []
        self._lock = threading.Lock()
        self._wakeup = threading.Event()
        self._stop = threading.Event()
        self._next_retry = 0.0
        self._failures = 0
        self._dropped = 0
        self._thread = threading.Thread(target=self._run, name="victoria-flush", daemon=True)
        self._thread.start()

    def _write_url(self):
        return f"{self.base_url}/write?db={self.database}"

    def add(self, line: str):
        with self._lock:
            self.buffer.append(line)
            if len(self.buffer) > self.max_buffer_lines:
                overflow = len(self.buffer) - self.max_buffer_lines
                del self.buffer[:overflow]
                self._dropped += overflow
                print(f"[WARN] VictoriaMetrics buffer full: dropped {overflow} oldest points", flush=True)
            if len(self.buffer) >= self.batch_size:
                self._wakeup.set()

    def flush(self):
        self._wakeup.set()

    def stop(self):
        self._stop.set()
        self._wakeup.set()
        self._thread.join(timeout=self.timeout + 10)
        with self._lock:
            self._next_retry = 0.0
        self._drain()

    def _run(self):
        while not self._stop.is_set():
            self._wakeup.wait(timeout=self._wait_timeout())
            self._wakeup.clear()
            self._drain()

    def _wait_timeout(self):
        with self._lock:
            now = time.monotonic()
            if self.buffer and self._next_retry > now:
                return self._next_retry - now
            return self.flush_interval

    def _drain(self):
        batch = self._take_batch()
        while batch is not None:
            self._post_batch(batch)
            if self._stop.is_set():
                break
            batch = self._take_batch()

    def _take_batch(self):
        with self._lock:
            if time.monotonic() < self._next_retry or not self.buffer:
                return None
            batch = self.buffer[:self.batch_size]
            self.buffer = self.buffer[self.batch_size:]
            return batch

    def _post_batch(self, batch):
        body = "\n".join(batch) + "\n"
        payload = gzip.compress(body.encode("utf-8"))
        headers = {
            "Content-Type": "text/plain; charset=utf-8",
            "Content-Encoding": "gzip",
        }
        n = len(batch)
        try:
            resp = requests.post(
                self._write_url(),
                data=payload,
                headers=headers,
                auth=self.auth,
                timeout=self.timeout,
                verify=self.verify_tls,
            )
        except requests.RequestException as exc:
            self._on_failure(batch, f"network error: {exc}")
            return
        if resp.status_code in (200, 204):
            with self._lock:
                self._failures = 0
                self._next_retry = 0.0
            print(f"[OK] wrote {n} points to VictoriaMetrics", flush=True)
        else:
            self._on_failure(batch, f"HTTP {resp.status_code}: {resp.text[:200]}")

    def _on_failure(self, batch, reason):
        with self._lock:
            self._failures += 1
            self.buffer = batch + self.buffer
            if len(self.buffer) > self.max_buffer_lines:
                overflow = len(self.buffer) - self.max_buffer_lines
                del self.buffer[:overflow]
                self._dropped += overflow
            backoff = min(self.max_retry_backoff, 2 ** min(self._failures - 1, 10))
            self._next_retry = time.monotonic() + backoff
        print(f"[ERROR] VictoriaMetrics {reason}; {len(batch)} points kept, retry in {backoff:.0f}s", flush=True)


def build_line(measurement, tags, fields, ts_ns):
    """Build an InfluxDB line-protocol line."""
    tag_str = ",".join(f"{k}={v}" for k, v in tags.items() if v not in (None, ""))
    field_parts = []
    for k, v in fields.items():
        if isinstance(v, bool):
            field_parts.append(f"{k}={str(v).lower()}")
        elif isinstance(v, (int, float)):
            field_parts.append(f"{k}={v}")
        else:
            field_parts.append(f'{k}="{v}"')
    field_str = ",".join(field_parts)
    line = f"{measurement}"
    if tag_str:
        line += f",{tag_str}"
    line += f" {field_str}"
    if ts_ns is not None:
        line += f" {ts_ns}"
    return line


class TTN2Victoria:
    def __init__(self, args):
        self.args = args
        self.sink = VictoriaSink(
            args.vm_url, args.vm_database, args.vm_username, args.vm_password,
            args.verify_tls, args.batch_size, args.write_timeout,
            args.flush_interval, args.max_buffer_lines,
        )

    def handle_message(self, topic, payload: bytes):
        if not topic.endswith("/up"):
            return

        try:
            msg = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            print(f"[WARN] non-JSON message on {topic}: {exc}", flush=True)
            return

        uplink = msg.get("uplink_message")
        if not uplink:
            return

        # Determine fields: prefer TTN decoded_payload, fall back to raw bytes.
        decoded = uplink.get("decoded_payload")
        if isinstance(decoded, dict) and decoded:
            fields = flatten_decoded_payload(decoded)
        else:
            frm = uplink.get("frm_payload")
            if not frm:
                return
            try:
                raw = base64.b64decode(frm)
                fields = decode_binary_payload(raw)
            except (ValueError, struct.error) as exc:
                print(f"[WARN] cannot decode frm_payload: {exc}", flush=True)
                return

        if not fields:
            return

        ids = msg.get("end_device_ids", {})
        base_tags = {
            "application_id": ids.get("application_ids", {}).get("application_id", ""),
            "device_id": ids.get("device_id", ""),
            "dev_eui": ids.get("dev_eui", ""),
        }

        radio_tags, radio_fields = extract_radio(uplink)
        tags = {**base_tags, **radio_tags}
        fields = {**fields, **radio_fields}

        ts = parse_iso_ns(uplink.get("received_at") or msg.get("received_at"))

        # Telemetry + radio/TX summary in one measurement.
        self.sink.add(build_line(self.args.measurement, tags, fields, ts))

        # Per-gateway signal levels in a dedicated measurement.
        for gateway_id, rx_fields in extract_rx_metadata(uplink):
            rx_tags = {**base_tags, "gateway_id": gateway_id}
            self.sink.add(build_line(self.args.measurement + "_rx", rx_tags, rx_fields, ts))


def main():
    load_env_file(os.path.join(SCRIPT_DIR, "secrets.env"))

    parser = argparse.ArgumentParser(description="TTN MQTT -> VictoriaMetrics")
    parser.add_argument("--ttn-host", default=os.environ.get("TTN_HOST", "eu1.cloud.thethings.network"))
    parser.add_argument("--ttn-port", type=int, default=int(os.environ.get("TTN_PORT", "8883")))
    parser.add_argument("--ttn-username", default=os.environ.get("TTN_USERNAME", "test-application-11111@ttn"))
    parser.add_argument("--ttn-password", default=os.environ.get("TTN_PASSWORD", ""))
    parser.add_argument("--ttn-topic", default=os.environ.get("TTN_TOPIC", "#"))
    parser.add_argument("--vm-url", default=os.environ.get("VM_URL", "https://vmetrics.lab.holad.de"))
    parser.add_argument("--vm-username", default=os.environ.get("VM_USERNAME", "test"))
    parser.add_argument("--vm-password", default=os.environ.get("VM_PASSWORD", "test"))
    parser.add_argument("--vm-database", default=os.environ.get("VM_DATABASE", "db-had"))
    parser.add_argument("--measurement", default=os.environ.get("MEASUREMENT", "picoharvester"))
    parser.add_argument("--batch-size", type=int, default=int(os.environ.get("BATCH_SIZE", "64")))
    parser.add_argument("--write-timeout", type=float, default=float(os.environ.get("WRITE_TIMEOUT", "30")))
    parser.add_argument("--flush-interval", type=float, default=float(os.environ.get("FLUSH_INTERVAL", "30")))
    parser.add_argument("--max-buffer-lines", type=int, default=int(os.environ.get("MAX_BUFFER_LINES", "10000")))
    parser.add_argument("--verify-tls", default=True,
                        action=argparse.BooleanOptionalAction,
                        help="verify VictoriaMetrics TLS certificate")
    args = parser.parse_args()

    bridge = TTN2Victoria(args)

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"[ERROR] TTN MQTT connect failed, rc={reason_code}", flush=True)
            return
        print(f"[OK] connected to {args.ttn_host}, subscribing '{args.ttn_topic}'", flush=True)
        client.subscribe(args.ttn_topic, qos=1)

    def on_disconnect(client, userdata, flags, reason_code, properties):
        print(f"[WARN] TTN MQTT disconnected (rc={reason_code}), will retry", flush=True)

    def on_message(client, userdata, msg):
        bridge.handle_message(msg.topic, msg.payload)

    client = mqtt.Client(client_id="ttn-to-victoria", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.tls_set(tls_version=ssl.PROTOCOL_TLS_CLIENT)
    client.username_pw_set(args.ttn_username, args.ttn_password)
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=120)

    reconnect_wait = 1
    try:
        while True:
            try:
                client.connect(args.ttn_host, args.ttn_port, keepalive=60)
                reconnect_wait = 1
                client.loop_forever()
            except KeyboardInterrupt:
                raise
            except Exception as exc:
                print(f"[ERROR] MQTT connection/loop error: {exc}; retrying in {reconnect_wait}s", flush=True)
                time.sleep(reconnect_wait)
                reconnect_wait = min(reconnect_wait * 2, 120)
    except KeyboardInterrupt:
        pass
    finally:
        try:
            client.disconnect()
        except Exception:
            pass
        bridge.sink.stop()


if __name__ == "__main__":
    main()
