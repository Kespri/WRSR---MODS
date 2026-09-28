using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // The lines tab of the change dialog: the donor's building.ini on the left, your own lines on
    // the right, and below them what each of your lines removes from the donor. That last part is
    // the point of the whole thing - one $STORAGE line of yours takes every storage of the donor
    // with it, and until now nothing said so until the building was in the game.
    //
    // A token and the lines below it without a token of their own are one block, exactly as the
    // plugin treats them. Selecting any line of a block puts the whole block into the text box,
    // so an anchor or a pile is edited in one go instead of line by line.
    sealed class DonorLinesEditor : TableLayoutPanel
    {
        readonly Language language;
        readonly ListBox left = new ListBox(), mineList = new ListBox();
        readonly TextBox search = new TextBox(), edit = new TextBox();
        readonly Panel effect = new Panel { Dock = DockStyle.Fill, AutoScroll = true, BackColor = Color.White };
        readonly Label count = new Label(), readOnly = new Label();
        readonly Button donorView, resultView;
        readonly List<string> donor;
        readonly List<string> mine = new List<string>();
        readonly List<string> strip = new List<string>();
        readonly List<Button> editButtons = new List<Button>();
        readonly HashSet<string> doubles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        readonly Func<string> nameLine;
        bool showResult;
        DonorOutcome outcome;
        List<DonorLine> shown = new List<DonorLine>();

        public IEnumerable<string> Lines { get { return mine; } }
        public IEnumerable<string> Strip { get { return strip; } }
        public int Maximum = 512;
        // Called after every change, so the values are in the draft at once.
        public Action Changed;
        bool ready;

        public DonorLinesEditor(Language language, List<string> donorLines, IEnumerable<string> lines, IEnumerable<string> strips, Func<string> nameLine, Font font)
        {
            this.language = language; this.donor = donorLines ?? new List<string>(); this.nameLine = nameLine;
            mine.AddRange((lines ?? Enumerable.Empty<string>()).Where(x => x != null));
            strip.AddRange((strips ?? Enumerable.Empty<string>()).Where(x => !String.IsNullOrWhiteSpace(x)));
            Dock = DockStyle.Fill; ColumnCount = 2; RowCount = 4; Font = font; BackColor = Color.White;
            ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52)); ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48));
            // Row 1 carries the "read only" line of the result view and is 0 high in the donor view.
            RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); RowStyles.Add(new RowStyle(SizeType.Absolute, 0));
            RowStyles.Add(new RowStyle(SizeType.Percent, 100)); RowStyles.Add(new RowStyle(SizeType.Absolute, 112));

            var head = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            donorView = Chip(language.T("donor_view_donor"), () => { showResult = false; Refill(); });
            resultView = Chip(language.T("donor_view_result"), () => { showResult = true; Refill(); });
            head.Controls.Add(donorView); head.Controls.Add(resultView);
            search.BorderStyle = BorderStyle.None; search.Font = Fields.Font; search.AccessibleName = "donor:search";
            var wrapped = Fields.Wrap(search); wrapped.Width = 220; wrapped.Margin = new Padding(16, 0, 0, 0);
            search.TextChanged += (s, e) => Refill();
            head.Controls.Add(wrapped);
            Controls.Add(head, 0, 0);

            var right = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Margin = new Padding(14, 0, 0, 0) };
            right.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); right.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var title = Theme.Label(language.T("donor_mine"), 10, true); title.Anchor = AnchorStyles.Left;
            count.AutoSize = true; count.ForeColor = Theme.Muted; count.Font = new Font("Segoe UI", 9); count.Anchor = AnchorStyles.Right; count.AccessibleName = "donor:count";
            right.Controls.Add(title, 0, 0); right.Controls.Add(count, 1, 0);
            Controls.Add(right, 1, 0);

            readOnly.Text = language.T("donor_view_read_only"); readOnly.AutoSize = true; readOnly.Font = new Font("Segoe UI", 9, FontStyle.Bold);
            readOnly.ForeColor = Color.FromArgb(139, 81, 0); readOnly.Margin = new Padding(2, 4, 0, 4); readOnly.Visible = false;
            readOnly.AccessibleName = "donor:read-only";
            Controls.Add(readOnly, 0, 1);

            Style(left, "donor:donor"); Style(mineList, "donor:mine");
            // Owner drawn, because a line the declaration removes has to look removed: struck
            // through and red, with its storage number behind it if it has one.
            left.DrawItem += (s, e) =>
            {
                e.DrawBackground();
                if (e.Index < 0 || e.Index >= shown.Count) return;
                DonorLine line = shown[e.Index];
                bool selected = (e.State & DrawItemState.Selected) != 0;
                Color colour = selected ? Color.White : line.Dropped ? Color.FromArgb(150, 28, 28) : Theme.Ink;
                using (var glyph = new Font("Consolas", 10, line.Dropped ? FontStyle.Strikeout : FontStyle.Regular))
                    TextRenderer.DrawText(e.Graphics, Label(line), glyph, e.Bounds, colour, TextFormatFlags.Left | TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter);
            };
            mineList.DrawItem += (s, e) =>
            {
                e.DrawBackground();
                if (e.Index < 0 || e.Index >= mineList.Items.Count) return;
                string text = mineList.Items[e.Index].ToString();
                bool selected = (e.State & DrawItemState.Selected) != 0, removal = e.Index >= mine.Count;
                // Amber also marks a line whose setting is already set by another line of yours.
                bool twice = !removal && doubles.Contains(DonorPlan.SettingKey(mine[e.Index]));
                Color colour = selected ? Color.White : removal || twice ? Color.FromArgb(139, 81, 0) : Theme.Ink;
                using (var glyph = new Font("Consolas", 10))
                    TextRenderer.DrawText(e.Graphics, text, glyph, e.Bounds, colour, TextFormatFlags.Left | TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter);
            };
            left.DoubleClick += (s, e) => Take();
            left.SelectedIndexChanged += (s, e) => ShowLeft();
            mineList.SelectedIndexChanged += (s, e) => ShowMine();
            Controls.Add(left, 0, 2);

            var rightSplit = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3, Margin = new Padding(14, 0, 0, 0) };
            rightSplit.RowStyles.Add(new RowStyle(SizeType.Percent, 54)); rightSplit.RowStyles.Add(new RowStyle(SizeType.Absolute, 48)); rightSplit.RowStyles.Add(new RowStyle(SizeType.Percent, 46));
            rightSplit.Controls.Add(mineList, 0, 0);
            var order = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = new Padding(0, 4, 0, 4) };
            order.Controls.Add(Small(language.T("donor_up"), () => Shift(-1), "donor:up"));
            order.Controls.Add(Small(language.T("donor_down"), () => Shift(1), "donor:down"));
            order.Controls.Add(Small(language.T("donor_drop"), Drop, "donor:drop"));
            rightSplit.Controls.Add(order, 0, 1);
            var box = new Panel { Dock = DockStyle.Fill, BorderStyle = BorderStyle.FixedSingle, BackColor = Color.White };
            box.Controls.Add(effect); rightSplit.Controls.Add(box, 0, 2);
            Controls.Add(rightSplit, 1, 2);

            var foot = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 2, Margin = new Padding(0, 8, 0, 0) };
            foot.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); foot.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
            edit.Multiline = true; edit.Font = new Font("Consolas", 10); edit.Dock = DockStyle.Fill; edit.ScrollBars = ScrollBars.Vertical; edit.AccessibleName = "donor:edit";
            foot.Controls.Add(edit, 0, 0);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = new Padding(0, 6, 0, 0) };
            actions.Controls.Add(Wide(language.T("donor_add"), Add, "donor:add"));
            actions.Controls.Add(Wide(language.T("donor_replace"), Replace, "donor:replace"));
            actions.Controls.Add(Wide(language.T("donor_strip"), StripToken, "donor:strip"));
            foot.Controls.Add(actions, 0, 1);
            SetColumnSpan(foot, 2); Controls.Add(foot, 0, 3);

            Recompute(); ready = true;
        }
        // An owner drawn list does not measure itself, so the sideways scrollbar needs the width
        // of the widest entry.
        static void Extent(ListBox list, IEnumerable<string> texts)
        {
            int width = 0;
            using (var font = new Font("Consolas", 10))
                foreach (string text in texts) width = Math.Max(width, TextRenderer.MeasureText(text, font, Size.Empty, TextFormatFlags.NoPadding).Width);
            list.HorizontalExtent = width + 12;
        }
        static void Style(ListBox list, string name)
        {
            list.Dock = DockStyle.Fill; list.Font = new Font("Consolas", 10); list.IntegralHeight = false;
            list.HorizontalScrollbar = true; list.AccessibleName = name; list.DrawMode = DrawMode.OwnerDrawFixed; list.ItemHeight = 18;
        }
        Button Chip(string text, Action action)
        {
            var button = Theme.Button(text, action, false);
            button.Height = 30; button.MinimumSize = Size.Empty; button.Margin = new Padding(0, 0, 6, 0); button.AccessibleName = "donor:view:" + text;
            return button;
        }
        // AutoSize keeps the width right whatever the text and the language say; only the height
        // has to be held, or the button ends up as high as its text and its frame is cut off at
        // the bottom of the row. MinimumSize does that without switching AutoSize off.
        Button Small(string text, Action action, string name)
        {
            var button = Theme.Button(text, action, false);
            button.MinimumSize = new Size(0, 32); button.Margin = new Padding(0, 0, 6, 0); button.AccessibleName = name;
            editButtons.Add(button); return button;
        }
        Button Wide(string text, Action action, string name)
        {
            var button = Theme.Button(text, action, false);
            button.MinimumSize = new Size(0, Fields.Height); button.Margin = new Padding(0, 0, 8, 0); button.AccessibleName = name;
            editButtons.Add(button); return button;
        }
        // The result view only shows what will be written. Everything that would change something
        // is switched off there - a disabled button cannot report a wrong reason.
        void SetEditable(bool on)
        {
            foreach (Button button in editButtons) button.Enabled = on;
            edit.ReadOnly = !on; edit.BackColor = on ? Color.White : Theme.Pale;
            readOnly.Visible = !on; RowStyles[1].Height = on ? 0 : 26;
        }

        // ---- the block a line belongs to ----
        static int BlockStart(IList<string> lines, int at)
        {
            while (at > 0 && DonorPlan.FirstToken(lines[at]).Length == 0) at--;
            return at;
        }
        static int BlockEnd(IList<string> lines, int start)
        {
            int end = start;
            while (end + 1 < lines.Count && DonorPlan.FirstToken(lines[end + 1]).Length == 0)
            {
                string bare = (lines[end + 1] ?? "").Trim();
                if (bare.Length == 0 || bare[0] == '-' || bare[0] == ';') break;
                end++;
            }
            return end;
        }
        internal string BlockAt(IList<string> lines, int at)
        {
            if (at < 0 || at >= lines.Count) return "";
            int start = BlockStart(lines, at), end = BlockEnd(lines, start);
            return String.Join("\r\n", Enumerable.Range(start, end - start + 1).Select(i => lines[i]));
        }

        void Recompute()
        {
            outcome = DonorPlan.Build(donor, mine, strip, nameLine == null ? "" : nameLine());
            // Which of your lines mean a setting that is already set further up: those are marked
            // in the list, because only one of them counts in the game.
            doubles.Clear();
            var once = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string line in mine)
            {
                string key = DonorPlan.SettingKey(line);
                if (key.Length > 0 && !once.Add(key)) doubles.Add(key);
            }
            count.Text = language.Format("donor_count", mine.Count, Maximum);
            count.ForeColor = mine.Count > Maximum ? Color.FromArgb(150, 28, 28) : Theme.Muted;
            Refill(); FillMine(); ShowEffect(null);
            if (ready && Changed != null) Changed();
        }
        void Refill()
        {
            donorView.BackColor = showResult ? Color.White : Theme.SelectionBlue; donorView.ForeColor = showResult ? Theme.Ink : Color.White;
            resultView.BackColor = showResult ? Theme.SelectionBlue : Color.White; resultView.ForeColor = showResult ? Color.White : Theme.Ink;
            SetEditable(!showResult);
            string filter = search.Text.Trim();
            shown = (showResult ? outcome.Result : outcome.Donor).Where(x => filter.Length == 0 || x.Text.IndexOf(filter, StringComparison.CurrentCultureIgnoreCase) >= 0).ToList();
            left.BeginUpdate(); left.Items.Clear();
            foreach (DonorLine line in shown) left.Items.Add(Label(line));
            left.EndUpdate(); Extent(left, shown.Select(Label));
        }
        string Label(DonorLine line)
        {
            string text = line.Text;
            if (line.Storage >= 0) text += "      " + language.Format("donor_storage", line.Storage);
            return (line.Dropped ? "x  " : "   ") + text;
        }
        void FillMine()
        {
            mineList.BeginUpdate(); mineList.Items.Clear();
            foreach (string line in mine) mineList.Items.Add(DonorPlan.FirstToken(line).Length == 0 ? "    " + line : line);
            foreach (string token in strip) mineList.Items.Add("- " + token);
            mineList.EndUpdate(); Extent(mineList, mineList.Items.Cast<object>().Select(x => x.ToString()));
        }
        void ShowLeft()
        {
            if (left.SelectedIndex < 0 || left.SelectedIndex >= shown.Count) return;
            mineList.ClearSelected();
            DonorLine line = shown[left.SelectedIndex];
            // The result view only shows what the file will look like. Nothing is taken out of it:
            // a line there is either one of yours already or a donor line that survives anyway.
            if (showResult) { ShowEffect(null); return; }
            var list = outcome.Donor.Select(x => x.Text).ToList();
            edit.Text = BlockAt(list, outcome.Donor.IndexOf(line));
            ShowEffect(line.Dropped ? line.DroppedBy : null);
        }
        // Double-click on a donor line takes it over. If one of your own lines already replaces
        // that very line, the second take overwrites your line instead of adding another one -
        // three $WORKERS_NEEDED lines were easy to collect, and only the first one ever counted.
        void Take()
        {
            if (showResult || edit.Text.Trim().Length == 0) return;
            int at = left.SelectedIndex >= 0 && left.SelectedIndex < shown.Count ? MineIndex(shown[left.SelectedIndex]) : -1;
            if (at >= 0) ReplaceAt(at); else Add();
        }
        // Where this exact block already sits among your lines, or -1.
        int Same(List<string> block)
        {
            for (int i = 0; i < mine.Count; i++)
            {
                int start = BlockStart(mine, i); if (start != i) continue;
                int end = BlockEnd(mine, start); if (end - start + 1 != block.Count) continue;
                bool same = true;
                for (int n = 0; n < block.Count && same; n++) same = mine[start + n].Trim() == block[n].Trim();
                if (same) return start;
            }
            return -1;
        }
        // Where one of your lines already sets the same thing, or -1. Only block starts count -
        // a data line below a token carries no setting of its own.
        int SettingIndex(string line)
        {
            string key = DonorPlan.SettingKey(line);
            if (key.Length == 0) return -1;
            for (int i = 0; i < mine.Count; i++)
                if (BlockStart(mine, i) == i && String.Equals(DonorPlan.SettingKey(mine[i]), key, StringComparison.OrdinalIgnoreCase)) return i;
            return -1;
        }
        internal Func<string, DialogResult> DoublePrompt = null;   // test hook: answers without a message loop
        // The message window shows a text as it is, so a literal \n of the language file has to
        // become a real line break here.
        static string Wrapped(string text) { return (text ?? "").Replace("\\n", Environment.NewLine); }
        DialogResult AskDouble(string token, string existing)
        {
            if (DoublePrompt != null) return DoublePrompt(token);
            return MessageWindow.Show(FindForm(), language, language.T("note"),
                Wrapped(language.Format("donor_double_question", token, (existing ?? "").Trim())), MessageWindow.Kind.Warning,
                new[]
                {
                    new KeyValuePair<DialogResult, string>(DialogResult.Yes, language.T("donor_double_overwrite")),
                    new KeyValuePair<DialogResult, string>(DialogResult.No, language.T("donor_double_keep")),
                    new KeyValuePair<DialogResult, string>(DialogResult.Cancel, language.T("cancel"))
                });
        }
        // Which of your lines drops this donor line: DonorPlan blames it as "line:<n>".
        int MineIndex(DonorLine line)
        {
            if (line == null || !line.Dropped || !(line.DroppedBy ?? "").StartsWith("line:")) return -1;
            int n; if (!Int32.TryParse(line.DroppedBy.Substring(5), out n)) return -1;
            return n >= 0 && n < mine.Count ? n : -1;
        }
        void ShowMine()
        {
            if (mineList.SelectedIndex < 0) return;
            left.ClearSelected();
            if (mineList.SelectedIndex >= mine.Count) { edit.Text = strip[mineList.SelectedIndex - mine.Count]; ShowEffect("strip:" + (mineList.SelectedIndex - mine.Count)); return; }
            edit.Text = BlockAt(mine, mineList.SelectedIndex);
            ShowEffect("line:" + BlockStart(mine, mineList.SelectedIndex));
        }

        // ---- what one of your entries removes from the donor ----
        void ShowEffect(string blame)
        {
            Theme.DisposeChildren(effect);
            var shell = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, ColumnCount = 1, Padding = new Padding(10, 8, 10, 10), BackColor = Color.White };
            shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            effect.Controls.Add(shell);
            foreach (string warning in outcome.Warnings)
            {
                var label = Theme.Label(language.Localize(warning), 9, false); label.ForeColor = Color.FromArgb(139, 81, 0); label.MaximumSize = new Size(Math.Max(240, effect.ClientSize.Width - 30), 0); label.Margin = new Padding(0, 0, 0, 8); label.AccessibleName = "donor:warning";
                AddLine(shell, label);
            }
            if (blame == null) { AddLine(shell, Note(language.T("donor_effect_help"), Theme.Muted)); return; }
            var casualties = outcome.Donor.Where(x => x.Dropped && x.DroppedBy == blame).ToList();
            if (casualties.Count == 0) { AddLine(shell, Note(language.T("donor_effect_none"), Theme.Muted)); return; }
            AddLine(shell, Note(language.T("donor_effect_drops"), Theme.Ink));
            foreach (DonorLine line in casualties.Where(x => !x.Data))
            {
                int extra = 0;
                int at = outcome.Donor.IndexOf(line);
                for (int i = at + 1; i < outcome.Donor.Count && outcome.Donor[i].Data && outcome.Donor[i].Dropped; i++) extra++;
                var label = new Label { Text = "    " + line.Text + (extra > 0 ? "   (+" + extra + ")" : ""), AutoSize = true, Font = new Font("Consolas", 9), ForeColor = Theme.Ink, Margin = new Padding(0, 1, 0, 1) };
                AddLine(shell, label);
            }
            var keep = casualties.Where(x => !x.Data).Select(x => outcome.Donor.IndexOf(x)).ToList();
            if (keep.Count == 0) return;
            var button = Theme.Button(language.Format("donor_keep", keep.Count), () => Keep(keep), false);
            button.Height = 32; button.MinimumSize = Size.Empty; button.Margin = new Padding(0, 8, 0, 0); button.AccessibleName = "donor:keep";
            AddLine(shell, button);
        }
        Label Note(string text, Color colour)
        {
            var label = Theme.Label(text, 9, false); label.ForeColor = colour; label.MaximumSize = new Size(Math.Max(240, effect.ClientSize.Width - 30), 0); label.Margin = new Padding(0, 0, 0, 4);
            return label;
        }
        static void AddLine(TableLayoutPanel table, Control control)
        { int row = table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.AutoSize)); control.Dock = DockStyle.Top; table.Controls.Add(control, 0, row); }

        // Copies the donor lines that one of your entries removes back into your own block, so
        // what you wanted to keep survives. Data lines come with their token.
        void Keep(List<int> indexes)
        {
            foreach (int at in indexes)
            {
                var block = new List<string> { outcome.Donor[at].Text };
                for (int i = at + 1; i < outcome.Donor.Count && outcome.Donor[i].Data; i++) block.Add(outcome.Donor[i].Text);
                mine.AddRange(block);
            }
            Recompute();
        }

        // ---- the three buttons ----
        IEnumerable<string> Typed { get { return edit.Text.Replace("\r\n", "\n").Split('\n').Select(x => x.TrimEnd()).Where(x => x.Trim().Length > 0); } }
        void Replace()
        {
            if (mineList.SelectedIndex < 0) return;
            if (mineList.SelectedIndex >= mine.Count) { var one = Typed.ToList(); if (one.Count == 0) return; strip[mineList.SelectedIndex - mine.Count] = one[0].Trim(); Recompute(); return; }
            ReplaceAt(mineList.SelectedIndex);
        }
        void ReplaceAt(int index)
        {
            var block = Typed.ToList();
            if (block.Count == 0) return;
            int start = BlockStart(mine, index), end = BlockEnd(mine, start);
            mine.RemoveRange(start, end - start + 1); mine.InsertRange(start, block);
            Recompute(); Select(start);
        }
        void Add()
        {
            var block = Typed.ToList();
            if (block.Count == 0) return;
            // The first line of a block has to carry a token: it is written after the name line,
            // and a data line there would attach itself to the name.
            if (DonorPlan.FirstToken(block[0]).Length == 0) { ShowError(language.T("donor_needs_token")); return; }
            // The very same block twice is never meant: only the first one counts and the second
            // just sits there. Same token with different values is a different matter - a building
            // may well carry several $STORAGE or $CONNECTION lines - so only an exact repeat is caught.
            int already = Same(block);
            if (already >= 0) { Select(already); return; }
            // Same setting, other value: only one of the two would count in the game and which one
            // is nowhere documented - so ask instead of piling a second one up silently.
            int twice = SettingIndex(block[0]);
            if (twice >= 0)
            {
                DialogResult answer = AskDouble(DonorPlan.FirstToken(block[0]), mine[twice]);
                if (answer == DialogResult.Cancel) return;
                if (answer == DialogResult.Yes) { ReplaceAt(twice); return; }
            }
            mine.AddRange(block); Recompute(); Select(mine.Count - block.Count);
        }
        void StripToken()
        {
            string token = DonorPlan.FirstToken(edit.Text.Replace("\r\n", "\n").Split('\n').FirstOrDefault() ?? "");
            if (token.Length == 0) { ShowError(language.T("donor_needs_token")); return; }
            if (!strip.Contains(token)) strip.Add(token);
            Recompute();
        }
        void Shift(int by)
        {
            if (mineList.SelectedIndex < 0 || mineList.SelectedIndex >= mine.Count) return;
            int start = BlockStart(mine, mineList.SelectedIndex), end = BlockEnd(mine, start);
            var block = mine.GetRange(start, end - start + 1);
            int target = by < 0 ? BlockStart(mine, Math.Max(0, start - 1)) : -1;
            if (by > 0)
            {
                if (end + 1 >= mine.Count) return;
                int nextEnd = BlockEnd(mine, end + 1);
                mine.RemoveRange(start, block.Count); target = nextEnd - block.Count + 1;
                mine.InsertRange(target, block);
            }
            else
            {
                if (start == 0) return;
                mine.RemoveRange(start, block.Count); mine.InsertRange(target, block);
            }
            Recompute(); Select(target);
        }
        void Drop()
        {
            if (mineList.SelectedIndex < 0) return;
            if (mineList.SelectedIndex >= mine.Count) { strip.RemoveAt(mineList.SelectedIndex - mine.Count); Recompute(); return; }
            int start = BlockStart(mine, mineList.SelectedIndex), end = BlockEnd(mine, start);
            mine.RemoveRange(start, end - start + 1); Recompute();
        }
        void Select(int index)
        { if (index >= 0 && index < mineList.Items.Count) mineList.SelectedIndex = index; }
        void ShowError(string text)
        { MessageWindow.Show(FindForm(), language, language.T("note"), text, MessageWindow.Kind.Warning, DialogResult.OK); }
        // Test hooks: the editor is driven without a message loop.
        internal void TestSelectMine(int index) { Select(index); }
        internal void TestType(string text) { edit.Text = text; }
        internal void TestAdd() { Add(); }
        internal void TestReplace() { Replace(); }
        internal void TestStrip() { StripToken(); }
        internal void TestKeepAll()
        {
            var keep = Children(effect).OfType<Button>().FirstOrDefault(x => (x.AccessibleName ?? "") == "donor:keep");
            if (keep != null) keep.PerformClick();
        }
        internal bool TestHasKeep { get { return Children(effect).OfType<Button>().Any(x => (x.AccessibleName ?? "") == "donor:keep"); } }
        internal IEnumerable<string> TestWarnings { get { return outcome.Warnings; } }
        internal bool TestDropped(string donorLine) { return outcome.Donor.Any(x => x.Text == donorLine && x.Dropped); }
        internal string TestEdit { get { return edit.Text; } }
        internal void TestSelectDonor(int index) { left.SelectedIndex = index; }
        internal void TestTake() { Take(); }
        internal void TestView(bool result) { showResult = result; Refill(); }
        internal bool TestEditable { get { return editButtons.All(x => x.Enabled) && !edit.ReadOnly && !readOnly.Visible; } }
        static IEnumerable<Control> Children(Control root)
        { foreach (Control child in root.Controls) { yield return child; foreach (Control deeper in Children(child)) yield return deeper; } }
    }
}
