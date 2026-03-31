import React, { useEffect, useState } from "react";
import "./LeaderboardBar.css";

// Simple bar for win/draw/loss breakdown per match row
function LeaderboardBar({ whiteWins, draws, blackWins, total }) {
  const winPct = (whiteWins / total) * 100;
  const drawPct = (draws / total) * 100;
  const lossPct = (blackWins / total) * 100;
  return (
    <div className="leaderboard-bar">
      <div
        className="bar win"
        style={{ width: `${winPct}%` }}
        title={`White wins: ${whiteWins}`}
      />
      <div
        className="bar draw"
        style={{ width: `${drawPct}%` }}
        title={`Draws: ${draws}`}
      />
      <div
        className="bar loss"
        style={{ width: `${lossPct}%` }}
        title={`Black wins: ${blackWins}`}
      />
    </div>
  );
}

export default function Leaderboard() {
  const [rows, setRows] = useState([]);
  useEffect(() => {
    fetch("/leaderboard")
      .then((r) => r.json())
      .then(setRows)
      .catch(() => setRows([]));
  }, []);

  return (
    <div className="leaderboard-table-container">
      <h2>Leaderboard History</h2>
      <div className="leaderboard-table">
        <div className="header-row">
          <span>Date</span>
          <span>White</span>
          <span>Black</span>
          <span>W/D/L</span>
          <span>Total</span>
        </div>
        {rows.map((row, i) => {
          const total =
            Number(row.white_wins) + Number(row.black_wins) + Number(row.draws);
          return (
            <div className="data-row" key={row.session_id || i}>
              <span>{row.timestamp?.slice(0, 19).replace("T", " ")}</span>
              <span>{row.white_engine}</span>
              <span>{row.black_engine}</span>
              <LeaderboardBar
                whiteWins={Number(row.white_wins)}
                draws={Number(row.draws)}
                blackWins={Number(row.black_wins)}
                total={total}
              />
              <span>{total}</span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
