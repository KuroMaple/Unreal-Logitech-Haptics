# Actions Ring profile

The default Actions Ring layout shipped with the Logi plugin
(`LogiPlugin/src/UnrealMxBridgePlugin/package/profiles/DefaultProfile72.lp5`) is an export of a
hand-built profile, customized in the Logi Options+ icon editor.

## Design rule

The top ring holds high-frequency actions that act on **what the mouse just selected**. Everything
else lives in folders. Actions that are one easy key press (Stop = Esc, Move/Rotate/Scale = W/E/R)
are not placed.

## Layout

```
TOP RING
 1  Focus Selected           violet  #B78CFF
 2  Play                     green   #3DDC84
 3  ▸ View (folder)          violet  #B78CFF   eye icon
 4  Live Coding              amber   #F9AB00
 5  Snap to Floor            blue    #1A73E8
 6  ▸ Nudge (folder)         blue    #1A73E8   move-arrows icon
 7  ▸ Navigate (folder)      teal    #00897B   compass icon
 8  Isolate Selected         violet  #B78CFF

▸ View       Bookmark 1 · Lit · Unlit · Wireframe · Game View · Simulate
▸ Nudge      Grid Snap (press) · Nudge Z · Nudge X · Nudge Y (dials)
▸ Navigate   Open Asset · Content Browser · Browse to Asset · Output Log
```

Tiles use background `#1A1A1A` (the View folder uses `#000000`) with white text, except where
noted in the icon editor.

## Colors by group

| Group | Icon color | Actions |
|---|---|---|
| Play | `#3DDC84` green | Play, Simulate |
| Stop | `#D93025` red | Stop |
| Build | `#F9AB00` amber | Live Coding, Compile Blueprints, Build Lighting, Save All |
| Transform | `#1A73E8` blue | Move, Rotate, Scale, World/Local, Grid Snap, Snap to Floor |
| Nudge dials | X `#E5484D`, Y `#3DDC84`, Z `#1A73E8` (gizmo axis colors) | Nudge X / Y / Z |
| Viewport | `#B78CFF` violet | Focus Selected, Isolate Selected, Bookmark 1, Game View, Lit, Unlit, Wireframe |
| Navigate | `#00897B` teal | Content Browser, Browse to Asset, Open Asset, Output Log |

## Icon sources

- **Icon marketplace:** Focus Selected (`SmartZoom.svg`), Play (`Play.svg`), Stop (`MediaStop.svg`),
  Content Browser (`folder-blank.svg`), Browse to Asset (`OpenFolder.svg`). These come from the
  icon library in the Options+ icon editor.
- **Plugin glyphs, recolored in the icon editor:** everything else, including the three folder
  icons (eye, move arrows, compass).

Custom icons are saved per action as `.ict` files inside the profile. They replace the plugin's
runtime icons, so the plugin's "active" / "not connected" states don't show on customized slots.

**Recoloring:** Options+ can only tint a **monochrome** SVG. All plugin icons are single-color
(including the "not connected" state). If an icon won't take a color, it was probably captured
from an old two-color icon: reset it in the icon editor while Unreal is connected, then recolor it.

## Applying the colors

`scripts/apply-ring-colors.ps1` applies this scheme to every action on your ring: marketplace
icons keep their image and get the group tint, plugin-glyph icons are re-rendered as clean
monochrome SVGs. It restarts Logi Plugin Service while it writes.

## Updating the shipped profile

After changing the ring in Options+, re-export it: zip the contents of
`%LOCALAPPDATA%\Logi\LogiPluginService\Applications\Loupedeck72\@_unrealmxbridge\Profiles\<id>\`
together with that app folder's `ApplicationInfo.json` (with `defaultProfileName` set to `<id>`)
into `DefaultProfile72.lp5`, using `/` path separators. Stage it in a short path (e.g. `C:\tmp`):
the icon file names are long enough to hit the Windows 260-character path limit.

> Before distributing the plugin publicly, confirm the icon marketplace's license allows the
> marketplace icons to be redistributed inside a plugin's default profile.
