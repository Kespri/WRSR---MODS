// Republic Mod Manager - "Choose a research" window (0.4.29): the game's
// research tree by id and name, plus the position of the automatic unlock in
// the chosen parent (normal, before or after one of the parent's own unlocks).
// The result is one `requires` line of a [research:] section:
// "<parent>" or "<parent> | before | <anchor>".
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    sealed class ResearchPickerWindow : Form
    {
        readonly Language language; readonly string game;
        readonly TextBox search = new TextBox(); readonly ListView list = new ListView(); readonly Label counter = Theme.Label("", 10, true);
        readonly ComboBox position = new ComboBox(); readonly ComboBox anchor = new ComboBox(); readonly Label anchorLabel;
        List<ResearchEntry> entries = new List<ResearchEntry>(); string problem = "";
        readonly bool idOnly; readonly HashSet<string> exclude;   // 0.4.42: choose an id only, some ids hidden
        public string Result { get; private set; }

        public ResearchPickerWindow(Language language, string build, IEnumerable<string> ownIds, Font font, Icon icon) : this(language, build, ownIds, font, icon, false, null) { }
        // idOnly: no position row, the result is the bare id (0.4.42). excludeIds: ids not offered.
        public ResearchPickerWindow(Language language, string build, IEnumerable<string> ownIds, Font font, Icon icon, bool idOnly, IEnumerable<string> excludeIds)
        {
            this.language = language; this.idOnly = idOnly; exclude = new HashSet<string>(excludeIds ?? new string[0], StringComparer.OrdinalIgnoreCase);
            game = GameBuildings.GameRoot(build);
            Text = language.T("research_picker_title"); Font = font; Icon = icon; Size = new Size(900, 700); MinimumSize = new Size(700, 480); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(12) };
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 44)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 26)); shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 50)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 50));
            Controls.Add(shell);
            var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            var searchLabel = Theme.Label(language.T("research_picker_search"), 10, false); searchLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(searchLabel);
            search.Width = 320; search.Margin = new Padding(0, 4, 14, 0); search.AccessibleName = "research-search"; search.TextChanged += delegate { ApplyFilter(); }; bar.Controls.Add(search);
            shell.Controls.Add(bar, 0, 0);
            counter.Dock = DockStyle.Fill; counter.Margin = new Padding(0, 4, 0, 0); shell.Controls.Add(counter, 0, 1);
            list.View = View.Details; list.FullRowSelect = true; list.HideSelection = false; list.MultiSelect = false; list.Dock = DockStyle.Fill; list.Margin = new Padding(0, 4, 0, 0); list.AccessibleName = "research-list";
            list.Columns.Add(language.T("research_picker_col_id"), 300); list.Columns.Add(language.T("research_picker_col_name"), 420); list.Columns.Add(language.T("research_picker_col_unlocks"), 90);
            list.SelectedIndexChanged += delegate { FillAnchors(); };
            list.DoubleClick += delegate { Apply(); };
            shell.Controls.Add(list, 0, 2);
            // Position of the automatic $UNLOCK_RESEARCH line in the parent: normal (last),
            // or before / after one of the parent's existing unlocks.
            var positionBar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            var positionLabel = Theme.Label(language.T("research_picker_position"), 10, false); positionLabel.Margin = new Padding(0, 12, 6, 0); positionBar.Controls.Add(positionLabel);
            position.DropDownStyle = ComboBoxStyle.DropDownList; position.Width = 150; position.Margin = new Padding(0, 8, 14, 0); position.AccessibleName = "research-position";
            position.Items.Add(language.T("research_pos_normal")); position.Items.Add(language.T("research_pos_before")); position.Items.Add(language.T("research_pos_after")); position.SelectedIndex = 0;
            position.SelectedIndexChanged += delegate { anchor.Enabled = position.SelectedIndex > 0; }; positionBar.Controls.Add(position);
            anchorLabel = Theme.Label(language.T("research_picker_anchor"), 10, false); anchorLabel.Margin = new Padding(0, 12, 6, 0); positionBar.Controls.Add(anchorLabel);
            anchor.DropDownStyle = ComboBoxStyle.DropDownList; anchor.Width = 340; anchor.Margin = new Padding(0, 8, 0, 0); anchor.AccessibleName = "research-anchor"; anchor.Enabled = false; positionBar.Controls.Add(anchor);
            shell.Controls.Add(positionBar, 0, 3);
            if (idOnly) { positionBar.Visible = false; shell.RowStyles[3].Height = 0; }
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Margin = new Padding(0, 10, 0, 0) };
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Theme.Button(language.T("apply"), Apply, true);
            buttons.Controls.Add(apply); buttons.Controls.Add(cancel); shell.Controls.Add(buttons, 0, 4); AcceptButton = apply; CancelButton = cancel;
            if (game == null) problem = language.T("research_picker_no_game");
            else
            {
                try { entries = GameResearch.Scan(game, GameTexts.DefaultLanguage(game, language.Code)); }
                catch (Exception e) { problem = Msg.Plain(e.Message); }
            }
            entries = entries.Where(e => !exclude.Contains(e.Id)).ToList();
            // The player's own research (from the INI) can be a parent as well; it carries
            // no name and no unlock list.
            foreach (string own in (ownIds ?? new string[0]).Where(x => !String.IsNullOrWhiteSpace(x)).Distinct(StringComparer.OrdinalIgnoreCase))
                if (!entries.Any(e => e.Id.Equals(own, StringComparison.OrdinalIgnoreCase))) entries.Add(new ResearchEntry { Id = own, Name = language.T("research_picker_own"), Own = true });
            ApplyFilter();
        }

        void ApplyFilter()
        {
            string query = search.Text.Trim();
            list.BeginUpdate(); list.Items.Clear();
            foreach (ResearchEntry entry in entries)
            {
                if (query.Length > 0 && entry.Id.IndexOf(query, StringComparison.OrdinalIgnoreCase) < 0 && entry.Name.IndexOf(query, StringComparison.OrdinalIgnoreCase) < 0) continue;
                var item = new ListViewItem(entry.Id) { Tag = entry };
                item.SubItems.Add(entry.Name); item.SubItems.Add(entry.Unlocks.Count.ToString());
                list.Items.Add(item);
            }
            list.EndUpdate();
            counter.Text = problem.Length > 0 ? problem : language.Format("research_picker_count", list.Items.Count, entries.Count);
            FillAnchors();
        }

        ResearchEntry Selected { get { return list.SelectedItems.Count == 0 ? null : list.SelectedItems[0].Tag as ResearchEntry; } }

        void FillAnchors()
        {
            anchor.Items.Clear();
            ResearchEntry entry = Selected;
            if (entry != null) foreach (string unlock in entry.Unlocks)
            {
                ResearchEntry target = entries.FirstOrDefault(e => e.Id.Equals(unlock, StringComparison.OrdinalIgnoreCase));
                anchor.Items.Add(new AnchorOption { Id = unlock, Caption = target != null ? target.Caption : unlock });
            }
            if (anchor.Items.Count > 0) anchor.SelectedIndex = 0;
            bool positional = anchor.Items.Count > 0;
            position.Enabled = positional; if (!positional) position.SelectedIndex = 0;
            anchor.Enabled = positional && position.SelectedIndex > 0;
        }

        sealed class AnchorOption { public string Id, Caption; public override string ToString() { return Caption; } }

        void Apply()
        {
            ResearchEntry entry = Selected;
            if (entry == null) return;
            if (!idOnly && position.SelectedIndex > 0)
            {
                var chosen = anchor.SelectedItem as AnchorOption;
                if (chosen == null) { MessageWindow.Show(this, language, Text, language.T("research_picker_need_anchor"), MessageWindow.Kind.Info, DialogResult.OK); return; }
                Result = entry.Id + " | " + (position.SelectedIndex == 1 ? "before" : "after") + " | " + chosen.Id;
            }
            else Result = entry.Id;
            DialogResult = DialogResult.OK; Close();
        }

        // Test hooks.
        internal int VisibleCount { get { return list.Items.Count; } }
        internal int LoadedCount { get { return entries.Count; } }
        internal void TestSetFilter(string text) { search.Text = text; ApplyFilter(); }
        internal void TestSelect(string id, int positionIndex, string anchorId)
        {
            if (!list.IsHandleCreated) { IntPtr forced = list.Handle; if (forced == IntPtr.Zero) return; }
            foreach (ListViewItem item in list.Items) if (((ResearchEntry)item.Tag).Id.Equals(id, StringComparison.OrdinalIgnoreCase)) { item.Selected = true; item.Focused = true; break; }
            FillAnchors();
            if (position.Enabled) position.SelectedIndex = positionIndex;
            if (anchorId != null) foreach (object option in anchor.Items) if (((AnchorOption)option).Id.Equals(anchorId, StringComparison.OrdinalIgnoreCase)) anchor.SelectedItem = option;
            Apply();
        }
    }
}
