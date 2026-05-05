# Steam Deck controls (Linux joydev)

This fork wires **analog gamepad movement** into the existing `JoystickMove` path by reading **Linux joydev** (`/dev/input/js*`) when the game is built for **Linux** (non-dedicated).

## What works

- **Left stick** (default axis mapping): **strafe** (`AXIS_SIDE`) and **move** (`AXIS_FORWARD`).
- **Optional vertical axis** (`AXIS_UP`): bound to a joydev axis index (default **2**, often L2/R2 analog on Deck layouts).
- **Right stick look**: axes mapped to `AXIS_YAW` / `AXIS_PITCH`, scaled by **`in_joyLookScale`** and applied when **not** holding strafe (same phase as left-stick look).

### Face buttons (joydev)

When **`in_joy_buttons` is 1** (default), joydev **button** events are queued as **`SE_KEY`** with **`K_JOY1`** … **`K_JOY32`** (`joydev` button index `n` maps to `K_JOY1 + n - in_joy_buttonBase`). Bind them in the game controls UI or `DoomConfig.cfg`, e.g.:

```
bind "JOY1" "_attack"
bind "JOY2" "_use"
bind "JOY3" "_reload"
bind "JOY4" "_impulse14"
```

If Steam Input already maps the pad to **keyboard** keys, set **`in_joy_buttons 0`** to avoid duplicate inputs.

### Default bind profile (in repo)

This repository ships **[base/steamdeck.cfg](base/steamdeck.cfg)** with starter `bind "JOYn"` lines (attack, PDA, reload, zoom, next/prev weapon, sprint, crouch). Copy it next to your game **`base/`** folder if missing, then:

```text
exec steamdeck.cfg
```

Or set **`in_joy_autoexec_profile 1`** (archive) so the engine **queues `exec steamdeck.cfg` once** when joydev first opens.

## CVars

| CVar | Default | Meaning |
|------|---------|---------|
| `in_joydev` | `/dev/input/js0` | Device path; **empty string disables** gamepad sampling. |
| `in_joy_axisSide` | `0` | joydev axis index → move strafe |
| `in_joy_axisForward` | `1` | joydev axis index → move forward/back |
| `in_joy_axisUp` | `2` | joydev axis index → jump/crouch axis |
| `in_joy_axisLookYaw` | `3` | right stick yaw |
| `in_joy_axisLookPitch` | `4` | right stick pitch |
| `in_joy_deadzone` | `20` | percent deadzone (0–90) on raw axis values |
| `in_joyLookScale` | `0.35` | scales right-stick contribution to look |
| `in_joy_buttons` | `1` | queue joydev buttons as `K_JOY*` keys |
| `in_joy_buttonBase` | `0` | subtract from joydev button number before mapping to `K_JOY1` |
| `in_joy_autoexec_profile` | `0` | if `1`, run `exec steamdeck.cfg` once when joydev opens |

## Tuning on Deck

1. In the game console, run **`in_joy_info`** to print the joydev **name**, **axis count**, and **button count** for the current **`in_joydev`** path (Linux, non-dedicated).
2. If the left stick does nothing, run `jstest /dev/input/js0` (or `evtest`) and set **`in_joy_axis*`** to match your device’s axis order.  
3. If look is too fast or inverted, adjust **`in_joyLookScale`** or swap **`in_joy_axisLookPitch`** with another axis index.  
4. If you use a different node (e.g. `js1`), set **`in_joydev`**.

## Permissions

Reading `/dev/input/js0` may require **`udev`** rules or membership in the **`input`** group on some distros. SteamOS usually exposes the virtual gamepad without extra setup.

## Implementation

- `neo/sys/sys_joystick.cpp` — `Sys_FillJoystickAxes` / `Sys_ShutdownJoystickFill`
- `neo/framework/UsercmdGen.cpp` — calls `Sys_FillJoystickAxes` from `Joystick()`; `JoystickMove` applies `AXIS_YAW` / `AXIS_PITCH` when `in_joyLookScale > 0`
- `neo/sys/linux/input.cpp` — `Sys_ShutdownInput` closes the joydev fd
