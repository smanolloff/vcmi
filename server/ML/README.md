# Battle checkpoints

## Headless v15 gym

```python
root = env.checkpoint()
obs, reward, terminated, truncated, info = env.step(candidate)
obs = env.restore(root)
# Try additional branches, then commit one action from the root:
env.finish_rollout(root)
obs, reward, terminated, truncated, info = env.step(selected)
```

A checkpoint includes the authoritative battle and combat RNG, the client mirror, both local controller histories and RNG, and the gym's episode counters. `restore()` returns a fresh observation and legal action IDs even after a speculative terminal step. `finish_rollout()` also re-enables normal battle finalization. Only restore or finish can continue from a speculative terminal state.

The connector posts work to the server thread while the client waits for input. On rewind it lets the old model callback return without sending a move, restores the client in place, and dispatches one decision without advancing the turn. It does not change the network protocol.

This requires one Python-controlled v15 MMAI and a local BattleAI, StupidAI, or v15 MMAI model opponent. Battle timers and spell-charge artifacts are not supported; headless ML replay recording is disabled. The snapshot restores RNG exactly and does not resample chance outcomes. Smoke test: `python -m vcmi_gym.tools.checkpoint_smoke_v15` (optionally `--model <VCMI resource path>`).

## Server-only API

`ServerPlugin` exposes an in-process API for a training worker that drives one live battle. Call it on the server thread, after `step` has finished all automatic actions and effects and the server is waiting for the next decision.

1. `auto root = plugin.checkpoint(battleID);` captures the current decision and enters rollout mode. Checkpoints are opaque and reusable within that rollout.
2. Run actions through the normal server battle processor. If `plugin.rolloutResult(battleID)` is set, return its outcome to the trainer and stop stepping. A speculative terminal action does not emit the normal battle result packs, run ML end hooks, or apply adventure-map consequences.
3. `plugin.restore(*root);` rewinds the battle, combat RNG, and terminal result. Recompute the observation and action mask from the restored state. Reacquire stack pointers by ID after every restore.
4. `plugin.finishRollout(*root);` rewinds once more and leaves rollout mode. Submit the selected action normally. If it ends the battle, normal finalization runs.

The server-only API does not synchronize clients. Use an isolated worker with battle timers and replay recording disabled. A speculative surrender is validated normally but does not charge adventure-map gold. `env.connector.capture_server_checkpoint()` remains available for a read-only server snapshot without entering rollout mode.
