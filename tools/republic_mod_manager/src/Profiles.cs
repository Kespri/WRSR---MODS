using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TesmioAutoload
{
    // Wish 6, profiles: a named copy of the whole configuration layer of a loader
    // folder - tesmioloader.ini, plugins\*.ini, user_config\*.ini and the receipts
    // and protected originals under user_config\.autoload. Never a DLL, never the
    // rolling backups. Applying a profile goes through the same transaction as a
    // save, so the state before it becomes a restore point.
    public static class ConfigSet
    {
        public const int FileLimit = 4 * 1024 * 1024;
        static readonly string[] Folders = { "plugins", "user_config", "user_config\\.autoload" };
        // Relative paths (backslashes) of every configuration file the profile covers, sorted.
        public static List<string> Files(string build)
        {
            var list = new List<string>();
            string root = Path.GetFullPath(build);
            if (File.Exists(Path.Combine(root, "tesmioloader.ini"))) list.Add("tesmioloader.ini");
            foreach (string folder in Folders)
            {
                string directory = Path.Combine(root, folder);
                if (!Directory.Exists(directory)) continue;
                foreach (string file in Directory.GetFiles(directory, "*.ini", SearchOption.TopDirectoryOnly))
                    list.Add(folder + "\\" + Path.GetFileName(file));
            }
            list.Sort(StringComparer.OrdinalIgnoreCase);
            return list;
        }
        // The overlay layer: files a profile may remove because the profile did not
        // have them. plugins\*.ini stays - it may belong to a plugin installed later.
        public static bool Removable(string relative)
        {
            return relative.StartsWith("user_config\\", StringComparison.OrdinalIgnoreCase);
        }
    }

    public sealed class Profile
    {
        public string Name = "", Folder = "", Note = "";
        public DateTime Created;
        public readonly List<KeyValuePair<string, string>> Files = new List<KeyValuePair<string, string>>();   // relative -> hash
        public string Path(string relative) { return System.IO.Path.Combine(Folder, "files", relative); }
    }

    public sealed class ProfileDifference
    {
        public string Relative = "", State = "";   // same | differs | missing_local | only_local
    }

    public static class Profiles
    {
        public const string Folder = "user_config\\.autoload\\profiles";
        public static string Root(string build) { return SafeFiles.Child(build, Folder); }
        public static bool ValidName(string name)
        {
            return name != null && Regex.IsMatch(name, "^[A-Za-z0-9À-ſ][A-Za-z0-9À-ſ _.()-]{0,39}$") && !name.EndsWith(".") && !name.EndsWith(" ");
        }
        public static List<Profile> List(string build)
        {
            var list = new List<Profile>();
            string root = Root(build);
            if (!Directory.Exists(root)) return list;
            foreach (string directory in Directory.GetDirectories(root))
            {
                try { Profile p = Load(directory); if (p != null) list.Add(p); }
                catch (Exception) { /* a damaged folder is not a profile */ }
            }
            list.Sort((a, b) => String.Compare(a.Name, b.Name, StringComparison.CurrentCultureIgnoreCase));
            return list;
        }
        public static Profile Find(string build, string name)
        {
            return List(build).FirstOrDefault(p => p.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
        }
        static Profile Load(string directory)
        {
            string meta = Path.Combine(directory, "profile.ini");
            if (!File.Exists(meta)) return null;
            var ini = new Ini(SafeFiles.Text(meta));
            if (ini.Get("profile", "format") != "1") return null;
            var profile = new Profile { Folder = directory, Name = ini.Get("profile", "name", Path.GetFileName(directory)), Note = ini.Get("profile", "note", "") };
            DateTime created;
            if (!DateTime.TryParseExact(ini.Get("profile", "created", ""), "yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture, DateTimeStyles.AssumeLocal, out created)) created = Directory.GetCreationTime(directory);
            profile.Created = created;
            foreach (var pair in ini.Values.Where(v => v.Key.StartsWith("files/", StringComparison.OrdinalIgnoreCase)).OrderBy(v => v.Key, StringComparer.OrdinalIgnoreCase))
            {
                int at = pair.Value.LastIndexOf('|');
                if (at < 1) throw new FormatException(Msg.Key("err_ungueltige_profilzeile", pair.Key));
                string relative = pair.Value.Substring(0, at).Trim(), hash = pair.Value.Substring(at + 1).Trim();
                SafeFiles.Child(directory, "files\\" + relative);   // validates the path
                profile.Files.Add(new KeyValuePair<string, string>(relative, hash));
            }
            return profile;
        }
        static string FolderName(string name)
        {
            var b = new StringBuilder();
            foreach (char c in name) b.Append(Char.IsLetterOrDigit(c) && c < 128 ? c : '_');
            return b.ToString() + "_" + SafeFiles.Hash(SafeFiles.Utf8.GetBytes(name.ToUpperInvariant())).Substring(0, 8);
        }
        // Copies the current configuration into a new profile folder. Fully written
        // into a staging folder first; the final name appears only when complete.
        public static Profile Save(string build, string name, string note, bool overwrite)
        {
            if (!ValidName(name)) throw new FormatException(Msg.Key("err_ungueltiger_profilname", name));
            if ((note ?? "").Any(Char.IsControl)) throw new FormatException(Msg.Key("err_ungueltige_notiz"));
            string root = Root(build); Directory.CreateDirectory(root);
            string target = Path.Combine(root, FolderName(name));
            Profile existing = Find(build, name);
            if (existing != null && !overwrite) throw new IOException(Msg.Key("err_profil_existiert_bereits", name));
            string staging = Path.Combine(root, "new_" + Guid.NewGuid().ToString("N").Substring(0, 8));
            Directory.CreateDirectory(Path.Combine(staging, "files"));
            var meta = new StringBuilder("[profile]\r\nformat = 1\r\nname = " + name + "\r\ncreated = " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss", CultureInfo.InvariantCulture) + "\r\n");
            if (!String.IsNullOrEmpty(note)) meta.Append("note = " + note + "\r\n");
            meta.Append("\r\n[files]\r\n");
            int index = 0;
            foreach (string relative in ConfigSet.Files(build))
            {
                string source = SafeFiles.Child(build, relative);
                byte[] bytes = SafeFiles.Read(source, ConfigSet.FileLimit);
                string copy = SafeFiles.Child(staging, "files\\" + relative);
                Directory.CreateDirectory(Path.GetDirectoryName(copy));
                File.WriteAllBytes(copy, bytes);
                if (SafeFiles.HashFile(copy) != SafeFiles.Hash(bytes)) throw new IOException(Msg.Key("err_kopie_stimmt_nicht_ueberein", relative));
                meta.Append("file." + index++ + " = " + relative + "|" + SafeFiles.Hash(bytes) + "\r\n");
            }
            File.WriteAllText(Path.Combine(staging, "profile.ini"), meta.ToString(), SafeFiles.Utf8);
            if (existing != null)
            {
                string retired = Path.Combine(root, "old_" + Guid.NewGuid().ToString("N").Substring(0, 8));
                Directory.Move(existing.Folder, retired);
                try { Directory.Move(staging, target); }
                catch { Directory.Move(retired, existing.Folder); throw; }
                DeleteTree(root, retired);
            }
            else Directory.Move(staging, target);
            return Load(target);
        }
        public static void Delete(string build, Profile profile)
        {
            DeleteTree(Root(build), profile.Folder);
        }
        // What applying the profile would change, file by file.
        public static List<ProfileDifference> Compare(string build, Profile profile)
        {
            var list = new List<ProfileDifference>();
            var covered = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in profile.Files)
            {
                covered.Add(pair.Key);
                string current = SafeFiles.HashFile(SafeFiles.Child(build, pair.Key));
                list.Add(new ProfileDifference { Relative = pair.Key, State = current == "absent" ? "missing_local" : current == pair.Value ? "same" : "differs" });
            }
            foreach (string relative in ConfigSet.Files(build))
                if (!covered.Contains(relative)) list.Add(new ProfileDifference { Relative = relative, State = ConfigSet.Removable(relative) ? "only_local" : "kept" });
            return list;
        }
        // Writes the profile's files back and removes overlay files the profile did
        // not have. One transaction under the id "profile" - its restore point holds
        // the state from before.
        public static string Apply(string build, Profile profile, Action guard)
        {
            var writes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            var before = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in profile.Files)
            {
                string source = profile.Path(pair.Key);
                byte[] bytes = SafeFiles.Read(source, ConfigSet.FileLimit);
                if (SafeFiles.Hash(bytes) != pair.Value) throw new IOException(Msg.Key("err_profildatei_wurde_veraendert", pair.Key));
                string target = SafeFiles.Child(build, pair.Key);
                writes[target] = bytes; before[target] = SafeFiles.HashFile(target);
            }
            foreach (ProfileDifference d in Compare(build, profile))
                if (d.State == "only_local") { string target = SafeFiles.Child(build, d.Relative); writes[target] = null; before[target] = SafeFiles.HashFile(target); }
            return Transaction.Apply(build, "profile", writes, before, guard);
        }
        static void DeleteTree(string allowedRoot, string target)
        {
            string root = Path.GetFullPath(allowedRoot).TrimEnd('\\', '/') + Path.DirectorySeparatorChar, full = Path.GetFullPath(target);
            if (!full.StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new IOException(Msg.Key("err_profilordner_liegt_ausserhalb_des", full));
            if (!Directory.Exists(full)) return;
            SafeFiles.NoLinks(full);
            Directory.Delete(full, true);
        }
    }

    // Wish 7, restore points: what Transaction keeps as backups\<plugin>\previous,
    // made visible and restorable. Restoring writes the snapshot back through the
    // transaction, so the rolling point then holds the state from just before.
    public sealed class RestoreFile
    {
        public string Path = "", HashBefore = "", Snapshot = "", CurrentHash = "";
        public bool Absent { get { return HashBefore == "absent"; } }
        public bool Differs { get { return CurrentHash != HashBefore; } }
    }
    public sealed class RestorePoint
    {
        public string PluginId = "", Folder = "", Reason = "";
        public DateTime Created;
        public readonly List<RestoreFile> Files = new List<RestoreFile>();
        public bool Complete = true;
        public int Changed { get { return Files.Count(f => f.Differs); } }
    }
    public static class RestorePoints
    {
        public const string Folder = "user_config\\.autoload\\backups";
        public static bool Pending(string build) { return File.Exists(SafeFiles.Child(build, "user_config\\.autoload\\pending.txt")); }
        public static List<RestorePoint> List(string build)
        {
            var list = new List<RestorePoint>();
            string root = SafeFiles.Child(build, Folder);
            if (!Directory.Exists(root)) return list;
            foreach (string group in Directory.GetDirectories(root))
            {
                string previous = Path.Combine(group, "previous");
                if (!Directory.Exists(previous)) continue;
                try { RestorePoint point = Load(build, previous); if (point != null) list.Add(point); }
                catch (Exception) { /* unreadable folders are skipped, never touched */ }
            }
            list.Sort((a, b) => b.Created.CompareTo(a.Created));
            return list;
        }
        static RestorePoint Load(string build, string folder)
        {
            string map = Path.Combine(folder, "files.txt");
            if (!File.Exists(map)) return null;
            string idFile = Path.Combine(folder, "plugin.id.txt");
            var point = new RestorePoint { Folder = folder, PluginId = File.Exists(idFile) ? SafeFiles.Text(idFile).Trim() : Path.GetFileName(Path.GetDirectoryName(folder)), Created = File.GetLastWriteTime(map) };
            string prefix = Path.GetFullPath(build).TrimEnd('\\', '/') + Path.DirectorySeparatorChar;
            foreach (string line in File.ReadAllLines(map, SafeFiles.Utf8))
            {
                if (line.Trim().Length == 0) continue;
                var match = Regex.Match(line, "^(\\d+): (.*) \\(vorher ([A-Fa-f0-9]{64}|absent)\\)$");
                if (!match.Success) { point.Complete = false; point.Reason = "line: " + line; continue; }
                string path = Path.GetFullPath(match.Groups[2].Value);
                // Only files inside this loader folder: a moved installation is not restored blindly.
                if (!path.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) { point.Complete = false; point.Reason = "outside: " + path; continue; }
                var file = new RestoreFile { Path = path, HashBefore = match.Groups[3].Value == "absent" ? "absent" : match.Groups[3].Value.ToUpperInvariant(), CurrentHash = SafeFiles.HashFile(path) };
                if (!file.Absent)
                {
                    file.Snapshot = Path.Combine(folder, match.Groups[1].Value + ".old");
                    if (!File.Exists(file.Snapshot)) { point.Complete = false; point.Reason = "missing: " + file.Snapshot; continue; }
                }
                point.Files.Add(file);
            }
            return point;
        }
        public static string Restore(string build, RestorePoint point, Action guard)
        {
            if (!point.Complete) throw new IOException(Msg.Key("err_wiederherstellungspunkt_ist_unvollstaendig_und", point.Folder, point.Reason));
            var writes = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            var before = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (RestoreFile file in point.Files)
            {
                byte[] bytes = null;
                if (!file.Absent)
                {
                    bytes = SafeFiles.Read(file.Snapshot, 64 * 1024 * 1024);
                    if (SafeFiles.Hash(bytes) != file.HashBefore) throw new IOException(Msg.Key("err_sicherungskopie_stimmt_nicht_mit", file.Snapshot));
                }
                writes[file.Path] = bytes; before[file.Path] = SafeFiles.HashFile(file.Path);
            }
            return Transaction.Apply(build, point.PluginId, writes, before, guard);
        }
    }
}
