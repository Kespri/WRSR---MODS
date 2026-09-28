// Republic Mod Manager - reference checks (0.4.40): values that must name something
// the game knows. A plugin that finds an unknown research id, building file or text
// id at game start rejects the entry or the whole extension, and the player only
// learns it from the log. The schema names the set (`reference = game_research`,
// `id_reference = game_texts`) and the session validates against it before saving.
//
// Sets: game_research (active blocks of media_soviet\research\research.ini),
// game_buildings (a building file relative to media_soviet or the Workshop folder),
// game_texts (the ids of the game language's .btf). Each set is read once per
// session; a set that cannot be read (no game folder next to the loader) is
// treated as "everything is known" so a broken setup never blocks the editor.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    public sealed class ReferenceSets
    {
        // resources (0.4.41): the base game's 57 resource names plus what plugins\resources.ini
        // registers ([list] with hook = 2) - the names a plugin can resolve at world load.
        // game_donor (0.4.80): a base-game building a clone can start from - a plain name under
        // media_soviet\buildings_types, which must have its .ini there.
        public static readonly string[] Names = { "game_research", "game_buildings", "game_texts", "resources", "game_donor" };
        public static bool Known(string name) { return Names.Contains(name); }

        readonly string build, workshopRoot, game, code;
        HashSet<string> research; HashSet<int> texts; bool researchRead, textsRead, languageRead; Language language;

        public ReferenceSets(string build, string workshopRoot, string uiLanguage)
        {
            this.build = build; this.workshopRoot = workshopRoot ?? "";
            game = GameBuildings.GameRoot(build);
            code = String.IsNullOrEmpty(uiLanguage) ? "en" : uiLanguage;
        }

        public string Game { get { return game; } }
        public string Code { get { return code; } }

        // The UI language for field labels in messages; null where the application's
        // language resources are not embedded (the core test runner).
        public Language Language
        {
            get
            {
                if (!languageRead) { languageRead = true; try { language = new Language(code); } catch (Exception) { language = null; } }
                return language;
            }
        }

        // True when the id is in the set or the set cannot be read.
        public bool Contains(string set, string id)
        {
            if (String.IsNullOrWhiteSpace(id)) return true;
            id = id.Trim();
            switch (set)
            {
                case "game_research": { var ids = Research(); return ids == null || ids.Contains(id); }
                case "game_texts": { var ids = Texts(); int number; return ids == null || Int32.TryParse(id, NumberStyles.None, CultureInfo.InvariantCulture, out number) && ids.Contains(number); }
                case "game_buildings": return BuildingFile(id);
                case "game_donor": return game == null || GameBuildings.DonorFile(build, id) != null;
                case "resources": return Resources().Contains(id);
                default: return true;
            }
        }

        HashSet<string> resources;
        HashSet<string> Resources()
        {
            if (resources != null) return resources;
            resources = new HashSet<string>(ResourceCatalogData.Templates.Select(t => t.Name), StringComparer.OrdinalIgnoreCase);
            try
            {
                var registry = ResourceRegistry.Load(build, "resources", "list", "resources", "hook", "2");
                if (registry.Ready) foreach (var option in registry.Options) resources.Add(option.Id);
            }
            catch (Exception) { }
            return resources;
        }

        HashSet<string> Research()
        {
            if (researchRead) return research;
            researchRead = true;
            try { if (game != null && File.Exists(GameResearch.FilePath(game))) research = new HashSet<string>(GameResearch.Scan(game, null).Select(x => x.Id), StringComparer.OrdinalIgnoreCase); }
            catch (Exception) { research = null; }
            return research;
        }

        HashSet<int> Texts()
        {
            if (textsRead) return texts;
            textsRead = true;
            try
            {
                string gameLanguage = game == null ? null : GameTexts.DefaultLanguage(game, code);
                if (gameLanguage != null && File.Exists(GameTexts.FilePath(game, gameLanguage))) texts = new HashSet<int>(GameTexts.Load(game, gameLanguage).Select(x => x.Id));
            }
            catch (Exception) { texts = null; }
            return texts;
        }

        // A target file the way Vanilla Buildings names it: relative to media_soviet
        // (buildings_types\x.ini, dlcN\buildings\...\building.ini) or to the Workshop
        // folder (<item id>\...\building.ini). The manager's own package root counts
        // too, because under a local test setup the items live there.
        bool BuildingFile(string target)
        {
            if (game == null) return true;
            return GameBuildings.TargetFile(build, workshopRoot, target) != null;
        }

        // The ids a field value refers to, by the field's reference_format:
        // id / file = the whole line; requires = "<parent> | before/after | <anchor>";
        // directive:$X = the first word after $X in every "|"-separated part.
        public static IEnumerable<string> IdsOf(LocalDetailField field, string value)
        {
            var result = new List<string>();
            if (String.IsNullOrWhiteSpace(value)) return result;
            foreach (string raw in value.Replace("\r", "").Split('\n'))
            {
                string line = raw.Trim();
                if (line.Length == 0) continue;
                switch (field.ReferenceFormat)
                {
                    case "requires":
                    {
                        string[] parts = line.Split('|').Select(x => x.Trim()).ToArray();
                        if (parts[0].Length > 0) result.Add(FirstWord(parts[0]));
                        if (parts.Length >= 3 && parts[2].Length > 0) result.Add(FirstWord(parts[2]));
                        break;
                    }
                    case "directive":
                    {
                        string directive = field.ReferenceDirective;
                        foreach (string part in line.Split('|'))
                        {
                            string p = part.Trim();
                            if (p.Length > directive.Length && p.StartsWith(directive, StringComparison.OrdinalIgnoreCase) && Char.IsWhiteSpace(p[directive.Length]))
                            {
                                string rest = p.Substring(directive.Length).Trim();
                                if (rest.Length > 0) result.Add(FirstWord(rest));
                            }
                        }
                        break;
                    }
                    default: result.Add(line); break;
                }
            }
            return result;
        }

        // 0.4.48: header check of a DDS file named by a picker = files field. "dds_dxt1" / "dds_dxt5" want
        // that compression, a square power-of-two side from 256 to 4096 and a complete mip chain - the
        // rules the Deposits Plus tile loader applies. Returns null when fine, else the message suffix.
        public static string DdsProblem(string path, string format)
        {
            if (format != "dds_dxt1" && format != "dds_dxt5") return null;
            byte[] h = new byte[128];
            try { using (var f = File.OpenRead(path)) { if (f.Read(h, 0, 128) < 128) return "not_dds"; } } catch (Exception) { return "not_dds"; }
            if (h[0] != 'D' || h[1] != 'D' || h[2] != 'S' || h[3] != ' ' || BitConverter.ToInt32(h, 4) != 124) return "not_dds";
            int height = BitConverter.ToInt32(h, 12), width = BitConverter.ToInt32(h, 16), mips = BitConverter.ToInt32(h, 28);
            string fourcc = System.Text.Encoding.ASCII.GetString(h, 84, 4);
            if (fourcc != (format == "dds_dxt1" ? "DXT1" : "DXT5")) return format == "dds_dxt1" ? "dxt1" : "dxt5";
            if (width != height || width < 256 || width > 4096 || (width & (width - 1)) != 0) return "size";
            int levels = 0; for (int s = width; s > 0; s >>= 1) levels++;
            if (mips != levels) return "mips";
            return null;
        }

        static string FirstWord(string text) { int i = text.IndexOfAny(new[] { ' ', '\t' }); return i < 0 ? text : text.Substring(0, i); }
    }
}
