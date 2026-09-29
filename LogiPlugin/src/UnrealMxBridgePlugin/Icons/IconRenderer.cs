using System.Collections.Concurrent;
using System.Text;

namespace Loupedeck.UnrealMxBridge;

internal enum IconState
{
    Normal,
    /// <summary>The thing this action controls is currently on/selected (e.g. Rotate mode, grid snap on).</summary>
    Active,
    /// <summary>Editor not reachable: greyed out with a slash (same grey, so the icon stays monochrome).</summary>
    Offline,
}

/// <summary>
/// Builds action icons from small inline SVG glyphs (80x80 viewBox, stroke-based, "C" = color).
/// Rendering at runtime lets one glyph produce the normal / active / offline variants.
/// </summary>
internal static class IconRenderer
{
    private const string NormalColor = "#FFFFFF";
    private const string ActiveColor = "#2FA8FF";
    private const string OfflineColor = "#5A5A5A";
    // Every variant must stay monochrome: Options+ can only tint (recolor) single-color SVGs, and
    // the icon editor captures whatever image is showing when the user customizes an action.
    private const string OfflineSlash = "<path d='M14 66 L66 14' stroke='" + OfflineColor + "' stroke-width='7' stroke-linecap='round'/>";

    private static readonly ConcurrentDictionary<(string, IconState), BitmapImage> Cache = new();

    public static BitmapImage Render(string glyph, IconState state) =>
        Cache.GetOrAdd((glyph, state), static key =>
        {
            var (glyph, state) = key;
            var color = state switch
            {
                IconState.Active => ActiveColor,
                IconState.Offline => OfflineColor,
                _ => NormalColor,
            };
            var svg =
                "<svg xmlns='http://www.w3.org/2000/svg' width='80' height='80' viewBox='0 0 80 80'>" +
                $"<g fill='none' stroke='{color}' stroke-width='6' stroke-linecap='round' stroke-linejoin='round'>" +
                glyph.Replace("'C'", $"'{color}'") +
                "</g>" +
                (state == IconState.Offline ? OfflineSlash : "") +
                "</svg>";
            return BitmapImage.FromArray(Encoding.UTF8.GetBytes(svg));
        });
}

/// <summary>Glyph path data. Keep shapes simple; they render at ~40px on the Actions Ring.</summary>
internal static class Glyphs
{
    public const string Play = "<path d='M28 18 L62 40 L28 62 Z' fill='C'/>";
    public const string Simulate = "<path d='M28 18 L62 40 L28 62 Z'/>";
    public const string Stop = "<rect x='22' y='22' width='36' height='36' rx='3' fill='C'/>";

    public const string LiveCoding = "<path d='M46 10 L22 44 H38 L34 70 L58 34 H42 Z' fill='C' stroke-width='3'/>";
    public const string Blueprint = "<rect x='10' y='18' width='22' height='16' rx='3'/><rect x='48' y='46' width='22' height='16' rx='3'/><path d='M32 26 C44 26 36 54 48 54'/>";
    public const string Save = "<path d='M16 14 H54 L66 26 V66 H16 Z'/><path d='M28 14 V28 H50 V14'/><rect x='26' y='44' width='30' height='22'/>";
    public const string Lighting = "<circle cx='40' cy='32' r='17'/><path d='M32 52 V58 H48 V52 M34 66 H46'/>";

    public const string Translate = "<path d='M40 10 V70 M10 40 H70 M40 10 L32 18 M40 10 L48 18 M40 70 L32 62 M40 70 L48 62 M10 40 L18 32 M10 40 L18 48 M70 40 L62 32 M70 40 L62 48'/>";
    public const string Rotate = "<path d='M62 40 A22 22 0 1 1 52 21'/><path d='M40 14 L53 20 L47 33'/>";
    public const string Scale = "<rect x='14' y='38' width='28' height='28'/><path d='M36 44 L64 16 M46 16 H64 V34'/>";
    public const string Space = "<circle cx='40' cy='40' r='26'/><ellipse cx='40' cy='40' rx='11' ry='26'/><path d='M14 40 H66'/>";
    public const string SnapToFloor = "<rect x='26' y='10' width='28' height='24' rx='2'/><path d='M40 40 V56 M32 48 L40 56 L48 48 M12 68 H68'/>";
    public const string NudgeX = "<path d='M10 40 H70 M10 40 L20 30 M10 40 L20 50 M70 40 L60 30 M70 40 L60 50'/>";
    public const string NudgeY = "<path d='M16 64 L64 16 M64 16 H48 M64 16 V32 M16 64 H32 M16 64 V48'/>";
    public const string NudgeZ = "<path d='M40 10 V70 M40 10 L30 20 M40 10 L50 20 M40 70 L30 60 M40 70 L50 60'/>";
    public const string Grid = "<path d='M14 14 V66 M31 14 V66 M49 14 V66 M66 14 V66 M14 14 H66 M14 31 H66 M14 49 H66 M14 66 H66' stroke-width='4'/>";

    public const string Focus = "<circle cx='40' cy='40' r='18'/><circle cx='40' cy='40' r='4' fill='C'/><path d='M40 10 V22 M40 58 V70 M10 40 H22 M58 40 H70'/>";
    public const string GameView = "<path d='M8 40 Q40 10 72 40 Q40 70 8 40 Z'/><circle cx='40' cy='40' r='9' fill='C'/>";
    public const string Bookmark = "<path d='M24 10 H56 V70 L40 56 L24 70 Z'/>";
    public const string Isolate = "<circle cx='40' cy='40' r='11' fill='C'/><circle cx='40' cy='40' r='28' stroke-dasharray='9 8'/>";
    public const string Lit = "<circle cx='40' cy='40' r='12' fill='C'/><path d='M40 10 V19 M40 61 V70 M10 40 H19 M61 40 H70 M19 19 L25 25 M55 55 L61 61 M61 19 L55 25 M25 55 L19 61'/>";
    public const string Unlit = "<circle cx='40' cy='40' r='22'/><path d='M40 18 A22 22 0 0 1 40 62 Z' fill='C'/>";
    public const string Wireframe = "<path d='M18 26 L40 14 L62 26 V54 L40 66 L18 54 Z M18 26 L40 38 L62 26 M40 38 V66'/>";

    public const string ContentBrowser = "<path d='M10 20 H32 L38 27 H70 V62 H10 Z'/>";
    public const string SyncBrowser = "<path d='M10 22 H28 L33 28 H56 V58 H10 Z'/><path d='M50 48 L68 66 M68 52 V66 H54'/>";
    public const string OpenAsset = "<circle cx='34' cy='34' r='18'/><path d='M47 47 L66 66'/>";
    public const string OutputLog = "<rect x='12' y='14' width='56' height='52' rx='4'/><path d='M22 30 L31 39 L22 48 M38 50 H56'/>";
}
