#pragma once
#include <pebble.h>

#define MAX_LEAGUES   2
#define MAX_STARTERS  9
#define MAX_STANDINGS 18
#define NAME_LEN      14
#define ABBR_LEN      4
#define OPP_LEN       16
#define SLOT_LEN      4
#define TEAM_LEN      4
#define SUB_LEN       12

enum {
  APP_STATE_NODATA = 0,
  APP_STATE_OFF    = 1,
  APP_STATE_PRE    = 2,
  APP_STATE_LIVE   = 3,
  APP_STATE_FINAL  = 4
};

enum { P_PRE = 0, P_IN = 1, P_POST = 2, P_BYE = 3 };
enum { LSTATE_PRE = 0, LSTATE_LIVE = 1, LSTATE_FINAL = 2 };

typedef struct {
  char slot[SLOT_LEN];
  char name[NAME_LEN];
  char team[TEAM_LEN];
  char sub[SUB_LEN];
  int16_t pts100;     // points * 100
  uint8_t status;     // P_*
} Starter;

typedef struct {
  char name[NAME_LEN];
  uint8_t rank, w, l, me;
  uint16_t pf;        // points for (integer)
} StandRow;

typedef struct {
  char name[NAME_LEN];
  char abbr[ABBR_LEN];
  char my_name[NAME_LEN];
  char opp_name[OPP_LEN];
  int16_t my_pts100, opp_pts100;
  int16_t my_proj10, opp_proj10;
  uint8_t w, l, rank, nteams;
  uint8_t wp;           // 0-100
  uint8_t state;        // LSTATE_*
  uint8_t my_left, opp_left;
  uint8_t n_starters, n_stand;
  Starter starters[MAX_STARTERS];
  StandRow stand[MAX_STANDINGS];
} League;

typedef struct {
  uint8_t week;
  uint8_t state;        // APP_STATE_*
  uint8_t n_leagues;
  int32_t ts;
  League leagues[MAX_LEAGUES];
} FantasyData;

void data_init(void);
void data_deinit(void);
FantasyData *data_get(void);
void data_request_sync(void);
bool data_has_usable(void);
void data_persist_save(void);
void data_persist_load(void);

/* Screen navigation helpers used by main */
enum {
  SCR_HOME = 0,
  SCR_MATCHUP,
  SCR_STARTERS,
  SCR_TABLE,
  SCR_COUNT
};
