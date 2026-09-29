namespace Loupedeck.UnrealMxBridge;

/// <summary>Parsed GET /context payload. Defaults describe "editor not reachable".</summary>
internal sealed record ContextSnapshot
{
    public bool IsPieRunning { get; init; }
    public bool IsSimulating { get; init; }
    public int SelectionCount { get; init; }
    public string SelectionClass { get; init; } = "";
    public string TransformMode { get; init; } = "";
    public string TransformSpace { get; init; } = "";
    public bool GridEnabled { get; init; }
    public double GridSize { get; init; }
    public string ViewMode { get; init; } = "";
    public bool GameView { get; init; }

    public static ContextSnapshot Parse(JsonElement context)
    {
        var selection = context.GetProperty("selection");
        var transform = context.GetProperty("transform");
        var grid = context.GetProperty("grid");
        var viewport = context.GetProperty("viewport");

        return new ContextSnapshot
        {
            IsPieRunning = context.GetProperty("is_pie_running").GetBoolean(),
            IsSimulating = context.GetProperty("is_simulating").GetBoolean(),
            SelectionCount = selection.GetProperty("count").GetInt32(),
            SelectionClass = selection.GetProperty("class").GetString() ?? "",
            TransformMode = transform.GetProperty("mode").GetString() ?? "",
            TransformSpace = transform.GetProperty("space").GetString() ?? "",
            GridEnabled = grid.GetProperty("enabled").GetBoolean(),
            GridSize = grid.GetProperty("size").GetDouble(),
            ViewMode = viewport.GetProperty("mode").GetString() ?? "",
            GameView = viewport.GetProperty("game_view").GetBoolean(),
        };
    }
}
