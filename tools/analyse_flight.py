#!/usr/bin/env python3
"""Simple post-flight analysis for reconstructed logger CSV output.

This script was created in 2026 as portfolio documentation. It is not
claimed to be the original analysis code used during the 2025 BIP.
"""

from pathlib import Path
import argparse

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=Path, help="flight.csv exported from the logger")
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    required = {"time_ms", "relative_altitude_m", "ax", "ay", "az"}
    missing = required - set(df.columns)
    if missing:
        raise SystemExit(f"Missing columns: {sorted(missing)}")

    t = df["time_ms"].to_numpy(dtype=float) / 1000.0
    altitude = df["relative_altitude_m"].to_numpy(dtype=float)

    # Numerical derivative of barometric altitude.
    # This is intentionally labelled an estimate: barometric altitude is noisy.
    velocity = np.gradient(altitude, t)

    ax = df["ax"].to_numpy(dtype=float)
    ay = df["ay"].to_numpy(dtype=float)
    az = df["az"].to_numpy(dtype=float)
    accel_mag = np.sqrt(ax * ax + ay * ay + az * az)

    out = df.copy()
    out["estimated_velocity_m_s"] = velocity
    out["accel_magnitude"] = accel_mag

    output_csv = args.csv.with_name(args.csv.stem + "_analysed.csv")
    out.to_csv(output_csv, index=False)
    print(f"Wrote {output_csv}")

    plt.figure()
    plt.plot(t, altitude)
    plt.xlabel("Time (s)")
    plt.ylabel("Relative altitude (m)")
    plt.title("Rocket altitude")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

    plt.figure()
    plt.plot(t, velocity)
    plt.xlabel("Time (s)")
    plt.ylabel("Estimated velocity (m/s)")
    plt.title("Velocity estimated from barometric altitude")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

    plt.figure()
    plt.plot(t, accel_mag)
    plt.xlabel("Time (s)")
    plt.ylabel("Acceleration magnitude")
    plt.title("IMU acceleration magnitude")
    plt.grid(True)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
