namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Links this plugin to the Unreal Editor process so Logi Options+ switches to the
/// Unreal profile when the editor is focused.
/// </summary>
public class UnrealMxBridgeApplication : ClientApplication
{
    protected override string GetProcessName() => "UnrealEditor";

    // Also matches debug/dev editor builds, e.g. UnrealEditor-Win64-DebugGame.
    protected override bool IsProcessNameSupported(string processName) =>
        processName.StartsWith("UnrealEditor", StringComparison.OrdinalIgnoreCase);

    protected override string GetBundleName() => "com.epicgames.UnrealEditor";
}
