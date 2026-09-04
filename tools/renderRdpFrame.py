#!/usr/bin/python3
import sys
import json
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Rectangle

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: renderRdpFrame.py <file>")
        sys.exit(1)

    fig, ax = plt.subplots()
    EXTENT = 512
    ax.set_xlim(-EXTENT + 160, EXTENT + 160)
    ax.set_ylim(-EXTENT + 120, EXTENT + 120)
    ax.set_aspect("equal")

    ax.add_patch(Rectangle((0, 0), 320, 240, fill=False, linewidth=2, edgecolor="red"))
    
    file = sys.argv[1]
    with open(file, "rb") as f:
        for textLine in f.readlines():
            log = json.loads(textLine)
            if log.get("command") == "SYNC_FULL":
                break
            if log.get("op") != "draw":
                continue

            coords = log.get("coords")[1:-1].split("), (")

            if log.get("type") == "rectangle":
                x0, y0 = map(float, coords[0].split(", "))
                x1, y1 = map(float, coords[1].split(", "))
                ax.add_patch(Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor=(0, 0, 0, 0.25)))
                pass
            elif log.get("type") == "triangle":
                x0, y0 = map(float, coords[0].split(", "))
                x1, y1 = map(float, coords[1].split(", "))
                x2, y2 = map(float, coords[2].split(", "))
                ax.add_patch(Polygon([[x0, y0], [x1, y1], [x2, y2]], facecolor=(0, 0, 0, 0.25)))

        plt.show()