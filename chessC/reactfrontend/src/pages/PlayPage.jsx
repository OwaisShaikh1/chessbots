import { useState, useCallback } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import EvalBar from '../components/EvalBar'
import MoveList from '../components/MoveList'
import AnalysisPanel from '../components/AnalysisPanel'
import { sendMove, startGame, streamAnalysis } from '../api'
import styles from './PlayPage.module.css'

const DEFAULT_DEPTH = 10

export default function PlayPage() {
  const [game, setGame] = useState(new Chess())
  const [gameId, setGameId] = useState(null)
  const [playerColor, setPlayerColor] = useState('white')
  const [depth, setDepth] = useState(DEFAULT_DEPTH)
  const [started, setStarted] = useState(false)
  const [thinking, setThinking] = useState(false)
  const [evalScore, setEvalScore] = useState(0)
  const [pvLine, setPvLine] = useState([])
  const [analysisDepth, setAnalysisDepth] = useState(0)
  const [status, setStatus] = useState('Press Start to begin.')

  /* ── helpers ──────────────────────────────────────────────────── */

  const cloneGame = (g) => { const c = new Chess(); c.loadPgn(g.pgn()); return c }

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
      const from = bestmove.slice(0, 2)
      const to   = bestmove.slice(2, 4)
      const promo = bestmove[4] || undefined
      const updated = cloneGame(currentGame)
      updated.move({ from, to, promotion: promo ?? 'q' })
      setGame(updated)
      updateStatus(updated)
      // also notify backend
      if (gId) sendMove(gId, bestmove).catch(console.error)
    })
    return cleanup
  }

  /* ── start game ───────────────────────────────────────────────── */

  async function handleStart() {
    const fresh = new Chess()
    setGame(fresh)
    setPvLine([])
    setEvalScore(0)
    setAnalysisDepth(0)
    setStarted(true)

    let gId = gameId
    try {
      const res = await startGame({ color: playerColor, depth })
      gId = res.game_id
      setGameId(gId)
    } catch {
      // backend not running — still allow local play
      setGameId(null)
    }

    setStatus((playerColor === 'white' ? 'White' : 'Black') + ' to move.')

    // If playing black, engine moves first
    if (playerColor === 'black') {
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
    const move = game.move({
      from: sourceSquare,
      to: targetSquare,
      promotion: promo === 'p' ? 'q' : (promo || 'q')
    })
    if (!move) return false

    const updated = cloneGame(game)
    setGame(updated)
    updateStatus(updated)

    if (!updated.isGameOver()) {
      fetchEngineMove(updated, gameId)
    }
    return true
  }, [game, started, thinking, playerColor, gameId, depth])

  /* ── reset ────────────────────────────────────────────────────── */

  function handleReset() {
    setGame(new Chess())
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
        <MoveList history={game.history()} />
        <AnalysisPanel depth={analysisDepth} pv={pvLine} score={evalScore} />
      </div>
    </div>
  )
}
