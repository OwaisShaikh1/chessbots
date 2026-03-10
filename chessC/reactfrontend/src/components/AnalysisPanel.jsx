import styles from './AnalysisPanel.module.css'

/**
 * Shows current search depth, evaluation and principal variation.
 */
export default function AnalysisPanel({ depth = 0, pv = [], score = 0 }) {
  const scoreStr = score >= 0 ? `+${score.toFixed(2)}` : score.toFixed(2)

  return (
    <div className={styles.card}>
      <h3 className={styles.title}>Engine Analysis</h3>
      <div className={styles.row}>
        <span className={styles.label}>Depth</span>
        <span className={styles.value}>{depth}</span>
      </div>
      <div className={styles.row}>
        <span className={styles.label}>Score</span>
        <span className={styles.value} style={{ color: score > 0 ? 'var(--positive)' : score < 0 ? 'var(--negative)' : 'var(--text)' }}>
          {scoreStr}
        </span>
      </div>
      <div className={styles.pvSection}>
        <span className={styles.label}>PV</span>
        <div className={styles.pvLine}>
          {pv.length > 0 ? pv.join(' ') : '—'}
        </div>
      </div>
    </div>
  )
}
