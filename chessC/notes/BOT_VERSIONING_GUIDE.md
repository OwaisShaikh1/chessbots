# ChessC Bot Versioning Guide

This guide explains how to create and manage bot versions so old binaries are never deleted and the backend can automatically discover bots from the bots folder.

## Goal

You want to:
- Build many bot iterations over time.
- Keep each version as a separate runnable binary.
- Keep old versions intact.
- Select any version in frontend Play/Arena.

## Current Versioning Model (Updated)

Primary bot storage:
- Active default bot:
  - chessC/bots/active/chessbot.exe
- Versioned bots:
  - chessC/bots/<version_name>/chessbot.exe

Legacy compatibility (still supported):
- chessC/engines/<version_name>/chessbot.exe

Backend discovery behavior:
- GET /engines returns default plus all valid bots under chessC/bots and legacy entries under chessC/engines.
- POST /game/start accepts optional engine_id.
- POST /engine/evaluate accepts optional engine_id.
- POST /bot-battle accepts optional engine_white_id and engine_black_id.

## 1. Build the latest engine

From chessC/engine:

```powershell
.\build.bat
```

This compiles and updates:
- chessC/engine/build/chessbot.exe

It does not create a version snapshot by itself.

## 2. Create a version snapshot (user-specified)

From chessC/engine:

```powershell
.\build.bat v007_eval_tuned
```

Result:
- chessC/bots/v007_eval_tuned/chessbot.exe

No old versions are deleted.

## 3. Auto naming status

Auto naming is intentionally disabled.

Commands below are blocked by design:

```powershell
.\build.bat auto
.\build.bat next
```

Reason:
- This prevents accidental overwrite risk from numbering logic.
- Every saved version must be explicit and intentional.

## 4. Recommended numbering conventions

Option A: explicit numeric versions
- v001, v002, v003

Option B: numeric + suffix
- v014_king_safety
- v015_pst_runtime

Naming rules:
- Use only letters, numbers, dot, underscore, dash.
- No spaces.

## 5. Promote a version to active default bot

The default engine is read from:
- chessC/bots/active/chessbot.exe

To promote a version:

```powershell
Copy-Item .\bots\v015_pst_runtime\chessbot.exe .\bots\active\chessbot.exe -Force
```

Then restart backend to ensure all sessions pick the new default.

## 6. Verify backend sees all bots

Start backend and call:
- GET http://127.0.0.1:8000/engines

You should see:
- `default`
- entries from `chessC/bots/*`
- legacy entries from `chessC/engines/*` (if present)

Additional behavior:
- If `chessC/bots/<folder>/` contains extra `.exe` files (for example archives), backend exposes them as `folder.filename` engine ids.

## 7. Use versions in frontend

Play page:
- Select engine version from dropdown.
- Start game.

Arena page:
- Choose white and black versions independently.
- Run benchmarks, mirrors, and self-play.

## 8. Safe workflow for future versions (no deletion)

1. Modify engine code.
2. Build and snapshot with explicit naming only:
  - `.\build.bat <version_name>`
3. Keep previous folders untouched.
4. Test in Arena against baseline.
5. Promote best version to `bots/active` when ready.
6. Never overwrite historical version folders.

Recommended safety step:
- Before saving a new version, run `GET /engines` and confirm your target name does not already exist.

## 9. Runtime PST tuning (no rebuild per value change)

PST values are loaded at runtime from:
- chessC/backend/pst_config.json

The backend passes this path to the engine automatically before search.

Meaning:
- You can tune PST values from the UI/backend config without rebuilding each tweak.
- Build a new bot version only when engine code/logic changes are finalized.

## 10. Running backend and frontend

Backend:
- folder: chessC/backend
- run: `uvicorn main:app --reload --host 127.0.0.1 --port 8000`

Frontend:
- folder: chessC/reactfrontend
- run: `npm run dev`

## 11. Troubleshooting

If a bot version is missing in dropdown:
- Confirm `chessbot.exe` exists in `chessC/bots/<version_name>/`.
- Confirm backend is running and GET /engines lists it.
- Restart backend after adding new folders.

If using a nested executable id (example `archive.chessbot_20260329_220637`):
- Confirm that `.exe` exists under `chessC/bots/archive/`.

If default bot does not change:
- Verify file exists at `chessC/bots/active/chessbot.exe`.
- Restart backend.

If build fails:
- Re-run from `chessC/engine`.
- Ensure CMake + compiler toolchain are in PATH.

---

Owner note:
Update this guide whenever build scripts, runtime config behavior, or backend engine discovery changes.
