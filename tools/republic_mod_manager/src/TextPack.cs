// Republic Mod Manager - a Localization text pack edited in place (0.4.30): the
// folder plugins\localization\<pack> with localization.ini (namespace, fallback,
// missingText) and one soviet<Language>.ini per language, each a [strings]
// section of "key = text". Edits stay in memory and are handed to the session
// as dependent writes, so they land with the next save and its backup.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    // [textpack] of an editor schema: where the local pack lives (a path spec like the
    // folder rows use), where the shipped copy comes from (a dependency's package folder)
    // and which item group supplies the "<id>.name" / "<id>.desc" rows.
    public sealed class TextPackSpec
    {
        public string Tab = "", Folder = "", SeedDependency = "", SeedPath = "", Namespace = "", KeysFrom = "";
        public string Label = "", LabelKey = "", Description = "", DescriptionKey = "", Missing = "", MissingKey = "";
        // required = 1 (0.4.75): while the local folder is missing, every tab shows a red box
        // with the notice and the text pack's tab is tinted red - the player must click
        // "Create locally" first, otherwise the plugin refuses its own entries in the game.
        public bool Required; public string RequiredNotice = "", RequiredNoticeKey = "";
    }

    public sealed class TextPackSession
    {
        public readonly string Folder; public readonly int Generation;
        public string Namespace = "", Fallback = "", MissingText = "";
        public bool Exists { get; private set; }
        LooseIni config;
        readonly Dictionary<string, LooseIni> files = new Dictionary<string, LooseIni>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string, string> original = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        public static string ConfigPath(string folder) { return Path.Combine(folder, "localization.ini"); }
        public string LanguageFile(string language) { return Path.Combine(Folder, "soviet" + language + ".ini"); }

        public TextPackSession(string folder, int generation)
        {
            Folder = Path.GetFullPath(folder); Generation = generation;
            string cfg = ConfigPath(Folder);
            Exists = File.Exists(cfg);
            if (!Exists) return;
            config = new LooseIni(SafeFiles.Text(cfg)); original[cfg] = config.Render();
            Namespace = config.Get("localization", "namespace") ?? "";
            string fallback = config.Get("localization", "fallback") ?? "";
            Fallback = fallback.StartsWith("soviet", StringComparison.OrdinalIgnoreCase) ? fallback.Substring(6) : fallback;
            MissingText = config.Get("localization", "missingText") ?? "";
            foreach (string path in Directory.GetFiles(Folder, "soviet*.ini"))
            {
                string language = Path.GetFileNameWithoutExtension(path).Substring(6);
                if (language.Length == 0) continue;
                var ini = new LooseIni(SafeFiles.Text(path));
                files[language] = ini; original[path] = ini.Render();
            }
        }

        public List<string> Languages { get { return files.Keys.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList(); } }
        public bool HasLanguage(string language) { return files.ContainsKey(language); }

        public void SetFallback(string language)
        {
            if (!files.ContainsKey(language)) throw new FormatException(Msg.Key("err_textpaket_sprache_fehlt", language));
            Fallback = language; config.EnsureSection("localization"); config.Set("localization", "fallback", "soviet" + language);
        }

        // A new, empty language file; the game's language names (German, English, ...) are the
        // only valid ones, which the caller checks against media_soviet.
        public void AddLanguage(string language)
        {
            if (String.IsNullOrWhiteSpace(language) || language.Any(c => !Char.IsLetter(c))) throw new FormatException(Msg.Key("err_textpaket_sprache_ungueltig", language ?? ""));
            if (files.ContainsKey(language)) throw new FormatException(Msg.Key("err_textpaket_sprache_vorhanden", language));
            files[language] = new LooseIni("[strings]\r\n");
        }

        public string Get(string language, string key)
        {
            LooseIni ini; if (!files.TryGetValue(language, out ini)) return "";
            try { return ini.Get("strings", key) ?? ""; } catch (FormatException) { return ini.GetAll("strings", key).FirstOrDefault() ?? ""; }
        }

        public void Set(string language, string key, string value)
        {
            LooseIni ini; if (!files.TryGetValue(language, out ini)) throw new FormatException(Msg.Key("err_textpaket_sprache_fehlt", language));
            key = (key ?? "").Trim(); value = (value ?? "").Replace("\r\n", "\\n").Replace("\n", "\\n").Trim();
            if (!ValidKey(key)) throw new FormatException(Msg.Key("err_textpaket_schluessel_ungueltig", key));
            ini.EnsureSection("strings");
            if (value.Length == 0) ini.Remove("strings", key); else ini.Set("strings", key, value);
        }

        // Every <id>.name and <id>.desc that no language file defines gets the id as its text in
        // the given language (0.4.77): an entry saved without texts then never makes the plugin
        // refuse the whole expansion; the player replaces the placeholder on the text pack tab.
        public int EnsureKeys(IEnumerable<string> ids, string language)
        {
            if (!files.ContainsKey(language)) return 0;
            var known = new HashSet<string>(Keys(), StringComparer.OrdinalIgnoreCase); int added = 0;
            foreach (string id in ids)
                foreach (string suffix in new[] { ".name", ".desc" })
                {
                    string key = id + suffix;
                    if (known.Contains(key) || !ValidKey(key)) continue;
                    Set(language, key, id); known.Add(key); added++;
                }
            return added;
        }
        // Removes a key from every language file (0.4.34); used for keys no research owns.
        public void RemoveKey(string key)
        {
            foreach (LooseIni ini in files.Values) ini.Remove("strings", key);
        }

        public static bool ValidKey(string key)
        { return !String.IsNullOrEmpty(key) && key.Length < 128 && key.All(c => Char.IsLetterOrDigit(c) || c == '.' || c == '_' || c == '-') && !key.StartsWith(".") && !key.EndsWith("."); }

        // Every key of every language, sorted.
        public List<string> Keys()
        {
            var keys = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (LooseIni ini in files.Values) foreach (var entry in ini.Entries("strings")) keys.Add(entry.Key);
            return keys.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        }

        // The files whose text differs from what was read (or did not exist), as bytes for
        // LocalResourceSession.StageDependencyWrites.
        public IDictionary<string, byte[]> Writes()
        {
            var writes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            if (config != null) Collect(writes, ConfigPath(Folder), config.Render());
            foreach (var pair in files) Collect(writes, LanguageFile(pair.Key), pair.Value.Render());
            return writes;
        }
        void Collect(IDictionary<string, byte[]> writes, string path, string text)
        {
            string was; if (original.TryGetValue(path, out was) && was == text) return;
            writes[path] = SafeFiles.Utf8.GetBytes(text);
        }

        // Creates the local pack: a copy of the shipped folder when there is one (every
        // localization.ini and soviet*.ini that does not exist locally yet), otherwise a
        // minimal localization.ini plus an empty sovietEnglish.ini.
        public static List<string> Seed(string folder, string seedFolder, string nameSpace)
        {
            var written = new List<string>();
            Directory.CreateDirectory(folder);
            if (!String.IsNullOrEmpty(seedFolder) && File.Exists(ConfigPath(seedFolder)))
            {
                foreach (string source in new[] { ConfigPath(seedFolder) }.Concat(Directory.GetFiles(seedFolder, "soviet*.ini")))
                {
                    string target = Path.Combine(folder, Path.GetFileName(source));
                    if (File.Exists(target)) continue;
                    SafeFiles.NoLinks(source); File.Copy(source, target); written.Add(target);
                }
                return written;
            }
            string cfg = ConfigPath(folder);
            if (!File.Exists(cfg)) { File.WriteAllText(cfg, "[localization]\r\nnamespace = " + (String.IsNullOrEmpty(nameSpace) ? Path.GetFileName(folder.TrimEnd('\\', '/')) : nameSpace) + "\r\nfallback = sovietEnglish\r\nmissingText = [MISSING TEXT: {key}]\r\n", SafeFiles.Utf8); written.Add(cfg); }
            string english = Path.Combine(folder, "sovietEnglish.ini");
            if (!File.Exists(english)) { File.WriteAllText(english, "[strings]\r\n", SafeFiles.Utf8); written.Add(english); }
            return written;
        }
    }
}
