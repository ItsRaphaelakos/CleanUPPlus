#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr COLORREF kBg = RGB(8, 11, 16);
constexpr COLORREF kSurface = RGB(16, 21, 28);
constexpr COLORREF kSurface2 = RGB(24, 33, 43);
constexpr COLORREF kText = RGB(230, 237, 243);
constexpr COLORREF kMuted = RGB(184, 196, 207);
constexpr COLORREF kCyan = RGB(34, 211, 238);
constexpr COLORREF kBlue = RGB(96, 165, 250);
constexpr COLORREF kGreen = RGB(52, 211, 153);
constexpr COLORREF kRed = RGB(248, 113, 113);
constexpr COLORREF kConsole = RGB(5, 9, 14);

enum class Screen { Home, Scan, Results, Settings };
enum class Category { All, DuplicatePhotos, LargeFiles, Screenshots, Downloads, EmptyFolders };

struct Item {
    std::wstring name;
    std::wstring path;
    std::wstring size;
    Category category;
    bool selected = false;
    bool duplicateCopy = false;
};

struct Hotspot {
    RECT rect{};
    std::function<void()> onClick;
    std::wstring label;
};

HWND gWindow = nullptr;
HFONT gFontTitle = nullptr;
HFONT gFontHeading = nullptr;
HFONT gFontBody = nullptr;
HFONT gFontSmall = nullptr;
HFONT gFontMono = nullptr;

Screen gScreen = Screen::Home;
Category gFilter = Category::All;
bool gHasStorageAccess = false;
bool gIncludeScreenshots = true;
bool gIncludeDownloads = true;
bool gDarkTheme = true;
int gThresholdMb = 100;
std::vector<Item> gItems;
std::vector<Hotspot> gHotspots;
std::vector<std::wstring> gLogs;

std::wstring PlatformName() {
#if defined(VIEWER_IOS)
    return L"iOS";
#else
    return L"Android";
#endif
}

std::wstring AppFileName() {
#if defined(VIEWER_IOS)
    return L"CleanUpPlus_iOS_Source.zip";
#else
    return L"CleanUpPlus-debug.apk";
#endif
}

void Log(const std::wstring& message) {
    if (gLogs.size() > 80) {
        gLogs.erase(gLogs.begin());
    }
    gLogs.push_back(L"> " + message);
}

void Refresh() {
    if (gWindow != nullptr) {
        InvalidateRect(gWindow, nullptr, TRUE);
    }
}

RECT Rect(int x, int y, int w, int h) {
    return RECT{x, y, x + w, y + h};
}

bool Contains(const RECT& rect, int x, int y) {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

void FillRound(HDC dc, RECT rect, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void StrokeRound(HDC dc, RECT rect, int radius, COLORREF color, int width = 1) {
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(dc, GetStockObject(NULL_BRUSH)));
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HPEN oldPen = static_cast<HPEN>(SelectObject(dc, pen));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
}

void Text(HDC dc, const std::wstring& value, RECT rect, HFONT font, COLORREF color, UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT oldFont = static_cast<HFONT>(SelectObject(dc, font));
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect, flags | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);
}

void TextAt(HDC dc, const std::wstring& value, int x, int y, HFONT font, COLORREF color) {
    Text(dc, value, Rect(x, y, 500, 28), font, color);
}

void AddHotspot(RECT rect, std::wstring label, std::function<void()> action) {
    gHotspots.push_back(Hotspot{rect, std::move(action), std::move(label)});
}

void Button(HDC dc, RECT rect, const std::wstring& label, COLORREF fill, COLORREF fg, std::function<void()> action) {
    FillRound(dc, rect, 16, fill);
    Text(dc, label, rect, gFontBody, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    AddHotspot(rect, label, std::move(action));
}

void Chip(HDC dc, RECT rect, const std::wstring& label, bool selected, std::function<void()> action) {
    FillRound(dc, rect, 14, selected ? RGB(7, 70, 84) : kSurface2);
    StrokeRound(dc, rect, 14, selected ? kCyan : RGB(48, 60, 72));
    Text(dc, label, rect, gFontSmall, selected ? kCyan : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    AddHotspot(rect, label, std::move(action));
}

std::wstring CategoryName(Category category) {
    switch (category) {
        case Category::DuplicatePhotos: return L"Duplicate Photos";
        case Category::LargeFiles: return L"Large Files";
        case Category::Screenshots: return L"Screenshots";
        case Category::Downloads: return L"Downloads";
        case Category::EmptyFolders: return L"Empty Folders";
        case Category::All: return L"All";
    }
    return L"All";
}

int Count(Category category) {
    if (category == Category::All) return static_cast<int>(gItems.size());
    return static_cast<int>(std::count_if(gItems.begin(), gItems.end(), [&](const Item& item) {
        return item.category == category;
    }));
}

int SelectedCount() {
    return static_cast<int>(std::count_if(gItems.begin(), gItems.end(), [](const Item& item) {
        return item.selected;
    }));
}

void SimulateScan() {
    if (!gHasStorageAccess) {
        Log(L"ERROR: storage access missing. Press Grant Access first.");
        gScreen = Screen::Scan;
        Refresh();
        return;
    }

    gItems = {
        {L"IMG_2401_copy.jpg", L"/storage/emulated/0/DCIM/Camera/IMG_2401_copy.jpg", L"4.8 MB", Category::DuplicatePhotos, false, true},
        {L"vacation_duplicate.png", L"/storage/emulated/0/Pictures/vacation_duplicate.png", L"8.2 MB", Category::DuplicatePhotos, false, true},
        {L"movie_export.mov", L"/storage/emulated/0/Download/movie_export.mov", L"734 MB", Category::LargeFiles, false, false},
        {L"Screenshot_2026-06-03.png", L"/storage/emulated/0/Pictures/Screenshots/Screenshot_2026-06-03.png", L"2.1 MB", Category::Screenshots, false, false},
        {L"installer_backup.zip", L"/storage/emulated/0/Download/installer_backup.zip", L"128 MB", Category::Downloads, false, false},
        {L"Old Empty Folder", L"/storage/emulated/0/Documents/Old Empty Folder", L"0 B", Category::EmptyFolders, false, false},
    };
    gFilter = Category::All;
    gScreen = Screen::Results;
    Log(L"Scan completed: 6 cleanup candidates, 2 duplicate copies.");
    Refresh();
}

void GrantAccess() {
    gHasStorageAccess = true;
    Log(L"Storage access enabled in viewer simulation.");
    Refresh();
}

void KeepNewest() {
    int selected = 0;
    for (auto& item : gItems) {
        if (item.duplicateCopy) {
            item.selected = true;
            ++selected;
        }
    }
    gFilter = Category::DuplicatePhotos;
    Log(L"Keep newest selected " + std::to_wstring(selected) + L" duplicate copy item(s).");
    Refresh();
}

void DeleteSelected() {
    const int selected = SelectedCount();
    if (selected == 0) {
        Log(L"ERROR: Delete Selected pressed with nothing selected.");
        Refresh();
        return;
    }

    const int answer = MessageBoxW(
        gWindow,
        L"CleanUp+ will permanently delete the selected sample item(s) in this viewer preview.\n\nContinue?",
        L"Confirm Delete Selected",
        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);

    if (answer != IDYES) {
        Log(L"Delete cancelled by user.");
        Refresh();
        return;
    }

    gItems.erase(std::remove_if(gItems.begin(), gItems.end(), [](const Item& item) {
        return item.selected;
    }), gItems.end());
    Log(L"Deleted " + std::to_wstring(selected) + L" selected item(s) from viewer state.");
    Refresh();
}

std::vector<Item*> FilteredItems() {
    std::vector<Item*> items;
    for (auto& item : gItems) {
        if (gFilter == Category::All || item.category == gFilter) {
            items.push_back(&item);
        }
    }
    return items;
}

void DrawTopBar(HDC dc, int sx, int sy, int sw) {
    Text(dc, L"9:41", Rect(sx + 16, sy + 8, 80, 24), gFontSmall, kMuted);
    Text(dc, PlatformName(), Rect(sx + sw - 112, sy + 8, 96, 24), gFontSmall, kMuted, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void DrawBottomNav(HDC dc, int sx, int sy, int sw) {
    const int y = sy + 604;
    FillRound(dc, Rect(sx + 10, y, sw - 20, 54), 20, RGB(13, 18, 25));

    const struct NavItem { Screen screen; const wchar_t* label; } nav[] = {
        {Screen::Home, L"Home"},
        {Screen::Scan, L"Scan"},
        {Screen::Results, L"Results"},
        {Screen::Settings, L"Settings"},
    };

    const int itemW = (sw - 36) / 4;
    for (int i = 0; i < 4; ++i) {
        RECT rect = Rect(sx + 18 + i * itemW, y + 7, itemW - 6, 40);
        if (gScreen == nav[i].screen) {
            FillRound(dc, rect, 14, RGB(7, 70, 84));
        }
        Text(dc, nav[i].label, rect, gFontSmall, gScreen == nav[i].screen ? kCyan : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        AddHotspot(rect, nav[i].label, [screen = nav[i].screen]() {
            gScreen = screen;
            Log(L"Opened " + std::wstring(screen == Screen::Home ? L"Home" : screen == Screen::Scan ? L"Scan" : screen == Screen::Results ? L"Results" : L"Settings") + L" tab.");
            Refresh();
        });
    }
}

void DrawStorageCard(HDC dc, int sx, int y, int sw) {
    FillRound(dc, Rect(sx + 18, y, sw - 36, 132), 24, kSurface);
    TextAt(dc, L"Device Storage", sx + 38, y + 18, gFontHeading, kText);
    TextAt(dc, L"Total: 256 GB", sx + 38, y + 54, gFontBody, kMuted);
    TextAt(dc, L"Available: 81 GB", sx + 38, y + 82, gFontBody, kMuted);
    FillRound(dc, Rect(sx + 180, y + 62, sw - 238, 12), 8, kSurface2);
    FillRound(dc, Rect(sx + 180, y + 62, 104, 12), 8, kCyan);
}

void DrawHome(HDC dc, int sx, int sy, int sw) {
    TextAt(dc, L"CleanUp+", sx + 20, sy + 44, gFontTitle, kText);
    TextAt(dc, L"Storage cleaner and duplicate finder", sx + 20, sy + 82, gFontBody, kMuted);
    DrawStorageCard(dc, sx, sy + 120, sw);
    Button(dc, Rect(sx + 22, sy + 270, sw - 44, 48), L"Scan Storage", kCyan, RGB(6, 43, 54), [] { SimulateScan(); });

    TextAt(dc, L"Cleanup targets", sx + 20, sy + 346, gFontHeading, kText);
    const Category cats[] = {Category::DuplicatePhotos, Category::LargeFiles, Category::Screenshots, Category::Downloads, Category::EmptyFolders};
    for (int i = 0; i < 5; ++i) {
        const int rowY = sy + 382 + i * 40;
        RECT rect = Rect(sx + 20, rowY, sw - 40, 34);
        FillRound(dc, rect, 14, kSurface);
        std::wstring label = CategoryName(cats[i]) + L"  " + std::to_wstring(Count(cats[i])) + L" found";
        Text(dc, label, Rect(rect.left + 14, rect.top, rect.right - rect.left - 28, 34), gFontSmall, kText);
        AddHotspot(rect, CategoryName(cats[i]), [cat = cats[i]] {
            gFilter = cat;
            gScreen = Screen::Results;
            Log(L"Opened category " + CategoryName(cat) + L".");
            Refresh();
        });
    }
}

void DrawScan(HDC dc, int sx, int sy, int sw) {
    TextAt(dc, L"Scan Storage", sx + 20, sy + 44, gFontTitle, kText);
    TextAt(dc, L"Find duplicates, downloads and old clutter", sx + 20, sy + 82, gFontBody, kMuted);

    FillRound(dc, Rect(sx + 18, sy + 126, sw - 36, 116), 24, kSurface);
    TextAt(dc, gHasStorageAccess ? L"Storage access enabled" : L"Storage access required", sx + 38, sy + 148, gFontHeading, gHasStorageAccess ? kGreen : kRed);
    TextAt(dc, gHasStorageAccess ? L"The viewer can run a sample scan." : L"Grant access before scanning.", sx + 38, sy + 184, gFontBody, kMuted);
    if (!gHasStorageAccess) {
        Button(dc, Rect(sx + 38, sy + 204, 148, 30), L"Grant Access", kSurface2, kCyan, [] { GrantAccess(); });
    }

    FillRound(dc, Rect(sx + 18, sy + 266, sw - 36, 128), 24, kSurface);
    TextAt(dc, L"Current scan rules", sx + 38, sy + 288, gFontHeading, kText);
    TextAt(dc, L"Large files: over " + std::to_wstring(gThresholdMb) + L" MB", sx + 38, sy + 324, gFontBody, kMuted);
    TextAt(dc, gIncludeScreenshots ? L"Screenshots: included" : L"Screenshots: skipped", sx + 38, sy + 350, gFontBody, kMuted);
    TextAt(dc, gIncludeDownloads ? L"Downloads: included" : L"Downloads: skipped", sx + 38, sy + 376, gFontBody, kMuted);

    Button(dc, Rect(sx + 22, sy + 430, sw - 44, 48), L"Scan Storage", kCyan, RGB(6, 43, 54), [] { SimulateScan(); });
}

void DrawResultRow(HDC dc, int sx, int y, int sw, Item& item) {
    RECT row = Rect(sx + 18, y, sw - 36, 72);
    FillRound(dc, row, 18, item.selected ? RGB(7, 54, 66) : kSurface);
    StrokeRound(dc, Rect(row.left + 12, row.top + 25, 20, 20), 6, item.selected ? kCyan : RGB(75, 88, 102), 2);
    if (item.selected) {
        FillRound(dc, Rect(row.left + 17, row.top + 30, 10, 10), 4, kCyan);
    }
    FillRound(dc, Rect(row.left + 44, row.top + 14, 44, 44), 12, kSurface2);
    Text(dc, item.category == Category::EmptyFolders ? L"DIR" : L"FILE", Rect(row.left + 47, row.top + 14, 38, 44), gFontSmall, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    Text(dc, item.name, Rect(row.left + 104, row.top + 12, sw - 164, 22), gFontBody, kText);
    Text(dc, item.path, Rect(row.left + 104, row.top + 36, sw - 162, 18), gFontSmall, kMuted);
    Text(dc, item.size, Rect(row.right - 74, row.top + 12, 58, 22), gFontSmall, kCyan, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    AddHotspot(row, item.name, [&item] {
        item.selected = !item.selected;
        Log(std::wstring(item.selected ? L"Selected " : L"Unselected ") + item.name + L".");
        Refresh();
    });
}

void DrawResults(HDC dc, int sx, int sy, int sw) {
    TextAt(dc, L"Results", sx + 20, sy + 44, gFontTitle, kText);
    TextAt(dc, std::to_wstring(Count(Category::All)) + L" candidates / sample cleanup preview", sx + 20, sy + 82, gFontBody, kMuted);

    const Category cats[] = {Category::All, Category::DuplicatePhotos, Category::LargeFiles, Category::Screenshots, Category::Downloads};
    int chipX = sx + 18;
    for (const auto cat : cats) {
        const int width = cat == Category::DuplicatePhotos ? 92 : 72;
        Chip(dc, Rect(chipX, sy + 118, width, 30), CategoryName(cat), gFilter == cat, [cat] {
            gFilter = cat;
            Log(L"Filter set to " + CategoryName(cat) + L".");
            Refresh();
        });
        chipX += width + 8;
    }

    Button(dc, Rect(sx + 18, sy + 162, 142, 34), L"Keep newest", kSurface2, kCyan, [] { KeepNewest(); });
    Button(dc, Rect(sx + 174, sy + 162, sw - 192, 34), L"Delete Selected", kRed, RGB(50, 8, 8), [] { DeleteSelected(); });

    auto items = FilteredItems();
    if (items.empty()) {
        FillRound(dc, Rect(sx + 18, sy + 230, sw - 36, 118), 24, kSurface);
        Text(dc, L"No results yet. Press Scan Storage.", Rect(sx + 38, sy + 270, sw - 76, 32), gFontBody, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    int y = sy + 214;
    const int maxRows = std::min<int>(5, static_cast<int>(items.size()));
    for (int i = 0; i < maxRows; ++i) {
        DrawResultRow(dc, sx, y, sw, *items[i]);
        y += 82;
    }
}

void DrawSettings(HDC dc, int sx, int sy, int sw) {
    TextAt(dc, L"Settings", sx + 20, sy + 44, gFontTitle, kText);
    TextAt(dc, L"Tune scan rules and appearance", sx + 20, sy + 82, gFontBody, kMuted);

    FillRound(dc, Rect(sx + 18, sy + 126, sw - 36, 106), 24, kSurface);
    TextAt(dc, L"Large file threshold", sx + 38, sy + 148, gFontHeading, kText);
    TextAt(dc, std::to_wstring(gThresholdMb) + L" MB", sx + 38, sy + 184, gFontBody, kCyan);
    Button(dc, Rect(sx + 176, sy + 178, 54, 30), L"-25", kSurface2, kText, [] {
        gThresholdMb = std::max(50, gThresholdMb - 25);
        Log(L"Threshold changed to " + std::to_wstring(gThresholdMb) + L" MB.");
        Refresh();
    });
    Button(dc, Rect(sx + 240, sy + 178, 54, 30), L"+25", kSurface2, kText, [] {
        gThresholdMb = std::min(2048, gThresholdMb + 25);
        Log(L"Threshold changed to " + std::to_wstring(gThresholdMb) + L" MB.");
        Refresh();
    });

    const struct SettingToggle { const wchar_t* label; bool* value; } toggles[] = {
        {L"Include screenshots", &gIncludeScreenshots},
        {L"Include downloads", &gIncludeDownloads},
        {L"Dark theme", &gDarkTheme},
    };

    for (int i = 0; i < 3; ++i) {
        const int y = sy + 256 + i * 72;
        RECT card = Rect(sx + 18, y, sw - 36, 56);
        FillRound(dc, card, 20, kSurface);
        Text(dc, toggles[i].label, Rect(card.left + 18, card.top, 210, 56), gFontBody, kText);
        RECT swRect = Rect(card.right - 76, card.top + 14, 52, 28);
        FillRound(dc, swRect, 16, *toggles[i].value ? RGB(7, 70, 84) : kSurface2);
        FillRound(dc, Rect(swRect.left + (*toggles[i].value ? 27 : 5), swRect.top + 5, 18, 18), 12, *toggles[i].value ? kCyan : kMuted);
        AddHotspot(card, toggles[i].label, [value = toggles[i].value, label = std::wstring(toggles[i].label)] {
            *value = !*value;
            Log(label + (*value ? L" enabled." : L" disabled."));
            Refresh();
        });
    }

    FillRound(dc, Rect(sx + 18, sy + 494, sw - 36, 72), 24, kSurface);
    TextAt(dc, L"About CleanUp+", sx + 38, sy + 510, gFontHeading, kText);
    TextAt(dc, L"Viewer file: " + AppFileName(), sx + 38, sy + 540, gFontSmall, kMuted);
}

void DrawPhone(HDC dc) {
    const int px = 36;
    const int py = 18;
    const int pw = 394;
    const int ph = 724;
    FillRound(dc, Rect(px, py, pw, ph), 42, RGB(1, 5, 10));
    StrokeRound(dc, Rect(px, py, pw, ph), 42, RGB(55, 64, 76), 2);

    const int sx = px + 22;
    const int sy = py + 28;
    const int sw = pw - 44;
    const int sh = ph - 58;
    FillRound(dc, Rect(sx, sy, sw, sh), 30, kBg);

#if defined(VIEWER_IOS)
    FillRound(dc, Rect(sx + 126, sy + 8, 98, 24), 14, RGB(0, 0, 0));
#else
    FillRound(dc, Rect(sx + 142, sy + 10, 64, 6), 6, RGB(55, 64, 76));
#endif

    DrawTopBar(dc, sx, sy, sw);

    switch (gScreen) {
        case Screen::Home: DrawHome(dc, sx, sy, sw); break;
        case Screen::Scan: DrawScan(dc, sx, sy, sw); break;
        case Screen::Results: DrawResults(dc, sx, sy, sw); break;
        case Screen::Settings: DrawSettings(dc, sx, sy, sw); break;
    }

    DrawBottomNav(dc, sx, sy, sw);
}

void DrawConsole(HDC dc, int windowWidth) {
    const int x = 462;
    const int y = 18;
    const int w = std::max(520, windowWidth - x - 28);
    const int h = 724;
    FillRound(dc, Rect(x, y, w, h), 26, kConsole);
    StrokeRound(dc, Rect(x, y, w, h), 26, RGB(44, 55, 68));
    TextAt(dc, L"Viewer console", x + 24, y + 22, gFontHeading, kText);
    TextAt(dc, PlatformName() + L" preview / " + AppFileName(), x + 24, y + 56, gFontBody, kMuted);
    Button(dc, Rect(x + w - 122, y + 24, 86, 30), L"Clear", kSurface2, kText, [] {
        gLogs.clear();
        Log(L"Console cleared.");
        Refresh();
    });

    FillRound(dc, Rect(x + 20, y + 96, w - 40, h - 122), 18, RGB(2, 6, 10));
    int lineY = y + 114;
    const int start = std::max(0, static_cast<int>(gLogs.size()) - 24);
    for (int i = start; i < static_cast<int>(gLogs.size()); ++i) {
        Text(dc, gLogs[i], Rect(x + 36, lineY, w - 72, 22), gFontMono, i == static_cast<int>(gLogs.size()) - 1 ? kCyan : kMuted);
        lineY += 24;
    }
}

void Paint(HWND hwnd) {
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client{};
    GetClientRect(hwnd, &client);
    FillRound(dc, client, 0, RGB(10, 14, 20));
    gHotspots.clear();
    DrawPhone(dc);
    DrawConsole(dc, client.right - client.left);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            gWindow = hwnd;
            gFontTitle = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontHeading = CreateFontW(20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontBody = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontSmall = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            gFontMono = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
            Log(L"Viewer started for " + PlatformName() + L".");
            Log(L"Click Scan Storage, tabs, rows and settings controls.");
            break;
        case WM_PAINT:
            Paint(hwnd);
            return 0;
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            for (auto it = gHotspots.rbegin(); it != gHotspots.rend(); ++it) {
                if (Contains(it->rect, x, y)) {
                    it->onClick();
                    return 0;
                }
            }
            return 0;
        }
        case WM_DESTROY:
            DeleteObject(gFontTitle);
            DeleteObject(gFontHeading);
            DeleteObject(gFontBody);
            DeleteObject(gFontSmall);
            DeleteObject(gFontMono);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const std::wstring className = L"CleanUpPlusViewerWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className.c_str();
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(10, 14, 20));
    RegisterClassW(&wc);

    const std::wstring title = L"CleanUp+ " + PlatformName() + L" Viewer";
    HWND hwnd = CreateWindowExW(
        0,
        className.c_str(),
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080,
        800,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
