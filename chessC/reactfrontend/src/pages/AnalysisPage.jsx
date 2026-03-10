import { useState, useRef, useEffect } from 'react'
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js'
import {
  LineChart, Line, XAxis, YAxis, Tooltip,
  ResponsiveContainer, ReferenceLine
} from 'recharts'
import { streamAnalysis } from '../api'
import EvalBar from '../components/EvalBar'
import styles from './AnalysisPage.module.css'

const START_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1'
const MAX_DEPTH = 20
const QUALITY_ICON = { blunder: '🔴', mistake: '🟠', inaccuracy: '🟡', good: '🟢' }

/** Parse a PGN string → array of FENs per ply (index 0 = start). */
function pgnToFens(pgn) {
  const g = new Chess()
  g.loadPgn(pgn)
  const history = g.history({ verbose: true })
  const fens = [new Chess().fen()]
  const walker = new Chess()
  for (const move of history) {
    walker.move(move)
    fens.push(walker.fen())
  }
  return { fens, history }
}

/** Normalise a side-to-move UCI score to white's perspective. */
function getWhitePov(fenStr, rawScore) {
  try { const g = new Chess(); g.load(fenStr); return g.turn() === 'w' ? rawScore : -rawScore }
  catch { return rawScore }
}

/** Convert a UCI move string (e.g. "e2e4") to SAN for the given FEN. */
function uciToSan(fenStr, uciMove) {
  if (!uciMove || uciMove.length < 4) return uciMove
  try {
    const g = new Chess(); g.load(fenStr)
    const m = g.move({ from: uciMove.slice(0, 2), to: uciMove.slice(2, 4), promotion: uciMove[4] })
    return m?.san || uciMove
  } catch { return uciMove }
}

export default function AnalysisPage() {
  // ── Input mode ────────────────────────────────────────────────────────────
  const [mode, setMode]         = useState('fen')
  const [fenInput, setFenInput] = useState(START_FEN)
  const [pgnInput, setPgnInput] = useState('')

  // ── Active position ───────────────────────────────────────────────────────
  const [fen, setFen]           = useState(START_FEN)

  // ── PGN navigation ────────────────────────────────────────────────────────
  const [pgnFens, setPgnFens]   = useState([])
  const [pgnMoves, setPgnMoves] = useState([])
  const [plyIndex, setPlyIndex] = useState(0)

  // ── Analysis ──────────────────────────────────────────────────────────────
  const [depth, setDepth]       = useState(15)
  const [autoMode, setAutoMode] = useState(false)
  const [running, setRunning]   = useState(false)
  const [evalScore, setEvalScore] = useState(0)    // pawns, white's POV
  const [evalHistory, setEvalHistory] = useState([])
  const [pvLine, setPvLine]     = useState([])
  const [bestmove, setBestmove] = useState('')
  const [error, setError]       = useState('')

  // ── Move quality ──────────────────────────────────────────────────────────
  const [moveEvals, setMoveEvals]     = useState({})   // ply → {gain, quality, bestmove}
  const [evaluating, setEvaluating]   = useState(false)
  const [evalProgress, setEvalProgress] = useState(0)

  const cleanupRef    = useRef(null)
  const evalCancelRef = useRef(false)
  const moveListRef   = useRef(null)

  // ── Keyboard navigation ───────────────────────────────────────────────────
  useEffect(() => {
    function onKey(e) {
      if (pgnFens.length === 0) return
      if (e.key === 'ArrowLeft')  goToPly(plyIndex - 1)
      if (e.key === 'ArrowRight') goToPly(plyIndex + 1)
      if (e.key === 'ArrowUp')    goToPly(0)
      if (e.key === 'ArrowDown')  goToPly(pgnFens.length - 1)
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [pgnFens, plyIndex])

  // ── Auto-analyse: restart stream whenever fen/depth changes ───────────────
  useEffect(() => {
    if (!autoMode) return
    cleanupRef.current?.()
    setEvalHistory([])
    setPvLine([])
    setBestmove('')
    setEvalScore(0)
    setRunning(true)
    let cancelled = false
    const currentFen = fen
    const cleanup = streamAnalysis(currentFen, depth, (info) => {
      if (cancelled) return
      if (info.score !== undefined) {
        const wp = getWhitePov(currentFen, info.score) / 100
        setEvalScore(wp)
        setEvalHistory(prev => [
          ...prev.filter(e => e.depth !== info.depth),
          { depth: info.depth, score: wp }
        ].sort((a, b) => a.depth - b.depth))
      }
      if (info.pv) setPvLine(info.pv.split(' '))
    }, (move) => {
      if (!cancelled) { setBestmove(move); setRunning(false) }
    })
    cleanupRef.current = cleanup
    return () => { cancelled = true; cleanup() }
  }, [fen, autoMode, depth])

  // ── Navigation ────────────────────────────────────────────────────────────
  function goToPly(idx) {
    if (pgnFens.length === 0) return
    const clamped = Math.max(0, Math.min(pgnFens.length - 1, idx))
    setPlyIndex(clamped)
    setFen(pgnFens[clamped])
    if (!autoMode) clearAnalysis()
    setTimeout(() => {
      moveListRef.current?.querySelector(`[data-ply="${clamped}"]`)?.scrollIntoView({ block: 'nearest' })
    }, 0)
  }

  function clearAnalysis() {
    setEvalHistory([]); setPvLine([]); setBestmove(''); setEvalScore(0)
    if (running) { cleanupRef.current?.(); setRunning(false) }
  }

  // ── Load FEN ──────────────────────────────────────────────────────────────
  function loadFen() {
    try {
      new Chess().load(fenInput)
      setFen(fenInput); setPgnFens([]); setPgnMoves([]); setPlyIndex(0)
      setError(''); setMoveEvals({})
      if (!autoMode) clearAnalysis()
    } catch { setError('Invalid FEN string.') }
  }

  function loadStartpos() {
    setFenInput(START_FEN); setFen(START_FEN)
    setPgnFens([]); setPgnMoves([]); setPlyIndex(0)
    setError(''); setMoveEvals({})
    if (!autoMode) clearAnalysis()
  }

  // ── Load PGN ──────────────────────────────────────────────────────────────
  function loadPgn() {
    try {
      const { fens, history } = pgnToFens(pgnInput.trim())
      setPgnFens(fens); setPgnMoves(history.map(m => m.san))
      setPlyIndex(fens.length - 1); setFen(fens[fens.length - 1])
      setError(''); setMoveEvals({})
      if (!autoMode) clearAnalysis()
    } catch { setError('Invalid PGN. Make sure it is complete and correctly formatted.') }
  }

  // ── Manual analysis ───────────────────────────────────────────────────────
  function startAnalysis() {
    if (autoMode) return
    if (running) { cleanupRef.current?.(); setRunning(false); return }
    setEvalHistory([]); setPvLine([]); setBestmove(''); setEvalScore(0)
    setRunning(true)
    const currentFen = fen
    const cleanup = streamAnalysis(currentFen, depth, (info) => {
      if (info.score !== undefined) {
        const wp = getWhitePov(currentFen, info.score) / 100
        setEvalScore(wp)
        setEvalHistory(prev => [
          ...prev.filter(e => e.depth !== info.depth),
          { depth: info.depth, score: wp }
        ].sort((a, b) => a.depth - b.depth))
      }
      if (info.pv) setPvLine(info.pv.split(' '))
    }, (move) => { setBestmove(move); setRunning(false) })
    cleanupRef.current = cleanup
  }

  // ── Evaluate game (sequential per-position analysis) ─────────────────────
  async function evaluateGame() {
    if (!hasPgn || evaluating) return
    evalCancelRef.current = false
    setEvaluating(true); setEvalProgress(0); setMoveEvals({})
    const fens = pgnFens  // snapshot
    const scores = []

    for (let i = 0; i < fens.length; i++) {
      if (evalCancelRef.current) break
      const posFen = fens[i]
      const g = new Chess(); g.load(posFen)
      const isWhiteTurn = g.turn() === 'w'
      const result = await new Promise((resolve) => {
        let lastScore = 0, lastPvMove = ''
        streamAnalysis(posFen, 10, (info) => {
          if (info.score !== undefined) lastScore = info.score
          if (info.pv) lastPvMove = info.pv.split(' ')[0]
        }, (move) => {
          const uciStr = move || lastPvMove
          resolve({
            whitePov: isWhiteTurn ? lastScore : -lastScore,
            bestmoveSan: uciToSan(posFen, uciStr)
          })
        })
      })
      scores[i] = result
      setEvalProgress(i + 1)
    }

    if (!evalCancelRef.current) {
      const quality = {}
      for (let ply = 1; ply < fens.length; ply++) {
        const before = scores[ply - 1]?.whitePov ?? 0
        const after  = scores[ply]?.whitePov ?? 0
        const isWhiteMove = ply % 2 === 1
        const gain = isWhiteMove ? after - before : before - after
        let q = 'good'
        if (gain < -200) q = 'blunder'
        else if (gain < -100) q = 'mistake'
        else if (gain < -50) q = 'inaccuracy'
        quality[ply] = { gain, quality: q, bestmove: scores[ply - 1]?.bestmoveSan }
      }
      setMoveEvals(quality)
    }
    setEvaluating(false)
  }

  function stopEvaluating() { evalCancelRef.current = true; setEvaluating(false) }

  // ── Render helpers ────────────────────────────────────────────────────────
  let safefen = START_FEN
  try { new Chess().load(fen); safefen = fen } catch {}

  const hasPgn = pgnFens.length > 0
  const currentMoveEval = hasPgn && plyIndex > 0 ? moveEvals[plyIndex] : null

  return (
    <div className={styles.layout}>
      <div className={styles.left}>

        {/* ── Mode tabs ── */}
        <div className={styles.tabs}>
          <button className={mode === 'fen' ? styles.tabActive : styles.tab}
            onClick={() => { setMode('fen'); setError('') }}>FEN</button>
          <button className={mode === 'pgn' ? styles.tabActive : styles.tab}
            onClick={() => { setMode('pgn'); setError('') }}>PGN</button>
        </div>

        {/* ── FEN input ── */}
        {mode === 'fen' && (
          <div className={styles.fenBar}>
            <input className={styles.fenInput} value={fenInput}
              onChange={e => setFenInput(e.target.value)}
              placeholder="Paste FEN…" spellCheck={false} />
            <button className={styles.btn} onClick={loadFen}>Load</button>
            <button className={styles.btnSecondary} onClick={loadStartpos}>Startpos</button>
          </div>
        )}

        {/* ── PGN input ── */}
        {mode === 'pgn' && (
          <div className={styles.pgnInputWrap}>
            <textarea className={styles.pgnTextarea} value={pgnInput}
              onChange={e => setPgnInput(e.target.value)}
              placeholder="Paste PGN here…" rows={5} spellCheck={false} />
            <button className={styles.btn} onClick={loadPgn}>Load PGN</button>
          </div>
        )}

        {error && <span className={styles.error}>{error}</span>}

        {/* ── Board row: EvalBar + board + nav ── */}
        <div className={styles.boardRow}>
          <EvalBar score={evalScore} orientation="white" />
          <div>
            <div className={styles.boardWrap}>
              <Chessboard
                id="analysis-board"
                position={safefen}
                arePiecesDraggable={false}
                customBoardStyle={{ borderRadius: '4px', boxShadow: '0 4px 24px #0008' }}
              />
            </div>

            {/* PGN nav controls */}
            {hasPgn && (
              <div className={styles.navRow}>
                <button className={styles.navBtn} onClick={() => goToPly(0)} title="Start">⏮</button>
                <button className={styles.navBtn} onClick={() => goToPly(plyIndex - 1)} title="Previous">◀</button>
                <span className={styles.plyLabel}>
                  {plyIndex === 0 ? 'Start' : `Move ${Math.ceil(plyIndex / 2)}${plyIndex % 2 === 1 ? ' (W)' : ' (B)'}`}
                  &nbsp;/&nbsp;{pgnFens.length - 1} ply
                </span>
                <button className={styles.navBtn} onClick={() => goToPly(plyIndex + 1)} title="Next">▶</button>
                <button className={styles.navBtn} onClick={() => goToPly(pgnFens.length - 1)} title="End">⏭</button>
              </div>
            )}

            {/* Move quality info bar */}
            {currentMoveEval && (
              <div className={styles.moveQualityBar}>
                <span className={styles.qualityPill} data-q={currentMoveEval.quality}>
                  {QUALITY_ICON[currentMoveEval.quality]}&nbsp;
                  <strong>{pgnMoves[plyIndex - 1]}</strong> — {currentMoveEval.quality}
                  {currentMoveEval.gain < 0 && ` (${Math.round(currentMoveEval.gain)}cp)`}
                </span>
                {currentMoveEval.bestmove && (
                  <span className={styles.bestWas}>
                    Best: <strong>{currentMoveEval.bestmove}</strong>
                  </span>
                )}
              </div>
            )}
          </div>
        </div>

        {/* ── Controls ── */}
        <div className={styles.controls}>
          <label className={styles.label}>
            Depth&nbsp;
            <input type="number" min={1} max={MAX_DEPTH} value={depth}
              onChange={e => setDepth(Number(e.target.value))}
              className={styles.numInput} />
          </label>

          {/* Manual analyse (hidden when autoMode active) */}
          {!autoMode && (
            <button className={running ? styles.btnStop : styles.btn}
              onClick={startAnalysis} disabled={evaluating}>
              {running ? 'Stop' : 'Analyse'}
            </button>
          )}

          {/* Auto-analyse toggle */}
          <button
            className={autoMode ? styles.btnAutoOn : styles.btnSecondary}
            onClick={() => {
              setAutoMode(v => {
                if (v) { cleanupRef.current?.(); setRunning(false) }
                return !v
              })
            }}
          >
            {autoMode ? '● Auto On' : '○ Auto'}
          </button>

          {/* Evaluate whole game */}
          {hasPgn && !evaluating && (
            <button className={styles.btnSecondary} onClick={evaluateGame}
              disabled={running && !autoMode}>
              Evaluate Game
            </button>
          )}
          {evaluating && (
            <span className={styles.evalProgress}>
              Evaluating {evalProgress}/{pgnFens.length}…
              <button className={styles.btnMini} onClick={stopEvaluating}>✕</button>
            </span>
          )}

          {bestmove && !autoMode && (
            <span className={styles.bestmove}>Best: <strong>{bestmove}</strong></span>
          )}
        </div>
      </div>

      {/* ── Right panel ── */}
      <div className={styles.right}>

        {/* Game Moves with quality badges */}
        {hasPgn && (
          <div className={styles.card}>
            <h3 className={styles.cardTitle}>Game Moves</h3>
            <div className={styles.pgnMoveList} ref={moveListRef}>
              {pgnMoves.map((san, i) => {
                const ply = i + 1
                const isWhite = ply % 2 === 1
                const mq = moveEvals[ply]
                return (
                  <span key={ply}>
                    {isWhite && <span className={styles.moveNum}>{Math.ceil(ply / 2)}.</span>}
                    <span
                      data-ply={ply}
                      className={plyIndex === ply ? styles.moveActive : styles.moveBtn}
                      onClick={() => goToPly(ply)}
                      title={mq ? `${mq.quality} (${Math.round(mq.gain)}cp) — best: ${mq.bestmove}` : undefined}
                    >
                      {san}
                      {mq && <sup className={styles.qualityDot}>{QUALITY_ICON[mq.quality]}</sup>}
                    </span>
                  </span>
                )
              })}
            </div>
          </div>
        )}

        {/* Eval graph */}
        <div className={styles.card}>
          <h3 className={styles.cardTitle}>Evaluation by Depth</h3>
          <ResponsiveContainer width="100%" height={200}>
            <LineChart data={evalHistory} margin={{ top: 8, right: 16, bottom: 0, left: 0 }}>
              <XAxis dataKey="depth" stroke="#8892a4" tick={{ fontSize: 11 }}
                label={{ value: 'depth', position: 'insideBottomRight', offset: -4, fontSize: 11, fill: '#8892a4' }} />
              <YAxis stroke="#8892a4" tick={{ fontSize: 11 }} domain={['auto', 'auto']} tickFormatter={v => v.toFixed(1)} />
              <Tooltip
                contentStyle={{ background: '#16213e', border: '1px solid #2a3a5c', fontSize: 12 }}
                formatter={(v) => [v.toFixed(2), 'score']}
              />
              <ReferenceLine y={0} stroke="#2a3a5c" />
              <Line type="monotone" dataKey="score" stroke="#e94560" dot={false} strokeWidth={2} isAnimationActive={false} />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {/* PV line */}
        <div className={styles.card}>
          <h3 className={styles.cardTitle}>Principal Variation</h3>
          <div className={styles.pv}>
            {pvLine.length > 0
              ? pvLine.map((m, i) => <span key={i} className={styles.pvMove}>{m}</span>)
              : <span className={styles.empty}>—</span>
            }
          </div>
        </div>
      </div>
    </div>
  )
}
