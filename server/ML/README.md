# Battle checkpoints

## Headless v15 gym

```python
root = env.checkpoint()
obs, reward, terminated, truncated, info = env.step(candidate)
obs = env.restore(root)
# Try additional branches, then commit one action from the root:
env.end_search(root)
obs, reward, terminated, truncated, info = env.step(selected)
```

`checkpoint()` starts a search session if none is active. A checkpoint includes the authoritative battle and combat RNG, the client mirror, both local controller histories and RNG, and the gym's episode counters. Use `restore()` between trial branches to rewind and get a fresh observation and legal action IDs, even after a speculative terminal step.

`end_search(root)` rewinds to the root and re-enables normal battle finalization without playing an action. The subsequent `step(selected)` commits the action. `reset()` discards any active search session before starting a fresh battle.

The connector posts work to the server thread while the client waits for input. On rewind it lets the old model callback return without sending a move, restores the client in place, and dispatches one decision without advancing the turn. It does not change the network protocol.

This requires one Python-controlled v15 MMAI and a local BattleAI, StupidAI, or v15 MMAI model opponent. Battle timers and spell-charge artifacts are not supported; headless ML replay recording is disabled. The snapshot restores RNG exactly and does not resample chance outcomes. Smoke test: `python -m vcmi_gym.tools.checkpoint_smoke_v15` (optionally `--model <VCMI resource path>`).

## Both players in Python

Use one `BattleSession` per battle instead of two `VcmiEnv` instances:

```python
from vcmi_gym.envs.v15 import BattleSession

players = {0: player0, 1: player1}
battle = BattleSession(controllers=players)
decision = battle.reset()
root = battle.checkpoint()

action = players[decision.player].act(decision.observation)
next_decision = battle.step(decision.id, action)
# Try a different branch, or end the search before committing:
decision = battle.restore(root)
# ...evaluate more trial branches...
decision = battle.end_search(root)
next_decision = battle.step(decision.id, selected_action)
```

`step()` returns the next actual player, who may be the same player, the other player, or `None` at terminal. `player` is the controller's player ID (0 or 1), not the battlefield side; those can differ when sides are swapped. `perspective` identifies the player whose observation is returned, including at terminal. Always use the returned decision ID when submitting an action. Restore issues a new ID, so actions from discarded decisions are rejected even if their action IDs are still legal.

Register stateful external controllers with `checkpoint()` and `restore(snapshot)` methods. Their checkpoint must capture policy memory and RNG as a reusable snapshot. `BattleSession` restores both controllers and VCMI before exposing the next decision. Stateless policies can be omitted. The coordinator owns policy invocation and episode resets: finish or cancel pending policy calls before rewinding, and initialize any new-episode policy state after `reset()`. `reset()` discards an active search and restores the external controllers to its root before starting the next battle.

Use `print(battle.render())` to display the current decision as ANSI text, including after restore or at terminal. Rendering does not consume the decision ID.

Smoke test: `python -m vcmi_gym.tools.battle_session_smoke_v15`.

## Server-only API

`ServerPlugin` exposes an in-process API for a training worker that drives one live battle. Call it on the server thread, after `step` has finished all automatic actions and effects and the server is waiting for the next decision.

1. `auto root = plugin.checkpoint(battleID);` captures the current decision and starts a search session. Checkpoints are opaque and reusable within that session.
2. Run actions through the normal server battle processor. If `plugin.rolloutResult(battleID)` is set, return its outcome to the trainer and stop stepping. A speculative terminal action does not emit the normal battle result packs, run ML end hooks, or apply adventure-map consequences.
3. `plugin.restore(*root);` rewinds the battle, combat RNG, and terminal result. Recompute the observation and action mask from the restored state. Reacquire stack pointers by ID after every restore.
4. `plugin.finishRollout(*root);` rewinds once more and ends the search session. Submit the selected action normally. If it ends the battle, normal finalization runs.

The server-only API does not synchronize clients. Use an isolated worker with battle timers and replay recording disabled. A speculative surrender is validated normally but does not charge adventure-map gold. `env.connector.capture_server_checkpoint()` remains available for a read-only server snapshot without starting a search session.
