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
// options: { color: 'white'|'black', depth: number, movetime_ms?: number, engine_id?: string|null }

export const sendMove = (gameId, move) => post('/move', { game_id: gameId, move })
// move: UCI string e.g. 'e2e4'

export const getGame = (gameId) => get(`/game/${gameId}`)

/* ── Engine / analysis endpoints ──────────────────────────────── */

export const evaluatePosition = (fen, depth = 10) =>
  post('/engine/evaluate', { fen, depth, movetime_ms: 0 })

export const evaluatePositionWithEngine = (fen, depth = 10, engineId = null, movetimeMs = 0) =>
  post('/engine/evaluate', { fen, depth, movetime_ms: movetimeMs, engine_id: engineId })

export const getAnalysis = (gameId) => get(`/analysis/${gameId}`)

/* ── Bot battle ────────────────────────────────────────────────── */

export const startBotBattle = (options) => post('/bot-battle', options)
// options: { depth_white, depth_black, games, engine_white_id?: string|null, engine_black_id?: string|null }

export function streamBotBattle(options, handlers = {}) {
  const protocol = location.protocol === 'https:' ? 'wss' : 'ws'
  const ws = new WebSocket(`${protocol}://${location.host}/ws/bot-battle`)
  let completed = false

  ws.onopen = () => {
    ws.send(JSON.stringify(options || {}))
  }

  ws.onmessage = (event) => {
    const data = JSON.parse(event.data)
    if (data.type === 'started') {
      handlers.onStarted && handlers.onStarted(data)
    } else if (data.type === 'ready') {
      handlers.onReady && handlers.onReady(data)
    } else if (data.type === 'game_start') {
      handlers.onGameStart && handlers.onGameStart(data)
    } else if (data.type === 'move') {
      handlers.onMove && handlers.onMove(data)
    } else if (data.type === 'progress') {
      handlers.onProgress && handlers.onProgress(data)
    } else if (data.type === 'done') {
      completed = true
      handlers.onDone && handlers.onDone(data)
      ws.close()
    } else if (data.type === 'error') {
      completed = true
      handlers.onError && handlers.onError(data)
      ws.close()
    }
  }

  ws.onerror = () => {
    handlers.onError && handlers.onError({ message: 'WebSocket error during arena stream' })
  }

  ws.onclose = () => {
    if (!completed) {
      handlers.onError && handlers.onError({ message: 'Battle stream closed before completion' })
    }
  }

  return () => ws.close()
}

export const getEngines = () => get('/engines')
export const getPositionSets = () => get('/position-sets')

/* ── PST config endpoints ─────────────────────────────────────── */

export const getPstConfig = () => get('/pst-config')

export const savePstConfig = (config) => post('/pst-config', { config })

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
export function streamAnalysis(fen, depth, onInfo, onDone, engineId = null, movetimeMs = 0) {
  const protocol = location.protocol === 'https:' ? 'wss' : 'ws'
  const ws = new WebSocket(`${protocol}://${location.host}/ws/analysis`)

  ws.onopen = () => {
    ws.send(JSON.stringify({ fen, depth, movetime_ms: movetimeMs, engine_id: engineId }))
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
