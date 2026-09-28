using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Windows.Forms;

namespace TesmioAutoload
{
    public sealed partial class MainForm : Form
    {
        readonly UiState state; readonly UiStateStore stateStore; bool persistUi;   // a reset switches persisting off (0.4.94)
        readonly ModList mods = new ModList(); readonly TextBox search = new TextBox();
        Button saveButton, startButton;   // 0.4.51: primary only while something is unsaved
        readonly Label searchLabel = Theme.Label("",10,false), pluginLabel = Theme.Label("",15,true);
        readonly Label heading = Theme.Label("",22,true), description = Theme.Label("",11,false), switchLabel = Theme.Label("",11,false), switchNote = Theme.Label("",8,false);
        readonly ToggleSwitch activation = new ToggleSwitch();
        readonly Label status = Theme.Label("",12,true), statusDetail = Theme.Label("",9,false);
        readonly TabStrip tabStrip = new TabStrip(); readonly FlowLayoutPanel content = new FlowLayoutPanel();
        // Status bar below the tabs (0.4.12): load path of the DLL, where the files live, last game start.
        readonly Panel statusBar = new Panel(); readonly FlowLayoutPanel statusFlow = new FlowLayoutPanel(); readonly Label statusRight = new Label();
        readonly Dictionary<Button,string> translatedButtons = new Dictionary<Button,string>();
        readonly Dictionary<SidebarButton,string> translatedSidebar = new Dictionary<SidebarButton,string>();
        readonly List<Button> actions = new List<Button>(); readonly List<CatalogEntry> entries = new List<CatalogEntry>();
        // 0.4.71: everything an entry's editor holds in memory lives in a Workspace, so an entry with
        // unsaved changes can be parked while another one is edited, and all of them are saved in one go.
        sealed class Workspace
        {
            public CatalogEntry Entry; public Session Session; public Presentation Presentation; public LocalResourceSession ResourceSession; public LocalEditorSpec LocalSpec; public TextPackSession TextPack; public ContentSession Content;
            public string SelectedLocalResource="", LastAction="ready", SelectedTab="", LanguageCode=""; public bool Valid=true;
            public readonly Dictionary<string,ResourceRegistry> Registries=new Dictionary<string,ResourceRegistry>(StringComparer.OrdinalIgnoreCase);
            public readonly Dictionary<string,string> Draft=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase), Baseline=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            public readonly HashSet<string> Resets=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        }
        Workspace work=new Workspace();
        // Parked workspaces with unsaved changes, by entry root; the shown entry is never in here.
        readonly Dictionary<string,Workspace> parked=new Dictionary<string,Workspace>(StringComparer.OrdinalIgnoreCase);
        Dictionary<string,string> draft {get{return work.Draft;}}
        Dictionary<string,string> baseline {get{return work.Baseline;}}
        HashSet<string> resets {get{return work.Resets;}}
        Dictionary<string,ResourceRegistry> registries {get{return work.Registries;}}
        Session session {get{return work.Session;}set{work.Session=value;}}
        Presentation presentation {get{return work.Presentation;}set{work.Presentation=value;}}
        LocalResourceSession resourceSession {get{return work.ResourceSession;}set{work.ResourceSession=value;}}
        LocalEditorSpec localSpec {get{return work.LocalSpec;}set{work.LocalSpec=value;}}
        // 0.4.80: a content package (no DLL) has neither Session nor editor, only its switch.
        ContentSession contentSession {get{return work.Content;}set{work.Content=value;}}
        string selectedLocalResource {get{return work.SelectedLocalResource;}set{work.SelectedLocalResource=value;}}
        string lastAction {get{return work.LastAction;}set{work.LastAction=value;}}
        readonly Dictionary<string,Action<string>> setters = new Dictionary<string,Action<string>>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string,Label> origins = new Dictionary<string,Label>(StringComparer.OrdinalIgnoreCase);
        readonly Dictionary<string,VectorButton> resetButtons = new Dictionary<string,VectorButton>(StringComparer.OrdinalIgnoreCase);   // 0.4.38: shown only while the value differs from the default
        readonly IconCache icons = new IconCache(); readonly ToolTip tips = new ToolTip(); readonly StringBuilder journal = new StringBuilder();
        readonly Panel header; readonly SidebarButton languageButton; Button restoreButton; TableLayoutPanel page;
        CatalogEntry current; Language language;
        bool selecting, initialized, refreshing, viewWarning, resizing;
        string lastConsistencyAlert="";
        internal Func<DialogResult> PendingPrompt = null;
        bool cliMode; string cliError="";
        internal Func<string,DialogResult> ResourceRemovePrompt = null;

        public MainForm(UiState initial, UiStateStore store, bool saveUi)
        {
            state = initial.Copy(); stateStore = store; persistUi = saveUi; language = new Language(state.Language); Theme.CopyMenuLabel=()=>language.T("copy_text");
            mods.Cache=icons; icons.Warning=message=>Report(language.T("icon_warning")+" "+message); icons.LoaderExe=()=>Path.Combine(state.Build,"tesmiolauncher.exe");
            Text = "Republic Mod Manager 0.5.21  ·  "+language.T("app_dependency"); Font = new Font("Segoe UI",10); ForeColor = Theme.Ink; BackColor = Color.White;Theme.ApplyWindowChrome(this);
            AutoScaleDimensions = new SizeF(96,96); AutoScaleMode = AutoScaleMode.Dpi; Size = new Size(1600,1000); MinimumSize = new Size(1560,760); StartPosition = FormStartPosition.CenterScreen;   // 0.4.36: minimum 1560 - the section editors' detail panel needs it; RestoreWindowSize caps it to the screen
            Load += (s,e) => RestoreWindowSize();
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Tesmio.icon")) using (var icon = new Icon(stream)) Icon = (Icon)icon.Clone();
            var shell = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 2, ColumnCount = 1, Margin = Padding.Empty, Padding = Padding.Empty };
            shell.RowStyles.Add(new RowStyle(SizeType.Percent,100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute,113)); Controls.Add(shell);
            var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Margin = Padding.Empty };
            body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,330)); body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); shell.Controls.Add(body,0,0);
            var sidebar = new TableLayoutPanel { BackColor = Theme.Navy, Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 6, Padding = new Padding(12,20,12,0), Margin = Padding.Empty };
            // rows: title, search label, search box + refresh, filter chips (0.4.98: a little more
            // air under the search line so the chips do not sit right against it)
            foreach (float height in new float[] {38,24,37,38}) sidebar.RowStyles.Add(new RowStyle(SizeType.Absolute,height));
            sidebar.RowStyles.Add(new RowStyle(SizeType.Percent,100)); sidebar.RowStyles.Add(new RowStyle(SizeType.Absolute,58));
            pluginLabel.ForeColor = Color.White; sidebar.Controls.Add(pluginLabel,0,0); searchLabel.ForeColor = Color.FromArgb(185,204,226); sidebar.Controls.Add(searchLabel,0,1);
            // 0.4.94: the refresh button sits next to the search box, not in the window row below -
            // it reads the catalog again and therefore belongs to the list, and it leaves the row
            // below one slot of air.
            var searchRow = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Margin = Padding.Empty, Padding = Padding.Empty, BackColor = Theme.Navy };
            // 0.4.98: the search box is a field like every other one (37 px, framed) instead of a
            // bare text box of font height - next to the square button the two now share one line.
            searchRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); searchRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,43));
            search.AccessibleName = "Plugin search"; search.TextChanged += (s,e) => FilterList();
            var searchField=Fields.Wrap(search); searchField.Dock=DockStyle.Fill; searchField.Margin=Padding.Empty; searchRow.Controls.Add(searchField,0,0);
            var refresh=Sidebar("refresh","reload",()=>Scan(true)); refresh.Accent=true; refresh.Size=new Size(Fields.Height,Fields.Height); refresh.Margin=new Padding(6,0,0,0); searchRow.Controls.Add(refresh,1,0);
            sidebar.Controls.Add(searchRow,0,2);
            sidebar.Controls.Add(FilterChips(),0,3);
            mods.Dock = DockStyle.Fill; mods.Margin = new Padding(-8,0,-8,0); mods.SelectedIndexChanged += (s,e) => { if (!selecting) Run(SelectionChanged); }; sidebar.Controls.Add(mods,0,4);
            var navigation=new SidebarBar();
            navigation.Controls.Add(Sidebar("folders","folder",ChooseFolders));
            navigation.Controls.Add(Sidebar("startcheck","check",ShowStartCheck));
            navigation.Controls.Add(Sidebar("log","document",ShowLog));
            navigation.Controls.Add(Sidebar("profiles","profile",ShowProfiles));
            languageButton=Sidebar("language","text:DE",ChooseLanguage); navigation.Controls.Add(languageButton);
            navigation.Controls.Add(Sidebar("options","sliders",ShowOptions));
            sidebar.Controls.Add(navigation,0,5);
            body.Controls.Add(sidebar,0,0);
            page = new TableLayoutPanel { Dock = DockStyle.Fill, RowCount = 4, ColumnCount = 1, Padding = new Padding(26,20,24,0), Margin = Padding.Empty };
            page.RowStyles.Add(new RowStyle(SizeType.Absolute,105)); page.RowStyles.Add(new RowStyle(SizeType.Absolute,48)); page.RowStyles.Add(new RowStyle(SizeType.Absolute,36)); page.RowStyles.Add(new RowStyle(SizeType.Percent,100)); body.Controls.Add(page,1,0);
            header = new Panel { Dock = DockStyle.Fill, Margin = Padding.Empty };
            heading.Location = new Point(0,0); description.Location = new Point(0,45); description.ForeColor=Theme.Muted;
            var enableBox = new Panel { Dock = DockStyle.Right, Width = 252 }; switchLabel.Location=new Point(0,12); activation.Location=new Point(183,4); switchNote.Location=new Point(0,42); switchNote.MaximumSize=new Size(244,0); switchNote.ForeColor=Theme.Muted;
            enableBox.Controls.AddRange(new Control[] {switchLabel,activation,switchNote});
            // The caption sits right beside the switch, whatever its translated width.
            switchLabel.SizeChanged+=(s,e)=>switchLabel.Left=Math.Max(0,activation.Left-switchLabel.Width-8);
            // 0.4.88: the package's own preview image, left of the title - a list of names becomes a
            // list of mods. 64 px, nothing clickable; missing or unreadable simply stays hidden.
            preview=new PictureBox{Size=new Size(64,64),SizeMode=PictureBoxSizeMode.Zoom,Location=new Point(0,4),Visible=false};
            header.Controls.AddRange(new Control[] {preview,heading,description,enableBox}); page.Controls.Add(header,0,0);
            activation.CheckedChanged += (s,e) => Run(ActivationChanged);
            // The header text wraps beside the switch boxes and the header row grows
            // with it, so the description never runs under the switches or into the
            // tab strip - on the first paint as well as after every resize.
            header.SizeChanged += (s,e) => LayoutHeader(); description.TextChanged += (s,e) => LayoutHeader(); heading.TextChanged += (s,e) => LayoutHeader();
            ResizeEnd += (s,e) => LayoutHeader();
            tabStrip.Dock=DockStyle.Fill; tabStrip.AutoScroll=true; page.Controls.Add(tabStrip,0,1);
            statusBar.Dock=DockStyle.Fill; statusBar.Margin=Padding.Empty; statusBar.BackColor=Theme.Pale; statusBar.Padding=new Padding(12,0,12,0); statusBar.Visible=false; statusBar.AccessibleName="status-bar";
            statusRight.AutoSize=true; statusRight.Dock=DockStyle.Right; statusRight.Font=new Font("Segoe UI",9.5f); statusRight.ForeColor=Theme.Muted; statusRight.Padding=new Padding(8,9,0,0); statusRight.UseMnemonic=false; statusRight.ContextMenuStrip=Theme.CopyMenu();
            statusFlow.Dock=DockStyle.Fill; statusFlow.WrapContents=false; statusFlow.Margin=Padding.Empty; statusFlow.Padding=new Padding(0,8,0,0);
            statusBar.Controls.Add(statusFlow); statusBar.Controls.Add(statusRight); page.Controls.Add(statusBar,0,2);
            content.Dock=DockStyle.Fill; content.FlowDirection=FlowDirection.TopDown; content.WrapContents=false; content.AutoScroll=true; content.Margin=Padding.Empty; content.Padding=new Padding(0,12,0,0); content.SizeChanged+=(s,e)=>ResizeCards(); content.ClientSizeChanged+=(s,e)=>ResizeCards(); page.Controls.Add(content,0,3);
            var footer = new TableLayoutPanel { Dock=DockStyle.Fill, ColumnCount=2, RowCount=1, BackColor=Theme.Frame, Padding=new Padding(22,17,18,10), Margin=Padding.Empty };
            footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize)); shell.Controls.Add(footer,0,1);
            var statuses=new FlowLayoutPanel { Dock=DockStyle.Fill, FlowDirection=FlowDirection.TopDown, WrapContents=false }; statuses.Controls.Add(status); statuses.Controls.Add(statusDetail); statusDetail.MaximumSize=new Size(440,0); footer.Controls.Add(statuses,0,0);
            var buttons = new FlowLayoutPanel { AutoSize=true, FlowDirection=FlowDirection.LeftToRight, WrapContents=false, Anchor=AnchorStyles.Right|AnchorStyles.Top };
            restoreButton=Button("restore",RestoreOriginal,false,false); restoreButton.Visible=false; restoreButton.Enabled=false; buttons.Controls.Add(restoreButton);
            var resetAll=Button("reset",ResetAll,false,true);resetAll.BackColor=Color.White;buttons.Controls.Add(resetAll); saveButton=Button("save",SaveAction,false,true);saveButton.BackColor=Color.White;buttons.Controls.Add(saveButton); startButton=Button("save_start",Launch,true,true);buttons.Controls.Add(startButton); footer.Controls.Add(buttons,1,0);
            TranslateShell(); SetActions(false); Report(language.T("startup"));
            Shown+=(s,e)=>{ if(!initialized) Run(InitializeCatalog); };
            FormClosing+=(s,e)=>{ try { if(!ResolvePending(false)) e.Cancel=true; else SaveView(); } catch(Exception failure) { e.Cancel=true; ShowError(failure); } };
        }
        Button Button(string key,Action action,bool primary,bool packageAction)
        { var b=Theme.Button(language.T(key),()=>Run(action),primary); translatedButtons[b]=key; if(packageAction) actions.Add(b); return b; }
        SidebarButton Sidebar(string key,string glyph,Action action)
        {
            var button=new SidebarButton {Glyph=glyph}; button.Click+=(s,e)=>Run(action); translatedSidebar[button]=key; return button;
        }
        PictureBox preview;
        // The preview image of the shown entry, if the package has one.
        void ShowPreview(string root)
        {
            if(preview==null)return;
            Image old=preview.Image; preview.Image=null; if(old!=null)old.Dispose();
            preview.Visible=false;
            if(String.IsNullOrEmpty(root))return;
            string file;
            try{file=SafeFiles.Child(root,"previewimage.png");}catch(Exception){return;}
            if(!File.Exists(file))return;
            // Loaded through a copy so the file stays free for a Workshop update.
            try{using(var stream=new MemoryStream(SafeFiles.Read(file,8*1024*1024)))preview.Image=Image.FromStream(stream);preview.Visible=true;}
            catch(Exception){preview.Visible=false;}
        }
        void LayoutHeader()
        {
            if(header==null||page==null||heading==null||description==null) return;
            int reserved=268;
            int shift=preview!=null&&preview.Visible?76:0;
            heading.Left=shift; description.Left=shift;
            int w=Math.Max(200,header.ClientSize.Width-reserved-shift);
            // Both labels are measured explicitly: a narrow window wraps the heading
            // too, and the description has to start below the wrapped heading.
            TextFormatFlags wrap=TextFormatFlags.WordBreak|TextFormatFlags.NoPrefix;
            int headingHeight=Math.Max(heading.Font.Height,TextRenderer.MeasureText(heading.Text.Length==0?" ":heading.Text,heading.Font,new Size(w,Int32.MaxValue),wrap).Height);
            heading.AutoSize=false; heading.MaximumSize=Size.Empty; heading.Size=new Size(w,headingHeight+2);
            // At most five lines of description; the tooltip on the heading has the rest.
            int wrapped=description.Text.Length==0?0:TextRenderer.MeasureText(description.Text,description.Font,new Size(w,Int32.MaxValue),wrap).Height;
            int lines=Math.Max(1,(int)Math.Ceiling(wrapped/(double)Math.Max(1,description.Font.Height)));
            int shown=Math.Min(lines,5)*description.Font.Height+4;
            // The X stays at the shift of the preview image: setting Location with 0 here put the
            // description back under the image and cut off its first words (0.4.89).
            description.AutoSize=false; description.MaximumSize=Size.Empty; description.Location=new Point(shift,heading.Bottom+6); description.Size=new Size(w,shown); description.AutoEllipsis=lines>5;
            int need=Math.Max(105,description.Bottom+18);
            if(page.RowStyles.Count>0 && (int)page.RowStyles[0].Height!=need)
            {
                page.RowStyles[0].Height=need;
                // A row style changed from inside a layout pass (a live resize) is
                // not applied until the next pass, so ask for one right after this.
                if(IsHandleCreated) BeginInvoke(new Action(()=>{ page.PerformLayout(); header.PerformLayout(); }));
                else page.PerformLayout();
            }
        }
        void TranslateShell()
        {
            pluginLabel.Text=language.T("plugins"); searchLabel.Text=language.T("search"); switchLabel.Text=language.T("enabled"); activation.AccessibleName=switchLabel.Text;
            foreach(var pair in translatedButtons) pair.Key.Text=language.T(pair.Value); UpdateStatus();
            foreach(var pair in translatedSidebar) { pair.Key.AccessibleName=language.T(pair.Value); tips.SetToolTip(pair.Key,language.T(pair.Value)); }
            languageButton.Glyph="text:"+LanguageInitials(); languageButton.Invalidate();
            tips.SetToolTip(mods,language.T("activity_help"));
        }
        string LanguageInitials()
        { string code=language.Code.Split('-')[0].ToUpperInvariant(); return code.Length>2?code.Substring(0,2):code; }
        void SetActions(bool enabled) { foreach(var button in actions) button.Enabled=enabled; restoreButton.Enabled=enabled&&session!=null&&session.Package.Installed&&session.Package.HasConfig; activation.Enabled=enabled && (contentSession!=null || resourceSession!=null || session!=null && presentation!=null && session.SwitchMode!="none" && session.Package.Visible); }
        void Run(Action action) { try { action(); } catch(Exception e) { ShowError(e); } }
        // The one header switch, "plugin active": the loader entry (or the bridge
        // list) plus the INI's own enabled key. Switching on sets both; switching off
        // only stops the loader, the INI keeps its values.
        void ActivationChanged()
        {
            if(refreshing) return; bool on=activation.Checked;
            // 0.4.88: switching something off that a saved game still needs is the one click that
            // can cost a world. The question names the saves; No puts the switch back.
            if(!on&&!SaveGamesAllowOff()) { refreshing=true; try{activation.Checked=true;}finally{refreshing=false;} return; }
            if(contentSession!=null) { contentSession.PendingOn=on; lastAction="ready"; UpdateStatus(); return; }
            if(resourceSession!=null) { if(session!=null&&session.LoaderSwitchApplies) session.SetLoaderEnabled(on); resourceSession.SetLoaderEnabled(on); lastAction="ready"; if(on && localSpec.ActivityField!=null) BuildLocalResourceEditor(); else { SyncActivation(); UpdateStatus(); } return; }
            if(session==null || presentation==null) return;
            if(session.LoaderSwitchApplies) { session.SetLoaderEnabled(on); lastAction="ready"; }
            if(presentation.EnabledField.Length>0 && (on || !session.LoaderSwitchApplies)) Edit(presentation.EnabledField,on?"1":"0"); else UpdateStatus();
            SyncActivation();
        }
        void SyncActivation()
        {
            refreshing=true;
            try
            {
                if(contentSession!=null) { activation.Checked=contentSession.PendingOn; return; }
                if(resourceSession!=null) { bool loaded=session!=null?(!session.LoaderSwitchApplies||session.LoaderEnabled):resourceSession.LoaderEnabled; activation.Checked=loaded&&resourceSession.ActivityOn; return; }
                if(session==null || presentation==null) { activation.Checked=false; return; }
                bool loader=!session.LoaderSwitchApplies || session.LoaderEnabled, ini=presentation.EnabledField.Length==0 || draft[presentation.EnabledField]=="1";
                activation.Checked=session.SwitchMode=="ini" ? ini : loader && ini;
            }
            finally { refreshing=false; }
        }
        void ShowSwitch(bool visible,string note)
        // The switch explains itself; the note only survives as a tooltip on it.
        { switchLabel.Visible=visible; activation.Visible=visible; switchNote.Visible=false; switchNote.Text=""; tips.SetToolTip(activation,note); tips.SetToolTip(switchLabel,note); }
        // 0.5.1: true when at least one part of this content package would land in a plugin that
        // Soviet Mod Loader hosts - then SML merges the package itself and RMM keeps its hands off.
        internal bool SmlOwnsContent(ContentSession c)
        {
            if(c==null) return false;
            foreach(var pair in c.Targets) if(pair.Value.Length>0&&Sml.Hosts(state.Build,pair.Value)) return true;
            return false;
        }
        internal bool SwitchVisible { get { return activation.Visible; } }
        // 0.5.16: the shown entry's dot - paused (amber) means blocked by SML and not counted as active.
        internal bool ShownPaused { get { bool active; return current!=null&&mods.BlockedRoots.Contains(current.Root)&&!(mods.ActiveStates.TryGetValue(current.Root,out active)&&active); } }
        internal bool SwitchChecked { get { return activation.Checked; } }
        internal string SwitchNote { get { return tips.GetToolTip(activation) ?? ""; } }
        internal void TestSwitch(bool on) { activation.Checked=on; }
        string ErrorText(Exception e)
        {
            var rule=e as RuleException;
            if(rule==null)return language.Localize(e.Message);
            if(rule.TranslationKey=="resource_custom_needs_transport")return language.Format(rule.TranslationKey,rule.TranslationArguments);
            if(rule.TranslationKey=="error_positive_collection_required"&&rule.TranslationArguments.Length>0&&presentation!=null)
            {
                string collectionId=Convert.ToString(rule.TranslationArguments[0]);
                CollectionSpec collection=presentation.Collections.FirstOrDefault(x=>x.Id.Equals(collectionId,StringComparison.OrdinalIgnoreCase));
                GroupSpec group=collection==null?null:presentation.Groups.FirstOrDefault(x=>x.Id.Equals(collection.ResourceGroup,StringComparison.OrdinalIgnoreCase));
                TabSpec tab=group==null?null:presentation.Tabs.FirstOrDefault(x=>x.Id.Equals(group.Tab,StringComparison.OrdinalIgnoreCase));
                if(group!=null&&tab!=null)return language.Format(rule.TranslationKey,tab.Label,group.Label);
                return language.T("error_positive_collection_required_fallback");
            }
            return language.Format(rule.TranslationKey,rule.TranslationArguments);
        }
        void ShowError(Exception e) { string detail=ErrorText(e);Report(detail); if(cliMode){cliError=detail;return;} MessageWindow.Show(this,language,Text,language.T("failed")+"\n\n"+language.T("technical")+":\n"+detail,MessageWindow.Kind.Error,DialogResult.OK); }
        public void Report(string text) { journal.AppendLine(DateTime.Now.ToString("HH:mm:ss")+"  "+language.Localize(text)); }
        public void InitializeCatalog() { initialized=true; Scan(false); BuildingPickerWindow.Prime(state.Build,state.WorkshopRoot); }
        void Scan(bool ask)
        {
            if(ask && !ResolvePending()) return;
            state.WorkshopRoot=Catalog.NormalizeRoot(state.WorkshopRoot); Catalog.SchemaRoot=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"settings_schemas"); var notes=new List<string>(); entries.Clear(); entries.AddRange(Catalog.Scan(state.WorkshopRoot,notes));entries.AddRange(Catalog.ScanLocalEditors(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"settings_schemas"),notes));entries.AddRange(Catalog.ScanInstalled(state.Build,entries,notes));Catalog.ResolveAll(entries);entries.Sort((a,b)=>String.Compare(a.Name,b.Name,StringComparison.CurrentCultureIgnoreCase));
            foreach(string note in notes) Report(note);
            RefreshActivity();
            var chosen=Catalog.RestoreSelection(entries,state.SelectedId,state.SelectedSource); FilterList(); ShowEntry(chosen); FilterList();InspectStartupConsistency();
        }
        void RefreshActivity()
        {
            mods.ActiveStates.Clear(); mods.BlockedRoots.Clear();
            foreach(var entry in entries){mods.ActiveStates[entry.Root]=RuntimeStatus.ConfiguredActive(entry,state.Build);if(RuntimeStatus.BlockedBySml(entry,state.Build))mods.BlockedRoots.Add(entry.Root);}
            mods.UpdateBadge=language.T("update_badge"); pendingUpdates=Catalog.MarkUpdates(entries,state.Build); mods.Invalidate();
        }
        List<string> pendingUpdates=new List<string>();
        // 0.4.88: besides the search a state filter - with twenty entries "show me the problems"
        // beats typing. 0.4.90: only entries that really match are listed. An entry with unsaved
        // changes stays under every filter, because the amber dot in front of its name says why it
        // is there; the shown entry gets no free pass any more - a clean plugin sitting in the
        // "problems" list with nothing to show for it reads like a broken filter.
        internal string ListFilter="all";
        bool Passes(CatalogEntry entry)
        {
            if(mods.DirtyRoots.Contains(entry.Root))return true;
            switch(ListFilter)
            {
                case "active": return RuntimeStatus.ConfiguredActive(entry,state.Build);
                case "problems": return entry.Problem.Length>0||entry.Dependencies.Any(d=>!d.Found||!d.VersionOk);
                case "updates": return entry.Updated;
                default: return true;
            }
        }
        void FilterList()
        {
            selecting=true; mods.BeginUpdate();
            try { mods.Items.Clear(); foreach(var e in entries.Where(e=> (e.Name+" "+e.Id).IndexOf(search.Text,StringComparison.CurrentCultureIgnoreCase)>=0&&Passes(e))) mods.Items.Add(e); mods.SelectedItem=current; }
            finally { mods.EndUpdate(); selecting=false; }
        }
        internal void TestFilter(string filter){SetFilter(filter);}
        readonly Dictionary<string,Label> filterChips=new Dictionary<string,Label>(StringComparer.OrdinalIgnoreCase);
        void SetFilter(string filter)
        {
            ListFilter=filter;
            foreach(var pair in filterChips)
            {
                bool on=pair.Key.Equals(filter,StringComparison.OrdinalIgnoreCase);
                pair.Value.BackColor=on?Theme.Blue:Color.FromArgb(24,52,84);
                pair.Value.ForeColor=on?Color.White:Color.FromArgb(185,204,226);
            }
            FilterList();
        }
        // The four chips under the search box.
        Control FilterChips()
        {
            var row=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=false,FlowDirection=FlowDirection.LeftToRight,Margin=Padding.Empty,BackColor=Theme.Navy,Padding=Padding.Empty};
            foreach(string key in new[]{"all","active","problems","updates"})
            {
                string captured=key;
                var chip=new Label{Text=language.T("filter_"+key),AutoSize=false,Size=new Size(66,22),TextAlign=ContentAlignment.MiddleCenter,Margin=new Padding(0,10,5,0),Cursor=Cursors.Hand,Font=new Font("Segoe UI",8.5f)};
                chip.AccessibleName="filter:"+key;
                chip.Click+=(s,e)=>SetFilter(captured);
                filterChips[key]=chip; row.Controls.Add(chip);
            }
            SetFilter("all");
            return row;
        }
        void SelectionChanged()
        { var candidate=mods.SelectedItem as CatalogEntry; if(candidate==null || candidate==current) return; ShowEntry(candidate); }
        // The shown entry keeps its editor in memory while another entry is edited (0.4.71); it comes
        // back untouched when the entry is shown again and is written by the next save.
        void Park()
        {
            if(current==null||(session==null&&resourceSession==null&&contentSession==null)||!HasPending) return;
            work.Entry=current; work.SelectedTab=state.SelectedTab; work.LanguageCode=language.Code; work.Valid=WorkspaceValid(work); parked[current.Root]=work;
        }
        bool WorkspaceValid(Workspace candidate)
        { Workspace keep=work; work=candidate; try { if(resourceSession!=null) resourceSession.ValidateAll(); else if(contentSession==null) Prospective(); return true; } catch(Exception) { return false; } finally { work=keep; } }
        void Resume(Workspace kept)
        {
            work=kept; state.SelectedTab=kept.SelectedTab;
            try
            {
                // Labels come from the presentation, which was built in the language of that moment; rebuilt
                // only after a language change, and never at the cost of the page: an invalid draft value
                // makes the rebuild throw, then the old labels stay and the editor shows the value in red.
                if(session!=null&&kept.LanguageCode!=language.Code) try { RebuildPresentation(); } catch(Exception) { }
                if(resourceSession!=null) { heading.Text=localSpec.LocalizedName(language); description.Text=localSpec.LocalizedDescription(language); BuildLocalResourceEditor(); }
                else if(contentSession!=null) { description.Text=language.T("content_package_short"); BuildContentPage(); ShowSwitch(true,language.T("content_switch_note")); switchLabel.Text=language.T("content_switch"); SyncActivation(); }
                else BuildEditor();
                SetActions(true); UpdateLoadPath();
            }
            catch(Exception e) { work=new Workspace{Entry=current}; string detail=ErrorText(e); ShowProblem(language.T("failed")+"\n\n"+detail); Report(detail); }
            SaveView(); UpdateStatus();
        }
        void ShowEntry(CatalogEntry entry)
        {
            bool same=current==null ? state.SelectedSource==(entry==null?"":entry.Root) : current.Root==(entry==null?"":entry.Root);
            if(!same) state.SelectedTab="";
            Park();
            current=entry; work=new Workspace{Entry=entry}; restoreButton.Visible=entry!=null&&entry.Installed&&entry.Supported&&entry.Problem.Length==0; ShowSwitch(false,""); switchLabel.Text=language.T("enabled"); SetActions(false);
            ShowPreview(entry==null||entry.Installed?null:entry.Root);
            Theme.DisposeChildren(tabStrip); Theme.DisposeChildren(content); setters.Clear(); origins.Clear(); resetButtons.Clear(); UpdateLoadPath();
            refreshing=true; activation.Checked=false; refreshing=false;
            if(entry==null) { heading.Text=language.T("no_mods"); description.Text=""; tips.SetToolTip(heading,""); ShowInfo(language.T("no_mods_help")); SaveView(); UpdateStatus(); return; }
            state.SelectedId=entry.Id; state.SelectedSource=entry.Root; heading.Text=entry.Name; description.Text=""; tips.SetToolTip(heading,language.T("source")+": "+entry.Root+"\n"+entry.Version);
            if(entry.Problem.Length>0 || !entry.Supported) { ShowProblem(language.T("unsupported")+"\n\n"+language.Localize(entry.Problem)); SaveView(); UpdateStatus(); return; }
            Workspace kept; if(parked.TryGetValue(entry.Root,out kept)) { parked.Remove(entry.Root); Resume(kept); return; }
            try
            {
                if(entry.LocalEditor)
                {
                    // 0.5.1: Soviet Mod Loader brings resources, deposits, needs and buildings
                    // along. Then there is no DLL of their own and their INI is generated output,
                    // so the page says who is in charge instead of failing on the missing files.
                    LocalEditorSpec hosted=LocalEditorSpec.Load(entry.Root);
                    // 0.5.3: with SML the editor works on its baseline. Only when that baseline does
                    // not exist yet - SML installed but never run - is there nothing to edit.
                    if(Sml.Hosts(state.Build,hosted.Plugin)&&Sml.Baseline(state.Build,hosted.Plugin,hosted.ConfigName)==null){heading.Text=hosted.LocalizedName(language);description.Text=hosted.LocalizedDescription(language);ShowSmlHosted(hosted);SaveView();UpdateStatus();return;}
                    localSpec=hosted;resourceSession=new LocalResourceSession(localSpec,state.Build);resourceSession.References=new ReferenceSets(state.Build,state.WorkshopRoot,language.Code);heading.Text=localSpec.LocalizedName(language);description.Text=localSpec.LocalizedDescription(language);BuildLocalResourceEditor();SetActions(true);UpdateLoadPath();foreach(string note in resourceSession.Notes)Report(note);if(resourceSession.DisappearedExternal.Count>0)ShowDisappeared(resourceSession.DisappearedExternal);SaveView();UpdateStatus();return;
                }
                var package=entry.Installed?InstalledPlugins.Load(state.Build,entry.Target,Catalog.SchemaRoot):Package.Load(entry.Root);
                Catalog.ResolveDependencies(package.Dependencies,entries);
                foreach(string hint in package.Hints) Report(entry.Name+": "+language.Localize(hint));
                foreach(Dependency d in package.Dependencies) Report(entry.Name+": "+language.Localize(d.Note));
                if(package.Kind=="content")
                {
                    // 0.4.80: no DLL, no editor - one switch that provides or removes the package's
                    // fragments and assets, and a page that says what it carries and where it goes.
                    contentSession=new ContentSession(package,state.Build); description.Text=language.T("content_package_short");
                    BuildContentPage(); ShowSwitch(!SmlOwnsContent(contentSession)||contentSession.Provided,language.T("content_switch_note")); switchLabel.Text=language.T("content_switch"); SyncActivation(); SetActions(true); UpdateLoadPath();
                    foreach(string note in contentSession.Notes) Report(entry.Name+": "+language.Localize(note));
                    SaveView(); UpdateStatus(); return;
                }
                session=new Session(package,state.Build); LoadDraft();
                if(package.EditorManaged)
                {
                    // The package ships a keyed editor schema: the master-detail editor owns
                    // the INI (its baseline is the package INI), Session handles DLL, loader
                    // entry, bridge list and local copy.
                    localSpec=LocalEditorSpec.Load(package.EditorSchema);resourceSession=new LocalResourceSession(localSpec,state.Build,package);resourceSession.References=new ReferenceSets(state.Build,state.WorkshopRoot,language.Code);
                    heading.Text=localSpec.LocalizedName(language);description.Text=localSpec.LocalizedDescription(language);
                    BuildLocalResourceEditor();SetActions(true);UpdateLoadPath();foreach(string note in session.Notes.Concat(resourceSession.Notes))Report(note);SaveView();UpdateStatus();return;
                }
                BuildEditor(); SetActions(true); UpdateLoadPath(); foreach(string note in session.Notes.Concat(presentation.Notes)) Report(note);
            }
            catch(Exception e) { work=new Workspace{Entry=entry}; string detail=ErrorText(e); ShowProblem(language.T("failed")+"\n\n"+detail); Report(detail); }
            SaveView(); UpdateStatus();
        }
        // The status bar: which loader brings the DLL into the game (bridge, SML or the classic
        // plugins folder), where the files live, and what the loader saw on its last run.
        void UpdateLoadPath()
        {
            statusBar.SuspendLayout(); Theme.DisposeChildren(statusFlow); statusRight.Text="";
            if(current==null||contentSession!=null||(session==null&&resourceSession==null)){statusBar.Visible=false;statusBar.ResumeLayout();return;}
            bool installed=session!=null?session.Package.Installed:current.Installed;
            // 0.5.3: a keyed editor working on SML's baseline is loaded by SML, not by TesmioLoader.
            bool smlBase=session==null&&resourceSession!=null&&resourceSession.SmlBaseline!=null;
            bool bridge=session!=null&&session.BridgeActive, sml=smlBase||session!=null&&session.SmlActive&&!installed, loader=!bridge&&!sml, local=session!=null&&session.PreferLocal;
            // 0.5.17: a plugin that SML pushes aside (Deposits Plus, Resources Plus, Needs Plus) is loaded
            // by nobody in any useful sense - it steps aside at startup. No load-path dot is lit then;
            // an amber "paused" dot says why, the same on a package and on a fork.
            string pausedKind=session!=null?(RuntimeStatus.Blocked(session.Package,state.Build)?session.Package.ReplacedBySml:""):(resourceSession!=null&&localSpec!=null&&RuntimeStatus.Blocked(localSpec,state.Build)?localSpec.ReplacedBySml:"");
            bool paused=pausedKind.Length>0, litBridge=bridge&&!paused, litSml=sml&&!paused, litLoader=loader&&!paused;
            LoadPathText(language.T("status_loadpath")+":",Theme.Muted,null);
            LoadPathDot(litBridge,language.T("status_bridge"),language.T(litBridge?"status_bridge_tip":"status_inactive_tip"));
            LoadPathDot(litSml,language.T("status_sml"),language.T(litSml?"status_sml_tip":"status_inactive_tip"));
            LoadPathDot(litLoader,language.T("status_loader"),language.T(litLoader?(installed?"status_loader_installed_tip":"status_loader_tip"):"status_inactive_tip"));
            if(paused)LoadPathDot(true,language.T("status_paused"),language.Format("status_paused_tip",language.T("content_kind_"+pausedKind)),Color.FromArgb(232,166,36),"paused");
            string filesKey=smlBase?"status_files_sml_base":local?"status_files_local":(bridge||sml)?"status_files_package":"status_files_plugins";
            string filesTip=smlBase?language.Format("status_files_sml_base_tip",Sml.Show(state.Build,resourceSession.SmlBaseline)):language.T(filesKey+"_tip");
            var gap=LoadPathText("",Theme.Muted,null); gap.Margin=new Padding(10,0,0,0);
            LoadPathText(language.T("status_files")+":",Theme.Muted,filesTip); var files=LoadPathText(language.T(filesKey),Theme.Blue,filesTip); files.Font=new Font("Segoe UI",9.5f,FontStyle.Bold);
            // 0.5.17: a local editor entry has no Target of its own - the plugin name comes from its
            // schema, otherwise "not loaded" stood there for every local editor whatever the log said.
            string target=session!=null?session.Package.Target:localSpec!=null?localSpec.Plugin:current.Target; DateTime? when=InstalledPlugins.LogTime(state.Build); string seen;
            if(when.HasValue&&!String.IsNullOrEmpty(target)&&InstalledPlugins.LoaderVersions(state.Build).TryGetValue(target,out seen)) statusRight.Text=language.Format("status_last_seen",when.Value.ToString("g"),seen);
            else if(when.HasValue&&!String.IsNullOrEmpty(target)&&InstalledPlugins.IdleAtLastStart(state.Build).Contains(target)) statusRight.Text=language.Format("status_last_seen_paused",when.Value.ToString("g"));
            else if(when.HasValue) statusRight.Text=language.Format("status_last_seen_not",when.Value.ToString("g"));
            tips.SetToolTip(statusRight,language.T("status_last_seen_tip"));
            statusBar.Visible=true; statusBar.ResumeLayout();
        }
        Label LoadPathText(string text,Color color,string tip)
        {var l=Theme.Label(text,9.5f,false);l.ForeColor=color;l.Margin=new Padding(0,0,5,0);if(tip!=null)tips.SetToolTip(l,tip);statusFlow.Controls.Add(l);return l;}
        void LoadPathDot(bool on,string text,string tip){LoadPathDot(on,text,tip,Color.FromArgb(45,202,83),on?"on":"off");}
        void LoadPathDot(bool on,string text,string tip,Color onColor,string state)
        {
            // A drawn 13 px dot, vertically centred on the text line, close to its caption (0.4.13).
            Color fill=on?onColor:Color.FromArgb(196,203,214);
            var dot=new Panel{Size=new Size(13,13),Margin=new Padding(14,4,4,0),BackColor=Color.Transparent,AccessibleName="status-dot:"+state};
            dot.Paint+=(s,e)=>{e.Graphics.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;using(var b=new SolidBrush(fill))e.Graphics.FillEllipse(b,0,0,12,12);};
            tips.SetToolTip(dot,tip);statusFlow.Controls.Add(dot);
            LoadPathText(text,on?Theme.Ink:Theme.Muted,tip);
        }
        Field PresentedField(string id)
        {return presentation==null?null:presentation.Fields.Select(x=>x.Field).FirstOrDefault(x=>x.Id==id);}
        string ComparableValue(string id,string value)
        {
            Field field=PresentedField(id);if(field==null)return value;
            if(field.Type=="decimal"||field.Type=="integer")value=NumberInput.Canonical(value);
            return field.Normalize(value);
        }
        bool SameDraftValue(string id,string left,string right)
        {try{return ComparableValue(id,left)==ComparableValue(id,right);}catch(FormatException){return left==right;}}
        bool IsPackageDefault(string id,string value)
        {try{return session.Package.IsDefault(id,ComparableValue(id,value));}catch(FormatException){return false;}}
        Dictionary<string,string> DraftDifferences()
        {
            var values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var pair in draft)
            {
                string value=ComparableValue(pair.Key,pair.Value);
                if(!session.Package.IsDefault(pair.Key,value))values[pair.Key]=value;
            }
            return values;
        }
        Ini DraftConfig() {return new Ini(session.Package.Defaults.Render(DraftDifferences()));}
        void EnsurePresentationValues(bool baselineToo)
        {
            foreach(var spec in presentation.Fields)
            {
                string id=spec.Field.Id;
                if(!draft.ContainsKey(id))draft[id]=spec.Field.DefaultValue;
                if(baselineToo&&!baseline.ContainsKey(id))baseline[id]=spec.Field.DefaultValue;
            }
        }
        void RebuildPresentation()
        {presentation=new Presentation(session.Package,language,DraftConfig(),registries);EnsurePresentationValues(false);}
        void LoadRegistries()
        {registries.Clear();foreach(CollectionSpec collection in session.Package.Collections)registries[collection.Id]=ResourceRegistry.Load(session.Build,collection);}
        void LoadDraft()
        {
            draft.Clear();baseline.Clear();resets.Clear();var config=new Ini(session.Effective());
            foreach(var pair in config.Values)
            {
                // A generic (INI-derived) schema shows the bare value; an inline comment
                // such as "; (stock 121)" stays in the file but not in the input box.
                string value=pair.Value;Field field=session.Package.Fields.FirstOrDefault(x=>x.Id==pair.Key);
                if(field!=null&&field.Lenient)value=GenericSchema.StripComment(value);
                draft[pair.Key]=value;baseline[pair.Key]=value;
            }
            LoadRegistries();presentation=new Presentation(session.Package,language,config,registries);EnsurePresentationValues(true);
        }
        // The window opens at 1600x1000 design units, or at the size it had when the view
        // was last saved (physical pixels, so it is applied after DPI scaling, in Load).
        // Both are clamped to the working area of the screen the window sits on.
        void RestoreWindowSize()
        {
            var area=Screen.FromControl(this).WorkingArea;
            // A minimum wider than the screen (1080p at 125 %, or 150 %) would leave a window nobody can
            // shrink; below the full minimum the detail panels fall back to a single column (0.4.36).
            MinimumSize=new Size(Math.Min(MinimumSize.Width,area.Width),Math.Min(MinimumSize.Height,area.Height));
            int w=state.WindowWidth>0?state.WindowWidth:Width, h=state.WindowHeight>0?state.WindowHeight:Height;
            Size=new Size(Math.Max(MinimumSize.Width,Math.Min(w,area.Width-24)),Math.Max(MinimumSize.Height,Math.Min(h,area.Height-24)));
            if(StartPosition==FormStartPosition.CenterScreen) Location=new Point(area.Left+(area.Width-Width)/2,area.Top+(area.Height-Height)/2);
            if(state.WindowMaximized) WindowState=FormWindowState.Maximized;
        }
        void RememberWindowSize()
        {
            if(!IsHandleCreated || WindowState==FormWindowState.Minimized) return;
            var size=WindowState==FormWindowState.Normal?Size:RestoreBounds.Size;
            state.WindowWidth=size.Width; state.WindowHeight=size.Height; state.WindowMaximized=WindowState==FormWindowState.Maximized;
        }
        void SaveView()
        { if(!persistUi || stateStore==null || !initialized) return; try { RememberWindowSize(); stateStore.Save(state); } catch(Exception e) { if(!viewWarning) { viewWarning=true; Report(language.T("view_warning")+" "+e.Message); } } }
        void ChangeLanguage(string code)
        {
            state.Language=code; language=new Language(code);
            TranslateShell();
            // A package-backed editor has both sessions; the editor decides the layout.
            if(resourceSession!=null){if(session!=null)RebuildPresentation();heading.Text=localSpec.LocalizedName(language);description.Text=localSpec.LocalizedDescription(language);BuildLocalResourceEditor();}
            else if(session!=null) {LoadRegistries();RebuildPresentation();BuildEditor();} else ShowEntry(current);
            SaveView();
        }
        void ChooseLanguage()
        {
            using(var dialog=new Form {Text=language.T("language"),Font=Font,Size=new Size(420,390),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);
                var list=new ListBox {Dock=DockStyle.Fill,BorderStyle=BorderStyle.None,Font=new Font("Segoe UI",12),ItemHeight=36,IntegralHeight=false};
                foreach(var item in Language.Available()) list.Items.Add(item); list.DisplayMember="Value"; list.ValueMember="Key";
                for(int i=0;i<list.Items.Count;i++) if(((KeyValuePair<string,string>)list.Items[i]).Key==state.Language) list.SelectedIndex=i;
                if(list.SelectedIndex<0) list.SelectedIndex=0;
                var row=new FlowLayoutPanel {Dock=DockStyle.Bottom,Height=62,FlowDirection=FlowDirection.RightToLeft,Padding=new Padding(8)};
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);
                var apply=Theme.Button(language.T("apply"),()=>{dialog.DialogResult=DialogResult.OK;dialog.Close();},true);
                row.Controls.Add(apply); row.Controls.Add(cancel); dialog.AcceptButton=apply; dialog.CancelButton=cancel;
                list.DoubleClick+=(s,e)=>{if(list.SelectedItem!=null){dialog.DialogResult=DialogResult.OK;dialog.Close();}};
                dialog.Controls.Add(list); dialog.Controls.Add(row);
                if(Theme.Modal(this,dialog)==DialogResult.OK && list.SelectedItem!=null) ChangeLanguage(((KeyValuePair<string,string>)list.SelectedItem).Key);
            }
        }
        bool HasPending { get { return contentSession!=null?contentSession.NeedsWrite:resourceSession!=null?(resourceSession.Dirty||session!=null&&(session.LoaderChanged||session.LocalCopyChanged)):session!=null && (session.LoaderChanged || session.LocalCopyChanged || resets.Count>0 || draft.Count!=baseline.Count || draft.Any(p=>!baseline.ContainsKey(p.Key)||!SameDraftValue(p.Key,p.Value,baseline[p.Key]))); } }
        DialogResult Question(string text,bool allowDiscard)
        {
            // The RMM message window (0.4.38); Yes/No keep their meaning "save" / "discard".
            var buttons=allowDiscard
                ?new[]{new KeyValuePair<DialogResult,string>(DialogResult.Yes,language.T("yes")),new KeyValuePair<DialogResult,string>(DialogResult.No,language.T("no")),new KeyValuePair<DialogResult,string>(DialogResult.Cancel,language.T("cancel"))}
                :new[]{new KeyValuePair<DialogResult,string>(DialogResult.OK,language.T("confirm")),new KeyValuePair<DialogResult,string>(DialogResult.Cancel,language.T("cancel"))};
            return MessageWindow.Show(this,language,language.T("confirm"),text,MessageWindow.Kind.Question,buttons);
        }
        bool AnyPending { get { return parked.Count>0 || HasPending; } }
        // Every workspace with unsaved changes: parked ones first (by name), the shown one last.
        List<Workspace> Unsaved
        { get { var all=parked.Values.OrderBy(x=>x.Entry.Name,StringComparer.CurrentCultureIgnoreCase).ToList(); if(current!=null&&(session!=null||resourceSession!=null||contentSession!=null)&&HasPending) all.Add(work); return all; } }
        bool ResolvePending() { return ResolvePending(true); }
        bool ResolvePending(bool reload)
        {
            if(!AnyPending) return true;
            var names=Unsaved.Select(x=>x.Entry.Name).ToList();
            string text=names.Count==1?language.T("pending"):language.Format("pending_many",names.Count,String.Join(", ",names));
            DialogResult choice=PendingPrompt!=null?PendingPrompt():Question(text,true);
            if(choice==DialogResult.Cancel) return false;
            if(choice==DialogResult.Yes) return SaveAll();
            Discard(reload); return true;
        }
        // "Discard": parked workspaces are dropped; the shown entry is reloaded from disk unless the window is closing.
        void Discard(bool reload)
        {
            parked.Clear();
            if(current!=null&&HasPending) { work=new Workspace{Entry=current}; if(reload) ShowEntry(current); }
            if(reload) UpdateStatus();
        }
        Dictionary<string,string> Prospective()
        {
            if(session==null) throw new InvalidOperationException(language.T("unavailable"));
            var desired=new Dictionary<string,string>(draft,StringComparer.OrdinalIgnoreCase);
            foreach(var field in presentation.Fields.Select(x=>x.Field).Where(f=>f.Type!="readonly"))
            {
                if(resets.Contains(field.Id))desired[field.Id]=field.DefaultValue;
                else desired[field.Id]=field.Normalize(NumberInput.Canonical(desired[field.Id]));
            }
            var values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(var pair in desired)if(!session.Package.IsDefault(pair.Key,pair.Value))values[pair.Key]=pair.Value;
            ConfigRules.Validate(session.Package,new Ini(session.Package.Defaults.Render(values))); return values;
        }
        void Collect()
        { var values=Prospective(); session.Overrides.Clear(); foreach(var p in values) session.Overrides[p.Key]=p.Value; }
        // Asked only when saving would actually write a DLL into plugins\: never
        // for installed plugins, never under SML or the bridge (they load the
        // package copy), and never when the copy in plugins\ is already this one.
        bool NativeDllChanges()
        { return session!=null && !session.Package.Installed && !session.SmlActive && !session.BridgeActive && SafeFiles.HashFile(session.LocalDll)!=SafeFiles.Hash(session.Package.Dll); }
        bool ConfirmNativeTrust()
        {
            string text=language.T("trust")+"\n\n"+session.Package.Name+" "+session.Package.Version+"\n"+language.T("source")+": "+session.Package.Root+"\nSHA-256: "+SafeFiles.Hash(session.Package.Dll)+"\n\n"+language.T("target")+": "+session.Build+"\\plugins\n\n"+language.T("trust_question");
            return MessageWindow.Show(this,language,language.T("confirm"),text,MessageWindow.Kind.Warning,new[]{new KeyValuePair<DialogResult,string>(DialogResult.OK,language.T("confirm")),new KeyValuePair<DialogResult,string>(DialogResult.Cancel,language.T("cancel"))})==DialogResult.OK;
        }
        bool SaveAndDeploy(Action guard,Func<bool> approveNative) { return SaveAndDeploy(true,guard,approveNative); }
        bool SaveAndDeploy(bool rebuild,Action guard,Func<bool> approveNative)
        {
            Collect(); guard(); session.AssertUnchanged();
            bool approved=false;
            if(session.LocalCopyChanged)
            {
                string text=LocalCopyText();
                if((LocalCopyPrompt!=null?LocalCopyPrompt(text):Question(text,false))!=DialogResult.OK) return false;
                approved=session.PreferLocal;    // the file list named the DLL and its hash
            }
            if(NativeDllChanges() && !approved && !approveNative()) return false;
            string result=session.Commit(true,guard); lastAction="saved"; Report(SavedNote(result));
            if(!rebuild) return true;
            LoadDraft(); BuildEditor(); SaveView(); RefreshActivity(); return true;
        }
        string SavedNote(string backup) { return (work.Entry!=null?work.Entry.Name+": ":"")+language.T("saved")+" "+language.T("backup")+": "+backup; }
        // Writes the workspace behind `work`. guard = null takes the runtime checks (game closed,
        // files present); rebuild = refresh the page afterwards, only for the shown entry.
        bool SaveWorkspace(bool rebuild,Action guard,Func<bool> approveNative)
        {
            if(contentSession!=null)
            {
                // 0.4.80: fragments, assets and receipt in one transaction, then every target editor
                // writes its effective INI again (with or without this package's entries).
                Action contentGuard=guard??(()=>RuntimeGuard.NoGameRunning(state.Build));
                string written=contentSession.Commit(contentGuard,SessionFor); lastAction="saved"; Report(SavedNote(written));
                if(!rebuild) return true;
                BuildContentPage(); SyncActivation(); SaveView(); RefreshActivity(); return true;
            }
            if(resourceSession!=null)
            {
                FillTextPackKeys();
                Action resourceGuard=guard??(()=>LocalResourceGuard.Check(resourceSession));
                resourceGuard();
                if(session!=null)
                {
                    // Package-backed editor: DLL, loader entry, bridge list and local copy go
                    // first (Session never touches this INI), the editor's INI afterwards.
                    Action sessionGuard=guard??(()=>RuntimeGuard.Check(session));
                    sessionGuard();session.AssertUnchanged();bool approved=false;
                    if(session.LocalCopyChanged){string text=LocalCopyText();if((LocalCopyPrompt!=null?LocalCopyPrompt(text):Question(text,false))!=DialogResult.OK)return false;approved=session.PreferLocal;}
                    if(NativeDllChanges()&&!approved&&!approveNative())return false;
                    session.Commit(true,sessionGuard);
                }
                string result=resourceSession.Commit(resourceGuard);
                lastAction="saved";Report(SavedNote(result));
                if(!rebuild) return true;
                if(session!=null)session=new Session(session.Package,state.Build);
                BuildLocalResourceEditor();SaveView();RefreshActivity();return true;
            }
            return SaveAndDeploy(rebuild,guard??(()=>RuntimeGuard.Check(session)),approveNative);
        }
        bool SaveCurrent() { return SaveWorkspace(true,null,ConfirmNativeTrust); }
        // Parked entries are written one after another, by name (0.4.71). After each one the remaining
        // sessions take fresh hashes of the shared files (tesmioloader.ini, the bridge list), because RMM
        // itself just wrote them. A failure or a declined prompt shows the entry it happened in.
        bool SaveParked(Action guard,Func<bool> approveNative)
        {
            foreach(Workspace entry in parked.Values.OrderBy(x=>x.Entry.Name,StringComparer.CurrentCultureIgnoreCase).ToList())
            {
                Workspace keep=work; bool ok; work=entry;
                try { ok=SaveWorkspace(false,guard,approveNative); }
                catch(Exception) { work=keep; Jump(entry); throw; }
                work=keep;
                if(!ok) { Jump(entry); return false; }
                parked.Remove(entry.Entry.Root);
                foreach(Workspace other in parked.Values.Concat(new[]{work})) { if(other.Session!=null) other.Session.RehashShared(); if(other.ResourceSession!=null) other.ResourceSession.RehashShared(); }
            }
            return true;
        }
        void Jump(Workspace target)
        { CatalogEntry entry=entries.FirstOrDefault(x=>x.Root.Equals(target.Entry.Root,StringComparison.OrdinalIgnoreCase))??target.Entry; ShowEntry(entry); FilterList(); }
        // 0.4.80: the keyed editor of a target plugin for a content package - a local schema
        // (settings_schemas) or a Workshop package with its own editor schema (Deposits Plus).
        LocalResourceSession SessionFor(string plugin)
        {
            foreach(CatalogEntry e in entries)
            {
                try
                {
                    if(e.LocalEditor){LocalEditorSpec spec=LocalEditorSpec.Load(e.Root);if(spec.Plugin.Equals(plugin,StringComparison.OrdinalIgnoreCase))return new LocalResourceSession(spec,state.Build);continue;}
                    if(e.Kind!="plugin"||e.Installed||!e.Supported||e.Problem.Length>0||!e.Target.Equals(plugin,StringComparison.OrdinalIgnoreCase))continue;
                    Package package=Package.Load(e.Root);if(!package.EditorManaged)continue;
                    return new LocalResourceSession(LocalEditorSpec.Load(package.EditorSchema),state.Build,package);
                }
                catch(Exception e2){Report(e.Name+": "+ErrorText(e2));}
            }
            return null;
        }
        bool SaveAll() { return SaveAll(null,ConfirmNativeTrust); }
        bool SaveAll(Action guard,Func<bool> approveNative)
        {
            if(!SaveParked(guard,approveNative)) return false;
            bool updatePending=session!=null&&session.Update.Pending;
            if((session!=null||resourceSession!=null||contentSession!=null)&&(HasPending||updatePending)) return SaveWorkspace(true,guard,approveNative);
            RefreshActivity(); SaveView(); UpdateStatus(); return true;
        }
        void SaveAction() { SaveAll(); }
        void Launch()
        {
            // 0.4.71: every unsaved entry is written first; the launcher then starts from the loader folder.
            if(!SaveAll()) return;
            if(!ConfirmGameVersion()) return;
            if(resourceSession!=null) LocalResourceGuard.Check(resourceSession); else if(session!=null) RuntimeGuard.Check(session);
            EnsureGlobalResourceConsistency();
            if(!ConfirmMissingResources()) return;
            if(!ConfirmSteamSession()) return;
            string build=Path.GetFullPath(state.Build);
            Process.Start(new ProcessStartInfo(SafeFiles.Child(build,"tesmiolauncher.exe")){Arguments=LauncherOptions.Arguments,WorkingDirectory=build,UseShellExecute=true});Report(language.T("launched"));
            WatchLaunch();
        }
        // 0.4.92: RMM used to close the moment the launcher was started, so a game that quit
        // a second later left the player with nothing but a Steam error and no idea why. Now
        // the window stays for a few seconds and says what it saw. Nothing is blocked while it
        // waits: the game has the screen, this only watches. launch_watch_seconds = 0 in
        // rmm.ini gives back the old behaviour.
        System.Windows.Forms.Timer launchTimer; DateTime launchStart; LaunchWatch launchWatch;
        // 0.5.9: the game is up and nothing went wrong - normally that is where RMM leaves.
        // With "keep_open" it stays instead and only says so in the footer, so the window is
        // still there when the player alt-tabs back out of the game.
        void Done()
        {
            if(!LauncherOptions.KeepOpen){Close();return;}
            statusDetail.Text=language.T("launch_open"); tips.SetToolTip(statusDetail,statusDetail.Text);
        }
        void WatchLaunch()
        {
            if(LauncherOptions.WatchSeconds<=0){Done();return;}
            launchWatch=new LaunchWatch(LauncherOptions.WatchSeconds); launchStart=DateTime.UtcNow;
            statusDetail.Text=language.T(LauncherOptions.KeepOpen?"launch_watching_open":"launch_watching"); tips.SetToolTip(statusDetail,statusDetail.Text);
            launchTimer=new System.Windows.Forms.Timer{Interval=500};
            launchTimer.Tick+=(s,e)=>LaunchTick();
            launchTimer.Start();
        }
        void LaunchTick()
        {
            bool running; try{running=RuntimeGuard.GameRunning();}catch(Exception){running=true;}
            launchWatch.Tick(running,(DateTime.UtcNow-launchStart).TotalSeconds);
            if(!launchWatch.Finished) return;
            launchTimer.Stop(); launchTimer.Dispose(); launchTimer=null;
            if(!launchWatch.Died){Done();return;}
            Report(language.T("launch_died"));
            MessageWindow.Show(this,language,Text,language.T("launch_died")+"\n\n"+language.T("launch_died_hint"),MessageWindow.Kind.Warning,DialogResult.OK);
            UpdateStatus();
        }
        // The game is started outside Steam, so it needs the client's own record
        // of a logged-in user. Without it the game shows its own Steam error and
        // nothing says why; this asks first instead. 0.4.91: the warning names what
        // RMM actually found, so a false alarm can be told from a real logout.
        // 0.5.13: a building line that names a resource nothing publishes crashes the game while it
        // reads the building types. Named before the start, with the way out; Yes starts anyway.
        bool ConfirmMissingResources()
        {
            List<MissingResource> missing;
            try{missing=Startup.MissingResources(state.Build,Startup.Order(state.Build,entries));}catch(Exception){return true;}
            if(missing.Count==0)return true;
            string rows=String.Join("\n",missing.Take(8).Select(m=>language.Format("startcheck_missing_resource",m.Plugin,m.Section,m.Resource)));
            Report(language.T("launch_missing_resources")+" "+rows.Replace("\n","; "));
            return MessageWindow.Show(this,language,Text,
                language.T("launch_missing_resources")+"\n\n"+rows+"\n\n"+language.T("launch_missing_resources_hint"),
                MessageWindow.Kind.Warning,DialogResult.Yes,DialogResult.No)==DialogResult.Yes;
        }
        bool ConfirmSteamSession()
        {
            string reason; if(SteamSession.Check(out reason)!=SteamSession.State.LoggedOut) return true;
            string found=reason.Length>0?"\n\n"+language.T(reason):"";
            Report(language.T("steam_logged_out")+(reason.Length>0?" "+language.T(reason):""));
            return MessageWindow.Show(this,language,Text,
                language.T("steam_logged_out")+found+"\n\n"+language.T("steam_logged_out_hint"),
                MessageWindow.Kind.Warning,DialogResult.Yes,DialogResult.No)==DialogResult.Yes;
        }
        void InspectStartupConsistency()
        {
            var messages=new List<string>(); var startup=new List<string>();
            try
            {
                foreach(string duplicate in Catalog.DuplicateDlls(entries,state.Build)) startup.Add(language.Format("duplicate_dll",duplicate));
                // Updates are news, not problems: reported in the log and the status line, never as a blocking dialog.
                if(pendingUpdates.Count>0) Report(language.Format("updates_pending",String.Join(", ",pendingUpdates)));
                string version; bool unsupported=GameVersion.Warn(state.Build,out version); version=language.Localize(version); Report(language.Format("game_version",version));
                if(GameVersion.CheckEnabled&&unsupported) startup.Add(language.Format("game_version_unknown",version));
                foreach(var local in entries.Where(x=>x.LocalEditor))
                { try { var check=new LocalResourceSession(LocalEditorSpec.Load(local.Root),state.Build);if(check.DisappearedExternal.Count>0)messages.Add(local.Name+": "+language.Format("resource_external_disappeared",String.Join(", ",check.DisappearedExternal))); } catch(IOException) { } }
                messages.AddRange(ResourceConsistency.ValidateReferences(state.Build,entries).Select(language.Localize));
            }
            catch(Exception e){messages.Add(ErrorText(e));}
            if(messages.Count==0&&startup.Count==0){lastConsistencyAlert="";return;}
            var parts=new List<string>(); if(startup.Count>0) parts.Add(language.T("startup_warnings")+"\n\n"+String.Join("\n",startup.Distinct())); if(messages.Count>0) parts.Add(language.T("resource_consistency_failed")+"\n\n"+String.Join("\n",messages.Distinct()));
            string text=String.Join("\n\n",parts);foreach(string message in startup.Concat(messages))Report(message);
            if(persistUi&&text!=lastConsistencyAlert){lastConsistencyAlert=text;MessageWindow.Show(this,language,Text,text,MessageWindow.Kind.Warning,DialogResult.OK);}
        }
        void ShowDisappeared(IEnumerable<string> ids)
        {string text=language.Format("resource_external_disappeared",String.Join(", ",ids));Report(text);if(persistUi&&text!=lastConsistencyAlert){lastConsistencyAlert=text;MessageWindow.Show(this,language,Text,text,MessageWindow.Kind.Warning,DialogResult.OK);}}
        void EnsureGlobalResourceConsistency()
        {
            ISet<string> ids=null;if(resourceSession!=null)ids=new HashSet<string>(new LooseIni(resourceSession.Effective()).Entries(localSpec.ListSection).Select(x=>x.Key),StringComparer.OrdinalIgnoreCase);
            var issues=ResourceConsistency.ValidateReferences(state.Build,entries,ids);if(issues.Count>0)throw new IOException(language.T("resource_start_blocked")+"\n"+String.Join("\n",issues.Select(language.Localize)));
        }
        bool SaveAndLaunch(Action guard,Func<bool> approveNative,Action start)
        {
            if(!SaveParked(guard,approveNative))return false;
            if(!SaveAndDeploy(guard,approveNative))return false;
            if(!ConfirmGameVersion())return false;
            guard();start();Report(language.T("launched"));Done();return true;
        }
        void RestoreOriginal()
        {
            if(session==null||!session.Package.Installed||!session.Package.HasConfig)return;
            if(Question(language.T("restore_question"),false)!=DialogResult.OK)return;
            RuntimeGuard.Check(session);session.AssertUnchanged();
            string result=session.RestoreOriginal(()=>RuntimeGuard.Check(session));lastAction="saved";Report(language.T("restored")+" "+language.T("backup")+": "+result);
            LoadDraft();BuildEditor();SaveView();RefreshActivity();
        }
        internal Func<DialogResult> VersionPrompt = null;
        bool ConfirmGameVersion()
        {
            string version; bool unsupported=GameVersion.Warn(state.Build,out version);
            if(!GameVersion.CheckEnabled||!unsupported) return true;
            Report(language.Format("game_version_unknown",version));
            // Non-interactive runs (tests, snapshots) never block on a dialog.
            if(VersionPrompt!=null) return VersionPrompt()==DialogResult.OK;
            if(!persistUi) return true;
            return Question(language.Format("game_version_confirm",version),false)==DialogResult.OK;
        }
        void ResetAll()
        {
            if(resourceSession!=null){if(Question(language.T(localSpec.IsList?"list_reset_question":"resource_reset_question"),false)!=DialogResult.OK)return;resourceSession.Reset();selectedLocalResource="";BuildLocalResourceEditor();return;}
            if(session==null||Question(language.T("reset_question"),false)!=DialogResult.OK)return;
            draft.Clear();foreach(var pair in session.Package.Defaults.Values)draft[pair.Key]=pair.Value;
            resets.Clear();foreach(var f in session.Package.Fields.Where(f=>f.Type!="readonly"))resets.Add(f.Id);
            RebuildPresentation();BuildEditor();
        }
        void ChooseFolders()
        {
            using(var form=new Form { Text=language.T("folders"), Font=Font, Size=new Size(820,260), StartPosition=FormStartPosition.CenterParent, MinimizeBox=false, MaximizeBox=false, FormBorderStyle=FormBorderStyle.FixedDialog, ShowInTaskbar=false, Icon=Icon })
            {
                var table=new TableLayoutPanel { Dock=DockStyle.Fill, ColumnCount=3, RowCount=3, Padding=new Padding(18) }; table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,160)); table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,105)); form.Controls.Add(table);
                var paths=new[] {new TextBox {Text=state.Build,Dock=DockStyle.Fill},new TextBox {Text=state.WorkshopRoot,Dock=DockStyle.Fill}};
                for(int i=0;i<2;i++) { int index=i; table.RowStyles.Add(new RowStyle(SizeType.Absolute,48)); table.Controls.Add(Theme.Label(language.T(i==0?"build":"workshop"),10,false),0,i); table.Controls.Add(paths[i],1,i); table.Controls.Add(Theme.Button(language.T("browse"),()=>{using(var browse=new FolderBrowserDialog {SelectedPath=paths[index].Text,ShowNewFolderButton=false}) if(browse.ShowDialog(form)==DialogResult.OK) paths[index].Text=browse.SelectedPath;},false),2,i); }
                table.Controls.Add(Theme.Button(language.T("apply"),()=>Run(()=>{ string b=Path.GetFullPath(paths[0].Text), w=Catalog.NormalizeRoot(paths[1].Text); if(!ResolvePending()) return; state.Build=b; state.WorkshopRoot=w; form.DialogResult=DialogResult.OK; }),true),2,2);
                if(Theme.Modal(this,form)==DialogResult.OK) Scan(false);
            }
        }
        void ShowLog()
        { using(var form=new LogWindow(language,state.Build,()=>journal.ToString(),Font,Icon)) Theme.Modal(this,form); }
        // 0.4.88: one page for "what happens at the next start" - what loads, in which order, and
        // everything that would spoil it. A click on a problem lands on the entry that has it.
        void ShowStartCheck()
        {
            DateTime? when=InstalledPlugins.LogTime(state.Build);
            string seen=when.HasValue?language.Format("startcheck_last_start",when.Value.ToString("g")):"";
            using(var form=new StartCheckWindow(language,state.Build,entries,Saves(),seen,Font,Icon))
            {
                Theme.Modal(this,form);
                if(form.Jump.Length==0)return;
                CatalogEntry target=entries.FirstOrDefault(e=>e.Root.Equals(form.Jump,StringComparison.OrdinalIgnoreCase));
                if(target!=null){ShowEntry(target);FilterList();}
            }
        }
        // The saves that need what the shown entry provides: a plugin by its target name, a content
        // package by the ids its receipt says it added.
        internal List<SaveGame> SavesUsingCurrent()
        {
            var found=new List<SaveGame>();
            if(current==null)return found;
            if(contentSession!=null)
            {
                foreach(var pair in contentSession.Added)
                {
                    string kind=pair.Key.Equals("deposits",StringComparison.OrdinalIgnoreCase)?"deposits":pair.Key.Equals("resources",StringComparison.OrdinalIgnoreCase)?"resources":"";
                    if(kind.Length==0)continue;
                    foreach(SaveGame save in SaveGames.Using(Saves(),kind,pair.Value))if(!found.Contains(save))found.Add(save);
                }
                return found;
            }
            string target=session!=null?session.Package.Target:current.Target;
            if(target.Length==0)return found;
            return SaveGames.Using(Saves(),"plugins",new[]{target});
        }
        // Answered with Yes = switch off anyway. The test hook replaces the dialog.
        internal Func<string,DialogResult> SaveOffPrompt=null;
        bool SaveGamesAllowOff()
        {
            var using_=SavesUsingCurrent();
            if(using_.Count==0)return true;
            string text=language.Format("saves_off_question",current.Name,SaveGames.Names(using_,3,language.T("saves_and_more")));
            DialogResult answer=SaveOffPrompt!=null?SaveOffPrompt(text)
                :MessageWindow.Show(this,language,language.T("app"),text,MessageWindow.Kind.Warning,DialogResult.Yes,DialogResult.No);
            return answer==DialogResult.Yes;
        }
        // Snapshot and test hook: the loss list of a full reset, planned from the real folder.
        // Planning reads only - nothing is written until somebody clicks through both dialogs.
        internal Form TestResetWindow()
        {
            return new ResetConfirmWindow(language,ResetTool.Plan(state.Build,entries,SaveGamesQuiet(),true),Font,Icon);
        }
        // Snapshot and test hook: the start check as a window, without showing it.
        internal Form TestStartCheckWindow()
        {
            DateTime? when=InstalledPlugins.LogTime(state.Build);
            return new StartCheckWindow(language,state.Build,entries,Saves(),when.HasValue?language.Format("startcheck_last_start",when.Value.ToString("g")):"",Font,Icon);
        }
        // The saved games of this game folder, read once per session: which of them still needs a
        // plugin or a resource decides how loud a warning has to be before something is switched off.
        List<SaveGame> savedGames;
        internal List<SaveGame> Saves()
        {
            if(savedGames==null)try{savedGames=SaveGames.Scan(state.Build);}catch(Exception){savedGames=new List<SaveGame>();}
            return savedGames;
        }
        // Profiles and restore points write through the same transaction as a save;
        // pending edits are settled first and the list is rescanned afterwards.
        // 0.4.93: the settings of RMM itself. Changes take effect while the window is open;
        // the saved view is written when it closes, and a new language rebuilds the page.
        void ShowOptions()
        {
            if(!ResolvePending()) return;
            string before=state.Language;
            using(var form=new OptionsWindow(language,state,Diagnostics,Font,Icon))
            {
                // The reset knows nothing about files: the main window hands it the plan and the
                // execution, so the window stays a window (and the tests can answer its dialogs).
                form.Planner=everything=>ResetTool.Plan(state.Build,entries,SaveGamesQuiet(),everything);
                form.Applier=plan=>
                {
                    string report=ResetTool.Apply(state.Build,plan,()=>RuntimeGuard.NoGameRunning(state.Build));
                    if(plan.View&&stateStore!=null)
                    {
                        // The saved view goes too - and RMM must stop writing it back, otherwise
                        // closing the window would recreate what was just removed.
                        try{ if(File.Exists(stateStore.PathName)) File.Delete(stateStore.PathName); }catch(Exception){}
                        persistUi=false;
                    }
                    return report;
                };
                form.Journal=Report;
                Theme.Modal(this,form);
                // 0.5.20: the loader's log settings are the only thing this window writes to a
                // file of the game; it happens here so a failure lands in the usual error box.
                try{ form.SaveLoaderLog(); }catch(Exception e){ ShowError(e); }
                if(form.Changed&&!form.ResetDone) SaveView();
                if(form.LanguageCode!=before) ChangeLanguage(form.LanguageCode);
                // 0.4.99: a reset rewrites the very files the open sessions were built from, so
                // every workspace - shown and parked - is stale afterwards. Keeping them meant
                // "save" failed with "file changed in the meantime". Everything is dropped and
                // read again from disk.
                if(form.ResetDone) { parked.Clear(); work=new Workspace{Entry=current}; Scan(false); }
            }
        }
        // The saved games, or an empty list when the game folder cannot be read.
        List<SaveGame> SaveGamesQuiet()
        {
            try { return SaveGames.Scan(state.Build); } catch(Exception) { return new List<SaveGame>(); }
        }
        // Everything a bug report needs, in one block: versions, folders, how many entries
        // were found and what Steam says right now.
        string Diagnostics()
        {
            var text=new StringBuilder();
            text.AppendLine("Republic Mod Manager "+Application.ProductVersion);
            text.AppendLine("Windows "+Environment.OSVersion.Version+", .NET "+Environment.Version);
            text.AppendLine("Loader: "+state.Build);
            text.AppendLine("Workshop: "+state.WorkshopRoot);
            try { text.AppendLine("Loader file: "+SafeFiles.HashFile(SafeFiles.Child(state.Build,"tesmioloader.dll")).Substring(0,16)); } catch(Exception){}
            try { bool known; text.AppendLine("Game: "+GameVersion.Describe(state.Build,out known)+(known?"":" (unknown build)")); } catch(Exception){}
            text.AppendLine("Entries: "+entries.Count+", shown: "+(current==null?"-":current.Name+" "+current.Version));
            string reason; SteamSession.State steam=SteamSession.Check(out reason);
            text.AppendLine("Steam: "+steam+(reason.Length>0?" ("+reason+")":"")+"  ["+SteamSession.Evidence()+"]");
            foreach(CatalogEntry entry in entries.Where(e=>e.Problem.Length>0)) text.AppendLine("Problem: "+entry.Name+" - "+entry.Problem);
            return text.ToString();
        }
        void ShowProfiles()
        {
            if(!ResolvePending()) return;
            using(var form=new ProfilesWindow(language,state.Build,Font,Icon)) { Theme.Modal(this,form); foreach(string note in form.Journal) Report(note); if(form.Changed) Scan(false); }
        }
        protected override void Dispose(bool disposing) { if(disposing) { tips.Dispose(); icons.Dispose(); if(Icon!=null) Icon.Dispose(); } base.Dispose(disposing); }
        internal int ModCount {get{return mods.Items.Count;}}
        internal string SelectedSource {get{return current==null?"":current.Root;}}
        internal string DisplayedValue(string id) {string value;return draft.TryGetValue(id,out value)?value:null;}
        internal void SelectIndex(int index) {mods.SelectedIndex=index;}
        internal bool HasEditor {get{return contentSession!=null||resourceSession!=null||session!=null && session.Package.Visible;}}
        internal bool ContentProvided {get{return contentSession!=null&&contentSession.Provided;}}
        internal int TabCount {get{return presentation!=null?presentation.Tabs.Count:localSpec!=null?localSpec.Tabs.Count:0;}}
        internal string SelectedTabId {get{return state.SelectedTab;}}
        internal int ActionCount {get{return actions.Count;}}
        internal string LanguageGlyph {get{return languageButton.Glyph;}}
        internal bool Activity(string root) {bool value;return mods.ActiveStates.TryGetValue(root,out value)&&value;}
        internal string SelectedTab {get{return state.SelectedTab;}}
        internal bool IsDirty {get{return HasPending;}}
        internal int ResourceChoiceCount {get{return registries.Values.Sum(x=>x.Options.Count);}}
        internal int ResourceUsedCount
        {get{if(session==null)return 0;Ini config=DraftConfig();int total=0;foreach(CollectionSpec c in session.Package.Collections){ResourceRegistry registry;if(registries.TryGetValue(c.Id,out registry))total+=registry.Options.Count(x=>CollectionRules.Names(c,config).Any(n=>n.Equals(x.Id,StringComparison.OrdinalIgnoreCase)));}return total;}}
        internal void TestEdit(string id,string value) {Edit(id,value);}
        internal void TestAddResource(string id,string first,string second,string third,string fourth)
        {var c=session.Package.Collections.First();string[] values={first,second,third,fourth};AddResource(c,id,c.TargetSections.Select((x,i)=>new {x,i}).ToDictionary(x=>x.x,x=>values[x.i],StringComparer.OrdinalIgnoreCase));}
        internal bool TestRemoveResource(string id) {return RequestRemoveResource(session.Package.Collections.First().Id,id);}
        internal Form TestResourceDialog() {return BuildResourceDialog(session.Package.Collections.First());}
        internal void TestResetField(string id) {ResetField(id);}
        internal void TestLanguage(string code) {ChangeLanguage(code);}
        internal void TestSearch(string query) {search.Text=query;}
        internal string StatusText {get{return status.Text;}}
        internal bool StatusValid {get{return status.Text==language.T("valid")||status.Text==language.T("unsaved");}}
        internal bool SaveEnabled {get{return saveButton!=null&&saveButton.Enabled;}}
        internal bool StartEnabled {get{return startButton!=null&&startButton.Enabled;}}
        internal int DirtyCount {get{return Unsaved.Count;}}
        internal int DirtyMarks {get{return mods.DirtyRoots.Count;}}
        internal bool ShownListed {get{return mods.Items.Cast<CatalogEntry>().Any(x=>x==current);}}
        internal bool TestSaveAll(Action guard) {return SaveAll(guard,()=>true);}
        // 0.4.81: --save. The Save button's own path with the real runtime checks; every question
        // the button would ask is declined, because nobody is there to answer it. Returns one line.
        // 0.4.82: --activate. Moves the header switch exactly like a click does, so that a
        // following --save writes what the Save button would write. Returns null when the
        // switch stands where it should, otherwise a FAIL line.
        internal string CliActivate(bool on)
        {
            if(current==null) return "FAIL no entry selected (use --package or --workshop)";
            if(!activation.Enabled) return "FAIL "+current.Name+": this entry has no switch";
            if(activation.Checked==on) return null;
            cliMode=true; cliError="";
            try { activation.Checked=on; }
            finally { cliMode=false; }
            if(cliError.Length>0) return "FAIL "+current.Name+": "+Msg.Plain(cliError);
            if(activation.Checked!=on) return "FAIL "+current.Name+": the switch did not move";
            return null;
        }
        internal string CliSave() { return CliSave(null); }
        // guard = null takes the real runtime checks (game closed, loader files present); the UI
        // tests hand in their own, because their fixture is a folder and not a game.
        internal string CliSave(Action guard)
        {

            if(current==null) return "FAIL no entry selected (use --package or --workshop)";

            if(!HasEditor) return "FAIL "+current.Name+": nothing to save here";
            bool updatePending=session!=null&&session.Update.Pending;

            if(!HasPending&&!updatePending) return "PASS "+current.Name+": nothing to save, already up to date";
            if(NativeDllChanges()) return "FAIL "+current.Name+": this save would copy a DLL into plugins\\ - do that in the window";

            LocalCopyPrompt=text=>DialogResult.Cancel;
            try
            {
                if(!SaveAll(guard,()=>false)) return "FAIL "+current.Name+": save declined (a confirmation was needed)";

                return "PASS "+current.Name+" saved.";
            }
            catch(Exception e) { return "FAIL "+current.Name+": "+Msg.Plain(ErrorText(e)); }
            finally { LocalCopyPrompt=null; }
        }
        internal bool TestResolvePending() {return ResolvePending();}
        internal string HeadingText {get{return heading.Text;}}
        internal string StatusDetailText {get{return statusDetail.Text;}}
        internal void TestCommit(Action guard) {Collect();session.Commit(false,guard);LoadDraft();BuildEditor();}
        internal bool TestApply(Action guard) {return SaveAndDeploy(guard,()=>true);}
        internal bool TestSaveAndLaunch(Action guard,Action start) {return SaveAndLaunch(guard,()=>true,start);}
        internal bool HasLocalResourceEditor {get{return resourceSession!=null;}}
        internal int LocalResourceCount {get{return resourceSession==null?0:resourceSession.Items().Count;}}
        internal void TestAddLocalResource(string id,string template,string display,string transport){resourceSession.Add(id,template,display,transport);selectedLocalResource=id;BuildLocalResourceEditor();}
        internal void TestAddListItem(string id,string raw){resourceSession.AddRaw(id,raw);selectedLocalResource=id;BuildLocalResourceEditor();}
        internal void TestAddSection(string name,Dictionary<string,string> values){resourceSession.AddSection(name,values);selectedLocalResource=name;BuildLocalResourceEditor();}
        internal string LocalNextValue(string field){return resourceSession.NextValue(localSpec.Fields.First(x=>x.Id==field));}
        internal void TestSetLocalField(string id,string field,string value){resourceSession.SetField(id,localSpec.Fields.First(x=>x.Id==field),value);UpdateStatus();}
        internal bool TestLocalOwned(string id){return resourceSession.Items().First(x=>x.Id==id).Owned;}
        internal bool TestSaveLocal(Action guard){string result=resourceSession.Commit(guard);BuildLocalResourceEditor();RefreshActivity();return result.Length>0;}
        internal string LocalEffective(){return resourceSession.Effective();}
        internal void TestRemoveLocalResource(string id){if(localSpec.HidesOriginals)RemoveLocalListItem(id,resourceSession.Items().First(x=>x.Id==id).Owned);else RemoveLocalResource(id);}
        internal void TestSetGlobal(string field,string value){resourceSession.SetGlobal(localSpec.Fields.First(x=>x.Id==field),value);UpdateStatus();}
        internal void TestSetListColumn(string id,int column,string value){string[] parts=localSpec.TupleColumns(resourceSession.Items().First(x=>x.Id==id).ListValue);parts[column]=value;resourceSession.SetListRaw(id,ListTuple.Render(parts));BuildLocalResourceEditor();}
        internal List<string> LocalHidden(){return resourceSession.SuppressedIds();}
    }
}

