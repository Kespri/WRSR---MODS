using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // The "profiles and restore" window: named configuration profiles of the
    // loader folder (wish 6) and the rolling restore point of every plugin
    // (wish 7). Every write goes through Transaction, guarded like a save.
    sealed class ProfilesWindow : Form
    {
        readonly Language language; readonly string build;
        readonly Button profilesTab, pointsTab; readonly Panel profilesPage = new Panel(), pointsPage = new Panel();
        readonly ListBox profileList = new ListBox(), pointList = new ListBox();
        readonly ListView profileFiles = new ListView(), pointFiles = new ListView();
        readonly Label profileInfo = Theme.Label("", 9.5f, false), pointInfo = Theme.Label("", 9.5f, false), summary = Theme.Label("", 9, false);
        readonly Button applyButton, deleteButton, openProfileButton, restoreButton, openPointButton;
        readonly List<Profile> profiles = new List<Profile>(); readonly List<RestorePoint> points = new List<RestorePoint>();
        bool loading, pointsShown;
        public bool Changed;                                 // the main window rescans afterwards
        public readonly List<string> Journal = new List<string>();
        internal Func<string, DialogResult> Confirm = null;  // test hooks: dialogs replaced
        internal Func<KeyValuePair<string, string>?> NamePrompt = null;
        internal Action Guard = null;

        public ProfilesWindow(Language language, string build, Font font, Icon icon)
        {
            this.language = language; this.build = build ?? "";
            Text = language.T("profiles"); Font = font; Icon = icon; Size = new Size(1180, 720); MinimumSize = new Size(900, 520); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3, Padding = new Padding(12) };
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 60)); shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 30));
            Controls.Add(shell);
            var strip = new TabStrip { Dock = DockStyle.Fill };
            profilesTab = Tab(language.T("profiles_tab"), () => Select(true)); pointsTab = Tab(language.T("points_tab"), () => Select(false));
            strip.Controls.Add(profilesTab); strip.Controls.Add(pointsTab); shell.Controls.Add(strip, 0, 0);
            var pages = new Panel { Dock = DockStyle.Fill, Margin = Padding.Empty };
            profilesPage.Dock = DockStyle.Fill; pointsPage.Dock = DockStyle.Fill; pages.Controls.Add(profilesPage); pages.Controls.Add(pointsPage); shell.Controls.Add(pages, 0, 1);
            summary.ForeColor = Theme.Muted; summary.Dock = DockStyle.Fill; summary.Margin = new Padding(0, 8, 0, 0); summary.AutoEllipsis = true; summary.AutoSize = false; shell.Controls.Add(summary, 0, 2);

            // Profiles page: list left, files and buttons right.
            var save = Theme.Button(language.T("profile_save"), () => Run(SaveProfile), true);
            applyButton = Theme.Button(language.T("profile_apply"), () => Run(ApplyProfile), false);
            deleteButton = Theme.Button(language.T("profile_delete"), () => Run(DeleteProfile), false);
            openProfileButton = Theme.Button(language.T("profile_open"), () => Run(() => OpenFolder(Profiles.Root(build))), false);
            var refreshProfiles = Theme.Button(language.T("profile_refresh"), () => Run(Reload), false);
            BuildPage(profilesPage, language.T("profile_note_header"), profileList, profileFiles, profileInfo, new[] { save, applyButton, deleteButton, openProfileButton, refreshProfiles });
            profileList.SelectedIndexChanged += delegate { if (!loading) ShowProfile(); };

            // Restore points page.
            restoreButton = Theme.Button(language.T("point_restore"), () => Run(RestoreSelectedPoint), true);
            openPointButton = Theme.Button(language.T("profile_open"), () => Run(() => OpenFolder(SelectedPoint() != null ? SelectedPoint().Folder : SafeFiles.Child(build, RestorePoints.Folder))), false);
            var refreshPoints = Theme.Button(language.T("profile_refresh"), () => Run(Reload), false);
            BuildPage(pointsPage, language.T("point_header"), pointList, pointFiles, pointInfo, new[] { restoreButton, openPointButton, refreshPoints });
            pointList.SelectedIndexChanged += delegate { if (!loading) ShowPoint(); };
            Select(true); Reload();
        }
        Button Tab(string text, Action click)
        {
            var b = Theme.Button(text, click, false); b.Margin = new Padding(0, 0, 12, 0); b.MinimumSize = new Size(120, TabStrip.TabHeight); b.Height = TabStrip.TabHeight; b.FlatAppearance.BorderSize = 0; return b;
        }
        void BuildPage(Panel page, string header, ListBox list, ListView files, Label info, Button[] buttons)
        {
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 4, Margin = Padding.Empty };
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 300)); grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            grid.RowStyles.Add(new RowStyle(SizeType.AutoSize)); grid.RowStyles.Add(new RowStyle(SizeType.AutoSize)); grid.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            var head = Theme.Label(header, 9.5f, false); head.ForeColor = Theme.Muted; head.MaximumSize = new Size(1100, 0); head.Margin = new Padding(0, 0, 0, 10); grid.Controls.Add(head, 0, 0); grid.SetColumnSpan(head, 2);
            var bar = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0, 0, 0, 8) };
            foreach (Button b in buttons) { b.Margin = new Padding(0, 0, 8, 0); bar.Controls.Add(b); }
            grid.Controls.Add(bar, 0, 1); grid.SetColumnSpan(bar, 2);
            list.Dock = DockStyle.Fill; list.Margin = new Padding(0, 0, 12, 0); list.IntegralHeight = false; list.BorderStyle = BorderStyle.FixedSingle; grid.Controls.Add(list, 0, 2);
            files.Dock = DockStyle.Fill; files.Margin = Padding.Empty; files.View = View.Details; files.FullRowSelect = true; files.HeaderStyle = ColumnHeaderStyle.Nonclickable; files.MultiSelect = false; files.HideSelection = false; files.BorderStyle = BorderStyle.FixedSingle;
            files.Columns.Add(language.T("column_file"), 520); files.Columns.Add(language.T("column_state"), 260); grid.Controls.Add(files, 1, 2);
            info.Margin = new Padding(0, 8, 0, 0); info.MaximumSize = new Size(1100, 0); grid.Controls.Add(info, 0, 3); grid.SetColumnSpan(info, 2);
            page.Controls.Add(grid);
        }
        void Select(bool profilesShown)
        {
            profilesPage.Visible = profilesShown; pointsPage.Visible = !profilesShown; pointsShown = !profilesShown;
            foreach (var pair in new[] { new { b = profilesTab, on = profilesShown }, new { b = pointsTab, on = !profilesShown } })
            { pair.b.BackColor = pair.on ? Theme.SelectionBlue : Color.White; pair.b.ForeColor = pair.on ? Color.White : Theme.Ink; pair.b.FlatAppearance.MouseOverBackColor = pair.on ? Theme.SelectionBlue : Theme.TabHover; }
            if (profilesTab.Parent != null) profilesTab.Parent.Invalidate();
        }
        void Run(Action action) { try { action(); } catch (Exception e) { string text = language.Localize(e.Message); Journal.Add(text); MessageWindow.Show(this, language, Text, text, MessageWindow.Kind.Warning, DialogResult.OK); } }
        bool HasBuild { get { return build.Length > 0 && Directory.Exists(build); } }

        public void Reload()
        {
            loading = true;
            try
            {
                string keepProfile = SelectedProfile() != null ? SelectedProfile().Name : null; string keepPoint = SelectedPoint() != null ? SelectedPoint().Folder : null;
                profiles.Clear(); points.Clear(); profileList.Items.Clear(); pointList.Items.Clear();
                if (HasBuild)
                {
                    profiles.AddRange(Profiles.List(build)); points.AddRange(RestorePoints.List(build));
                    foreach (Profile p in profiles) profileList.Items.Add(p.Name + "   (" + p.Created.ToString("g") + ", " + p.Files.Count + ")");
                    foreach (RestorePoint p in points) pointList.Items.Add(p.PluginId + "   (" + p.Created.ToString("g") + ")");
                }
                profileList.SelectedIndex = profiles.FindIndex(p => p.Name == keepProfile) >= 0 ? profiles.FindIndex(p => p.Name == keepProfile) : (profiles.Count > 0 ? 0 : -1);
                pointList.SelectedIndex = points.FindIndex(p => p.Folder == keepPoint) >= 0 ? points.FindIndex(p => p.Folder == keepPoint) : (points.Count > 0 ? 0 : -1);
            }
            finally { loading = false; }
            ShowProfile(); ShowPoint();
            if (!HasBuild) summary.Text = language.T("profile_build_missing");
            else if (RestorePoints.Pending(build)) summary.Text = language.T("point_pending");
        }
        Profile SelectedProfile() { int at = profileList.SelectedIndex; return at >= 0 && at < profiles.Count ? profiles[at] : null; }
        RestorePoint SelectedPoint() { int at = pointList.SelectedIndex; return at >= 0 && at < points.Count ? points[at] : null; }

        void ShowProfile()
        {
            Profile profile = SelectedProfile(); profileFiles.Items.Clear();
            applyButton.Enabled = deleteButton.Enabled = profile != null;
            if (profile == null) { profileInfo.Text = profiles.Count == 0 ? language.T("profile_none") : language.T("profile_empty"); return; }
            var diff = Profiles.Compare(build, profile);
            foreach (ProfileDifference d in diff)
            {
                var item = new ListViewItem(d.Relative); item.SubItems.Add(language.T("state_" + d.State));
                item.ForeColor = d.State == "same" || d.State == "kept" ? Theme.Ink : d.State == "differs" ? Color.FromArgb(140, 92, 0) : Theme.Danger;
                profileFiles.Items.Add(item);
            }
            profileInfo.Text = language.Format("profile_info", profile.Created.ToString("g"), profile.Files.Count, diff.Count(d => d.State == "same"), diff.Count(d => d.State == "differs"), diff.Count(d => d.State == "missing_local"), diff.Count(d => d.State == "only_local"))
                + (profile.Note.Length > 0 ? "\n" + profile.Note : "");
        }
        void ShowPoint()
        {
            RestorePoint point = SelectedPoint(); pointFiles.Items.Clear();
            restoreButton.Enabled = point != null && point.Complete;
            if (point == null) { pointInfo.Text = points.Count == 0 ? language.T("point_none") : language.T("point_empty"); return; }
            foreach (RestoreFile f in point.Files)
            {
                string relative = f.Path.StartsWith(Path.GetFullPath(build).TrimEnd('\\') + "\\", StringComparison.OrdinalIgnoreCase) ? f.Path.Substring(Path.GetFullPath(build).TrimEnd('\\').Length + 1) : f.Path;
                var item = new ListViewItem(relative); item.SubItems.Add(language.T(f.Absent ? (f.Differs ? "state_absent" : "state_same") : f.Differs ? "state_changed" : "state_same"));
                item.ForeColor = f.Differs ? Color.FromArgb(140, 92, 0) : Theme.Ink;
                pointFiles.Items.Add(item);
            }
            pointInfo.Text = language.Format("point_info", point.PluginId, point.Created.ToString("g"), point.Files.Count, point.Changed) + (point.Complete ? "" : "\n" + language.T("point_incomplete"));
        }

        void SaveProfile()
        {
            if (!HasBuild) throw new IOException(language.T("profile_build_missing"));
            KeyValuePair<string, string>? entry = NamePrompt != null ? NamePrompt() : AskName();
            if (entry == null) return;
            string name = entry.Value.Key.Trim(), note = entry.Value.Value.Trim();
            if (!Profiles.ValidName(name)) throw new FormatException(language.T("profile_name_invalid"));
            bool overwrite = Profiles.Find(build, name) != null;
            if (overwrite && Ask(language.Format("profile_exists", name)) != DialogResult.Yes) return;
            Profile saved = Profiles.Save(build, name, note, overwrite);
            Note(language.Format("profile_saved", saved.Name, saved.Files.Count));
            Reload(); profileList.SelectedIndex = profiles.FindIndex(p => p.Name == saved.Name); ShowProfile();
        }
        void ApplyProfile()
        {
            Profile profile = SelectedProfile(); if (profile == null) return;
            var diff = Profiles.Compare(build, profile);
            int written = diff.Count(d => d.State == "differs" || d.State == "missing_local"), removed = diff.Count(d => d.State == "only_local");
            if (written + removed == 0) { Note(language.Format("profile_apply_same", profile.Name)); return; }
            if (Ask(language.Format("profile_apply_question", profile.Name, written, removed)) != DialogResult.Yes) return;
            string backup = Profiles.Apply(build, profile, Guard ?? (() => RuntimeGuard.NoGameRunning(build)));
            Changed = true; Note(language.Format("profile_applied", profile.Name, backup)); Reload();
        }
        void DeleteProfile()
        {
            Profile profile = SelectedProfile(); if (profile == null) return;
            if (Ask(language.Format("profile_delete_question", profile.Name)) != DialogResult.Yes) return;
            Profiles.Delete(build, profile); Note(language.Format("profile_deleted", profile.Name)); Reload();
        }
        void RestoreSelectedPoint()
        {
            RestorePoint point = SelectedPoint(); if (point == null) return;
            if (point.Changed == 0) { Note(language.T("point_restore_same")); return; }
            if (Ask(language.Format("point_restore_question", point.PluginId, point.Created.ToString("g"), point.Changed)) != DialogResult.Yes) return;
            string backup = RestorePoints.Restore(build, point, Guard ?? (() => RuntimeGuard.NoGameRunning(build)));
            Changed = true; Note(language.Format("point_restored", point.PluginId, backup)); Reload();
        }
        DialogResult Ask(string question)
        {
            if (Confirm != null) return Confirm(question);
            return MessageWindow.Show(this, language, Text, question, MessageWindow.Kind.Question, DialogResult.Yes, DialogResult.No);
        }
        void Note(string text) { summary.Text = text; Journal.Add(text); }
        // Name and optional note for a new profile.
        KeyValuePair<string, string>? AskName()
        {
            using (var dialog = new Form { Text = language.T("profile_save"), Font = Font, Icon = Icon, Size = new Size(520, 250), FormBorderStyle = FormBorderStyle.FixedDialog, MinimizeBox = false, MaximizeBox = false, StartPosition = FormStartPosition.CenterParent, ShowInTaskbar = false, BackColor = Color.White })
            {
                var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(16) };
                var name = new TextBox { Width = 460, MaxLength = 40 }; var note = new TextBox { Width = 460, MaxLength = 200 };
                grid.Controls.Add(Theme.Label(language.T("profile_name"), 10, false), 0, 0); grid.Controls.Add(name, 0, 1);
                var noteLabel = Theme.Label(language.T("profile_note"), 10, false); noteLabel.Margin = new Padding(0, 10, 0, 7); grid.Controls.Add(noteLabel, 0, 2); grid.Controls.Add(note, 0, 3);
                var buttons = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.RightToLeft, Dock = DockStyle.Fill, Margin = new Padding(0, 12, 0, 0) };
                var ok = Theme.Button(language.T("apply"), () => { if (!Profiles.ValidName(name.Text.Trim())) { MessageWindow.Show(dialog, language, dialog.Text, language.T("profile_name_invalid"), MessageWindow.Kind.Warning, DialogResult.OK); return; } dialog.DialogResult = DialogResult.OK; }, true);
                var cancel = Theme.Button(language.T("cancel"), () => dialog.DialogResult = DialogResult.Cancel, false); cancel.Margin = new Padding(8, 0, 0, 0);
                buttons.Controls.Add(ok); buttons.Controls.Add(cancel); grid.Controls.Add(buttons, 0, 4);
                dialog.Controls.Add(grid); dialog.AcceptButton = ok; dialog.CancelButton = cancel;
                if (Theme.Modal(this,dialog) != DialogResult.OK) return null;
                return new KeyValuePair<string, string>(name.Text, note.Text);
            }
        }
        void OpenFolder(string folder)
        {
            if (!Directory.Exists(folder)) throw new IOException(language.T("link_missing") + ": " + folder);
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("explorer.exe", "\"" + folder + "\"") { UseShellExecute = true });
        }
        internal int ProfileCount { get { return profiles.Count; } }
        internal int PointCount { get { return points.Count; } }
        internal string SummaryText { get { return summary.Text; } }
        internal string ProfileInfoText { get { return profileInfo.Text; } }
        internal int ProfileFileRows { get { return profileFiles.Items.Count; } }
        internal void TestSelectProfile(string name) { profileList.SelectedIndex = profiles.FindIndex(p => p.Name == name); }
        internal void TestSelectPoint(string pluginId) { pointList.SelectedIndex = points.FindIndex(p => p.PluginId == pluginId); }
        internal void TestSave() { SaveProfile(); }
        internal void TestApply() { ApplyProfile(); }
        internal void TestDelete() { DeleteProfile(); }
        internal void TestRestore() { RestoreSelectedPoint(); }
        internal void TestShowPoints() { Select(false); }
        internal bool PointsShown { get { return pointsShown; } }
    }
}
