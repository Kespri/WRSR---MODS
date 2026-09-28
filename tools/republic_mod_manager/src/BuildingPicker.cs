using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // A "lines" field: a multi-line box with a live line counter, a drag grip and
    // an expand toggle below it, and an optional picker button beside it.
    sealed class LinesBox : Panel
    {
        // Footer (0.4.50): 8 px grip, 5 px air, the 34 px button row, 6 px below - the button's top
        // border used to sit right under the grip and looked cut off.
        public const int MinimumBoxHeight = 132, Footer = 53, RowTop = 13;
        public readonly TextBox Box = new TextBox { Multiline = true, ScrollBars = ScrollBars.Vertical, AcceptsReturn = true, WordWrap = false, Font = new Font("Consolas", 9.5f) };
        readonly Label count = Theme.Label("", 9, false); readonly Panel grip = new Panel(); readonly VectorButton expand; readonly Button pick; readonly ToolTip tips = new ToolTip();
        public Func<int, string> CountText = n => n.ToString();
        public Action Pick;
        public int MaximumLines;
        bool expanded; int dragStart, dragHeight;
        public LinesBox(bool withPicker, string pickLabel, string expandLabel)
        {
            Height = MinimumBoxHeight + Footer; Margin = Padding.Empty;
            Box.Location = new Point(0, 0); Box.Height = MinimumBoxHeight; Controls.Add(Box);
            Box.TextChanged += (s, e) => UpdateCount();
            count.ForeColor = Theme.Muted; count.AutoSize = true; Controls.Add(count);
            grip.Cursor = Cursors.SizeNS; grip.Height = 8; grip.BackColor = Color.Transparent; Controls.Add(grip);
            grip.Paint += (s, e) => { using (var pen = new Pen(Theme.Line)) for (int i = 0; i < 3; i++) e.Graphics.DrawLine(pen, grip.Width / 2 - 14, 2 + i * 2, grip.Width / 2 + 14, 2 + i * 2); };
            grip.MouseDown += (s, e) => { dragStart = Cursor.Position.Y; dragHeight = Box.Height; grip.Capture = true; };
            grip.MouseMove += (s, e) => { if (grip.Capture) SetBoxHeight(dragHeight + Cursor.Position.Y - dragStart); };
            grip.MouseUp += (s, e) => { grip.Capture = false; };
            // Footer: the counter on the left, the picker button in the middle, the
            // expand/shrink toggle (plus/minus) on the right.
            expand = new VectorButton { Symbol = "plus", Size = new Size(22, 22), GlyphScale = 1.1f, TabStop = false, AccessibleName = expandLabel, Cursor = Cursors.Hand };
            expand.Click += (s, e) => { expanded = !expanded; SetBoxHeight(expanded ? ContentHeight() : MinimumBoxHeight); };
            tips.SetToolTip(expand, expandLabel); Controls.Add(expand);
            if (withPicker)
            {
                pick = Theme.Button(pickLabel, () => { if (Pick != null) Pick(); }, false); pick.AutoSize = false; pick.Height = 34; pick.MinimumSize = Size.Empty; pick.Padding = new Padding(4, 0, 4, 0); pick.AccessibleName = pickLabel; pick.TextAlign = ContentAlignment.MiddleCenter;
                pick.Width = TextRenderer.MeasureText(pickLabel, pick.Font).Width + 36;
                Controls.Add(pick);
            }
            Layout += (s, e) => Arrange(); UpdateCount();
        }
        public override string Text { get { return Box.Text; } set { Box.Text = value ?? ""; UpdateCount(); } }
        public int BoxHeight { get { return Box.Height; } set { SetBoxHeight(value); } }
        public int LineCount { get { return Box.Text.Replace("\r\n", "\n").Split('\n').Count(l => l.Trim().Length > 0); } }
        int ContentHeight() { int lines = Math.Max(6, Box.Text.Replace("\r\n", "\n").Split('\n').Length); return Math.Max(MinimumBoxHeight, lines * Box.Font.Height + 14); }
        void SetBoxHeight(int height)
        {
            height = Math.Max(MinimumBoxHeight, Math.Min(height, Math.Max(MinimumBoxHeight, ContentHeight())));
            if (Box.Height == height) return;
            Box.Height = height; expanded = height >= ContentHeight(); expand.Symbol = expanded ? "minus" : "plus"; expand.Invalidate(); Height = height + Footer; Arrange();
        }
        void Arrange()
        {
            Box.Width = Math.Max(100, Width);
            grip.Location = new Point(0, Box.Bottom); grip.Width = Box.Width;
            // Footer: counter left, picker button right, the expand toggle only while there is more to show.
            bool expandable = ContentHeight() > MinimumBoxHeight; expand.Visible = expandable;
            count.Location = new Point(0, Box.Bottom + RowTop + (34 - count.Height) / 2);
            expand.Location = new Point(Box.Width - 22, Box.Bottom + RowTop + 6);
            if (pick != null)
            {
                // Measured here, not in the constructor: the real font arrives only once the box is parented.
                pick.Width = TextRenderer.MeasureText(pick.Text, pick.Font).Width + 36;
                pick.Location = new Point(Math.Max(count.Right + 8, Box.Width - (expandable ? 30 : 0) - pick.Width), Box.Bottom + RowTop);
            }
        }
        void UpdateCount()
        {
            int n = LineCount; count.Text = CountText(n); count.ForeColor = MaximumLines > 0 && n > MaximumLines ? Theme.Danger : Theme.Muted;
            if (expanded) SetBoxHeight(ContentHeight()); Arrange();
        }
    }

    // Groups of check boxes with collapsible headers, laid out in columns.
    // A grouped list of buildings that draws itself (0.4.73): one row per building, a
    // collapsible header per group, only the rows in view are painted. No control per
    // building - 1900 buildings used to mean 1900 check boxes, seconds to open, and a
    // layout pass per keystroke in the search box.
    sealed class GroupedCheckList : Panel
    {
        sealed class Item { public string Id, Text, Tooltip; public bool Checked, Enabled, Hidden; }
        sealed class Group { public string Key, Title; public readonly List<Item> Items = new List<Item>(); }
        // What is laid out right now: a group header (Item == null) or one building cell.
        sealed class Cell { public Group Group; public Item Item; public Rectangle Bounds; }
        readonly List<Group> groups = new List<Group>(); readonly Dictionary<string, Item> items = new Dictionary<string, Item>(StringComparer.OrdinalIgnoreCase);
        readonly List<Cell> cells = new List<Cell>(); Cell hover; ToolTip tips;
        public readonly HashSet<string> Collapsed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        public Action<string, bool> Toggled;
        public int Columns = 3;
        const int HeaderHeight = 30, RowHeight = 24, GroupGap = 4, ItemsTop = 4, ItemsBottom = 6, LeftPad = 8;
        public GroupedCheckList()
        {
            AutoScroll = true; BackColor = Color.White; BorderStyle = BorderStyle.FixedSingle;
            SetStyle(ControlStyles.OptimizedDoubleBuffer | ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable, true);
        }
        public void Clear() { groups.Clear(); items.Clear(); cells.Clear(); hover = null; AutoScrollMinSize = Size.Empty; Invalidate(); }
        public void AddGroup(string key, string title) { groups.Add(new Group { Key = key, Title = title }); }
        public void AddItem(string groupKey, string id, string text, bool isChecked, bool enabled, string tooltip, ToolTip tips)
        {
            Group group = groups.First(g => g.Key == groupKey);
            var item = new Item { Id = id, Text = text, Checked = isChecked, Enabled = enabled, Tooltip = tooltip ?? "" };
            group.Items.Add(item); items[id] = item; if (tips != null) this.tips = tips;
        }
        public bool Contains(string id) { return items.ContainsKey(id); }
        // Visibility is a flag on the item; the next Relayout lays out only the shown ones.
        public void SetVisible(string id, bool visible) { Item item; if (items.TryGetValue(id, out item)) item.Hidden = !visible; }
        public void SetChecked(string id, bool isChecked) { Item item; if (items.TryGetValue(id, out item)) item.Checked = isChecked; }
        public bool IsVisible(string id) { Item item; return items.TryGetValue(id, out item) && !item.Hidden; }
        public void Remove(string id) { Item item; if (!items.TryGetValue(id, out item)) return; foreach (Group g in groups) g.Items.Remove(item); items.Remove(id); }
        public int VisibleCount { get { return items.Values.Count(x => !x.Hidden); } }
        public IEnumerable<string> Ids { get { return items.Keys; } }
        public void ExpandAll() { Collapsed.Clear(); Relayout(); }
        public void CollapseAll() { foreach (Group g in groups) Collapsed.Add(g.Key); Relayout(); }
        // Recomputes the cells; nothing is created, so this is cheap enough for every keystroke.
        public void Relayout()
        {
            cells.Clear(); hover = null;
            int width = Math.Max(300, ClientSize.Width), y = 0, columns = Math.Max(1, Columns), column = Math.Max(140, (width - LeftPad - 8) / columns);
            foreach (Group group in groups)
            {
                var shown = group.Items.Where(i => !i.Hidden).ToList();
                if (shown.Count == 0) continue;
                bool collapsed = Collapsed.Contains(group.Key);
                cells.Add(new Cell { Group = group, Bounds = new Rectangle(0, y, width, HeaderHeight) }); y += HeaderHeight;
                if (!collapsed)
                {
                    y += ItemsTop;
                    for (int i = 0; i < shown.Count; i++)
                    {
                        int row = i / columns, col = i % columns;
                        cells.Add(new Cell { Group = group, Item = shown[i], Bounds = new Rectangle(LeftPad + col * column, y + row * RowHeight, column - 6, RowHeight) });
                    }
                    y += ((shown.Count + columns - 1) / columns) * RowHeight + ItemsBottom;
                }
                y += GroupGap;
            }
            AutoScrollMinSize = new Size(0, y); Invalidate();
        }
        Cell HitTest(Point p)
        {
            int y = p.Y - AutoScrollPosition.Y;
            foreach (Cell cell in cells) if (cell.Bounds.Contains(p.X, y)) return cell;
            return null;
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            int offset = AutoScrollPosition.Y; var clip = e.ClipRectangle; clip.Offset(0, -offset);
            foreach (Cell cell in cells)
            {
                if (!cell.Bounds.IntersectsWith(clip)) continue;
                var r = cell.Bounds; r.Offset(0, offset);
                if (cell.Item == null)
                {
                    using (var fill = new SolidBrush(Theme.Chrome)) e.Graphics.FillRectangle(fill, r);
                    bool collapsed = Collapsed.Contains(cell.Group.Key); int shown = cell.Group.Items.Count(i => !i.Hidden);
                    using (var bold = new Font(Font, FontStyle.Bold))
                        TextRenderer.DrawText(e.Graphics, (collapsed ? "▸ " : "▾ ") + cell.Group.Title + "  (" + shown + ")", bold, new Rectangle(r.X + 8, r.Y, r.Width - 16, r.Height), Theme.Ink, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
                    continue;
                }
                Item item = cell.Item;
                System.Windows.Forms.VisualStyles.CheckBoxState state = item.Enabled ? (item.Checked ? System.Windows.Forms.VisualStyles.CheckBoxState.CheckedNormal : System.Windows.Forms.VisualStyles.CheckBoxState.UncheckedNormal) : (item.Checked ? System.Windows.Forms.VisualStyles.CheckBoxState.CheckedDisabled : System.Windows.Forms.VisualStyles.CheckBoxState.UncheckedDisabled);
                if (cell == hover && item.Enabled) state = item.Checked ? System.Windows.Forms.VisualStyles.CheckBoxState.CheckedHot : System.Windows.Forms.VisualStyles.CheckBoxState.UncheckedHot;
                Size glyph = CheckBoxRenderer.GetGlyphSize(e.Graphics, state);
                CheckBoxRenderer.DrawCheckBox(e.Graphics, new Point(r.X, r.Y + (r.Height - glyph.Height) / 2), state);
                TextRenderer.DrawText(e.Graphics, item.Text, Font, new Rectangle(r.X + glyph.Width + 4, r.Y, r.Width - glyph.Width - 4, r.Height), item.Enabled ? Theme.Ink : Theme.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
            }
        }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            base.OnMouseMove(e); Cell now = HitTest(e.Location);
            if (now != hover) { hover = now; if (tips != null) tips.SetToolTip(this, now != null && now.Item != null ? now.Item.Tooltip : ""); Invalidate(); }
        }
        protected override void OnMouseLeave(EventArgs e) { base.OnMouseLeave(e); if (hover != null) { hover = null; Invalidate(); } }
        protected override void OnMouseDown(MouseEventArgs e)
        {
            base.OnMouseDown(e); Focus(); if (e.Button != MouseButtons.Left) return;
            Cell cell = HitTest(e.Location); if (cell == null) return;
            if (cell.Item == null) { if (Collapsed.Contains(cell.Group.Key)) Collapsed.Remove(cell.Group.Key); else Collapsed.Add(cell.Group.Key); Relayout(); return; }
            if (!cell.Item.Enabled) return;
            cell.Item.Checked = !cell.Item.Checked; Invalidate();
            if (Toggled != null) Toggled(cell.Item.Id, cell.Item.Checked);
        }
        protected override void OnScroll(ScrollEventArgs se) { base.OnScroll(se); Invalidate(); }
        protected override void OnMouseWheel(MouseEventArgs e) { base.OnMouseWheel(e); Invalidate(); }
        protected override void OnResize(EventArgs e) { base.OnResize(e); if (groups.Count > 0) Relayout(); }
    }

    // The building picker: filters and a counter on top, the selected buildings in
    // the middle, every other building below, grouped by kind.
    sealed class BuildingPickerWindow : Form
    {
        readonly Language language; readonly List<BuildingEntry> all; readonly List<string> initial;
        readonly Dictionary<string, string> usedElsewhere; readonly Dictionary<string, BuildingEntry> byTarget = new Dictionary<string, BuildingEntry>(StringComparer.OrdinalIgnoreCase);
        readonly TextBox search = new TextBox(); readonly ComboBox origin = new ComboBox(), kind = new ComboBox(); readonly CheckBox obsolete = new CheckBox();
        readonly Label counter = Theme.Label("", 10, true); readonly GroupedCheckList selected = new GroupedCheckList(), available = new GroupedCheckList();
        readonly List<string> chosen = new List<string>(); readonly ToolTip tips = new ToolTip();
        readonly List<string> originKeys = new List<string>(), kindKeys = new List<string>();
        readonly Dictionary<string, string> kindLabels = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        // The search waits for a short pause in typing before it filters (0.4.73).
        readonly Timer searchTimer = new Timer { Interval = 150 };
        static readonly Dictionary<string, List<BuildingEntry>> cache = new Dictionary<string, List<BuildingEntry>>(StringComparer.OrdinalIgnoreCase);
        public List<string> Result;
        bool loading; readonly bool single;
        // Reads the building list on a background thread so the first picker opens without the wait (0.4.73).
        public static void Prime(string build, string workshopRoot)
        {
            string cacheKey = (build ?? "") + "|" + (workshopRoot ?? "");
            lock (cache) { if (cache.ContainsKey(cacheKey)) return; }
            System.Threading.ThreadPool.QueueUserWorkItem(delegate
            {
                try { var list = GameBuildings.Scan(build, workshopRoot); lock (cache) { if (!cache.ContainsKey(cacheKey)) cache[cacheKey] = list; } }
                catch (Exception) { }
            });
        }

        public BuildingPickerWindow(Language language, string build, string workshopRoot, IEnumerable<string> current, IDictionary<string, string> used, Font font, Icon icon)
            : this(language, build, workshopRoot, current, used, font, icon, false) { }
        // How a donor is written: a base-game building by its plain name, a Workshop building
        // as <item>\<object> - its target is <item>\<object>\building.ini, so the file name
        // comes off. An entry that is shaped differently is not a donor and drops out.
        static string DonorOf(BuildingEntry entry)
        {
            if (entry.Origin == "game") return entry.Name;
            string[] parts = entry.Target.Split('\\');
            return parts.Length == 3 && parts[2].Equals("building.ini", StringComparison.OrdinalIgnoreCase) ? parts[0] + "\\" + parts[1] : "";
        }

        // 0.4.80: donorsOnly restricts the list to the base game's buildings_types (the only buildings
        // a clone can start from) and works with their plain names; picking one replaces the choice.
        public BuildingPickerWindow(Language language, string build, string workshopRoot, IEnumerable<string> current, IDictionary<string, string> used, Font font, Icon icon, bool donorsOnly)
        {
            this.language = language; this.single = donorsOnly; initial = current.Select(x => x.Trim()).Where(x => x.Length > 0).ToList(); usedElsewhere = new Dictionary<string, string>(used ?? new Dictionary<string, string>(), StringComparer.OrdinalIgnoreCase);
            string cacheKey = (build ?? "") + "|" + (workshopRoot ?? "");
            lock (cache) { if (!cache.TryGetValue(cacheKey, out all)) { all = GameBuildings.Scan(build, workshopRoot); cache[cacheKey] = all; } }
            // A base-game donor is addressed by its plain name, a Workshop building by
            // <item>\<object> (0.4.86) - in both cases what the plugin writes into `donor`
            // becomes the identity here. DLC buildings are left out: the clone cannot
            // start from them.
            if (donorsOnly) all = all.Where(e => (e.Origin == "game" && e.Target.StartsWith("buildings_types\\", StringComparison.OrdinalIgnoreCase)) || e.Origin.StartsWith("workshop:", StringComparison.Ordinal))
                .Select(e => new BuildingEntry { Target = DonorOf(e), Name = e.Name, Type = e.Type, Origin = e.Origin, OriginLabel = e.OriginLabel, Obsolete = e.Obsolete })
                .Where(e => e.Target.Length > 0).ToList();
            foreach (BuildingEntry entry in all) byTarget[entry.Target] = entry;
            foreach (string target in initial) if (!byTarget.ContainsKey(target)) { var missing = new BuildingEntry { Target = target, Name = target, Type = "", Origin = "missing", OriginLabel = "" }; all = all.Concat(new[] { missing }).ToList(); byTarget[target] = missing; }
            chosen.AddRange(initial.Where(t => byTarget.ContainsKey(t)).Distinct(StringComparer.OrdinalIgnoreCase));
            Text = language.T("picker_title"); Font = font; Icon = icon; Size = new Size(1180, 820); MinimumSize = new Size(900, 600); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 6, Padding = new Padding(12) };
            shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 44)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 36)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 200)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 26)); shell.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute, 52));
            Controls.Add(shell);
            // Filters.
            var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, Margin = Padding.Empty };
            var searchLabel = Theme.Label(language.T("picker_search"), 10, false); searchLabel.Margin = new Padding(0, 8, 6, 0); bar.Controls.Add(searchLabel);
            search.Width = 200; search.Margin = new Padding(0, 4, 14, 0); search.TextChanged += delegate { searchTimer.Stop(); searchTimer.Start(); }; searchTimer.Tick += delegate { searchTimer.Stop(); ApplyFilter(); }; FormClosed += delegate { searchTimer.Dispose(); }; bar.Controls.Add(search);
            origin.DropDownStyle = ComboBoxStyle.DropDownList; origin.Width = 190; origin.Margin = new Padding(0, 4, 14, 0); bar.Controls.Add(origin);
            kind.DropDownStyle = ComboBoxStyle.DropDownList; kind.Width = 220; kind.Margin = new Padding(0, 4, 14, 0); bar.Controls.Add(kind);
            obsolete.Text = language.T("picker_obsolete"); obsolete.AutoSize = true; obsolete.Margin = new Padding(0, 8, 14, 0); obsolete.CheckedChanged += delegate { ApplyFilter(); }; bar.Controls.Add(obsolete);
            shell.Controls.Add(bar, 0, 0);
            // Counter row with the expand/collapse buttons at its right.
            var counterRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, Margin = Padding.Empty };
            counterRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); counterRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            counter.Dock = DockStyle.Fill; counter.Margin = new Padding(0, 6, 0, 0); counterRow.Controls.Add(counter, 0, 0);
            var fold = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = Padding.Empty };
            var expandAll = Theme.Button(language.T("picker_expand"), () => { selected.ExpandAll(); available.ExpandAll(); }, false); expandAll.Height = 28; expandAll.MinimumSize = new Size(60, 28); expandAll.Margin = new Padding(0, 0, 8, 0); fold.Controls.Add(expandAll);
            var collapseAll = Theme.Button(language.T("picker_collapse"), () => { selected.CollapseAll(); available.CollapseAll(); }, false); collapseAll.Height = 28; collapseAll.MinimumSize = new Size(60, 28); collapseAll.Margin = Padding.Empty; fold.Controls.Add(collapseAll);
            counterRow.Controls.Add(fold, 1, 0); shell.Controls.Add(counterRow, 0, 1);
            selected.Dock = DockStyle.Fill; selected.Margin = new Padding(0, 0, 0, 6); shell.Controls.Add(selected, 0, 2);
            var availableTitle = Theme.Label(language.T("picker_available"), 10, true); availableTitle.Margin = new Padding(0, 4, 0, 0); shell.Controls.Add(availableTitle, 0, 3);
            available.Dock = DockStyle.Fill; available.Margin = Padding.Empty; shell.Controls.Add(available, 0, 4);
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Margin = new Padding(0, 10, 0, 0) };
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Theme.Button(language.T("apply"), () => { Result = new List<string>(chosen); DialogResult = DialogResult.OK; Close(); }, true);
            buttons.Controls.Add(apply); buttons.Controls.Add(cancel); shell.Controls.Add(buttons, 0, 5); AcceptButton = apply; CancelButton = cancel;
            selected.Toggled = (id, on) => { if (!on) Unselect(id); };
            available.Toggled = (id, on) => { if (on) Select(id); };
            FillFilters(); Populate(); ApplyFilter();
        }
        string OriginLabel(BuildingEntry e)
        {
            if (e.Origin == "game") return language.T("picker_game");
            if (e.Origin.StartsWith("dlc", StringComparison.Ordinal)) return language.Format("picker_dlc", e.OriginLabel);
            if (e.Origin == "missing") return language.T("picker_not_found");
            return language.T("picker_workshop") + ": " + e.OriginLabel;
        }
        string KindLabelCached(string type) { string label; if (!kindLabels.TryGetValue(type, out label)) { label = KindLabel(type); kindLabels[type] = label; } return label; }
        string KindLabel(string type) { if (type.Length == 0) return language.T("picker_kind_unknown"); string key = "btype." + type.ToLowerInvariant(); string text = language.T(key); return text == key ? type : text; }
        void FillFilters()
        {
            loading = true;
            origin.Items.Add(language.T("picker_all_origins")); originKeys.Add("");
            foreach (var group in all.Where(e => e.Origin != "missing").GroupBy(e => e.Origin).OrderBy(g => g.First().OriginRank).ThenBy(g => g.First().OriginLabel, StringComparer.CurrentCultureIgnoreCase))
            { origin.Items.Add(OriginLabel(group.First())); originKeys.Add(group.Key); }
            origin.SelectedIndex = 0; origin.SelectedIndexChanged += delegate { if (!loading) ApplyFilter(); };
            kind.Items.Add(language.T("picker_all_kinds")); kindKeys.Add("");
            foreach (string type in all.Select(e => e.Type).Distinct().OrderBy(t => KindLabel(t), StringComparer.CurrentCultureIgnoreCase)) { kind.Items.Add(KindLabel(type)); kindKeys.Add(type); }
            kind.SelectedIndex = 0; kind.SelectedIndexChanged += delegate { if (!loading) ApplyFilter(); };
            loading = false;
        }
        IEnumerable<IGrouping<string, BuildingEntry>> Grouped(IEnumerable<BuildingEntry> entries)
        { return entries.GroupBy(e => e.Origin == "missing" ? "missing" : e.Type).OrderBy(g => g.Key == "missing" ? 1 : 0).ThenBy(g => KindLabel(g.Key == "missing" ? "" : g.Key), StringComparer.CurrentCultureIgnoreCase); }
        string ItemText(BuildingEntry e) { return e.Name + "  · " + OriginLabel(e) + (e.Obsolete ? "  · " + language.T("picker_obsolete_tag") : ""); }
        void Populate()
        {
            selected.Clear(); available.Clear();
            var chosenSet = new HashSet<string>(chosen, StringComparer.OrdinalIgnoreCase);
            foreach (var group in Grouped(all))
            {
                string key = group.Key; string title = key == "missing" ? language.T("picker_not_found") : KindLabel(key);
                selected.AddGroup(key, title); available.AddGroup(key, title);
                foreach (BuildingEntry e in group.OrderBy(x => x.OriginRank).ThenBy(x => x.Name, StringComparer.CurrentCultureIgnoreCase))
                {
                    string other; bool used = usedElsewhere.TryGetValue(e.Target, out other) && !chosenSet.Contains(e.Target);
                    string text = ItemText(e) + (used ? "  · " + language.Format("picker_used", other) : "");
                    if (chosenSet.Contains(e.Target)) selected.AddItem(key, e.Target, ItemText(e), true, true, e.Target, tips);
                    available.AddItem(key, e.Target, text, false, !used, used ? language.Format("picker_used", other) : e.Target, tips);
                    if (chosenSet.Contains(e.Target)) available.SetVisible(e.Target, false);
                }
            }
        }
        bool Passes(BuildingEntry e)
        {
            if (e.Origin == "missing") return false;
            string needle = search.Text.Trim();
            if (needle.Length > 0 && e.Name.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0 && e.Target.IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0 && KindLabelCached(e.Type).IndexOf(needle, StringComparison.CurrentCultureIgnoreCase) < 0) return false;
            string o = origin.SelectedIndex >= 0 ? originKeys[origin.SelectedIndex] : ""; if (o.Length > 0 && e.Origin != o) return false;
            string k = kind.SelectedIndex >= 0 ? kindKeys[kind.SelectedIndex] : ""; if (k.Length > 0 && e.Type != k) return false;
            if (e.Obsolete && !obsolete.Checked) return false;
            return true;
        }
        void ApplyFilter()
        {
            var chosenSet = new HashSet<string>(chosen, StringComparer.OrdinalIgnoreCase);
            foreach (BuildingEntry e in all) available.SetVisible(e.Target, !chosenSet.Contains(e.Target) && Passes(e));
            available.Relayout(); selected.Relayout(); UpdateCounter();
        }
        void UpdateCounter()
        {
            counter.Text = language.Format("picker_count", chosen.Count, available.VisibleCount, all.Count(e => e.Origin != "missing"));
            counter.ForeColor = Theme.Ink;
        }
        void Select(string target)
        {
            if (chosen.Contains(target, StringComparer.OrdinalIgnoreCase)) return;
            if (single) foreach (string other in chosen.ToList()) Unselect(other);
            chosen.Add(target); BuildingEntry e = byTarget[target];
            available.SetVisible(target, false); selected.AddItem(e.Origin == "missing" ? "missing" : e.Type, target, ItemText(e), true, true, target, tips);
            available.Relayout(); selected.Relayout(); UpdateCounter();
        }
        void Unselect(string target)
        {
            chosen.RemoveAll(t => t.Equals(target, StringComparison.OrdinalIgnoreCase)); selected.Remove(target);
            BuildingEntry e; if (byTarget.TryGetValue(target, out e) && e.Origin != "missing") { available.SetChecked(target, false); available.SetVisible(target, Passes(e)); }
            available.Relayout(); selected.Relayout(); UpdateCounter();
        }
        internal int Total { get { return all.Count(e => e.Origin != "missing"); } }
        internal int SelectedCount { get { return chosen.Count; } }
        internal int VisibleCount { get { return available.VisibleCount; } }
        internal string CounterText { get { return counter.Text; } }
        internal void TestSearch(string text) { search.Text = text; searchTimer.Stop(); ApplyFilter(); }
        internal void TestObsolete(bool show) { obsolete.Checked = show; }
        internal void TestSelect(string target) { Select(target); }
        internal void TestUnselect(string target) { Unselect(target); }
        internal List<string> TestResult() { return new List<string>(chosen); }
    }

    // 0.4.80: the lines of a clone's donor building.ini, to take one over and change it. Every line
    // with a $TOKEN is offered; the chosen one lands in the edit box, where the numbers are adjusted
    // before it goes into the rule set.
    sealed class DonorLinesWindow : Form
    {
        readonly ListBox lines = new ListBox(); readonly TextBox edit = new TextBox(); readonly TextBox search = new TextBox();
        readonly List<string> all;
        public string Result { get; private set; }
        public DonorLinesWindow(Language language, string build, string donor, Font font, Icon icon)
        {
            all = GameBuildings.DonorLines(build, donor);
            Text = language.T("donor_lines_title"); Font = font; Icon = icon; Size = new Size(860, 660); MinimumSize = new Size(640, 460); StartPosition = FormStartPosition.CenterParent; ShowInTaskbar = false; BackColor = Color.White;
            Theme.ApplyWindowChrome(this);
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, Padding = new Padding(12) };
            Controls.Add(shell);
            Action<Control, RowStyle> row = (control, style) => { shell.RowStyles.Add(style); control.Dock = DockStyle.Fill; shell.Controls.Add(control, 0, shell.RowStyles.Count - 1); };
            var header = Theme.Label(donor + (all.Count > 0 ? "" : "  ·  " + language.T("donor_lines_none")), 12, true); header.Margin = new Padding(0, 0, 0, 6);
            if (all.Count == 0) header.ForeColor = Theme.Danger;
            row(header, new RowStyle(SizeType.AutoSize));
            var searchRow = new TableLayoutPanel { ColumnCount = 2, Margin = Padding.Empty, AutoSize = true };
            searchRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); searchRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var searchLabel = Theme.Label(language.T("picker_search"), 10, false); searchLabel.Margin = new Padding(0, 10, 8, 0); searchRow.Controls.Add(searchLabel, 0, 0);
            search.AccessibleName = "donor-search"; Control searchHost = Fields.Wrap(search); searchHost.Dock = DockStyle.Top; searchRow.Controls.Add(searchHost, 1, 0);
            search.TextChanged += delegate { Refill(); };
            row(searchRow, new RowStyle(SizeType.AutoSize));
            lines.Font = new Font("Consolas", 10f); lines.IntegralHeight = false; lines.BorderStyle = BorderStyle.FixedSingle; lines.AccessibleName = "donor-lines";
            lines.SelectedIndexChanged += delegate { if (lines.SelectedItem != null) edit.Text = Convert.ToString(lines.SelectedItem); };
            lines.DoubleClick += delegate { Apply(); };
            row(lines, new RowStyle(SizeType.Percent, 100));
            var editLabel = Theme.Label(language.T("donor_lines_new"), 10, true); editLabel.Margin = new Padding(0, 8, 0, 2);
            row(editLabel, new RowStyle(SizeType.AutoSize));
            edit.Font = new Font("Consolas", 10f); edit.AccessibleName = "donor-new";
            Control host = Fields.Wrap(edit); host.Dock = DockStyle.Top; row(host, new RowStyle(SizeType.AutoSize));
            var buttons = new FlowLayoutPanel { FlowDirection = FlowDirection.RightToLeft, WrapContents = false, Margin = new Padding(0, 10, 0, 0) };
            var cancel = Theme.Button(language.T("cancel"), () => { DialogResult = DialogResult.Cancel; Close(); }, false); cancel.Margin = new Padding(8, 0, 0, 0);
            var apply = Theme.Button(language.T("apply"), Apply, true);
            buttons.Controls.Add(cancel); buttons.Controls.Add(apply);
            row(buttons, new RowStyle(SizeType.AutoSize));
            AcceptButton = apply; CancelButton = cancel;
            Refill();
        }
        void Refill()
        {
            string query = search.Text.Trim();
            lines.BeginUpdate();
            try { lines.Items.Clear(); foreach (string line in all) if (query.Length == 0 || line.IndexOf(query, StringComparison.CurrentCultureIgnoreCase) >= 0) lines.Items.Add(line); }
            finally { lines.EndUpdate(); }
        }
        void Apply()
        {
            string value = edit.Text.Trim();
            if (value.Length == 0) { DialogResult = DialogResult.Cancel; Close(); return; }
            Result = value; DialogResult = DialogResult.OK; Close();
        }
        // Test hooks.
        internal int LineCount { get { return lines.Items.Count; } }
        internal void TestSearch(string text) { search.Text = text; }
        internal void TestChoose(int index, string text)
        { if (index >= 0 && index < lines.Items.Count) lines.SelectedIndex = index; if (text != null) edit.Text = text; Apply(); }
    }
}
