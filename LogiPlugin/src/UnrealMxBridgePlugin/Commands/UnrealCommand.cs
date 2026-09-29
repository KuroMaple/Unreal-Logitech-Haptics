namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Base for every ring action: sends one MXBridge event id and draws a context-aware icon.
/// Subclasses are one-liners; the SDK discovers them by reflection.
/// </summary>
public abstract class UnrealCommand : PluginDynamicCommand
{
    private readonly string _eventId;
    private readonly string _glyph;
    private readonly string _color;
    private readonly Func<ContextSnapshot, bool>? _isActive;

    private protected UnrealCommand(
        string eventId,
        string displayName,
        string description,
        string group,
        string glyph,
        Func<ContextSnapshot, bool>? isActive = null,
        string? color = null)
        : base(displayName, description, group)
    {
        _eventId = eventId;
        _glyph = glyph;
        _isActive = isActive;
        _color = color ?? group switch
        {
            "Play" => GroupColors.Play,
            "Build" => GroupColors.Build,
            "Transform" => GroupColors.Transform,
            "Viewport" => GroupColors.Viewport,
            _ => GroupColors.Navigate,
        };
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

    private void OnContextChanged() => this.ActionImageChanged();

    protected override void RunCommand(string actionParameter) => Bridge.Send(_eventId);

    protected override BitmapImage GetCommandImage(string actionParameter, PluginImageSize imageSize)
    {
        var state = !Bridge.IsConnected ? IconState.Offline
            : _isActive?.Invoke(Bridge.Snapshot) == true ? IconState.Active
            : IconState.Normal;
        return IconRenderer.Render(_glyph, _color, state);
    }
}
