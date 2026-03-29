import { useState, useCallback, useEffect } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import EvalBar from '../components/EvalBar'
import MoveList from '../components/MoveList'
import AnalysisPanel from '../components/AnalysisPanel'
import { sendMove, startGame, streamAnalysis, getEngines } from '../api'
import styles from './PlayPage.module.css'

const DEFAULT_DEPTH = 10

export default function PlayPage() {
  const [game, setGame] = useState(new Chess())
  const [moveHistory, setMoveHistory] = useState([])
  const [gameId, setGameId] = useState(null)
  const [playerColor, setPlayerColor] = useState('white')
  const [depth, setDepth] = useState(DEFAULT_DEPTH)
  const [started, setStarted] = useState(false)
  const [thinking, setThinking] = useState(false)
  const [evalScore, setEvalScore] = useState(0)
  const [pvLine, setPvLine] = useState([])
  const [analysisDepth, setAnalysisDepth] = useState(0)
  const [status, setStatus] = useState('Press Start to begin.')
  const [engines, setEngines] = useState([{ id: 'default', engine_id: null, exists: true }])
  const [engineId, setEngineId] = useState('default')

  useEffect(() => {
    let mounted = true
    getEngines()
      .then((data) => {
        if (!mounted) return
        const available = (data.engines || []).filter(e => e.exists)
        if (available.length > 0) {
          setEngines(available)
          if (!available.find(e => e.id === engineId)) setEngineId(available[0].id)
        }
      })
      .catch(() => {
        if (!mounted) return
        setEngines([{ id: 'default', engine_id: null, exists: true }])
        setEngineId('default')
      })
    return () => { mounted = false }
  }, [])

  function selectedEnginePayload() {
    return engineId === 'default' ? null : engineId
  }

  /* ── helpers ──────────────────────────────────────────────────── */

  const cloneGame = (g) => { const c = new Chess(); c.loadPgn(g.pgn()); return c }
  const gameFromFen = (fen) => { const c = new Chess(); c.load(fen); return c }

  function applyUciMove(baseGame, uci) {
    if (!uci || uci.length < 4) return { game: baseGame, san: null }
    const next = cloneGame(baseGame)
    const applied = next.move({
      from: uci.slice(0, 2),
      to: uci.slice(2, 4),
      promotion: uci[4] || 'q'
    })
    return { game: next, san: applied?.san || null }
  }

  function updateStatus(g) {
    if (g.isCheckmate()) setStatus('Checkmate! ' + (g.turn() === 'w' ? 'Black' : 'White') + ' wins.')
    else if (g.isDraw()) setStatus('Draw.')
    else if (g.isCheck()) setStatus((g.turn() === 'w' ? 'White' : 'Black') + ' is in check.')
    else setStatus((g.turn() === 'w' ? 'White' : 'Black') + ' to move.')
  }

  /* ── engine response ──────────────────────────────────────────── */

  async function fetchEngineMove(currentGame, gId) {
    setThinking(true)
    const fen = currentGame.fen()
    const cleanup = streamAnalysis(fen, depth, (info) => {
      if (info.score !== undefined) setEvalScore(info.score / 100)
      if (info.pv)    setPvLine(info.pv.split(' '))
      if (info.depth) setAnalysisDepth(info.depth)
    }, async (bestmove) => {
      setThinking(false)
      if (!bestmove || bestmove === '(none)') return
      const { game: updated, san } = applyUciMove(currentGame, bestmove)
      setGame(updated)
      if (san) setMoveHistory(prev => [...prev, san])
      updateStatus(updated)
      // For local fallback mode only (no backend game id), apply bestmove locally.
    }, selectedEnginePayload())
    return cleanup
  }

  /* ── start game ───────────────────────────────────────────────── */

  async function handleStart() {
    const fresh = new Chess()
    setGame(fresh)
    setMoveHistory([])
    setPvLine([])
    setEvalScore(0)
    setAnalysisDepth(0)
    setStarted(true)

    let gId = gameId
    try {
      const res = await startGame({ color: playerColor, depth, engine_id: selectedEnginePayload() })
      gId = res.game_id
      setGameId(gId)

      // If backend already made the opening move (playing black), keep SAN history coherent.
      if (res?.engine_move) {
        const { game: withEngineMove, san } = applyUciMove(fresh, res.engine_move)
        setGame(withEngineMove)
        if (san) setMoveHistory([san])
      } else if (res?.fen) {
        setGame(gameFromFen(res.fen))
      }
    } catch {
      // backend not running — still allow local play
      setGameId(null)
    }

    setStatus((playerColor === 'white' ? 'White' : 'Black') + ' to move.')

    // If playing black without backend, engine moves first in local fallback mode.
    if (playerColor === 'black' && !gId) {
      fetchEngineMove(fresh, gId)
    }
  }

  /* ── player move ──────────────────────────────────────────────── */

  const onDrop = useCallback((sourceSquare, targetSquare, piece) => {
    if (!started || thinking) return false
    if (game.isGameOver()) return false

    const isPlayerTurn =
      (playerColor === 'white' && game.turn() === 'w') ||
      (playerColor === 'black' && game.turn() === 'b')
    if (!isPlayerTurn) return false

    const promo = piece?.slice(-1).toLowerCase()
    const base = cloneGame(game)
    const move = base.move({
      from: sourceSquare,
      to: targetSquare,
      promotion: promo === 'p' ? 'q' : (promo || 'q')
    })
    if (!move) return false

    const updated = base
    setGame(updated)
    setMoveHistory(prev => [...prev, move.san])
    updateStatus(updated)

    if (!updated.isGameOver()) {
      if (gameId) {
        setThinking(true)
        sendMove(gameId, move.lan)
          .then((res) => {
            setThinking(false)
            if (res?.engine_move) {
              const { game: withEngineMove, san } = applyUciMove(updated, res.engine_move)
              setGame(withEngineMove)
              if (san) setMoveHistory(prev => [...prev, san])
              updateStatus(withEngineMove)
              return
            }
            if (res?.fen) {
              const synced = gameFromFen(res.fen)
              setGame(synced)
              updateStatus(synced)
            }
          })
          .catch(() => {
            setThinking(false)
            // Fallback path if backend is unavailable.
            fetchEngineMove(updated, null)
          })
      } else {
        fetchEngineMove(updated, null)
      }
    }
    return true
  }, [game, started, thinking, playerColor, gameId, depth])

  /* ── reset ────────────────────────────────────────────────────── */

  function handleReset() {
    setGame(new Chess())
    setMoveHistory([])
    setStarted(false)
    setThinking(false)
    setPvLine([])
    setEvalScore(0)
    setAnalysisDepth(0)
    setStatus('Press Start to begin.')
  }

  const boardOrientation = playerColor === 'black' ? 'black' : 'white'

  return (
    <div className={styles.layout}>
      {/* ── left: settings + board ── */}
      <div className={styles.left}>
        {!started && (
          <div className={styles.settings}>
            <label>
              Play as&nbsp;
              <select value={playerColor} onChange={e => setPlayerColor(e.target.value)}>
                <option value="white">White</option>
                <option value="black">Black</option>
              </select>
            </label>
            <label>
              Engine depth&nbsp;
              <input
                type="number" min={1} max={20} value={depth}
                onChange={e => setDepth(Number(e.target.value))}
                className={styles.depthInput}
              />
            </label>
            <label>
              Engine version&nbsp;
              <select value={engineId} onChange={e => setEngineId(e.target.value)}>
                {engines.map((engine) => (
                  <option key={engine.id} value={engine.id}>{engine.id}</option>
                ))}
              </select>
            </label>
          </div>
        )}

        <div className={styles.boardRow}>
          <EvalBar score={evalScore} orientation={boardOrientation} />
          <div className={styles.boardWrap}>
            <Chessboard
              id="play-board"
              position={game.fen()}
              onPieceDrop={onDrop}
              boardOrientation={boardOrientation}
              customBoardStyle={{ borderRadius: '4px', boxShadow: '0 4px 24px #0008' }}
            />
          </div>
        </div>

        <div className={styles.controls}>
          <span className={styles.status}>{thinking ? '⏳ Engine thinking…' : status}</span>
          {!started
            ? <button className={styles.btn} onClick={handleStart}>Start Game</button>
            : <button className={styles.btnSecondary} onClick={handleReset}>New Game</button>
          }
        </div>
      </div>

      {/* ── right: move list + analysis ── */}
      <div className={styles.right}>
        <MoveList history={moveHistory} />
        <AnalysisPanel depth={analysisDepth} pv={pvLine} score={evalScore} />
      </div>
    </div>
  )
}
