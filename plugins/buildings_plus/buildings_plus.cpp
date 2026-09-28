// buildings_plus - new buildings declared in a config file, as a TesmioLoader plugin.
//
// A new building needs no reverse engineering. The game's own Workshop format
// describes one completely: a folder of seven files, five of which are byte
// copies of a base-game asset and one of which is the donor's own building.ini
// with a few lines changed. This plugin writes that folder from one section of
// plugins\buildings_plus.ini before the game has run a single instruction:
//
//   [salt_mine]
//   donor  = bauxite_mine
//   object = SaltMine
//   name   = Salt Mine
//   line   = $PRODUCTION raw_salt 1.0
//
// The Workshop id of such a section is assigned by the plugin (9300000000
// upwards, the highest number found plus one) and kept in
// plugins\buildings_plus.ids.ini under the section name, so it never changes
// once given; an explicit `id =` still wins.
//
// It patches nothing: no hook, no import swap, no game structure. The loader is
// injected into a suspended process and plugins are initialised from DllMain,
// so a folder generated at startup is a folder the game finds when it scans
// media_soviet\workshop_wip.
//
// Fork of the buildings plugin from TesmioLoader (MaxLegend, GPL v3) with the
// same section format that Soviet Mod Loader reads from a mod's
// tesmio\buildings.ini, so one declaration serves both loaders. Differences of
// this fork: configuration falls back to the INI beside the DLL, a detail log
// under logs\, growable limits, validation of every declared value, atomic
// file writes, cleanup of stale object folders and - on request - of folders
// whose declaration disappeared (prune), and wide-character paths so a game
// folder with non-ASCII characters works.
//
// WHAT IS COPIED AND WHAT IS REWRITTEN
//
//   media_soviet\buildings\<donor>.nmf          -> <item>\<object>\model.nmf
//   media_soviet\buildings_types\<donor>.bbox   -> <item>\<object>\building.bbox
//   media_soviet\buildings_types\<donor>.fire   -> <item>\<object>\building.fire
//   media_soviet\editor\tool_<donor>.png        -> <item>\<object>\imagegui.png
//                                                  and <item>\previewimage.png
//   media_soviet\buildings\<donor>.mtl          -> <item>\material.mtl
//   media_soviet\buildings\<donor>_e.mtl        -> <item>\material_e.mtl
//   media_soviet\buildings_types\<donor>.ini    -> <item>\<object>\building.ini
//
// The two .mtl copies are rewritten: $TEXTURE_MTL resolves next to the .mtl,
// which in a Workshop item is the mod's own folder, so every such line becomes
// `$TEXTURE <slot> buildings/<path>`, which resolves against media_soviet.
//
// building.ini starts from the donor's own file and changes only the economy.
// Everything geometric ($CONNECTION_*, $COST_WORK*, $VEHICLE_STATION,
// $PARTICLE, $TEXT_CAPTION) is measured against the copied mesh and survives
// verbatim; a donor line is dropped only when a declared line replaces it.
// Most tokens replace only themselves. Four families replace each other:
//   $NAME / $NAME_STR                      a name is a name
//   $TYPE_*                                exactly one may be in effect
//   $STORAGE* + $RESOURCE_VISUALIZATION    the visualisation counts storages
//                                          from zero, so re-declaring them
//                                          would silently move every pile
//   $PRODUCTION, $CONSUMPTION,             a recipe is replaced whole
//   $CONSUMPTION_PER_SECOND
// `strip = $TOKEN` drops anything else. A building.ini has no comment syntax:
// the first $TOKEN anywhere in a line is what the game's parser matches.
//
// WHERE THE FOLDER GOES
//
// media_soviet\workshop_wip\<id>, the folder the game scans for unpublished
// Workshop items. It cannot be served from the loader's VFS: the game finds
// items by listing that directory, and the VFS redirects opens, not listings.
// Every generated folder carries tesmioloader.stamp; a folder without one is
// never touched, so an id that collides with a real subscription is refused.

#include "../../src/tesmio_plugin.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>

// Optional text service. A `name =` that looks like a localisation key is
// resolved to the text id the game's own $NAME takes, so a generated building
// takes its caption from the Localization pack like a base-game one. Without
// the service the name stays a literal and nothing is lost.
#ifndef TSM_SERVICE_LOCALIZATION
#define TSM_SERVICE_LOCALIZATION "localization"
#define TSM_LOCALIZATION_VERSION 1u
typedef struct TsmLocalizationApi
{
    int (*resolve)(const char* nameSpace, const char* key);
    int (*resolveFull)(const char* fullyQualifiedKey);
} TsmLocalizationApi;
#endif

#define PLUGIN_VERSION    "0.1.10"
#define PLUGIN_INI        "plugins\\buildings_plus.ini"
#define PLUGIN_LOG_NAME   "tesmioloader.buildings_plus.log"
#define STAMP_NAME        "tesmioloader.stamp"
#define STAMP_MARK        "buildings_plus generated this folder"
#define GENERATOR_VERSION 5
// A Workshop donor is looked for here first, then in media_soviet\workshop_wip.
#define WRSR_APP_ID       L"784150"
#define MAX_DECLS         256
#define MAX_LINES         512
#define MAX_LINE_LEN      4096
#define MAX_TEXT_FILE     (4u * 1024 * 1024)
#define MAX_STRIPS        64
#define MAX_COPY_FILES    4096          // files copied from one Workshop item
#define MAX_COPY_DEPTH    16            // folder depth walked inside an item
#define MAX_ASSET_FILE    (256ull * 1024 * 1024)
#define MIN_ITEM_ID       9000000000ULL
#define MAX_ITEM_ID       9999999999ULL
// Sections without `id =` get one from this range, chosen clear of Soviet Mod
// Loader (9100000000..9199999999) and of the numbers in Tesmio's examples.
#define AUTO_MIN_ID       9300000000ULL
#define AUTO_MAX_ID       9399999999ULL
// Reserved by Soviet Mod Loader for its own generated buildings. It refuses to start the game
// when workshop_wip holds a folder in this range that is not in its plan, so nothing of ours
// may ever land there - not automatically and not by hand.
#define SML_MIN_ID        9100000000ULL
#define SML_MAX_ID        9199999999ULL
// Assigned ids are remembered here, next to the plugin INI, so a building keeps
// its number for as long as its section name exists. The number is part of the
// folder path a saved game refers to.
#define CATALOG_NAME      "buildings_plus.ids.ini"

// ---------------------------------------------------------------- declarations

struct Decl
{
    std::string section;
    std::string id;        // the Workshop item id, and the folder name
    std::string object;    // the object subfolder, named by $OBJECT_BUILDING
    std::string donor;     // a base-game buildings_types name, no extension
    std::string name;      // a caption or a localisation key; also the Workshop item name
    std::string nameLine;  // what that becomes: $NAME <id> or $NAME_STR "..."
    std::string desc;      // the Workshop description, lines joined with \n
    int  life = 3000;      // renderconfig LIFE
    bool enabled = true;
    bool valid = true;
    std::vector<std::string> lines;    // building.ini lines to put in
    std::vector<std::string> strips;   // extra tokens to take out
    size_t configLine = 0;
};

static std::vector<Decl> g_decls;

static bool g_enabled = true;
static bool g_always  = false;   // regenerate even when the stamp matches
static bool g_verbose = false;   // one detail-log line per copied file and dropped donor line
// Consumed in the start phase, when every plugin's init has run. Null is the
// normal case for anyone without the Localization plugin.
static const TsmLocalizationApi* g_localization = NULL;
static bool g_prune   = false;   // remove our stamped folders whose declaration is gone
// Put a missing Steam owner id into generated folders of other generators too.
// On by default: without it the game asks about missing Workshop items on every
// single save load, and there is nothing the player can do about it.
static bool g_repairOwners = true;
static std::wstring g_outDir = L"media_soviet\\workshop_wip";
static std::wstring g_gameDir;
static std::wstring g_media;
#ifdef BUILDINGS_PLUS_TEST
static std::wstring g_testGameDir;
#endif

static HANDLE g_detail = INVALID_HANDLE_VALUE;
static int g_warnings = 0;
static int g_errors = 0;

enum GenResult { GEN_GENERATED, GEN_UP_TO_DATE, GEN_SKIPPED, GEN_FAILED };

// ---------------------------------------------------------------- text and paths

static std::wstring Wide(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &w[0], n);
    return w;
}

static std::wstring WideAnsi(const char* ansi)
{
    if (!ansi || !*ansi) return std::wstring();
    int n = MultiByteToWideChar(CP_ACP, 0, ansi, -1, NULL, 0);
    if (n <= 0) return std::wstring();
    std::wstring w((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, &w[0], n);
    return w;
}

static std::string Narrow(const std::wstring& w)
{
    if (w.empty()) return std::string();
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::string TrimCopy(const std::string& s)
{
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n')) e--;
    return s.substr(b, e - b);
}

static std::string FormatV(const char* fmt, va_list ap)
{
    int n = _vscprintf(fmt, ap);
    if (n < 0) return std::string();
    std::string s((size_t)n, '\0');
    if (n > 0) _vsnprintf_s(&s[0], (size_t)n + 1, _TRUNCATE, fmt, ap);
    return s;
}

static std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t nl = text.find('\n', start);
        std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        out.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    // A trailing newline produces one empty last line; the game's files end that way.
    if (!out.empty() && out.back().empty() && text.size() && text[text.size() - 1] == '\n') out.pop_back();
    return out;
}

static bool FileExists(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirExists(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool EnsureDir(const std::wstring& p)
{
    if (CreateDirectoryW(p.c_str(), NULL)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS && DirExists(p);
}

// ---------------------------------------------------------------- logging

static void DetailLine(const char* level, const std::string& text)
{
    if (g_detail == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char stamp[64];
    _snprintf_s(stamp, sizeof(stamp), _TRUNCATE, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s",
                t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, level);
    DWORD put = 0;
    WriteFile(g_detail, stamp, (DWORD)strlen(stamp), &put, NULL);
    WriteFile(g_detail, text.c_str(), (DWORD)text.size(), &put, NULL);
    WriteFile(g_detail, "\r\n", 2, &put, NULL);
}

// Detail log only.
static void Info(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt); std::string s = FormatV(fmt, ap); va_end(ap);
    DetailLine("INFO  ", s);
}

// Detail log and tesmioloader.log: the lines a player should see without opening the detail log.
static void Note(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt); std::string s = FormatV(fmt, ap); va_end(ap);
    DetailLine("INFO  ", s);
    Logf("buildings_plus  %s", s.c_str());
}

static void Warn(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt); std::string s = FormatV(fmt, ap); va_end(ap);
    g_warnings++;
    DetailLine("WARN: ", s);
    Logf("buildings_plus  WARN: %s", s.c_str());
}

static void Error(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt); std::string s = FormatV(fmt, ap); va_end(ap);
    g_errors++;
    DetailLine("ERROR: ", s);
    Logf("buildings_plus  ERROR: %s", s.c_str());
}

// ---------------------------------------------------------------- the name

// A name that is a localisation key rather than a caption: at least one dot and
// nothing but the characters a key may carry. "Large Medicine Factory" can
// never be one because of the spaces, "localization.lang.medicine" is one.
static bool LooksLikeKey(const std::string& s)
{
    if (s.empty() || s.find('.') == std::string::npos) return false;
    for (size_t i = 0; i < s.size(); i++)
    {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return s[0] != '.' && s[s.size() - 1] != '.';
}

// What a key that cannot be resolved falls back to: the part after the last
// dot. A build menu reading "medicine_factory" is poor; one reading the whole
// key is worse, and an empty name would look like a broken building.
static std::string KeyTail(const std::string& s)
{
    size_t dot = s.rfind('.');
    return dot == std::string::npos ? s : s.substr(dot + 1);
}

// The one name line a declaration produces, or nothing when it declares no
// name. $NAME takes an id into the language files and is what every base-game
// building uses; $NAME_STR takes a literal. Never both: the game's parser reads
// each of them into the same field and which one would win is not established.
// Resolution happens once per generation, never while the game draws.
static std::string NameLine(const Decl& d)
{
    if (d.name.empty()) return std::string();
    if (!LooksLikeKey(d.name)) return "$NAME_STR \"" + d.name + "\"";

    int id = 0;
    if (g_localization && g_localization->resolveFull) id = g_localization->resolveFull(d.name.c_str());
    if (id >= 2000000 && id <= 2999999)
    {
        char buf[32];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%d", id);
        return "$NAME " + std::string(buf);
    }
    Warn("[%s] name '%s' did not resolve%s - using \"%s\"", d.section.c_str(), d.name.c_str(),
         g_localization ? "" : " (no localization plugin)", KeyTail(d.name).c_str());
    return "$NAME_STR \"" + KeyTail(d.name) + "\"";
}

// ---------------------------------------------------------------- files

// Reads a whole text file. Every file this touches is an ini or an mtl of a few
// kilobytes; the cap keeps a wrong path from asking for a gigabyte.
static bool ReadTextFile(const std::wstring& path, std::string* out)
{
    out->clear();
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > MAX_TEXT_FILE) { CloseHandle(h); return false; }
    out->resize((size_t)size.QuadPart);
    DWORD got = 0;
    BOOL ok = size.QuadPart == 0 ? TRUE : ReadFile(h, &(*out)[0], (DWORD)size.QuadPart, &got, NULL);
    CloseHandle(h);
    if (!ok || got != (DWORD)size.QuadPart) { out->clear(); return false; }
    // A UTF-8 byte order mark is not part of the text.
    if (out->size() >= 3 && (unsigned char)(*out)[0] == 0xEF && (unsigned char)(*out)[1] == 0xBB && (unsigned char)(*out)[2] == 0xBF) out->erase(0, 3);
    return true;
}

// Written to a temporary name first and moved into place, so a crash halfway
// never leaves the game a half-written building.ini.
// The Windows error of the last failed write, taken BEFORE the temporary file is
// cleaned up - DeleteFileW would otherwise overwrite it and the log would name
// the wrong cause.
static DWORD g_ioError;
static bool WriteTextFileAtomic(const std::wstring& path, const std::string& text)
{
    std::wstring tmp = path + L".buildings_plus.tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { g_ioError = GetLastError(); return false; }
    DWORD wrote = 0;
    BOOL ok = text.empty() ? TRUE : WriteFile(h, text.c_str(), (DWORD)text.size(), &wrote, NULL);
    if (ok) ok = FlushFileBuffers(h);
    if (!ok) g_ioError = GetLastError();
    CloseHandle(h);
    if (!ok || wrote != (DWORD)text.size()) { if (ok) g_ioError = ERROR_WRITE_FAULT; DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    { g_ioError = GetLastError(); DeleteFileW(tmp.c_str()); return false; }
    return true;
}

static bool CopyAsset(const std::wstring& src, const std::wstring& dst, bool required, const char* what)
{
    if (!FileExists(src))
    {
        if (required) Error("%s not found: %s", what, Narrow(src).c_str());
        else if (g_verbose) Info("no %s (%s) - skipped", what, Narrow(src).c_str());
        return !required;
    }
    // A donor file is a mesh, a texture or an ini - never anything that needs a
    // quarter of a gigabyte. A bigger file is a mistake, not an asset.
    {
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fa))
        {
            unsigned long long size = ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
            if (size > MAX_ASSET_FILE)
            {
                Error("%s is %llu MB, more than the %u MB a donor file may have - not copied: %s", what, size >> 20, (unsigned)(MAX_ASSET_FILE >> 20), Narrow(src).c_str());
                return false;
            }
        }
    }
    if (!CopyFileW(src.c_str(), dst.c_str(), FALSE))
    {
        Error("could not copy %s -> %s (%lu)", Narrow(src).c_str(), Narrow(dst).c_str(), GetLastError());
        return false;
    }
    if (g_verbose) Info("%s <- %s", Narrow(dst).c_str(), Narrow(src).c_str());
    return true;
}

// Removes a folder tree we wrote ourselves. Junctions and symbolic links are
// removed as links, never followed.
static bool DeleteTree(const std::wstring& dir)
{
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            std::wstring n = fd.cFileName;
            if (n == L"." || n == L"..") continue;
            std::wstring p = dir + L"\\" + n;
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) DeleteTree(p);
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p.c_str());
            else { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(p.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return RemoveDirectoryW(dir.c_str()) != 0;
}

static std::vector<std::wstring> SubDirs(const std::wstring& dir)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do
    {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        // A junction or symlink is never followed - a loop in a Workshop item would
        // otherwise be walked until the stack runs out.
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) out.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

// FNV-1a over everything that decides the output, so a folder is rebuilt when
// its declaration or a donor file changed and left alone when nothing did.
static unsigned long long HashBytes(unsigned long long h, const void* p, size_t n)
{
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; }
    return h;
}

static unsigned long long HashStr(unsigned long long h, const std::string& s)
{
    return HashBytes(h, s.c_str(), s.size() + 1);
}

static unsigned long long HashFileStamp(unsigned long long h, const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa))
    {
        h = HashBytes(h, &fa.nFileSizeLow, sizeof(fa.nFileSizeLow));
        h = HashBytes(h, &fa.ftLastWriteTime, sizeof(fa.ftLastWriteTime));
    }
    return h;
}

// ---------------------------------------------------------------- ini tokens

// The first $TOKEN anywhere in the line, which is what the game's own parser
// matches - a `//` in front of one does not make it a comment. Only the
// upper-case-digits-underscore run is taken.
static std::string FirstToken(const std::string& line)
{
    size_t p = line.find('$');
    if (p == std::string::npos) return std::string();
    std::string out = "$";
    for (size_t q = p + 1; q < line.size(); q++)
    {
        char c = line[q];
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') out += c;
        else break;
    }
    return out.size() > 1 ? out : std::string();
}

static bool StartsWith(const std::string& s, const char* prefix)
{
    return s.compare(0, strlen(prefix), prefix) == 0;
}

static bool IsRecipeToken(const std::string& t)
{
    return t == "$PRODUCTION" || t == "$CONSUMPTION" || t == "$CONSUMPTION_PER_SECOND";
}

// Does a declared line opening with `mine` replace a donor line opening with
// `theirs`? Equality plus the four families from the header comment.
static bool Replaces(const std::string& mine, const std::string& theirs)
{
    if (mine == theirs) return true;
    if (StartsWith(mine, "$TYPE_") && StartsWith(theirs, "$TYPE_")) return true;
    if (StartsWith(mine, "$NAME") && StartsWith(theirs, "$NAME")) return true;
    if (StartsWith(mine, "$STORAGE") && (StartsWith(theirs, "$STORAGE") || theirs == "$RESOURCE_VISUALIZATION")) return true;
    if (IsRecipeToken(mine) && IsRecipeToken(theirs)) return true;
    return false;
}

static bool DonorLineIsReplaced(const Decl& d, const std::string& token)
{
    // `name =` is emitted as a $NAME_STR without being a `line =`.
    if (!d.name.empty() && Replaces("$NAME_STR", token)) return true;
    for (size_t i = 0; i < d.lines.size(); i++)
    {
        std::string mine = FirstToken(d.lines[i]);
        if (!mine.empty() && Replaces(mine, token)) return true;
    }
    for (size_t i = 0; i < d.strips.size(); i++)
        if (Replaces(d.strips[i], token)) return true;
    return false;
}

// ---------------------------------------------------------------- the donor

// A donor is either a base-game building - a plain name under
// media_soviet\buildings_types - or one building of a Workshop item, written
// `<item>\<object>`, the spelling Vanilla Buildings uses for its targets. The
// second kind is searched in the game's Workshop folder and in
// media_soviet\workshop_wip, so an unpublished building of your own serves too.
//
// Nothing of a Workshop donor is shipped with this plugin: the copy is made on
// the player's own machine from the item he is subscribed to.
struct DonorPaths
{
    bool fromItem;            // false: a base-game name
    std::wstring itemDir;     // <workshop root>\<item>
    std::wstring objectDir;   // <item>\<object>
    std::wstring ini;         // the donor's building.ini
    std::string  objectName;  // the folder name the donor's author chose
    DonorPaths() : fromItem(false) {}
};

// One separator, so a donor names one building and never a path into a tree.
static bool SplitDonor(const std::string& donor, std::string* item, std::string* object)
{
    size_t at = donor.find_first_of("\\/");
    if (at == std::string::npos) return false;
    *item = TrimCopy(donor.substr(0, at));
    *object = TrimCopy(donor.substr(at + 1));
    return object->find_first_of("\\/") == std::string::npos;
}

// The folder names inside somebody else's item are not ours to dictate - spaces
// and dots occur. What must not occur is anything that leaves the folder.
static bool SafeDonorSegment(const std::string& s)
{
    if (s.empty() || s.size() > 96) return false;
    if (s.find("..") != std::string::npos) return false;
    if (s[0] == ' ' || s[0] == '.' || s[s.size() - 1] == ' ' || s[s.size() - 1] == '.') return false;
    for (size_t i = 0; i < s.size(); i++)
    {
        char c = s[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.' || c == ' ' || c == '+' || c == '(' || c == ')';
        if (!ok) return false;
    }
    return true;
}

static std::vector<std::wstring> WorkshopRoots()
{
    std::vector<std::wstring> roots;
    // <library>\steamapps\common\SovietRepublic -> <library>\steamapps\workshop\content\784150
    size_t slash = g_gameDir.find_last_of(L'\\');
    if (slash != std::wstring::npos)
    {
        std::wstring common = g_gameDir.substr(0, slash);
        size_t up = common.find_last_of(L'\\');
        if (up != std::wstring::npos) roots.push_back(common.substr(0, up) + L"\\workshop\\content\\" + WRSR_APP_ID);
    }
    roots.push_back(g_media + L"\\workshop_wip");
    return roots;
}

// True when the donor was found. A donor that looks like an item but is not
// there fills in the name, so the error message can say what was looked for.
static bool ResolveDonor(const std::string& donor, DonorPaths* out)
{
    std::string itemName, objectName;
    *out = DonorPaths();
    if (!SplitDonor(donor, &itemName, &objectName))
    {
        out->ini = g_media + L"\\buildings_types\\" + Wide(donor) + L".ini";
        return FileExists(out->ini);
    }
    out->fromItem = true;
    out->objectName = objectName;
    std::vector<std::wstring> roots = WorkshopRoots();
    for (size_t i = 0; i < roots.size(); i++)
    {
        std::wstring itemDir = roots[i] + L"\\" + Wide(itemName);
        std::wstring objDir = itemDir + L"\\" + Wide(objectName);
        if (FileExists(objDir + L"\\building.ini"))
        {
            out->itemDir = itemDir; out->objectDir = objDir; out->ini = objDir + L"\\building.ini";
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- copying an item

struct CopyItem { std::wstring rel; std::wstring src; };

static std::vector<std::wstring> FilesIn(const std::wstring& dir)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do
    {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

static bool IsObjectDir(const std::wstring& dir) { return FileExists(dir + L"\\building.ini"); }

// Bounded: a Workshop item is a few folders deep and holds a few dozen files;
// anything past these limits is reported once and left out rather than walked
// without end.
static void CollectTree(const std::wstring& src, const std::wstring& rel, std::vector<CopyItem>* out, int depth = 0)
{
    if (depth > MAX_COPY_DEPTH) { Warn("%s lies deeper than %d folders - not copied", Narrow(src).c_str(), MAX_COPY_DEPTH); return; }
    std::vector<std::wstring> files = FilesIn(src);
    for (size_t i = 0; i < files.size(); i++)
    {
        if (out->size() >= MAX_COPY_FILES) { Warn("more than %d files under the item - the rest is not copied", MAX_COPY_FILES); return; }
        CopyItem c; c.rel = rel.empty() ? files[i] : rel + L"\\" + files[i]; c.src = src + L"\\" + files[i];
        out->push_back(c);
    }
    std::vector<std::wstring> subs = SubDirs(src);
    for (size_t i = 0; i < subs.size(); i++)
        CollectTree(src + L"\\" + subs[i], rel.empty() ? subs[i] : rel + L"\\" + subs[i], out, depth + 1);
}

static bool LessRel(const CopyItem& a, const CopyItem& b) { return _wcsicmp(a.rel.c_str(), b.rel.c_str()) < 0; }

// Everything the clone needs: the chosen building under the declared object
// name, every loose file of the item, and every subfolder that holds no
// building of its own. Asset folders carry whatever name their author picked
// (mtl, Textures, materials, "parking acc"), so the rule asks what is IN a
// folder and never what it is called. The other buildings of a multi-building
// item stay behind - one section is one building - and the item's own
// workshopconfig.ini stays behind too, because the clone gets one of its own
// with a single $OBJECT_BUILDING line.
static std::vector<CopyItem> ItemPlan(const DonorPaths& p, const std::string& object)
{
    std::vector<CopyItem> plan;
    CollectTree(p.objectDir, Wide(object), &plan);
    std::vector<std::wstring> files = FilesIn(p.itemDir);
    for (size_t i = 0; i < files.size(); i++)
    {
        if (_wcsicmp(files[i].c_str(), Wide(STAMP_NAME).c_str()) == 0) continue;
        if (_wcsicmp(files[i].c_str(), L"workshopconfig.ini") == 0) continue;
        CopyItem c; c.rel = files[i]; c.src = p.itemDir + L"\\" + files[i];
        plan.push_back(c);
    }
    std::vector<std::wstring> subs = SubDirs(p.itemDir);
    for (size_t i = 0; i < subs.size(); i++)
    {
        std::wstring dir = p.itemDir + L"\\" + subs[i];
        if (_wcsicmp(dir.c_str(), p.objectDir.c_str()) == 0) continue;
        if (IsObjectDir(dir)) continue;
        CollectTree(dir, subs[i], &plan);
    }
    std::sort(plan.begin(), plan.end(), LessRel);
    return plan;
}

static bool EnsureParent(const std::wstring& path)
{
    size_t slash = path.find_last_of(L'\\');
    if (slash == std::wstring::npos) return true;
    std::wstring dir = path.substr(0, slash);
    if (DirExists(dir)) return true;
    EnsureParent(dir);
    return EnsureDir(dir);
}

static bool CopyPlan(const std::wstring& item, const std::vector<CopyItem>& plan, int* files)
{
    bool ok = true;
    for (size_t i = 0; i < plan.size(); i++)
    {
        std::wstring dst = item + L"\\" + plan[i].rel;
        if (!EnsureParent(dst)) { Error("could not create the folder for %s (%lu)", Narrow(dst).c_str(), GetLastError()); ok = false; continue; }
        if (CopyAsset(plan[i].src, dst, true, "donor file")) (*files)++;
        else ok = false;
    }
    return ok;
}

// ---------------------------------------------------------------- references

// `a\b\..\c.dds` -> `a\c.dds`, and forward slashes become backslashes. Returns
// false when the path climbs out of the folder it started in.
static bool NormalizeRel(const std::wstring& rel, std::wstring* out)
{
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (size_t i = 0; i <= rel.size(); i++)
    {
        wchar_t c = i < rel.size() ? rel[i] : L'\\';
        if (c == L'/' || c == L'\\')
        {
            if (cur == L"..") { if (parts.empty()) return false; parts.pop_back(); }
            else if (!cur.empty() && cur != L".") parts.push_back(cur);
            cur.clear();
        }
        else cur += c;
    }
    out->clear();
    for (size_t i = 0; i < parts.size(); i++) { if (i) *out += L"\\"; *out += parts[i]; }
    return !out->empty();
}

static std::string LowerAscii(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++) if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
    return out;
}

static bool EndsWith(const std::string& s, const char* tail)
{
    size_t n = strlen(tail);
    return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

// The two references a building writes as a path: renderconfig MODEL/MATERIAL/
// MATERIALEMISSIVE, relative to the building's folder, and $TEXTURE_MTL of a
// material, relative to that material's folder. ($TEXTURE without _MTL resolves
// against media_soviet and is on the player's disk anyway.)
static void ReferencesOf(const std::wstring& file, const std::wstring& relDir, std::vector<std::wstring>* out)
{
    std::string text;
    if (!ReadTextFile(file, &text)) return;
    std::string name = LowerAscii(Narrow(file));
    bool render = EndsWith(name, "\\renderconfig.ini");
    bool material = EndsWith(name, ".mtl");
    if (!render && !material) return;
    std::vector<std::string> lines = SplitLines(text);
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = TrimCopy(lines[i]);
        std::string ref;
        if (render)
        {
            if (StartsWith(line, "MODEL ")) ref = TrimCopy(line.substr(6));
            else if (StartsWith(line, "MATERIALEMISSIVE ")) ref = TrimCopy(line.substr(17));
            else if (StartsWith(line, "MATERIAL ")) ref = TrimCopy(line.substr(9));
        }
        else if (StartsWith(line, "$TEXTURE_MTL"))
        {
            // $TEXTURE_MTL <slot> <path>
            size_t at = line.find(' ', 12);
            if (at == std::string::npos) continue;
            at = line.find(' ', at + 1);
            if (at == std::string::npos) continue;
            ref = TrimCopy(line.substr(at + 1));
        }
        if (ref.empty() || ref.find(':') != std::string::npos) continue;
        std::wstring joined = relDir.empty() ? Wide(ref) : relDir + L"\\" + Wide(ref), normalised;
        if (NormalizeRel(joined, &normalised)) out->push_back(normalised);
    }
}

// A building may point at a file that sits beside it - a material in an asset
// folder, a texture next to that material. Everything that is not another
// building's folder travelled with the clone, so what can still be missing is a
// reference INTO another building's folder. Rather than parse the whole
// material chain, the clone is measured against its own written references and
// a missing file is fetched from the donor item under the same relative path.
static int RescueReferences(const Decl& d, const std::wstring& cloneItem, const DonorPaths& p, int* missing)
{
    int rescued = 0;
    for (int pass = 0; pass < 2; pass++)
    {
        std::vector<CopyItem> have;
        CollectTree(cloneItem, L"", &have);
        std::vector<std::wstring> wanted;
        for (size_t i = 0; i < have.size(); i++)
        {
            size_t slash = have[i].rel.find_last_of(L'\\');
            std::wstring relDir = slash == std::wstring::npos ? L"" : have[i].rel.substr(0, slash);
            ReferencesOf(have[i].src, relDir, &wanted);
        }
        int before = rescued;
        for (size_t i = 0; i < wanted.size(); i++)
        {
            std::wstring dst = cloneItem + L"\\" + wanted[i];
            if (FileExists(dst)) continue;
            std::wstring src = p.itemDir + L"\\" + wanted[i];
            if (!FileExists(src))
            {
                if (pass == 1) { Warn("[%s] %s refers to %s, which the donor item does not have", d.section.c_str(), d.donor.c_str(), Narrow(wanted[i]).c_str()); (*missing)++; }
                continue;
            }
            if (!EnsureParent(dst)) continue;
            if (CopyAsset(src, dst, false, "referenced file")) { rescued++; Info("[%s] fetched %s from the donor item", d.section.c_str(), Narrow(wanted[i]).c_str()); }
        }
        // The second pass runs even when the first fetched nothing: it is the one
        // that reports what the donor item does not have either. Breaking out early
        // used to leave a missing texture unreported.
        (void)before;
    }
    return rescued;
}

// ---------------------------------------------------------------- generation

// $TEXTURE_MTL resolves next to the .mtl file; in a Workshop item that folder
// is the mod's own and the textures are not found. Each such line becomes
// `$TEXTURE <slot> buildings/<path>`, which resolves against media_soviet.
static bool RewriteMaterial(const std::wstring& src, const std::wstring& dst, int* rewritten)
{
    *rewritten = 0;
    std::string text;
    if (!ReadTextFile(src, &text)) { Error("could not read material %s", Narrow(src).c_str()); return false; }
    std::string out;
    std::vector<std::string> lines = SplitLines(text);
    for (size_t i = 0; i < lines.size(); i++)
    {
        const std::string& line = lines[i];
        size_t at = line.find("$TEXTURE_MTL");
        if (at == std::string::npos) { out += line; out += "\r\n"; continue; }
        std::string rest = TrimCopy(line.substr(at + strlen("$TEXTURE_MTL")));
        size_t sp = rest.find_first_of(" \t");
        std::string slot = sp == std::string::npos ? rest : rest.substr(0, sp);
        std::string path = sp == std::string::npos ? std::string() : TrimCopy(rest.substr(sp));
        if (slot.empty()) slot = "0";
        out += line.substr(0, at);
        out += "$TEXTURE " + slot + " buildings/" + path + "\r\n";
        (*rewritten)++;
    }
    if (!WriteTextFileAtomic(dst, out)) { Error("could not write material %s (%lu)", Narrow(dst).c_str(), g_ioError); return false; }
    return true;
}

// The donor's building.ini with the declared block in front and every line the
// block replaces taken out. In front, because a few base files end in a bare
// `end` and nothing is known about whether the parser stops there.
static bool WriteBuildingIni(const Decl& d, const std::wstring& donorIni, const std::wstring& dst, int* dropped)
{
    *dropped = 0;
    std::string text;
    if (!ReadTextFile(donorIni, &text)) { Error("could not read donor %s", Narrow(donorIni).c_str()); return false; }
    std::string out;
    // No dollar sign anywhere in these header lines: the parser would read it as a keyword.
    out += "; generated by TesmioLoader plugins\\buildings_plus.dll - section [" + d.section + "]\r\n";
    // Where the copy started. A Workshop donor is named by item and building,
    // never by its path: this file can end up on somebody else's disk, and the
    // folder a Steam library sits in is nobody's business.
    std::string itemPart, objectPart;
    if (SplitDonor(d.donor, &itemPart, &objectPart)) out += "; donor: Workshop item " + itemPart + ", building " + objectPart + "\r\n";
    else out += "; donor: media_soviet\\buildings_types\\" + d.donor + ".ini\r\n";
    out += "; edits here are overwritten on the next launch - change buildings_plus.ini instead\r\n\r\n";
    if (!d.nameLine.empty()) out += d.nameLine + "\r\n";
    for (size_t i = 0; i < d.lines.size(); i++) out += d.lines[i] + "\r\n";
    out += "\r\n";
    std::vector<std::string> lines = SplitLines(text);
    bool dropping = false;
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string token = FirstToken(lines[i]);
        if (!token.empty())
        {
            dropping = DonorLineIsReplaced(d, token);
            if (dropping) { (*dropped)++; if (g_verbose) Info("dropped donor line: %s", lines[i].c_str()); continue; }
        }
        else if (dropping)
        {
            // A token's data lines follow it without a token of their own: the
            // points of a $CONNECTION_*, the placement of a $RESOURCE_VISUALIZATION.
            // They go with the token; a blank or separator line ends the block.
            std::string bare = TrimCopy(lines[i]);
            if (!bare.empty() && bare[0] != '-' && bare[0] != ';') { (*dropped)++; if (g_verbose) Info("dropped donor line: %s", lines[i].c_str()); continue; }
            dropping = false;
        }
        out += lines[i] + "\r\n";
    }
    if (!WriteTextFileAtomic(dst, out)) { Error("could not write %s (%lu)", Narrow(dst).c_str(), g_ioError); return false; }
    return true;
}

// ------------------------------------------------------------- Steam owner id
//
// The game's own editor writes the signed-in player's SteamID64 into
// $OWNER_ID, and a saved game that uses a building under media_soviet\
// workshop_wip is only accepted when that number is there. With $OWNER_ID 0
// every load of such a save opens "the Workshop items used in this saved game
// were not found" and logs `Trying load saved game missing items: Item ident
// <id>/<object> type 1`. Proven in the game: same folder, same save, only the
// number changed - and the warning was gone.
//
// Two sources, in this order:
//   1. HKCU\Software\Valve\Steam\ActiveProcess, value ActiveUser - the 32 bit
//      account id of the signed-in player, SteamID64 = 76561197960265728 + it.
//      Zero while the client is signed out, and then it tells us nothing.
//   2. <SteamPath>\config\loginusers.vdf - the 17 digit section marked
//      "MostRecent" "1", otherwise the first one in the file.
// Neither answers: the id stays 0, because a wrong owner is worse than none.
// advapi32 is loaded by hand so the build line keeps its single kernel32.lib.

static const unsigned long long STEAM_ID64_BASE = 76561197960265728ULL;

typedef LONG (WINAPI *PFN_RegGetValueW)(HKEY, LPCWSTR, LPCWSTR, DWORD, LPDWORD, PVOID, LPDWORD);

static bool RegRead(const wchar_t* subkey, const wchar_t* value, DWORD flags, void* data, DWORD bytes)
{
    static PFN_RegGetValueW get = NULL;
    static bool tried = false;
    if (!tried)
    {
        tried = true;
        HMODULE m = LoadLibraryW(L"advapi32.dll");
        if (m) get = (PFN_RegGetValueW)GetProcAddress(m, "RegGetValueW");
    }
    if (!get) return false;
    DWORD size = bytes;
    return get(HKEY_CURRENT_USER, subkey, value, flags, NULL, data, &size) == ERROR_SUCCESS;
}

static unsigned long long OwnerFromLoginUsers()
{
    wchar_t path[MAX_PATH];
    path[0] = 0;
    if (!RegRead(L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, path, sizeof(path))) return 0;
    std::string text;
    if (!ReadTextFile(std::wstring(path) + L"\\config\\loginusers.vdf", &text)) return 0;

    unsigned long long first = 0, current = 0;
    bool mostRecent = false;
    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] != '"') continue;
        size_t end = text.find('"', i + 1);
        if (end == std::string::npos) break;
        std::string token = text.substr(i + 1, end - i - 1);
        i = end;

        if (token.size() == 17 && token.find_first_not_of("0123456789") == std::string::npos)
        {
            current = _strtoui64(token.c_str(), NULL, 10);
            if (!first) first = current;
            mostRecent = false;
        }
        else if (_stricmp(token.c_str(), "MostRecent") == 0) mostRecent = true;
        else if (mostRecent)
        {
            if (token == "1" && current) return current;
            mostRecent = false;
        }
    }
    return first;
}

static unsigned long long g_owner = 0;
static bool g_ownerKnown = false;

static unsigned long long SteamOwner()
{
    if (g_ownerKnown) return g_owner;
    g_ownerKnown = true;
    DWORD account = 0;
    if (RegRead(L"Software\\Valve\\Steam\\ActiveProcess", L"ActiveUser", RRF_RT_REG_DWORD, &account, sizeof(account)) && account)
        g_owner = STEAM_ID64_BASE + account;
    else
        g_owner = OwnerFromLoginUsers();
    return g_owner;
}

#ifdef BUILDINGS_PLUS_TEST
static void SetTestOwner(unsigned long long id) { g_owner = id; g_ownerKnown = true; }
#endif

static bool WriteWorkshopConfig(const Decl& d, const std::wstring& dst)
{
    char owner[32];
    _snprintf_s(owner, sizeof(owner), _TRUNCATE, "%llu", SteamOwner());

    std::string out;
    out += "$ITEM_ID " + d.id + "\r\n\r\n";
    out += "$OWNER_ID " + std::string(owner) + "\r\n\r\n";
    out += "$ITEM_TYPE WORKSHOP_ITEMTYPE_BUILDING\r\n\r\n";
    out += "$VISIBILITY 0\r\n";
    out += "$OBJECT_BUILDING " + d.object + "\r\n\r\n";
    out += "$ITEM_NAME \"" + (d.name.empty() ? d.object : d.name) + "\"\r\n\r\n";
    out += "$ITEM_DESC \"" + (d.desc.empty() ? std::string("Generated by buildings_plus.") : d.desc) + "\"\r\n\r\n";
    out += "$END\r\n";
    return WriteTextFileAtomic(dst, out);
}

// A folder written before the owner id was known carries $OWNER_ID 0, and every
// saved game that uses it reports a missing Workshop item. The declaration has
// not changed, so the stamp is right and nothing else needs rewriting - this
// one file does. The owner is deliberately NOT part of the stamp hash: a start
// with the client signed out would otherwise rewrite every folder with a zero.
static void RepairOwner(const Decl& d, const std::wstring& item)
{
    if (!SteamOwner()) return;
    std::wstring path = item + L"\\workshopconfig.ini";
    std::string text;
    if (!ReadTextFile(path, &text)) return;
    if (text.find("$OWNER_ID 0\r\n") == std::string::npos &&
        text.find("$OWNER_ID 0\n")   == std::string::npos) return;
    if (WriteWorkshopConfig(d, path))
        Note("[%s] wrote the Steam owner id into %s - saved games using this building no longer report it as missing",
             d.section.c_str(), Narrow(path).c_str());
    else Warn("[%s] could not write the Steam owner id into %s (%lu) - tried again at the next start", d.section.c_str(), Narrow(path).c_str(), g_ioError);
}

// MATERIALEMISSIVE is not optional when the donor has an _e.mtl: a mesh built
// for the lit-window glow renders with a null node array without it and takes
// the process down on the first frame.
static bool WriteRenderConfig(const Decl& d, const std::wstring& dst, bool emissive)
{
    std::string out;
    out += "$TYPE_WORKSHOP\r\n";
    out += " MODEL model.nmf\r\n";
    out += " MATERIAL ../material.mtl\r\n";
    if (emissive) out += " MATERIALEMISSIVE ../material_e.mtl\r\n";
    char life[64];
    _snprintf_s(life, sizeof(life), _TRUNCATE, " LIFE %d.000000\r\n", d.life);
    out += life;
    out += " EXPLOSION_GROUP 0\r\n";
    out += " DERBIS_FALLING_FX buildingfall1 1.000000\r\n";
    out += " DERBIS_FALLED_FX buildingfall2 1.400000\r\n";
    out += " DERBIS_FALLED_SFX collapse\r\n";
    out += " DERBIS_NUM 20\r\n";
    out += " DERBIS_FALLING_FX_MAXTIME 3.000000\r\n";
    out += " DERBIS_SCALE 1.000000\r\n";
    out += " DERBIS_MESH buildings/buildingwreck1.nmf buildings/buildingwreck.mtl\r\n";
    out += " DERBIS_MESH buildings/buildingwreck2.nmf buildings/buildingwreck.mtl\r\n";
    out += " END\r\n";
    return WriteTextFileAtomic(dst, out);
}

static unsigned long long DeclHash(const Decl& d)
{
    unsigned long long h = 14695981039346656037ULL;
    int ver = GENERATOR_VERSION;
    h = HashBytes(h, &ver, sizeof(ver));
    h = HashStr(h, d.id);
    h = HashStr(h, d.object);
    h = HashStr(h, d.donor);
    // The line that really goes into the file, not the declared value: when a
    // key resolves to a different id than last time, the folder is rewritten.
    h = HashStr(h, d.nameLine);
    h = HashStr(h, d.desc);
    h = HashBytes(h, &d.life, sizeof(d.life));
    for (size_t i = 0; i < d.lines.size(); i++) h = HashStr(h, d.lines[i]);
    for (size_t i = 0; i < d.strips.size(); i++) h = HashStr(h, d.strips[i]);
    // The donor's own files, so a game patch or an updated Workshop item
    // regenerates too. Name, size and write time of every file that will be
    // copied - reading several MB of meshes at every game start would cost far
    // more than it could ever catch.
    DonorPaths p;
    if (ResolveDonor(d.donor, &p) && p.fromItem)
    {
        std::vector<CopyItem> plan = ItemPlan(p, d.object);
        for (size_t i = 0; i < plan.size(); i++)
        {
            h = HashStr(h, Narrow(plan[i].rel));
            h = HashFileStamp(h, plan[i].src);
        }
        return h;
    }
    std::wstring donor = Wide(d.donor);
    h = HashFileStamp(h, g_media + L"\\buildings_types\\" + donor + L".ini");
    h = HashFileStamp(h, g_media + L"\\buildings\\" + donor + L".mtl");
    h = HashFileStamp(h, g_media + L"\\buildings\\" + donor + L"_e.mtl");
    h = HashFileStamp(h, g_media + L"\\buildings\\" + donor + L".nmf");
    return h;
}

static std::wstring OutRoot()
{
    bool absolute = (g_outDir.size() > 1 && g_outDir[1] == L':') || (!g_outDir.empty() && (g_outDir[0] == L'\\' || g_outDir[0] == L'/'));
    return absolute ? g_outDir : g_gameDir + L"\\" + g_outDir;
}

static bool ReadStamp(const std::wstring& item, std::string* text)
{
    return ReadTextFile(item + L"\\" + Wide(STAMP_NAME), text);
}

static bool StampMatches(const std::string& stamp, unsigned long long want)
{
    size_t at = stamp.find("hash=");
    if (at == std::string::npos) return false;
    unsigned long long have = _strtoui64(stamp.c_str() + at + 5, NULL, 16);
    return have == want;
}

// The receipt of a generated folder: what it was made from and the hash that
// decides at the next start whether it still matches its declaration. A folder
// without it is somebody else's and is never touched.
static bool WriteStamp(const Decl& d, const std::wstring& item, unsigned long long want)
{
    char hash[32];
    _snprintf_s(hash, sizeof(hash), _TRUNCATE, "%016llX", want);
    std::string stampText = std::string(STAMP_MARK) + " (TesmioLoader plugins\\buildings_plus.dll).\r\n"
        "section=" + d.section + " donor=" + d.donor + " object=" + d.object + "\r\n"
        "hash=" + hash + "\r\n"
        "Delete this file to make the plugin leave the folder alone; delete the folder to have it written again.\r\n";
    if (WriteTextFileAtomic(item + L"\\" + Wide(STAMP_NAME), stampText)) return true;
    Error("[%s] could not write %s\\%s (%lu) - the folder will be written again at the next start", d.section.c_str(), Narrow(item).c_str(), STAMP_NAME, g_ioError);
    return false;
}

// The stamps this plugin may take a folder over from: its own, and the one
// Soviet Mod Loader's generator writes for the same section (0.1.9). A stamp
// of any other generator means the folder is somebody else's.
static bool OwnStamp(const std::string& stamp) { return stamp.find(STAMP_MARK) != std::string::npos; }
static bool SmlStamp(const std::string& stamp) { return stamp.find("plugins\\buildings.dll generated this folder") != std::string::npos; }

static GenResult GenerateCore(const Decl& d);
// A C++ exception out of the generator (bad_alloc, a string that grew past its
// limit) ends this building, not the loader: the loader is plain C and has no
// handler for it.
static GenResult Generate(const Decl& d)
{
    try { return GenerateCore(d); }
    catch (const std::exception& e) { Error("[%s] C++ exception while generating: %s - skipped", d.section.c_str(), e.what()); }
    catch (...) { Error("[%s] unknown C++ exception while generating - skipped", d.section.c_str()); }
    return GEN_FAILED;
}

static GenResult GenerateCore(const Decl& d)
{
    DonorPaths p;
    if (!ResolveDonor(d.donor, &p))
    {
        if (p.fromItem)
            Error("[%s] donor \"%s\" was not found - no such building under the Workshop folder or media_soviet\\workshop_wip; is the item subscribed?",
                  d.section.c_str(), d.donor.c_str());
        else
            Error("[%s] donor \"%s\" has no %s - nothing generated", d.section.c_str(), d.donor.c_str(), Narrow(p.ini).c_str());
        return GEN_FAILED;
    }
    std::wstring donor = Wide(d.donor);
    std::wstring donorIni = p.ini;

    std::wstring outRoot = OutRoot();
    std::wstring item = outRoot + L"\\" + Wide(d.id);
    std::wstring obj = item + L"\\" + Wide(d.object);
    unsigned long long want = DeclHash(d);

    // An id collision with a real Workshop item would otherwise overwrite
    // somebody's subscription. A folder we did not write has no stamp.
    if (DirExists(item))
    {
        std::string stamp;
        if (!ReadStamp(item, &stamp))
        {
            Error("[%s] %s exists and was not written by this plugin (no %s) - refusing to touch it; give the section another id",
                  d.section.c_str(), Narrow(item).c_str(), STAMP_NAME);
            return GEN_FAILED;
        }
        if (!OwnStamp(stamp) && !SmlStamp(stamp))
        {
            Error("[%s] %s carries the stamp of another generator - refusing to touch it; give the section another id",
                  d.section.c_str(), Narrow(item).c_str());
            return GEN_FAILED;
        }
        if (!g_always && StampMatches(stamp, want))
        {
            RepairOwner(d, item);
            Info("[%s] -> %s up to date", d.section.c_str(), d.id.c_str());
            return GEN_UP_TO_DATE;
        }
        // 0.1.9: the folder SML wrote for the same section (adopted number) is written again
        // from our declaration; SML will do the same with its own stamp when it runs next.
        if (SmlStamp(stamp))
            Info("[%s] %s was written by Soviet Mod Loader's generator - taking it over", d.section.c_str(), d.id.c_str());
        if (p.fromItem)
        {
            // A Workshop donor brings its own file names along, so files of an
            // earlier donor would linger beside the new ones. The folder is
            // ours - it carries our stamp - and is written from scratch.
            Info("[%s] rebuilding %s from the donor item", d.section.c_str(), Narrow(item).c_str());
            if (!DeleteTree(item))
            {
                Error("[%s] could not clear %s for the rebuild (%lu) - skipped", d.section.c_str(), Narrow(item).c_str(), GetLastError());
                return GEN_FAILED;
            }
            // The stamp goes back first, with a hash that matches nothing: should a
            // write below fail, the folder is still ours and is simply written again
            // at the next start instead of being refused as somebody else's for good.
            if (!EnsureDir(item) || !WriteStamp(d, item, 0)) return GEN_FAILED;
        }
        else
        {
            // A renamed object leaves its old subfolder behind otherwise.
            std::vector<std::wstring> subs = SubDirs(item);
            for (size_t i = 0; i < subs.size(); i++)
                if (_wcsicmp(subs[i].c_str(), Wide(d.object).c_str()) != 0)
                {
                    Info("[%s] removing stale object folder %s", d.section.c_str(), Narrow(subs[i]).c_str());
                    if (!DeleteTree(item + L"\\" + subs[i]))
                        Warn("[%s] could not remove the stale folder %s (%lu)", d.section.c_str(), Narrow(subs[i]).c_str(), GetLastError());
                }
        }
    }

    // media_soviet\workshop_wip may itself be absent on a clean install.
    EnsureDir(outRoot);
    if (!EnsureDir(item)) { Error("[%s] could not create %s (%lu)", d.section.c_str(), Narrow(item).c_str(), GetLastError()); return GEN_FAILED; }
    if (!EnsureDir(obj))  { Error("[%s] could not create %s (%lu)", d.section.c_str(), Narrow(obj).c_str(), GetLastError()); return GEN_FAILED; }

    bool ok = true;
    int dropped = 0;
    if (p.fromItem)
    {
        // The clone is the donor item's own layout with one building in it:
        // every file is copied as it lies, and building.ini is the only one
        // that is rewritten. Nothing has to be known about where that item
        // keeps its materials and textures.
        std::vector<CopyItem> plan = ItemPlan(p, d.object);
        int copied = 0;
        ok &= CopyPlan(item, plan, &copied);
        ok &= WriteBuildingIni(d, donorIni, obj + L"\\building.ini", &dropped);
        // The donor's renderconfig names its own mesh and material and came
        // with the copy; only an item without one gets ours.
        if (!FileExists(obj + L"\\renderconfig.ini"))
            ok &= WriteRenderConfig(d, obj + L"\\renderconfig.ini", FileExists(item + L"\\material_e.mtl"));
        if (!FileExists(item + L"\\previewimage.png"))
            CopyAsset(obj + L"\\imagegui.png", item + L"\\previewimage.png", false, "preview image");
        int missing = 0;
        int rescued = RescueReferences(d, item, p, &missing);
        ok &= WriteWorkshopConfig(d, item + L"\\workshopconfig.ini");
        if (!ok)
        {
            Error("[%s] -> %s INCOMPLETE - the game may refuse it or crash on it", d.section.c_str(), d.id.c_str());
            return GEN_FAILED;
        }
        if (!WriteStamp(d, item, want)) return GEN_FAILED;
        char extra[64];
        extra[0] = 0;
        if (rescued) _snprintf_s(extra, sizeof(extra), _TRUNCATE, ", %d fetched by reference", rescued);
        Note("[%s] -> %s\\%s from \"%s\": %d file(s) copied%s, %d line(s) in, %d donor line(s) out%s",
             d.section.c_str(), d.id.c_str(), d.object.c_str(), d.donor.c_str(), copied, extra,
             (int)d.lines.size() + (d.name.empty() ? 0 : 1), dropped,
             missing ? " - SOME REFERENCED FILES ARE MISSING, see the warnings" : "");
        return GEN_GENERATED;
    }

    ok &= CopyAsset(g_media + L"\\buildings\\" + donor + L".nmf", obj + L"\\model.nmf", true, "mesh");
    ok &= CopyAsset(g_media + L"\\buildings_types\\" + donor + L".bbox", obj + L"\\building.bbox", false, "collision box");
    ok &= CopyAsset(g_media + L"\\buildings_types\\" + donor + L".fire", obj + L"\\building.fire", false, "fire points");
    std::wstring icon = g_media + L"\\editor\\tool_" + donor + L".png";
    ok &= CopyAsset(icon, obj + L"\\imagegui.png", false, "build-menu icon");
    CopyAsset(icon, item + L"\\previewimage.png", false, "preview image");

    int rewrote = 0, rewroteE = 0;
    std::wstring mtl = g_media + L"\\buildings\\" + donor + L".mtl";
    if (FileExists(mtl)) ok &= RewriteMaterial(mtl, item + L"\\material.mtl", &rewrote);
    else { Error("[%s] no material %s", d.section.c_str(), Narrow(mtl).c_str()); ok = false; }

    bool emissive = false;
    std::wstring mtlE = g_media + L"\\buildings\\" + donor + L"_e.mtl";
    if (FileExists(mtlE)) { emissive = RewriteMaterial(mtlE, item + L"\\material_e.mtl", &rewroteE); ok &= emissive; }

    ok &= WriteWorkshopConfig(d, item + L"\\workshopconfig.ini");
    ok &= WriteRenderConfig(d, obj + L"\\renderconfig.ini", emissive);
    ok &= WriteBuildingIni(d, donorIni, obj + L"\\building.ini", &dropped);

    if (!ok)
    {
        Error("[%s] -> %s INCOMPLETE - the game may refuse it or crash on it", d.section.c_str(), d.id.c_str());
        return GEN_FAILED;
    }

    if (!WriteStamp(d, item, want)) return GEN_FAILED;

    Note("[%s] -> %s\\%s from \"%s\": %d line(s) in, %d donor line(s) out, %d texture path(s) rewritten%s",
         d.section.c_str(), d.id.c_str(), d.object.c_str(), d.donor.c_str(),
         (int)d.lines.size() + (d.name.empty() ? 0 : 1), dropped, rewrote + rewroteE,
         emissive ? ", emissive material" : "");
    return GEN_GENERATED;
}

// __try needs a frame without objects that unwind; Generate keeps its own.
static void GenerateGuarded(const Decl* d, GenResult* out)
{
    __try { *out = Generate(*d); }
    __except (FaultFilter("buildings_plus generate", GetExceptionInformation()))
    {
        Error("[%s] faulted while generating - skipped", d->section.c_str());
        *out = GEN_FAILED;
    }
}

// Replaces the single zero of an "$OWNER_ID 0" line and leaves every other byte
// of the file exactly as it was, line endings included. A full rewrite is out of
// the question here: the file belongs to another generator and we know nothing
// about the rest of it. The match has to start a line and end one, so an
// "$OWNER_ID 07..." is never mistaken for a zero.
static bool ReplaceOwnerZero(const std::wstring& path, unsigned long long owner)
{
    std::string text;
    if (!ReadTextFile(path, &text)) return false;
    static const std::string needle = "$OWNER_ID 0";
    size_t at = std::string::npos;
    for (size_t p = text.find(needle); p != std::string::npos; p = text.find(needle, p + 1))
    {
        if (p != 0 && text[p - 1] != '\n' && text[p - 1] != '\r') continue;
        size_t end = p + needle.size();
        if (end != text.size() && text[end] != '\r' && text[end] != '\n') continue;
        at = p; break;
    }
    if (at == std::string::npos) return false;
    char id[32];
    _snprintf_s(id, sizeof(id), _TRUNCATE, "%llu", owner);
    std::string out = text.substr(0, at) + "$OWNER_ID " + id + text.substr(at + needle.size());
    return WriteTextFileAtomic(path, out);
}

// The same repair for the folders of other generators. Soviet Mod Loader writes
// $OWNER_ID 0 into every building it makes, and the game then reports each of
// them as a missing Workshop item on every save load - a dialog the player
// cannot get rid of. All of these have to hold before anything is written: the
// folder name is a generated id (9000000000..9999999999), the folder carries a
// tesmioloader.stamp, so some generator wrote it and not the game's own editor,
// the config names no owner at all, and our own id is known. A folder that
// already names somebody is never touched, and neither is the stamp - Soviet Mod
// Loader treats a missing stamp as a reason to close the game.
static int RepairForeignOwners(const std::vector<std::string>& skip)
{
    if (!g_repairOwners || !SteamOwner()) return 0;
    int fixed = 0;
    std::wstring outRoot = OutRoot();
    std::vector<std::wstring> subs = SubDirs(outRoot);
    for (size_t i = 0; i < subs.size(); i++)
    {
        std::string name = Narrow(subs[i]);
        if (name.size() != 10) continue;
        bool digits = true;
        for (size_t c = 0; c < name.size(); c++) if (name[c] < '0' || name[c] > '9') { digits = false; break; }
        if (!digits) continue;
        unsigned long long id = _strtoui64(name.c_str(), NULL, 10);
        if (id < MIN_ITEM_ID || id > MAX_ITEM_ID) continue;
        bool mine = false;
        for (size_t k = 0; k < skip.size(); k++) if (skip[k] == name) { mine = true; break; }
        if (mine) continue;   // one of ours, and the generator has already seen to it
        std::wstring item = outRoot + L"\\" + subs[i];
        std::string stamp;
        if (!ReadStamp(item, &stamp)) continue;
        if (ReplaceOwnerZero(item + L"\\workshopconfig.ini", SteamOwner()))
        {
            Note("wrote the Steam owner id into %s - saved games using this building no longer report it as missing",
                 name.c_str());
            fixed++;
        }
    }
    return fixed;
}

// Removes our own stamped folders whose declaration no longer exists or is
// switched off. Folders without a stamp, and stamped folders of another
// generator, are never touched.
static int Prune(const std::vector<std::string>& keep)
{
    int removed = 0;
    std::wstring outRoot = OutRoot();
    std::vector<std::wstring> subs = SubDirs(outRoot);
    for (size_t i = 0; i < subs.size(); i++)
    {
        std::string name = Narrow(subs[i]);
        bool kept = false;
        for (size_t k = 0; k < keep.size(); k++) if (keep[k] == name) { kept = true; break; }
        if (kept) continue;
        std::string stamp;
        if (!ReadStamp(outRoot + L"\\" + subs[i], &stamp)) continue;
        if (stamp.find(STAMP_MARK) == std::string::npos) continue;
        if (DeleteTree(outRoot + L"\\" + subs[i])) { Note("prune: removed %s (no declaration for it any more)", name.c_str()); removed++; }
        else Warn("prune: could not remove %s (%lu)", name.c_str(), GetLastError());
    }
    return removed;
}

// ---------------------------------------------------------------- validation

static bool AllDigits(const std::string& s)
{
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); i++) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

static bool SafeName(const std::string& s)
{
    if (s.empty() || s.size() > 64) return false;
    for (size_t i = 0; i < s.size(); i++)
    {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool HasControlChars(const std::string& s)
{
    for (size_t i = 0; i < s.size(); i++) { unsigned char c = (unsigned char)s[i]; if (c < 32 && c != '\t') return true; }
    return false;
}

// A Workshop id as the folder is named: digits only, leading zeros dropped, so
// "0930..." and "930..." never end up as two sections with one folder.
static std::string NormalizeId(const std::string& s)
{
    size_t i = 0;
    while (i + 1 < s.size() && s[i] == '0') i++;
    return s.substr(i);
}

static bool PlainText(const std::string& s)
{
    for (size_t i = 0; i < s.size(); i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || (c < 32 && c != '\n')) return false;
    }
    return true;
}

static bool ValidToken(const std::string& t)
{
    return t.size() > 1 && t[0] == '$' && FirstToken(t) == t;
}

static bool SmlCatalogBuildingId(const std::string& section, std::string* id);

static bool ValidateDecl(const Decl& d, std::string* why)
{
    char buf[256];
    if (d.id.empty()) { *why = "has no id and none could be assigned"; return false; }
    if (!AllDigits(d.id) || d.id.size() > 19) { *why = "id must be a number"; return false; }
    unsigned long long id = _strtoui64(d.id.c_str(), NULL, 10);
    if (id < MIN_ITEM_ID || id > MAX_ITEM_ID)
    {
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "id must lie between %llu and %llu to stay clear of real Workshop items", MIN_ITEM_ID, MAX_ITEM_ID);
        *why = buf; return false;
    }
    // Soviet Mod Loader reserves this range for the buildings it generates itself. It checks
    // every folder in there against its own plan before the game starts and TERMINATES the
    // launch when it finds one it does not know - a folder of ours would do exactly that. The
    // automatic assignment stays out of the range anyway; this catches a hand-written id.
    // 0.1.9: unless Soviet Mod Loader's own catalog gives exactly this number to a section of
    // this name - then it is the folder SML would write itself, and both loaders share it.
    if (id >= SML_MIN_ID && id <= SML_MAX_ID)
    {
        std::string smlId;
        if (!(SmlCatalogBuildingId(d.section, &smlId) && smlId == d.id))
        {
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "id %llu is inside %llu..%llu, which Soviet Mod Loader reserves for itself - "
                        "a folder of ours in there stops the game from starting; leave id empty or pick one between %llu and %llu",
                        id, SML_MIN_ID, SML_MAX_ID, AUTO_MIN_ID, AUTO_MAX_ID);
            *why = buf; return false;
        }
    }
    if (d.donor.empty()) { *why = "has no donor"; return false; }
    {
        // Two spellings: a base-game name, or one building of a Workshop item
        // as `<item>\<object>`. Neither may contain anything that leaves the
        // folder it names.
        std::string itemPart, objectPart;
        if (SplitDonor(d.donor, &itemPart, &objectPart))
        {
            if (!SafeDonorSegment(itemPart) || !SafeDonorSegment(objectPart))
            { *why = "a Workshop donor is written <item>\\<object>, each part a folder name without .. or drive letters"; return false; }
        }
        else if (!SafeName(d.donor))
        { *why = "donor must be a plain buildings_types name (letters, digits, _ and -) or a Workshop building as <item>\\<object>"; return false; }
    }
    if (!SafeName(d.object)) { *why = "object must be letters, digits, _ or - (at most 64)"; return false; }
    if (d.name.size() > 128 || !PlainText(d.name)) { *why = "name must be plain text without quotes (at most 128 characters)"; return false; }
    if (d.desc.size() > 4096 || !PlainText(d.desc)) { *why = "desc must be plain text without quotes (at most 4096 characters)"; return false; }
    if (d.life < 1 || d.life > 1000000) { *why = "life must lie between 1 and 1000000"; return false; }
    if (d.lines.size() > MAX_LINES) { *why = "too many line keys"; return false; }
    if (d.strips.size() > MAX_STRIPS) { *why = "too many strip keys"; return false; }
    for (size_t i = 0; i < d.lines.size(); i++)
    {
        if (d.lines[i].size() > MAX_LINE_LEN) { *why = "a line is too long"; return false; }
        // A line without a $TOKEN is a data line of the one above it - the grammar the game's
        // own building.ini uses for $CONNECTION_*, $RESOURCE_VISUALIZATION and the like. Only
        // the first line of a section has to carry a token, so a typo in it is still caught
        // instead of quietly turning into data for nothing.
        if (FirstToken(d.lines[i]).empty() && i == 0)
        {
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "line 1 has no $TOKEN and has nothing to belong to: %.60s", d.lines[i].c_str());
            *why = buf; return false;
        }
        if (d.lines[i].find_first_not_of(" \t") == std::string::npos)
        {
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, "line %u is empty", (unsigned)(i + 1));
            *why = buf; return false;
        }
        // Quotes are fine in a line ($NAME_STR "..."), control characters are not -
        // the two checks are separate on purpose.
        if (HasControlChars(d.lines[i])) { _snprintf_s(buf, sizeof(buf), _TRUNCATE, "line %u contains control characters", (unsigned)(i + 1)); *why = buf; return false; }
    }
    for (size_t i = 0; i < d.strips.size(); i++)
        if (!ValidToken(d.strips[i])) { _snprintf_s(buf, sizeof(buf), _TRUNCATE, "strip must be a single $TOKEN: %.60s", d.strips[i].c_str()); *why = buf; return false; }
    return true;
}

// ---------------------------------------------------------------- the config

// [buildings_plus] holds the switches ([buildings] is accepted for the same
// purpose); every other section is a building. Parsed by hand, because `line`
// and `desc` are deliberately repeatable.
static bool IsSwitchSection(const std::string& name)
{
    return _stricmp(name.c_str(), "buildings_plus") == 0 || _stricmp(name.c_str(), "buildings") == 0;
}

static bool ParseBool(const std::string& v, bool* out)
{
    if (v == "0") { *out = false; return true; }
    if (v == "1") { *out = true; return true; }
    return false;
}

static void LoadRegistry(const std::string& text)
{
    g_decls.clear();
    std::vector<std::string> lines = SplitLines(text);
    Decl* d = NULL;
    bool inSwitches = false;
    for (size_t li = 0; li < lines.size(); li++)
    {
        std::string line = TrimCopy(lines[li]);
        size_t no = li + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            d = NULL; inSwitches = false;
            size_t end = line.find(']');
            if (end == std::string::npos) { Warn("line %u: section header without ]", (unsigned)no); continue; }
            std::string name = TrimCopy(line.substr(1, end - 1));
            if (IsSwitchSection(name)) { inSwitches = true; continue; }
            if (!SafeName(name)) { Warn("line %u: section name [%s] must be letters, digits, _ or - - ignored", (unsigned)no, name.c_str()); continue; }
            if (g_decls.size() >= MAX_DECLS) { Warn("line %u: at most %d building sections - [%s] ignored", (unsigned)no, MAX_DECLS, name.c_str()); continue; }
            g_decls.push_back(Decl());
            d = &g_decls.back();
            d->section = name;
            d->object = name;
            d->configLine = no;
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) { Warn("line %u: no key = value", (unsigned)no); continue; }
        std::string key = TrimCopy(line.substr(0, eq));
        std::string val = TrimCopy(line.substr(eq + 1));
        if (inSwitches)
        {
            bool b;
            if      (_stricmp(key.c_str(), "enabled") == 0) { if (ParseBool(val, &b)) g_enabled = b; else Warn("line %u: enabled must be 0 or 1", (unsigned)no); }
            else if (_stricmp(key.c_str(), "always")  == 0) { if (ParseBool(val, &b)) g_always  = b; else Warn("line %u: always must be 0 or 1", (unsigned)no); }
            else if (_stricmp(key.c_str(), "verbose") == 0) { if (ParseBool(val, &b)) g_verbose = b; else Warn("line %u: verbose must be 0 or 1", (unsigned)no); }
            else if (_stricmp(key.c_str(), "prune")   == 0) { if (ParseBool(val, &b)) g_prune   = b; else Warn("line %u: prune must be 0 or 1", (unsigned)no); }
            else if (_stricmp(key.c_str(), "repair_owner_ids") == 0) { if (ParseBool(val, &b)) g_repairOwners = b; else Warn("line %u: repair_owner_ids must be 0 or 1", (unsigned)no); }
            else if (_stricmp(key.c_str(), "out")     == 0) { if (!val.empty() && val.find("..") == std::string::npos) g_outDir = Wide(val); else Warn("line %u: out must be a folder without ..", (unsigned)no); }
            else Warn("line %u: unknown switch \"%s\" - ignored", (unsigned)no, key.c_str());
            continue;
        }
        if (!d) { Warn("line %u: key outside any section - ignored", (unsigned)no); continue; }
        if      (_stricmp(key.c_str(), "id")      == 0) d->id = AllDigits(val) ? NormalizeId(val) : val;
        else if (_stricmp(key.c_str(), "object")  == 0) d->object = val;
        else if (_stricmp(key.c_str(), "donor")   == 0) d->donor = val;
        else if (_stricmp(key.c_str(), "name")    == 0) d->name = val;
        else if (_stricmp(key.c_str(), "desc")    == 0) { if (!d->desc.empty()) d->desc += "\n"; d->desc += val; }
        else if (_stricmp(key.c_str(), "life")    == 0) d->life = atoi(val.c_str());
        else if (_stricmp(key.c_str(), "enabled") == 0) { bool b; if (ParseBool(val, &b)) d->enabled = b; else Warn("line %u: [%s] enabled must be 0 or 1", (unsigned)no, d->section.c_str()); }
        else if (_stricmp(key.c_str(), "line")    == 0)
        {
            if (val.empty()) continue;
            if (d->lines.size() >= MAX_LINES) { if (d->lines.size() == MAX_LINES) Warn("line %u: [%s] has more than %d line keys - the rest is ignored", (unsigned)no, d->section.c_str(), MAX_LINES); d->lines.push_back(""); continue; }
            d->lines.push_back(val);
        }
        else if (_stricmp(key.c_str(), "strip")   == 0)
        {
            if (val.empty()) continue;
            if (d->strips.size() >= MAX_STRIPS) { if (d->strips.size() == MAX_STRIPS) Warn("line %u: [%s] has more than %d strip keys - the rest is ignored", (unsigned)no, d->section.c_str(), MAX_STRIPS); d->strips.push_back(""); continue; }
            d->strips.push_back(val);
        }
        else Warn("line %u: [%s] unknown key \"%s\" - ignored", (unsigned)no, d->section.c_str(), key.c_str());
    }
    // Two sections with one id would fight over one folder.
    for (size_t i = 0; i < g_decls.size(); i++)
        for (size_t k = i + 1; k < g_decls.size(); k++)
            if (!g_decls[i].id.empty() && g_decls[i].id == g_decls[k].id && g_decls[i].valid && g_decls[k].valid)
            {
                Error("[%s] and [%s] share id %s - both skipped", g_decls[i].section.c_str(), g_decls[k].section.c_str(), g_decls[i].id.c_str());
                g_decls[i].valid = false; g_decls[k].valid = false;
            }
}

// plugins\buildings_plus.ini when it exists (the classic install, or the
// effective INI Republic Mod Manager writes), otherwise the INI beside the DLL
// (the Workshop package under Soviet Mod Loader or the Workshop Bridge).
static std::wstring ConfigPath()
{
    std::wstring ini = WideAnsi(H->pluginDir) + L"\\buildings_plus.ini";
    if (FileExists(ini)) return ini;
    static const char anchor = 0;
    HMODULE self = NULL;
    wchar_t own[MAX_PATH];
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&anchor, &self) &&
        GetModuleFileNameW(self, own, MAX_PATH) > 0)
    {
        std::wstring beside = own;
        size_t slash = beside.find_last_of(L'\\');
        if (slash != std::wstring::npos)
        {
            beside = beside.substr(0, slash) + L"\\buildings_plus.ini";
            if (FileExists(beside)) return beside;
        }
    }
    return ini;
}

// ---------------------------------------------------------------- id catalog

// plugins\buildings_plus.ids.ini: `[ids]` with one `<section> = <id>` per
// building that ever had a number assigned or declared. Append-only in spirit:
// an entry is never removed here, so a section that disappears and comes back
// gets its old folder, and a number is never handed out twice.
struct CatalogEntry { std::string key; std::string id; };
static std::vector<CatalogEntry> g_catalog;
static bool g_catalogDirty = false;

static std::string LowerCopy(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++) if (out[i] >= 'A' && out[i] <= 'Z') out[i] = (char)(out[i] - 'A' + 'a');
    return out;
}

static std::wstring CatalogPath()
{
    return WideAnsi(H->pluginDir) + L"\\" + Wide(CATALOG_NAME);
}

static void LoadCatalog()
{
    g_catalog.clear();
    g_catalogDirty = false;
    std::string text;
    if (!ReadTextFile(CatalogPath(), &text)) return;
    std::vector<std::string> lines = SplitLines(text);
    bool inIds = false;
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = TrimCopy(lines[i]);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            size_t close = line.find(']');
            std::string name = close == std::string::npos ? line.substr(1) : line.substr(1, close - 1);
            inIds = _stricmp(TrimCopy(name).c_str(), "ids") == 0;
            continue;
        }
        if (!inIds) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) { Warn("%s line %u: not a `section = id` line - ignored", Narrow(CatalogPath()).c_str(), (unsigned)(i + 1)); continue; }
        std::string key = LowerCopy(TrimCopy(line.substr(0, eq)));
        std::string val = TrimCopy(line.substr(eq + 1));
        if (key.empty() || !AllDigits(val) || val.size() > 19) { Warn("%s line %u: id must be a number - ignored", Narrow(CatalogPath()).c_str(), (unsigned)(i + 1)); continue; }
        CatalogEntry e; e.key = key; e.id = NormalizeId(val);
        g_catalog.push_back(e);
    }
}

static const CatalogEntry* CatalogFind(const std::string& key)
{
    for (size_t i = 0; i < g_catalog.size(); i++) if (g_catalog[i].key == key) return &g_catalog[i];
    return NULL;
}

static void CatalogSet(const std::string& key, const std::string& id)
{
    for (size_t i = 0; i < g_catalog.size(); i++)
        if (g_catalog[i].key == key) { if (g_catalog[i].id != id) { g_catalog[i].id = id; g_catalogDirty = true; } return; }
    CatalogEntry e; e.key = key; e.id = id;
    g_catalog.push_back(e);
    g_catalogDirty = true;
}

static bool SaveCatalog()
{
    if (!g_catalogDirty) return true;
    std::string out;
    out += "; Written by TesmioLoader plugins\\buildings_plus.dll - the Workshop ids of the\r\n";
    out += "; buildings declared in buildings_plus.ini, one per section name. A number\r\n";
    out += "; stays with its section for good, because saved games refer to the folder\r\n";
    out += "; media_soviet\\workshop_wip\\<id>. Keep this file with your saved games; do not\r\n";
    out += "; edit it by hand.\r\n\r\n[ids]\r\n";
    for (size_t i = 0; i < g_catalog.size(); i++) out += g_catalog[i].key + " = " + g_catalog[i].id + "\r\n";
    if (!WriteTextFileAtomic(CatalogPath(), out)) { Error("could not write %s (%lu) - assigned ids would not survive the next start", Narrow(CatalogPath()).c_str(), g_ioError); return false; }
    g_catalogDirty = false;
    return true;
}

static unsigned long long IdValue(const std::string& id)
{
    return AllDigits(id) && !id.empty() && id.size() <= 19 ? _strtoui64(id.c_str(), NULL, 10) : 0ULL;
}

// Sections without `id =` get the highest number in the automatic range seen
// anywhere - catalog, explicit ids, folders under workshop_wip (foreign ones
// too) - plus one, unless the catalog already knows the section.
// ---------------------------------------------------------------- Soviet Mod Loader's catalog
//
// 0.1.9: SML numbers the buildings it generates in <state>\catalog.ini,
// [building_ids] "<mod id>::<section> = <number>", and keeps the number for
// good. When such a number exists for a section of the same name, this plugin
// adopts it: both loaders then write the same folder under workshop_wip, a
// saved game finds its buildings whichever loader is running, and the build
// menu does not show every package building twice. SML checks every 91xx
// folder against that catalog before the game starts (folder known, stamp
// with the right section=), and our stamp carries section= the way SML reads
// it, so a folder written by us passes its check.
static std::wstring SmlStateDir()
{
    std::wstring base = WideAnsi(H->baseDir);
    std::wstring ini = base + L"\\plugins\\soviet_mod_loader.ini";
    wchar_t dir[MAX_PATH] = L"soviet_mod_loader";
    if (FileExists(ini)) GetPrivateProfileStringW(L"loader", L"state_dir", L"soviet_mod_loader", dir, MAX_PATH, ini.c_str());
    std::wstring d = dir;
    if (d.size() > 1 && (d[1] == L':' || d[0] == L'\\')) return d;
    return base + L"\\" + d;
}

// True when SML's catalog names exactly one building with this section name.
static bool SmlCatalogBuildingId(const std::string& section, std::string* id)
{
    std::string text;
    if (!ReadTextFile(SmlStateDir() + L"\\catalog.ini", &text)) return false;
    std::vector<std::string> lines = SplitLines(text);
    std::string want = LowerCopy(section);
    bool inIds = false; int hits = 0; std::string found;
    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = TrimCopy(lines[i]);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[')
        {
            size_t close = line.find(']');
            std::string name = close == std::string::npos ? line.substr(1) : line.substr(1, close - 1);
            inIds = _stricmp(TrimCopy(name).c_str(), "building_ids") == 0;
            continue;
        }
        if (!inIds) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = LowerCopy(TrimCopy(line.substr(0, eq)));
        std::string val = TrimCopy(line.substr(eq + 1));
        size_t sep = key.rfind("::");
        if (sep == std::string::npos || key.substr(sep + 2) != want) continue;
        if (!AllDigits(val) || val.size() > 19) continue;
        hits++; found = val;
    }
    if (hits != 1) return false;
    *id = found;
    return true;
}

static void AssignIds()
{
    LoadCatalog();
    unsigned long long highest = 0;
    std::vector<unsigned long long> taken;
    for (size_t i = 0; i < g_catalog.size(); i++)
    {
        unsigned long long v = IdValue(g_catalog[i].id);
        taken.push_back(v);
        if (v >= AUTO_MIN_ID && v <= AUTO_MAX_ID && v > highest) highest = v;
    }
    for (size_t i = 0; i < g_decls.size(); i++)
    {
        if (!g_decls[i].valid || g_decls[i].id.empty()) continue;
        unsigned long long v = IdValue(g_decls[i].id);
        if (!v) continue;
        taken.push_back(v);
        if (v >= AUTO_MIN_ID && v <= AUTO_MAX_ID && v > highest) highest = v;
        CatalogSet(LowerCopy(g_decls[i].section), g_decls[i].id);
    }
    std::vector<std::wstring> subs = SubDirs(OutRoot());
    for (size_t i = 0; i < subs.size(); i++)
    {
        unsigned long long v = IdValue(Narrow(subs[i]));
        if (!v) continue;
        taken.push_back(v);
        if (v >= AUTO_MIN_ID && v <= AUTO_MAX_ID && v > highest) highest = v;
    }
    for (size_t i = 0; i < g_decls.size(); i++)
    {
        Decl& d = g_decls[i];
        if (!d.valid || !d.id.empty()) continue;
        std::string key = LowerCopy(d.section);
        // 0.1.9: Soviet Mod Loader's number for this section comes first - one folder for both
        // loaders. Our own catalog follows it; an older number of ours keeps its folder (prune
        // removes it when switched on).
        std::string smlId;
        if (SmlCatalogBuildingId(d.section, &smlId))
        {
            bool clash = false;
            for (size_t k = 0; k < g_decls.size(); k++)
                if (k != i && g_decls[k].valid && g_decls[k].id == smlId) { clash = true; break; }
            if (!clash)
            {
                const CatalogEntry* had = CatalogFind(key);
                if (had && had->id != smlId)
                    Note("[%s] Soviet Mod Loader's catalog numbers it %s - adopting that instead of %s so both loaders use one folder "
                         "(the folder %s stays; prune = 1 removes it)", d.section.c_str(), smlId.c_str(), had->id.c_str(), had->id.c_str());
                else if (!had)
                    Note("[%s] id %s adopted from Soviet Mod Loader's catalog", d.section.c_str(), smlId.c_str());
                d.id = smlId;
                taken.push_back(IdValue(smlId));
                CatalogSet(key, d.id);
                continue;
            }
            Warn("[%s] Soviet Mod Loader's catalog gives it %s, but another section declares that id - keeping our own number", d.section.c_str(), smlId.c_str());
        }
        const CatalogEntry* known = CatalogFind(key);
        if (known)
        {
            // The catalog's number, unless another section now declares it explicitly.
            bool clash = false;
            for (size_t k = 0; k < g_decls.size(); k++)
                if (k != i && g_decls[k].valid && g_decls[k].id == known->id) { clash = true; break; }
            if (!clash) { d.id = known->id; Info("[%s] id %s from the catalog", d.section.c_str(), d.id.c_str()); continue; }
            Warn("[%s] its catalog id %s is now declared by another section - assigning a new one", d.section.c_str(), known->id.c_str());
        }
        unsigned long long next = highest < AUTO_MIN_ID ? AUTO_MIN_ID : highest + 1;
        for (;;)
        {
            bool used = false;
            for (size_t k = 0; k < taken.size(); k++) if (taken[k] == next) { used = true; break; }
            if (!used) break;
            next++;
        }
        if (next > AUTO_MAX_ID)
        {
            Error("[%s] no free id left between %llu and %llu - skipped", d.section.c_str(), AUTO_MIN_ID, AUTO_MAX_ID);
            d.valid = false;
            continue;
        }
        char buf[32];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%llu", next);
        d.id = buf;
        highest = next;
        taken.push_back(next);
        CatalogSet(key, d.id);
        Note("[%s] assigned id %s (kept in plugins\\%s)", d.section.c_str(), d.id.c_str(), CATALOG_NAME);
    }
    SaveCatalog();
}

// The game's own folder from the executable, not from the working directory:
// anything in the game may call SetCurrentDirectory, and the world editor does.
static bool ResolveGameDir()
{
#ifdef BUILDINGS_PLUS_TEST
    if (!g_testGameDir.empty()) { g_gameDir = g_testGameDir; g_media = g_gameDir + L"\\media_soviet"; return DirExists(g_media); }
#endif
    wchar_t exe[MAX_PATH];
    DWORD len = GetModuleFileNameW((HMODULE)H->exeModule, exe, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;
    std::wstring path = exe;
    size_t slash = path.find_last_of(L'\\');
    if (slash == std::wstring::npos) return false;
    g_gameDir = path.substr(0, slash);
    g_media = g_gameDir + L"\\media_soviet";
    return DirExists(g_media);
}

// Everything happens here, and it has to: plugins are initialised from DllMain
// before the game's main thread runs, so a folder written now is found when
// the game scans workshop_wip. Returns 0 (stay listed) even when nothing is
// declared - Republic Mod Manager shows an empty registry as a normal state.
static int Run(const std::string& configText, const char* configName)
{
    g_decls.clear();
    LoadRegistry(configText);
    if (!g_enabled)
    {
        Note("enabled = 0 - no buildings generated (folders written earlier stay)");
        return 1;
    }
    if (!ResolveGameDir())
    {
        Error("could not find media_soviet beside SOVIET64.exe - nothing generated");
        return 1;
    }
    Info("configuration: %s; %u section(s); out = %s", configName, (unsigned)g_decls.size(), Narrow(OutRoot()).c_str());
    AssignIds();
    // Once per run, so a key that cannot be resolved is reported once and both
    // the hash and the written file see the same line.
    for (size_t i = 0; i < g_decls.size(); i++)
        if (g_decls[i].valid && g_decls[i].enabled) g_decls[i].nameLine = NameLine(g_decls[i]);

    std::vector<std::string> keep;
    int generated = 0, upToDate = 0, skipped = 0, failed = 0;
    for (size_t i = 0; i < g_decls.size(); i++)
    {
        Decl& d = g_decls[i];
        std::string why;
        // A section that is refused stays in `keep`: a typo in a line must never
        // make prune delete the folder a saved game is using. Only a section that is
        // switched off or gone gives its folder up.
        if (!d.valid) { if (!d.id.empty()) keep.push_back(d.id); skipped++; continue; }
        if (!ValidateDecl(d, &why))
        {
            Error("[%s] (line %u) %s - skipped", d.section.c_str(), (unsigned)d.configLine, why.c_str());
            d.valid = false; if (!d.id.empty()) keep.push_back(d.id); skipped++; continue;
        }
        if (!d.enabled) { Info("[%s] enabled = 0 - skipped", d.section.c_str()); skipped++; continue; }
        keep.push_back(d.id);
        GenResult r = GEN_FAILED;
        GenerateGuarded(&d, &r);
        if (r == GEN_GENERATED) generated++;
        else if (r == GEN_UP_TO_DATE) upToDate++;
        else failed++;
    }
    int pruned = g_prune ? Prune(keep) : 0;
    // After prune, so a folder that is about to go is not written to first.
    int owners = RepairForeignOwners(keep);
    Note("%d generated, %d up to date, %d skipped, %d failed, %d pruned, %d owner id(s) fixed; %d warning(s), %d error(s)",
         generated, upToDate, skipped, failed, pruned, owners, g_warnings, g_errors);
    return 0;
}

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion(void)
{
    return TSM_API_VERSION;
}

extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* host, TsmPluginInfo* info)
{
    TsmBind(host);
    info->name    = "buildings_plus";
    info->version = PLUGIN_VERSION;
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    Note("TesmioLoader buildings_plus %s starting; detail log: logs\\%s", PLUGIN_VERSION, PLUGIN_LOG_NAME);
    // The folders are written in the start phase, not here: a `name =` that is
    // a localisation key needs the Localization service, and a service only
    // exists once every plugin's init has run.
    return 0;
}

// Second phase. Nothing here hooks the game - the work is reading the INI and
// writing folders under media_soviet, which the game only scans later.
extern "C" __declspec(dllexport) int TsmPluginStart(void)
{
    if (H && H->consume)
        g_localization = (const TsmLocalizationApi*)H->consume(TSM_SERVICE_LOCALIZATION,
                                                              TSM_LOCALIZATION_VERSION);
    if (g_localization && g_localization->resolveFull)
        Info("localization service available; a name with dots is resolved as a key");

    // Nothing here hooks the game, so a failure ends this plugin and nothing else -
    // but it must end HERE: the loader is plain C and a C++ exception (a file that
    // will not fit, a string past its limit) would otherwise unwind into it.
    try
    {
        std::wstring cfg = ConfigPath();
        std::string text;
        if (!ReadTextFile(cfg, &text))
        {
            Note("no %s - nothing declared", Narrow(cfg).c_str());
            return 0;
        }
        return Run(text, Narrow(cfg).c_str());
    }
    catch (const std::exception& e) { Error("C++ exception during startup: %s - nothing more generated this start", e.what()); }
    catch (...) { Error("unknown C++ exception during startup - nothing more generated this start"); }
    Note("startup aborted; %d warning(s), %d error(s)", g_warnings, g_errors);
    return 1;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }

// ---------------------------------------------------------------- self-test

#ifdef BUILDINGS_PLUS_TEST
#include <stdio.h>

static void TestLog(const char* fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); printf("\n"); }
static long TestFault(const char*, void*) { return EXCEPTION_EXECUTE_HANDLER; }

// A stand-in for the Localization plugin: exactly one key resolves.
static int TestResolveFull(const char* key)
{
    return (key && strcmp(key, "localization.lang.medicine_factory") == 0) ? 2000123 : 0;
}
static int TestResolve(const char*, const char*) { return 0; }
static const TsmLocalizationApi kTestLocalization = { TestResolve, TestResolveFull };

static bool Has(const std::string& text, const char* wanted) { return text.find(wanted) != std::string::npos; }
static int g_failed = 0;
static void Check(bool ok, const char* what) { if (ok) printf("PASS %s\n", what); else { printf("FAIL %s\n", what); g_failed++; } }
static void Put(const std::wstring& path, const std::string& text) { WriteTextFileAtomic(path, text); }

// --run <ini> <game folder> <out folder>: generates the sections of a real INI
// from a real game folder into an out folder of your choice (never into the
// game), so a declaration can be checked before it goes into the game.
static int RunConfig(const std::wstring& ini, const std::wstring& game, const std::wstring& out)
{
    static TsmHost host;
    memset(&host, 0, sizeof(host));
    host.apiVersion = TSM_API_VERSION; host.structSize = sizeof(host);
    static std::string baseA, pluginsA;
    EnsureDir(out);
    // The id catalog goes next to the "plugin" of this dry run, never into the game.
    EnsureDir(out + L"\\plugins");
    baseA = Narrow(out); pluginsA = Narrow(out + L"\\plugins");
    host.baseDir = baseA.c_str(); host.pluginDir = pluginsA.c_str();
    host.log = TestLog; host.faultFilter = TestFault;
    TsmBind(&host);
    g_testGameDir = game;
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);
    std::string text;
    if (!ReadTextFile(ini, &text)) { printf("cannot read %ls\n", ini.c_str()); return 2; }
    // The out folder of the command line wins over any `out` in the file.
    text += "\n[buildings_plus]\nout = " + Narrow(out) + "\n";
    int rc = Run(text, Narrow(ini).c_str());
    printf("RESULT rc=%d, %d warning(s), %d error(s); output under %ls\n", rc, g_warnings, g_errors, out.c_str());
    return g_errors ? 1 : 0;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc == 5 && wcscmp(argv[1], L"--run") == 0) return RunConfig(argv[2], argv[3], argv[4]);
    if (argc < 2) { printf("usage: buildings_plus_test <fresh scratch folder>\n       buildings_plus_test --run <ini> <game folder> <out folder>\n"); return 2; }
    std::wstring root = argv[1];
    if (DirExists(root)) { printf("scratch folder must not exist yet: %ls\n", root.c_str()); return 2; }
    // A fixed owner, so the checks say the same thing on every machine and with
    // the Steam client signed out.
    SetTestOwner(76561198017498697ULL);
    std::wstring game = root + L"\\game", build = root + L"\\game\\tesmioloader\\build", plugins = build + L"\\plugins";
    EnsureDir(root); EnsureDir(game); EnsureDir(game + L"\\tesmioloader"); EnsureDir(build); EnsureDir(plugins);
    std::wstring media = game + L"\\media_soviet";
    EnsureDir(media); EnsureDir(media + L"\\buildings"); EnsureDir(media + L"\\buildings_types"); EnsureDir(media + L"\\editor");
    Put(media + L"\\buildings_types\\donor_mine.ini",
        "$NAME 6160\n$TYPE_FACTORY\n$WORKERS_NEEDED 10\n$PRODUCTION coal 1.0\n$CONSUMPTION eletric 0.1\n"
        "$STORAGE_EXPORT RESOURCE_TRANSPORT_GRAVEL 20\n$RESOURCE_VISUALIZATION 0\npositon 5 0 5\nscale 1 1 1\n\n$CONNECTION_ROAD\n1 0 0\n2 0 0\n"
        "$COST_WORK SOVIET_CONSTRUCTION_GROUNDWORKS 1.0\n$PARTICLE smoke 1 2 3\nend\n");
    Put(media + L"\\buildings_types\\donor_mine.bbox", "bbox");
    Put(media + L"\\buildings_types\\donor_mine.fire", "fire");
    Put(media + L"\\buildings\\donor_mine.nmf", "mesh");
    Put(media + L"\\buildings\\donor_mine.mtl", "$SUBMATERIAL x\n$TEXTURE_MTL 0 donor_mine.dds\n$TEXTURE 1 buildings/other.dds\n");
    Put(media + L"\\buildings\\donor_mine_e.mtl", "$TEXTURE_MTL 0 donor_mine_e.dds\n");
    Put(media + L"\\editor\\tool_donor_mine.png", "png");

    static TsmHost host;
    memset(&host, 0, sizeof(host));
    host.apiVersion = TSM_API_VERSION; host.structSize = sizeof(host);
    std::string baseA = Narrow(build), pluginsA = Narrow(plugins);
    host.baseDir = baseA.c_str(); host.pluginDir = pluginsA.c_str();
    host.log = TestLog; host.faultFilter = TestFault;
    TsmBind(&host);
    g_testGameDir = game;
    g_detail = TsmOpenLog(PLUGIN_LOG_NAME);

    std::string ini =
        "[buildings_plus]\nenabled = 1\nverbose = 1\nprune = 1\n\n"
        "[salt_mine]\nid = 9400000021\ndonor = donor_mine\nobject = SaltMine\nname = Salt Mine\ndesc = First line\ndesc = Second line\n"
        "line = $PRODUCTION raw_salt 1.0\nline = $STORAGE_EXPORT RESOURCE_TRANSPORT_OPEN 20\n"
        "line = $CONNECTION_WATERPIPE_INPUT\nline = -92.5 -3.2 5.0\nline = -91.5 -3.2 5.0\nstrip = $WORKERS_NEEDED\n\n"
        "[bad_id]\nid = 123\ndonor = donor_mine\n\n"
        "[bad_first_line]\nid = 9400000026\ndonor = donor_mine\nline = 1 2 3\n\n"
        "[sml_range]\nid = 9100000042\ndonor = donor_mine\n\n"
        "[bad_name]\nid = 9400000022\ndonor = donor_mine\nname = Say \"hi\"\n\n"
        "[no_donor]\nid = 9400000023\ndonor = missing_donor\n\n"
        "[off]\nid = 9400000024\ndonor = donor_mine\nenabled = 0\n\n"
        "[collide]\nid = 9400000025\ndonor = donor_mine\n";
    std::wstring out = media + L"\\workshop_wip";
    EnsureDir(out);
    // A folder that is not ours must never be touched, and a stale stamped folder of ours goes away with prune.
    EnsureDir(out + L"\\9400000025"); Put(out + L"\\9400000025\\workshopconfig.ini", "$ITEM_ID 9400000025\n");
    EnsureDir(out + L"\\9400000099"); Put(out + L"\\9400000099\\" + Wide(STAMP_NAME), std::string(STAMP_MARK) + "\nhash=0\n");
    EnsureDir(out + L"\\9400000098"); Put(out + L"\\9400000098\\" + Wide(STAMP_NAME), "tesmioloader plugins\\buildings.dll generated this folder.\nhash=0\n");

    g_warnings = 0; g_errors = 0;
    int rc = Run(ini, "test");
    Check(rc == 0, "run returns 0 with a registry");
    std::string b;
    std::wstring item = out + L"\\9400000021", obj = item + L"\\SaltMine";
    Check(ReadTextFile(obj + L"\\building.ini", &b), "building.ini written");
    Check(Has(b, "$NAME_STR \"Salt Mine\"") && !Has(b, "$NAME 6160"), "name replaces the donor's $NAME");
    Check(Has(b, "$PRODUCTION raw_salt 1.0") && !Has(b, "$PRODUCTION coal") && !Has(b, "$CONSUMPTION eletric"), "a declared recipe replaces the donor recipe whole");
    Check(Has(b, "$STORAGE_EXPORT RESOURCE_TRANSPORT_OPEN 20") && !Has(b, "RESOURCE_TRANSPORT_GRAVEL") && !Has(b, "$RESOURCE_VISUALIZATION"), "storages replace storages and the visualisation");
    Check(Has(b, "$CONNECTION_WATERPIPE_INPUT\r\n-92.5 -3.2 5.0\r\n-91.5 -3.2 5.0"), "a token keeps its data lines, in order");
    Check(!DirExists(out + L"\\9400000026"), "a first line without a $TOKEN is refused");
    Check(!Has(b, "positon 5 0 5") && !Has(b, "scale 1 1 1"), "the data lines of a dropped token go with it");
    Check(!Has(b, "$WORKERS_NEEDED"), "strip drops a donor line");
    Check(Has(b, "$CONNECTION_ROAD\r\n1 0 0\r\n2 0 0") && Has(b, "$COST_WORK SOVIET_CONSTRUCTION_GROUNDWORKS 1.0") && Has(b, "$PARTICLE smoke 1 2 3") && Has(b, "$TYPE_FACTORY"), "geometry, costs and type survive");
    std::string m;
    Check(ReadTextFile(item + L"\\material.mtl", &m) && Has(m, "$TEXTURE 0 buildings/donor_mine.dds") && Has(m, "$TEXTURE 1 buildings/other.dds") && !Has(m, "$TEXTURE_MTL"), "$TEXTURE_MTL is rewritten, $TEXTURE kept");
    std::string r;
    Check(ReadTextFile(obj + L"\\renderconfig.ini", &r) && Has(r, "MATERIALEMISSIVE ../material_e.mtl") && Has(r, "LIFE 3000.000000"), "renderconfig names the emissive material");
    std::string w;
    Check(ReadTextFile(item + L"\\workshopconfig.ini", &w) && Has(w, "$OBJECT_BUILDING SaltMine") && Has(w, "$ITEM_NAME \"Salt Mine\"") && Has(w, "First line\nSecond line"), "workshopconfig carries object, name and both desc lines");
    Check(FileExists(obj + L"\\model.nmf") && FileExists(obj + L"\\building.bbox") && FileExists(obj + L"\\building.fire") && FileExists(obj + L"\\imagegui.png") && FileExists(item + L"\\previewimage.png"), "assets copied");
    std::string stamp;
    Check(ReadStamp(item, &stamp) && Has(stamp, STAMP_MARK) && Has(stamp, "hash="), "stamp written");
    Check(!FileExists(out + L"\\9400000022\\workshopconfig.ini"), "a name with a quote is skipped");
    Check(!DirExists(out + L"\\9400000021\\bad_id"), "an id outside the range is skipped");
    Check(!DirExists(out + L"\\9100000042"), "an id inside the range Soviet Mod Loader reserves is refused");
    Check(!FileExists(out + L"\\9400000025\\" + Wide(STAMP_NAME)) && FileExists(out + L"\\9400000025\\workshopconfig.ini") && !DirExists(out + L"\\9400000025\\collide"), "a folder without a stamp is refused and left alone");
    Check(!DirExists(out + L"\\9400000023"), "a missing donor generates nothing");
    Check(!DirExists(out + L"\\9400000099"), "prune removes our stamped folder without a declaration");
    Check(DirExists(out + L"\\9400000098"), "prune leaves a stamped folder of another generator");
    Check(!DirExists(out + L"\\9400000024"), "a disabled section generates nothing");
    Check(g_errors == 6, "exactly the six bad sections (id, reserved id, first line, name, donor, no-stamp folder) are errors");

    // Second run: up to date, nothing rewritten.
    WIN32_FILE_ATTRIBUTE_DATA before, after;
    GetFileAttributesExW((obj + L"\\building.ini").c_str(), GetFileExInfoStandard, &before);
    Sleep(30);
    g_warnings = 0; g_errors = 0;
    Run(ini, "test");
    GetFileAttributesExW((obj + L"\\building.ini").c_str(), GetFileExInfoStandard, &after);
    Check(CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) == 0, "an unchanged declaration is left alone");

    // The Steam owner id: written into workshopconfig.ini, and put right on a
    // folder that still carries a zero from an older version. Without it the
    // game reports the building as a missing Workshop item on every save load.
    std::string cfg;
    Check(ReadTextFile(item + L"\\workshopconfig.ini", &cfg) &&
          cfg.find("$OWNER_ID 76561198017498697\r\n") != std::string::npos,
          "the workshopconfig carries the Steam owner id of whoever generated it");
    Put(item + L"\\workshopconfig.ini", "$ITEM_ID 9400000001\r\n\r\n$OWNER_ID 0\r\n\r\n$END\r\n");
    g_warnings = 0; g_errors = 0;
    Run(ini, "test");
    Check(ReadTextFile(item + L"\\workshopconfig.ini", &cfg) &&
          cfg.find("$OWNER_ID 76561198017498697\r\n") != std::string::npos &&
          cfg.find("$OBJECT_BUILDING") != std::string::npos,
          "an up to date folder with $OWNER_ID 0 has the owner written into it");

    // The same repair for the folders of another generator. Soviet Mod Loader
    // writes $OWNER_ID 0 into every building it makes, so these ids look like
    // the real ones do. Only the number may change - the file belongs to
    // somebody else, and its line endings have to survive byte for byte.
    std::string smlCfg =
        "$ITEM_ID 9188000001\r\n\r\n$OWNER_ID 0\r\n\r\n$ITEM_TYPE WORKSHOP_ITEMTYPE_BUILDING\r\n\r\n"
        "$VISIBILITY 0\r\n$OBJECT_BUILDING SaltMine\r\n\r\n$ITEM_NAME \"Salt Mine\"\r\n\r\n$END\r\n";
    std::string smlWant = smlCfg;
    smlWant.replace(smlWant.find("$OWNER_ID 0"), strlen("$OWNER_ID 0"), "$OWNER_ID 76561198017498697");
    const std::string foreignStamp = "tesmioloader plugins\\buildings.dll generated this folder.\r\nhash=0\r\n";
    EnsureDir(out + L"\\9188000001"); Put(out + L"\\9188000001\\" + Wide(STAMP_NAME), foreignStamp);
    Put(out + L"\\9188000001\\workshopconfig.ini", smlCfg);
    EnsureDir(out + L"\\9188000002"); Put(out + L"\\9188000002\\workshopconfig.ini", smlCfg);   // no stamp: hand-built
    EnsureDir(out + L"\\9188000003"); Put(out + L"\\9188000003\\" + Wide(STAMP_NAME), foreignStamp);
    Put(out + L"\\9188000003\\workshopconfig.ini", "$ITEM_ID 9188000003\r\n\r\n$OWNER_ID 76561197960287777\r\n\r\n$END\r\n");

    g_warnings = 0; g_errors = 0;
    Run("[buildings_plus]\nenabled = 1\nprune = 0\nrepair_owner_ids = 0\n", "test");
    Check(ReadTextFile(out + L"\\9188000001\\workshopconfig.ini", &cfg) && cfg == smlCfg,
          "repair_owner_ids = 0 leaves a foreign folder alone");

    g_warnings = 0; g_errors = 0;
    Run("[buildings_plus]\nenabled = 1\nprune = 0\nrepair_owner_ids = 1\n", "test");
    Check(ReadTextFile(out + L"\\9188000001\\workshopconfig.ini", &cfg) && cfg == smlWant,
          "a foreign generated folder gets the owner id, and nothing else in the file changes");
    Check(FileExists(out + L"\\9188000001\\" + Wide(STAMP_NAME)) &&
          ReadTextFile(out + L"\\9188000001\\" + Wide(STAMP_NAME), &cfg) && cfg == foreignStamp,
          "the foreign stamp is left untouched");
    Check(ReadTextFile(out + L"\\9188000002\\workshopconfig.ini", &cfg) && cfg == smlCfg,
          "a folder without a stamp is not repaired");
    Check(ReadTextFile(out + L"\\9188000003\\workshopconfig.ini", &cfg) &&
          cfg.find("$OWNER_ID 76561197960287777") != std::string::npos,
          "a folder that already names an owner keeps it");

    // A renamed object removes the stale subfolder.
    std::string ini2 = ini;
    size_t at = ini2.find("object = SaltMine");
    ini2.replace(at, strlen("object = SaltMine"), "object = SaltMine2");
    Run(ini2, "test");
    Check(DirExists(item + L"\\SaltMine2") && !DirExists(item + L"\\SaltMine"), "renaming the object replaces the object folder");

    // Switching the plugin off touches nothing.
    Run("[buildings_plus]\nenabled = 0\n", "test");
    Check(DirExists(item), "enabled = 0 keeps generated folders");

    // Sections without an id: highest number in the automatic range plus one,
    // counting explicit ids and any folder under workshop_wip, kept in the catalog.
    EnsureDir(out + L"\\9300000003"); Put(out + L"\\9300000003\\workshopconfig.ini", "$ITEM_ID 9300000003\n");
    std::string ini3 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[Auto_One]\ndonor = donor_mine\n\n"
        "[auto_two]\ndonor = donor_mine\n\n"
        "[fixed]\nid = 9300000007\ndonor = donor_mine\n";
    g_warnings = 0; g_errors = 0;
    Run(ini3, "test");
    Check(g_errors == 0 && DirExists(out + L"\\9300000008\\Auto_One") && DirExists(out + L"\\9300000009\\auto_two") && DirExists(out + L"\\9300000007\\fixed"),
          "ids are assigned above the highest number seen (explicit id and foreign folder included)");
    std::string cat;
    Check(ReadTextFile(plugins + L"\\" + Wide(CATALOG_NAME), &cat) && Has(cat, "[ids]") && Has(cat, "auto_one = 9300000008") && Has(cat, "auto_two = 9300000009") && Has(cat, "fixed = 9300000007") && Has(cat, "salt_mine = 9400000021"),
          "the catalog records assigned and explicit ids by section name");
    // Removing an earlier section and adding a new one leaves the old numbers alone.
    std::string ini4 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[auto_two]\ndonor = donor_mine\n\n"
        "[auto_three]\ndonor = donor_mine\n";
    g_warnings = 0; g_errors = 0;
    Run(ini4, "test");
    Check(g_errors == 0 && DirExists(out + L"\\9300000009\\auto_two") && DirExists(out + L"\\9300000010\\auto_three"),
          "a section keeps its catalog id; a new section gets the next number, never a freed one");
    Check(ReadTextFile(plugins + L"\\" + Wide(CATALOG_NAME), &cat) && Has(cat, "auto_one = 9300000008") && Has(cat, "auto_three = 9300000010"),
          "the catalog keeps the entry of a removed section");
    // An explicit id that takes a cataloged number moves the other section to a new one.
    std::string ini5 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[auto_two]\ndonor = donor_mine\n\n"
        "[grabber]\nid = 9300000009\ndonor = donor_mine\n";
    g_warnings = 0; g_errors = 0;
    Run(ini5, "test");
    Check(g_warnings == 1 && g_errors == 0 && DirExists(out + L"\\9300000011\\auto_two") && DirExists(out + L"\\9300000009\\grabber"),
          "an explicit id wins over the catalog and the displaced section is renumbered with a warning");

    // 0.1.3: a name that is a localisation key becomes the id the game's own
    // $NAME takes; a caption stays a literal; an unresolvable key falls back.
    std::string ini6 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[plain_name]\nid = 9300000020\ndonor = donor_mine\nobject = Plain\nname = Large Medicine Factory\n\n"
        "[key_name]\nid = 9300000021\ndonor = donor_mine\nobject = Keyed\nname = localization.lang.medicine_factory\n\n"
        "[key_missing]\nid = 9300000022\ndonor = donor_mine\nobject = Missing\nname = localization.lang.nothing_here\n";
    g_warnings = 0; g_errors = 0;
    g_localization = &kTestLocalization;
    Run(ini6, "test");
    g_localization = NULL;
    std::string plainIni, keyIni, missIni;
    Check(ReadTextFile(out + L"\\9300000020\\Plain\\building.ini", &plainIni) &&
              Has(plainIni, "$NAME_STR \"Large Medicine Factory\"") && !Has(plainIni, "$NAME 6160"),
          "a plain name stays a literal and replaces the donor's numbered name");
    Check(ReadTextFile(out + L"\\9300000021\\Keyed\\building.ini", &keyIni) &&
              Has(keyIni, "$NAME 2000123") && !Has(keyIni, "$NAME_STR") && !Has(keyIni, "localization.lang"),
          "a name that is a localisation key becomes $NAME with the resolved id, never both lines");
    Check(ReadTextFile(out + L"\\9300000022\\Missing\\building.ini", &missIni) &&
              Has(missIni, "$NAME_STR \"nothing_here\"") && g_warnings == 1,
          "an unresolvable key falls back to the part after the last dot and warns once");

    // 0.1.4: one building of a Workshop item as the donor. The fake item has
    // everything that makes real items awkward: three buildings, an asset
    // folder with a name of its author's choosing, a loose file at the root,
    // a reference into a sibling building's folder and one that goes nowhere.
    std::wstring mod = out + L"\\mod_item";
    EnsureDir(mod); EnsureDir(mod + L"\\mtl"); EnsureDir(mod + L"\\house_a"); EnsureDir(mod + L"\\house_b"); EnsureDir(mod + L"\\house_c");
    Put(mod + L"\\workshopconfig.ini", "$ITEM_ID 1234567890\n$OBJECT_BUILDING house_a\n$OBJECT_BUILDING house_b\n$OBJECT_BUILDING house_c\n$ITEM_NAME \"Three Houses\"\n$END\n");
    Put(mod + L"\\previewimage.png", "png");
    Put(mod + L"\\shared.dds", "dds");
    Put(mod + L"\\mtl\\diffuse.mtl", "$SUBMATERIAL x\n$TEXTURE_MTL 0 main.dds\n$TEXTURE_MTL 1 gone.dds\n$TEXTURE 2 buildings/blankbump.dds\n");
    Put(mod + L"\\mtl\\main.dds", "dds");
    Put(mod + L"\\house_a\\building.ini", "$NAME 6161\n$TYPE_LIVING\n$WORKERS_NEEDED 4\n$PRODUCTION coal 1.0\n");
    Put(mod + L"\\house_a\\custom.nmf", "mesh");
    Put(mod + L"\\house_a\\imagegui.png", "png");
    Put(mod + L"\\house_a\\renderconfig.ini", "$TYPE_WORKSHOP\n MODEL custom.nmf\n MATERIAL ../mtl/diffuse.mtl\n MATERIALEMISSIVE ../house_b/extra.mtl\n LIFE 3000\n$END\n");
    Put(mod + L"\\house_b\\building.ini", "$NAME 6162\n$TYPE_LIVING\n");
    Put(mod + L"\\house_b\\extra.mtl", "$TEXTURE_MTL 0 deep.dds\n");
    Put(mod + L"\\house_b\\deep.dds", "dds");
    Put(mod + L"\\house_b\\other.nmf", "mesh");
    Put(mod + L"\\house_c\\building.ini", "$NAME 6163\n$TYPE_LIVING\n");
    Put(mod + L"\\house_c\\unused.dds", "dds");

    std::string ini7 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[from_item]\nid = 9300000030\ndonor = mod_item\\house_a\nobject = MyHouse\nname = My House\nline = $PRODUCTION raw_salt 2.0\n\n"
        "[item_missing]\nid = 9300000031\ndonor = mod_item\\house_x\n\n"
        "[item_escape]\nid = 9300000032\ndonor = mod_item\\..\\..\\secret\n";
    g_warnings = 0; g_errors = 0;
    Run(ini7, "test");
    std::wstring clone = out + L"\\9300000030";
    std::string cloneIni, cloneCfg, cloneRender;
    Check(ReadTextFile(clone + L"\\MyHouse\\building.ini", &cloneIni) &&
              Has(cloneIni, "$NAME_STR \"My House\"") && Has(cloneIni, "$PRODUCTION raw_salt 2.0") && !Has(cloneIni, "$NAME 6161"),
          "a Workshop building is cloned into the declared object folder and only its building.ini is rewritten");
    Check(FileExists(clone + L"\\MyHouse\\custom.nmf") && FileExists(clone + L"\\MyHouse\\imagegui.png"),
          "the donor's own file names are kept, not renamed to model.nmf");
    Check(FileExists(clone + L"\\mtl\\diffuse.mtl") && FileExists(clone + L"\\mtl\\main.dds") && FileExists(clone + L"\\shared.dds"),
          "an asset folder of any name and the loose files of the item travel with the clone");
    Check(!DirExists(clone + L"\\house_c") && !FileExists(clone + L"\\house_b\\other.nmf"),
          "the other buildings of the item stay behind - one section is one building");
    Check(FileExists(clone + L"\\house_b\\extra.mtl") && FileExists(clone + L"\\house_b\\deep.dds"),
          "a file referenced into a sibling building's folder is fetched, and what that file refers to as well");
    Check(g_warnings == 1, "a reference the donor item cannot satisfy warns exactly once");
    Check(ReadTextFile(clone + L"\\workshopconfig.ini", &cloneCfg) &&
              Has(cloneCfg, "$ITEM_ID 9300000030") && Has(cloneCfg, "$OBJECT_BUILDING MyHouse") && !Has(cloneCfg, "house_b") && !Has(cloneCfg, "1234567890"),
          "the clone gets its own workshopconfig with one object, never the donor's list");
    Check(ReadTextFile(clone + L"\\MyHouse\\renderconfig.ini", &cloneRender) &&
              Has(cloneRender, "MODEL custom.nmf") && !Has(cloneRender, "MODEL model.nmf"),
          "the donor's renderconfig is taken over, because it names the donor's own mesh");
    Check(FileExists(clone + L"\\previewimage.png"), "the item's preview image travels with the clone");
    Check(!DirExists(out + L"\\9300000031") && !DirExists(out + L"\\9300000032") && g_errors == 2,
          "a donor that is not there and one that tries to climb out of the item are both refused");

    // Up to date, then a changed donor file, then a second section from the
    // same item - the plan is per building, not per item.
    g_warnings = 0; g_errors = 0;
    Run(ini7, "test");
    // An up-to-date clone is not copied again, so the reference check does not
    // run either and its warning stays silent; only the two refusals speak up.
    Check(g_errors == 2 && g_warnings == 0, "a second run leaves the clone alone and copies nothing");
    Sleep(1100);
    Put(mod + L"\\mtl\\main.dds", "dds changed");
    std::string ini8 = "[buildings_plus]\nenabled = 1\n\n[from_item]\nid = 9300000030\ndonor = mod_item\\house_a\nobject = MyHouse\nname = My House\n";
    g_warnings = 0; g_errors = 0;
    Run(ini8, "test");
    std::string changed;
    Check(ReadTextFile(clone + L"\\mtl\\main.dds", &changed) && changed == "dds changed",
          "an updated file in the donor item regenerates the clone");

    // 0.1.9: Soviet Mod Loader's catalog numbers a building - both loaders share one folder.
    EnsureDir(build + L"\\soviet_mod_loader");
    Put(build + L"\\soviet_mod_loader\\catalog.ini",
        "[resource_order]\ncopper_ore = 0\n\n[building_ids]\n"
        "test.mod::adopted = 9150000001\nother.mod::twice = 9150000002\nthird.mod::twice = 9150000003\nx.mod::explicit_ok = 9150000004\n");
    // The folder SML wrote for the adopted section, with its own stamp and a stale file.
    EnsureDir(out + L"\\9150000001"); EnsureDir(out + L"\\9150000001\\Adopted");
    Put(out + L"\\9150000001\\" + Wide(STAMP_NAME), "tesmioloader plugins\\buildings.dll generated this folder.\r\nsection=adopted donor=donor_mine object=Adopted\r\nhash=ABCD\r\n");
    Put(out + L"\\9150000001\\Adopted\\building.ini", "$NAME 6160\nstale\nend\n");
    Put(out + L"\\9150000001\\workshopconfig.ini", "$ITEM_ID 9150000001\r\n$OWNER_ID 0\r\n$END\r\n");
    std::string ini9 =
        "[buildings_plus]\nenabled = 1\n\n"
        "[adopted]\ndonor = donor_mine\nobject = Adopted\nname = Adopted\n\n"
        "[twice]\ndonor = donor_mine\n\n"
        "[explicit_ok]\nid = 9150000004\ndonor = donor_mine\n\n"
        "[explicit_bad]\nid = 9150000009\ndonor = donor_mine\n";
    g_warnings = 0; g_errors = 0;
    Run(ini9, "test");
    std::string adoptedIni, adoptedStamp, adoptedCfg, cat9;
    Check(ReadTextFile(out + L"\\9150000001\\Adopted\\building.ini", &adoptedIni) && !Has(adoptedIni, "stale") && Has(adoptedIni, "$NAME_STR \"Adopted\""),
          "a section SML numbered gets SML's number and its folder is written again from our declaration");
    Check(ReadTextFile(out + L"\\9150000001\\" + Wide(STAMP_NAME), &adoptedStamp) && Has(adoptedStamp, STAMP_MARK) && Has(adoptedStamp, "section=adopted donor="),
          "the taken-over folder carries our stamp with section= the way SML reads it");
    Check(ReadTextFile(out + L"\\9150000001\\workshopconfig.ini", &adoptedCfg) && Has(adoptedCfg, "$OWNER_ID 76561198017498697"),
          "and the owner id SML left at 0 is filled in");
    Check(ReadTextFile(CatalogPath(), &cat9) && Has(cat9, "adopted = 9150000001") && Has(cat9, "explicit_ok = 9150000004"),
          "our catalog records SML's numbers");
    Check(!Has(cat9, "twice = 9150000002") && !Has(cat9, "twice = 9150000003") && Has(cat9, "twice = 93"),
          "a section SML lists twice is not adopted - it gets a number of our own");
    Check(DirExists(out + L"\\9150000004") && !DirExists(out + L"\\9150000009") && g_errors == 1,
          "an explicit id inside SML's range is accepted only when SML's catalog gives exactly that number to this section");

    // 0.1.10: the audit round. A folder stamped by a third generator is refused, not taken
    // over; a section that fails validation keeps its folder through prune; a leading zero
    // in an id does not make a second folder; control characters in a line are refused
    // while quotes stay allowed; a missing reference is reported even when nothing else
    // could be fetched.
    EnsureDir(out + L"\\9400000050");
    Put(out + L"\\9400000050\\" + Wide(STAMP_NAME), "some_other_tool generated this folder.\r\nhash=0\r\n");
    Put(out + L"\\9400000050\\keep.txt", "theirs");
    std::string ini10 =
        "[buildings_plus]\nenabled = 1\nprune = 1\n\n"
        "[foreign]\nid = 9400000050\ndonor = donor_mine\n\n"
        "[keepme]\nid = 9400000060\ndonor = donor_mine\nname = Keep Me\n\n"
        "[zeros]\nid = 09400000061\ndonor = donor_mine\n\n"
        "[quoted]\nid = 9400000062\ndonor = donor_mine\nline = $NAME_STR \"Quoted Name\"\n\n"
        "[ctrl]\nid = 9400000063\ndonor = donor_mine\nline = $WORKERS_NEEDED 5\x01\n";
    g_warnings = 0; g_errors = 0;
    Run(ini10, "test");
    std::string otherStamp, otherKeep;
    Check(ReadTextFile(out + L"\\9400000050\\" + Wide(STAMP_NAME), &otherStamp) && Has(otherStamp, "some_other_tool") &&
              ReadTextFile(out + L"\\9400000050\\keep.txt", &otherKeep) && otherKeep == "theirs" && !DirExists(out + L"\\9400000050\\donor_mine"),
          "a folder stamped by another generator is refused and left exactly as it was");
    Check(DirExists(out + L"\\9400000060") && DirExists(out + L"\\9400000061") && !DirExists(out + L"\\09400000061"),
          "a leading zero in an id is dropped, so the folder is the ten-digit one");
    std::string quotedIni;
    Check(ReadTextFile(out + L"\\9400000062\\quoted\\building.ini", &quotedIni) && Has(quotedIni, "$NAME_STR \"Quoted Name\""),
          "a line with quotes is accepted");
    Check(!DirExists(out + L"\\9400000063"), "a line with a control character is refused");
    // The section [keepme] now fails validation (its first line has no token); with prune on,
    // its folder from the run above must survive - a typo never deletes a building.
    std::string ini11 =
        "[buildings_plus]\nenabled = 1\nprune = 1\n\n"
        "[keepme]\nid = 9400000060\ndonor = donor_mine\nline = 1 2 3\n";
    g_warnings = 0; g_errors = 0;
    Run(ini11, "test");
    Check(g_errors >= 1 && DirExists(out + L"\\9400000060") && FileExists(out + L"\\9400000060\\" + Wide(STAMP_NAME)),
          "a section refused by validation keeps its folder through prune");
    Check(!DirExists(out + L"\\9400000061") && !DirExists(out + L"\\9400000062"),
          "sections that are gone lose their folders to prune as before");
    // A donor item whose only problem is one reference into nowhere: nothing can be
    // fetched, and the missing file must still be reported.
    std::wstring mod2 = out + L"\\mod_item2";
    EnsureDir(mod2); EnsureDir(mod2 + L"\\house");
    Put(mod2 + L"\\workshopconfig.ini", "$ITEM_ID 1234567891\n$OBJECT_BUILDING house\n$END\n");
    Put(mod2 + L"\\house\\building.ini", "$NAME 6170\n$TYPE_LIVING\n");
    Put(mod2 + L"\\house\\renderconfig.ini", "$TYPE_WORKSHOP\n MODEL custom.nmf\n MATERIAL lost.mtl\n LIFE 3000\n$END\n");
    Put(mod2 + L"\\house\\custom.nmf", "mesh");
    std::string ini12 = "[buildings_plus]\nenabled = 1\n\n[lonely]\nid = 9400000070\ndonor = mod_item2\\house\nobject = Lonely\n";
    g_warnings = 0; g_errors = 0;
    Run(ini12, "test");
    Check(g_warnings == 1 && DirExists(out + L"\\9400000070\\Lonely"),
          "a reference the donor item does not have is reported even when nothing else could be fetched");

    printf(g_failed ? "RESULT %d check(s) FAILED\n" : "RESULT all buildings_plus self-tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
#endif
