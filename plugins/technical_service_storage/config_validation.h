// Startup-only validation. No game memory or save data is read or written here.
// Unknown/legacy keys are warnings, never a reason to reject the whole plugin.
#pragma once
#include <string>
#include <vector>
#include <errno.h>
#include <locale.h>
#include <limits.h>

struct ConfigRule { const char* section; const char* key; int fallback, minimum, maximum; bool boolean; };
static const ConfigRule CONFIG_RULES[] = {
    {"general","enabled",1,0,1,true}, {"general","debug",0,0,1,true},
    {"general","debug_limit",100,0,10000,false},
    {"storage_migration","enabled",1,0,1,true},
    {"ui","resource_gap",14,0,50,false},
    {"ui","office_priority_wrap",1,0,1,true},
    {"ui","office_priority_max_width",245,120,400,false},
    {"ui","office_priority_line_height",22,12,28,false},
    {"ui","material_priority_enabled",1,0,1,true},
    {"ui","material_priority_offset_x",390,100,800,false},
    {"ui","material_priority_offset_y",0,-50,100,false},
    {"ui","material_priority_width",150,60,300,false},
    {"ui","material_priority_height",24,12,60,false},
    {"ui","material_section_divider",1,0,1,true},
    {"ui","material_section_divider_height",DEFAULT_MATERIAL_DIVIDER_HEIGHT,48,100,false},
    {"ui","material_status_warnings",1,0,1,true},
    {"priority_persistence","enabled",1,0,1,true},
    {"tank_persistence","enabled",1,0,1,true},
    {"sand_diagnostic","enabled",1,0,1,true},
    {"sand_diagnostic","consumption_enabled",0,0,1,true}, // recognized, still ignored
    {"sand_diagnostic","grit_fuel_consumption_factor_percent",110,1,1000,false},
    {"sand_diagnostic","duplicate_window_ms",5000,0,60000,false},
    {"sand_diagnostic","sample_interval_ms",1000,100,60000,false},
    {"sand_diagnostic","clear_log_interval_ms",500,0,60000,false},
    {"sand_diagnostic","shadow_tank_enabled",1,0,1,true},
    {"sand_diagnostic","automatic_return_enabled",1,0,1,true},
    {"sand_diagnostic","return_lock_enabled",1,0,1,true},
    {"sand_diagnostic","return_retry_interval_ms",250,250,10000,false},
    {"sand_diagnostic","return_arrival_settle_ms",10,10,2000,false},
    {"sand_diagnostic","return_threshold_percent",20,0,100,false},
    {"sand_diagnostic","return_threshold_basis_points",2000,0,10000,false},   // 0.3.2 key, read only when the percent key is absent
    {"sand_diagnostic","tank_weight_percent",10,0,50,false},
    {"sand_diagnostic","tank_power_kg_per_kw",2,0,20,false},
    {"sand_diagnostic","tank_capacity_multiplier_percent",100,10,500,false},
    {"sand_diagnostic","tank_capacity_step_kg",50,1,1000,false},
    {"sand_diagnostic","tank_minimum_capacity_kg",250,1,10000,false},
    {"sand_diagnostic","tank_maximum_capacity_kg",5000,1,20000,false},
    {"sand_diagnostic","vehicle_tank_display_enabled",1,0,1,true},
    {"sand_diagnostic","vehicle_tank_display_offset_x",0,-1000,1000,false},
    {"sand_diagnostic","vehicle_tank_display_offset_y",22,-1000,1000,false},
    {"sand_diagnostic","global_initialization_interval_ms",1000,250,10000,false},
    {"sand_diagnostic","max_vehicles",512,1,1024,false},
    {"tank_probe","building_lifecycle",1,0,1,true}
};
struct ConfigEntry { std::string section, key, value; size_t line; };
static const char* const CONFIG_MATERIAL_SECTION = "grit_materials";
static std::vector<ConfigEntry> g_configEntries;
static bool g_configMaterialsPresent = false;
static bool g_configComplete = true;
static const size_t CONFIG_MAX_BYTES = 1024 * 1024;

static std::string ConfigTrim(std::string s)
{
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
static std::string ConfigLower(std::string s)
{
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
static const ConfigRule* FindConfigRule(const std::string& section, const std::string& key)
{
    for (const auto& r : CONFIG_RULES) if (section == r.section && key == r.key) return &r;
    return nullptr;
}
static bool KnownConfigSection(const std::string& name)
{
    if (name == CONFIG_MATERIAL_SECTION) return true;
    for (const auto& r : CONFIG_RULES) if (name == r.section) return true;
    return false;
}
// Strip only an unquoted comment separated from its value by whitespace.
static std::string ConfigValue(std::string s)
{
    bool quoted = false; char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((s[i] == '"' || s[i] == '\'') && (!quoted || s[i] == quote)) {
            quoted = !quoted; quote = s[i];
        } else if (!quoted && (s[i] == ';' || s[i] == '#') &&
                   (i == 0 || s[i-1] == ' ' || s[i-1] == '\t')) {
            s.resize(i); break;
        }
    }
    s = ConfigTrim(s);
    if (s.size() >= 2 && (s[0] == '"' || s[0] == '\'') && s.back() == s[0])
        s = s.substr(1, s.size()-2);
    return ConfigTrim(s);
}
static bool ParseConfigText(const std::string& input)
{
    g_configEntries.clear(); g_configMaterialsPresent = false; g_configComplete = false;
    if (input.size() > CONFIG_MAX_BYTES || input.find('\0') != std::string::npos) {
        Report("WARN", INI_NAME, "ini-incomplete",
            "Configuration exceeds 1 MiB or contains embedded NUL bytes; no partial settings/material list accepted");
        return false;
    }
    std::vector<std::string> seenSections;
    std::string section; bool skipSection = false; size_t number = 0;
    for (size_t pos = 0; pos < input.size();) {
        size_t end = input.find('\n', pos);
        if (end == std::string::npos) end = input.size();
        std::string line = ConfigTrim(input.substr(pos, end - pos));
        pos = end == input.size() ? end : end + 1; ++number;
        if (number == 1 && line.compare(0,3,"\xEF\xBB\xBF") == 0) line = ConfigTrim(line.substr(3));
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            size_t close = line.find(']');
            if (close == std::string::npos || !ConfigValue(line.substr(close + 1)).empty()) {
                Report("WARN", INI_NAME, "ini-section", "line=%llu malformed section; following keys ignored until the next valid section", (unsigned long long)number);
                section.clear(); skipSection = true; continue;
            }
            section = ConfigLower(ConfigTrim(line.substr(1,close-1)));
            // Normalize the historical name before duplicate detection. Both
            // spellings represent one list; the existing first-section policy stays.
            if (section == "streumaterialien") section = CONFIG_MATERIAL_SECTION;
            skipSection = !KnownConfigSection(section);
            if (skipSection) Report("WARN", INI_NAME, "ini-unknown-section",
                "line=%llu section=[%s] unknown/obsolete; ignored without disabling the plugin", (unsigned long long)number, section.c_str());
            for (const auto& s : seenSections) if (s == section) {
                skipSection = true;
                Report("WARN", INI_NAME, "ini-duplicate-section",
                    "line=%llu section=[%s] repeated; first section retained (legacy profile policy)", (unsigned long long)number, section.c_str());
                break;
            }
            seenSections.push_back(section);
            if (section == CONFIG_MATERIAL_SECTION) g_configMaterialsPresent = true;
            continue;
        }
        if (skipSection) continue;
        size_t equals = line.find('=');
        if (section.empty() || equals == std::string::npos) {
            Report("WARN", INI_NAME, "ini-entry", "line=%llu expected key=value inside a section; ignored", (unsigned long long)number);
            continue;
        }
        ConfigEntry entry = {section, ConfigTrim(line.substr(0,equals)), ConfigValue(line.substr(equals+1)), number};
        std::string key = ConfigLower(entry.key);
        if (section != CONFIG_MATERIAL_SECTION && !FindConfigRule(section,key)) {
            Report("WARN", INI_NAME, "ini-unknown-key", "line=%llu section=[%s] key='%s' unknown/obsolete; ignored", (unsigned long long)number, section.c_str(), entry.key.c_str());
            continue;
        }
        bool duplicate = false;
        for (const auto& old : g_configEntries) if (old.section == section && ConfigLower(old.key) == key) {
            // Material duplicates are handled after validation: preserve the first VALID entry.
            if (section != CONFIG_MATERIAL_SECTION) {
                Report("WARN", INI_NAME, "ini-duplicate-key",
                    "line=%llu [%s] %s repeated; first value at line=%llu retained", (unsigned long long)number, section.c_str(), entry.key.c_str(), (unsigned long long)old.line);
                duplicate = true;
            }
            break;
        }
        if (!duplicate) g_configEntries.push_back(entry);
    }
    g_configComplete = true;
    return true;
}
// Folder of this DLL, used only when plugins\technical_service_storage.ini
// is missing: a Workshop package loaded by Soviet Mod Loader or the Workshop
// Bridge carries its INI beside the DLL.
static std::string ConfigOwnDirectory()
{
    HMODULE self = nullptr; char buffer[MAX_PATH];
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)&ConfigOwnDirectory, &self) || !GetModuleFileNameA(self, buffer, (DWORD)sizeof(buffer))) return std::string();
    std::string result(buffer); size_t slash = result.find_last_of('\\');
    return slash == std::string::npos ? std::string() : result.substr(0, slash + 1);
}
static bool LoadConfigFile()
{
    // 0.3.1: plugins\technical_service_storage.ini when it exists (classic
    // installation, or the effective INI Republic Mod Manager writes), otherwise
    // the INI beside this DLL. Parsing, validation and every fallback are unchanged.
    std::string path = std::string(g_baseDir ? g_baseDir : "") + "\\" + INI_NAME;
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::string own = ConfigOwnDirectory();
        if (!own.empty() && GetFileAttributesA((own + "technical_service_storage.ini").c_str()) != INVALID_FILE_ATTRIBUTES)
            path = own + "technical_service_storage.ini";
    }
    Report("INFO", INI_NAME, "ini-path", "Configuration file: %s", path.c_str());
    g_configEntries.clear(); g_configMaterialsPresent = false; g_configComplete = false;
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            g_configComplete = true; // legacy missing-file/default-material behavior
            Report("WARN", INI_NAME, "ini-missing", "Configuration file missing; documented defaults are used");
            return true;
        }
        ReportWindows("WARN", INI_NAME, "ini-read", "Cannot open configuration; no partial material list accepted", error, "Check file permissions and restart");
        return false;
    }
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(file,&size) != 0;
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    if (ok && (size.QuadPart < 0 || size.QuadPart > CONFIG_MAX_BYTES)) { ok = false; error = ERROR_FILE_TOO_LARGE; }
    std::string input;
    if (ok) {
        input.resize((size_t)size.QuadPart);
        DWORD read = 0;
        char emptyBuffer = 0;
        ok = ReadFile(file, input.empty() ? &emptyBuffer : &input[0], (DWORD)input.size(), &read, nullptr) != 0;
        error = ok ? ERROR_SUCCESS : GetLastError();
        if (ok && read != input.size()) { ok = false; error = ERROR_HANDLE_EOF; }
    }
    CloseHandle(file);
    if (!ok) {
        ReportWindows("WARN", INI_NAME, "ini-read", "Configuration could not be read completely; no prefix accepted", error, "Correct the file (maximum 1 MiB) and restart");
        return false;
    }
    // Preserve Win32 profile compatibility with UTF-16 LE BOM files.
    if (input.size() >= 2 && (unsigned char)input[0] == 0xFF && (unsigned char)input[1] == 0xFE) {
        if ((input.size() - 2) % 2) {
            Report("WARN", INI_NAME, "ini-encoding", "Incomplete UTF-16 configuration; no prefix accepted");
            return false;
        }
        std::wstring wide((input.size()-2)/2, L'\0');
        if (!wide.empty()) memcpy(&wide[0],input.data()+2,wide.size()*sizeof(wchar_t));
        int n = wide.empty() ? 0 : WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),(int)wide.size(),nullptr,0,nullptr,nullptr);
        if (!wide.empty() && !n) {
            DWORD conversionError = GetLastError();
            ReportWindows("WARN",INI_NAME,"ini-encoding","Invalid UTF-16 configuration",conversionError,"Save as UTF-8 or valid UTF-16 LE");
            return false;
        }
        input.assign(n,'\0');
        if (n && !WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.data(),(int)wide.size(),&input[0],n,nullptr,nullptr)) {
            DWORD conversionError = GetLastError();
            ReportWindows("WARN",INI_NAME,"ini-encoding","UTF-16 conversion failed",conversionError,"Save as UTF-8 and restart");
            return false;
        }
    }
    return ParseConfigText(input);
}
static int ConfigInt(const char* section, const char* key)
{
    const ConfigRule* rule = FindConfigRule(ConfigLower(section), ConfigLower(key));
    if (!rule) { Report("ERROR", INI_NAME, "ini-schema", "Internal setting [%s] %s has no schema",section,key); return 0; }
    for (const auto& entry : g_configEntries) if (entry.section == rule->section && ConfigLower(entry.key) == rule->key) {
        errno = 0; char* end = nullptr;
        long long value = _strtoi64(entry.value.c_str(),&end,10);
        if (entry.value.empty() || end == entry.value.c_str() || *end || errno == ERANGE ||
            (rule->boolean && value != 0 && value != 1)) {
            Report("WARN", INI_NAME, "ini-value",
                "line=%llu [%s] %s='%s' invalid; expected %s; default=%d",
                (unsigned long long)entry.line,section,key,entry.value.c_str(),
                rule->boolean ? "0 or 1" : "a complete decimal integer",rule->fallback);
            return rule->fallback;
        }
        int result = value < rule->minimum ? rule->minimum : value > rule->maximum ? rule->maximum : (int)value;
        if (result != value) Report("WARN",INI_NAME,"ini-range",
            "line=%llu [%s] %s=%lld clamped to %d (range=%d..%d)",(unsigned long long)entry.line,section,key,value,result,rule->minimum,rule->maximum);
        return result;
    }
    return rule->fallback;
}
// True when the INI carries this key at all (used to prefer a new key over its legacy form).
static bool ConfigKeyPresent(const char* section, const char* key)
{
    std::string s = ConfigLower(section), k = ConfigLower(key);
    for (const auto& entry : g_configEntries) if (entry.section == s && ConfigLower(entry.key) == k) return true;
    return false;
}
static bool ParseMaterialStrength(const char* text, double* out)
{
    if (!text || !out) return false;
    // A per-call C locale leaves the game's process/thread locale untouched.
    _locale_t locale = _create_locale(LC_NUMERIC,"C");
    if (!locale) return false;
    errno = 0; char* end = nullptr;
    double value = _strtod_l(text,&end,locale);
    bool valid = text && text[0] && end != text && !*end && errno != ERANGE &&
                 _finite(value) && value >= 0.0 && value <= 1.0;
    _free_locale(locale);
    if (valid) *out = value;
    return valid;
}
