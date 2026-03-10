import { useState, useRef } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import { startBotBattle } from '../api'
import MoveList from '../components/MoveList'
import styles from './ArenaPage.module.css'

export default function ArenaPage() {
  const [depthWhite, setDepthWhite] = useState(6)
  const [depthBlack, setDepthBlack] = useState(6)
  const [delayMs, setDelayMs]       = useState(500)
  const [game, setGame]             = useState(new Chess())
  const [running, setRunning]       = useState(false)
  const [result, setResult]         = useState('')
  const [scores, setScores]         = useState({ white: 0, draw: 0, black: 0 })
  const stopRef = useRef(false)

  function cloneGame(g) { const c = new Chess(); c.loadPgn(g.pgn()); return c }

  async function fetchBestmove(fen, depth) {
    // In local mode, call the backend; gracefully skip if unavailable
    try {
      const res = await fetch('/api/engine/evaluate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ fen, depth })
      })
      if (!res.ok) throw new Error()
      const data = await res.json()
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
      const depth  = current.turn() === 'w' ? depthWhite : depthBlack
      const bm = await fetchBestmove(current.fen(), depth)
      if (!bm || stopRef.current) break

      const from  = bm.slice(0, 2)
      const to    = bm.slice(2, 4)
      const promo = bm[4] || 'q'
      const updated = cloneGame(current)
      updated.move({ from, to, promotion: promo })
      current = updated
      setGame(cloneGame(updated))

      await new Promise(r => setTimeout(r, delayMs))
    }

    if (current.isCheckmate()) {
      const winner = current.turn() === 'w' ? 'Black' : 'White'
      setResult(`${winner} wins by checkmate`)
      setScores(s => ({
        ...s,
        [winner.toLowerCase()]: s[winner.toLowerCase()] + 1
      }))
    } else if (current.isDraw()) {
      setResult('Draw')
      setScores(s => ({ ...s, draw: s.draw + 1 }))
    } else {
      setResult('Game stopped')
    }
  }

  async function handleStart() {
    stopRef.current = false
    setRunning(true)
    await runGame()
    setRunning(false)
  }

  function handleStop() {
    stopRef.current = true
    setRunning(false)
  }

  function handleReset() {
    stopRef.current = true
    setRunning(false)
    setGame(new Chess())
    setResult('')
    setScores({ white: 0, draw: 0, black: 0 })
  }

  return (
    <div className={styles.layout}>
      <div className={styles.left}>
        {/* Settings */}
        <div className={styles.settings}>
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
          <label>
            Delay (ms)&nbsp;
            <input type="number" min={0} max={5000} step={100} value={delayMs}
              onChange={e => setDelayMs(Number(e.target.value))} className={styles.numInput} />
          </label>
        </div>

        {/* Board */}
        <div className={styles.boardWrap}>
          <Chessboard
            id="arena-board"
            position={game.fen()}
            arePiecesDraggable={false}
            customBoardStyle={{ borderRadius: '4px', boxShadow: '0 4px 24px #0008' }}
          />
        </div>

        {/* Controls */}
        <div className={styles.controls}>
          {!running
            ? <button className={styles.btn} onClick={handleStart}>Start</button>
            : <button className={styles.btnStop} onClick={handleStop}>Stop</button>
          }
          <button className={styles.btnSecondary} onClick={handleReset}>Reset</button>
          {result && <span className={styles.result}>{result}</span>}
        </div>
      </div>

      <div className={styles.right}>
        {/* Score summary */}
        <div className={styles.card}>
          <h3 className={styles.cardTitle}>Score</h3>
          <div className={styles.scoreRow}>
            <div className={styles.scoreItem}>
              <span className={styles.scoreNum}>{scores.white}</span>
              <span className={styles.scoreLabel}>White</span>
            </div>
            <div className={styles.scoreItem}>
              <span className={styles.scoreNum}>{scores.draw}</span>
              <span className={styles.scoreLabel}>Draw</span>
            </div>
            <div className={styles.scoreItem}>
              <span className={styles.scoreNum}>{scores.black}</span>
              <span className={styles.scoreLabel}>Black</span>
            </div>
          </div>
        </div>

        <MoveList history={game.history()} />
      </div>
    </div>
  )
}
