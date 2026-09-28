using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    public sealed partial class MainForm
    {
        sealed class CardLayout { public int MinimumWidth; public TableLayoutPanel Inner; public Control Tall; public readonly List<Label> Wrap = new List<Label>(); public readonly List<Label> NoticeWrap = new List<Label>(); public readonly List<Label> FieldWrap = new List<Label>(); }
        void BuildEditor()
        {
            if(session==null || presentation==null) return;
            // 0.4.50: no painting while the tab strip and the cards are replaced (Theme.SetRedraw).
            Theme.SetRedraw(tabStrip,false);Theme.SetRedraw(content,false);
            try{BuildEditorCore();}
            finally{Theme.SetRedraw(tabStrip,true);Theme.SetRedraw(content,true);}
        }
        void BuildEditorCore()
        {
            description.Text=presentation.Description;
            Theme.DisposeChildren(tabStrip); refreshing=true;
            string note=session.SwitchMode=="loader"?language.T(session.BridgeActive?"switch_note_bridge":"switch_note_loader"):session.SwitchMode=="ini"?language.T("switch_note_ini"):language.T("switch_note_sml_only");
            ShowSwitch(session.SwitchMode!="none" && session.Package.Visible,session.Package.Visible?note:""); header.PerformLayout();
            refreshing=false; SyncActivation();
            if(!session.Package.Visible) { Theme.DisposeChildren(content); ShowInfo(language.T("hidden")); UpdateStatus(); return; }
            if(!presentation.Tabs.Any(t=>t.Id==state.SelectedTab)) state.SelectedTab=presentation.Tabs.Any(t=>t.Id==presentation.DefaultTab)?presentation.DefaultTab:presentation.Tabs.First().Id;
            foreach(var tab in presentation.Tabs)
            {
                string id=tab.Id; var button=Theme.TabButton(tab.Label,()=>Run(()=>SelectTab(id))); button.Tag=id;
                tabStrip.Controls.Add(button);
            }
            SelectTab(state.SelectedTab);
        }
        internal void SelectTab(string id)
        {
            if(resourceSession!=null && localSpec!=null && localSpec.Tabs.Any(t=>t.Id==id)) { state.SelectedTab=id; BuildLocalResourceEditor(); return; }
            Theme.SetRedraw(content,false);
            try{SelectTabCore(id);}
            finally{Theme.SetRedraw(content,true);}
        }
        void SelectTabCore(string id)
        {
            if(presentation==null || !presentation.Tabs.Any(t=>t.Id==id)) return;
            state.SelectedTab=id;
            foreach(Button button in tabStrip.Controls) { bool active=(string)button.Tag==id; button.ForeColor=active?Color.White:Theme.Ink; button.BackColor=active?Theme.SelectionBlue:Theme.Frame; button.FlatAppearance.MouseOverBackColor=active?Theme.SelectionBlue:Theme.TabHover; }
            tabStrip.PerformLayout(); foreach(Button button in tabStrip.Controls) if((string)button.Tag==id) tabStrip.ScrollControlIntoView(button); tabStrip.Invalidate();
            Theme.DisposeChildren(content); setters.Clear(); origins.Clear(); resetButtons.Clear();
            // 0.5.9: Soviet Mod Loader has this plugin's job, so nothing here reaches the game. The
            // warning stands on every tab and stays white; the rest of the page goes grey and dead
            // below it, and "Trotzdem bearbeiten" opens it for the day SML is switched off.
            Card pausedCard=null;
            if(RuntimeStatus.Blocked(session.Package,state.Build))
            {
                pausedCard=BeginCard(780);
                AddNotice(pausedCard,language.Format(pausedUnlocked?"sml_replaced":"sml_replaced_locked",session.Package.Name,language.T("content_kind_"+session.Package.ReplacedBySml)),"warning");
                if(!pausedUnlocked)
                {
                    var layout=(CardLayout)pausedCard.Tag;
                    var unlock=Theme.Button(language.T("sml_replaced_unlock"),()=>Run(()=>{pausedUnlocked=true;BuildEditor();}),false);
                    unlock.AccessibleName="sml:unlock";unlock.Margin=new Padding(0,2,8,4);
                    var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,0,0,4),Padding=Padding.Empty};
                    flow.Controls.Add(unlock);AddRow(layout.Inner,flow);
                }
            }
            // The notices card belongs to the first tab only ("Allgemein"); the other tabs start with their groups.
            if(presentation.Tabs.Count==0||id==presentation.Tabs[0].Id) AddNotices();
            if(presentation.Links.Count>0&&(presentation.LinksTab.Length==0?(presentation.Tabs.Count==0||id==presentation.Tabs[0].Id):presentation.LinksTab==id)) AddPresentationLinks();
            foreach(var group in presentation.Groups.Where(g=>g.Tab==id)) AddGroup(group);
            // 0.5.9: Soviet Mod Loader has this plugin's job, so nothing here reaches the game. The
            // same picture as in the keyed editors: the warning stays white, the rest goes grey and
            // dead, and "Trotzdem bearbeiten" opens it up for the day SML is switched off.
            if(pausedCard!=null&&!pausedUnlocked)
                foreach(Control card in content.Controls)
                {
                    if(ReferenceEquals(card,pausedCard))continue;
                    var box=card as Card;if(box!=null){box.Dimmed=true;box.BackColor=Theme.Dim;}
                    LockInputs(card);DimCard(card);
                }
            ResizeCards(); UpdateStatus(); SaveView();
        }
        Card BeginCard(int minimumWidth,string header="")
        {
            var card=new Card(header); var inner=new TableLayoutPanel { ColumnCount=1, AutoSize=true, AutoSizeMode=AutoSizeMode.GrowAndShrink, Dock=DockStyle.Top, Margin=Padding.Empty };
            inner.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            card.Controls.Add(inner); card.Tag=new CardLayout {MinimumWidth=minimumWidth,Inner=inner}; content.Controls.Add(card); return card;
        }
        static void AddRow(TableLayoutPanel table,Control control)
        { int row=table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.AutoSize)); control.Dock=DockStyle.Top; table.Controls.Add(control,0,row); }
        void AddText(Card card,string text,float size,bool bold,Color color)
        {
            if(String.IsNullOrEmpty(text)) return;
            var layout=(CardLayout)card.Tag; var label=Theme.Label(text,size,bold); label.ForeColor=color; label.Margin=new Padding(0,0,0,12); layout.Wrap.Add(label); AddRow(layout.Inner,label);
        }
        void AddNotice(Card card,string text,bool warning){AddNotice(card,text,warning?"warning":"info");}
        // 0.4.55: tone info | warning | error - the error box carries what used to be a red text line.
        void AddNotice(Card card,string text,string tone)
        {
            if(String.IsNullOrEmpty(text))return;
            var layout=(CardLayout)card.Tag;
            var notice=new NoticeBox(tone);
            notice.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,36));notice.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            var icon=new NoticeIcon {Warning=tone=="warning",Error=tone=="error",BackColor=notice.Tint};
            var label=Theme.Label(text,10,false);label.ForeColor=tone=="error"?Color.FromArgb(150,28,28):tone=="warning"?Color.FromArgb(139,81,0):Theme.Blue;label.Margin=new Padding(0,3,0,0);label.Dock=DockStyle.Top;label.BackColor=notice.Tint;
            notice.Controls.Add(icon,0,0);notice.Controls.Add(label,1,0);layout.NoticeWrap.Add(label);AddRow(layout.Inner,notice);
        }
        void ShowInfo(string message) { var card=BeginCard(0); AddText(card,message,11,false,Theme.Muted); ResizeCards(); }
        // 0.4.55: an entry that cannot be shown gets a red box instead of grey text.
        void ShowProblem(string message) { var card=BeginCard(0); AddNotice(card,message,"error"); ResizeCards(); }
        // 0.5.1: resources, deposits, needs and buildings are built into Soviet Mod Loader.
        // No DLL of their own, and their plugins\<name>.ini is generated at every start - so
        // this is a blue notice about who is in charge, not a red box about missing files.
        void ShowSmlHosted(LocalEditorSpec spec)
        {
            string name=spec.LocalizedName(language);
            var card=BeginCard(0,language.Format("sml_hosted",name));
            AddNotice(card,language.Format("sml_hosted_notice",name,spec.Plugin),"info");
            AddText(card,language.Format("sml_hosted_generated",spec.ConfigName),10,false,Theme.Ink);
            AddText(card,language.T("sml_hosted_wait"),10,true,Theme.Ink);
            AddText(card,language.T("sml_hosted_how"),10,false,Theme.Ink);
            AddText(card,language.Format("sml_hosted_back",spec.Plugin,spec.ConfigName),10,false,Theme.Muted);
            ResizeCards();
        }
        // 0.4.80: the page of a content package - its state, what it carries and which plugin takes each part.
        void BuildContentPage()
        {
            Theme.DisposeChildren(tabStrip); Theme.DisposeChildren(content); setters.Clear(); origins.Clear(); resetButtons.Clear();
            ContentSession c=contentSession; if(c==null) return;
            var notes=BeginCard(0,language.T("notices"));
            if(c.UpdatePending)
            {
                string text=language.Format("content_update",c.ProvidedVersion,c.Package.Version);
                string changes=Startup.Changes(c.Package.Root,c.Package.Version,language.Code);
                if(changes.Length>0)text+="  "+language.Format("update_changes",c.Package.Version,changes);
                AddNotice(notes,text,"info");
            }
            else
            {
                // 0.5.3: under SML the entries are in the game without a receipt of ours - say that
                // instead of "not provided yet", which the green dot would already contradict.
                bool byLoader=!c.Provided&&ContentSession.MergedByLoader(state.Build,c.Package);
                AddText(notes,c.Provided?language.Format("content_state_provided",c.ProvidedVersion):byLoader?language.T("content_state_sml"):language.T("content_state_absent"),10,false,c.Provided||byLoader?Color.FromArgb(26,132,61):Theme.Muted);
            }
            foreach(string note in c.Notes) AddNotice(notes,language.Localize(note),"warning");
            // 0.5.1: Soviet Mod Loader reads a package with content itself and merges it into its
            // own generated INIs. Providing it a second time from here would write into a file SML
            // rewrites at the next start, so the page says who is in charge and the switch stays away.
            if(SmlOwnsContent(c)) AddNotice(notes,language.Format("sml_content",c.Package.Name),"info");
            foreach(var pair in c.Skipped) if(pair.Value.Count>0) AddNotice(notes,language.Format("content_skipped",language.T("content_kind_"+pair.Key.ToLowerInvariant()),String.Join(", ",pair.Value)),"info");
            // 0.4.87: an id that already exists elsewhere blocks the package. Red, with the name
            // of the entry in the way and what to do about it - saving would fail with the same text.
            // 0.5.9: not while Soviet Mod Loader owns the content. It merges the package itself,
            // there is no switch to press here, and the entry it "clashes" with is usually the one
            // SML built from this very package - a red box would only make RMM look broken.
            if(!SmlOwnsContent(c))
                foreach(var clash in c.Conflicts(SessionFor))
                {
                    string plugin; c.Targets.TryGetValue(clash.Key,out plugin);
                    AddNotice(notes,language.Format("content_conflict",c.Package.Name,String.Join(", ",clash.Value),plugin??"",language.T("content_kind_"+clash.Key.ToLowerInvariant())),"error");
                }
            var contents=BeginCard(0,language.T("content_card_contents"));
            foreach(string key in ContentTargets.Keys)
            {
                string text; if(!c.Package.ContentFragments.TryGetValue(key,out text)) continue;
                var ids=ContentTargets.IdsOf(key,text); string plugin; c.Targets.TryGetValue(key,out plugin); string list=ids.Count>0?String.Join(", ",ids):"-";
                AddText(contents,!String.IsNullOrEmpty(plugin)?language.Format("content_target_line",language.T("content_kind_"+key),plugin,list):language.Format("content_target_none",language.T("content_kind_"+key),list),10,false,Theme.Ink);
            }
            if(c.Package.AssetFiles.Count>0) AddText(contents,language.Format("content_assets_count",c.Package.AssetFiles.Count,LocalEditorSpec.VfsRoot(state.Build)),10,false,Theme.Ink);
            // 0.4.88: saved games that already know entries of this package.
            var usedBy=SavesUsingCurrent();
            if(usedBy.Count>0) AddText(notes,language.Format("saves_used_by",usedBy.Count,SaveGames.Names(usedBy,3,language.T("saves_and_more"))),10,false,Theme.Muted);
            ResizeCards();
        }
        // Everything a user should know about this entry before touching it: unmet
        // dependencies and double loading in red, the rest as quiet notes.
        void AddNotices()
        {
            if(session==null) return;
            var problems=new List<string>(); var notes=new List<string>(); var warnings=new List<string>();
            // 0.5.3: the package says SML replaces it, and SML really has that capability. Amber,
            // because nothing is broken - the plugin simply waits, and the page says what to do
            // if the player wants this one instead.
            // 0.5.9: the "SML has this job" warning is no longer one line among the notices - it now
            // has its own card above every tab and greys the page out, so it must not stand twice.
            // 0.4.27: found is not enough - a dependency that is present but not switched on
            // gets a yellow box, an active one a quiet line; both name the package.
            foreach(Dependency d in session.Package.Dependencies)
            {
                if(!d.Found||!d.VersionOk){problems.Add(language.Localize(d.Note));continue;}
                string name=d.Name.Length>0?d.Name:d.Id;
                if(RuntimeGuard.Provisioned(session,d)) notes.Add(language.Format("dep_active",name,d.Version));
                else warnings.Add(language.Format("dep_inactive",name,d.Version,session.Package.Name));
            }
            if(session.SmlActive&&!session.Package.Installed&&File.Exists(session.LocalDll)) problems.Add(language.Format("duplicate_dll",session.Package.Name+" (plugins\\"+session.Package.Target+".dll)"));
            if(session.ForeignLocalDll) problems.Add(language.Format("duplicate_dll_bridge",session.Package.Name+" (plugins\\"+session.Package.Target+".dll)"));
            // 0.4.53: the bridge and this manager must walk the same folder, or the package list means nothing.
            if(Bridge.Present(state.Build)&&(session.Package.Target==Bridge.Name||session.BridgeActive&&!session.Package.Installed))
            {
                string bridgeRoot=Bridge.Root(state.Build),ownRoot=Catalog.NormalizeRoot(state.WorkshopRoot);
                if(bridgeRoot.Length>0&&ownRoot.Length>0&&!Catalog.NormalizeRoot(bridgeRoot).Equals(ownRoot,StringComparison.OrdinalIgnoreCase))problems.Add(language.Format("bridge_root_mismatch",bridgeRoot,ownRoot));
            }
            // The update notice gets its own blue box above everything else; it is
            // gone after the next save, when the receipt matches the package again.
            string update=null;
            if(session.Update.Pending) update=language.Format("update_available",session.Update.PreviousVersion.Length>0?session.Update.PreviousVersion:"?",session.Update.CurrentVersion,(session.Update.DllChanged?language.T("update_dll"):"")+(session.Update.DllChanged&&session.Update.DefaultsChanged?", ":"")+(session.Update.DefaultsChanged?language.T("update_ini"):""));
            // 0.4.88: what the update actually brings, out of the package README's version history -
            // the yellow mark alone never said it.
            if(update!=null)
            {
                string changes=Startup.Changes(session.Package.Root,session.Update.CurrentVersion,language.Code);
                if(changes.Length>0) update+="  "+language.Format("update_changes",session.Update.CurrentVersion,changes);
            }
            // Load path, file location and the last game start moved to the status bar below the tabs (0.4.12).
            if(session.RemoveOwnDll) notes.Add(language.Format("bridge_remove_copy","plugins\\"+session.Package.Target+".dll"));
            // 0.4.88: saved games that still need this plugin - the line that makes the switch-off
            // question below no surprise.
            var usedBy=SavesUsingCurrent();
            if(usedBy.Count>0) notes.Add(language.Format("saves_used_by",usedBy.Count,SaveGames.Names(usedBy,3,language.T("saves_and_more"))));
            // The overlay hint is a blue box of its own (0.4.19); the other package hints stay plain lines.
            string overlay=session.Package.Hints.Where(h=>Msg.KeyOf(h)=="hint_overlay").Select(language.Localize).FirstOrDefault()??"";
            notes.AddRange(session.Package.Hints.Where(h=>Msg.KeyOf(h)!="hint_overlay").Select(language.Localize));
            notes.AddRange(session.Notes.Where(n=>{string k=Msg.KeyOf(n);return k=="note_first_import"||k=="note_collection_kept";}).Select(language.Localize));
            problems=problems.Distinct().ToList(); notes=notes.Distinct().ToList(); warnings=warnings.Distinct().ToList();
            // The package-wide notice comes from the presentation schema or, for list/section editors, from the local spec.
            // An active list/section editor wins: LoadDraft also builds a presentation for such packages, but that one carries no notice.
            string packageNotice=localSpec!=null?localSpec.LocalizedNotice(language):presentation!=null?presentation.Notice:"";
            bool packageWarning=localSpec!=null?localSpec.NoticeStyle=="warning":presentation!=null&&presentation.NoticeStyle=="warning";
            string packageInfo=localSpec!=null?localSpec.LocalizedInfo(language):presentation!=null?presentation.Info:"";   // [launcher] info (0.4.10)
            if(update==null&&problems.Count==0&&notes.Count==0&&warnings.Count==0&&overlay.Length==0&&!session.LocalCopyOffered&&packageNotice.Length==0&&packageInfo.Length==0) return;
            var card=BeginCard(0,language.T("notices"));
            AddNotice(card,update,false);
            AddNotice(card,packageNotice,packageWarning);
            AddNotice(card,packageInfo,false);
            AddNotice(card,overlay,false);
            foreach(string w in warnings) AddNotice(card,w,true);
            foreach(string p in problems) AddNotice(card,p,"error");
            foreach(string n in notes) AddText(card,n,9.5f,false,Theme.Muted);
            if(session.LocalCopyOffered)
            {
                // "Dateien nur lokal": copy the package into plugins\ instead of loading it
                // from the Workshop through the bridge. Applied on save, after a confirmation.
                var row=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,10,0,0)};row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,70));
                var title=Theme.Label(language.T("local_copy"),10,true);var note=Theme.Label(language.T(session.PreferLocal?"local_copy_on_note":"local_copy_note"),9,false);note.ForeColor=Theme.Muted;note.MaximumSize=new Size(640,0);note.AutoSize=true;
                var textBox=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=1,Margin=Padding.Empty};textBox.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));AddRow(textBox,title);AddRow(textBox,note);
                var toggle=new ToggleSwitch{Checked=session.PreferLocal,AccessibleName="local-copy",Margin=new Padding(6,4,0,0)};toggle.CheckedChanged+=(s,e)=>{if(!refreshing)Run(()=>SetPreferLocal(toggle.Checked));};
                row.Controls.Add(textBox,0,0);row.Controls.Add(toggle,1,0);AddRow(((CardLayout)card.Tag).Inner,row);
            }
        }
        // The package's guides as buttons, one per UI language when the schema says so.
        void AddPresentationLinks()
        {
            var card=BeginCard(0,presentation.LinksLabel.Length>0?presentation.LinksLabel:language.T("links"));
            var row=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,4,0,0),Padding=Padding.Empty};
            foreach(LinkSpec link in presentation.LinksFor(language))
            {
                LinkSpec captured=link;string path;try{path=presentation.LinkPath(link);}catch(Exception e){Report(e.Message);continue;}
                var button=Theme.Button(link.Label,()=>Run(()=>{string file=presentation.LinkPath(captured);if(!File.Exists(file))throw new IOException(language.T("link_missing")+": "+captured.File);System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(file){UseShellExecute=true});}),false);
                button.Margin=new Padding(0,2,8,2);button.AccessibleName="link:"+link.Id;button.Enabled=File.Exists(path);tips.SetToolTip(button,path);row.Controls.Add(button);
            }
            AddRow(((CardLayout)card.Tag).Inner,row);
        }
        void SetPreferLocal(bool on)
        {
            if(session==null||session.PreferLocal==on) return;
            Collect(); var kept=new Dictionary<string,string>(session.Overrides,StringComparer.OrdinalIgnoreCase);
            session=new Session(session.Package,state.Build,on);
            session.Overrides.Clear(); foreach(var pair in kept) session.Overrides[pair.Key]=pair.Value;
            lastAction="ready"; RebuildPresentation(); BuildEditor();
        }
        // The confirmation before a local copy is made or removed: every file by name.
        string LocalCopyText()
        {
            string target=session.Build+"\\plugins";
            if(session.PreferLocal) return language.Format("local_copy_confirm_on",target)+"\n\n"+String.Join("\n",session.LocalCopyFiles())+"\n\nSHA-256 ("+session.Package.Target+".dll): "+SafeFiles.Hash(session.Package.Dll)+"\n\n"+language.T("trust_question");
            var present=session.AssetTargets.Where(File.Exists).Select(x=>x.Substring(session.Build.Length+1)).ToList();if(File.Exists(session.LocalDll))present.Insert(0,"plugins\\"+session.Package.Target+".dll");
            return language.Format("local_copy_confirm_off",target)+"\n\n"+String.Join("\n",present)+"\n\n"+language.T("trust_question");
        }
        internal Func<string,DialogResult> LocalCopyPrompt=null;
        internal bool PreferLocal{get{return session!=null&&session.PreferLocal;}}
        internal bool LocalCopyOffered{get{return session!=null&&session.LocalCopyOffered;}}
        internal void TestSetPreferLocal(bool on){SetPreferLocal(on);}
        // 0.4.56: a schema [folder_list:] - one row per subfolder found under the list's paths: the name,
        // a short text from the language file, and whether it lives in the package, locally, or both.
        void AddFolderList(CardLayout layout,FolderListSpec list)
        {
            var found=new SortedDictionary<string,int>(StringComparer.OrdinalIgnoreCase);   // name -> bit 1 package, bit 2 local
            foreach(string spec in list.Paths)
            {
                string folder;try{folder=presentation.ResolveFolder(spec,state.Build);}catch(FormatException){continue;}
                if(!Directory.Exists(folder))continue;int bit=Presentation.IsPackagePath(spec)?1:2;
                foreach(string dir in Directory.GetDirectories(folder)){string name=Path.GetFileName(dir);int have;found.TryGetValue(name,out have);found[name]=have|bit;}
            }
            var table=new TableLayoutPanel{ColumnCount=3,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Dock=DockStyle.Top,Margin=new Padding(0,4,0,0),Padding=Padding.Empty};
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            if(found.Count==0){var none=Theme.Label(language.T("folder_list_empty"),9.5f,false);none.ForeColor=Theme.Muted;AddRow(layout.Inner,none);}
            foreach(var pair in found)
            {
                int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                var name=Theme.Label(pair.Key,10,true);name.Margin=new Padding(0,4,18,6);name.AutoSize=true;
                var text=Theme.Label(presentation.FolderText(list.DescriptionPrefix,pair.Key),9.5f,false);text.ForeColor=Theme.Muted;text.Margin=new Padding(0,5,18,6);text.Dock=DockStyle.Top;layout.FieldWrap.Add(text);
                var where=Theme.Label(language.T(pair.Value==3?"folder_list_both":pair.Value==1?"folder_list_package":"folder_list_local"),9,false);where.ForeColor=pair.Value==1?Theme.Muted:Theme.Blue;where.Margin=new Padding(0,5,0,6);where.AutoSize=true;
                table.Controls.Add(name,0,row);table.Controls.Add(text,1,row);table.Controls.Add(where,2,row);
            }
            if(found.Count>0)AddRow(layout.Inner,table);
            if(list.Note.Length>0){var note=Theme.Label(list.Note,9,false);note.ForeColor=Theme.Muted;note.Margin=new Padding(0,10,0,4);layout.Wrap.Add(note);AddRow(layout.Inner,note);}
        }
        // 0.4.53: a schema [action:] row - label and description left, one button right, never stretched.
        void AddActionRow(TableLayoutPanel table,CardLayout layout,ActionSpec action)
        {
            int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            var text=new TableLayoutPanel{ColumnCount=1,AutoSize=true,Dock=DockStyle.Top,Margin=new Padding(0,5,20,20)};text.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            var label=Theme.Label(action.Label,11,true);AddRow(text,label);var help=Theme.Label(action.Description,9,false);help.ForeColor=Theme.Muted;AddRow(text,help);
            layout.FieldWrap.Add(label);layout.FieldWrap.Add(help);
            string command=action.Command;var button=Theme.Button(action.Button,()=>Run(()=>RunAction(command)),false);button.Anchor=AnchorStyles.Left|AnchorStyles.Top;button.Margin=new Padding(0,5,0,0);button.AccessibleName="action:"+action.Id;
            table.Controls.Add(text,0,row);table.Controls.Add(button,1,row);
        }
        // The commands behind [action:] rows. bridge_prune: drop [packages] entries of the Workshop Bridge
        // whose package is no longer in this manager's list; the player decides when (0.4.53).
        void RunAction(string command)
        {
            if(command!="bridge_prune")throw new InvalidOperationException(Msg.Key("err_unbekannte_aktion",command));
            if(!ResolvePending())return;
            var known=new HashSet<string>(entries.Where(e=>!e.Installed&&!String.IsNullOrEmpty(e.Root)).Select(e=>Path.GetFileName(e.Root.TrimEnd('\\','/'))),StringComparer.OrdinalIgnoreCase);
            var stale=Bridge.PackageKeys(state.Build).Where(k=>!known.Contains(k)).ToList();
            if(stale.Count==0){MessageWindow.Show(this,language,language.T("bridge_prune_title"),language.T("bridge_prune_clean"),MessageWindow.Kind.Info,DialogResult.OK);return;}
            if(MessageWindow.Show(this,language,language.T("bridge_prune_title"),language.Format("bridge_prune_question",stale.Count,String.Join(", ",stale)),MessageWindow.Kind.Question,DialogResult.OK,DialogResult.Cancel)!=DialogResult.OK)return;
            List<string> removed;string backup=Bridge.Prune(state.Build,k=>known.Contains(k),()=>RuntimeGuard.NoGameRunning(state.Build),out removed);
            Report(language.Format("bridge_prune_done",removed.Count,String.Join(", ",removed))+(backup.Length>0?"  "+backup:""));
            ShowEntry(current);
        }
        void AddGroup(GroupSpec group)
        {
            var fields=presentation.Fields.Where(f=>f.Group==group.Id && f.Field.Id!=presentation.EnabledField).ToList();
            // A group that only carries the header switch and says nothing else would be an empty card (0.4.19).
            var folderLists=presentation.FolderLists.Where(l=>l.Group==group.Id).OrderBy(l=>l.Order).ToList();
            if(fields.Count==0&&folderLists.Count==0&&group.Layout!="matrix"&&String.IsNullOrEmpty(group.Description)&&String.IsNullOrEmpty(group.Notice)&&String.IsNullOrEmpty(group.EmptyNotice)) return;
            var columns=fields.GroupBy(f=>f.Column).Select(g=>g.First()).ToList();
            bool matrix=group.Layout=="matrix" && fields.Count>0;
            var card=BeginCard(matrix?230+columns.Count*185+40:0,group.Label); var layout=(CardLayout)card.Tag;
            // 0.4.61: the card text uses the same size as the field help below it (was 10, read as a heading).
            AddText(card,group.Description,9,false,Theme.Muted);
            AddNotice(card,group.EmptyNotice,group.EmptyNoticeStyle=="warning");
            AddNotice(card,group.Notice,group.NoticeStyle=="warning");
            var table=new TableLayoutPanel { AutoSize=true, AutoSizeMode=AutoSizeMode.GrowAndShrink, Margin=Padding.Empty, Padding=Padding.Empty };
            if(matrix)
            {
                table.ColumnCount=columns.Count+1; table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,230));
                foreach(var c in columns) table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100f/columns.Count));
                table.RowCount=1; table.RowStyles.Add(new RowStyle(SizeType.Absolute,55));
                var first=Theme.Label(language.T("vehicle_class"),10,true); first.Margin=new Padding(0,6,8,0); table.Controls.Add(first,0,0);
                for(int i=0;i<columns.Count;i++) { var c=columns[i]; var label=Theme.Label(c.ColumnLabel+"\n"+c.Unit,10,true); label.Margin=new Padding(8,2,0,10); table.Controls.Add(label,i+1,0); }
                foreach(var row in fields.GroupBy(f=>f.Row))
                {
                    int y=table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.Absolute,96)); var firstField=row.First();
                    var label=new FlowLayoutPanel {Dock=DockStyle.Fill,WrapContents=false,Padding=new Padding(0,10,0,0),Margin=Padding.Empty};
                    if(firstField.Icon.Length>0) label.Controls.Add(new Glyph {Cache=icons,Root=session.Package.Root,Key=firstField.Icon,ForeColor=Theme.Ink});
                    var title=Theme.Label(firstField.RowLabel,10,false); title.Margin=new Padding(0,7,0,0); title.MaximumSize=new Size(176,0); label.Controls.Add(title); table.Controls.Add(label,0,y);
                    for(int x=0;x<columns.Count;x++) { var spec=row.FirstOrDefault(f=>f.Column==columns[x].Column); if(spec!=null) table.Controls.Add(MakeEditor(spec),x+1,y); }
                }
            }
            else
            {
                // Label column and input column share the row; both grow with the window.
                table.ColumnCount=2; FluidColumns(table,55,800);
                var actions=presentation.Actions.Where(a=>a.Group==group.Id).OrderBy(a=>a.Order).ToList();int nextAction=0;
                foreach(var field in fields)
                {
                    while(nextAction<actions.Count&&actions[nextAction].Order<=field.Order)AddActionRow(table,layout,actions[nextAction++]);
                    int row=table.RowCount++; table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                    var text=new TableLayoutPanel {ColumnCount=1,AutoSize=true,Dock=DockStyle.Top,Margin=new Padding(0,5,20,20)};
                    text.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
                    var label=Theme.Label(field.Label,11,true); AddRow(text,label);
                    var help=Theme.Label(field.Description,9,false); help.ForeColor=Theme.Muted; AddRow(text,help);
                    layout.FieldWrap.Add(label); layout.FieldWrap.Add(help);
                    table.Controls.Add(text,0,row); table.Controls.Add(MakeEditor(field),1,row);
                }
                while(nextAction<actions.Count)AddActionRow(table,layout,actions[nextAction++]);
            }
            AddRow(layout.Inner,table);
            foreach(var list in folderLists)AddFolderList(layout,list);
            var collection=presentation.Collections.FirstOrDefault(x=>x.ResourceGroup==group.Id);
            if(collection!=null)
            {
                var bar=new FlowLayoutPanel {AutoSize=true,Dock=DockStyle.Top,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Margin=new Padding(0,8,0,0)};
                var add=new VectorButton {Symbol="plus",Primary=true,BackColor=Theme.Blue,ForeColor=Color.White,Size=new Size(40,38),GlyphScale=2.5f,AccessibleName=language.T("add_resource")};
                ResourceRegistry registry;registries.TryGetValue(collection.Id,out registry);add.Enabled=registry!=null&&registry.Ready;
                add.Click+=(s,e)=>Run(()=>OpenResourceDialog(collection));tips.SetToolTip(add,add.Enabled?language.T("add_resource"):(registry==null?language.T("resource_unavailable"):registry.Problem));
                bar.Controls.Add(add);AddRow(layout.Inner,bar);
            }
        }
        Control MakeEditor(FieldSpec spec)
        {
            var field=spec.Field; string id=field.Id; var panel=new Panel {Dock=DockStyle.Top,Height=88,Margin=new Padding(8,3,9,5)};
            Control input;
            if(field.Type=="boolean")
            {
                var toggle=new ToggleSwitch {Checked=draft[id]=="1"}; toggle.CheckedChanged+=(s,e)=>{if(!refreshing) Edit(id,toggle.Checked?"1":"0");}; input=toggle; setters[id]=value=>toggle.Checked=value=="1";
            }
            else if(field.Type=="choice")
            {
                var select=new ComboBox {DropDownStyle=ComboBoxStyle.DropDownList,Dock=DockStyle.Top}; Fields.Tall(select); select.Items.AddRange(field.Choices); select.SelectedItem=draft[id]; select.SelectedIndexChanged+=(s,e)=>{if(!refreshing) Edit(id,Convert.ToString(select.SelectedItem));}; input=select; setters[id]=value=>select.SelectedItem=value;
            }
            else if(field.Type=="readonly")
            {
                var read=new Label {Text=draft[id],AutoSize=false,Size=new Size(120,Fields.Height),Font=Fields.Font,BackColor=Theme.Pale,BorderStyle=BorderStyle.FixedSingle,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(5,0,5,0),AutoEllipsis=true,TabStop=false,AccessibleRole=AccessibleRole.StaticText,AccessibleName=spec.Label,AccessibleDescription=spec.Description};
                bool removable=field.RemovableCollectionItem;
                if(removable)
                {
                    // The value box takes whatever width the row offers; the trash button keeps
                    // its fixed 34 px so buttons look the same at every window size.
                    string resource=draft[id];var line=new TableLayoutPanel {ColumnCount=2,RowCount=1,Size=new Size(159,Fields.Height),Margin=Padding.Empty,Padding=Padding.Empty};
                    line.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));line.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,39));
                    read.Dock=DockStyle.Fill;
                    var remove=new VectorButton {Symbol="trash",Danger=true,Size=new Size(34,Fields.Height),Anchor=AnchorStyles.Top|AnchorStyles.Left,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.1f,Margin=new Padding(5,0,0,0),AccessibleName=language.T("remove_resource")+": "+resource};
                    string collectionId=field.CollectionId;remove.Click+=(s,e)=>Run(()=>RequestRemoveResource(collectionId,resource));tips.SetToolTip(remove,remove.AccessibleName);
                    line.Controls.Add(read,0,0);line.Controls.Add(remove,1,0);input=line;
                }
                else input=read;
            }
            else if(field.Type=="text")
            {
                var box=new TextBox {Dock=DockStyle.Top,Text=draft[id],BorderStyle=BorderStyle.FixedSingle}; box.TextChanged+=(s,e)=>{if(!refreshing) Edit(id,box.Text);}; input=Fields.Wrap(box); setters[id]=value=>box.Text=value;
            }
            else
            {
                var number=new NumberInput(field,spec.Step) {Dock=DockStyle.Top}; number.Input.Text=draft[id]; number.Input.AccessibleName=spec.Label; number.ValueEdited+=(s,e)=>{if(!refreshing) Edit(id,number.Input.Text);}; input=number; setters[id]=value=>number.Input.Text=value;
            }
            input.AccessibleName=spec.Label; input.AccessibleDescription=spec.Description;
            // The input takes the row up to the reset button's own column on the right;
            // the "Standard / Persönlich" line below spans the full width, so neither
            // the text nor the button ever sits on top of the other.
            bool resettable=field.Type!="readonly"; int reserve=resettable?Fields.Reserve:0;
            // Explicit widths on every panel resize instead of anchors (0.4.15): a NumberInput anchored
            // Left|Right ended up wider than its panel, running under the reset button.
            input.Dock=DockStyle.None; input.Location=new Point(0,0); input.Anchor=AnchorStyles.Left|AnchorStyles.Top; panel.Controls.Add(input);
            VectorButton resetButton=null; Label originLabel=null;
            // Switches sit at the right edge beside the reset column (0.4.38); everything else fills the row.
            EventHandler fit=(s,e)=>{int w=panel.ClientSize.Width; if(input is ToggleSwitch) input.Left=Math.Max(0,w-reserve-input.Width); else input.Width=Math.Max(60,w-reserve); if(originLabel!=null) originLabel.Width=Math.Max(80,w); if(resetButton!=null){resetButton.Left=w-Fields.Reset; resetButton.Top=Math.Max(0,(input.Height-Fields.Reset)/2);}};
            panel.SizeChanged+=fit;
            string help=spec.Description; if(field.Type=="decimal" || field.Type=="integer") help+="\n"+language.T("input_range")+": "+field.Minimum+" – "+field.Maximum;
            tips.SetToolTip(input,help);
            // A NumberInput is a bordered panel whose text box covers it; the tooltip has to sit on the box too.
            var innerBox=Fields.Inner(input); if(innerBox!=null) tips.SetToolTip(innerBox,help);
            // A switch shows its state itself: no "Standard / Persönlich" line under it (0.4.38).
            if(field.Type=="boolean") panel.Height=44;
            else { var origin=Theme.Label("",8,false); origin.AutoSize=false; origin.Location=new Point(0,42); origin.Size=new Size(Math.Max(80,panel.Width),40); origin.Anchor=AnchorStyles.Left|AnchorStyles.Top; origins[id]=origin; panel.Controls.Add(origin); originLabel=origin; }
            if(resettable)
            { var reset=new VectorButton {Symbol="reset",Primary=true,BackColor=Theme.Blue,ForeColor=Color.White,GlyphScale=1.05f,Size=new Size(Fields.Reset,Fields.Reset),Location=new Point(panel.Width-Fields.Reset,0),Anchor=AnchorStyles.Left|AnchorStyles.Top,AccessibleName=language.T("reset")+": "+spec.Label}; reset.Click+=(s,e)=>ResetField(id); tips.SetToolTip(reset,reset.AccessibleName); panel.Controls.Add(reset); resetButton=reset; resetButtons[id]=reset; reset.Visible=!SameDraftValue(id,draft[id],field.DefaultValue); }
            fit(panel,EventArgs.Empty);
            return panel;
        }
        // Two-column editor grids: the text column takes its share of the width but
        // never more than `cap` pixels, the input column takes the rest. Re-applied
        // whenever the grid is resized, so dragging the window keeps the split.
        static void FluidColumns(TableLayoutPanel grid,int percent,int cap)
        {
            EventHandler apply=(s,e)=>
            {
                int w=grid.ClientSize.Width; if(w<=0) return; int text=Math.Min(cap,w*percent/100);
                if(grid.ColumnStyles.Count==2&&grid.ColumnStyles[0].SizeType==SizeType.Absolute&&(int)grid.ColumnStyles[0].Width==text) return;
                grid.SuspendLayout(); grid.ColumnStyles.Clear(); grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,text)); grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100)); grid.ResumeLayout(true);
            };
            grid.SizeChanged+=apply; apply(grid,EventArgs.Empty);
        }
        void ResizeCards()
        {
            if(resizing) return; resizing=true; int available=Math.Max(250,content.ClientSize.Width-24);
            try
            {
                foreach(Control card in content.Controls)
                {
                    var layout=card.Tag as CardLayout; if(layout==null) continue;
                    card.Width=Math.Max(available,layout.MinimumWidth); int width=card.Width-card.Padding.Horizontal;
                    foreach(var label in layout.Wrap) label.MaximumSize=new Size(width,0);
                    foreach(var label in layout.NoticeWrap) label.MaximumSize=new Size(Math.Max(100,width-64),0);
                    foreach(var label in layout.FieldWrap) label.MaximumSize=new Size(Math.Max(100,Math.Min(800,width*55/100)-40),0);
                    // A list/detail split fills the visible height instead of staying at its minimum.
                    if(layout.Tall!=null){int rest=layout.Inner.GetPreferredSize(new Size(width,0)).Height-layout.Tall.Height; layout.Tall.Height=Math.Max(535,content.ClientSize.Height-rest-card.Padding.Vertical-content.Padding.Vertical-36);}
                    layout.Inner.Width=width; layout.Inner.PerformLayout(); card.Height=layout.Inner.GetPreferredSize(new Size(width,0)).Height+card.Padding.Vertical;
                }
            }
            finally {resizing=false;}
            // Adding the cards can show or hide the scrollbar, which changes ClientSize only after this
            // layout pass; measure once more against the new width (0.4.11).
            content.PerformLayout();
            if(content.ClientSize.Width-24!=available&&!resizingAgain){resizingAgain=true;try{ResizeCards();}finally{resizingAgain=false;}}
        }
        bool resizingAgain;
        void Edit(string id,string value)
        {
            if(session==null || presentation==null || !presentation.Fields.Any(x=>x.Field.Id==id && x.Field.Type!="readonly")) return;
            draft[id]=value; resets.Remove(id); lastAction="ready"; UpdateStatus();
            if(id==presentation.EnabledField) SyncActivation();
        }
        void ResetField(string id)
        {
            var field=presentation.Fields.Select(x=>x.Field).First(x=>x.Id==id);draft[id]=field.DefaultValue;resets.Add(id);
            refreshing=true; try { Action<string> setter; if(setters.TryGetValue(id,out setter)) setter(draft[id]); } finally {refreshing=false;}
            if(presentation.EnabledField==id) SyncActivation(); UpdateStatus();
        }
        void UpdateStatus()
        {
            if(language==null) return;
            bool available=session!=null||resourceSession!=null||contentSession!=null,valid=available; string error="";
            if(valid) try {if(resourceSession!=null)resourceSession.ValidateAll();else if(contentSession==null)Prospective();} catch(Exception e) {valid=false;error=ErrorText(e);}
            // 0.4.51: three states in the heading - green saved and valid, amber unsaved, red invalid -
            // and the same state on the save button (primary only while there is something to save),
            // as a " *" on the page heading and as an amber dot at the list entry.
            // 0.4.71: parked entries count too - the footer names every unsaved entry, an invalid parked
            // one blocks saving and is named, and the amber dots mark all of them.
            bool pending=available&&HasPending; List<Workspace> unsaved=Unsaved; int total=unsaved.Count;
            var invalidParked=parked.Values.Where(x=>!x.Valid).Select(x=>x.Entry.Name).ToList(); bool blocked=(available&&!valid)||invalidParked.Count>0;
            status.Text=language.T(!available&&total==0?"unavailable":blocked?"invalid":total>0?"unsaved":"valid"); status.ForeColor=!available&&total==0?Theme.Muted:blocked?Color.Firebrick:total>0?Theme.Amber:Color.FromArgb(26,132,61);
            statusDetail.Text=error.Length>0?error:invalidParked.Count>0?language.Format("invalid_elsewhere",String.Join(", ",invalidParked)):total>1||(total==1&&!pending)?language.Format("unsaved_many",String.Join(", ",unsaved.Select(x=>x.Entry.Name))):language.T(pending?"unsaved_detail":lastAction); tips.SetToolTip(statusDetail,statusDetail.Text);
            // 0.4.57: a pending package update (yellow "Update" mark) is saved away too, so the button stays usable then.
            bool updatePending=session!=null&&session.Update.Pending;
            if(saveButton!=null){bool canSave=!blocked&&(total>0||updatePending);saveButton.Enabled=canSave;saveButton.BackColor=canSave?Theme.Blue:Color.White;saveButton.ForeColor=canSave?Color.White:Theme.Ink;saveButton.FlatAppearance.BorderSize=canSave?0:1;saveButton.FlatAppearance.MouseOverBackColor=canSave?Color.FromArgb(0,70,180):Color.FromArgb(222,232,248);}
            // Save + Start follows the same rule as Save: locked while any entry is invalid; a page without
            // an editor still offers it while other entries wait to be written.
            if(startButton!=null) startButton.Enabled=!blocked&&(available||total>0);
            string headingBase=heading.Text.EndsWith(" *")?heading.Text.Substring(0,heading.Text.Length-2):heading.Text;string headingNow=pending?headingBase+" *":headingBase;if(heading.Text!=headingNow)heading.Text=headingNow;
            var dirtyNow=new HashSet<string>(parked.Keys,StringComparer.OrdinalIgnoreCase);if(pending&&this.current!=null)dirtyNow.Add(this.current.Root);if(!mods.DirtyRoots.SetEquals(dirtyNow)){mods.DirtyRoots.Clear();mods.DirtyRoots.UnionWith(dirtyNow);mods.Invalidate();if(ListFilter!="all")FilterList();}
            foreach(var pair in origins)
            {
                string id=pair.Key,current=draft.ContainsKey(id)?draft[id]:"",original=baseline.ContainsKey(id)?baseline[id]:"";
                bool changed=resets.Contains(id)||!baseline.ContainsKey(id)||!SameDraftValue(id,current,original);bool personal=!resets.Contains(id)&&session.Overrides.ContainsKey(id)&&!IsPackageDefault(id,current);
                var field=presentation.Fields.Select(x=>x.Field).First(x=>x.Id==id);string standard=field.DefaultValue;
                pair.Value.Text=language.T("standard")+": "+standard+"\n"+(changed?language.T("unsaved"):personal?language.T("personal"):"");
                pair.Value.ForeColor=changed?Color.FromArgb(155,100,0):personal?Theme.Blue:Theme.Muted;
            }
            // The reset button appears only while the value differs from the package default (0.4.38).
            foreach(var pair in resetButtons)
            {
                string id=pair.Key,current=draft.ContainsKey(id)?draft[id]:"";var field=presentation.Fields.Select(x=>x.Field).FirstOrDefault(x=>x.Id==id);
                if(field!=null&&!pair.Value.IsDisposed)pair.Value.Visible=!SameDraftValue(id,current,field.DefaultValue);
            }
        }

        void OpenResourceDialog(CollectionSpec collection)
        {using(var dialog=BuildResourceDialog(collection))Theme.Modal(this,dialog);}
        Form BuildResourceDialog(CollectionSpec collection)
        {
            if(collection==null)throw new InvalidOperationException(language.T("resource_unavailable"));
            ResourceRegistry resourceRegistry=ResourceRegistry.Load(session.Build,collection);registries[collection.Id]=resourceRegistry;
            if(!resourceRegistry.Ready)throw new IOException(resourceRegistry.Problem);
            var used=new HashSet<string>(CollectionRules.Names(collection,DraftConfig()),StringComparer.OrdinalIgnoreCase);
            bool hasCapacity=used.Count<collection.MaximumItems;
            var dialog=new Form {Text=language.T("add_resource"),Font=Font,Size=new Size(660,690),MinimumSize=new Size(560,600),StartPosition=FormStartPosition.CenterParent,ShowInTaskbar=false,MinimizeBox=false,MaximizeBox=false,Icon=Icon};Theme.ApplyWindowChrome(dialog);
            var shell=new TableLayoutPanel {Dock=DockStyle.Fill,ColumnCount=1,RowCount=5,Padding=new Padding(20),Margin=Padding.Empty};
            shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));shell.RowStyles.Add(new RowStyle(SizeType.Percent,100));shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));shell.RowStyles.Add(new RowStyle(SizeType.Absolute,58));dialog.Controls.Add(shell);
            var help=Theme.Label(language.T("resource_picker_help"),10,false);help.ForeColor=Theme.Muted;help.MaximumSize=new Size(590,0);shell.Controls.Add(help,0,0);
            var list=new ResourcePickerList {Dock=DockStyle.Fill,Margin=new Padding(0,8,0,18)};
            foreach(var option in resourceRegistry.Options)list.Items.Add(new ResourceChoice {Option=option,Used=used.Contains(option.Id)});
            shell.Controls.Add(list,0,1);
            var coefficientTitle=Theme.Label(language.T("resource_coefficients"),11,true);shell.Controls.Add(coefficientTitle,0,2);
            var values=new Dictionary<string,NumberInput>(StringComparer.OrdinalIgnoreCase);
            var grid=new TableLayoutPanel {Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,5,0,4)};grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,165));
            int row=0;foreach(string target in collection.TargetSections)
            {
                var template=presentation.Fields.FirstOrDefault(x=>x.Group==collection.MatrixGroup&&x.Row.Equals(target,StringComparison.OrdinalIgnoreCase));
                var field=new Field {Section=target,Key="new",Type=collection.Type,Minimum=collection.Minimum,Maximum=collection.Maximum,DefaultValue=collection.DefaultValue};
                var input=new NumberInput(field,collection.Step) {Dock=DockStyle.Top,Margin=new Padding(10,3,0,8)};input.Input.Text=collection.DefaultValue;values[target]=input;
                grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));grid.Controls.Add(Theme.Label(template==null?target:template.RowLabel,10,false),0,row);grid.Controls.Add(input,1,row++);
            }
            shell.Controls.Add(grid,0,3);
            var buttons=new FlowLayoutPanel {Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
            var add=Theme.Button(language.T("add"),()=>
            {
                try
                {
                    var choice=list.Choice;if(choice==null||choice.Used)return;
                    AddResource(collection,choice.Option.Id,values.ToDictionary(x=>x.Key,x=>NumberInput.Canonical(x.Value.Input.Text),StringComparer.OrdinalIgnoreCase));
                    dialog.DialogResult=DialogResult.OK;dialog.Close();
                }
                catch(Exception failure){ShowError(failure);}
            },true);
            add.Enabled=hasCapacity;
            var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(add);buttons.Controls.Add(cancel);dialog.CancelButton=cancel;shell.Controls.Add(buttons,0,4);
            return dialog;
        }
        void AddResource(CollectionSpec collection,string id,IDictionary<string,string> coefficients)
        {
            if(collection==null)throw new InvalidOperationException(language.T("resource_unavailable"));
            var latest=ResourceRegistry.Load(session.Build,collection);if(!latest.Ready)throw new IOException(latest.Problem);
            if(latest.Find(id)==null)throw new InvalidOperationException(language.T("resource_changed")+": "+id);
            var current=new Ini(session.Package.Defaults.Render(Prospective()));var added=CollectionRules.Add(session.Package,collection,current,id,coefficients);
            draft.Clear();foreach(var pair in added.Values)draft[pair.Key]=pair.Value;resets.Clear();registries[collection.Id]=latest;lastAction="ready";
            RebuildPresentation();BuildEditor();
        }
        bool RequestRemoveResource(string collectionId,string id)
        {
            CollectionSpec collection=presentation.Collections.FirstOrDefault(x=>x.Id.Equals(collectionId,StringComparison.OrdinalIgnoreCase));if(collection==null)throw new InvalidOperationException(language.T("resource_unavailable"));
            ResourceRegistry resourceRegistry=null;registries.TryGetValue(collection.Id,out resourceRegistry);ResourceOption option=resourceRegistry==null?null:resourceRegistry.Find(id);
            string shown=option!=null&&!option.Display.Equals(id,StringComparison.OrdinalIgnoreCase)?option.Display+" ("+id+")":id;
            string message=String.Format(language.T("remove_resource_question"),shown);
            DialogResult answer=ResourceRemovePrompt!=null?ResourceRemovePrompt(message):Question(message,false);
            if(answer!=DialogResult.OK)return false;
            var current=new Ini(session.Package.Defaults.Render(Prospective()));var removed=CollectionRules.Remove(session.Package,collection,current,id);
            draft.Clear();foreach(var pair in removed.Values)draft[pair.Key]=pair.Value;resets.Clear();lastAction="ready";
            RebuildPresentation();BuildEditor();UpdateStatus();return true;
        }
    }
}
