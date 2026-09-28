using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // The log window: Republic Mod Manager' own journal, tesmioloader.log and every
    // tesmioloader.<plugin>.log from the loader folder, with a text filter, a
    // plugin filter, a "problems only" switch and colour for problems and
    // warnings. Files are read with shared access, so the window works while
    // the game runs; "Refresh" re-reads them.
    sealed class LogWindow : Form
    {
        readonly Language language; readonly string build; readonly Func<string> journal;
        readonly ListBox sources = new ListBox(); readonly TextBox filter = new TextBox(); readonly ComboBox plugin = new ComboBox();
        readonly CheckBox problemsOnly = new CheckBox(); readonly LogList view = new LogList(); readonly Label summary = Theme.Label("", 9, false);
        readonly List<GameLogs.Source> list = new List<GameLogs.Source>();
        string[] lines = new string[0]; bool loading;

        // One line per item, coloured by kind; Ctrl+C copies the selection, Ctrl+A selects all.
        sealed class LogList : ListBox
        {
            public readonly List<GameLogs.Kind> Kinds = new List<GameLogs.Kind>();
            public LogList()
            {
                DrawMode = DrawMode.OwnerDrawFixed; ItemHeight = 18; IntegralHeight = false; SelectionMode = SelectionMode.MultiExtended; HorizontalScrollbar = true;
                BorderStyle = BorderStyle.FixedSingle; BackColor = Theme.Pale; ForeColor = Theme.Ink; Font = new Font("Consolas", 9.5f);
            }
            public void Fill(List<string> items, List<GameLogs.Kind> kinds)
            {
                BeginUpdate();
                try
                {
                    Items.Clear(); Kinds.Clear(); int widest = 0;
                    foreach (string s in items) { Items.Add(s); widest = Math.Max(widest, s.Length); }
                    Kinds.AddRange(kinds); HorizontalExtent = widest * 8 + 40;
                }
                finally { EndUpdate(); }
            }
            protected override void OnDrawItem(DrawItemEventArgs e)
            {
                if (e.Index < 0 || e.Index >= Items.Count) return;
                bool selected = (e.State & DrawItemState.Selected) != 0;
                GameLogs.Kind kind = e.Index < Kinds.Count ? Kinds[e.Index] : GameLogs.Kind.Plain;
                Color back = selected ? Theme.SelectionBlue : (kind == GameLogs.Kind.Problem ? Color.FromArgb(255, 236, 236) : kind == GameLogs.Kind.Warning ? Color.FromArgb(255, 247, 225) : BackColor);
                Color fore = selected ? Color.White : (kind == GameLogs.Kind.Problem ? Theme.Danger : kind == GameLogs.Kind.Warning ? Color.FromArgb(140, 92, 0) : ForeColor);
                using (var fill = new SolidBrush(back)) e.Graphics.FillRectangle(fill, e.Bounds);
                TextRenderer.DrawText(e.Graphics, (string)Items[e.Index], kind == GameLogs.Kind.Plain ? Font : new Font(Font, FontStyle.Bold), new Rectangle(e.Bounds.X + 4, e.Bounds.Y, Math.Max(e.Bounds.Width, HorizontalExtent), e.Bounds.Height), fore, TextFormatFlags.NoPrefix | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine);
            }
            protected override void OnKeyDown(KeyEventArgs e)
            {
                if (e.Control && e.KeyCode == Keys.C) { var sb = new StringBuilder(); foreach (object o in SelectedItems) sb.AppendLine((string)o); if (sb.Length > 0) Clipboard.SetText(sb.ToString()); e.Handled = true; return; }
                if (e.Control && e.KeyCode == Keys.A) { BeginUpdate(); for (int i = 0; i < Items.Count; i++) SetSelected(i, true); EndUpdate(); e.Handled = true; return; }
                base.OnKeyDown(e);
            }
        }

        public LogWindow(Language language, string build, Func<string> journal, Font font, Icon icon)
        {
            this.language = language; this.build = build; this.journal = journal;
            Text = language.T("log"); Font = font; Icon = icon; Size = new Size(1180, 700); MinimumSize = new Size(820, 480); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 3, Padding = new Padding(12) };
            shell.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 250)); shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 30));
            Controls.Add(shell);

            var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            var filterLabel = Theme.Label(language.T("log_filter"), 10, false); filterLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(filterLabel);
            filter.Width = 220; filter.Margin = new Padding(0, 4, 14, 0); filter.TextChanged += delegate { Render(); }; bar.Controls.Add(filter);
            var pluginLabel = Theme.Label(language.T("log_plugin"), 10, false); pluginLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(pluginLabel);
            plugin.DropDownStyle = ComboBoxStyle.DropDownList; plugin.Width = 190; plugin.Margin = new Padding(0, 4, 14, 0); plugin.SelectedIndexChanged += delegate { if (!loading) Render(); }; bar.Controls.Add(plugin);
            problemsOnly.Text = language.T("log_problems_only"); problemsOnly.AutoSize = true; problemsOnly.Margin = new Padding(0, 8, 14, 0); problemsOnly.CheckedChanged += delegate { Render(); }; bar.Controls.Add(problemsOnly);
            var refresh = Theme.Button(language.T("log_refresh"), Reload, false); refresh.Margin = new Padding(0, 2, 8, 0); bar.Controls.Add(refresh);
            var open = Theme.Button(language.T("log_open_folder"), OpenFolder, false); open.Margin = new Padding(0, 2, 0, 0); bar.Controls.Add(open);
            shell.Controls.Add(bar, 0, 0); shell.SetColumnSpan(bar, 2);

            sources.Dock = DockStyle.Fill; sources.Margin = new Padding(0, 0, 12, 0); sources.IntegralHeight = false; sources.SelectedIndexChanged += delegate { if (!loading) LoadSelected(); }; shell.Controls.Add(sources, 0, 1);
            view.Dock = DockStyle.Fill; view.Margin = Padding.Empty; shell.Controls.Add(view, 1, 1);
            summary.ForeColor = Theme.Muted; summary.Dock = DockStyle.Fill; summary.Margin = new Padding(0, 8, 0, 0); shell.Controls.Add(summary, 0, 2); shell.SetColumnSpan(summary, 2);
            Reload();
        }

        void Reload()
        {
            loading = true;
            try
            {
                int keep = sources.SelectedIndex;
                list.Clear(); sources.Items.Clear();
                list.Add(new GameLogs.Source { Name = language.T("log_journal"), Path = "" });
                list.AddRange(GameLogs.Sources(build));
                foreach (var s in list) sources.Items.Add(s.Name);
                sources.SelectedIndex = keep >= 0 && keep < list.Count ? keep : (list.Count > 1 ? 1 : 0);
            }
            finally { loading = false; }
            LoadSelected();
        }
        void LoadSelected()
        {
            int at = sources.SelectedIndex; if (at < 0 || at >= list.Count) { lines = new string[0]; Render(); return; }
            GameLogs.Source source = list[at];
            string text = source.Path.Length == 0 ? journal() : GameLogs.Read(source.Path);
            lines = text.Replace("\r\n", "\n").Split('\n').Where(l => l.Length > 0).ToArray();
            loading = true;
            try
            {
                string chosen = plugin.SelectedItem as string;
                plugin.Items.Clear(); plugin.Items.Add(language.T("log_plugin_all"));
                foreach (string name in GameLogs.Subjects(lines)) plugin.Items.Add(name);
                plugin.SelectedIndex = chosen != null && plugin.Items.Contains(chosen) ? plugin.Items.IndexOf(chosen) : 0;
            }
            finally { loading = false; }
            Render();
        }
        void Render()
        {
            string needle = filter.Text.Trim(); string subject = plugin.SelectedIndex > 0 ? (string)plugin.SelectedItem : null;
            var shown = new List<string>(); var kinds = new List<GameLogs.Kind>(); int problems = 0, warnings = 0;
            foreach (string line in lines)
            {
                GameLogs.Kind kind = GameLogs.Classify(line);
                if (kind == GameLogs.Kind.Problem) problems++; else if (kind == GameLogs.Kind.Warning) warnings++;
                if (subject != null && !String.Equals(GameLogs.SubjectOf(line), subject, StringComparison.OrdinalIgnoreCase)) continue;
                if (problemsOnly.Checked && kind == GameLogs.Kind.Plain) continue;
                if (needle.Length > 0 && line.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0) continue;
                shown.Add(line); kinds.Add(kind);
            }
            view.Fill(shown, kinds);
            if (view.Items.Count > 0) view.TopIndex = view.Items.Count - 1;     // the newest lines are what one came for
            summary.Text = language.Format("log_summary", lines.Length, shown.Count, problems, warnings, GameLogs.LastRun(lines));
        }
        void OpenFolder()
        {
            try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("explorer.exe", "\"" + build + "\"") { UseShellExecute = true }); }
            catch (Exception e) { MessageWindow.Show(this, language, Text, language.Localize(e.Message), MessageWindow.Kind.Warning, DialogResult.OK); }
        }
    }
}
