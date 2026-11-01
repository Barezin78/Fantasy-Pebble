/* Fantasy — watchapp Pebble Time 2 (Sleeper) v1.0.2
 * Telas: Início → Confronto → Titulares | Confronto → Tabela
 * Voltar sobe. Dados via PebbleKit JS (Sleeper) + persist.
 */
#include <pebble.h>
#include <stdio.h>
#include <string.h>
#include "data.h"

#define C_TEAL   PBL_IF_COLOR_ELSE(GColorTiffanyBlue, GColorWhite)
#define C_NAVY   PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define C_DKTEAL PBL_IF_COLOR_ELSE(GColorCadetBlue, GColorDarkGray)
#define C_ROSE   PBL_IF_COLOR_ELSE(GColorFolly, GColorLightGray)
#define C_GOLD   PBL_IF_COLOR_ELSE(GColorChromeYellow, GColorWhite)
#define C_GRN    PBL_IF_COLOR_ELSE(GColorIslamicGreen, GColorWhite)
#define C_LIVEBG PBL_IF_COLOR_ELSE(GColorDarkCandyAppleRed, GColorDarkGray)

static Window *s_win;
static Layer *s_layer;
static int s_screen = SCR_HOME;
static int s_sel = 0;          /* liga selecionada no início */
static int s_scroll = 0;       /* offset em titulares / tabela */
static AppTimer *s_refresh_timer;

/* ---------- formatters ---------- */
static void fmt_pts(char *buf, size_t n, int16_t pts100) {
  int whole = pts100 / 100;
  int frac = pts100 % 100;
  if (frac < 0) frac = -frac;
  if (pts100 < 0 && whole == 0)
    snprintf(buf, n, "-0.%02d", frac);
  else
    snprintf(buf, n, "%d.%02d", whole, frac);
}

static GColor slot_color(const char *slot) {
  if (!slot) return GColorDarkGray;
  if (strcmp(slot, "QB") == 0) return PBL_IF_COLOR_ELSE(GColorFashionMagenta, GColorLightGray);
  if (strcmp(slot, "RB") == 0) return C_TEAL;
  if (strcmp(slot, "WR") == 0) return PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite);
  if (strcmp(slot, "TE") == 0) return PBL_IF_COLOR_ELSE(GColorOrange, GColorLightGray);
  if (strcmp(slot, "K") == 0)  return PBL_IF_COLOR_ELSE(GColorPurple, GColorLightGray);
  if (strcmp(slot, "DEF") == 0) return PBL_IF_COLOR_ELSE(GColorBrass, GColorLightGray);
  return GColorDarkGray; /* FLX */
}

static void draw_header(GContext *ctx, const char *left, const char *right, bool live) {
  GRect b = layer_get_bounds(s_layer);
  graphics_context_set_fill_color(ctx, C_NAVY);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, 22), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, left, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(4, 0, b.size.w - 8, 22), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  if (live) {
    graphics_context_set_fill_color(ctx, C_ROSE);
    graphics_fill_circle(ctx, GPoint(b.size.w - 62, 11), 3);
    graphics_context_set_text_color(ctx, C_ROSE);
    graphics_draw_text(ctx, "AO VIVO", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(b.size.w - 56, 0, 52, 22), GTextOverflowModeWordWrap,
                       GTextAlignmentLeft, NULL);
  } else if (right && right[0]) {
    graphics_context_set_text_color(ctx, C_TEAL);
    graphics_draw_text(ctx, right, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(4, 0, b.size.w - 8, 22), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }
  graphics_context_set_stroke_color(ctx, C_TEAL);
  graphics_draw_line(ctx, GPoint(0, 22), GPoint(b.size.w, 22));
}

static void draw_footer(GContext *ctx, const char *txt) {
  GRect b = layer_get_bounds(s_layer);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, txt, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(2, b.size.h - 16, b.size.w - 4, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

/* ---------- HOME ---------- */
static void draw_home(GContext *ctx) {
  FantasyData *d = data_get();
  GRect b = layer_get_bounds(s_layer);
  bool live = d->state == APP_STATE_LIVE;
  char right[12];
  if (d->state == APP_STATE_OFF) snprintf(right, sizeof(right), "off");
  else if (d->state == APP_STATE_NODATA) right[0] = 0;
  else snprintf(right, sizeof(right), "Sem %d", d->week);

  if (d->state == APP_STATE_NODATA || d->n_leagues == 0) {
    draw_header(ctx, "FANTASY", NULL, false);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "Sem dados", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                       GRect(0, 70, b.size.w, 30), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "Abra o app no celular\npara sincronizar",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(10, 110, b.size.w - 20, 50), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
    graphics_context_set_fill_color(ctx, C_TEAL);
    graphics_fill_rect(ctx, GRect(50, 165, b.size.w - 100, 26), 4, GCornersAll);
    graphics_context_set_text_color(ctx, GColorBlack);
    graphics_draw_text(ctx, "sincronizar", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(50, 166, b.size.w - 100, 24), GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
    return;
  }

  if (d->state == APP_STATE_OFF) {
    draw_header(ctx, "FANTASY", "off", false);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, "Fora de temporada", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, 60, b.size.w, 24), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "Nenhuma rodada ativa", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0, 88, b.size.w, 20), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_fill_color(ctx, C_NAVY);
    graphics_fill_rect(ctx, GRect(20, 120, b.size.w - 40, 50), 6, GCornersAll);
    graphics_context_set_text_color(ctx, C_TEAL);
    char buf[40];
    snprintf(buf, sizeof(buf), "Última: Sem %d", d->week);
    graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(20, 128, b.size.w - 40, 18), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    if (d->n_leagues >= 2) {
      snprintf(buf, sizeof(buf), "%s %d-%d · %s %d-%d",
               d->leagues[0].abbr, d->leagues[0].w, d->leagues[0].l,
               d->leagues[1].abbr, d->leagues[1].w, d->leagues[1].l);
      graphics_context_set_text_color(ctx, GColorLightGray);
      graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(20, 148, b.size.w - 40, 18), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
    draw_footer(ctx, "atualiza via celular");
    return;
  }

  draw_header(ctx, "FANTASY", right, live);

  int y = 26;
  int card_h = (b.size.h - 42) / 2;
  if (card_h > 94) card_h = 94;
  for (int i = 0; i < d->n_leagues && i < MAX_LEAGUES; i++) {
    League *L = &d->leagues[i];
    bool sel = (i == s_sel);
    bool lost = L->my_pts100 < L->opp_pts100;
    bool won = L->my_pts100 > L->opp_pts100;
    GRect card = GRect(4, y, b.size.w - 8, card_h - 4);
    graphics_context_set_fill_color(ctx, C_NAVY);
    graphics_fill_rect(ctx, card, 4, GCornersAll);
    if (sel) {
      graphics_context_set_stroke_color(ctx, C_TEAL);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_round_rect(ctx, card, 4);
      graphics_context_set_stroke_width(ctx, 1);
    }
    /* strip */
    graphics_context_set_fill_color(ctx, i == 0 ? C_DKTEAL : PBL_IF_COLOR_ELSE(GColorBlue, GColorDarkGray));
    graphics_fill_rect(ctx, GRect(card.origin.x, card.origin.y, card.size.w, 16), 4, GCornersTop);
    graphics_fill_rect(ctx, GRect(card.origin.x, card.origin.y + 8, card.size.w, 8), 0, GCornerNone);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, L->name, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(card.origin.x + 4, card.origin.y - 2, 110, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    char rec[20];
    snprintf(rec, sizeof(rec), "%d-%d · %dº", L->w, L->l, L->rank);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, rec, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(card.origin.x + 4, card.origin.y - 2, card.size.w - 8, 18),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    graphics_context_set_text_color(ctx, C_TEAL);
    graphics_draw_text(ctx, L->my_name[0] ? L->my_name : "Blue Comets",
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(card.origin.x + 4, card.origin.y + 16, card.size.w / 2 - 4, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, L->opp_name,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(card.origin.x + card.size.w / 2, card.origin.y + 16, card.size.w / 2 - 4, 16),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    char me_s[12], opp_s[12];
    fmt_pts(me_s, sizeof(me_s), L->my_pts100);
    fmt_pts(opp_s, sizeof(opp_s), L->opp_pts100);
    graphics_context_set_text_color(ctx, (won || (!lost && !won)) ? C_TEAL : GColorWhite);
    if (lost) graphics_context_set_text_color(ctx, GColorWhite);
    if (!lost && L->state != LSTATE_FINAL) graphics_context_set_text_color(ctx, C_TEAL);
    graphics_draw_text(ctx, me_s, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                       GRect(card.origin.x + 2, card.origin.y + 28, 78, 32),
                       GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "x", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(card.origin.x, card.origin.y + 34, card.size.w, 20),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, lost ? C_ROSE : GColorWhite);
    graphics_draw_text(ctx, opp_s, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                       GRect(card.origin.x + card.size.w - 80, card.origin.y + 28, 78, 32),
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

    if (L->state == LSTATE_FINAL) {
      const char *chip = lost ? "DERROTA" : (won ? "VITORIA" : "EMPATE");
      GColor chip_c = lost ? C_ROSE : (won ? C_GRN : GColorDarkGray);
      graphics_context_set_fill_color(ctx, chip_c);
      graphics_fill_rect(ctx, GRect(card.origin.x + 6, card.origin.y + card.size.h - 22, 60, 14), 2, GCornersAll);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, chip, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         GRect(card.origin.x + 6, card.origin.y + card.size.h - 24, 60, 16),
                         GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      graphics_context_set_text_color(ctx, GColorDarkGray);
      graphics_draw_text(ctx, "final", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                         GRect(card.origin.x + 4, card.origin.y + card.size.h - 24, card.size.w - 8, 16),
                         GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    } else if (L->state == LSTATE_LIVE) {
      /* win% bar */
      int bx = card.origin.x + 6, bw = card.size.w - 50, by = card.origin.y + card.size.h - 18;
      graphics_context_set_fill_color(ctx, GColorDarkGray);
      graphics_fill_rect(ctx, GRect(bx, by, bw, 8), 2, GCornersAll);
      int fw = (bw * L->wp) / 100; if (fw < 2) fw = 2;
      graphics_context_set_fill_color(ctx, L->wp >= 50 ? C_TEAL : C_ROSE);
      graphics_fill_rect(ctx, GRect(bx, by, fw, 8), 2, GCornersAll);
      char wp[8]; snprintf(wp, sizeof(wp), "%d%%", L->wp);
      graphics_context_set_text_color(ctx, L->wp >= 50 ? C_TEAL : C_ROSE);
      graphics_draw_text(ctx, wp, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         GRect(bx + bw + 2, by - 6, 40, 16), GTextOverflowModeWordWrap,
                         GTextAlignmentLeft, NULL);
    }
    y += card_h;
  }
  draw_footer(ctx, "* liga   v tabela");
}

/* ---------- MATCHUP ---------- */
static void draw_matchup(GContext *ctx) {
  FantasyData *d = data_get();
  if (s_sel < 0 || s_sel >= d->n_leagues) return;
  League *L = &d->leagues[s_sel];
  GRect b = layer_get_bounds(s_layer);
  bool live = L->state == LSTATE_LIVE;
  bool lost = L->my_pts100 < L->opp_pts100;
  bool won = L->my_pts100 > L->opp_pts100;
  char title[20], right[12];
  snprintf(title, sizeof(title), "%s · Sem %d", L->abbr, d->week);
  if (L->state == LSTATE_FINAL) snprintf(right, sizeof(right), "FINAL");
  else if (live) right[0] = 0;
  else snprintf(right, sizeof(right), "pré");
  draw_header(ctx, title, right, live);

  graphics_context_set_text_color(ctx, C_TEAL);
  graphics_draw_text(ctx, L->my_name[0] ? L->my_name : "Blue Comets",
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(0, 28, b.size.w, 20), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  char vs[40]; snprintf(vs, sizeof(vs), "vs %s", L->opp_name);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, vs, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(4, 48, b.size.w - 8, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  char me_s[12], opp_s[12];
  fmt_pts(me_s, sizeof(me_s), L->my_pts100);
  fmt_pts(opp_s, sizeof(opp_s), L->opp_pts100);
  graphics_context_set_text_color(ctx, lost ? GColorWhite : C_TEAL);
  graphics_draw_text(ctx, me_s, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(2, 70, 90, 34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, GColorDarkGray);
  graphics_draw_text(ctx, "-", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(0, 74, b.size.w, 28), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, lost ? C_ROSE : GColorWhite);
  graphics_draw_text(ctx, opp_s, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                     GRect(b.size.w - 92, 70, 90, 34), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  if (L->state == LSTATE_LIVE && (L->my_proj10 || L->opp_proj10)) {
    char p1[16], p2[16];
    snprintf(p1, sizeof(p1), "proj %d.%d", L->my_proj10 / 10, L->my_proj10 % 10);
    snprintf(p2, sizeof(p2), "proj %d.%d", L->opp_proj10 / 10, L->opp_proj10 % 10);
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, p1, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(2, 108, 88, 16), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, p2, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(b.size.w - 90, 108, 88, 16), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  if (L->state == LSTATE_FINAL) {
    const char *chip = lost ? "DERROTA" : (won ? "VITORIA" : "EMPATE");
    GColor chip_c = lost ? C_ROSE : (won ? C_GRN : GColorDarkGray);
    int cw = 80;
    graphics_context_set_fill_color(ctx, chip_c);
    graphics_fill_rect(ctx, GRect((b.size.w - cw) / 2, 118, cw, 20), 4, GCornersAll);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, chip, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect((b.size.w - cw) / 2, 116, cw, 22), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else if (L->state == LSTATE_LIVE) {
    char wp[24]; snprintf(wp, sizeof(wp), "%d%% vitória", L->wp);
    graphics_context_set_text_color(ctx, L->wp >= 50 ? C_TEAL : C_ROSE);
    graphics_draw_text(ctx, wp, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                       GRect(0, 118, b.size.w, 22), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    int bx = 20, bw = b.size.w - 40, by = 142;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(bx, by, bw, 10), 3, GCornersAll);
    int fw = (bw * L->wp) / 100; if (fw < 2) fw = 2;
    graphics_context_set_fill_color(ctx, L->wp >= 50 ? C_TEAL : C_ROSE);
    graphics_fill_rect(ctx, GRect(bx, by, fw, 10), 3, GCornersAll);
    char lefts[40];
    snprintf(lefts, sizeof(lefts), "%d a jogar          adv %d a jogar", L->my_left, L->opp_left);
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, lefts, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(8, 156, b.size.w - 16, 18), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  /* record box */
  graphics_context_set_fill_color(ctx, C_NAVY);
  graphics_fill_rect(ctx, GRect(8, b.size.h - 68, b.size.w - 16, 40), 4, GCornersAll);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "Recorde", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(16, b.size.h - 66, 80, 16), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "Posicao", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(b.size.w - 96, b.size.h - 66, 80, 16), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  char rec[12], pos[16];
  snprintf(rec, sizeof(rec), "%d-%d", L->w, L->l);
  snprintf(pos, sizeof(pos), "%dº / %d", L->rank, L->nteams);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, rec, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(16, b.size.h - 50, 80, 22), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, pos, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(b.size.w - 96, b.size.h - 50, 80, 22), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

  draw_footer(ctx, "* titulares   v tabela");
}

/* ---------- STARTERS ---------- */
static void draw_starters(GContext *ctx) {
  FantasyData *d = data_get();
  if (s_sel < 0 || s_sel >= d->n_leagues) return;
  League *L = &d->leagues[s_sel];
  GRect b = layer_get_bounds(s_layer);
  bool live = L->state == LSTATE_LIVE;
  char title[24], pts[12];
  snprintf(title, sizeof(title), "Titulares · %s", L->abbr);
  fmt_pts(pts, sizeof(pts), L->my_pts100);
  draw_header(ctx, title, live ? NULL : pts, live);

  /* score strip — "EU 88.44 x 81.20 · final" (evita sobrepor Blue Comets + placar) */
  graphics_context_set_fill_color(ctx, C_DKTEAL);
  graphics_fill_rect(ctx, GRect(0, 22, b.size.w, 18), 0, GCornerNone);
  char sc[48], me_s[12], opp_s[12];
  fmt_pts(me_s, sizeof(me_s), L->my_pts100);
  fmt_pts(opp_s, sizeof(opp_s), L->opp_pts100);
  const char *stlabel = L->state == LSTATE_FINAL ? "final" : (live ? "vivo" : "pré");
  snprintf(sc, sizeof(sc), "EU %s x %s · %s", me_s, opp_s, stlabel);
  bool lost = L->my_pts100 < L->opp_pts100;
  graphics_context_set_text_color(ctx, lost ? C_ROSE : GColorWhite);
  graphics_draw_text(ctx, sc, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(2, 20, b.size.w - 4, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  int row_h = 28;  /* nome + sub sem sobrepor a linha de baixo */
  int visible = (b.size.h - 44 - 14) / row_h;
  if (visible < 1) visible = 1;
  int max_off = L->n_starters > visible ? L->n_starters - visible : 0;
  if (s_scroll > max_off) s_scroll = max_off;
  if (s_scroll < 0) s_scroll = 0;

  for (int i = 0; i < visible; i++) {
    int idx = s_scroll + i;
    if (idx >= L->n_starters) break;
    Starter *s = &L->starters[idx];
    int yy = 42 + i * row_h;
    if (s->status == P_IN) {
      graphics_context_set_fill_color(ctx, C_LIVEBG);
      graphics_fill_rect(ctx, GRect(0, yy, b.size.w, row_h), 0, GCornerNone);
    } else if (i % 2 == 0) {
      graphics_context_set_fill_color(ctx, C_NAVY);
      graphics_fill_rect(ctx, GRect(0, yy, b.size.w, row_h), 0, GCornerNone);
    }
    /* slot badge */
    graphics_context_set_fill_color(ctx, slot_color(s->slot));
    graphics_fill_rect(ctx, GRect(3, yy + 6, 30, 16), 2, GCornersAll);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s->slot, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(3, yy + 4, 30, 18), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, s->name, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(38, yy + 1, 100, 14), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    char sub[24];
    if (s->team[0] && s->sub[0]) snprintf(sub, sizeof(sub), "%s · %s", s->team, s->sub);
    else if (s->sub[0]) snprintf(sub, sizeof(sub), "%s", s->sub);
    else snprintf(sub, sizeof(sub), "%s", s->team);
    graphics_context_set_text_color(ctx, s->status == P_IN ? C_ROSE : GColorDarkGray);
    graphics_draw_text(ctx, sub, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(38, yy + 14, 110, 14), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    if (s->status != P_PRE && s->status != P_BYE) {
      char p[12]; fmt_pts(p, sizeof(p), s->pts100);
      GColor pc = (s->status == P_IN) ? C_GOLD : (s->pts100 >= 1000 ? C_TEAL : GColorWhite);
      graphics_context_set_text_color(ctx, pc);
      graphics_draw_text(ctx, p, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(b.size.w - 56, yy + 4, 52, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    } else {
      graphics_context_set_text_color(ctx, GColorDarkGray);
      graphics_draw_text(ctx, "-", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                         GRect(b.size.w - 40, yy + 4, 36, 20), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    }
  }
  char foot[32];
  if (max_off > 0)
    snprintf(foot, sizeof(foot), "v %d-%d/%d", s_scroll + 1,
             s_scroll + (visible < L->n_starters - s_scroll ? visible : L->n_starters - s_scroll),
             L->n_starters);
  else
    snprintf(foot, sizeof(foot), "< voltar");
  draw_footer(ctx, foot);
}

/* ---------- TABLE ---------- */
static void draw_table(GContext *ctx) {
  FantasyData *d = data_get();
  if (s_sel < 0 || s_sel >= d->n_leagues) return;
  League *L = &d->leagues[s_sel];
  GRect b = layer_get_bounds(s_layer);
  char title[24], right[16];
  snprintf(title, sizeof(title), "Tabela · %s", L->abbr);
  snprintf(right, sizeof(right), "%d times", L->nteams);
  draw_header(ctx, title, right, false);

  graphics_context_set_fill_color(ctx, C_DKTEAL);
  graphics_fill_rect(ctx, GRect(0, 22, b.size.w, 16), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorLightGray);
  graphics_draw_text(ctx, "#", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(0, 20, 20, 16), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  graphics_draw_text(ctx, "Time", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(26, 20, 80, 16), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "V-D", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(b.size.w - 70, 20, 30, 16), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "PF", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(b.size.w - 36, 20, 32, 16), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);

  int row_h = 18;
  int visible = (b.size.h - 40 - 14) / row_h;
  if (visible < 1) visible = 1;
  int max_off = L->n_stand > visible ? L->n_stand - visible : 0;
  if (s_scroll > max_off) s_scroll = max_off;
  if (s_scroll < 0) s_scroll = 0;

  for (int i = 0; i < visible; i++) {
    int idx = s_scroll + i;
    if (idx >= L->n_stand) break;
    StandRow *r = &L->stand[idx];
    int yy = 40 + i * row_h;
    if (r->me) {
      graphics_context_set_fill_color(ctx, C_DKTEAL);
      graphics_fill_rect(ctx, GRect(0, yy, b.size.w, row_h), 0, GCornerNone);
    } else if (i % 2 == 0) {
      graphics_context_set_fill_color(ctx, C_NAVY);
      graphics_fill_rect(ctx, GRect(0, yy, b.size.w, row_h), 0, GCornerNone);
    }
    GColor fg = GColorWhite;
    GFont f = fonts_get_system_font(r->me ? FONT_KEY_GOTHIC_14_BOLD : FONT_KEY_GOTHIC_14);
    graphics_context_set_text_color(ctx, fg);
    char rk[4]; snprintf(rk, sizeof(rk), "%d", r->rank);
    /* rank right-aligned; nome em x=26 → espaço claro entre "#" e o time */
    graphics_draw_text(ctx, rk, f, GRect(0, yy - 2, 20, row_h + 2),
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    graphics_draw_text(ctx, r->name, f, GRect(26, yy - 2, b.size.w - 104, row_h + 2),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    char rec[10]; snprintf(rec, sizeof(rec), "%d-%d", r->w, r->l);
    graphics_draw_text(ctx, rec, f, GRect(b.size.w - 70, yy - 2, 30, row_h + 2),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    char pf[8]; snprintf(pf, sizeof(pf), "%d", r->pf);
    graphics_context_set_text_color(ctx, r->me ? GColorWhite : GColorLightGray);
    graphics_draw_text(ctx, pf, f, GRect(b.size.w - 36, yy - 2, 32, row_h + 2),
                       GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
  }
  draw_footer(ctx, max_off > 0 ? "^v rolar   < voltar" : "< voltar");
}

/* ---------- layer / input ---------- */
static void layer_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  switch (s_screen) {
    case SCR_HOME: draw_home(ctx); break;
    case SCR_MATCHUP: draw_matchup(ctx); break;
    case SCR_STARTERS: draw_starters(ctx); break;
    case SCR_TABLE: draw_table(ctx); break;
  }
}

void fantasy_data_updated(void) {
  if (s_layer) layer_mark_dirty(s_layer);
  /* schedule periodic refresh while live */
  FantasyData *d = data_get();
  if (s_refresh_timer) { app_timer_cancel(s_refresh_timer); s_refresh_timer = NULL; }
  if (d->state == APP_STATE_LIVE) {
    s_refresh_timer = app_timer_register(15 * 60 * 1000, (AppTimerCallback)data_request_sync, NULL);
  }
}

static void select_click(ClickRecognizerRef r, void *ctx) {
  FantasyData *d = data_get();
  if (s_screen == SCR_HOME) {
    if (d->state == APP_STATE_NODATA || d->n_leagues == 0) {
      data_request_sync();
      return;
    }
    if (d->state == APP_STATE_OFF) return;
    s_screen = SCR_MATCHUP;
    s_scroll = 0;
  } else if (s_screen == SCR_MATCHUP) {
    s_screen = SCR_STARTERS;
    s_scroll = 0;
  }
  layer_mark_dirty(s_layer);
}

static void up_click(ClickRecognizerRef r, void *ctx) {
  FantasyData *d = data_get();
  if (s_screen == SCR_HOME) {
    if (s_sel > 0) s_sel--;
  } else if (s_screen == SCR_STARTERS || s_screen == SCR_TABLE) {
    if (s_scroll > 0) s_scroll--;
  }
  (void)d;
  layer_mark_dirty(s_layer);
}

static void down_click(ClickRecognizerRef r, void *ctx) {
  FantasyData *d = data_get();
  if (s_screen == SCR_HOME) {
    if (d->n_leagues > 0 && s_sel < d->n_leagues - 1) s_sel++;
  } else if (s_screen == SCR_MATCHUP) {
    s_screen = SCR_TABLE;
    s_scroll = 0;
    /* centraliza o Blue Comets na janela (ex.: LB 5º) */
    {
      FantasyData *dd = data_get();
      if (s_sel >= 0 && s_sel < dd->n_leagues) {
        League *LL = &dd->leagues[s_sel];
        int me = -1;
        for (int i = 0; i < LL->n_stand; i++) if (LL->stand[i].me) { me = i; break; }
        if (me >= 0) {
          GRect bb = layer_get_bounds(s_layer);
          int vis = (bb.size.h - 40 - 14) / 18;
          if (vis < 1) vis = 1;
          s_scroll = me - vis / 2;
          if (s_scroll < 0) s_scroll = 0;
          int maxo = LL->n_stand > vis ? LL->n_stand - vis : 0;
          if (s_scroll > maxo) s_scroll = maxo;
        }
      }
    }
  } else if (s_screen == SCR_STARTERS || s_screen == SCR_TABLE) {
    s_scroll++;
  }
  layer_mark_dirty(s_layer);
}

static void back_click(ClickRecognizerRef r, void *ctx) {
  if (s_screen == SCR_STARTERS || s_screen == SCR_TABLE) {
    s_screen = SCR_MATCHUP;
    s_scroll = 0;
    layer_mark_dirty(s_layer);
  } else if (s_screen == SCR_MATCHUP) {
    s_screen = SCR_HOME;
    s_scroll = 0;
    layer_mark_dirty(s_layer);
  } else {
    window_stack_pop(true);
  }
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click);
}

static void win_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, layer_update);
  layer_add_child(root, s_layer);
}

static void win_unload(Window *w) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void prv_init(void) {
  data_init();
  s_win = window_create();
  window_set_background_color(s_win, GColorBlack);
  window_set_window_handlers(s_win, (WindowHandlers){ .load = win_load, .unload = win_unload });
  window_set_click_config_provider(s_win, click_config);
#ifdef PREVIEW_SCREEN
  s_screen = PREVIEW_SCREEN;
  s_sel = PREVIEW_SEL;
  s_scroll = PREVIEW_SCROLL;
#endif
  window_stack_push(s_win, true);
#ifndef PREVIEW_MOCK
  data_request_sync();
#endif
}

static void prv_deinit(void) {
  if (s_refresh_timer) app_timer_cancel(s_refresh_timer);
  window_destroy(s_win);
  data_deinit();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
