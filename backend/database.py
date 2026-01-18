"""
Database operations for training data storage
"""
import sqlite3


def init_db():
    """Initialize the training database"""
    conn = sqlite3.connect("training_data.db")
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS training_moves (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            game_id TEXT,
            start_fen TEXT,
            step INTEGER,
            turn TEXT,
            material_score INTEGER,
            result INTEGER,
            fen TEXT
        )
    ''')
    try:
        cursor.execute("ALTER TABLE training_moves ADD COLUMN opponent TEXT DEFAULT 'Self'")
    except:
        pass  # Column likely exists
    
    try:
        cursor.execute("ALTER TABLE training_moves ADD COLUMN stockfish_eval INTEGER DEFAULT NULL")
    except:
        pass  # Column likely exists
    
    conn.commit()
    conn.close()


def get_analytics_data():
    """Retrieve analytics data from the database"""
    try:
        conn = sqlite3.connect("training_data.db")
        cursor = conn.cursor()
        
        # Win rate history (last 5000 games for performance)
        cursor.execute('''
            SELECT result 
            FROM training_moves 
            WHERE turn = 'White' AND opponent = 'Self'
            GROUP BY game_id 
            ORDER BY id DESC
            LIMIT 5000
        ''')
        rows = cursor.fetchall()
        rows.reverse()  # Chronological order
        
        history = []
        window_size = 50
        for i in range(len(rows)):
            if i >= window_size:
                window = rows[i-window_size:i]
                wins = sum(1 for r in window if r[0] == 1)
                draws = sum(1 for r in window if r[0] == 0)
                win_rate = wins / window_size
                draw_rate = draws / window_size
                history.append({"game": i, "win_rate": win_rate, "draw_rate": draw_rate})
        
        # Recent games
        cursor.execute('''
            SELECT game_id, start_fen, result, opponent
            FROM training_moves
            WHERE turn = 'White'
            GROUP BY game_id
            ORDER BY id DESC
            LIMIT 10
        ''')
        recent_rows = cursor.fetchall()
        recent = []
        for row in recent_rows:
            result_str = "Win" if row[2] == 1 else ("Draw" if row[2] == 0 else "Loss")
            recent.append({
                "game_id": row[0],
                "start_fen": row[1],
                "result": result_str,
                "opponent": row[3]
            })
        
        conn.close()
        return {"history": history, "recent": recent}
    except Exception as e:
        print(f"Analytics error: {e}")
        return {"history": [], "recent": []}
