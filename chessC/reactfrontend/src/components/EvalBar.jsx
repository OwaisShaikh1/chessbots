import styles from './EvalBar.module.css'

/**
 * Vertical evaluation bar.
 * score: centipawns from white's perspective (positive = white better)
 * orientation: 'white' | 'black'
 */
export default function EvalBar({ score = 0, orientation = 'white' }) {
  // Clamp to ±15 pawns for display purposes
  const clamped = Math.max(-15, Math.min(15, score))
  // White % of bar height (score=0 → 50%)
  const whitePct = ((clamped + 15) / 30) * 100

  const display = Math.abs(score) >= 100
    ? `M${Math.abs(Math.round(score / 100))}`       // show "M3" for mate
    : (score >= 0 ? '+' : '') + score.toFixed(1)

  // If board is flipped, white bar is at the top
  const topPct    = orientation === 'white' ? 100 - whitePct : whitePct
  const bottomPct = 100 - topPct

  return (
    <div className={styles.bar}>
      <div className={styles.black} style={{ height: `${topPct}%` }} />
      <div className={styles.white} style={{ height: `${bottomPct}%` }} />
      <span className={styles.label}>{display}</span>
    </div>
  )
}
