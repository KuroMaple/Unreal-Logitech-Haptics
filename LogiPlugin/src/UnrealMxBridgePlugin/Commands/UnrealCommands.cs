namespace Loupedeck.UnrealMxBridge;

// One class per Actions Ring action. Event ids must match the registry in
// UnrealPlugin/MXBridge/Source/MXBridge/Private/MXBridgeCommands.cpp.

// ── Play ────────────────────────────────────────────────────────────────────

public sealed class PiePlayCommand() : UnrealCommand(
    "pie.play", "Play", "Play in the active viewport", "Play", Glyphs.Play,
    s => s.IsPieRunning && !s.IsSimulating);

public sealed class PieSimulateCommand() : UnrealCommand(
    "pie.simulate", "Simulate", "Simulate in the active viewport", "Play", Glyphs.Simulate,
    s => s.IsSimulating);

public sealed class PieStopCommand() : UnrealCommand(
    "pie.stop", "Stop", "Stop Play / Simulate", "Play", Glyphs.Stop);

// ── Build ───────────────────────────────────────────────────────────────────

public sealed class LiveCodingCommand() : UnrealCommand(
    "build.livecoding", "Live Coding", "Compile C++ with Live Coding", "Build", Glyphs.LiveCoding);

public sealed class CompileBlueprintsCommand() : UnrealCommand(
    "build.blueprint_compile", "Compile Blueprints", "Compile modified Blueprints", "Build", Glyphs.Blueprint);

public sealed class SaveAllCommand() : UnrealCommand(
    "file.save_all", "Save All", "Save all modified assets and levels", "Build", Glyphs.Save);

public sealed class BuildLightingCommand() : UnrealCommand(
    "build.lighting", "Build Lighting", "Build lighting only", "Build", Glyphs.Lighting);

// ── Transform ───────────────────────────────────────────────────────────────

public sealed class TranslateModeCommand() : UnrealCommand(
    "transform.translate", "Move", "Translate gizmo (W)", "Transform", Glyphs.Translate,
    s => s.TransformMode == "translate");

public sealed class RotateModeCommand() : UnrealCommand(
    "transform.rotate", "Rotate", "Rotate gizmo (E)", "Transform", Glyphs.Rotate,
    s => s.TransformMode == "rotate");

public sealed class ScaleModeCommand() : UnrealCommand(
    "transform.scale", "Scale", "Scale gizmo (R)", "Transform", Glyphs.Scale,
    s => s.TransformMode == "scale");

public sealed class ToggleSpaceCommand() : UnrealCommand(
    "transform.toggle_space", "World / Local", "Toggle world/local gizmo space (highlighted = local)", "Transform", Glyphs.Space,
    s => s.TransformSpace == "local");

public sealed class ToggleGridSnapCommand() : UnrealCommand(
    "snap.toggle_grid", "Grid Snap", "Toggle location grid snapping", "Transform", Glyphs.Grid,
    s => s.GridEnabled);

// ── Viewport ────────────────────────────────────────────────────────────────

public sealed class FocusSelectedCommand() : UnrealCommand(
    "viewport.focus_selected", "Focus Selected", "Frame the selection (F)", "Viewport", Glyphs.Focus);

public sealed class GameViewCommand() : UnrealCommand(
    "viewport.game_view", "Game View", "Toggle game view (G)", "Viewport", Glyphs.GameView,
    s => s.GameView);

public sealed class LitViewCommand() : UnrealCommand(
    "viewport.lit", "Lit", "Lit view mode", "Viewport", Glyphs.Lit,
    s => s.ViewMode == "lit");

public sealed class UnlitViewCommand() : UnrealCommand(
    "viewport.unlit", "Unlit", "Unlit view mode", "Viewport", Glyphs.Unlit,
    s => s.ViewMode == "unlit");

public sealed class WireframeViewCommand() : UnrealCommand(
    "viewport.wireframe", "Wireframe", "Wireframe view mode", "Viewport", Glyphs.Wireframe,
    s => s.ViewMode == "wireframe");

// ── Navigate ────────────────────────────────────────────────────────────────

public sealed class ContentBrowserCommand() : UnrealCommand(
    "nav.content_browser", "Content Browser", "Open the Content Browser", "Navigate", Glyphs.ContentBrowser);

public sealed class SyncBrowserCommand() : UnrealCommand(
    "nav.sync_browser", "Browse to Asset", "Find the selection in the Content Browser (Ctrl+B)", "Navigate", Glyphs.SyncBrowser);

public sealed class OpenAssetCommand() : UnrealCommand(
    "nav.open_asset", "Open Asset", "Open Asset picker (Ctrl+P)", "Navigate", Glyphs.OpenAsset);

public sealed class OutputLogCommand() : UnrealCommand(
    "nav.output_log", "Output Log", "Open the Output Log", "Navigate", Glyphs.OutputLog);
