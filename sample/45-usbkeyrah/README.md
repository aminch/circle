# USB Keyboard + Gamepad Test Harness

Combined USB keyboard and gamepad test harness. Binds every `ukbd1`..`ukbd4`
and `upad1`..`upad4` device that USB plug-and-play reports. Keyboards are read
in raw mode (no ASCII translation, no LED handling).

## Output

One line is printed per event, not per USB poll:

```
ukbd1 connected
ukbd1 disconnected (128 reports total)
ukbd1: modifiers 0x02 keys 1A 16
upad1 connected: 4 axes, 1 hat(s), 3 button(s)
upad1 disconnected (954 reports total)
upad1: buttons 0x1 axes 128 255 0 0 hats 3
Now connected: ukbd1 ukbd2 upad1 upad2
```

- `connected` / `disconnected` — a keyboard or gamepad binds or is removed.
  `disconnected` includes the number of reports it delivered while connected.
- `ukbdN: modifiers ... keys ...` — printed only when the key state changes.
- `upadN: buttons ... axes ... hats ...` — printed only when a control changes.
- `Now connected: ...` — printed whenever the set of bound devices changes.

A gamepad reporting 0 axes/hats/buttons, or with a null `GetInitialState()`, is
flagged as a non-joystick HID interface that got bound as a gamepad.

## Heartbeat

Every report is counted per device. Every `HEARTBEAT_LOOPS` loops (default
5 s) a summary is printed:

```
alive 42s, connected: ukbd1 upad1
  ukbd1: 6 reports, last 3.2s ago
  upad1: 954 reports, last 0.1s ago
```

A gamepad reports on every poll, so its age should stay near `0s`. A keyboard
only reports on change, so a growing age there is normal.

If a gamepad that was active goes quiet for `SILENCE_WARN_LOOPS` loops
(default 3 s), one warning is printed:

```
upad2: no reports for 3000ms (was active, 954 total) - endpoint stalled?
```

USB-relevant kernel options (`usbspeed=`, `usbboost=`, `usbignore=`) are
logged once at startup.

## Diagnosing a stall

Use the serial console (GPIO14/15, 115200 8N1) alongside the screen: if the
report counters freeze while the heartbeat keeps ticking, the USB stack has
stopped servicing interrupt endpoints; if the heartbeat itself stops, the
system is wedged.

On the Raspberry Pi 1-3 and Zero, `USE_USB_SOF_INTR` must be defined in
`include/circle/sysconfig.h` (on by default). For flaky full-speed devices
behind the internal hub, also try `USE_USB_FIQ` in the same file.

## Build

```
make
```
