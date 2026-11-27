#!/usr/bin/env python3
"""Split a Tailwind USB terminal recording into chronologically tagged CSV files."""

import argparse
import csv
from contextlib import ExitStack
from dataclasses import dataclass
from pathlib import Path


SCHEMAS = {
    "B": ("boots", "boot_id,reset_reason,uptime_us"),
    "S": (
        "sensor",
        "boot_id,seq,uptime_us,sensor_ms,mode,pas_position,pedal_direction,pas_age_ms,"
        "torque_adc,torque_zero_adc,torque_nm,filtered_torque_nm,cadence_rpm,rider_w,"
        "assist_factor,assist_w,target_a,commanded_a,motor_enabled,speed_kmh,motor_rpm,"
        "speed_valid,command_published",
    ),
    "C": ("commands", "boot_id,seq,uptime_us,sensor_seq,sensor_ms,current_a,write_us"),
    "V": (
        "vesc",
        "boot_id,seq,uptime_us,query_us,reply,speed_valid,speed_kmh,erpm,motor_a,"
        "battery_a,battery_v,duty,fet_c,watt_hours,watt_hours_charged,fault",
    ),
}


@dataclass
class Stream:
    first_us: int
    last_us: int
    last_seq: int
    count: int = 1
    missing: int = 0

    def add(self, seq: int, uptime_us: int) -> None:
        if seq <= self.last_seq or uptime_us < self.last_us:
            raise ValueError("out-of-order sample")
        self.missing += seq - self.last_seq - 1
        self.last_seq = seq
        self.last_us = uptime_us
        self.count += 1


def convert(source: Path, output: Path) -> None:
    stats = {}
    reset_reasons = {}
    invalid = 0
    with ExitStack() as stack:
        raw = stack.enter_context(source.open(encoding="utf-8-sig", errors="replace"))
        output.mkdir(parents=True)
        writers = {}
        for kind, (name, columns) in SCHEMAS.items():
            target = stack.enter_context((output / f"{name}.csv").open("x", newline="", encoding="utf-8"))
            writer = csv.writer(target)
            writer.writerow(columns.split(","))
            writers[kind] = (writer, columns.count(",") + 1)

        for line in raw:
            if not line.startswith("TW1,"):
                continue
            _, kind, *row = line.rstrip("\r\n").split(",")
            try:
                writer, width = writers[kind]
                if len(row) != width:
                    raise ValueError("wrong number of columns")
                boot, seq_or_reason, timestamp = map(int, row[:3])
                if min(boot, seq_or_reason, timestamp) < 0:
                    raise ValueError("invalid timestamp or identifier")
                if kind == "B":
                    reset_reasons[boot] = seq_or_reason
                else:
                    key = (boot, kind)
                    if key in stats:
                        stats[key].add(seq_or_reason, timestamp)
                    else:
                        stats[key] = Stream(timestamp, timestamp, seq_or_reason)
                if kind == "V" and row[4] == "0":
                    row[6:] = [""] * (width - 6)
                elif kind == "V" and row[5] == "0":
                    row[6] = ""
            except (KeyError, ValueError):
                invalid += 1
                continue
            writer.writerow(row)

    if not stats:
        raise ValueError(f"no valid Tailwind samples ({invalid} malformed lines)")
    for (boot, kind), stream in stats.items():
        seconds = (stream.last_us - stream.first_us) / 1_000_000
        rate = f", {(stream.count - 1) / seconds:.1f} Hz" if seconds > 0 else ""
        print(f"Boot {boot} (reset reason {reset_reasons.get(boot, 'unknown')}) "
              f"{SCHEMAS[kind][0]}: {stream.count} rows{rate}, {stream.missing} skipped sequence numbers")
    print(f"Wrote {output} ({invalid} malformed or out-of-order lines skipped)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("recording", type=Path)
    parser.add_argument("--output", type=Path, help="new directory for the CSV files")
    args = parser.parse_args()
    output = args.output or args.recording.with_name(args.recording.stem + "-csv")
    try:
        convert(args.recording, output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
