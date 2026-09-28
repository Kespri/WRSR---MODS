using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace TesmioAutoload
{
    // 0.4.88: "Vor dem Spielstart" - the one page that answers what the next start will do.
    // Everything here exists elsewhere too (notices card, status line, log window); what is new
    // is seeing it together, and being able to click a problem to land on the entry that has it.
    sealed class StartCheckWindow : Form
    {
        readonly Language language;
        public string Jump="";                                // root of the entry the user clicked

        public StartCheckWindow(Language language,string build,List<CatalogEntry> entries,List<SaveGame> saves,string lastSeen,Font font,Icon icon)
        {
            this.language=language;
            Text=language.T("startcheck"); Font=font; Icon=icon; Size=new Size(1080,760); MinimumSize=new Size(820,560);
            StartPosition=FormStartPosition.CenterParent; ShowInTaskbar=false; BackColor=Color.White;
            var shell=new TableLayoutPanel{Dock=DockStyle.Fill,ColumnCount=1,RowCount=2,Padding=new Padding(14)};
            shell.RowStyles.Add(new RowStyle(SizeType.Percent,100)); shell.RowStyles.Add(new RowStyle(SizeType.Absolute,52));
            Controls.Add(shell);
            var scroll=new Panel{Dock=DockStyle.Fill,AutoScroll=true}; shell.Controls.Add(scroll,0,0);
            var stack=new TableLayoutPanel{Dock=DockStyle.Top,ColumnCount=1,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink};
            stack.Width=scroll.ClientSize.Width-4; scroll.SizeChanged+=(s,e)=>stack.Width=scroll.ClientSize.Width-4;
            scroll.Controls.Add(stack);

            var list=entries??new List<CatalogEntry>();
            var steps=Startup.Order(build,list);
            var late=Startup.LateDependencies(steps,list);
            // What counts as "loads": a step of the order that belongs to a known entry.
            int problems=list.Count(e=>e.Problem.Length>0||e.Dependencies.Any(d=>!d.Found||!d.VersionOk));
            int updates=list.Count(e=>e.Updated);
            int off=list.Count(e=>e.Supported&&e.Problem.Length==0&&!steps.Any(s=>Belongs(s,e))&&e.Kind!="content");

            var summary=Card(stack,language.T("startcheck_summary"));
            Line(summary,language.Format("startcheck_loads",steps.Count),Theme.Ink,null);
            if(off>0)Line(summary,language.Format("startcheck_off",off),Theme.Muted,null);
            if(updates>0)Line(summary,language.Format("startcheck_updates",updates),Theme.Amber,null);
            Line(summary,lastSeen,Theme.Muted,null);
            if(saves!=null&&saves.Count>0)Line(summary,language.Format("startcheck_saves",saves.Count),Theme.Muted,null);

            var trouble=Card(stack,language.T("startcheck_problems"));
            bool any=false;
            foreach(CatalogEntry entry in list.Where(e=>e.Problem.Length>0).OrderBy(e=>e.Name,StringComparer.CurrentCultureIgnoreCase))
            { Line(trouble,entry.Name+" - "+language.Localize(entry.Problem),Theme.Danger,entry.Root); any=true; }
            foreach(CatalogEntry entry in list)
                foreach(Dependency dependency in entry.Dependencies.Where(d=>!d.Found||!d.VersionOk))
                { Line(trouble,language.Format("startcheck_dependency",entry.Name,dependency.Id),Theme.Danger,entry.Root); any=true; }
            foreach(var pair in late)
            { Line(trouble,language.Format("startcheck_late",pair.Key,pair.Value),Theme.Amber,""); any=true; }
            // 0.5.13: a building line naming a resource nothing publishes ends the game at start.
            foreach(MissingResource missing in Startup.MissingResources(build,steps))
            { Line(trouble,language.Format("startcheck_missing_resource",missing.Plugin,missing.Section,missing.Resource),Theme.Danger,""); any=true; }
            foreach(CatalogEntry entry in list.Where(e=>e.Updated).OrderBy(e=>e.Name,StringComparer.CurrentCultureIgnoreCase))
            { Line(trouble,language.Format("startcheck_update_row",entry.Name,entry.PreviousVersion,entry.Version),Theme.Amber,entry.Root); any=true; }
            if(!any)Line(trouble,language.T("startcheck_clean"),Color.FromArgb(26,132,61),null);

            var order=Card(stack,language.T("startcheck_order"));
            // 0.5.4: who hands the packages over decides the order, so the sentence has to follow.
            bool sml=Sml.Active(build);
            Line(order,language.T(sml?"startcheck_order_help_sml":"startcheck_order_help"),Theme.Muted,null);
            foreach(LoadStep step in steps)
            {
                string where=step.Bridge?language.T(sml?"startcheck_from_sml":"startcheck_from_bridge"):language.T("startcheck_from_plugins");
                Line(order,(step.Index+1).ToString().PadLeft(2)+".  "+step.Name+"   ("+where+")",Theme.Ink,null);
            }
            if(steps.Count==0)Line(order,language.T("startcheck_order_empty"),Theme.Muted,null);

            var footer=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.RightToLeft,WrapContents=false};
            footer.Controls.Add(Theme.Button(language.T("close"),()=>{DialogResult=DialogResult.Cancel;Close();},false));
            shell.Controls.Add(footer,0,1);
        }
        static bool Belongs(LoadStep step,CatalogEntry entry)
        {
            string folder=System.IO.Path.GetFileName((entry.Root??"").TrimEnd('\\'));
            return step.Bridge?step.Key.Equals(folder,StringComparison.OrdinalIgnoreCase)
                :entry.Target.Length>0&&step.Target.Equals(entry.Target,StringComparison.OrdinalIgnoreCase);
        }
        // A card is a panel with a header stripe; its rows live in a table inside it, the same way
        // the plugin pages build theirs.
        static Card Card(TableLayoutPanel stack,string title)
        {
            var card=new Card(title){Dock=DockStyle.Top,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink};
            var inner=new TableLayoutPanel{ColumnCount=1,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Dock=DockStyle.Top,Margin=Padding.Empty};
            inner.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));
            card.Controls.Add(inner); card.Tag=inner;
            stack.RowStyles.Add(new RowStyle(SizeType.AutoSize)); stack.Controls.Add(card);
            return card;
        }
        // A row: plain text, or a link that closes the window and jumps to the entry.
        void Line(Card card,string text,Color colour,string root)
        {
            if(String.IsNullOrEmpty(text))return;
            var inner=(TableLayoutPanel)card.Tag;
            Label label=Theme.Label(text,9.5f,false);
            label.ForeColor=colour; label.Margin=new Padding(0,2,0,2); label.AutoSize=true;
            if(!String.IsNullOrEmpty(root))
            {
                label.Cursor=Cursors.Hand;
                label.Click+=(s,e)=>{Jump=root;DialogResult=DialogResult.OK;Close();};
            }
            int row=inner.RowCount++; inner.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            label.Dock=DockStyle.Top; inner.Controls.Add(label,0,row);
        }
    }
}
