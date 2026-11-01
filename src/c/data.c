#include "data.h"
#include <string.h>

static void copy_str(char *dst, size_t n, const char *src) {
  if (!n) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = 0;
}

#define PERSIST_META  1
#define PERSIST_L0    10
#define PERSIST_L1    11
#define PERSIST_S0    20
#define PERSIST_S1    21
#define PERSIST_T0    30
#define PERSIST_T1    31

static FantasyData s_data;
static FantasyData s_pending;   /* built during SYNC=1..2; promoted only on SYNC=2 */
static bool s_receiving;
static int s_cur_league = -1;

/* Target for inbox writes: pending while a sync is in flight, else live data. */
static FantasyData *inbox_target(void) {
  return s_receiving ? &s_pending : &s_data;
}

FantasyData *data_get(void) { return &s_data; }

bool data_has_usable(void) {
  return s_data.n_leagues > 0 && s_data.state != APP_STATE_NODATA;
}

/* ---- packed persist blobs (fit in 256-byte persist limit) ---- */
typedef struct __attribute__((packed)) {
  uint8_t week, state, n_leagues;
  int32_t ts;
} MetaBlob;

typedef struct __attribute__((packed)) {
  char name[NAME_LEN], abbr[ABBR_LEN], my_name[NAME_LEN], opp_name[OPP_LEN];
  int16_t my_pts100, opp_pts100, my_proj10, opp_proj10;
  uint8_t w, l, rank, nteams, wp, state, my_left, opp_left, n_starters, n_stand;
} LeagueBlob;

typedef struct __attribute__((packed)) {
  char slot[SLOT_LEN], name[12], team[TEAM_LEN];
  int16_t pts100;
  uint8_t status;
} StarterBlob;  /* sub not persisted (re-filled as "" / "Final" on load) */

typedef struct __attribute__((packed)) {
  char name[NAME_LEN];
  uint8_t rank, w, l, me;
  uint16_t pf;
} StandBlob;

void data_persist_save(void) {
  MetaBlob m = { s_data.week, s_data.state, s_data.n_leagues, s_data.ts };
  persist_write_data(PERSIST_META, &m, sizeof(m));
  for (int i = 0; i < s_data.n_leagues && i < MAX_LEAGUES; i++) {
    League *L = &s_data.leagues[i];
    LeagueBlob lb;
    memset(&lb, 0, sizeof(lb));
    strncpy(lb.name, L->name, NAME_LEN - 1);
    strncpy(lb.abbr, L->abbr, ABBR_LEN - 1);
    strncpy(lb.my_name, L->my_name, NAME_LEN - 1);
    strncpy(lb.opp_name, L->opp_name, OPP_LEN - 1);
    lb.my_pts100 = L->my_pts100; lb.opp_pts100 = L->opp_pts100;
    lb.my_proj10 = L->my_proj10; lb.opp_proj10 = L->opp_proj10;
    lb.w = L->w; lb.l = L->l; lb.rank = L->rank; lb.nteams = L->nteams;
    lb.wp = L->wp; lb.state = L->state;
    lb.my_left = L->my_left; lb.opp_left = L->opp_left;
    lb.n_starters = L->n_starters; lb.n_stand = L->n_stand;
    persist_write_data(PERSIST_L0 + i, &lb, sizeof(lb));

    /* Persist up to 8 starters (256-byte key limit). */
    {
      typedef struct __attribute__((packed)) {
        uint8_t n;
        StarterBlob rows[8];
      } SP8;
      SP8 sp; memset(&sp, 0, sizeof(sp));
      sp.n = L->n_starters > 8 ? 8 : L->n_starters;
      for (int j = 0; j < sp.n; j++) {
        Starter *s = &L->starters[j];
        strncpy(sp.rows[j].slot, s->slot, SLOT_LEN - 1);
        strncpy(sp.rows[j].name, s->name, 11);
        strncpy(sp.rows[j].team, s->team, TEAM_LEN - 1);
        sp.rows[j].pts100 = s->pts100;
        sp.rows[j].status = s->status;
      }
      persist_write_data(PERSIST_S0 + i, &sp, sizeof(sp));
    }
    /* Persist up to 12 standings rows. */
    {
      typedef struct __attribute__((packed)) {
        uint8_t n;
        StandBlob rows[12];
      } TP12;
      TP12 tp; memset(&tp, 0, sizeof(tp));
      tp.n = L->n_stand > 12 ? 12 : L->n_stand;
      /* If >12, keep top 5 + window around me */
      if (L->n_stand > 12) {
        int me = -1;
        for (int j = 0; j < L->n_stand; j++) if (L->stand[j].me) { me = j; break; }
        int idxs[12]; int n = 0;
        for (int j = 0; j < 5 && j < L->n_stand; j++) idxs[n++] = j;
        if (me >= 5) {
          if (me - 1 >= 5) idxs[n++] = me - 1;
          idxs[n++] = me;
          if (me + 1 < L->n_stand) idxs[n++] = me + 1;
        }
        tp.n = n > 12 ? 12 : n;
        for (int j = 0; j < tp.n; j++) {
          StandRow *r = &L->stand[idxs[j]];
          strncpy(tp.rows[j].name, r->name, NAME_LEN - 1);
          tp.rows[j].rank = r->rank; tp.rows[j].w = r->w; tp.rows[j].l = r->l;
          tp.rows[j].me = r->me; tp.rows[j].pf = r->pf;
        }
      } else {
        for (int j = 0; j < tp.n; j++) {
          StandRow *r = &L->stand[j];
          strncpy(tp.rows[j].name, r->name, NAME_LEN - 1);
          tp.rows[j].rank = r->rank; tp.rows[j].w = r->w; tp.rows[j].l = r->l;
          tp.rows[j].me = r->me; tp.rows[j].pf = r->pf;
        }
      }
      persist_write_data(PERSIST_T0 + i, &tp, sizeof(tp));
    }
  }
}

void data_persist_load(void) {
  memset(&s_data, 0, sizeof(s_data));
  if (!persist_exists(PERSIST_META)) return;
  MetaBlob m;
  if (persist_read_data(PERSIST_META, &m, sizeof(m)) <= 0) return;
  s_data.week = m.week; s_data.state = m.state;
  s_data.n_leagues = m.n_leagues > MAX_LEAGUES ? MAX_LEAGUES : m.n_leagues;
  s_data.ts = m.ts;
  for (int i = 0; i < s_data.n_leagues; i++) {
    if (!persist_exists(PERSIST_L0 + i)) continue;
    LeagueBlob lb;
    if (persist_read_data(PERSIST_L0 + i, &lb, sizeof(lb)) <= 0) continue;
    League *L = &s_data.leagues[i];
    strncpy(L->name, lb.name, NAME_LEN - 1);
    strncpy(L->abbr, lb.abbr, ABBR_LEN - 1);
    strncpy(L->my_name, lb.my_name, NAME_LEN - 1);
    strncpy(L->opp_name, lb.opp_name, OPP_LEN - 1);
    L->my_pts100 = lb.my_pts100; L->opp_pts100 = lb.opp_pts100;
    L->my_proj10 = lb.my_proj10; L->opp_proj10 = lb.opp_proj10;
    L->w = lb.w; L->l = lb.l; L->rank = lb.rank; L->nteams = lb.nteams;
    L->wp = lb.wp; L->state = lb.state;
    L->my_left = lb.my_left; L->opp_left = lb.opp_left;
    L->n_starters = lb.n_starters; L->n_stand = lb.n_stand;

    if (persist_exists(PERSIST_S0 + i)) {
      typedef struct __attribute__((packed)) {
        uint8_t n;
        StarterBlob rows[8];
      } SP8;
      SP8 sp;
      if (persist_read_data(PERSIST_S0 + i, &sp, sizeof(sp)) > 0) {
        L->n_starters = sp.n > MAX_STARTERS ? MAX_STARTERS : sp.n;
        for (int j = 0; j < L->n_starters; j++) {
          strncpy(L->starters[j].slot, sp.rows[j].slot, SLOT_LEN - 1);
          strncpy(L->starters[j].name, sp.rows[j].name, NAME_LEN - 1);
          strncpy(L->starters[j].team, sp.rows[j].team, TEAM_LEN - 1);
          L->starters[j].pts100 = sp.rows[j].pts100;
          L->starters[j].status = sp.rows[j].status;
          if (sp.rows[j].status == P_POST)
            strncpy(L->starters[j].sub, "Final", SUB_LEN - 1);
          else
            L->starters[j].sub[0] = 0;
        }
      }
    }
    if (persist_exists(PERSIST_T0 + i)) {
      typedef struct __attribute__((packed)) {
        uint8_t n;
        StandBlob rows[12];
      } TP12;
      TP12 tp;
      if (persist_read_data(PERSIST_T0 + i, &tp, sizeof(tp)) > 0) {
        L->n_stand = tp.n > MAX_STANDINGS ? MAX_STANDINGS : tp.n;
        for (int j = 0; j < L->n_stand; j++) {
          strncpy(L->stand[j].name, tp.rows[j].name, NAME_LEN - 1);
          L->stand[j].rank = tp.rows[j].rank;
          L->stand[j].w = tp.rows[j].w; L->stand[j].l = tp.rows[j].l;
          L->stand[j].me = tp.rows[j].me; L->stand[j].pf = tp.rows[j].pf;
        }
      }
    }
  }
}

static void copy_cstr(char *dst, size_t n, Tuple *t) {
  if (!t || t->type != TUPLE_CSTRING) { dst[0] = 0; return; }
  strncpy(dst, t->value->cstring, n - 1);
  dst[n - 1] = 0;
}

static void inbox(DictionaryIterator *iter, void *ctx) {
  Tuple *t;
  t = dict_find(iter, MESSAGE_KEY_ERR);
  if (t) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "phone err: %s", t->value->cstring);
    return;
  }
  t = dict_find(iter, MESSAGE_KEY_SYNC);
  if (t) {
    int v = (int)t->value->int32;
    if (v == 1) {
      /* Start a fresh pending snapshot; keep s_data on screen until SYNC=2. */
      s_receiving = true;
      s_cur_league = -1;
      memset(&s_pending, 0, sizeof(s_pending));
      Tuple *w = dict_find(iter, MESSAGE_KEY_WEEK);
      Tuple *st = dict_find(iter, MESSAGE_KEY_STATE);
      Tuple *nl = dict_find(iter, MESSAGE_KEY_N_LEAGUES);
      Tuple *ts = dict_find(iter, MESSAGE_KEY_TS);
      if (w) s_pending.week = (uint8_t)w->value->int32;
      if (st) s_pending.state = (uint8_t)st->value->int32;
      if (nl) s_pending.n_leagues = (uint8_t)nl->value->int32;
      if (ts) s_pending.ts = ts->value->int32;
      if (s_pending.n_leagues > MAX_LEAGUES) s_pending.n_leagues = MAX_LEAGUES;
    } else if (v == 2) {
      /* Promote pending → live only when the full sync arrived. */
      s_receiving = false;
      s_data = s_pending;
      data_persist_save();
      /* main.c listens via app_focus / tick — mark dirty through weak symbol */
      extern void fantasy_data_updated(void);
      fantasy_data_updated();
    }
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_L_I);
  if (t) {
    int i = (int)t->value->int32;
    if (i < 0 || i >= MAX_LEAGUES) return;
    s_cur_league = i;
    League *L = &inbox_target()->leagues[i];
    copy_cstr(L->name, NAME_LEN, dict_find(iter, MESSAGE_KEY_L_NAME));
    copy_cstr(L->abbr, ABBR_LEN, dict_find(iter, MESSAGE_KEY_L_ABBR));
    copy_cstr(L->my_name, NAME_LEN, dict_find(iter, MESSAGE_KEY_L_MY_NAME));
    copy_cstr(L->opp_name, OPP_LEN, dict_find(iter, MESSAGE_KEY_L_OPP_NAME));
    if ((t = dict_find(iter, MESSAGE_KEY_L_MY_PTS))) L->my_pts100 = (int16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_OPP_PTS))) L->opp_pts100 = (int16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_MY_PROJ))) L->my_proj10 = (int16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_OPP_PROJ))) L->opp_proj10 = (int16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_W))) L->w = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_L))) L->l = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_RANK))) L->rank = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_N))) L->nteams = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_WP))) L->wp = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_STATE))) L->state = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_MY_LEFT))) L->my_left = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_OPP_LEFT))) L->opp_left = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_NS))) L->n_starters = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_L_NT))) L->n_stand = (uint8_t)t->value->int32;
    if (L->n_starters > MAX_STARTERS) L->n_starters = MAX_STARTERS;
    if (L->n_stand > MAX_STANDINGS) L->n_stand = MAX_STANDINGS;
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_S_I);
  if (t && s_cur_league >= 0) {
    int si = (int)t->value->int32;
    if (si < 0 || si >= MAX_STARTERS) return;
    League *L = &inbox_target()->leagues[s_cur_league];
    Starter *s = &L->starters[si];
    copy_cstr(s->slot, SLOT_LEN, dict_find(iter, MESSAGE_KEY_S_SLOT));
    copy_cstr(s->name, NAME_LEN, dict_find(iter, MESSAGE_KEY_S_NAME));
    copy_cstr(s->team, TEAM_LEN, dict_find(iter, MESSAGE_KEY_S_TEAM));
    copy_cstr(s->sub, SUB_LEN, dict_find(iter, MESSAGE_KEY_S_SUB));
    if ((t = dict_find(iter, MESSAGE_KEY_S_PTS))) s->pts100 = (int16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_S_ST))) s->status = (uint8_t)t->value->int32;
    return;
  }

  t = dict_find(iter, MESSAGE_KEY_T_I);
  if (t && s_cur_league >= 0) {
    int ti = (int)t->value->int32;
    if (ti < 0 || ti >= MAX_STANDINGS) return;
    League *L = &inbox_target()->leagues[s_cur_league];
    StandRow *r = &L->stand[ti];
    copy_cstr(r->name, NAME_LEN, dict_find(iter, MESSAGE_KEY_T_NAME));
    if ((t = dict_find(iter, MESSAGE_KEY_T_RANK))) r->rank = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_T_W))) r->w = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_T_L))) r->l = (uint8_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_T_PF))) r->pf = (uint16_t)t->value->int32;
    if ((t = dict_find(iter, MESSAGE_KEY_T_ME))) r->me = (uint8_t)t->value->int32;
  }
}

static void inbox_dropped(AppMessageResult r, void *ctx) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "inbox dropped %d", (int)r);
}

void data_request_sync(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_int32(iter, MESSAGE_KEY_REQUEST, 1);
  app_message_outbox_send();
}

#ifdef PREVIEW_MOCK
static void load_preview(void) {
  /* Dados FICTÍCIOS de demonstração (não são de nenhuma liga real). */
  memset(&s_data, 0, sizeof(s_data));
  s_data.week = 7;
  s_data.state = APP_STATE_FINAL;
  s_data.n_leagues = 2;
  s_data.ts = 1761552000;

  League *A = &s_data.leagues[0];
  copy_str(A->name, NAME_LEN, "Liga Alfa");
  copy_str(A->abbr, ABBR_LEN, "LA");
  copy_str(A->my_name, NAME_LEN, "Blue Comets");
  copy_str(A->opp_name, OPP_LEN, "Pixel Foxes");
  A->my_pts100 = 8844; A->opp_pts100 = 8120;
  A->w = 4; A->l = 2; A->rank = 3; A->nteams = 12;
  A->wp = 0; A->state = LSTATE_FINAL; A->my_left = 0; A->opp_left = 0;
  A->n_starters = 9;
  {
    const char *slots[] = {"QB","RB","RB","WR","WR","TE","FLX","K","DEF"};
    const char *names[] = {"A. Nova","B. River","C. Stone","D. Sky","E. Lake","F. Hill","G. Park","H. Moon","CAR"};
    const char *teams[] = {"NE","BAL","GB","DAL","DET","SF","PIT","SF","CAR"};
    int pts[] = {2844,480,450,1370,2360,980,660,800,-300};
    int i;
    for (i = 0; i < 9; i++) {
      copy_str(A->starters[i].slot, SLOT_LEN, slots[i]);
      copy_str(A->starters[i].name, NAME_LEN, names[i]);
      copy_str(A->starters[i].team, TEAM_LEN, teams[i]);
      copy_str(A->starters[i].sub, SUB_LEN, "Final");
      A->starters[i].pts100 = pts[i];
      A->starters[i].status = P_POST;
    }
  }
  A->n_stand = 8;
  {
    const char *tn[] = {"Pixel Foxes","Neon Owls","Blue Comets","Solar Sharks","Jade Dragons","Iron Wolves","Copper Hawks","Silver Lynx"};
    uint8_t w[] = {5,5,4,4,3,3,2,2};
    uint8_t me[] = {0,0,1,0,0,0,0,0};
    int i;
    for (i = 0; i < 8; i++) {
      copy_str(A->stand[i].name, NAME_LEN, tn[i]);
      A->stand[i].rank = (uint8_t)(i + 1);
      A->stand[i].w = w[i]; A->stand[i].l = (uint8_t)(6 - w[i]);
      A->stand[i].pf = (uint16_t)(900 - i * 30); A->stand[i].me = me[i];
    }
  }

  League *B = &s_data.leagues[1];
  copy_str(B->name, NAME_LEN, "Liga Beta");
  copy_str(B->abbr, ABBR_LEN, "LB");
  copy_str(B->my_name, NAME_LEN, "Blue Comets");
  copy_str(B->opp_name, OPP_LEN, "Neon Owls");
  B->my_pts100 = 7210; B->opp_pts100 = 7980;
  B->w = 3; B->l = 3; B->rank = 5; B->nteams = 10;
  B->wp = 0; B->state = LSTATE_FINAL; B->my_left = 0; B->opp_left = 0;
  B->n_starters = 9;
  {
    const char *slots[] = {"QB","RB","RB","WR","WR","TE","FLX","K","DEF"};
    const char *names[] = {"I. Spark","J. Dash","K. Bolt","L. Beam","M. Wave","N. Grove","O. Field","P. Kick","CLE"};
    const char *teams[] = {"LV","IND","HOU","DAL","ARI","PIT","PIT","IND","CLE"};
    int pts[] = {2040,1800,860,1370,860,300,420,400,600};
    int i;
    for (i = 0; i < 9; i++) {
      copy_str(B->starters[i].slot, SLOT_LEN, slots[i]);
      copy_str(B->starters[i].name, NAME_LEN, names[i]);
      copy_str(B->starters[i].team, TEAM_LEN, teams[i]);
      copy_str(B->starters[i].sub, SUB_LEN, "Final");
      B->starters[i].pts100 = pts[i];
      B->starters[i].status = P_POST;
    }
  }
  B->n_stand = 8;
  {
    const char *tn[] = {"Neon Owls","Pixel Foxes","Solar Sharks","Jade Dragons","Blue Comets","Iron Wolves","Copper Hawks","Silver Lynx"};
    uint8_t w[] = {5,4,4,3,3,3,2,2};
    uint8_t me[] = {0,0,0,0,1,0,0,0};
    int i;
    for (i = 0; i < 8; i++) {
      copy_str(B->stand[i].name, NAME_LEN, tn[i]);
      B->stand[i].rank = (uint8_t)(i + 1);
      B->stand[i].w = w[i]; B->stand[i].l = (uint8_t)(6 - w[i]);
      B->stand[i].pf = (uint16_t)(880 - i * 25); B->stand[i].me = me[i];
    }
  }
}
#endif

void data_init(void) {
  memset(&s_data, 0, sizeof(s_data));
#ifdef PREVIEW_MOCK
  load_preview();
#else
  data_persist_load();
  app_message_register_inbox_received(inbox);
  app_message_register_inbox_dropped(inbox_dropped);
  /* generous buffers for multi-key messages */
  app_message_open(app_message_inbox_size_maximum(), 256);
#endif
}

void data_deinit(void) {
#ifndef PREVIEW_MOCK
  /* nothing special */
#endif
}
