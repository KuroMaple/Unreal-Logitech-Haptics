namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// MX Master 4 haptic waveforms. Each is registered as a plugin event of the same name and
/// mapped 1:1 in package/events/extra/eventMapping.yaml.
/// </summary>
internal static class Waveforms
{
    public static readonly IReadOnlyList<string> All =
    [
        "sharp_collision", "sharp_state_change", "knock", "damp_collision", "mad",
        "ringing", "subtle_collision", "completed", "jingle", "damp_state_change",
        "firework", "happy_alert", "wave", "angry_alert", "square",
    ];

    private static readonly HashSet<string> Known = new(All, StringComparer.Ordinal);

    public static bool IsKnown(string name) => Known.Contains(name);
}
