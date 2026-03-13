#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parent.parent
OUTPUT_DIR = ROOT / "assets" / "icons"
SIZES = (16, 24, 32, 48, 64, 128, 256, 512)

BODY_TOP = (43, 61, 51)
BODY_BOTTOM = (24, 33, 28)
INNER_BODY = (34, 48, 41)
BEZEL_TOP = (62, 82, 70)
BEZEL_BOTTOM = (39, 52, 44)
SCREEN_TOP = (215, 245, 149)
SCREEN_BOTTOM = (137, 191, 71)
GLYPH = (69, 107, 35, 220)
BAR = (92, 139, 49, 235)
STATUS = (223, 180, 79, 255)
ACCENT = (148, 210, 67, 255)
OUTLINE = (83, 105, 91, 255)
SHADOW = (11, 19, 14, 110)


def vertical_gradient(size: int, top, bottom) -> Image.Image:
    image = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(image)
    for y in range(size):
        t = y / max(size - 1, 1)
        color = tuple(
            round(top[index] * (1 - t) + bottom[index] * t) for index in range(3)
        ) + (255,)
        draw.line([(0, y), (size, y)], fill=color)
    return image


def paste_masked(base: Image.Image, layer: Image.Image, mask: Image.Image):
    masked = Image.new("RGBA", base.size, (0, 0, 0, 0))
    masked.paste(layer, (0, 0), mask)
    base.alpha_composite(masked)


def draw_icon(size: int) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    body_box = (
        round(size * 0.168),
        round(size * 0.113),
        round(size * 0.832),
        round(size * 0.902),
    )
    radius = round(size * 0.082)
    shadow_draw.rounded_rectangle(
        (body_box[0], body_box[1] + round(size * 0.027), body_box[2], body_box[3]),
        radius=radius,
        fill=SHADOW,
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=max(1, round(size * 0.031))))
    image.alpha_composite(shadow)

    body = vertical_gradient(size, BODY_TOP, BODY_BOTTOM)
    body_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(body_mask).rounded_rectangle(body_box, radius=radius, fill=255)
    paste_masked(image, body, body_mask)

    inner_body_overlay = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    inner_body_draw = ImageDraw.Draw(inner_body_overlay)
    inner_box = (
        round(size * 0.199),
        round(size * 0.145),
        round(size * 0.801),
        round(size * 0.871),
    )
    inner_body_draw.rounded_rectangle(
        inner_box,
        radius=round(size * 0.059),
        fill=INNER_BODY + (166,),
    )
    image.alpha_composite(inner_body_overlay)

    bezel = vertical_gradient(size, BEZEL_TOP, BEZEL_BOTTOM)
    bezel_box = (
        round(size * 0.234),
        round(size * 0.188),
        round(size * 0.766),
        round(size * 0.672),
    )
    bezel_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(bezel_mask).rounded_rectangle(
        bezel_box, radius=round(size * 0.055), fill=255
    )
    paste_masked(image, bezel, bezel_mask)

    screen = vertical_gradient(size, SCREEN_TOP, SCREEN_BOTTOM)
    screen_box = (
        round(size * 0.266),
        round(size * 0.219),
        round(size * 0.734),
        round(size * 0.641),
    )
    screen_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(screen_mask).rounded_rectangle(
        screen_box, radius=round(size * 0.039), fill=255
    )
    paste_masked(image, screen, screen_mask)

    glass = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glass_draw = ImageDraw.Draw(glass)
    glass_draw.rounded_rectangle(
        screen_box,
        radius=round(size * 0.039),
        fill=(255, 255, 255, 20),
    )
    image.alpha_composite(glass)

    draw = ImageDraw.Draw(image)
    screen_w = screen_box[2] - screen_box[0]
    screen_h = screen_box[3] - screen_box[1]
    glyph_rows = (
        ((0.083, 0.111), (0.233, 0.111), (0.383, 0.111), (0.533, 0.111), (0.683, 0.111), (0.833, 0.111)),
        ((0.116, 0.287), (0.332, 0.287), (0.466, 0.287), (0.666, 0.287), (0.832, 0.287)),
        ((0.083, 0.463), (0.216, 0.463), (0.35, 0.463), (0.625, 0.463), (0.8, 0.463)),
        ((0.108, 0.639), (0.283, 0.639), (0.55, 0.639), (0.733, 0.639), (0.883, 0.639)),
    )
    glyph_widths = (
        (0.116, 0.116, 0.116, 0.116, 0.116, 0.083),
        (0.183, 0.091, 0.166, 0.108, 0.15),
        (0.1, 0.1, 0.241, 0.1, 0.158),
        (0.141, 0.216, 0.075, 0.141, 0.125),
    )
    glyph_h = max(2, round(screen_h * 0.111))
    glyph_r = max(1, round(size * 0.012))
    for row_index, row in enumerate(glyph_rows):
        for glyph_index, (x_frac, y_frac) in enumerate(row):
            w_frac = glyph_widths[row_index][glyph_index]
            x0 = round(screen_box[0] + screen_w * x_frac)
            y0 = round(screen_box[1] + screen_h * y_frac)
            width = max(2, round(screen_w * w_frac))
            draw.rounded_rectangle(
                (x0, y0, x0 + width, y0 + glyph_h),
                radius=glyph_r,
                fill=GLYPH,
            )

    bar_bottom = round(screen_box[1] + screen_h * 0.88)
    bar_specs = (
        (0.083, 0.142, 0.065),
        (0.25, 0.142, 0.102),
        (0.417, 0.142, 0.148),
        (0.583, 0.142, 0.204),
        (0.75, 0.142, 0.259),
    )
    for x_frac, width_frac, height_frac in bar_specs:
        x0 = round(screen_box[0] + screen_w * x_frac)
        width = max(2, round(screen_w * width_frac))
        height = max(2, round(screen_h * height_frac))
        draw.rounded_rectangle(
            (x0, bar_bottom - height, x0 + width, bar_bottom),
            radius=max(1, round(size * 0.012)),
            fill=BAR,
        )

    button_y = round(size * 0.762)
    button_r = max(1, round(size * 0.021))
    for index in range(3):
        cx = round(size * (0.297 + index * 0.062))
        draw.ellipse(
            (cx - button_r, button_y - button_r, cx + button_r, button_y + button_r),
            fill=STATUS,
        )

    accent_r = max(1, round(size * 0.012))
    accent_x0 = round(size * 0.59)
    accent_y0 = round(size * 0.738)
    accent_w = round(size * 0.144)
    draw.rounded_rectangle(
        (accent_x0, accent_y0, accent_x0 + accent_w, accent_y0 + max(2, round(size * 0.024))),
        radius=accent_r,
        fill=ACCENT,
    )
    draw.rounded_rectangle(
        (
            round(size * 0.637),
            round(size * 0.781),
            round(size * 0.734),
            round(size * 0.805),
        ),
        radius=accent_r,
        fill=(ACCENT[0], ACCENT[1], ACCENT[2], 191),
    )

    draw.rounded_rectangle(
        (
            round(size * 0.238),
            round(size * 0.191),
            round(size * 0.762),
            round(size * 0.668),
        ),
        radius=round(size * 0.051),
        outline=OUTLINE,
        width=max(1, round(size * 0.008)),
    )

    return image


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for size in SIZES:
        draw_icon(size).save(OUTPUT_DIR / f"sure-smartie-gui-{size}.png")


if __name__ == "__main__":
    main()
