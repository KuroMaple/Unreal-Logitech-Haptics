using System.Net.Http.Headers;
using System.Text;

namespace Loupedeck.UnrealMxBridge;

/// <summary>
/// Talks to the MXBridge editor plugin: discovers the endpoint, polls GET /context on a timer,
/// and posts ring commands to POST /events.
/// </summary>
internal sealed class BridgeClient
{
    public const string TokenHeader = "X-MXBridge-Token";

    /// <summary>First poll is delayed so nothing blocks Logi Plugin Service while it loads.</summary>
    private const int InitialDelayMs = 2000;
    private const int PollPeriodMs = 400;

    private readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(2) };
    private readonly object _lock = new();
    private System.Threading.Timer? _timer;
    private int _pollGate;
    private string? _lastContextRaw;
    private bool _reachable;

    public event Action<bool>? ReachableChanged;

    /// <summary>Raised (on a pool thread) when the editor state changes or connection flips.</summary>
    public event Action? ContextChanged;

    public BridgeEndpoint? Endpoint { get; private set; }

    public ContextSnapshot Snapshot { get; private set; } = new();

    public bool IsConnected => _reachable;

    public void Start()
    {
        _timer ??= new System.Threading.Timer(_ => this.PollSafe(), null, InitialDelayMs, PollPeriodMs);
    }

    public void Stop()
    {
        _timer?.Dispose();
        _timer = null;
    }

    /// <summary>
    /// Posts one event. With <paramref name="relativeValue"/> it is a dial step
    /// (kind "set_float", relative) instead of a button trigger.
    /// </summary>
    public void Send(string eventId, double? relativeValue = null)
    {
        var endpoint = this.Endpoint;
        if (endpoint == null)
        {
            PluginLog.Warning($"'{eventId}' ignored: Unreal Editor not connected.");
            return;
        }

        // Fire-and-forget so the ring press returns immediately.
        _ = Task.Run(async () =>
        {
            try
            {
                var evt = new Dictionary<string, object> { ["id"] = eventId, ["kind"] = "trigger" };
                if (relativeValue is { } value)
                {
                    evt["kind"] = "set_float";
                    evt["value"] = value;
                    evt["relative"] = true;
                }
                var body = JsonSerializer.Serialize(new { schema = 1, events = new[] { evt } });
                using var request = new HttpRequestMessage(HttpMethod.Post, new Uri(endpoint.BaseUri, "events"))
                {
                    Content = new StringContent(body, Encoding.UTF8, "application/json"),
                };
                request.Headers.Add(TokenHeader, endpoint.Token);
                using var response = await _http.SendAsync(request).ConfigureAwait(false);
                if (!response.IsSuccessStatusCode)
                    PluginLog.Warning($"POST /events '{eventId}' -> {(int)response.StatusCode}");

                // Commands like transform.rotate change context; refresh icons promptly.
                await Task.Delay(100).ConfigureAwait(false);
                this.PollSafe();
            }
            catch (Exception ex)
            {
                PluginLog.Warning($"POST /events '{eventId}' failed: {ex.Message}");
            }
        });
    }

    private void PollSafe()
    {
        // Skip if the previous poll is still running (timer callbacks can overlap).
        if (Interlocked.CompareExchange(ref _pollGate, 1, 0) != 0)
            return;
        try
        {
            this.Poll();
        }
        catch (Exception ex)
        {
            PluginLog.Warning($"Bridge poll failed: {ex.Message}");
        }
        finally
        {
            Interlocked.Exchange(ref _pollGate, 0);
        }
    }

    private void Poll()
    {
        this.Endpoint ??= BridgeDiscovery.Find();
        var endpoint = this.Endpoint;
        if (endpoint == null)
        {
            this.SetDisconnected();
            return;
        }

        string text;
        try
        {
            using var request = new HttpRequestMessage(HttpMethod.Get, new Uri(endpoint.BaseUri, "context"));
            request.Headers.Add(TokenHeader, endpoint.Token);
            using var response = _http.Send(request);
            if (!response.IsSuccessStatusCode)
            {
                // 401 = stale token from an earlier session; rediscover next tick.
                this.Endpoint = null;
                this.SetDisconnected();
                return;
            }
            text = response.Content.ReadAsStringAsync().GetAwaiter().GetResult();
        }
        catch
        {
            this.Endpoint = null;
            this.SetDisconnected();
            return;
        }

        using var doc = JsonDocument.Parse(text);
        var context = doc.RootElement.GetProperty("context");
        var raw = context.GetRawText(); // excludes "seq", which changes on every request

        bool changed;
        lock (_lock)
        {
            changed = raw != _lastContextRaw;
            if (changed)
            {
                _lastContextRaw = raw;
                this.Snapshot = ContextSnapshot.Parse(context);
            }
        }

        this.SetReachable(true);
        if (changed)
            this.ContextChanged?.Invoke();
    }

    private void SetDisconnected()
    {
        lock (_lock)
        {
            _lastContextRaw = null;
            this.Snapshot = new ContextSnapshot();
        }
        this.SetReachable(false);
    }

    private void SetReachable(bool reachable)
    {
        if (_reachable == reachable)
            return;
        _reachable = reachable;
        PluginLog.Info(reachable
            ? $"Connected to Unreal Editor ({this.Endpoint?.Project}, port {this.Endpoint?.Port})"
            : "Unreal Editor disconnected");
        this.ReachableChanged?.Invoke(reachable);
        this.ContextChanged?.Invoke();
    }
}
