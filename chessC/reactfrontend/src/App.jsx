import { BrowserRouter, Routes, Route, NavLink } from 'react-router-dom'
import PlayPage   from './pages/PlayPage'
import AnalysisPage from './pages/AnalysisPage'
import ArenaPage  from './pages/ArenaPage'
import styles from './App.module.css'

export default function App() {
  return (
    <BrowserRouter>
      <div className={styles.shell}>
        <nav className={styles.nav}>
          <span className={styles.logo}>♟ ChessBot</span>
          <NavLink to="/"        className={({isActive}) => isActive ? styles.active : ''}>Play</NavLink>
          <NavLink to="/analysis" className={({isActive}) => isActive ? styles.active : ''}>Analysis</NavLink>
          <NavLink to="/arena"   className={({isActive}) => isActive ? styles.active : ''}>Arena</NavLink>
        </nav>
        <main className={styles.main}>
          <Routes>
            <Route path="/"         element={<PlayPage />} />
            <Route path="/analysis" element={<AnalysisPage />} />
            <Route path="/arena"    element={<ArenaPage />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  )
}
