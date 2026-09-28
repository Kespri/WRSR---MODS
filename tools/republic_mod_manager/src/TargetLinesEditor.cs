using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // The commands tab of a Vanilla Buildings rule set: the target file on the left, the commands
    // on the right, and below them what the selected command does to THAT file. A rule set can
    // carry many targets and every command is checked against each one separately - so the file is
    // picked at the top and the whole picture changes with it.
    //
    // Why it matters: a command whose anchor does not sit exactly once in the original is rejected
    // for that target at game start and only mentioned in the log. Here you click the line instead
    // of typing it, and a target where a command does not fit says so before the game runs.
    sealed class TargetLinesEditor : TableLayoutPanel
    {
        readonly Language language;
        readonly ComboBox targets = new ComboBox();
        readonly ListBox left = new ListBox(), commandList = new ListBox();
        readonly TextBox search = new TextBox(), edit = new TextBox();
        readonly Panel effect = new Panel { Dock = DockStyle.Fill, AutoScroll = true, BackColor = Color.White };
        readonly Label count = new Label(), missing = new Label(), readOnly = new Label();
        readonly Button originalView, resultView;
        readonly List<Button> editButtons = new List<Button>();
        readonly Func<string, List<string>> read;
        readonly List<string> add = new List<string>(), replace = new List<string>(), remove = new List<string>(), insert = new List<string>();
        readonly List<string> addLink = new List<string>(), replaceLink = new List<string>(), removeLink = new List<string>();
        readonly List<string> targetList = new List<string>();
        readonly CheckBox everywhere = new CheckBox();
        List<ConnectionBlock> blocks = new List<ConnectionBlock>();
        List<string> lines = new List<string>();
        TargetOutcome outcome;
        List<TargetResultLine> result = new List<TargetResultLine>();
        List<int> shown = new List<int>();
        bool showResult, fileHere;

        public IEnumerable<string> Add { get { return add; } }
        public IEnumerable<string> Replace { get { return replace; } }
        public IEnumerable<string> Remove { get { return remove; } }
        public IEnumerable<string> Insert { get { return insert; } }
        public IEnumerable<string> AddLink { get { return addLink; } }
        public IEnumerable<string> ReplaceLink { get { return replaceLink; } }
        public IEnumerable<string> RemoveLink { get { return removeLink; } }
        public Action Changed;
        bool ready;

        public TargetLinesEditor(Language language, IEnumerable<string> targetFiles, Func<string, List<string>> readTarget,
            IEnumerable<string> adds, IEnumerable<string> replaces, IEnumerable<string> removes, IEnumerable<string> inserts, Font font)
            : this(language, targetFiles, readTarget, adds, replaces, removes, inserts, null, null, null, font) { }
        public TargetLinesEditor(Language language, IEnumerable<string> targetFiles, Func<string, List<string>> readTarget,
            IEnumerable<string> adds, IEnumerable<string> replaces, IEnumerable<string> removes, IEnumerable<string> inserts,
            IEnumerable<string> addLinks, IEnumerable<string> replaceLinks, IEnumerable<string> removeLinks, Font font)
        {
            this.language = language; this.read = readTarget;
            targetList.AddRange((targetFiles ?? Enumerable.Empty<string>()).Select(x => (x ?? "").Trim()).Where(x => x.Length > 0));
            add.AddRange(Clean(adds)); replace.AddRange(Clean(replaces)); remove.AddRange(Clean(removes)); insert.AddRange(Clean(inserts));
            addLink.AddRange(Clean(addLinks)); replaceLink.AddRange(Clean(replaceLinks)); removeLink.AddRange(Clean(removeLinks));
            Dock = DockStyle.Fill; ColumnCount = 2; RowCount = 4; Font = font; BackColor = Color.White;
            ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 52)); ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 48));
            RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); RowStyles.Add(new RowStyle(SizeType.Absolute, 0));
            RowStyles.Add(new RowStyle(SizeType.Percent, 100)); RowStyles.Add(new RowStyle(SizeType.Absolute, 140));

            var head = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Margin = Padding.Empty };
            head.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            head.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); head.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            // The same two views as the donor editor: the unchanged file, and what the game will
            // read once the commands have run.
            var views = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, AutoSize = true, Margin = new Padding(0, 0, 12, 0) };
            originalView = Chip(language.T("target_view_original"), () => { showResult = false; Refill(); });
            resultView = Chip(language.T("target_view_result"), () => { showResult = true; Refill(); });
            views.Controls.Add(originalView); views.Controls.Add(resultView);
            head.Controls.Add(views, 0, 0);
            targets.DropDownStyle = ComboBoxStyle.DropDownList; targets.Height = Fields.Height; targets.AccessibleName = "target:file";
            Fields.Tall(targets);
            foreach (string file in targetList) targets.Items.Add(file);
            if (targets.Items.Count > 0) targets.SelectedIndex = 0;
            targets.SelectedIndexChanged += (s, e) => { if (ready) Recompute(); };
            var host = Fields.Host(targets); host.Margin = new Padding(0, 0, 12, 0); host.Dock = DockStyle.Fill;
            head.Controls.Add(host, 1, 0);
            search.BorderStyle = BorderStyle.None; search.Font = Fields.Font; search.AccessibleName = "target:search";
            var wrapped = Fields.Wrap(search); wrapped.Width = 200; wrapped.Margin = Padding.Empty;
            search.TextChanged += (s, e) => Refill();
            head.Controls.Add(wrapped, 2, 0);
            Controls.Add(head, 0, 0);

            var right = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Margin = new Padding(14, 0, 0, 0) };
            right.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); right.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var title = Theme.Label(language.T("target_commands"), 10, true); title.Anchor = AnchorStyles.Left;
            count.AutoSize = true; count.ForeColor = Theme.Muted; count.Font = new Font("Segoe UI", 9); count.Anchor = AnchorStyles.Right; count.AccessibleName = "target:count";
            right.Controls.Add(title, 0, 0); right.Controls.Add(count, 1, 0);
            Controls.Add(right, 1, 0);

            missing.AutoSize = true; missing.Font = new Font("Segoe UI", 9, FontStyle.Bold); missing.ForeColor = Color.FromArgb(150, 28, 28);
            missing.Margin = new Padding(2, 4, 0, 4); missing.Visible = false; missing.AccessibleName = "target:missing";
            readOnly.AutoSize = true; readOnly.Font = new Font("Segoe UI", 9, FontStyle.Bold); readOnly.ForeColor = Color.FromArgb(139, 81, 0);
            readOnly.Margin = new Padding(2, 4, 0, 4); readOnly.Visible = false; readOnly.AccessibleName = "target:read-only";
            var notes = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            notes.Controls.Add(missing); notes.Controls.Add(readOnly);
            SetColumnSpan(notes, 2); Controls.Add(notes, 0, 1);

            Style(left, "target:lines"); Style(commandList, "target:list");
            // A line one of the commands touches is marked; the file itself is never changed here.
            left.DrawItem += (s, e) =>
            {
                e.DrawBackground();
                if (e.Index < 0 || e.Index >= shown.Count) return;
                bool selected = (e.State & DrawItemState.Selected) != 0;
                bool touched = CommandOf(shown[e.Index]) >= 0;
                Color colour = selected ? Color.White : touched ? Color.FromArgb(0, 92, 175) : Theme.Ink;
                using (var glyph = new Font("Consolas", 10, touched ? FontStyle.Bold : FontStyle.Regular))
                    TextRenderer.DrawText(e.Graphics, LineLabel(shown[e.Index]), glyph, e.Bounds, colour, TextFormatFlags.Left | TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter);
            };
            commandList.DrawItem += (s, e) =>
            {
                e.DrawBackground();
                if (e.Index < 0 || e.Index >= outcome.Commands.Count) return;
                TargetCommand command = outcome.Commands[e.Index];
                bool selected = (e.State & DrawItemState.Selected) != 0;
                Color colour = selected ? Color.White : command.Problem.Length > 0 ? Color.FromArgb(150, 28, 28) : Theme.Ink;
                using (var glyph = new Font("Consolas", 10))
                    TextRenderer.DrawText(e.Graphics, CommandLabel(e.Index), glyph, e.Bounds, colour, TextFormatFlags.Left | TextFormatFlags.NoPadding | TextFormatFlags.VerticalCenter);
            };
            left.SelectedIndexChanged += (s, e) => ShowLeft();
            commandList.SelectedIndexChanged += (s, e) => ShowCommand();
            Controls.Add(left, 0, 2);

            var rightSplit = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3, Margin = new Padding(14, 0, 0, 0) };
            rightSplit.RowStyles.Add(new RowStyle(SizeType.Percent, 54)); rightSplit.RowStyles.Add(new RowStyle(SizeType.Absolute, 48)); rightSplit.RowStyles.Add(new RowStyle(SizeType.Percent, 46));
            rightSplit.Controls.Add(commandList, 0, 0);
            var order = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = new Padding(0, 4, 0, 4) };
            order.Controls.Add(Small(language.T("donor_up"), () => Shift(-1), "target:up"));
            order.Controls.Add(Small(language.T("donor_down"), () => Shift(1), "target:down"));
            order.Controls.Add(Small(language.T("donor_drop"), Drop, "target:drop"));
            rightSplit.Controls.Add(order, 0, 1);
            var box = new Panel { Dock = DockStyle.Fill, BorderStyle = BorderStyle.FixedSingle, BackColor = Color.White };
            box.Controls.Add(effect); rightSplit.Controls.Add(box, 0, 2);
            Controls.Add(rightSplit, 1, 2);

            var foot = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 3, Margin = new Padding(0, 8, 0, 0) };
            foot.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); foot.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
            foot.RowStyles.Add(new RowStyle(SizeType.Absolute, 26));
            edit.Font = new Font("Consolas", 10); edit.Dock = DockStyle.Fill; edit.Multiline = true; edit.ScrollBars = ScrollBars.Vertical; edit.AccessibleName = "target:edit";
            foot.Controls.Add(edit, 0, 0);
            var actions = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = new Padding(0, 6, 0, 0) };
            actions.Controls.Add(Wide(language.T("target_add"), MakeAdd, "target:make-add"));
            actions.Controls.Add(Wide(language.T("target_replace"), MakeReplace, "target:make-replace"));
            actions.Controls.Add(Wide(language.T("target_before"), () => MakeInsert(false), "target:make-before"));
            actions.Controls.Add(Wide(language.T("target_after"), () => MakeInsert(true), "target:make-after"));
            actions.Controls.Add(Wide(language.T("target_remove"), MakeRemove, "target:make-remove"));
            foot.Controls.Add(actions, 0, 1);
            // Checking against every target instead of the chosen one: a command that does not fit
            // ONE target throws the whole rule set out for that target, so it pays to know early.
            everywhere.Text = language.T("target_all"); everywhere.AutoSize = true; everywhere.Margin = new Padding(2, 4, 0, 0);
            everywhere.Font = new Font("Segoe UI", 9); everywhere.AccessibleName = "target:all";
            everywhere.CheckedChanged += (s, e) => { if (ready) Recompute(); };
            foot.Controls.Add(everywhere, 0, 2);
            SetColumnSpan(foot, 2); Controls.Add(foot, 0, 3);

            Recompute(); ready = true;
        }
        static IEnumerable<string> Clean(IEnumerable<string> raw)
        { return (raw ?? Enumerable.Empty<string>()).Select(x => (x ?? "").Trim()).Where(x => x.Length > 0); }
        void Style(ListBox list, string name)
        {
            list.Dock = DockStyle.Fill; list.BorderStyle = BorderStyle.FixedSingle; list.Font = new Font("Consolas", 10);
            list.IntegralHeight = false; list.DrawMode = DrawMode.OwnerDrawFixed; list.ItemHeight = 18;
            list.HorizontalScrollbar = true; list.AccessibleName = name;
        }
        Button Chip(string text, Action action)
        {
            var button = Theme.Button(text, action, false);
            button.Height = 30; button.MinimumSize = Size.Empty; button.Margin = new Padding(0, 0, 6, 0);
            button.AccessibleName = "target:view:" + text;
            return button;
        }
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

        public string Target { get { return targets.SelectedItem == null ? "" : targets.SelectedItem.ToString(); } }
        void Recompute()
        {
            List<string> file = null;
            try { if (Target.Length > 0 && read != null) file = read(Target); } catch (Exception) { }
            lines = file ?? new List<string>();
            fileHere = file != null;
            missing.Text = language.Format("target_file_missing", Target);
            missing.Visible = file == null;
            outcome = TargetPlan.Check(lines, add, replace, remove, insert, addLink, replaceLink, removeLink);
            result = TargetPlan.Result(outcome, lines);
            blocks = TargetPlan.Connections(lines);
            CheckEverywhere();
            int bad = outcome.Commands.Count(x => x.Problem.Length > 0);
            count.Text = bad > 0 ? language.Format("target_count_bad", outcome.Commands.Count, bad) : language.Format("target_count", outcome.Commands.Count);
            count.ForeColor = bad > 0 ? Color.FromArgb(150, 28, 28) : Theme.Muted;
            Refill(); FillCommands(); ShowEffect(-1);
            if (ready && Changed != null) Changed();
        }
        // With the box ticked every other target file is checked too. Not to apply anything - the
        // commands of a rule set always go to every target - but to say where one of them does not
        // fit, because THAT target then loses the whole rule set.
        readonly Dictionary<string, int> fits = new Dictionary<string, int>();
        readonly Dictionary<string, string> firstBad = new Dictionary<string, string>();
        int checkedFiles;
        void CheckEverywhere()
        {
            fits.Clear(); firstBad.Clear(); checkedFiles = 0;
            if (!everywhere.Checked) return;
            foreach (string file in targetList)
            {
                List<string> other = null;
                try { if (read != null) other = read(file); } catch (Exception) { }
                if (other == null) continue;
                checkedFiles++;
                TargetOutcome result = TargetPlan.Check(other, add, replace, remove, insert, addLink, replaceLink, removeLink);
                foreach (TargetCommand command in result.Commands)
                {
                    string key = command.Kind + ":" + command.Index;
                    if (command.Problem.Length == 0) { int had; fits[key] = (fits.TryGetValue(key, out had) ? had : 0) + 1; }
                    else if (!firstBad.ContainsKey(key)) firstBad[key] = file;
                }
            }
        }
        void Refill()
        {
            originalView.BackColor = showResult ? Color.White : Theme.SelectionBlue; originalView.ForeColor = showResult ? Theme.Ink : Color.White;
            resultView.BackColor = showResult ? Theme.SelectionBlue : Color.White; resultView.ForeColor = showResult ? Color.White : Theme.Ink;
            SetEditable(!showResult);
            string filter = search.Text.Trim();
            shown = Enumerable.Range(0, SourceCount)
                .Where(i => filter.Length == 0 || TextOf(i).IndexOf(filter, StringComparison.CurrentCultureIgnoreCase) >= 0).ToList();
            left.BeginUpdate(); left.Items.Clear();
            foreach (int i in shown) left.Items.Add(LineLabel(i));
            left.EndUpdate(); Extent(left, shown.Select(LineLabel));
        }
        // Both views feed the same list: the unchanged file, or the file the game will read.
        int SourceCount { get { return showResult ? result.Count : outcome.Lines.Count; } }
        string TextOf(int index)
        { return showResult ? result[index].Text : outcome.Lines[index].Text; }
        int CommandOf(int index)
        { return showResult ? result[index].Command : outcome.Lines[index].Command; }
        string LineLabel(int index)
        {
            int command = CommandOf(index);
            string prefix = command < 0 ? "   "
                : showResult ? (outcome.Commands[command].Kind.StartsWith("replace", StringComparison.Ordinal) ? ">  " : "+  ")
                : Mark(outcome.Commands[command].Kind) + "  ";
            return prefix + TextOf(index);
        }
        static string Mark(string kind)
        { return kind == "replace" ? ">" : kind == "remove" ? "-" : kind == "insert" ? "+" : " "; }
        // The result view only shows what will be written. Everything that would change something
        // is switched off there - a disabled button cannot report a wrong reason.
        void SetEditable(bool on)
        {
            foreach (Button button in editButtons) button.Enabled = on && fileHere;
            edit.ReadOnly = !on; edit.BackColor = on ? Color.White : Theme.Pale;
            readOnly.Text = outcome != null && outcome.Any ? language.T("target_result_blocked") : language.T("target_view_read_only");
            readOnly.ForeColor = outcome != null && outcome.Any ? Color.FromArgb(150, 28, 28) : Color.FromArgb(139, 81, 0);
            readOnly.Visible = !on;
            RowStyles[1].Height = missing.Visible || readOnly.Visible ? 26 : 0;
        }
        void FillCommands()
        {
            commandList.BeginUpdate(); commandList.Items.Clear();
            for (int i = 0; i < outcome.Commands.Count; i++) commandList.Items.Add(CommandLabel(i));
            commandList.EndUpdate(); Extent(commandList, Enumerable.Range(0, outcome.Commands.Count).Select(CommandLabel));
        }
        string CommandLabel(int index)
        {
            TargetCommand command = outcome.Commands[index];
            return (command.Problem.Length > 0 ? "x  " : "   ") + language.T("target_kind_" + command.Kind) + "  " + command.Text;
        }
        static void Extent(ListBox list, IEnumerable<string> texts)
        {
            int width = 0;
            using (var font = new Font("Consolas", 10))
                foreach (string text in texts) width = Math.Max(width, TextRenderer.MeasureText(text, font, Size.Empty, TextFormatFlags.NoPadding).Width);
            list.HorizontalExtent = width + 12;
        }

        // A connection is a block: clicking any of its lines picks the whole thing, exactly as the
        // plugin treats it, and the text box then holds token and points together.
        ConnectionBlock BlockAt(int line)
        { return blocks.FirstOrDefault(b => line >= b.Start && line <= b.End); }
        string BlockText(ConnectionBlock block)
        {
            var parts = new List<string>();
            for (int i = block.Start; i <= block.End && i < outcome.Lines.Count; i++) parts.Add(outcome.Lines[i].Text.Trim());
            return String.Join(Environment.NewLine, parts);
        }
        void ShowLeft()
        {
            if (left.SelectedIndex < 0 || left.SelectedIndex >= shown.Count) return;
            commandList.ClearSelected();
            int at = shown[left.SelectedIndex];
            // In the result view a line is only read: it says which command put it there.
            if (showResult) { ShowEffect(CommandOf(at)); return; }
            ConnectionBlock block = BlockAt(at);
            edit.Text = block != null ? BlockText(block) : outcome.Lines[at].Text.Trim();
            ShowEffect(outcome.Lines[at].Command);
        }
        void ShowCommand()
        {
            if (commandList.SelectedIndex < 0 || commandList.SelectedIndex >= outcome.Commands.Count) return;
            left.ClearSelected();
            TargetCommand command = outcome.Commands[commandList.SelectedIndex];
            edit.Text = command.Kind == "remove" ? command.Anchor : command.Value;
            ShowEffect(commandList.SelectedIndex);
        }
        // What the selected command does to THIS target, in words.
        void ShowEffect(int index)
        {
            Theme.DisposeChildren(effect);
            var shell = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, ColumnCount = 1, Padding = new Padding(10, 8, 10, 10), BackColor = Color.White };
            shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            effect.Controls.Add(shell);
            if (index < 0 || index >= outcome.Commands.Count) { AddLine(shell, Note(language.T("target_pick"), Theme.Muted)); return; }
            TargetCommand command = outcome.Commands[index];
            if (command.Problem.Length > 0)
            {
                AddLine(shell, Note(language.Localize(command.Problem), Color.FromArgb(150, 28, 28)));
                AddLine(shell, Note(language.Format("target_problem_where", Target), Theme.Muted));
                return;
            }
            if (command.Kind == "add" || command.Kind == "add_connection") AddLine(shell, Note(language.T("target_effect_add"), Theme.Ink));
            else if (command.FromOwn) AddLine(shell, Note(language.T("target_effect_own"), Theme.Ink));
            else
            {
                AddLine(shell, Note(language.Format("target_effect_at", command.At + 1), Theme.Ink));
                AddLine(shell, Note(outcome.Lines[command.At].Text.Trim(), Theme.Muted));
            }
            Spread(shell, command);
        }
        // How many of the rule set's targets this command fits, once the box is ticked.
        void Spread(TableLayoutPanel shell, TargetCommand command)
        {
            if (!everywhere.Checked || checkedFiles == 0) return;
            string key = command.Kind + ":" + command.Index;
            int good; fits.TryGetValue(key, out good);
            bool all = good >= checkedFiles;
            AddLine(shell, Note(language.Format(all ? "target_spread_all" : "target_spread_some", good, checkedFiles),
                all ? Theme.Ink : Color.FromArgb(150, 28, 28)));
            string bad;
            if (!all && firstBad.TryGetValue(key, out bad)) AddLine(shell, Note(language.Format("target_spread_first", bad), Theme.Muted));
        }
        Label Note(string text, Color colour)
        {
            var label = Theme.Label(text, 9, false); label.ForeColor = colour;
            label.MaximumSize = new Size(Math.Max(240, effect.ClientSize.Width - 30), 0); label.Margin = new Padding(0, 0, 0, 6);
            return label;
        }
        static void AddLine(TableLayoutPanel shell, Control control)
        { int row = shell.RowCount++; shell.RowStyles.Add(new RowStyle(SizeType.AutoSize)); control.Dock = DockStyle.Top; shell.Controls.Add(control, 0, row); }

        string Typed { get { return edit.Text.Replace("\r\n", "\n").Trim(); } }
        string Picked
        {
            get { return !showResult && left.SelectedIndex >= 0 && left.SelectedIndex < shown.Count ? outcome.Lines[shown[left.SelectedIndex]].Text.Trim() : ""; }
        }
        ConnectionBlock PickedBlock
        { get { return !showResult && left.SelectedIndex >= 0 && left.SelectedIndex < shown.Count ? BlockAt(shown[left.SelectedIndex]) : null; } }
        // A connection is written as one command field: token | point [| point].
        static string BlockFields(ConnectionBlock block)
        {
            string text = block.Token + " | " + Numbers(block.First);
            if (block.Points == 2) text += " | " + Numbers(block.Second);
            return text;
        }
        static string Numbers(double[] point)
        { return point == null ? "" : String.Join(" ", point.Select(x => x.ToString("0.######", System.Globalization.CultureInfo.InvariantCulture))); }
        // What the text box holds, as command fields: a block becomes "token | point [| point]",
        // a single line stays a single line.
        string TypedFields
        {
            get
            {
                var rows = Typed.Split('\n').Select(x => x.Trim()).Where(x => x.Length > 0).ToList();
                if (rows.Count == 0) return "";
                string token = TargetPlan.Token(rows[0]);
                if (!TargetPlan.ConnectionToken(token)) return Typed.Replace("\n", " ").Trim();
                if (rows.Count == 1) return rows[0] == token ? token : token + " | " + rows[0].Substring(token.Length).Trim();
                return token + " | " + String.Join(" | ", rows.Skip(1));
            }
        }
        bool TypedIsLink { get { return TargetPlan.ConnectionToken(TargetPlan.Token(Typed.Split('\n').FirstOrDefault() ?? "")); } }
        void ShowError(string text)
        { MessageWindow.Show(FindForm(), language, language.T("note"), text, MessageWindow.Kind.Warning, DialogResult.OK); }
        void Push(string kind, string value)
        {
            List<string> field = Field(kind);
            if (field.Contains(value)) { Pick(kind, field.IndexOf(value)); return; }
            field.Add(value); Recompute(); Pick(kind, field.Count - 1);
        }
        void MakeAdd()
        {
            if (Typed.Length == 0) return;
            Push(TypedIsLink ? "add_connection" : "add", TypedFields);
        }
        void MakeRemove()
        {
            ConnectionBlock block = PickedBlock;
            if (block != null) { Push("remove_connection", BlockFields(block)); return; }
            string anchor = Picked.Length > 0 ? Picked : Typed;
            if (anchor.Length == 0) return;
            if (TargetPlan.ConnectionToken(TargetPlan.Token(anchor))) { ShowError(language.T("target_link_pick")); return; }
            Push("remove", anchor);
        }
        void MakeReplace()
        {
            ConnectionBlock block = PickedBlock;
            if (block != null)
            {
                if (!TypedIsLink) { ShowError(language.T("target_link_need_new")); return; }
                string fresh = TypedFields;
                if (fresh == BlockFields(block)) { ShowError(language.T("target_need_new")); return; }
                // token | point [| point] | new token [| new point ...]
                string[] parts = fresh.Split('|').Select(x => x.Trim()).ToArray();
                string value = BlockFields(block) + " | " + parts[0] + String.Join("", parts.Skip(1).Select(x => " | " + x));
                int at = replaceLink.FindIndex(x => x.StartsWith(BlockFields(block) + " |", StringComparison.Ordinal));
                if (at >= 0) replaceLink[at] = value; else { replaceLink.Add(value); at = replaceLink.Count - 1; }
                Recompute(); Pick("replace_connection", at); return;
            }
            if (Picked.Length == 0) { ShowError(language.T("target_need_line")); return; }
            if (Typed.Length == 0 || Typed == Picked) { ShowError(language.T("target_need_new")); return; }
            string line = Picked + " | " + Typed.Replace("\n", " ").Trim();
            int where = replace.FindIndex(x => x.Split('|')[0].Trim() == Picked);
            if (where >= 0) replace[where] = line; else { replace.Add(line); where = replace.Count - 1; }
            Recompute(); Pick("replace", where);
        }
        void MakeInsert(bool after)
        {
            if (PickedBlock != null) { ShowError(language.T("target_link_no_insert")); return; }
            if (Picked.Length == 0) { ShowError(language.T("target_need_line")); return; }
            if (Typed.Length == 0 || Typed == Picked) { ShowError(language.T("target_need_new")); return; }
            if (TypedIsLink) { ShowError(language.T("target_link_no_insert")); return; }
            Push("insert", (after ? "1" : "0") + " | " + Picked + " | " + Typed.Replace("\n", " ").Trim());
        }
        void Pick(string kind, int index)
        {
            for (int i = 0; i < outcome.Commands.Count; i++)
                if (outcome.Commands[i].Kind == kind && outcome.Commands[i].Index == index) { commandList.SelectedIndex = i; return; }
        }
        List<string> Field(string kind)
        {
            switch (kind)
            {
                case "add": return add;
                case "replace": return replace;
                case "remove": return remove;
                case "add_connection": return addLink;
                case "replace_connection": return replaceLink;
                case "remove_connection": return removeLink;
                default: return insert;
            }
        }
        void Drop()
        {
            if (commandList.SelectedIndex < 0) return;
            TargetCommand command = outcome.Commands[commandList.SelectedIndex];
            List<string> field = Field(command.Kind);
            if (command.Index < 0 || command.Index >= field.Count) return;
            field.RemoveAt(command.Index); Recompute();
        }
        // Order matters only inside one command kind - the plugin reads add, replace, remove and
        // insert in that order whatever the rows say, and only an earlier command can be an anchor.
        void Shift(int direction)
        {
            if (commandList.SelectedIndex < 0) return;
            TargetCommand command = outcome.Commands[commandList.SelectedIndex];
            List<string> field = Field(command.Kind);
            int at = command.Index, to = at + direction;
            if (at < 0 || at >= field.Count || to < 0 || to >= field.Count) return;
            string value = field[at]; field.RemoveAt(at); field.Insert(to, value);
            Recompute(); Pick(command.Kind, to);
        }

        // Test hooks: the editor is driven without a message loop.
        internal void TestTarget(int index) { if (index >= 0 && index < targets.Items.Count) targets.SelectedIndex = index; }
        internal void TestSelectLine(int index) { left.SelectedIndex = index; }
        internal void TestSelectCommand(int index) { commandList.SelectedIndex = index; }
        internal void TestType(string text) { edit.Text = text; }
        internal void TestAdd() { MakeAdd(); }
        internal void TestReplace() { MakeReplace(); }
        internal void TestRemove() { MakeRemove(); }
        internal void TestInsert(bool after) { MakeInsert(after); }
        internal void TestDrop() { Drop(); }
        internal IEnumerable<string> TestProblems { get { return outcome.Commands.Where(x => x.Problem.Length > 0).Select(x => x.Problem); } }
        internal bool TestEditable { get { return editButtons.All(x => x.Enabled) && !edit.ReadOnly && !readOnly.Visible; } }
        internal void TestView(bool result) { showResult = result; Refill(); }
        internal IEnumerable<string> TestResult { get { return result.Select(x => x.Text); } }
        internal int TestResultCommand(int index) { return index >= 0 && index < result.Count ? result[index].Command : -1; }
        internal string TestTyped { get { return edit.Text; } }
        internal void TestEverywhere(bool on) { everywhere.Checked = on; }
        internal int TestSpread(string kind, int index) { int good; return fits.TryGetValue(kind + ":" + index, out good) ? good : 0; }
        internal string TestCount { get { return count.Text; } }
    }
}
