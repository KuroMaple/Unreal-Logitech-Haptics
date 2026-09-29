# Actions Ring profile

The default Actions Ring layout shipped with the Logi plugin
(`LogiPlugin/src/UnrealMxBridgePlugin/package/profiles/DefaultProfile72.lp5`) is an export of a
hand-built profile, customized in the Logi Options+ icon editor.

**Icons:** most icons come from the **Logi Options+ icon marketplace** (the icon library in the
Options+ icon editor), not from this plugin. The rest are the plugin's own glyphs, recolored in the
icon editor. Custom icons are saved per action as `.ict` files inside the profile and replace the
plugin's runtime icons, so the plugin's "active" / "not connected" icon states don't show for
these slots.

All tiles use background `#1A1A1A`.

| Slot | Action | Icon | Source | Icon color | Text color |
|---|---|---|---|---|---|
| 1 | Focus Selected | `SmartZoom.svg` | Icon marketplace | `#B78CFF` violet | `#B78CFF` |
| 2 | Play | `Play.svg` | Icon marketplace | `#3DDC84` green | `#FFFFFF` |
| 3 | Save All | floppy disk | Plugin glyph | `#FFB020` amber | `#FFFFFF` |
| 4 | Live Coding | lightning bolt | Plugin glyph | `#F9AB00` amber | `#FFFFFF` |
| 5 | Grid Snap | grid | Plugin glyph | `#1A73E8` blue | `#FFFFFF` |
| 6 | Content Browser | `folder-blank.svg` | Icon marketplace | `#00897B` teal | `#FFFFFF` |
| 7 | Browse to Asset | `OpenFolder.svg` | Icon marketplace | `#00897B` teal | `#FFFFFF` |
| 8 | Stop | `MediaStop.svg` | Icon marketplace | `#D93025` red | `#D93025` |

Color groups: Play = green, Stop = red, Build = amber, Transform = blue, Viewport = violet,
Navigate = teal.

## Updating the shipped profile

After changing the ring in Options+, re-export it: zip the contents of
`%LOCALAPPDATA%\Logi\LogiPluginService\Applications\Loupedeck72\@_unrealmxbridge\Profiles\<id>\`
together with that app folder's `ApplicationInfo.json` (with `defaultProfileName` set to `<id>`)
into `DefaultProfile72.lp5`, using `/` path separators.

> Before distributing the plugin publicly, confirm the icon marketplace's license allows the
> marketplace icons to be redistributed inside a plugin's default profile.
