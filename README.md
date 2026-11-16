# vFTDI

An independent VATSIM EuroScope plugin that displays and lets controllers adjust an arrival sequence, with final-approach separation targets for adjacent aircraft. It is a controller-training and simulation aid, not a certified or approved operational ATC system.

<img width="729" height="471" alt="image" src="https://github.com/user-attachments/assets/d83830ff-5e12-43d9-be76-af71d7df8a7a" />
<img width="232" height="254" alt="image" src="https://github.com/user-attachments/assets/886cad67-d934-49a6-8d0c-eea8640df939" />



## Current features

- Press `Alt+W` and select one aircraft to append it to the end of the arrival sequence. Targets use adjacent aircraft in sequence order and the succeeding aircraft's assigned runway/localizer when available.
- When EuroScope marks a radar target invalid, it is automatically removed from the sequence on the next radar refresh. Its aircraft-specific prediction state, speed override, selections, and pending actions are cleared; FTD/ITD is recalculated between the remaining aircraft. A valid target with an old position report is not removed solely for report age. Reconnecting aircraft must be added again.
- Display a red FTD arc and, when compression is nonzero, a blue ITD arc without numeric distance labels.
- Left-clicking a row selects that aircraft as EuroScope ASEL and highlights the row yellow; this does not highlight target markers. Right-clicking focuses the aircraft's own FTD/ITD/GAP targets as a succeeding aircraft and turns its callsign brown; right-click it again to clear that focus. The first row has no preceding aircraft and therefore no target set. With `RTE OFF`, middle-click a row to show or hide its route preview.
- Show an FTD breach alert at the radar marker only after the follower has CA/VA approach clearance and actual spacing is below FTD.
- Open a compact, centered arrival-sequence manager on the standard radar screen; it starts collapsed. Reorder arrivals, remove aircraft, set pairwise GAP distances, choose one of up to five named runway-pair profiles, and adjust route visibility. Its position is saved in EuroScope plugin settings and restored on restart.
- Optionally add arrivals automatically for one destination airport and review suggested sequence changes that reduce estimated average delay.
- Calculate FTD from the greater of configured minimum distance and wake-category minimum, plus the configured final buffer. ITD adds the existing modeled compression margin.
- Override an aircraft's final approach speed from its sequence row; the custom knots value is used in compression calculations instead of the wake-category default.
- Set a pairwise distance GAP from the sequence row using a preset or custom NM value. A governing GAP is shown as a yellow bracket.
- Optionally display a dashed route with constant-radius turns from the aircraft to LOCALIZERPOINT and then to the runway threshold. With CA/VA approach clearance, draw and calculate only present position to runway threshold, bypassing LOCALIZERPOINT. Turn radius uses a nominal 25-degree bank and current groundspeed. FTD/ITD chevrons remain independent of route display.
- Optionally display a cyan turn-start circle on each sequenced follower's projected current track, indicating when to turn and capture final at its assigned runway's configured LOCALIZERPOINT. Turn recommendations are independent of route visibility.

## Controls

Each callsign has a wake-category badge: green `L`, blue `M`, amber `H`, or red `S`. Heavy and Super badges use bold text for extra emphasis. EuroScope category `J` is displayed as `S`; an unavailable or unrecognized category is shown as a neutral `?`. Badges appear in both vFTDI views.

The radar manager is 228 px wide in both collapsed and expanded states and uses 38 px aircraft rows. Up to five named runway-pair profiles appear as buttons below the toolbar; profile names are 1-3 alphanumeric characters. Turn status appears in compact color-coded badges: red `TURN NOW`, cyan countdowns, amber `UPDATING`, and subdued `NO DELAY`, cleared/leader, or unavailable states. When no delay is needed, the blue turn point is hidden. Row-count `+`/`-` controls are in the title bar; the duplicate ARRIVALS header has been removed. Available screen height and the configured startup row count still govern scrolling.

Between the callsign and wake badge, each aircraft row shows the signed difference between the latest radar groundspeed and the groundspeed currently used by the modeled route phase, rounded to the nearest knot: `+20` means 20 kt faster than modeled, `-20` means 20 kt slower, and `+05` means 5 kt faster. Hover to see both speeds. The modeled speed uses the existing speed phases, smoothing, wind estimate and final-speed overrides; when the current phase uses the observed groundspeed, the difference will normally be close to zero. `--` means a fresh report or modeled speed is unavailable. This is a prediction comparison, not a comparison to an assigned indicated airspeed or a speed instruction.

- Drag the sequence manager's title strip to move it; it stays inside the radar viewport. Click `+` to expand it or `-` to collapse it. In the expanded window, `+`/`-` adjust its height and the right-side scrollbar navigates longer sequences.
- Use **+ ACFT** or `Alt+W` first; the control changes to **PICK ACFT** while waiting. Then click an aircraft to append it. A click made before arming does not add the aircraft. Left-click a sequence row to set EuroScope ASEL and highlight the row yellow; this does not highlight targets. Right-click to focus the aircraft's FTD/ITD/GAP targets when it is a succeeding aircraft, and turn its callsign brown; right-click the same row again to clear that focus. The first row has no target set. With `RTE OFF`, middle-click a row to show or hide its route preview. To reorder, click **MOVE** on the source row, click the destination row, then choose **Before** or **After**. **x** removes an aircraft and recalculates adjacent targets. `RTE ON/OFF` controls route visibility.
- Set a pairwise GAP with the `GAP` row button and choose a preset or custom NM distance. `FAS` opens a custom final-speed option; **Use aircraft type default** is offered only when that aircraft currently has an override. Right-click while selecting to cancel.
- Select one of the named runway-pair profiles below the toolbar in either vFTDI window. This is one shared profile for the entire sequence, not a per-callsign override. The colored strip at the left of each follower row shows its effective pair mode.
- Enter `SHOWLOCALIZERROUTE` or `.SHOWLOCALIZERROUTE` to toggle route display.
- Select the `TURN` checkbox in the expanded vFTDI manager or the separate vFTDI view to enable or disable turn recommendations on the radar. It defaults to off for each plugin session; panel statuses remain available either way. The current-track leg to the dot, curved leg to LOCALIZERPOINT, and final leg to the runway are shown when routes are enabled.

### Automatic Sequence

Click `AUTO OFF` in either vFTDI window and enter a four-letter destination ICAO code. Auto mode scans EuroScope radar targets on radar refresh and adds matching correlated flights above `minimumAltitude`, within `maxDistance`, and with an assigned arrival runway listed under `[runways.<airport>.<runway>]` in the DLL's configuration. The same runway must have geometry in the active sector file; it need not be marked active for arrivals. Flights without a matching assignment wait until one is set. If an auto-added flight's runway is cleared or changed to an unconfigured or unavailable runway, it is removed and can be added again when a valid runway is assigned. If the airport has no configured runways with usable sector-file geometry, automatic processing pauses. Manually added entries are not removed by the runway filter.

Existing sequence entries remain in their current relative order. New non-CA arrivals are inserted using flight-plan distance and groundspeed when available, after aircraft locked past LOCALIZERPOINT and without breaking an existing GAP pair. A newly discovered CA aircraft or an arrival without a usable estimate is appended. The selected airport button opens options to change airports or disable auto mode. Disabling preserves the sequence; changing airports or leaving the distance limit removes affected aircraft added by auto mode, while retaining pre-existing manual entries. A manually removed matching aircraft is added again while auto mode is active.

When a beneficial change is available, the lamp at the right of the auto-sequence row turns yellow. Click the lamp to display an arrow on the radar from the aircraft to be moved toward the aircraft it would precede and open a menu naming both aircraft. Choose `Accept` or `Decline`; the arrow disappears when the menu is dismissed, including with Escape or a click elsewhere. The arrow shows sequence order only; it is not a flight path or heading instruction. A collapsed radar manager displays `SUGGESTION` in its title, and a user message announces a new proposal. The preview also disappears when the proposal changes or either target is unavailable or stale. An active standard radar screen is needed for the preview.

A proposal remains in place for up to 30 seconds while it remains valid and meets the savings and individual-delay limits, even if another proposal becomes preferable. It is rechecked on evaluation and immediately before acceptance; missing data, changed sequence, a lock, an invalid GAP pair, or lost benefit discards it at the next evaluation. After 30 seconds it can be replaced by a better proposal. After accepting or declining, new suggestions pause for one minute unless the sequence changes. Declined changes are not proposed again for the same sequence, but other improvements can be offered. Clearing or manually changing the sequence permits reconsideration.

The estimate uses the existing modeled remaining-route times, speed phases, runway profile, wake/dependent minima, configured GAPs, and threshold offsets. It assigns each flight an earliest slot satisfying applicable preceding flights and scores the average added time relative to its unconstrained modeled arrival. It proposes moving one non-CA aircraft forward by at most two positions, including between CA aircraft, only when the estimated average saving is at least five seconds per aircraft and no individual aircraft gains more than 30 seconds of modeled delay. A CA aircraft is never the aircraft moved by an automatic suggestion; its sequence number can still change when another aircraft is inserted before it. If the moving aircraft receives CA while a suggestion is pending, the suggestion is invalidated on revalidation. This CA-specific rule does not treat VA as CA. The limits reduce disruptive suggestions but may leave some usable gaps or larger improvements unproposed; this is not a globally optimal scheduler. Configured GAP pairs retain their applicable predecessor and spacing. To avoid blocking radar refresh on large sequences, the search has a work limit and starts with aircraft at the back; hover over the airport button to see if the search was partial. Aircraft are locked once established inside LOCALIZERPOINT, and proposals only move aircraft after the last locked aircraft. The established check requires the aircraft to be within 0.5 NM of the localizer and within 30 degrees of its inbound heading. CA/VA still changes the modeled route and speeds. Proposal evaluation pauses when a manually sequenced aircraft is outside `maxDistance`, when a sequence entry is for another airport, or when any flight lacks a fresh radar report, valid speed, flight plan, assigned runway, or usable runway geometry. Hover over the airport button for status, including the callsign when data is missing or the last locked aircraft when no beneficial move is found. Scanning and proposal evaluation require an active standard radar screen; proposals are normally re-evaluated every five seconds when it refreshes, except during the decision pause.

These are simulation estimates, not clearances or operational sequencing advice. Actual delay, route feasibility, pilot response, and separation must be assessed independently.

### Sequence TAG Item

In EuroScope's TAG editor, add the vFTDI `Sequence Number` item and bind its desired mouse action to vFTDI `Sequence Move/Pick`. The item displays only the aircraft's current one-based sequence number, in dark orange, with at least two digits (`01`, `02`, ..., `10`, ..., `100`); aircraft outside the vFTDI sequence have a blank item. Numbers update from the shared sequence after reordering.

Click the source aircraft's number to open its menu, then click **Move** to pick it for moving. Opening or dismissing the menu does not start a move. Once picked, click another aircraft's number and choose **Before** or **After**. Clicking the picked source again opens a **Cancel Move** option; the move remains active unless that option is selected. The positioning popup also offers **Cancel move**, and `Cancel Sequence Move` is available as a separate bindable TAG action. The picked source is shared with the panel's MOVE/PICK controls, so TAG and panel actions can be combined. Picking a source does not change the TAG's number or color.

The color is configurable under `[colors]` as `TAG_SEQUENCE_NUMBER = 205, 100, 0`. Reload the plugin after changing the color or installing the updated DLL; adding the item and binding its mouse action are EuroScope TAG-layout settings.

Bind the vFTDI `Toggle Targets` action to a TAG mouse action (for example, right-click on `Sequence Number`) to focus that aircraft's FTD/ITD/GAP targets, just like right-clicking its panel row. Invoke it again on the same aircraft to clear focus, or on another sequenced aircraft to switch focus. The panel reflects the same selection; aircraft outside the sequence are ignored. This does not change the sequence or the MOVE/PICK state. An aircraft without a preceding spacing pair has no targets to highlight.

### Suggestion TAG Item

In EuroScope's TAG editor, add the vFTDI `Sequence Suggestion` item and bind its mouse action to the vFTDI `Sequence Suggestion` function. When a current auto-sequence suggestion exists, the moving aircraft and the aircraft it would precede show purple `SUG`; all other aircraft have a blank item. Clicking `SUG` on either affected aircraft displays the radar arrow and opens the same `Accept` / `Decline` menu as the panel lamp; the arrow disappears when that menu is dismissed. A stale selection is ignored and acceptance is revalidated against current data. The TAG indicator color can be configured with `TAG_SEQUENCE_SUGGESTION` under `[colors]` and defaults to `145, 55, 190`.

## Downstream Targets

Each aircraft's expected route position is its modeled remaining-route distance mapped onto its assigned localizer. It uses the curved leg to LOCALIZERPOINT plus the final leg, including the added curve distance. Before approach clearance, the reference for spacing the next aircraft is whichever is farther behind on the localizer: this expected position or the aircraft's own FTD target. Here, farther behind means a larger distance from the runway threshold. An uncleared aircraft's expected position ahead of FTD therefore cannot pull downstream targets forward; one behind FTD pushes them back.

When an aircraft's route is visible (`RTE ON` or its individual route preview), or its FTD/ITD targets are focused through the panel or TAG action, an orange hollow diamond without a text label shows this expected position on the assigned localizer, extended outward when necessary. It updates on radar refresh, including during `NO DELAY`, after approach clearance, and for the sequence leader when its route is visible. It is a route-distance equivalent, not a predicted physical intercept point. The marker is hidden only when neither its route nor its target focus is active; the `TURN` checkbox does not control it.

Once CA/VA approach clearance is set, the downstream reference uses the cleared aircraft's expected route position, even when it is ahead of its old FTD. If that expected position is unavailable, its current actual position projected onto the localizer is used; its old FTD and ITD are not carried forward. The following aircraft's own configured separation requirement is unchanged.

This comparison is recalculated for every adjacent pair on radar refresh and propagates through the sequence without latching. For a dependent pair with both runway routes available, the following FTD is also placed at least its required direct distance behind the preceding aircraft's expected position, using the expected marker's lateral distance from the following localizer. This applies before and after CA/VA, including when the preceding aircraft is the sequence leader. The following ITD uses the compression-adjusted distance and remains at or behind its FTD; an already farther-back target is retained. Additionally, when a dependent pair has an uncleared preceding aircraft with an ITD target, the following FTD is placed at least its required direct distance behind that ITD. The following ITD also respects its own compression-adjusted distance from that ITD. These dependent floors do not apply to alternating pairs; the preceding ITD floor stops after CA/VA.

## Turn recommendations

The blue/cyan dot is the **earliest modeled point ahead on the current track where a turn toward LOCALIZERPOINT leaves the required ITD spacing or more**. It is not a later best-fitting or exact-match-preferred point. If turning now already leaves enough space, the dot is at the report-age-adjusted current position. This is a simulation planning boundary under the model's assumptions, not an operational clearance authorization.

At each candidate point, the solver compares the follower's remaining curved route distance (via LOCALIZERPOINT, then to the runway) with the moving ITD slot's remaining localizer distance. It uses the displayed ITD marker after compression, GAP, runway-coordinate conversion, and downstream adjustments, advancing that slot by the leader's modeled movement until turn initiation. It searches forward in 0.05 NM steps and refines the first insufficient-to-sufficient spacing boundary by bisection. It does not search for past turn points or prefer a later exact match over an already feasible point. `+` means the earliest point leaves more than 0.02 NM extra behind ITD. FTD remains the final-separation minimum, not the turn target.

A single failed solve can retain a recent point for at most eight seconds as an amber `UPDATING` circle with a dotted connector. It must still be ahead, geometrically feasible, on or behind the current ITD slot, within five seconds of groundspeed-equivalent ITD error, and in the unchanged runway/sequence/speed context. It is not a fresh earliest-point result and never displays `TURN NOW`. Retention cannot extend its own expiry. Clearance, data loss, invalid geometry, changed context, excessive error, or expiry removes it immediately.

LOCALIZERPOINT remains the routing point before final for an uncleared aircraft. The route consists of a circular turn from the current track, a tangent straight leg, and a circular turn that rolls onto the inbound localizer at LOCALIZERPOINT. The model compares the four left/right turn combinations and selects the shortest feasible arc-straight-arc route, preferring a consistent side for nearly tied centerline reversals. It is not a procedure or obstacle-aware route planner.

If the modeled route needs either turn to exceed 180 degrees, or no curve can be constructed, vFTDI instead uses straight segments from the aircraft or modeled turn point to LOCALIZERPOINT and then to the runway. The same straight-line distance is used for the displayed route, expected position, spacing, turn timing, and sequence estimates; these predictions are not suppressed. This fallback assumes an instantaneous change of direction and does not model the actual turn or guarantee that the route can be flown. It is a simulation approximation, not an instruction or operational separation guarantee.

Turn radius is estimated from current groundspeed using a fixed 25-degree bank assumption; higher speeds produce wider turns. If speed is missing, geometry uses a nominal 180 kt, although valid speed data are still required for turn timing. Modeled distance uses exact circular arc lengths plus the tangent straight leg. The drawing samples each turn separately so a long straight leg cannot hide a short final arc. CA/VA-cleared and already-established aircraft retain their existing direct/final-leg handling.

CA/VA approach clearance immediately stops the follower's turn recommendation: its circle and turn preview disappear, its row shows `CLEARED`, and the previous turn countdown is discarded. Its route becomes one straight segment from present position to the assigned runway threshold; LOCALIZERPOINT is not visited or counted. This applies to leaders and followers, in route display, remaining distance, spacing, compression, and downstream expected position. Removing clearance restores the LOCALIZERPOINT route and permits turn recommendations again when otherwise eligible.

Planning predictions use time-based heading/groundspeed filtering with an eight-second response time to reduce radar jitter. Fresh earliest-point results are not shifted by countdown smoothing, which could move them away from the first feasible point. Report age is accounted for in the current-position bound and moving-slot estimate. The route preview uses the same candidate geometry as the displayed point and rolls out at LOCALIZERPOINT.

FTD/ITD markers use the leader's remaining modeled route distance instead of a potentially distant heading/localizer intersection. Before placement, the leader distance is translated from its own runway threshold into the follower's localizer coordinate frame, accounting for staggered parallel runways. FTD is placed at least its required distance behind the leader's physical projection on that localizer; ITD is no closer than FTD. Route-based current spacing uses the same threshold-offset conversion. This avoids targets appearing ahead of the leader when reduced dependent spacing is smaller than the runway stagger, as well as large jumps when the track is nearly parallel to the localizer. Filtering applies to planning speed/track, compression estimates, and turn timing; observed current spacing, configured FTD/wake/GAP minima, and breach-alert comparisons are not smoothed or reduced.

Filters reset immediately for significant motion changes (15 degrees of heading or 25 kt relative to the filtered value), runway/clearance changes, leader or spacing-mode changes, governing GAP changes, and final-speed overrides. Long data gaps reset filters; reports older than 30 seconds cannot generate a new turn recommendation. State is retained for unchanged adjacent pairs when resizing or rebuilding the sequence. These measures reduce nuisance movement but do not validate the predictor for operational ATC.

Current route distances for uncleared leaders and followers use the curved distance to LOCALIZERPOINT plus the final leg to the threshold. CA/VA-cleared aircraft instead use present-position-to-runway distance only, regardless of LOCALIZERPOINT configuration. Aircraft already established inside the join point also use their remaining final leg without routing back to LOCALIZERPOINT. The same curved model feeds remaining-route spacing, compression, ITD delivery, and route previews. A missing turn-time recommendation does not suppress chevrons or remove the current route. Route geometry does not require groundspeed; speed data are still needed for timing and compression estimates.

`TURN NOW` is shown from 10 seconds before until 10 seconds after the fresh earliest modeled point. The first 10 seconds of this warning precede the modeled spacing boundary and do not mean a turn already meets the modeled ITD. Once the point is reached, the blue point and connector are hidden; after the warning expires, the row shows `NO DELAY` while a fresh zero-delay solution remains available. A newly calculated future point can move the warning window. `TURN ...s` is the countdown before the warning window and can include `+` if the earliest feasible point leaves extra modeled ITD spacing. A retained provisional point continues to show `UPDATING`, not `TURN NOW`.

The turn-start calculation projects forward along the aircraft's current radar ground track, then measures the curved route to `LOCALIZERPOINT_<ICAO>_<RUNWAY>` and the final leg to the threshold. For example, `LOCALIZERPOINT_EDDF_25L 14` routes via the point 14 NM from runway 25L's threshold. CA/VA still immediately removes turn recommendations, and established aircraft use their actual remaining final leg.

The turn approximation includes extra mileage and a speed-based radius, but does not model aircraft-specific bank limits, roll-in/roll-out time, or wind drift around the turn. Groundspeed is a practical approximation for the radius calculation, not true airspeed. Configured speed phases, aircraft-specific final-speed overrides, wind estimates for speed prediction, governing GAP, wake minima, and the earliest-ITD-point rule remain in use. Predictions may differ from earlier geometric-curve builds.

The search covers ten minutes or 40 NM ahead, whichever is shorter. `TURN` controls the radar circle; `RTE OFF` does not suppress it. Panel statuses remain visible regardless of `TURN`. The first aircraft shows `LEADER`, since the boundary is relative to a preceding aircraft. When no point can be calculated, the row shows `NO RWY`, `NO SPEED`, `ON FINAL`, or `NO MATCH`. A valid route and its chevrons remain available when only the earliest-turn boundary cannot be found. The model assumes the current track is maintained until initiation and does not model pilot reaction delay or physical turn dynamics.

This is a simulation approximation inspired by the paper's turn-circle presentation, not a reproduction of the DFS trajectory algorithm, which the paper does not specify. It provides one recommendation per manually sequenced pair; automatic gap detection and earliest/latest insertion windows are not implemented. Do not use the recommendations for operational ATC.

The standalone geometry/timing regression checks are in `tests/TurningPointMathTests.cpp`. From a Visual Studio C++ developer terminal, compile with `cl /EHsc /W4 tests\TurningPointMathTests.cpp /Fe:TurningPointMathTests.exe` and run `TurningPointMathTests.exe`.

## Configuration

`DEFAULTROWS` sets the initial maximum number of visible aircraft rows in both vFTDI views. For example, `#DEFAULTROWS 999` allows up to 999 rows to be visible when space permits. Values below 1 are clamped to 1; omitting the setting or supplying invalid text keeps the previous default of three. The panel's `+` and `-` controls still adjust the row count during the session. Available screen height limits the number drawn at once, and scrolling remains available for additional aircraft; this does not limit the arrival sequence length. Reload the plugin to apply a changed startup default.

The plugin reads `config.ini` from the directory containing the DLL. The sample configuration at `src/config.ini` is copied next to the DLL during a build. Settings are grouped into `[general]`, `[runways.<ICAO>.<RUNWAY>]`, `[separation]`, `[wake]`, `[recat]`, `[profiles]`, `[profile.<NAME>]`, `[speed]`, and `[colors]` sections. Distances use nautical miles, speeds use knots, courses use degrees, and colors are RGB triples. `FTD_TARGET` controls the red FTD arc and `ITD_TARGET` the blue ITD arc.

`minimumAltitude` in `[general]` sets the inclusive minimum pressure altitude in feet for both manual and automatic sequencing. `maxDistance` sets the inclusive straight-line distance in nautical miles from the selected airport for automatic sequencing only; `0` or an omitted value disables the limit. The plugin uses the airport position in the active sector file, falling back to a runway threshold. With a positive limit and no available position, automatic processing pauses. Auto-added aircraft that leave the range are removed and can be added again when they return; manual entries are retained, but an out-of-range manual entry pauses optimization. Reload the plugin after changing the settings.

Wake spacing uses ICAO WTC by default. Set `[separation] scheme = RECAT-EU` to select the six-category RECAT-EU distance minima, or `scheme = WTC` to retain the current behavior. Under `[recat]`, each category letter A-F maps to a comma-separated list of ICAO aircraft type designators (for example, `D = A318,A319,A320,A321`). The sample config includes starter assignments transcribed from the RECAT-EU example list; verify and maintain assignments against the applicable current aircraft-category data. Configure leader/follower distances in nautical miles under `[recat.separation]` using keys such as `A_D = 5`. Unassigned aircraft pairs fall back to their existing WTC minimum; a mapped RECAT pair without a configured distance uses the configured general minimum distance. Final-approach speed defaults continue to use ICAO WTC regardless of the selected spacing scheme.

### Speed Model

The `[speed]` settings describe predicted speeds for compression and turn timing; they do not issue speed instructions or change an aircraft's actual speed. The model switches speeds at its phase boundaries immediately; it does not simulate gradual deceleration.

Before an uncleared aircraft reaches LOCALIZERPOINT, the prediction starts with its observed groundspeed. If `longApproachNm` is greater than zero, the model uses `longApproachSpeedKts` for the portion of the route within that remaining distance of the runway, up to LOCALIZERPOINT. The resulting long-approach groundspeed is capped at the aircraft's current groundspeed. Set `longApproachNm = 0` to disable this phase.

From LOCALIZERPOINT to `speedReductionNm` before the runway, the model uses `speedUntilReductionKts`. For the last `speedReductionNm`, it uses the appropriate `finalApproachSpeed_<category>`. An aircraft already established inside LOCALIZERPOINT uses the same final-leg phases without routing back to the join point.

CA/VA clearance changes the modeled route to the direct distance from the aircraft to the runway. The `[speed]` switch `reduceSpeedOnClearance` controls when the predicted speed changes:

- `true` (also `1`): use `speedUntilReductionKts` immediately on clearance, until `speedReductionNm`, then final approach speed. This preserves the previous behavior and is the fallback if the setting is omitted.
- `false` (also `0`): retain observed GS until the assigned runway's `localizerPointNm` distance from the threshold, then use `speedUntilReductionKts` until `speedReductionNm`, then final approach speed. The observed-GS portion is not replaced by `longApproachSpeedKts`, even with `longApproachNm = 999`. This is the setting in the supplied configs.

```ini
[speed]
reduceSpeedOnClearance = false
speedUntilReductionKts = 170
speedReductionNm = 5
```

For example, with a 12 NM localizer point and delayed reduction, an aircraft cleared at 20 NM retains its observed GS for 8 NM, then is modeled at 170 kt from 12 to 5 NM, then at its configured final speed for the last 5 NM. If cleared at 10 NM it starts in the 170 kt phase; at 4 NM it starts in the final-speed phase. These boundaries use remaining direct distance for cleared aircraft; they do not restore a detour through LOCALIZERPOINT. If the localizer distance is inside the final-speed reduction distance, the final-speed boundary takes precedence.

The switch applies to both CA and VA, in compression and leader movement used for turn predictions. Un-cleared aircraft retain the existing long-approach/localizer speed model. Clearance does not itself command a speed change. Reload the plugin after changing the setting.

Final speed comes from `finalApproachSpeed_H`, `_M`, `_L`, or `_S` according to wake category; category `J` uses `_S`. An aircraft's `FAS` row control overrides its category value for both compression and turn predictions. Where usable weather is available, the model adjusts configured speeds for headwind, tailwind, and the crosswind correction needed to follow the modeled track. It uses the current track before final, the final localizer track for final phases, and the direct-to-runway track for cleared aircraft not yet established. These estimates are shared by the speed comparison, compression, and turn predictions. Actual speed means observed GS, not IAS: it already includes wind and is never wind-adjusted again. The existing planning filter still smooths observed GS.

Weather comes from the nearest usable configured airport METAR, refreshed approximately every five minutes and ignored when older than 90 minutes. When weather is unavailable, configured speeds are used without a wind adjustment; hover over the speed difference to see whether airport wind was available. Surface wind is not a forecast of wind aloft, and the wind triangle approximates configured indicated speeds as true airspeeds without altitude or temperature correction. It does not model changing wind along the route, gusts, or wind drift during turns. The result is a simulation estimate, not an operational speed or separation guarantee.

### Sequence Mode And Runway Rules

Each `[profile.<NAME>]` section declares a selectable runway-pair profile; up to five names of one to three letters or digits are supported. `[profiles] default` selects the profile used on plugin load. Within a profile section, each pair key is `<ICAO>.<RUNWAY>.<RUNWAY>` and its value is `ALTERNATING`, `DEPENDANT`, or `INDEPENDENT`. Rules are symmetric. Unlisted pairs default to Alternating. `DEPENDENT` is accepted as an alternative spelling to `DEPENDANT`. Independent pairs receive no inter-runway FTD/ITD/GAP target; their succeeding aircraft's expected route position still anchors the next pair. Selecting a profile in either vFTDI window applies it to the whole sequence immediately.

```ini
[profiles]
default = DEP

[profile.ALT]
EDDF.25L.25C = ALTERNATING
EDDF.25L.25R = ALTERNATING

[profile.DEP]
EDDF.25L.25C = ALTERNATING
EDDF.25L.25R = DEPENDANT
EDDF.25C.25R = DEPENDANT

[profile.IND]
EDDF.25L.25C = ALTERNATING
EDDF.25L.25R = INDEPENDENT
EDDF.25C.25R = DEPENDANT

[profile.IN2]
EDDF.25L.25C = ALTERNATING
```

In this example, `ALT` uses Alternating spacing, `DEP` uses reduced Dependant spacing for its listed pairs, and `IND` makes 25L/25R Independent while 25C/25L remains Alternating. Airport and runway values are case-insensitive; single-digit runway numbers are normalized with a leading zero. The last valid rule for a pair within a profile wins.

Same-runway pairs, pairs at different airports, missing or invalid assignments, and unlisted pairs always remain Alternating. For an Independent pair, the sequence skips that aircraft and searches earlier arrivals for the nearest applicable predecessor; an aircraft without one has no spacing target. Alternating mode overrides all reduced-spacing rules. Rules are reevaluated on radar refresh, including after runway changes or sequence reordering. No manual S/D callsign overrides remain.

Alternating FTD is the greater of `minimumDistanceNm` and the configured wake separation, plus `finalBufferNm`. A configured Dependant pair instead uses `minimumParallelDependentDistanceNm` plus `finalBufferNm`, measured as the direct aircraft-to-aircraft distance (DS), not the along-runway component. Target placement accounts for runway cross-track offset so the diagonal distance meets the configured minimum. A larger pairwise GAP still takes precedence, and compression is then added for ITD as before. These configurable values are for simulation and must not be interpreted as locally approved operational minima.

The native parser/rule regression checks are in `tests/RunwayModeTests.cpp`.

### Marker Size

FTD defaults slightly larger than ITD, following the relative sizing shown in the EUROCONTROL guideline. Set the symbol span across the approach path in screen pixels:

```text
[general]
ftdMarkerSize = 26
itdMarkerSize = 22
```

Arc depth and GAP bracket dimensions scale with the span; pen thickness remains unchanged. Alert-label placement follows the configured dimensions. ITD spans are limited to 6-64 px and FTD to 8-96 px. FTD is automatically raised to at least ITD + 2 px if necessary, so the two remain distinguishable. Missing or invalid-text settings use the defaults. Reload the plugin after editing. These sizes affect only the symbols, not separation distances or their 0.1 NM placement resolution. The guideline does not prescribe these specific pixel values.

### Colors

All colors drawn by vFTDI are configurable in the DLL's `config.ini` under `[colors]`, using comma-separated RGB triples with integer channels from 0 to 255. The sample configuration lists every supported color while retaining the existing appearance. Reload the plugin or restart EuroScope after changing the file. Missing entries use built-in defaults; malformed or out-of-range entries are ignored. If route colors are omitted, they inherit the corresponding configured target colors.

| Setting | Controls |
| --- | --- |
| `FTD_TARGET` / `ITD_TARGET` | Final and initial target arcs |
| `FTD_ROUTE` / `ITD_ROUTE` | Leader/FTD and ITD dashed route overlays |
| `GAP_TARGET` | Governing GAP bracket |
| `BREACH_ALERT` | Separation breach alert text |
| `SELECTION_TEXT` | Aircraft-selection prompt |
| `TURN_POINT` / `TURN_PASSED` | Earliest-point and provisional UPDATING turn circles and their connector lines |
| `TOOLTIP_BACKGROUND` / `TOOLTIP_TEXT` | Custom radar target hover-label background and text |
| `PANEL_BACKGROUND` / `PANEL_SURFACE` | Window background and header/button surfaces |
| `PANEL_ROW` / `PANEL_DIVIDER` | Sequence row backgrounds, borders, and scrollbar tracks |
| `PANEL_TEXT` / `PANEL_HEADER_TEXT` | Normal and header/button text |
| `PANEL_MUTED_TEXT` | Secondary and disabled text |
| `PANEL_ACCENT` / `PANEL_WARNING` | Selection accents, active GAP/dependent highlights, and scrollbar thumbs |
| `PANEL_DISABLED` / `PANEL_CONTROL` | Disabled button and title/resize button backgrounds |
| `PANEL_REMOVE_BACKGROUND` / `PANEL_REMOVE_TEXT` | Remove button background and text |
| `PANEL_STATUS_WARNING` | Passed-turn and unavailable-prediction status text |
| `PANEL_WAKE_L` / `PANEL_WAKE_M` / `PANEL_WAKE_H` / `PANEL_WAKE_S` | Light, Medium, Heavy, and Super wake-category badge backgrounds |
| `TAG_SEQUENCE_NUMBER` | Sequence-number TAG text (dark orange by default) |
| `TAG_SEQUENCE_SUGGESTION` | Suggestion TAG text (purple by default) |

EuroScope-owned popup menus and native screen-object tooltips remain controlled by EuroScope and Windows, not by these plugin settings.

## Scope

The EUROCONTROL FTDi guidelines and DFS Spacing-Assistant paper supplied as design references describe a broader set of functions, including runway-occupancy-time constraints, automatic sequence alerts, and safety alerts. This implementation does not currently provide those functions. Each succeeding aircraft follows the preceding aircraft in the sequence: ITD is FTD plus the existing compression margin and converges to FTD at the delivery point. The infringement alert compares actual spacing to FTD, not ITD, and is displayed only after the follower is cleared for approach. ITD here is not time-based separation. The PDFs are not a substitute for locally approved minima, procedures, safety assessment, or operational validation.

References:

- [EUROCONTROL FTDi Guidelines](https://www.eurocontrol.int/publication/eurocontrol-guidelines-final-target-distance-indicator-ftdi-distance-based-separation)
- [Haugg and Konopka, Spacing-Assistant for Leipzig and Munich Approach](https://doi.org/10.1016/j.trpro.2022.12.029)

## Credits and license

This project is distributed under GPLv3. vFTDI is maintained by Roger Puig and is independent of EUROCONTROL, DFS, NATS, and real-world FTDi deployments.
