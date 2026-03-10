import { useRef, useEffect } from 'react'
import styles from './MoveList.module.css'

/**
 * history: string[] from chess.js game.history() — SAN moves
 */
export default function MoveList({ history = [], currentPly = null }) {
  const endRef = useRef(null)

  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'smooth' })
  }, [history.length])

  // Pair moves into [white, black?]
  const pairs = []
  for (let i = 0; i < history.length; i += 2) {
    pairs.push({ move: i / 2 + 1, white: history[i], black: history[i + 1] })
  }

  return (
    <div className={styles.card}>
      <h3 className={styles.title}>Moves</h3>
      <div className={styles.list}>
        {pairs.length === 0
          ? <span className={styles.empty}>No moves yet.</span>
          : pairs.map(({ move, white, black }) => (
            <div key={move} className={styles.pair}>
              <span className={styles.num}>{move}.</span>
              <span className={styles.move}>{white}</span>
              <span className={styles.move}>{black ?? ''}</span>
            </div>
          ))
        }
        <div ref={endRef} />
      </div>
    </div>
  )
}
