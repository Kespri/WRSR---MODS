// Republic Mod Manager - "Choose a line" window (0.4.42): the lines of one Vanilla
// research block (media_soviet\research\research.ini), so the edit commands of a
// [modify:] section are written against the lines the game really has instead of
// being typed from memory. The field's picker_format decides the shape of the result:
//   line         "<line>"                        remove
//   line_edit    "<line> | <edited copy>"        replace
//   line_anchor  "<line> | <anchor line>"        move_before / move_after
//   anchor_edit  "<anchor line> | <new line>"    insert_before / insert_after
//   edit         "<new line>"                    add
// "Research as $UNLOCK_RESEARCH..." fills the new line from the research picker; the
// player's own research counts as a target there.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    sealed class ResearchLinesWindow : Form
    {
        readonly Language language; readonly string build, format; readonly List<string> own;
        readonly ListBox first = new ListBox(), second = new ListBox(); readonly TextBox edit = new TextBox();
        readonly bool hasFirst, hasSecond, hasEdit;
        public string Result { get; private set; }

        public ResearchLinesWindow(Language language, string build, string researchId, ResearchEntry entry, string format, IEnumerable<string> ownIds, Font font, Icon icon)
        {
            this.language = language; this.build = build; this.format = format ?? "line"; own = (ownIds ?? new string[0]).ToList();
            hasFirst = this.format != "edit"; hasSecond = this.format == "line_anchor"; hasEdit = this.format == "line_edit" || this.format == "anchor_edit" || this.format == "edit";
            Text = language.T("research_lines_title"); Font = font; Icon = icon; Size = new Size(820, 640); MinimumSize = new Size(640, 460); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            Theme.ApplyWindowChrome(this);
            var lines = entry == null ? new List<string>() : GameResearch.EditableLines(entry);
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(12) };
            Controls.Add(shell);
            Action<Control, RowStyle> row = (control, style) => { shell.RowStyles.Add(style); control.Dock = DockStyle.Fill; shell.Controls.Add(control, 0, shell.RowStyles.Count - 1); };
            // Header: the research id and its name in the game; a missing block is said plainly.
            var header = Theme.Label(researchId + (entry != null && entry.Name.Length > 0 ? "  ·  " + entry.Name : ""), 12, true); header.Margin = new Padding(0, 0, 0, 6);
            row(header, new RowStyle(SizeType.AutoSize));
            if (entry == null) { var missing = Theme.Label(language.T("research_lines_none"), 10, false); missing.ForeColor = Theme.Danger; missing.Margin = new Padding(0, 0, 0, 8); row(missing, new RowStyle(SizeType.AutoSize)); }
            if (hasFirst)
            {
                var label = Theme.Label(language.T(this.format == "anchor_edit" ? "research_lines_anchor" : "research_lines_line"), 10, true); label.Margin = new Padding(0, 4, 0, 2);
                row(label, new RowStyle(SizeType.AutoSize));
                Fill(first, lines, "lines-first"); row(first, new RowStyle(SizeType.Percent, hasSecond ? 50 : 100));
                first.SelectedIndexChanged += delegate { if (this.format == "line_edit") edit.Text = Convert.ToString(first.SelectedItem); };
                if (!hasSecond && !hasEdit) first.DoubleClick += delegate { Apply(); };
            }
            if (hasSecond)
            {
                var label = Theme.Label(language.T("research_lines_anchor"), 10, true); label.Margin = new Padding(0, 8, 0, 2);
                row(label, new RowStyle(SizeType.AutoSize));
                Fill(second, lines, "lines-second"); row(second, new RowStyle(SizeType.Percent, 50));
            }
            if (hasEdit)
            {
                var label = Theme.Label(language.T("research_lines_new"), 10, true); label.Margin = new Padding(0, 8, 0, 2);
                row(label, new RowStyle(SizeType.AutoSize));
                var editRow = new TableLayoutPanel { ColumnCount = 2, Margin = Padding.Empty, AutoSize = true };
                editRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); editRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                edit.Font = new Font("Consolas", 10f); edit.AccessibleName = "lines-new";
                Control host = Fields.Wrap(edit); host.Dock = DockStyle.Top; host.Margin = new Padding(0, 0, 8, 0); editRow.Controls.Add(host, 0, 0);
                var unlock = Theme.Button(language.T("research_lines_unlock"), ChooseUnlock, false);
                unlock.AutoSize = false; unlock.MinimumSize = Size.Empty; unlock.Height = Fields.Height; unlock.Width = TextRenderer.MeasureText(unlock.Text, unlock.Font).Width + 36; unlock.Margin = Padding.Empty; unlock.AccessibleName = "lines-unlock";
                editRow.Controls.Add(unlock, 1, 0);
                row(editRow, new RowStyle(SizeType.AutoSize));
                if (!hasFirst) row(new Panel(), new RowStyle(SizeType.Percent, 100));
            }
            var buttons = new FlowLayoutPanel { FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Margin = new Padding(0, 10, 0, 0) };
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Theme.Button(language.T("apply"), Apply, true);
            buttons.Controls.Add(apply); buttons.Controls.Add(cancel); row(buttons, new RowStyle(SizeType.Absolute, 50)); AcceptButton = apply; CancelButton = cancel;
        }

        static void Fill(ListBox list, IEnumerable<string> lines, string name)
        {
            list.Font = new Font("Consolas", 10f); list.IntegralHeight = false; list.AccessibleName = name; list.BorderStyle = BorderStyle.FixedSingle;
            foreach (string line in lines) list.Items.Add(line);
        }

        void ChooseUnlock()
        {
            using (var window = new ResearchPickerWindow(language, build, own, Font, Icon, true, null))
                if (Theme.Modal(this,window) == DialogResult.OK && !String.IsNullOrEmpty(window.Result)) edit.Text = "$UNLOCK_RESEARCH " + window.Result;
        }

        void Say(string key) { MessageWindow.Show(this, language, Text, language.T(key), MessageWindow.Kind.Info, DialogResult.OK); }

        void Apply()
        {
            string a = first.SelectedItem as string, b = second.SelectedItem as string, n = edit.Text.Trim();
            if (hasFirst && a == null) { Say("research_lines_need"); return; }
            if (hasSecond && b == null) { Say("research_lines_need"); return; }
            if (hasSecond && a == b) { Say("research_lines_same"); return; }
            if (hasEdit && n.Length == 0) { Say("research_lines_need_new"); return; }
            if (hasEdit && n.Contains("|")) { Say("research_lines_bar"); return; }
            switch (format)
            {
                case "line_edit": Result = a + " | " + n; break;
                case "line_anchor": Result = a + " | " + b; break;
                case "anchor_edit": Result = a + " | " + n; break;
                case "edit": Result = n; break;
                default: Result = a; break;
            }
            DialogResult = DialogResult.OK; Close();
        }

        // Test hooks.
        internal int LineCount { get { return first.Items.Count; } }
        internal void TestChoose(int firstIndex, int secondIndex, string text)
        {
            if (firstIndex >= 0 && firstIndex < first.Items.Count) first.SelectedIndex = firstIndex;
            if (secondIndex >= 0 && secondIndex < second.Items.Count) second.SelectedIndex = secondIndex;
            if (text != null) edit.Text = text;
            Apply();
        }
    }
}
