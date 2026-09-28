// Republic Mod Manager 0.5.21: generic manifest/schema driven plugin deployment.
// Never loads a DLL during discovery and never edits Workshop defaults or loader code.
// Since 0.9.0 a package needs only [mod] and [hooks] dll; everything Autoload used
// to declare is derived by convention, and a plugin without a launcher schema gets
// its user interface from its own INI (comments become descriptions).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using Microsoft.Win32;

namespace TesmioAutoload
{
    // Messages the core hands to the window as a language key plus arguments, so
    // the window renders them in the chosen language (Language.Localize). Logs and
    // tests read them through Plain. Layout: \u0001key\u0002arg\u0002arg...
    public static class Msg
    {
        public static string Key(string key, params object[] args) { return "\u0001" + key + String.Concat(args.Select(a => "\u0002" + Convert.ToString(a, System.Globalization.CultureInfo.InvariantCulture))); }
        public static bool IsKey(string text) { return text != null && text.StartsWith("\u0001", StringComparison.Ordinal); }
        public static string KeyOf(string text) { if (!IsKey(text)) return ""; int at = text.IndexOf('\u0002'); return at < 0 ? text.Substring(1) : text.Substring(1, at - 1); }
        public static string[] ArgsOf(string text) { if (!IsKey(text)) return new string[0]; return text.Split('\u0002').Skip(1).ToArray(); }
        public static string Plain(string text) { if (!IsKey(text)) return text; string[] args = ArgsOf(text); return KeyOf(text) + (args.Length > 0 ? " " + String.Join(" | ", args) : ""); }
    }

    public static class SafeFiles
    {
        public static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        public static string Hash(byte[] bytes)
        {
            using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
        }
        public static string HashFile(string path) { return File.Exists(path) ? Hash(File.ReadAllBytes(path)) : "absent"; }
        public static void NoLinks(string path)
        {
            for (string p = Path.GetFullPath(path); p != null; p = Path.GetDirectoryName(p))
                if ((Directory.Exists(p) || File.Exists(p)) && (File.GetAttributes(p) & FileAttributes.ReparsePoint) != 0)
                    throw new IOException(Msg.Key("err_verknuepfung_reparse_point_nicht", p));
        }
        public static string Child(string root, string relative)
        {
            if (String.IsNullOrWhiteSpace(relative) || Path.IsPathRooted(relative) || relative.Contains(':') ||
                relative.Split('\\', '/').Any(x => x == ".." || x == "." || x.Length == 0 || x.EndsWith(".") || x.EndsWith(" ")))
                throw new IOException(Msg.Key("err_ungueltiger_relativer_pfad", relative));
            string prefix = Path.GetFullPath(root).TrimEnd('\\', '/') + Path.DirectorySeparatorChar;
            string result = Path.GetFullPath(Path.Combine(prefix, relative));
            if (!result.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) throw new IOException(Msg.Key("err_pfad_verlaesst_paket", relative));
            NoLinks(result);
            return result;
        }
        public static byte[] Read(string path, int limit)
        {
            NoLinks(path);
            using (var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
            {
                if (file.Length > limit) throw new IOException(Msg.Key("err_datei_zu_gross", path));
                var data = new byte[(int)file.Length];
                int done = 0;
                while (done < data.Length)
                {
                    int n = file.Read(data, done, data.Length - done);
                    if (n == 0) throw new EndOfStreamException(path);
                    done += n;
                }
                return data;
            }
        }
        public static string Text(string path) { return Decode(Read(path, 1024 * 1024)); }
        public static string Decode(byte[] bytes)
        {
            string text = Utf8.GetString(bytes);
            if (text.IndexOf('\0') >= 0 || text.StartsWith("\uFEFF", StringComparison.Ordinal)) throw new FormatException(Msg.Key("err_ini_muss_utf_8"));
            return text;
        }
        public static void RequireX64Dll(byte[] b)
        {
            if (b.Length < 256 || b[0] != 'M' || b[1] != 'Z') throw new FormatException(Msg.Key("err_keine_pe_dll"));
            int p = BitConverter.ToInt32(b, 60);
            if (p < 64 || p > b.Length - 26 || BitConverter.ToUInt32(b, p) != 0x4550 ||
                BitConverter.ToUInt16(b, p + 4) != 0x8664 || (BitConverter.ToUInt16(b, p + 22) & 0x2000) == 0 ||
                BitConverter.ToUInt16(b, p + 24) != 0x20b)
                throw new FormatException(Msg.Key("err_eine_x64_plugin_dll"));
        }
    }

    // Original lines are retained. Repeated sections/keys are rejected explicitly,
    // not collapsed: plugins with repeated directives need a future dedicated adapter.
    public sealed class Ini
    {
        public readonly List<string> Lines;
        public readonly Dictionary<string, string> Values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        public readonly HashSet<string> Sections = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private readonly Dictionary<string, int> indices = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        public readonly string Newline;
        public Ini(string text)
        {
            if (text.Length > 1024 * 1024 || text.IndexOf('\0') >= 0 || text.StartsWith("\uFEFF", StringComparison.Ordinal)) throw new FormatException(Msg.Key("err_ini_zu_gross_oder"));
            Newline = text.Contains("\r\n") ? "\r\n" : "\n";
            Lines = text.Replace("\r\n", "\n").Split('\n').ToList();
            string section = null;
            for (int i = 0; i < Lines.Count; ++i)
            {
                string line = Lines[i].Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#") || line.All(c => c == '-')) continue;
                if (line.StartsWith("["))
                {
                    if (!line.EndsWith("]") || line.Length < 3) throw new FormatException(Msg.Key("err_zeile_abschnitt_ungueltig", i + 1));
                    section = line.Substring(1, line.Length - 2).Trim();
                    if (!Sections.Add(section)) throw new FormatException(Msg.Key("err_zeile_abschnitt_doppelt", i + 1, section));
                    continue;
                }
                int eq = line.IndexOf('=');
                if (section == null || eq <= 0) throw new FormatException(Msg.Key("err_zeile_zuweisung_ohne_abschnitt", i + 1));
                string key = line.Substring(0, eq).Trim(), value = line.Substring(eq + 1).Trim();
                // An empty value is legal INI (the loader's own plugins ship "speed_concrete =" for
                // "use the default"); only an empty key is a broken line.
                if (key.Length == 0) throw new FormatException(Msg.Key("err_zeile_leerer_schluessel_wert", i + 1));
                string id = Id(section, key);
                if (Values.ContainsKey(id)) throw new FormatException(Msg.Key("err_zeile_mehrdeutiger_doppelter_schluessel", i + 1, id));
                Values.Add(id, value); indices.Add(id, i);
            }
        }
        public static string Id(string section, string key) { return section.ToLowerInvariant() + "/" + key.ToLowerInvariant(); }
        public string Get(string section, string key, string fallback = null)
        {
            string value; return Values.TryGetValue(Id(section, key), out value) ? value : fallback;
        }
        public string Required(string section, string key)
        {
            string value = Get(section, key); if (value == null) throw new FormatException(Msg.Key("err_fehlt", section, key)); return value;
        }
        public string Render(Dictionary<string, string> changes)
        {
            var lines = new List<string>(Lines);
            var extra = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in changes)
            {
                if (pair.Value.Contains('\n') || pair.Value.Contains('\r') || pair.Value.Contains('\0') || pair.Value.Length == 0)
                    throw new FormatException(Msg.Key("err_ungueltiger_ini_wert", pair.Key));
                int at;
                if (indices.TryGetValue(pair.Key, out at))
                {
                    if (Values[pair.Key] != pair.Value) lines[at] = lines[at].Substring(0, lines[at].IndexOf('=') + 1) + " " + pair.Value;
                }
                else
                {
                    string[] id = pair.Key.Split('/');
                    if (id.Length != 2) throw new FormatException(Msg.Key("err_ungueltiger_ini_schluessel", pair.Key));
                    if (!extra.ContainsKey(id[0])) extra[id[0]] = new List<string>();
                    extra[id[0]].Add(id[1] + " = " + pair.Value);
                }
            }
            // Insert missing keys into their existing section, never create duplicates.
            var result = new List<string>(); string current = null;
            Action flush = () => { if (current != null && extra.ContainsKey(current)) { result.AddRange(extra[current]); extra.Remove(current); } };
            foreach (string line in lines)
            {
                string t = line.Trim();
                if (t.StartsWith("[") && t.EndsWith("]")) { flush(); current = t.Substring(1, t.Length - 2).Trim().ToLowerInvariant(); }
                result.Add(line);
            }
            flush();
            foreach (var section in extra) { result.Add("[" + section.Key + "]"); result.AddRange(section.Value); }
            return String.Join(Newline, result);
        }
        public static string Sparse(Dictionary<string, string> values)
        {
            var b = new StringBuilder("; Personal overrides. Package defaults remain untouched.\r\n");
            foreach (var group in values.OrderBy(p => p.Key).GroupBy(p => p.Key.Split('/')[0]))
            {
                b.Append("\r\n[" + group.Key + "]\r\n");
                foreach (var pair in group) b.Append(pair.Key.Substring(pair.Key.IndexOf('/') + 1) + " = " + pair.Value + "\r\n");
            }
            return b.ToString();
        }
        public string RewriteValues(Dictionary<string, string> values)
        {
            var removed = new HashSet<int>(indices.Where(p => !values.ContainsKey(p.Key)).Select(p => p.Value));
            string text = String.Join(Newline, Lines.Where((line, i) => !removed.Contains(i)));
            return new Ini(text).Render(values);
        }
    }

    public sealed class Field
    {
        public string Section, Key, Label, Description, Type;
        public string DefaultValue;
        public bool HasPackageDefault;
        public string CollectionId = "", CollectionItem = "";
        public bool RemovableCollectionItem;
        public decimal Minimum, Maximum;
        public string[] Choices;
        // Generic (INI-derived) schema: a value may carry an inline comment such as
        // "130 ; (stock 121)". The comment is ignored for comparison and validation
        // and is dropped only when the user actually changes that value.
        public bool Lenient;
        public string Id { get { return Ini.Id(Section, Key); } }
        public string Normalize(string value)
        {
            if (Type == "readonly") return value;
            if (Lenient) value = GenericSchema.StripComment(value);
            if (Type == "text")
            {
                // Lenient (INI-derived or local) schemas accept an empty text value: the plugin then uses its default.
                if ((value.Length == 0 && !Lenient) || value.Length > 4096 || value.IndexOfAny(new[] { '\n', '\r', '\0' }) >= 0) throw new FormatException(Msg.Key("err_ein_einzeiliger_wert_ist", Label));
                return value;
            }
            if (Type == "boolean") { if (value != "0" && value != "1") throw new FormatException(Msg.Key("err_nur_0_oder_1", Label)); return value; }
            if (Type == "choice") { if (!Choices.Contains(value)) throw new FormatException(Msg.Key("err_ungueltige_auswahl", Label)); return value; }
            decimal n;
            if (!Decimal.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out n) || n < Minimum || n > Maximum ||
                (Type == "integer" && Decimal.Truncate(n) != n)) throw new FormatException(Msg.Key("err_zahl_von_bis_erforderlich", Label, Minimum, Maximum));
            return n.ToString("0.############################", CultureInfo.InvariantCulture);
        }
        public bool Equivalent(string left,string right)
        {
            try{return Normalize(left)==Normalize(right);}catch(FormatException){return false;}
        }
    }

    public sealed class CollectionSpec
    {
        public string Id, Source, SourcePlugin, SourceSection, SourceReadySection, SourceReadyKey, SourceReadyValue;
        public string ResourceGroup, MatrixGroup, CountSection, CountKey, ItemPrefix;
        public string[] TargetSections, TargetLabels, TargetLabelKeys, TargetIcons;
        public string Type, DefaultValue, Unit, UnitKey, ItemLabel, ItemLabelKey, ItemDescriptionKey, CoefficientDescriptionKey;
        public string EmptyNotice, EmptyNoticeKey, EmptyNoticeStyle;
        public decimal Minimum, Maximum, Step;
        public int MaximumItems;
        public bool UserOwned, AllowRemoveDefaults, RequirePositiveWhenEnabled;
        public string CountId { get { return Ini.Id(CountSection, CountKey); } }
    }

    // Tolerant reader for soviet.mod.ini. Soviet Mod Loader allows a directive to
    // repeat (several `dll =` lines under [hooks]) and keeps commented-out sections
    // around; the strict Ini class rejects both on purpose, so the manifest gets
    // its own reader. Nothing here is executed or resolved - it is text.
    public sealed class Manifest
    {
        readonly Dictionary<string, List<KeyValuePair<string, string>>> sections = new Dictionary<string, List<KeyValuePair<string, string>>>(StringComparer.OrdinalIgnoreCase);
        public static Manifest Parse(string text)
        {
            var m = new Manifest(); string section = null; int number = 0;
            foreach (string raw in text.Replace("\r\n", "\n").Split('\n'))
            {
                number++; string line = raw.Trim();
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                if (line.StartsWith("["))
                {
                    if (!line.EndsWith("]") || line.Length < 3) throw new FormatException(Msg.Key("err_manifest_zeile_abschnitt_ungueltig", number));
                    section = line.Substring(1, line.Length - 2).Trim();
                    if (!sections_(m).ContainsKey(section)) sections_(m)[section] = new List<KeyValuePair<string, string>>();
                    continue;
                }
                int eq = line.IndexOf('=');
                if (section == null || eq <= 0) throw new FormatException(Msg.Key("err_manifest_zeile_zuweisung_ohne", number));
                string key = line.Substring(0, eq).Trim(), value = line.Substring(eq + 1).Trim();
                if (key.Length == 0) throw new FormatException(Msg.Key("err_manifest_zeile_leerer_schluessel", number));
                sections_(m)[section].Add(new KeyValuePair<string, string>(key, value));
            }
            return m;
        }
        static Dictionary<string, List<KeyValuePair<string, string>>> sections_(Manifest m) { return m.sections; }
        public bool Has(string section) { return sections.ContainsKey(section); }
        // Keys of a section that actually carry a value (a bare `key =` counts as absent).
        public List<string> Keys(string section)
        {
            List<KeyValuePair<string, string>> list;
            return sections.TryGetValue(section, out list) ? list.Where(x => x.Value.Length > 0).Select(x => x.Key).ToList() : new List<string>();
        }
        public List<string> All(string section, string key)
        {
            List<KeyValuePair<string, string>> list;
            if (!sections.TryGetValue(section, out list)) return new List<string>();
            return list.Where(x => x.Key.Equals(key, StringComparison.OrdinalIgnoreCase) && x.Value.Length > 0).Select(x => x.Value).ToList();
        }
        public string Get(string section, string key, string fallback = null)
        {
            var values = All(section, key);
            if (values.Count > 1) throw new FormatException(Msg.Key("err_mehrdeutiger_doppelter_schluessel_im", section, key));
            return values.Count == 1 ? values[0] : fallback;
        }
        public string Required(string section, string key)
        {
            string value = Get(section, key);
            if (value == null) throw new FormatException(Msg.Key("err_fehlt_im_manifest", section, key));
            return value;
        }
    }

    // Builds a launcher schema for a plugin INI that ships without one. Every key
    // becomes a field, the comment block above it (plus an inline comment) becomes
    // the description, and the value decides the type. Fields are grouped by INI
    // section on the application's own settings tab. The result is an ordinary
    // schema Ini, so the rest of the application does not know the difference -
    // except ConfigRules, which cannot reject unknown keys it never declared.
    public static class GenericSchema
    {
        static readonly Regex BooleanKey = new Regex("^(enabled?|debug|probe|verbose|active|front|log|allow_.*|use_.*|show_.*|hide_.*|force_.*|.*_enabled|.*_only|.*_check|.*_fix)$", RegexOptions.IgnoreCase);
        static readonly Regex BooleanHint = new Regex(@"\b0\s*(or|oder|/|and|und)\s*1\b|\b1\s*(or|oder|/)\s*0\b|\b0\s+(leaves|disables|turns off|unloads|removes|schaltet|deaktiviert|laesst|lÃ¤sst)\b|\b(on/off|ein/aus|aktiviert|deaktiviert|disable|enable)\b", RegexOptions.IgnoreCase);
        static readonly Regex IntegerValue = new Regex(@"^-?\d{1,18}$"), DecimalValue = new Regex(@"^-?(\d{1,18}\.\d{1,18}|\.\d{1,18}|\d{1,18}\.)$");
        // A 0/1 value is a switch unless something says it is a quantity: a key
        // that names a count, or a comment that mentions another whole number or a
        // range ("2-12", "1..13", "up to 400"). Version-like tokens (v1.6, 1.1.1.9)
        // and decimals are not whole numbers for this purpose.
        static readonly Regex QuantityKey = new Regex("(^|_)(count|max|maximum|min|minimum|limit|number|num|days|frequency|size|level|index|slot|priority|seed|port|id|scale|steps?|amount|radius|distance|speed|percent|factor|ratio|delay|timeout|interval|width|height|length|depth|mode|rva|address|offset|origin|component|channel|x|y|z|cm|mm|m|km|kg|ms|sec|seconds|minutes|hours|value|weight|strength|threshold|version)(_|$)", RegexOptions.IgnoreCase);
        static readonly Regex OtherNumber = new Regex(@"(?<![\w.\-])(\d+)(?![\w.])");
        static readonly Regex RangeHint = new Regex(@"\b\d+\s*(-|\.\.|to|bis)\s*\d+\b|\b(up to|at most|at least|maximal|mindestens|hoechstens|hÃ¶chstens)\b", RegexOptions.IgnoreCase);
        static bool LooksLikeSwitch(string key, string value, string text)
        {
            if (value != "0" && value != "1") return false;
            if (BooleanKey.IsMatch(key) || BooleanHint.IsMatch(text)) return true;
            if (QuantityKey.IsMatch(key) || RangeHint.IsMatch(text)) return false;
            foreach (Match m in OtherNumber.Matches(text)) if (m.Groups[1].Value != "0" && m.Groups[1].Value != "1") return false;
            return true;
        }
        public static string StripComment(string value)
        {
            if (value == null) return "";
            for (int i = 0; i < value.Length; i++)
            {
                char c = value[i];
                if ((c == ';' || c == '#') && (i == 0 || Char.IsWhiteSpace(value[i - 1]))) return value.Substring(0, i).Trim();
            }
            return value.Trim();
        }
        static string Clean(string comment)
        {
            string s = comment.Trim();
            if (s.StartsWith(";") || s.StartsWith("#")) s = s.Substring(1);
            if (s.StartsWith(" ")) s = s.Substring(1);
            return s.TrimEnd();
        }
        static string OneLine(string text) { return text.Replace("\r", " ").Replace("\n", " ").Replace("\0", "").Trim(); }
        public static Ini Build(Package p)
        {
            var b = new StringBuilder(); var lines = p.Defaults.Lines;
            var header = new List<string>();
            for (int i = 0; i < lines.Count; i++)
            {
                string t = lines[i].Trim();
                if (t.StartsWith("[")) break;
                if (t.StartsWith(";") || t.StartsWith("#")) header.Add(Clean(t));
            }
            // The header goes into the one-line heading area: first paragraph only,
            // cut at a sentence end once it gets long.
            var firstParagraph = new List<string>();
            foreach (string h in header) { if (h.Length == 0 && firstParagraph.Count > 0) break; if (h.Length > 0) firstParagraph.Add(h); }
            string description = OneLine(String.Join(" ", firstParagraph));
            if (description.Length > 200)
            {
                int cut = description.LastIndexOf(". ", 200, StringComparison.Ordinal);
                description = cut > 40 ? description.Substring(0, cut + 1) : description.Substring(0, 200).TrimEnd() + " ...";
            }
            b.Append("[launcher]\nlayout_version = 1\nvisible = 1\nid = " + p.Id + "\nconfig = " + p.ConfigName + "\nmaximum_value_length = 4096\ndefault_tab = settings\nicon = builtin:gear\n");
            if (description.Length > 0) b.Append("description = " + description + "\n");
            string enabledField = ""; var fields = new StringBuilder(); var groupText = new StringBuilder(); var groups = new List<string>(); string section = null; int order = 0;
            for (int i = 0; i < lines.Count; i++)
            {
                string t = lines[i].Trim();
                if (t.Length == 0 || t.StartsWith(";") || t.StartsWith("#") || t.All(c => c == '-')) continue;
                if (t.StartsWith("["))
                {
                    section = t.Substring(1, t.Length - 2).Trim();
                    if (!groups.Contains(section)) { groups.Add(section); groupText.Append("[group:" + section + "]\ntab = settings\nlabel = " + section + "\norder = " + (groups.Count * 10) + "\n"); }
                    continue;
                }
                int eq = t.IndexOf('='); if (section == null || eq <= 0) continue;
                string key = t.Substring(0, eq).Trim(), raw = t.Substring(eq + 1).Trim(), value = StripComment(raw);
                if (key.Length == 0 || raw.Length == 0) continue;
                var block = new List<string>();
                for (int j = i - 1; j >= 0; j--) { string c = lines[j].Trim(); if (c.StartsWith(";") || c.StartsWith("#")) block.Insert(0, Clean(c)); else break; }
                if (raw.Length > value.Length) block.Add(Clean(raw.Substring(value.Length).Trim()));
                // Line structure survives as the two-character sequence \n, which the
                // presentation turns back into line breaks; an INI value itself cannot
                // hold a newline. Runs of blank comment lines collapse to one.
                var kept = new List<string>();
                foreach (string line in block) { if (line.Length == 0 && (kept.Count == 0 || kept[kept.Count - 1].Length == 0)) continue; kept.Add(line); }
                while (kept.Count > 0 && kept[kept.Count - 1].Length == 0) kept.RemoveAt(kept.Count - 1);
                string text = OneLine(String.Join("\\n", kept));
                string type, range = "";
                if (LooksLikeSwitch(key, value, text)) type = "boolean";
                else if (IntegerValue.IsMatch(value)) { type = "integer"; range = "minimum = -1000000000\nmaximum = 1000000000\nstep = 1\n"; }
                else if (DecimalValue.IsMatch(value)) { type = "decimal"; range = "minimum = -1000000000\nmaximum = 1000000000\nstep = 0.01\n"; }
                else type = "text";
                if (enabledField.Length == 0 && type == "boolean" && key.Equals("enabled", StringComparison.OrdinalIgnoreCase)) enabledField = Ini.Id(section, key);
                order++;
                fields.Append("[field:g" + order + "]\nsection = " + section + "\nkey = " + key + "\nlabel = " + key + "\ntype = " + type + "\ngroup = " + section + "\norder = " + order + "\n" + range);
                if (text.Length > 0) fields.Append("description = " + text + "\n");
            }
            if (enabledField.Length > 0) b.Append("enabled_field = " + enabledField + "\n");
            b.Append(groupText);
            if (groups.Count == 0) b.Append("[group:general]\ntab = settings\nlabel = general\norder = 10\nnotice = Dieses Paket hat keine Einstellungen.\n");
            b.Append(fields);
            return new Ini(b.ToString());
        }
    }

    public sealed class Package
    {
        public string Root, Id, Name, Version, Target, ConfigName, DllPath, DefaultsPath, SchemaPath, EnabledField;
        public bool LocalCopyOffered; public string AssetsDir, AssetsFolder = ""; public readonly List<string> AssetFiles = new List<string>();
        // 0.4.80: a content package's INI fragments by kind (resources, deposits, needs, buildings).
        public readonly Dictionary<string, string> ContentFragments = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        // A keyed editor schema (editor_type = keyed_*) shipped in the package: the
        // master-detail editor owns the INI, Session only deploys DLL, assets and switches.
        public string EditorSchema; public bool EditorManaged;
        public byte[] Dll, DefaultsBytes = new byte[0];
        public Ini Defaults, Schema;
        // "plugin": a native hook DLL this application can deploy and configure.
        // "content": SML resources/deposits/needs/buildings only - listed, never deployed.
        public string Kind = "plugin";
        // Generic: no launcher schema shipped, user interface derived from the INI.
        // UserOverlay: the plugin reads user_config\<name>.ini itself, so the
        // original INI is deployed byte for byte and never carries personal values.
        // HasConfig: false for a DLL that ships without any INI.
        public bool Generic, UserOverlay, HasConfig = true;
        // The SML capability that takes this package's place while SML runs ("" = none).
        public string ReplacedBySml = "";
        // Installed: a DLL found in build\plugins without a Workshop package. Its
        // configuration is managed as a protected base (see InstalledPlugins):
        // UpstreamPath keeps the original INI, RefreshUpstream says that the INI
        // currently in plugins\ is the (new) original and must be copied there.
        public bool Installed, RefreshUpstream;
        // A local schema (settings_schemas\<name>.launcher.ini) describes a foreign
        // INI it does not own: values may carry inline comments and the INI may
        // have keys the schema does not know. Only a package's own schema is strict.
        public bool LenientValues;
        public string UpstreamPath = "", DefaultsHash = "absent";
        public readonly List<string> Hints = new List<string>();
        // [dependencies] of the SML manifest, resolved against the catalog by Catalog.ResolveDependencies.
        public readonly List<Dependency> Dependencies = new List<Dependency>();
        public readonly List<Field> Fields = new List<Field>();
        public readonly List<CollectionSpec> Collections = new List<CollectionSpec>();
        public readonly List<string> RequiresLocal = new List<string>(), ConflictsLocal = new List<string>();
        public readonly Dictionary<string, string> InputHashes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        public bool Visible;
        public int MaximumValueLength = 4096;
        public bool IsDefault(string id,string value)
        {
            string standard;if(!Defaults.Values.TryGetValue(id,out standard))return false;
            Field field=Fields.FirstOrDefault(x=>x.Id==id);
            return field==null?standard==value:field.Equivalent(standard,value);
        }
        public override string ToString() { return Name + " â€” " + Version + " â€” " + Root; }
        // 0.4.48: a file or folder name under [assets] dir - letters, digits, space and plain punctuation,
        // starting with a letter or digit, not ending in a dot or space (Windows would drop it).
        static bool SafeAssetName(string part)
        { return !String.IsNullOrEmpty(part) && part.Length <= 96 && Regex.IsMatch(part, "^[A-Za-z0-9][A-Za-z0-9 ._+()&,'-]*$") && !part.EndsWith(".") && !part.EndsWith(" ") && !part.Contains(".."); }
        static bool SafeToken(string value, int maximum)
        { return !String.IsNullOrWhiteSpace(value) && value.Length <= maximum && Regex.IsMatch(value, "^[A-Za-z0-9][A-Za-z0-9._-]*$"); }
        static string[] Tokens(string raw, string label)
        {
            if (String.IsNullOrWhiteSpace(raw)) return new string[0];
            string[] values = raw.Split('|').Select(x => x.Trim()).ToArray();
            if (values.Any(x => !SafeToken(x, 64)) || values.Distinct(StringComparer.OrdinalIgnoreCase).Count() != values.Length)
                throw new FormatException(Msg.Key("err_ungueltige_oder_doppelte_kennung", label));
            return values;
        }
        static string[] List(Ini ini, string section, string key, int length, string fallback)
        {
            string raw = ini.Get(section, key, "");
            string[] values = raw.Length == 0 ? Enumerable.Repeat(fallback, length).ToArray() : raw.Split('|').Select(x => x.Trim()).ToArray();
            if (values.Length != length) throw new FormatException(Msg.Key("err_anzahl_passt_nicht_zu", section, key));
            return values;
        }
        // [assets] dir of a plugin package (travels with the DLL) or [content] assets of a content
        // package (copied into the loader's vfs): a folder of plain files, no executables.
        static void LoadAssets(Package p, string declaredAssets)
        {
            if (declaredAssets.Length == 0) return;
            p.AssetsDir = SafeFiles.Child(p.Root, declaredAssets);
            if (!Directory.Exists(p.AssetsDir)) throw new FormatException(Msg.Key("err_deklarierter_assets_ordner_fehlt", declaredAssets));
            p.AssetsFolder = Path.GetFileName(p.AssetsDir.TrimEnd('\\', '/'));
            if (!SafeToken(p.AssetsFolder, 64) || p.AssetsFolder.Equals(p.Target + ".dll", StringComparison.OrdinalIgnoreCase)) throw new FormatException(Msg.Key("err_ungueltiger_assets_ordnername", p.AssetsFolder));
            SafeFiles.NoLinks(p.AssetsDir);
            foreach (string file in Directory.GetFiles(p.AssetsDir, "*", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                string relative = file.Substring(p.AssetsDir.Length).TrimStart('\\', '/');
                if (relative.Split('\\', '/').Any(part => !SafeAssetName(part))) throw new FormatException(Msg.Key("err_ungueltiger_asset_dateiname", relative));
                if (relative.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) || relative.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)) throw new FormatException(Msg.Key("err_ausfuehrbare_dateien_gehoeren_nicht", relative));
                if (new FileInfo(file).Length > 64L * 1024 * 1024) throw new FormatException(Msg.Key("err_asset_zu_gross", relative));
                p.AssetFiles.Add(relative);
            }
            if (p.AssetFiles.Count > 512) throw new FormatException(Msg.Key("err_zu_viele_asset_dateien"));
        }
        public static Package Load(string root)
        {
            var p = new Package(); p.Root = Path.GetFullPath(root); SafeFiles.NoLinks(p.Root);
            string manifestPath = SafeFiles.Child(p.Root, "soviet.mod.ini");
            byte[] manifestBytes = SafeFiles.Read(manifestPath, 1024 * 1024);
            var manifest = Manifest.Parse(SafeFiles.Decode(manifestBytes));
            p.InputHashes.Add(manifestPath, SafeFiles.Hash(manifestBytes));
            p.Id = manifest.Required("mod", "id"); p.Name = manifest.Required("mod", "name"); p.Version = manifest.Get("mod", "version", "?");
            if (manifest.Get("mod", "enabled", "1") != "1") throw new FormatException(Msg.Key("err_paket_ist_im_manifest"));
            if (!SafeToken(p.Id, 160)) throw new FormatException(Msg.Key("err_ungueltige_paket_id", p.Id));
            if (p.Name.Length > 200 || p.Name.Any(Char.IsControl) || p.Version.Length > 80 || p.Version.Any(Char.IsControl)) throw new FormatException(Msg.Key("err_ungueltiger_paketname_oder_version"));
            // The API range is optional; when given it must include the loader's API 4.
            string apiMinRaw = manifest.Get("mod", "tesmio_api_min", ""), apiMaxRaw = manifest.Get("mod", "tesmio_api_max", "");
            if (apiMinRaw.Length > 0 || apiMaxRaw.Length > 0)
            {
                int apiMin = 1, apiMax = 4;
                if ((apiMinRaw.Length > 0 && !Int32.TryParse(apiMinRaw, out apiMin)) || (apiMaxRaw.Length > 0 && !Int32.TryParse(apiMaxRaw, out apiMax)) || apiMin > 4 || apiMax < 4)
                    throw new FormatException(Msg.Key("err_tesmioloader_api_4_liegt"));
            }
            // [autoload] and [configuration] are optional overrides of what is derived below.
            string format = manifest.Get("autoload", "format", ""), declaredKind = manifest.Get("autoload", "kind", "");
            if (format.Length > 0 && format != "1") throw new FormatException(Msg.Key("err_republic_mod_manager_unterstuetzt"));
            if (declaredKind.Length > 0 && declaredKind != "plugin" && declaredKind != "content") throw new FormatException(Msg.Key("err_unbekannte_autoload_kind", declaredKind));
            var dlls = manifest.All("hooks", "dll");
            var content = manifest.Keys("content");
            var dependencies = manifest.Keys("dependencies");
            if (dlls.Count > 1) throw new FormatException(Msg.Key("err_mehrere_native_hooks_in", dlls.Count));
            if (dlls.Count == 0)
            {
                if (content.Count == 0) throw new FormatException(Msg.Key("err_manifest_ohne_hooks_dll"));
                // A content package (the Soviet Mod Loader convention): SML merges it at game
                // start; RMM provides it through the editors of the target plugins (0.4.80).
                p.Kind = "content"; p.HasConfig = false; p.Target = ""; p.ConfigName = ""; p.Visible = false;
                foreach (string key in ContentTargets.Keys)
                {
                    string relative = manifest.Get("content", key, ""); if (relative.Length == 0) continue;
                    string path = SafeFiles.Child(p.Root, relative);
                    if (!File.Exists(path)) throw new FormatException(Msg.Key("err_inhaltsdatei_fehlt", key, relative));
                    byte[] bytes = SafeFiles.Read(path, 1024 * 1024); string text = SafeFiles.Decode(bytes);
                    try { new LooseIni(text); } catch (Exception) { throw new FormatException(Msg.Key("err_inhaltsdatei_ungueltig", relative)); }
                    p.ContentFragments[key] = text; p.InputHashes.Add(path, SafeFiles.Hash(bytes));
                }
                foreach (string key in content)
                    // allow_settings is Soviet Mod Loader's switch for the [custom:] sections of a
                    // package; RMM reads those sections anyway, so the key is simply not ours to
                    // complain about (0.5.9).
                    if (!ContentTargets.Keys.Contains(key, StringComparer.OrdinalIgnoreCase) && !key.Equals("assets", StringComparison.OrdinalIgnoreCase) && !key.Equals("allow_settings", StringComparison.OrdinalIgnoreCase)) p.Hints.Add(Msg.Key("hint_content_unknown_kind", key));
                LoadAssets(p, manifest.Get("content", "assets", ""));
                if (p.ContentFragments.Count == 0 && p.AssetFiles.Count == 0) throw new FormatException(Msg.Key("err_inhaltspaket_ohne_inhalt"));
                foreach (string id in dependencies)
                {
                    if (!SafeToken(id, 160)) throw new FormatException(Msg.Key("err_ungueltige_abhaengigkeits_id", id));
                    p.Dependencies.Add(new Dependency { Id = id, Constraint = manifest.Get("dependencies", id, "").Trim() });
                }
                p.Hints.Add(Msg.Key("hint_sml_content_package", String.Join(", ", p.ContentFragments.Keys.Concat(p.AssetFiles.Count > 0 ? new[] { "assets" } : new string[0]))));
                p.Defaults = new Ini(""); p.Schema = new Ini("[launcher]\nlayout_version = 1\nvisible = 0\nid = " + p.Id + "\nconfig = none.ini\n");
                return p;
            }
            if (declaredKind == "content") throw new FormatException(Msg.Key("err_autoload_kind_content_passt"));
            p.DllPath = SafeFiles.Child(p.Root, dlls[0]);
            p.Target = Path.GetFileNameWithoutExtension(p.DllPath);
            if (!SafeToken(p.Target, 64)) throw new FormatException(Msg.Key("err_ungueltiger_dll_name_als", p.Target));
            string declaredTarget = manifest.Get("autoload", "target", "");
            if (declaredTarget.Length > 0 && !declaredTarget.Equals(p.Target, StringComparison.OrdinalIgnoreCase))
                throw new FormatException(Msg.Key("err_dll_dateiname_und_autoload"));
            if (content.Count > 0) p.Hints.Add(Msg.Key("hint_sml_content_extra", String.Join(", ", content)));
            foreach (string id in dependencies)
            {
                if (!SafeToken(id, 160)) throw new FormatException(Msg.Key("err_ungueltige_abhaengigkeits_id", id));
                p.Dependencies.Add(new Dependency { Id = id, Constraint = manifest.Get("dependencies", id, "").Trim() });
            }
            p.RequiresLocal.AddRange(Tokens(manifest.Get("autoload", "requires_local", ""), "requires_local"));
            p.ConflictsLocal.AddRange(Tokens(manifest.Get("autoload", "conflicts_local", ""), "conflicts_local"));
            p.ConfigName = manifest.Get("configuration", "user_config", p.Target + ".ini");
            if (Path.GetFileName(p.ConfigName) != p.ConfigName || !p.ConfigName.EndsWith(".ini", StringComparison.OrdinalIgnoreCase) || !SafeToken(Path.GetFileNameWithoutExtension(p.ConfigName), 64))
                throw new FormatException(Msg.Key("err_user_config_muss_ein"));
            p.UserOverlay = manifest.Get("configuration", "user_overlay", "0") == "1";
            // local_copy = 1: the author allows the package to be copied into plugins\ as a
            // whole ("Dateien nur lokal"); [assets] dir names a folder that travels with the DLL.
            p.LocalCopyOffered = manifest.Get("configuration", "local_copy", "0") == "1";
            // 0.5.3: replaced_by_sml = resources | deposits | needs | buildings. The package does
            // the same job as one of the capabilities Soviet Mod Loader has compiled in, and its
            // plugin steps aside while SML runs. RMM then shows it as paused instead of pretending
            // it is off, and says why. Anything else in the key is a manifest error.
            p.ReplacedBySml = manifest.Get("configuration", "replaced_by_sml", "").Trim().ToLowerInvariant();
            if (p.ReplacedBySml.Length > 0 && !Sml.Embedded.Contains(p.ReplacedBySml))
                throw new FormatException(Msg.Key("err_unbekannte_sml_faehigkeit", p.ReplacedBySml, String.Join(", ", Sml.Embedded.OrderBy(x => x, StringComparer.Ordinal))));
            LoadAssets(p, manifest.Get("assets", "dir", ""));
            FormatException defaultsProblem = null;
            string declaredDefaults = manifest.Get("configuration", "defaults", "");
            p.DefaultsPath = declaredDefaults.Length > 0 ? SafeFiles.Child(p.Root, declaredDefaults) : Path.Combine(Path.GetDirectoryName(p.DllPath), p.ConfigName);
            if (!String.Equals(Path.Combine(Path.GetDirectoryName(p.DllPath), p.ConfigName), p.DefaultsPath, StringComparison.OrdinalIgnoreCase))
                throw new FormatException(Msg.Key("err_fuer_sml_muss_die"));
            p.Dll = SafeFiles.Read(p.DllPath, 64 * 1024 * 1024); SafeFiles.RequireX64Dll(p.Dll);
            p.InputHashes.Add(p.DllPath, SafeFiles.Hash(p.Dll));
            if (File.Exists(p.DefaultsPath))
            {
                p.DefaultsBytes = SafeFiles.Read(p.DefaultsPath, 1024 * 1024);
                p.DefaultsHash = SafeFiles.Hash(p.DefaultsBytes);
                p.InputHashes.Add(p.DefaultsPath, p.DefaultsHash);
                // An editor-managed package may repeat keys in its INI (vanilla_buildings:
                // target = ..., add = ...); its editor reads the file loosely, so the strict
                // parse only decides for packages without an editor schema.
                try { p.Defaults = new Ini(SafeFiles.Decode(p.DefaultsBytes)); }
                catch (FormatException problem) { p.Defaults = new Ini(""); defaultsProblem = problem; }
            }
            else
            {
                if (declaredDefaults.Length > 0) throw new FormatException(Msg.Key("err_deklarierte_standard_ini_fehlt", declaredDefaults));
                p.HasConfig = false; p.Defaults = new Ini(""); p.InputHashes.Add(p.DefaultsPath, "absent");
                p.Hints.Add(Msg.Key("hint_no_ini"));
            }
            string declaredSchema = manifest.Get("configuration", "launcher_schema", "");
            string conventionalSchema = Path.Combine(p.Root, "config", p.Target + ".launcher.ini");
            if (declaredSchema.Length > 0) p.SchemaPath = SafeFiles.Child(p.Root, declaredSchema);
            else if (File.Exists(conventionalSchema)) p.SchemaPath = conventionalSchema;
            if (p.SchemaPath != null)
            {
                try { if (new Ini(SafeFiles.Decode(SafeFiles.Read(p.SchemaPath, 1024 * 1024))).Get("launcher", "editor_type", "").Length > 0) { p.EditorSchema = p.SchemaPath; p.EditorManaged = true; p.SchemaPath = null; } }
                catch (FormatException) { }
                if (p.EditorManaged && !p.HasConfig) throw new FormatException(Msg.Key("err_ein_editor_schema_braucht"));
            }
            if (defaultsProblem != null && !p.EditorManaged) throw defaultsProblem;
            Ini schema;
            if (p.SchemaPath != null)
            {
                byte[] schemaBytes = SafeFiles.Read(p.SchemaPath, 1024 * 1024);
                p.InputHashes.Add(p.SchemaPath, SafeFiles.Hash(schemaBytes));
                schema = new Ini(SafeFiles.Decode(schemaBytes));
                if (schema.Get("launcher", "id") != p.Id || schema.Get("launcher", "config") != p.ConfigName) throw new FormatException(Msg.Key("err_schema_passt_nicht_zu"));
            }
            else
            {
                p.Generic = true; schema = GenericSchema.Build(p);
                if (p.HasConfig && !p.EditorManaged) p.Hints.Add(Msg.Key("hint_no_schema_comments"));
            }
            if (p.UserOverlay) p.Hints.Add(Msg.Key("hint_overlay", p.ConfigName));
            p.ApplySchema(schema);
            return p;
        }
        // Shared by Workshop packages and installed plugins: turns the schema into
        // fields and collections and validates the defaults against them.
        public void ApplySchema(Ini schema)
        {
            var p = this;
            p.Schema = schema; p.Visible = schema.Get("launcher", "visible", "0") == "1";
            if (!Int32.TryParse(schema.Get("launcher", "maximum_value_length", "4096"), out p.MaximumValueLength) || p.MaximumValueLength < 1 || p.MaximumValueLength > 65535)
                throw new FormatException(Msg.Key("err_ungueltige_maximum_value_length"));
            var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string section in schema.Sections.Where(s => s.StartsWith("field:", StringComparison.OrdinalIgnoreCase)))
            {
                var f = new Field { Section = schema.Required(section, "section"), Key = schema.Required(section, "key"),
                    Label = schema.Required(section, "label"), Description = schema.Get(section, "description", ""), Type = schema.Required(section, "type") };
                if (!new[] { "boolean", "integer", "decimal", "choice", "readonly", "text" }.Contains(f.Type)) throw new FormatException(Msg.Key("err_nicht_unterstuetzter_feldtyp", f.Type));
                if (!used.Add(f.Id) || !p.Defaults.Values.ContainsKey(f.Id)) throw new FormatException(Msg.Key("err_schemafeld_doppelt_oder_ohne", f.Id));
                f.Lenient = p.Generic || p.LenientValues;
                if (f.Type == "decimal" || f.Type == "integer")
                {
                    f.Minimum = Decimal.Parse(schema.Required(section, "minimum"), CultureInfo.InvariantCulture);
                    f.Maximum = Decimal.Parse(schema.Required(section, "maximum"), CultureInfo.InvariantCulture);
                    if (f.Minimum > f.Maximum) throw new FormatException(Msg.Key("err_ungueltiger_zahlenbereich", f.Id));
                }
                f.Choices = schema.Get(section, "choices", "").Split('|');
                f.DefaultValue = p.Defaults.Values[f.Id]; f.HasPackageDefault = true;
                f.Normalize(f.DefaultValue); p.Fields.Add(f);
            }
            p.EnabledField = schema.Get("launcher", "enabled_field", "");
            if (p.EnabledField.Length > 0 && !p.Fields.Any(f => f.Id == p.EnabledField && f.Type == "boolean"))
                throw new FormatException(Msg.Key("err_enabled_field_muss_auf"));
            foreach (string s in schema.Sections.Where(x => x.StartsWith("collection:", StringComparison.OrdinalIgnoreCase)))
            {
                decimal minimum, maximum, step; int maximumItems;
                if (!Decimal.TryParse(schema.Get(s,"minimum","0"),NumberStyles.Float,CultureInfo.InvariantCulture,out minimum) ||
                    !Decimal.TryParse(schema.Get(s,"maximum","1000000"),NumberStyles.Float,CultureInfo.InvariantCulture,out maximum) || minimum>maximum ||
                    !Decimal.TryParse(schema.Get(s,"step","0.001"),NumberStyles.Float,CultureInfo.InvariantCulture,out step) || step<=0 ||
                    !Int32.TryParse(schema.Get(s,"maximum_items","32"),NumberStyles.None,CultureInfo.InvariantCulture,out maximumItems) || maximumItems<1 || maximumItems>256)
                    throw new FormatException(Msg.Key("err_ungueltige_sammlungsgrenzen", s));
                string source=schema.Required(s,"source"); Match sourceMatch=Regex.Match(source,"^local-plugin:([A-Za-z0-9][A-Za-z0-9._-]{0,63})/([A-Za-z0-9][A-Za-z0-9._-]{0,63})$");
                if(!sourceMatch.Success)throw new FormatException(Msg.Key("err_nicht_unterstuetzte_sammlungsquelle", source));
                string[] targets=Tokens(schema.Required(s,"target_sections"),s+" target_sections");
                if(targets.Length==0)throw new FormatException(Msg.Key("err_sammlung_ohne_target_sections", s));
                var collection=new CollectionSpec {
                    Id=s.Substring(11),Source=source,SourcePlugin=sourceMatch.Groups[1].Value,SourceSection=sourceMatch.Groups[2].Value,
                    SourceReadySection=schema.Get(s,"source_ready_section",""),SourceReadyKey=schema.Get(s,"source_ready_key",""),SourceReadyValue=schema.Get(s,"source_ready_value",""),
                    ResourceGroup=schema.Required(s,"resource_group"),MatrixGroup=schema.Required(s,"matrix_group"),CountSection=schema.Get(s,"count_section","resources"),CountKey=schema.Get(s,"count_key","count"),ItemPrefix=schema.Get(s,"item_prefix","resource"),
                    TargetSections=targets,TargetLabels=List(schema,s,"target_labels",targets.Length,""),TargetLabelKeys=List(schema,s,"target_label_keys",targets.Length,""),TargetIcons=List(schema,s,"target_icons",targets.Length,""),
                    Type=schema.Get(s,"type","decimal"),DefaultValue=schema.Get(s,"default","0"),Unit=schema.Get(s,"unit",""),UnitKey=schema.Get(s,"unit_key",""),
                    ItemLabel=schema.Get(s,"item_label","Item"),ItemLabelKey=schema.Get(s,"item_label_key",""),ItemDescriptionKey=schema.Get(s,"item_description_key",""),CoefficientDescriptionKey=schema.Get(s,"coefficient_description_key",""),
                    EmptyNotice=schema.Get(s,"empty_notice",""),EmptyNoticeKey=schema.Get(s,"empty_notice_key",""),EmptyNoticeStyle=schema.Get(s,"empty_notice_style","warning"),
                    Minimum=minimum,Maximum=maximum,Step=step,MaximumItems=maximumItems,UserOwned=schema.Get(s,"ownership","package")=="user",AllowRemoveDefaults=schema.Get(s,"allow_remove_defaults","0")=="1",RequirePositiveWhenEnabled=schema.Get(s,"require_positive_when_enabled","0")=="1" };
                if(!SafeToken(collection.Id,64) || collection.Type!="decimal" && collection.Type!="integer" || !p.Fields.Any(f=>f.Id==collection.CountId && f.Type=="readonly"))
                    throw new FormatException(Msg.Key("err_unvollstaendige_oder_ungueltige_sammlung", s));
                if(collection.EmptyNoticeStyle!="info"&&collection.EmptyNoticeStyle!="warning")throw new FormatException(Msg.Key("err_empty_notice_style_muss", s));
                bool noReady=collection.SourceReadySection.Length==0&&collection.SourceReadyKey.Length==0&&collection.SourceReadyValue.Length==0;
                bool fullReady=SafeToken(collection.SourceReadySection,64)&&SafeToken(collection.SourceReadyKey,64)&&collection.SourceReadyValue.Length>0;
                if(!noReady&&!fullReady)throw new FormatException(Msg.Key("err_source_ready_section_source", s));
                if(!p.RequiresLocal.Contains(collection.SourcePlugin,StringComparer.OrdinalIgnoreCase))throw new FormatException(Msg.Key("err_sammlungsquelle_muss_auch_in", collection.SourcePlugin));
                new Field {Type=collection.Type,Minimum=minimum,Maximum=maximum,Label=collection.Id}.Normalize(collection.DefaultValue);
                p.Collections.Add(collection);
            }
            ConfigRules.Validate(p,p.Defaults);
        }
        public void AssertUnchanged()
        {
            foreach (var pair in InputHashes) { SafeFiles.NoLinks(pair.Key); if (SafeFiles.HashFile(pair.Key) != pair.Value) throw new IOException(Msg.Key("err_paket_wurde_inzwischen_aktualisiert", pair.Key)); }
        }
    }

    public static class CollectionRules
    {
        public static List<string> Names(CollectionSpec collection, Ini ini)
        {
            int count;if(!Int32.TryParse(ini.Required(collection.CountSection,collection.CountKey),NumberStyles.None,CultureInfo.InvariantCulture,out count)||count<0||count>collection.MaximumItems)
                throw new FormatException(Msg.Key("err_ganzzahl_0_erforderlich", collection.CountSection, collection.CountKey, collection.MaximumItems));
            var result=new List<string>();for(int i=0;i<count;i++)result.Add(ini.Required(collection.CountSection,collection.ItemPrefix+i));return result;
        }
        public static bool SafeItem(string name)
        {return !String.IsNullOrWhiteSpace(name)&&name.Length<64&&!name.Any(c=>c<33||c>126||"=;#[]:/\\\"'".Contains(c));}
        // An item id of an extra section group (0.4.29): "<prefix>:<name>" with both parts safe.
        public static bool SafeItemOrPrefixed(string name)
        {if(SafeItem(name))return true;if(name==null)return false;int colon=name.IndexOf(':');return colon>0&&colon==name.LastIndexOf(':')&&SafeItem(name.Substring(0,colon))&&SafeItem(name.Substring(colon+1));}
        // A typed name as a section or item id (0.4.25): trimmed, lower case, spaces and
        // runs of other separators become one underscore, umlauts are transliterated,
        // everything else outside letters, digits, '_', '-' and '.' is dropped. What comes
        // out either passes SafeItem or is empty.
        public static string IdFromName(string raw)
        {
            if(raw==null)return "";
            string text=raw.Trim().ToLowerInvariant().Replace("ä","ae").Replace("ö","oe").Replace("ü","ue").Replace("ß","ss");
            var result=new System.Text.StringBuilder();bool separator=false;
            foreach(char c in text)
            {
                bool keep=(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';
                if(keep){if(separator&&result.Length>0)result.Append('_');separator=false;result.Append(c);}
                else if(c==' '||c=='\t'||c=='/'||c=='\\'||c==':')separator=true;
            }
            return result.ToString();
        }
        public static Ini Add(Package package,CollectionSpec collection,Ini current,string name,IDictionary<string,string> coefficients)
        {
            ConfigRules.Validate(package,current);if(!SafeItem(name))throw new FormatException(Msg.Key("err_ungueltige_eintrags_id", name));
            var names=Names(collection,current);if(names.Any(x=>x.Equals(name,StringComparison.OrdinalIgnoreCase)))throw new FormatException(Msg.Key("err_eintrag_wird_bereits_verwendet", name));
            if(names.Count>=collection.MaximumItems)throw new FormatException(Msg.Key("err_sammlung_erlaubt_hoechstens_eintraege", collection.MaximumItems));
            var changes=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase){{collection.CountId,(names.Count+1).ToString(CultureInfo.InvariantCulture)},{Ini.Id(collection.CountSection,collection.ItemPrefix+names.Count),name}};
            var validator=new Field {Type=collection.Type,Minimum=collection.Minimum,Maximum=collection.Maximum,Label=name};
            foreach(string target in collection.TargetSections){string value;if(!coefficients.TryGetValue(target,out value))throw new FormatException(Msg.Key("err_standardwert_fehlt", target, name));changes[Ini.Id(target,name)]=validator.Normalize(value);}
            var added=new Ini(current.Render(changes));ConfigRules.Validate(package,added);return added;
        }
        public static bool CanRemove(Package package,CollectionSpec collection,string name)
        {
            return collection.AllowRemoveDefaults||!Names(collection,package.Defaults).Any(x=>x.Equals(name,StringComparison.OrdinalIgnoreCase));
        }
        public static Ini Remove(Package package,CollectionSpec collection,Ini current,string name)
        {
            ConfigRules.Validate(package,current);if(!CanRemove(package,collection,name))throw new FormatException(Msg.Key("err_paketstandard_kann_nicht_entfernt", name));
            var names=Names(collection,current);int at=names.FindIndex(x=>x.Equals(name,StringComparison.OrdinalIgnoreCase));if(at<0)throw new FormatException(Msg.Key("err_eintrag_wird_nicht_verwendet", name));names.RemoveAt(at);
            var values=new Dictionary<string,string>(current.Values,StringComparer.OrdinalIgnoreCase);
            for(int i=0;i<=names.Count;i++)values.Remove(Ini.Id(collection.CountSection,collection.ItemPrefix+i));
            values[collection.CountId]=names.Count.ToString(CultureInfo.InvariantCulture);for(int i=0;i<names.Count;i++)values[Ini.Id(collection.CountSection,collection.ItemPrefix+i)]=names[i];
            foreach(string target in collection.TargetSections)values.Remove(Ini.Id(target,name));
            if(collection.RequirePositiveWhenEnabled&&package.EnabledField.Length>0)
            {
                bool active=names.Any(item=>collection.TargetSections.Any(target=>{decimal number;return Decimal.TryParse(values.ContainsKey(Ini.Id(target,item))?values[Ini.Id(target,item)]:collection.DefaultValue,NumberStyles.Float,CultureInfo.InvariantCulture,out number)&&number>0;}));
                if(!active)values[package.EnabledField]="0";
            }
            var removed=new Ini(current.RewriteValues(values));ConfigRules.Validate(package,removed);return removed;
        }
        public static Dictionary<string,string> State(CollectionSpec collection,Ini ini)
        {
            var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);var names=Names(collection,ini);result[collection.CountId]=names.Count.ToString(CultureInfo.InvariantCulture);
            for(int i=0;i<names.Count;i++){string name=names[i];result[Ini.Id(collection.CountSection,collection.ItemPrefix+i)]=name;foreach(string target in collection.TargetSections){string value=ini.Get(target,name);if(value!=null)result[Ini.Id(target,name)]=value;}}
            return result;
        }
    }

    public sealed class RuleException : FormatException
    {
        public readonly string TranslationKey;
        public readonly object[] TranslationArguments;
        public RuleException(string translationKey, params object[] translationArguments)
            : base(translationKey)
        {
            TranslationKey=translationKey;
            TranslationArguments=translationArguments??new object[0];
        }
    }

    // A plugin that exists only as build\plugins\<name>.dll (+ .ini), the way a
    // plain TesmioLoader user installs it, without any Workshop package. There
    // is no shipped original to fall back to, so the INI found in plugins\ is
    // declared the original, copied once to user_config\.autoload\<name>.upstream.ini
    // and never edited in place without that copy existing ("protected base").
    // An INI changed outside this application (a new plugin version, a manual
    // edit) is recognised by the receipt and becomes the new original; personal
    // values from user_config are laid over it again.
    // The loader's log files as data: which ones exist, what a line is about,
    // and whether it reports a problem. Used by the log window and the notices.
    public static class GameLogs
    {
        public sealed class Source { public string Name, Path; }
        public enum Kind { Plain, Warning, Problem }
        static readonly Regex Problem = new Regex(@"\b(error|fatal|fail(ed|ure)?|refused?|mismatch|crash(ed)?|not initialised|exception)\b", RegexOptions.IgnoreCase);
        static readonly Regex Warning = new Regex(@"\b(warn(ing)?|declined|skipped|missing|unknown|unsupported|idle)\b", RegexOptions.IgnoreCase);
        // Plugin summaries such as "0 warning(s), 0 error(s), 0 fatal error(s)" are good news.
        static readonly Regex Clean = new Regex(@"\b0 warning\(s\), 0 error\(s\), 0 fatal", RegexOptions.IgnoreCase);
        // 0.4.78: the level a plugin prints after its name ("weather_roads  INFO  Init ...",
        // "research_expansion  WARN: ...") decides first; an INFO line is raised only by a
        // counter above zero ("12 warning(s)", "errors=2"), never by the word "error" in "errors=0".
        // The subject before the level is optional: a plugin's own detail log starts its lines
        // with the stamp and the level ("[2026-09-12 04:53:48.614] INFO  Init [ready] ...").
        static readonly Regex Level = new Regex(@"^\[[^\]]*\]\s*(?:\S+\s+)?(INFO|WARN(?:ING)?|ERROR|FATAL|DEBUG|TRACE)\b:?", RegexOptions.IgnoreCase);
        static readonly Regex ErrorCount = new Regex(@"\b[1-9]\d*\s+(?:fatal )?error\(s\)|\b(?:errors|fatal)=[1-9]\d*", RegexOptions.IgnoreCase);
        static readonly Regex WarningCount = new Regex(@"\b[1-9]\d*\s+warning\(s\)|\bwarnings=[1-9]\d*", RegexOptions.IgnoreCase);
        // 0.4.83: loader lines carry no level ("buildings_plus  2 generated, ... 0 failed ... 0 error(s)").
        // A word that a number introduces is a counter, not a complaint: a counter above zero decides,
        // a counter at zero is struck out before the keywords are looked at.
        static readonly Regex ZeroCount = new Regex(@"\b0\s+(?:fatal\s+)?[a-z_]+(?:\(s\))?|\b[a-z_]+=0\b", RegexOptions.IgnoreCase);
        static readonly Regex Stamp = new Regex(@"^\[[^\]]*\]\s*");
        public static List<Source> Sources(string build)
        {
            var result = new List<Source>();
            try
            {
                string root = Path.GetFullPath(build);
                string main = Path.Combine(root, "tesmioloader.log");
                if (File.Exists(main)) result.Add(new Source { Name = "tesmioloader.log", Path = main });
                foreach (string file in Directory.GetFiles(root, "tesmioloader.*.log").OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
                    result.Add(new Source { Name = Path.GetFileName(file), Path = file });
                // Plugin detail logs live in logs\ next to the loader's log since the logs folder round; older
                // plugins still write next to tesmioloader.log, so both places are listed.
                string logs = Path.Combine(root, "logs");
                if (Directory.Exists(logs))
                    foreach (string file in Directory.GetFiles(logs, "tesmioloader.*.log").OrderBy(f => f, StringComparer.OrdinalIgnoreCase))
                        result.Add(new Source { Name = "logs\\" + Path.GetFileName(file), Path = file });
            }
            catch (Exception) { }
            return result;
        }
        // Shared read: the game keeps its logs open while it runs.
        public static string Read(string path)
        {
            try
            {
                using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var reader = new StreamReader(stream, new UTF8Encoding(false, false))) return reader.ReadToEnd();
            }
            catch (Exception e) { return "(" + e.Message + ")"; }
        }
        public static Kind Classify(string line)
        {
            if (line == null) return Kind.Plain;
            if (Clean.IsMatch(line)) return Kind.Plain;
            if (line.IndexOf("hook ok", StringComparison.OrdinalIgnoreCase) >= 0) return Kind.Plain;
            Match level = Level.Match(line);
            if (level.Success)
            {
                string kind = level.Groups[1].Value.ToUpperInvariant();
                if (kind == "ERROR" || kind == "FATAL") return Kind.Problem;
                if (kind.StartsWith("WARN")) return Kind.Warning;
                if (ErrorCount.IsMatch(line)) return Kind.Problem;
                if (WarningCount.IsMatch(line)) return Kind.Warning;
                return Kind.Plain;
            }
            // The game's own lines carry the level in the subject: "game.WARN 6", "game.ERROR setfocus".
            string subject = SubjectOf(line);
            if (subject.EndsWith(".ERROR", StringComparison.OrdinalIgnoreCase) || subject.EndsWith(".FATAL", StringComparison.OrdinalIgnoreCase)) return Kind.Problem;
            if (subject.EndsWith(".WARN", StringComparison.OrdinalIgnoreCase) || subject.EndsWith(".WARNING", StringComparison.OrdinalIgnoreCase)) return Kind.Warning;
            if (ErrorCount.IsMatch(line)) return Kind.Problem;
            if (WarningCount.IsMatch(line)) return Kind.Warning;
            string residue = ZeroCount.Replace(line, " ");
            if (Problem.IsMatch(residue)) return Kind.Problem;
            if (Warning.IsMatch(residue)) return Kind.Warning;
            return Kind.Plain;
        }
        // The first word after the time stamp: "plugin", "bridge", "hook", a plugin's own name, "game.ERROR".
        public static string SubjectOf(string line)
        {
            if (line == null) return "";
            string rest = Stamp.Replace(line, "");
            int end = rest.IndexOfAny(new[] { ' ', '\t' });
            return end < 0 ? rest : rest.Substring(0, end);
        }
        public static List<string> Subjects(IEnumerable<string> lines)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase); var result = new List<string>();
            foreach (string line in lines) { string s = SubjectOf(line); if (s.Length > 0 && s.Length <= 40 && s != "---" && seen.Add(s)) result.Add(s); }
            result.Sort(StringComparer.OrdinalIgnoreCase); return result;
        }
        // What the loader said at the end of the last run, or its plugin count.
        public static string LastRun(IEnumerable<string> lines)
        {
            string loaded = "", shutdown = "";
            foreach (string line in lines)
            {
                if (line.IndexOf("--- shutdown", StringComparison.Ordinal) >= 0) shutdown = Stamp.Replace(line, "").Trim('-', ' ');
                else if (Regex.IsMatch(line, @"plugin\s+\d+ loaded")) { loaded = Stamp.Replace(line, ""); shutdown = ""; }
            }
            return shutdown.Length > 0 ? shutdown : loaded;
        }
    }

    public static class InstalledPlugins
    {
        static readonly Regex LogLine = new Regex(@"^\[[^\]]*\]\s+plugin\s+(\S+)\s+(\S+)\s+from\s+(\S+)\.dll", RegexOptions.Multiline);
        // The bridge logs "bridge   hook <name> <version> from <folder>\hooks\<file>.dll". The name is
        // the plugin's display name and may carry spaces ("UI Layout Fixes 0.3.0 from ..."), so it is
        // everything up to the version token right before "from" (0.4.46); the key is the DLL name.
        static readonly Regex BridgeLine = new Regex(@"^\[[^\]]*\]\s+bridge\s+hook\s+(.+?)\s+(\S+)\s+from\s+\S*?([^\\/\s]+)\.dll", RegexOptions.Multiline);
        public static string UpstreamPath(string build, string name) { return SafeFiles.Child(Path.GetFullPath(build), "user_config\\.autoload\\" + name + ".upstream.ini"); }
        // Versions as the loader reported them on its last start ("plugin <name> <version> from <file>.dll"),
        // including hooks the Workshop Bridge loaded.
        public static Dictionary<string, string> LoaderVersions(string build)
        {
            var versions = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string log = Path.Combine(Path.GetFullPath(build), "tesmioloader.log");
                if (!File.Exists(log)) return versions;
                string text = GameLogs.Read(log);
                foreach (Match m in LogLine.Matches(text)) versions[m.Groups[3].Value] = m.Groups[2].Value;
                foreach (Match m in BridgeLine.Matches(text)) versions[m.Groups[3].Value] = m.Groups[2].Value;
            }
            catch (Exception) { }
            return versions;
        }
        // 0.5.17: plugins that stepped aside on the last start ("<name>  idle - ..."): the forks and
        // Deposits Plus do that while Soviet Mod Loader hosts their capability. The loader prints no
        // "plugin <name> <version>" line for them, so without this they would read as "not loaded".
        static readonly Regex IdleLine = new Regex(@"^\[[^\]]*\]\s+(\S+)\s+idle - ", RegexOptions.Multiline);
        public static HashSet<string> IdleAtLastStart(string build)
        {
            var idle = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string log = Path.Combine(Path.GetFullPath(build), "tesmioloader.log");
                if (!File.Exists(log)) return idle;
                foreach (Match m in IdleLine.Matches(GameLogs.Read(log))) idle.Add(m.Groups[1].Value);
            }
            catch (Exception) { }
            return idle;
        }
        // When the loader last wrote its log, or null.
        public static DateTime? LogTime(string build)
        {
            try { string log = Path.Combine(Path.GetFullPath(build), "tesmioloader.log"); return File.Exists(log) ? File.GetLastWriteTime(log) : (DateTime?)null; }
            catch (Exception) { return null; }
        }
        public static Package Load(string build, string name, string schemaRoot)
        {
            build = Path.GetFullPath(build);
            if (!Regex.IsMatch(name ?? "", "^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")) throw new FormatException(Msg.Key("err_ungueltiger_plugin_name", name));
            var p = new Package { Installed = true, Kind = "plugin", Target = name, ConfigName = name + ".ini" };
            p.DllPath = SafeFiles.Child(build, "plugins\\" + name + ".dll");
            if (!File.Exists(p.DllPath)) throw new FileNotFoundException(Msg.Key("err_installierte_plugin_dll_fehlt", p.DllPath));
            p.Dll = SafeFiles.Read(p.DllPath, 64 * 1024 * 1024); SafeFiles.RequireX64Dll(p.Dll);
            p.InputHashes.Add(p.DllPath, SafeFiles.Hash(p.Dll));
            string local = SafeFiles.Child(build, "plugins\\" + p.ConfigName), upstream = UpstreamPath(build, name);
            string receipt = SafeFiles.Child(build, "user_config\\.autoload\\" + name + ".receipt.ini");
            p.UpstreamPath = upstream; p.Root = Path.GetDirectoryName(p.DllPath);
            bool managed = false;
            if (File.Exists(receipt))
            {
                try { var r = new Ini(SafeFiles.Text(receipt)); managed = r.Get("state", "mode", "") == "installed" && r.Get("state", "ini_hash", "") == SafeFiles.HashFile(local); }
                catch (FormatException) { managed = false; }
            }
            if (managed && File.Exists(upstream)) p.DefaultsPath = upstream;
            else if (File.Exists(local)) { p.DefaultsPath = local; p.RefreshUpstream = true; }
            else { p.DefaultsPath = local; p.HasConfig = false; }
            if (p.HasConfig)
            {
                p.DefaultsBytes = SafeFiles.Read(p.DefaultsPath, 1024 * 1024);
                p.Defaults = new Ini(SafeFiles.Decode(p.DefaultsBytes)); p.DefaultsHash = SafeFiles.Hash(p.DefaultsBytes);
            }
            else { p.Defaults = new Ini(""); p.Hints.Add(Msg.Key("hint_no_ini_installed")); }
            // Schema tiers: settings_schemas\<name>.launcher.ini, otherwise the INI itself.
            Ini schema = null;
            string schemaPath = String.IsNullOrWhiteSpace(schemaRoot) ? "" : Path.Combine(Path.GetFullPath(schemaRoot), name + ".launcher.ini");
            if (schemaPath.Length > 0 && File.Exists(schemaPath))
            {
                SafeFiles.NoLinks(schemaPath);
                byte[] bytes = SafeFiles.Read(schemaPath, 1024 * 1024); var candidate = new Ini(SafeFiles.Decode(bytes));
                if (candidate.Get("launcher", "editor_type", "").Length == 0)     // keyed editors belong to LocalEditorSpec
                {
                    if (candidate.Get("launcher", "config", "") != p.ConfigName) throw new FormatException(Msg.Key("err_lokales_schema_passt_nicht", schemaPath));
                    p.SchemaPath = schemaPath; p.InputHashes.Add(schemaPath, SafeFiles.Hash(bytes)); schema = candidate;
                    p.Root = Path.GetDirectoryName(schemaPath);                  // language_directory resolves beside the schema
                    p.LenientValues = true;
                }
            }
            p.Id = schema != null ? schema.Get("launcher", "id", "local." + name) : "local." + name;
            p.Name = schema != null ? schema.Get("launcher", "name", name) : name;
            string version; p.Version = LoaderVersions(build).TryGetValue(name, out version) ? version : "installiert";
            if (schema == null) { p.Generic = true; schema = GenericSchema.Build(p); }
            if (p.HasConfig)
            {
                if (p.RefreshUpstream && File.Exists(upstream)) p.Hints.Add(Msg.Key("hint_installed_ini_changed"));
                else if (p.RefreshUpstream) p.Hints.Add(Msg.Key("hint_installed_first_save", name));   // 0.4.54: the routine "original kept" line is gone, the footer button says it
                if (p.Generic) p.Hints.Add(Msg.Key("hint_no_local_schema"));
            }
            p.ApplySchema(schema);
            if (p.LenientValues)
            {
                // Keys the INI has and the local schema does not: not editable here, but named.
                var known = new HashSet<string>(p.Fields.Select(f => f.Id), StringComparer.OrdinalIgnoreCase);
                var missing = p.Defaults.Values.Keys.Where(k => !known.Contains(k)).ToList();
                if (missing.Count > 0) p.Hints.Add(Msg.Key("hint_not_in_schema", String.Join(", ", missing)));
            }
            return p;
        }
    }

    public static class ConfigRules
    {
        public static void Validate(Package package,Ini ini)
        {
            var allowed=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var sections=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(Field field in package.Fields)
            {
                // A generic schema was derived from the package INI; a local INI may
                // legitimately lack a key the package added later, or carry keys the
                // package never declared. Only a shipped schema is authoritative.
                string value=ini.Get(field.Section,field.Key);
                if(value==null){if(package.Generic)continue;throw new FormatException(Msg.Key("err_fehlt", field.Section, field.Key));}
                field.Normalize(value);allowed.Add(field.Id);sections.Add(field.Section);
            }
            foreach(CollectionSpec collection in package.Collections)
            {
                bool anyPositive=false;
                sections.Add(collection.CountSection);foreach(string target in collection.TargetSections)sections.Add(target);
                var names=CollectionRules.Names(collection,ini);var unique=new HashSet<string>(StringComparer.OrdinalIgnoreCase);allowed.Add(collection.CountId);
                var validator=new Field {Type=collection.Type,Minimum=collection.Minimum,Maximum=collection.Maximum,Label=collection.Id};
                for(int i=0;i<names.Count;i++)
                {
                    string name=names[i];if(!CollectionRules.SafeItem(name)||!unique.Add(name))throw new FormatException(Msg.Key("err_ungueltiger_oder_doppelter_sammlungseintrag", name));
                    allowed.Add(Ini.Id(collection.CountSection,collection.ItemPrefix+i));
                    foreach(string target in collection.TargetSections){string id=Ini.Id(target,name);allowed.Add(id);string raw=ini.Get(target,name,collection.DefaultValue);decimal n=Decimal.Parse(validator.Normalize(raw),CultureInfo.InvariantCulture);if(n>0)anyPositive=true;}
                }
                if(collection.RequirePositiveWhenEnabled&&package.EnabledField.Length>0&&ini.Values[package.EnabledField]=="1"&&!anyPositive)
                    throw new RuleException("error_positive_collection_required",collection.Id);
            }
            if(package.Generic||package.LenientValues)
            {
                foreach(var pair in ini.Values)if(pair.Value.Length>package.MaximumValueLength)throw new FormatException(Msg.Key("err_zu_langer_wert", pair.Key));
                return;
            }
            foreach(string section in ini.Sections)if(!sections.Contains(section))throw new FormatException(Msg.Key("err_unbekannter_abschnitt", section));
            foreach(var pair in ini.Values)if(!allowed.Contains(pair.Key)||pair.Value.Length>package.MaximumValueLength)throw new FormatException(Msg.Key("err_unbekannter_schluessel_oder_zu", pair.Key));
        }
    }

    // Soviet Mod Loader as a neighbour. When SML is installed and switched on it
    // loads hook DLLs straight out of the Workshop packages, so this application
    // must not put a second copy into plugins\ - it only provides the INI (or the
    // overlay). SML also embeds four capabilities that otherwise are separate
    // plugins, which is what requires_local aliases against.
    public static class Sml
    {
        public static readonly string[] Names = { "soviet_mod_loader", "000_soviet_mod_loader" };
        public static readonly HashSet<string> Embedded = new HashSet<string>(new[] { "resources", "deposits", "needs", "buildings" }, StringComparer.OrdinalIgnoreCase);
        public static bool Active(string build)
        {
            try
            {
                string root = Path.GetFullPath(build);
                string cfg = Path.Combine(root, "tesmioloader.ini");
                Ini settings = File.Exists(cfg) ? new Ini(SafeFiles.Text(cfg)) : new Ini("");
                if (settings.Get("tesmioloader", "plugins", "1") == "0") return false;
                foreach (string name in Names)
                    if (File.Exists(Path.Combine(root, "plugins", name + ".dll")) && settings.Get("plugins", name, "1") != "0") return true;
                return false;
            }
            catch (Exception) { return false; }
        }
        // 0.5.1: one of the four embedded capabilities, with no DLL of its own in plugins\.
        // Its plugins\<name>.ini is then generated output - the first line of the file says
        // "generated by Soviet Mod Loader" - so nothing here may open an editor on it.
        public static bool Hosts(string build, string plugin)
        {
            if (String.IsNullOrWhiteSpace(build) || String.IsNullOrWhiteSpace(plugin) || !Embedded.Contains(plugin)) return false;
            try { if (File.Exists(Path.Combine(Path.GetFullPath(build), "plugins", plugin + ".dll"))) return false; }
            catch (Exception) { return false; }
            return Active(build);
        }
        // 0.5.3: where SML keeps its own state. [loader] state_dir names the folder next to the
        // loader; "soviet_mod_loader" is its default and what every normal installation uses.
        public static string StateDir(string build)
        {
            string root = Path.GetFullPath(build), fallback = Path.Combine(root, "soviet_mod_loader");
            foreach (string name in Names)
            {
                string settings = Path.Combine(root, "plugins", name + ".ini");
                try
                {
                    if (!File.Exists(settings)) continue;
                    string value = new LooseIni(SafeFiles.Text(settings)).Get("loader", "state_dir");
                    if (String.IsNullOrWhiteSpace(value)) continue;
                    return SafeFiles.Child(root, GenericSchema.StripComment(value).Trim());
                }
                catch (Exception) { }
            }
            return fallback;
        }
        // The baseline SML merges the packages over. It snapshots it once from plugins\<name>.ini
        // and from then on only reads it - so this, not the generated file, is where personal
        // entries belong. Null means there is nothing to edit: no SML, or it has not run yet.
        // A path as it is worth showing: relative to the loader folder, so the box reads
        // "soviet_mod_loader\base\resources.ini" and not a line of Steam library.
        public static string Show(string build, string path)
        {
            try
            {
                string root = Path.GetFullPath(build).TrimEnd('\\') + "\\", full = Path.GetFullPath(path);
                return full.StartsWith(root, StringComparison.OrdinalIgnoreCase) ? full.Substring(root.Length) : full;
            }
            catch (Exception) { return path; }
        }
        // 0.5.9: SML searches ONE folder for mods - [loader] workshop_root, with "auto" meaning the
        // Steam Workshop folder of the game library. There is no second root, so a package that is
        // not from the Workshop has to live there too.
        public static string WorkshopRoot(string build)
        {
            foreach (string name in Names)
            {
                try
                {
                    string settings = Path.Combine(Path.GetFullPath(build), "plugins", name + ".ini");
                    if (!File.Exists(settings)) continue;
                    string value = new LooseIni(SafeFiles.Text(settings)).Get("loader", "workshop_root");
                    if (String.IsNullOrWhiteSpace(value)) continue;
                    value = GenericSchema.StripComment(value).Trim();
                    if (value.Length == 0 || value.Equals("auto", StringComparison.OrdinalIgnoreCase)) break;
                    return Path.GetFullPath(value);
                }
                catch (Exception) { }
            }
            try { string game = GameBuildings.GameRoot(build); return game == null ? "" : (GameBuildings.SteamWorkshopFor(game) ?? ""); }
            catch (Exception) { return ""; }
        }
        // 0.5.12: the order SML merged the resources in, from <state>\catalog.ini [resource_order]
        // (id -> index). Empty when there is no catalogue. Read only.
        public static Dictionary<string, int> CatalogOrder(string build)
        {
            var result = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string path = Path.Combine(StateDir(build), "catalog.ini");
                if (!File.Exists(path)) return result;
                foreach (var pair in new LooseIni(SafeFiles.Text(path)).Entries("resource_order"))
                {
                    int at; if (Int32.TryParse((pair.Value ?? "").Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out at)) result[pair.Key.Trim()] = at;
                }
            }
            catch (Exception) { result.Clear(); }
            return result;
        }
        // 0.5.15: the deposit type numbers SML gave the packages' deposits, from catalog.ini
        // [deposit_types] ("<mod id>::<section>" -> number). Read only.
        public static Dictionary<string, int> CatalogDepositTypes(string build)
        {
            var result = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            try
            {
                string path = Path.Combine(StateDir(build), "catalog.ini");
                if (!File.Exists(path)) return result;
                foreach (var pair in new LooseIni(SafeFiles.Text(path)).Entries("deposit_types"))
                {
                    int at; if (Int32.TryParse((pair.Value ?? "").Trim(), NumberStyles.None, CultureInfo.InvariantCulture, out at)) result[pair.Key.Trim()] = at;
                }
            }
            catch (Exception) { result.Clear(); }
            return result;
        }
        public static string Baseline(string build, string plugin, string configName)
        {
            if (!Hosts(build, plugin)) return null;
            try
            {
                string path = SafeFiles.Child(StateDir(build), "base\\" + configName);
                return File.Exists(path) ? path : null;
            }
            catch (Exception) { return null; }
        }
    }

    // workshop_bridge: the loader plugin that loads Workshop hook DLLs when SML
    // is absent. Its allowlist is [packages] <Workshop folder> = 1/0 in
    // user_config\workshop_bridge.ini, which only Republic Mod Manager writes; the
    // base INI beside the DLL carries the policy (list = only listed, all).
    public static class Bridge
    {
        public const string Name = "workshop_bridge";
        public static bool Present(string build)
        {
            try
            {
                string root = Path.GetFullPath(build);
                if (!File.Exists(Path.Combine(root, "plugins", Name + ".dll"))) return false;
                string cfg = Path.Combine(root, "tesmioloader.ini");
                Ini settings = File.Exists(cfg) ? new Ini(SafeFiles.Text(cfg)) : new Ini("");
                return settings.Get("tesmioloader", "plugins", "1") != "0" && settings.Get("plugins", Name, "1") != "0";
            }
            catch (Exception) { return false; }
        }
        // Present, switched on, and not shadowed by SML (the bridge idles under SML).
        public static bool Active(string build) { return Present(build) && !Sml.Active(build) && Enabled(build); }
        public static string OverlayPath(string build) { return SafeFiles.Child(build, "user_config\\" + Name + ".ini"); }
        static string BasePath(string build) { return SafeFiles.Child(build, "plugins\\" + Name + ".ini"); }
        // Overlay first, then base - the same rule the DLL applies.
        static string Setting(string build, string section, string key, string fallback)
        {
            foreach (string path in new[] { OverlayPath(build), BasePath(build) })
            {
                try
                {
                    if (!File.Exists(path)) continue;
                    string value = new Ini(SafeFiles.Text(path)).Get(section, key, null);
                    if (value != null) return GenericSchema.StripComment(value);
                }
                catch (FormatException) { }
            }
            return fallback;
        }
        static bool Enabled(string build) { return Setting(build, "bridge", "enabled", "1") != "0"; }
        public static string Policy(string build) { return Setting(build, "bridge", "policy", "list").ToLowerInvariant() == "all" ? "all" : "list"; }
        // The folder name is the key: the Workshop item number for a subscription.
        public static string Key(Package package) { return Path.GetFileName(package.Root.TrimEnd('\\', '/')); }
        // Whether the bridge would load this package as the files stand now.
        public static bool Listed(string build, string key)
        {
            string value = Setting(build, "packages", key, null);
            if (value != null) return value != "0";
            return Policy(build) == "all";
        }
        public static byte[] Render(string build, string key, bool enabled)
        {
            string path = OverlayPath(build);
            var change = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { { Ini.Id("packages", key), enabled ? "1" : "0" } };
            // 0.4.52: an overlay written by the old Tesmio Settings keeps its header line; rename it in passing.
            string text = File.Exists(path) ? new Ini(SafeFiles.Text(path)).Render(change).Replace("; Written by Tesmio Settings:", "; Written by Republic Mod Manager:")
                : "; Written by Republic Mod Manager: which Workshop packages workshop_bridge loads.\r\n[packages]\r\n" + key + " = " + (enabled ? "1" : "0") + "\r\n";
            return SafeFiles.Utf8.GetBytes(text);
        }
        // 0.4.53: the folder the DLL walks - the workshop_root setting, "auto" resolved the way the DLL
        // does it (the Workshop folder of the Steam library the game sits in); "" when it cannot be told.
        public static string Root(string build)
        {
            string setting = Setting(build, "bridge", "workshop_root", "auto").Trim();
            if (setting.Length == 0 || setting.Equals("auto", StringComparison.OrdinalIgnoreCase))
            { string game = GameBuildings.GameRoot(build); string steam = game == null ? null : GameBuildings.SteamWorkshopFor(game); return steam ?? ""; }
            try { return Path.GetFullPath(setting); } catch (Exception) { return setting; }
        }
        // The [packages] lines of one file: line index and key.
        static List<KeyValuePair<int, string>> PackageEntries(string[] lines)
        {
            var result = new List<KeyValuePair<int, string>>(); bool inside = false;
            for (int i = 0; i < lines.Length; i++)
            {
                string line = lines[i].Trim();
                if (line.StartsWith("[")) { inside = line.Equals("[packages]", StringComparison.OrdinalIgnoreCase); continue; }
                if (!inside || line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                int eq = line.IndexOf('='); if (eq <= 0) continue;
                result.Add(new KeyValuePair<int, string>(i, line.Substring(0, eq).Trim()));
            }
            return result;
        }
        static string[] LinesOf(string text) { return text.Replace("\r\n", "\n").Split('\n'); }
        // Every key under [packages] of overlay and base, in file order, without duplicates.
        public static List<string> PackageKeys(string build)
        {
            var keys = new List<string>();
            foreach (string path in new[] { OverlayPath(build), BasePath(build) })
            {
                if (!File.Exists(path)) continue;
                foreach (var entry in PackageEntries(LinesOf(SafeFiles.Text(path)))) if (!keys.Contains(entry.Value, StringComparer.OrdinalIgnoreCase)) keys.Add(entry.Value);
            }
            return keys;
        }
        // Drops the [packages] lines whose key `keep` rejects - in the overlay and in the base, every
        // other line untouched - through one transaction. Returns the backup note, "" when nothing changed.
        public static string Prune(string build, Func<string, bool> keep, Action guard, out List<string> removed)
        {
            removed = new List<string>();
            var writes = new Dictionary<string, byte[]>(); var before = new Dictionary<string, string>();
            foreach (string path in new[] { OverlayPath(build), BasePath(build) })
            {
                if (!File.Exists(path)) continue;
                string text = SafeFiles.Text(path); string newline = text.Contains("\r\n") ? "\r\n" : "\n";
                string[] lines = LinesOf(text); var drop = new HashSet<int>();
                foreach (var entry in PackageEntries(lines)) if (!keep(entry.Value)) { drop.Add(entry.Key); if (!removed.Contains(entry.Value, StringComparer.OrdinalIgnoreCase)) removed.Add(entry.Value); }
                if (drop.Count == 0) continue;
                before[path] = SafeFiles.HashFile(path); writes[path] = SafeFiles.Utf8.GetBytes(String.Join(newline, lines.Where((l, i) => !drop.Contains(i))));
            }
            if (writes.Count == 0) return "";
            return Transaction.Apply(build, Name, writes, before, guard);
        }
    }

    // Update detection: the receipt written at the last deployment remembers
    // the package version and the hashes of what was deployed. A package whose
    // DLL or default INI no longer matches has been updated (Steam, or the
    // author) since then; saving takes the new files over.
    public sealed class UpdateState
    {
        public bool Deployed, Pending;
        public string PreviousVersion = "", CurrentVersion = "";
        public bool DllChanged, DefaultsChanged;
    }
    public static class UpdateCheck
    {
        public static UpdateState Of(Package package, string build)
        {
            var state = new UpdateState { CurrentVersion = package.Version };
            if (package == null || package.Installed || package.Kind != "plugin") return state;
            try
            {
                string receipt = SafeFiles.Child(Path.GetFullPath(build), "user_config\\.autoload\\" + package.Target + ".receipt.ini");
                if (!File.Exists(receipt)) return state;
                var r = new Ini(SafeFiles.Text(receipt));
                if (r.Get("state", "id", "") != package.Id) return state;
                state.Deployed = true; state.PreviousVersion = r.Get("state", "version", "");
                string mode = r.Get("state", "mode", "package");
                // The DLL only counts where this program copied it; under SML or the
                // bridge the game loads the package copy and is current by itself.
                string dllHash = r.Get("state", "dll_hash", "");
                state.DllChanged = mode == "package" && dllHash.Length > 0 && dllHash != "absent" && package.Dll != null && dllHash != SafeFiles.Hash(package.Dll);
                state.DefaultsChanged = package.HasConfig && r.Get("state", "defaults_hash", "") != package.DefaultsHash;
                state.Pending = state.DllChanged || state.DefaultsChanged;
            }
            catch (Exception) { }
            return state;
        }
    }

    // One line of an SML manifest's [dependencies]: `mod.id = >=1.2.0`.
    public sealed class Dependency
    {
        public string Id, Constraint = "";
        public bool Found, VersionOk = true;
        public string Target = "", Kind = "", Version = "", Note = "", Root = "";
        public string Name = "";   // 0.4.27: display name of the matched package, for the notices
    }

    // Version constraints as SML writes them: ">=1.2.0", ">1", "=1.0", "<=2", a
    // bare "1.0" (meaning at least), "*" or nothing (anything). Dotted numbers are
    // compared part by part; a suffix such as "-beta" is ignored.
    public static class VersionRule
    {
        static int[] Parts(string version)
        {
            var m = Regex.Match(version ?? "", @"^\s*v?(\d+(?:\.\d+)*)");
            if (!m.Success) return null;
            return m.Groups[1].Value.Split('.').Select(x => { int n; return Int32.TryParse(x, out n) ? n : 0; }).ToArray();
        }
        static int Compare(int[] a, int[] b)
        {
            for (int i = 0; i < Math.Max(a.Length, b.Length); i++)
            {
                int x = i < a.Length ? a[i] : 0, y = i < b.Length ? b[i] : 0;
                if (x != y) return x < y ? -1 : 1;
            }
            return 0;
        }
        public static bool Satisfied(string constraint, string version)
        {
            string c = (constraint ?? "").Trim();
            if (c.Length == 0 || c == "*" || c == "any") return true;
            var m = Regex.Match(c, @"^(>=|<=|==|=|>|<)?\s*(.+)$");
            string op = m.Groups[1].Value, wanted = m.Groups[2].Value;
            int[] have = Parts(version), need = Parts(wanted);
            if (have == null || need == null) return false;
            int cmp = Compare(have, need);
            switch (op)
            {
                case ">": return cmp > 0;
                case "<": return cmp < 0;
                case "<=": return cmp <= 0;
                case "=": case "==": return cmp == 0;
                default: return cmp >= 0;   // ">=" and a bare version
            }
        }
    }

    // Which game build is installed. Every plugin in this ecosystem carries
    // addresses for exactly one build of SOVIET64.exe, identified by the PE
    // header's TimeDateStamp (docs/07-pitfalls.md). A Steam update changes the
    // stamp, and from then on those plugins must not be injected.
    public static class GameVersion
    {
        public static bool CheckEnabled = true;
        static readonly Dictionary<uint, string> supported = new Dictionary<uint, string> { { 0x6A3EB6ADu, "1.1.1.9" } };
        static readonly Dictionary<uint, string> retired = new Dictionary<uint, string> { { 0x69C4098Cu, "1.1.1.7" } };
        // settings_schemas\game_versions.ini: [supported] 6A3EB6AD = 1.1.1.9
        public static void LoadTable(string schemaRoot)
        {
            try
            {
                string path = Path.Combine(schemaRoot ?? "", "game_versions.ini");
                if (!File.Exists(path)) return;
                var ini = new Ini(SafeFiles.Text(path));
                foreach (var pair in ini.Values)
                {
                    if (!pair.Key.StartsWith("supported/")) continue;
                    uint stamp;
                    if (UInt32.TryParse(pair.Key.Substring(10), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out stamp)) supported[stamp] = pair.Value;
                }
            }
            catch (Exception) { }
        }
        public static string ExePath(string build)
        {
            string root = Path.GetFullPath(build);
            try
            {
                string cfg = Path.Combine(root, "tesmioloader.ini");
                if (File.Exists(cfg)) { string exe = new Ini(SafeFiles.Text(cfg)).Get("tesmioloader", "game_exe", ""); if (exe.Length > 0 && File.Exists(exe)) return Path.GetFullPath(exe); }
            }
            catch (Exception) { }
            return Path.GetFullPath(Path.Combine(root, "..\\..\\SOVIET64.exe"));
        }
        // 0 when the file is not a PE image.
        public static uint Stamp(string exe)
        {
            try
            {
                using (var file = new FileStream(exe, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                {
                    var head = new byte[64];
                    if (file.Read(head, 0, 64) != 64 || head[0] != 'M' || head[1] != 'Z') return 0;
                    int pe = BitConverter.ToInt32(head, 60);
                    if (pe < 64 || pe > file.Length - 12) return 0;
                    file.Seek(pe, SeekOrigin.Begin);
                    var sig = new byte[12];
                    if (file.Read(sig, 0, 12) != 12 || BitConverter.ToUInt32(sig, 0) != 0x4550) return 0;
                    return BitConverter.ToUInt32(sig, 8);
                }
            }
            catch (Exception) { return 0; }
        }
        // True only when the game executable is there and its build is not one the
        // plugins were made for. A missing executable is the launcher's problem.
        public static bool Warn(string build, out string text)
        {
            bool ok; text = Describe(build, out ok);
            return !ok && File.Exists(ExePath(build));
        }
        public static string Describe(string build, out bool ok)
        {
            ok = false;
            string exe = ExePath(build);
            if (!File.Exists(exe)) return Msg.Key("game_exe_missing", exe);
            uint stamp = Stamp(exe);
            if (stamp == 0) return Msg.Key("game_exe_unreadable");
            string hex = "0x" + stamp.ToString("X8");
            string name;
            if (supported.TryGetValue(stamp, out name)) { ok = true; return Msg.Key("game_version_supported", name, hex); }
            if (retired.TryGetValue(stamp, out name)) return Msg.Key("game_version_retired", name, hex);
            return Msg.Key("game_version_unknown_stamp", hex);
        }
    }

    public static class RuntimeStatus
    {
        // 0.5.3: the package declares that SML replaces it, and SML really hosts that capability.
        // The plugin then steps aside at startup - not off, not broken, just not its turn.
        public static bool BlockedBySml(CatalogEntry entry, string build)
        {
            if (entry == null || !entry.Supported || entry.Problem.Length > 0 || entry.Installed) return false;
            // 0.5.10: a local editor schema may declare replaced_by_sml as well (Resources Plus).
            if (entry.LocalEditor)
            {
                try { var spec = LocalEditorSpec.Load(entry.Root); return spec.ReplacedBySml.Length > 0 && Sml.Hosts(build, spec.ReplacedBySml); }
                catch (Exception) { return false; }
            }
            try { return Blocked(Package.Load(entry.Root), build); }
            catch (Exception) { return false; }
        }
        public static bool Blocked(LocalEditorSpec spec, string build)
        { return spec != null && spec.ReplacedBySml.Length > 0 && Sml.Hosts(build, spec.ReplacedBySml); }
        public static bool Blocked(Package package, string build)
        { return package != null && package.ReplacedBySml.Length > 0 && Sml.Hosts(build, package.ReplacedBySml); }
        public static bool ConfiguredActive(CatalogEntry entry, string build)
        {
            if (entry == null || !entry.Supported || entry.Problem.Length > 0) return false;
            // 0.5.16: a fork that SML pushes aside is paused, not active - amber in the list, like Deposits Plus.
            if(entry.LocalEditor)return !BlockedBySml(entry,build)&&LocalResourceRuntime.Active(entry,build);
            try
            {
                Package package=entry.Installed?InstalledPlugins.Load(build,entry.Target,Catalog.SchemaRoot):Package.Load(entry.Root);
                if (Blocked(package, build)) return false;
                if (package.Kind != "plugin") return package.Kind == "content" && (ContentSession.IsProvided(build, package.Id) || ContentSession.MergedByLoader(build, package));
                string root = Path.GetFullPath(build);
                string dll = SafeFiles.Child(root, "plugins\\"+package.Target+".dll");
                // Under SML a Workshop package's DLL is loaded from the package itself;
                // under the bridge only when the package is on its list.
                if (!File.Exists(dll))
                {
                    if (package.Installed) return false;
                    if (!Sml.Active(root) && !(Bridge.Active(root) && Bridge.Listed(root, Bridge.Key(package)))) return false;
                }
                if (package.HasConfig && !package.EditorManaged)
                {
                    string ini = SafeFiles.Child(root, "plugins\\"+package.ConfigName);
                    Ini plugin;
                    if (File.Exists(ini)) { plugin = new Ini(SafeFiles.Text(ini)); ConfigRules.Validate(package,plugin); }
                    // 0.5.2: nothing deployed to plugins\ yet. Loaded from the package (SML or the
                    // bridge), the DLL reads the INI beside itself, so that one says what runs.
                    else if (!File.Exists(dll) && package.DefaultsBytes != null) plugin = new Ini(SafeFiles.Decode(package.DefaultsBytes));
                    else return false;
                    // With an overlay the deployed INI is the untouched original; the
                    // personal value of the switch lives in user_config.
                    string overlayPath = SafeFiles.Child(root, "user_config\\"+package.ConfigName);
                    var overlay = package.UserOverlay && File.Exists(overlayPath) ? new Ini(SafeFiles.Text(overlayPath)) : null;
                    if (package.EnabledField.Length>0)
                    {
                        string state; if (overlay == null || !overlay.Values.TryGetValue(package.EnabledField, out state)) plugin.Values.TryGetValue(package.EnabledField, out state);
                        if (GenericSchema.StripComment(state) != "1") return false;
                    }
                }
                string loaderPath = SafeFiles.Child(root, "tesmioloader.ini");
                var loader = File.Exists(loaderPath) ? new Ini(SafeFiles.Text(loaderPath)) : new Ini("");
                if (loader.Get("tesmioloader", "plugins", "1") == "0") return false;
                // A bridge-loaded package has no [plugins] key of its own: the bridge's
                // switch and its list decide.
                if (!File.Exists(dll)) return Sml.Active(root) || loader.Get("plugins", Bridge.Name, "1") != "0";
                return loader.Get("plugins", package.Target, "1") != "0";
            }
            catch (Exception) { return false; }
        }
    }

    // How the game is started. tesmiolauncher --nogui launches straight away with
    // what tesmioloader.ini says; tesmiolauncher_window = 1 in rmm.ini brings the
    // launcher's own window back.
    public static class LauncherOptions
    {
        public static bool ShowWindow;
        public static string Arguments { get { return ShowWindow ? "" : "--nogui"; } }
        // 0.4.92: how many seconds RMM keeps looking after the game start before it closes
        // itself. 0 turns the watch off and closes right away, the way it worked before.
        public static double WatchSeconds = 15;
        // 0.5.9: leave the window standing after the game was started - for everyone who wants
        // to carry on setting things up while the game loads and close RMM by hand.
        public static bool KeepOpen;
    }

    // 0.4.92: the launcher hands the game over and ends, so nobody notices when the game
    // quits again one second later - the signature of a Steam client that refuses a game
    // started outside Steam. Three outcomes: the game shows up and stays (nothing to say),
    // it never shows up (a slow disk looks the same, so also nothing to say), or it shows
    // up and is gone again inside the window - that one is worth a word.
    public sealed class LaunchWatch
    {
        public readonly double Seconds;
        // 0.5.6: a game that quits again is only worth a warning when it quits IMMEDIATELY.
        // Measured twice on the real machine: the Steam failure ran 1.3 s (hooks installed
        // 08:45:53.5, shutdown 08:45:54.8), a player closing it on purpose as fast as he could
        // still took 10.1 s (20:17:52.998 to 20:18:03.082) - the game has to load first. Below
        // the grace it is Steam, above it the player, and the player gets no lecture.
        public const double DefaultGrace = 5.0;
        public readonly double Grace;
        public LaunchWatch(double seconds) : this(seconds, DefaultGrace) { }
        public LaunchWatch(double seconds, double grace) { Seconds = seconds; Grace = grace; }
        double seenAt = -1;
        public bool Finished { get; private set; }
        public bool Died { get; private set; }
        public double Lived { get; private set; }
        public void Tick(bool gameRunning, double elapsedSeconds)
        {
            if (Finished) return;
            if (gameRunning)
            {
                if (seenAt < 0) seenAt = elapsedSeconds;
                if (elapsedSeconds >= Seconds) Finished = true;
                return;
            }
            if (seenAt >= 0)
            {
                Lived = elapsedSeconds - seenAt;
                Died = Lived < Grace;
                Finished = true;
                return;
            }
            if (elapsedSeconds >= Seconds) Finished = true;
        }
    }


    // 0.4.93: the handful of settings that belong to RMM itself and not to a plugin. rmm.ini
    // ships the defaults (that is the file the Workshop package installs); what the options
    // window sets goes into the saved view and wins, the same rule that already applies to
    // folders and language. The window shows the rmm.ini value underneath when both differ.
    public static class AppOptions
    {
        public static string IniLauncherWindow = "0", IniWatchSeconds = "15", IniVersionCheck = "1", IniKeepOpen = "0";
        public static double DefaultWatchSeconds { get { double d; return Double.TryParse(IniWatchSeconds, NumberStyles.Float, CultureInfo.InvariantCulture, out d) && d >= 0 ? d : 15; } }
        public static void Apply(UiState state)
        {
            if (state == null) return;
            if (state.LauncherWindow == "0" || state.LauncherWindow == "1") LauncherOptions.ShowWindow = state.LauncherWindow == "1";
            if (state.VersionCheck == "0" || state.VersionCheck == "1") GameVersion.CheckEnabled = state.VersionCheck == "1";
            if (state.KeepOpen == "0" || state.KeepOpen == "1") LauncherOptions.KeepOpen = state.KeepOpen == "1";
            double seconds;
            if (Double.TryParse(state.WatchSeconds, NumberStyles.Float, CultureInfo.InvariantCulture, out seconds) && seconds >= 0) LauncherOptions.WatchSeconds = seconds;
        }
    }
    // 0.5.20: the two log settings of the loader itself, in [tesmioloader] of
    // tesmioloader.ini. That file belongs to the loader, not to a plugin, so no
    // schema describes it and no entry in the list owns it - it is edited from the
    // options window, in place, through Ini.Render, so the launcher's comments and
    // every other key survive. Both values are read by the loader at game start.
    public static class LoaderLog
    {
        public const int DefaultLevel = 2, DefaultRepeat = 3, MaxRepeat = 50;
        static string Path(string build) { return SafeFiles.Child(build, "tesmioloader.ini"); }
        static int Read(string build, string key, int fallback, int max)
        {
            try
            {
                string file = Path(build);
                if (!File.Exists(file)) return fallback;
                string raw = new Ini(SafeFiles.Text(file)).Get("tesmioloader", key, "");
                int v;
                if (raw == null || !Int32.TryParse(raw.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out v)) return fallback;
                return Math.Max(0, Math.Min(max, v));
            }
            catch (IOException) { return fallback; }
            catch (FormatException) { return fallback; }
        }
        // 0 off, 1 errors, 2 errors and warnings, 3 everything.
        public static int Level(string build) { return Read(build, "log_game", DefaultLevel, 3); }
        // How often a repeating message of the game is written before it is only counted; 0 = always.
        public static int Repeat(string build) { return Read(build, "game_repeat_limit", DefaultRepeat, MaxRepeat); }
        public static bool Available(string build) { return File.Exists(Path(build)); }
        // The file as it would look with these two values. Returns null when nothing changes.
        public static byte[] Render(string build, int level, int repeat)
        {
            string file = Path(build);
            if (!File.Exists(file)) return null;
            if (Level(build) == level && Repeat(build) == repeat) return null;
            var change = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                { Ini.Id("tesmioloader", "log_game"), level.ToString(CultureInfo.InvariantCulture) },
                { Ini.Id("tesmioloader", "game_repeat_limit"), repeat.ToString(CultureInfo.InvariantCulture) }
            };
            return SafeFiles.Utf8.GetBytes(new Ini(SafeFiles.Text(file)).Render(change));
        }
        public static void Save(string build, int level, int repeat)
        {
            byte[] text = Render(build, level, repeat);
            if (text == null) return;
            string file = Path(build);
            var writes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase) { { file, text } };
            var before = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { { file, SafeFiles.HashFile(file) } };
            Transaction.Apply(build, "loader-log", writes, before, () => RuntimeGuard.NoGameRunning(build));
        }
    }
    public static class RuntimeGuard
    {
        // Profiles and restore points need only this part: nothing may be written
        // while the game or the launcher runs.
        public static void NoGameRunning(string build)
        {
            SafeFiles.NoLinks(build);
            foreach (var process in Process.GetProcesses())
                using (process)
                {
                    string name;
                    try { name = process.ProcessName; } catch (InvalidOperationException) { continue; }
                    if (name.StartsWith("SOVIET", StringComparison.OrdinalIgnoreCase) || name.Equals("tesmiolauncher", StringComparison.OrdinalIgnoreCase))
                        throw new IOException(Msg.Key("err_zuerst_spiel_und_tesmiolauncher", name));
                }
        }
        // 0.4.92: the same name rule as the guard above, as a question instead of a refusal -
        // the launch watch asks it every half second.
        public static bool GameRunning()
        {
            foreach (var process in Process.GetProcesses())
                using (process)
                {
                    string name;
                    try { name = process.ProcessName; } catch (InvalidOperationException) { continue; }
                    if (name.StartsWith("SOVIET", StringComparison.OrdinalIgnoreCase)) return true;
                }
            return false;
        }

        // 0.4.27: whether a found dependency is really loaded on the next game start,
        // by the same rules Check applies below: SML content and hooks, a folder on the
        // bridge list, or an enabled plugins\<target>.dll. False for unresolved ones.
        public static bool Provisioned(Session session, Dependency d)
        {
            if(!d.Found||!d.VersionOk) return false;
            string build=session.Build; bool sml=Sml.Active(build);
            if(d.Kind=="content") return sml||ContentSession.IsProvided(build,d.Id);
            if(sml||d.Target.Length==0) return true;
            if(session.BridgeActive&&d.Root.Length>0&&Bridge.Listed(build,Path.GetFileName(d.Root.TrimEnd('\\','/')))) return true;
            if(!File.Exists(SafeFiles.Child(build,"plugins\\"+d.Target+".dll"))) return false;
            string cfg=SafeFiles.Child(build,"tesmioloader.ini");
            Ini settings=File.Exists(cfg)?new Ini(SafeFiles.Text(cfg)):new Ini("");
            return settings.Get("plugins",d.Target,"1")!="0";
        }
        public static void Check(Session session)
        {
            string build=session.Build;Package package=session.Package;
            NoGameRunning(build);
            foreach (string file in new[] { "tesmioloader.dll", "tesmiolauncher.exe" })
                if (!File.Exists(SafeFiles.Child(build, file))) throw new IOException(Msg.Key("err_benoetigte_lokale_datei_fehlt", file));
            string cfg = SafeFiles.Child(build, "tesmioloader.ini");
            Ini settings = File.Exists(cfg) ? new Ini(SafeFiles.Text(cfg)) : new Ini("");
            if (settings.Get("tesmioloader", "plugins", "1") == "0")throw new IOException(Msg.Key("err_plugins_sind_im_vorhandenen"));
            bool sml = Sml.Active(build);
            foreach(string dependency in package.RequiresLocal)
            {
                // SML brings resources/deposits/needs/buildings along; with SML active
                // those are satisfied without a separate DLL.
                if(sml && Sml.Embedded.Contains(dependency)) continue;
                // 0.5.11: a fork with the same service (resources_plus for resources) satisfies the
                // requirement - the first one installed counts, the original last.
                string provider=ContentTargets.PluginsFor(dependency).FirstOrDefault(p=>File.Exists(SafeFiles.Child(build,"plugins\\"+p+".dll")))??dependency;
                if(!File.Exists(SafeFiles.Child(build,"plugins\\"+provider+".dll")))throw new IOException(Msg.Key("err_benoetigte_lokale_plugin_dll", dependency));
                if(settings.Get("plugins",provider,"1")=="0")throw new IOException(Msg.Key("err_benoetigtes_lokales_plugin_ist", provider));
            }
            // First what the Workshop folder lacks, then what is not deployed yet.
            foreach(Dependency d in package.Dependencies)
            {
                if(!d.Found)throw new IOException(Msg.Key("err_abhaengigkeit_nicht_im_workshop", d.Id, (d.Constraint.Length>0?" ("+d.Constraint+")":"")));
                if(!d.VersionOk)throw new IOException(Msg.Key("err_abhaengigkeit_in_falscher_version", d.Id, d.Version, d.Constraint));
            }
            foreach(Dependency d in package.Dependencies)
            {
                if(d.Kind=="content"){if(!sml&&!ContentSession.IsProvided(build,d.Id))throw new IOException(Msg.Key("err_abhaengigkeit_ist_sml_inhalt", d.Id));continue;}
                if(sml||d.Target.Length==0)continue;      // SML loads hook packages itself
                if(session.BridgeActive&&d.Root.Length>0&&Bridge.Listed(build,Path.GetFileName(d.Root.TrimEnd('\\','/'))))continue;   // the bridge loads it
                if(!File.Exists(SafeFiles.Child(build,"plugins\\"+d.Target+".dll")))throw new IOException(Msg.Key("err_abhaengigkeit_ist_nicht_bereitgestellt", d.Id, d.Target));
                if(settings.Get("plugins",d.Target,"1")=="0")throw new IOException(Msg.Key("err_abhaengigkeit_ist_im_tesmiolauncher", d.Id, d.Target));
            }
            // With SML active the package DLL must not also sit in plugins\, or the
            // game loads it twice - once from the Workshop, once from here.
            if(sml && !package.Installed && File.Exists(session.LocalDll))
                throw new IOException(Msg.Key("err_sml_ist_aktiv_und", package.Target));
            foreach(string conflict in package.ConflictsLocal)if(File.Exists(SafeFiles.Child(build,"plugins\\"+conflict+".dll"))&&settings.Get("plugins",conflict,"1")!="0")
                throw new IOException(Msg.Key("err_konflikt_plugin_ist_noch", conflict));
        }
    }

    public sealed class Session
    {
        public readonly Package Package;
        public readonly string Build, LocalDll, LocalIni, UserIni, Receipt, Upstream, LoaderIni;
        public readonly Dictionary<string, string> Overrides;
        public readonly Dictionary<string, string> Before = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        public readonly List<string> Notes = new List<string>();
        // SML installed and enabled in this loader folder: the DLL is SML's business.
        public readonly bool SmlActive;
        // workshop_bridge installed and on, SML absent, and this is a Workshop
        // package: the bridge loads the DLL from the package, Republic Mod Manager
        // deploys the INI only and keeps the package on the bridge's list.
        public readonly bool BridgeActive;
        // A plugins\<target>.dll that Republic Mod Manager itself deployed earlier (the
        // receipt proves it): with the bridge active it is removed on the next
        // save, so the package copy is the one that runs. Any other local copy is
        // the user's and stays; the bridge then skips the package.
        public readonly bool RemoveOwnDll, ForeignLocalDll;
        public readonly string BridgeIni;
        // What the last receipt says about this package versus the files now.
        public readonly UpdateState Update;
        // The loader's own switch, [plugins] <target> in tesmioloader.ini - whether
        // TesmioLoader loads the DLL at all. Meaningless for a package under SML
        // (SML loads it from the Workshop) and for content packages. Under the
        // bridge the same switch is the package's line in the bridge's list.
        public readonly bool LoaderSwitchApplies;
        public bool LoaderEnabled { get; private set; }
        // What the single "plugin active" switch controls here: the loader entry (or
        // the bridge list) with the INI's enabled field set alongside, only the INI
        // field when SML loads the package, or nothing at all.
        public string SwitchMode { get { return LoaderSwitchApplies ? "loader" : Package.EnabledField.Length > 0 ? "ini" : "none"; } }
        bool restoring;
        // "Dateien nur lokal": the author allows it (local_copy = 1), the bridge would
        // otherwise load the package, and the user chose the local copy. Then the
        // package is deployed like any plain package - DLL, INI and the assets folder
        // go to plugins\ - and the bridge skips it because the DLL is there.
        public readonly bool LocalCopyOffered, PreferLocal;
        public readonly List<string> AssetTargets = new List<string>();
        public bool LocalCopyChanged { get { return LocalCopyOffered && PreferLocal != ReceiptPrefersLocal(Build, Package); } }
        public static bool ReceiptPrefersLocal(string build, Package package)
        {
            try
            {
                string receipt = SafeFiles.Child(Path.GetFullPath(build), "user_config\\.autoload\\" + package.Target + ".receipt.ini");
                if (!File.Exists(receipt)) return false;
                var r = new Ini(SafeFiles.Text(receipt)); return r.Get("state", "id") == package.Id && r.Get("state", "local_copy", "0") == "1";
            }
            catch (Exception) { return false; }
        }
        // Asset files an earlier local copy put under plugins\, as the receipt lists them.
        static List<string> ReceiptAssets(string receipt)
        {
            var list = new List<string>();
            try { if (File.Exists(receipt)) foreach (var pair in new Ini(SafeFiles.Text(receipt)).Values) if (pair.Key.StartsWith("state/asset.", StringComparison.OrdinalIgnoreCase)) list.Add(pair.Value); }
            catch (FormatException) { }
            return list;
        }
        string AssetTarget(string relativeToPlugins) { return SafeFiles.Child(Build, "plugins\\" + relativeToPlugins); }
        // What a local copy puts into plugins\, relative to that folder.
        public List<string> LocalCopyFiles()
        {
            var files = new List<string> { Package.Target + ".dll" }; if (Package.HasConfig) files.Add(Package.ConfigName);
            files.AddRange(Package.AssetFiles.Select(x => Package.AssetsFolder + "\\" + x)); return files;
        }
        public Session(Package package, string build) : this(package, build, ReceiptPrefersLocal(build, package)) { }

        // A local INI written from an earlier package revision may lack keys the
        // package added since, or carry keys it dropped. It is read on top of the
        // current defaults, so a package update never blocks the entry; the values
        // it still knows are kept and validated like any other.
        Ini StaleLocal(string path)
        {
            var known = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in new Ini(SafeFiles.Text(path)).Values)
            {
                if (pair.Value.Length == 0) continue;
                // Collection sections are user-grown; every item of theirs travels along.
                string section = pair.Key.Split('/')[0];
                bool dynamic = Package.Collections.Any(c => string.Equals(section, c.CountSection, StringComparison.OrdinalIgnoreCase) || c.TargetSections.Any(t => string.Equals(t, section, StringComparison.OrdinalIgnoreCase)));
                if (dynamic || Package.Defaults.Values.ContainsKey(pair.Key)) known[pair.Key] = pair.Value;
            }
            var merged = new Ini(Package.Defaults.Render(known)); ConfigRules.Validate(Package, merged); return merged;
        }
        public Session(Package package, string build, bool preferLocal)
        {
            Package = package; Build = Path.GetFullPath(build); SmlActive = Sml.Active(Build);
            LoaderIni = SafeFiles.Child(Build, "tesmioloader.ini"); BridgeIni = Bridge.OverlayPath(Build);
            LocalDll = SafeFiles.Child(Build, "plugins\\"+Package.Target+".dll"); LocalIni = SafeFiles.Child(Build, "plugins\\"+Package.ConfigName);
            UserIni = SafeFiles.Child(Build, "user_config\\"+Package.ConfigName); Receipt = SafeFiles.Child(Build, "user_config\\.autoload\\"+Package.Target+".receipt.ini");
            Upstream = Package.Installed ? InstalledPlugins.UpstreamPath(Build, Package.Target) : null;
            bool bridge = Package.Kind == "plugin" && !Package.Installed && !SmlActive && Bridge.Active(Build);
            LocalCopyOffered = Package.LocalCopyOffered && bridge; PreferLocal = preferLocal && LocalCopyOffered;
            if (PreferLocal) bridge = false;
            if (bridge && File.Exists(LocalDll))
            {
                bool own = false;
                try { if (File.Exists(Receipt)) { var r = new Ini(SafeFiles.Text(Receipt)); own = r.Get("state", "id") == Package.Id && r.Get("state", "mode") == "package" && r.Get("state", "dll_hash") == SafeFiles.HashFile(LocalDll); } }
                catch (FormatException) { own = false; }
                RemoveOwnDll = own; ForeignLocalDll = !own;
                if (ForeignLocalDll) bridge = false;      // the local copy runs; treat it as a plain deployment
            }
            BridgeActive = bridge;
            Update = UpdateCheck.Of(Package, Build);
            LoaderSwitchApplies = Package.Kind == "plugin" && (Package.Installed || !SmlActive);
            LoaderEnabled = !LoaderSwitchApplies || ReadLoaderSwitch();
            if (!Package.Installed)
            {
                string packagePrefix = Package.Root.TrimEnd('\\', '/') + "\\", buildPrefix = Build.TrimEnd('\\', '/') + "\\";
                if (packagePrefix.StartsWith(buildPrefix, StringComparison.OrdinalIgnoreCase) || buildPrefix.StartsWith(packagePrefix, StringComparison.OrdinalIgnoreCase))
                    throw new IOException(Msg.Key("err_paket_und_loader_ordner"));
            }
            foreach (string path in new[] { LocalDll, LocalIni, UserIni, Receipt, LoaderIni }) Before[path] = SafeFiles.HashFile(path);
            if (Upstream != null) Before[Upstream] = SafeFiles.HashFile(Upstream);
            if (BridgeActive) Before[BridgeIni] = SafeFiles.HashFile(BridgeIni);
            if (Package.Kind == "plugin" && !Package.Installed && !SmlActive)
            {
                foreach (string relative in Package.AssetFiles) { string target = AssetTarget(Package.AssetsFolder + "\\" + relative); if (!AssetTargets.Contains(target)) AssetTargets.Add(target); }
                foreach (string relative in ReceiptAssets(Receipt)) { string target = AssetTarget(relative); if (!AssetTargets.Contains(target)) AssetTargets.Add(target); }
                foreach (string target in AssetTargets) Before[target] = SafeFiles.HashFile(target);
            }
            Overrides = File.Exists(UserIni) ? new Ini(SafeFiles.Text(UserIni)).Values : new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach(string id in Overrides.Keys.Where(id=>Package.IsDefault(id,Overrides[id])).ToList())Overrides.Remove(id);
            if (Package.Installed)
            {
                // The protected base decides what "original" means; InstalledPlugins
                // already chose it and told the package whether the copy must be refreshed.
                foreach (string hint in Package.Hints) if (Msg.KeyOf(hint).StartsWith("hint_installed_")) Notes.Add(hint);
            }
            else if (File.Exists(Receipt))
            {
                var receipt = new Ini(SafeFiles.Text(Receipt));
                // With SML active the DLL in plugins\ is never ours, so only the INI counts.
                // Under the bridge the DLL is either our own earlier copy (checked above) or absent.
                if (receipt.Get("state", "id") != Package.Id || (!Package.EditorManaged && receipt.Get("state", "ini_hash") != Before[LocalIni]) || (!SmlActive && !BridgeActive && receipt.Get("state", "dll_hash") != Before[LocalDll]))
                    throw new IOException(Msg.Key("err_lokale_dll_ini_wurde"));
                string currentDefaultsHash=Package.DefaultsHash;
                if(!Package.EditorManaged&&File.Exists(LocalIni)&&receipt.Get("state","defaults_hash","")!=currentDefaultsHash)
                {
                    var local=StaleLocal(LocalIni);
                    foreach(CollectionSpec collection in Package.Collections.Where(x=>x.UserOwned&&!Overrides.ContainsKey(x.CountId)))
                    {
                        var localState=CollectionRules.State(collection,local);var packageState=CollectionRules.State(collection,Package.Defaults);
                        bool changed=localState.Count!=packageState.Count||localState.Any(x=>!packageState.ContainsKey(x.Key)||packageState[x.Key]!=x.Value);
                        if(!changed)continue;
                        foreach(var pair in localState)if(!Package.IsDefault(pair.Key,pair.Value))Overrides[pair.Key]=pair.Value;
                        Notes.Add(Msg.Key("note_collection_kept", collection.Id));
                    }
                }
            }
            else if (!Package.EditorManaged && File.Exists(LocalIni) && !File.Exists(UserIni))
            {
                var local = StaleLocal(LocalIni);
                foreach (var pair in local.Values)
                {
                    if (!Overrides.ContainsKey(pair.Key) && !Package.IsDefault(pair.Key,pair.Value)) Overrides[pair.Key] = pair.Value;
                }
                Notes.Add(Msg.Key("note_first_import"));
            }
            Notes.Add(Msg.Key("note_originals_safe"));
            if (SmlActive && !Package.Installed) Notes.Add(Msg.Key("note_sml_active"));
            if (Update.Pending) Notes.Add(Msg.Key("note_update_pending", Update.PreviousVersion.Length > 0 ? Update.PreviousVersion : "?", Update.CurrentVersion, (Update.DllChanged ? "DLL" : "") + (Update.DllChanged && Update.DefaultsChanged ? ", " : "") + (Update.DefaultsChanged ? "INI" : "")));
            if (BridgeActive) Notes.Add(Msg.Key("note_bridge_active"));
            if (RemoveOwnDll) Notes.Add(Msg.Key("note_bridge_remove_copy", "plugins\\" + Package.Target + ".dll"));
            if (ForeignLocalDll) Notes.Add(Msg.Key("note_bridge_foreign_copy", "plugins\\" + Package.Target + ".dll"));
            if (Package.UserOverlay) Notes.Add(Msg.Key("note_overlay", Package.ConfigName));
            if (!Package.HasConfig) Notes.Add(Msg.Key("note_no_ini"));
            if (Package.Generic && Package.HasConfig) Notes.Add(Msg.Key("note_no_schema"));
            Effective();
        }
        public string Effective()
        {
            string text = Package.Defaults.Render(Overrides); ConfigRules.Validate(Package,new Ini(text)); return text;
        }
        bool ReadLoaderSwitch()
        {
            if (BridgeActive) return Bridge.Listed(Build, Bridge.Key(Package));
            try { return !File.Exists(LoaderIni) || new Ini(SafeFiles.Text(LoaderIni)).Get("plugins", Package.Target, "1") != "0"; }
            catch (FormatException) { return true; }
        }
        public void SetLoaderEnabled(bool enabled) { if (LoaderSwitchApplies) LoaderEnabled = enabled; }
        public bool LoaderChanged { get { return LoaderSwitchApplies && LoaderEnabled != ReadLoaderSwitch(); } }
        // tesmioloader.ini is the launcher's file: edited in place so its comments
        // and the other plugins' switches survive, never written with a BOM.
        byte[] RenderLoaderIni()
        {
            if (BridgeActive) return Bridge.Render(Build, Bridge.Key(Package), LoaderEnabled);
            var change = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { { Ini.Id("plugins", Package.Target), LoaderEnabled ? "1" : "0" } };
            string text = File.Exists(LoaderIni) ? new Ini(SafeFiles.Text(LoaderIni)).Render(change) : "[plugins]\r\n" + Package.Target + "=" + (LoaderEnabled ? "1" : "0") + "\r\n";
            return SafeFiles.Utf8.GetBytes(text);
        }
        public void Set(Field field, string value)
        {
            if (field.Type == "readonly") return;
            Overrides[field.Id] = field.Normalize(value);
        }
        public void ResetVisible()
        {
            foreach (Field field in Package.Fields.Where(f => f.Type != "readonly")) Overrides.Remove(field.Id);
        }
        public void AssertUnchanged()
        {
            Package.AssertUnchanged();
            foreach (var pair in Before) { SafeFiles.NoLinks(pair.Key); if (SafeFiles.HashFile(pair.Key) != pair.Value) throw new IOException(Msg.Key("err_datei_inzwischen_geaendert_neu", pair.Key)); }
        }
        // Shared files RMM writes when another entry is saved (0.4.71): the loader's plugin switches
        // and the bridge list. Their hashes are taken afresh, so the next commit of this session does
        // not mistake RMM's own write for a foreign change; every other file stays guarded.
        public void RehashShared() { foreach (string path in new[] { LoaderIni, BridgeIni }) if (path != null && Before.ContainsKey(path)) Before[path] = SafeFiles.HashFile(path); }
        public string Commit(bool deploy, Action guard)
        {
            // Caller confirms native-code trust for this exact loaded package/hash.
            string mutexName = "Local\\TesmioAutoload_" + SafeFiles.Hash(SafeFiles.Utf8.GetBytes(Build.ToUpperInvariant()));
            using (var mutex = new Mutex(false, mutexName))
            {
                bool held = false;
                try
                {
                    try { held = mutex.WaitOne(0); } catch (AbandonedMutexException) { held = true; }
                    if (!held) throw new IOException(Msg.Key("err_ein_anderes_autoload_fenster"));
                    guard(); AssertUnchanged();
                    byte[] effective = SafeFiles.Utf8.GetBytes(Effective());
                    var writes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
                    if (Package.HasConfig && !Package.EditorManaged)
                    {
                        var personal = Overrides;
                        if (Package.Target.Equals(Bridge.Name, StringComparison.OrdinalIgnoreCase) && File.Exists(UserIni))
                        {
                            // The bridge's overlay doubles as its package list: [packages]
                            // lines survive every save and even "restore original".
                            personal = new Dictionary<string, string>(Overrides, StringComparer.OrdinalIgnoreCase);
                            foreach (var pair in new Ini(SafeFiles.Text(UserIni)).Values)
                                if (pair.Key.StartsWith("packages/", StringComparison.OrdinalIgnoreCase) && !personal.ContainsKey(pair.Key)) personal[pair.Key] = pair.Value;
                        }
                        writes[UserIni] = SafeFiles.Utf8.GetBytes(File.Exists(UserIni)
                            ? new Ini(SafeFiles.Text(UserIni)).RewriteValues(personal) : Ini.Sparse(personal));
                    }
                    if (LoaderChanged) writes[BridgeActive ? BridgeIni : LoaderIni] = RenderLoaderIni();
                    if (deploy)
                    {
                        // Overlay plugins read user_config themselves: the deployed INI is
                        // the package original, byte for byte. Everything else gets the
                        // effective INI (defaults plus personal values). Restoring an
                        // installed plugin writes the protected original back verbatim.
                        string iniHash = Before[LocalIni];
                        if (Package.HasConfig && !Package.EditorManaged)
                        {
                            byte[] local = (Package.UserOverlay || restoring) ? Package.DefaultsBytes : effective;
                            writes[LocalIni] = local; iniHash = SafeFiles.Hash(local);
                        }
                        string dllHash = Before[LocalDll]; var copiedAssets = new List<string>();
                        if (Package.Installed)
                        {
                            // Never the DLL - it is the user's own installation. The original
                            // INI is copied once, and again whenever it changed outside.
                            if (Package.HasConfig && (Package.RefreshUpstream || !File.Exists(Upstream))) writes[Upstream] = Package.DefaultsBytes;
                        }
                        else if (BridgeActive)
                        {
                            // The bridge takes over: our own DLL copy and the assets we copied go.
                            if (RemoveOwnDll) { writes[LocalDll] = null; dllHash = "absent"; foreach (string target in AssetTargets) if (File.Exists(target)) writes[target] = null; }
                        }
                        else if (!SmlActive)
                        {
                            writes[LocalDll] = Package.Dll; dllHash = SafeFiles.Hash(Package.Dll);
                            foreach (string relative in Package.AssetFiles)
                            {
                                string entry = Package.AssetsFolder + "\\" + relative;
                                writes[AssetTarget(entry)] = SafeFiles.Read(Path.Combine(Package.AssetsDir, relative), 64 * 1024 * 1024); copiedAssets.Add(entry);
                            }
                            // Assets of an earlier package version that the current one no longer ships.
                            foreach (string target in AssetTargets) if (!writes.ContainsKey(target) && File.Exists(target)) writes[target] = null;
                        }
                        var receipt = new StringBuilder("[state]\r\nid = " + Package.Id + "\r\nversion = " + Package.Version + "\r\nmode = " + (Package.Installed ? "installed" : SmlActive ? "sml" : BridgeActive ? "bridge" : "package") + "\r\ndll_hash = " + dllHash + "\r\nini_hash = " + iniHash + "\r\ndefaults_hash = "+Package.DefaultsHash+"\r\n");
                        if (LocalCopyOffered || PreferLocal) receipt.Append("local_copy = " + (PreferLocal ? "1" : "0") + "\r\n");
                        for (int i = 0; i < copiedAssets.Count; ++i) receipt.Append("asset." + i + " = " + copiedAssets[i] + "\r\n");
                        writes[Receipt] = SafeFiles.Utf8.GetBytes(receipt.ToString());
                    }
                    string backup = Transaction.Apply(Build, Package.Id, writes, Before, guard);
                    foreach (string path in Before.Keys.ToList()) Before[path] = SafeFiles.HashFile(path);
                    if (deploy && Package.Installed) Package.RefreshUpstream = false;
                    return backup;
                }
                finally { if (held) mutex.ReleaseMutex(); }
            }
        }
        // Installed plugins only: the protected original goes back into plugins\
        // byte for byte and every personal value is dropped. Transactional like Commit.
        public string RestoreOriginal(Action guard)
        {
            if (!Package.Installed || !Package.HasConfig) throw new InvalidOperationException(Msg.Key("err_nur_ein_installiertes_plugin"));
            Overrides.Clear(); restoring = true;
            try { return Commit(true, guard); }
            finally { restoring = false; }
        }
    }

    public static class Transaction
    {
        // Keeps one complete rolling restore point per plugin. A new snapshot is
        // promoted only after all writes have passed verification; the former one
        // remains available throughout the transaction.
        public static string Apply(string build, string pluginId, Dictionary<string, byte[]> writes, Dictionary<string, string> before, Action guard)
        {
            string state = SafeFiles.Child(build, "user_config\\.autoload");
            Directory.CreateDirectory(state);
            string pending = SafeFiles.Child(state, "pending.txt");
            if (File.Exists(pending)) throw new IOException(Msg.Key("err_unvollstaendige_bereitstellung_erkannt_erst", SafeFiles.Text(pending)));
            // A null value deletes the file (only ever a copy this program made itself).
            Func<byte[], string> hashOf = bytes => bytes == null ? "absent" : SafeFiles.Hash(bytes);
            var changed = writes.Where(p => hashOf(p.Value) != before[p.Key]).ToList();
            if (changed.Count == 0) return "Keine Datei musste geaendert werden.";
            string backupRoot = SafeFiles.Child(state,"backups"); Directory.CreateDirectory(backupRoot);
            string group = SafeFiles.Child(backupRoot,PluginFolder(pluginId)); Directory.CreateDirectory(group);
            string backup = SafeFiles.Child(group,"new_"+Guid.NewGuid().ToString("N").Substring(0,8));
            Directory.CreateDirectory(backup);
            string staging=SafeFiles.Child(backup,"staged"); Directory.CreateDirectory(staging);
            var installed = new List<string>(); var staged = new Dictionary<string, string>(); var old = new Dictionary<string, string>();
            var map = new StringBuilder(); bool markerCreated = false;
            try
            {
                int snapshotIndex=0;
                foreach(var pair in before.OrderBy(p=>p.Key,StringComparer.OrdinalIgnoreCase))
                {
                    SafeFiles.NoLinks(pair.Key);
                    if (SafeFiles.HashFile(pair.Key) != before[pair.Key]) throw new IOException(Msg.Key("err_datei_wurde_waehrend_der", pair.Key));
                    string previous=Path.Combine(backup,snapshotIndex+".old");
                    if(File.Exists(pair.Key)) {File.Copy(pair.Key,previous);if(SafeFiles.HashFile(previous)!=before[pair.Key])throw new IOException(Msg.Key("err_sicherung_stimmt_nicht_ueberein"));old.Add(pair.Key,previous);}
                    map.AppendLine(snapshotIndex+": "+pair.Key+" (vorher "+before[pair.Key]+")"); snapshotIndex++;
                }
                foreach (var pair in changed)
                {
                    if (pair.Value == null) continue;
                    string temp=Path.Combine(staging,staged.Count+".new"); File.WriteAllBytes(temp,pair.Value); staged.Add(pair.Key,temp);
                }
                File.WriteAllText(Path.Combine(backup, "files.txt"), map.ToString(), SafeFiles.Utf8);
                File.WriteAllText(Path.Combine(backup,"plugin.id.txt"),pluginId,SafeFiles.Utf8);
                using (var file = new FileStream(pending, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                { byte[] b = SafeFiles.Utf8.GetBytes(backup); file.Write(b, 0, b.Length); file.Flush(true); }
                markerCreated = true;
                foreach (var pair in changed)
                {
                    guard(); SafeFiles.NoLinks(pair.Key);
                    if (SafeFiles.HashFile(pair.Key) != before[pair.Key]) throw new IOException(Msg.Key("err_zieldatei_wurde_zwischenzeitlich_geaendert", pair.Key));
                    if (pair.Value == null) { File.Delete(pair.Key); installed.Add(pair.Key); }
                    else { Directory.CreateDirectory(Path.GetDirectoryName(pair.Key)); Replace(staged[pair.Key], pair.Key); installed.Add(pair.Key); }
                    if (SafeFiles.HashFile(pair.Key) != hashOf(pair.Value)) throw new IOException(Msg.Key("err_schreibpruefung_fehlgeschlagen", pair.Key));
                }
                File.Delete(pending); markerCreated = false;
                try { DeleteTree(group,staging); } catch { /* Staged copies are harmless inside the verified backup. */ }
                string promoted;
                try { promoted=Promote(group,backup); }
                catch { return backup+" (rollierendes Aufraeumen beim naechsten Speichern wiederholen)"; }
                try { PruneGroup(group,promoted); PruneLegacy(backupRoot,group,new HashSet<string>(before.Keys,StringComparer.OrdinalIgnoreCase)); }
                catch { return promoted+" (alte Sicherungen konnten noch nicht vollstaendig entfernt werden)"; }
                return promoted;
            }
            catch (Exception failure)
            {
                bool restored = true;
                foreach (string path in installed.AsEnumerable().Reverse())
                {
                    try
                    {
                        SafeFiles.NoLinks(path);
                        if (SafeFiles.HashFile(path) != hashOf(writes[path])) { restored = false; continue; }
                        if (old.ContainsKey(path)) { string restore = Path.Combine(backup, Guid.NewGuid().ToString("N") + ".restore"); File.Copy(old[path], restore); Replace(restore, path); }
                        else File.Delete(path); // only a newly created file belonging to this transaction
                    }
                    catch { restored = false; }
                }
                if (restored && markerCreated) File.Delete(pending);
                if(restored) try {DeleteTree(group,backup);} catch { }
                if (!restored) throw new IOException(Msg.Key("err_bereitstellung_nicht_vollstaendig_zurueckgesetzt", backup), failure);
                throw;
            }
        }
        static string PluginFolder(string id)
        {
            if(Regex.IsMatch(id??"","^[A-Za-z0-9._-]{1,80}$")) return id;
            return "plugin_"+SafeFiles.Hash(SafeFiles.Utf8.GetBytes(id??"")).Substring(0,16);
        }
        static string Promote(string group,string candidate)
        {
            string previous=SafeFiles.Child(group,"previous"),retired=SafeFiles.Child(group,"old_"+Guid.NewGuid().ToString("N").Substring(0,8)); bool moved=false;
            if(Directory.Exists(previous)){SafeFiles.NoLinks(previous);Directory.Move(previous,retired);moved=true;}
            try {Directory.Move(candidate,previous);}
            catch {if(moved&&!Directory.Exists(previous)&&Directory.Exists(retired))Directory.Move(retired,previous);throw;}
            if(moved&&Directory.Exists(retired)) try {DeleteTree(group,retired);} catch { }
            return previous;
        }
        static void PruneLegacy(string backupRoot,string group,HashSet<string> managed)
        {
            foreach(string directory in Directory.GetDirectories(backupRoot))
            {
                if(String.Equals(Path.GetFullPath(directory),Path.GetFullPath(group),StringComparison.OrdinalIgnoreCase))continue;
                string map=Path.Combine(directory,"files.txt");if(!File.Exists(map))continue;
                var paths=new List<string>();bool valid=true;
                foreach(string line in File.ReadAllLines(map,SafeFiles.Utf8))
                {
                    var match=Regex.Match(line,"^\\d+: (.*) \\(vorher (?:[A-Fa-f0-9]{64}|absent)\\)$");
                    if(!match.Success){valid=false;break;}paths.Add(match.Groups[1].Value);
                }
                if(valid&&paths.Count>0&&paths.All(managed.Contains))DeleteTree(backupRoot,directory);
            }
        }
        static void PruneGroup(string group,string previous)
        {
            foreach(string directory in Directory.GetDirectories(group))
                if(!String.Equals(Path.GetFullPath(directory),Path.GetFullPath(previous),StringComparison.OrdinalIgnoreCase))DeleteTree(group,directory);
        }
        static void DeleteTree(string allowedRoot,string target)
        {
            string root=Path.GetFullPath(allowedRoot).TrimEnd('\\','/')+Path.DirectorySeparatorChar,full=Path.GetFullPath(target);
            if(!full.StartsWith(root,StringComparison.OrdinalIgnoreCase))throw new IOException(Msg.Key("err_sicherungsordner_liegt_ausserhalb_des", full));
            if(!Directory.Exists(full))return;
            SafeFiles.NoLinks(full);
            foreach(string directory in Directory.GetDirectories(full,"*",SearchOption.AllDirectories))SafeFiles.NoLinks(directory);
            foreach(string file in Directory.GetFiles(full,"*",SearchOption.AllDirectories))SafeFiles.NoLinks(file);
            Directory.Delete(full,true);
        }
        private static void Replace(string source, string destination)
        {
            if (File.Exists(destination)) File.Replace(source, destination, null); else File.Move(source, destination);
        }
    }

    // Steam's own note of who is logged in on this machine. A game started
    // outside Steam - which is what the TesmioLauncher does - reads exactly this
    // to attach to the client, and aborts with its own Steam error when no user
    // is in it. The usual way into that state is uploading a Workshop item:
    // SteamCMD logs in with the same account, Steam replaces the client's
    // session and deliberately does not reconnect it.
    public static class SteamSession
    {
        public enum State { Unknown, LoggedOut, Ready }
        const string Key = @"HKEY_CURRENT_USER\Software\Valve\Steam\ActiveProcess";
        public static State Check() { string reason; return Check(out reason); }
        // 0.4.91: the raw record behind Check(), for the --steam-check line. Diagnosis only,
        // never a decision - it is what a human would read out of the registry.
        public static string Evidence()
        {
            try
            {
                object user = Registry.GetValue(Key, "ActiveUser", null), pid = Registry.GetValue(Key, "pid", null);
                string clients;
                try { Process[] running = Process.GetProcessesByName("steam"); clients = running.Length + " steam process(es) running"; foreach (Process p in running) p.Dispose(); }
                catch (Exception) { clients = "process list unavailable"; }
                return "ActiveUser=" + (user ?? "<none>") + ", pid=" + (pid ?? "<none>") + ", " + clients;
            }
            catch (Exception e) { return e.GetType().Name; }
        }
        // 0.4.91: the check says what it found. "reason" is a language key for the warning and the
        // journal; it stays empty for Ready and Unknown. A lookup that simply fails is Unknown, never
        // LoggedOut - the old code turned every exception of the process list into "Steam is logged
        // out" and warned the user while his client was running and logged on.
        public static State Check(out string reason)
        {
            reason = "";
            try
            {
                object user = Registry.GetValue(Key, "ActiveUser", null);
                object pid = Registry.GetValue(Key, "pid", null);
                // No values at all: another Steam build, another platform, or no
                // Steam. Never block on a guess.
                if (!(user is int) || !(pid is int)) return State.Unknown;
                if ((int)user == 0) { reason = "steam_reason_user"; return State.LoggedOut; }
                if ((int)pid == 0) { reason = "steam_reason_pid"; return State.LoggedOut; }
                // Steam records a logged-in user and a client process. Only a client that is
                // really gone counts as logged out; a recorded id that no longer matches while
                // some client runs is Steam's own bookkeeping, not a login state.
                Process[] clients;
                try { clients = Process.GetProcessesByName("steam"); }
                catch (Exception) { return State.Unknown; }
                try { if (clients.Length == 0) { reason = "steam_reason_gone"; return State.LoggedOut; } }
                finally { foreach (Process client in clients) client.Dispose(); }
                return State.Ready;
            }
            catch (Exception) { return State.Unknown; }
        }
    }

    public static class Discovery
    {
        public static List<string> SteamLibraries()
        {
            var result = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string steam = Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
            if (!String.IsNullOrEmpty(steam) && Directory.Exists(steam))
            {
                result.Add(Path.GetFullPath(steam)); string vdf = Path.Combine(steam, @"steamapps\libraryfolders.vdf");
                if (File.Exists(vdf)) foreach (Match match in Regex.Matches(SafeFiles.Text(vdf), "\"path\"\\s*\"([^\"]+)\""))
                { string p = match.Groups[1].Value.Replace(@"\\", @"\"); if (Directory.Exists(p)) result.Add(Path.GetFullPath(p)); }
            }
            return result.ToList();
        }
        public static List<string> FindPackages(List<string> diagnostics)
        {
            var result = new List<string>();
            foreach (string library in SteamLibraries())
            {
                string root = Path.Combine(library, @"steamapps\workshop\content\784150");
                if (!Directory.Exists(root)) continue;
                try
                {
                    SafeFiles.NoLinks(root);
                    foreach (string dir in Directory.GetDirectories(root))
                    {
                        if (!File.Exists(Path.Combine(dir, "soviet.mod.ini"))) continue;
                        // 0.5.9: our own overlay layer is not an entry of its own - it belongs to the
                        // editors that write it (see SmlOverlay).
                        if (SmlOverlay.IsOverlay(Path.Combine(dir, "soviet.mod.ini"))) continue;
                        try { var p = Package.Load(dir); result.Add(p.Root); }
                        catch (Exception e) { diagnostics.Add(dir + ": " + e.Message); }
                    }
                }
                catch (Exception e) { diagnostics.Add(root + ": " + e.Message); }
            }
            return result;
        }
    }
}
