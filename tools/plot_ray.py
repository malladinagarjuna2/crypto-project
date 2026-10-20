"""Plot a ray path exported by `refcrypt trace` (CSV) in 3D.

Usage:  python tools/plot_ray.py ray.csv [out.png]
"""
import csv
import sys

import matplotlib.pyplot as plt

COLORS = {"start": "green", "refract": "black", "tir": "red", "wall": "orange"}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    with open(sys.argv[1], newline="") as f:
        rows = list(csv.DictReader(f))

    xs = [float(r["x"]) for r in rows]
    ys = [float(r["y"]) for r in rows]
    zs = [float(r["z"]) for r in rows]

    fig = plt.figure(figsize=(8, 7))
    ax = fig.add_subplot(projection="3d")
    ax.plot(xs, ys, zs, color="royalblue", linewidth=1.5)
    for event, color in COLORS.items():
        pts = [(x, y, z) for x, y, z, r in zip(xs, ys, zs, rows) if r["event"] == event]
        if pts:
            px, py, pz = zip(*pts)
            ax.scatter(px, py, pz, color=color, s=18, label=f"{event} ({len(pts)})")
    ax.set_xlim(0, 16); ax.set_ylim(0, 16); ax.set_zlim(0, 16)
    ax.set_xlabel("x (cells)"); ax.set_ylabel("y (cells)"); ax.set_zlabel("z (cells)")
    ax.set_title("Ray path through the secret 16x16x16 crystal")
    ax.legend()
    if len(sys.argv) > 2:
        fig.savefig(sys.argv[2], dpi=120)
        print("saved", sys.argv[2])
    else:
        plt.show()


if __name__ == "__main__":
    main()

# =====================================================================
# EXPLANATION (in simple words)
# ---------------------------------------------------------------------
# This Python script draws the path of one ray inside the crystal.
#
#  1. It reads the CSV file made by "refcrypt trace" (one row per event:
#     the step number, the event type and the x, y, z position).
#  2. It draws a blue line through all positions in a 3D chart.
#  3. It puts coloured dots on the events: green = entry point,
#     black = refraction, red = total internal reflection, orange = the
#     ray bounced off the outer wall.
#  4. If you give a second file name it saves a PNG image, otherwise
#     it opens an interactive window you can rotate with the mouse.
#
# It needs matplotlib:  pip install matplotlib
# =====================================================================
