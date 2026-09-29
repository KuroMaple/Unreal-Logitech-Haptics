namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Plugin entry point. Owns the bridge client (context polling + commands) and the haptics pump.
/// Commands reach the client through the static <see cref="Bridge"/> because the SDK creates
/// command instances by reflection, so constructor injection isn't possible.
/// </summary>
public class UnrealMxBridgePlugin : Plugin
{
    private const string HelpUrl = "https://github.com/KuroMaple/Unreal-Logitech-Haptics#readme";

    /// <summary>Actions run regardless of the foreground app; the profile still follows the editor.</summary>
    public override bool UsesApplicationApiOnly => true;

    /// <summary>false = linked to <see cref="UnrealMxBridgeApplication"/>.</summary>
    public override bool HasNoApplication => false;

    internal static BridgeClient Bridge { get; } = new();

    private HapticsPump? _haptics;

    public UnrealMxBridgePlugin()
    {
        PluginLog.Init(this.Log);
    }

    public override void Load()
    {
        // Haptic events (one per waveform) are declared in package/events/DefaultEventSource.yaml.
        Bridge.ReachableChanged += this.OnReachableChanged;
        this.OnReachableChanged(false);

        // Start() only arms timers; no HTTP happens on the Load path (it would block Logi Plugin Service).
        Bridge.Start();
        _haptics = new HapticsPump(Bridge, this.RaiseHaptic);
        _haptics.Start();

        PluginLog.Info("UnrealMxBridge loaded.");
    }

    public override void Unload()
    {
        _haptics?.Stop();
        _haptics = null;
        Bridge.Stop();
        Bridge.ReachableChanged -= this.OnReachableChanged;
    }

    private void RaiseHaptic(string waveform)
    {
        try
        {
            this.PluginEvents.RaiseEvent(waveform);
            PluginLog.Info($"Haptic: {waveform}");
        }
        catch (Exception ex)
        {
            PluginLog.Warning($"Haptic '{waveform}' failed: {ex.Message}");
        }
    }

    private void OnReachableChanged(bool reachable)
    {
        if (reachable)
        {
            this.OnPluginStatusChanged(global::Loupedeck.PluginStatus.Normal, null);
        }
        else
        {
            this.OnPluginStatusChanged(
                global::Loupedeck.PluginStatus.Error,
                "Unreal Editor not connected. Open a project with the MXBridge plugin enabled.",
                HelpUrl,
                "Setup help");
        }
    }
}
