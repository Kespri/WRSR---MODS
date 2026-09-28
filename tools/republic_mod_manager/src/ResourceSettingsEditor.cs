using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    public sealed partial class MainForm
    {
        // 0.4.26: a rebuild after a committed value used to reset the scroll position and the
        // focus, so every change far down a long detail pane threw the user back to the top.
        // The scroll offsets of the page and of the detail pane plus the focused control's
        // accessible name are captured before the controls are disposed and restored after.
        Panel detailsPanel;
        sealed class EditorView{public int ContentScroll,DetailsScroll;public string Focus;}
        EditorView CaptureView()
        {
            var view=new EditorView();
            try
            {
                view.ContentScroll=-content.AutoScrollPosition.Y;
                if(detailsPanel!=null&&!detailsPanel.IsDisposed)view.DetailsScroll=-detailsPanel.AutoScrollPosition.Y;
                Control active=ActiveControl;while(active is ContainerControl&&((ContainerControl)active).ActiveControl!=null)active=((ContainerControl)active).ActiveControl;
                if(active!=null&&!String.IsNullOrEmpty(active.AccessibleName))view.Focus=active.AccessibleName;
            }
            catch(Exception){}
            return view;
        }
        void RestoreView(EditorView view)
        {
            if(view==null)return;
            try
            {
                if(view.DetailsScroll>0&&detailsPanel!=null&&!detailsPanel.IsDisposed){detailsPanel.PerformLayout();detailsPanel.AutoScrollPosition=new Point(0,view.DetailsScroll);}
                if(view.ContentScroll>0){content.PerformLayout();content.AutoScrollPosition=new Point(0,view.ContentScroll);}
                if(!String.IsNullOrEmpty(view.Focus)){Control target=FindByAccessibleName(content,view.Focus);if(target!=null&&target.CanFocus)target.Focus();}
            }
            catch(Exception){}
        }
        static Control FindByAccessibleName(Control root,string name)
        {
            foreach(Control child in root.Controls){if(child.AccessibleName==name)return child;Control hit=FindByAccessibleName(child,name);if(hit!=null)return hit;}
            return null;
        }
        void BuildLocalResourceEditor()
        {
            if(resourceSession==null||localSpec==null)return;
            // 0.4.50: tab strip and content stay frozen while their cards are replaced (Theme.SetRedraw).
            Theme.SetRedraw(tabStrip,false);Theme.SetRedraw(content,false);
            try{BuildLocalResourceEditorCore();}
            finally{Theme.SetRedraw(tabStrip,true);Theme.SetRedraw(content,true);}
        }
        void BuildLocalResourceEditorCore()
        {
            EditorView view=CaptureView();
            description.Text=localSpec.LocalizedDescription(language);Theme.DisposeChildren(tabStrip);Theme.DisposeChildren(content);setters.Clear();origins.Clear();detailsPanel=null;
            string switchNote=session==null?language.T("switch_note_local"):session.SwitchMode=="loader"?language.T(session.BridgeActive?"switch_note_bridge":"switch_note_loader"):session.SwitchMode=="ini"?language.T("switch_note_ini"):language.T("switch_note_sml_only");
            // 0.5.16: while SML hosts the capability the fork's switch is hidden, exactly as a package's
            // switch is under SML (SwitchMode "none") - the plugin steps aside whatever the key says.
            ShowSwitch(session==null?!RuntimeStatus.Blocked(localSpec,state.Build):session.SwitchMode!="none",switchNote);
            // Tabs come from the schema; the list card and the plugin-wide card each name theirs.
            if(!localSpec.Tabs.Any(t=>t.Id==state.SelectedTab))state.SelectedTab=localSpec.Tabs.Any(t=>t.Id==localSpec.DefaultTab)?localSpec.DefaultTab:localSpec.Tabs[0].Id;
            // 0.4.75: a required text pack that is not local yet tints its tab red and puts a red
            // box on every tab, so nobody misses the "Create locally" step.
            bool textPackDue=localSpec.TextPackDue(resourceSession.Build);
            foreach(LocalTab entry in localSpec.Tabs)
            {
                string id=entry.Id;bool active=id==state.SelectedTab;var tab=Theme.TabButton(localSpec.LocalizedTabLabel(language,entry),()=>Run(()=>{state.SelectedTab=id;BuildLocalResourceEditor();}));tab.Tag=id;
                tab.ForeColor=active?Color.White:Theme.Ink;tab.BackColor=active?Theme.SelectionBlue:Theme.Frame;tab.FlatAppearance.MouseOverBackColor=active?Theme.SelectionBlue:Theme.TabHover;
                if(textPackDue&&!active&&id.Equals(localSpec.TextPack.Tab,StringComparison.OrdinalIgnoreCase)){tab.BackColor=Color.FromArgb(253,235,235);tab.ForeColor=Color.FromArgb(150,28,28);tab.FlatAppearance.MouseOverBackColor=Color.FromArgb(247,214,214);tab.AccessibleName="tab-due:"+id;}
                tabStrip.Controls.Add(tab);
            }
            tabStrip.Invalidate();
            if(textPackDue)
            {
                string due=localSpec.LocalizedTextPackRequired(language);if(due.Length==0)due=language.Format("textpack_required",localSpec.LocalizedTabLabel(language,localSpec.Tabs.First(t=>t.Id.Equals(localSpec.TextPack.Tab,StringComparison.OrdinalIgnoreCase))),language.T("textpack_create"));
                var card=BeginCard(780);AddNotice(card,due,"error");
            }
            // 0.5.9: Soviet Mod Loader has taken this plugin's job over - it steps aside at the game
            // start and NOTHING in here reaches the game. Saying that once in the notice card of the
            // first tab was not enough: every other tab looked ready to use. Now the box stands on
            // every tab and the page below it is dead, until you say you want to prepare something
            // for the day you switch SML off.
            // 0.5.10: a local editor schema may declare replaced_by_sml itself (Resources Plus).
            bool replacedByPackage=session!=null&&RuntimeStatus.Blocked(session.Package,state.Build);
            bool replaced=replacedByPackage||(session==null&&RuntimeStatus.Blocked(localSpec,state.Build));
            Control pausedCard=null;
            if(replaced)
            {
                string pausedName=replacedByPackage?session.Package.Name:localSpec.LocalizedName(language),pausedKind=replacedByPackage?session.Package.ReplacedBySml:localSpec.ReplacedBySml;
                var card=BeginCard(780);pausedCard=card;
                AddNotice(card,language.Format(pausedUnlocked?"sml_replaced":"sml_replaced_locked",pausedName,language.T("content_kind_"+pausedKind)),"warning");
                if(!pausedUnlocked)
                {
                    var layout=(CardLayout)card.Tag;
                    var unlock=Theme.Button(language.T("sml_replaced_unlock"),()=>Run(()=>{pausedUnlocked=true;BuildLocalResourceEditor();}),false);
                    unlock.AccessibleName="sml:unlock";unlock.Margin=new Padding(0,2,8,4);
                    var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,0,0,4),Padding=Padding.Empty};
                    flow.Controls.Add(unlock);AddRow(layout.Inner,flow);
                }
            }
            // 0.5.3: this editor writes SML's baseline, not the generated file in plugins\. That
            // changes where every value goes, so it is said on every tab, not tucked into one card.
            if(resourceSession.SmlBaseline!=null)
            {
                var card=BeginCard(780);
                AddNotice(card,language.Format("sml_base_notice",Sml.Show(resourceSession.Build,resourceSession.SmlBaseline),"plugins\\"+localSpec.ConfigName),"info");
            }
            // 0.5.11: the page created the fork's INI from the player's list just now - say where from.
            if(resourceSession.SeededFrom.Length>0)
            {
                var card=BeginCard(780);
                AddNotice(card,language.Format("fork_seeded","plugins\\"+localSpec.ConfigName,resourceSession.SeededFrom),"info");
            }
            // 0.5.9: the overlay layer only exists for Soviet Mod Loader. Is it switched off, the
            // layer sits in the Workshop folder doing nothing - say so instead of letting the
            // changes vanish silently.
            else if(SmlOverlay.Covers(localSpec.Plugin)&&SmlOverlay.Idle(resourceSession.Build))
            {
                var card=BeginCard(780);
                AddNotice(card,language.Format("sml_overlay_idle",SmlOverlay.Name,SmlOverlay.Root(resourceSession.Build)),"warning");
            }
            bool isList=localSpec.IsList,isSections=localSpec.IsSections;
            // A package-backed editor shows the package's notices (bridge, local copy) on
            // the plugin-wide tab, then the guides, then the cards of that tab.
            if(session!=null&&state.SelectedTab==localSpec.GlobalTab)AddNotices();
            BuildLocalLinks();
            if(localSpec.GroupTab==state.SelectedTab)BuildLocalListCard(null);
            foreach(ItemGroup extra in localSpec.ExtraGroups)if(extra.Tab==state.SelectedTab)BuildLocalListCard(extra);   // 0.4.29
            if(localSpec.TextPack!=null&&localSpec.TextPack.Tab==state.SelectedTab)BuildTextPackCard();   // 0.4.30
            if(localSpec.Wip!=null&&localSpec.Wip.Tab==state.SelectedTab)BuildWipCard();   // 0.5.5
            BuildLocalGlobals();
            // Everything that was built after the warning is switched off - the switch in the header
            // and the warning card itself stay alive, they are the way out.
            if(replaced&&!pausedUnlocked)
                foreach(Control card in content.Controls)
                {
                    if(ReferenceEquals(card,pausedCard))continue;
                    var box=card as Card;if(box!=null){box.Dimmed=true;box.BackColor=Theme.Dim;}
                    LockInputs(card);DimCard(card);
                }
            ResizeCards();SyncActivation();UpdateStatus();RestoreView(view);SaveView();
        }
        // The list card of one item group: the default group ([group:]/[list]/[new]) or an
        // extra group with its own section prefix (0.4.29). Texts and fields follow the group;
        // one list per tab shares the detail panel.
        void BuildLocalListCard(ItemGroup group)
        {
            // 0.5.8: with style = cards the entries are rows, not a list beside a detail panel.
            if(localSpec.IsCards){BuildLocalCardList(group);return;}
            bool isList=localSpec.IsList,isSections=localSpec.IsSections;
            var card=BeginCard(780,localSpec.LocalizedGroup(language,group));AddText(card,localSpec.LocalizedGroupDescription(language,group),9,false,Theme.Muted);string groupNotice=localSpec.LocalizedGroupNotice(language,group);if(groupNotice.Length>0)AddNotice(card,groupNotice,false);string saveWarning=localSpec.LocalizedSaveWarning(language,group);
            // 0.5.4: the schema texts name the effective INI in plugins\. Under SML that file is
            // generated and the entries go to its baseline, so the warning has to name that one.
            if(resourceSession.SmlBaseline!=null)saveWarning=language.Format("save_warning_sml",Sml.Show(resourceSession.Build,resourceSession.SmlBaseline));
            AddNotice(card,saveWarning.Length>0?saveWarning:localSpec.HidesOriginals?language.Format("list_save_warning",localSpec.ConfigName):language.T("resource_save_warning"),true);
            var split=new SplitContainer{Dock=DockStyle.Top,Size=new Size(1000,535),FixedPanel=FixedPanel.Panel1,Panel1MinSize=240,Panel2MinSize=380,SplitterDistance=300,IsSplitterFixed=false,BorderStyle=BorderStyle.FixedSingle,Margin=new Padding(0,2,0,0)};split.Panel1.Padding=new Padding(0);split.Panel2.Padding=new Padding(18,0,0,0);
            var left=new TableLayoutPanel{Dock=DockStyle.Fill,RowCount=4,ColumnCount=1,Padding=new Padding(10),Margin=Padding.Empty};left.RowStyles.Add(new RowStyle(SizeType.AutoSize));left.RowStyles.Add(new RowStyle(SizeType.AutoSize));left.RowStyles.Add(new RowStyle(SizeType.Percent,100));left.RowStyles.Add(new RowStyle(SizeType.Absolute,50));
            string listLabel=localSpec.LocalizedListLabel(language,group);var listTitle=Theme.Label(listLabel.Length>0?listLabel:language.T("resource_list"),11,true);listTitle.Margin=new Padding(0,0,0,8);left.Controls.Add(listTitle,0,0);
            var list=new ResourceDefinitionList{Dock=DockStyle.Fill,Font=new Font("Segoe UI",10)};var items=resourceSession.Items(group);if(localSpec.IdPickerOf(group)=="game_research")foreach(var item in items){ResearchEntry found=FindResearch(localSpec.DisplayId(item.Id));if(found!=null&&found.Name.Length>0)item.Subtitle=found.Name+(item.Subtitle.Length>0?"  ·  "+item.Subtitle:"");}foreach(var item in items)list.Items.Add(item);left.Controls.Add(list,0,2);
            // 0.5.19: a search box above the list once it is long enough to need one - the same rule
            // as the card list (8 entries). Typing only refills the ListBox; nothing is rebuilt, and the
            // selected entry stays selected as long as it still matches.
            Action refill=null;
            if(items.Count>=8)
            {
                var all=items.ToList();
                var box=new TextBox{Text=listFilter,BorderStyle=BorderStyle.None,Font=Fields.Font,AccessibleName="list-search"};var search=Fields.Wrap(box);search.Dock=DockStyle.Top;search.Margin=new Padding(0,0,0,8);
                refill=()=>{string needle=listFilter.Trim();refreshingListItem=true;try{list.BeginUpdate();list.Items.Clear();foreach(var it in all)if(needle.Length==0||it.ToString().IndexOf(needle,StringComparison.CurrentCultureIgnoreCase)>=0||it.Subtitle.IndexOf(needle,StringComparison.CurrentCultureIgnoreCase)>=0)list.Items.Add(it);list.EndUpdate();var keep=list.Items.Cast<LocalResourceItem>().FirstOrDefault(x=>x.Id.Equals(selectedLocalResource,StringComparison.OrdinalIgnoreCase));if(keep!=null)list.SelectedItem=keep;}finally{refreshingListItem=false;}if(list.SelectedItem==null&&list.Items.Count>0)list.SelectedIndex=0;};
                box.TextChanged+=(s,e)=>{if(refreshing)return;listFilter=box.Text;refill();};
                left.Controls.Add(search,0,1);
            }
            string addLabel=localSpec.LocalizedAddLabel(language,group);
            // Below the list: an optional note from [list] note (0.4.8) on the left, the + button on the right.
            var addBar=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2,Margin=Padding.Empty,Padding=new Padding(0,8,0,0)};addBar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));addBar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,44));
            string listNote=localSpec.LocalizedListNote(language,group);if(listNote.Length>0){var noteLabel=Theme.Label(listNote,9,false);noteLabel.ForeColor=Theme.Muted;noteLabel.Dock=DockStyle.Top;noteLabel.Margin=new Padding(0,4,8,0);noteLabel.AccessibleName="list-note";addBar.Controls.Add(noteLabel,0,0);}
            var add=new VectorButton{Symbol="plus",Primary=true,BackColor=Theme.Blue,ForeColor=Color.White,Size=new Size(40,38),GlyphScale=2.5f,Anchor=AnchorStyles.Top|AnchorStyles.Right,AccessibleName=addLabel.Length>0?addLabel:language.T("resource_create")};add.Click+=(s,e)=>Run(()=>{if(isSections)OpenLocalSectionDialog(group);else if(isList)OpenLocalListDialog();else OpenLocalResourceDialog();});tips.SetToolTip(add,add.AccessibleName);addBar.Controls.Add(add,1,0);left.Controls.Add(addBar,0,3);split.Panel1.Controls.Add(left);
            var details=new Panel{Dock=DockStyle.Fill,AutoScroll=true};detailsPanel=details;detailsList=list;split.Panel2.Controls.Add(details);narrowDetails=false;
            Action<LocalResourceItem> show=item=>{if(isSections)BuildLocalSectionDetails(details,list,item);else if(isList)BuildLocalListDetails(details,list,item);else BuildLocalResourceDetails(details,list,item);};
            list.SelectedIndexChanged+=(s,e)=>{if(refreshingListItem)return;var item=list.SelectedItem as LocalResourceItem;if(item==null)return;selectedLocalResource=item.Id;show(item);};
            // Crossing the narrow threshold re-lays the details out (0.4.36).
            details.SizeChanged+=(s,e)=>{bool now=Narrow(details);if(now==narrowDetails)return;narrowDetails=now;var item=list.SelectedItem as LocalResourceItem;if(item!=null)show(item);};
            LocalResourceItem selected=items.FirstOrDefault(x=>x.Id.Equals(selectedLocalResource,StringComparison.OrdinalIgnoreCase))??items.FirstOrDefault();if(selected!=null)list.SelectedItem=selected;else BuildLocalResourceEmpty(details,group);
            if(refill!=null&&listFilter.Trim().Length>0)refill();
            AddRow(((CardLayout)card.Tag).Inner,split);((CardLayout)card.Tag).Tall=split;
            if(group==null&&localSpec.HidesOriginals)AddHiddenOriginals(((CardLayout)card.Tag).Inner);
        }
        // ---- 0.5.8: the card list ([list] style = cards) ----
        // One row per entry instead of a list beside a detail panel: switch, name, the values
        // named by subtitle, a state line and the buttons named by actions. The detail panel
        // moves into the dialog behind "Aendern...". Nothing about the session changes - the
        // switch and the dialog write into the draft, saving stays where it always was.
        string cardFilter="";
        string listFilter="";
        // The "Add" pill in the blue header strip of a card. The strip is painted by the card
        // itself, so the button has to know the colour it sits on.
        void AddCardHeaderButton(Card card,string text,Action action)
        {
            var button=new VectorButton{Symbol="plus",Caption=text,Pill=true,Behind=Theme.SelectionBlue,BackColor=Color.White,ForeColor=Theme.SelectionBlue,GlyphScale=2.1f,Height=34,AccessibleName="card:add"};
            using(var font=new Font("Segoe UI",10,FontStyle.Bold))button.Width=30+TextRenderer.MeasureText(text,font,Size.Empty,TextFormatFlags.NoPadding).Width+16;
            button.Click+=(s,e)=>Run(action);tips.SetToolTip(button,text);
            card.Controls.Add(button);button.BringToFront();
            EventHandler place=(s,e)=>button.Location=new Point(Math.Max(0,card.Width-Theme.Shadow-button.Width-18),(56-button.Height)/2);
            card.Resize+=place;place(null,EventArgs.Empty);
        }
        // The values the state provider needs, read out of the entry's fields by key.
        BuildingRow CardRowOf(LocalResourceItem item,ItemGroup group)
        {
            var fields=localSpec.ItemFields(group).ToList();
            Func<string,LocalDetailField> byKey=key=>fields.FirstOrDefault(x=>x.Key.Equals(key,StringComparison.OrdinalIgnoreCase));
            Func<string,string> value=key=>{LocalDetailField f=byKey(key);return f==null?"":resourceSession.Value(item.Id,f).Trim();};
            // No object field in the schema means the folder is simply the section name and there
            // is nothing to compare - null says "unknown" so the state line stays quiet instead of
            // claiming a rebuild forever (a hand-written object key is outside the editor anyway).
            var row=new BuildingRow{Id=localSpec.DisplayId(item.Id),Number=value("id"),Donor=value("donor"),Object=byKey("object")==null?null:value("object")};
            // No number of its own: the plugin assigned one and keeps it in the INI that
            // assigned_from names - the same file the grey placeholder in the field reads.
            if(row.Number.Length==0){LocalDetailField f=byKey("id");if(f!=null&&f.AssignedFrom.Length>0)row.Number=AssignedValue(f,item.Id);}
            LocalDetailField toggle=localSpec.FieldById(localSpec.CardToggle);
            if(toggle!=null){string on=resourceSession.Value(item.Id,toggle);row.Enabled=on.Length==0?toggle.Default=="1":on=="1";}
            return row;
        }
        bool CardPrune()
        {
            LocalDetailField prune=localSpec.Fields.FirstOrDefault(x=>x.Scope=="global"&&x.Key.Equals("prune",StringComparison.OrdinalIgnoreCase));
            if(prune==null)return false;
            string value=resourceSession.GlobalValue(prune);
            return value.Length==0?prune.Default=="1":value=="1";
        }
        string CardFolder(BuildingRow row)
        {
            if(row.Number.Length==0)return null;
            try{string root=WipBuildings.Root(resourceSession.Build);return root==null?null:Path.Combine(root,row.Number);}catch(Exception){return null;}
        }
        void BuildLocalCardList(ItemGroup group)
        {
            var card=BeginCard(780,localSpec.LocalizedGroup(language,group));
            string addLabel=localSpec.LocalizedAddLabel(language,group);
            AddCardHeaderButton(card,addLabel.Length>0?addLabel:language.T("resource_create"),()=>OpenNewCardDialog(group));
            AddText(card,localSpec.LocalizedGroupDescription(language,group),9,false,Theme.Muted);
            string groupNotice=localSpec.LocalizedGroupNotice(language,group);if(groupNotice.Length>0)AddNotice(card,groupNotice,false);
            string saveWarning=localSpec.LocalizedSaveWarning(language,group);
            if(resourceSession.SmlBaseline!=null)saveWarning=language.Format("save_warning_sml",Sml.Show(resourceSession.Build,resourceSession.SmlBaseline));
            AddNotice(card,saveWarning.Length>0?saveWarning:language.Format("list_save_warning",localSpec.ConfigName),true);
            LocalDetailField titleField=localSpec.FieldById(localSpec.CardTitle),toggleField=localSpec.FieldById(localSpec.CardToggle);
            Func<LocalResourceItem,string> caption=item=>{string t=titleField==null?"":resourceSession.Value(item.Id,titleField).Trim();return t.Length>0?t:localSpec.DisplayId(item.Id);};
            var all=resourceSession.Items(group).OrderBy(caption,StringComparer.CurrentCultureIgnoreCase).ToList();
            var layout=(CardLayout)card.Tag;
            if(all.Count==0){AddText(card,localSpec.LocalizedSelectHelp(language,group),10,false,Theme.Muted);return;}
            // A search box only once the list is long enough to need one.
            if(all.Count>=8)
            {
                var box=new TextBox{Text=cardFilter,BorderStyle=BorderStyle.None,Font=Fields.Font,AccessibleName="card-search"};
                var search=Fields.Wrap(box);
                box.TextChanged+=(s,e)=>{if(refreshing)return;cardFilter=box.Text;};
                box.Leave+=(s,e)=>{if(!refreshing)Run(()=>BuildLocalResourceEditor());};
                box.KeyDown+=(s,e)=>{if(e.KeyCode==Keys.Enter){e.SuppressKeyPress=true;Run(()=>BuildLocalResourceEditor());}};
                search.Width=320;search.Margin=new Padding(0,0,0,10);AddRow(layout.Inner,search);
            }
            var items=cardFilter.Trim().Length==0?all:all.Where(x=>caption(x).IndexOf(cardFilter.Trim(),StringComparison.CurrentCultureIgnoreCase)>=0||localSpec.DisplayId(x.Id).IndexOf(cardFilter.Trim(),StringComparison.OrdinalIgnoreCase)>=0).ToList();
            // Changed lines are invisible to RMM (the generator's hash covers the donor file too),
            // so this is said once for the whole list instead of being claimed per row.
            if(localSpec.CardState.Length>0)AddText(card,language.T("cards_pending_hint"),9,false,Theme.Muted);
            if(items.Count==0){AddText(card,language.T("cards_no_match"),10,false,Theme.Muted);return;}
            var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=4,Margin=new Padding(0,4,0,8)};
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            bool prune=localSpec.CardState=="buildings_plus"&&CardPrune();
            foreach(LocalResourceItem entry in items)
            {
                LocalResourceItem item=entry;
                if(table.RowCount>0)
                {
                    var line=new Panel{Height=1,BackColor=Theme.Line,Dock=DockStyle.Top,Margin=new Padding(0,6,0,6)};
                    int r=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.SetColumnSpan(line,4);table.Controls.Add(line,0,r);
                }
                Control switchCell=new Panel{Width=0,Height=0,Margin=Padding.Empty};
                if(toggleField!=null)
                {
                    var cell=new TableLayoutPanel{AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Margin=new Padding(0,0,14,0),Anchor=AnchorStyles.None};
                    string on=resourceSession.Value(item.Id,toggleField);
                    // The switch paints itself at a fixed 60 x 32 - anything smaller cuts the knob off.
                    var toggle=new ToggleSwitch{Checked=on.Length==0?toggleField.Default=="1":on=="1",Margin=new Padding(0,0,0,2),AccessibleName="card:toggle:"+localSpec.DisplayId(item.Id)};
                    LocalDetailField captured=toggleField;
                    toggle.CheckedChanged+=(s,e)=>{if(refreshing)return;Run(()=>{resourceSession.SetField(item.Id,captured,toggle.Checked?"1":"0");BuildLocalResourceEditor();});};
                    cell.Controls.Add(toggle,0,0);
                    var caption2=Theme.Label(localSpec.FieldLabel(language,toggleField),8,false);caption2.ForeColor=Theme.Muted;caption2.Margin=Padding.Empty;cell.Controls.Add(caption2,0,1);
                    switchCell=cell;
                }
                var names=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=Padding.Empty};
                names.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));names.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                // The padlock of the list marks an original: one the effective INI already had or
                // one a content package provides. It can be overwritten, but not deleted.
                if(!item.Owned)
                {
                    var mark=new Panel{Size=new Size(20,18),Margin=new Padding(0,1,4,0),BackColor=Color.White,AccessibleName="card:locked:"+localSpec.DisplayId(item.Id)};
                    mark.Paint+=(s,e)=>IconCache.DrawBuiltin(e.Graphics,"lock",new Rectangle(1,0,17,17),Theme.Muted);
                    tips.SetToolTip(mark,language.T("list_original_locked"));names.Controls.Add(mark,0,0);
                }
                var title=Theme.Label(caption(item),10,true);title.Margin=new Padding(0,0,0,2);
                names.Controls.Add(title,1,0);
                var parts=new List<string>();
                foreach(string id in localSpec.CardSubtitle)
                {
                    LocalDetailField field=localSpec.FieldById(id);if(field==null)continue;
                    string value=resourceSession.Value(item.Id,field).Trim();
                    if(value.Length==0&&field.AssignedFrom.Length>0)value=AssignedValue(field,item.Id);
                    if(value.Length==0&&field.Key.Equals("object",StringComparison.OrdinalIgnoreCase))value=localSpec.DisplayId(item.Id);
                    if(value.Length==0)continue;
                    // A lines field would drag the whole row apart: the first entry plus how many follow.
                    if(field.Type=="lines"){string[] rows=value.Split('\n');value=rows[0].Trim()+(rows.Length>1?" (+"+(rows.Length-1)+")":"");}
                    parts.Add(field.Key.Equals("donor",StringComparison.OrdinalIgnoreCase)?language.T("card_donor")+": "+value:value);
                }
                if(parts.Count>0){var detail=Theme.Label(String.Join("  ·  ",parts),9,false);detail.ForeColor=Theme.Muted;names.Controls.Add(detail,1,1);}
                BuildingRow row=localSpec.CardState=="buildings_plus"?CardRowOf(item,group):null;
                BuildingStates.Result state=row==null?null:BuildingStates.Of(resourceSession.Build,row,prune,language);
                var stateLabel=Theme.Label(state==null?"":state.Text,9,false);
                stateLabel.ForeColor=state==null?Theme.Muted:state.Tone=="error"?Color.FromArgb(150,28,28):state.Tone=="warn"?Color.FromArgb(139,81,0):state.Tone=="normal"?Theme.Ink:Theme.Muted;
                stateLabel.Anchor=AnchorStyles.None;stateLabel.Margin=new Padding(12,0,8,0);stateLabel.AccessibleName="card:state:"+localSpec.DisplayId(item.Id);
                var actions=new FlowLayoutPanel{AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Margin=new Padding(8,0,0,0),Anchor=AnchorStyles.None,WrapContents=false};
                if(localSpec.CardAction("edit"))
                {
                    var change=Theme.Button(language.T("card_edit"),()=>Run(()=>OpenCardDialog(item,group)),false);
                    change.AccessibleName="card:edit:"+localSpec.DisplayId(item.Id);change.Margin=new Padding(0,0,8,0);actions.Controls.Add(change);
                }
                if(localSpec.CardAction("open"))
                {
                    string folder=row==null?null:CardFolder(row);
                    bool there=folder!=null&&Directory.Exists(folder);
                    var open=Theme.Button(language.T("path_open"),()=>Run(()=>OpenPath(folder,false)),false);
                    open.Enabled=there;open.Margin=new Padding(0,0,8,0);open.AccessibleName="card:open:"+localSpec.DisplayId(item.Id);
                    tips.SetToolTip(open,there?folder:language.T("card_open_pending"));actions.Controls.Add(open);
                }
                if(localSpec.CardAction("delete"))
                {
                    // An original - one the effective INI already had, or one a content package
                    // brought in - is not deleted but hidden, exactly as the old list did it.
                    var remove=new VectorButton{Symbol="trash",Danger=true,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.15f,Size=new Size(38,34),Margin=Padding.Empty,AccessibleName="card:delete:"+localSpec.DisplayId(item.Id)};
                    remove.Click+=(s,e)=>Run(()=>{if(item.Owned)RemoveCardItem(item,row,prune);else RemoveLocalListItem(item.Id,false);});
                    tips.SetToolTip(remove,language.T(item.Owned?"list_remove":"list_hide"));actions.Controls.Add(remove);
                }
                int at=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                table.Controls.Add(switchCell,0,at);table.Controls.Add(names,1,at);table.Controls.Add(stateLabel,2,at);table.Controls.Add(actions,3,at);
            }
            int last=layout.Inner.RowCount++;layout.Inner.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Dock=DockStyle.Top;layout.Inner.Controls.Add(table,0,last);
        }
        // The bin of a card row. Deleting takes the section out of the INI; the folder under
        // workshop_wip is the plugin's business - it removes it at the next start when "prune"
        // is on. The question says which of the two will happen instead of warning in general.
        void RemoveCardItem(LocalResourceItem item,BuildingRow row,bool prune)
        {
            if(localSpec.CardState!="buildings_plus"||row==null){RemoveLocalListItem(item.Id,item.Owned);return;}
            var lines=new List<string>{language.Format("list_remove_save_warning",localSpec.DisplayId(item.Id))};
            string folder=CardFolder(row);
            if(folder!=null&&Directory.Exists(folder))lines.Add(language.Format(prune?"card_remove_folder_pruned":"card_remove_folder_kept",row.Number));
            if(row.Number.Length>0)lines.Add(language.T("card_remove_number"));
            lines.Add(language.T("card_remove_saves"));
            string message=String.Join("\n\n",lines);
            DialogResult answer=ResourceRemovePrompt!=null?ResourceRemovePrompt(message):Question(message,false);
            if(answer!=DialogResult.OK)return;
            var extras=RemovalExtras(item.Id);foreach(RemovalExtra extra in extras)extra.Apply();
            resourceSession.Remove(item.Id);selectedLocalResource="";BuildLocalResourceEditor();
        }
        // The dialog behind "Aendern...". It carries what the detail panel carried, split into
        // tabs: every item field with card = global on the first one, every other card value on
        // one of its own (label from [card:<id>]). Values go into the draft, so "Uebernehmen"
        // is not a save - the footer says that, and the save button below the window does it.
        internal Func<Form,DialogResult> CardDialogPrompt=null;   // test hook: drives the dialog without a modal loop
        // The + of a card list opens the very same dialog, only empty: one way for adding and
        // changing instead of two. Every field of the dialog writes straight into the session, so
        // the entry has to exist while it is open - it is created under a placeholder name and
        // renamed to what you typed when you hit Apply, or thrown away again on Cancel.
        // A section needs at least one value to exist, so the placeholder starts with the schema
        // defaults; a list whose fields carry none keeps the old small add dialog.
        void OpenNewCardDialog(ItemGroup group)
        {
            if(resourceSession.Items(group).Count>=localSpec.MaximumItemsOf(group))throw new InvalidOperationException(language.Format("list_full",localSpec.MaximumItemsOf(group)));
            var seed=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(LocalDetailField field in localSpec.ItemFields(group))if(field.Default.Length>0&&field.Default!="{name}")seed[field.Id]=field.Default;
            if(seed.Count==0){LocalDetailField toggle=localSpec.FieldById(localSpec.CardToggle);if(toggle!=null)seed[toggle.Id]="1";}
            if(seed.Count==0){OpenLocalSectionDialog(group);return;}
            string prefix=group==null?"":group.Prefix,name="new_entry";
            var used=new HashSet<string>(resourceSession.Items().Select(x=>x.Id).Concat(resourceSession.SuppressedIds()),StringComparer.OrdinalIgnoreCase);
            for(int n=2;used.Contains(prefix+name);n++)name="new_entry_"+n;
            resourceSession.AddSection(name,seed,group);
            LocalResourceItem item=resourceSession.Items(group).FirstOrDefault(x=>x.Id.Equals(prefix+name,StringComparison.OrdinalIgnoreCase));
            if(item==null){resourceSession.Remove(prefix+name);return;}
            OpenCardDialog(item,group,true);
        }
        // Apply of a new entry: take the values off the placeholder, drop it and add the entry
        // under the name that was typed. A rejected name (empty, taken, reserved) puts the
        // placeholder back untouched and the dialog stays open with everything still in it.
        string CommitNewCard(LocalResourceItem item,ItemGroup group,string typed)
        {
            var values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(LocalDetailField field in localSpec.ItemFields(group)){string v=resourceSession.Value(item.Id,field);if(v.Length>0)values[field.Id]=v;}
            string placeholder=localSpec.DisplayId(item.Id);
            resourceSession.Remove(item.Id);
            try{resourceSession.AddSection(typed,values,group);}
            catch{resourceSession.AddSection(placeholder,values,group);throw;}
            return (group==null?"":group.Prefix)+CollectionRules.IdFromName(typed);
        }
        void OpenCardDialog(LocalResourceItem item,ItemGroup group){OpenCardDialog(item,group,false);}
        void OpenCardDialog(LocalResourceItem item,ItemGroup group,bool isNew)
        {
            var fields=localSpec.ItemFields(group).ToList();
            var tabs=new List<string>{"global"};
            foreach(LocalDetailField field in fields)if(!tabs.Any(x=>x.Equals(field.Card,StringComparison.OrdinalIgnoreCase)))tabs.Add(field.Card);
            string caption=localSpec.DisplayId(item.Id);
            LocalDetailField titleField=localSpec.FieldById(localSpec.CardTitle);
            string shown=titleField==null?"":resourceSession.Value(item.Id,titleField).Trim();
            BuildingRow row=localSpec.CardState=="buildings_plus"&&!isNew?CardRowOf(item,group):null;
            string newTitle=localSpec.LocalizedCardNewTitle(language);if(newTitle.Length==0)newTitle=language.T("card_dialog_new");
            var identity=new List<string>{isNew?newTitle:(shown.Length>0?shown:caption)};
            if(row!=null){if(row.Number.Length>0)identity.Add(row.Number);string obj=(row.Object??"").Trim();identity.Add(obj.Length>0?obj:caption);}
            // The typed name lives here: select() rebuilds the tab and disposes its controls, so
            // the box is made fresh each time and only its text is kept.
            string typedName="";
            if(!isNew)RememberCardValues(item,group);
            using(var dialog=new Form{Text=isNew?newTitle:language.Format("card_dialog_title",caption),Font=Font,Size=new Size(1120,780),MinimumSize=new Size(900,620),StartPosition=FormStartPosition.CenterParent,MaximizeBox=true,MinimizeBox=false,ShowInTaskbar=false,Icon=FindForm()==null?null:FindForm().Icon})
            {
                Theme.ApplyWindowChrome(dialog);
                var shell=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=4,Padding=new Padding(20,16,20,14),BackColor=Color.White};
                shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));shell.RowStyles.Add(new RowStyle(SizeType.Absolute,TabStrip.TabHeight+TabStrip.Inset+2));
                shell.RowStyles.Add(new RowStyle(SizeType.Percent,100));shell.RowStyles.Add(new RowStyle(SizeType.Absolute,54));
                var head=Theme.Label(String.Join("  ·  ",identity),15,true);head.Margin=new Padding(0,0,0,10);shell.Controls.Add(head,0,0);
                var strip=new TabStrip{Dock=DockStyle.Fill};shell.Controls.Add(strip,0,1);
                var body=new Panel{Dock=DockStyle.Fill,AutoScroll=true,BackColor=Color.White,Padding=new Padding(0,14,0,0)};shell.Controls.Add(body,0,2);
                var hidden=new ListBox();   // the detail builder refreshes a list; here nobody sees it
                string current=tabs[0];
                var buttons=new Dictionary<string,Button>(StringComparer.OrdinalIgnoreCase);
                Action<string> select=null;
                select=id=>
                {
                    current=id;
                    foreach(var pair in buttons){bool active=pair.Key.Equals(id,StringComparison.OrdinalIgnoreCase);pair.Value.BackColor=active?Theme.SelectionBlue:Theme.Frame;pair.Value.ForeColor=active?Color.White:Theme.Ink;pair.Value.FlatAppearance.MouseOverBackColor=active?Theme.SelectionBlue:Theme.TabHover;}
                    // The strip paints the halo of the active tab itself and has to be told, or
                    // the highlight stays where it was.
                    strip.Invalidate();
                    Theme.SetRedraw(body,false);
                    try
                    {
                        LocalCard card=localSpec.AllCards().FirstOrDefault(x=>x.Id.Equals(id,StringComparison.OrdinalIgnoreCase));
                        bool first=id.Equals("global",StringComparison.OrdinalIgnoreCase);
                        Control idInput=null;
                        if(isNew&&first)
                        {
                            var box=new TextBox{Text=typedName,Height=Fields.Height,AccessibleName="card-dialog:id"};
                            box.TextChanged+=(s2,e2)=>typedName=box.Text;idInput=Fields.Wrap(box);
                        }
                        if(card!=null&&card.Editor=="donor_lines")BuildDonorEditor(body,item,group,card);
                        else if(card!=null&&card.Editor=="target_lines")BuildTargetEditor(body,item,group,card);
                        else BuildLocalSectionDetails(body,hidden,item,f=>f.Card.Equals(id,StringComparison.OrdinalIgnoreCase),false,first,idInput);
                    }
                    finally{Theme.SetRedraw(body,true);}
                };
                foreach(string id in tabs)
                {
                    string captured=id;
                    string label=id.Equals("global",StringComparison.OrdinalIgnoreCase)?language.T("card_tab_details"):localSpec.LocalizedCardLabel(language,localSpec.AllCards().FirstOrDefault(x=>x.Id.Equals(id,StringComparison.OrdinalIgnoreCase)));
                    if(label.Length==0)label=id;
                    var button=Theme.TabButton(label,()=>select(captured));
                    button.Margin=new Padding(0,0,1,0);button.AccessibleName="card-tab:"+id;
                    buttons[id]=button;strip.Controls.Add(button);
                }
                if(tabs.Count<2)strip.Visible=false;
                // Test hook: a window that is never shown has no clickable buttons, so the tab
                // switch is reachable as an action of its own.
                dialog.Tag=select;
                var footer=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2,Margin=new Padding(0,8,0,0)};
                footer.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));footer.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                var note=Theme.Label(language.T("card_dialog_footer"),9,false);note.ForeColor=Theme.Muted;note.Anchor=AnchorStyles.Left;footer.Controls.Add(note,0,0);
                var row2=new FlowLayoutPanel{AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,FlowDirection=FlowDirection.LeftToRight,WrapContents=false,Margin=Padding.Empty};
                // AutoSize has to go, or the two end up different heights: the primary button
                // draws no border and measures one pixel less than the other.
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);
                cancel.AutoSize=false;cancel.Size=new Size(140,Fields.Height);cancel.MinimumSize=Size.Empty;cancel.Margin=new Padding(0,0,8,0);cancel.AccessibleName="card-dialog:cancel";
                // A new entry is only really created here. A rejected name reports and leaves the
                // dialog open - the result is set after the section went in, never before.
                string created="";
                Action commit=()=>{if(isNew){bool ok=false;Run(()=>{created=CommitNewCard(item,group,typedName);ok=true;});if(!ok)return;}dialog.DialogResult=DialogResult.OK;dialog.Close();};
                var apply=Theme.Button(language.T("card_apply"),()=>commit(),true);
                // Test hook: a window that was never shown has no clickable buttons.
                apply.Tag=commit;
                apply.AutoSize=false;apply.Size=new Size(140,Fields.Height);apply.MinimumSize=Size.Empty;apply.Margin=Padding.Empty;apply.AccessibleName="card-dialog:apply";
                row2.Controls.Add(cancel);row2.Controls.Add(apply);footer.Controls.Add(row2,1,0);
                shell.Controls.Add(footer,0,3);
                dialog.Controls.Add(shell);
                // Enter must not close the window - the line editor types into text boxes.
                dialog.AcceptButton=null;dialog.CancelButton=cancel;
                select(tabs[0]);
                DialogResult answer=CardDialogPrompt!=null?CardDialogPrompt(dialog):Theme.Modal(FindForm(),dialog);
                hidden.Dispose();
                // Every field writes into the session as it is edited, so cancelling has to put
                // the entry back the way it was. The session keeps no per-entry undo, so the
                // dialog remembers the values it started with and restores them.
                if(isNew)
                {
                    // Cancelling a new entry throws the placeholder away; there is nothing to put back.
                    if(answer==DialogResult.OK)selectedLocalResource=created;
                    else try{resourceSession.Remove(item.Id);}catch(Exception){}
                }
                else if(answer!=DialogResult.OK)RestoreCardValues(item,group);
            }
            BuildLocalResourceEditor();
        }
        // The lines tab: the donor's building.ini beside your own lines. Everything the editor
        // changes goes straight into the draft, so the dialog's Cancel can put it back like any
        // other field.
        void BuildDonorEditor(Panel body,LocalResourceItem item,ItemGroup group,LocalCard card)
        {
            Theme.DisposeChildren(body);donorEditor=null;
            var fields=localSpec.ItemFields(group).ToList();
            LocalDetailField lineField=localSpec.FieldById(card.EditorLines),stripField=localSpec.FieldById(card.EditorStrip);
            if(lineField==null){BuildLocalSectionDetails(body,new ListBox(),item,f=>f.Card.Equals(card.Id,StringComparison.OrdinalIgnoreCase),false,false);return;}
            LocalDetailField donorField=fields.FirstOrDefault(x=>x.Key.Equals("donor",StringComparison.OrdinalIgnoreCase));
            LocalDetailField nameField=fields.FirstOrDefault(x=>x.Key.Equals("name",StringComparison.OrdinalIgnoreCase));
            string donor=donorField==null?"":resourceSession.Value(item.Id,donorField).Trim();
            // Without a donor there is no file to show beside your lines - say so instead of
            // drawing an empty editor (0.5.8: the + opens this dialog before a donor is picked).
            if(donorField!=null&&donor.Length==0)
            {
                var wait=new NoticeBox("info"){Dock=DockStyle.Top,AutoSize=true,Padding=new Padding(14,12,14,12),Margin=new Padding(0,6,8,0)};
                var text=Theme.Label(language.T("donor_pick_first"),10,false);text.Dock=DockStyle.Top;text.BackColor=wait.Tint;text.MaximumSize=new Size(Math.Max(300,body.ClientSize.Width-80),0);
                wait.Controls.Add(text);body.Controls.Add(wait);return;
            }
            List<string> donorLines=null;
            try{if(donor.Length>0)donorLines=GameBuildings.DonorLines(state.Build,donor);}catch(Exception){}
            var shell=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=2,Margin=Padding.Empty,BackColor=Color.White};
            shell.RowStyles.Add(new RowStyle(SizeType.AutoSize));shell.RowStyles.Add(new RowStyle(SizeType.Percent,100));
            if(donorLines==null)
            {
                // Without the donor there is nothing to compare against - but the lines stay
                // editable: a declaration is written for other machines too.
                var box=new NoticeBox("error");box.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,36));box.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
                var icon=new NoticeIcon{Error=true,BackColor=box.Tint};
                var text=Theme.Label(language.Format("donor_missing",donor.Length>0?donor:"-"),10,false);text.ForeColor=Color.FromArgb(150,28,28);text.BackColor=box.Tint;text.Dock=DockStyle.Top;text.AccessibleName="donor:missing";
                box.Controls.Add(icon,0,0);box.Controls.Add(text,1,0);shell.Controls.Add(box,0,0);
            }
            var editor=new DonorLinesEditor(language,donorLines??new List<string>(),Split(item,lineField),Split(item,stripField),()=>DonorNameLine(item,nameField),Font);
            editor.Maximum=lineField.MaximumLines>0?lineField.MaximumLines:512;
            editor.Changed=()=>Run(()=>
            {
                resourceSession.SetField(item.Id,lineField,String.Join("\n",editor.Lines));
                if(stripField!=null)resourceSession.SetField(item.Id,stripField,String.Join("\n",editor.Strip));
                UpdateStatus();
            });
            shell.Controls.Add(editor,0,1);body.Controls.Add(shell);
            donorEditor=editor;
        }
        DonorLinesEditor donorEditor;
        internal DonorLinesEditor TestDonorEditor{get{return donorEditor;}}
        // The commands tab of a rule set: the chosen target file beside the commands, and what each
        // command does to that file. Unlike a donor clone a rule set can have many targets, and a
        // command that does not fit one of them is rejected for that one alone.
        void BuildTargetEditor(Panel body,LocalResourceItem item,ItemGroup group,LocalCard card)
        {
            Theme.DisposeChildren(body);targetEditor=null;
            LocalDetailField targetField=localSpec.FieldById(card.EditorTargets);
            LocalDetailField addField=localSpec.FieldById(card.EditorAdd),replaceField=localSpec.FieldById(card.EditorReplace);
            LocalDetailField removeField=localSpec.FieldById(card.EditorRemove),insertField=localSpec.FieldById(card.EditorInsert);
            if(targetField==null||addField==null||replaceField==null||removeField==null||insertField==null)
            {BuildLocalSectionDetails(body,new ListBox(),item,f=>f.Card.Equals(card.Id,StringComparison.OrdinalIgnoreCase),false,false);return;}
            var files=Split(item,targetField);
            if(files.Count==0)
            {
                var wait=new NoticeBox("info"){Dock=DockStyle.Top,AutoSize=true,Padding=new Padding(14,12,14,12),Margin=new Padding(0,6,8,0)};
                var text=Theme.Label(language.T("target_pick_first"),10,false);text.Dock=DockStyle.Top;text.BackColor=wait.Tint;text.MaximumSize=new Size(Math.Max(300,body.ClientSize.Width-80),0);
                wait.Controls.Add(text);body.Controls.Add(wait);return;
            }
            LocalDetailField addLink=localSpec.FieldById(card.EditorAddLink),replaceLink=localSpec.FieldById(card.EditorReplaceLink),removeLink=localSpec.FieldById(card.EditorRemoveLink);
            string root=resourceSession.Build;string shop=state.WorkshopRoot;
            var editor=new TargetLinesEditor(language,files,file=>GameBuildings.TargetLines(root,shop,file),
                Split(item,addField),Split(item,replaceField),Split(item,removeField),Split(item,insertField),
                Split(item,addLink),Split(item,replaceLink),Split(item,removeLink),Font);
            editor.Changed=()=>Run(()=>
            {
                resourceSession.SetField(item.Id,addField,String.Join("\n",editor.Add));
                resourceSession.SetField(item.Id,replaceField,String.Join("\n",editor.Replace));
                resourceSession.SetField(item.Id,removeField,String.Join("\n",editor.Remove));
                resourceSession.SetField(item.Id,insertField,String.Join("\n",editor.Insert));
                if(addLink!=null)resourceSession.SetField(item.Id,addLink,String.Join("\n",editor.AddLink));
                if(replaceLink!=null)resourceSession.SetField(item.Id,replaceLink,String.Join("\n",editor.ReplaceLink));
                if(removeLink!=null)resourceSession.SetField(item.Id,removeLink,String.Join("\n",editor.RemoveLink));
                UpdateStatus();
            });
            editor.Dock=DockStyle.Fill;body.Controls.Add(editor);
            targetEditor=editor;
        }
        TargetLinesEditor targetEditor;
        internal TargetLinesEditor TestTargetEditor{get{return targetEditor;}}
        List<string> Split(LocalResourceItem item,LocalDetailField field)
        { return field==null?new List<string>():resourceSession.Value(item.Id,field).Replace("\r\n","\n").Split('\n').Where(x=>x.Trim().Length>0).ToList(); }
        // What the name field will put into the file. A value with dots is a translation key the
        // plugin resolves through Localization - for the effect view it only matters that a
        // $NAME line appears at all, because every $NAME* of the donor goes either way.
        string DonorNameLine(LocalResourceItem item,LocalDetailField field)
        {
            if(field==null)return "";
            string value=resourceSession.Value(item.Id,field).Trim();
            if(value.Length==0)return "";
            return value.Contains(".")&&!value.Contains(" ")?"$NAME <"+value+">":"$NAME_STR \""+value+"\"";
        }
        readonly Dictionary<string,string> cardBefore=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        void RememberCardValues(LocalResourceItem item,ItemGroup group)
        {
            cardBefore.Clear();
            foreach(LocalDetailField field in localSpec.ItemFields(group))cardBefore[field.Id]=resourceSession.Value(item.Id,field);
        }
        void RestoreCardValues(LocalResourceItem item,ItemGroup group)
        {
            foreach(LocalDetailField field in localSpec.ItemFields(group))
            {
                string was;if(!cardBefore.TryGetValue(field.Id,out was))continue;
                if(resourceSession.Value(item.Id,field)==was)continue;
                try{resourceSession.SetField(item.Id,field,was);}catch(Exception){}
            }
        }
        void BuildLocalResourceEmpty(Panel panel){BuildLocalResourceEmpty(panel,null);}
        void BuildLocalResourceEmpty(Panel panel,ItemGroup group)
        {Theme.DisposeChildren(panel);string help=localSpec.LocalizedSelectHelp(language,group);var label=Theme.Label(help.Length>0?help:language.T("resource_select_help"),11,false);label.ForeColor=Theme.Muted;label.Dock=DockStyle.Top;label.MaximumSize=new Size(Math.Max(250,panel.ClientSize.Width-20),0);panel.Controls.Add(label);}
        // Original lines the user hid, each with a button that shows it again.
        void AddHiddenOriginals(TableLayoutPanel inner)
        {
            var hidden=resourceSession.SuppressedIds();if(hidden.Count==0)return;
            var row=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,10,0,0),Padding=Padding.Empty};
            var title=Theme.Label(language.T("list_hidden")+":",10,true);title.Margin=new Padding(0,8,10,0);row.Controls.Add(title);
            foreach(string id in hidden){string captured=id;var button=Theme.Button(captured+"  ·  "+language.T("list_show_again"),()=>Run(()=>{resourceSession.Unsuppress(captured);selectedLocalResource=captured;BuildLocalResourceEditor();}),false);button.Margin=new Padding(0,2,8,2);button.AccessibleName="unhide:"+captured;row.Controls.Add(button);}
            AddRow(inner,row);
        }
        // Plugin-wide values (scope = global) in a card of their own below the list.
        // Links to the package's guides, opened with the file's own application.
        void BuildLocalLinks()
        {
            if(localSpec.Links.Count==0||localSpec.LinksTab!=state.SelectedTab)return;
            string title=localSpec.LocalizedLinksLabel(language);var card=BeginCard(780,title.Length>0?title:language.T("links"));
            var row=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,4,0,0),Padding=Padding.Empty};
            foreach(LocalLink link in localSpec.LinksFor(language))
            {
                LocalLink captured=link;string path;try{path=localSpec.LinkPath(link);}catch(Exception e){Report(e.Message);continue;}
                var button=Theme.Button(localSpec.LocalizedLinkLabel(language,link),()=>Run(()=>{string file=localSpec.LinkPath(captured);if(!File.Exists(file))throw new IOException(language.T("link_missing")+": "+captured.File);Process.Start(new ProcessStartInfo(file){UseShellExecute=true});}),false);
                button.Margin=new Padding(0,2,8,2);button.AccessibleName="link:"+link.Id;button.Enabled=File.Exists(path);tips.SetToolTip(button,path);row.Controls.Add(button);
            }
            AddRow(((CardLayout)card.Tag).Inner,row);
        }
        // Plugin-wide values: the [global] card and every [card:] on the selected tab.
        void BuildLocalGlobals()
        {
            if(!localSpec.HasGlobals&&localSpec.PathRows.Count==0)return;
            LocalDetailField activity=localSpec.ActivityField;
            foreach(LocalCard cardSpec in localSpec.AllCards())
            {
                if(cardSpec.Tab!=state.SelectedTab)continue;
                var fields=localSpec.Fields.Where(x=>x.Scope=="global"&&x!=activity&&x.Card.Equals(cardSpec.Id,StringComparison.OrdinalIgnoreCase)).ToList();
                var rows=localSpec.PathRows.Where(x=>x.Card.Equals(cardSpec.Id,StringComparison.OrdinalIgnoreCase)).ToList();   // 0.4.28
                if(fields.Count==0&&rows.Count==0&&localSpec.LocalizedCardNotice(language,cardSpec).Length==0)continue;
                BuildLocalGlobalCard(cardSpec,fields,rows);
            }
        }
        void BuildLocalGlobalCard(LocalCard cardSpec,List<LocalDetailField> fields,List<LocalPathRow> rows)
        {
            string title=localSpec.LocalizedCardLabel(language,cardSpec);var card=BeginCard(780,title.Length>0?title:localSpec.LocalizedName(language));string help=localSpec.LocalizedCardDescription(language,cardSpec);if(help.Length>0)AddText(card,help,10,false,Theme.Muted);
            string notice=localSpec.LocalizedCardNotice(language,cardSpec);if(notice.Length>0)AddNotice(card,notice,cardSpec.NoticeStyle!="info");
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=new Padding(0,6,0,0)};FluidColumns(grid,46,800);
            foreach(LocalDetailField field in fields)
            {
                LocalDetailField captured=field;string value=resourceSession.GlobalValue(field),baselineValue=resourceSession.GlobalBaseline(field);Control input;
                // A switch without a value shows the schema default, the way the plugin reads a missing key (0.4.76 did this for item fields).
                if(field.Type=="boolean"){var toggle=new ToggleSwitch{Checked=value.Length==0?field.Default=="1":value=="1",AccessibleName="global:"+field.Id};toggle.CheckedChanged+=(s,e)=>{if(refreshing)return;Run(()=>{resourceSession.SetGlobal(captured,toggle.Checked?"1":"0");UpdateStatus();});};input=toggle;}
                else if(field.Type=="choice"){var combo=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="global:"+field.Id};Fields.Tall(combo);var choiceField=field;combo.Items.AddRange(field.Choices.Select(c=>localSpec.ChoiceText(language,choiceField,c)).ToArray());string shownChoice=localSpec.ChoiceText(language,field,value);if(shownChoice.Length>0&&!combo.Items.Contains(shownChoice))combo.Items.Add(shownChoice);combo.SelectedItem=shownChoice.Length>0?shownChoice:null;combo.SelectedIndexChanged+=(s,e)=>{if(combo.Focused)Run(()=>{resourceSession.SetGlobal(captured,localSpec.ChoiceValue(language,choiceField,Convert.ToString(combo.SelectedItem)));UpdateStatus();});};input=combo;}
                else if(field.Type=="integer"||field.Type=="decimal"){var number=new NumberInput(field.Minimum,field.Maximum,field.Step){AccessibleName="global:"+field.Id};number.Input.Text=value;number.Input.TextChanged+=(s,e)=>{if(!number.Input.Focused)return;try{resourceSession.SetGlobal(captured,number.Input.Text);}catch(Exception){}UpdateStatus();};number.Leave+=(s,e)=>Run(()=>{resourceSession.SetGlobal(captured,number.Input.Text);UpdateStatus();});number.Stepped+=(s,e)=>Run(()=>{resourceSession.SetGlobal(captured,number.Input.Text);UpdateStatus();});input=number;}   // 0.4.39
                else{var text=new TextBox{Text=value,Height=Fields.Height,AccessibleName="global:"+field.Id};text.TextChanged+=(s,e)=>{if(!text.Focused)return;try{resourceSession.SetGlobal(captured,text.Text);}catch(Exception){}UpdateStatus();};text.Leave+=(s,e)=>Run(()=>{resourceSession.SetGlobal(captured,text.Text);UpdateStatus();});input=Fields.Wrap(text);}
                RangeTip(input,localSpec.FieldDescription(language,field),field.Type,field.Minimum,field.Maximum);
                bool personal=!value.Equals(baselineValue,StringComparison.Ordinal);string source=baselineValue.Length>0?language.T("list_original_value")+": "+(field.Type=="choice"?localSpec.ChoiceText(language,field,baselineValue):baselineValue):language.T("list_default_value");if(personal)source=language.T("personal")+"  ·  "+source;
                Action reset=personal?(Action)(()=>{resourceSession.SetGlobal(captured,"");BuildLocalResourceEditor();}):null;AddLocalRow(grid,LocalLabel(localSpec.FieldLabel(language,field),localSpec.FieldDescription(language,field)),LocalInput(input,source,reset));
            }
            foreach(LocalPathRow row in rows)AddPathRow(card,grid,row);
            AddRow(((CardLayout)card.Tag).Inner,grid);
        }
        // [textpack] (0.4.30): fallback language, the language files with "+", and the texts of
        // every own research (name / description) plus free keys of the selected language.
        // Edits are staged as dependent writes and land with the next save.
        // The text pack session lives in the workspace (0.4.77), so a parked entry keeps its staged edits.
        TextPackSession textPack {get{return work.TextPack;}set{work.TextPack=value;}} string selectedTextLanguage="";
        void BuildTextPackCard()
        {
            TextPackSpec tp=localSpec.TextPack;string folder=localSpec.ResolvePath(tp.Folder,resourceSession.Build);
            textPack=CurrentTextPack();
            var card=BeginCard(780,localSpec.LocalizedTextPackLabel(language));string help=localSpec.LocalizedTextPackDescription(language);if(help.Length>0)AddText(card,help,10,false,Theme.Muted);
            var inner=((CardLayout)card.Tag).Inner;
            if(!textPack.Exists)
            {
                string missing=localSpec.LocalizedTextPackMissing(language);AddNotice(card,missing.Length>0?missing:language.Format("textpack_missing",folder),true);
                var create=Theme.Button(language.T("textpack_create"),()=>Run(()=>{var written=TextPackSession.Seed(folder,TextPackSeedFolder(tp),tp.Namespace);Report(language.Format("textpack_created",written.Count,folder));textPack=null;BuildLocalResourceEditor();}),false);
                create.AccessibleName="textpack:create";create.Margin=new Padding(0,2,8,8);tips.SetToolTip(create,folder);
                var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,0,0,4),Padding=Padding.Empty};flow.Controls.Add(create);AddRow(inner,flow);
                return;
            }
            // Fallback language from localization.ini.
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=new Padding(0,6,0,0)};FluidColumns(grid,46,800);
            var fallback=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="textpack:fallback"};Fields.Tall(fallback);
            foreach(string lang in textPack.Languages)fallback.Items.Add(lang);if(textPack.Languages.Contains(textPack.Fallback,StringComparer.OrdinalIgnoreCase))fallback.SelectedItem=textPack.Languages.First(x=>x.Equals(textPack.Fallback,StringComparison.OrdinalIgnoreCase));
            fallback.SelectedIndexChanged+=(s,e)=>{if(!fallback.Focused)return;string chosen=(string)fallback.SelectedItem;Later(fallback,()=>Run(()=>{textPack.SetFallback(chosen);StageTextPack();}));};
            // The folder is the tooltip, not the origin line: that line cannot wrap and cut the path off (0.4.33).
            tips.SetToolTip(fallback,folder);
            AddLocalRow(grid,LocalLabel(language.T("textpack_fallback"),language.T("textpack_fallback_help")),LocalInput(Fields.Host(fallback),"",null));
            AddRow(inner,grid);
            // Languages on the left, the texts of the selected language on the right.
            var split=new SplitContainer{Dock=DockStyle.Top,Size=new Size(1000,520),FixedPanel=FixedPanel.Panel1,Panel1MinSize=240,Panel2MinSize=380,SplitterDistance=300,IsSplitterFixed=false,BorderStyle=BorderStyle.FixedSingle,Margin=new Padding(0,10,0,0)};split.Panel1.Padding=new Padding(0);split.Panel2.Padding=new Padding(18,0,0,0);
            var left=new TableLayoutPanel{Dock=DockStyle.Fill,RowCount=3,ColumnCount=1,Padding=new Padding(10),Margin=Padding.Empty};left.RowStyles.Add(new RowStyle(SizeType.AutoSize));left.RowStyles.Add(new RowStyle(SizeType.Percent,100));left.RowStyles.Add(new RowStyle(SizeType.Absolute,50));
            var listTitle=Theme.Label(language.T("textpack_languages"),11,true);listTitle.Margin=new Padding(0,0,0,8);left.Controls.Add(listTitle,0,0);
            var list=new ListBox{Dock=DockStyle.Fill,Font=new Font("Segoe UI",10),BorderStyle=BorderStyle.FixedSingle,AccessibleName="textpack:languages"};foreach(string lang in textPack.Languages)list.Items.Add("soviet"+lang+".ini");left.Controls.Add(list,0,1);
            var addBar=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=2,Margin=Padding.Empty,Padding=new Padding(0,8,0,0)};addBar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));addBar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,44));
            var add=new VectorButton{Symbol="plus",Primary=true,BackColor=Theme.Blue,ForeColor=Color.White,Size=new Size(40,38),GlyphScale=2.5f,Anchor=AnchorStyles.Top|AnchorStyles.Right,AccessibleName=language.T("textpack_add_language")};add.Click+=(s,e)=>Run(OpenTextPackLanguageDialog);tips.SetToolTip(add,add.AccessibleName);addBar.Controls.Add(add,1,0);left.Controls.Add(addBar,0,2);split.Panel1.Controls.Add(left);
            var details=new Panel{Dock=DockStyle.Fill,AutoScroll=true};split.Panel2.Controls.Add(details);narrowTextDetails=false;
            list.SelectedIndexChanged+=(s,e)=>{string chosen=list.SelectedItem as string;if(chosen==null)return;selectedTextLanguage=chosen.Substring(6,chosen.Length-10);BuildTextPackDetails(details,selectedTextLanguage);};
            details.SizeChanged+=(s,e)=>{bool now=Narrow(details);if(now==narrowTextDetails)return;narrowTextDetails=now;if(textPack.HasLanguage(selectedTextLanguage))BuildTextPackDetails(details,selectedTextLanguage);};
            string preset=textPack.Languages.FirstOrDefault(x=>x.Equals(selectedTextLanguage,StringComparison.OrdinalIgnoreCase))??textPack.Languages.FirstOrDefault();
            if(preset!=null)list.SelectedItem="soviet"+preset+".ini";else{var hint=Theme.Label(language.T("textpack_select_help"),11,false);hint.ForeColor=Theme.Muted;hint.Dock=DockStyle.Top;details.Controls.Add(hint);}
            AddRow(inner,split);((CardLayout)card.Tag).Tall=split;
        }
        // 0.5.5: the generated Workshop folders under media_soviet\workshop_wip. Read-only, with
        // one exception: a folder whose config says $OWNER_ID 0 can have the number filled in.
        // That zero is why the game asks about missing Workshop items on every save load, and
        // Soviet Mod Loader's built-in generator writes it into every building it makes.
        void BuildWipCard()
        {
            WipSpec spec=localSpec.Wip;
            var card=BeginCard(780,localSpec.LocalizedWipLabel(language));
            string help=localSpec.LocalizedWipDescription(language);if(help.Length>0)AddText(card,help,9,false,Theme.Muted);
            List<WipBuilding> found=WipBuildings.Scan(resourceSession.Build,spec.Range);
            // Before anything is drawn: a folder the generator rewrote gets the personal changes
            // put back in. That is the promise of storing changes instead of a copy, and this is
            // the moment for it - RMM runs before the game does.
            var reapplied=new List<string>();var stuck=new List<string>();var foreign=new List<string>();
            foreach(WipBuilding candidate in found)
            {
                WipEdit recorded=WipEdits.Load(resourceSession.Build,candidate.Id);
                WipEdits.Sync sync=WipEdits.State(resourceSession.Build,candidate,recorded);
                if(sync==WipEdits.Sync.Foreign){foreign.Add(candidate.Display);continue;}
                if(sync!=WipEdits.Sync.Regenerated)continue;
                try
                {
                    List<string> trouble=WipEdits.Reapply(resourceSession.Build,candidate,recorded);
                    reapplied.Add(candidate.Display);
                    foreach(string anchor in trouble)stuck.Add(candidate.Display+": "+anchor);
                    Report(language.Format("wip_reapplied_one",candidate.Display,recorded.Operations.Count));
                }
                catch(Exception e){stuck.Add(candidate.Display+": "+language.Localize(e.Message));}
            }
            if(found.Count==0)
            {
                string empty=localSpec.LocalizedWipEmpty(language);
                AddText(card,empty.Length>0?empty:language.T("wip_none"),10,false,Theme.Muted);
                return;
            }
            var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=3,Margin=new Padding(0,4,0,8)};
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            foreach(WipBuilding entry in found)
            {
                if(table.RowCount>0)
                {
                    var line=new Panel{Height=1,BackColor=Theme.Line,Dock=DockStyle.Top,Margin=new Padding(0,6,0,6)};
                    int r=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.SetColumnSpan(line,3);table.Controls.Add(line,0,r);
                }
                var names=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Margin=Padding.Empty};
                var title=Theme.Label(entry.Display,10,true);title.Margin=new Padding(0,0,0,2);names.Controls.Add(title,0,0);
                string origin=language.T("wip_origin_"+entry.Origin);
                var detail=Theme.Label(entry.Id+"  ·  "+(entry.Object.Length>0?entry.Object+"  ·  ":"")+origin,9,false);detail.ForeColor=Theme.Muted;names.Controls.Add(detail,0,1);
                // Personal changes get their own line: the state column can only say one thing, and
                // a stale or owner-less folder must not hide the fact that changes of yours are in it.
                WipEdit edit=WipEdits.Load(resourceSession.Build,entry.Id);
                if(edit.Exists)
                {
                    var mine=Theme.Label(language.Format("wip_state_changed",edit.Operations.Count),9,false);
                    mine.ForeColor=Theme.Blue;mine.Margin=new Padding(0,2,0,0);names.Controls.Add(mine,0,2);
                }
                string label=entry.Orphan?language.T("wip_state_orphan"):entry.OwnerMissing?language.T("wip_state_owner_missing"):entry.Owner.Length>0?language.T("wip_state_ok"):language.T("wip_state_no_config");
                var state=Theme.Label(label,9,false);
                state.ForeColor=entry.Orphan?Color.FromArgb(150,28,28):entry.OwnerMissing?Color.FromArgb(139,81,0):Theme.Muted;
                state.Anchor=AnchorStyles.None;state.Margin=new Padding(12,0,8,0);
                var actions=new FlowLayoutPanel{AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Margin=new Padding(8,0,0,0),Anchor=AnchorStyles.None,WrapContents=false};
                WipBuilding captured=entry;
                var change=Theme.Button(language.T("wip_edit_button"),()=>Run(()=>OpenWipEditor(captured)),false);
                change.AccessibleName="wip:edit:"+entry.Id;change.Enabled=WipEdits.BuildingFile(entry)!=null;change.Margin=new Padding(0,0,8,0);
                string folder=entry.Folder;
                var open=Theme.Button(language.T("path_open"),()=>Run(()=>OpenPath(folder,false)),false);
                open.Margin=Padding.Empty;open.AccessibleName="wip:open:"+entry.Id;
                actions.Controls.Add(change);actions.Controls.Add(open);
                int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
                table.Controls.Add(names,0,row);table.Controls.Add(state,1,row);table.Controls.Add(actions,2,row);
            }
            var layout=(CardLayout)card.Tag;int at=layout.Inner.RowCount++;layout.Inner.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Dock=DockStyle.Top;layout.Inner.Controls.Add(table,0,at);
            if(reapplied.Count>0)AddNotice(card,language.Format("wip_reapplied",String.Join(", ",reapplied)),"info");
            if(foreign.Count>0)AddNotice(card,language.Format("wip_foreign",String.Join(", ",foreign)),"warning");
            if(stuck.Count>0)AddNotice(card,language.Format("wip_stuck",String.Join("; ",stuck)),"warning");
            // Stale folders first: this is the one that stops the game from starting at all.
            var orphans=found.Where(x=>x.Orphan).ToList();
            if(orphans.Count>0)AddNotice(card,language.Format("wip_orphan_notice",String.Join(", ",orphans.Select(x=>x.Id+" ("+x.Section+")"))),"error");
            var missing=found.Where(x=>x.OwnerMissing).ToList();
            if(missing.Count==0)return;
            string owner=WipBuildings.Owner();
            if(owner.Length==0){AddNotice(card,language.T("wip_no_steam_id"),"warning");return;}
            AddNotice(card,language.Format("wip_owner_notice",missing.Count),"warning");
            var buttons=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Margin=new Padding(0,4,0,0),Padding=Padding.Empty};
            var fix=Theme.Button(language.T("wip_fix_owner"),()=>Run(()=>
            {
                int done=0;
                foreach(WipBuilding entry in missing)if(WipBuildings.FixOwner(entry.Folder,owner))done++;
                Report(language.Format("wip_owner_written",done,owner));
                BuildLocalResourceEditor();
            }),true);
            fix.AccessibleName="wip:fix-owner";buttons.Controls.Add(fix);
            int last=layout.Inner.RowCount++;layout.Inner.RowStyles.Add(new RowStyle(SizeType.AutoSize));layout.Inner.Controls.Add(buttons,0,last);
        }
        // The editor for one generated building. It writes straight through - the file belongs to
        // the generator, not to the session, so there is nothing to stage and nothing to save
        // afterwards. What RMM keeps is the receipt with the changes and the generator's baseline.
        void OpenWipEditor(WipBuilding entry){RunWipEditor(entry,window=>Theme.Modal(FindForm(),window));}
        void RunWipEditor(WipBuilding entry,Func<WipEditWindow,DialogResult> show)
        {
            string file=WipEdits.BuildingFile(entry);
            if(file==null)throw new IOException(language.Format("wip_no_building_ini",entry.Id));
            WipEdit edit=WipEdits.Load(resourceSession.Build,entry.Id);
            if(!edit.Exists||edit.Baseline.Length==0)edit.Baseline=SafeFiles.Text(file);
            using(var window=new WipEditWindow(language,entry,edit,Font,FindForm()==null?null:FindForm().Icon))
            {
                if(show(window)!=DialogResult.OK)return;
                if(window.Reset)
                {
                    WipEdits.Revert(resourceSession.Build,entry,edit);
                    Report(language.Format("wip_reverted",entry.Display));
                }
                else if(edit.Operations.Count==0)
                {
                    WipEdits.Revert(resourceSession.Build,entry,edit);
                    Report(language.Format("wip_reverted",entry.Display));
                }
                else
                {
                    List<string> trouble=WipEdits.Write(resourceSession.Build,entry,edit);
                    Report(language.Format("wip_written",entry.Display,edit.Operations.Count));
                    if(trouble.Count>0)Report(language.Format("wip_stuck",entry.Display+": "+String.Join("; ",trouble)));
                }
            }
            BuildLocalResourceEditor();
        }
        // Test hook: the window without a modal loop, driven by the test and then applied.
        internal void TestWipEdit(string id,Action<WipEditWindow> drive)
        {
            WipBuilding entry=WipBuildings.Scan(resourceSession.Build,localSpec.Wip.Range).First(x=>x.Id==id);
            RunWipEditor(entry,window=>{drive(window);return DialogResult.OK;});
        }
        void StageTextPack(){resourceSession.StageDependencyWrites(textPack.Writes());UpdateStatus();}
        // Before a save (0.4.77): every own entry of the text pack's key group gets its missing
        // name/desc keys filled with the id in the fallback language, so the plugin never refuses
        // the whole expansion over an entry saved without texts.
        void FillTextPackKeys()
        {
            if(localSpec==null||resourceSession==null||localSpec.TextPack==null||localSpec.TextPack.KeysFrom.Length==0)return;
            TextPackSession pack=CurrentTextPack();if(pack==null||!pack.Exists)return;
            ItemGroup group=localSpec.GroupById(localSpec.TextPack.KeysFrom);if(group==null)return;
            var own=resourceSession.Items(group).Where(i=>i.Owned).Select(i=>localSpec.DisplayId(i.Id)).ToList();
            string lang=pack.HasLanguage(pack.Fallback)?pack.Fallback:pack.Languages.FirstOrDefault();if(lang==null||own.Count==0)return;
            int added=pack.EnsureKeys(own,lang);
            if(added>0){resourceSession.StageDependencyWrites(pack.Writes());Report(language.Format("textpack_filled",added,lang));}
        }
        // The text pack session of the schema, loaded on demand and reloaded after a save or reset.
        TextPackSession CurrentTextPack()
        {
            if(localSpec.TextPack==null)return null;string folder=localSpec.ResolvePath(localSpec.TextPack.Folder,resourceSession.Build);
            if(textPack==null||!textPack.Folder.Equals(Path.GetFullPath(folder),StringComparison.OrdinalIgnoreCase)||textPack.Generation!=resourceSession.Generation)textPack=new TextPackSession(folder,resourceSession.Generation);
            return textPack;
        }
        string TextPackSeedFolder(TextPackSpec tp)
        {
            if(tp.SeedDependency.Length==0||session==null)return null;
            Dependency d=session.Package.Dependencies.FirstOrDefault(x=>x.Id.Equals(tp.SeedDependency,StringComparison.OrdinalIgnoreCase));
            if(d==null||String.IsNullOrEmpty(d.Root))return null;
            try{return SafeFiles.Child(d.Root,tp.SeedPath);}catch(IOException){return null;}
        }
        // The texts of one language: name and description per own research first, then every
        // other key of the pack, then a "+" for a free key. Blank = the fallback applies.
        void BuildTextPackDetails(Panel panel,string lang)
        {
            Theme.DisposeChildren(panel);var shell=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Padding=new Padding(0,0,8,18),Margin=Padding.Empty};shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));panel.Controls.Add(shell);
            var title=Theme.Label("soviet"+lang+".ini",15,true);title.Margin=new Padding(0,0,0,4);AddRow(shell,title);
            var path=Theme.Label(textPack.LanguageFile(lang),9,false);path.ForeColor=Theme.Muted;path.Margin=new Padding(0,0,0,12);AddRow(shell,path);
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=Padding.Empty};FluidColumns(grid,46,800);grid.Tag=Narrow(panel)?"narrow":null;   // 0.4.36
            ItemGroup group=localSpec.GroupById(localSpec.TextPack.KeysFrom);var own=resourceSession.Items(group).Select(i=>localSpec.DisplayId(i.Id)).ToList();
            var covered=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if(own.Count>0)AddLocalHeading(grid,language.T("textpack_research_texts"));else{var none=Theme.Label(language.T("textpack_no_research"),9,false);none.ForeColor=Theme.Muted;none.Margin=new Padding(0,0,0,10);AddRow(shell,none);}
            foreach(string id in own)
            {
                foreach(string suffix in new[]{".name",".desc"})
                {
                    string key=id+suffix;covered.Add(key);
                    AddLocalRow(grid,LocalLabel(id+"  ·  "+language.T(suffix==".name"?"textpack_name":"textpack_desc"),key),LocalInput(TextPackInput(lang,key),textPack.Get(lang,key).Length>0?language.T("personal"):language.T("textpack_blank"),null));
                }
            }
            var others=textPack.Keys().Where(k=>!covered.Contains(k)).ToList();
            if(others.Count>0)
            {
                AddLocalHeading(grid,language.T("textpack_other_keys"));
                // Keys no research of the INI owns (0.4.34): a note and a trash button that removes the key from every language file.
                int noteRow=grid.RowCount++;grid.RowStyles.Add(new RowStyle(SizeType.AutoSize));var note=Theme.Label(language.T("textpack_other_keys_help"),9,false);note.ForeColor=Theme.Muted;note.Margin=new Padding(0,0,0,10);note.Dock=DockStyle.Top;grid.Controls.Add(note,0,noteRow);grid.SetColumnSpan(note,2);
                foreach(string key in others)
                {
                    string captured=key;
                    var row=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=Padding.Empty};row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                    var input=LocalInput(TextPackInput(lang,key),textPack.Get(lang,key).Length>0?language.T("personal"):language.T("textpack_blank"),null);input.Dock=DockStyle.Fill;
                    var remove=new VectorButton{Symbol="trash",Danger=true,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.2f,Size=new Size(38,34),Margin=new Padding(8,2,0,0),Anchor=AnchorStyles.Top,AccessibleName="textpack:remove:"+key};tips.SetToolTip(remove,language.T("textpack_remove_key_button"));
                    remove.Click+=(s,e)=>Run(()=>{if(Question(language.Format("textpack_remove_key",captured),false)!=DialogResult.OK)return;textPack.RemoveKey(captured);StageTextPack();BuildLocalResourceEditor();});
                    row.Controls.Add(input,0,0);row.Controls.Add(remove,1,0);
                    AddLocalRow(grid,LocalLabel(key,""),row);
                }
            }
            AddRow(shell,grid);
            var addKey=Theme.Button(language.T("textpack_add_key"),()=>Run(()=>OpenTextPackKeyDialog(lang)),false);addKey.AccessibleName="textpack:add-key";addKey.Margin=new Padding(0,6,0,0);
            var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=Padding.Empty,Padding=Padding.Empty};flow.Controls.Add(addKey);AddRow(shell,flow);
        }
        Control TextPackInput(string lang,string key)
        {
            var text=new TextBox{Text=textPack.Get(lang,key),Height=Fields.Height,AccessibleName="textpack:"+lang+":"+key};
            text.Leave+=(s,e)=>Run(()=>{if(text.Text.Trim()==textPack.Get(lang,key))return;textPack.Set(lang,key,text.Text);StageTextPack();});
            tips.SetToolTip(text,key);return Fields.Wrap(text);
        }
        void OpenTextPackLanguageDialog()
        {
            string game=GameBuildings.GameRoot(state.Build);var names=game!=null?GameTexts.Languages(game):new List<string>();
            var free=names.Where(n=>!textPack.HasLanguage(n)).ToList();if(free.Count==0)throw new InvalidOperationException(language.T("textpack_all_languages"));
            using(var dialog=new Form{Text=language.T("textpack_add_language_title"),Font=Font,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,MinimumSize=new Size(520,0),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,RowCount=2,Padding=new Padding(20)};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,150));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
                var combo=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="textpack-language"};Fields.Tall(combo);foreach(string n in free)combo.Items.Add(n);combo.SelectedIndex=0;
                table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(Theme.Label(language.T("textpack_language"),10,true),0,0);combo.Dock=DockStyle.Top;combo.Margin=new Padding(3,3,3,18);table.Controls.Add(combo,1,0);
                var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
                var ok=Theme.Button(language.T("add"),()=>Run(()=>{string chosen=(string)combo.SelectedItem;textPack.AddLanguage(chosen);selectedTextLanguage=chosen;StageTextPack();dialog.DialogResult=DialogResult.OK;dialog.Close();}),true);
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(ok);buttons.Controls.Add(cancel);buttons.Dock=DockStyle.Top;buttons.AutoSize=true;table.SetColumnSpan(buttons,2);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(buttons,0,1);dialog.Controls.Add(table);dialog.AcceptButton=ok;dialog.CancelButton=cancel;
                if(Theme.Modal(this,dialog)==DialogResult.OK)BuildLocalResourceEditor();
            }
        }
        void OpenTextPackKeyDialog(string lang)
        {
            using(var dialog=new Form{Text=language.T("textpack_add_key"),Font=Font,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,MinimumSize=new Size(640,0),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,RowCount=3,Padding=new Padding(20)};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,150));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
                var key=new TextBox{Height=Fields.Height,AccessibleName="textpack-key"};var value=new TextBox{Height=Fields.Height,AccessibleName="textpack-value"};
                Action<string,Control> place=(label,control)=>{table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(Theme.Label(label,10,true),0,table.RowStyles.Count-1);control.Dock=DockStyle.Top;control.Margin=new Padding(3,3,3,18);table.Controls.Add(control,1,table.RowStyles.Count-1);};
                place(language.T("textpack_key"),Fields.Wrap(key));place(language.T("textpack_value"),Fields.Wrap(value));
                var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
                var ok=Theme.Button(language.T("add"),()=>Run(()=>{textPack.Set(lang,key.Text,value.Text);StageTextPack();dialog.DialogResult=DialogResult.OK;dialog.Close();}),true);
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(ok);buttons.Controls.Add(cancel);buttons.Dock=DockStyle.Top;buttons.AutoSize=true;table.SetColumnSpan(buttons,2);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(buttons,0,2);dialog.Controls.Add(table);dialog.AcceptButton=ok;dialog.CancelButton=cancel;
                if(Theme.Modal(this,dialog)==DialogResult.OK)BuildLocalResourceEditor();
            }
        }
        // [folder:] / [file:] rows (0.4.28): a missing folder shows its notice and, with create = 1,
        // a button that creates it; an existing folder and the first existing file candidate get a
        // read-only path box with "Open" and "Refresh". Nothing here touches the INI or the receipt.
        void AddPathRow(Card card,TableLayoutPanel grid,LocalPathRow row)
        {
            string build=resourceSession.Build;
            if(row.Kind=="folder")
            {
                string path=localSpec.ResolvePath(row.Candidates[0].Value,build);
                if(!Directory.Exists(path))
                {
                    string missing=localSpec.LocalizedPathMissing(language,row);AddNotice(card,missing.Length>0?missing:language.Format("path_folder_missing",path),true);
                    if(row.Create)
                    {
                        var create=Theme.Button(language.T("path_create"),()=>Run(()=>{Directory.CreateDirectory(path);Report(language.Format("path_created",path));BuildLocalResourceEditor();}),false);
                        create.AccessibleName="path:create:"+row.Id;create.Margin=new Padding(0,2,8,8);tips.SetToolTip(create,path);
                        var flow=new FlowLayoutPanel{Dock=DockStyle.Top,AutoSize=true,WrapContents=true,Margin=new Padding(0,0,0,4),Padding=Padding.Empty};flow.Controls.Add(create);AddRow(((CardLayout)card.Tag).Inner,flow);
                    }
                    return;
                }
                AddLocalRow(grid,LocalLabel(localSpec.LocalizedPathLabel(language,row),localSpec.LocalizedPathDescription(language,row)),PathInput(row,path,path,()=>OpenPath(path,false)));
                return;
            }
            string found=null,tag="";var all=new List<string>();
            foreach(var candidate in row.Candidates){string p=localSpec.ResolvePath(candidate.Value,build);all.Add((candidate.Key.Length>0?PathTag(candidate.Key)+": ":"")+p);if(found==null&&File.Exists(p)){found=p;tag=candidate.Key;}}
            string shown=found==null?language.T("path_missing"):(tag.Length>0?PathTag(tag):Path.GetFileName(found));string chosen=found;
            AddLocalRow(grid,LocalLabel(localSpec.LocalizedPathLabel(language,row),localSpec.LocalizedPathDescription(language,row)),PathInput(row,shown,String.Join("\n",all),found==null?null:(Action)(()=>OpenPath(chosen,true))));
        }
        string PathTag(string tag){return language.T("path_tag_"+tag);}
        static void OpenPath(string path,bool select){Process.Start(new ProcessStartInfo("explorer.exe",(select?"/select,":"")+"\""+path+"\""){UseShellExecute=true});}
        // Read-only box that fills the row, the two buttons keep their own width (never stretched).
        Panel PathInput(LocalPathRow row,string text,string tooltip,Action open)
        {
            var panel=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=3,Margin=new Padding(0,2,4,12)};
            panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var box=new TextBox{Text=text,ReadOnly=true,Height=Fields.Height,AccessibleName="path:"+row.Id};var wrapped=Fields.Wrap(box);wrapped.Dock=DockStyle.Fill;wrapped.Margin=new Padding(0);tips.SetToolTip(box,tooltip);
            var openButton=Theme.Button(language.T("path_open"),()=>Run(()=>{if(open==null)throw new IOException(language.T("path_missing"));open();}),false);openButton.Enabled=open!=null;openButton.Anchor=AnchorStyles.None;openButton.Margin=new Padding(8,0,0,0);openButton.AccessibleName="path:open:"+row.Id;tips.SetToolTip(openButton,tooltip);
            var refresh=Theme.Button(language.T("path_refresh"),()=>Run(()=>BuildLocalResourceEditor()),false);refresh.Anchor=AnchorStyles.None;refresh.Margin=new Padding(8,0,0,0);refresh.AccessibleName="path:refresh:"+row.Id;
            panel.Controls.Add(wrapped,0,0);panel.Controls.Add(openButton,1,0);panel.Controls.Add(refresh,2,0);return panel;
        }
        // "1.0" and "1" are the same figure; only a real difference counts as personal.
        static bool Same(ListColumn column,string a,string b){try{return column.Normalize(a)==column.Normalize(b);}catch(FormatException){return a==b;}}
        // One input per column of a keyed_list line; every change rewrites the line.
        // Tooltip (0.4.9): the description plus, for numbers, the allowed range - on every input of
        // the list/section editors, like the Presentation editor since 0.4.4.
        void RangeTip(Control input,string help,string type,decimal minimum,decimal maximum)
        {
            if(type=="decimal"||type=="integer")help=(help.Length>0?help+"\n":"")+language.T("input_range")+": "+minimum+" – "+maximum;
            if(help.Length>0){tips.SetToolTip(input,help);var inner=Fields.Inner(input);if(inner!=null)tips.SetToolTip(inner,help);}
        }
        // A ComboBox handler must never tear down its own control while Windows is still inside the
        // CBN_SELCHANGE message (AccessViolation in comctl32, seen with the template picker of the
        // Resources editor); the work runs once the message has returned.
        // After a deferred rebuild the page gets one more layout pass, because a rebuild that starts from a
        // posted message has been seen to leave the content area collapsed until the window was resized.
        void Later(Control control,Action action){Control owner=control.FindForm();if(owner!=null&&owner.IsHandleCreated){owner.BeginInvoke(action);owner.BeginInvoke(new Action(()=>{if(content.IsDisposed)return;content.PerformLayout();ResizeCards();content.Invalidate(true);}));}else action();}
        // apply: the final value when the field is left (may rebuild the editor); stage: every typed
        // change while the field has focus (session and footer only, never a rebuild), so a typed value
        // counts as a change right away instead of only after clicking elsewhere.
        Control ColumnInput(ListColumn column,string value,Action<string> apply,Action<string> stage=null)
        {Control input=ColumnInputCore(column,value,apply,stage);RangeTip(input,localSpec.ColumnDescription(language,column),column.Type,column.Minimum,column.Maximum);return input;}
        Control ColumnInputCore(ListColumn column,string value,Action<string> apply,Action<string> stage)
        {
            if(column.Type=="choice")
            {
                var combo=new ComboBox{DropDownStyle=column.AllowOther?ComboBoxStyle.DropDown:ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="column:"+column.Id};combo.Items.AddRange(column.Choices);
                if(value.Length>0&&!combo.Items.Contains(value)){if(column.AllowOther)combo.Text=value;else{combo.Items.Add(value);combo.SelectedItem=value;}}else combo.SelectedItem=value.Length>0?value:null;
                combo.SelectedIndexChanged+=(s,e)=>{if(!combo.Focused)return;string chosen=Convert.ToString(combo.SelectedItem);Later(combo,()=>apply(chosen));};if(column.AllowOther)combo.Leave+=(s,e)=>apply(combo.Text);return Fields.Host(combo);
            }
            if(column.Type=="integer"||column.Type=="decimal")
            {
                // +/- buttons like the presentation schemas (0.4.39); the value applies when the field is left.
                var number=new NumberInput(column.Minimum,column.Maximum,column.Step){AccessibleName="column:"+column.Id};number.Input.Text=value;if(stage!=null)number.Input.TextChanged+=(s,e)=>{if(number.Input.Focused)stage(number.Input.Text);};number.Leave+=(s,e)=>apply(number.Input.Text);number.Stepped+=(s,e)=>apply(number.Input.Text);return number;
            }
            var text=new TextBox{Text=value,Height=Fields.Height,AccessibleName="column:"+column.Id};if(stage!=null)text.TextChanged+=(s,e)=>{if(text.Focused)stage(text.Text);};text.Leave+=(s,e)=>apply(text.Text);return Fields.Wrap(text);
        }
        void BuildLocalListDetails(Panel panel,ListBox list,LocalResourceItem item)
        {
            Theme.DisposeChildren(panel);var shell=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Padding=new Padding(0,0,8,18),Margin=Padding.Empty};shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));panel.Controls.Add(shell);
            var titleRow=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,0,0,12)};titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,48));var title=Theme.Label(item.Id,15,true);titleRow.Controls.Add(title,0,0);
            var remove=new VectorButton{Symbol="trash",Danger=true,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.2f,Size=new Size(38,34),AccessibleName=language.T(item.Owned?"list_remove":"list_hide")};remove.Click+=(s,e)=>Run(()=>RemoveLocalListItem(item.Id,item.Owned));tips.SetToolTip(remove,remove.AccessibleName);remove.Visible=!ForeignEntry(item.Id);titleRow.Controls.Add(remove,1,0);
            AddRow(shell,titleRow);var origin=Theme.Label(item.Owned?language.T("list_personal"):OriginText(item.Id,"list_original_locked"),9,false);origin.ForeColor=item.Owned?Color.FromArgb(26,132,61):Theme.Muted;origin.MaximumSize=new Size(Math.Max(300,panel.ClientSize.Width-40),0);origin.Margin=new Padding(0,0,0,15);AddRow(shell,origin);
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=Padding.Empty};FluidColumns(grid,46,800);grid.Tag=Narrow(panel)?"narrow":null;   // 0.4.36
            string idLabel=localSpec.LocalizedItemIdLabel(language),idHelp=localSpec.LocalizedItemIdHelp(language);
            var idValue=new Label{Text=item.Id,AutoSize=false,Height=Fields.Height,Font=Fields.Font,BackColor=Theme.Pale,BorderStyle=BorderStyle.FixedSingle,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(5,0,5,0)};AddLocalRow(grid,LocalLabel(idLabel.Length>0?idLabel:language.T("resource_identifier"),idHelp.Length>0?idHelp:language.T("resource_identifier_help")),LocalInput(idValue,item.Owned?language.T("personal"):language.T("standard"),null));
            string[] current=localSpec.TupleColumns(item.ListValue),original=item.Owned?null:localSpec.TupleColumns(resourceSession.OriginalListValue(item.Id));
            for(int i=0;i<localSpec.Columns.Count;i++)
            {
                int index=i;ListColumn column=localSpec.Columns[i];
                string shown=current[i];Panel row=null;string back=original==null?null:original[index];
                Func<string,string> describe=v=>item.Owned?language.T("personal"):((!Same(column,v,back)?language.T("personal")+"  ·  ":"")+language.T("list_original_value")+": "+(back.Length>0?back:column.Default));
                Action<string> apply=value=>{bool same=false;try{same=(value??"").Trim()==shown||column.Normalize(value??"")==column.Normalize(shown);}catch(FormatException){}if(same)return;Later(content,()=>Run(()=>{string[] parts=localSpec.TupleColumns(resourceSession.Items().First(x=>x.Id.Equals(item.Id,StringComparison.OrdinalIgnoreCase)).ListValue);parts[index]=value;resourceSession.SetListRaw(item.Id,ListTuple.Render(parts));shown=value;UpdateRow(row,describe(value),!item.Owned&&!Same(column,value,back));if(detailsList!=null&&!detailsList.IsDisposed)detailsList.Refresh();UpdateStatus();}));};
                Action<string> stage=value=>{try{string[] parts=localSpec.TupleColumns(resourceSession.Items().First(x=>x.Id.Equals(item.Id,StringComparison.OrdinalIgnoreCase)).ListValue);if((value??"").Trim()==parts[index])return;parts[index]=value;resourceSession.SetListRaw(item.Id,ListTuple.Render(parts));}catch(Exception){}UpdateStatus();};
                Control input=ColumnInput(column,current[i],apply,stage);
                string heading=localSpec.ColumnHeading(language,column);if(heading.Length>0)AddLocalHeading(grid,heading);
                Action reset=item.Owned?null:(Action)(()=>{string[] parts=localSpec.TupleColumns(resourceSession.Items().First(x=>x.Id.Equals(item.Id,StringComparison.OrdinalIgnoreCase)).ListValue);parts[index]=back;resourceSession.SetListRaw(item.Id,ListTuple.Render(parts));RefreshLocalDetails(item.Id);});
                row=LocalInput(input,describe(current[i]),reset,!item.Owned&&!Same(column,current[i],back));
                AddLocalRow(grid,LocalLabel(localSpec.ColumnLabel(language,column),localSpec.ColumnDescription(language,column)),row);
            }
            LockForeign(panel,shell,grid,item.Id);
            AddRow(shell,grid);list.Refresh();
        }
        // The add dialog: a resource from the source plugin's list plus every column,
        // prefilled with the column defaults.
        // Snapshot hook (--window add): opens the add dialog of the current list editor.
        // Presentation packages open the add dialog of their first collection (0.4.43).
        // Test hooks: rebuild the editor the deferred way a dropdown does, and read the card widths afterwards.
        public void TestDeferredRebuild(){Control combo=FirstCombo(content);if(combo!=null&&combo.CanFocus)combo.Focus();Later(content,()=>Run(BuildLocalResourceEditor));}
        static Control FirstCombo(Control root){foreach(Control c in root.Controls){if(c is ComboBox)return c;Control d=FirstCombo(c);if(d!=null)return d;}return null;}
        public string TestContentSize{get{return content.ClientSize.Width+"x"+content.ClientSize.Height+" scroll "+content.AutoScrollPosition;}}
        public int[] TestCardWidths(){return content.Controls.Cast<Control>().Where(c=>c.Tag is CardLayout).Select(c=>c.Width).ToArray();}
        public int TestContentWidth{get{return content.ClientSize.Width;}}
        public void TestOpenAddDialog(){if(localSpec==null){CollectionSpec first=presentation==null?null:presentation.Collections.FirstOrDefault();if(first!=null)OpenResourceDialog(first);return;}if(localSpec.IsSections)OpenLocalSectionDialog(null);else if(localSpec.IsList)OpenLocalListDialog();else OpenLocalResourceDialog();}
        void OpenLocalListDialog()
        {
            if(resourceSession.Items().Count>=localSpec.MaximumItems)throw new InvalidOperationException(language.Format("list_full",localSpec.MaximumItems));
            // With a [source] the identifier is picked from that plugin's list; without one
            // (0.34.0) it is typed, with the base game's resource names and, when present,
            // the Resources list as suggestions - a grit material of Technical Service Storage.
            ResourceRegistry registry=null;
            if(localSpec.SourcePlugin.Length>0){registry=ResourceRegistry.Load(state.Build,localSpec.SourcePlugin,localSpec.SourceSection,localSpec.SourceReadySection,localSpec.SourceReadyKey,localSpec.SourceReadyValue);if(!registry.Ready)throw new IOException(registry.Problem);}
            var used=new HashSet<string>(resourceSession.Items().Select(x=>x.Id).Concat(resourceSession.SuppressedIds()),StringComparer.OrdinalIgnoreCase);
            int rows=localSpec.Columns.Count+2;
            // 0.4.8: rows grow with the help texts (same label + help as the detail pane); without
            // a [source] the identifier picker is typed AND grouped (Resources list, then base game).
            using(var dialog=new Form{Text=localSpec.LocalizedAddLabel(language),Font=Font,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,MinimumSize=new Size(700,0),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,RowCount=rows,Padding=new Padding(20)};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,270));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
                ComboBox picker=null;TextBox typed=null;
                if(registry!=null){var plain=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="list-resource"};Fields.Tall(plain);foreach(var option in registry.Options)if(!used.Contains(option.Id))plain.Items.Add(option);if(plain.Items.Count>0)plain.SelectedIndex=0;picker=plain;}
                else if(localSpec.ItemIdPicker=="game_texts"){typed=new TextBox{AccessibleName="list-resource"};}   // 0.4.21: a typed id plus the game-text picker
                else{var grouped=new GroupedPicker{Editable=true,Height=Fields.Height,AccessibleName="list-resource"};if(localSpec.ItemIdSuggestions)foreach(var group in RegistryGroups()){var names=group.Value.Where(x=>!used.Contains(x)).ToList();if(names.Count==0)continue;grouped.AddHeader(language.T(group.Key));foreach(string name in names)grouped.AddChoice(name,name);}picker=grouped;}
                string idLabel=localSpec.LocalizedItemIdLabel(language),idHelp=localSpec.LocalizedAddItemIdHelp(language);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(LocalLabel(idLabel.Length>0?idLabel:language.T("resource_identifier"),idHelp.Length>0?idHelp:language.T("resource_identifier_help")),0,0);
                Control pickerHost=typed!=null?(Control)Fields.Wrap(typed):Fields.Host(picker);pickerHost.Dock=DockStyle.Top;pickerHost.Margin=new Padding(0,4,0,0);
                if(typed!=null)
                {
                    // Field plus "Choose text..." in one row; the picker lists the captions of the game language and writes the chosen id into the field.
                    var idRow=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=Padding.Empty};idRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));idRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                    pickerHost.Margin=new Padding(0,4,8,0);idRow.Controls.Add(pickerHost,0,0);
                    var usedIds=used.Select(x=>{int v;return Int32.TryParse(x,out v)?v:-1;}).Where(v=>v>0).ToList();
                    var choose=Theme.Button(language.T("text_picker_button"),()=>{using(var window=new GameTextPickerWindow(language,state.Build,usedIds,Font,Icon)){if(window.ShowDialog(dialog)==DialogResult.OK&&window.Result.HasValue)typed.Text=window.Result.Value.ToString();}},false);
                    choose.AutoSize=false;choose.Height=Fields.Height;choose.Font=Font;choose.Width=TextRenderer.MeasureText(choose.Text,Font).Width+36;choose.Margin=new Padding(0,4,0,0);choose.AccessibleName="text-picker";idRow.Controls.Add(choose,1,0);
                    table.Controls.Add(idRow,1,0);
                }
                else table.Controls.Add(pickerHost,1,0);
                var inputs=new List<Control>();for(int i=0;i<localSpec.Columns.Count;i++){ListColumn column=localSpec.Columns[i];Control input=ColumnInput(column,column.Default,v=>{});input.Dock=DockStyle.Top;input.Margin=new Padding(0,4,0,0);inputs.Add(input);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(LocalLabel(localSpec.ColumnLabel(language,column),localSpec.ColumnDescription(language,column)),0,i+1);table.Controls.Add(input,1,i+1);}
                var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
                var add=Theme.Button(language.T("add"),()=>Run(()=>{string id;if(registry!=null){var option=picker.SelectedItem as ResourceOption;if(option==null)throw new InvalidOperationException(language.T("list_no_options"));id=option.Id;}else id=(typed!=null?typed.Text:picker.Text).Trim();resourceSession.AddRaw(id,ListTuple.Render(inputs.Select(x=>x.Text)));selectedLocalResource=id;dialog.DialogResult=DialogResult.OK;dialog.Close();}),true);add.Enabled=registry==null||picker.Items.Count>0;
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(add);buttons.Controls.Add(cancel);buttons.Dock=DockStyle.Top;buttons.AutoSize=true;table.SetColumnSpan(buttons,2);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(buttons,0,rows-1);dialog.Controls.Add(table);dialog.AcceptButton=add;dialog.CancelButton=cancel;
                if(registry!=null&&picker.Items.Count==0)Report(language.T("list_no_options"));
                if(Theme.Modal(this,dialog)==DialogResult.OK)BuildLocalResourceEditor();
            }
        }
        // ---- keyed_sections (one INI section per item) ----
        // Names the source plugin registers plus the base game's own, for
        // choices_source = registry fields (the minimap icon of a deposit).
        List<string> RegistryNames(){return RegistryGroups().SelectMany(g=>g.Value).ToList();}
        // Grouped suggestions (0.4.8): the Resources list ("custom", from [source] or, without
        // one, plugins\resources.ini [list] once its hook is armed) and the base game's names.
        // The key of each group is an app string.
        List<KeyValuePair<string,List<string>>> RegistryGroups()
        {
            var custom=new List<string>();
            try
            {
                ResourceRegistry registry=localSpec.SourcePlugin.Length>0
                    ?ResourceRegistry.Load(state.Build,localSpec.SourcePlugin,localSpec.SourceSection,localSpec.SourceReadySection,localSpec.SourceReadyKey,localSpec.SourceReadyValue)
                    :ResourceRegistry.Load(state.Build,"resources","list","resources","hook","2");
                if(registry.Ready)custom.AddRange(registry.Options.Select(x=>x.Id));
            }
            catch(Exception){}
            var vanilla=ResourceCatalogData.Templates.Select(t=>t.Name).Where(n=>!custom.Contains(n,StringComparer.OrdinalIgnoreCase)).ToList();
            var groups=new List<KeyValuePair<string,List<string>>>();
            if(custom.Count>0)groups.Add(new KeyValuePair<string,List<string>>("resources_custom",custom));
            groups.Add(new KeyValuePair<string,List<string>>("resources_vanilla",vanilla));
            return groups;
        }
        // Box heights the user dragged open, per item and field, for this session.
        readonly Dictionary<string,int> linesHeights=new Dictionary<string,int>(StringComparer.OrdinalIgnoreCase);
        // Targets other enabled rule sets already use (the plugin rejects duplicates).
        Dictionary<string,string> TargetsUsedElsewhere(LocalDetailField field,string itemId)
        {
            var used=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            LocalDetailField enabled=localSpec.Fields.FirstOrDefault(x=>x.Scope=="item"&&x.Key.Equals("enabled",StringComparison.OrdinalIgnoreCase));
            foreach(var item in resourceSession.Items())
            {
                if(itemId!=null&&item.Id.Equals(itemId,StringComparison.OrdinalIgnoreCase))continue;
                if(enabled!=null&&resourceSession.Value(item.Id,enabled)=="0")continue;
                foreach(string line in resourceSession.Value(item.Id,field).Split('\n'))if(line.Trim().Length>0&&!used.ContainsKey(line.Trim()))used[line.Trim()]=item.Id;
            }
            return used;
        }
        // The files a picker = files field offers, grouped: the first existing picker folder's own files
        // under the folder's name, then each subfolder (any depth) under its relative path (0.4.48).
        List<KeyValuePair<string,List<string>>> PickerFileGroups(LocalDetailField field)
        {
            var groups=new List<KeyValuePair<string,List<string>>>();
            foreach(string spec in field.PickerFolders)
            {
                string root;try{root=localSpec.ResolvePath(spec,state.Build);}catch(FormatException){continue;}
                if(!Directory.Exists(root))continue;
                var dirs=new List<string>{root};dirs.AddRange(Directory.GetDirectories(root,"*",SearchOption.AllDirectories).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase));
                foreach(string dir in dirs)
                {
                    string rel=dir.Length>root.Length?dir.Substring(root.Length+1).Replace('\\','/'):"";
                    var files=Directory.GetFiles(dir,field.PickerPattern.Length>0?field.PickerPattern:"*").Select(f=>(rel.Length>0?rel+"/":"")+Path.GetFileName(f)).OrderBy(x=>x,StringComparer.OrdinalIgnoreCase).ToList();
                    if(files.Count>0)groups.Add(new KeyValuePair<string,List<string>>(rel.Length>0?rel:Path.GetFileName(root),files));
                }
                break;   // the folder the plugin reads from wins; the others are fallbacks
            }
            return groups;
        }
        // 0.4.80: the donor an entry names, for the line picker of that entry.
        string DonorOf(string itemId)
        {
            LocalDetailField donor=localSpec.ItemFields(localSpec.GroupOfId(itemId)).FirstOrDefault(x=>x.Picker=="game_donor");
            return donor==null?"":resourceSession.Value(itemId,donor).Trim();
        }
        // 0.4.80: what the plugin assigned to this entry, read from the INI named by assigned_from.
        string AssignedValue(LocalDetailField field,string itemId)
        {
            try
            {
                string path=localSpec.ResolvePath(field.AssignedFrom,state.Build);
                if(!File.Exists(path))return "";
                return (new LooseIni(SafeFiles.Text(path)).Get(field.AssignedSection,localSpec.DisplayId(itemId))??"").Trim();
            }
            catch(Exception){return "";}
        }
        Control ItemFieldInput(LocalDetailField field,string value,Action<string> apply,string itemId=null,Action<string> stage=null)
        {Control input=ItemFieldInputCore(field,value,apply,itemId,stage);RangeTip(input,localSpec.FieldDescription(language,field),field.Type,field.Minimum,field.Maximum);return input;}
        Control ItemFieldInputCore(LocalDetailField field,string value,Action<string> apply,string itemId=null,Action<string> stage=null)
        {
            if(field.Type=="lines")
            {
                // One INI line per row; the plugin repeats the key for every row.
                LocalDetailField captured=field;bool buildings=field.Picker=="game_buildings",research=field.Picker=="game_research",lineList=field.Picker=="research_lines",donorLines=field.Picker=="donor_lines",picker=buildings||research||lineList||donorLines;
                var box=new LinesBox(picker,language.T(research?"pick_research":lineList||donorLines?"pick_line":"pick_buildings"),language.T("lines_expand")){MaximumLines=field.MaximumLines,AccessibleName="item:"+field.Id};
                string countLabel=localSpec.FieldCountLabel(language,field);box.CountText=n=>(countLabel.Length>0?countLabel:language.T("lines_count"))+": "+n;
                box.Text=value.Replace("\n","\r\n");
                string heightKey=(itemId??"")+"/"+field.Id;int remembered;if(linesHeights.TryGetValue(heightKey,out remembered))box.BoxHeight=remembered;
                box.Box.SizeChanged+=(s,e)=>{linesHeights[heightKey]=box.BoxHeight;};
                if(stage!=null)box.Box.TextChanged+=(s,e)=>{if(box.Box.Focused)stage(box.Text);};
                box.Box.Leave+=(s,e)=>apply(box.Text);
                if(research)box.Pick=()=>Run(()=>
                {
                    // One requires line per pick: "<parent>" or "<parent> | before | <anchor>" (0.4.29).
                    // The player's own research of every extra group can be a parent too.
                    var own=resourceSession.Items().Where(i=>localSpec.GroupOfId(i.Id)!=null).Select(i=>localSpec.DisplayId(i.Id)).ToList();
                    using(var window=new ResearchPickerWindow(language,state.Build,own,Font,Icon))
                        if(Theme.Modal(this,window)==DialogResult.OK&&!String.IsNullOrEmpty(window.Result)){string existing=box.Text.Trim();box.Text=(existing.Length>0?existing+"\r\n":"")+window.Result;apply(box.Text);}
                });
                else if(lineList)box.Pick=()=>Run(()=>
                {
                    // 0.4.42: a line of the entry's Vanilla research block, shaped by picker_format.
                    if(itemId==null)throw new InvalidOperationException(language.T("research_lines_no_entry"));
                    var own=resourceSession.Items().Where(i=>localSpec.GroupOfId(i.Id)!=null).Select(i=>localSpec.DisplayId(i.Id)).ToList();string display=localSpec.DisplayId(itemId);
                    using(var window=new ResearchLinesWindow(language,state.Build,display,FindResearch(display),captured.PickerFormat,own,Font,Icon))
                        if(Theme.Modal(this,window)==DialogResult.OK&&!String.IsNullOrEmpty(window.Result)){string existing=box.Text.Trim();box.Text=(existing.Length>0?existing+"\r\n":"")+window.Result;apply(box.Text);}
                });
                else if(donorLines)box.Pick=()=>Run(()=>
                {
                    // 0.4.80: a line out of the donor's building.ini - the donor comes from the entry's own donor field.
                    if(itemId==null)throw new InvalidOperationException(language.T("donor_lines_no_entry"));
                    string donor=DonorOf(itemId);
                    if(donor.Length==0)throw new InvalidOperationException(language.T("donor_lines_no_donor"));
                    using(var window=new DonorLinesWindow(language,state.Build,donor,Font,Icon))
                        if(Theme.Modal(this,window)==DialogResult.OK&&!String.IsNullOrEmpty(window.Result)){string existing=box.Text.Trim();box.Text=(existing.Length>0?existing+"\r\n":"")+window.Result;apply(box.Text);}
                });
                else if(picker)box.Pick=()=>Run(()=>
                {
                    var current=box.Text.Replace("\r\n","\n").Split('\n').Select(x=>x.Trim()).Where(x=>x.Length>0).ToList();
                    using(var window=new BuildingPickerWindow(language,state.Build,state.WorkshopRoot,current,itemId!=null?TargetsUsedElsewhere(captured,itemId):null,Font,Icon))
                        if(Theme.Modal(this,window)==DialogResult.OK&&window.Result!=null){box.Text=String.Join("\r\n",window.Result);apply(box.Text);}
                });
                return box;
            }
            // 0.4.76: a switch without a value shows the schema default (default = 1 -> ON), the way the plugin reads a missing key.
            if(field.Type=="boolean"){var toggle=new ToggleSwitch{Checked=value.Length==0?field.Default=="1":value=="1",AccessibleName="item:"+field.Id};toggle.CheckedChanged+=(s,e)=>{if(!refreshing)apply(toggle.Checked?"1":"0");};return toggle;}
            if(field.Type=="choice")
            {
                var combo=new ComboBox{DropDownStyle=field.AllowOther?ComboBoxStyle.DropDown:ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="item:"+field.Id};if(!field.AllowOther)combo.Items.Add("");
                // choice_labels only on plain lists: free text and registry names stay what they are.
                bool labelled=!field.AllowOther&&field.ChoicesSource.Length==0;var choiceField=field;
                combo.Items.AddRange(labelled?field.Choices.Select(c=>localSpec.ChoiceText(language,choiceField,c)).ToArray():field.Choices);
                if(field.ChoicesSource=="registry")foreach(string name in RegistryNames())if(!combo.Items.Contains(name))combo.Items.Add(name);
                string shownChoice=labelled?localSpec.ChoiceText(language,field,value):value;
                if(shownChoice.Length>0&&!combo.Items.Contains(shownChoice)){if(field.AllowOther)combo.Text=shownChoice;else{combo.Items.Add(shownChoice);combo.SelectedItem=shownChoice;}}else combo.SelectedItem=shownChoice.Length>0?shownChoice:(field.AllowOther?null:"");
                combo.SelectedIndexChanged+=(s,e)=>{if(!combo.Focused)return;string chosen=Convert.ToString(combo.SelectedItem);if(labelled)chosen=localSpec.ChoiceValue(language,choiceField,chosen);Later(combo,()=>apply(chosen));};if(field.AllowOther)combo.Leave+=(s,e)=>apply(combo.Text);return Fields.Host(combo);
            }
            if(field.Type=="integer"||field.Type=="decimal")
            {
                // +/- buttons like the presentation schemas (0.4.39); the value applies when the field is left.
                var number=new NumberInput(field.Minimum,field.Maximum,field.Step){AccessibleName="item:"+field.Id};number.Input.Text=value;if(stage!=null)number.Input.TextChanged+=(s,e)=>{if(number.Input.Focused)stage(number.Input.Text);};number.Leave+=(s,e)=>apply(number.Input.Text);number.Stepped+=(s,e)=>apply(number.Input.Text);return number;
            }
            if(field.Picker=="files")
            {
                // 0.4.48: a grouped file list per picker folder (its files first, then every subfolder), typing allowed.
                var picker=new GroupedPicker{Editable=true,Height=Fields.Height,AccessibleName="item:"+field.Id};
                foreach(var group in PickerFileGroups(field)){picker.AddHeader(group.Key);foreach(string file in group.Value)picker.AddChoice(file,file);}
                // The chosen value is applied after the selection message has finished: apply() rebuilds the
                // details and disposes this editable combo, which comctl32 still writes to right after
                // CBN_SELCHANGE (access violation in the 0.4.48 first cut).
                picker.Text=value;picker.Leave+=(s,e)=>apply(picker.Text);picker.SelectedIndexChanged+=(s,e)=>{if(!picker.Focused||picker.Value.Length==0)return;string chosen=picker.Value;Control owner=picker.FindForm();if(owner!=null&&owner.IsHandleCreated)owner.BeginInvoke(new Action(()=>apply(chosen)));else apply(chosen);};
                return Fields.Host(picker);
            }
            var text=new TextBox{Text=value,Height=Fields.Height,AccessibleName="item:"+field.Id};if(stage!=null)text.TextChanged+=(s,e)=>{if(text.Focused)stage(text.Text);};text.Leave+=(s,e)=>apply(text.Text);
            // 0.4.80: the plugin assigns the value itself when the field stays empty - show what it
            // assigned as the placeholder, so the number of an existing entry is never a mystery.
            if(field.AssignedFrom.Length>0&&itemId!=null){string assigned=AssignedValue(field,itemId);if(assigned.Length>0)Cue.Set(text,language.Format("assigned_value",assigned));}
            // 0.4.80: picker = game_donor - the building window restricted to the base game's
            // buildings_types, single choice, giving back the plain name a clone needs.
            if(field.Picker=="game_donor")
            {
                var choose=Theme.Button(language.T("pick_donor"),()=>Run(()=>
                {
                    using(var window=new BuildingPickerWindow(language,state.Build,state.WorkshopRoot,new[]{text.Text.Trim()},null,Font,Icon,true))
                        if(Theme.Modal(this,window)==DialogResult.OK&&window.Result!=null&&window.Result.Count>0){text.Text=window.Result[0];apply(text.Text);}
                }),false);
                choose.AccessibleName="item:"+field.Id+":pick";
                return new PickRow(text,choose);
            }
            if(field.Suffix.Length==0)return Fields.Wrap(text);
            // suffix = .name (0.4.31): the fixed tail of a key in a locked box, the item id as placeholder
            // - "[quartz_smasher].[name]" - so only the middle part is ever typed.
            if(itemId!=null)Cue.Set(text,localSpec.DisplayId(itemId));
            var row=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=3,Margin=Padding.Empty};row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));row.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
            var wrapped=Fields.Wrap(text);wrapped.Dock=DockStyle.Fill;wrapped.Margin=new Padding(0);
            var dot=Theme.Label(".",12,true);dot.Anchor=AnchorStyles.None;dot.Margin=new Padding(4,0,4,0);
            var tail=new TextBox{Text=field.Suffix.TrimStart('.'),ReadOnly=true,Enabled=false,Height=Fields.Height,AccessibleName="item:"+field.Id+":suffix",TextAlign=HorizontalAlignment.Center};
            var tailBox=Fields.Wrap(tail);tailBox.Margin=new Padding(0);tailBox.Width=Math.Max(64,TextRenderer.MeasureText(tail.Text,Fields.Font).Width+24);tailBox.Anchor=AnchorStyles.Top;
            row.Controls.Add(wrapped,0,0);row.Controls.Add(dot,1,0);row.Controls.Add(tailBox,2,0);return row;
        }
        void BuildLocalSectionDetails(Panel panel,ListBox list,LocalResourceItem item){BuildLocalSectionDetails(panel,list,item,null,true,true);}
        // 0.5.8: `only` limits the fields to one dialog tab, `header` draws the title row with the
        // bin and `idRow` the locked identifier - the card list has the first two in the row, and
        // the identifier belongs on the dialog's first tab only.
        void BuildLocalSectionDetails(Panel panel,ListBox list,LocalResourceItem item,Func<LocalDetailField,bool> only,bool header,bool idRow){BuildLocalSectionDetails(panel,list,item,only,header,idRow,null);}
        // 0.5.8: idInput replaces the locked identifier with a box you can type in - that is how the
        // + opens this dialog for an entry that does not exist yet. The name is applied at the end,
        // because a section cannot be renamed once it is in the file.
        void BuildLocalSectionDetails(Panel panel,ListBox list,LocalResourceItem item,Func<LocalDetailField,bool> only,bool header,bool idRow,Control idInput)
        {
            Theme.DisposeChildren(panel);var shell=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Padding=new Padding(0,0,8,18),Margin=Padding.Empty};shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));panel.Controls.Add(shell);
            if(header)
            {
                var titleRow=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,0,0,12)};titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,48));var title=Theme.Label(localSpec.DisplayId(item.Id),15,true);titleRow.Controls.Add(title,0,0);
                var remove=new VectorButton{Symbol="trash",Danger=true,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.2f,Size=new Size(38,34),AccessibleName=language.T(item.Owned?"list_remove":"list_hide")};remove.Click+=(s,e)=>Run(()=>RemoveLocalListItem(item.Id,item.Owned));tips.SetToolTip(remove,remove.AccessibleName);remove.Visible=!ForeignEntry(item.Id);titleRow.Controls.Add(remove,1,0);
                AddRow(shell,titleRow);var origin=Theme.Label(item.Owned?language.T("list_personal"):OriginText(item.Id,"list_original_locked"),9,false);origin.ForeColor=item.Owned?Color.FromArgb(26,132,61):Theme.Muted;origin.MaximumSize=new Size(Math.Max(300,panel.ClientSize.Width-40),0);origin.Margin=new Padding(0,0,0,15);AddRow(shell,origin);
            }
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=Padding.Empty};FluidColumns(grid,46,800);grid.Tag=Narrow(panel)?"narrow":null;   // 0.4.36
            ItemGroup group=localSpec.GroupOfId(item.Id);   // 0.4.29
            string idLabel=localSpec.LocalizedItemIdLabel(language,group),idHelp=localSpec.LocalizedItemIdHelp(language,group);
            var idValue=new Label{Text=localSpec.DisplayId(item.Id),AutoSize=false,Height=Fields.Height,Font=Fields.Font,BackColor=Theme.Pale,BorderStyle=BorderStyle.FixedSingle,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(5,0,5,0)};
            // Field order (0.4.35): fields with position = above_id first, then the id row, then the
            // remaining fields and the [picture:] rows merged by order.
            var fields=localSpec.ItemFields(group).Where(x=>only==null||only(x)).ToList();
            foreach(LocalDetailField field in fields.Where(x=>x.AboveId))AddItemFieldRow(grid,item,field);
            if(idRow&&idInput!=null)
            {
                string nameLabel=localSpec.LocalizedNameLabel(language,group),hint=localSpec.LocalizedNewHint(language,group);
                AddLocalRow(grid,LocalLabel(nameLabel.Length>0?nameLabel:(idLabel.Length>0?idLabel:language.T("resource_identifier")),idHelp),LocalInput(idInput,hint,null));
            }
            else if(idRow)AddLocalRow(grid,LocalLabel(idLabel.Length>0?idLabel:language.T("resource_identifier"),idHelp.Length>0?idHelp:language.T("resource_identifier_help")),LocalInput(idValue,item.Owned?language.T("personal"):language.T("standard"),null));
            var sequence=fields.Where(x=>!x.AboveId).Select(f=>new{Order=f.Order,Field=f,Picture=(LocalPictureRow)null,Block=(LocalBlockRow)null}).Concat((only==null||idRow?localSpec.PicturesFor(group):Enumerable.Empty<LocalPictureRow>()).Select(p=>new{Order=p.Order,Field=(LocalDetailField)null,Picture=p,Block=(LocalBlockRow)null})).Concat((only==null||idRow?localSpec.BlocksFor(group):Enumerable.Empty<LocalBlockRow>()).Select(b=>new{Order=b.Order,Field=(LocalDetailField)null,Picture=(LocalPictureRow)null,Block=b})).OrderBy(x=>x.Order).ToList();
            foreach(var entry in sequence){if(entry.Field!=null)AddItemFieldRow(grid,item,entry.Field);else if(entry.Picture!=null)AddPictureRow(grid,entry.Picture,localSpec.DisplayId(item.Id));else AddBlockRow(grid,entry.Block,localSpec.DisplayId(item.Id));}
            LockForeign(panel,shell,grid,item.Id);
            AddRow(shell,grid);list.Refresh();
        }
        // The add dialog: resource from the source plugin's list, the section name and
        // token proposed from it, then only the fields marked dialog = 1.
        void OpenLocalSectionDialog(ItemGroup group)
        {
            if(resourceSession.Items(group).Count>=localSpec.MaximumItemsOf(group))throw new InvalidOperationException(language.Format("list_full",localSpec.MaximumItemsOf(group)));
            // Without a [source] the dialog asks for the name only (plus the dialog fields).
            ResourceRegistry registry=null;
            if(localSpec.SourcePlugin.Length>0&&group==null){registry=ResourceRegistry.Load(state.Build,localSpec.SourcePlugin,localSpec.SourceSection,localSpec.SourceReadySection,localSpec.SourceReadyKey,localSpec.SourceReadyValue);if(!registry.Ready)throw new IOException(registry.Problem);}
            LocalDetailField tokenField=group!=null?null:localSpec.ItemFields(null).FirstOrDefault(x=>x.Key.Equals(localSpec.TokenKey,StringComparison.OrdinalIgnoreCase));var dialogFields=localSpec.ItemFields(group).Where(x=>x.InDialog&&x!=tokenField).ToList();
            var used=new HashSet<string>(resourceSession.Items().Select(x=>x.Id).Concat(resourceSession.SuppressedIds()),StringComparer.OrdinalIgnoreCase);
            int rows=dialogFields.Count+(registry!=null?1:0)+(tokenField!=null?2:1)+2;
            // 0.4.43: the dialog and every row grow with their content (no fixed 58-px rows, nothing clipped).
            using(var dialog=new Form{Text=localSpec.LocalizedAddLabel(language,group),Font=Font,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,MinimumSize=new Size(660,0),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,RowCount=rows,Padding=new Padding(20)};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,200));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));int row=0;
                Action<string,Control> place=(label,control)=>{table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(Theme.Label(label,10,true),0,row);control.Dock=DockStyle.Top;control.Margin=new Padding(3,3,3,18);table.Controls.Add(control,1,row++);};
                var picker=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height,AccessibleName="section-resource"};Fields.Tall(picker);if(registry!=null)foreach(var option in registry.Options)if(!used.Contains(option.Id))picker.Items.Add(option);
                string idLabel=localSpec.LocalizedItemIdLabel(language,group);if(registry!=null)place(idLabel.Length>0?idLabel:language.T("resource_identifier"),picker);
                var name=new TextBox{Height=Fields.Height,AccessibleName="section-name"};Control nameHost=Fields.Wrap(name);
                if(localSpec.IdPickerOf(group)=="game_research")
                {
                    // 0.4.42: "Choose research..." fills the id with a Vanilla research; used ids and own research are hidden.
                    var nameRow=new TableLayoutPanel{AutoSize=true,ColumnCount=2,Margin=Padding.Empty};nameRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));nameRow.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
                    nameHost.Dock=DockStyle.Top;nameHost.Margin=new Padding(0,0,8,0);nameRow.Controls.Add(nameHost,0,0);
                    var exclude=used.Concat(resourceSession.Items().Where(i=>localSpec.GroupOfId(i.Id)!=null).Select(i=>localSpec.DisplayId(i.Id))).ToList();
                    var choose=Theme.Button(language.T("pick_research"),()=>{using(var window=new ResearchPickerWindow(language,state.Build,null,Font,Icon,true,exclude)){if(window.ShowDialog(dialog)==DialogResult.OK&&!String.IsNullOrEmpty(window.Result))name.Text=window.Result;}},false);
                    choose.AutoSize=false;choose.MinimumSize=Size.Empty;choose.Height=Fields.Height;choose.Font=Font;choose.Width=TextRenderer.MeasureText(choose.Text,Font).Width+36;choose.Margin=Padding.Empty;choose.AccessibleName="research-id-picker";nameRow.Controls.Add(choose,1,0);
                    nameHost=nameRow;
                }
                place(localSpec.LocalizedNameLabel(language,group),nameHost);
                TextBox token=null;if(tokenField!=null){token=new TextBox{Height=Fields.Height,AccessibleName="section-token"};place(localSpec.FieldLabel(language,tokenField),Fields.Wrap(token));}
                var inputs=new Dictionary<LocalDetailField,Control>();
                foreach(LocalDetailField field in dialogFields){string preset=field.AutoIncrement?resourceSession.NextValue(field):field.Default=="{name}"?"":field.Default;Control input=ItemFieldInput(field,preset,v=>{});inputs[field]=input;place(localSpec.FieldLabel(language,field),input);}
                // Choosing a resource proposes name, token, icon and brush; all stay editable.
                Action propose=()=>{var option=picker.SelectedItem as ResourceOption;if(option==null)return;name.Text=option.Id;if(token!=null)token.Text=localSpec.ProposedToken(option.Id);foreach(var pair in inputs){if(pair.Key.ChoicesSource=="registry"){var combo=Fields.ComboOf(pair.Value);if(combo!=null)combo.Text=option.Id;}else if(pair.Key.Default=="{name}"){pair.Value.Text=option.Id;}}};
                picker.SelectedIndexChanged+=(s,e)=>propose();name.TextChanged+=(s,e)=>{if(token!=null&&name.Focused)token.Text=localSpec.ProposedToken(name.Text);};
                if(picker.Items.Count>0){picker.SelectedIndex=0;propose();}
                string hint=localSpec.LocalizedNewHint(language,group);var hintLabel=Theme.Label(hint,9,false);hintLabel.ForeColor=Theme.Muted;hintLabel.MaximumSize=new Size(590,0);hintLabel.AutoSize=true;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.SetColumnSpan(hintLabel,2);table.Controls.Add(hintLabel,0,row++);
                var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
                var add=Theme.Button(language.T("add"),()=>Run(()=>{var values=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);if(tokenField!=null)values[tokenField.Id]=token.Text;foreach(var pair in inputs){var toggle=pair.Value as ToggleSwitch;values[pair.Key.Id]=toggle!=null?(toggle.Checked?"1":"0"):pair.Key.Type=="choice"&&!pair.Key.AllowOther&&pair.Key.ChoicesSource.Length==0?localSpec.ChoiceValue(language,pair.Key,pair.Value.Text):pair.Value.Text;}resourceSession.AddSection(name.Text,values,group);selectedLocalResource=(group==null?"":group.Prefix)+CollectionRules.IdFromName(name.Text);dialog.DialogResult=DialogResult.OK;dialog.Close();}),true);
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(add);buttons.Controls.Add(cancel);buttons.Dock=DockStyle.Top;buttons.AutoSize=true;table.SetColumnSpan(buttons,2);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(buttons,0,row++);dialog.Controls.Add(table);dialog.AcceptButton=add;dialog.CancelButton=cancel;
                if(registry!=null&&picker.Items.Count==0)Report(language.T("list_no_options"));
                if(Theme.Modal(this,dialog)==DialogResult.OK)BuildLocalResourceEditor();
            }
        }
        // One field of the detail panel (moved out of BuildLocalSectionDetails in 0.4.35).
        void AddItemFieldRow(TableLayoutPanel grid,LocalResourceItem item,LocalDetailField field)
        {
            LocalDetailField captured=field;string value=resourceSession.Value(item.Id,field),baselineValue=resourceSession.BaselineValue(item.Id,field);
            // Compare the normalised text: a lines box hands back CRLF while the store keeps LF, so a
            // plain comparison rebuilt the editor on every focus change of a multi-line field (0.4.26).
            Func<string,bool> isPersonal=v=>item.Owned?v.Length>0:!v.Equals(baselineValue,StringComparison.Ordinal);
            string emptyHint=localSpec.FieldEmptyHint(language,field);
            Func<string,string> describe=v=>{bool personal=isPersonal(v);string source=baselineValue.Length>0?language.T("list_original_value")+": "+(field.Type=="choice"&&!field.AllowOther&&field.ChoicesSource.Length==0?localSpec.ChoiceText(language,field,baselineValue):baselineValue):language.T("resource_no_entry");if(personal)source=language.T("personal")+"  ·  "+source;
                // A personal entry has no original to compare with: "Persönlich" when the field is set, nothing when it is empty (0.4.37).
                if(item.Owned)source=personal?language.T("personal"):"";
                // Lines fields carry their own counter; the original lines would only be cut off after a few entries (0.4.25).
                if(field.Type=="lines")source=personal?language.T("personal"):"";
                // A switch shows its state itself; no "no entry" line under it (0.4.32).
                if(field.Type=="boolean")source=personal?language.T("personal"):"";
                // 0.4.85: an empty field with an empty_hint says what empty means instead of showing nothing.
                if(v.Trim().Length==0&&emptyHint.Length>0)source=language.T("field_default")+": "+emptyHint;return source;};
            Func<string,bool> hinted=v=>v.Trim().Length==0&&emptyHint.Length>0;
            Panel row=null;
            // apply (field left): only when the text differs from what the row shows, and only after the focus
            // change has finished (Later). The row's note and reset button are updated in place; nothing is
            // redrawn, so the detail area neither flashes nor jumps. Rebuilding inside Leave destroyed the
            // control that was about to receive the focus and left the page collapsed.
            Action<string> apply=v=>{bool same=false;try{same=captured.Normalize(v??"")==captured.Normalize(value);}catch(FormatException){}if(same)return;Later(content,()=>Run(()=>{resourceSession.SetField(item.Id,captured,v);string now=resourceSession.Value(item.Id,captured);value=now;UpdateRow(row,describe(now),isPersonal(now),hinted(now));if(detailsList!=null&&!detailsList.IsDisposed)detailsList.Refresh();UpdateStatus();}));};
            Action<string> stage=v=>{try{if(captured.Normalize(v??"")==resourceSession.Value(item.Id,captured))return;resourceSession.SetField(item.Id,captured,v);}catch(Exception){}UpdateStatus();};
            Control input=ItemFieldInput(field,value,apply,item.Id,stage);
            string heading=localSpec.FieldHeading(language,field);if(heading.Length>0)AddLocalHeading(grid,heading);
            Action reset=()=>{resourceSession.SetField(item.Id,captured,"");RefreshLocalDetails(item.Id);};
            row=LocalInput(input,describe(value),reset,isPersonal(value),hinted(value));
            AddLocalRow(grid,LocalLabel(localSpec.FieldLabel(language,field),localSpec.FieldDescription(language,field)),row);
        }
        // [picture:<id>] rows (0.4.35): preview of the entry's picture plus "Insert picture...", which
        // copies a chosen PNG into the folder under the entry's name (and checks the size).
        string PicturePath(LocalPictureRow picture,string display){return SafeFiles.Child(localSpec.ResolvePath(picture.Folder,resourceSession.Build),picture.File.Replace("{id}",display));}
        // The game's research tree, read once per game folder and UI language (0.4.42): names for
        // the list of edited research and the block rows of the detail panel.
        readonly Dictionary<string,List<ResearchEntry>> researchCache=new Dictionary<string,List<ResearchEntry>>(StringComparer.OrdinalIgnoreCase);
        List<ResearchEntry> GameResearchEntries()
        {
            string game=GameBuildings.GameRoot(state.Build);if(game==null)return new List<ResearchEntry>();
            string key=game+"|"+language.Code;List<ResearchEntry> cached;
            if(!researchCache.TryGetValue(key,out cached)){try{cached=GameResearch.Scan(game,GameTexts.DefaultLanguage(game,language.Code));}catch(Exception){cached=new List<ResearchEntry>();}researchCache[key]=cached;}
            return cached;
        }
        ResearchEntry FindResearch(string id){return GameResearchEntries().FirstOrDefault(x=>x.Id.Equals(id,StringComparison.OrdinalIgnoreCase));}
        // [research_block:] (0.4.42): the entry's Vanilla research block, read-only, so the edit
        // commands can be written against the lines the game really has.
        void AddBlockRow(TableLayoutPanel grid,LocalBlockRow block,string display)
        {
            ResearchEntry entry=FindResearch(display);
            var box=new TextBox{Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,WordWrap=false,Font=new Font("Consolas",9.5f),BackColor=Theme.Pale,AccessibleName="block:"+block.Id};
            box.Text=entry==null?language.T("research_block_missing"):String.Join("\r\n",entry.Lines);
            int count=entry==null?1:entry.Lines.Count;box.Height=Math.Min(300,Math.Max(60,count*17+14));
            AddLocalRow(grid,LocalLabel(localSpec.LocalizedBlockLabel(language,block),localSpec.LocalizedBlockDescription(language,block)),LocalInput(box,entry!=null?entry.Name:"",null));
        }
        void AddPictureRow(TableLayoutPanel grid,LocalPictureRow picture,string display)
        {
            string path=PicturePath(picture,display);bool exists=File.Exists(path);int side=picture.Size>0?picture.Size:128;
            var panel=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,2,4,12)};panel.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));panel.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            var box=new PictureBox{Size=new Size(side,side),BorderStyle=BorderStyle.FixedSingle,SizeMode=PictureBoxSizeMode.Zoom,BackColor=Theme.Pale,Margin=Padding.Empty,AccessibleName="picture:"+picture.Id};
            if(exists){try{using(var stream=new MemoryStream(File.ReadAllBytes(path)))using(var loaded=Image.FromStream(stream))box.Image=new Bitmap(loaded);}catch(Exception){box.Image=null;}}
            tips.SetToolTip(box,path);panel.Controls.Add(box,0,0);
            var column=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,WrapContents=false,AutoSize=true,Margin=new Padding(12,0,0,0),Padding=Padding.Empty};
            var note=Theme.Label(exists?path:language.T("picture_missing"),9,false);note.ForeColor=Theme.Muted;note.AutoSize=true;note.MaximumSize=new Size(460,0);note.Margin=new Padding(0,0,0,8);column.Controls.Add(note);column.SizeChanged+=(s,e)=>note.MaximumSize=new Size(Math.Max(160,column.ClientSize.Width-8),0);
            var buttons=new FlowLayoutPanel{AutoSize=true,WrapContents=false,Margin=Padding.Empty,Padding=Padding.Empty};
            var insert=Theme.Button(language.T("picture_insert"),()=>Run(()=>InsertPicture(picture,display)),false);insert.AccessibleName="picture:insert:"+picture.Id;insert.Margin=Padding.Empty;buttons.Controls.Add(insert);
            if(exists){var open=Theme.Button(language.T("path_open"),()=>Run(()=>OpenPath(path,true)),false);open.Margin=new Padding(8,0,0,0);buttons.Controls.Add(open);}
            column.Controls.Add(buttons);panel.Controls.Add(column,1,0);
            AddLocalRow(grid,LocalLabel(localSpec.LocalizedPictureLabel(language,picture),localSpec.LocalizedPictureDescription(language,picture)),panel);
        }
        void InsertPicture(LocalPictureRow picture,string display)
        {
            string target=PicturePath(picture,display);
            using(var dialog=new OpenFileDialog{Title=language.T("picture_choose_title"),Filter=language.T("picture_filter"),CheckFileExists=true,Multiselect=false})
            {
                if(Theme.Modal(this,dialog)!=DialogResult.OK)return;
                string source=dialog.FileName;
                using(var stream=new MemoryStream(File.ReadAllBytes(source)))using(var image=Image.FromStream(stream))
                    if(picture.Size>0&&(image.Width!=picture.Size||image.Height!=picture.Size))throw new FormatException(language.Format("picture_wrong_size",picture.Size,image.Width,image.Height));
                if(File.Exists(target)&&Question(language.Format("picture_replace",Path.GetFileName(target)),false)!=DialogResult.OK)return;
                Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(source,target,true);Report(language.Format("picture_copied",target));BuildLocalResourceEditor();
            }
        }
        // What else belongs to a personal entry (0.4.35): its "<id>.name" / "<id>.desc" in the text
        // pack and the files of its picture rows. "Remove everything" takes them along.
        sealed class RemovalExtra{public string Note;public Action Apply;}
        List<RemovalExtra> RemovalExtras(string id)
        {
            var list=new List<RemovalExtra>();string display=localSpec.DisplayId(id);ItemGroup group=localSpec.GroupOfId(id);
            if(localSpec.TextPack!=null&&(group==null?localSpec.TextPack.KeysFrom.Length==0:localSpec.TextPack.KeysFrom.Equals(group.Id,StringComparison.OrdinalIgnoreCase)))
            {
                TextPackSession pack=CurrentTextPack();
                if(pack!=null&&pack.Exists)
                {
                    var keys=pack.Keys().Where(k=>k.Equals(display+".name",StringComparison.OrdinalIgnoreCase)||k.Equals(display+".desc",StringComparison.OrdinalIgnoreCase)).ToList();
                    if(keys.Count>0)list.Add(new RemovalExtra{Note=language.Format("remove_texts_note",String.Join(", ",keys)),Apply=()=>{foreach(string k in keys)pack.RemoveKey(k);StageTextPack();}});
                }
            }
            foreach(LocalPictureRow picture in localSpec.PicturesFor(group)){string path=PicturePath(picture,display);if(File.Exists(path))list.Add(new RemovalExtra{Note=language.Format("remove_picture_note",path),Apply=()=>File.Delete(path)});}
            return list;
        }
        void RemoveWithExtras(string id,List<RemovalExtra> extras)
        {
            string display=localSpec.DisplayId(id);ItemGroup group=localSpec.GroupOfId(id);
            string message=language.Format("remove_choice",display,String.Join("\n",extras.Select(x=>x.Note)));
            DialogResult answer;
            if(ResourceRemovePrompt!=null)answer=ResourceRemovePrompt(message);
            else using(var dialog=new Form{Text=language.T("list_remove"),Font=Font,Size=new Size(700,330),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=2,Padding=new Padding(20)};table.RowStyles.Add(new RowStyle(SizeType.Percent,100));table.RowStyles.Add(new RowStyle(SizeType.Absolute,58));
                var text=Theme.Label(message,10,false);text.AutoSize=false;text.Dock=DockStyle.Fill;table.Controls.Add(text,0,0);
                var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};
                var all=Theme.Button(language.T("remove_all"),()=>{dialog.DialogResult=DialogResult.Yes;dialog.Close();},true);all.BackColor=Theme.Danger;all.AccessibleName="remove:all";
                var only=Theme.Button(localSpec.LocalizedRemoveLabel(language,group),()=>{dialog.DialogResult=DialogResult.No;dialog.Close();},false);only.Margin=new Padding(8,0,0,0);only.AccessibleName="remove:only";
                var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);cancel.Margin=new Padding(8,0,0,0);
                buttons.Controls.Add(all);buttons.Controls.Add(only);buttons.Controls.Add(cancel);table.Controls.Add(buttons,0,1);dialog.Controls.Add(table);dialog.CancelButton=cancel;
                answer=Theme.Modal(this,dialog);
            }
            if(answer!=DialogResult.Yes&&answer!=DialogResult.No&&answer!=DialogResult.OK)return;
            if(answer==DialogResult.Yes)foreach(RemovalExtra extra in extras)extra.Apply();
            resourceSession.Remove(id);selectedLocalResource="";BuildLocalResourceEditor();
        }
        // Personal lines are removed; original lines are only hidden from the effective file.
        void RemoveLocalListItem(string id,bool owned)
        {
            if(owned){var extras=RemovalExtras(id);if(extras.Count>0){RemoveWithExtras(id,extras);return;}}
            string message=language.Format(owned?"list_remove_save_warning":"list_hide_save_warning",id);
            DialogResult answer=ResourceRemovePrompt!=null?ResourceRemovePrompt(message):Question(message,false);if(answer!=DialogResult.OK)return;
            if(owned)resourceSession.Remove(id);else resourceSession.Suppress(id);selectedLocalResource="";BuildLocalResourceEditor();
        }
        // A divider line with a small bold title across both columns of the detail grid.
        static void AddLocalHeading(TableLayoutPanel table,string text)
        {
            int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            var box=new TableLayoutPanel{ColumnCount=1,AutoSize=true,Dock=DockStyle.Top,Margin=new Padding(0,6,0,10)};box.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            var line=new Panel{Height=1,Dock=DockStyle.Top,BackColor=Theme.Line,Margin=new Padding(0,0,0,10)};AddRow(box,line);
            var title=Theme.Label(text,11,true);title.ForeColor=Theme.Navy;title.Margin=new Padding(0,0,0,4);title.AccessibleName="heading:"+text;AddRow(box,title);
            table.Controls.Add(box,0,row);table.SetColumnSpan(box,2);
        }
        // Detail panels narrower than this show the caption above the input (0.4.36).
        const int NarrowDetailWidth=600;
        static bool Narrow(Control panel){return panel.ClientSize.Width>0&&panel.ClientSize.Width<NarrowDetailWidth;}
        bool narrowDetails,narrowTextDetails;
        static void AddLocalRow(TableLayoutPanel table,Control label,Control input)
        {
            if(Equals(table.Tag,"narrow"))
            {
                // Narrow detail panel: caption above the input, both across the full width.
                label.Margin=new Padding(0,4,0,2);int r1=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));label.Dock=DockStyle.Top;table.Controls.Add(label,0,r1);table.SetColumnSpan(label,2);
                int r2=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));input.Dock=DockStyle.Top;table.Controls.Add(input,0,r2);table.SetColumnSpan(input,2);return;
            }int row=table.RowCount++;table.RowStyles.Add(new RowStyle(SizeType.AutoSize));label.Dock=DockStyle.Top;input.Dock=DockStyle.Top;table.Controls.Add(label,0,row);table.Controls.Add(input,1,row);}
        Control LocalLabel(string title,string help)
        {var box=new TableLayoutPanel{ColumnCount=1,AutoSize=true,Dock=DockStyle.Top,Margin=new Padding(0,4,16,17)};box.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));var label=Theme.Label(title,10,true);AddRow(box,label);if(help.Length>0){var descriptionLabel=Theme.Label(help,8,false);descriptionLabel.ForeColor=Theme.Muted;descriptionLabel.MaximumSize=new Size(300,0);AddRow(box,descriptionLabel);box.SizeChanged+=(s,e)=>{int w=Math.Max(120,box.ClientSize.Width);if(descriptionLabel.MaximumSize.Width!=w)descriptionLabel.MaximumSize=new Size(w,0);};}return box;}
        // The note and reset button of a row, kept on the row panel so a field edit can update them
        // in place instead of redrawing the detail area (which flashed white and lost the scroll position).
        sealed class RowNote{public Label Note;public VectorButton Reset;public Font Plain,Strong;}
        // strong (0.4.85): the note line carries the meaning of the empty field ("Standard: ...") and is
        // drawn bold and in the normal ink colour, so it reads as a statement and not as a faded remark.
        static void UpdateRow(Panel row,string origin,bool resetVisible,bool strong=false)
        {
            var state=row==null||row.IsDisposed?null:row.Tag as RowNote;if(state==null)return;
            state.Note.Text=origin;if(state.Plain!=null)state.Note.Font=strong?state.Strong:state.Plain;state.Note.ForeColor=strong?Theme.Ink:Theme.Muted;
            if(state.Reset!=null)state.Reset.Visible=resetVisible;
        }
        Panel LocalInput(Control input,string origin,Action reset,bool resetVisible=true,bool strong=false)
        {
            // Input on the left, the reset button in its own column on the right at the input's
            // height - the column is always reserved so rows line up whether or not the button
            // shows (0.4.38) - and the origin note below across the full width. Tall inputs
            // (lines boxes) push the note down; their note sits right under the counter (0.4.38).
            // Switches sit at the right edge beside the reset column and carry no note (0.4.38).
            bool toggle=input is ToggleSwitch;if(toggle)origin="";
            int inputHeight=Math.Max(29,input.Height);
            Func<int,int> noteTop=h=>input is LinesBox?h-13:h+7;
            Func<int,int> panelHeight=h=>toggle?h+12:noteTop(h)+36;
            var panel=new Panel{Dock=DockStyle.Top,Height=panelHeight(inputHeight),Margin=new Padding(0,2,4,4)};int reserve=Fields.Reserve;
            input.Location=new Point(0,0);input.Anchor=AnchorStyles.Left|AnchorStyles.Top;panel.Controls.Add(input);
            var note=Theme.Label(origin,8,false);note.ForeColor=Theme.Muted;note.Location=new Point(0,noteTop(inputHeight));note.Size=new Size(Math.Max(120,panel.Width),30);note.Anchor=AnchorStyles.Left|AnchorStyles.Top;note.Visible=!toggle;panel.Controls.Add(note);
            input.SizeChanged+=(s,e)=>{int h=Math.Max(29,input.Height);if(panel.Height!=panelHeight(h)){panel.Height=panelHeight(h);note.Location=new Point(0,noteTop(h));}};
            VectorButton button=null;
            if(reset!=null){button=new VectorButton{Symbol="reset",Primary=true,BackColor=Theme.Blue,ForeColor=Color.White,GlyphScale=1.05f,Size=new Size(Fields.Reset,Fields.Reset),Location=new Point(panel.Width-Fields.Reset,Math.Max(0,(inputHeight-Fields.Reset)/2)),Anchor=AnchorStyles.Left|AnchorStyles.Top,AccessibleName=language.T("reset")};button.Click+=(s,e)=>Run(reset);button.Visible=resetVisible;panel.Controls.Add(button);}
            panel.Tag=new RowNote{Note=note,Reset=button,Plain=note.Font,Strong=new Font(note.Font,FontStyle.Bold)};
            if(strong){note.Font=((RowNote)panel.Tag).Strong;note.ForeColor=Theme.Ink;}
            EventHandler fit=(s,e)=>{int w=panel.ClientSize.Width;if(toggle)input.Left=Math.Max(0,w-reserve-input.Width);else input.Width=Math.Max(120,w-reserve);note.Width=Math.Max(120,w);if(button!=null)button.Left=w-Fields.Reset;};
            panel.SizeChanged+=fit;fit(panel,EventArgs.Empty);return panel;
        }
        // The 57 base-game donors plus "custom", grouped, each with its transport class.
        GroupedPicker TemplatePicker(string current)
        {
            var picker=new GroupedPicker{Height=Fields.Height};Fields.Tall(picker);
            picker.AddChoice("custom",language.T("resource_template_custom"));
            foreach(string group in ResourceCatalogData.Groups)
            {
                picker.AddHeader(language.T("resource_group_"+group));
                foreach(var t in ResourceCatalogData.Templates.Where(x=>x.Group==group)) picker.AddChoice(t.Name,t.Name+"  ("+t.Transport+")");
            }
            if(!picker.Select(current)&&!String.IsNullOrWhiteSpace(current)){picker.AddChoice(current.Trim(),current.Trim()+"  (?)");picker.Select(current);}
            return picker;
        }
        // The eighteen transport classes; an empty first entry where the class is optional.
        ComboBox TransportPicker(string current,bool optional)
        {
            var combo=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height};Fields.Tall(combo);if(optional)combo.Items.Add("");combo.Items.AddRange(ResourceCatalogData.TransportClasses);
            string head=ResourceCatalogData.Head(current);if(head.Length>0&&!combo.Items.Contains(head))combo.Items.Add(head);combo.SelectedItem=head.Length>0?head:(optional?"":null);return combo;
        }
        // Template or display name changed: only the detail area and the list entry are redrawn, the
        // rest of the editor stays (a full rebuild flickered and, from a dropdown, collapsed the page).
        // Every field edit of a list, section or Resources entry redraws only the detail area and the
        // list entry (never the whole editor): the tabs, cards and scroll position stay, and the control
        // that had the focus gets it back by its accessible name.
        void RefreshLocalDetails(string id)
        {
            Panel panel=detailsPanel;ListBox list=detailsList;if(panel==null||panel.IsDisposed||list==null||list.IsDisposed)return;
            LocalResourceItem fresh=resourceSession.Items().FirstOrDefault(x=>x.Id.Equals(id,StringComparison.OrdinalIgnoreCase));
            if(fresh==null){BuildLocalResourceEditor();return;}
            string focusName=null;Control active=ActiveControl;while(active is ContainerControl&&((ContainerControl)active).ActiveControl!=null)active=((ContainerControl)active).ActiveControl;
            for(Control c=active;c!=null&&c!=this;c=c.Parent){if(c==panel){for(Control n=active;n!=null&&n!=panel;n=n.Parent)if(!String.IsNullOrEmpty(n.AccessibleName)){focusName=n.AccessibleName;break;}break;}}
            refreshingListItem=true;
            try{for(int i=0;i<list.Items.Count;i++){var entry=list.Items[i] as LocalResourceItem;if(entry!=null&&entry.Id.Equals(id,StringComparison.OrdinalIgnoreCase)){list.Items[i]=fresh;list.SelectedIndex=i;break;}}}
            finally{refreshingListItem=false;}
            // Frozen while it is replaced and back at the same scroll position afterwards.
            Point scroll=panel.AutoScrollPosition;Theme.SetRedraw(panel,false);
            try
            {
                if(localSpec.IsSections)BuildLocalSectionDetails(panel,list,fresh);else if(localSpec.IsList)BuildLocalListDetails(panel,list,fresh);else BuildLocalResourceDetails(panel,list,fresh);
                panel.PerformLayout();panel.AutoScrollPosition=new Point(Math.Abs(scroll.X),Math.Abs(scroll.Y));
            }
            finally{Theme.SetRedraw(panel,true);}
            if(focusName!=null){Control target=FindByAccessibleName(panel,focusName);if(target!=null&&target.CanFocus)target.Focus();}
            UpdateStatus();
        }
        bool refreshingListItem;ListBox detailsList;bool pausedUnlocked;
        void BuildLocalResourceDetails(Panel panel,ListBox list,LocalResourceItem item)
        {
            Theme.DisposeChildren(panel);var shell=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=1,Padding=new Padding(0,0,8,18),Margin=Padding.Empty};shell.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));panel.Controls.Add(shell);
            var titleRow=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,ColumnCount=2,Margin=new Padding(0,0,0,12)};titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));titleRow.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,item.Owned?48:0));var title=Theme.Label(item.Display.Length==0?item.Id:item.Display,15,true);titleRow.Controls.Add(title,0,0);
            if(item.Owned){var remove=new VectorButton{Symbol="trash",Danger=true,BackColor=Theme.Danger,ForeColor=Color.White,GlyphScale=1.2f,Size=new Size(38,34),AccessibleName=language.T("remove_resource")};remove.Click+=(s,e)=>Run(()=>RemoveLocalResource(item.Id));titleRow.Controls.Add(remove,1,0);}
            AddRow(shell,titleRow);var origin=Theme.Label(item.Owned?language.T("resource_personal"):OriginText(item.Id,"resource_original_locked"),9,false);origin.ForeColor=item.Owned?Color.FromArgb(26,132,61):Theme.Muted;origin.Margin=new Padding(0,0,0,15);AddRow(shell,origin);
            var grid=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,Margin=Padding.Empty};FluidColumns(grid,46,800);grid.Tag=Narrow(panel)?"narrow":null;   // 0.4.36
            var idValue=new Label{Text=item.Id,AutoSize=false,Height=Fields.Height,Font=Fields.Font,BackColor=Theme.Pale,BorderStyle=BorderStyle.FixedSingle,TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(5,0,5,0)};AddLocalRow(grid,LocalLabel(language.T("resource_identifier"),language.T("resource_identifier_help")),LocalInput(idValue,item.Owned?language.T("personal"):language.T("standard"),null));
            var template=TemplatePicker(item.Template);template.SelectedIndexChanged+=(s,e)=>{if(!template.Focused||template.Value.Length==0||template.Value.Equals(item.Template,StringComparison.OrdinalIgnoreCase))return;string chosen=template.Value;Later(template,()=>Run(()=>{resourceSession.SetList(item.Id,chosen,item.Display);RefreshLocalDetails(item.Id);}));};AddLocalRow(grid,LocalLabel(language.T("resource_template"),language.T("resource_template_help")),LocalInput(template,item.Owned?language.T("personal"):language.T("resource_inherited"),item.Owned?null:(Action)(()=>{var baseValue=ResourceListValue.Parse(resourceSession.OriginalListValue(item.Id));resourceSession.SetList(item.Id,baseValue.Template,baseValue.Display);BuildLocalResourceEditor();})));
            var display=new TextBox{Text=item.Display,Height=Fields.Height};display.TextChanged+=(s,e)=>{if(!display.Focused)return;try{resourceSession.SetList(item.Id,item.Template,display.Text);}catch(Exception){}UpdateStatus();};display.Leave+=(s,e)=>{if(display.Text==item.Display)return;string typed=display.Text;Later(display,()=>Run(()=>{resourceSession.SetList(item.Id,item.Template,typed);RefreshLocalDetails(item.Id);}));};AddLocalRow(grid,LocalLabel(language.T("resource_display_name"),language.T("resource_display_help")),LocalInput(Fields.Wrap(display),item.Owned?language.T("personal"):language.T("resource_inherited"),item.Owned?null:(Action)(()=>{var baseValue=ResourceListValue.Parse(resourceSession.OriginalListValue(item.Id));resourceSession.SetList(item.Id,item.Template,baseValue.Display);BuildLocalResourceEditor();})));
            foreach(LocalDetailField field in localSpec.Fields)
            {
                // 0.5.19: plugin-wide fields (scope = global) belong to the cards of their tab, never to a
                // resource record - the resources schema had none before 0.5.18, so this loop never filtered.
                if(field.Scope=="global")continue;
                // 0.5.19: a field's heading (rule line plus bold title) groups the resource record the same
                // way it does in the list and section editors; the resources builder never called it before.
                string heading=localSpec.FieldHeading(language,field);if(heading.Length>0)AddLocalHeading(grid,heading);
                string value=resourceSession.Value(item.Id,field),baselineValue=resourceSession.BaselineValue(item.Id,field),label=localSpec.FieldLabel(language,field),help=localSpec.FieldDescription(language,field);Control input;
                if(field.Type=="choice")
                {
                    // Shown by name: the first token of a class line ("oil, 1, 5, 5, 0" -> oil)
                    // and a family number (13 -> plastic). Choosing writes the name alone.
                    string shown=field.ChoicePrefix?ResourceCatalogData.Head(value):value;if(field.Key.Equals("family",StringComparison.OrdinalIgnoreCase))shown=ResourceCatalogData.FamilyName(shown);
                    var combo=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList,Height=Fields.Height};Fields.Tall(combo);combo.Items.Add("");combo.Items.AddRange(field.Choices);if(shown.Length>0&&!combo.Items.Contains(shown))combo.Items.Add(shown);combo.SelectedItem=shown;
                    combo.SelectedIndexChanged+=(s,e)=>{if(!combo.Focused)return;string chosen=Convert.ToString(combo.SelectedItem);Later(combo,()=>Run(()=>{resourceSession.SetField(item.Id,field,chosen);UpdateStatus();}));};input=combo;
                }
                else{var text=new TextBox{Text=value,Height=Fields.Height};text.TextChanged+=(s,e)=>{if(!text.Focused)return;try{resourceSession.SetField(item.Id,field,text.Text);}catch(Exception){}UpdateStatus();};text.Leave+=(s,e)=>Run(()=>{resourceSession.SetField(item.Id,field,text.Text);UpdateStatus();});input=Fields.Wrap(text);}
                // 0.5.19: an empty field says nothing under it - "no entry" twelve times per resource was noise.
                string source=baselineValue.Length>0?language.T("resource_original_value")+": "+baselineValue:value.Length>0?language.T("personal"):"";Action reset=()=>{resourceSession.SetField(item.Id,field,"");BuildLocalResourceEditor();};AddLocalRow(grid,LocalLabel(label,help),LocalInput(input,source,reset));
            }
            LockForeign(panel,shell,grid,item.Id);
            AddRow(shell,grid);list.Refresh();
        }
        // 0.5.9: an entry that a mod package brings in under Soviet Mod Loader. It is in the game,
        // so it belongs in the list - but this editor writes the baseline, and SML merges the
        // packages on top of that afterwards. Typing here would change nothing, so the values are
        // shown and nothing more, with a line that says where the entry comes from.
        bool LockedEntry(string id){return resourceSession!=null&&resourceSession.Locked(id);}
        bool ForeignEntry(string id){return resourceSession!=null&&resourceSession.Foreigner(id);}
        // The usual "an original stays, you may override it" line is wrong for a package entry -
        // it says nothing instead, and LockForeign puts the real story there.
        string OriginText(string id,string key){return ForeignEntry(id)?"":language.T(key);}
        void LockForeign(Panel panel,TableLayoutPanel shell,TableLayoutPanel grid,string id)
        {
            if(!ForeignEntry(id))return;
            string owner=resourceSession.Foreign[id];bool locked=resourceSession.Locked(id);
            string text=locked
                ?(owner.Length>0?language.Format("sml_foreign_entry",owner):language.T("sml_foreign_entry_plain"))
                :(owner.Length>0?language.Format("sml_foreign_overlay",owner,SmlOverlay.Name):language.Format("sml_foreign_overlay_plain",SmlOverlay.Name));
            var note=Theme.Label(text,9,true);
            note.ForeColor=Color.FromArgb(139,81,0);note.Margin=new Padding(0,0,0,12);
            note.MaximumSize=new Size(Math.Max(300,panel.ClientSize.Width-30),0);AddRow(shell,note);
            if(locked)LockInputs(grid);
        }
        // 0.5.9: a paused page is greyed out as far as WinForms allows - a real translucent layer
        // over live controls does not exist there, so the cards, their panels and their text take
        // the grey themselves. The notice boxes keep their colour: they carry the message.
        static void DimCard(Control root)
        {
            foreach(Control child in root.Controls)
            {
                if(child is NoticeBox)continue;
                var label=child as Label;
                if(label!=null)label.ForeColor=Theme.DimInk;
                else if((child is Panel||child is TableLayoutPanel||child is FlowLayoutPanel)&&child.BackColor==Color.White)child.BackColor=Theme.Dim;
                DimCard(child);
            }
        }
        static void LockInputs(Control root)
        {
            foreach(Control child in root.Controls)
            {
                string role=child.AccessibleName??"";
                if(role=="list-search"||role=="card-search"){LockInputs(child);continue;}
                if(child is TextBox){var box=(TextBox)child;box.ReadOnly=true;box.BackColor=Theme.Pale;}
                else if(child is ComboBox||child is CheckBox||child is ToggleSwitch||child is Button||child is VectorButton||child is NumberInput||child is LinesBox)child.Enabled=false;
                LockInputs(child);
            }
        }
        void OpenLocalResourceDialog()
        {
            using(var dialog=new Form{Text=language.T("resource_create"),Font=Font,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,MinimumSize=new Size(590,0),FormBorderStyle=FormBorderStyle.FixedDialog,StartPosition=FormStartPosition.CenterParent,MaximizeBox=false,MinimizeBox=false,ShowInTaskbar=false,Icon=Icon})
            {
                Theme.ApplyWindowChrome(dialog);var table=new TableLayoutPanel{Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,ColumnCount=2,RowCount=5,Padding=new Padding(20)};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,190));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));var id=new TextBox();var template=TemplatePicker("custom");var display=new TextBox();var transport=TransportPicker("",true);
                // Picking a donor suggests its transport class; the class stays editable.
                template.SelectedIndexChanged+=(s,e)=>{var donor=ResourceCatalogData.Find(template.Value);if(donor!=null&&template.Focused)transport.SelectedItem=donor.Transport;};
                string[] labels={"resource_identifier","resource_template","resource_display_name","resource_transport"};Control[] controls={Fields.Wrap(id),template,Fields.Wrap(display),transport};for(int i=0;i<4;i++){table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(Theme.Label(language.T(labels[i]),10,true),0,i);controls[i].Dock=DockStyle.Top;controls[i].Margin=new Padding(3,3,3,18);table.Controls.Add(controls[i],1,i);}var buttons=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false,Padding=new Padding(0,8,0,0)};var add=Theme.Button(language.T("add"),()=>Run(()=>{resourceSession.Add(id.Text,template.Value,display.Text,Convert.ToString(transport.SelectedItem));selectedLocalResource=id.Text.Trim().ToLowerInvariant();dialog.DialogResult=DialogResult.OK;dialog.Close();}),true);var cancel=Theme.Button(language.T("cancel"),()=>{dialog.DialogResult=DialogResult.Cancel;dialog.Close();},false);buttons.Controls.Add(add);buttons.Controls.Add(cancel);buttons.Dock=DockStyle.Top;buttons.AutoSize=true;table.SetColumnSpan(buttons,2);table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(buttons,0,4);dialog.Controls.Add(table);dialog.AcceptButton=add;dialog.CancelButton=cancel;if(Theme.Modal(this,dialog)==DialogResult.OK)BuildLocalResourceEditor();
            }
        }
        void RemoveLocalResource(string id)
        {
            ResourceCleanupPlan cleanup=ResourceConsistency.PlanRemoval(state.Build,entries,id);string message=language.Format("resource_remove_save_warning",id);
            if(cleanup.Plugins.Count>0)message+="\n\n"+language.Format("resource_remove_dependencies",String.Join(", ",cleanup.Plugins.Distinct(StringComparer.CurrentCultureIgnoreCase)));
            DialogResult answer=ResourceRemovePrompt!=null?ResourceRemovePrompt(message):Question(message,false);if(answer!=DialogResult.OK)return;
            resourceSession.Remove(id);resourceSession.StageDependencyWrites(cleanup.Writes);selectedLocalResource="";BuildLocalResourceEditor();
        }
    }
}
