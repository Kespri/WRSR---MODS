using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("Republic Mod Manager")]
[assembly: System.Reflection.AssemblyProduct("Republic Mod Manager (RMM)")]
[assembly: System.Reflection.AssemblyDescription("Plugin manager for TesmioLoader - Workers & Resources: Soviet Republic")]
[assembly: System.Reflection.AssemblyVersion("0.5.21.0")]
[assembly: System.Reflection.AssemblyFileVersion("0.5.21.0")]
[assembly: System.Reflection.AssemblyInformationalVersion("0.5.21")]

namespace TesmioAutoload
{
    static class Program
    {
        [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window, int command);
        static void BringExistingToFront()
        {
            try
            {
                int self = Process.GetCurrentProcess().Id;
                foreach (var process in Process.GetProcessesByName("rmm"))
                    using (process)
                    {
                        if (process.Id == self || process.MainWindowHandle == IntPtr.Zero) continue;
                        ShowWindow(process.MainWindowHandle, 9 /* SW_RESTORE */); SetForegroundWindow(process.MainWindowHandle); return;
                    }
            }
            catch (Exception) { }
        }
        [STAThread] static int Main(string[] args)
        {
            try
            {
                string directory = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
                var defaults = new UiState { Build = directory }; string legacy = "", screenshot = null, snapshotSize = null, snapshotWindow = null; bool saveMode = false, steamCheck = args.Contains("--steam-check"); bool? activate = null;
                if (!File.Exists(Path.Combine(defaults.Build, "tesmioloader.dll"))) try
                {
                    foreach (string library in Discovery.SteamLibraries())
                    {
                        string candidate = Path.Combine(library, @"steamapps\common\SovietRepublic\tesmioloader\build");
                        if (File.Exists(Path.Combine(candidate, "tesmioloader.dll"))) { defaults.Build = candidate; break; }
                    }
                }
                catch (IOException) { } catch (UnauthorizedAccessException) { } catch (FormatException) { }
                // rmm.ini beside the executable; the pre-0.17 name still counts when
                // no rmm.ini exists, so an upgraded installation keeps its paths.
                string preferences = Path.Combine(directory, "rmm.ini");
                if (!File.Exists(preferences) && File.Exists(Path.Combine(directory, "tesmio_autoload.ini"))) preferences = Path.Combine(directory, "tesmio_autoload.ini");
                if (File.Exists(preferences))
                {
                    var ini = new Ini(SafeFiles.Text(preferences));
                    defaults.Build = ini.Get("paths", "loader_build", defaults.Build); legacy = ini.Get("paths", "package", "");
                    defaults.WorkshopRoot = ini.Get("paths", "workshop_root", "");
                    defaults.Language = ini.Get("settings", "language", "auto");
                    AppOptions.IniVersionCheck = ini.Get("settings", "version_check", "1") != "0" ? "1" : "0";
                    AppOptions.IniLauncherWindow = ini.Get("settings", "tesmiolauncher_window", "0") == "1" ? "1" : "0";
                    AppOptions.IniWatchSeconds = ini.Get("settings", "launch_watch_seconds", "15");
                    AppOptions.IniKeepOpen = ini.Get("settings", "keep_open", "0") == "1" ? "1" : "0";
                    LauncherOptions.KeepOpen = ini.Get("settings", "keep_open", "0") == "1";
                    GameVersion.CheckEnabled = ini.Get("settings", "version_check", "1") != "0";
                    LauncherOptions.ShowWindow = ini.Get("settings", "tesmiolauncher_window", "0") == "1";
                    // 0.4.92: how long RMM watches the game after a start; 0 closes right away.
                    double watch; if (Double.TryParse(ini.Get("settings", "launch_watch_seconds", "15"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out watch) && watch >= 0) LauncherOptions.WatchSeconds = watch;
                }
                GameVersion.LoadTable(Path.Combine(directory, "settings_schemas"));
                if (defaults.WorkshopRoot.Length == 0) defaults.WorkshopRoot = Catalog.InitialRoot(defaults.Build, legacy);
                if (legacy.Length > 0) defaults.SelectedSource = Path.GetFullPath(legacy);
                var notes = new List<string>(); UiStateStore.Migrate(directory); var store = new UiStateStore(UiStateStore.DefaultPath(directory));
                bool snapshotMode = args.Contains("--ui-snapshot") || args.Contains("--save") || steamCheck;
                var state = snapshotMode ? defaults : store.Load(defaults, notes);
                // What the options window saved wins over the rmm.ini defaults (0.4.93).
                AppOptions.Apply(state);
                for (int i = 0; i < args.Length; ++i)
                {
                    // --save is the one option without a value; everything else is a pair.
                    if (args[i] == "--save") { saveMode = true; continue; }
                    if (args[i] == "--steam-check") continue;   // valueless too: report the Steam login state and end
                    if (i + 1 >= args.Length) throw new ArgumentException(Msg.Key("err_argument_ben_tigt_einen", args[i]));
                    string key = args[i], value = args[++i];
                    if (key == "--build") state.Build = value;
                    else if (key == "--workshop") { state.WorkshopRoot = Catalog.NormalizeRoot(value); state.SelectedId = ""; state.SelectedSource = ""; }
                    else if (key == "--package") { state.WorkshopRoot = Catalog.NormalizeRoot(value); state.SelectedId = ""; state.SelectedSource = Path.GetFullPath(value); }
                    else if (key == "--activate") { if (value != "on" && value != "off") throw new ArgumentException(Msg.Key("err_unbekanntes_argument", key + " " + value)); activate = value == "on"; }
                    else if (key == "--language") state.Language = value;
                    else if (key == "--ui-snapshot") screenshot = value;
                    else if (key == "--size") snapshotSize = value;      // WxH, snapshot mode only
                    else if (key == "--tab") state.SelectedTab = value;   // tab id to open, snapshot mode only
                    else if (key == "--window") snapshotWindow = value;   // profiles or points, buildings or add: snapshot of that window instead
                    else throw new ArgumentException(Msg.Key("err_unbekanntes_argument", key));
                }
                // --activate only makes sense together with --save: it stages the switch, --save writes it.
                if (activate.HasValue && !saveMode) throw new ArgumentException(Msg.Key("err_unbekanntes_argument", "--activate (only together with --save)"));
                // 0.4.91: --steam-check answers the one question the launch warning asks, without a
                // window: is Steam logged in? It prints the raw record as well, so a false alarm can
                // be told apart from a real logout afterwards.
                if (steamCheck)
                {
                    string why; SteamSession.State steam = SteamSession.Check(out why);
                    var text = new Language(state.Language);
                    Console.WriteLine((steam == SteamSession.State.LoggedOut ? "FAIL " : "PASS ")
                        + (steam == SteamSession.State.Ready ? "Steam is logged in."
                           : steam == SteamSession.State.Unknown ? "Steam state unknown - RMM would not warn."
                           : "Steam is logged out - RMM would warn. " + text.T(why))
                        + "  [" + SteamSession.Evidence() + "]");
                    Console.Out.Flush();
                    return steam == SteamSession.State.LoggedOut ? 1 : 0;
                }
                // One window at a time: two instances would race for the same files and
                // block every installer. A second start only brings the first window up.
                bool first = true; Mutex instance = null;
                if (!snapshotMode) instance = new Mutex(true, "Local\\RepublicModManager_Window", out first);
                if (instance != null && !first) { BringExistingToFront(); return 0; }
                Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
                using (var form = new MainForm(state, store, !snapshotMode))
                {
                    foreach (string note in notes) form.Report(note);
                    // 0.4.81: save the selected entry without showing the window - the same code path
                    // as the Save button, with the same runtime checks. Anything that would ask the
                    // player (a native DLL going into plugins\, the local-copy confirmation) is
                    // refused instead of answered, so this can never decide something on their behalf.
                    if (saveMode)
                    {
                        form.ShowInTaskbar = false; form.StartPosition = FormStartPosition.Manual; form.Location = new Point(-20000, -20000);
                        form.Show(); form.PerformLayout(); Application.DoEvents();
                        if (activate.HasValue)
                        {
                            string moved = form.CliActivate(activate.Value);
                            if (moved != null) { Console.WriteLine(moved); Console.Out.Flush(); form.PendingPrompt = () => DialogResult.No; form.Close(); return 1; }
                        }
                        string result = form.CliSave();
                        Console.WriteLine(result); Console.Out.Flush();
                        // Closing asks about anything still unsaved, and there is nobody to answer:
                        // whatever is left over is dropped, which writes nothing.
                        form.PendingPrompt = () => DialogResult.No;
                        form.Close();
                        return result.StartsWith("PASS", StringComparison.Ordinal) ? 0 : 1;
                    }
                    if (screenshot != null)
                    {
                        form.ShowInTaskbar = false; form.StartPosition = FormStartPosition.Manual; form.Location = new Point(-20000, -20000);
                        form.Show(); form.PerformLayout(); Application.DoEvents();
                        // --size is applied after the first paint, the way a user resizes a
                        // window that is already open - the harder case for the layout.
                        if (snapshotSize != null) { string[] wh = snapshotSize.ToLowerInvariant().Split('x'); form.Size = new Size(Int32.Parse(wh[0]), Int32.Parse(wh[1])); Application.DoEvents(); Application.DoEvents(); }
                        if (snapshotWindow == "add")
                        {
                            // The add dialog is modal: a timer inside its message loop draws and closes it.
                            var timer = new System.Windows.Forms.Timer { Interval = 500 };
                            timer.Tick += (s, e) =>
                            {
                                timer.Stop(); Form dialog = Form.ActiveForm;
                                if (dialog != null && dialog != form) { using (var image = new Bitmap(dialog.Width, dialog.Height)) { dialog.DrawToBitmap(image, new Rectangle(0, 0, dialog.Width, dialog.Height)); image.Save(screenshot); } dialog.Close(); }
                            };
                            timer.Start(); form.TestOpenAddDialog(); form.Close();
                            return 0;
                        }
                        if (snapshotWindow == "texts")
                        {
                            using (var window = new GameTextPickerWindow(new Language(state.Language), state.Build, new[] { 1970 }, form.Font, form.Icon))
                            {
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000); window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        if (snapshotWindow == "buildings")
                        {
                            using (var window = new BuildingPickerWindow(new Language(state.Language), state.Build, state.WorkshopRoot, new[] { "buildings_types\\technical_services.ini" }, null, form.Font, form.Icon))
                            {
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000); window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        // 0.4.94: the loss list of a full reset - planned, never carried out.
                        if (snapshotWindow == "reset")
                        {
                            using (var window = form.TestResetWindow())
                            {
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000); window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        // 0.4.93: the options window of RMM itself.
                        if (snapshotWindow == "options")
                        {
                            using (var window = new OptionsWindow(new Language(state.Language), state, () => "report", form.Font, form.Icon))
                            {
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000);
                                // --size works here too: the window scrolls and a snapshot cannot.
                                if (snapshotSize != null) { string[] wh = snapshotSize.ToLowerInvariant().Split('x'); window.Size = new Size(Int32.Parse(wh[0]), Int32.Parse(wh[1])); }
                                window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        // 0.4.88: the start check, drawn like the other windows.
                        if (snapshotWindow == "startcheck")
                        {
                            using (var window = form.TestStartCheckWindow())
                            {
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000); window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        if (snapshotWindow != null)
                        {
                            using (var window = new ProfilesWindow(new Language(state.Language), state.Build, form.Font, form.Icon))
                            {
                                if (snapshotWindow == "points") window.TestShowPoints();
                                window.ShowInTaskbar = false; window.StartPosition = FormStartPosition.Manual; window.Location = new Point(-20000, -20000); window.Show(); Application.DoEvents();
                                using (var image = new Bitmap(window.Width, window.Height)) { window.DrawToBitmap(image, new Rectangle(0, 0, window.Width, window.Height)); image.Save(screenshot); }
                            }
                            return 0;
                        }
                        using (var image = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(image, new Rectangle(0, 0, form.Width, form.Height)); image.Save(screenshot); }
                        return 0;
                    }
                    Application.Run(form);
                }
                GC.KeepAlive(instance);
                return 0;
            }
            catch (Exception e)
            {
                // A command-line run must never wait for a click: it writes the reason and ends.
                if (args.Contains("--ui-snapshot") || args.Contains("--save") || args.Contains("--activate") || args.Contains("--steam-check")) { Console.Error.WriteLine(e); Console.Error.Flush(); }
                else { var fallback = new Language("auto"); MessageWindow.Show(null, fallback, "Republic Mod Manager", fallback.Localize(e.Message), MessageWindow.Kind.Error, DialogResult.OK); }
                return 1;
            }
        }
    }
}
