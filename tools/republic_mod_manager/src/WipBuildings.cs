using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TesmioAutoload
{
    // A folder under media_soviet\workshop_wip. The game reads these like a subscription, but
    // nothing in there came from Steam: a plugin wrote it on this machine - Buildings Plus,
    // Soviet Mod Loader's built-in buildings component, or the game's own editor. That is why
    // this is the one Workshop-shaped place RMM may look into and, on request, put right.
    public sealed class WipBuilding
    {
        public string Id = "", Folder = "", Object = "", Name = "", Section = "", Owner = "", Donor = "";
        public string Origin = "other";   // buildings_plus | sml | editor | other
        public bool Generated;            // ten digits starting with 9 - written, never subscribed
        public bool SmlRange;             // 9100000000..9199999999, reserved by Soviet Mod Loader
        public bool NoStamp;              // no tesmioloader.stamp: the editor's, or hand-built
        public bool OwnerMissing;         // $OWNER_ID 0 - the game reports it as a missing item
        public bool Orphan;               // no package declares its section any more
        public string Display { get { return Name.Length > 0 ? Name : Object.Length > 0 ? Object : Id; } }
    }

    public static class WipBuildings
    {
        // The two stamp texts that say who generated a folder. Buildings Plus writes its own;
        // Soviet Mod Loader ships the original plugin and keeps its wording.
        public const string OwnMark = "buildings_plus generated this folder";
        public const string LoaderMark = "tesmioloader plugins\\buildings.dll generated this folder";
        public const long SmlMinId = 9100000000L, SmlMaxId = 9199999999L;
        public const long MinId = 9000000000L, MaxId = 9999999999L;

        public static string Root(string build)
        {
            string game = GameBuildings.GameRoot(build);
            return game == null ? null : Path.Combine(game, "media_soviet", "workshop_wip");
        }
        // A generated id is ten digits starting with 9. Steam's own published file ids are far
        // below that, so a folder named this way was written here and is not a subscription.
        public static bool IsGeneratedId(string name)
        {
            if (name == null || name.Length != 10 || name[0] != '9') return false;
            long id;
            return Int64.TryParse(name, out id) && id >= MinId && id <= MaxId;
        }
        public static bool IsSmlId(string name)
        {
            long id;
            return IsGeneratedId(name) && Int64.TryParse(name, out id) && id >= SmlMinId && id <= SmlMaxId;
        }

        // range: "sml" the buildings Soviet Mod Loader generates, "own" the ones Buildings Plus
        // wrote, "all" every folder. The list is sorted by what the player sees, the name.
        public static List<WipBuilding> Scan(string build, string range)
        {
            var found = new List<WipBuilding>();
            string root = Root(build);
            if (root == null || !Directory.Exists(root)) return found;
            HashSet<string> declared = null;
            string[] dirs;
            try { dirs = Directory.GetDirectories(root); }
            catch (Exception) { return found; }
            foreach (string dir in dirs)
            {
                WipBuilding entry = Read(dir);
                if (!Wanted(entry, range)) continue;
                if (entry.Origin == "sml" && entry.Section.Length > 0)
                {
                    if (declared == null) declared = DeclaredSections(build);
                    entry.Orphan = !declared.Contains(entry.Section);
                }
                found.Add(entry);
            }
            return found.OrderBy(x => x.Display, StringComparer.CurrentCultureIgnoreCase).ToList();
        }
        static bool Wanted(WipBuilding entry, string range)
        {
            if (range == "all") return true;
            if (range == "own") return entry.Origin == "buildings_plus";
            return entry.Origin == "sml" || (entry.SmlRange && entry.Origin != "buildings_plus");
        }

        public static WipBuilding Read(string dir)
        {
            var entry = new WipBuilding { Folder = dir, Id = Path.GetFileName(dir) };
            entry.Generated = IsGeneratedId(entry.Id);
            entry.SmlRange = IsSmlId(entry.Id);
            string stamp = TextOf(Path.Combine(dir, "tesmioloader.stamp"));
            if (stamp == null) { entry.NoStamp = true; entry.Origin = "editor"; }
            else
            {
                entry.Origin = stamp.IndexOf(OwnMark, StringComparison.OrdinalIgnoreCase) >= 0 ? "buildings_plus"
                             : stamp.IndexOf(LoaderMark, StringComparison.OrdinalIgnoreCase) >= 0 ? "sml" : "other";
                Match section = Regex.Match(stamp, @"section=([^\s]+)");
                if (section.Success) entry.Section = section.Groups[1].Value;
                // donor= and object= are in there too. They let the card list say "will be rebuilt
                // at the next start" without recomputing the generator's hash, which covers the
                // size and time of the donor file as well and cannot be repeated here.
                Match donor = Regex.Match(stamp, @"donor=([^\s]+)");
                if (donor.Success) entry.Donor = donor.Groups[1].Value;
                Match stamped = Regex.Match(stamp, @"object=([^\s]+)");
                if (stamped.Success && entry.Object.Length == 0) entry.Object = stamped.Groups[1].Value;
            }
            string config = TextOf(Path.Combine(dir, "workshopconfig.ini"));
            if (config != null)
            {
                Match owner = Regex.Match(config, @"(?m)^\$OWNER_ID\s+(\d+)\s*$");
                if (owner.Success) { entry.Owner = owner.Groups[1].Value; entry.OwnerMissing = entry.Owner == "0"; }
                Match obj = Regex.Match(config, @"(?m)^\$OBJECT_BUILDING\s+(\S+)\s*$");
                if (obj.Success) entry.Object = obj.Groups[1].Value;
                Match name = Regex.Match(config, "(?m)^\\$ITEM_NAME\\s+\"([^\"]*)\"");
                if (name.Success) entry.Name = name.Groups[1].Value.Trim();
            }
            if (entry.Object.Length == 0)
            {
                try
                {
                    string[] subs = Directory.GetDirectories(dir);
                    if (subs.Length == 1) entry.Object = Path.GetFileName(subs[0]);
                }
                catch (Exception) { }
            }
            if (entry.Name.Length == 0 && entry.Object.Length > 0)
            {
                string building = TextOf(Path.Combine(dir, entry.Object, "building.ini"));
                if (building != null)
                {
                    Match caption = Regex.Match(building, "(?m)^\\$NAME_STR\\s+\"([^\"]*)\"");
                    if (caption.Success) entry.Name = caption.Groups[1].Value.Trim();
                }
            }
            return entry;
        }

        // Every building section a package or Soviet Mod Loader's own baseline declares right
        // now. SML compares the folders of its id range against exactly this and CLOSES THE GAME
        // when it finds one nothing declares any more, so a stale folder is worth a warning
        // before the player runs into a message box with no way out.
        public static HashSet<string> DeclaredSections(string build)
        {
            var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            Collect(names, Path.Combine(Sml.StateDir(build), "base", "buildings.ini"));
            string game = GameBuildings.GameRoot(build);
            string workshop = game == null ? null : GameBuildings.SteamWorkshopFor(game);
            if (workshop == null || !Directory.Exists(workshop)) return names;
            string[] dirs;
            try { dirs = Directory.GetDirectories(workshop); }
            catch (Exception) { return names; }
            foreach (string dir in dirs)
            {
                string manifest = Path.Combine(dir, "soviet.mod.ini");
                if (!File.Exists(manifest)) continue;
                string relative = "tesmio\\buildings.ini";
                try
                {
                    string value = new LooseIni(SafeFiles.Text(manifest)).Get("content", "buildings");
                    if (!String.IsNullOrWhiteSpace(value)) relative = GenericSchema.StripComment(value).Trim();
                }
                catch (Exception) { }
                try { Collect(names, SafeFiles.Child(dir, relative)); }
                catch (Exception) { }
            }
            return names;
        }
        static void Collect(HashSet<string> names, string file)
        {
            string text = TextOf(file);
            if (text == null) return;
            LooseIni ini;
            try { ini = new LooseIni(text); }
            catch (Exception) { return; }
            foreach (Match m in Regex.Matches(text, @"(?m)^\s*\[([^\]\r\n]+)\]"))
            {
                string name = m.Groups[1].Value.Trim();
                if (name.Length == 0 || name.Equals("buildings", StringComparison.OrdinalIgnoreCase)
                    || name.Equals("buildings_plus", StringComparison.OrdinalIgnoreCase)) continue;
                try
                {
                    if (String.IsNullOrWhiteSpace(ini.Get(name, "donor"))) continue;
                    string enabled = ini.Get(name, "enabled");
                    if (enabled != null && GenericSchema.StripComment(enabled).Trim() == "0") continue;
                }
                catch (Exception) { }
                names.Add(name);
            }
        }

        static string TextOf(string path)
        {
            try { return File.Exists(path) ? SafeFiles.Text(path) : null; }
            catch (Exception) { return null; }
        }

        // The one write this offers: the missing owner id, and nothing else. The file belongs to
        // another generator, so the number is spliced in at byte level and every other byte -
        // line endings included - stays as it was. Returns false when there is nothing to do.
        // The stamp is never touched: Soviet Mod Loader closes the game over a missing one.
        public static bool FixOwner(string folder, string owner)
        {
            if (String.IsNullOrEmpty(owner) || owner == "0" || !Regex.IsMatch(owner, "^[0-9]{1,20}$")) return false;
            string path = Path.Combine(folder, "workshopconfig.ini");
            if (!File.Exists(path)) return false;
            byte[] bytes;
            try { bytes = File.ReadAllBytes(path); }
            catch (Exception) { return false; }
            if (bytes.Length > 256 * 1024) return false;
            byte[] needle = Encoding.ASCII.GetBytes("$OWNER_ID 0");
            int at = -1;
            for (int i = 0; i + needle.Length <= bytes.Length && at < 0; i++)
            {
                if (i != 0 && bytes[i - 1] != (byte)'\n' && bytes[i - 1] != (byte)'\r') continue;
                bool hit = true;
                for (int k = 0; k < needle.Length && hit; k++) if (bytes[i + k] != needle[k]) hit = false;
                if (!hit) continue;
                int end = i + needle.Length;
                if (end != bytes.Length && bytes[end] != (byte)'\r' && bytes[end] != (byte)'\n') continue;
                at = i;
            }
            if (at < 0) return false;
            byte[] inserted = Encoding.ASCII.GetBytes("$OWNER_ID " + owner);
            var result = new List<byte>(bytes.Length + inserted.Length);
            for (int i = 0; i < at; i++) result.Add(bytes[i]);
            result.AddRange(inserted);
            for (int i = at + needle.Length; i < bytes.Length; i++) result.Add(bytes[i]);
            string temp = path + ".rmm.tmp";
            try
            {
                File.WriteAllBytes(temp, result.ToArray());
                if (File.Exists(path)) File.Replace(temp, path, null);
                else File.Move(temp, path);
                return true;
            }
            catch (Exception)
            {
                try { if (File.Exists(temp)) File.Delete(temp); } catch (Exception) { }
                throw;
            }
        }

        // The SteamID64 of whoever is signed in here, the same two sources the plugin uses:
        // the account id in the registry plus the base, otherwise the most recent account in
        // loginusers.vdf. Empty when neither answers - a wrong owner is worse than none.
        // Set by the tests, so a check on the fill-in button does not depend on whether a Steam
        // client happens to be signed in on the machine running them.
        public static string TestOwner;
        public static string Owner()
        {
            const long Base = 76561197960265728L;
            if (!String.IsNullOrEmpty(TestOwner)) return TestOwner;
            try
            {
                object user = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam\ActiveProcess", "ActiveUser", null);
                long account = user == null ? 0 : Convert.ToInt64(user);
                if (account > 0) return (Base + account).ToString();
            }
            catch (Exception) { }
            try
            {
                object path = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null);
                string steam = path == null ? null : Convert.ToString(path);
                if (String.IsNullOrWhiteSpace(steam)) return "";
                string file = Path.Combine(steam.Replace('/', '\\'), "config", "loginusers.vdf");
                if (!File.Exists(file)) return "";
                string text = SafeFiles.Text(file);
                string first = "";
                foreach (Match m in Regex.Matches(text, "\"(7656\\d{13})\"\\s*\\{(.*?)\\n\\s*\\}", RegexOptions.Singleline))
                {
                    if (first.Length == 0) first = m.Groups[1].Value;
                    if (Regex.IsMatch(m.Groups[2].Value, "\"MostRecent\"\\s*\"1\"")) return m.Groups[1].Value;
                }
                return first;
            }
            catch (Exception) { return ""; }
        }
    }
}
