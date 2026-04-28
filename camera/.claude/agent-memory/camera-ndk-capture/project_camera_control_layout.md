---
name: camerandk control state pattern
description: Where AF/AE/EV control state lives on CameraController and how it survives request rebuilds
type: project
---

CameraController applies AF mode, AF/AE regions, and AE compensation through an `applyControlState()` helper that runs after every `applyQualityToRequest()` call. The request_ object is rebuilt on mode switch, lens switch, and quality switch — so any control written directly to the request without going through `applyControlState()` is silently dropped.

**Why:** ACaptureRequest is freed and recreated in `openDevice`, `setMode`, `reopenSession`, and `setPreviewQuality`. Earlier code wrote `AF_MODE_CONTINUOUS_VIDEO` inside `applyQualityToRequest` and overwrote whatever lock/tap-to-focus state existed. Pulling AF/region/EV writes into `applyControlState()` and calling it after every quality apply lets `lockFocus`, `focusAt`, and `setExposureCompensation` keep their effect across these transitions.

**How to apply:** When adding any new sticky camera-side control (AWB lock, scene mode, ISO override, etc.), put the write in `applyControlState()`, store the user intent on a member (`focusLocked_`, `aeCompCurrent_`, …), and ensure every code path that rebuilds `request_` calls `applyQualityToRequest()` then `applyControlState()` before re-arming the repeating request.

Coordinate mapping for tap-to-focus: view-space (px) → normalized [0,1] → undo front-lens horizontal mirror → rotate by sensor orientation (90/180/270) → scale into `ACAMERA_SENSOR_INFO_ACTIVE_ARRAY_SIZE` rect. Sensor active array is cached per-open in `cacheCharacteristicsFor()` along with `lensIsFront_`, `sensorOrientationCached_`, and `aeCompMin_/aeCompMax_`.
