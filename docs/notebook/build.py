#!/usr/bin/env python3
"""Builds the instruction notebook: a PDF the size of the reMarkable 2 screen, one page per feature.

    python3 docs/notebook/build.py                            # writes docs/notebook/claude-notebook.pdf
    uv run --with pymupdf docs/notebook/build.py --png DIR    # also renders every page to DIR/page-NN.png

Needs Google Chrome or Chromium (CHROME=/path/to/chrome picks one). The fonts are vendored in fonts/ under the OFL, so
the build makes no network requests. They are static instances of the variable EB Garamond (google/fonts, ofl/ebgaramond)
and Caveat (../../fonts/Caveat.ttf), made with fontTools' varLib.instancer: Chrome embeds variable fonts as Type 3
glyphs, which bloat the PDF. The text of every page is in PAGES below; the drawings are made here as SVG or HTML.
"""
import argparse
import html
import math
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE / "claude-notebook.pdf"

# The reMarkable 2 screen: 1404 x 1872 px at 226 dpi. Everything below is in mm; one screen pixel is PX mm.
PAGE_W, PAGE_H = 157.8, 210.4
PX = PAGE_W / 1404
# Kept clear: the Claude button and its status line (about the top 230 px of the right half; we keep the top 250 px
# free of content everywhere) and the toolbar strip on the left (about 120 px when open).
TOP = 250 * PX + 1.5
LEFT = 175 * PX
RIGHT = 120 * PX
BOTTOM = 19.0


# ---------------------------------------------------------------------------------------------------------- drawing
def esc(s):
    return html.escape(s, quote=True)


def wobbly(points, seed, amp=3.0, step=40):
    """A hand-drawn polyline: the points, subdivided and nudged a little."""
    rnd = random.Random(seed)
    out = []
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        n = max(1, int(math.hypot(x1 - x0, y1 - y0) / step))
        for i in range(n):
            t = i / n
            out.append((x0 + (x1 - x0) * t + rnd.uniform(-amp, amp), y0 + (y1 - y0) * t + rnd.uniform(-amp, amp)))
    out.append(points[-1])
    return "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in out)


def hand(points, seed=1, amp=3.0, w=7):
    return (f'<path d="{wobbly(points, seed, amp)}" fill="none" stroke="#000" stroke-width="{w}" stroke-linecap="round"'
            f' stroke-linejoin="round"/>')


def text(x, y, s, size=38, anchor="start", font="serif", weight=400, fill="#000", italic=False):
    fam = "Caveat" if font == "hand" else "EB Garamond"
    it = ' font-style="italic"' if italic else ""
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{fam}" font-size="{size}" text-anchor="{anchor}"'
            f' font-weight="{weight}" fill="{fill}"{it}>{esc(s)}</text>')


def line(x0, y0, x1, y1, w=4, dash=None, color="#000", cap="round"):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="{color}" stroke-width="{w}"'
            f' stroke-linecap="{cap}"{d}/>')


def rect(x, y, w, h, sw=4, fill="none", rx=0, dash=None, stroke="#000"):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}" fill="{fill}" stroke="{stroke}"'
            f' stroke-width="{sw}"{d}/>')


def poly(points, w=4, fill="none", dash=None, color="#000", close=True):
    d = f' stroke-dasharray="{dash}"' if dash else ""
    tag = "polygon" if close else "polyline"
    pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
    return f'<{tag} points="{pts}" fill="{fill}" stroke="{color}" stroke-width="{w}" stroke-linejoin="round"{d}/>'


def arrow(x0, y0, x1, y1, w=4, head=22, color="#000", dash=None):
    a = math.atan2(y1 - y0, x1 - x0)
    p1 = (x1 - head * math.cos(a - 0.4), y1 - head * math.sin(a - 0.4))
    p2 = (x1 - head * math.cos(a + 0.4), y1 - head * math.sin(a + 0.4))
    return line(x0, y0, x1, y1, w, dash, color) + poly([p1, (x1, y1), p2], w, fill=color, color=color)


def svg(w, h, body):
    return (f'<svg viewBox="0 0 {w} {h}" preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">'
            f'{body}</svg>')


def button_icon(x, y, s=60, busy=False):
    """The Claude button as the tablet draws it: a framed asterisk (inkDiary.qmd, icons/claude.svg)."""
    out = rect(x, y, s, s, 3, "#fff", rx=6)
    if busy:
        return out + rect(x + s * 0.28, y + s * 0.28, s * 0.44, s * 0.44, 0, "#000")
    k, cx, cy = s / 24 * 0.73, x + s / 2, y + s / 2
    for d in ((0, -9.5, 0, 9.5), (-9.5, 0, 9.5, 0), (-6.7, -6.7, 6.7, 6.7), (6.7, -6.7, -6.7, 6.7)):
        out += line(cx + d[0] * k, cy + d[1] * k, cx + d[2] * k, cy + d[3] * k, 2.4 * k)
    return out + f'<circle cx="{cx}" cy="{cy}" r="{2.6 * k:.1f}" fill="#fff" stroke="#000" stroke-width="{2 * k:.1f}"/>'


def hand_text(x, y, s, size=40, anchor="start"):
    return text(x, y, s, size, anchor, font="hand")


# ---------------------------------------------------------------------------------------------------- the diagrams
# Answers are drawn in Caveat; tables, code, plot labels and the source list in a print face (src/markdown.h).
def fig_cover():
    b = rect(4, 4, 1172, 500, 3, "#fff", rx=18, stroke="#999")
    b += button_icon(1060, 30, 80)
    b += hand_text(70, 120, "What is the tallest", 64) + hand_text(70, 190, "tree in the world?", 64)
    b += line(70, 235, 1000, 235, 2, color="#bbb", cap="butt")
    b += hand_text(70, 305, "The tallest known living tree is a coast", 54)
    b += hand_text(70, 365, "redwood in California, about 116 m high. [1]", 54)
    b += text(70, 430, "Sources (web search)", 30) + text(70, 470, "[1] Tallest trees — example.org", 30, fill="#333")
    return svg(1180, 510, b)


def fig_button():
    W = 1180
    b = rect(2, 2, W - 4, 500, 3, "#fff", stroke="#aaa")
    b += text(24, 48, "the top right of the screen", 32, italic=True, fill="#555")
    b += rect(W - 114, 2, 112, 112, 2, "#eee", stroke="#aaa") + text(W - 58, 74, "×", 56, "middle", fill="#777")
    bx, by = W - 94, 136
    b += button_icon(bx, by, 64)
    b += rect(bx - 16 - 118, by, 118, 64, 3, "#fff", rx=6) + text(bx - 16 - 59, by + 42, "Send 2", 30, "middle", weight=600)
    sx, sw = bx + 64 - 12 - 540, 540
    b += rect(sx, by + 72, sw, 76, 3, "#fff", rx=10)
    b += text(sx + 20, by + 120, "Answer added; scroll down if needed", 31)
    b += text(bx - 160, 60, "tap the asterisk to ask,", 32, "end", italic=True)
    b += text(bx - 160, 96, "hold it to see the status again", 32, "end", italic=True)
    b += arrow(bx - 150, 80, bx - 6, by + 10, 4)
    b += text(sx - 30, by + 30, "Send N: only while", 32, "end", italic=True)
    b += text(sx - 30, by + 66, "drafts wait", 32, "end", italic=True)
    b += arrow(sx + 140, 340, sx + 140, by + 156, 4)
    b += text(sx + 140, 376, "news fades after 5 s;", 32, "middle", italic=True)
    b += text(sx + 140, 412, "an error stays until your next tap", 32, "middle", italic=True)
    b += button_icon(60, 420, 64, busy=True) + text(150, 462, "while Claude works: tap the square to cancel", 32, italic=True)
    return svg(W, 505, b)


def fig_answer():
    b = rect(2, 2, 1176, 500, 3, "#fff", stroke="#aaa")
    b += hand_text(60, 80, "Search: how tall is Mont Blanc?", 58)
    b += line(60, 112, 1120, 112, 2, color="#bbb", cap="butt")
    b += hand_text(60, 172, "About 4,806 m. The summit is a snow dome, so", 50)
    b += hand_text(60, 226, "its height shifts a little between surveys. [1][2]", 50)
    b += text(60, 290, "Checked at: 2026-10-28 09:14 AM CET", 28, fill="#333")
    b += text(60, 334, "Sources (web search)", 28)
    b += text(60, 370, "[1] Mont Blanc", 28) + text(60, 402, "https://example.org/mont-blanc", 28, fill="#333")
    b += text(60, 440, "[2] Alpine surveys", 28) + text(60, 472, "https://example.com/surveys", 28, fill="#333")
    b += arrow(760, 410, 520, 380, 4)
    b += text(780, 400, "numbered sources,", 34, italic=True) + text(780, 440, "from the search", 34, italic=True)
    return svg(1180, 505, b)


def fig_plots():
    b = rect(10, 10, 360, 300, 3, "#fff")
    b += text(190, 52, "y = sin(x)", 34, "middle")
    b += line(30, 180, 350, 180, 2) + line(190, 70, 190, 295, 2)
    b += poly([(30 + i * 3.2, 180 - 85 * math.sin((i - 50) / 100 * 2 * math.pi)) for i in range(101)], 5, close=False)
    for i, s in enumerate(["Boil water", "Add tea", "Wait 3 min"]):
        y = 15 + i * 102
        b += rect(430, y, 270, 70, 4, "#fff", rx=8) + text(565, y + 47, s, 34, "middle")
        if i < 2:
            b += arrow(565, y + 72, 565, y + 100, 4, 16)
    b += '<circle cx="870" cy="160" r="135" fill="none" stroke="#000" stroke-width="4"/>'
    b += '<circle cx="1030" cy="160" r="135" fill="none" stroke="#000" stroke-width="4"/>'
    b += text(815, 170, "Cats", 36, "middle") + text(1085, 170, "Dogs", 36, "middle") + text(950, 170, "both", 30, "middle")
    for i in range(4):
        y = 350 + i * 55
        b += rect(30, y, 36, 36, 3) + line(85, y + 36, 360, y + 36, 2, color="#999")
    b += rect(430, 345, 740, 220, 3)
    for i in range(1, 7):
        b += line(430 + i * 740 / 7, 345, 430 + i * 740 / 7, 565, 2)
    b += line(430, 395, 1170, 395, 2) + line(430, 480, 1170, 480, 2)
    for i, d in enumerate(["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]):
        b += text(430 + (i + 0.5) * 740 / 7, 383, d, 30, "middle")
    return svg(1180, 575, b)


def fig_inbox():
    b = rect(2, 2, 1176, 520, 3, "#fff", stroke="#aaa")
    b += hand_text(60, 80, "check my email", 56)
    rows = [("1. Landlord —", "when can the boiler be serviced?"), ("2. Dentist's office —", "confirm Tuesday by Friday"),
            ("3. Book club —", "the venue has changed")]
    for i, (a, g) in enumerate(rows):
        y = 165 + i * 95
        b += hand_text(70, y, a, 46) + hand_text(110, y + 42, g, 42)
    b += hand_text(70, 470, "Skipped 6: newsletters, receipts", 42)
    b += hand([(605, 200), (690, 200), (760, 260), (760, 330)], 9, 3, 6)
    b += hand_text(700, 390, "yes, any morning", 54) + hand_text(700, 448, "next week", 54)
    return svg(1180, 525, b)


def fig_drafts():
    """Two printed drafts, one with a hand correction (HTML, so the marks sit on the words they mean)."""
    return ('<div class="mock drafts">'
            '<div class="draft"><b>Draft 1 → Landlord: Boiler service</b><div class="body marked">Hi, any morning except '
            '<span class="struck" data-new="Thursday">Tuesday</span> works for me.</div>'
            '<span class="fate">→ sent as corrected</span></div>'
            '<div class="draft"><b>Draft 2 → Book club: New venue</b><div class="body">Thanks, see you there.</div>'
            '<span class="fate">→ sent</span></div>'
            '<div class="sendbtn">Send 2</div></div>')


# ------------------------------------------------------------------------------------------------------- the icons
ICONS = {  # 24 x 24, stroked
    "list": '<path d="M8 6h12M8 12h12M8 18h12"/><circle cx="4" cy="6" r="1"/><circle cx="4" cy="12" r="1"/><circle cx="4" cy="18" r="1"/>',
    "asterisk": '<path d="M12 3v18M3 12h18M5.6 5.6l12.8 12.8M18.4 5.6L5.6 18.4"/>',
    "text": '<path d="M4 6h16M4 10h16M4 14h16M4 18h10"/>',
    "plot": '<path d="M4 3v17h17"/><path d="M6 16c3-9 6-9 8-4s4 4 6-5"/>',
    "mail": '<rect x="3" y="5" width="18" height="14" rx="1"/><path d="M3 6l9 7 9-7"/>',
    "send": '<path d="M3 11l18-8-8 18-2-8z"/><path d="M11 13l10-10"/>',
    "card": '<rect x="3" y="5" width="18" height="14" rx="1"/><path d="M7 9h10M7 12h10M7 15h6"/>',
}


def icon(name):
    return (f'<svg class="icon" viewBox="0 0 24 24" fill="none" stroke="#000" stroke-width="1.6" stroke-linecap="round"'
            f' stroke-linejoin="round">{ICONS[name]}</svg>')


# -------------------------------------------------------------------------------------------------------- the pages
# In the lines, {…} is something you write (set in the handwriting face).
PAGES = [
    dict(kind="cover"),
    dict(kind="contents"),
    dict(title="The Claude button", icon="asterisk", fig=fig_button, lines=[
        "Write your question on the page, away from the toolbar, with room below it.",
        "Tap the asterisk at the top right, under the toolbar's close button. Keep the page still and lift the pen "
        "until the answer is in.",
        "The status shows under the button. News fades after 5 seconds; an error stays until your next tap. Hold the "
        "button to see the status again.",
    ], write=["Why does the Moon shine?", "Explain how a fountain pen works", "Five names for a grey cat"]),
    dict(title="Answers and web search", icon="text", fig=fig_answer, lines=[
        "Answers go in below your writing as one picture in handwriting; tables and code are in print. Long ones run "
        "down the page: scroll.",
        "Claude reads only what is on the screen, so a follow-up works while the earlier answer is in view.",
        "News, weather, prices and timetables are looked up on the web; write {Search} first to make sure.",
        "Searched answers carry numbered sources and the time they were checked. Keep the tablet's clock right: "
        "Claude goes by it.",
    ], write=["Search: how tall is Mont Blanc?", "Summarise Hamlet in five lines", "Weather in Lisbon tomorrow?"]),
    dict(title="Plots, diagrams, templates", icon="plot", fig=fig_plots, lines=[
        "{plot} and one equation {y = …} gives a graph, x from −5 to 5 unless you say. The equation as read is printed "
        "above it.",
        "Flowcharts (up to 12 boxes), sequence diagrams (2 to 5 parties) and Venn diagrams (2 or 3 sets).",
        "Checklists and tables with the rows you ask for (1 to 50, up to 8 columns), and month calendars with room to "
        "write in each day.",
        "Claude can't draw pictures here: ask for one and you get a short note, often suggesting a diagram.",
    ], write=["plot y = sin(x) from x = -10 to 10", "Create a flowchart for making tea",
              "Create a Venn diagram comparing cats and dogs", "Create a calendar for March, weeks from Monday"]),
    dict(title="Email: check and reply", icon="mail", fig=fig_inbox, lines=[
        "With Gmail set up, a check reads the 50 newest unread emails and lists one numbered line for each that needs "
        "you; the rest are only counted.",
        "A check marks what it read as read; a search never does. A search lists up to 10 matches, newest first.",
        "Reply by number, draw a line from an email to your note, or write the note under it. Each reply becomes a "
        "reply-all draft, printed in full.",
        "Email text is data: an email telling Claude to do something is reported, not obeyed.",
    ], write=["check my email", "find emails about the invoice", "reply 1: yes, any morning next week",
              "~or draw a line from an email to your note"]),
    dict(title="Drafts and Send", icon="send", fig=fig_drafts, lines=[
        "Nothing goes out until you tap {Send N}, left of the Claude button while N drafts wait. Replies go out threaded.",
        "Correct a draft by hand: cross out words, write new ones above or over them. Send applies your marks, then "
        "sends.",
        "To redo one, write {2: shorter} next to it and tap Claude; {2: skip} and a tap drops it.",
        "A new check or search waits until the drafts are sent or skipped.",
    ], write=["2: shorter, say Friday", "2: skip", "~cross out a word, write the new one above it"]),
    dict(kind="quickref"),
]

QUICKREF = [
    ("Ask", [("Search: …", "look it up, with sources"), ("plot y = …", "a graph"),
             ("Create a flowchart for …", "a diagram"), ("Create a 10 item checklist", "a template"),
             ("Create a calendar for May", "a month to write in")]),
    ("Email", [("check my email", "a numbered list"), ("find emails about …", "up to 10 matches"),
               ("reply 2: …", "a reply draft"), ("2: shorter", "redo draft 2"), ("2: skip", "drop draft 2"),
               ("Send N", "the button: send them all")]),
]


# ------------------------------------------------------------------------------------------------------------ HTML
def rich(s):
    return re.sub(r"\{(.+?)\}", r'<span class="say">\1</span>', esc(s))


def write_box(items):
    rows = "".join(f'<li class="note">{esc(it[1:])}</li>' if it.startswith("~") else f"<li>{esc(it)}</li>" for it in items)
    return f'<div class="write"><div class="write-h">Write this</div><ul>{rows}</ul></div>'


def chrome_html(n, label):
    return (f'<div class="runhead">Claude on reMarkable · how to</div>'
            f'<div class="foot"><span>{esc(label)}</span><span class="pn">{n}</span></div>')


def header(title, ic):
    return f'<header><div class="ibox">{icon(ic)}</div><div><h1>{esc(title)}</h1></div></header>'


def feature_page(n, p):
    lines = "".join(f"<li>{rich(s)}</li>" for s in p["lines"])
    return (f'<section class="page">{chrome_html(n, p["title"])}{header(p["title"], p["icon"])}'
            f'<ul class="lead">{lines}</ul><div class="fig">{p["fig"]()}</div>{write_box(p["write"])}</section>')


def cover_page():
    return (f'<section class="page cover"><div class="runhead">Claude on reMarkable</div>'
            f'<div class="cover-ibox">{icon("asterisk")}</div>'
            f'<h1 class="cover-title">Claude in<br>your notebook</h1>'
            f'<p class="cover-sub">A how-to, one feature to a page</p>'
            f'<div class="fig">{fig_cover()}</div>'
            f'<ul class="lead">'
            f'<li>Write a question by hand, tap the Claude button, and the answer appears below your writing.</li>'
            f'<li>Each page shows one thing Claude can do, with requests to copy onto a notebook page of your own.</li>'
            f'</ul>'
            f'<div class="legend"><div>All of it runs on the tablet, over Wi-Fi, with your own Anthropic API key. '
            f'Email also needs a Gmail app password (INSTALL.md, section 8).</div></div></section>')


def contents_page(n, entries):
    rows = "".join(f'<li><span class="ci">{icon(ic)}</span><span class="ct">{esc(t)}</span>'
                   f'<span class="cd"></span><span class="cn">{pn}</span></li>' for pn, t, ic in entries)
    return (f'<section class="page">{chrome_html(n, "Contents")}{header("Contents", "list")}'
            f'<ul class="toc">{rows}</ul>'
            f'<p class="toc-note">Try each request on a notebook page in portrait, at normal zoom, with Wi-Fi on.</p></section>')


def quickref_page(n):
    groups = ""
    for title, rows in QUICKREF:
        items = "".join(f'<li><span class="say">{esc(a)}</span><span class="what">{esc(b)}</span></li>' for a, b in rows)
        groups += f'<div class="qgroup"><h2>{esc(title)}</h2><ul>{items}</ul></div>'
    return (f'<section class="page">{chrome_html(n, "Quick reference")}{header("Quick reference", "card")}'
            f'<div class="qref">{groups}</div></section>')


CSS = f"""
@font-face {{ font-family: 'EB Garamond'; src: url('fonts/EBGaramond-Regular.ttf'); font-weight: 400; font-style: normal; }}
@font-face {{ font-family: 'EB Garamond'; src: url('fonts/EBGaramond-SemiBold.ttf'); font-weight: 600 700; font-style: normal; }}
@font-face {{ font-family: 'EB Garamond'; src: url('fonts/EBGaramond-Italic.ttf'); font-weight: 400; font-style: italic; }}
@font-face {{ font-family: 'Caveat'; src: url('fonts/Caveat-Regular.ttf'); font-weight: 400; }}
@page {{ size: {PAGE_W}mm {PAGE_H}mm; margin: 0; }}
* {{ box-sizing: border-box; margin: 0; padding: 0; }}
html, body {{ background: #fff; color: #000; }}
body {{ font-family: 'EB Garamond'; font-size: 11.5pt; line-height: 1.28; font-kerning: normal;
       font-variant-numeric: lining-nums; -webkit-print-color-adjust: exact; print-color-adjust: exact; }}
.page {{ width: {PAGE_W}mm; height: {PAGE_H}mm; position: relative; overflow: hidden; break-after: page;
        padding: {TOP:.2f}mm {RIGHT:.2f}mm {BOTTOM}mm {LEFT:.2f}mm; display: flex; flex-direction: column; }}
.runhead {{ position: absolute; top: 9mm; left: {LEFT:.2f}mm; font-size: 10pt; font-style: italic; color: #444; }}
.foot {{ position: absolute; bottom: 6mm; left: {LEFT:.2f}mm; right: {RIGHT:.2f}mm; display: flex;
        justify-content: space-between; align-items: baseline; font-size: 10pt; color: #333; font-style: italic;
        border-top: .3mm solid #000; padding-top: 1.6mm; }}
.foot .pn {{ font-size: 13pt; font-weight: 600; font-style: normal; color: #000; }}
header {{ display: flex; align-items: center; gap: 4mm; margin-bottom: 4mm; }}
.ibox {{ width: 13mm; height: 13mm; border: .55mm solid #000; border-radius: 2.2mm; display: flex; align-items: center;
        justify-content: center; flex: none; }}
.ibox .icon {{ width: 8.5mm; height: 8.5mm; }}
h1 {{ font-size: 23pt; font-weight: 600; line-height: 1.05; }}
.say {{ font-family: 'Caveat'; font-size: 1.3em; line-height: 1; }}
ul.lead {{ list-style: none; flex: none; }}
ul.lead li {{ position: relative; padding-left: 4.5mm; margin-bottom: 1.4mm; }}
ul.lead li::before {{ content: ''; position: absolute; left: .4mm; top: 2.05mm; width: 1.4mm; height: 1.4mm; background: #000; }}
.fig {{ flex: 1 1 auto; min-height: 0; display: flex; align-items: center; justify-content: center; padding: 2.5mm 0 3mm; }}
.fig svg {{ width: 100%; height: 100%; }}
.write {{ border: .55mm solid #000; border-radius: 2.5mm; padding: 2mm 5mm 2.4mm; flex: none; }}
.write-h {{ font-size: 9pt; font-weight: 600; letter-spacing: .1em; text-transform: uppercase; margin-bottom: .2mm; }}
.write ul {{ list-style: none; }}
.write li {{ font-family: 'Caveat'; font-size: 18pt; line-height: 1.1; padding: .7mm 0 .4mm; border-bottom: .2mm solid #bbb; }}
.write li:last-child {{ border-bottom: none; }}
.write li.note {{ font-family: 'EB Garamond'; font-style: italic; font-size: 11pt; color: #333; padding: 1.3mm 0 .8mm; }}
/* HTML mock-up of printed drafts with a hand mark; drafts are printed in the handwriting face */
.mock {{ width: 100%; font-size: 11pt; line-height: 1.35; }}
.mock .struck {{ position: relative; white-space: nowrap; }}
.mock .struck::after {{ content: ''; position: absolute; left: -.8mm; right: -.8mm; top: 50%; border-top: .7mm solid #000;
                       transform: rotate(-3deg); }}
.mock .struck::before {{ content: attr(data-new); position: absolute; left: 0; bottom: 78%; font-family: 'Caveat';
                        font-size: 17pt; font-weight: 400; white-space: nowrap; }}
.drafts {{ position: relative; padding: 8.5mm 30mm 0 0; }}
.draft {{ position: relative; border: .35mm solid #000; border-radius: 1.5mm; padding: 2mm 3mm 2mm; margin-bottom: 2.5mm;
         font-family: 'Caveat'; font-size: 15pt; line-height: 1.15; }}
.draft b {{ font-weight: 700; }}
.draft .body {{ margin-top: .6mm; }}
.draft .body.marked {{ margin-top: 5mm; }}
.draft .fate {{ position: absolute; left: 100%; margin-left: 3mm; top: 50%; transform: translateY(-50%); font-style: italic;
               font-family: 'EB Garamond'; font-size: 10.5pt; white-space: nowrap; }}
.sendbtn {{ position: absolute; right: 0; top: 0; border: .45mm solid #000; border-radius: 1.2mm; padding: .6mm 2.5mm;
           font-weight: 600; font-size: 11pt; }}
/* cover */
.cover {{ padding-top: 34mm; }}
.cover-ibox {{ position: absolute; top: 34mm; right: {RIGHT:.2f}mm; width: 22mm; height: 22mm; border: .7mm solid #000;
              border-radius: 3.5mm; display: flex; align-items: center; justify-content: center; }}
.cover-ibox .icon {{ width: 15mm; height: 15mm; }}
.cover-title {{ font-size: 38pt; line-height: 1.0; font-weight: 600; }}
.cover-sub {{ font-size: 15pt; font-style: italic; margin-top: 3mm; color: #222; }}
.cover .fig {{ flex: 0 1 62mm; margin: 5mm 0 4mm; }}
.legend {{ margin-top: auto; border-top: .3mm solid #000; padding-top: 3mm; font-size: 11pt; }}
/* contents */
.toc {{ list-style: none; font-size: 11.5pt; }}
.toc li {{ display: flex; align-items: center; gap: 2.5mm; padding: .45mm 0; }}
.toc .ci .icon {{ width: 4.6mm; height: 4.6mm; display: block; }}
.toc .cd {{ flex: 1; border-bottom: .3mm dotted #666; margin: 0 1mm; transform: translateY(-.8mm); }}
.toc .cn {{ font-weight: 600; min-width: 6mm; text-align: right; }}
.toc-note {{ margin-top: auto; font-style: italic; font-size: 11pt; }}
/* quick reference */
.qref {{ display: grid; grid-template-columns: 1fr 1fr; column-gap: 7mm; row-gap: 4mm; }}
.qgroup h2 {{ font-size: 9pt; font-weight: 600; letter-spacing: .1em; text-transform: uppercase;
             border-bottom: .35mm solid #000; padding-bottom: .5mm; margin-bottom: 1mm; }}
.qgroup ul {{ list-style: none; }}
.qgroup li {{ margin-bottom: 1.3mm; line-height: 1.05; }}
.qgroup .say {{ display: block; font-size: 16pt; }}
.qgroup .what {{ display: block; font-size: 11pt; font-style: italic; color: #333; }}
"""


def build_html():
    entries = [(i, p["title"], p["icon"]) for i, p in enumerate(PAGES, 1) if p.get("title")]
    kinds = [p.get("kind") for p in PAGES]
    entries += [(kinds.index("quickref") + 1, "Quick reference", "card")]
    sections = []
    for i, p in enumerate(PAGES, 1):
        kind = p.get("kind")
        sections.append(cover_page() if kind == "cover" else contents_page(i, entries) if kind == "contents"
                        else quickref_page(i) if kind == "quickref" else feature_page(i, p))
    return (f'<!doctype html><html lang="en"><head><meta charset="utf-8"><title>Claude - how to</title>'
            f'<style>{CSS}</style></head><body>{"".join(sections)}</body></html>')


def find_chrome():
    if os.environ.get("CHROME"):
        return os.environ["CHROME"]
    for name in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "chrome"):
        if shutil.which(name):
            return shutil.which(name)
    mac = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")  # where macOS keeps it
    if mac.exists():
        return str(mac)
    sys.exit("No Chrome or Chromium found; set CHROME to its path")


def build_pdf(out):
    src = HERE / ".notebook.html"  # beside fonts/, so the font URLs resolve
    src.write_text(build_html(), encoding="utf-8")
    try:
        with tempfile.TemporaryDirectory() as profile:
            run = subprocess.run([find_chrome(), "--headless=new", "--disable-gpu", "--no-pdf-header-footer",
                                  f"--user-data-dir={profile}", "--allow-file-access-from-files",
                                  "--virtual-time-budget=5000", f"--print-to-pdf={out}", src.as_uri()],
                                 capture_output=True, text=True, timeout=180)
        if run.returncode or not out.exists() or out.stat().st_size == 0:
            sys.exit(f"Chrome made no PDF:\n{run.stderr[-2000:]}")
    finally:
        src.unlink(missing_ok=True)


def render_png(pdf, folder):
    try:
        import pymupdf
    except ImportError:
        sys.exit("--png needs pymupdf: uv run --with pymupdf docs/notebook/build.py --png DIR")
    folder.mkdir(parents=True, exist_ok=True)
    doc = pymupdf.open(pdf)
    for i, page in enumerate(doc, 1):
        zoom = 1404 / page.rect.width  # one page to the screen's 1404 px
        page.get_pixmap(matrix=pymupdf.Matrix(zoom, zoom), colorspace=pymupdf.csGRAY).save(folder / f"page-{i:02d}.png")
    fonts = sorted({f[3] for p in doc for f in p.get_fonts()})
    return len(doc), doc[0].rect, fonts


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=OUT)
    ap.add_argument("--png", type=Path, help="also render each page to this folder (needs pymupdf)")
    args = ap.parse_args()
    out = args.out.resolve()
    build_pdf(out)
    print(f"{out} ({out.stat().st_size // 1024} KB)")
    if args.png:
        n, r, fonts = render_png(out, args.png)
        print(f"{n} pages of {r.width / 72 * 25.4:.1f} x {r.height / 72 * 25.4:.1f} mm; PNGs in {args.png}")
        print("fonts:", ", ".join(fonts))


if __name__ == "__main__":
    main()
