// UI discovery and preferences only. No DLL loading, config deployment or game changes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TesmioAutoload
{
    public sealed class CatalogEntry
    {
        public string Root, Id = "", Name, Version = "", Problem = "";
        public bool Supported, LocalEditor;
        // Where the entry comes from, for the list icon: "steam" (a Workshop folder of
        // a Steam library), "loader" (the loader's own plugins folder), else "unknown".
        public string Origin = "unknown";
        // "plugin" (deployable native hook) or "content" (SML-only content, listed for information).
        public string Kind = "plugin";
        // Installed: found as build\plugins\<Target>.dll without a Workshop package (Root is the DLL path).
        public bool Installed;
        // Deployed once and changed in the Workshop since (see UpdateCheck).
        public bool Updated; public string PreviousVersion = "";
        public string Target = "";
        public readonly List<string> Hints = new List<string>();
        public readonly List<Dependency> Dependencies = new List<Dependency>();
        public string Status { get { return Problem.Length > 0 ? "Hinweis" : !Supported ? "Nicht unterstützt" : Kind == "content" ? "SML-Inhalt" : Installed ? "Installiert" : "Verfügbar"; } }
        public override string ToString() { return Name + (Problem.Length > 0 ? " [Hinweis]" : !Supported ? " [nicht unterstützt]" : Kind == "content" ? " [SML-Inhalt]" : Installed ? " [installiert]" : ""); }
    }

    public static class Catalog
    {
        // Folder with local launcher schemas (settings_schemas beside the exe): the
        // second schema tier for installed plugins, and home of the keyed editors.
        public static string SchemaRoot = "";
        // Workshop packages whose DLL also sits in plugins\ while SML is active: SML
        // loads the package copy and TesmioLoader the local one - twice in one game.
        // Marks every Workshop package that was deployed once and has changed
        // since; returns the names of those, for the startup summary.
        public static List<string> MarkUpdates(IEnumerable<CatalogEntry> entries, string build)
        {
            var result = new List<string>();
            if (String.IsNullOrWhiteSpace(build)) return result;
            foreach (var e in entries.Where(x => x.Supported && !x.Installed && !x.LocalEditor && (x.Kind == "plugin" || x.Kind == "content") && x.Problem.Length == 0))
            {
                e.Updated = false; e.PreviousVersion = "";
                try
                {
                    if (e.Kind == "content")
                    {
                        // 0.4.80: a provided content package whose fragments or assets changed in the Workshop.
                        var content = new ContentSession(Package.Load(e.Root), build);
                        e.Updated = content.UpdatePending; e.PreviousVersion = content.ProvidedVersion;
                        if (e.Updated) result.Add(e.Name + (content.ProvidedVersion.Length > 0 && content.ProvidedVersion != e.Version ? " (" + content.ProvidedVersion + " -> " + e.Version + ")" : ""));
                        continue;
                    }
                    UpdateState u = UpdateCheck.Of(Package.Load(e.Root), build);
                    e.Updated = u.Pending; e.PreviousVersion = u.PreviousVersion;
                    if (u.Pending) result.Add(e.Name + (u.PreviousVersion.Length > 0 ? " (" + u.PreviousVersion + " -> " + e.Version + ")" : ""));
                }
                catch (Exception) { }
            }
            return result;
        }
        // Workshop packages whose DLL also sits in plugins\ while SML or the bridge
        // would load them from the package. Under SML that is a double load; under
        // the bridge the local copy wins and the package is skipped - either way
        // worth a word at startup.
        public static List<string> DuplicateDlls(IEnumerable<CatalogEntry> entries, string build)
        {
            var result = new List<string>();
            if (String.IsNullOrWhiteSpace(build) || !(Sml.Active(build) || Bridge.Active(build))) return result;
            foreach (var e in entries.Where(x => x.Supported && !x.Installed && x.Kind == "plugin" && x.Target.Length > 0))
                if (File.Exists(Path.Combine(Path.GetFullPath(build), "plugins", e.Target + ".dll"))) result.Add(e.Name + " (plugins\\" + e.Target + ".dll)");
            return result;
        }
        // Matches every [dependencies] line of a package against the packages in the
        // catalog (by mod id) and checks the version constraint. Pure bookkeeping:
        // RuntimeGuard decides what an unmet dependency means for a deployment.
        public static void ResolveDependencies(List<Dependency> dependencies, IEnumerable<CatalogEntry> entries)
        {
            var list = entries.Where(e => e.Supported && e.Id.Length > 0).ToList();
            foreach (Dependency d in dependencies)
            {
                CatalogEntry match = list.FirstOrDefault(e => e.Id.Equals(d.Id, StringComparison.OrdinalIgnoreCase) && !e.Installed);
                d.Found = match != null; d.Target = ""; d.Kind = ""; d.Version = ""; d.VersionOk = true;
                if (match == null)
                {
                    // 0.34.1: a plugin installed the classic way (plugins\<name>.dll without a
                    // package) satisfies an id whose last segment is that name, e.g.
                    // tesmio.localization -> localization.dll. Its version is not checked.
                    string tail = d.Id.Substring(d.Id.LastIndexOf('.') + 1);
                    CatalogEntry local = tail.Length > 0 ? entries.FirstOrDefault(e => e.Supported && e.Installed && e.Kind != "content" && e.Target.Equals(tail, StringComparison.OrdinalIgnoreCase)) : null;
                    if (local != null)
                    {
                        d.Found = true; d.Target = local.Target; d.Kind = "plugin"; d.Version = local.Version; d.Root = ""; d.VersionOk = true; d.Name = String.IsNullOrEmpty(local.Name) ? d.Id : local.Name;
                        d.Note = Msg.Key("dep_found_installed", d.Id, local.Target);
                        continue;
                    }
                }
                if (match == null) { d.Note = Msg.Key("dep_missing", d.Id + (d.Constraint.Length > 0 ? " (" + d.Constraint + ")" : "")); continue; }
                d.Target = match.Target; d.Kind = match.Kind; d.Version = match.Version; d.Root = match.Root ?? ""; d.Name = String.IsNullOrEmpty(match.Name) ? d.Id : match.Name;
                d.VersionOk = VersionRule.Satisfied(d.Constraint, match.Version);
                // 0.4.27: the notes name the package, not the id; the editor asks RuntimeGuard.Provisioned whether it is switched on.
                d.Note = d.VersionOk ? Msg.Key(match.Kind == "content" ? "dep_found_content" : "dep_found", d.Name, match.Version) : Msg.Key("dep_wrong_version", d.Name, match.Version, d.Constraint);
            }
        }
        // Once the installed plugins are known (ScanInstalled), the dependency notes
        // of the Workshop entries are resolved again against the complete list, so a
        // dependency met only by a classic installation loses its "missing" hint.
        public static void ResolveAll(List<CatalogEntry> entries)
        {
            foreach (var entry in entries.Where(e => e.Dependencies.Count > 0))
            {
                ResolveDependencies(entry.Dependencies, entries);
                entry.Hints.RemoveAll(h => h.StartsWith("\u0001dep_", StringComparison.Ordinal));
                foreach (Dependency d in entry.Dependencies.Where(x => !x.Found || !x.VersionOk)) entry.Hints.Add(d.Note);
            }
        }
        // Plugins present in build\plugins that no listed Workshop package or local
        // editor accounts for - the plain TesmioLoader installation.
        public static List<CatalogEntry> ScanInstalled(string build, IEnumerable<CatalogEntry> known, List<string> diagnostics)
        {
            var entries = new List<CatalogEntry>();
            try
            {
                if (String.IsNullOrWhiteSpace(build)) return entries;
                string plugins = Path.Combine(Path.GetFullPath(build), "plugins");
                if (!Directory.Exists(plugins)) return entries;
                SafeFiles.NoLinks(plugins);
                var covered = new HashSet<string>(known.Where(e => e.Target.Length > 0).Select(e => e.Target), StringComparer.OrdinalIgnoreCase);
                foreach (var editor in known.Where(e => e.LocalEditor)) { try { covered.Add(LocalEditorSpec.Load(editor.Root).Plugin); } catch (Exception) { } }
                foreach (string dll in Directory.GetFiles(plugins, "*.dll").OrderBy(s => s, StringComparer.OrdinalIgnoreCase))
                {
                    string name = Path.GetFileNameWithoutExtension(dll);
                    if (covered.Contains(name)) continue;
                    var entry = new CatalogEntry { Root = dll, Name = name, Target = name, Installed = true, Origin = "loader" };
                    try
                    {
                        Package package = InstalledPlugins.Load(build, name, SchemaRoot);
                        entry.Supported = true; entry.Id = package.Id; entry.Name = package.Name; entry.Version = package.Version; entry.Kind = package.Kind; entry.Hints.AddRange(package.Hints);
                    }
                    catch (Exception e) { entry.Problem = Msg.Key("catalog_installed_rejected", e.Message); entry.Supported = false; }
                    entries.Add(entry);
                }
            }
            catch (Exception e) { diagnostics.Add("Installierte Plugins konnten nicht gelesen werden: " + e.Message); }
            foreach (var entry in entries.Where(e => e.Problem.Length > 0)) diagnostics.Add(entry.Root + ": " + entry.Problem);
            return entries.OrderBy(e => e.Name, StringComparer.CurrentCultureIgnoreCase).ToList();
        }
        public static string NormalizeRoot(string path)
        {
            if (String.IsNullOrWhiteSpace(path)) return "";
            string full = Path.GetFullPath(path);
            if (full.Length > Path.GetPathRoot(full).Length) full = full.TrimEnd('\\', '/');
            // Backwards compatibility: an old single-package path becomes its parent.
            if (File.Exists(Path.Combine(full, "soviet.mod.ini"))) return Path.GetDirectoryName(full);
            return full;
        }
        public static string WorkshopForBuild(string build)
        {
            if (String.IsNullOrWhiteSpace(build)) return "";
            for (var dir = new DirectoryInfo(Path.GetFullPath(build)); dir != null; dir = dir.Parent)
                if (dir.Name.Equals("steamapps", StringComparison.OrdinalIgnoreCase))
                    return Path.Combine(dir.FullName, @"workshop\content\784150");
            return "";
        }
        public static string InitialRoot(string build, string legacyPackage)
        {
            string steam = WorkshopForBuild(build);
            if (!String.IsNullOrWhiteSpace(legacyPackage))
            {
                string legacy = Path.GetFullPath(legacyPackage).TrimEnd('\\', '/');
                // Prefer the subscribed copy when this formerly private WIP item
                // is now present in Steam. Otherwise keep the WIP collection usable.
                if (steam.Length > 0 && File.Exists(Path.Combine(steam, Path.GetFileName(legacy), "soviet.mod.ini"))) return steam;
                return NormalizeRoot(legacy);
            }
            return steam;
        }
        static List<string> steamWorkshopRoots;
        static bool UnderSteamWorkshop(string path)
        {
            try
            {
                if (steamWorkshopRoots == null) steamWorkshopRoots = Discovery.SteamLibraries().Select(l => Path.Combine(l, "steamapps\\workshop") + "\\").ToList();
                string full = Path.GetFullPath(path);
                return steamWorkshopRoots.Any(r => full.StartsWith(r, StringComparison.OrdinalIgnoreCase));
            }
            catch (Exception) { return false; }
        }
        public static List<CatalogEntry> Scan(string root, List<string> diagnostics)
        {
            var entries = new List<CatalogEntry>();
            if (String.IsNullOrWhiteSpace(root)) { diagnostics.Add("Kein Workshop-Ordner gewählt."); return entries; }
            string full = NormalizeRoot(root);
            try
            {
                SafeFiles.NoLinks(full);
                if (!Directory.Exists(full)) { diagnostics.Add("Workshop-Ordner nicht vorhanden: " + full); return entries; }
                foreach (string child in Directory.GetDirectories(full).OrderBy(s => s, StringComparer.OrdinalIgnoreCase))
                {
                    string manifest = Path.Combine(child, "soviet.mod.ini");
                    if (!File.Exists(manifest)) continue;
                    // 0.5.9: the overlay layer Republic Mod Manager writes for package entries under
                    // Soviet Mod Loader is not an entry of its own - the editors that fill it own it.
                    if (SmlOverlay.IsOverlay(manifest)) continue;
                    var entry = new CatalogEntry { Root = child, Name = Path.GetFileName(child), Origin = UnderSteamWorkshop(child) ? "steam" : "unknown" };
                    try
                    {
                        SafeFiles.NoLinks(child);
                        // Only parse [mod] metadata here. Other SML sections may
                        // legitimately contain repeated hooks/content directives.
                        string text = SafeFiles.Text(manifest);
                        var metadata = new StringBuilder(); bool inMod = false, seenMod = false;
                        foreach (string raw in text.Replace("\r\n", "\n").Split('\n'))
                        {
                            string line = raw.Trim();
                            if (line.StartsWith("[", StringComparison.Ordinal))
                            {
                                if (!line.EndsWith("]", StringComparison.Ordinal)) throw new FormatException(Msg.Key("err_ung_ltiger_abschnitt_im"));
                                inMod = line.Substring(1, line.Length - 2).Trim().Equals("mod", StringComparison.OrdinalIgnoreCase);
                                if (inMod && seenMod) throw new FormatException(Msg.Key("err_mod_ist_mehrfach_vorhanden"));
                                if (inMod) { seenMod = true; metadata.AppendLine("[mod]"); }
                            }
                            else if (inMod) metadata.AppendLine(raw);
                        }
                        var mod = new Ini(metadata.ToString());
                        entry.Id = mod.Required("mod", "id"); entry.Name = mod.Required("mod", "name"); entry.Version = mod.Get("mod", "version", "?");
                        if (entry.Id.Length > 160 || entry.Name.Length > 200 || entry.Name.Any(Char.IsControl)) throw new FormatException(Msg.Key("err_ung_ltige_paketkennung_name"));
                        if (mod.Get("mod", "enabled", "1") != "1") entry.Problem = "Dieses Paket ist im Manifest deaktiviert.";
                        else
                        {
                            Package package=Package.Load(child);entry.Supported=true;entry.Id=package.Id;entry.Name=package.Name;entry.Version=package.Version;entry.Kind=package.Kind;entry.Target=package.Target;entry.Hints.AddRange(package.Hints);entry.Dependencies.AddRange(package.Dependencies);
                        }
                    }
                    catch (Exception e) { entry.Problem = Msg.Key("catalog_manifest_rejected", e.Message); entry.Supported = false; }
                    entries.Add(entry);
                }
            }
            catch (Exception e) { diagnostics.Add("Workshop konnte nicht vollständig gelesen werden: " + e.Message); }
            foreach (var duplicates in entries.Where(e => e.Id.Length > 0).GroupBy(e => e.Id, StringComparer.OrdinalIgnoreCase).Where(g => g.Count() > 1))
                foreach (var entry in duplicates) entry.Problem = "Paket-ID mehrfach vorhanden: " + entry.Id + ". Doppelte Pakete zuerst klären; keine automatische Bereitstellung.";
            foreach (var entry in entries.Where(e => e.Dependencies.Count > 0))
            {
                ResolveDependencies(entry.Dependencies, entries);
                foreach (Dependency d in entry.Dependencies.Where(x => !x.Found || !x.VersionOk)) entry.Hints.Add(d.Note);
            }
            foreach (var entry in entries.Where(e => e.Problem.Length > 0)) diagnostics.Add(entry.Root + ": " + entry.Problem);
            return entries.OrderBy(e => e.Name, StringComparer.CurrentCultureIgnoreCase).ThenBy(e => e.Root, StringComparer.OrdinalIgnoreCase).ToList();
        }
        public static List<CatalogEntry> ScanLocalEditors(string schemaRoot,List<string> diagnostics)
        {
            var entries=new List<CatalogEntry>();
            try
            {
                if(!Directory.Exists(schemaRoot))return entries;SafeFiles.NoLinks(schemaRoot);
                foreach(string path in Directory.GetFiles(schemaRoot,"*.launcher.ini").OrderBy(x=>x,StringComparer.OrdinalIgnoreCase))
                {
                    // Plain launcher schemas in this folder are the second schema tier for
                    // installed plugins (InstalledPlugins); only keyed editors are entries here.
                    try{if(new Ini(SafeFiles.Text(path)).Get("launcher","editor_type","").Length==0)continue;}catch(Exception){continue;}
                    try{LocalEditorSpec spec=LocalEditorSpec.Load(path);entries.Add(new CatalogEntry{Root=path,Id=spec.Id,Name=spec.Name,Version=spec.Version,Supported=true,LocalEditor=true,Origin="loader"});}
                    catch(Exception e){diagnostics.Add(path+": Lokaler Editor abgewiesen: "+e.Message);}
                }
            }
            catch(Exception e){diagnostics.Add("Lokale Editor-Schemas konnten nicht gelesen werden: "+e.Message);}
            return entries;
        }
        public static CatalogEntry RestoreSelection(List<CatalogEntry> entries, string id, string source)
        {
            var exact = entries.FirstOrDefault(e => String.Equals(e.Root, source, StringComparison.OrdinalIgnoreCase));
            if (exact != null) return exact;
            var sameId = entries.Where(e => id.Length > 0 && e.Id.Equals(id, StringComparison.OrdinalIgnoreCase)).ToList();
            if (sameId.Count == 1) return sameId[0];
            return entries.FirstOrDefault(e => e.Supported && e.Problem.Length == 0) ?? entries.FirstOrDefault();
        }
    }

    public sealed class UiState
    {
        public string Build = "", WorkshopRoot = "", SelectedId = "", SelectedSource = "", SelectedTab = "", Language = "auto";
        // 0.4.93: what the options window set. Empty means "not set here" - then the rmm.ini value counts.
        public string LauncherWindow = "", WatchSeconds = "", VersionCheck = "", KeepOpen = "";
        // Last window size in physical pixels (0 = not saved yet) and whether it was maximized.
        public int WindowWidth, WindowHeight; public bool WindowMaximized;
        public UiState Copy() { return new UiState { Build = Build, WorkshopRoot = WorkshopRoot, SelectedId = SelectedId, SelectedSource = SelectedSource, SelectedTab = SelectedTab, Language = Language, WindowWidth = WindowWidth, WindowHeight = WindowHeight, WindowMaximized = WindowMaximized, LauncherWindow = LauncherWindow, WatchSeconds = WatchSeconds, VersionCheck = VersionCheck, KeepOpen = KeepOpen }; }
    }
    public sealed class UiStateStore
    {
        public readonly string PathName;
        string expected;
        bool readable = true;
        public UiStateStore(string path) { PathName = Path.GetFullPath(path); }
        public UiState Load(UiState defaults, List<string> notes)
        {
            var state = defaults.Copy();
            try
            {
                SafeFiles.NoLinks(PathName); expected = SafeFiles.HashFile(PathName);
                if (expected == "absent") return state;
                var ini = new Ini(SafeFiles.Text(PathName));
                if (ini.Get("ui", "format") != "1") throw new FormatException(Msg.Key("err_unbekanntes_ui_speicherformat"));
                state.Build = ini.Get("paths", "loader_build", state.Build); state.WorkshopRoot = ini.Get("paths", "workshop_root", state.WorkshopRoot);
                state.SelectedId = ini.Get("selection", "id", ""); state.SelectedSource = ini.Get("selection", "source", "");
                state.SelectedTab = ini.Get("selection", "tab", "");
                state.Language = ini.Get("ui", "language", "auto");
                state.LauncherWindow = ini.Get("ui", "tesmiolauncher_window", ""); state.WatchSeconds = ini.Get("ui", "launch_watch_seconds", ""); state.VersionCheck = ini.Get("ui", "version_check", ""); state.KeepOpen = ini.Get("ui", "keep_open", "");
                int w, h; Int32.TryParse(ini.Get("ui", "window_width", "0"), out w); Int32.TryParse(ini.Get("ui", "window_height", "0"), out h);
                state.WindowWidth = w > 0 ? w : 0; state.WindowHeight = h > 0 ? h : 0; state.WindowMaximized = ini.Get("ui", "window_maximized", "0") == "1";
            }
            catch (Exception e) { readable = false; notes.Add(Msg.Key("view_restore_failed", e.Message)); }
            return state;
        }
        public void Save(UiState state)
        {
            if (!readable) throw new IOException(Msg.Key("err_besch_digter_ansichtsspeicher_wird", PathName));
            var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase) { { "ui/format", "1" } };
            values.Add("ui/language", state.Language);
            if (state.LauncherWindow.Length > 0) values.Add("ui/tesmiolauncher_window", state.LauncherWindow);
            if (state.WatchSeconds.Length > 0) values.Add("ui/launch_watch_seconds", state.WatchSeconds);
            if (state.VersionCheck.Length > 0) values.Add("ui/version_check", state.VersionCheck);
            if (state.KeepOpen.Length > 0) values.Add("ui/keep_open", state.KeepOpen);
            if (state.WindowWidth > 0 && state.WindowHeight > 0) { values.Add("ui/window_width", state.WindowWidth.ToString(System.Globalization.CultureInfo.InvariantCulture)); values.Add("ui/window_height", state.WindowHeight.ToString(System.Globalization.CultureInfo.InvariantCulture)); values.Add("ui/window_maximized", state.WindowMaximized ? "1" : "0"); }
            if (state.Build.Length > 0) values.Add("paths/loader_build", state.Build);
            if (state.WorkshopRoot.Length > 0) values.Add("paths/workshop_root", state.WorkshopRoot);
            if (state.SelectedId.Length > 0) values.Add("selection/id", state.SelectedId);
            if (state.SelectedSource.Length > 0) values.Add("selection/source", state.SelectedSource);
            if (state.SelectedTab.Length > 0) values.Add("selection/tab", state.SelectedTab);
            foreach (string value in values.Values) if (value.Any(Char.IsControl)) throw new FormatException(Msg.Key("err_ung_ltiger_wert_im"));
            SafeFiles.NoLinks(PathName);
            if (SafeFiles.HashFile(PathName) != expected) throw new IOException(Msg.Key("err_ansicht_wurde_in_einem"));
            byte[] bytes = SafeFiles.Utf8.GetBytes(Ini.Sparse(values)); string hash = SafeFiles.Hash(bytes);
            if (hash == expected) return;
            Directory.CreateDirectory(Path.GetDirectoryName(PathName));
            string temp = PathName + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (var file = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None)) { file.Write(bytes, 0, bytes.Length); file.Flush(true); }
                SafeFiles.NoLinks(PathName);
                if (SafeFiles.HashFile(PathName) != expected) throw new IOException(Msg.Key("err_ansicht_w_hrend_des"));
                if (File.Exists(PathName)) File.Replace(temp, PathName, null); else File.Move(temp, PathName);
                expected = hash;
            }
            finally { if (File.Exists(temp)) File.Delete(temp); } // own uniquely-created staging file only
        }
        static string Identity(string executableDirectory)
        { return SafeFiles.Hash(SafeFiles.Utf8.GetBytes(System.IO.Path.GetFullPath(executableDirectory).TrimEnd('\\').ToUpperInvariant())).Substring(0, 20); }
        public static string DefaultPath(string executableDirectory)
        {
            return System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RepublicModManager", "profiles", Identity(executableDirectory) + ".ini");
        }
        // Before 0.17 the state lived under %LOCALAPPDATA%\TesmioAutoload. Copied once.
        public static void Migrate(string executableDirectory)
        {
            try
            {
                string fresh = DefaultPath(executableDirectory);
                string old = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "TesmioAutoload", "profiles", Identity(executableDirectory) + ".ini");
                if (File.Exists(fresh) || !File.Exists(old)) return;
                Directory.CreateDirectory(System.IO.Path.GetDirectoryName(fresh)); File.Copy(old, fresh);
            }
            catch (Exception) { }
        }
    }
}
