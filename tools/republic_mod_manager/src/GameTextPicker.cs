// Republic Mod Manager - "Choose a game text" window (0.4.21): the captions of
// one game language with id, line count and longest line, filtered by text and
// minimum line length, so a text id for a wrap list can be picked instead of
// looked up by hand.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    sealed class GameTextPickerWindow : Form
    {
        readonly Language language; readonly string game; readonly HashSet<int> used;
        readonly ComboBox languages = new ComboBox(); readonly TextBox search = new TextBox(); readonly NumericUpDown minLength = new NumericUpDown();
        readonly ListView list = new ListView(); readonly Label counter = Theme.Label("", 10, true);
        List<GameTextEntry> entries = new List<GameTextEntry>(); string problem = "";
        public int? Result { get; private set; }

        public GameTextPickerWindow(Language language, string build, IEnumerable<int> usedIds, Font font, Icon icon)
        {
            this.language = language; used = new HashSet<int>(usedIds ?? new int[0]);
            game = GameBuildings.GameRoot(build);
            Text = language.T("text_picker_title"); Font = font; Icon = icon; Size = new Size(1100, 760); MinimumSize = new Size(800, 500); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4, Padding = new Padding(12) };
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 44)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 26)); shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
            Controls.Add(shell);
            // Filters: language, text search, minimum length of the longest line.
            var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            var languageLabel = Theme.Label(language.T("text_picker_language"), 10, false); languageLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(languageLabel);
            languages.DropDownStyle = ComboBoxStyle.DropDownList; languages.Width = 200; languages.Margin = new Padding(0, 4, 14, 0); languages.AccessibleName = "text-language"; bar.Controls.Add(languages);
            var searchLabel = Theme.Label(language.T("text_picker_search"), 10, false); searchLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(searchLabel);
            search.Width = 260; search.Margin = new Padding(0, 4, 14, 0); search.AccessibleName = "text-search"; search.TextChanged += delegate { ApplyFilter(); }; bar.Controls.Add(search);
            var minLabel = Theme.Label(language.T("text_picker_min_length"), 10, false); minLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(minLabel);
            minLength.Minimum = 0; minLength.Maximum = 400; minLength.Value = 60; minLength.Width = 70; minLength.Margin = new Padding(0, 4, 0, 0); minLength.AccessibleName = "text-min-length"; minLength.ValueChanged += delegate { ApplyFilter(); }; bar.Controls.Add(minLength);
            shell.Controls.Add(bar, 0, 0);
            counter.Dock = DockStyle.Fill; counter.Margin = new Padding(0, 4, 0, 0); shell.Controls.Add(counter, 0, 1);
            // The list: id, lines, longest line, first line of the text.
            list.View = View.Details; list.FullRowSelect = true; list.HideSelection = false; list.MultiSelect = false; list.Dock = DockStyle.Fill; list.Margin = new Padding(0, 4, 0, 0); list.AccessibleName = "text-list";
            list.Columns.Add(language.T("text_picker_col_id"), 90); list.Columns.Add(language.T("text_picker_col_lines"), 70); list.Columns.Add(language.T("text_picker_col_length"), 90); list.Columns.Add(language.T("text_picker_col_text"), 780);
            list.DoubleClick += delegate { Apply(); };
            shell.Controls.Add(list, 0, 2);
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Margin = new Padding(0, 10, 0, 0) };
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Theme.Button(language.T("apply"), Apply, true);
            buttons.Controls.Add(apply); buttons.Controls.Add(cancel); shell.Controls.Add(buttons, 0, 3); AcceptButton = apply; CancelButton = cancel;
            // Languages: the game's own setting first, otherwise the UI language.
            if (game == null) problem = language.T("text_picker_no_game");
            else
            {
                var names = GameTexts.Languages(game);
                foreach (string name in names) languages.Items.Add(name);
                string preset = GameTexts.DefaultLanguage(game, language.Code);
                if (preset != null) languages.SelectedItem = preset;
                if (names.Count == 0) problem = language.T("text_picker_no_languages");
            }
            languages.SelectedIndexChanged += delegate { LoadLanguage(); };
            LoadLanguage();
        }

        void LoadLanguage()
        {
            entries = new List<GameTextEntry>();
            if (game != null && languages.SelectedItem != null)
            {
                try { entries = GameTexts.Load(game, (string)languages.SelectedItem); problem = ""; }
                catch (Exception e) { problem = Msg.Plain(e.Message); }
            }
            ApplyFilter();
        }

        void ApplyFilter()
        {
            string query = search.Text.Trim(); int minimum = (int)minLength.Value;
            list.BeginUpdate(); list.Items.Clear();
            foreach (GameTextEntry entry in entries)
            {
                if (used.Contains(entry.Id) || entry.Longest < minimum) continue;
                if (query.Length > 0 && entry.Text.IndexOf(query, StringComparison.OrdinalIgnoreCase) < 0 && entry.Id.ToString() != query) continue;
                var item = new ListViewItem(entry.Id.ToString()) { Tag = entry.Id };
                item.SubItems.Add(entry.Lines.ToString()); item.SubItems.Add(entry.Longest.ToString());
                string shown = entry.FirstLine.Length > 140 ? entry.FirstLine.Substring(0, 140) + "…" : entry.FirstLine;
                item.SubItems.Add(shown);
                list.Items.Add(item);
            }
            list.EndUpdate();
            counter.Text = problem.Length > 0 ? problem : language.Format("text_picker_count", list.Items.Count, entries.Count);
        }

        void Apply()
        {
            if (list.SelectedItems.Count == 0) return;
            Result = (int)list.SelectedItems[0].Tag; DialogResult = DialogResult.OK; Close();
        }

        // Test hooks.
        internal int VisibleCount { get { return list.Items.Count; } }
        internal int LoadedCount { get { return entries.Count; } }
        internal string SelectedLanguage { get { return languages.SelectedItem as string; } }
        internal void TestSetFilter(string text, int minimum) { minLength.Value = minimum; search.Text = text; ApplyFilter(); }
        internal void TestSelect(int id)
        {
            // A ListView reports selection only once its handle exists.
            if (!list.IsHandleCreated) { IntPtr forced = list.Handle; if (forced == IntPtr.Zero) return; }
            foreach (ListViewItem item in list.Items) if ((int)item.Tag == id) { item.Selected = true; item.Focused = true; Apply(); return; }
        }
    }
}
