using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // 0.5.5 step 3: the window that makes personal changes to a generated building.ini.
    // Left the lines the generator writes - that is what the changes are anchored to, so the
    // list never shows the result; a line the player already changed is marked instead.
    // Right the changes themselves, in the order they are applied.
    sealed class WipEditWindow : Form
    {
        readonly Language language; readonly WipEdit edit; readonly List<string> baseline;
        readonly ListBox lines = new ListBox(), changes = new ListBox();
        readonly TextBox search = new TextBox(), text = new TextBox();
        public bool Reset { get; private set; }

        public WipEditWindow(Language language, WipBuilding entry, WipEdit edit, Font font, Icon icon)
        {
            this.language = language; this.edit = edit; baseline = WipEdits.Split(edit.Baseline);
            Text = language.T("wip_edit_title"); Font = font; Icon = icon; Size = new Size(1100, 700); MinimumSize = new Size(860, 560);
            StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White; Theme.ApplyWindowChrome(this);
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(12) };
            Controls.Add(shell);
            Action<Control, RowStyle> row = (control, style) => { shell.RowStyles.Add(style); control.Dock = DockStyle.Fill; shell.Controls.Add(control, 0, shell.RowStyles.Count - 1); };

            var header = Theme.Label(entry.Display + "  ·  " + entry.Id + (entry.Object.Length > 0 ? "  ·  " + entry.Object : ""), 12, true);
            header.Margin = new Padding(0, 0, 0, 2); row(header, new RowStyle(SizeType.AutoSize));
            var hint = Theme.Label(language.T("wip_edit_hint"), 9, false); hint.ForeColor = Theme.Muted; hint.Margin = new Padding(0, 0, 0, 8);
            row(hint, new RowStyle(SizeType.AutoSize));
            // The hint is a sentence, not a caption: it has to wrap at the window's width.
            Action rewrap = () => hint.MaximumSize = new Size(Math.Max(240, ClientSize.Width - 28), 0);
            Resize += delegate { rewrap(); }; rewrap();

            var split = new TableLayoutPanel { ColumnCount = 2, Margin = Padding.Empty };
            split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58)); split.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
            split.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            row(split, new RowStyle(SizeType.Percent, 100));

            var left = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Margin = new Padding(0, 0, 8, 0) };
            left.RowStyles.Add(new RowStyle(SizeType.AutoSize)); left.RowStyles.Add(new RowStyle(SizeType.AutoSize)); left.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var leftTitle = Theme.Label(language.T("wip_edit_lines"), 10, true); leftTitle.Margin = new Padding(0, 0, 0, 4); left.Controls.Add(leftTitle, 0, 0);
            search.AccessibleName = "wip-search"; var searchHost = Fields.Wrap(search); searchHost.Dock = DockStyle.Top; searchHost.Margin = new Padding(0, 0, 0, 6);
            search.TextChanged += delegate { FillLines(); };
            left.Controls.Add(searchHost, 0, 1);
            Style(lines, "wip-lines"); lines.Dock = DockStyle.Fill; left.Controls.Add(lines, 0, 2);
            lines.SelectedIndexChanged += delegate { string picked = Selected(); if (picked != null) text.Text = picked; };
            split.Controls.Add(left, 0, 0);

            var right = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Margin = Padding.Empty };
            right.RowStyles.Add(new RowStyle(SizeType.AutoSize)); right.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); right.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            var rightTitle = Theme.Label(language.T("wip_edit_changes"), 10, true); rightTitle.Margin = new Padding(0, 0, 0, 4); right.Controls.Add(rightTitle, 0, 0);
            Style(changes, "wip-changes"); changes.Dock = DockStyle.Fill; right.Controls.Add(changes, 0, 1);
            var dropBar = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 6, 0, 0) };
            var drop = Button("wip_edit_drop", "wip-drop", Drop); dropBar.Controls.Add(drop);
            right.Controls.Add(dropBar, 0, 2);
            split.Controls.Add(right, 1, 0);

            // One text box for both commands: a selected line fills it, so replacing is two clicks.
            var editRow = new TableLayoutPanel { ColumnCount = 5, Margin = new Padding(0, 10, 0, 0) };
            editRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            for (int i = 0; i < 4; i++) editRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            // 0.5.8: multi-line, because a block goes in here whole - a token with its data lines.
            text.Font = new Font("Consolas", 10f); text.AccessibleName = "wip-new";
            text.Multiline = true; text.ScrollBars = ScrollBars.Vertical; text.BorderStyle = BorderStyle.FixedSingle;
            text.Dock = DockStyle.Fill; text.Margin = new Padding(0, 0, 8, 0);
            editRow.Controls.Add(text, 0, 0);
            var commands = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, WrapContents = false, Margin = Padding.Empty, Anchor = AnchorStyles.Top | AnchorStyles.Right };
            commands.Controls.Add(Button("wip_edit_replace", "wip-replace", Replace));
            commands.Controls.Add(Button("wip_edit_add", "wip-add", Add));
            commands.Controls.Add(Button("wip_edit_remove", "wip-remove", Remove));
            editRow.Controls.Add(commands, 1, 0);
            row(editRow, new RowStyle(SizeType.Absolute, 104));

            var buttons = new TableLayoutPanel { ColumnCount = 2, Margin = new Padding(0, 10, 0, 0) };
            buttons.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); buttons.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var resetButton = Button("wip_edit_reset", "wip-reset", AskReset); buttons.Controls.Add(resetButton, 0, 0);
            var right2 = new FlowLayoutPanel { FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Dock = DockStyle.Fill };
            var cancel = Button("cancel", "wip-cancel", () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Button("apply", "wip-apply", () => { DialogResult = DialogResult.OK; Close(); }, true); apply.Margin = Padding.Empty;
            right2.Controls.Add(apply); right2.Controls.Add(cancel); buttons.Controls.Add(right2, 1, 0);
            row(buttons, new RowStyle(SizeType.Absolute, 52)); AcceptButton = apply; CancelButton = cancel;
            FillLines(); FillChanges();
        }

        Button Button(string key, string name, Action action) { return Button(key, name, action, false); }
        // Every button of this window is the same height. Theme.Button carries MinimumSize 80x38
        // and grows with its own text, so a long caption ("Abbrechen") came out taller than a
        // short one - the row looked ragged.
        Button Button(string key, string name, Action action, bool primary)
        {
            var button = Theme.Button(language.T(key), action, primary);
            button.AutoSize = false; button.MinimumSize = Size.Empty; button.Height = Fields.Height;
            button.Width = TextRenderer.MeasureText(button.Text, new Font("Segoe UI", 10)).Width + 30;
            button.Margin = new Padding(0, 0, 8, 0); button.AccessibleName = name; return button;
        }
        static void Style(ListBox list, string name)
        {
            list.Font = new Font("Consolas", 10f); list.IntegralHeight = false; list.AccessibleName = name; list.BorderStyle = BorderStyle.FixedSingle;
            // A building.ini line is wider than any sensible window, so the lists scroll sideways
            // instead of cutting the end off. The extent has to be set by hand after every fill -
            // a ListBox does not measure its own items.
            list.HorizontalScrollbar = true;
        }
        static void Extent(ListBox list)
        {
            int widest = 0;
            using (Graphics g = list.CreateGraphics())
                foreach (object item in list.Items)
                {
                    int width = TextRenderer.MeasureText(g, Convert.ToString(item), list.Font, new Size(Int32.MaxValue, Int32.MaxValue), TextFormatFlags.NoPadding).Width;
                    if (width > widest) widest = width;
                }
            list.HorizontalExtent = widest + 12;
        }
        // A line the player already touched is marked, so the list of the generator's lines still
        // says what has been done to it.
        void FillLines()
        {
            string needle = search.Text.Trim();
            lines.BeginUpdate(); lines.Items.Clear();
            foreach (string line in baseline)
            {
                if (needle.Length > 0 && line.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0) continue;
                bool touched = edit.Operations.Any(o => o.Kind != "add" && o.AnchorLines.Any(x => x.Trim() == line.Trim()));
                lines.Items.Add((touched ? "* " : "  ") + line);
            }
            lines.EndUpdate(); Extent(lines);
        }
        void FillChanges()
        {
            changes.BeginUpdate(); changes.Items.Clear();
            foreach (WipOperation op in edit.Operations) changes.Items.Add(op.ToString());
            changes.EndUpdate(); Extent(changes); FillLines();
        }
        // 0.5.8: what is picked is the whole block the selected line belongs to. With a filter in
        // the search box the list is a subset, so the line is looked up in the baseline again.
        string Selected()
        {
            string picked = lines.SelectedItem as string;
            if (picked == null) return null;
            string line = picked.Length > 2 ? picked.Substring(2) : "";
            int at = baseline.FindIndex(x => x == line);
            return at < 0 ? line : WipEdits.BlockAt(baseline, at);
        }
        void Say(string key) { MessageWindow.Show(this, language, Text, language.T(key), MessageWindow.Kind.Info, DialogResult.OK); }
        // A "|" is no longer forbidden: a receipt that contains one is written as sections, where
        // the separator plays no part.
        bool NewText(out string value)
        {
            value = text.Text.Trim();
            if (value.Length == 0) { Say("wip_edit_need_new"); return false; }
            return true;
        }
        void Replace()
        {
            string anchor = Selected(); string value;
            if (anchor == null) { Say("wip_edit_need_line"); return; }
            if (!NewText(out value)) return;
            if (value.Trim() == anchor.Trim()) { Say("wip_edit_same"); return; }
            Forget(anchor);
            edit.Operations.Add(new WipOperation { Kind = "replace", Anchor = anchor.Trim(), Value = value });
            FillChanges();
        }
        void Remove()
        {
            string anchor = Selected();
            if (anchor == null) { Say("wip_edit_need_line"); return; }
            if (anchor.Trim().Length == 0) { Say("wip_edit_need_line"); return; }
            Forget(anchor);
            edit.Operations.Add(new WipOperation { Kind = "remove", Anchor = anchor.Trim() });
            FillChanges();
        }
        void Add()
        {
            string value;
            if (!NewText(out value)) return;
            edit.Operations.Add(new WipOperation { Kind = "add", Value = value });
            FillChanges();
        }
        // One anchor, one change: a second command on the same line would never match.
        void Forget(string anchor)
        {
            var block = WipOperation.Cut(anchor).Select(x => x.Trim()).ToList();
            edit.Operations.RemoveAll(o => o.Kind != "add" && o.AnchorLines.Select(x => x.Trim()).Any(block.Contains));
        }
        void Drop()
        {
            int at = changes.SelectedIndex;
            if (at < 0 || at >= edit.Operations.Count) { Say("wip_edit_need_change"); return; }
            edit.Operations.RemoveAt(at); FillChanges();
        }
        void AskReset()
        {
            if (MessageWindow.Show(this, language, Text, language.T("wip_edit_reset_question"), MessageWindow.Kind.Question, DialogResult.Yes, DialogResult.No) != DialogResult.Yes) return;
            Reset = true; DialogResult = DialogResult.OK; Close();
        }

        // Test hooks.
        internal int LineCount { get { return lines.Items.Count; } }
        internal int ChangeCount { get { return changes.Items.Count; } }
        internal string ChangeText(int index) { return index >= 0 && index < changes.Items.Count ? Convert.ToString(changes.Items[index]) : null; }
        internal int ChangeExtent { get { return changes.HorizontalExtent; } }
        internal void TestReplace(int index, string value) { lines.SelectedIndex = index; text.Text = value; Replace(); }
        internal void TestRemove(int index) { lines.SelectedIndex = index; Remove(); }
        internal void TestAdd(string value) { text.Text = value; Add(); }
        internal void TestDrop(int index) { changes.SelectedIndex = index; Drop(); }
        internal void TestSearch(string needle) { search.Text = needle; }
        internal void TestApply() { DialogResult = DialogResult.OK; Close(); }
    }
}
