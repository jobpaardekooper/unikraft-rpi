#!/usr/bin/env python3
"""
lora_sender.py — Simulated telemetry sender for ground-station receive testing.

Connects to an E220 LoRa module via USB-UART adapter and sends CSV telemetry
packets in transparent mode.  The ground station parses:

    ax,ay,az,gx,gy,gz,speed,alt\n

Hardware wiring (E220 on the sender side):
    E220 TX  → USB-UART RX
    E220 RX  → USB-UART TX
    E220 M0  → GND  (normal / transparent mode)
    E220 M1  → GND
    E220 AUX → (optional, not needed for TX)
    E220 VCC → 3.3 V
    E220 GND → GND

Simulated flight profile:
    Max altitude : 9 000 m  (900 000 cm)
    Max speed    : Mach 2   ≈ 680 m/s  (68 000 cm/s)
    Motor burn   : 0 – 6 s  (~20 g axial, spin-stabilised)
    Coast        : 6 – 62 s (drag + gravity, spin decaying)
    Apogee       : ~62 s
    Parachute    : 65 s onwards (~15 m/s descent)
    Landing      : ~420 s

Usage:
    pip install pyserial
    python3 lora_sender.py /dev/ttyUSB0
    python3 lora_sender.py COM3 --baud 9600 --rate 2
"""

import argparse
import math
import sys
import time

try:
    import serial
except ImportError:
    print("pyserial not found. Install with:  pip install pyserial", file=sys.stderr)
    sys.exit(1)


# ---------------------------------------------------------------------------
# Rocket flight model
#
# Target specs:
#   Max altitude : 9 000 m  (900 000 cm)
#   Max speed    : Mach 2   ≈ 680 m/s  (68 000 cm/s)
#
# Flight phases:
#   0  – T_BOOST  s  Motor burn   : 0 → Mach 2, 0 → ~2 km, ~20 g axial
#   T_BOOST – T_APOGEE s  Coast   : Mach 2 → 0, ~2 km → 9 km, drag + gravity
#   T_APOGEE – T_CHUTE s  Tumble  : near-zero speed, chute deploys
#   T_CHUTE – T_LAND s   Descent  : ~15 m/s under parachute
# ---------------------------------------------------------------------------

T_BOOST  =   6.0     # motor burn ends (s)
T_APOGEE =  62.0     # apogee (s)
T_CHUTE  =  65.0     # parachute deploys (s)
T_LAND   = 420.0     # touchdown (s)  — ~7 min total flight


def _simulate(t: float):
    """
    Return (ax, ay, az, gx, gy, gz, speed, alt) for elapsed time t (seconds).

    Rocket is vertical, so az is the axial (thrust) axis:
      +az = upward acceleration  (boost: ~+20 000 mg)
      -az = deceleration/gravity (coast: ~-(981 + drag))

    ax/ay model lateral perturbations (wind, thrust misalignment, pendulum
    under chute).  gx/gy model spin stabilisation during boost, slow tumble
    near apogee, and gentle pendulum under canopy.
    """

    # ── Speed (m/s — integer) ──────────────────────────────────────────────
    MACH2_MPS = 680    # Mach 2 ≈ 680 m/s
    if t <= T_BOOST:
        speed = int(MACH2_MPS * t / T_BOOST)
    elif t <= T_APOGEE:
        frac  = (t - T_BOOST) / (T_APOGEE - T_BOOST)
        speed = max(0, int(MACH2_MPS * (1.0 - frac) ** 1.4))
    elif t <= T_CHUTE:
        speed = int(MACH2_MPS * 0.01 * (T_CHUTE - t) / (T_CHUTE - T_APOGEE))
    else:
        speed = 15      # ~15 m/s terminal velocity under parachute

    # ── Altitude (m — integer) ─────────────────────────────────────────────
    ALT_MAX_M = 9_000   # 9 km
    if t <= T_APOGEE:
        frac = t / T_APOGEE
        alt  = int(ALT_MAX_M * math.sin(math.pi / 2.0 * frac) ** 0.65)
    else:
        alt = int(max(0, ALT_MAX_M - 15 * (t - T_APOGEE)))

    # ── Accelerations (g — integer) ────────────────────────────────────────
    if t <= T_BOOST:
        # ~20 g axial thrust + vibration; small lateral perturbations
        az = int(20 + 1 * math.sin(t * 31.4))   # 5 Hz vibration ±1 g
        ax = int( 1 * math.sin(t * 7.3))
        ay = int( 1 * math.cos(t * 6.1))
    elif t <= T_APOGEE:
        # Drag deceleration + gravity (values negative = decelerating upward)
        frac   = (t - T_BOOST) / (T_APOGEE - T_BOOST)
        drag_g = int(5 * (1.0 - frac) ** 2)     # ±5 g drag → 0 at apogee
        az     = -(1 + drag_g)
        ax     = int(1 * math.sin(t * 2.1))
        ay     = int(1 * math.cos(t * 1.7))
    elif t <= T_CHUTE:
        # Tumbling near apogee
        az = int(-1 + 3 * math.sin(t * 12.6))
        ax = int( 4 * math.sin(t * 9.4))
        ay = int( 4 * math.cos(t * 8.1))
    else:
        # Gentle pendulum under parachute (≈ 1 g vertical, ±1 g lateral)
        az = int(-1 + 1 * math.sin(t * 0.63))
        ax = int( 1 * math.sin(t * 0.41))
        ay = int( 1 * math.cos(t * 0.29))

    # ── Angular rates (°/s — integer) ─────────────────────────────────────
    if t <= T_BOOST:
        # Spin-stabilised: ~3 Hz roll = 1080 °/s
        spin = 1_080
        gx   = int(spin * math.sin(t * 18.85))
        gy   = int(spin * math.cos(t * 18.85))
        gz   = int(   8 * math.sin(t * 4.2))    # slight yaw drift
    elif t <= T_APOGEE:
        # Spin decaying due to aerodynamic damping
        frac = (t - T_BOOST) / (T_APOGEE - T_BOOST)
        spin = int(1_080 * (1.0 - frac) ** 1.5)
        gx   = int(spin * math.sin(t * 15.7))
        gy   = int(spin * math.cos(t * 15.7))
        gz   = int(   3 * math.sin(t * 2.0))
    elif t <= T_CHUTE:
        # Tumbling — all axes excited
        gx = int(35 * math.sin(t * 6.3))
        gy = int(35 * math.cos(t * 5.0))
        gz = int(28 * math.sin(t * 3.8))
    else:
        # Slow pendulum swing under canopy
        gx = int(4 * math.sin(t * 0.52))
        gy = int(4 * math.cos(t * 0.38))
        gz = int(2 * math.sin(t * 0.21))

    return ax, ay, az, gx, gy, gz, speed, alt

def main():
    parser = argparse.ArgumentParser(
        description="Send simulated LoRa telemetry packets to the ground station.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "port",
        help="Serial port connected to the E220 (e.g. /dev/ttyUSB0 or COM3)",
    )
    parser.add_argument(
        "--baud", type=int, default=9600,
        help="UART baud rate — must match E220 air-rate config",
    )
    parser.add_argument(
        "--rate", type=float, default=1.0,
        help="Packets per second (max safe rate depends on E220 air data rate: "
             "SF9≈2/s, SF10≈1/s, SF12≈0.3/s)",
    )
    parser.add_argument(
        "--count", type=int, default=0,
        help="Stop after N packets (0 = run forever)",
    )
    args = parser.parse_args()

    if args.rate <= 0:
        print("--rate must be > 0", file=sys.stderr)
        sys.exit(1)

    interval = 1.0 / args.rate

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1,
        )
    except serial.SerialException as exc:
        print(f"Cannot open {args.port}: {exc}", file=sys.stderr)
        sys.exit(1)

    print(f"Port  : {ser.name}  ({args.baud} baud)")
    print(f"Rate  : {args.rate} pkt/s  (every {interval*1000:.0f} ms)")
    print(f"Count : {'unlimited' if args.count == 0 else args.count}")
    print("─" * 60)
    print(f"{'#':>5}  {'ax(g)':>6} {'ay(g)':>6} {'az(g)':>6}  "
          f"{'gx(dps)':>8} {'gy(dps)':>8} {'gz(dps)':>8}  "
          f"{'spd(m/s)':>9} {'alt(m)':>7}")
    print("─" * 75)

    t_start = time.monotonic()
    seq = 0

    try:
        while True:
            t = time.monotonic() - t_start
            ax, ay, az, gx, gy, gz, speed, alt = _simulate(t)

            packet = f"{ax},{ay},{az},{gx},{gy},{gz},{speed},{alt}\n"
            ser.write(packet.encode("ascii"))
            ser.flush()   # push bytes into the kernel TX buffer immediately

            seq += 1
            print(f"{seq:5d}  {ax:6d} {ay:6d} {az:6d}  "
                  f"{gx:8d} {gy:8d} {gz:8d}  "
                  f"{speed:9d} {alt:7d}")

            if args.count and seq >= args.count:
                break

            # Minimum gap between packets must exceed:
            #   LoRa air time  (SF9/BW125 ~100ms, SF10 ~270ms, SF12 ~2500ms)
            # + UART output on receiver (50 bytes @ 9600 baud = ~52ms)
            # + E220 turnaround margin (~50ms)
            # At 500ms the E220 has time to finish TX, RX has time to drain
            # its UART buffer, and the module is ready for the next frame.
            next_send = t_start + seq * interval
            sleep_for = max(0.50, next_send - time.monotonic())
            time.sleep(sleep_for)

    except KeyboardInterrupt:
        print()
    finally:
        ser.close()

    print("─" * 60)
    print(f"Sent {seq} packet(s).")


if __name__ == "__main__":
    main()
