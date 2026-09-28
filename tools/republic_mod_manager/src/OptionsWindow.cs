using System;
using System.Drawing;
using System.Globalization;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // 0.4.93: the settings of RMM itself, in one place. Everything here belongs to the program,
    // not to a plugin - that is why it lives behind its own sidebar button and not on a plugin
    // page. Values are written into the saved view (%LOCALAPPDATA%\RepublicModManager), because
    // that is the layer that wins over rmm.ini; where both differ, the rmm.ini value is shown
    // underneath so nobody wonders why editing that file changes nothing.
    sealed class OptionsWindow : Form
    {
        readonly Language language; readonly UiState state;
        readonly ToggleSwitch launcherWindow = new ToggleSwitch(), versionCheck = new ToggleSwitch(), keepOpen = new ToggleSwitch();
        readonly NumberInput watchSeconds = new NumberInput(0, 600, 5);
        // 0.5.20: the loader's own log settings, in tesmioloader.ini rather than in rmm.ini.
        readonly StepSwitch logLevel = new StepSwitch(4);
        readonly NumberInput logRepeat = new NumberInput(0, LoaderLog.MaxRepeat, 1);
        string logBuild; int logLevelWas, logRepeatWas;
        readonly ComboBox languageBox = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList };
        readonly Func<string> diagnostics;
        public bool Changed;                       // the main window saves the view afterwards
        public string LanguageCode;                // set when the language was changed here
        internal Action<string> Copied = null;     // test hook instead of the clipboard

        public OptionsWindow(Language language, UiState state, Func<string> diagnostics, Font font, Icon icon)
        {
            this.language = language; this.state = state; this.diagnostics = diagnostics; LanguageCode = state.Language;
            Text = language.T("options"); Font = font; Icon = icon; Size = new Size(860, 800); MinimumSize = new Size(680, 520);
            StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, Padding = new Padding(14) };
            shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
            Controls.Add(shell);
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true }; shell.Controls.Add(scroll, 0, 0);
            var stack = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 1, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink };
            stack.Width = scroll.ClientSize.Width - 4; scroll.SizeChanged += (s, e) => stack.Width = scroll.ClientSize.Width - 4;
            scroll.Controls.Add(stack);

            // Everything that happens when the game is started.
            Card start = Card(stack, language.T("options_start"));
            launcherWindow.Checked = LauncherOptions.ShowWindow;
            launcherWindow.CheckedChanged += delegate { state.LauncherWindow = launcherWindow.Checked ? "1" : "0"; LauncherOptions.ShowWindow = launcherWindow.Checked; Changed = true; UpdateNotes(); };
            Row(start, language.T("options_launcher_window"), language.T("options_launcher_window_help"), launcherWindow, "launcher_window");
            watchSeconds.Text = LauncherOptions.WatchSeconds.ToString(CultureInfo.InvariantCulture);
            watchSeconds.ValueEdited += delegate { StageWatch(); };
            watchSeconds.Stepped += delegate { StageWatch(); };
            Row(start, language.T("options_watch"), language.T("options_watch_help"), watchSeconds, "watch_seconds");
            keepOpen.Checked = LauncherOptions.KeepOpen;
            keepOpen.CheckedChanged += delegate { state.KeepOpen = keepOpen.Checked ? "1" : "0"; LauncherOptions.KeepOpen = keepOpen.Checked; Changed = true; UpdateNotes(); };
            Row(start, language.T("options_keep_open"), language.T("options_keep_open_help"), keepOpen, "keep_open");

            // The rest of the program.
            Card general = Card(stack, language.T("options_general"));
            Fields.Tall(languageBox); foreach (var item in Language.Available()) languageBox.Items.Add(item);
            languageBox.DisplayMember = "Value"; languageBox.ValueMember = "Key";
            for (int i = 0; i < languageBox.Items.Count; i++) if (((System.Collections.Generic.KeyValuePair<string, string>)languageBox.Items[i]).Key == state.Language) languageBox.SelectedIndex = i;
            if (languageBox.SelectedIndex < 0) languageBox.SelectedIndex = 0;
            languageBox.SelectedIndexChanged += delegate
            {
                if (languageBox.SelectedItem == null) return;
                string code = ((System.Collections.Generic.KeyValuePair<string, string>)languageBox.SelectedItem).Key;
                if (code == state.Language) return;
                state.Language = code; LanguageCode = code; Changed = true;
            };
            Row(general, language.T("options_language"), language.T("options_language_help"), languageBox, "language");
            versionCheck.Checked = GameVersion.CheckEnabled;
            versionCheck.CheckedChanged += delegate { state.VersionCheck = versionCheck.Checked ? "1" : "0"; GameVersion.CheckEnabled = versionCheck.Checked; Changed = true; UpdateNotes(); };
            Row(general, language.T("options_version_check"), language.T("options_version_check_help"), versionCheck, "version_check");

            // The loader's log. Not an rmm.ini value and not a plugin's: it lives in
            // [tesmioloader] of tesmioloader.ini and is written straight through.
            logBuild = state.Build;
            if (logBuild != null && LoaderLog.Available(logBuild))
            {
                Card log = Card(stack, language.T("options_log"));
                logLevelWas = LoaderLog.Level(logBuild); logRepeatWas = LoaderLog.Repeat(logBuild);
                logLevel.Value = logLevelWas;
                logLevel.ValueChanged += delegate { Changed = true; UpdateNotes(); };
                Row(log, language.T("options_log_level"), language.T("options_log_level_help"), logLevel, "log_level");
                logRepeat.Text = logRepeatWas.ToString(CultureInfo.InvariantCulture);
                logRepeat.ValueEdited += delegate { Changed = true; UpdateNotes(); };
                logRepeat.Stepped += delegate { Changed = true; UpdateNotes(); };
                Row(log, language.T("options_log_repeat"), language.T("options_log_repeat_help"), logRepeat, "log_repeat");
            }

            // One click for a bug report: versions, folders and the Steam state in the clipboard.
            Card support = Card(stack, language.T("options_support"));
            var copy = Theme.Button(language.T("options_copy_report"), CopyReport, false);
            copy.MinimumSize = Size.Empty; copy.AutoSize = true; copy.Margin = new Padding(0, 4, 0, 0);
            Row(support, language.T("options_report"), language.T("options_report_help"), copy, "copy_report");


            // Starting over. Two levels: the harmless one and the one that can cost saved games.
            Card danger = Card(stack, language.T("options_reset"));
            var dataButton = Theme.Button(language.T("options_reset_data"), () => StartReset(false), false);
            dataButton.MinimumSize = Size.Empty; dataButton.AutoSize = true; dataButton.Margin = new Padding(0, 4, 0, 0);
            dataButton.BackColor = Color.FromArgb(255, 244, 214); dataButton.FlatAppearance.BorderColor = Color.FromArgb(214, 173, 74);
            Row(danger, language.T("options_reset_data_title"), language.T("options_reset_data_help"), dataButton, "reset_data");
            var allButton = Theme.Button(language.T("options_reset_all"), () => StartReset(true), false);
            allButton.MinimumSize = Size.Empty; allButton.AutoSize = true; allButton.Margin = new Padding(0, 4, 0, 0);
            allButton.BackColor = Color.FromArgb(196, 43, 43); allButton.ForeColor = Color.White;
            allButton.FlatAppearance.BorderSize = 0; allButton.FlatAppearance.MouseOverBackColor = Color.FromArgb(166, 30, 30);
            Row(danger, language.T("options_reset_all_title"), language.T("options_reset_all_help"), allButton, "reset_all");

            var footer = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false };
            footer.Controls.Add(Theme.Button(language.T("close"), () => { DialogResult = DialogResult.OK; Close(); }, true));
            shell.Controls.Add(footer, 0, 1);
            UpdateNotes();
        }
        // The reset in three steps, exactly as agreed: the loss list with the affected saved
        // games, then a field where the word has to be typed, then the report. The focus sits
        // on Cancel everywhere, so Enter never destroys anything.
        internal Func<bool, ResetPlan> Planner;          // level -> what would happen
        internal Func<ResetPlan, string> Applier;        // carry it out, returns the report
        internal Action<string> Journal;                 // lines for the RMM journal
        internal Func<ResetPlan, bool> ConfirmHook = null;      // tests answer the dialogs here
        public bool ResetDone;
        void StartReset(bool everything)
        {
            if (Planner == null || Applier == null) return;
            ResetPlan plan;
            try { plan = Planner(everything); }
            catch (Exception e) { MessageWindow.Show(this, language, Text, ErrorTextOf(e), MessageWindow.Kind.Error, DialogResult.OK); return; }
            if (plan.Count == 0) { if (ConfirmHook == null) MessageWindow.Show(this, language, Text, language.T("options_reset_nothing"), MessageWindow.Kind.Info, DialogResult.OK); return; }
            if (ConfirmHook != null) { if (!ConfirmHook(plan)) return; }
            else if (!Confirm(plan)) return;
            string report;
            try { report = Applier(plan); }
            catch (Exception e) { MessageWindow.Show(this, language, Text, ErrorTextOf(e), MessageWindow.Kind.Error, DialogResult.OK); return; }
            ResetDone = true; Changed = true;
            if (Journal != null) foreach (string line in report.Replace("\r\n", "\n").Split('\n')) if (line.Trim().Length > 0) Journal("reset: " + line.Trim());
            string backup = ""; foreach (string line in report.Replace("\r\n", "\n").Split('\n')) if (line.StartsWith("backup = ", StringComparison.Ordinal)) backup = line.Substring(9).Trim();
            if (ConfirmHook == null) MessageWindow.Show(this, language, Text, language.Format("options_reset_done", plan.Count, backup), MessageWindow.Kind.Info, DialogResult.OK);
        }
        static string ErrorTextOf(Exception e) { return e.Message ?? e.GetType().Name; }
        bool Confirm(ResetPlan plan)
        {
            using (var window = new ResetConfirmWindow(language, plan, Font, Icon))
                if (Theme.Modal(this,window) != DialogResult.OK) return false;
            if (!plan.Everything) return true;                       // level 1 needs no second word
            using (var window = new ResetWordWindow(language, Font, Icon))
                return Theme.Modal(this,window) == DialogResult.OK;
        }
        void StageWatch()
        {
            decimal seconds;
            if (!Decimal.TryParse(watchSeconds.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out seconds) || seconds < 0) return;
            state.WatchSeconds = seconds.ToString(CultureInfo.InvariantCulture);
            LauncherOptions.WatchSeconds = (double)seconds; Changed = true; UpdateNotes();
        }
        // "Default from rmm.ini: ..." wherever the personal value differs from the shipped one.
        void UpdateNotes()
        {
            Note("launcher_window", AppOptions.IniLauncherWindow == "1", launcherWindow.Checked);
            Note("version_check", AppOptions.IniVersionCheck == "1", versionCheck.Checked);
            Note("keep_open", AppOptions.IniKeepOpen == "1", keepOpen.Checked);
            // What the knob is standing on, in words, right under it - a stepped switch
            // shows how far along it is, never what that means.
            if (notes.ContainsKey("log_level"))
                notes["log_level"].Text = language.T("options_log_level_" + logLevel.Value);
            Label note = notes.ContainsKey("watch_seconds") ? notes["watch_seconds"] : null;
            if (note == null) return;
            double mine; Double.TryParse(watchSeconds.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out mine);
            bool same = Math.Abs(mine - AppOptions.DefaultWatchSeconds) < 0.0001;
            note.Text = same ? "" : language.Format("options_ini_default", AppOptions.DefaultWatchSeconds.ToString(CultureInfo.InvariantCulture));
        }
        // The two loader values are written on the way out, in one transaction, and only
        // when they really differ - the game must not be running for it.
        internal void SaveLoaderLog()
        {
            if (logBuild == null || !LoaderLog.Available(logBuild)) return;
            int repeat; if (!Int32.TryParse(logRepeat.Text.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out repeat)) repeat = logRepeatWas;
            repeat = Math.Max(0, Math.Min(LoaderLog.MaxRepeat, repeat));
            if (logLevel.Value == logLevelWas && repeat == logRepeatWas) return;
            LoaderLog.Save(logBuild, logLevel.Value, repeat);
            logLevelWas = logLevel.Value; logRepeatWas = repeat;
            if (Journal != null) Journal(language.Format("options_log_saved", logLevel.Value.ToString(CultureInfo.InvariantCulture), repeat.ToString(CultureInfo.InvariantCulture)));
        }
        internal int TestLogLevel { get { return logLevel.Value; } set { logLevel.Value = value; } }
        void Note(string key, bool fromIni, bool mine)
        {
            if (!notes.ContainsKey(key)) return;
            notes[key].Text = fromIni == mine ? "" : language.Format("options_ini_default", language.T(fromIni ? "state_on" : "state_off"));
        }
        void CopyReport()
        {
            string text = diagnostics != null ? diagnostics() : "";
            if (Copied != null) { Copied(text); return; }
            try { Clipboard.SetText(text); MessageWindow.Show(this, language, Text, language.T("options_report_copied"), MessageWindow.Kind.Info, DialogResult.OK); }
            catch (Exception e) { MessageWindow.Show(this, language, Text, e.Message, MessageWindow.Kind.Error, DialogResult.OK); }
        }
        readonly System.Collections.Generic.Dictionary<string, Label> notes = new System.Collections.Generic.Dictionary<string, Label>(StringComparer.OrdinalIgnoreCase);
        static Card Card(TableLayoutPanel stack, string title)
        {
            var card = new Card(title) { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink };
            var inner = new TableLayoutPanel { ColumnCount = 2, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top, Margin = Padding.Empty };
            inner.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 62)); inner.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 38));
            card.Controls.Add(inner); card.Tag = inner;
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(card);
            return card;
        }
        // A row: title and explanation on the left, the control plus its note on the right.
        void Row(Card card, string title, string help, Control input, string key)
        {
            var inner = (TableLayoutPanel)card.Tag;
            var text = new TableLayoutPanel { ColumnCount = 1, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill, Margin = new Padding(0, 10, 12, 10) };
            Label caption = Theme.Label(title, 10, true); caption.AutoSize = true; caption.Margin = new Padding(0, 0, 0, 2);
            // The help texts carry their line break as a literal \n, the way every other RMM text does.
            Label explain = Theme.Label((help ?? "").Replace("\\n", "\n"), 9, false); explain.ForeColor = Theme.Muted; explain.AutoSize = true; explain.MaximumSize = new Size(440, 0);
            text.Controls.Add(caption); text.Controls.Add(explain);
            var right = new TableLayoutPanel { ColumnCount = 1, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Fill, Margin = new Padding(0, 10, 0, 10) };
            input.Margin = new Padding(0, 0, 0, 2); input.AccessibleName = "option:" + key;
            if (input is NumberInput || input is ComboBox) input.Width = 190;
            right.Controls.Add(input);
            Label note = Theme.Label("", 9, false); note.ForeColor = Theme.Muted; note.AutoSize = true; note.Margin = new Padding(2, 0, 0, 0); note.AccessibleName = "note:" + key;
            right.Controls.Add(note); notes[key] = note;
            int row = inner.RowCount++; inner.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            inner.Controls.Add(text, 0, row); inner.Controls.Add(right, 1, row);
        }
        // Test hooks.
        internal bool TestLauncherWindow { get { return launcherWindow.Checked; } set { launcherWindow.Checked = value; } }
        internal bool TestVersionCheck { get { return versionCheck.Checked; } set { versionCheck.Checked = value; } }
        internal bool TestKeepOpen { get { return keepOpen.Checked; } set { keepOpen.Checked = value; } }
        internal string TestWatch { get { return watchSeconds.Text; } set { watchSeconds.Text = value; StageWatch(); } }
        internal string TestNote(string key) { return notes.ContainsKey(key) ? notes[key].Text : null; }
        internal void TestCopy() { CopyReport(); }
        internal void TestReset(bool everything) { StartReset(everything); }
    }

    // Step two: what exactly goes, and which saved games it costs. 0.4.95: the losses are a
    // table with one line per kind instead of a paragraph of sentences - the saved games stay
    // on top in red, because that is the part nobody can rebuild - and what RMM deliberately
    // leaves alone sits below a rule, clearly apart from the list of what disappears.
    sealed class ResetConfirmWindow : Form
    {
        readonly System.Collections.Generic.List<Label> wrapped = new System.Collections.Generic.List<Label>();
        public ResetConfirmWindow(Language language, ResetPlan plan, Font font, Icon icon)
        {
            Text = language.T("options_reset_title"); Font = font; Icon = icon; Size = new Size(880, 680); MinimumSize = new Size(660, 480);
            StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, Padding = new Padding(16, 14, 16, 10) };
            shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
            Controls.Add(shell);
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true }; shell.Controls.Add(scroll, 0, 0);
            var stack = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 1, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink };
            stack.Width = scroll.ClientSize.Width - 4;
            scroll.SizeChanged += (s, e) => { stack.Width = scroll.ClientSize.Width - 4; Rewrap(scroll.ClientSize.Width); };
            scroll.Controls.Add(stack);

            if (plan.Everything)
            {
                // Icon and text carry the box tint, the way every other notice box does it -
                // a label without it paints the white of the parent and the box looks patched.
                var box = new NoticeBox("error");
                box.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 36)); box.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                var symbol = new NoticeIcon { Error = true, Size = new Size(24, 24), Margin = new Padding(0, 3, 10, 0), BackColor = box.Tint };
                Label warning = Theme.Label(language.T("options_reset_warning"), 10, false);
                warning.AutoSize = true; warning.Dock = DockStyle.Top; warning.Margin = new Padding(0, 3, 0, 0);
                warning.ForeColor = Color.FromArgb(150, 28, 28); warning.BackColor = box.Tint; Wrap(warning, 120);
                box.Controls.Add(symbol, 0, 0); box.Controls.Add(warning, 1, 0);
                stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(box);
            }
            // The saved games first and on their own: the only loss that cannot be undone.
            if (plan.Saves.Count > 0)
            {
                Label head = Theme.Label(language.Format("options_reset_saves", plan.Saves.Count), 11, true);
                head.ForeColor = Color.FromArgb(150, 28, 28); head.Margin = new Padding(2, 6, 2, 4); head.AutoSize = true;
                stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(head);
                // One chip per saved game, wrapping across the width. A single joined line was
                // unreadable here, because the folder names carry a dash themselves
                // ("18826 - Logistik Test") and ran into each other.
                var names = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top, WrapContents = true, Margin = new Padding(2, 0, 2, 10), Padding = Padding.Empty };
                foreach (string save in plan.Saves)
                {
                    Label chip = Theme.Label("•  " + save, 9.5f, false);
                    chip.ForeColor = Color.FromArgb(150, 28, 28); chip.AutoSize = true; chip.Margin = new Padding(0, 2, 24, 2);
                    names.Controls.Add(chip);
                }
                stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(names);
            }
            else if (plan.Everything) Plain(stack, language.T("options_reset_saves_none"), Theme.Muted, 2);

            // Everything else as a table: kind on the left, what happens to it on the right.
            var table = new TableLayoutPanel { ColumnCount = 2, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Dock = DockStyle.Top, Margin = new Padding(0, 2, 0, 0) };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190)); table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            if (plan.Packages.Count > 0) Entry(table, language.T("options_reset_packages"), Join(plan.Packages));
            if (plan.Ids.Count > 0) Entry(table, language.T("options_reset_ids"), Join(plan.Ids));
            if (plan.Files.Count > 0) Entry(table, language.T("options_reset_caption_files"), language.Format("options_reset_files", plan.Files.Count));
            if (plan.Folders.Count > 0) Entry(table, language.T("options_reset_caption_folders"), language.Format("options_reset_folders", Join(plan.Folders)));
            if (plan.Restored.Count > 0) Entry(table, language.T("options_reset_caption_restored"), language.Format("options_reset_restored", Join(plan.Restored)));
            if (plan.Edited.Count > 0) Entry(table, language.T("options_reset_caption_edited"), language.Format("options_reset_edited", Join(plan.Edited)));
            if (plan.Buildings.Count > 0) Entry(table, language.T("options_reset_caption_buildings"), language.Format("options_reset_buildings", Join(plan.Buildings)));
            if (plan.Overlays.Count > 0) Entry(table, language.T("options_reset_caption_overlay"), language.Format("options_reset_overlay", SmlOverlay.Name, Join(plan.Overlays)));
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(table);

            // A rule, then what stays: it must not read like part of the list above.
            var rule = new Panel { Height = 1, Dock = DockStyle.Top, BackColor = Theme.Line, Margin = new Padding(0, 26, 0, 14) };
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(rule);
            Label keepsTitle = Theme.Label(language.T("options_reset_keeps_title"), 10, true);
            keepsTitle.ForeColor = Theme.Muted; keepsTitle.Margin = new Padding(2, 0, 2, 4); keepsTitle.AutoSize = true;
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(keepsTitle);
            Plain(stack, language.T("options_reset_keeps"), Theme.Muted, 2);
            Plain(stack, language.T("options_reset_backup_note"), Theme.Muted, 2);

            var footer = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false };
            var go = Theme.Button(plan.Everything ? language.T("options_reset_go") : language.T("options_reset_data"), () => { DialogResult = DialogResult.OK; Close(); }, false);
            if (plan.Everything) { go.BackColor = Color.FromArgb(196, 43, 43); go.ForeColor = Color.White; go.FlatAppearance.BorderSize = 0; go.FlatAppearance.MouseOverBackColor = Color.FromArgb(166, 30, 30); }
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, true);
            footer.Controls.Add(cancel); footer.Controls.Add(go);
            shell.Controls.Add(footer, 0, 1);
            AcceptButton = null; CancelButton = cancel; ActiveControl = cancel;   // Enter never deletes
        }
        static string Join(System.Collections.Generic.List<string> list) { return String.Join(" · ", list.ToArray()); }
        void Entry(TableLayoutPanel table, string caption, string value)
        {
            // A hairline between the rows: the grid style of WinForms draws a full box, this
            // gives the table its horizontal rules and nothing else.
            if (table.RowCount > 0)
            {
                var rule = new Panel { Height = 1, Dock = DockStyle.Top, BackColor = Theme.Line, Margin = new Padding(2, 0, 2, 0) };
                int line = table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                table.Controls.Add(rule, 0, line); table.SetColumnSpan(rule, 2);
            }
            Label left = Theme.Label(caption, 9.5f, true); left.AutoSize = true; left.Margin = new Padding(2, 9, 10, 9); left.MaximumSize = new Size(180, 0);
            Label right = Theme.Label(value, 9.5f, false); right.AutoSize = true; right.Margin = new Padding(0, 9, 2, 9); Wrap(right, 250);
            int row = table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            table.Controls.Add(left, 0, row); table.Controls.Add(right, 1, row);
        }
        void Plain(TableLayoutPanel stack, string text, Color colour, int indent)
        {
            if (String.IsNullOrEmpty(text)) return;
            Label label = Theme.Label(text, 9.5f, false);
            label.ForeColor = colour; label.AutoSize = true; label.Margin = new Padding(indent, 2, indent, 4); Wrap(label, 60);
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(label);
        }
        // Wrapping labels use the full width of the window minus what sits beside them
        // (icon column, caption column, scrollbar) - they must not stay squeezed in the middle.
        void Wrap(Label label, int inset) { label.Tag = inset; wrapped.Add(label); label.MaximumSize = new Size(Math.Max(240, 860 - inset), 0); }
        void Rewrap(int width)
        {
            foreach (Label label in wrapped)
            {
                int inset = label.Tag is int ? (int)label.Tag : 60;
                label.MaximumSize = new Size(Math.Max(240, width - inset), 0);
            }
        }
    }

    // Step three: the word has to be typed. Ceremony that cannot be clicked away by accident.
    sealed class ResetWordWindow : Form
    {
        readonly TextBox field = new TextBox { Width = 220, Font = Fields.Font, BorderStyle = BorderStyle.None };
        readonly Button go;
        public ResetWordWindow(Language language, Font font, Icon icon)
        {
            string word = language.T("options_reset_word");
            Text = language.T("options_reset_sure"); Font = font; Icon = icon; Size = new Size(560, 300);
            FormBorderStyle = FormBorderStyle.FixedDialog; MaximizeBox = false; MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            Theme.ApplyWindowChrome(this);
            var stack = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4, Padding = new Padding(18, 16, 18, 12) };
            Controls.Add(stack);
            Label heading = Theme.Label(language.T("options_reset_sure"), 13, true); heading.AutoSize = true; heading.Margin = new Padding(0, 0, 0, 6);
            Label hint = Theme.Label(language.Format("options_reset_sure_hint", word), 9.5f, false);
            hint.ForeColor = Theme.Muted; hint.AutoSize = true; hint.MaximumSize = new Size(480, 0); hint.Margin = new Padding(0, 0, 0, 12);
            stack.Controls.Add(heading); stack.Controls.Add(hint);
            var box = Fields.Wrap(field); box.Margin = new Padding(0, 0, 0, 12); stack.Controls.Add(box);
            var footer = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Height = 48 };
            go = Theme.Button(language.T("options_reset_final"), () => { DialogResult = DialogResult.OK; Close(); }, false);
            go.Enabled = false; go.BackColor = Color.FromArgb(214, 176, 176); go.ForeColor = Color.White; go.FlatAppearance.BorderSize = 0;
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, true);
            footer.Controls.Add(cancel); footer.Controls.Add(go); stack.Controls.Add(footer);
            field.TextChanged += delegate
            {
                bool ok = field.Text.Trim().Equals(word, StringComparison.CurrentCultureIgnoreCase);
                go.Enabled = ok; go.BackColor = ok ? Color.FromArgb(196, 43, 43) : Color.FromArgb(214, 176, 176);
                go.FlatAppearance.MouseOverBackColor = ok ? Color.FromArgb(166, 30, 30) : go.BackColor;
            };
            AcceptButton = null; CancelButton = cancel; ActiveControl = cancel;
        }
        internal void TestType(string text) { field.Text = text; }
        internal bool TestReady { get { return go.Enabled; } }
    }
}
