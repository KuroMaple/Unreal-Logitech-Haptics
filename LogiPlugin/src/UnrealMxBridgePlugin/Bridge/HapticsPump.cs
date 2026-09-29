namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Long-polls GET /haptics and plays each returned waveform on the MX Master 4.
///
/// The editor keeps a sequence cursor; we pass the last seq we saw as ?since=. On (re)connect we
/// send since=-1, which only syncs the cursor, so old events from before we connected don't all
/// fire at once.
/// </summary>
internal sealed class HapticsPump
{
    private const int WaitMs = 4000;

    private readonly BridgeClient _bridge;
    private readonly Action<string> _raise;
    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromMilliseconds(WaitMs + 3000) };
    private CancellationTokenSource? _cts;

    public HapticsPump(BridgeClient bridge, Action<string> raise)
    {
        _bridge = bridge;
        _raise = raise;
    }

    public void Start()
    {
        if (_cts != null)
            return;
        _cts = new CancellationTokenSource();
        var token = _cts.Token;
        _ = Task.Run(() => this.RunAsync(token), token);
    }

    public void Stop()
    {
        _cts?.Cancel();
        _cts?.Dispose();
        _cts = null;
    }

    private async Task RunAsync(CancellationToken ct)
    {
        BridgeEndpoint? current = null;
        long since = -1;

        while (!ct.IsCancellationRequested)
        {
            var endpoint = _bridge.Endpoint;
            if (endpoint == null || !_bridge.IsConnected)
            {
                current = null;
                await Delay(1000, ct).ConfigureAwait(false);
                continue;
            }

            if (!ReferenceEquals(endpoint, current))
            {
                current = endpoint;
                since = -1;
            }

            try
            {
                var uri = new Uri(endpoint.BaseUri, $"haptics?since={since}&wait={(since < 0 ? 0 : WaitMs)}");
                using var request = new HttpRequestMessage(HttpMethod.Get, uri);
                request.Headers.Add(BridgeClient.TokenHeader, endpoint.Token);
                using var response = await _http.SendAsync(request, ct).ConfigureAwait(false);
                if (!response.IsSuccessStatusCode)
                {
                    await Delay(1000, ct).ConfigureAwait(false);
                    continue;
                }

                var text = await response.Content.ReadAsStringAsync(ct).ConfigureAwait(false);
                using var doc = JsonDocument.Parse(text);
                var root = doc.RootElement;
                var seq = root.GetProperty("seq").GetInt64();

                if (since >= 0)
                {
                    foreach (var evt in root.GetProperty("events").EnumerateArray())
                    {
                        var waveform = evt.GetProperty("waveform").GetString() ?? "";
                        if (Waveforms.IsKnown(waveform))
                            _raise(waveform);
                        else
                            PluginLog.Warning($"Unknown waveform '{waveform}' from editor");
                    }
                }

                since = seq;
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                PluginLog.Warning($"Haptics poll failed: {ex.Message}");
                await Delay(1000, ct).ConfigureAwait(false);
            }
        }
    }

    private static async Task Delay(int ms, CancellationToken ct)
    {
        try
        {
            await Task.Delay(ms, ct).ConfigureAwait(false);
        }
        catch (OperationCanceledException)
        {
            /* shutting down */
        }
    }
}
