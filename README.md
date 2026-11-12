# vFTDI

An independent VATSIM EuroScope plugin that displays and lets controllers adjust an arrival sequence, with final-approach separation targets for adjacent aircraft. It is a controller-training and simulation aid, not a certified or approved operational ATC system.

## Current features

- Press `Ctrl+W` and select one aircraft to append it to the end of the arrival sequence. Targets use adjacent aircraft in sequence order and the succeeding aircraft's assigned runway/localizer when available.
- Display a red FTD arc and, when compression is nonzero, a blue ITD arc without numeric distance labels.
- Left-clicking a row selects that aircraft as EuroScope ASEL and highlights the row yellow; this does not highlight target markers. Right-clicking focuses the aircraft's own FTD/ITD/GAP targets as a succeeding aircraft and turns its callsign brown; right-click it again to clear that focus. The first row has no preceding aircraft and therefore no target set. With `RTE OFF`, middle-click a row to show or hide its route preview.
- Show an FTD breach alert at the radar marker only after the follower has CA/VA approach clearance and actual spacing is below FTD.
- Open a compact, centered arrival-sequence manager on the standard radar screen; it starts collapsed. Reorder arrivals, remove aircraft, set pairwise GAP distances, choose one of up to five named runway-pair profiles, and adjust route visibility. Its position is saved in EuroScope plugin settings and restored on restart.
- Calculate FTD from the greater of configured minimum distance and wake-category minimum, plus the configured final buffer. ITD adds the existing modeled compression margin.
- Override an aircraft's final approach speed from its sequence row; the custom knots value is used in compression calculations instead of the wake-category default.
- Set a pairwise distance GAP from the sequence row using a preset or custom NM value. A governing GAP is shown as a yellow bracket.
- Optionally display a dashed straight-leg route from the aircraft to LOCALIZERPOINT and then to the runway threshold. With CA/VA approach clearance, draw and calculate only present position to runway threshold, bypassing LOCALIZERPOINT. Trajectory arcs, bank-radius calculations, and capture-lead adjustments are not generated. FTD/ITD chevrons remain independent of route display.
- Optionally display a cyan turn-start circle on each sequenced follower's projected current track, indicating when to turn and capture final at its assigned runway's configured LOCALIZERPOINT. Turn recommendations are independent of route visibility.

## Controls

The radar manager is 228 px wide when expanded and uses 38 px aircraft rows. Up to five named runway-pair profiles appear as buttons below the toolbar; profile names are 1-3 letters. Turn status appears in compact color-coded badges: yellow `TURN SOON`, cyan countdowns, amber `UPDATING`, and subdued `NO DELAY`, cleared/leader, or unavailable states. When no delay is needed, the blue turn point is hidden. Row-count `+`/`-` controls are in the title bar; the duplicate ARRIVALS header has been removed. Available screen height and the configured startup row count still govern scrolling.

- Drag the sequence manager's title strip to move it; it stays inside the radar viewport. Click `+` to expand it or `-` to collapse it. In the expanded window, `+`/`-` adjust its height and the right-side scrollbar navigates longer sequences.
- Use **+ ACFT** or `Ctrl+W` first; the control changes to **PICK ACFT** while waiting. Then click an aircraft to append it. A click made before arming does not add the aircraft. Left-click a sequence row to set EuroScope ASEL and highlight the row yellow; this does not highlight targets. Right-click to focus the aircraft's FTD/ITD/GAP targets when it is a succeeding aircraft, and turn its callsign brown; right-click the same row again to clear that focus. The first row has no target set. With `RTE OFF`, middle-click a row to show or hide its route preview. To reorder, click **MOVE** on the source row, click the destination row, then choose **Above** or **Below**. **x** removes an aircraft and recalculates adjacent targets. `RTE ON/OFF` controls route visibility.
- Set a pairwise GAP with the `GAP` row button and choose a preset or custom NM distance. `FAS` opens a custom final-speed option; **Use aircraft type default** is offered only when that aircraft currently has an override. Right-click while selecting to cancel.
- Select one of the named runway-pair profiles below the toolbar in either vFTDI window. This is one shared profile for the entire sequence, not a per-callsign override. The colored strip at the left of each follower row shows its effective pair mode.
- Enter `SHOWLOCALIZERROUTE` or `.SHOWLOCALIZERROUTE` to toggle route display.
- Select the `TURN` checkbox in the expanded vFTDI manager or the separate vFTDI view to enable or disable turn recommendations. It defaults to off for each plugin session. The current-track leg to the dot, direct leg to LOCALIZERPOINT, and final leg to the runway are shown when routes are enabled.

## Downstream Targets

Each aircraft's expected route position is its modeled remaining-route distance mapped onto its assigned localizer. It uses the direct leg to LOCALIZERPOINT plus the final leg, without added turn-arc mileage. Before approach clearance, the reference for spacing the next aircraft is whichever is farther behind on the localizer: this expected position or the aircraft's own FTD target. Here, farther behind means a larger distance from the runway threshold. An uncleared aircraft's expected position ahead of FTD therefore cannot pull downstream targets forward; one behind FTD pushes them back.

Once CA/VA approach clearance is set, downstream spacing uses only the cleared aircraft's expected route position, even when it is ahead of its old FTD. If that expected position is unavailable, its current actual position projected onto the localizer is used; its old FTD is not carried forward. The following aircraft's own configured separation requirement is unchanged.

This comparison is recalculated for every adjacent pair on radar refresh and propagates through the sequence without latching. The following aircraft's FTD/ITD must remain at least its required spacing behind that reference; an already farther-back target is retained. For an uncleared aircraft whose expected position is unavailable, the FTD target is used. Upstream ITD is not substituted for this comparison.

## Turn recommendations

The blue/cyan dot is the **earliest modeled point ahead on the current track where a turn toward LOCALIZERPOINT leaves the required ITD spacing or more**. It is not a later best-fitting or exact-match-preferred point. If turning now already leaves enough space, the dot is at the report-age-adjusted current position. This is a simulation planning boundary under the model's assumptions, not an operational clearance authorization.

At each candidate point, the solver compares the follower's remaining straight-leg route distance (direct to LOCALIZERPOINT, then to the runway) with the moving ITD slot's remaining localizer distance. It uses the displayed ITD marker after compression, GAP, runway-coordinate conversion, and downstream adjustments, advancing that slot by the leader's modeled movement until turn initiation. It searches forward in 0.05 NM steps and refines the first insufficient-to-sufficient spacing boundary by bisection. It does not search for past turn points or prefer a later exact match over an already feasible point. `+` means the earliest point leaves more than 0.02 NM extra behind ITD. FTD remains the final-separation minimum, not the turn target.

A single failed solve can retain a recent point for at most eight seconds as an amber `UPDATING` circle with a dotted connector. It must still be ahead, geometrically feasible, on or behind the current ITD slot, within five seconds of groundspeed-equivalent ITD error, and in the unchanged runway/sequence/speed context. It is not a fresh earliest-point result and never displays `TURN NOW`. Retention cannot extend its own expiry. Clearance, data loss, invalid geometry, changed context, excessive error, or expiry removes it immediately.

LOCALIZERPOINT remains the routing point before final for an uncleared aircraft. The estimate changes direction at that point without modeling the physical turn radius or a fly-by capture arc. All headings, including opposite-direction approaches, have the same direct-route calculation; there is no bank-angle, turn-circle overlap, or maximum-arc-angle rejection.

CA/VA approach clearance immediately stops the follower's turn recommendation: its circle and turn preview disappear, its row shows `CLEARED`, and the previous turn countdown is discarded. Its route becomes one straight segment from present position to the assigned runway threshold; LOCALIZERPOINT is not visited or counted. This applies to leaders and followers, in route display, remaining distance, spacing, compression, and downstream expected position. Removing clearance restores the LOCALIZERPOINT route and permits turn recommendations again when otherwise eligible.

Planning predictions use time-based heading/groundspeed filtering with an eight-second response time to reduce radar jitter. Fresh earliest-point results are not shifted by countdown smoothing, which could move them away from the first feasible point. Report age is accounted for in the current-position bound and moving-slot estimate. The route preview uses the same candidate geometry as the displayed point and rolls out at LOCALIZERPOINT.

FTD/ITD markers use the leader's remaining modeled route distance instead of a potentially distant heading/localizer intersection. Before placement, the leader distance is translated from its own runway threshold into the follower's localizer coordinate frame, accounting for staggered parallel runways. FTD is placed at least its required distance behind the leader's physical projection on that localizer; ITD is no closer than FTD. Route-based current spacing uses the same threshold-offset conversion. This avoids targets appearing ahead of the leader when reduced dependent spacing is smaller than the runway stagger, as well as large jumps when the track is nearly parallel to the localizer. Filtering applies to planning speed/track, compression estimates, and turn timing; observed current spacing, configured FTD/wake/GAP minima, and breach-alert comparisons are not smoothed or reduced.

Filters reset immediately for significant motion changes (15 degrees of heading or 25 kt relative to the filtered value), runway/clearance changes, leader or spacing-mode changes, governing GAP changes, and final-speed overrides. Long data gaps reset filters; reports older than 30 seconds cannot generate a new turn recommendation. State is retained for unchanged adjacent pairs when resizing or rebuilding the sequence. These measures reduce nuisance movement but do not validate the predictor for operational ATC.

Current route distances for uncleared leaders and followers use direct distance to LOCALIZERPOINT plus the final leg to the threshold. CA/VA-cleared aircraft instead use present-position-to-runway distance only, regardless of LOCALIZERPOINT configuration. Aircraft already established inside the join point also use their remaining final leg without routing back to LOCALIZERPOINT. The same straight-leg model feeds remaining-route spacing, compression, ITD delivery, and route previews. A missing turn-time recommendation does not suppress chevrons or remove the current route. The `NO ARC` state and its chevron-suppression branch have been removed entirely. Route geometry no longer requires groundspeed; speed data are still needed for timing and compression estimates.

`TURN SOON` is the warning during the final 15 seconds before the earliest point. When the modeled delay reaches zero, the row shows `NO DELAY` and the blue point and its connector are hidden. `IN ...s` is the countdown outside the warning window. These row badges can include `+` if the earliest feasible point leaves extra modeled ITD spacing.

The turn-start calculation projects forward along the aircraft's current radar ground track, then measures direct distance to `LOCALIZERPOINT_<ICAO>_<RUNWAY>` and the final leg to the threshold. For example, `LOCALIZERPOINT_EDDF_25L 14` routes via the point 14 NM from runway 25L's threshold. No trajectory arcs or capture leads are drawn or included in distance. CA/VA still immediately removes turn recommendations, and established aircraft use their actual remaining final leg.

The straight-leg approximation intentionally ignores physical turn duration and extra turn mileage. It is not a turn-performance model. Configured speed phases, aircraft-specific final-speed overrides, wind estimates, governing GAP, wake minima, and the earliest-ITD-point rule remain in use. Because trajectory arcs are no longer part of the estimator, predictions may differ from earlier arc-based builds.

The search covers ten minutes or 40 NM ahead, whichever is shorter. `TURN` must be enabled; `RTE OFF` does not suppress the circle. The first aircraft shows `LEADER`, since the boundary is relative to a preceding aircraft. When no point can be calculated, the row shows `NO RWY`, `NO SPEED`, `ON FINAL`, or `NO MATCH`. A valid route and its chevrons remain available when only the earliest-turn boundary cannot be found. The model assumes the current track is maintained until initiation and does not model pilot reaction delay or physical turn dynamics.

This is a simulation approximation inspired by the paper's turn-circle presentation, not a reproduction of the DFS trajectory algorithm, which the paper does not specify. It provides one recommendation per manually sequenced pair; automatic gap detection and earliest/latest insertion windows are not implemented. Do not use the recommendations for operational ATC.

The standalone geometry/timing regression checks are in `tests/TurningPointMathTests.cpp`. From a Visual Studio C++ developer terminal, compile with `cl /EHsc /W4 tests\TurningPointMathTests.cpp /Fe:TurningPointMathTests.exe` and run `TurningPointMathTests.exe`.

## Configuration

`DEFAULTROWS` sets the initial maximum number of visible aircraft rows in both vFTDI views. For example, `#DEFAULTROWS 999` allows up to 999 rows to be visible when space permits. Values below 1 are clamped to 1; omitting the setting or supplying invalid text keeps the previous default of three. The panel's `+` and `-` controls still adjust the row count during the session. Available screen height limits the number drawn at once, and scrolling remains available for additional aircraft; this does not limit the arrival sequence length. Reload the plugin to apply a changed startup default.

The plugin reads `config.txt` from the directory containing the DLL. The repository and `Release` folders include example configurations. Key settings include `MINDIST`, `MINPARALLELDEPENDENTDISTANCE`, `FINALBUFFER`, `WTC_<leader>_<follower>`, `LONGAPPROACHNM`, `LONGAPPROACHSPEEDKTS`, `SPEEDUNTILREDUCTIONKTS`, `SPEEDREDUCTIONNM`, runway-specific `LOCALIZERCOURSE_<ICAO>_<RUNWAY>` and `LOCALIZERPOINT_<ICAO>_<RUNWAY>`, and marker color RGB values. `LONGAPPROACHNM` defines the remaining-route distance where the model transitions from current groundspeed to `LONGAPPROACHSPEEDKTS` for its approach estimate; `0` disables this phase. The speed is accounted for in predicted ITD calculations before an aircraft reaches that boundary. After joining, the existing `SPEEDUNTILREDUCTIONKTS` to `SPEEDREDUCTIONNM` and final-approach-speed phases are unchanged. `COLOR_FTD_TARGET` controls the red FTD arc and `COLOR_ITD_TARGET` the blue ITD arc.

### Sequence Mode And Runway Rules

`PROFILE` declares up to five selectable runway-pair profiles. Each name must contain 1-3 letters. `DEFAULTPROFILE` selects the profile used on plugin load. `RUNWAYPAIR` entries begin with the profile name, then airport, two runways, and a mode (`ALTERNATING`, `DEPENDANT`, or `INDEPENDENT`). Rules are symmetric. Unlisted pairs default to Alternating. `DEPENDENT` is accepted as an alternative spelling to `DEPENDANT`. Independent pairs receive no inter-runway FTD/ITD/GAP target; their succeeding aircraft's expected route position still anchors the next pair. Selecting a profile in either vFTDI window applies it to the whole sequence immediately.

```text
#PROFILE ALT
#PROFILE DEP
#PROFILE IND
#DEFAULTPROFILE DEP
#RUNWAYPAIR ALT EDDF 25L 25C ALTERNATING
#RUNWAYPAIR ALT EDDF 25L 25R ALTERNATING
#RUNWAYPAIR DEP EDDF 25L 25C ALTERNATING
#RUNWAYPAIR DEP EDDF 25L 25R DEPENDANT
#RUNWAYPAIR DEP EDDF 25C 25R DEPENDANT
#RUNWAYPAIR IND EDDF 25L 25C ALTERNATING
#RUNWAYPAIR IND EDDF 25L 25R INDEPENDENT
#RUNWAYPAIR IND EDDF 25C 25R DEPENDANT
```

In this example, `ALT` uses Alternating spacing, `DEP` uses reduced Dependant spacing for its listed pairs, and `IND` makes 25L/25R Independent while 25C/25L remains Alternating. Airport and runway values are case-insensitive; single-digit runway numbers are normalized with a leading zero. The last valid rule for a pair within a profile wins.

Same-runway pairs, pairs at different airports, missing or invalid assignments, and unlisted pairs always remain Alternating. Alternating mode overrides all reduced-spacing rules. Rules are reevaluated on radar refresh, including after runway changes or sequence reordering. No manual S/D callsign overrides remain.

Alternating FTD is the greater of `MINDIST` and the configured wake separation, plus `FINALBUFFER`. A configured Dependant pair instead uses `MINPARALLELDEPENDENTDISTANCE` plus `FINALBUFFER`. A larger pairwise GAP still takes precedence, and compression is then added for ITD as before. These configurable values are for simulation and must not be interpreted as locally approved operational minima. Legacy `SEQUENCEMODE` and four-field `RUNWAYPAIR` entries are still accepted when no named profiles are declared.

The native parser/rule regression checks are in `tests/RunwayModeTests.cpp`.

### Marker Size

FTD defaults slightly larger than ITD, following the relative sizing shown in the EUROCONTROL guideline. Set the symbol span across the approach path in screen pixels:

```text
#FTDMARKERSIZE 26
#ITDMARKERSIZE 22
```

Arc depth and GAP bracket dimensions scale with the span; pen thickness remains unchanged. Alert-label placement follows the configured dimensions. ITD spans are limited to 6-64 px and FTD to 8-96 px. FTD is automatically raised to at least ITD + 2 px if necessary, so the two remain distinguishable. Missing or invalid-text settings use the defaults. Reload the plugin after editing. These sizes affect only the symbols, not separation distances or their 0.1 NM placement resolution. The guideline does not prescribe these specific pixel values.

### Colors

All colors drawn by vFTDI are configurable in the DLL's `config.txt` using `#COLOR_NAME R G B`, with integer channels from 0 to 255. The Release configuration lists every supported color while retaining the existing appearance. Reload the plugin or restart EuroScope after changing the file. Missing entries use built-in defaults; malformed or out-of-range entries are ignored. If route colors are omitted, they inherit the corresponding configured target colors.

| Setting | Controls |
| --- | --- |
| `COLOR_FTD_TARGET` / `COLOR_ITD_TARGET` | Final and initial target arcs |
| `COLOR_FTD_ROUTE` / `COLOR_ITD_ROUTE` | Leader/FTD and ITD dashed route overlays |
| `COLOR_GAP_TARGET` | Governing GAP bracket |
| `COLOR_BREACH_ALERT` | Separation breach alert text |
| `COLOR_SELECTION_TEXT` | Aircraft-selection prompt |
| `COLOR_TURN_POINT` / `COLOR_TURN_PASSED` | Earliest-point and provisional UPDATING turn circles and their connector lines |
| `COLOR_TOOLTIP_BACKGROUND` / `COLOR_TOOLTIP_TEXT` | Custom radar target hover-label background and text |
| `COLOR_PANEL_BACKGROUND` / `COLOR_PANEL_SURFACE` | Window background and header/button surfaces |
| `COLOR_PANEL_ROW` / `COLOR_PANEL_DIVIDER` | Sequence row backgrounds, borders, and scrollbar tracks |
| `COLOR_PANEL_TEXT` / `COLOR_PANEL_HEADER_TEXT` | Normal and header/button text |
| `COLOR_PANEL_MUTED_TEXT` | Secondary and disabled text |
| `COLOR_PANEL_ACCENT` / `COLOR_PANEL_WARNING` | Selection accents, active GAP/dependent highlights, and scrollbar thumbs |
| `COLOR_PANEL_DISABLED` / `COLOR_PANEL_CONTROL` | Disabled button and title/resize button backgrounds |
| `COLOR_PANEL_REMOVE_BACKGROUND` / `COLOR_PANEL_REMOVE_TEXT` | Remove button background and text |
| `COLOR_PANEL_STATUS_WARNING` | Passed-turn and unavailable-prediction status text |

EuroScope-owned popup menus and native screen-object tooltips remain controlled by EuroScope and Windows, not by these plugin settings.

## Scope

The EUROCONTROL FTDi guidelines and DFS Spacing-Assistant paper supplied as design references describe a broader set of functions, including runway-occupancy-time constraints, automatic sequence alerts, and safety alerts. This implementation does not currently provide those functions. Each succeeding aircraft follows the preceding aircraft in the sequence: ITD is FTD plus the existing compression margin and converges to FTD at the delivery point. The infringement alert compares actual spacing to FTD, not ITD, and is displayed only after the follower is cleared for approach. ITD here is not time-based separation. The PDFs are not a substitute for locally approved minima, procedures, safety assessment, or operational validation.

References:

- [EUROCONTROL FTDi Guidelines](https://www.eurocontrol.int/publication/eurocontrol-guidelines-final-target-distance-indicator-ftdi-distance-based-separation)
- [Haugg and Konopka, Spacing-Assistant for Leipzig and Munich Approach](https://doi.org/10.1016/j.trpro.2022.12.029)

## Credits and license

This project is distributed under GPLv3. vFTDI is maintained by Roger Puig and is independent of EUROCONTROL, DFS, NATS, and real-world FTDi deployments.
