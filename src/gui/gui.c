#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================ THEME ============================ */

#define C_BG          (Color){ 13,  17,  25, 255}
#define C_PANEL       (Color){ 22,  28,  40, 255}
#define C_CARD        (Color){ 30,  38,  54, 255}
#define C_CARD_HOVER  (Color){ 38,  48,  67, 255}
#define C_BORDER      (Color){ 50,  62,  82, 255}
#define C_TEXT        (Color){236, 241, 248, 255}
#define C_MUTED       (Color){139, 153, 174, 255}
#define C_FAINT       (Color){ 95, 108, 128, 255}
#define C_ACCENT      (Color){ 64, 156, 255, 255}
#define C_ACCENT_DIM  (Color){ 44, 122, 214, 255}
#define C_ACCENT_DOWN (Color){ 34, 100, 180, 255}
#define C_GREEN       (Color){ 62, 201, 132, 255}
#define C_AMBER       (Color){255, 184,  48, 255}
#define C_RED         (Color){240,  98,  98, 255}
#define C_TEAL        (Color){ 32, 184, 196, 255}
#define C_ROUTE       (Color){255, 176,  32, 255}
#define C_ROUTE_CASE  (Color){ 24,  28,  38, 235}
#define C_CHIP        (Color){ 13,  17,  25, 225}
#define C_INK         (Color){ 28,  32,  42, 255}

#define FONT_PATH       "assets/fonts/DejaVuSans.ttf"
#define FONT_BOLD_PATH  "assets/fonts/DejaVuSans-Bold.ttf"
#define FONT_BASE_SIZE  48

static const char *locations[LOCATION_COUNT] = {
    "Dehradun",
    "Mussoorie",
    "Chakrata",
    "Rishikesh",
    "Haridwar",
    "Saharanpur",
    "Paonta Sahib",
    "New Tehri",
    "Roorkee"
};

/* Same order as 'locations' and BuildCityGraph() in main.c: {lat, lon}. */
static const double cityCoords[LOCATION_COUNT][2] = {
    {30.3165, 78.0322},
    {30.4598, 78.0644},
    {30.7037, 77.8625},
    {30.0869, 78.2676},
    {29.9457, 78.1642},
    {29.9640, 77.5460},
    {30.4380, 77.6240},
    {30.3753, 78.4804},
    {29.8543, 77.8880}
};

/* ============================ FONTS ============================ */

static Font uiFont;
static Font uiBold;
static bool uiFontsOwned = false;

static Font LoadUIFont(const char *path)
{
    /* ASCII plus the few symbols the UI uses. */
    static const char extra[] = "—·→⇅–…";

    int count = 0;
    int *extraCps = LoadCodepoints(extra, &count);
    int total = 95 + count;
    int *cps = malloc((size_t)total * sizeof(int));

    Font f = {0};

    if (cps) {
        for (int i = 0; i < 95; i++)
            cps[i] = 32 + i;
        for (int i = 0; i < count; i++)
            cps[95 + i] = extraCps[i];

        if (FileExists(path))
            f = LoadFontEx(path, FONT_BASE_SIZE, cps, total);

        free(cps);
    }

    UnloadCodepoints(extraCps);

    if (f.texture.id != 0) {
        GenTextureMipmaps(&f.texture);
        SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    }

    return f;
}

static void LoadUIFonts(void)
{
    uiFont = LoadUIFont(FONT_PATH);
    uiBold = LoadUIFont(FONT_BOLD_PATH);

    if (uiFont.texture.id == 0 || uiBold.texture.id == 0) {
        fprintf(stderr, "Warning: UI fonts not found in assets/fonts, "
                        "using raylib default font.\n");

        if (uiFont.texture.id != 0) UnloadFont(uiFont);
        if (uiBold.texture.id != 0) UnloadFont(uiBold);

        uiFont = GetFontDefault();
        uiBold = GetFontDefault();
        uiFontsOwned = false;
        return;
    }

    uiFontsOwned = true;
}

/* raylib's default bitmap font needs letter spacing, TTF fonts do not. */
static float Spacing(float size, float extra)
{
    return (uiFontsOwned ? 0.0f : size / 10.0f) + extra;
}

static void Text(const char *t, float x, float y, float size, Color c)
{
    DrawTextEx(uiFont, t, (Vector2){roundf(x), roundf(y)}, size,
               Spacing(size, 0), c);
}

static void TextBold(const char *t, float x, float y, float size, Color c)
{
    DrawTextEx(uiBold, t, (Vector2){roundf(x), roundf(y)}, size,
               Spacing(size, 0), c);
}

static float TextWidth(const char *t, float size, bool bold)
{
    return MeasureTextEx(bold ? uiBold : uiFont, t, size,
                         Spacing(size, 0)).x;
}

/* Small upper-case label with a little letter spacing. */
static void Caption(const char *t, float x, float y, float size, Color c)
{
    DrawTextEx(uiBold, t, (Vector2){roundf(x), roundf(y)}, size,
               Spacing(size, size * 0.08f), c);
}

/* Word-wrapped text inside 'width'. Returns the height used. */
static float WrappedText(const char *text, float x, float y, float width,
                         float size, Color c, bool draw)
{
    char line[400] = "";
    char word[128];
    float lineH = size * 1.35f;
    float cy = y;
    const char *p = text;

    while (*p) {
        while (*p == ' ')
            p++;

        if (!*p)
            break;

        size_t n = 0;

        while (p[n] && p[n] != ' ' && n < sizeof(word) - 1)
            n++;

        memcpy(word, p, n);
        word[n] = '\0';
        p += n;

        char trial[sizeof(line) + sizeof(word) + 2];
        snprintf(trial, sizeof(trial), "%s%s%s",
                 line, line[0] ? " " : "", word);

        if (line[0] && TextWidth(trial, size, false) > width) {
            if (draw)
                Text(line, x, cy, size, c);
            cy += lineH;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            size_t len = strlen(trial);
            if (len >= sizeof(line))
                len = sizeof(line) - 1;
            memcpy(line, trial, len);
            line[len] = '\0';
        }
    }

    if (line[0]) {
        if (draw)
            Text(line, x, cy, size, c);
        cy += lineH;
    }

    return cy - y;
}

/* ============================ SHAPES ============================ */

/* raylib roundness is relative to the short side; use a fixed radius. */
static float Roundness(Rectangle r, float radius)
{
    float m = fminf(r.width, r.height);
    return m > 0 ? fminf(1.0f, 2.0f * radius / m) : 0.0f;
}

static void RoundRect(Rectangle r, float radius, Color c)
{
    DrawRectangleRounded(r, Roundness(r, radius), 12, c);
}

static void RoundRectLines(Rectangle r, float radius, float thick, Color c)
{
    DrawRectangleRoundedLinesEx(r, Roundness(r, radius), 12, thick, c);
}

static Color WithAlpha(Color c, float a)
{
    c.a = (unsigned char)(c.a * a);
    return c;
}

/* ============================ LAYOUT ============================ */

typedef struct {
    float s;
    float pad;
    float headerH;

    Rectangle sidebar;
    Rectangle origin;
    Rectangle destination;
    Rectangle swap;
    Rectangle capacity;
    Rectangle charge;
    Rectangle consumption;
    Rectangle button;
    Rectangle status;

    Rectangle map;
    Rectangle results;

    float rowH;          /* dropdown row height */
} Layout;

static Layout GetLayout(void)
{
    Layout l = {0};

    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    float s = fminf(w / 1600.0f, h / 1000.0f);
    if (s < 0.75f) s = 0.75f;
    if (s > 1.4f)  s = 1.4f;

    l.s = s;
    l.pad = 20 * s;
    l.headerH = 64 * s;
    l.rowH = 38 * s;

    l.sidebar = (Rectangle){
        l.pad, l.headerH + l.pad,
        380 * s, h - l.headerH - 2 * l.pad
    };

    float sx = l.sidebar.x + 24 * s;
    float iw = l.sidebar.width - 48 * s;
    float y0 = l.sidebar.y;
    float fh = 46 * s;

    l.origin      = (Rectangle){sx, y0 + 126 * s, iw, fh};
    l.destination = (Rectangle){sx, y0 + 206 * s, iw, fh};
    l.swap        = (Rectangle){sx + iw - 74 * s, y0 + 180 * s,
                                74 * s, 22 * s};
    l.capacity    = (Rectangle){sx, y0 + 310 * s, iw, fh};
    l.charge      = (Rectangle){sx, y0 + 390 * s, iw, fh};
    l.consumption = (Rectangle){sx, y0 + 470 * s, iw, fh};
    l.button      = (Rectangle){sx, y0 + 540 * s, iw, 52 * s};
    l.status      = (Rectangle){sx, y0 + 606 * s, iw, 0};

    float mx = l.sidebar.x + l.sidebar.width + l.pad;
    float mw = w - mx - l.pad;
    float rh = 118 * s;

    l.results = (Rectangle){mx, h - l.pad - rh, mw, rh};
    l.map = (Rectangle){
        mx, l.headerH + l.pad,
        mw, l.results.y - 16 * s - (l.headerH + l.pad)
    };

    return l;
}

static Rectangle DropdownMenuRect(const Layout *l, int which)
{
    Rectangle f = which == 0 ? l->origin : l->destination;

    return (Rectangle){
        f.x, f.y + f.height + 6 * l->s,
        f.width, LOCATION_COUNT * l->rowH + 12 * l->s
    };
}

/* ============================ MAP DATA ============================ */

void MapDataInit(MapData *map)
{
    memset(map, 0, sizeof(*map));
}

bool LoadMapImage(MapData *map, const char *filename)
{
    if (!map || !filename)
        return false;

    map->texture = LoadTexture(filename);
    if (map->texture.id == 0)
        return false;

    GenTextureMipmaps(&map->texture);
    SetTextureFilter(map->texture, TEXTURE_FILTER_TRILINEAR);
    map->loaded = true;

    printf("Map image loaded: %d x %d\n",
           map->texture.width, map->texture.height);
    return true;
}

void FreeMapData(MapData *map)
{
    if (!map)
        return;
    if (map->loaded)
        UnloadTexture(map->texture);
    memset(map, 0, sizeof(*map));
}

static double MercatorY(double latDeg)
{
    return log(tan(PI / 4.0 + latDeg * PI / 360.0));
}

/* lon/lat -> normalized (0..1, 0..1), origin top-left of the image. */
static Vector2 GeoToUnit(double lon, double lat)
{
    double nTop = MercatorY(MAP_NORTH);
    double nBot = MercatorY(MAP_SOUTH);

    double x = (lon - MAP_WEST) / (MAP_EAST - MAP_WEST);
    double y = (nTop - MercatorY(lat)) / (nTop - nBot); /* inverted */

    return (Vector2){ (float)x, (float)y };
}

/* Image-pixel coordinates. */
Vector2 MapCoordinate(const MapData *map, double lon, double lat)
{
    Vector2 u = GeoToUnit(lon, lat);
    return (Vector2){ u.x * map->texture.width, u.y * map->texture.height };
}

/* ============================ STATE ============================ */

static void SetStatus(PlannerGUI *g, StatusKind kind, const char *msg)
{
    g->statusKind = kind;
    snprintf(g->statusMessage, sizeof(g->statusMessage), "%s", msg);
}

void GUIFreeRoute(PlannerGUI *g)
{
    if (!g)
        return;

    free(g->routePoints);
    g->routePoints = NULL;
    g->routePointCount = 0;

    free(g->chargeStops);
    g->chargeStops = NULL;
    g->chargeStopCount = 0;
}

void GUISetStations(PlannerGUI *g, const Vector2 *points, size_t count)
{
    if (!g)
        return;

    g->stationPoints = points;
    g->stationPointCount = points ? count : 0;
}

void GUISetRoute(PlannerGUI *g, const RouteSummary *r)
{
    if (!g || !r)
        return;

    GUIFreeRoute(g);

    g->routeCalculated = r->success;
    g->batterySufficient = r->batterySufficient;

    /* Stats are kept even for infeasible searches. */
    g->stats = (AlgoStats){
        .valid = r->statsValid,
        .searchMs = r->searchMs,
        .stateSpace = r->stateSpace,
        .statesSettled = r->statesSettled,
        .heapPushes = r->heapPushes,
        .graphVertices = r->graphVertices,
        .baselineValid = r->baselineValid,
        .baselineMs = r->baselineMs,
        .baselineDistanceKm = r->baselineDistanceKm,
        .baselineSettled = r->baselineSettled
    };

    if (!r->success) {
        SetStatus(g, STATUS_ERROR, r->message);
        return;
    }

    SetStatus(g, STATUS_SUCCESS, r->message);

    g->distanceKm = r->distanceKm;
    g->travelTimeHours = r->travelTimeHours;
    g->drivingTimeHours = r->drivingTimeHours;
    g->chargingTimeHours = r->chargingTimeHours;
    g->energyRequiredKWh = r->energyRequiredKWh;
    g->remainingEnergyKWh = r->remainingEnergyKWh;
    g->capacityKWh = atof(g->batteryCapacity);
    g->consumptionKWhPerKm = atof(g->energyConsumption);

    if (r->routePointCount > 0) {
        g->routePoints = malloc(r->routePointCount * sizeof(Vector2));

        if (g->routePoints) {
            memcpy(g->routePoints, r->routePoints,
                   r->routePointCount * sizeof(Vector2));
            g->routePointCount = r->routePointCount;
        }
    }

    if (r->chargeStopCount > 0 && r->chargeStops) {
        g->chargeStops = malloc(r->chargeStopCount * sizeof(Vector2));

        if (g->chargeStops) {
            memcpy(g->chargeStops, r->chargeStops,
                   r->chargeStopCount * sizeof(Vector2));
            g->chargeStopCount = r->chargeStopCount;
        }
    }
}

void InitializeGUI(PlannerGUI *g)
{
    memset(g, 0, sizeof(*g));

    g->originIndex = 0;
    g->destinationIndex = 1;
    g->openDropdown = -1;
    g->activeField = -1;

    strcpy(g->origin, locations[g->originIndex]);
    strcpy(g->destination, locations[g->destinationIndex]);

    strcpy(g->batteryCapacity, "60");
    strcpy(g->batteryCharge, "50");
    strcpy(g->energyConsumption, "0.35");

    SetStatus(g, STATUS_INFO,
              "Choose a start and destination, then press Calculate "
              "route (or Enter).");

    LoadUIFonts();
}

void ShutdownGUI(PlannerGUI *g)
{
    GUIFreeRoute(g);

    if (uiFontsOwned) {
        UnloadFont(uiFont);
        UnloadFont(uiBold);
        uiFontsOwned = false;
    }
}

/* ============================ INPUT ============================ */

static bool ParseNumber(const char *s, double *v)
{
    char *end;

    if (!s || !s[0])
        return false;

    double x = strtod(s, &end);

    if (*end || !isfinite(x))
        return false;

    *v = x;
    return true;
}

static void ProcessNumericInput(char *text, size_t maxLength)
{
    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        size_t n = strlen(text);
        if (n)
            text[n - 1] = '\0';
    }

    int c = GetCharPressed();

    while (c > 0) {
        size_t n = strlen(text);
        bool isDigit = c >= '0' && c <= '9';
        bool isDot = c == '.' && !strchr(text, '.');

        if ((isDigit || isDot) && n < maxLength - 1) {
            text[n] = (char)c;
            text[n + 1] = '\0';
        }

        c = GetCharPressed();
    }
}

static void RouteChanged(PlannerGUI *g)
{
    g->routeCalculated = false;
    GUIFreeRoute(g);
}

static void TryCalculate(PlannerGUI *g)
{
    double cap, charge, consumption;

    g->activeField = -1;

    if (g->originIndex == g->destinationIndex) {
        SetStatus(g, STATUS_ERROR,
                  "Start and destination are the same. "
                  "Pick two different cities.");
        return;
    }

    if (!ParseNumber(g->batteryCapacity, &cap) || cap <= 0 || cap > 300) {
        SetStatus(g, STATUS_ERROR,
                  "Battery capacity must be between 1 and 300 kWh.");
        return;
    }

    if (!ParseNumber(g->batteryCharge, &charge) ||
        charge < 0 || charge > 100) {
        SetStatus(g, STATUS_ERROR,
                  "Current charge must be between 0 and 100 %.");
        return;
    }

    if (!ParseNumber(g->energyConsumption, &consumption) ||
        consumption <= 0 || consumption > 5) {
        SetStatus(g, STATUS_ERROR,
                  "Consumption must be between 0.01 and 5 kWh/km.");
        return;
    }

    RouteChanged(g);
    g->calculationRequested = true;
    SetStatus(g, STATUS_INFO, "Calculating route…");
}

void UpdateGUI(PlannerGUI *g)
{
    Layout l = GetLayout();
    Vector2 m = GetMousePosition();

    /* ---- cursor shape ---- */
    int cursor = MOUSE_CURSOR_DEFAULT;

    if (g->openDropdown >= 0 &&
        CheckCollisionPointRec(m, DropdownMenuRect(&l, g->openDropdown)))
        cursor = MOUSE_CURSOR_POINTING_HAND;
    else if (CheckCollisionPointRec(m, l.origin) ||
             CheckCollisionPointRec(m, l.destination) ||
             CheckCollisionPointRec(m, l.swap) ||
             CheckCollisionPointRec(m, l.button))
        cursor = MOUSE_CURSOR_POINTING_HAND;
    else if (CheckCollisionPointRec(m, l.capacity) ||
             CheckCollisionPointRec(m, l.charge) ||
             CheckCollisionPointRec(m, l.consumption))
        cursor = MOUSE_CURSOR_IBEAM;

    SetMouseCursor(cursor);

    /* ---- mouse ---- */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

        if (g->openDropdown >= 0) {
            Rectangle menu = DropdownMenuRect(&l, g->openDropdown);

            if (CheckCollisionPointRec(m, menu)) {
                int index = (int)floorf((m.y - menu.y - 6 * l.s) / l.rowH);

                if (index >= 0 && index < LOCATION_COUNT) {
                    if (g->openDropdown == 0) {
                        g->originIndex = index;
                        strcpy(g->origin, locations[index]);
                    } else {
                        g->destinationIndex = index;
                        strcpy(g->destination, locations[index]);
                    }

                    RouteChanged(g);
                    SetStatus(g, STATUS_INFO,
                              "Route changed. Press Calculate route.");
                }
            }

            g->openDropdown = -1;
        } else if (CheckCollisionPointRec(m, l.swap)) {
            int t = g->originIndex;
            g->originIndex = g->destinationIndex;
            g->destinationIndex = t;
            strcpy(g->origin, locations[g->originIndex]);
            strcpy(g->destination, locations[g->destinationIndex]);
            RouteChanged(g);
            SetStatus(g, STATUS_INFO, "Start and destination swapped.");
        } else if (CheckCollisionPointRec(m, l.origin)) {
            g->openDropdown = 0;
            g->activeField = -1;
        } else if (CheckCollisionPointRec(m, l.destination)) {
            g->openDropdown = 1;
            g->activeField = -1;
        } else if (CheckCollisionPointRec(m, l.capacity)) {
            g->activeField = 0;
        } else if (CheckCollisionPointRec(m, l.charge)) {
            g->activeField = 1;
        } else if (CheckCollisionPointRec(m, l.consumption)) {
            g->activeField = 2;
        } else if (CheckCollisionPointRec(m, l.button)) {
            TryCalculate(g);
        } else {
            g->activeField = -1;
        }
    }

    /* ---- keyboard ---- */
    if (IsKeyPressed(KEY_TAB)) {
        g->openDropdown = -1;
        g->activeField = (g->activeField + 1) % 3;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        g->openDropdown = -1;
        TryCalculate(g);
    }

    if (g->activeField == 0)
        ProcessNumericInput(g->batteryCapacity,
                            sizeof(g->batteryCapacity));

    if (g->activeField == 1)
        ProcessNumericInput(g->batteryCharge,
                            sizeof(g->batteryCharge));

    if (g->activeField == 2)
        ProcessNumericInput(g->energyConsumption,
                            sizeof(g->energyConsumption));
}

/* ============================ WIDGETS ============================ */

static void FieldFrame(Rectangle r, float s, bool focused, bool hover)
{
    RoundRect(r, 9 * s, hover && !focused ? C_CARD_HOVER : C_CARD);

    if (focused) {
        Rectangle glow = {r.x - 3 * s, r.y - 3 * s,
                          r.width + 6 * s, r.height + 6 * s};
        RoundRectLines(glow, 12 * s, 3 * s, WithAlpha(C_ACCENT, 0.25f));
        RoundRectLines(r, 9 * s, 1.5f * s, C_ACCENT);
    } else {
        RoundRectLines(r, 9 * s, 1.0f * s, C_BORDER);
    }
}

static void FieldLabel(const char *label, Rectangle r, float s)
{
    Text(label, r.x + 2 * s, r.y - 22 * s, 14 * s, C_MUTED);
}

static void DrawInput(const char *label, const char *value,
                      const char *unit, Rectangle r, float s,
                      bool active)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    float size = 17 * s;
    float ty = r.y + (r.height - size) / 2 - 1 * s;

    FieldLabel(label, r, s);
    FieldFrame(r, s, active, hover);

    const char *shown = value[0] ? value : "0";
    Text(shown, r.x + 14 * s, ty, size, value[0] ? C_TEXT : C_FAINT);

    if (active && fmod(GetTime(), 1.0) < 0.55) {
        float cx = r.x + 14 * s +
                   (value[0] ? TextWidth(value, size, false) : 0) + 2 * s;
        DrawRectangleRec((Rectangle){cx, r.y + 12 * s, 1.5f * s,
                                     r.height - 24 * s}, C_ACCENT);
    }

    float uw = TextWidth(unit, 14 * s, false);
    Text(unit, r.x + r.width - 14 * s - uw,
         r.y + (r.height - 14 * s) / 2, 14 * s, C_MUTED);
}

static Color LevelColor(double pct)
{
    if (pct < 20) return C_RED;
    if (pct < 50) return C_AMBER;
    return C_GREEN;
}

/* Thin progress bar along the bottom edge of the charge field. */
static void ChargeBar(Rectangle r, float s, const char *value)
{
    double pct = atof(value);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    Rectangle track = {r.x + 12 * s, r.y + r.height - 8 * s,
                       r.width - 24 * s, 3 * s};

    RoundRect(track, 1.5f * s, C_BORDER);

    Rectangle fill = track;
    fill.width = (float)(track.width * pct / 100.0);

    if (fill.width > 0)
        RoundRect(fill, 1.5f * s, LevelColor(pct));
}

static void DrawChevron(Vector2 c, float size, bool up, Color col)
{
    float h = (up ? -1.0f : 1.0f) * size * 0.5f;
    float t = size * 0.32f;

    DrawLineEx((Vector2){c.x - size, c.y - h}, (Vector2){c.x, c.y + h},
               t, col);
    DrawLineEx((Vector2){c.x, c.y + h}, (Vector2){c.x + size, c.y - h},
               t, col);
}

static void DrawDropdown(const char *label, const char *value,
                         Color dot, Rectangle r, float s, bool open)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);

    FieldLabel(label, r, s);
    FieldFrame(r, s, open, hover);

    DrawCircleV((Vector2){r.x + 20 * s, r.y + r.height / 2}, 5 * s, dot);

    Text(value, r.x + 36 * s, r.y + (r.height - 17 * s) / 2 - 1 * s,
         17 * s, C_TEXT);

    DrawChevron((Vector2){r.x + r.width - 22 * s, r.y + r.height / 2},
                5 * s, open, open ? C_ACCENT : C_MUTED);
}

static void DrawSwapButton(Rectangle r, float s)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);

    RoundRect(r, r.height / 2, hover ? C_CARD_HOVER : C_CARD);
    RoundRectLines(r, r.height / 2, 1.0f * s, hover ? C_ACCENT : C_BORDER);

    const char *t = "⇅ Swap";
    float size = 12.5f * s;
    float w = TextWidth(t, size, false);

    Text(t, r.x + (r.width - w) / 2, r.y + (r.height - size) / 2 - 1 * s,
         size, hover ? C_TEXT : C_MUTED);
}

static void DrawDropdownMenu(const Layout *l, int which, int selected,
                             int other)
{
    float s = l->s;
    Rectangle menu = DropdownMenuRect(l, which);

    /* soft shadow */
    for (int i = 3; i >= 1; i--) {
        Rectangle sh = {menu.x - i * 2 * s, menu.y - i * s + 4 * s,
                        menu.width + i * 4 * s, menu.height + i * 3 * s};
        RoundRect(sh, 12 * s, (Color){0, 0, 0, 30});
    }

    RoundRect(menu, 10 * s, C_PANEL);
    RoundRectLines(menu, 10 * s, 1.0f * s, C_BORDER);

    Vector2 mouse = GetMousePosition();

    for (int i = 0; i < LOCATION_COUNT; i++) {
        Rectangle row = {menu.x + 6 * s, menu.y + 6 * s + i * l->rowH,
                         menu.width - 12 * s, l->rowH};

        bool hover = CheckCollisionPointRec(mouse, row);

        if (i == selected)
            RoundRect(row, 7 * s, WithAlpha(C_ACCENT, 0.18f));
        else if (hover)
            RoundRect(row, 7 * s, C_CARD_HOVER);

        Color tc = i == other ? C_FAINT : C_TEXT;

        Text(locations[i], row.x + 12 * s,
             row.y + (row.height - 16 * s) / 2 - 1 * s, 16 * s, tc);

        if (i == other) {
            const char *note = which == 0 ? "destination" : "start";
            float nw = TextWidth(note, 12 * s, false);
            Text(note, row.x + row.width - 12 * s - nw,
                 row.y + (row.height - 12 * s) / 2, 12 * s, C_FAINT);
        } else if (i == selected) {
            /* check mark */
            float cx = row.x + row.width - 20 * s;
            float cy = row.y + row.height / 2;
            DrawLineEx((Vector2){cx - 5 * s, cy},
                       (Vector2){cx - 1.5f * s, cy + 4 * s},
                       2 * s, C_ACCENT);
            DrawLineEx((Vector2){cx - 1.5f * s, cy + 4 * s},
                       (Vector2){cx + 6 * s, cy - 4.5f * s},
                       2 * s, C_ACCENT);
        }
    }
}

static void DrawButton(Rectangle r, float s, const char *label)
{
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    bool down = hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

    Color c = down ? C_ACCENT_DOWN : hover ? C_ACCENT : C_ACCENT_DIM;

    RoundRect((Rectangle){r.x, r.y + 3 * s, r.width, r.height},
              10 * s, (Color){0, 0, 0, 60});
    RoundRect(r, 10 * s, c);

    float size = 17 * s;
    float w = TextWidth(label, size, true);

    TextBold(label, r.x + (r.width - w) / 2,
             r.y + (r.height - size) / 2 - 1 * s, size, WHITE);
}

/* Returns the height of the drawn status box. */
static float DrawStatus(const PlannerGUI *g, Rectangle area, float s)
{
    Color c = g->statusKind == STATUS_SUCCESS ? C_GREEN
            : g->statusKind == STATUS_ERROR   ? C_RED
            : C_ACCENT;

    float size = 14 * s;
    float padX = 16 * s;
    float padY = 12 * s;
    float textW = area.width - padX - 12 * s;

    float th = WrappedText(g->statusMessage, 0, 0, textW, size,
                           C_TEXT, false);

    Rectangle box = {area.x, area.y, area.width, th + 2 * padY};

    RoundRect(box, 9 * s, WithAlpha(c, 0.12f));
    RoundRect((Rectangle){box.x, box.y, 4 * s, box.height}, 2 * s, c);

    WrappedText(g->statusMessage, box.x + padX, box.y + padY, textW,
                size, C_TEXT, true);

    return box.height;
}

/* 1234567 -> "1,234,567" */
static void FormatCount(size_t n, char *buf, size_t len)
{
    char digits[32];
    snprintf(digits, sizeof(digits), "%zu", n);

    size_t nd = strlen(digits);
    size_t o = 0;

    for (size_t i = 0; i < nd && o + 2 < len; i++) {
        if (i > 0 && (nd - i) % 3 == 0)
            buf[o++] = ',';
        buf[o++] = digits[i];
    }

    buf[o] = '\0';
}

/* 1764294 -> "1.76 M" */
static void FormatBig(size_t n, char *buf, size_t len)
{
    if (n >= 1000000)
        snprintf(buf, len, "%.2f M", n / 1e6);
    else if (n >= 10000)
        snprintf(buf, len, "%.1f k", n / 1e3);
    else
        FormatCount(n, buf, len);
}

static void FormatMs(double ms, char *buf, size_t len)
{
    if (ms < 1.0)
        snprintf(buf, len, "%.3f ms", ms);
    else if (ms < 100.0)
        snprintf(buf, len, "%.1f ms", ms);
    else
        snprintf(buf, len, "%.0f ms", ms);
}

/* One "label .......... value" row of the analysis card. */
static void StatRow(float x, float y, float w, float s, const char *label,
                    const char *value, bool heading)
{
    float size = (heading ? 13.5f : 13) * s;

    if (heading)
        TextBold(label, x, y, size, C_TEXT);
    else
        Text(label, x, y, size, C_MUTED);

    float vw = TextWidth(value, size, heading);

    if (heading)
        TextBold(value, x + w - vw, y, size, C_TEXT);
    else
        Text(value, x + w - vw, y, size, C_TEXT);
}

/*
 * Search statistics for the last query: the battery-aware search over
 * (vertex, battery level) states next to plain Dijkstra over vertices.
 * Draws the full card when there is room, otherwise only the first part.
 */
static void DrawAnalysis(const AlgoStats *st, Rectangle area, float s)
{
    float rowH = 21 * s;
    bool full = area.height >= 184 * s;
    float h = full ? 184 * s : 114 * s;

    if (area.height < h)
        return;

    Rectangle card = {area.x, area.y + area.height - h, area.width, h};

    RoundRect(card, 10 * s, C_CARD);

    float x = card.x + 14 * s;
    float w = card.width - 28 * s;
    float y = card.y + 14 * s;

    Caption("SEARCH ANALYSIS", x, y, 11.5f * s, C_FAINT);

    const char *hint = st->valid ? "last query" : "run a route";
    float hw = TextWidth(hint, 12 * s, false);
    Text(hint, x + w - hw, y - 1 * s, 12 * s, C_FAINT);

    y += 26 * s;

    char a[48], b[48], c[160];
    const char *dash = "—";

    /* battery-aware search */
    if (st->valid) FormatMs(st->searchMs, a, sizeof(a));
    StatRow(x, y, w, s, "Battery-aware Dijkstra",
            st->valid ? a : dash, true);
    y += rowH;

    if (st->valid) {
        FormatCount(st->statesSettled, a, sizeof(a));
        FormatBig(st->stateSpace, b, sizeof(b));
        double pct = st->stateSpace
            ? 100.0 * st->statesSettled / st->stateSpace : 0.0;
        snprintf(c, sizeof(c), "%s of %s (%.2f%%)", a, b, pct);
    }
    StatRow(x, y, w, s, "States settled", st->valid ? c : dash, false);
    y += rowH;

    if (st->valid) FormatCount(st->heapPushes, a, sizeof(a));
    StatRow(x, y, w, s, "Heap pushes", st->valid ? a : dash, false);
    y += rowH;

    if (!full)
        return;

    y += 4 * s;
    DrawLineEx((Vector2){x, y}, (Vector2){x + w, y}, 1.0f * s, C_BORDER);
    y += 10 * s;

    bool bv = st->valid && st->baselineValid;

    /* baseline */
    if (bv) FormatMs(st->baselineMs, a, sizeof(a));
    StatRow(x, y, w, s, "Plain Dijkstra (no battery)",
            bv ? a : dash, true);
    y += rowH;

    if (bv) {
        FormatCount(st->baselineSettled, a, sizeof(a));
        FormatCount(st->graphVertices, b, sizeof(b));
        snprintf(c, sizeof(c), "%s of %s", a, b);
    }
    StatRow(x, y, w, s, "Vertices settled", bv ? c : dash, false);
    y += rowH;

    if (bv) {
        if (st->baselineDistanceKm >= 0)
            snprintf(c, sizeof(c), "%.1f km", st->baselineDistanceKm);
        else
            snprintf(c, sizeof(c), "unreachable");
    }
    StatRow(x, y, w, s, "Shortest distance", bv ? c : dash, false);
}

static void DrawSidebar(const PlannerGUI *g, const Layout *l)
{
    float s = l->s;
    Rectangle sb = l->sidebar;
    float sx = l->origin.x;

    RoundRect(sb, 14 * s, C_PANEL);

    TextBold("Plan a trip", sx, sb.y + 22 * s, 22 * s, C_TEXT);
    Text("Battery-aware route with charging stops",
         sx, sb.y + 54 * s, 14 * s, C_MUTED);

    Caption("ROUTE", sx, sb.y + 84 * s, 12 * s, C_FAINT);

    DrawDropdown("From", g->origin, C_GREEN, l->origin, s,
                 g->openDropdown == 0);
    DrawDropdown("To", g->destination, C_RED, l->destination, s,
                 g->openDropdown == 1);
    DrawSwapButton(l->swap, s);

    Caption("VEHICLE", sx, l->capacity.y - 46 * s, 12 * s, C_FAINT);

    DrawInput("Battery capacity", g->batteryCapacity, "kWh",
              l->capacity, s, g->activeField == 0);
    DrawInput("Current charge", g->batteryCharge, "%",
              l->charge, s, g->activeField == 1);
    ChargeBar(l->charge, s, g->batteryCharge);
    DrawInput("Energy consumption", g->energyConsumption, "kWh/km",
              l->consumption, s, g->activeField == 2);

    DrawButton(l->button, s, "Calculate route");

    float statusH = DrawStatus(g, l->status, s);

    /* analysis card fills the space between the status and the bottom */
    float top = l->status.y + statusH + 14 * s;
    Rectangle area = {sx, top, l->status.width,
                      sb.y + sb.height - 20 * s - top};

    DrawAnalysis(&g->stats, area, s);
}

/* ============================ HEADER ============================ */

static float ChipWidth(const char *text, float s)
{
    return TextWidth(text, 13 * s, false) + 34 * s;
}

static void DrawChip(const char *text, float rightX, float cy, float s,
                     Color dot)
{
    float size = 13 * s;
    float w = ChipWidth(text, s);
    Rectangle r = {rightX - w, cy - 14 * s, w, 28 * s};

    RoundRect(r, 14 * s, C_CARD);
    DrawCircleV((Vector2){r.x + 14 * s, cy}, 4 * s, dot);
    Text(text, r.x + 24 * s, cy - size / 2 - 1 * s, size, C_MUTED);
}

static void DrawHeader(const PlannerGUI *g, const Layout *l)
{
    float s = l->s;
    float w = (float)GetScreenWidth();
    float cy = l->headerH / 2;

    DrawRectangleRec((Rectangle){0, 0, w, l->headerH}, C_PANEL);
    DrawLineEx((Vector2){0, l->headerH}, (Vector2){w, l->headerH},
               1.0f * s, C_BORDER);

    /* logo: rounded square with a lightning bolt (counter-clockwise tris) */
    Rectangle logo = {l->pad, cy - 17 * s, 34 * s, 34 * s};
    RoundRect(logo, 9 * s, C_ACCENT);

    Vector2 o = {logo.x + logo.width / 2, logo.y + logo.height / 2};
    DrawTriangle((Vector2){o.x + 3 * s, o.y - 11 * s},
                 (Vector2){o.x - 7 * s, o.y + 2 * s},
                 (Vector2){o.x + 1 * s, o.y + 2 * s}, WHITE);
    DrawTriangle((Vector2){o.x - 1 * s, o.y - 2 * s},
                 (Vector2){o.x - 3 * s, o.y + 11 * s},
                 (Vector2){o.x + 7 * s, o.y - 2 * s}, WHITE);

    float tx = logo.x + logo.width + 14 * s;
    TextBold("EV Route Planner", tx, cy - 12 * s, 21 * s, C_TEXT);

    float tw = TextWidth("EV Route Planner", 21 * s, true);
    Text("Dehradun region · Uttarakhand", tx + tw + 14 * s,
         cy - 8 * s, 14 * s, C_MUTED);

    char stations[48];
    snprintf(stations, sizeof(stations), "%zu charging stations",
             g->stationPointCount);

    float right = w - l->pad;
    DrawChip(stations, right, cy, s, C_TEAL);
    right -= ChipWidth(stations, s) + 10 * s;
    DrawChip("9 cities", right, cy, s, C_TEXT);
}

/* ============================ MAP ============================ */

static Rectangle MapDestRect(const MapData *map, Rectangle area)
{
    float sx = area.width  / (float)map->texture.width;
    float sy = area.height / (float)map->texture.height;
    float s  = sx < sy ? sx : sy;
    float w  = map->texture.width  * s;
    float h  = map->texture.height * s;

    return (Rectangle){ area.x + (area.width - w) * 0.5f,
                        area.y + (area.height - h) * 0.5f, w, h };
}

/* Same transform for background and overlay: geo -> image px -> screen. */
static Vector2 GeoToScreen(const MapData *map, Rectangle dest,
                           double lon, double lat)
{
    Vector2 p = MapCoordinate(map, lon, lat);
    return (Vector2){ dest.x + p.x / map->texture.width  * dest.width,
                      dest.y + p.y / map->texture.height * dest.height };
}

static void Pin(Vector2 p, float s, Color c, const char *letter)
{
    DrawCircleV((Vector2){p.x, p.y + 2 * s}, 13 * s, (Color){0, 0, 0, 70});
    DrawCircleV(p, 13 * s, WHITE);
    DrawCircleV(p, 10.5f * s, c);

    float size = 13 * s;
    float w = TextWidth(letter, size, true);
    TextBold(letter, p.x - w / 2, p.y - size / 2 - 1 * s, size, WHITE);
}

static void DrawRoute(const PlannerGUI *g, const MapData *map,
                      Rectangle dest, float s)
{
    size_t n = g->routePointCount;
    Vector2 *pts = malloc(n * sizeof(Vector2));

    if (!pts)
        return;

    for (size_t i = 0; i < n; i++)
        pts[i] = GeoToScreen(map, dest, g->routePoints[i].x,
                             g->routePoints[i].y);

    /* casing, then the line; circles at the joints round the corners */
    float wc = 9 * s, wl = 5 * s;

    for (size_t i = 0; i + 1 < n; i++)
        DrawLineEx(pts[i], pts[i + 1], wc, C_ROUTE_CASE);
    for (size_t i = 0; i < n; i++)
        DrawCircleV(pts[i], wc / 2, C_ROUTE_CASE);

    for (size_t i = 0; i + 1 < n; i++)
        DrawLineEx(pts[i], pts[i + 1], wl, C_ROUTE);
    for (size_t i = 0; i < n; i++)
        DrawCircleV(pts[i], wl / 2, C_ROUTE);

    free(pts);
}

static void DrawMapLegend(Rectangle box, float s)
{
    float size = 12.5f * s;
    float gap = 18 * s;

    const char *labels[] = {"Route", "City", "EV station", "Charging stop"};
    float widths[4];
    float total = 28 * s;

    for (int i = 0; i < 4; i++) {
        widths[i] = 22 * s + TextWidth(labels[i], size, false);
        total += widths[i] + (i < 3 ? gap : 0);
    }

    Rectangle r = {box.x + 14 * s, box.y + box.height - 46 * s,
                   total, 32 * s};

    RoundRect(r, 16 * s, C_CHIP);

    float x = r.x + 14 * s;
    float cy = r.y + r.height / 2;

    for (int i = 0; i < 4; i++) {
        Vector2 c = {x + 7 * s, cy};

        switch (i) {
        case 0:
            DrawLineEx((Vector2){x, cy}, (Vector2){x + 14 * s, cy},
                       7 * s, C_ROUTE_CASE);
            DrawLineEx((Vector2){x, cy}, (Vector2){x + 14 * s, cy},
                       4 * s, C_ROUTE);
            break;
        case 1:
            DrawCircleV(c, 5.5f * s, C_INK);
            DrawCircleV(c, 4 * s, WHITE);
            break;
        case 2:
            DrawCircleV(c, 5 * s, WHITE);
            DrawCircleV(c, 3.5f * s, C_TEAL);
            break;
        default:
            DrawCircleV(c, 7 * s, WHITE);
            DrawCircleV(c, 5.5f * s, C_TEAL);
            break;
        }

        Text(labels[i], x + 22 * s, cy - size / 2 - 1 * s, size, C_TEXT);
        x += widths[i] + gap;
    }
}

static void DrawMap(const PlannerGUI *g, const MapData *map,
                    const Layout *l)
{
    float s = l->s;
    Rectangle box = l->map;

    RoundRect(box, 14 * s, C_PANEL);

    if (!map || !map->loaded) {
        const char *t = "Map image not available";
        float w = TextWidth(t, 18 * s, true);
        TextBold(t, box.x + (box.width - w) / 2,
                 box.y + box.height / 2 - 20 * s, 18 * s, C_MUTED);

        const char *t2 = "Expected data/processed/region_map.png";
        w = TextWidth(t2, 14 * s, false);
        Text(t2, box.x + (box.width - w) / 2,
             box.y + box.height / 2 + 8 * s, 14 * s, C_FAINT);
        return;
    }

    Rectangle inner = {box.x + 8 * s, box.y + 8 * s,
                       box.width - 16 * s, box.height - 16 * s};
    Rectangle dest = MapDestRect(map, inner);
    Rectangle src = {0, 0, (float)map->texture.width,
                     (float)map->texture.height};

    BeginScissorMode((int)box.x, (int)box.y,
                     (int)box.width, (int)box.height);

    DrawTexturePro(map->texture, src, dest, (Vector2){0, 0}, 0, WHITE);
    DrawRectangleLinesEx(dest, 1.0f, C_BORDER);

    bool route = g->routeCalculated && g->routePointCount > 1;

    /* all charging stations, faded while a route is shown */
    float stAlpha = route ? 0.55f : 1.0f;

    for (size_t i = 0; i < g->stationPointCount; i++) {
        Vector2 p = GeoToScreen(map, dest, g->stationPoints[i].x,
                                g->stationPoints[i].y);
        DrawCircleV(p, 5 * s, WithAlpha(WHITE, stAlpha));
        DrawCircleV(p, 3.5f * s, WithAlpha(C_TEAL, stAlpha));
    }

    if (route)
        DrawRoute(g, map, dest, s);

    /* cities with labels */
    for (int i = 0; i < LOCATION_COUNT; i++) {
        Vector2 p = GeoToScreen(map, dest, cityCoords[i][1],
                                cityCoords[i][0]);
        bool endpoint = route && (i == g->originIndex ||
                                  i == g->destinationIndex);

        if (!endpoint) {
            DrawCircleV(p, 6 * s, C_INK);
            DrawCircleV(p, 4.5f * s, WHITE);
        }

        /* Dark pill above the dot. It deliberately covers the name that
           is printed on the base map, so the two never overlap. */
        float size = (endpoint ? 13.5f : 12.5f) * s;
        float tw = TextWidth(locations[i], size, true);
        float lift = (endpoint ? 17 : 9) * s;
        Rectangle pill = {p.x - tw / 2 - 8 * s, p.y - lift - 22 * s,
                          tw + 16 * s, 22 * s};

        RoundRect(pill, 11 * s, (Color){20, 24, 33, 225});

        if (endpoint)
            RoundRectLines(pill, 11 * s, 1.5f * s,
                           i == g->originIndex ? C_GREEN : C_RED);

        TextBold(locations[i], pill.x + 8 * s,
                 pill.y + (pill.height - size) / 2 - 1 * s, size, WHITE);
    }

    if (route) {
        for (size_t i = 0; i < g->chargeStopCount; i++) {
            Vector2 p = GeoToScreen(map, dest, g->chargeStops[i].x,
                                    g->chargeStops[i].y);

            DrawCircleV((Vector2){p.x, p.y + 2 * s}, 11 * s,
                        (Color){0, 0, 0, 70});
            DrawCircleV(p, 11 * s, WHITE);
            DrawCircleV(p, 8.5f * s, C_TEAL);

            char num[24];
            snprintf(num, sizeof(num), "%zu", i + 1);
            float size = 11 * s;
            float w = TextWidth(num, size, true);
            TextBold(num, p.x - w / 2, p.y - size / 2 - 1 * s, size, WHITE);
        }

        Vector2 a = GeoToScreen(map, dest, g->routePoints[0].x,
                                g->routePoints[0].y);
        Vector2 b = GeoToScreen(map, dest,
                                g->routePoints[g->routePointCount - 1].x,
                                g->routePoints[g->routePointCount - 1].y);

        Pin(a, s, C_GREEN, "A");
        Pin(b, s, C_RED, "B");
    }

    EndScissorMode();

    /* title chip */
    const char *title = "OpenStreetMap · Dehradun region";
    float tsz = 13 * s;
    float tw = TextWidth(title, tsz, true);
    Rectangle chip = {box.x + 14 * s, box.y + 14 * s, tw + 28 * s, 30 * s};
    RoundRect(chip, 15 * s, C_CHIP);
    TextBold(title, chip.x + 14 * s,
             chip.y + (chip.height - tsz) / 2 - 1 * s, tsz, C_TEXT);

    if (route) {
        char trip[160];
        snprintf(trip, sizeof(trip), "%s  →  %s",
                 g->origin, g->destination);
        float w = TextWidth(trip, tsz, true);
        Rectangle tc = {box.x + box.width - 14 * s - w - 28 * s,
                        box.y + 14 * s, w + 28 * s, 30 * s};
        RoundRect(tc, 15 * s, C_CHIP);
        TextBold(trip, tc.x + 14 * s,
                 tc.y + (tc.height - tsz) / 2 - 1 * s, tsz, C_ROUTE);
    }

    DrawMapLegend(box, s);

    /* bottom-right note: the data caveat */
    const char *t = "Manual road distances · not GPS navigation";
    float w = TextWidth(t, 12.5f * s, false);
    Rectangle r = {box.x + box.width - 14 * s - w - 28 * s,
                   box.y + box.height - 46 * s, w + 28 * s, 32 * s};
    RoundRect(r, 16 * s, C_CHIP);
    Text(t, r.x + 14 * s, r.y + (r.height - 12.5f * s) / 2 - 1 * s,
         12.5f * s, C_MUTED);
}

/* ============================ RESULTS ============================ */

static void FormatDuration(double hours, char *buf, size_t n)
{
    long mins = lround(hours * 60.0);

    if (mins < 0)
        mins = 0;

    if (mins < 60)
        snprintf(buf, n, "%ld min", mins);
    else if (mins % 60 == 0)
        snprintf(buf, n, "%ld h", mins / 60);
    else
        snprintf(buf, n, "%ld h %ld min", mins / 60, mins % 60);
}

static void MetricCard(Rectangle r, float s, const char *label,
                       const char *value, const char *sub, Color accent,
                       bool empty)
{
    RoundRect(r, 12 * s, C_PANEL);
    RoundRectLines(r, 12 * s, 1.0f * s, WithAlpha(C_BORDER, 0.7f));

    float x = r.x + 18 * s;

    DrawCircleV((Vector2){x + 4 * s, r.y + 24 * s}, 4 * s,
                empty ? C_FAINT : accent);
    Caption(label, x + 14 * s, r.y + 17 * s, 11.5f * s, C_MUTED);

    /* shrink the value if the card is narrow */
    float vs = 26 * s;
    while (vs > 16 * s && TextWidth(value, vs, true) > r.width - 36 * s)
        vs -= 1 * s;

    TextBold(value, x, r.y + 42 * s, vs, empty ? C_FAINT : C_TEXT);

    if (sub && sub[0]) {
        char clipped[96];
        snprintf(clipped, sizeof(clipped), "%s", sub);

        while (strlen(clipped) > 4 &&
               TextWidth(clipped, 12.5f * s, false) > r.width - 36 * s) {
            size_t len = strlen(clipped);
            strcpy(clipped + len - 4, "...");
        }

        Text(clipped, x, r.y + r.height - 30 * s, 12.5f * s, C_MUTED);
    }
}

static void DrawResults(const PlannerGUI *g, const Layout *l)
{
    float s = l->s;
    Rectangle a = l->results;
    float gap = 14 * s;
    float cw = (a.width - 4 * gap) / 5;

    Rectangle c[5];
    for (int i = 0; i < 5; i++)
        c[i] = (Rectangle){a.x + i * (cw + gap), a.y, cw, a.height};

    if (!g->routeCalculated) {
        const char *dash = "—";
        const char *hint = "Calculate a route";
        MetricCard(c[0], s, "DISTANCE", dash, hint, C_ACCENT, true);
        MetricCard(c[1], s, "TRAVEL TIME", dash, "", C_ACCENT, true);
        MetricCard(c[2], s, "ENERGY USED", dash, "", C_AMBER, true);
        MetricCard(c[3], s, "CHARGING", dash, "", C_TEAL, true);
        MetricCard(c[4], s, "ON ARRIVAL", dash, "", C_GREEN, true);
        return;
    }

    char v[64], sub[96], t[32];

    snprintf(v, sizeof(v), "%.1f km", g->distanceKm);
    MetricCard(c[0], s, "DISTANCE", v, "Road distance", C_ACCENT, false);

    FormatDuration(g->travelTimeHours, v, sizeof(v));
    FormatDuration(g->drivingTimeHours, t, sizeof(t));
    snprintf(sub, sizeof(sub), "%s driving", t);
    MetricCard(c[1], s, "TRAVEL TIME", v, sub, C_ACCENT, false);

    snprintf(v, sizeof(v), "%.1f kWh", g->energyRequiredKWh);
    snprintf(sub, sizeof(sub), "at %.2f kWh/km", g->consumptionKWhPerKm);
    MetricCard(c[2], s, "ENERGY USED", v, sub, C_AMBER, false);

    if (g->chargeStopCount == 0) {
        MetricCard(c[3], s, "CHARGING", "None", "No stop needed",
                   C_TEAL, false);
    } else {
        snprintf(v, sizeof(v), "%zu stop%s", g->chargeStopCount,
                 g->chargeStopCount == 1 ? "" : "s");
        FormatDuration(g->chargingTimeHours, t, sizeof(t));
        snprintf(sub, sizeof(sub), "%s plugged in", t);
        MetricCard(c[3], s, "CHARGING", v, sub, C_TEAL, false);
    }

    /* battery on arrival, with a level bar and the 10% reserve mark */
    double pct = g->capacityKWh > 0
        ? 100.0 * g->remainingEnergyKWh / g->capacityKWh : 0.0;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    Color lc = g->batterySufficient ? LevelColor(pct) : C_RED;

    snprintf(v, sizeof(v), "%.0f%%", pct);
    MetricCard(c[4], s, "ON ARRIVAL", v, "", lc, false);

    Rectangle r = c[4];
    float vw = TextWidth(v, 26 * s, true);
    snprintf(sub, sizeof(sub), "%.1f kWh", g->remainingEnergyKWh);
    Text(sub, r.x + 18 * s + vw + 10 * s, r.y + 52 * s, 13 * s, C_MUTED);

    Rectangle track = {r.x + 18 * s, r.y + r.height - 26 * s,
                       r.width - 36 * s, 6 * s};
    RoundRect(track, 3 * s, C_CARD);

    Rectangle fill = track;
    fill.width = (float)(track.width * pct / 100.0);
    if (fill.width > 0)
        RoundRect(fill, 3 * s, lc);

    float rx = track.x + track.width * 0.10f;
    DrawLineEx((Vector2){rx, track.y - 3 * s},
               (Vector2){rx, track.y + track.height + 3 * s},
               1.5f * s, C_MUTED);
}

/* ============================ FRAME ============================ */

void DrawGUI(const PlannerGUI *g, const MapData *map)
{
    Layout l = GetLayout();

    ClearBackground(C_BG);

    DrawHeader(g, &l);
    DrawSidebar(g, &l);
    DrawMap(g, map, &l);
    DrawResults(g, &l);

    /* dropdown menus go on top of everything */
    if (g->openDropdown == 0)
        DrawDropdownMenu(&l, 0, g->originIndex, g->destinationIndex);

    if (g->openDropdown == 1)
        DrawDropdownMenu(&l, 1, g->destinationIndex, g->originIndex);
}
