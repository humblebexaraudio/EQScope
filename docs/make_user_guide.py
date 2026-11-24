#!/usr/bin/env python3
"""Builds docs/EQScope User Guide.pdf.

Run from anywhere:  python3 docs/make_user_guide.py      (needs: pip install reportlab pillow)
Screenshots live in docs/images. Edit the text below, then run this again.
"""
import os
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import inch
from reportlab.graphics.shapes import Drawing, Rect, String, Line, Polygon
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle,
                                PageBreak, KeepTogether, ListFlowable, ListItem)

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.join(HERE, "images")

# Humble Bexar bear head icon, centred at the top of the cover page (Humble-Bexar-Logo-Options.md)
BEAR_ICON = os.path.join(os.path.dirname(HERE), "Resources", "logo", "humble-bexar-bear-head-icon.png")
OUT = os.path.join(HERE, "EQScope User Guide.pdf")
VERSION = "1.1.0"

# ---------------------------------------------------------------- fonts
FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", "/Library/Fonts", "/System/Library/Fonts/Supplemental",
             "C:/Windows/Fonts", os.path.join(HERE, "fonts")]

def find_font(name):
    for d in FONT_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    return None

regular, bold = find_font("DejaVuSans.ttf"), find_font("DejaVuSans-Bold.ttf")
if regular and bold:
    pdfmetrics.registerFont(TTFont("Body", regular))
    pdfmetrics.registerFont(TTFont("Body-Bold", bold))
    BODY, BOLD, UNICODE = "Body", "Body-Bold", True
else:  # built-in fonts: fine, but only Latin-1 characters
    BODY, BOLD, UNICODE = "Helvetica", "Helvetica-Bold", False

def t(s):
    """Text through the font's character set."""
    if UNICODE:
        return s
    return s.replace("\u2212", "-").replace("\u2192", "->").replace("\u2264", "<=").replace("\u2265", ">=")

# ---------------------------------------------------------------- styles
INK = colors.HexColor("#202327")
ACCENT = colors.HexColor("#b4600e")  # amber, like the panel LEDs
GREY = colors.HexColor("#5b6169")
RULE = colors.HexColor("#c8cbcf")

H1 = ParagraphStyle("H1", fontName=BOLD, fontSize=19, leading=23, textColor=INK, spaceBefore=4, spaceAfter=10)
H2 = ParagraphStyle("H2", fontName=BOLD, fontSize=13.5, leading=17, textColor=ACCENT, spaceBefore=12, spaceAfter=5)
H3 = ParagraphStyle("H3", fontName=BOLD, fontSize=11, leading=14, textColor=INK, spaceBefore=8, spaceAfter=3)
P = ParagraphStyle("P", fontName=BODY, fontSize=10, leading=14.2, textColor=INK, spaceAfter=6)
SMALL = ParagraphStyle("Small", parent=P, fontSize=8.6, leading=11.5, textColor=GREY)
CELL = ParagraphStyle("Cell", parent=P, fontSize=9, leading=12, spaceAfter=0)
CELLB = ParagraphStyle("CellB", parent=CELL, fontName=BOLD)
CAPTION = ParagraphStyle("Caption", parent=SMALL, alignment=TA_CENTER, spaceBefore=3, spaceAfter=10)
CODE = ParagraphStyle("Code", parent=P, fontName="Courier", fontSize=9, leading=12, backColor=colors.HexColor("#f1f2f4"),
                      borderPadding=(4, 6, 4, 6), spaceBefore=4, spaceAfter=8)

def para(s, style=P):
    return Paragraph(t(s), style)

def bullets(items):
    return ListFlowable([ListItem(para(i), leftIndent=12, value="\u2022" if UNICODE else "-") for i in items],
                        bulletType="bullet", start="\u2022" if UNICODE else "-", leftIndent=14, bulletFontName=BODY)

def steps(items):
    return ListFlowable([ListItem(para(i), leftIndent=14) for i in items], bulletType="1", leftIndent=16,
                        bulletFontName=BOLD, bulletColor=ACCENT)

def image(name, width, caption=None):
    from PIL import Image as PILImage
    path = os.path.join(IMG, name)
    w, h = PILImage.open(path).size
    flow = [Image(path, width=width, height=width * h / w)]
    if caption:
        flow.append(para(caption, CAPTION))
    return KeepTogether(flow)

def cell_image(name, width, caption):
    """Image plus caption for use inside a table cell (no KeepTogether there)."""
    from PIL import Image as PILImage
    path = os.path.join(IMG, name)
    w, h = PILImage.open(path).size
    return [Image(path, width=width, height=width * h / w), para(caption, CAPTION)]

def table(rows, widths, header=True):
    data = [[para(c, CELLB if (header and r == 0) or (ci == 0) else CELL) for ci, c in enumerate(row)]
            for r, row in enumerate(rows)]
    tb = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    style = [("VALIGN", (0, 0), (-1, -1), "TOP"),
             ("LINEBELOW", (0, 0), (-1, -1), 0.4, RULE),
             ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 5)]
    if header:
        style += [("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e9eaec")), ("LINEBELOW", (0, 0), (-1, 0), 0.8, GREY)]
    tb.setStyle(TableStyle(style))
    return tb

def chain_diagram(width):
    """SIGNAL -> your EQ -> ANALYZE, the way the inserts are stacked."""
    d = Drawing(width, 74)
    boxes = [("EQScope", "SIGNAL", colors.HexColor("#2f3237")), ("Your EQ", "any plug-in", colors.HexColor("#8a9098")),
             ("EQScope", "ANALYZE", colors.HexColor("#2f3237"))]
    bw, gap = (width - 2 * 46) / 3.0, 46
    for i, (top, sub, fill) in enumerate(boxes):
        x = i * (bw + gap)
        d.add(Rect(x, 10, bw, 52, rx=6, ry=6, fillColor=fill, strokeColor=None))
        d.add(String(x + bw / 2, 39, top, fontName=BOLD, fontSize=11, fillColor=colors.white, textAnchor="middle"))
        d.add(String(x + bw / 2, 22, sub, fontName=BODY, fontSize=9, textAnchor="middle",
                     fillColor=colors.HexColor("#ffb347") if fill != boxes[1][2] else colors.white))
        if i < 2:
            x0, x1, y = x + bw + 6, x + bw + gap - 8, 36
            d.add(Line(x0, y, x1, y, strokeColor=ACCENT, strokeWidth=2))
            d.add(Polygon([x1, y + 5, x1 + 7, y, x1, y - 5], fillColor=ACCENT, strokeColor=None))
    return d

# ---------------------------------------------------------------- page furniture
def on_page(canvas, doc):
    canvas.saveState()
    w, h = letter
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.6)
    canvas.line(doc.leftMargin, 0.62 * inch, w - doc.rightMargin, 0.62 * inch)
    canvas.setFont(BODY, 8)
    canvas.setFillColor(GREY)
    canvas.drawString(doc.leftMargin, 0.45 * inch, "EQScope User Guide \u00b7 Humble Bexar Audio" if UNICODE else "EQScope User Guide - Humble Bexar Audio")
    canvas.drawRightString(w - doc.rightMargin, 0.45 * inch, "Page %d" % doc.page)
    canvas.restoreState()

def on_cover(canvas, doc):
    canvas.saveState()
    w, h = letter
    canvas.setFillColor(colors.HexColor("#c8cbcf"))
    canvas.rect(0, h - 3.2 * inch, w, 3.2 * inch, stroke=0, fill=1)
    icon = 1.0 * inch
    canvas.drawImage(BEAR_ICON, (w - icon) / 2, h - 0.3 * inch - icon, icon, icon, mask="auto")
    canvas.setFillColor(colors.HexColor("#202327"))
    canvas.setFont(BOLD, 34)
    canvas.drawString(doc.leftMargin, h - 2.15 * inch, "EQSCOPE")
    canvas.setFont(BOLD, 11)
    canvas.setFillColor(colors.HexColor("#4a4f56"))
    canvas.drawString(doc.leftMargin, h - 2.52 * inch, "H U M B L E   B E X A R   A U D I O")
    canvas.setFont(BODY, 11)
    canvas.drawRightString(w - doc.rightMargin, h - 2.15 * inch, "User Guide")
    canvas.drawRightString(w - doc.rightMargin, h - 2.4 * inch, "Version " + VERSION)
    canvas.restoreState()
    on_page(canvas, doc)

# ---------------------------------------------------------------- content
def build():
    doc = SimpleDocTemplate(OUT, pagesize=letter, leftMargin=0.8 * inch, rightMargin=0.8 * inch,
                            topMargin=0.75 * inch, bottomMargin=0.85 * inch,
                            title="EQScope User Guide", author="Humble Bexar Audio")
    W = letter[0] - doc.leftMargin - doc.rightMargin
    s = []

    # Cover
    s += [Spacer(1, 2.45 * inch),
          para("See exactly what your EQ is doing.", ParagraphStyle("Lead", parent=H1, fontSize=17)),
          para("EQScope sends a perfectly flat test signal into any EQ plug-in and draws the EQ's frequency and phase "
               "response live, as you turn its knobs. It works with any EQ in any host on macOS, Windows, iPhone and iPad."),
          Spacer(1, 6),
          image("silver.png", W * 0.72, "EQScope in ANALYZE mode: the live response (cyan) of a three-band EQ, with an earlier "
                "setting kept as a snapshot (orange)."),
          Spacer(1, 4),
          table([["In this guide", ""],
                 ["1  How it works", "6  Faceplate finishes"],
                 ["2  Installing", "7  Starting points"],
                 ["3  Setting up a measurement", "8  Troubleshooting"],
                 ["4  Controls", "9  Uninstalling and specifications"],
                 ["5  Status lights, the display and instructions", ""]], [W / 2, W / 2]),
          PageBreak()]

    # 1. How it works
    s += [para("1  How it works", H1),
          para("You use <b>two copies of EQScope</b> with the EQ you want to see between them, on one channel:"),
          chain_diagram(W), Spacer(1, 6),
          bullets(["<b>SIGNAL</b> replaces the audio with the probe: a test signal with exactly the same level at every "
                   "frequency, at \u221218 dBFS RMS. Its own screen shows the flat 0 dB line it is sending.",
                   "<b>ANALYZE</b> compares what comes out of the EQ with that flat signal and draws the difference: the EQ's "
                   "response. By default it outputs silence, so you don't have to listen to the probe."]),
          para("The two copies don't need any routing between them, and EQ latency doesn't matter. Because the probe repeats "
               "exactly, the measurement needs no averaging and has no noise. The curve follows a change in your EQ within about "
               "0.18 seconds. Linear-phase EQs and EQs that add latency are handled automatically."),
          para("For very narrow, deep bands below about 60 Hz, press <b>FINE</b> on the SIGNAL copy: the probe gets four times "
               "longer, so the curve takes about 0.7 seconds to follow a change but the low end is exact. The ANALYZE copy notices "
               "by itself, and tells you on screen when an EQ needs FINE."),
          image("signal.png", W * 0.82, "SIGNAL mode: the flat line is what goes into your EQ."),
          PageBreak()]

    # 2. Installing
    s += [para("2  Installing", H1),
          para("macOS (10.13 High Sierra or later, Intel and Apple Silicon)", H2),
          steps(["Double-click <b>EQScope-1.1.0-macOS.pkg</b> and follow the steps. <b>Customize</b> lets you leave out the AU, the "
                 "VST3 or the standalone app.",
                 "If macOS says the installer can't be opened, Control-click it and choose <b>Open</b>, or open "
                 "<b>System Settings \u2192 Privacy &amp; Security</b> and click <b>Open Anyway</b>. "
                 "This only happens with unsigned builds.",
                 "Restart your DAW. In Logic, if EQScope doesn't appear, open <b>Logic Pro \u2192 Settings \u2192 Plug-in "
                 "Manager</b>, select it and click <b>Reset &amp; Rescan Selection</b>."]),
          table([["Installed", "Where"],
                 ["Audio Unit (Logic, GarageBand, MainStage)", "/Library/Audio/Plug-Ins/Components/EQScope.component"],
                 ["VST3 (Ableton Live, Cubase, Reaper, Studio One, Bitwig, FL Studio)", "/Library/Audio/Plug-Ins/VST3/EQScope.vst3"],
                 ["Standalone app and this guide", "/Applications/Humble Bexar Audio/"]], [W * 0.45, W * 0.55]),
          para("<b>Upgrading from EQ Probe:</b> EQScope was called EQ Probe before version 1.0. The installers replace "
               "EQ Probe, and sessions saved with it open in EQScope with all their settings.", SMALL),
          para("Windows (10 and 11, 64-bit)", H2),
          steps(["Run <b>EQScope-1.1.0-Windows-Setup.exe</b>. If SmartScreen appears, click <b>More info \u2192 Run anyway</b>.",
                 "Choose whether to install the standalone app. The VST3 and this guide are always installed.",
                 "Rescan plug-ins in your DAW. EQScope is listed under <b>Humble Bexar Audio</b>."]),
          table([["Installed", "Where"],
                 ["VST3", "C:\\Program Files\\Common Files\\VST3\\EQScope.vst3"],
                 ["Standalone app and this guide", "C:\\Program Files\\Humble Bexar Audio\\EQScope\\ (with Start menu shortcuts)"]],
                [W * 0.45, W * 0.55]),
          para("iPhone and iPad (iOS / iPadOS 13 or later)", H2),
          para("EQScope for iOS is an <b>AUv3</b> effect that lives inside the EQScope app. Install the app (from TestFlight, "
               "the App Store, or built and installed from Xcode), open it once, and it appears in any AUv3 host under "
               "<b>Humble Bexar Audio</b>: Logic Pro for iPad, GarageBand, AUM, Cubasis, Loopy Pro and others."),
          PageBreak()]

    # 3. Setting up a measurement
    s += [para("3  Setting up a measurement", H1),
          para("Logic Pro (Mac)", H2),
          steps(["Create an <b>Aux channel strip</b>: <b>Mixer \u2192 Options \u2192 Create New Auxiliary Channel Strip</b>. "
                 "Aux strips are always processing, so you don't need to press play.",
                 "Slot 1: <b>Audio FX \u2192 Audio Units \u2192 Humble Bexar Audio \u2192 EQScope</b>. Press <b>SIGNAL</b>.",
                 "Slot 2: the EQ you want to see.",
                 "Slot 3: <b>EQScope</b> again, left on <b>ANALYZE</b>. Open it: the <b>LOCKED</b> light comes on.",
                 "Turn the EQ's knobs and watch the curve follow."]),
          para("Other DAWs", H2),
          para("Same order on one track: <b>EQScope (SIGNAL) \u2192 your EQ \u2192 EQScope (ANALYZE)</b>. Use a track that "
               "is processing: some DAWs only run plug-ins while playing, or while a track is record-armed or monitoring."),
          para("iPad and iPhone", H2),
          para("Add three effects on one channel in your host: <b>EQScope \u2192 your EQ \u2192 EQScope</b>. Set the first to "
               "<b>SIGNAL</b> and open the last one. In AUM, an audio channel with no input works and is always live. In Logic "
               "Pro for iPad and GarageBand, press play if no light comes on."),
          para("Keep the EQ's own output gain at 0 dB if you want to read band gains directly, or press <b>ZERO</b> to "
               "cancel it out (see Controls)."),
          image("iphone.png", W * 0.3, "On an iPhone the controls wrap onto three rows."),
          PageBreak()]

    # 4. Controls
    s += [para("4  Controls", H1),
          image("controls.png", W * 0.74, "The control strip in ANALYZE mode."),
          table([["Control", "Mode", "What it does"],
                 ["SIGNAL / ANALYZE", "both", "What this copy does. SIGNAL goes before the EQ; ANALYZE goes after it. The LED shows the active one."],
                 ["OUTPUT (green LED)", "SIGNAL", "Turns the probe on or off."],
                 ["FINE", "SIGNAL", "Off (default): Fast probe, the curve follows the EQ within about 0.18 s. On: Fine probe, about 0.7 s, exact even for very narrow bands below about 60 Hz. The ANALYZE copy follows automatically."],
                 ["CHANNEL knob", "ANALYZE", "Measure MID (left and right averaged), L or R. Use L or R for EQs with different settings per side, or in mid/side mode."],
                 ["MONITOR", "ANALYZE", "Off (default): this copy outputs silence. On: passes the audio through."],
                 ["RANGE knob", "both", "Vertical scale of the display: \u00b16, \u00b112, \u00b124 or \u00b148 dB."],
                 ["SMOOTH knob", "both", "OFF, or 1/48 to 1/3 octave smoothing. Usually unnecessary: the measurement isn't noisy."],
                 ["PHASE", "both", "Shows the phase response under the frequency response."],
                 ["SNAPSHOT", "ANALYZE", "Freezes the current curve as an orange overlay, to compare settings or two different EQs. The LED shows a snapshot is stored. Saved with your session."],
                 ["CLEAR", "ANALYZE", "Removes the snapshot."],
                 ["ZERO", "ANALYZE", "Treats the current response as flat, cancelling anything else in the chain (an output trim, another plug-in). Press again to remove. Saved with your session."],
                 ["FINISH swatches", "both", "Eight small round swatches at the bottom left of the faceplate; the current finish's name is shown beside them (see section 6)."],
                 ["? INSTRUCTIONS", "both", "Bottom right: opens short instructions on the faceplate (see section 5)."]],
                [W * 0.22, W * 0.12, W * 0.66]),
          para("Using the knobs and screen", H2),
          bullets(["<b>Knobs:</b> drag up or down, scroll, or click a printed position. Double-click resets.",
                   "<b>Reading values:</b> hover over the graph (or tap it) for frequency, gain and phase.",
                   "<b>Resizing:</b> drag the corner on Mac and Windows. On iPhone and iPad the faceplate rearranges itself."]),
          PageBreak()]

    # 5. Status and display
    s += [para("5  Status lights, the display and instructions", H1),
          image("header-right.png", W * 0.7),
          table([["Light", "Meaning"],
                 ["LOCKED (green)", "A steady probe signal is arriving. The curve is exact."],
                 ["SETTLING (amber)", "You're moving a knob, or the EQ is still ringing. The curve is drawn dimmed and locks again within about half a second (Fast probe)."],
                 ["NO PROBE (red)", "There's audio, but it isn't the probe (for example music). Put a SIGNAL copy before the EQ."],
                 ["All off", "Nothing is arriving. The screen says NO SIGNAL or HOST NOT PROCESSING: use an always-live channel or press play."]],
                [W * 0.25, W * 0.75]),
          para("Reading the screen", H2),
          bullets(["The dashed <b>FLAT 0 dB</b> line is the probe itself: what the response would be with nothing in between.",
                   "The <b>cyan</b> curve is the EQ's frequency response; the <b>violet</b> curve below it is its phase.",
                   "The text next to the status says whether the EQ is <b>minimum-phase</b> (normal, analog-style EQs: the phase shown "
                   "is the EQ's true phase) or <b>linear/mixed-phase</b> (the phase is shown relative to the EQ's impulse peak, so "
                   "a linear-phase EQ reads as a flat 0\u00b0 line and its latency is removed automatically).",
                   "<b>ZEROED</b> beside the flat line means a zero calibration is active."]),
          para("Built-in instructions", H2),
          para("Press <b>? INSTRUCTIONS</b> at the bottom right of the faceplate for a short version of this guide: setup, "
               "reading the response, every control, the status lights and the finishes. Click <b>CLOSE</b> or anywhere outside it to close."),
          image("instructions.png", W * 0.66),
          PageBreak()]

    # 6. Starting points
    s += [para("6  Faceplate finishes", H1),
          para("Choose a finish with the eight small round <b>FINISH</b> swatches at the <b>bottom left of the faceplate</b>: "
               "the selected swatch has an amber ring and its name is shown beside the swatches. Right-clicking the "
               "faceplate, or clicking the EQSCOPE logo, gives the same choices. \u201cHumble Bexar Audio\u201d is on every finish. "
               "Six are brushed metal; the two <b>Polished Enamel</b> finishes are smooth gloss paint with no brushed grain. "
               "The finish only changes the look: it's saved with your session, and the last finish you pick becomes the "
               "default for new copies of EQScope."),
          table([["Finish", "Surface", "Lettering", "Knob caps"],
                 ["Silver (default)", "Brushed aluminium", "Black", "Aluminium"],
                 ["Gunmetal Blue", "Brushed dark blued steel", "White", "Blued steel"],
                 ["Burnt Orange", "Brushed burnt-orange metal", "White", "Bronze"],
                 ["Vintage", "Brushed champagne brass, darker worn edges, light patina", "Dark brown", "Brass"],
                 ["Glacier Blue", "Brushed pale icy-blue metal", "Navy", "Frosted aluminium"],
                 ["Pecan", "Brushed warm brown metal", "Cream", "Copper"],
                 ["Polished Enamel Cream", "Glossy cream enamel (not brushed)", "Dark brown", "Chrome"],
                 ["Polished Enamel Black", "Glossy black enamel (not brushed)", "Cream", "Chrome"]],
                [W * 0.25, W * 0.43, W * 0.15, W * 0.17]),
          Spacer(1, 6),
          Table([[cell_image(f + ".png", W * 0.235, n) for f, n in row] for row in
                 [[("silver", "Silver"), ("gunmetal-blue", "Gunmetal Blue"), ("burnt-orange", "Burnt Orange"), ("vintage", "Vintage")],
                  [("glacier-blue", "Glacier Blue"), ("pecan", "Pecan"), ("enamel-cream", "Enamel Cream"), ("enamel-black", "Enamel Black")]]],
                colWidths=[W * 0.25] * 4, style=[("VALIGN", (0, 0), (-1, -1), "TOP"),
                                                 ("LEFTPADDING", (0, 0), (-1, -1), 1), ("RIGHTPADDING", (0, 0), (-1, -1), 1)]),
          para("The default finish is stored in <i>~/Library/Application Support/Humble Bexar Audio/EQScope.settings</i> on macOS "
               "and <i>%APPDATA%\\Humble Bexar Audio\\EQScope.settings</i> on Windows.", SMALL),
          PageBreak()]

    s += [para("7  Starting points", H1),
          table([["Task", "How"],
                 ["Learn what a knob really does", "Sweep one band's frequency, gain and Q and watch the curve. Many EQs' Q and gain interact differently from what their labels suggest."],
                 ["Match two EQs", "Dial in EQ A, press SNAPSHOT, then swap in EQ B and adjust it until the cyan curve sits on the orange one."],
                 ["Check a linear-phase EQ", "Turn PHASE on. A true linear-phase EQ shows a flat 0\u00b0 phase line and \u201clinear/mixed-phase\u201d in the status text."],
                 ["Measure only your changes", "With the EQ set to where you started, press ZERO. From then on the curve shows only what you change."],
                 ["Check stereo or M/S EQs", "Set CHANNEL to L, then R, to see each side's curve. Use SNAPSHOT on one side to compare them."],
                 ["Deep low-end work", "For narrow bands below about 60 Hz (for example +12 dB at 30 Hz, Q 4), press FINE on the SIGNAL copy. The screen suggests it when it's needed."],
                 ["Analog-modelled EQs (Pultec style)", "The probe plays at \u221218 dBFS RMS, the level these EQs are designed for. Their saturation adds a little blur to the curve; that is the EQ, not the probe."]],
                [W * 0.3, W * 0.7]),
          para("8  Troubleshooting", H1),
          table([["Problem", "Fix"],
                 ["No lights, \u201cHOST NOT PROCESSING\u201d", "The host isn't running the channel. In Logic use an Aux channel strip; elsewhere press play or arm/monitor the track."],
                 ["Screen says \u201cNO SIGNAL\u201d", "Open the first copy: SIGNAL must be lit (new copies start in ANALYZE), its screen must say GENERATING PROBE and its OUTPUT LED must be on. The ANALYZE copy must be the last of the three on the channel."],
                 ["NO PROBE light", "There's no SIGNAL copy before the EQ, or something in between adds changing audio (a synth, a modulated effect, noise). Keep only the EQ between the two copies."],
                 ["Screen says the EQ \u201crings longer than the Fast probe\u201d", "A very narrow low band rings longer than the Fast probe, so the low end is approximate (by more than 0.1 dB). Press FINE on the SIGNAL copy."],
                 ["Stays on SETTLING", "Something in the chain keeps changing: an LFO, an auto-gain, or an EQ with dynamic bands. Dynamic EQs and compressors can't show a steady response."],
                 ["Curve is offset up or down", "The EQ's output gain isn't at 0 dB, or another plug-in changes the level. Press ZERO with the EQ flat."],
                 ["I hear a loud hiss", "That's the probe. Turn MONITOR off on the ANALYZE copy (it's off by default), or turn OUTPUT off on the SIGNAL copy when you're done."]],
                [W * 0.3, W * 0.7]),
          PageBreak()]

    # 8. Uninstalling and specifications
    s += [para("9  Uninstalling", H1),
          bullets(["<b>macOS:</b> delete <i>EQScope.component</i> from /Library/Audio/Plug-Ins/Components, <i>EQScope.vst3</i> "
                   "from /Library/Audio/Plug-Ins/VST3, and the EQScope items in /Applications/Humble Bexar Audio.",
                   "<b>Windows:</b> Settings \u2192 Apps \u2192 Installed apps \u2192 EQScope \u2192 Uninstall.",
                   "<b>iPhone and iPad:</b> delete the EQScope app. Its AUv3 is removed with it."]),
          para("Specifications", H1),
          table([["Formats", "macOS: AU, VST3, standalone. Windows: VST3, standalone. iOS/iPadOS: AUv3 (and app)."],
                 ["Systems", "macOS 10.13 High Sierra or later (Intel and Apple Silicon); Windows 10/11 64-bit; iOS/iPadOS 13 or later."],
                 ["Channels", "Mono or stereo"],
                 ["Probe signal", "Periodic flat-spectrum multisine (Schroeder phases), \u221218 dBFS RMS, 3.3 dB crest factor. Fast: about 170 ms period (8,192 samples at 48 kHz). Fine: about 680 ms."],
                 ["Response time", "About 0.18 s (Fast) or 0.7 s (Fine) from an EQ change to the full new curve; display updates up to 60 times a second"],
                 ["Resolution", "1.46 Hz at 48 kHz (sample rate / 32,768); display 20 Hz to 20 kHz"],
                 ["Accuracy", "Better than 0.01 dB and 0.2\u00b0 against exact EQ responses in our tests"],
                 ["Latency", "None (no audio delay). EQ latency is removed from the phase display automatically."]],
                [W * 0.22, W * 0.78], header=False),
          Spacer(1, 14),
          para("EQScope is made by Humble Bexar Audio. Version " + VERSION + ".", SMALL)]

    doc.build(s, onFirstPage=on_cover, onLaterPages=on_page)
    print("Created", OUT)

if __name__ == "__main__":
    build()
