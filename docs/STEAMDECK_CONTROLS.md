# Steam Deck controls (Linux joydev)

This fork wires **analog gamepad movement** into the existing `JoystickMove` path by reading **Linux joydev** (`/dev/input/js*`) when the game is built for **Linux** (non-dedicated).

## What works

- **Left stick** (default axis mapping): **strafe** (`AXIS_SIDE`) and **move** (`AXIS_FORWARD`).
- **Optional vertical axis** (`AXIS_UP`): bound to a joydev axis index (default **2**, often L2/R2 analog on Deck layouts).
- **Right stick look**: axes mapped to `AXIS_YAW` / `AXIS_PITCH`, scaled by **`in_joyLookScale`** and applied when **not** holding strafe (same phase as left-stick look).

Face buttons still arrive as **keyboard** events from Steam Input / the compositor unless you add `EV_KEY` joydev handling later.

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

## Tuning on Deck

1. If the left stick does nothing, run `jstest /dev/input/js0` (or `evtest`) and set **`in_joy_axis*`** to match your device’s axis order.  
2. If look is too fast or inverted, adjust **`in_joyLookScale`** or swap **`in_joy_axisLookPitch`** with another axis index.  
3. If you use a different node (e.g. `js1`), set **`in_joydev`**.

## Permissions

Reading `/dev/input/js0` may require **`udev`** rules or membership in the **`input`** group on some distros. SteamOS usually exposes the virtual gamepad without extra setup.

## Implementation

- `neo/sys/sys_joystick.cpp` — `Sys_FillJoystickAxes` / `Sys_ShutdownJoystickFill`
- `neo/framework/UsercmdGen.cpp` — calls `Sys_FillJoystickAxes` from `Joystick()`; `JoystickMove` applies `AXIS_YAW` / `AXIS_PITCH` when `in_joyLookScale > 0`
- `neo/sys/linux/input.cpp` — `Sys_ShutdownInput` closes the joydev fd
