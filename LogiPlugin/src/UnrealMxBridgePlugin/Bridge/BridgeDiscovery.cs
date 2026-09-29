using System.Diagnostics;
using System.Text.RegularExpressions;

namespace Loupedeck.UnrealMxBridge;

internal sealed record BridgeEndpoint(int Port, string Token, int Pid, string Project)
{
    public Uri BaseUri { get; } = new($"http://127.0.0.1:{Port}/");
}

/// <summary>
/// Finds the running editor's bridge.token. Candidate projects come from each installed engine
/// version's recent-projects list (%LOCALAPPDATA%\UnrealEngine\&lt;ver&gt;\...\EditorSettings.ini),
/// plus any folders in the MXBRIDGE_PROJECTS environment variable (';'-separated).
/// A token is only accepted if its pid is a live UnrealEditor process, which filters out
/// files left behind by an editor crash.
/// </summary>
internal static partial class BridgeDiscovery
{
    [GeneratedRegex("ProjectName=\"([^\"]+)\"")]
    private static partial Regex RecentProjectRegex();

    public static BridgeEndpoint? Find()
    {
        BridgeEndpoint? best = null;
        var bestTime = DateTime.MinValue;

        foreach (var projectDir in CandidateProjectDirs())
        {
            var tokenPath = Path.Combine(projectDir, "Saved", "MXBridge", "bridge.token");
            try
            {
                if (!File.Exists(tokenPath))
                    continue;

                var endpoint = Parse(File.ReadAllText(tokenPath));
                if (endpoint == null || !IsLiveEditor(endpoint.Pid))
                    continue;

                var written = File.GetLastWriteTimeUtc(tokenPath);
                if (written > bestTime)
                {
                    best = endpoint;
                    bestTime = written;
                }
            }
            catch (Exception ex)
            {
                PluginLog.Warning($"Discovery: could not read {tokenPath}: {ex.Message}");
            }
        }

        return best;
    }

    private static IEnumerable<string> CandidateProjectDirs()
    {
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        var extra = Environment.GetEnvironmentVariable("MXBRIDGE_PROJECTS");
        if (!string.IsNullOrWhiteSpace(extra))
        {
            foreach (var dir in extra.Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                if (seen.Add(dir))
                    yield return dir;
            }
        }

        var engineRoot = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealEngine");
        if (!Directory.Exists(engineRoot))
            yield break;

        foreach (var versionDir in Directory.EnumerateDirectories(engineRoot))
        {
            var ini = Path.Combine(versionDir, "Saved", "Config", "WindowsEditor", "EditorSettings.ini");
            string text;
            try
            {
                if (!File.Exists(ini))
                    continue;
                text = File.ReadAllText(ini);
            }
            catch
            {
                continue;
            }

            foreach (Match m in RecentProjectRegex().Matches(text))
            {
                var dir = Path.GetDirectoryName(m.Groups[1].Value.Replace('/', Path.DirectorySeparatorChar));
                if (!string.IsNullOrEmpty(dir) && seen.Add(dir))
                    yield return dir;
            }
        }
    }

    private static BridgeEndpoint? Parse(string json)
    {
        using var doc = JsonDocument.Parse(json);
        var root = doc.RootElement;
        if (!root.TryGetProperty("port", out var port) || !root.TryGetProperty("token", out var token))
            return null;

        var pid = root.TryGetProperty("pid", out var p) ? p.GetInt32() : 0;
        var project = root.TryGetProperty("project", out var pr) ? pr.GetString() ?? "" : "";
        return new BridgeEndpoint(port.GetInt32(), token.GetString() ?? "", pid, project);
    }

    private static bool IsLiveEditor(int pid)
    {
        try
        {
            using var process = Process.GetProcessById(pid);
            return process.ProcessName.StartsWith("UnrealEditor", StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false; // no such process
        }
    }
}
