import React, { useEffect, useState } from "react";
import "./LeaderboardBar.css";

// Simple bar for win/draw/loss breakdown per match row
function LeaderboardBar({ whiteWins, draws, blackWins, total, whiteName, blackName }) {
  const winPct = (whiteWins / total) * 100;
  const drawPct = (draws / total) * 100;
  const lossPct = (blackWins / total) * 100;
  return (
    <div className="arena-bar-outer">
      <div className="arena-bar-labels-row">
        <span className="arena-bar-name left">{whiteName}</span>
        <span className="arena-bar-vs">vs</span>
        <span className="arena-bar-name right">{blackName}</span>
      </div>
      <div className="arena-bar-stats-row">
        <span className="arena-bar-stat win">wins: {whiteWins}</span>
        <span className="arena-bar-stat draw">draws: {draws}</span>
        <span className="arena-bar-stat loss">losses: {blackWins}</span>
      </div>
      <div className="arena-bar">
        <div
          className="arena-bar-segment win"
          style={{ width: `${winPct}%`, minWidth: winPct > 0 ? 2 : 0 }}
          title={`White wins: ${whiteWins}`}
        />
        <div
          className="arena-bar-segment draw"
          style={{ width: `${drawPct}%`, minWidth: drawPct > 0 ? 2 : 0 }}
          title={`Draws: ${draws}`}
        />
        <div
          className="arena-bar-segment loss"
          style={{ width: `${lossPct}%`, minWidth: lossPct > 0 ? 2 : 0 }}
          title={`Black wins: ${blackWins}`}
        />
      </div>
    </div>
  );
}

export default function Leaderboard() {
  const [rows, setRows] = useState([]);
  useEffect(() => {
    console.log("Leaderboard useEffect running");
    fetch("/api/leaderboard")
      .then((r) => {
        console.log("Fetch response", r);
        return r.json();
      })
      .then((data) => {
        console.log("Fetched leaderboard rows:", data);
        setRows(data);
      })
      .catch((err) => {
        console.log("Fetch error", err);
        setRows([]);
      });
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
        {rows.length === 0 && (
          <div style={{ color: '#888', padding: '2rem', textAlign: 'center' }}>
            No leaderboard data found.
          </div>
        )}
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
                whiteName={row.white_engine}
                blackName={row.black_engine}
              />
              <span>{total}</span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
