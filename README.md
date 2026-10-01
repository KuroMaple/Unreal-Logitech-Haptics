# Unreal Logitech Haptics

Drive the Unreal Engine 5 editor from the **Logitech MX Master 4** Actions Ring, and get
**haptic feedback** in the mouse for editor events (PIE start/stop, Live Coding, Blueprint
errors, saves, lighting builds).

Two components talk over localhost HTTP:

```
 MX Master 4 ──(Logi Options+)── Logi Plugin Service                     Unreal Editor
                                  └─ UnrealMxBridge (C#) ── HTTP ──▶ MXBridge (C++ editor plugin)
                                       • ring actions ─── POST /events ─▶ run on game thread
                                       • icon state  ◀── GET /context ──  editor snapshot
                                       • haptics     ◀── GET /haptics ──  event queue (long-poll)
```

| Folder | What |
|---|---|
| `UnrealPlugin/MXBridge` | UE 5.6 editor plugin (C++). Hosts the HTTP server on `127.0.0.1`. |
| `LogiPlugin/src/UnrealMxBridgePlugin` | Logi Actions SDK plugin (C#, .NET 10). Ring actions + haptics. |
| `scripts/test-bridge.ps1` | Calls the editor's endpoints without the mouse, for debugging. |

## Requirements

- Windows, Unreal Engine 5.6 (C++ project, or any project you can build plugins for)
- Logi Options+ with an MX Master 4 (installs Logi Plugin Service and `PluginApi.dll`)
- .NET 10 SDK (Logi Plugin Service runs on .NET 10)
- Visual Studio 2022 or Rider with the Unreal C++ toolchain

## Install

### 1. Unreal editor plugin

1. Link (or copy) the plugin into your project's `Plugins` folder:
   ```powershell
   New-Item -ItemType Directory -Force "<Project>\Plugins"
   cmd /c mklink /J "<Project>\Plugins\MXBridge" "<this repo>\UnrealPlugin\MXBridge"
   ```
2. Close the editor, then build the project (Rider/VS, or open the `.uproject` and accept the rebuild prompt).
3. Open the editor. The Output Log should show:
   `LogMXBridge: MXBridge listening on 127.0.0.1:495xx`

### 2. Logi plugin

```powershell
cd LogiPlugin\src\UnrealMxBridgePlugin
dotnet build -c Debug
```

The build writes `%LOCALAPPDATA%\Logi\LogiPluginService\Plugins\UnrealMxBridgePlugin.link` pointing
at `LogiPlugin\bin\Debug\` and asks Logi Plugin Service to reload. Nothing else to install.

### 3. Assign actions in Logi Options+

The plugin ships a default Actions Ring layout (`package/profiles/DefaultProfile72.lp5`) that
Logi applies when it first creates the **Unreal Engine** app profile: Focus Selected, Play, Save All,
Live Coding, Grid Snap, Content Browser, Browse to Asset, Stop.

To customize: Logi Options+ → MX Master 4 → **Unreal Engine** profile → **Actions Ring**. All 21
actions are under **Unreal Engine**, grouped as Play, Build, Transform, Viewport and Navigate.

> If the ring is blank in Unreal, an empty profile was created before the default existed.
> Delete `%LOCALAPPDATA%\Logi\LogiPluginService\Applications\Loupedeck72\@_unrealmxbridge`
> and rebuild/reload the plugin, or drag actions onto the ring in Options+.

The plugin's own icons show state: **white** = normal, **blue** = currently active (e.g. Rotate mode,
grid snap on, Lit view), **grey with a red slash** = editor not connected. Icons customized in the
Options+ icon editor replace these. The shipped ring uses custom, color-coded icons, mostly from the
Options+ icon marketplace; see [docs/ring-profile.md](docs/ring-profile.md).

## Actions

| Group | Actions (event id) |
|---|---|
| Play | Play `pie.play`, Simulate `pie.simulate`, Stop `pie.stop` |
| Build | Live Coding `build.livecoding`, Compile Blueprints `build.blueprint_compile`, Save All `file.save_all`, Build Lighting `build.lighting` |
| Transform | Move / Rotate / Scale `transform.translate/rotate/scale`, World/Local `transform.toggle_space`, Grid Snap `snap.toggle_grid`, Snap to Floor `actor.snap_to_floor` |
| Transform (dials) | Nudge X / Y / Z `transform.nudge_x/y/z`: moves the selection one grid step per detent, one undo step per dial event, `subtle_collision` tick per dial event while grid snap is on |
| Viewport | Focus Selected `viewport.focus_selected`, Game View `viewport.game_view`, Lit / Unlit / Wireframe `viewport.lit/unlit/wireframe`, Bookmark 1 `viewport.bookmark_1`, Isolate Selected `viewport.isolate_selected` |
| Navigate | Content Browser `nav.content_browser`, Browse to Asset `nav.sync_browser`, Open Asset `nav.open_asset`, Output Log `nav.output_log` |

## Viewport navigation with the thumb wheel

Unreal ignores the horizontal mouse wheel. MXBridge hooks it so the MX Master thumb wheel pans the
perspective viewport under the cursor (no Logi configuration needed; leave the thumb wheel on its
default horizontal scroll):

| Movement | Mouse |
|---|---|
| Forward / back | Vertical wheel (built into Unreal) |
| Left / right | Thumb wheel |
| Up / down | Shift + thumb wheel |

Pan distance scales with the viewport camera speed (right-click + vertical wheel changes it).
Disabled while playing in the viewport; orthographic views are not affected.

## Haptics

| Editor event | Waveform |
|---|---|
| PIE / Simulate starts | `sharp_state_change` |
| PIE / Simulate ends | `damp_state_change` |
| Live Coding patch applied | `completed` |
| Live Coding ended without a patch (error, or no changes) | `mad` |
| Blueprint compiled with errors | `mad` |
| Package saved (coalesced, max once per second) | `subtle_collision` (lightest) |
| Lighting build succeeded / failed | `happy_alert` / `mad` |

## Protocol

On startup the editor writes `<Project>/Saved/MXBridge/bridge.token`:

```json
{ "schema": 1, "port": 49518, "token": "<64 hex>", "pid": 42244,
  "project": ".../GodTree.uproject", "engine": "5.6.1", "started_utc": "..." }
```

The C# plugin finds it by scanning the recent-projects list of every installed engine version
(`%LOCALAPPDATA%\UnrealEngine\<ver>\Saved\Config\WindowsEditor\EditorSettings.ini`), plus any folders
in the `MXBRIDGE_PROJECTS` environment variable. A token is only used if `pid` is a running
`UnrealEditor` process, so files left behind by a crash are ignored.

Every request needs the header `X-MXBridge-Token: <token>`, or it gets `401`.

| Endpoint | Description |
|---|---|
| `GET /context` | `{schema, seq, context: {is_pie_running, is_simulating, selection:{count, class}, transform:{mode, space}, grid:{enabled, size}, viewport:{mode, game_view}}}` |
| `POST /events` | Body `{schema:1, events:[{id:"pie.play", kind:"trigger"}]}` → `202 {accepted, rejected:[ids]}`. Commands run on the game thread. |
| `GET /haptics?since=<seq>&wait=<ms>` | Long-poll (wait ≤ 5000 ms). Returns `{seq, events:[{seq, waveform, source}]}`. `since=-1` returns only the current cursor. |

## Troubleshooting

- **Actions show the red slash:** the editor isn't reachable. Check the Output Log for
  `MXBridge listening`, and that the project appears in the editor's recent projects list.
- **Logi plugin log:** `%LOCALAPPDATA%\Logi\LogiPluginService\Logs\plugin_logs\UnrealMxBridge.log`.
  It records connect/disconnect and every `Haptic: <waveform>` fired.
- **Test without the mouse:**
  ```powershell
  .\scripts\test-bridge.ps1 -Project "<Project dir>" -Command transform.rotate
  ```

## Known limitations

- Live Coding has no failure delegate, so "no changes" compiles also report `mad`.
- Grid-snap ticks while dragging the gizmo (`subtle_collision`) are not implemented yet.
- Dial adjustments beyond Nudge X/Y/Z (rotation, grid size, timeline scrubbing) are future work.

## References

Architecture modeled on [Godot-MXConsoleAddon](https://github.com/NahuelBigu/Godot-MXConsoleAddon)
and [Godot-LogiActionsPlugin](https://github.com/NahuelBigu/Godot-LogiActionsPlugin); haptics
usage informed by [HapticWebPlugin](https://github.com/Fallstop/HapticWebPlugin).
