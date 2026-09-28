using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TesmioAutoload
{
    // One building definition the game can load, in the form a Vanilla Buildings
    // rule set names it: buildings_types\<file>.ini, dlcN\buildings\<folder>\building.ini
    // or <workshop id>\<folder>\building.ini.
    public sealed class BuildingEntry
    {
        public string Target = "", Name = "", Type = "", Origin = "", OriginLabel = "";
        public bool Obsolete;
        // Sort key: game first, then DLC by number, then Workshop by item name.
        public int OriginRank { get { return Origin == "game" ? 0 : Origin.StartsWith("dlc", StringComparison.Ordinal) ? 1 : 2; } }
    }

    // Reads the building definitions of the installed game, its DLCs and the
    // subscribed Workshop items. Only the head of each file is read: the first
    // $TYPE_ token names the kind, $OBSOLETE marks a superseded version.
    public static class GameBuildings
    {
        const int HeadBytes = 64 * 1024;
        static readonly Regex TypeToken = new Regex(@"^\s*\$TYPE_([A-Z0-9_]+)", RegexOptions.Multiline);
        static readonly Regex ObsoleteToken = new Regex(@"^\s*\$OBSOLETE\b", RegexOptions.Multiline);
        static readonly Regex ItemName = new Regex("\\$ITEM_NAME\\s+\"([^\"]*)\"");

        // The game folder: walks up from the loader folder until media_soviet is found.
        // 0.4.80: a clone's donor is a plain name under media_soviet\buildings_types; this is its
        // building.ini path, and the lines the player can take over from it.
        // 0.4.86: a donor is either that plain name or one building of a Workshop item,
        // written <item>\<object> - the same spelling the target files use. The item is
        // looked for in the Steam Workshop folder and in media_soviet\workshop_wip, the
        // two places the plugin itself searches.
        public static string DonorFile(string build, string donor)
        {
            string game = GameRoot(build);
            if (game == null || String.IsNullOrWhiteSpace(donor)) return null;
            donor = donor.Trim().Replace('/', '\\');
            if (Regex.IsMatch(donor, "^[A-Za-z0-9._-]{1,64}$"))
            {
                string plain = Path.Combine(game, "media_soviet", "buildings_types", donor + ".ini");
                return File.Exists(plain) ? plain : null;
            }
            if (!Regex.IsMatch(donor, @"^[A-Za-z0-9._+()\- ]{1,96}\\[A-Za-z0-9._+()\- ]{1,96}$") || donor.Contains("..")) return null;
            foreach (string root in new[] { SteamWorkshopFor(game), Path.Combine(game, "media_soviet", "workshop_wip") })
            {
                if (String.IsNullOrEmpty(root)) continue;
                string path;
                try { path = Path.Combine(root, donor, "building.ini"); }
                catch (ArgumentException) { return null; }
                if (File.Exists(path)) return path;
            }
            return null;
        }
        // A target file of a Vanilla Buildings rule set: a path relative to media_soviet, to the
        // Steam Workshop folder or to the package folder. One implementation, used by the reference
        // check and by the line editor, so both agree on what "the file is there" means.
        public static string TargetFile(string build, string workshopRoot, string target)
        {
            string game = GameRoot(build);
            if (game == null || String.IsNullOrWhiteSpace(target)) return null;
            target = target.Trim().Replace('/', '\\');
            if (target.Contains("..") || target.Contains(":") || target.StartsWith("\\")) return null;
            var roots = new List<string> { Path.Combine(game, "media_soviet") };
            string steam = SteamWorkshopFor(game); if (steam != null) roots.Add(steam);
            if (!String.IsNullOrEmpty(workshopRoot)) roots.Add(workshopRoot);
            foreach (string root in roots)
            {
                try { string path = Path.Combine(root, target); if (File.Exists(path)) return path; }
                catch (Exception) { }
            }
            return null;
        }
        public static List<string> TargetLines(string build, string workshopRoot, string target)
        {
            string path = TargetFile(build, workshopRoot, target);
            if (path == null) return null;
            try { return SafeFiles.Text(path).Replace("\r\n", "\n").Split('\n').ToList(); }
            catch (Exception) { return null; }
        }
        public static List<string> DonorLines(string build, string donor)
        {
            var result = new List<string>(); string path = DonorFile(build, donor);
            if (path == null) return result;
            try
            {
                foreach (string raw in SafeFiles.Text(path).Replace("\r\n", "\n").Split('\n'))
                {
                    string line = raw.Trim();
                    // A building.ini has no comment syntax; a line without a $TOKEN is a data line
                    // of the token above it or one of the dashed separators the files are full of.
                    if (line.Length == 0 || line.Length > 4096 || line.IndexOf('$') < 0) continue;
                    if (!result.Contains(line, StringComparer.Ordinal)) result.Add(line);
                }
            }
            catch (Exception) { }
            return result;
        }
        public static string GameRoot(string build)
        {
            try
            {
                string dir = Path.GetFullPath(build);
                for (int i = 0; i < 5 && dir != null; i++)
                {
                    if (Directory.Exists(Path.Combine(dir, "media_soviet"))) return dir;
                    dir = Path.GetDirectoryName(dir);
                }
            }
            catch (Exception) { }
            return null;
        }

        public static List<BuildingEntry> Scan(string build, string workshopRoot)
        {
            var result = new List<BuildingEntry>();
            string game = GameRoot(build);
            if (game != null)
            {
                string media = Path.Combine(game, "media_soviet");
                string types = Path.Combine(media, "buildings_types");
                if (Directory.Exists(types))
                    foreach (string file in Directory.GetFiles(types, "*.ini", SearchOption.TopDirectoryOnly).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                        Add(result, file, "buildings_types\\" + Path.GetFileName(file), Path.GetFileNameWithoutExtension(file), "game", "");
                foreach (string dlc in Directory.GetDirectories(media).Where(d => Regex.IsMatch(Path.GetFileName(d), "^dlc\\d+$", RegexOptions.IgnoreCase)).OrderBy(d => Number(Path.GetFileName(d))))
                {
                    string buildings = Path.Combine(dlc, "buildings");
                    if (!Directory.Exists(buildings)) continue;
                    string dlcName = Path.GetFileName(dlc).ToLowerInvariant();
                    foreach (string file in Directory.GetFiles(buildings, "building.ini", SearchOption.AllDirectories).OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                        Add(result, file, dlcName + "\\buildings\\" + Relative(buildings, file), Path.GetFileName(Path.GetDirectoryName(file)), dlcName, Number(dlcName).ToString());
                }
            }
            // Workshop items: the game's own Steam Workshop folder first (rule sets name their
            // targets by item id, and the plugin resolves them there), then the manager's
            // package folder when it is a different place with numbered items (0.4.24).
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            string steam = game != null ? SteamWorkshopFor(game) : null;
            if (steam != null) ScanWorkshop(result, steam, seen);
            if (!String.IsNullOrWhiteSpace(workshopRoot) && Directory.Exists(workshopRoot)) ScanWorkshop(result, workshopRoot, seen);
            return result;
        }

        // <library>\steamapps\workshop\content\784150, derived from a game root that sits
        // under <library>\steamapps\common; null when the game is not in a Steam library.
        public static string SteamWorkshopFor(string game)
        {
            try
            {
                string dir = Path.GetFullPath(game);
                for (int i = 0; i < 4 && dir != null; i++)
                {
                    if (Path.GetFileName(dir).Equals("steamapps", StringComparison.OrdinalIgnoreCase))
                    {
                        string content = Path.Combine(dir, "workshop", "content", "784150");
                        return Directory.Exists(content) ? content : null;
                    }
                    dir = Path.GetDirectoryName(dir);
                }
            }
            catch (Exception) { }
            return null;
        }

        static void ScanWorkshop(List<BuildingEntry> result, string workshopRoot, HashSet<string> seen)
        {
            {
                foreach (string item in Directory.GetDirectories(workshopRoot).Where(d => Regex.IsMatch(Path.GetFileName(d), "^\\d+$")).OrderBy(d => Path.GetFileName(d), StringComparer.Ordinal))
                {
                    string id = Path.GetFileName(item); string label = id;
                    if (!seen.Add(id)) continue;
                    string config = Path.Combine(item, "workshopconfig.ini");
                    try { if (File.Exists(config)) { var match = ItemName.Match(File.ReadAllText(config, Encoding.UTF8)); if (match.Success && match.Groups[1].Value.Trim().Length > 0) label = match.Groups[1].Value.Trim(); } }
                    catch (Exception) { }
                    string[] files;
                    try { files = Directory.GetFiles(item, "building.ini", SearchOption.AllDirectories); } catch (Exception) { continue; }
                    foreach (string file in files.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
                        Add(result, file, id + "\\" + Relative(item, file), Path.GetFileName(Path.GetDirectoryName(file)), "workshop:" + id, label);
                }
            }
        }
        static int Number(string dlc) { int n; return Int32.TryParse(Regex.Replace(dlc, "[^0-9]", ""), out n) ? n : 0; }
        static string Relative(string root, string file) { return file.Substring(Path.GetFullPath(root).TrimEnd('\\').Length + 1); }
        static void Add(List<BuildingEntry> list, string file, string target, string name, string origin, string originLabel)
        {
            string head;
            try
            {
                using (var stream = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                {
                    var buffer = new byte[Math.Min(HeadBytes, (int)Math.Min(stream.Length, Int32.MaxValue))];
                    int read = 0; while (read < buffer.Length) { int n = stream.Read(buffer, read, buffer.Length - read); if (n <= 0) break; read += n; }
                    head = Encoding.UTF8.GetString(buffer, 0, read);
                }
            }
            catch (Exception) { return; }
            var type = TypeToken.Match(head);
            list.Add(new BuildingEntry { Target = target, Name = name, Type = type.Success ? type.Groups[1].Value : "", Origin = origin, OriginLabel = originLabel, Obsolete = ObsoleteToken.IsMatch(head) });
        }
    }
}
