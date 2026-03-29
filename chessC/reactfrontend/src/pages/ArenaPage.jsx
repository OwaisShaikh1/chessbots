import { useState, useRef, useEffect } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import { getEngines, evaluatePositionWithEngine } from '../api'
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
  const [engines, setEngines]       = useState([{ id: 'default', engine_id: null, exists: true }])
  const [whiteEngineId, setWhiteEngineId] = useState('default')
  const [blackEngineId, setBlackEngineId] = useState('default')
  const stopRef = useRef(false)

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

  function cloneGame(g) { const c = new Chess(); c.loadPgn(g.pgn()); return c }

  async function fetchBestmove(fen, depth, engineId) {
    // In local mode, call the backend; gracefully skip if unavailable
    try {
      const payloadId = engineId === 'default' ? null : engineId
      const data = await evaluatePositionWithEngine(fen, depth, payloadId)
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
      const engineId = current.turn() === 'w' ? whiteEngineId : blackEngineId
      const bm = await fetchBestmove(current.fen(), depth, engineId)
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
