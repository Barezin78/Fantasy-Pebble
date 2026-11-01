#!/usr/bin/env python3
"""Mockups do watchapp Fantasy NFL para Pebble Time 2 (emery: 200x228, 64 cores).
Só visual — sem código do app. Dados FICTÍCIOS de demonstração.
"""
from PIL import Image, ImageDraw, ImageFont
import json, os, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "mockups")
os.makedirs(OUT, exist_ok=True)
W, H, S = 200, 228, 4  # render 4x then downsample + snap to 64 palette
G = "/usr/share/fonts/truetype/sand-box/google/"

def font(path, size):
    return ImageFont.truetype(G + path, int(size * S))

# Gothic ≈ Pebble Gothic; Barlow Condensed ≈ Bitham numbers
GOTH  = lambda s: font("Gothic A1/GothicA1-Regular.ttf", s)
GOTHB = lambda s: font("Gothic A1/GothicA1-Bold.ttf", s)
GOTHX = lambda s: font("Gothic A1/GothicA1-Black.ttf", s)
BARB  = lambda s: font("Barlow Condensed/BarlowCondensed-Bold.ttf", s)
BARK  = lambda s: font("Barlow Condensed/BarlowCondensed-Black.ttf", s)
BARM  = lambda s: font("Barlow Condensed/BarlowCondensed-Medium.ttf", s)
BEBAS = lambda s: font("Bebas Neue/BebasNeue-Regular.ttf", s)

# ---- Pebble 64-colour palette (channels in {0,85,170,255}) ----
BLK, WHT = (0, 0, 0), (255, 255, 255)
DGRY, MGRY, LGRY = (85, 85, 85), (170, 170, 170), (170, 170, 170)
NAVY  = (0, 0, 85)
INK   = (0, 0, 0)
TEAL  = (0, 170, 170)
LTEAL = (85, 255, 255)
DKTEAL= (0, 85, 85)
ROSE  = (255, 85, 85)
GOLD  = (255, 170, 0)
GRN   = (0, 170, 0)
LGRN  = (85, 255, 85)
YEL   = (255, 255, 85)
BLUE  = (85, 170, 255)
PURP  = (170, 85, 255)
ORNG  = (255, 170, 85)
CARD  = (17, 17, 34)   # will snap → (0,0,85) or nearby; we use NAVY-ish
ROW   = (0, 0, 34)

POSC = {
    "QB": (255, 85, 170), "RB": TEAL, "WR": BLUE, "TE": ORNG,
    "FLEX": MGRY, "FLX": MGRY, "K": PURP, "DEF": (170, 170, 85),
    "W/R": MGRY, "SUPER_FLEX": MGRY,
}

DATA = json.load(open(os.path.join(OUT, "semana7-final.json")))
LA, LB = DATA["LA"], DATA["LB"]

def snap64(im):
    lut = [min((0, 85, 170, 255), key=lambda c: abs(c - v)) for v in range(256)]
    return im.point(lut * 3)

class C:
    def __init__(self, bg=BLK):
        self.im = Image.new("RGB", (W * S, H * S), bg)
        self.d = ImageDraw.Draw(self.im)

    def s(self, v):
        return [round(x * S) for x in v]

    def rect(self, b, fill=None, outline=None, w=1, r=0):
        if r:
            self.d.rounded_rectangle(self.s(b), r * S, fill=fill,
                                     outline=outline, width=round(w * S) if outline else 0)
        else:
            self.d.rectangle(self.s(b), fill=fill, outline=outline,
                             width=round(w * S) if outline else 0)

    def ell(self, b, fill=None, outline=None, w=1):
        self.d.ellipse(self.s(b), fill=fill, outline=outline,
                       width=round(w * S) if outline else 0)

    def line(self, pts, fill, w=1):
        self.d.line([(x * S, y * S) for x, y in pts], fill=fill, width=max(1, round(w * S)))

    def text(self, xy, t, f, fill, anchor="la"):
        self.d.text((xy[0] * S, xy[1] * S), str(t), font=f, fill=fill, anchor=anchor)

    def tw(self, t, f):
        b = self.d.textbbox((0, 0), str(t), font=f)
        return (b[2] - b[0]) / S

    def finish(self, name):
        im = self.im.resize((W, H), Image.LANCZOS)
        im = snap64(im)
        im.save(f"{OUT}/{name}.png")
        im2 = im.resize((W * 2, H * 2), Image.NEAREST)
        im2.save(f"{OUT}/{name}@2x.png")
        return im


def header(c, title, right=None, live=False):
    c.rect([0, 0, W, 22], fill=NAVY)
    c.text((6, 11), title, GOTHB(13), WHT, "lm")
    if live:
        c.ell([W - 78, 7, W - 70, 15], fill=ROSE)
        c.text((W - 66, 11), "AO VIVO", GOTHB(10), ROSE, "lm")
    elif right:
        c.text((W - 6, 11), right, GOTH(11), LTEAL, "rm")
    c.line([(0, 22), (W, 22)], TEAL, 1)


def footer_hint(c, txt, y=H - 14):
    c.text((W // 2, y), txt, GOTH(9), DGRY, "mm")


def fmt2(x):
    return f"{x:.2f}"


def truncate(c, text, f, maxw):
    t = str(text)
    if c.tw(t, f) <= maxw:
        return t
    while t and c.tw(t + "…", f) > maxw:
        t = t[:-1]
    return t + "…"


def short_opp(name):
    n = (name or "").replace("“", '"').replace("”", '"').replace("’", "'")
    if len(n) <= 14:
        return n
    # prefer first word + initial of rest, else truncate
    parts = n.split()
    if len(parts) >= 2:
        cand = parts[0] + " " + parts[1][0] + "."
        if len(cand) <= 14:
            return cand
        return parts[0][:14]
    return n[:13] + "…"


# ===================== 1. INÍCIO (semana encerrada) =====================
def home_final():
    c = C(BLK)
    header(c, "FANTASY", "Sem 7")
    y = 26
    for L, accent in ((LA, TEAL), (LB, BLUE)):
        eu, ad = L["eu"], L["adv"]
        lost = eu["pts"] < ad["pts"]
        c.rect([4, y, W - 4, y + 88], fill=NAVY, r=6)
        # league strip
        strip = DKTEAL if L is LA else (0, 0, 170)
        c.rect([4, y, W - 4, y + 18], fill=strip, r=6)
        c.rect([4, y + 10, W - 4, y + 18], fill=strip)
        c.text((10, y + 9), L["nome"], GOTHB(11), WHT, "lm")
        c.text((W - 10, y + 9), f"{eu['rec']} · {eu['rank']}º", GOTH(10), MGRY, "rm")
        # teams
        c.text((10, y + 24), "Blue Comets", GOTHB(11), TEAL, "lm")
        c.text((W - 10, y + 24), truncate(c, short_opp(ad["time"]), GOTHB(11), 88),
               GOTHB(11), MGRY, "rm")
        # scores
        sc_me = TEAL if not lost else WHT
        sc_ad = ROSE if lost else WHT
        c.text((10, y + 48), fmt2(eu["pts"]), BARK(28), sc_me, "lm")
        c.text((W // 2, y + 50), "x", GOTHB(14), DGRY, "mm")
        c.text((W - 10, y + 48), fmt2(ad["pts"]), BARK(28), sc_ad, "rm")
        # result chip
        chip = "DERROTA" if lost else "VITÓRIA"
        chip_c = ROSE if lost else GRN
        c.rect([10, y + 70, 10 + c.tw(chip, GOTHB(10)) + 10, y + 84], fill=chip_c, r=3)
        c.text((15, y + 77), chip, GOTHB(10), WHT, "lm")
        c.text((W - 10, y + 77), "final", GOTH(10), DGRY, "rm")
        y += 94
    footer_hint(c, "● Selecionar liga   ▼ tabela")
    return c.finish("1-inicio")


# ===================== 1b. INÍCIO AO VIVO (exemplo) =====================
def home_live():
    """Exemplo: como ficaria no domingo com jogos rolando (números ilustrativos)."""
    c = C(BLK)
    header(c, "FANTASY", live=True)
    # fake mid-game: LA leading slightly, LB trailing
    cards = [
        ("Liga Alfa", "LA", 62.10, 48.40, "3-3 · 7º", 0.61, "Pixel Foxes"),
        ("Liga Beta", "LB", 41.20, 72.80, "2-4 · 12º", 0.18, 'Neon Owls'),
    ]
    y = 26
    for i, (nome, sig, me, adv, rec, wp, opp) in enumerate(cards):
        c.rect([4, y, W - 4, y + 88], fill=NAVY, r=6)
        c.rect([4, y, W - 4, y + 18], fill=DKTEAL, r=6)
        c.rect([4, y + 10, W - 4, y + 18], fill=DKTEAL)
        c.text((10, y + 9), nome, GOTHB(11), WHT, "lm")
        c.ell([W - 56, y + 6, W - 48, y + 14], fill=ROSE)
        c.text((W - 10, y + 9), rec, GOTH(10), MGRY, "rm")
        c.text((10, y + 24), "Blue Comets", GOTHB(11), TEAL, "lm")
        c.text((W - 10, y + 24), opp, GOTHB(11), MGRY, "rm")
        lead = me >= adv
        c.text((10, y + 48), fmt2(me), BARK(28), TEAL if lead else WHT, "lm")
        c.text((W // 2, y + 50), "x", GOTHB(14), DGRY, "mm")
        c.text((W - 10, y + 48), fmt2(adv), BARK(28), ROSE if not lead else WHT, "rm")
        # win% bar
        c.text((10, y + 72), "vitória", GOTH(9), MGRY, "lm")
        bx0, bx1, by = 50, 150, y + 70
        c.rect([bx0, by, bx1, by + 10], fill=DGRY, r=2)
        fx = bx0 + max(2, int((bx1 - bx0) * wp))
        c.rect([bx0, by, fx, by + 10], fill=TEAL if wp >= 0.5 else ROSE, r=2)
        c.text((W - 10, y + 75), f"{int(wp*100)}%", GOTHB(11),
               TEAL if wp >= 0.5 else ROSE, "rm")
        y += 94
    c.text((W // 2, H - 10), "exemplo ao vivo · dados ilustrativos", GOTH(9), GOLD, "mm")
    return c.finish("1b-inicio-ao-vivo-EXEMPLO")


# ===================== 2. CONFRONTO (final) =====================
def confronto(L, fname):
    eu, ad = L["eu"], L["adv"]
    lost = eu["pts"] < ad["pts"]
    c = C(BLK)
    header(c, L["sigla"] + " · Sem 7", "FINAL")
    # teams
    c.text((W // 2, 34), "Blue Comets", GOTHB(13), TEAL, "mm")
    c.text((W // 2, 50), "vs " + truncate(c, ad["time"], GOTH(12), 170), GOTH(12), MGRY, "mm")
    # big scores
    c.text((52, 100), fmt2(eu["pts"]), BARK(36), TEAL if not lost else WHT, "mm")
    c.text((W // 2, 100), "–", GOTHB(22), DGRY, "mm")
    c.text((148, 100), fmt2(ad["pts"]), BARK(36), ROSE if lost else WHT, "mm")
    # result
    chip = "DERROTA" if lost else "VITÓRIA"
    chip_c = ROSE if lost else GRN
    tw = c.tw(chip, GOTHB(14)) + 20
    c.rect([W // 2 - tw / 2, 128, W // 2 + tw / 2, 148], fill=chip_c, r=4)
    c.text((W // 2, 138), chip, GOTHB(14), WHT, "mm")
    # record / rank
    c.rect([8, 160, W - 8, 200], fill=NAVY, r=6)
    c.text((20, 172), "Recorde", GOTH(11), MGRY, "lm")
    c.text((20, 188), eu["rec"], BARK(20), WHT, "lm")
    c.text((W - 20, 172), "Posição", GOTH(11), MGRY, "rm")
    c.text((W - 20, 188), f"{eu['rank']}º / {L['times']}", BARK(20), WHT, "rm")
    footer_hint(c, "● titulares   ▼ tabela   ◄ voltar")
    return c.finish(fname)


# ===================== 2b. CONFRONTO AO VIVO (exemplo) =====================
def confronto_live():
    c = C(BLK)
    header(c, "LA · Sem 7", live=True)
    c.text((W // 2, 34), "Blue Comets", GOTHB(13), TEAL, "mm")
    c.text((W // 2, 50), "vs Pixel Foxes Troopers", GOTH(12), MGRY, "mm")
    c.text((52, 96), "62.10", BARK(34), TEAL, "mm")
    c.text((W // 2, 96), "–", GOTHB(20), DGRY, "mm")
    c.text((148, 96), "48.40", BARK(34), WHT, "mm")
    # projections
    c.text((52, 118), "proj 118.2", GOTH(10), DGRY, "mm")
    c.text((148, 118), "proj 105.4", GOTH(10), DGRY, "mm")
    # win bar
    c.text((W // 2, 138), "61% vitória", GOTHB(13), TEAL, "mm")
    c.rect([20, 150, 180, 162], fill=DGRY, r=3)
    c.rect([20, 150, 20 + int(160 * 0.61), 162], fill=TEAL, r=3)
    c.text((20, 176), "3 a jogar", GOTH(11), MGRY, "lm")
    c.text((W - 20, 176), "adv 2 a jogar", GOTH(11), MGRY, "rm")
    c.text((W // 2, 200), "2 titulares em campo", GOTHB(11), ROSE, "mm")
    c.text((W // 2, H - 10), "exemplo ao vivo · dados ilustrativos", GOTH(9), GOLD, "mm")
    return c.finish("2b-confronto-ao-vivo-EXEMPLO")


# ===================== 3. TITULARES =====================
def titulares(L, fname, offset=0, max_rows=7):
    eu = L["eu"]
    c = C(BLK)
    header(c, "Titulares · " + L["sigla"], fmt2(eu["pts"]))
    # mini score strip
    ad = L["adv"]
    lost = eu["pts"] < ad["pts"]
    c.rect([0, 22, W, 40], fill=DKTEAL)
    c.text((6, 31), "Blue Comets", GOTHB(10), TEAL, "lm")
    c.text((W // 2, 31), f"{fmt2(eu['pts'])}  x  {fmt2(ad['pts'])}", BARM(13),
           ROSE if lost else WHT, "mm")
    c.text((W - 6, 31), "final", GOTH(9), MGRY, "rm")

    rows = eu["tit"][offset:offset + max_rows]
    y = 44
    rh = 24
    for i, p in enumerate(rows):
        yy = y + i * rh
        if i % 2 == 0:
            c.rect([0, yy, W, yy + rh], fill=(0, 0, 34))
        slot = "FLX" if p["slot"] in ("FLEX", "SUPER_FLEX") else p["slot"]
        col = POSC.get(p["slot"], POSC.get(slot, MGRY))
        c.rect([4, yy + 5, 34, yy + 19], fill=col, r=2)
        c.text((19, yy + 12), slot[:3], GOTHB(9), WHT, "mm")
        nome = truncate(c, p["nome"], GOTHB(12), 100)
        c.text((40, yy + 7), nome, GOTHB(12), WHT, "lm")
        team = p.get("time") or ""
        c.text((40, yy + 18), f"{team} · Final", GOTH(9), DGRY, "lm")
        c.text((W - 6, yy + 12), fmt2(p["pts"]), BARK(15),
               TEAL if p["pts"] >= 10 else WHT, "rm")
    # scroll hint
    total = len(eu["tit"])
    if offset + max_rows < total:
        c.text((W // 2, H - 10), f"▼ mais {total - offset - max_rows}  ·  {offset+1}–{offset+len(rows)}/{total}",
               GOTH(9), DGRY, "mm")
    elif offset > 0:
        c.text((W // 2, H - 10), f"▲  ·  {offset+1}–{offset+len(rows)}/{total}", GOTH(9), DGRY, "mm")
    else:
        footer_hint(c, "▼ rolar   ◄ voltar")
    return c.finish(fname)


def titulares_live_exemplo():
    """Lista com status de jogo misto — exemplo ao vivo."""
    c = C(BLK)
    header(c, "Titulares · LA", live=True)
    c.rect([0, 22, W, 40], fill=DKTEAL)
    c.text((6, 31), "Blue Comets", GOTHB(10), TEAL, "lm")
    c.text((W // 2, 31), "62.10  x  48.40", BARM(13), TEAL, "mm")
    c.text((W - 6, 31), "61%", GOTHB(10), TEAL, "rm")

    # mix of states — clearly example
    rows = [
        ("QB", "D. Maye", "NE", 28.44, "Final", "post"),
        ("RB", "J. Hill", "BAL", 4.80, "Final", "post"),
        ("RB", "M. Lloyd", "GB", 2.10, "2ºQ 8:42", "in"),
        ("WR", "G. Pickens", "DAL", 8.40, "1ºQ 3:10", "in"),
        ("WR", "J. Williams", "DET", 0.00, "dom 17h25", "pre"),
        ("TE", "G. Kittle", "SF", 0.00, "dom 20h25", "pre"),
        ("FLX", "D. Metcalf", "PIT", 6.60, "Final", "post"),
    ]
    y, rh = 44, 24
    for i, (slot, nome, team, pts, st, estado) in enumerate(rows):
        yy = y + i * rh
        if estado == "in":
            c.rect([0, yy, W, yy + rh], fill=(85, 0, 0))
        elif i % 2 == 0:
            c.rect([0, yy, W, yy + rh], fill=(0, 0, 34))
        col = POSC.get(slot, MGRY)
        c.rect([4, yy + 5, 34, yy + 19], fill=col, r=2)
        c.text((19, yy + 12), slot[:3], GOTHB(9), WHT, "mm")
        c.text((40, yy + 7), nome, GOTHB(12), WHT, "lm")
        stc = ROSE if estado == "in" else (DGRY if estado == "post" else MGRY)
        prefix = "● " if estado == "in" else ""
        c.text((40, yy + 18), f"{team} · {prefix}{st}", GOTH(9), stc, "lm")
        pts_c = TEAL if estado == "post" and pts >= 10 else (YEL if estado == "in" else MGRY)
        c.text((W - 6, yy + 12), fmt2(pts) if estado != "pre" else "–", BARK(15), pts_c, "rm")
    c.text((W // 2, H - 10), "exemplo ao vivo · dados ilustrativos", GOTH(9), GOLD, "mm")
    return c.finish("3c-titulares-ao-vivo-EXEMPLO")


# ===================== 4. TABELA =====================
def tabela(L, fname, focus_me=True):
    rows = L["standings"]
    me_idx = next(i for i, r in enumerate(rows) if r["me"])
    c = C(BLK)
    header(c, "Tabela · " + L["sigla"], f"{L['times']} times")
    # column headers
    c.rect([0, 22, W, 38], fill=DKTEAL)
    c.text((8, 30), "#", GOTHB(10), MGRY, "lm")
    c.text((28, 30), "Time", GOTHB(10), MGRY, "lm")
    c.text((148, 30), "V-D", GOTHB(10), MGRY, "mm")
    c.text((W - 8, 30), "PF", GOTHB(10), MGRY, "rm")

    # Show top 5 + separator + me (if me not in top 5), or window around me
    show = []
    if me_idx < 6:
        show = list(range(min(8, len(rows))))
        gap_before = None
    else:
        show = list(range(5)) + [me_idx - 1, me_idx, min(me_idx + 1, len(rows) - 1)]
        # unique preserve order
        seen, uniq = set(), []
        for i in show:
            if i not in seen:
                seen.add(i); uniq.append(i)
        show = uniq
        gap_before = me_idx - 1

    y, rh = 40, 20
    prev = -1
    for i in show:
        if gap_before is not None and i == gap_before and prev == 4:
            # dots separator
            c.text((W // 2, y + 6), "· · ·", GOTH(10), DGRY, "mm")
            y += 14
        r = rows[i]
        yy = y
        me = r["me"]
        if me:
            c.rect([0, yy, W, yy + rh], fill=DKTEAL)
        elif (i % 2) == 0:
            c.rect([0, yy, W, yy + rh], fill=(0, 0, 34))
        fg = TEAL if me else WHT
        ff = GOTHB(11) if me else GOTH(11)
        c.text((14, yy + rh // 2), str(r["rank"]), GOTHB(11) if me else GOTH(11), fg, "rm")
        team = truncate(c, r["team"].replace("“", '"').replace("”", '"'), ff, 110)
        c.text((28, yy + rh // 2), team, ff, fg, "lm")
        rec = f"{r['w']}-{r['l']}"
        c.text((148, yy + rh // 2), rec, GOTHB(11) if me else GOTH(11), fg, "mm")
        c.text((W - 8, yy + rh // 2), f"{r['pf']:.0f}", GOTH(11), fg if me else MGRY, "rm")
        y += rh
        prev = i
    footer_hint(c, "▲▼ rolar   ◄ voltar")
    return c.finish(fname)


# ===================== 5. SEM RODADA / OFF-SEASON =====================
def offseason():
    c = C(BLK)
    header(c, "FANTASY", "off")
    c.text((W // 2, 70), "Fora de temporada", GOTHB(14), WHT, "mm")
    c.text((W // 2, 92), "Nenhuma rodada ativa", GOTH(12), MGRY, "mm")
    c.rect([30, 120, W - 30, 170], fill=NAVY, r=8)
    c.text((W // 2, 138), "Última: Sem 7", GOTHB(12), TEAL, "mm")
    c.text((W // 2, 156), "LA 3-4 · LB 2-5", GOTH(11), MGRY, "mm")
    footer_hint(c, "atualiza via celular (PebbleKit JS)")
    return c.finish("5-off-season")


def sem_dados():
    c = C(BLK)
    header(c, "FANTASY")
    c.text((W // 2, 90), "Sem dados", GOTHB(16), WHT, "mm")
    c.text((W // 2, 114), "Abra o app no celular", GOTH(12), MGRY, "mm")
    c.text((W // 2, 132), "para sincronizar o Sleeper", GOTH(12), MGRY, "mm")
    c.rect([50, 160, W - 50, 186], fill=TEAL, r=6)
    c.text((W // 2, 173), "● sincronizar", GOTHB(12), BLK, "mm")
    return c.finish("5b-sem-dados")


# ===================== 6. EXTRA: linha no watchface Mengão =====================
def mengao_com_fantasy():
    """Watchface Mengão minimalista + faixa Fantasy (extra opcional)."""
    c = C(BLK)
    # time
    c.text((10, 4), "14", BEBAS(56), WHT, "la")
    c.text((10, 68), "37", BEBAS(56), (255, 0, 0), "la")
    c.line([(118, 14), (118, 130)], (255, 0, 0), 1.5)
    x = 128
    c.text((x, 18), "TER", BARB(14), LGRY, "lm")
    c.text((x, 38), "27", BARK(26), WHT, "lm")
    c.text((x, 58), "OUT", BARB(14), LGRY, "lm")
    c.line([(x, 74), (190, 74)], DGRY, 1)
    c.text((x, 88), "FLA", BARK(16), (255, 0, 0), "lm")
    c.text((x, 106), "líder 70 pts", GOTH(10), MGRY, "lm")
    # Fantasy strip at bottom
    c.rect([0, 148, W, H], fill=NAVY)
    c.line([(0, 148), (W, 148)], TEAL, 2)
    c.text((6, 160), "FANTASY", GOTHB(10), TEAL, "lm")
    c.text((W - 6, 160), "Sem 7 · final", GOTH(9), MGRY, "rm")
    c.text((6, 180), "LA", GOTHB(11), WHT, "lm")
    c.text((30, 180), "88.4", BARK(16), ROSE, "lm")
    c.text((70, 180), "x 91.3", GOTH(11), MGRY, "lm")
    c.text((6, 202), "LB", GOTHB(11), WHT, "lm")
    c.text((30, 202), "86.5", BARK(16), ROSE, "lm")
    c.text((70, 202), "x 104.3", GOTH(11), MGRY, "lm")
    c.text((W - 6, 190), "3-4", GOTHB(11), MGRY, "rm")
    c.text((W - 6, 208), "2-5", GOTHB(11), MGRY, "rm")
    return c.finish("6-extra-mengao-com-fantasy")


# ===================== OVERVIEW =====================
def overview(tiles):
    """tiles: list of (img, label, sub)"""
    cols = 4
    rows_n = math.ceil(len(tiles) / cols)
    sw, sh = W * 2, H * 2
    pad, bezel = 36, 28
    cw, ch = sw + 2 * bezel, sh + 2 * bezel
    label_h = 70
    title_h = 100
    sheet_w = cols * cw + (cols + 1) * pad
    sheet_h = title_h + rows_n * (ch + label_h + pad) + 120
    sheet = Image.new("RGB", (sheet_w, sheet_h), (24, 24, 32))
    d = ImageDraw.Draw(sheet)
    tf = ImageFont.truetype(G + "Barlow Condensed/BarlowCondensed-Black.ttf", 48)
    hf = ImageFont.truetype(G + "Gothic A1/GothicA1-Bold.ttf", 22)
    sf = ImageFont.truetype(G + "Gothic A1/GothicA1-Regular.ttf", 16)
    d.text((sheet_w // 2, 36), "FANTASY · Watchapp Pebble Time 2", font=tf, fill=WHT, anchor="mm")
    d.text((sheet_w // 2, 72),
           "200×228 · 64 cores · Sleeper (demo / Blue Comets) · Semana 7 final",
           font=sf, fill=MGRY, anchor="mm")

    for i, (im, label, sub) in enumerate(tiles):
        col, row = i % cols, i // cols
        x = pad + col * (cw + pad)
        y = title_h + row * (ch + label_h + pad)
        # bezel
        d.rounded_rectangle([x, y, x + cw, y + ch], 36, fill=(10, 10, 14),
                            outline=(80, 80, 90), width=4)
        # screen
        d.rounded_rectangle([x + bezel - 4, y + bezel - 4,
                             x + bezel + sw + 4, y + bezel + sh + 4], 8, fill=BLK)
        big = im.resize((sw, sh), Image.NEAREST)
        sheet.paste(big, (x + bezel, y + bezel))
        d.text((x + cw / 2, y + ch + 22), label, font=hf, fill=WHT, anchor="mm")
        if sub:
            d.text((x + cw / 2, y + ch + 46), sub, font=sf, fill=TEAL if "exemplo" not in sub.lower() else GOLD, anchor="mm")

    # button flow legend
    ly = sheet_h - 100
    d.rectangle([pad, ly, sheet_w - pad, sheet_h - 20], fill=(16, 16, 24))
    flow = ("Fluxo de botões:  Início  —●→  Confronto da liga  —●→  Titulares  —▼→  (rolar)   |   "
            "Confronto —▼→ Tabela   |   ◄ Voltar sobe um nível   |   "
            "Dados via PebbleKit JS no celular (Sleeper API)")
    # wrap manually
    d.text((pad + 16, ly + 18), "Fluxo de botões", font=hf, fill=TEAL, anchor="lm")
    lines = [
        "Início  —● Selecionar→  Confronto  —●→  Titulares  —▼/▲→  rolar lista",
        "Confronto  —▼→  Tabela da liga (▲▼ rolar)     ◄ Voltar = sobe um nível",
        "Atualização: PebbleKit JS no celular busca o Sleeper; no relógio fica o cache. Em jogos, refresh ~15–30 min (orçamento do Pebble).",
    ]
    for j, line in enumerate(lines):
        d.text((pad + 16, ly + 44 + j * 18), line, font=sf, fill=MGRY, anchor="lm")

    sheet.save(f"{OUT}/overview.png")
    # also a compact telas-2x strip of the main 5
    return sheet


def telas_2x(ims_labels):
    """Horizontal strip of main screens at 2x."""
    sw, sh = W * 2, H * 2
    pad = 20
    n = len(ims_labels)
    img = Image.new("RGB", (n * (sw + pad) + pad, sh + 60), (24, 24, 32))
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(G + "Gothic A1/GothicA1-Bold.ttf", 18)
    for i, (im, lab) in enumerate(ims_labels):
        x = pad + i * (sw + pad)
        img.paste(im.resize((sw, sh), Image.NEAREST), (x, 10))
        d.text((x + sw / 2, sh + 30), lab, font=f, fill=WHT, anchor="mm")
    img.save(f"{OUT}/telas-2x.png")


def main():
    print("gerando mockups…")
    tiles = []

    im = home_final()
    tiles.append((im, "1 · Início", "Sem 7 final · duas ligas"))

    im = confronto(LA, "2-confronto-la")
    tiles.append((im, "2 · Confronto LA", "88,44 x 81,20 · vitória"))

    im = confronto(LB, "2c-confronto-lb")
    tiles.append((im, "2c · Confronto LB", "72,10 x 79,80 · derrota"))

    im = titulares(LA, "3-titulares-la", offset=0)
    tiles.append((im, "3 · Titulares LA", "lista rolável"))

    im = titulares(LA, "3b-titulares-la-rolada", offset=3)
    tiles.append((im, "3b · Titulares (rolada)", "▼ continua"))

    im = tabela(LA, "4-tabela-la")
    tiles.append((im, "4 · Tabela LA", "Blue Comets 3º · 4-2"))

    im = tabela(LB, "4b-tabela-lb")
    tiles.append((im, "4b · Tabela LB", "Blue Comets 5º · 3-3"))

    im = home_live()
    tiles.append((im, "1b · Início ao vivo", "EXEMPLO ilustrativo"))

    im = confronto_live()
    tiles.append((im, "2b · Confronto ao vivo", "EXEMPLO ilustrativo"))

    im = titulares_live_exemplo()
    tiles.append((im, "3c · Titulares ao vivo", "EXEMPLO ilustrativo"))

    im = offseason()
    tiles.append((im, "5 · Off-season", "fora de temporada"))

    im = sem_dados()
    tiles.append((im, "5b · Sem dados", "precisa sincronizar"))

    im = mengao_com_fantasy()
    tiles.append((im, "6 · Extra Mengão", "faixa Fantasy no watchface"))

    overview(tiles)
    telas_2x([
        (tiles[0][0], "Início"),
        (tiles[1][0], "Confronto"),
        (tiles[3][0], "Titulares"),
        (tiles[5][0], "Tabela"),
        (tiles[7][0], "Ao vivo*"),
        (tiles[10][0], "Off-season"),
    ])
    print("ok →", OUT)
    for f in sorted(os.listdir(OUT)):
        if f.endswith(".png"):
            p = os.path.join(OUT, f)
            print(f"  {f}  ({os.path.getsize(p)} bytes)")


if __name__ == "__main__":
    main()
