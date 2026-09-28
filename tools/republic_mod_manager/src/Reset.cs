using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TesmioAutoload
{
    // 0.4.94: starting over. Two levels, because two very different wishes hide in one wish:
    //
    //   Data       - profiles, restore points, the saved view. Touches nothing the game reads,
    //                so no saved game can suffer. That is what most people mean by "start fresh".
    //   Everything - additionally everything RMM ever wrote into the loader folder, taken back
    //                along the receipts: overlays, local copies in plugins\, files in the vfs,
    //                its entries in tesmioloader.ini and in the bridge list, and the original
    //                INIs written back from the *.upstream.ini copies.
    //
    // Never touched: media_soviet and with it the saved games of the game itself, plugins and
    // INIs that RMM never wrote, the loader and its launcher, Workshop subscriptions.
    public sealed class ResetPlan
    {
        public bool Everything;
        public readonly List<string> Files = new List<string>();      // deleted, paths as shown
        public readonly List<string> Folders = new List<string>();    // deleted with everything in them
        public readonly List<string> Restored = new List<string>();   // written back from an upstream copy
        public readonly List<string> Edited = new List<string>();     // rewritten (loader ini, bridge list)
        public readonly List<string> Packages = new List<string>();   // packages whose entries go away (names)
        public readonly List<string> Targets = new List<string>();    // their loader keys, for tesmioloader.ini
        public readonly List<string> Ids = new List<string>();        // resources, deposits, needs, buildings
        public readonly List<string> Saves = new List<string>();      // saved games that use one of those ids
        public readonly List<string> Buildings = new List<string>();  // 0.5.5: generated building.ini put back
        public readonly List<string> Overlays = new List<string>();   // 0.5.9: our overlay mod, an absolute path outside the loader folder
        public bool View = true;                                      // the saved window state goes in both levels
        public int Count { get { return Files.Count + Folders.Count + Restored.Count + Edited.Count + Buildings.Count; } }
    }

    public static class ResetTool
    {
        public const string BackupFolder = "rmm_reset_backup";
        static string State(string build) { return SafeFiles.Child(build, "user_config\\.autoload"); }

        // Reading only: what an apply would do. The window shows exactly this list.
        public static ResetPlan Plan(string build, List<CatalogEntry> entries, List<SaveGame> saves, bool everything)
        {
            var plan = new ResetPlan { Everything = everything };
            string state = State(build);
            foreach (string folder in new[] { "profiles", "backups" })
                if (Directory.Exists(Path.Combine(state, folder))) plan.Folders.Add("user_config\\.autoload\\" + folder);
            if (!everything) return plan;

            // Plugins: the receipt says what was written for this target.
            foreach (string receipt in Files(state, "*.receipt.ini"))
            {
                Ini ini;
                try { ini = new Ini(SafeFiles.Text(receipt)); } catch (Exception) { continue; }
                // The file name of the receipt is the overlay it belongs to; a keyed editor writes
                // "<plugin>.editor.receipt.ini" for "<plugin>.editor.ini". The PLUGIN behind it is
                // the name without that suffix - it owns the loader entry and the upstream copy.
                // 0.4.99: without this, resources, needs, deposits and buildings_plus kept their
                // effective INI and their loader entry through a full reset.
                string overlay = Path.GetFileName(receipt); overlay = overlay.Substring(0, overlay.Length - ".receipt.ini".Length);
                string target = overlay.EndsWith(".editor", StringComparison.OrdinalIgnoreCase) ? overlay.Substring(0, overlay.Length - ".editor".Length) : overlay;
                string name = Named(entries, ini.Get("state", "id", target), target);
                if (!plan.Packages.Contains(name)) plan.Packages.Add(name);
                if (!plan.Targets.Contains(target, StringComparer.OrdinalIgnoreCase)) plan.Targets.Add(target);
                Add(plan.Files, build, "user_config\\" + overlay + ".ini");
                if (ini.Get("state", "local_copy", "0") == "1")
                {
                    Add(plan.Files, build, "plugins\\" + target + ".dll");
                    foreach (var pair in ini.Values.Where(v => v.Key.StartsWith("state/asset.", StringComparison.OrdinalIgnoreCase)))
                        Add(plan.Files, build, "plugins\\" + pair.Value);
                }
                // 0.5.4: which file the editor really wrote. Under Soviet Mod Loader that is its
                // baseline, not the generated file in plugins\ - restoring the latter would leave
                // the personal entries in place and overwrite a file SML rewrites anyway. Receipts
                // written before 0.5.4 have no such key, and for those plugins\<target>.ini is right.
                string written = ini.Get("state", "effective_file", "").Trim();
                if (written.Length == 0) written = "plugins\\" + target + ".ini";
                string upstream = Path.Combine(state, target + ".upstream.ini");
                if (File.Exists(upstream) && !plan.Restored.Contains(written, StringComparer.OrdinalIgnoreCase)) plan.Restored.Add(written);
            }
            // 0.5.9: the little overlay mod the editors write for package entries under SML. It sits
            // in the Workshop folder, outside the loader folder, and only ours is ever touched - the
            // marker in its manifest says so.
            try
            {
                string overlayRoot = SmlOverlay.Root(build);
                if (overlayRoot.Length > 0 && Directory.Exists(overlayRoot) && SmlOverlay.IsOverlay(Path.Combine(overlayRoot, "soviet.mod.ini"))
                    && !plan.Overlays.Contains(overlayRoot, StringComparer.OrdinalIgnoreCase)) plan.Overlays.Add(overlayRoot);
            }
            catch (Exception) { }
            // Content packages: their files in the vfs and the ids they added.
            string vfs = LocalEditorSpec.VfsRoot(build);
            foreach (string receipt in Files(Path.Combine(state, "content"), "receipt.ini", true))
            {
                Ini ini;
                try { ini = new Ini(SafeFiles.Text(receipt)); } catch (Exception) { continue; }
                if (ini.Get("content", "provided", "0") != "1") continue;
                string name = Named(entries, ini.Get("content", "id", ""), ini.Get("content", "name", ""));
                if (name.Length > 0 && !plan.Packages.Contains(name)) plan.Packages.Add(name);
                foreach (var pair in ini.Values.Where(v => v.Key.StartsWith("assets/", StringComparison.OrdinalIgnoreCase)))
                    try { string file = SafeFiles.Child(vfs, pair.Value); if (File.Exists(file)) plan.Files.Add("vfs\\" + pair.Value); } catch (Exception) { }
                foreach (var pair in ini.Values.Where(v => v.Key.StartsWith("added/", StringComparison.OrdinalIgnoreCase)))
                    foreach (string id in pair.Value.Split('|').Select(x => x.Trim()).Where(x => x.Length > 0))
                        if (!plan.Ids.Contains(id, StringComparer.OrdinalIgnoreCase)) plan.Ids.Add(id);
            }
            // 0.5.0: only name what really has something left to do. A second run used to list the
            // state folder and the bridge list although the folder was gone and the list was empty.
            string loaderIni = SafeFiles.Child(build, "tesmioloader.ini");
            if (File.Exists(loaderIni)) try
            {
                if (new LooseIni(SafeFiles.Text(loaderIni)).Entries("plugins").Any(e => plan.Targets.Contains((e.Key ?? "").Trim(), StringComparer.OrdinalIgnoreCase)))
                    plan.Edited.Add("tesmioloader.ini");
            }
            catch (Exception) { }
            // 0.5.5: personal changes in a generated building.ini live in the game folder, not
            // under the loader - deleting the receipts would leave the changed file behind.
            foreach (string id in WipEdits.Recorded(build)) plan.Buildings.Add(id);
            string bridgeIni = SafeFiles.Child(build, "user_config\\workshop_bridge.ini");
            if (File.Exists(bridgeIni)) try
            {
                if (new LooseIni(SafeFiles.Text(bridgeIni)).Entries("packages").Any()) plan.Edited.Add("user_config\\workshop_bridge.ini");
            }
            catch (Exception) { }
            if (Directory.Exists(state))
            {
                plan.Folders.Add("user_config\\.autoload");
                plan.Folders.RemoveAll(x => x.StartsWith("user_config\\.autoload\\", StringComparison.OrdinalIgnoreCase));
            }
            // The one thing a player must see before saying yes: which of his games this breaks.
            foreach (SaveGame save in saves ?? new List<SaveGame>())
                if (plan.Ids.Any(id => save.Knows("resources", id) || save.Knows("deposits", id)))
                    plan.Saves.Add(save.Name);
            return plan;
        }
        // The name a player knows the package by, not its id.
        static string Named(List<CatalogEntry> entries, string id, string target)
        {
            foreach (CatalogEntry entry in entries ?? new List<CatalogEntry>())
                if (entry.Id.Equals(id, StringComparison.OrdinalIgnoreCase) || (entry.Target.Length > 0 && entry.Target.Equals(target, StringComparison.OrdinalIgnoreCase)))
                    return entry.Name;
            return id.Length > 0 ? id : target;
        }
        static void Add(List<string> list, string build, string relative)
        {
            try { if (File.Exists(SafeFiles.Child(build, relative)) && !list.Contains(relative, StringComparer.OrdinalIgnoreCase)) list.Add(relative); }
            catch (Exception) { }
        }
        static IEnumerable<string> Files(string root, string pattern, bool recursive = false)
        {
            if (!Directory.Exists(root)) return new string[0];
            try { return Directory.GetFiles(root, pattern, recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly).OrderBy(x => x, StringComparer.OrdinalIgnoreCase); }
            catch (Exception) { return new string[0]; }
        }

        // Copies everything the reset touches into <loader>\rmm_reset_backup\<timestamp> before
        // a single file is removed. Small - INIs only, no DLLs and no vfs assets.
        public static string Backup(string build)
        {
            string root = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(build).TrimEnd('\\')) ?? build, BackupFolder);
            string target = Path.Combine(root, DateTime.Now.ToString("yyyyMMdd_HHmmss"));
            Directory.CreateDirectory(target);
            Copy(SafeFiles.Child(build, "tesmioloader.ini"), Path.Combine(target, "tesmioloader.ini"));
            // 0.5.4: under SML the editors write its baseline, so that folder has to travel into
            // the backup as well - otherwise a reset would take back entries nothing kept a copy of.
            var folders = new List<string> { "user_config", "plugins" };
            string baseFolder = Sml.Show(build, Path.Combine(Sml.StateDir(build), "base"));
            if (!Path.IsPathRooted(baseFolder) && Directory.Exists(SafeFiles.Child(build, baseFolder))) folders.Add(baseFolder);
            // 0.5.9: the overlay mod lives outside the loader folder, so it travels into the backup
            // under its own name.
            try
            {
                string overlayRoot = SmlOverlay.Root(build);
                if (overlayRoot.Length > 0 && Directory.Exists(overlayRoot) && SmlOverlay.IsOverlay(Path.Combine(overlayRoot, "soviet.mod.ini")))
                    foreach (string file in Directory.GetFiles(overlayRoot, "*.ini", SearchOption.AllDirectories))
                    {
                        string copy = Path.Combine(target, SmlOverlay.Folder, file.Substring(overlayRoot.Length).TrimStart('\\'));
                        Directory.CreateDirectory(Path.GetDirectoryName(copy)); Copy(file, copy);
                    }
            }
            catch (Exception) { }
            foreach (string folder in folders)
            {
                string source = SafeFiles.Child(build, folder);
                if (!Directory.Exists(source)) continue;
                foreach (string file in Directory.GetFiles(source, "*.ini", SearchOption.AllDirectories))
                {
                    string relative = file.Substring(source.Length).TrimStart('\\');
                    string copy = Path.Combine(target, folder, relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(copy));
                    Copy(file, copy);
                }
            }
            return target;
        }
        static void Copy(string from, string to)
        {
            try { if (File.Exists(from)) File.Copy(from, to, true); } catch (Exception) { }
        }

        // Carries the plan out. The guard is the usual one (no game, no launcher running).
        public static string Apply(string build, ResetPlan plan, Action guard)
        {
            if (guard != null) guard();
            string backup = Backup(build);
            var report = new StringBuilder();
            report.AppendLine("backup = " + backup);
            string state = State(build);
            // 1. Originals back into plugins\ before the copies of them disappear.
            foreach (string relative in plan.Restored)
            {
                string target = SafeFiles.Child(build, relative);
                string upstream = Path.Combine(state, Path.GetFileNameWithoutExtension(relative) + ".upstream.ini");
                try { if (File.Exists(upstream)) { File.Copy(upstream, target, true); report.AppendLine("restored = " + relative); } }
                catch (Exception e) { report.AppendLine("failed = " + relative + ": " + e.Message); }
            }
            // 2. The two lists RMM keeps for the loader.
            foreach (string relative in plan.Edited)
            {
                try
                {
                    string path = SafeFiles.Child(build, relative);
                    string text = SafeFiles.Text(path);
                    string cleaned = relative.EndsWith("workshop_bridge.ini", StringComparison.OrdinalIgnoreCase)
                        ? Strip(text, "packages", null) : Strip(text, "plugins", plan.Targets);
                    if (cleaned != text) { File.WriteAllText(path, cleaned, SafeFiles.Utf8); report.AppendLine("edited = " + relative); }
                }
                catch (Exception e) { report.AppendLine("failed = " + relative + ": " + e.Message); }
            }
            // 0.5.5: every generated building.ini back to what its generator writes, before the
            // receipts holding that copy are deleted with the state folder.
            foreach (string id in plan.Buildings)
            {
                try { if (WipEdits.RevertById(build, id)) report.AppendLine("restored = media_soviet\\workshop_wip\\" + id); }
                catch (Exception e) { report.AppendLine("failed = workshop_wip\\" + id + ": " + e.Message); }
            }
            // 3. Files, then whole folders.
            string vfs = LocalEditorSpec.VfsRoot(build);
            foreach (string relative in plan.Files)
            {
                try
                {
                    string path = relative.StartsWith("vfs\\", StringComparison.OrdinalIgnoreCase)
                        ? SafeFiles.Child(vfs, relative.Substring(4)) : SafeFiles.Child(build, relative);
                    if (File.Exists(path)) { File.Delete(path); report.AppendLine("removed = " + relative); }
                }
                catch (Exception e) { report.AppendLine("failed = " + relative + ": " + e.Message); }
            }
            foreach (string relative in plan.Folders)
            {
                try
                {
                    string path = SafeFiles.Child(build, relative);
                    if (!Directory.Exists(path)) continue;
                    SafeFiles.NoLinks(path); Directory.Delete(path, true); report.AppendLine("removed = " + relative + "\\");
                }
                catch (Exception e) { report.AppendLine("failed = " + relative + ": " + e.Message); }
            }
            // 0.5.9: our overlay mod in the Workshop folder. Only a folder whose manifest carries
            // our marker is touched, and it is checked again here - nothing else in that folder is
            // ours to delete.
            foreach (string path in plan.Overlays)
            {
                try
                {
                    if (!Directory.Exists(path) || !SmlOverlay.IsOverlay(Path.Combine(path, "soviet.mod.ini"))) continue;
                    SafeFiles.NoLinks(path); Directory.Delete(path, true); report.AppendLine("removed = " + path + "\\");
                }
                catch (Exception e) { report.AppendLine("failed = " + path + ": " + e.Message); }
            }
            return report.ToString();
        }
        // Removes keys from one section: the named ones, or the whole section when names is null.
        // Comments, other sections and the line endings stay as they are.
        public static string Strip(string text, string section, List<string> keep)
        {
            var lines = (text ?? "").Replace("\r\n", "\n").Split('\n');
            var result = new List<string>(); bool inside = false;
            foreach (string line in lines)
            {
                string trimmed = line.Trim();
                if (trimmed.StartsWith("[", StringComparison.Ordinal))
                { inside = trimmed.Equals("[" + section + "]", StringComparison.OrdinalIgnoreCase); result.Add(line); continue; }
                if (inside && trimmed.Length > 0 && !trimmed.StartsWith(";", StringComparison.Ordinal) && trimmed.Contains("="))
                {
                    // keep == null clears the section; otherwise only what RMM put there goes.
                    if (keep == null) continue;
                    string key = trimmed.Substring(0, trimmed.IndexOf('=')).Trim();
                    if (keep.Any(x => Tail(x).Equals(key, StringComparison.OrdinalIgnoreCase))) continue;
                }
                result.Add(line);
            }
            return String.Join("\r\n", result.ToArray());
        }
        static string Tail(string id)
        {
            if (String.IsNullOrEmpty(id)) return "";
            int dot = id.LastIndexOf('.');
            return dot < 0 ? id : id.Substring(dot + 1);
        }
    }
}
