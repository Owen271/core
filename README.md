# InputActions Core
Core library containing shared logic.

This project used to be a part of the [taj-ny/InputActions](https://github.com/taj-ny/InputActions) repository. The latest commit before the split is [1055fabc31d77d59a8f1e1910197c44934078bc4](https://github.com/taj-ny/InputActions/tree/1055fabc31d77d59a8f1e1910197c44934078bc4).

The [taj-ny/InputActions](https://github.com/taj-ny/InputActions) repository serves as a global issue tracker for all subprojects and a place for discussion.

## Swipe locking and directional keys

Set `lock_direction: true` on a swipe trigger to retain its initial action as the
swipe changes direction. The lock resets when the gesture ends or is cancelled.
The default is `false`, preserving gesture chaining.

The `directional_keys` update action converts two-axis movement into shortcuts:

- `distance`: positive, finite movement per repeated shortcut; defaults to `75`.
- `keys`: lists of keys for `left`, `right`, `up`, and `down`.
- `initial`: optional bindings sent on the first recognized movement instead of
  waiting for `distance`. This movement does not count toward a repeat.
- `repeat_conditions`: optional condition evaluated before sending navigation
  keys. When false, movement is discarded. If already true on the first update,
  navigation continues without sending an initial binding.

Initial bindings use the recognized average direction and run at most once per
gesture. Keys are pressed in list order and released in reverse order.

# Acknowledgements
- [Strokognition](https://invent.kde.org/jpetso/strokognition), [wstroke](https://github.com/dkondor/wstroke), [easystroke](https://github.com/thjaeger/easystroke) - Stroke triggers
- [libinput](https://gitlab.freedesktop.org/libinput/libinput) - Input handling, touchscreen trigger recognition
- [circular-scrolling-improved](https://github.com/galundin/circular-scrolling-improved) - Circle triggers
- [KWin](https://invent.kde.org/plasma/kwin) - Trigger handling code (heavily extended and modified)
