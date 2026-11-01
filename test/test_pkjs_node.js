/**
 * Smoke test of the Fantasy PebbleKit JS data shaping.
 * Default: uses an in-memory fixture (no network, no SLEEPER_USER).
 * Optional live check: SLEEPER_USER=... node test/test_pkjs_node.js --live
 */
'use strict';
const https = require('https');
const http = require('http');

const LIVE = process.argv.includes('--live');
const USER = process.env.SLEEPER_USER || '';
const API = 'https://api.sleeper.app/v1';

function get(url) {
  return new Promise((resolve, reject) => {
    const lib = url.startsWith('https') ? https : http;
    lib.get(url, { headers: { 'User-Agent': 'FantasyPebbleTest/1.0.2', 'Cache-Control': 'no-cache' } }, res => {
      let d = '';
      res.on('data', c => d += c);
      res.on('end', () => { try { resolve(JSON.parse(d)); } catch (e) { reject(e); } });
    }).on('error', reject);
  });
}

/** Same ORDEM dedup logic as src/pkjs/index.js */
function pickLeagues(leagues, ORDEM) {
  const wanted = [];
  if (ORDEM.length) {
    for (let o = 0; o < ORDEM.length; o++) {
      for (let i = 0; i < leagues.length; i++) {
        if ((leagues[i].name || '').toLowerCase().indexOf(ORDEM[o].toLowerCase()) >= 0) {
          if (!wanted.some(w => w.league_id === leagues[i].league_id)) wanted.push(leagues[i]);
        }
      }
    }
    return wanted.slice(0, 2);
  }
  return (leagues || []).slice().sort((a, b) => (a.name || '').localeCompare(b.name || '')).slice(0, 2);
}

function fixture() {
  return {
    leagues: [
      { league_id: '1', name: 'Liga Alfa' },
      { league_id: '2', name: 'Liga Beta' },
      { league_id: '1', name: 'Liga Alfa' }, // duplicate id if ORDEM repeats match
    ],
  };
}

function assert(cond, msg) {
  if (!cond) throw new Error(msg);
}

(async () => {
  if (!LIVE) {
    const leagues = [
      { league_id: '1', name: 'Liga Alfa' },
      { league_id: '2', name: 'Liga Beta' },
      { league_id: '3', name: 'Outra Liga' },
    ];
    // Repeated ORDEM entry must not push the same league twice
    const a = pickLeagues(leagues, ['Alfa', 'Alfa', 'Beta']);
    assert(a.length === 2, 'expected 2, got ' + a.length);
    assert(a[0].league_id === '1' && a[1].league_id === '2', 'ids ' + JSON.stringify(a));
    // Cap at 2 even if ORDEM lists more
    const b = pickLeagues(leagues, ['Alfa', 'Beta', 'Outra']);
    assert(b.length === 2, 'cap 2');
    // Empty ORDEM → alpha sort, max 2
    const c = pickLeagues(leagues, []);
    assert(c.length === 2 && c[0].name === 'Liga Alfa' && c[1].name === 'Liga Beta', 'alpha');
    console.log('OK fixture: ORDEM dedup + alpha slice (no network)');
    return;
  }

  if (!USER || USER === 'seu_usuario') {
    console.error('LIVE mode needs SLEEPER_USER set to a real username');
    process.exit(2);
  }
  const st = await get(API + '/state/nfl');
  const week = st.leg || st.week;
  const season = st.league_season || st.season;
  console.log('state', season, 'week', week, st.season_type);
  const user = await get(API + '/user/' + USER);
  const leagues = await get(API + `/user/${user.user_id}/leagues/nfl/${season}`);
  const wanted = pickLeagues(leagues, (process.env.SLEEPER_LEAGUES || '').split(',').map(s => s.trim()).filter(Boolean));
  console.log('leagues', wanted.map(l => l.name));
  assert(wanted.length <= 2, 'max 2');
  console.log('OK live');
})().catch(e => { console.error(e); process.exit(1); });
