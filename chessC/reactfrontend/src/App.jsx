import { BrowserRouter, Routes, Route, NavLink } from 'react-router-dom'
import PlayPage   from './pages/PlayPage'
import AnalysisPage from './pages/AnalysisPage'
import ArenaPage  from './pages/ArenaPage'
import PstEditorPage from './pages/PstEditorPage'
import LeaderboardPage from './pages/LeaderboardPage'
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
          <NavLink to="/pst"   className={({isActive}) => isActive ? styles.active : ''}>PST</NavLink>
          <NavLink to="/leaderboard" className={({isActive}) => isActive ? styles.active : ''}>Leaderboard</NavLink>
        </nav>
        <main className={styles.main}>
          <Routes>
            <Route path="/"         element={<PlayPage />} />
            <Route path="/analysis" element={<AnalysisPage />} />
            <Route path="/arena"    element={<ArenaPage />} />
            <Route path="/pst"      element={<PstEditorPage />} />
            <Route path="/leaderboard" element={<LeaderboardPage />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  )
}
