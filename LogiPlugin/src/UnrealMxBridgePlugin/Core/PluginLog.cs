namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Global plugin logger. Initialized once by <see cref="UnrealMxBridgePlugin"/>
/// so any class can log without a direct reference to the plugin instance.
/// </summary>
internal static class PluginLog
{
    private static PluginLogFile? _file;

    public static void Init(PluginLogFile pluginLogFile)
    {
        ArgumentNullException.ThrowIfNull(pluginLogFile);
        _file = pluginLogFile;
    }

    public static void Info(string text)    => _file?.Info(text);
    public static void Warning(string text) => _file?.Warning(text);
    public static void Error(string text)   => _file?.Error(text);
}
