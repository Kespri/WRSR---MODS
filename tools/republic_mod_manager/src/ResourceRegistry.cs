using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    public sealed class ResourceOption
    {
        public string Id, Display;
        public override string ToString() { return Display == Id ? Id : Display + " (" + Id + ")"; }
    }

    // Reads a declarative option section from a local Tesmio plugin. It does not
    // use the strict general INI parser because provider INIs may contain dynamic
    // sections or repeated directives unrelated to the selected catalogue.
    public sealed class ResourceRegistry
    {
        public string Path = "", Hash = "absent", Problem = "";
        public readonly List<ResourceOption> Options = new List<ResourceOption>();
        public bool Ready { get { return Problem.Length == 0; } }

        static bool SafeName(string value)
        {
            return value.Length > 0 && value.Length < 64 &&
                !value.Any(c => c < 33 || c > 126 || "=;#[]:/\\\"'".Contains(c));
        }
        static string DisplayName(string value, string fallback)
        {
            string remaining = value.Trim(); int comma = remaining.IndexOf(',');
            if (comma < 0) return fallback;
            string first = remaining.Substring(0, comma).Trim();
            int slot;
            if (Int32.TryParse(first, NumberStyles.None, CultureInfo.InvariantCulture, out slot) || first.Equals("auto", StringComparison.OrdinalIgnoreCase))
            {
                remaining = remaining.Substring(comma + 1).Trim(); comma = remaining.IndexOf(',');
                if (comma < 0) return fallback;
            }
            string display = remaining.Substring(comma + 1).Trim();
            if(display.Length>128)display=display.Substring(0,128);
            return display.Length == 0 ? fallback : display;
        }
        public static ResourceRegistry Parse(string text, string path, CollectionSpec collection)
        { return Parse(text, path, collection.SourceSection, collection.SourceReadySection, collection.SourceReadyKey, collection.SourceReadyValue); }
        public static ResourceRegistry Load(string build, CollectionSpec collection)
        { return Load(build, collection.SourcePlugin, collection.SourceSection, collection.SourceReadySection, collection.SourceReadyKey, collection.SourceReadyValue); }
        // Bare form for editors that name their source directly (keyed_list [source]).
        public static ResourceRegistry Parse(string text, string path, string sourceSection, string readySection, string readyKey, string readyValue)
        {
            var collection = new CollectionSpec { SourceSection = sourceSection, SourceReadySection = readySection ?? "", SourceReadyKey = readyKey ?? "", SourceReadyValue = readyValue ?? "" };
            var result = new ResourceRegistry { Path = path, Hash = SafeFiles.Hash(SafeFiles.Utf8.GetBytes(text)) };
            var used = new HashSet<string>(StringComparer.OrdinalIgnoreCase); string section = ""; string ready = null;
            string[] lines = text.Replace("\r\n", "\n").Split('\n');
            for (int i = 0; i < lines.Length; ++i)
            {
                string line = lines[i].Trim();
                if(line.Length>4096)throw new FormatException(Msg.Key("err_zeile_zeile_ist_zu", System.IO.Path.GetFileName(path), (i+1)));
                if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#")) continue;
                if (line.StartsWith("[") && line.EndsWith("]") && line.Length > 2)
                { section = line.Substring(1, line.Length - 2).Trim(); continue; }
                int eq = line.IndexOf('=');
                if (eq <= 0) continue;
                string key = line.Substring(0, eq).Trim(), value = line.Substring(eq + 1).Trim();
                if (collection.SourceReadySection.Length>0&&section.Equals(collection.SourceReadySection,StringComparison.OrdinalIgnoreCase)&&key.Equals(collection.SourceReadyKey,StringComparison.OrdinalIgnoreCase))ready=value;
                if (!section.Equals(collection.SourceSection, StringComparison.OrdinalIgnoreCase)) continue;
                if (!SafeName(key)) throw new FormatException(Msg.Key("err_zeile_ungueltige_eintrags_id", System.IO.Path.GetFileName(path), (i + 1), collection.SourceSection, key));
                if (!used.Add(key)) throw new FormatException(Msg.Key("err_zeile_doppelte_eintrags_id", System.IO.Path.GetFileName(path), (i + 1), collection.SourceSection, key));
                result.Options.Add(new ResourceOption { Id = key, Display = DisplayName(value, key) });
            }
            if (result.Options.Count == 0) result.Problem = System.IO.Path.GetFileName(path)+" enthaelt keine auswaehlbaren Eintraege in ["+collection.SourceSection+"].";
            else if (collection.SourceReadySection.Length>0&&ready != collection.SourceReadyValue) result.Problem = System.IO.Path.GetFileName(path)+" muss ["+collection.SourceReadySection+"] "+collection.SourceReadyKey+" = "+collection.SourceReadyValue+" verwenden.";
            return result;
        }
        public static ResourceRegistry Load(string build, string sourcePlugin, string sourceSection, string readySection, string readyKey, string readyValue)
        {
            var result = new ResourceRegistry();
            try
            {
                string root = System.IO.Path.GetFullPath(build);
                // 0.5.11: a fork with a file of its own (resources_plus) is the source when it is
                // installed; its INI is created from the player's list if it does not exist yet.
                foreach (string fork in ContentTargets.PluginsFor(sourcePlugin))
                {
                    if (fork.Equals(sourcePlugin, StringComparison.OrdinalIgnoreCase)) break;
                    if (!File.Exists(SafeFiles.Child(root, "plugins\\"+fork+".dll"))) continue;
                    if (!File.Exists(SafeFiles.Child(root, "plugins\\"+fork+".ini"))) ForkSeed.Ensure(root, fork, fork+".ini");
                    if (File.Exists(SafeFiles.Child(root, "plugins\\"+fork+".ini"))) { sourcePlugin = fork; break; }
                }
                string dll = SafeFiles.Child(root, "plugins\\"+sourcePlugin+".dll"), ini = SafeFiles.Child(root, "plugins\\"+sourcePlugin+".ini");
                result.Path = ini;
                // 0.5.2: Soviet Mod Loader carries resources, deposits, needs and buildings inside
                // itself. There is no DLL of their own then, and their own key in tesmioloader.ini
                // is off on purpose - the generated INI in plugins\ is still the list that counts.
                bool sml = Sml.Hosts(root, sourcePlugin);
                if (!sml && !File.Exists(dll)) { result.Problem = "Benoetigte lokale Datei fehlt: plugins\\"+sourcePlugin+".dll"; return result; }
                if (!File.Exists(ini)) { result.Problem = "Benoetigte lokale Datei fehlt: plugins\\"+sourcePlugin+".ini"; return result; }
                byte[] bytes = SafeFiles.Read(ini, 4 * 1024 * 1024); string text = SafeFiles.Decode(bytes);
                result = Parse(text, ini, sourceSection, readySection, readyKey, readyValue); result.Hash = SafeFiles.Hash(bytes);
                string loader = SafeFiles.Child(root, "tesmioloader.ini");
                if (result.Ready && !sml && File.Exists(loader) && new Ini(SafeFiles.Text(loader)).Get("plugins", sourcePlugin, "1") == "0")
                    result.Problem = sourcePlugin+" ist im vorhandenen TesmioLauncher deaktiviert.";
            }
            catch (Exception e) { result.Problem = "Quellkatalog konnte nicht sicher gelesen werden: " + e.Message; }
            return result;
        }
        public ResourceOption Find(string id) { return Options.FirstOrDefault(x => x.Id.Equals(id, StringComparison.OrdinalIgnoreCase)); }
    }
}
