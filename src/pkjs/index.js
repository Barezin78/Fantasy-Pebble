/* Fantasy watchapp — PebbleKit JS (Sleeper)
 * Usuário e ligas vêm das Configurações (Clay). Sem default pessoal.
 * Cache-buster em endpoints ao vivo. Jogadores em localStorage (7 dias).
 */
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

var API = 'https://api.sleeper.app/v1';
var USER = '';
var ORDEM = [];
var SIGLAS = {};

function loadSettings() {
  try {
    var s = JSON.parse(localStorage.getItem('clay-settings')) || {};
    USER = String(s.USER || '').trim();
    var ord = String(s.ORDEM || '').trim();
    ORDEM = ord ? ord.split(',').map(function (x) { return x.trim(); }).filter(Boolean) : [];
    SIGLAS = {};
    ORDEM.forEach(function (n) {
      SIGLAS[n] = n.split(/\s+/).map(function (w) { return w[0] || ''; }).join('').slice(0, 3).toUpperCase() || n.slice(0, 2).toUpperCase();
    });
  } catch (e) {
    USER = ''; ORDEM = []; SIGLAS = {};
  }
}
var DESVIO = 0.55;
var PLAYERS_KEY = 'fantasy_players_v1';
var PLAYERS_TS = 'fantasy_players_ts';
var PLAYERS_TTL = 7 * 24 * 3600 * 1000;

var queue = [], sending = false;
function send(dict) { queue.push({ d: dict, tries: 0 }); pump(); }
function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.d, function () {
    queue.shift(); sending = false; pump();
  }, function () {
    sending = false;
    if (++item.tries >= 3) queue.shift();
    setTimeout(pump, 800);
  });
}

function get(url, ok, fail, fresh) {
  var u = fresh ? (url + (url.indexOf('?') >= 0 ? '&' : '?') + 't=' + Date.now()) : url;
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (this.status !== 200) { fail('HTTP ' + this.status); return; }
    try { ok(JSON.parse(this.responseText)); } catch (e) { fail('parse'); }
  };
  xhr.onerror = function () { fail('network'); };
  xhr.ontimeout = function () { fail('timeout'); };
  xhr.timeout = 40000;
  xhr.open('GET', u);
  if (fresh) xhr.setRequestHeader('Cache-Control', 'no-cache');
  xhr.send();
}

function phi(z) {
  var t = 1 / (1 + 0.3275911 * Math.abs(z));
  var y = 1 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t * Math.exp(-z * z / 2);
  return z >= 0 ? (1 + y) / 2 : (1 - y) / 2;
}

function shortName(p) {
  if (!p) return '?';
  if (typeof p === 'object' && p.length === 4) {
    /* trimmed cache: [first, last, pos, team] */
    if (p[2] === 'DEF') return String(p[3] || 'DEF').substring(0, 12);
    var ini = p[0] ? String(p[0]).charAt(0) + '. ' : '';
    return (ini + (p[1] || '')).substring(0, 13);
  }
  var fn = p.first_name || '', ln = p.last_name || '';
  if (p.position === 'DEF') return (p.team || ln || 'DEF').substring(0, 12);
  return ((fn ? fn.charAt(0) + '. ' : '') + ln).substring(0, 13);
}

function teamOf(p) {
  if (!p) return '';
  if (p.length === 4) return String(p[3] || '').substring(0, 3);
  return String(p.team || '').substring(0, 3);
}

function teamName(user) {
  var meta = (user && user.metadata) || {};
  return String(meta.team_name || (user && user.display_name) || 'Time').substring(0, 13);
}

function loadPlayers(cb) {
  try {
    var ts = parseInt(localStorage.getItem(PLAYERS_TS) || '0', 10);
    if (Date.now() - ts < PLAYERS_TTL) {
      var cached = JSON.parse(localStorage.getItem(PLAYERS_KEY) || 'null');
      if (cached) { cb(null, cached); return; }
    }
  } catch (e) {}
  console.log('Fantasy: baixando /players/nfl (uma vez, ~cache 7d)...');
  get(API + '/players/nfl', function (full) {
    var o = {};
    for (var pid in full) {
      if (!full.hasOwnProperty(pid)) continue;
      var x = full[pid];
      if (!x) continue;
      var pos = x.position;
      if (pos === 'QB' || pos === 'RB' || pos === 'WR' || pos === 'TE' || pos === 'K' || pos === 'DEF') {
        o[pid] = [x.first_name || '', x.last_name || '', pos, x.team || ''];
      }
    }
    try {
      localStorage.setItem(PLAYERS_KEY, JSON.stringify(o));
      localStorage.setItem(PLAYERS_TS, String(Date.now()));
    } catch (e2) { console.log('players cache write fail ' + e2); }
    cb(null, o);
  }, function (e) { cb(e, {}); });
}

function parallel(urls, done) {
  var out = new Array(urls.length), left = urls.length, err = null;
  urls.forEach(function (item, i) {
    get(item.url, function (j) { out[i] = j; if (--left === 0) done(err, out); },
        function (e) { err = err || e; out[i] = null; if (--left === 0) done(err, out); },
        !!item.fresh);
  });
}

function matchupsHaveScores(matchups) {
  if (!matchups || !matchups.length) return false;
  for (var i = 0; i < matchups.length; i++) {
    if (Number(matchups[i].points || 0) > 0) return true;
  }
  return false;
}

function buildOne(league, users, rosters, matchups, players, meId) {
  var umap = {}, rmap = {}, i;
  for (i = 0; i < users.length; i++) umap[users[i].user_id] = users[i];
  for (i = 0; i < rosters.length; i++) rmap[rosters[i].roster_id] = rosters[i];
  var mine = null;
  for (i = 0; i < rosters.length; i++) if (rosters[i].owner_id === meId) { mine = rosters[i]; break; }
  if (!mine) return null;

  var mm = null, opp = null;
  for (i = 0; i < matchups.length; i++) if (matchups[i].roster_id === mine.roster_id) mm = matchups[i];
  if (mm) {
    for (i = 0; i < matchups.length; i++) {
      if (matchups[i].matchup_id === mm.matchup_id && matchups[i].roster_id !== mine.roster_id) {
        opp = matchups[i]; break;
      }
    }
  }

  var startPos = [];
  var positions = league.roster_positions || [];
  for (i = 0; i < positions.length; i++) {
    if (positions[i] !== 'BN' && positions[i] !== 'IR' && positions[i] !== 'TAXI')
      startPos.push(positions[i] === 'FLEX' ? 'FLX' : positions[i]);
  }

  function rowsOf(m) {
    var rows = [];
    if (!m) return rows;
    var starters = m.starters || [];
    var sp = m.starters_points || [];
    var pp = m.players_points || {};
    for (var k = 0; k < starters.length; k++) {
      var pid = starters[k];
      var slot = (startPos[k] || '?').substring(0, 3);
      if (!pid || pid === '0') {
        rows.push({ slot: slot, name: '(vazio)', team: '', pts: 0, proj: 0, status: 3, sub: 'BYE', pid: null });
        continue;
      }
      var p = players[pid];
      var pts = sp[k] != null ? Number(sp[k]) : Number(pp[pid] || 0);
      rows.push({
        slot: slot,
        name: shortName(p),
        team: teamOf(p),
        pts: pts,
        proj: pts,
        status: 0,
        sub: '',
        pid: pid
      });
    }
    return rows;
  }

  var stand = [];
  for (i = 0; i < rosters.length; i++) {
    var r = rosters[i], s = r.settings || {}, u = umap[r.owner_id];
    stand.push({
      name: teamName(u).substring(0, 13),
      w: s.wins || 0, l: s.losses || 0,
      pf: Math.round((s.fpts || 0) + (s.fpts_decimal || 0) / 100),
      me: r.roster_id === mine.roster_id ? 1 : 0,
      roster_id: r.roster_id
    });
  }
  stand.sort(function (a, b) { return (b.w - a.w) || (b.pf - a.pf); });
  for (i = 0; i < stand.length; i++) stand[i].rank = i + 1;
  var myRow = stand.filter(function (x) { return x.me; })[0] || { w: 0, l: 0, rank: 0 };
  var oppR = opp ? rmap[opp.roster_id] : null;
  var oppU = oppR ? umap[oppR.owner_id] : null;

  return {
    name: league.name,
    abbr: SIGLAS[league.name] || league.name.substring(0, 3),
    my_name: teamName(umap[meId]),
    opp_name: oppU ? teamName(oppU).substring(0, 15) : '—',
    my_pts: mm ? Number(mm.points || 0) : 0,
    opp_pts: opp ? Number(opp.points || 0) : 0,
    my_proj: 0, opp_proj: 0,
    w: myRow.w, l: myRow.l, rank: myRow.rank, n: stand.length,
    wp: 50, state: 0, my_left: 0, opp_left: 0,
    starters: rowsOf(mm),
    opp_starters: rowsOf(opp),
    stand: stand
  };
}

function enrichScoreboard(leagues, season, week, seasonType, cb) {
  var stype = seasonType === 'post' ? 3 : 2;
  var url = 'https://site.api.espn.com/apis/site/v2/sports/football/nfl/scoreboard?dates=' +
    season + '&seasontype=' + stype + '&week=' + week;
  get(url, function (j) {
    var byTeam = {}, anyLive = false, anyPre = false, allPost = true;
    var events = (j && j.events) || [];
    for (var i = 0; i < events.length; i++) {
      var e = events[i], c = e.competitions[0];
      var st = ((c.status || {}).type || {}).state || 'pre';
      var detail = ((c.status || {}).type || {}).shortDetail || '';
      detail = detail.replace('Halftime', 'Interv.')
        .replace(' - 1st', ' 1ºQ').replace(' - 2nd', ' 2ºQ')
        .replace(' - 3rd', ' 3ºQ').replace(' - 4th', ' 4ºQ');
      var away, home, comps = c.competitors || [];
      for (var k = 0; k < comps.length; k++) {
        if (comps[k].homeAway === 'home') home = comps[k]; else away = comps[k];
      }
      function ab(x) {
        var a = (x && x.team && x.team.abbreviation) || '';
        if (a === 'WSH') a = 'WAS'; if (a === 'JAC') a = 'JAX';
        return a;
      }
      var info = { state: st, detail: detail, kick: e.date };
      if (away) byTeam[ab(away)] = info;
      if (home) byTeam[ab(home)] = info;
      if (st === 'in') anyLive = true;
      if (st === 'pre') { anyPre = true; allPost = false; }
      if (st !== 'post') allPost = false;
    }
    if (!events.length) { allPost = false; }

    for (var li = 0; li < leagues.length; li++) {
      var L = leagues[li];
      var myLeft = 0, oppLeft = 0, myRem = 0, oppRem = 0, myVar = 0, oppVar = 0;

      function touch(rows, isMe) {
        for (var r = 0; r < rows.length; r++) {
          var s = rows[r];
          var g = byTeam[s.team];
          if (!g) {
            if (s.status !== 3) { s.status = 3; s.sub = 'BYE'; }
            continue;
          }
          if (g.state === 'post') { s.status = 2; s.sub = 'Final'; }
          else if (g.state === 'in') {
            s.status = 1; s.sub = String(g.detail).substring(0, 11);
            if (isMe) myLeft++; else oppLeft++;
            var rem = Math.max(0, (s.proj || 0) - s.pts);
            if (isMe) { myRem += rem; myVar += Math.pow(DESVIO * Math.max(rem, 1), 2); }
            else { oppRem += rem; oppVar += Math.pow(DESVIO * Math.max(rem, 1), 2); }
          } else {
            s.status = 0;
            var d = new Date(g.kick);
            var dias = ['dom', 'seg', 'ter', 'qua', 'qui', 'sex', 'sáb'];
            var mm = d.getMinutes();
            s.sub = dias[d.getDay()] + ' ' + d.getHours() + 'h' + (mm < 10 ? '0' : '') + mm;
            if (isMe) myLeft++; else oppLeft++;
            var rem2 = s.proj || 0;
            if (isMe) { myRem += rem2; myVar += Math.pow(DESVIO * Math.max(rem2, 1), 2); }
            else { oppRem += rem2; oppVar += Math.pow(DESVIO * Math.max(rem2, 1), 2); }
          }
        }
      }
      touch(L.starters, true);
      touch(L.opp_starters || [], false);
      L.my_left = myLeft; L.opp_left = oppLeft;
      L.my_proj = Math.round((L.my_pts + myRem) * 10) / 10;
      L.opp_proj = Math.round((L.opp_pts + oppRem) * 10) / 10;

      if (allPost && !anyLive && !anyPre) {
        L.state = 2; L.my_left = 0; L.opp_left = 0;
        L.wp = L.my_pts > L.opp_pts ? 100 : (L.my_pts < L.opp_pts ? 0 : 50);
      } else if (anyLive || myLeft > 0 || oppLeft > 0) {
        L.state = anyLive || (myLeft + oppLeft < (L.starters.length + (L.opp_starters || []).length)) ? 1 : 0;
        if (!anyLive && myLeft === L.starters.length) L.state = 0;
        if (anyLive) L.state = 1;
        var dlt = (L.my_pts + myRem) - (L.opp_pts + oppRem);
        var sd = Math.sqrt(myVar + oppVar + 1);
        L.wp = Math.round(100 * phi(dlt / sd));
        if (L.wp < 1) L.wp = 1; if (L.wp > 99) L.wp = 99;
        if (!anyLive && !anyPre) { L.state = 2; L.wp = L.my_pts > L.opp_pts ? 100 : (L.my_pts < L.opp_pts ? 0 : 50); }
      } else {
        L.state = 0; L.wp = 50;
      }
    }
    cb(null, { anyLive: anyLive, allPost: allPost && !anyPre && !anyLive, anyPre: anyPre });
  }, function () {
    for (var i = 0; i < leagues.length; i++) {
      leagues[i].state = 2;
      leagues[i].wp = leagues[i].my_pts > leagues[i].opp_pts ? 100 :
        (leagues[i].my_pts < leagues[i].opp_pts ? 0 : 50);
    }
    cb(null, { anyLive: false, allPost: true, anyPre: false });
  }, true);
}

function sendAll(week, appState, leagues) {
  send({ SYNC: 1, WEEK: week, STATE: appState, N_LEAGUES: leagues.length, TS: Math.floor(Date.now() / 1000) });
  for (var i = 0; i < leagues.length; i++) {
    var L = leagues[i];
    send({
      L_I: i,
      L_NAME: String(L.name).substring(0, 13),
      L_ABBR: String(L.abbr).substring(0, 3),
      L_MY_NAME: String(L.my_name).substring(0, 13),
      L_OPP_NAME: String(L.opp_name).substring(0, 15),
      L_MY_PTS: Math.round(L.my_pts * 100),
      L_OPP_PTS: Math.round(L.opp_pts * 100),
      L_MY_PROJ: Math.round(L.my_proj * 10),
      L_OPP_PROJ: Math.round(L.opp_proj * 10),
      L_W: L.w, L_L: L.l, L_RANK: L.rank, L_N: L.n,
      L_WP: L.wp, L_STATE: L.state,
      L_MY_LEFT: L.my_left, L_OPP_LEFT: L.opp_left,
      L_NS: Math.min(L.starters.length, 9),
      L_NT: Math.min(L.stand.length, 18)
    });
    var ns = Math.min(L.starters.length, 9);
    for (var s = 0; s < ns; s++) {
      var st = L.starters[s];
      send({
        S_I: s, S_SLOT: st.slot, S_NAME: st.name,
        S_PTS: Math.round(st.pts * 100), S_TEAM: st.team || '',
        S_ST: st.status, S_SUB: String(st.sub || '').substring(0, 11)
      });
    }
    var nt = Math.min(L.stand.length, 18);
    for (var t = 0; t < nt; t++) {
      var row = L.stand[t];
      send({ T_I: t, T_RANK: row.rank, T_NAME: row.name, T_W: row.w, T_L: row.l, T_PF: row.pf, T_ME: row.me });
    }
  }
  send({ SYNC: 2 });
}

function loadLeagueBundle(lid, week, meId, players, cb) {
  var base = API + '/league/' + lid;
  parallel([
    { url: base },
    { url: base + '/users' },
    { url: base + '/rosters' },
    { url: base + '/matchups/' + week, fresh: true }
  ], function (err, out) {
    if (err || !out[0] || !out[3]) { cb(err || 'missing'); return; }
    var L = buildOne(out[0], out[1] || [], out[2] || [], out[3] || [], players, meId);
    if (!L) { cb('no roster'); return; }
    L._raw_matchups = out[3];
    cb(null, L);
  });
}

function refresh() {
  console.log('Fantasy: refresh');
  loadSettings();
  if (!USER) {
    send({ ERR: 'Configure o usuario Sleeper nas Configuracoes' });
    return;
  }
  get(API + '/state/nfl', function (st) {
    var week = st.leg || st.week || 1;
    var season = String(st.league_season || st.season || '2026');
    var seasonType = st.season_type || 'regular';

    if (seasonType === 'off' || seasonType === 'pre') {
      sendAll(week, 1, []);
      return;
    }

    get(API + '/user/' + USER, function (user) {
      var meId = user.user_id;
      get(API + '/user/' + meId + '/leagues/nfl/' + season, function (leagues) {
        var wanted = [];
        if (ORDEM.length) {
          for (var o = 0; o < ORDEM.length; o++) {
            for (var i = 0; i < leagues.length; i++) {
              if ((leagues[i].name || '').toLowerCase().indexOf(ORDEM[o].toLowerCase()) >= 0) {
                var already = false;
                for (var w = 0; w < wanted.length; w++) {
                  if (wanted[w].league_id === leagues[i].league_id) { already = true; break; }
                }
                if (!already) wanted.push(leagues[i]);
              }
            }
          }
          if (wanted.length > 2) wanted = wanted.slice(0, 2);
        } else {
          wanted = (leagues || []).slice().sort(function (a, b) {
            return (a.name || '').localeCompare(b.name || '');
          }).slice(0, 2);
          ORDEM = wanted.map(function (l) { return l.name; });
          wanted.forEach(function (l) {
            SIGLAS[l.name] = (l.name || '').split(/\s+/).map(function (w) { return w[0] || ''; }).join('').slice(0, 3).toUpperCase();
          });
        }
        if (!wanted.length) { sendAll(week, 1, []); return; }

        loadPlayers(function (pe, players) {
          players = players || {};

          function loadWeek(w, next) {
            var built = [], left = wanted.length, scored = false;
            wanted.forEach(function (lg) {
              loadLeagueBundle(lg.league_id, w, meId, players, function (err, L) {
                if (L) {
                  L.name = lg.name;
                  L.abbr = SIGLAS[lg.name] || L.abbr;
                  if (matchupsHaveScores(L._raw_matchups)) scored = true;
                  built.push(L);
                }
                if (--left === 0) next(built, scored);
              });
            });
          }

          loadWeek(week, function (built, scored) {
            var displayWeek = week;
            function finish(list, w) {
              /* keep ORDEM */
              list.sort(function (a, b) {
                return ORDEM.indexOf(a.name) - ORDEM.indexOf(b.name);
              });
              enrichScoreboard(list, season, w, seasonType, function (e, flags) {
                var appState = 2;
                if (flags && flags.anyLive) appState = 3;
                else if (flags && flags.allPost) appState = 4;
                /* if every league is final */
                var allFinal = list.length && list.every(function (L) { return L.state === 2; });
                if (allFinal) appState = 4;
                sendAll(w, appState, list);
                console.log('Fantasy: week ' + w + ' state ' + appState + ' leagues ' + list.length);
              });
            }

            if (!scored && week > 1) {
              loadWeek(week - 1, function (prev, prevScored) {
                if (prevScored && prev.length) {
                  displayWeek = week - 1;
                  finish(prev, displayWeek);
                } else {
                  finish(built, week);
                }
              });
            } else {
              finish(built, week);
            }
          });
        });
      }, function (e) { send({ ERR: 'leagues ' + e }); });
    }, function (e) { send({ ERR: 'user ' + e }); });
  }, function (e) { send({ ERR: 'state ' + e }); }, true);
}

Pebble.addEventListener('ready', function () {
  console.log('Fantasy PKJS ready');
  loadSettings();
  refresh();
});
Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});
Pebble.addEventListener('webviewclosed', function (e) {
  if (e && e.response) {
    var settings = clay.getSettings(e.response);
    localStorage.setItem('clay-settings', JSON.stringify(settings));
  }
  refresh();
});
Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && e.payload.REQUEST !== undefined) refresh();
});
