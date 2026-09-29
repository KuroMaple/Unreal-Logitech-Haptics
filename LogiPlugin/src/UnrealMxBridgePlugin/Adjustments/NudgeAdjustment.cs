namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Dial that moves the selected actors one grid step per detent along a world axis.
/// Each detent plays a light haptic tick locally (no round trip), and the editor applies the
/// move as one undoable transaction per dial event.
/// </summary>
public abstract class NudgeAdjustment : PluginDynamicAdjustment
{
    private const string TickWaveform = "subtle_collision";

    private readonly string _eventId;
    private readonly string _glyph;

    private protected NudgeAdjustment(string eventId, string axis, string glyph)
        : base($"Nudge {axis}", $"Move the selection along world {axis} by the grid size", "Transform", hasReset: false)
    {
        _eventId = eventId;
        _glyph = glyph;
    }

    private static BridgeClient Bridge => UnrealMxBridgePlugin.Bridge;

    protected override bool OnLoad()
    {
        Bridge.ContextChanged += this.OnContextChanged;
        return true;
    }

    protected override bool OnUnload()
    {
        Bridge.ContextChanged -= this.OnContextChanged;
        return true;
    }

    private void OnContextChanged()
    {
        this.AdjustmentValueChanged();
        this.ActionImageChanged();
    }

    protected override void ApplyAdjustment(string actionParameter, int diff)
    {
        if (diff == 0 || !Bridge.IsConnected || Bridge.Snapshot.SelectionCount == 0)
            return;

        Bridge.Send(_eventId, diff);
        try
        {
            this.Plugin.PluginEvents.RaiseEvent(TickWaveform);
        }
        catch (Exception ex)
        {
            PluginLog.Warning($"Nudge haptic failed: {ex.Message}");
        }
    }

    /// <summary>Shows the step size next to the dial, e.g. "10".</summary>
    protected override string GetAdjustmentValue(string actionParameter) =>
        Bridge.IsConnected ? $"{Bridge.Snapshot.GridSize:0.##}" : "";

    protected override BitmapImage GetAdjustmentImage(string actionParameter, PluginImageSize imageSize) =>
        IconRenderer.Render(_glyph, Bridge.IsConnected ? IconState.Normal : IconState.Offline);
}

public sealed class NudgeXAdjustment() : NudgeAdjustment("transform.nudge_x", "X", Glyphs.NudgeX);

public sealed class NudgeYAdjustment() : NudgeAdjustment("transform.nudge_y", "Y", Glyphs.NudgeY);

public sealed class NudgeZAdjustment() : NudgeAdjustment("transform.nudge_z", "Z", Glyphs.NudgeZ);
