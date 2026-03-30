import { useState, useRef, useEffect } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import { getEngines, evaluatePositionWithEngine, getPositionSets, streamBotBattle } from '../api'
import MoveList from '../components/MoveList'
import styles from './ArenaPage.module.css'

const DEFAULT_TIME_MODE_DEPTH_CAP = 64

export default function ArenaPage() {
  const [arenaMode, setArenaMode] = useState('quick')
  const [searchMode, setSearchMode] = useState('depth')
  const [depthWhite, setDepthWhite] = useState(6)
  const [depthBlack, setDepthBlack] = useState(6)
  const [moveTimeWhiteMs, setMoveTimeWhiteMs] = useState(1500)
  const [moveTimeBlackMs, setMoveTimeBlackMs] = useState(1500)
  const [delayMs, setDelayMs]       = useState(500)
  const [game, setGame]             = useState(new Chess())
  const [running, setRunning]       = useState(false)
  const [result, setResult]         = useState('')
  const [scores, setScores]         = useState({ bot1: 0, draw: 0, bot2: 0 })
  const [engines, setEngines]       = useState([{ id: 'default', engine_id: null, exists: true }])
  const [whiteEngineId, setWhiteEngineId] = useState('default')
  const [blackEngineId, setBlackEngineId] = useState('default')
  const [positionSets, setPositionSets] = useState([])
  const [positionSetId, setPositionSetId] = useState('')
  const [fixedSummary, setFixedSummary] = useState(null)
  const [sessionLogFile, setSessionLogFile] = useState('')
  const [liveGameLabel, setLiveGameLabel] = useState('')
  const [streamStats, setStreamStats] = useState({ ready: 0, started: 0, gameStart: 0, move: 0, progress: 0 })
  const [boardSideLabels, setBoardSideLabels] = useState({
    top: 'Top (Black): default',
    bottom: 'Bottom (White): default'
  })
  const stopRef = useRef(false)
  const fixedStreamCloseRef = useRef(null)
  const moveQueueRef = useRef([])
  const playbackTimerRef = useRef(null)
  const playbackActiveRef = useRef(false)
  const delayMsRef = useRef(delayMs)
  const liveGameRef = useRef(new Chess())

  useEffect(() => {
    let mounted = true
    getEngines()
      .then((data) => {
        if (!mounted) return
        const available = (data.engines || []).filter(e => e.exists)
        if (available.length > 0) {
          setEngines(available)
          if (!available.find(e => e.id === whiteEngineId)) setWhiteEngineId(available[0].id)
          if (!available.find(e => e.id === blackEngineId)) setBlackEngineId(available[0].id)
        }
      })
      .catch(() => {
        if (!mounted) return
        setEngines([{ id: 'default', engine_id: null, exists: true }])
        setWhiteEngineId('default')
        setBlackEngineId('default')
      })
    return () => { mounted = false }
  }, [])

  useEffect(() => {
    return () => {
      if (fixedStreamCloseRef.current) {
        fixedStreamCloseRef.current()
        fixedStreamCloseRef.current = null
      }
      if (playbackTimerRef.current) {
        clearTimeout(playbackTimerRef.current)
        playbackTimerRef.current = null
      }
    }
  }, [])

  useEffect(() => {
    delayMsRef.current = delayMs
  }, [delayMs])

  useEffect(() => {
    let mounted = true
    getPositionSets()
      .then((data) => {
        if (!mounted) return
        const sets = data?.sets || []
        setPositionSets(sets)
        if (sets.length > 0) {
          setPositionSetId(sets[0].id)
        }
      })
      .catch(() => {
        if (!mounted) return
        setPositionSets([])
        setPositionSetId('')
      })
    return () => { mounted = false }
  }, [])

  function cloneGame(g) { const c = new Chess(); c.loadPgn(g.pgn()); return c }
  const bot1Id = whiteEngineId === 'default' ? 'default' : whiteEngineId
  const bot2Id = blackEngineId === 'default' ? 'default' : blackEngineId
  const bot1Label = `Bot 1 (${bot1Id})`
  const bot2Label = `Bot 2 (${bot2Id})`

  useEffect(() => {
    if (arenaMode !== 'fixed' || !running) {
      setBoardSideLabels({
        top: `Top (Black): ${bot2Id}`,
        bottom: `Bottom (White): ${bot1Id}`
      })
    }
  }, [arenaMode, running, bot1Id, bot2Id])
  function gameFromFen(fen) {
    try {
      const c = new Chess()
      c.load(fen)
      return c
    } catch {
      return new Chess()
    }
  }

  function stopMovePlayback() {
    moveQueueRef.current = []
    playbackActiveRef.current = false
    if (playbackTimerRef.current) {
      clearTimeout(playbackTimerRef.current)
      playbackTimerRef.current = null
    }
  }

  function syncVisibleGame(g) {
    liveGameRef.current = g
    setGame(cloneGame(g))
  }

  function pumpMovePlayback() {
    if (playbackActiveRef.current) return
    playbackActiveRef.current = true

    const step = () => {
      const nextEvent = moveQueueRef.current.shift()
      if (!nextEvent) {
        playbackActiveRef.current = false
        playbackTimerRef.current = null
        return
      }

      const current = liveGameRef.current || new Chess()
      const updated = cloneGame(current)
      const bm = nextEvent?.move || ''
      const from = bm.slice(0, 2)
      const to = bm.slice(2, 4)
      const promotion = bm[4] || 'q'
      let applied = null
      if (bm.length >= 4) {
        try {
          applied = updated.move({ from, to, promotion })
        } catch {
          applied = null
        }
      }

      if (!applied && nextEvent?.fen) {
        syncVisibleGame(gameFromFen(nextEvent.fen))
      } else {
        syncVisibleGame(updated)
      }

      const waitMs = Math.max(0, Number(delayMsRef.current) || 0)
      if (waitMs > 0) {
        playbackTimerRef.current = setTimeout(step, waitMs)
      } else {
        step()
      }
    }

    step()
  }

  async function fetchBestmove(fen, depth, engineId, moveTimeMs) {
    // In local mode, call the backend; gracefully skip if unavailable
    try {
      const payloadId = engineId === 'default' ? null : engineId
      const data = await evaluatePositionWithEngine(fen, depth, payloadId, moveTimeMs)
      return data.bestmove
    } catch {
      return null
    }
  }

  async function runGame() {
    const fresh = new Chess()
    setGame(fresh)
    setResult('')

    let current = fresh
    while (!current.isGameOver() && !stopRef.current) {
      const depthValue = current.turn() === 'w' ? depthWhite : depthBlack
      const timeValueMs = current.turn() === 'w' ? moveTimeWhiteMs : moveTimeBlackMs
      const depth = searchMode === 'time'
        ? DEFAULT_TIME_MODE_DEPTH_CAP
        : Math.max(1, Number(depthValue) || 1)
      const moveTimeMs = searchMode === 'time'
        ? Math.max(0, Number(timeValueMs) || 0)
        : 0
      const engineId = current.turn() === 'w' ? whiteEngineId : blackEngineId
      const bm = await fetchBestmove(current.fen(), depth, engineId, moveTimeMs)
      if (!bm || stopRef.current) break

      const from  = bm.slice(0, 2)
      const to    = bm.slice(2, 4)
      const promo = bm[4] || 'q'
      const updated = cloneGame(current)
      const applied = updated.move({ from, to, promotion: promo })
      if (!applied) {
        setResult(`Engine returned invalid move: ${bm}`)
        break
      }
      current = updated
      setGame(cloneGame(updated))

      await new Promise(r => setTimeout(r, delayMs))
    }

    if (current.isCheckmate()) {
      const winner = current.turn() === 'w' ? 'Black' : 'White'
      setResult(`${winner} wins by checkmate`)
      setScores((s) => {
        if (winner === 'White') return { ...s, bot1: s.bot1 + 1 }
        return { ...s, bot2: s.bot2 + 1 }
      })
    } else if (current.isDraw()) {
      setResult('Draw')
      setScores(s => ({ ...s, draw: s.draw + 1 }))
    } else {
      setResult('Game stopped')
    }
  }

  async function handleStart() {
    if (arenaMode === 'fixed') {
      setRunning(true)
      setResult('Running fixed arena session...')
      setFixedSummary(null)
      setSessionLogFile('')
      setLiveGameLabel('')
      setStreamStats({ ready: 0, started: 0, gameStart: 0, move: 0, progress: 0 })
      stopMovePlayback()
      syncVisibleGame(new Chess())

      const depthWhiteReq = searchMode === 'time'
        ? DEFAULT_TIME_MODE_DEPTH_CAP
        : Math.max(1, Number(depthWhite) || 1)
      const depthBlackReq = searchMode === 'time'
        ? DEFAULT_TIME_MODE_DEPTH_CAP
        : Math.max(1, Number(depthBlack) || 1)
      const movetimeWhiteReq = searchMode === 'time'
        ? Math.max(0, Number(moveTimeWhiteMs) || 0)
        : 0
      const movetimeBlackReq = searchMode === 'time'
        ? Math.max(0, Number(moveTimeBlackMs) || 0)
        : 0

      const payload = {
        depth_white: depthWhiteReq,
        depth_black: depthBlackReq,
        movetime_white_ms: movetimeWhiteReq,
        movetime_black_ms: movetimeBlackReq,
        games: 1,
        engine_white_id: whiteEngineId === 'default' ? null : whiteEngineId,
        engine_black_id: blackEngineId === 'default' ? null : blackEngineId,
        position_set: positionSetId || null,
        mirror_positions: true,
        limit_positions: 500,
        max_plies: 300,
        return_games_in_response: false
      }

      if (fixedStreamCloseRef.current) {
        fixedStreamCloseRef.current()
        fixedStreamCloseRef.current = null
      }

      fixedStreamCloseRef.current = streamBotBattle(payload, {
        onReady: () => {
          setStreamStats((prev) => ({ ...prev, ready: prev.ready + 1 }))
          setResult('Connected to battle stream, preparing session...')
        },
        onStarted: (msg) => {
          setStreamStats((prev) => ({ ...prev, started: prev.started + 1 }))
          const totalGames = Number(msg?.total_games) || 0
          setFixedSummary({
            total_games: totalGames,
            draws: 0,
            unfinished: 0,
            by_engine: {
              [bot1Id]: { wins: 0, losses: 0, draws: 0, games: 0 },
              [bot2Id]: { wins: 0, losses: 0, draws: 0, games: 0 }
            }
          })
          setResult(`Running fixed arena session... 0/${totalGames}`)
        },
        onGameStart: (msg) => {
          setStreamStats((prev) => ({ ...prev, gameStart: prev.gameStart + 1 }))
          const pairIndex = Number(msg?.pair_index) || 0
          const leg = Number(msg?.leg) || 0
          setLiveGameLabel(`Pair ${pairIndex} Leg ${leg}`)
          const whiteId = msg?.white_engine_id ?? 'default'
          const blackId = msg?.black_engine_id ?? 'default'
          setBoardSideLabels({
            top: `Top (Black): ${blackId}`,
            bottom: `Bottom (White): ${whiteId}`
          })
          stopMovePlayback()
          if (msg?.initial_fen) {
            syncVisibleGame(gameFromFen(msg.initial_fen))
          } else {
            syncVisibleGame(new Chess())
          }
        },
        onMove: (msg) => {
          setStreamStats((prev) => ({ ...prev, move: prev.move + 1 }))
          moveQueueRef.current.push(msg)
          pumpMovePlayback()
        },
        onProgress: (msg) => {
          setStreamStats((prev) => ({ ...prev, progress: prev.progress + 1 }))
          const completed = Number(msg?.completed_games) || 0
          const totalGames = Number(msg?.total_games) || 0
          setFixedSummary((prev) => {
            const base = prev || {
              total_games: totalGames,
              draws: 0,
              unfinished: 0,
              by_engine: {
                [bot1Id]: { wins: 0, losses: 0, draws: 0, games: 0 },
                [bot2Id]: { wins: 0, losses: 0, draws: 0, games: 0 }
              }
            }
            const next = {
              ...base,
              total_games: totalGames || base.total_games || 0,
              by_engine: { ...(base.by_engine || {}) }
            }
            if (msg?.winner === 'draw') {
              next.draws += 1
            } else if (msg?.winner_engine_id) {
              const winnerId = msg.winner_engine_id
              const winnerRow = next.by_engine[winnerId] || { wins: 0, losses: 0, draws: 0, games: 0 }
              next.by_engine[winnerId] = { ...winnerRow, wins: winnerRow.wins + 1 }
            } else {
              next.unfinished += 1
            }
            return next
          })
          setResult(`Running fixed arena session... ${completed}/${totalGames}`)
        },
        onDone: (data) => {
          stopMovePlayback()
          setFixedSummary(data?.summary || null)
          setSessionLogFile(data?.log_file || '')
          setLiveGameLabel('')
          setResult(`Fixed arena completed: ${data?.total_games || 0} games.`)
          setRunning(false)
          fixedStreamCloseRef.current = null
        },
        onError: (err) => {
          stopMovePlayback()
          setLiveGameLabel('')
          setResult(`Fixed arena failed: ${err?.message || 'Unknown error'}`)
          setRunning(false)
          fixedStreamCloseRef.current = null
        }
      })
      return
    }

    stopRef.current = false
    setRunning(true)
    await runGame()
    setRunning(false)
  }

  function handleStop() {
    stopRef.current = true
    stopMovePlayback()
    if (fixedStreamCloseRef.current) {
      fixedStreamCloseRef.current()
      fixedStreamCloseRef.current = null
    }
    setLiveGameLabel('')
    setRunning(false)
  }

  function handleReset() {
    stopRef.current = true
    stopMovePlayback()
    if (fixedStreamCloseRef.current) {
      fixedStreamCloseRef.current()
      fixedStreamCloseRef.current = null
    }
    setLiveGameLabel('')
    setRunning(false)
    syncVisibleGame(new Chess())
    setResult('')
    setScores({ bot1: 0, draw: 0, bot2: 0 })
  }

  return (
    <div className={styles.layout}>
      <div className={styles.left}>
        {/* Settings */}
        <div className={styles.settings}>
          <label>
            Arena mode&nbsp;
            <select value={arenaMode} onChange={e => setArenaMode(e.target.value)}>
              <option value="quick">Quick local</option>
              <option value="fixed">Fixed set (mirrored)</option>
            </select>
          </label>
          <label>
            Search mode&nbsp;
            <select value={searchMode} onChange={e => setSearchMode(e.target.value)}>
              <option value="depth">Depth</option>
              <option value="time">Time</option>
            </select>
          </label>
          {arenaMode === 'fixed' && (
            <label>
              Position set&nbsp;
              <select value={positionSetId} onChange={e => setPositionSetId(e.target.value)}>
                {positionSets.map((setInfo) => (
                  <option key={setInfo.id} value={setInfo.id}>
                    {setInfo.id} ({setInfo.count})
                  </option>
                ))}
              </select>
            </label>
          )}
          {searchMode === 'depth' ? (
            <>
              <label>
                White depth&nbsp;
                <input type="number" min={1} max={20} value={depthWhite}
                  onChange={e => setDepthWhite(Number(e.target.value))} className={styles.numInput} />
              </label>
              <label>
                Black depth&nbsp;
                <input type="number" min={1} max={20} value={depthBlack}
                  onChange={e => setDepthBlack(Number(e.target.value))} className={styles.numInput} />
              </label>
            </>
          ) : (
            <>
              <label>
                White move time (ms)&nbsp;
                <input type="number" min={0} max={60000} step={100} value={moveTimeWhiteMs}
                  onChange={e => setMoveTimeWhiteMs(Number(e.target.value))} className={styles.numInput} />
              </label>
              <label>
                Black move time (ms)&nbsp;
                <input type="number" min={0} max={60000} step={100} value={moveTimeBlackMs}
                  onChange={e => setMoveTimeBlackMs(Number(e.target.value))} className={styles.numInput} />
              </label>
            </>
          )}
          <label>
            White engine&nbsp;
            <select value={whiteEngineId} onChange={e => setWhiteEngineId(e.target.value)}>
              {engines.map((engine) => (
                <option key={`white-${engine.id}`} value={engine.id}>{engine.id}</option>
              ))}
            </select>
          </label>
          <label>
            Black engine&nbsp;
            <select value={blackEngineId} onChange={e => setBlackEngineId(e.target.value)}>
              {engines.map((engine) => (
                <option key={`black-${engine.id}`} value={engine.id}>{engine.id}</option>
              ))}
            </select>
          </label>
          <label>
            Delay (ms)&nbsp;
            <input type="number" min={0} max={5000} step={100} value={delayMs}
              onChange={e => setDelayMs(Number(e.target.value))} className={styles.numInput} />
          </label>
        </div>

        {/* Board */}
        <div className={styles.boardWrap}>
          <div style={{ marginBottom: 8, fontSize: 12, color: '#a9b7d0' }}>{boardSideLabels.top}</div>
          <Chessboard
            id="arena-board"
            position={game.fen()}
            arePiecesDraggable={false}
            customBoardStyle={{ borderRadius: '4px', boxShadow: '0 4px 24px #0008' }}
          />
          <div style={{ marginTop: 8, fontSize: 12, color: '#a9b7d0' }}>{boardSideLabels.bottom}</div>
        </div>

        {/* Controls */}
        <div className={styles.controls}>
          {!running ? (
            <button className={styles.btn} onClick={handleStart}>Start</button>
          ) : (
            arenaMode === 'quick'
              ? <button className={styles.btnStop} onClick={handleStop}>Stop</button>
              : <button className={styles.btnStop} disabled>Running...</button>
          )}
          <button className={styles.btnSecondary} onClick={handleReset}>Reset</button>
          {result && <span className={styles.result}>{result}</span>}
          {liveGameLabel && <span className={styles.result}>{liveGameLabel}</span>}
        </div>
      </div>

      <div className={styles.right}>
        {/* Score summary */}
        <div className={styles.card}>
          <h3 className={styles.cardTitle}>Score</h3>
          {(() => {
            const byEngine = fixedSummary?.by_engine || {}
            const bot1Wins = fixedSummary ? (byEngine[bot1Id]?.wins ?? 0) : scores.bot1
            const bot2Wins = fixedSummary ? (byEngine[bot2Id]?.wins ?? 0) : scores.bot2
            const draws = fixedSummary ? (fixedSummary.draws ?? 0) : scores.draw
            return (
          <div className={styles.scoreRow}>
            <div className={styles.scoreItem}>
                  <span className={styles.scoreNum}>{bot1Wins}</span>
                  <span className={styles.scoreLabel}>{bot1Label}</span>
            </div>
            <div className={styles.scoreItem}>
                  <span className={styles.scoreNum}>{draws}</span>
              <span className={styles.scoreLabel}>Draw</span>
            </div>
            <div className={styles.scoreItem}>
                  <span className={styles.scoreNum}>{bot2Wins}</span>
                  <span className={styles.scoreLabel}>{bot2Label}</span>
            </div>
          </div>
            )
          })()}
          {fixedSummary && (
            <div style={{ marginTop: 12, fontSize: 12 }}>
              <div>Total games: {fixedSummary.total_games}</div>
              <div>Unfinished: {fixedSummary.unfinished}</div>
              <div>
                Stream: ready {streamStats.ready}, started {streamStats.started}, game_start {streamStats.gameStart}, move {streamStats.move}, progress {streamStats.progress}
              </div>
              {sessionLogFile && <div>Log: {sessionLogFile}</div>}
            </div>
          )}
        </div>

        <MoveList history={game.history()} />
      </div>
    </div>
  )
}
