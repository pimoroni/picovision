# Demo showing how to calculate and plot the Mandelbrot set fractal, and display it on a PicoVision

from picovision import PicoVision, PEN_RGB555

# the two main factors controling how quickly this renders are the number of points to calculate, and the number of iterations when checking for divergence.
# To speed it up, choose a lower resolution for fewer pixels (and so fewer calculations) and fewer iterations for a less precice plot.
# 640*480 @ 300 iterations gives a good render, but will take ~15 minutes to complete.
# 320*240 @ 50 iterations is a lot faster, but much less detailed

display = PicoVision(PEN_RGB555, 640, 480)
ITERATIONS = 300

WIDTH, HEIGHT = display.get_bounds()
# a plot centered on (-0.3, 0) with a distance of 1.3 will give a good view of the entire set, but it's a fractal so it's all fun to look at. 
# Remember that the more you zoom in on the boundary, the more iterations you'll need to resolve the detail.
PLOT_CENTRE = (-0.3, 0)
PLOT_DISTANCE_H = 1.3


PLOT_DISTANCE_W = PLOT_DISTANCE_H * WIDTH / HEIGHT  # maintain aspect ratio

# work out the limits of the plot, based on our centre point and distance
PLOT_LIMITS = (
    PLOT_CENTRE[0] - PLOT_DISTANCE_W,  # r min 
    PLOT_CENTRE[0] + PLOT_DISTANCE_W,  # r max 
    PLOT_CENTRE[1] - PLOT_DISTANCE_H,  # i min
    PLOT_CENTRE[1] + PLOT_DISTANCE_H,  # i max
)

R_PIX = (PLOT_LIMITS[1] - PLOT_LIMITS[0]) / WIDTH  # calculate distance on complex plane per plotted pixel
I_PIX = (PLOT_LIMITS[3] - PLOT_LIMITS[2]) / HEIGHT

BLACK = display.create_pen(0, 0, 0)


def mandel(fr, fi):
    fc = complex(fr, fi)  # take a point called c on the complex plane
    fz = 0
    for j in range(ITERATIONS):
        fz = (
            fz**2 + fc
        )  # and let z2 = z1 ** 2 + c, and let z3 = z2 ** 2 + c, and so on
        if ((fc.real - fz.real) ** 2 + (fc.imag - fz.imag) ** 2) > 4:
            return j
    return 0  # this series of z has always stayed close to c and never trends away, so this point is in the Mandelbrot set.


def paintcol(fx, col, fi):  # paint a saved column to the display
    for fy, p in enumerate(col):
        if p == 0:
            display.set_pen(
                BLACK
            )  # black for when a point didn't diverge and falls within the set
        else:
            display.set_pen(
                display.create_pen_hsv(float(p / fi), 1, 1)
            )  # if a point diverges, select a colour based on how many iterations before it diverged
        display.pixel(fx, fy)
    display.update()


r = PLOT_LIMITS[0]

for x in range(WIDTH):
    i = PLOT_LIMITS[2]
    plot_col = []  # clear the list so we can start saving a new column
    for y in range(HEIGHT):  # for each pixel in the column
        plot_col.append(
            mandel(r, i)
        )  # check if point falls within the set (0), or if not how many iterations we went through before the series diverged. Then save this to our list
        i += I_PIX  # next pixel
    paintcol(x, plot_col, ITERATIONS)  # paint the column to the display
    paintcol(x, plot_col, ITERATIONS)  # double up for both frame buffers
    r += R_PIX  # next column
