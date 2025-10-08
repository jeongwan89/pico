#!/usr/bin/env python3
"""
esp_at_probe.py

Probe an ESP AT firmware via a serial port to check for support of MQTT-related AT commands.

Usage:
  python3 tools/esp_at_probe.py /dev/ttyUSB0 57600

The script sends a small set of candidate AT commands and logs responses to
tools/esp_at_probe_results.txt and prints a short summary.
"""
import sys
import time
from datetime import datetime

try:
    import serial
except Exception:
    print("pyserial is required. Install with: pip3 install pyserial")
    sys.exit(1)


COMMANDS = [
    "AT",
    "AT+GMR",
    "AT+HELP",
    "AT+SYSMSG",
    # MQTT-style commands used by esp-at builds
    "AT+MQTTUSERCFG",
    "AT+MQTTUSERCFG?",
    "AT+MQTTCONN",
    "AT+MQTTCONN?",
    "AT+MQTTPUB",
    "AT+MQTTPUB?",
    "AT+MQTTSUB",
    "AT+MQTTSUB?",
    "AT+MQTTUNSUB",
    "AT+MQTTDISCONN",
    "AT+MQTTSTATE",
    "AT+MQTT?",
    # fallback TCP commands
    "AT+CIPSTART",
    "AT+CIPSEND",
]


def read_response(ser, overall_timeout=2.0, idle_timeout=0.2):
    """Read lines from serial until overall_timeout or idle_timeout passed."""
    deadline = time.time() + overall_timeout
    last_data = time.time()
    out_lines = []
    while time.time() < deadline:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            if not data:
                time.sleep(0.01)
                continue
            try:
                text = data.decode('utf-8', errors='replace')
            except Exception:
                text = repr(data)
            lines = text.splitlines()
            out_lines.extend(lines)
            last_data = time.time()
        else:
            # no data currently
            if time.time() - last_data > idle_timeout:
                break
            time.sleep(0.01)
    return out_lines


def probe(port, baud):
    ser = serial.Serial(port, baud, timeout=0.1)
    time.sleep(0.1)
    results = []

    # Flush any initial
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    for cmd in COMMANDS:
        line = cmd + '\r\n'
        ser.write(line.encode('utf-8'))
        ser.flush()
        # small settle
        time.sleep(0.05)
        resp = read_response(ser, overall_timeout=2.0, idle_timeout=0.2)
        results.append((cmd, resp))
        print(f"Sent: {cmd}  -> {len(resp)} line(s)")
        for r in resp:
            print("  ", r)
        print()

    ser.close()
    return results


def summarize(results):
    supported = []
    for cmd, resp in results:
        joined = '\n'.join(resp).lower()
        # Heuristic: presence of 'mqtt' or '+mqtt' or a non-error OK response
        if 'mqtt' in joined:
            supported.append((cmd, 'contains mqtt'))
        elif any(r.strip().upper() == 'OK' for r in resp):
            # OK could be generic, but still record
            supported.append((cmd, 'OK'))
        elif any('error' in r.lower() for r in resp):
            # explicit error
            pass
    return supported


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 tools/esp_at_probe.py <serial-port> [baud]")
        sys.exit(1)
    port = sys.argv[1]
    baud = int(sys.argv[2]) if len(sys.argv) >= 3 else 57600

    print(f"Probing {port} at {baud} bps for MQTT-related AT commands...")
    results = probe(port, baud)

    outpath = "tools/esp_at_probe_results.txt"
    with open(outpath, 'w', encoding='utf-8') as f:
        f.write(f"ESP AT probe results - {datetime.utcnow().isoformat()} UTC\n")
        f.write(f"Port: {port} Baud: {baud}\n\n")
        for cmd, resp in results:
            f.write(f">>> {cmd}\n")
            if resp:
                for r in resp:
                    f.write(r + '\n')
            else:
                f.write("<no response>\n")
            f.write('\n')

    supported = summarize(results)
    print("\nSummary: possible support for the following commands: ")
    if supported:
        for s in supported:
            print(f" - {s[0]} : {s[1]}")
    else:
        print("  No clear MQTT-related responses detected. See tools/esp_at_probe_results.txt for full logs.")

    print(f"Full log written to {outpath}")


if __name__ == '__main__':
    main()
