from picographics import PicoGraphics, PEN_RGB555, PEN_RGB888


def HVGA():
    return PicoGraphics(320, 240, PEN_RGB888)


def VGA():
    return PicoGraphics(640, 480, PEN_RGB555)


def Mode7Plus():
    return PicoGraphics(720, 400, PEN_RGB555)


def PAL():
    return PicoGraphics(720, 576, PEN_RGB555)


def NTSC():
    return PicoGraphics(720, 480, PEN_RGB555)


def WVGA():
    return PicoGraphics(800, 480, PEN_RGB555)
