#!/usr/bin/python3
import sys
import json
import matplotlib.pyplot as plt
from matplotlib.widgets import Button
from matplotlib.patches import Polygon, Rectangle

def pointFromLog(logStr: str):
    point = list(map(float, logStr[1:-1].split(", ")))
    point[1] = 240 - point[1]
    return point

class PlotNavigator:
    def __init__(self, fileName: str):
        self.file = open(fileName, "rb")
        self.frame = 0
        self.primColour = [0, 0, 0, 1]
        self.isPlaying = False
        self.fig, self.ax = plt.subplots()

        self.frames = self.loadFrames()
        plt.subplots_adjust(bottom=0.2)
        self.fig.set_size_inches(15, 10)

        EXTENT_X = 400
        EXTENT_Y = 300
        self.ax.set_xlim(-EXTENT_X + 160, EXTENT_X + 160)
        self.ax.set_ylim(-EXTENT_Y + 120, EXTENT_Y + 120)

        self.animation = self.fig.canvas.new_timer(interval=50)
        self.animation.add_callback(self.animate)
        self.update()

        buttons = [
            (0.35, 'Reset', self.resetFrame),
            (0.45, 'Previous', self.previousFrame),
            (0.55, 'Play', self.toggleAnimation),
            (0.65, 'Next', self.nextFrame),
        ]
        self.buttons = {}
        for position, label, callback in buttons:
            axis = self.fig.add_axes([position, 0.05, 0.075, 0.075])
            button = Button(axis, label)
            button.on_clicked(callback)
            self.buttons[label] = button

    def loadFrames(self):
        frames = [[]]
        for textLine in self.file:
            try:
                log = json.loads(textLine)
            except json.JSONDecodeError:
                print("Malformed JSON:", textLine)
                break

            if log.get("command") == "SYNC_FULL":
                frames.append([])
                continue
            
            if log.get("op") == "setColour":
                self.primColour = list(map(float, log.get("colour")[1:-1].split(", ")))
                continue

            if log.get("op") == "draw":
                coords = log.get("coords")[1:-1].split("), (")
                if log.get("type") == "rectangle":
                    x0, y0 = pointFromLog(coords[0])
                    x1, y1 = pointFromLog(coords[1])
                    patch = Rectangle((x0, y0), x1 - x0, y1 - y0, facecolor=self.primColour)
                elif log.get("type") == "triangle":
                    points = [pointFromLog(coord) for coord in coords]
                    patch = Polygon(points, facecolor=self.primColour)
                else:
                    continue
                frames[-1].append(patch)
                continue

        self.file.close()
        return frames

    def update(self):
        xlim = self.ax.get_xlim()
        ylim = self.ax.get_ylim()
        self.ax.clear()
        self.ax.set_xlim(xlim)
        self.ax.set_ylim(ylim)
        
        self.ax.set_aspect("equal")
        self.ax.add_patch(Rectangle((0, 0), 320, 240, fill=False, linewidth=2, edgecolor="red"))
        for patch in self.frames[self.frame]:
            self.ax.add_patch(patch)

        self.fig.canvas.draw()

    def previousFrame(self, event):
        self.frame = max(0, self.frame - 1)
        self.update()
        plt.draw()

    def nextFrame(self, event):
        self.frame = min(len(self.frames) - 1, self.frame + 1)
        self.update()
        plt.draw()

    def resetFrame(self, event):
        self.frame = 0
        self.update()
        plt.draw()

    def endFrame(self, event):
        self.frame = len(self.frames) - 1
        self.update()
        plt.draw()

    def toggleAnimation(self, event):
        self.isPlaying = not self.isPlaying
        if self.isPlaying:
            self.buttons['Play'].label.set_text('Stop')
            self.animation.start()
        else:
            self.buttons['Play'].label.set_text('Play')
            self.animation.stop()
        self.fig.canvas.draw_idle()

    def animate(self):
        if self.frame >= len(self.frames) - 1:
            self.isPlaying = False
            self.buttons['Play'].label.set_text('Play')
            self.animation.stop()
            self.fig.canvas.draw_idle()
            return

        self.frame += 1
        self.update()

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: renderRdpFrame.py <file>")
        sys.exit(1)
    
    file = sys.argv[1]
    plotNav = PlotNavigator(file)
    plt.show()