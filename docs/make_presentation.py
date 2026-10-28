"""Builds docs/presentation.pptx, the project presentation deck.

Usage:  python docs/make_presentation.py
Needs:  pip install python-pptx
"""
from pathlib import Path

from pptx import Presentation
from pptx.chart.data import CategoryChartData
from pptx.dml.color import RGBColor
from pptx.enum.chart import XL_CHART_TYPE, XL_LEGEND_POSITION, XL_MARKER_STYLE
from pptx.enum.dml import MSO_LINE_DASH_STYLE
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import MSO_ANCHOR, PP_ALIGN
from pptx.util import Emu, Inches, Pt

DOCS = Path(__file__).resolve().parent
IMG = DOCS / "images"
OUT = DOCS / "presentation.pptx"

# ---- theme (light) ----
SURFACE = RGBColor(0xFC, 0xFC, 0xFB)
INK = RGBColor(0x0B, 0x0B, 0x0B)
INK2 = RGBColor(0x52, 0x51, 0x4E)
MUTED = RGBColor(0x8A, 0x89, 0x85)
ACCENT = RGBColor(0x2A, 0x78, 0xD6)        # categorical slot 1 (validated)
ACCENT_SOFT = RGBColor(0xE6, 0xEF, 0xFA)
BAD = RGBColor(0xC2, 0x2F, 0x2E)
GOOD = RGBColor(0x00, 0x6E, 0x00)
GRID = RGBColor(0xE2, 0xE1, 0xDD)
NAVY = RGBColor(0x12, 0x24, 0x3D)
FONT = "Segoe UI"
MONO = "Consolas"

W, H = Inches(13.333), Inches(7.5)
MARGIN = Inches(0.6)


# ============================================================================
# helpers
# ============================================================================
def new_deck():
    prs = Presentation()
    prs.slide_width, prs.slide_height = W, H
    return prs


def blank(prs, notes=""):
    s = prs.slides.add_slide(prs.slide_layouts[6])
    bg = s.background.fill
    bg.solid()
    bg.fore_color.rgb = SURFACE
    if notes:
        s.notes_slide.notes_text_frame.text = notes
    return s


def text(slide, x, y, w, h, content, size=18, color=INK, bold=False, font=FONT,
         align=PP_ALIGN.LEFT, anchor=MSO_ANCHOR.TOP):
    """content: str or list of str / (str, dict) runs as paragraphs."""
    tb = slide.shapes.add_textbox(x, y, w, h)
    tf = tb.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = anchor
    tf.margin_left = tf.margin_right = Inches(0.05)
    paras = content if isinstance(content, list) else [content]
    for i, p in enumerate(paras):
        para = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        para.alignment = align
        opts = {}
        if isinstance(p, tuple):
            p, opts = p
        run = para.add_run()
        run.text = p
        f = run.font
        f.name = opts.get("font", font)
        f.size = Pt(opts.get("size", size))
        f.bold = opts.get("bold", bold)
        f.color.rgb = opts.get("color", color)
        para.space_after = Pt(opts.get("after", 6))
    return tb


def bullets(slide, x, y, w, h, items, size=18):
    tb = slide.shapes.add_textbox(x, y, w, h)
    tf = tb.text_frame
    tf.word_wrap = True
    for i, item in enumerate(items):
        sub = item.startswith("  ")
        para = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        para.space_after = Pt(8 if not sub else 4)
        r1 = para.add_run()
        r1.text = ("–  " if sub else "•  ")
        r1.font.color.rgb = ACCENT
        r1.font.size = Pt(size - (3 if sub else 0))
        r1.font.name = FONT
        body = item.strip()
        # **bold lead** support: "**Lead:** rest"
        if body.startswith("**") and "**" in body[2:]:
            lead, rest = body[2:].split("**", 1)
            r = para.add_run(); r.text = lead
            r.font.bold = True; r.font.size = Pt(size - (3 if sub else 0)); r.font.name = FONT
            r.font.color.rgb = INK
            body = rest
        r = para.add_run(); r.text = body
        r.font.size = Pt(size - (3 if sub else 0)); r.font.name = FONT
        r.font.color.rgb = INK2 if sub else INK
        if sub:
            para.level = 1
    return tb


def header(slide, title, kicker=None):
    bar = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, Inches(0.18), H)
    bar.fill.solid(); bar.fill.fore_color.rgb = ACCENT; bar.line.fill.background()
    if kicker:
        text(slide, MARGIN, Inches(0.35), Inches(11), Inches(0.4), kicker.upper(),
             size=12, color=ACCENT, bold=True)
    text(slide, MARGIN, Inches(0.62), Inches(12), Inches(0.9), title, size=30, bold=True, color=NAVY)


def footer(slide, n):
    text(slide, MARGIN, H - Inches(0.45), Inches(8), Inches(0.3),
         "3D Chaotic Refractive Cipher  ·  Malladi Nagarjuna", size=10, color=MUTED)
    text(slide, W - Inches(1.4), H - Inches(0.45), Inches(0.9), Inches(0.3), str(n),
         size=10, color=MUTED, align=PP_ALIGN.RIGHT)


def box(slide, x, y, w, h, label, fill=ACCENT_SOFT, color=NAVY, size=14, bold=True, line=None,
        shape=MSO_SHAPE.ROUNDED_RECTANGLE, font=FONT, sub_color=INK2):
    sh = slide.shapes.add_shape(shape, x, y, w, h)
    sh.fill.solid(); sh.fill.fore_color.rgb = fill
    if line is None:
        sh.line.fill.background()
    else:
        sh.line.color.rgb = line; sh.line.width = Pt(1.25)
    if shape == MSO_SHAPE.ROUNDED_RECTANGLE:
        sh.adjustments[0] = 0.12
    tf = sh.text_frame
    tf.word_wrap = True
    tf.vertical_anchor = MSO_ANCHOR.MIDDLE
    tf.margin_left = tf.margin_right = Inches(0.08)
    lines = label.split("\n")
    for i, ln in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = PP_ALIGN.CENTER
        r = p.add_run(); r.text = ln
        r.font.size = Pt(size if i == 0 else size - 3)
        r.font.bold = bold if i == 0 else False
        r.font.color.rgb = color if i == 0 else sub_color
        r.font.name = font
    return sh


def arrow(slide, x, y, w=Inches(0.45), h=Inches(0.32)):
    a = slide.shapes.add_shape(MSO_SHAPE.RIGHT_ARROW, x, y, w, h)
    a.fill.solid(); a.fill.fore_color.rgb = MUTED; a.line.fill.background()
    return a


def down_arrow(slide, x, y, w=Inches(0.32), h=Inches(0.4)):
    a = slide.shapes.add_shape(MSO_SHAPE.DOWN_ARROW, x, y, w, h)
    a.fill.solid(); a.fill.fore_color.rgb = MUTED; a.line.fill.background()
    return a


def image(slide, path, x, y, w=None, h=None, border=True):
    pic = slide.shapes.add_picture(str(path), x, y, width=w, height=h)
    if border:
        pic.line.color.rgb = GRID; pic.line.width = Pt(1)
    return pic


def table(slide, x, y, w, rows, col_w, size=13, row_h=Inches(0.42), header_fill=NAVY,
          highlight=None):
    shape = slide.shapes.add_table(len(rows), len(rows[0]), x, y, w, row_h * len(rows))
    tbl = shape.table
    for j, cw in enumerate(col_w):
        tbl.columns[j].width = cw
    for i, row in enumerate(rows):
        for j, val in enumerate(row):
            cell = tbl.cell(i, j)
            cell.fill.solid()
            cell.fill.fore_color.rgb = header_fill if i == 0 else (
                ACCENT_SOFT if highlight and highlight(i, j) else (SURFACE if i % 2 else RGBColor(0xF3, 0xF2, 0xEF)))
            cell.margin_left = cell.margin_right = Inches(0.08)
            cell.vertical_anchor = MSO_ANCHOR.MIDDLE
            tf = cell.text_frame
            tf.word_wrap = True
            p = tf.paragraphs[0]
            r = p.add_run(); r.text = str(val)
            r.font.size = Pt(size); r.font.name = FONT
            r.font.bold = (i == 0) or j == 0
            r.font.color.rgb = RGBColor(0xFF, 0xFF, 0xFF) if i == 0 else INK
    return tbl


def style_axis(axis, size=12):
    axis.tick_labels.font.size = Pt(size)
    axis.tick_labels.font.color.rgb = INK2
    axis.tick_labels.font.name = FONT
    axis.format.line.color.rgb = GRID


# ============================================================================
# slides
# ============================================================================
def build():
    prs = new_deck()
    n = 0

    # 1 ---- title -----------------------------------------------------------
    s = blank(prs, notes=(
        "Hello everyone. My project is an encryption algorithm that uses the physics of light: "
        "refraction and total internal reflection inside a secret 3D crystal. I built it in C, "
        "tested it, attacked it myself, and made a simulation GUI to show it working."))
    band = s.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, W, H)
    band.fill.solid(); band.fill.fore_color.rgb = NAVY; band.line.fill.background()
    image(s, IMG / "gui-ray-simulation.png", Inches(7.0), Inches(1.2), w=Inches(5.9), border=False)
    text(s, MARGIN, Inches(1.6), Inches(6.2), Inches(0.4), "CRYPTOGRAPHY PROJECT",
         size=14, color=RGBColor(0x8F, 0xBF, 0xF5), bold=True)
    text(s, MARGIN, Inches(2.0), Inches(6.3), Inches(2.0), "3D Chaotic Refractive Cipher",
         size=44, bold=True, color=RGBColor(0xFF, 0xFF, 0xFF))
    text(s, MARGIN, Inches(3.75), Inches(6.2), Inches(1.2),
         "Encrypting data with light bending through a secret crystal",
         size=22, color=RGBColor(0xD7, 0xE3, 0xF2))
    text(s, MARGIN, Inches(5.4), Inches(6), Inches(0.9),
         [("Malladi Nagarjuna", {"size": 20, "bold": True, "color": RGBColor(0xFF, 0xFF, 0xFF)}),
          ("C11  ·  Python simulation GUI", {"size": 13, "color": RGBColor(0xB8, 0xC7, 0xDA), "after": 2}),
          ("github.com/malladinagarjuna2/crypto-project", {"size": 13, "color": RGBColor(0xB8, 0xC7, 0xDA)})])

    # 2 ---- the idea ----------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Imagine a glass cube made of 4096 small blocks, each bending light differently. The key decides "
        "the pattern. Data decides where and at what angle a laser enters. Without the key you cannot "
        "predict where it exits. That unpredictability is turned into a cipher."))
    header(s, "The idea: hide data in the path of light", "Motivation")
    bullets(s, MARGIN, Inches(1.7), Inches(6.2), Inches(4.8), [
        "**Secret crystal:** 16×16×16 = 4,096 glass cubes, each with its own refractive index n (1.0 = air … 2.417 = diamond)",
        "**The key builds the crystal:** the same key always gives the same crystal; a different key gives a completely different one",
        "**Data steers a light ray:** entry point + angle come from the data",
        "**Physics scrambles it:** the ray bends (Snell's law) or bounces back (total internal reflection) at every cube face",
        "**Exit state → random-looking bits** that encrypt the message",
    ], size=17)
    image(s, IMG / "gui-crystal.png", Inches(7.2), Inches(1.75), w=Inches(5.6))
    text(s, Inches(7.2), Inches(5.4), Inches(5.6), Inches(0.5),
         "One depth slice of a real key-generated crystal (from the GUI)", size=11, color=MUTED)
    footer(s, n)

    # 3 ---- original plan problems ----------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "The first plan looked impressive but had real flaws. The biggest: encrypting one byte per ray is "
        "just a 256-entry lookup table, so 256 chosen bytes break it completely. My attack suite later "
        "proved this: 100% of a secret message recovered. Every fix on the right is in the final design."))
    header(s, "Reviewing the first plan: 6 key flaws, 6 fixes", "Design review")
    rows = [["Original idea", "Problem", "Fix in final design"],
            ["Encrypt 1 byte per ray", "Only 256 inputs → a lookup table breaks it", "128-bit blocks in a Feistel network"],
            ["Decrypt by tracing backwards", "Fixed-point rounding makes the path drift", "Feistel: F only ever runs forwards"],
            ["XOR with previous ciphertext", "Attacker can undo it; no IV", "CTR mode + random 96-bit nonce"],
            ["No integrity check", "Bits can be flipped silently", "Encrypt-then-MAC (HMAC-SHA256)"],
            ["Q16.16 range 100000–241700", "That's decimal scaling, not Q16.16", "65536–158401 (= 1.0–2.417)"],
            ["Security from chaos alone", "Fixed-point chaos falls into cycles", "Chaos ⊕ HMAC stream per cell"]]
    table(s, MARGIN, Inches(1.75), Inches(12.1), rows,
          [Inches(3.3), Inches(4.5), Inches(4.3)], size=14, row_h=Inches(0.62))
    footer(s, n)

    # 4 ---- architecture -------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Here is the whole pipeline. The 256-bit key goes through HKDF to produce four independent "
        "subkeys. The crystal and S-box feed the ray tracer, which is the round function F of a "
        "16-round Feistel cipher. CTR mode turns it into a stream cipher for files of any size, and an "
        "HMAC tag detects tampering."))
    header(s, "Architecture", "How it works")
    y1 = Inches(1.9)
    box(s, MARGIN, y1, Inches(1.9), Inches(1.0), "256-bit key\nsecret", fill=NAVY, color=RGBColor(0xFF, 0xFF, 0xFF),
        sub_color=RGBColor(0xB8, 0xC7, 0xDA))
    arrow(s, Inches(2.6), y1 + Inches(0.34))
    box(s, Inches(3.15), y1, Inches(1.9), Inches(1.0), "HKDF-SHA256\nkey derivation")
    arrow(s, Inches(5.15), y1 + Inches(0.34))
    subs = [("Crystal", "chaos ⊕ HMAC"), ("S-box", "key shuffle"), ("16 round keys", "64-bit each"), ("MAC key", "256-bit")]
    for i, (a, b) in enumerate(subs):
        box(s, Inches(5.7 + i * 1.85), y1, Inches(1.7), Inches(1.0), f"{a}\n{b}", fill=RGBColor(0xF3, 0xF2, 0xEF), line=GRID)
    y2 = Inches(3.75)
    down_arrow(s, Inches(7.4), Inches(3.05))
    box(s, Inches(5.7), y2, Inches(3.6), Inches(1.0), "Ray tracer = round function F\n48 refraction / TIR events + S-box")
    arrow(s, Inches(9.4), y2 + Inches(0.34))
    box(s, Inches(9.95), y2, Inches(2.75), Inches(1.0), "16-round Feistel\n128-bit block cipher")
    y3 = Inches(5.45)
    down_arrow(s, Inches(11.15), Inches(4.9))
    box(s, Inches(5.7), y3, Inches(3.6), Inches(0.95), "HMAC-SHA256 tag\ntamper detection", fill=RGBColor(0xE6, 0xF4, 0xE6))
    arrow_l = s.shapes.add_shape(MSO_SHAPE.LEFT_ARROW, Inches(9.4), y3 + Inches(0.32), Inches(0.45), Inches(0.32))
    arrow_l.fill.solid(); arrow_l.fill.fore_color.rgb = MUTED; arrow_l.line.fill.background()
    box(s, Inches(9.95), y3, Inches(2.75), Inches(0.95), "CTR mode\nnonce ‖ counter → keystream")
    text(s, MARGIN, Inches(3.75), Inches(4.8), Inches(2.6), [
        ("Encrypted file", {"size": 16, "bold": True, "color": NAVY}),
        ("\"RFC1\" ‖ nonce (12 B) ‖ ciphertext ‖ tag (32 B)", {"size": 14, "font": MONO, "color": INK2}),
        ("Always exactly 48 bytes bigger than the original.", {"size": 14, "color": INK2}),
    ])
    footer(s, n)

    # 5 ---- crystal & key schedule ---------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Fixed-point is essential: floats round differently on different CPUs, and a cipher must be "
        "bit-exact everywhere. The chaotic map gives structure, but because chaos in finite precision "
        "repeats, I XOR in HMAC output so security never depends on the chaos."))
    header(s, "Growing the secret crystal", "Key schedule")
    bullets(s, MARGIN, Inches(1.7), Inches(6.4), Inches(4.9), [
        "**HKDF-SHA256** turns one key into independent subkeys (labels: grid, sbox, round, mac)",
        "**4D hyperchaotic Lorenz system**, integrated with fixed-point RK4",
        "  ẋ = a(y−x)+w,  ẏ = cx−y−xz,  ż = xy−bz,  ẇ = −yz+rw",
        "**Each cell:** chaos sample ⊕ HMAC stream → n ∈ [1.0, 2.417]",
        "**Q16.16 fixed-point:** value × 65536 stored as an integer",
        "  bit-identical results on every CPU (floats are not)",
        "**Key avalanche:** 1 key bit flipped → 100% of cells change",
    ], size=17)
    table(s, Inches(7.4), Inches(1.9), Inches(5.3), [
        ["Real value", "Q16.16 integer"], ["1.0", "65,536"], ["1.5", "98,304"],
        ["2.417 (diamond)", "158,401"], ["1.5 × 1.5", "(98304² ≫ 16) = 147,456 = 2.25"]],
        [Inches(2.0), Inches(3.3)], size=14, row_h=Inches(0.5))
    text(s, Inches(7.4), Inches(4.7), Inches(5.3), Inches(1.4), [
        ("Why not floats?", {"size": 15, "bold": True, "color": NAVY}),
        ("x86, ARM and different compilers round floats slightly differently. One bit off in the "
         "crystal = a file that can never be decrypted on another machine.", {"size": 14, "color": INK2})])
    footer(s, n)

    # 6 ---- physics ------------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "I use the vector form of Snell's law because it needs no sine or arcsine, only a square root, "
        "which I compute with integers. If k is negative the ray cannot pass: total internal reflection. "
        "Worked example: glass 1.5 to air 1.0 at 30 degrees exits at 48.6 degrees; at 50 degrees it reflects."))
    header(s, "The physics: Snell's law & total internal reflection", "Ray tracing")
    box(s, MARGIN, Inches(1.8), Inches(6.0), Inches(1.95), "", fill=RGBColor(0xF3, 0xF2, 0xEF))
    text(s, MARGIN + Inches(0.25), Inches(1.95), Inches(5.6), Inches(2.2), [
        ("r = n₁ / n₂     c₁ = −N·I     k = 1 − r²(1 − c₁²)", {"size": 17, "font": "Cambria Math"}),
        ("k ≥ 0  → refract:  R = r·I + (r·c₁ − √k)·N", {"size": 17, "font": "Cambria Math", "color": NAVY}),
        ("k < 0  → total internal reflection:  R = I + 2c₁·N", {"size": 17, "font": "Cambria Math", "color": BAD}),
        ("No sin/cos/arcsin needed, only an integer square root", {"size": 13, "color": INK2}),
    ])
    table(s, Inches(7.0), Inches(1.8), Inches(5.7), [
        ["Worked example", "n₁ → n₂", "Angle in", "Result"],
        ["Glass → air", "1.5 → 1.0", "30°", "refracts to 48.6°"],
        ["Air → glass", "1.0 → 1.5", "30°", "refracts to 19.5°"],
        ["Glass → air", "1.5 → 1.0", "50°", "TIR (θc = 41.8°)"]],
        [Inches(1.7), Inches(1.2), Inches(1.1), Inches(1.7)], size=13, row_h=Inches(0.5))
    bullets(s, MARGIN, Inches(4.2), Inches(12), Inches(2.2), [
        "**Unit tests check these exact numbers** in Q16.16 (to within ±0.002)",
        "**Branchless:** both outcomes are computed and one is selected with a bit mask, so there is no secret-dependent if",
        "**Fixed 48 events per ray:** running time doesn't depend on the data",
    ], size=16)
    footer(s, n)

    # 7 ---- ray tracer as F -----------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "This is the ray tracer running in my simulation GUI. 64 input bits choose the entry point and "
        "angle. The ray makes exactly 48 boundary events. Black dots are refractions, red crosses are "
        "total internal reflections, orange squares are wall bounces. At every step a secret S-box is "
        "mixed in, and the whole journey is compressed to 64 output bits."))
    header(s, "The ray tracer is the cipher's round function F", "Round function")
    image(s, IMG / "gui-ray-simulation.png", Inches(5.2), Inches(1.6), w=Inches(7.6))
    bullets(s, MARGIN, Inches(1.75), Inches(4.4), Inches(5), [
        "**Input (64 bits):**",
        "  20 + 20 bits → entry x, y",
        "  12 + 12 bits → tilt angle",
        "**48 events** via 3D voxel walk (DDA)",
        "**Secret S-box** mixed in at every step",
        "**Output:** 64-bit digest of position, direction and path history",
        "**1 input bit flipped → ~50%** of the 64 output bits change",
    ], size=16)
    footer(s, n)

    # 8 ---- Feistel ------------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Why Feistel? Tracing a fixed-point ray backwards does not work, because rounding errors make it "
        "drift. In a Feistel network decryption only needs F running forwards, so decryption is always "
        "exact. The chart shows one flipped input bit spreading to about half of all 128 bits within "
        "two rounds."))
    header(s, "Feistel network: why decryption is always exact", "Block cipher")
    box(s, MARGIN, Inches(1.8), Inches(5.2), Inches(1.35), "", fill=RGBColor(0xF3, 0xF2, 0xEF))
    text(s, MARGIN + Inches(0.2), Inches(1.9), Inches(4.9), Inches(1.2), [
        ("Encrypt:  Lᵢ₊₁ = Rᵢ      Rᵢ₊₁ = Lᵢ ⊕ F(Rᵢ, Kᵢ)", {"size": 16, "font": "Cambria Math"}),
        ("Decrypt:  Rᵢ = Lᵢ₊₁      Lᵢ = Rᵢ₊₁ ⊕ F(Lᵢ₊₁, Kᵢ)", {"size": 16, "font": "Cambria Math", "color": NAVY}),
    ])
    bullets(s, MARGIN, Inches(3.4), Inches(5.3), Inches(3.3), [
        "**F is only ever run forwards,** so it may round, branch, even collide",
        "**Backward ray tracing would drift** due to fixed-point rounding",
        "**16 rounds** on 128-bit blocks (DES-style structure; Luby–Rackoff)",
        "**Tested:** 200 random blocks plus all reduced-round versions decrypt exactly",
    ], size=16)
    image(s, IMG / "gui-feistel-rounds.png", Inches(6.2), Inches(1.6), w=Inches(6.6))
    footer(s, n)

    # 9 ---- CTR + HMAC ---------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "A block cipher alone always maps the same input to the same output. CTR mode with a fresh random "
        "nonce means the same file encrypts differently every time. The HMAC tag is checked before "
        "decryption: flip a single bit anywhere and the file is rejected and nothing is output."))
    header(s, "Encrypting real files: CTR mode + HMAC", "Mode of operation")
    segs = [("RFC1", "4 B", RGBColor(0x6A, 0x3E, 0xC0), 1.2), ("nonce", "12 B random", RGBColor(0xD9, 0x6A, 0x1E), 2.0),
            ("ciphertext", "same size as message", ACCENT, 5.2), ("HMAC tag", "32 B", RGBColor(0x1E, 0x7F, 0x4E), 2.4)]
    x = MARGIN
    for name, sub, col, wid in segs:
        box(s, x, Inches(1.85), Inches(wid), Inches(0.95), f"{name}\n{sub}", fill=col,
            color=RGBColor(0xFF, 0xFF, 0xFF), shape=MSO_SHAPE.RECTANGLE,
            sub_color=RGBColor(0xF0, 0xF0, 0xF0))
        x += Inches(wid) + Inches(0.06)
    bullets(s, MARGIN, Inches(3.15), Inches(6.2), Inches(3.5), [
        "**CTR:** keystreamᵢ = Feistel(nonce ‖ counterᵢ), then ciphertext = message ⊕ keystream",
        "**New random nonce every time:** same file → ~50% of bits differ",
        "**Encrypt-then-MAC:** the tag covers magic + nonce + ciphertext",
        "**Tag checked first,** in constant time: tampered or wrong key → nothing is decrypted",
    ], size=16)
    image(s, IMG / "gui-encrypt-decrypt.png", Inches(7.1), Inches(3.05), w=Inches(5.7))
    footer(s, n)

    # 10 ---- testing -----------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "I tested at three levels. Unit tests check every building block against official test vectors and "
        "the physics against worked examples. CLI tests run the real program on real files. And a separate "
        "attack program tries to break the cipher. All passed."))
    header(s, "Testing at three levels: everything passes", "Verification")
    cards = [("67 / 67", "unit tests",
              "SHA-256, HMAC, HKDF official vectors · fixed-point maths · Snell/TIR physics · round trips · tamper rejection at every byte"),
             ("19 / 19", "end-to-end CLI checks",
              "empty / text / 200 KB files · +48-byte overhead · fresh nonce · wrong key & tampered file rejected"),
             ("All PASS", "automated attack suite",
              "avalanche matrix · linearity / impulse · codebook attack · entropy, χ², correlation, NIST monobit & runs")]
    for i, (big, label, desc) in enumerate(cards):
        x = MARGIN + Inches(i * 4.1)
        box(s, x, Inches(1.9), Inches(3.8), Inches(3.3), "", fill=RGBColor(0xF3, 0xF2, 0xEF))
        text(s, x + Inches(0.25), Inches(2.15), Inches(3.4), Inches(0.9), big, size=40, bold=True, color=GOOD)
        text(s, x + Inches(0.25), Inches(3.1), Inches(3.4), Inches(0.5), label, size=18, bold=True, color=NAVY)
        text(s, x + Inches(0.25), Inches(3.7), Inches(3.4), Inches(2.4), desc, size=14, color=INK2)
    footer(s, n)

    # 11 ---- avalanche chart -----------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Strict avalanche criterion: flip one input bit, and every output bit should flip with 50% probability. "
        "With one round, only 13% flip: that is correctly flagged as weak, which proves the test works. From "
        "three rounds on the cipher is at 50%. We use 16 rounds for a large safety margin."))
    header(s, "Avalanche: one flipped bit reaches 50% after 3 rounds", "Attack results")
    cd = CategoryChartData()
    cd.categories = ["1", "2", "3", "4", "8", "16"]
    cd.add_series("Mean output bits flipped", (13.27, 37.90, 50.00, 50.00, 49.98, 49.98))
    cd.add_series("Ideal (random) = 50%", (50, 50, 50, 50, 50, 50))
    gf = s.shapes.add_chart(XL_CHART_TYPE.LINE_MARKERS, MARGIN, Inches(1.6), Inches(8.0), Inches(5.2), cd)
    ch = gf.chart
    ch.font.name = FONT
    ch.has_legend = True
    ch.legend.position = XL_LEGEND_POSITION.BOTTOM
    ch.legend.include_in_layout = False
    ch.legend.font.size = Pt(12); ch.legend.font.color.rgb = INK2
    va = ch.value_axis
    va.minimum_scale, va.maximum_scale, va.major_unit = 0, 60, 10
    va.has_major_gridlines = True
    va.major_gridlines.format.line.color.rgb = GRID
    va.has_title = True
    va.axis_title.text_frame.text = "% of 128 output bits flipped"
    va.axis_title.text_frame.paragraphs[0].runs[0].font.size = Pt(12)
    va.axis_title.text_frame.paragraphs[0].runs[0].font.color.rgb = INK2
    style_axis(va)
    ca = ch.category_axis
    ca.has_title = True
    ca.axis_title.text_frame.text = "Feistel rounds"
    ca.axis_title.text_frame.paragraphs[0].runs[0].font.size = Pt(12)
    ca.axis_title.text_frame.paragraphs[0].runs[0].font.color.rgb = INK2
    style_axis(ca)
    data_s, ideal_s = ch.plots[0].series
    data_s.format.line.color.rgb = ACCENT; data_s.format.line.width = Pt(2.25)
    data_s.smooth = False
    data_s.marker.style = XL_MARKER_STYLE.CIRCLE; data_s.marker.size = 9
    data_s.marker.format.fill.solid(); data_s.marker.format.fill.fore_color.rgb = ACCENT
    data_s.marker.format.line.color.rgb = SURFACE
    for idx in (0, 2):                       # selective direct labels
        pt = data_s.points[idx]
        pt.data_label.has_text_frame = True
        pt.data_label.text_frame.text = f"{(13.27, 37.90, 50.00)[idx]:.1f}%"
        pt.data_label.text_frame.paragraphs[0].runs[0].font.size = Pt(13)
        pt.data_label.text_frame.paragraphs[0].runs[0].font.bold = True
        pt.data_label.text_frame.paragraphs[0].runs[0].font.color.rgb = INK
        from pptx.enum.chart import XL_LABEL_POSITION
        pt.data_label.position = XL_LABEL_POSITION.RIGHT if idx == 0 else XL_LABEL_POSITION.BELOW
    ideal_s.format.line.color.rgb = MUTED; ideal_s.format.line.width = Pt(1.5)
    ideal_s.format.line.dash_style = MSO_LINE_DASH_STYLE.DASH
    ideal_s.marker.style = XL_MARKER_STYLE.NONE
    ideal_s.smooth = False
    bullets(s, Inches(8.9), Inches(1.9), Inches(3.9), Inches(4.8), [
        "**1 round: 75% of SAC-matrix cells badly off,** correctly flagged as weak (the test works)",
        "**3+ rounds: 50.0%;** worst cell only 0.117 from ideal",
        "**We use 16 rounds,** a 5× safety margin",
        "**Key avalanche:** 1 key bit → 50.3% of ciphertext bits change",
    ], size=15)
    footer(s, n)

    # 12 ---- codebook attack chart ----------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "This is the most important result. I implemented the original byte-per-ray design and attacked it "
        "with a codebook: encrypt all 256 bytes once, then read any message. 100% recovered, no key needed. "
        "The same attack on the final cipher recovers 1.4%, which is just chance on a 71-byte message. "
        "The linearity test also catches a 1-round cipher 100% of the time, and 16 rounds 0%."))
    header(s, "Codebook attack: original breaks, final holds", "Attack results")
    text(s, MARGIN, Inches(1.55), Inches(7.0), Inches(0.4),
         "% of a secret sentence recovered without the key", size=14, color=INK2)
    cd = CategoryChartData()
    cd.categories = ["Original design\n(1 byte = 1 ray, no nonce)", "Final cipher\n(Feistel + CTR + nonce)"]
    cd.add_series("Secret message recovered", (100.0, 1.4))
    gf = s.shapes.add_chart(XL_CHART_TYPE.COLUMN_CLUSTERED, MARGIN, Inches(1.95), Inches(7.0), Inches(4.9), cd)
    ch = gf.chart
    ch.font.name = FONT
    ch.has_legend = False
    ch.has_title = False
    va = ch.value_axis
    va.minimum_scale, va.maximum_scale, va.major_unit = 0, 115, 25
    va.major_gridlines.format.line.color.rgb = GRID
    style_axis(va)
    style_axis(ch.category_axis, size=13)
    plot = ch.plots[0]
    plot.gap_width = 90
    ser = plot.series[0]
    ser.format.fill.solid(); ser.format.fill.fore_color.rgb = ACCENT
    plot.has_data_labels = True
    dl = plot.data_labels
    dl.number_format = '0.0"%"'; dl.number_format_is_linked = False
    dl.font.size = Pt(16); dl.font.bold = True; dl.font.color.rgb = INK
    table(s, Inches(7.9), Inches(1.9), Inches(4.85), [
        ["Linearity test", "Relation holds"],
        ["1 round, sparse impulses", "100% (caught)"],
        ["16 rounds, sparse", "0%"],
        ["16 rounds, random pairs", "0%"]],
        [Inches(2.85), Inches(2.0)], size=13, row_h=Inches(0.5))
    text(s, Inches(7.9), Inches(4.2), Inches(4.85), Inches(2.2), [
        ("Why it matters", {"size": 15, "bold": True, "color": NAVY}),
        ("Every attack is shown to succeed on a deliberately weak version first, so a pass on the "
         "real cipher means something. 1.4% is chance level (1 of 71 bytes).", {"size": 14, "color": INK2})])
    footer(s, n)

    # 13 ---- statistics ---------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "I encrypted 10 megabytes of a repeating English sentence: very low entropy input. The ciphertext "
        "scores 7.999984 bits per byte, a normal chi-square, no correlation, and passes two NIST tests. "
        "Important: passing statistical tests shows no obvious flaw. It does not prove security."))
    header(s, "Statistics on 10 MB: ciphertext looks like pure noise", "Attack results")
    table(s, MARGIN, Inches(1.8), Inches(12.1), [
        ["Test (10 MB)", "Plaintext (repeated sentence)", "Ciphertext", "Ideal / pass condition"],
        ["Shannon entropy", "4.44 bits/byte", "7.999984 bits/byte", "> 7.999 (max 8)"],
        ["Chi-square (255 dof)", "168,251,837 (p = 0)", "221.5 (p = 0.936)", "p between 0.001 and 0.999"],
        ["Serial correlation", "−0.126", "−0.00016", "≈ 0"],
        ["NIST monobit p-value", "0.000", "0.558", "> 0.001"],
        ["NIST runs p-value", "0.000", "0.790", "> 0.001"]],
        [Inches(3.0), Inches(3.3), Inches(2.8), Inches(3.0)], size=15, row_h=Inches(0.58),
        highlight=lambda i, j: j == 2 and i > 0)
    box(s, MARGIN, Inches(5.6), Inches(12.1), Inches(0.75),
        "Passing statistical tests rules out obvious flaws. It does not prove a cipher is secure.",
        fill=RGBColor(0xFD, 0xEC, 0xEC), color=BAD, size=15)
    footer(s, n)

    # 14 ---- vs AES ------------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Honest comparison. Same block and key size, both look random. But AES has 25 years of public "
        "cryptanalysis and proven bounds, and with hardware support it runs about 50,000 times faster "
        "on my laptop. With a 256-bit key no cipher can beat brute force, and AES is already essentially there."))
    header(s, "Honest comparison with AES-256", "Context")
    table(s, MARGIN, Inches(1.7), Inches(12.1), [
        ["", "Refractive cipher (this project)", "AES-256"],
        ["Block / key size", "128-bit / 256-bit", "128-bit / 256-bit"],
        ["Structure", "16-round Feistel, ray tracer as F", "14-round substitution-permutation network"],
        ["Speed (measured, same laptop)", "~0.14 MB/s", "~7,400 MB/s (AES-GCM with AES-NI)"],
        ["Security proof", "None", "Provable bounds vs differential / linear attacks"],
        ["Public cryptanalysis", "Only my own attack suite", "25+ years worldwide; NIST FIPS 197"],
        ["Best known attack", "Unknown (unstudied)", "≈ 2²⁵⁴·⁴ (essentially brute force)"],
        ["Timing side channels", "Table lookups depend on secrets", "Constant time with AES-NI"]],
        [Inches(3.3), Inches(4.4), Inches(4.4)], size=14, row_h=Inches(0.55))
    text(s, MARGIN, Inches(6.25), Inches(12.1), Inches(0.5),
         "Takeaway: use AES-GCM for real data. This project is a working demonstration of how ciphers are designed and attacked.",
         size=14, bold=True, color=NAVY)
    footer(s, n)

    # 15 ---- related work & limitations ------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "My idea combines three existing research areas. Optical encryption (DRPE) was broken because optics "
        "is linear. Optical PUFs use a real physical token, while mine is computed from the key. Many "
        "chaos ciphers were broken by finite-precision effects. My design avoids each of these specific mistakes."))
    header(s, "Related research and limitations", "Context")
    text(s, MARGIN, Inches(1.7), Inches(6), Inches(0.5), "Related research", size=19, bold=True, color=NAVY)
    bullets(s, MARGIN, Inches(2.25), Inches(6.0), Inches(4.4), [
        "**Optical encryption (DRPE),** Réfrégier & Javidi 1995",
        "  broken: optics is linear → chosen/known-plaintext attacks",
        "**Optical PUFs,** Pappu et al., Science 2002",
        "  light through a real random token; mine is computed from the key",
        "**Chaos-based ciphers,** since Matthews 1989",
        "  many broken; Alvarez & Li (2006) list the common mistakes",
        "**Feistel networks,** Luby & Rackoff 1988",
    ], size=15)
    text(s, Inches(7.0), Inches(1.7), Inches(5.8), Inches(0.5), "Limitations", size=19, bold=True, color=NAVY)
    bullets(s, Inches(7.0), Inches(2.25), Inches(5.8), Inches(4.4), [
        "**No independent cryptanalysis** yet",
        "**Cache-timing leaks** from secret-dependent lookups",
        "**~50,000× slower than AES** with hardware support",
        "**Core mixing (SplitMix64)** is not a vetted crypto primitive",
        "**Whole file loaded into memory**",
    ], size=15)
    footer(s, n)

    # 16 ---- demo ----------------------------------------------------------------
    n += 1
    s = blank(prs, notes=(
        "Now a live demo. I'll launch a ray and step through refractions and total internal reflections, "
        "flip one bit to show a completely different path, run the Feistel rounds with the avalanche chart, "
        "then tamper with a ciphertext and show it being rejected."))
    header(s, "Live demo: simulation GUI", "Demo")
    image(s, IMG / "gui-feistel-rounds.png", Inches(6.6), Inches(1.6), w=Inches(6.2))
    bullets(s, MARGIN, Inches(1.75), Inches(5.8), Inches(3.4), [
        "**Ray simulation:** animated photon, each event explained",
        "**Feistel rounds:** live table + avalanche chart",
        "**Encrypt / Decrypt:** tamper 1 bit → rejected",
        "**Crystal:** explore the key-generated crystal",
        "**Attack lab:** run the real attack suite",
    ], size=16)
    box(s, MARGIN, Inches(5.0), Inches(5.7), Inches(1.45), "", fill=NAVY)
    text(s, MARGIN + Inches(0.2), Inches(5.1), Inches(5.4), Inches(1.3), [
        ("mingw32-make all", {"size": 15, "font": MONO, "color": RGBColor(0xFF, 0xFF, 0xFF)}),
        ("python tools/refcrypt_gui.py", {"size": 15, "font": MONO, "color": RGBColor(0xFF, 0xFF, 0xFF)}),
        ("github.com/malladinagarjuna2/crypto-project", {"size": 12, "color": RGBColor(0xB8, 0xC7, 0xDA)}),
    ])
    footer(s, n)

    # 17 ---- conclusion ---------------------------------------------------------
    n += 1
    s = blank(prs, notes="Summarise the four takeaways, then open for questions.")
    header(s, "Conclusion", "Summary")
    points = [
        ("Built", "a complete cipher in C11: crystal, ray tracer, Feistel, CTR + HMAC, CLI"),
        ("Fixed", "6 design flaws in the original plan, and proved the main one with a real attack"),
        ("Verified", "67 unit tests, 19 CLI checks, all attack-suite checks pass (10 MB)"),
        ("Learned", "complexity ≠ security; trust comes from public analysis, so AES stays the choice for real data"),
    ]
    for i, (k, v) in enumerate(points):
        y = Inches(1.85 + i * 1.12)
        box(s, MARGIN, y, Inches(2.2), Inches(0.9), k, fill=ACCENT, color=RGBColor(0xFF, 0xFF, 0xFF), size=18)
        text(s, MARGIN + Inches(2.45), y, Inches(9.6), Inches(0.9), v, size=18, color=INK,
             anchor=MSO_ANCHOR.MIDDLE)
    footer(s, n)

    # 18 ---- thanks -------------------------------------------------------------
    s = blank(prs, notes="Thank the audience and take questions.")
    band = s.shapes.add_shape(MSO_SHAPE.RECTANGLE, 0, 0, W, H)
    band.fill.solid(); band.fill.fore_color.rgb = NAVY; band.line.fill.background()
    text(s, 0, Inches(2.6), W, Inches(1.2), "Thank you", size=54, bold=True,
         color=RGBColor(0xFF, 0xFF, 0xFF), align=PP_ALIGN.CENTER)
    text(s, 0, Inches(3.8), W, Inches(0.6), "Questions?", size=26,
         color=RGBColor(0xD7, 0xE3, 0xF2), align=PP_ALIGN.CENTER)
    text(s, 0, Inches(5.2), W, Inches(0.5), "github.com/malladinagarjuna2/crypto-project  ·  Malladi Nagarjuna",
         size=15, color=RGBColor(0xB8, 0xC7, 0xDA), align=PP_ALIGN.CENTER)

    prs.save(OUT)
    print(f"saved {OUT} ({len(prs.slides)} slides)")


if __name__ == "__main__":
    build()

# =====================================================================
# EXPLANATION (in simple words)
# ---------------------------------------------------------------------
# This script creates the PowerPoint presentation docs/presentation.pptx
# using the python-pptx library, so the deck can be rebuilt any time
# the project changes.
#
#  - The colour constants at the top are the slide theme: a light
#    background, dark text, and one blue accent (checked to be readable
#    for colour-blind viewers).
#  - Helper functions draw the building blocks every slide uses:
#      blank()   new empty slide + speaker notes
#      header()  the blue side bar, small "kicker" label and big title
#      text() / bullets()  text boxes and bullet lists (**bold lead**)
#      box() / arrow() / down_arrow()  shapes for the diagrams
#      table() / image()   tables and GUI screenshots
#      footer()  name and slide number at the bottom
#  - build() creates the 18 slides in order: title, idea, design
#    review, architecture, crystal, physics, ray tracer, Feistel,
#    CTR + HMAC, testing, two real charts (avalanche vs rounds and the
#    codebook attack), statistics, AES comparison, related research,
#    live demo, conclusion and thank-you.
#  - Every slide has speaker notes (what to say), visible in
#    PowerPoint's Notes pane or Presenter View.
#  - The charts are native PowerPoint charts, so you can click them and
#    edit the numbers in PowerPoint.
#
# Run:  python docs/make_presentation.py
# =====================================================================
