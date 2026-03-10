/* ── REST helpers ──────────────────────────────────────────────── */
const BASE = '/api'

async function post(path, body) {
  const res = await fetch(BASE + path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
  if (!res.ok) throw new Error(`HTTP ${res.status}: ${await res.text()}`)
  return res.json()
}

async function get(path) {
  const res = await fetch(BASE + path)
  if (!res.ok) throw new Error(`HTTP ${res.status}: ${await res.text()}`)
  return res.json()
}

/* ── Game endpoints ────────────────────────────────────────────── */

export const startGame = (options) => post('/game/start', options)
// options: { color: 'white'|'black', depth: number }

export const sendMove = (gameId, move) => post('/move', { game_id: gameId, move })
// move: UCI string e.g. 'e2e4'

export const getGame = (gameId) => get(`/game/${gameId}`)

/* ── Engine / analysis endpoints ──────────────────────────────── */

export const evaluatePosition = (fen, depth = 10) =>
  post('/engine/evaluate', { fen, depth })

export const getAnalysis = (gameId) => get(`/analysis/${gameId}`)

/* ── Bot battle ────────────────────────────────────────────────── */

export const startBotBattle = (options) => post('/bot-battle', options)
// options: { depth_white, depth_black, games }

/* ── WebSocket analysis stream ─────────────────────────────────── */

/**
 * Opens a WebSocket to /ws/analysis and streams search info back.
 * Returns a cleanup function.
 *
 * @param {string} fen
 * @param {number} depth
 * @param {(info: AnalysisInfo) => void} onInfo
 * @param {() => void} onDone
 */
export function streamAnalysis(fen, depth, onInfo, onDone) {
  const protocol = location.protocol === 'https:' ? 'wss' : 'ws'
  const ws = new WebSocket(`${protocol}://${location.host}/ws/analysis`)

  ws.onopen = () => {
    ws.send(JSON.stringify({ fen, depth }))
  }

  ws.onmessage = (event) => {
    const data = JSON.parse(event.data)
    if (data.type === 'info') {
      onInfo(data)
    } else if (data.type === 'bestmove') {
      onDone && onDone(data.move)
      ws.close()
    }
  }

  ws.onerror = (e) => console.error('WS error', e)

  return () => ws.close()
}
