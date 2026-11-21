"""Generates src/app.ico: a window frame with the blue->green glow and a pin dot."""
import sys
from PIL import Image, ImageDraw

S = 256
BLUE, GREEN = (0x1E, 0x8C, 0xFF), (0x00, 0xE6, 0x8C)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def draw(size):
    big = S * 4
    grad = Image.new("RGBA", (big, big))
    px = grad.load()
    for y in range(big):
        for x in range(big):
            t = (x + y) / (2 * big)
            px[x, y] = lerp(BLUE, GREEN, t) + (255,)

    m = big // 16
    outer = Image.new("L", (big, big), 0)
    ImageDraw.Draw(outer).rounded_rectangle([m, m, big - m, big - m], radius=big // 5, fill=255)
    th = big // 9
    inner = Image.new("L", (big, big), 0)
    ImageDraw.Draw(inner).rounded_rectangle([m + th, m + th, big - m - th, big - m - th],
                                            radius=big // 5 - th, fill=255)

    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    img.paste(grad, (0, 0), outer)                       # gradient frame
    dark = Image.new("RGBA", (big, big), (0x1F, 0x21, 0x27, 255))
    img.paste(dark, (0, 0), inner)                       # dark window
    d = ImageDraw.Draw(img)
    r = big // 7
    c = big // 2
    d.ellipse([c - r, c - r, c + r, c + r], fill=(255, 255, 255, 255))   # the pin
    return img.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "app.ico"
    base = draw(256)
    base.save(out, sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (256, 256)])
    base.save(out.replace(".ico", "_preview.png"))
