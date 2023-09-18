from picovision import PicoVision, PEN_RGB555, PEN_RGB888


def HVGA():
    return PicoVision(PEN_RGB888, 320, 240)


def VGA():
    return PicoVision(PEN_RGB555, 640, 480)


def Mode7Plus():
    return PicoVision(PEN_RGB555, 720, 400)


def PAL():
    return PicoVision(PEN_RGB555, 720, 576)


def NTSC():
    return PicoVision(PEN_RGB555, 720, 480)


def WVGA():
    return PicoVision(PEN_RGB555, 800, 480)
