import { useMemo, useRef, useState } from 'react'
import { getPstConfig, savePstConfig } from '../api'
import styles from './PstEditorPage.module.css'

const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']

const TABLES = [
  { key: 'p', label: 'Pawn', symbol: 'P' },
  { key: 'n', label: 'Knight', symbol: 'N' },
  { key: 'b', label: 'Bishop', symbol: 'B' },
  { key: 'r', label: 'Rook', symbol: 'R' },
  { key: 'q', label: 'Queen', symbol: 'Q' },
  { key: 'k_open', label: 'King Opening', symbol: 'K-O' },
  { key: 'k_end', label: 'King Endgame', symbol: 'K-E' }
]

const PIECE_SYMBOLS = {
  p: '♙',
  n: '♘',
  b: '♗',
  r: '♖',
  q: '♕',
  k_open: '♔',
  k_end: '♔'
}

function squareToIndex(file, rank) {
  return (rank - 1) * 8 + file
}

function indexToSquare(index) {
  const file = index % 8
  const rank = Math.floor(index / 8) + 1
  return `${FILES[file]}${rank}`
}

function createPawnPromotionTable() {
  const rankWeight = [0, 8, 14, 24, 40, 62, 90, 0]
  const table = []
  for (let rank = 1; rank <= 8; rank += 1) {
    for (let file = 0; file < 8; file += 1) {
      table.push(rankWeight[rank - 1])
    }
  }
  return table
}

function createDefaultTables() {
  return {
    p: createPawnPromotionTable(),
    n: [
      -50, -40, -30, -30, -30, -30, -40, -50,
      -40, -20, 0, 0, 0, 0, -20, -40,
      -30, 0, 10, 15, 15, 10, 0, -30,
      -30, 5, 15, 20, 20, 15, 5, -30,
      -30, 0, 15, 20, 20, 15, 0, -30,
      -30, 5, 10, 15, 15, 10, 5, -30,
      -40, -20, 0, 5, 5, 0, -20, -40,
      -50, -40, -30, -30, -30, -30, -40, -50
    ],
    b: [
      -20, -10, -10, -10, -10, -10, -10, -20,
      -10, 0, 0, 0, 0, 0, 0, -10,
      -10, 0, 5, 10, 10, 5, 0, -10,
      -10, 5, 5, 10, 10, 5, 5, -10,
      -10, 0, 10, 10, 10, 10, 0, -10,
      -10, 10, 10, 10, 10, 10, 10, -10,
      -10, 5, 0, 0, 0, 0, 5, -10,
      -20, -10, -10, -10, -10, -10, -10, -20
    ],
    r: [
      0, 0, 0, 0, 0, 0, 0, 0,
      5, 10, 10, 10, 10, 10, 10, 5,
      -5, 0, 0, 0, 0, 0, 0, -5,
      -5, 0, 0, 0, 0, 0, 0, -5,
      -5, 0, 0, 0, 0, 0, 0, -5,
      -5, 0, 0, 0, 0, 0, 0, -5,
      -5, 0, 0, 0, 0, 0, 0, -5,
      0, 0, 0, 5, 5, 0, 0, 0
    ],
    q: [
      -20, -10, -10, -5, -5, -10, -10, -20,
      -10, 0, 0, 0, 0, 0, 0, -10,
      -10, 0, 5, 5, 5, 5, 0, -10,
      -5, 0, 5, 5, 5, 5, 0, -5,
      0, 0, 5, 5, 5, 5, 0, -5,
      -10, 5, 5, 5, 5, 5, 0, -10,
      -10, 0, 5, 0, 0, 0, 0, -10,
      -20, -10, -10, -5, -5, -10, -10, -20
    ],
    k_open: [
      -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -30, -40, -40, -50, -50, -40, -40, -30,
      -20, -30, -30, -40, -40, -30, -30, -20,
      -10, -20, -20, -20, -20, -20, -20, -10,
      20, 20, 0, 0, 0, 0, 20, 20,
      20, 30, 10, 0, 0, 10, 30, 20
    ],
    k_end: [
      -50, -40, -30, -20, -20, -30, -40, -50,
      -30, -20, -10, 0, 0, -10, -20, -30,
      -30, -10, 20, 30, 30, 20, -10, -30,
      -30, -10, 30, 40, 40, 30, -10, -30,
      -30, -10, 30, 40, 40, 30, -10, -30,
      -30, -10, 20, 30, 30, 20, -10, -30,
      -30, -30, 0, 0, 0, 0, -30, -30,
      -50, -30, -30, -30, -30, -30, -30, -50
    ]
  }
}

function clampTable(arr) {
  const output = Array.isArray(arr) ? arr.slice(0, 64) : []
  while (output.length < 64) output.push(0)
  return output.map((v) => {
    const num = Number(v)
    if (!Number.isFinite(num)) return 0
    return Math.round(num)
  })
}

function sanitizeConfig(config) {
  const fallback = createDefaultTables()
  const clean = {}
  for (const table of TABLES) {
    clean[table.key] = clampTable(config?.[table.key] ?? fallback[table.key])
  }
  return clean
}

function serializeAsCpp(tables) {
  return [
    `static const int PAWN_PST[64] = { ${tables.p.join(', ')} };`,
    `static const int KNIGHT_PST[64] = { ${tables.n.join(', ')} };`,
    `static const int BISHOP_PST[64] = { ${tables.b.join(', ')} };`,
    `static const int ROOK_PST[64] = { ${tables.r.join(', ')} };`,
    `static const int QUEEN_PST[64] = { ${tables.q.join(', ')} };`,
    `static const int KING_OPEN_PST[64] = { ${tables.k_open.join(', ')} };`,
    `static const int KING_END_PST[64] = { ${tables.k_end.join(', ')} };`
  ].join('\n\n')
}

function downloadTextFile(name, content) {
  const blob = new Blob([content], { type: 'text/plain;charset=utf-8' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = name
  document.body.appendChild(a)
  a.click()
  document.body.removeChild(a)
  URL.revokeObjectURL(url)
}

export default function PstEditorPage() {
  const [activeTable, setActiveTable] = useState('p')
  const [tables, setTables] = useState(() => sanitizeConfig(createDefaultTables()))
  const [selectedIndex, setSelectedIndex] = useState(squareToIndex(4, 2))
  const [blackView, setBlackView] = useState(false)
  const [status, setStatus] = useState('')
  const [busy, setBusy] = useState(false)
  const fileInputRef = useRef(null)

  const exportJson = useMemo(() => JSON.stringify(tables, null, 2), [tables])
  const exportCpp = useMemo(() => serializeAsCpp(tables), [tables])

  const squareName = useMemo(() => indexToSquare(selectedIndex), [selectedIndex])
  const currentValue = tables[activeTable][selectedIndex]

  const topFiles = blackView ? [...FILES].reverse() : FILES
  const ranksByRow = blackView
    ? [1, 2, 3, 4, 5, 6, 7, 8]
    : [8, 7, 6, 5, 4, 3, 2, 1]

  function updateTableValue(tableKey, idx, value) {
    const parsed = Number(value)
    const nextVal = Number.isFinite(parsed) ? Math.round(parsed) : 0
    setTables((prev) => ({
      ...prev,
      [tableKey]: prev[tableKey].map((v, i) => (i === idx ? nextVal : v))
    }))
  }

  function nudgeSelected(delta) {
    updateTableValue(activeTable, selectedIndex, currentValue + delta)
  }

  function applyPawnPromotionBias() {
    setTables((prev) => ({ ...prev, p: createPawnPromotionTable() }))
    setStatus('Applied a stronger pawn progression curve toward promotion.')
  }

  function resetAll() {
    setTables(sanitizeConfig(createDefaultTables()))
    setStatus('Reset all piece tables to defaults.')
  }

  async function loadFromBackend() {
    setBusy(true)
    try {
      const response = await getPstConfig()
      setTables(sanitizeConfig(response.config))
      setStatus('Loaded PST config from backend.')
    } catch (err) {
      setStatus(`Failed to load from backend: ${String(err.message || err)}`)
    } finally {
      setBusy(false)
    }
  }

  async function saveToBackend() {
    setBusy(true)
    try {
      await savePstConfig(tables)
      setStatus('Saved PST config to backend.')
    } catch (err) {
      setStatus(`Failed to save to backend: ${String(err.message || err)}`)
    } finally {
      setBusy(false)
    }
  }

  function importConfigText(text) {
    try {
      const parsed = JSON.parse(text)
      setTables(sanitizeConfig(parsed))
      setStatus('Imported PST values from local JSON.')
    } catch {
      setStatus('Import failed. JSON must include p, n, b, r, q, k_open, k_end.')
    }
  }

  function openFilePicker() {
    fileInputRef.current?.click()
  }

  function onJsonFilePicked(event) {
    const file = event.target.files?.[0]
    if (!file) return
    const reader = new FileReader()
    reader.onload = () => importConfigText(String(reader.result || ''))
    reader.readAsText(file)
    event.target.value = ''
  }

  return (
    <div className={styles.page}>
      <header className={styles.header}>
        <h1>Piece-Square Table Editor</h1>
        <p>
          Edit PST values directly on a chessboard. Indexing is engine-friendly: A1 is index 0.
          Use separate king tables for opening and endgame.
        </p>
      </header>

      <section className={styles.controls}>
        <div className={styles.tabs}>
          {TABLES.map((table) => (
            <button
              key={table.key}
              type="button"
              className={activeTable === table.key ? styles.tabActive : styles.tab}
              onClick={() => setActiveTable(table.key)}
            >
              {table.symbol} {table.label}
            </button>
          ))}
        </div>

        <div className={styles.actions}>
          <button type="button" className={styles.btn} onClick={applyPawnPromotionBias}>Apply Pawn Promotion Bias</button>
          <button type="button" className={styles.btnSecondary} onClick={() => setBlackView((v) => !v)}>
            View: {blackView ? 'Black Perspective' : 'White Perspective'}
          </button>
          <button type="button" className={styles.btnSecondary} onClick={loadFromBackend} disabled={busy}>Load Backend</button>
          <button type="button" className={styles.btnSecondary} onClick={saveToBackend} disabled={busy}>Save Backend</button>
          <button type="button" className={styles.btnSecondary} onClick={() => downloadTextFile('pst_config.json', exportJson)}>Download JSON</button>
          <button type="button" className={styles.btnSecondary} onClick={() => downloadTextFile('pst_tables.hpp', exportCpp)}>Download C++</button>
          <button type="button" className={styles.btnSecondary} onClick={openFilePicker}>Load JSON File</button>
          <button type="button" className={styles.btnSecondary} onClick={resetAll}>Reset Defaults</button>
          <input
            ref={fileInputRef}
            type="file"
            accept="application/json,.json"
            onChange={onJsonFilePicked}
            style={{ display: 'none' }}
          />
        </div>

        {status && <p className={styles.status}>{status}</p>}
      </section>

      <section className={styles.mainGrid}>
        <article className={styles.boardWrap}>
          <div className={styles.boardHeader}>
            <h2>
              {PIECE_SYMBOLS[activeTable]} {TABLES.find((t) => t.key === activeTable)?.label}
            </h2>
            <p>Click a square to edit its value.</p>
          </div>

        <div className={styles.fileLabelsTop}>
          <span />
            {topFiles.map((file) => <span key={file}>{file}</span>)}
        </div>

        <div className={styles.board}>
            {ranksByRow.map((rank, rowDisplay) => (
              <div className={styles.rankRow} key={`${rank}-${rowDisplay}`}>
              <span className={styles.rankLabel}>{rank}</span>
                {topFiles.map((fileChar, colDisplay) => {
                  const file = FILES.indexOf(fileChar)
                  const idx = squareToIndex(file, rank)
                  const light = (rowDisplay + colDisplay) % 2 === 0
                  const selected = idx === selectedIndex
                return (
                    <button
                      key={`${fileChar}${rank}`}
                      type="button"
                      className={[
                        light ? styles.squareLight : styles.squareDark,
                        selected ? styles.squareSelected : ''
                      ].join(' ')}
                      onClick={() => setSelectedIndex(idx)}
                    >
                      <span className={styles.squareName}>{fileChar}{rank}</span>
                      <span className={styles.squareValue}>{tables[activeTable][idx]}</span>
                    </button>
                )
              })}
            </div>
          ))}
        </div>
        </article>

        <aside className={styles.editorPanel}>
          <h2>Square Editor</h2>
          <p className={styles.editorMeta}>
            Active square: <strong>{squareName}</strong>
          </p>
          <label className={styles.inputLabel}>
            Value (centipawns)
            <input
              type="number"
              value={currentValue}
              onChange={(e) => updateTableValue(activeTable, selectedIndex, e.target.value)}
            />
          </label>

          <div className={styles.nudgeRow}>
            <button type="button" className={styles.btnSecondary} onClick={() => nudgeSelected(-10)}>-10</button>
            <button type="button" className={styles.btnSecondary} onClick={() => nudgeSelected(-5)}>-5</button>
            <button type="button" className={styles.btnSecondary} onClick={() => nudgeSelected(5)}>+5</button>
            <button type="button" className={styles.btnSecondary} onClick={() => nudgeSelected(10)}>+10</button>
          </div>

          <p className={styles.helpText}>
            Positive values favor White piece placement. Black values are mirrored in engine evaluation.
          </p>
        </aside>
      </section>

      <section className={styles.exports}>
        <article className={styles.card}>
          <h2>JSON Config</h2>
          <textarea readOnly value={exportJson} />
        </article>

        <article className={styles.card}>
          <h2>Engine C++ Arrays</h2>
          <textarea readOnly value={exportCpp} />
        </article>
      </section>
    </div>
  )
}
