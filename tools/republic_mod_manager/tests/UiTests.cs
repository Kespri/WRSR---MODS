using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using TesmioAutoload;

static class UiTests
{
    static int passed;
    static void Check(bool value,string name){if(!value)throw new Exception("FAIL "+name);passed++;Console.WriteLine("PASS "+name);}
    static void HiddenShow(MainForm form){form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-20000,-20000);form.Show();Application.DoEvents();}
    static IEnumerable<Control> Children(Control root){foreach(Control child in root.Controls){yield return child;foreach(Control nested in Children(child))yield return nested;}}
    static void CopyTree(string source,string target){foreach(string file in Directory.GetFiles(source,"*",SearchOption.AllDirectories)){string destination=Path.Combine(target,file.Substring(source.Length+1));Directory.CreateDirectory(Path.GetDirectoryName(destination));File.Copy(file,destination,true);}}
    static void Write(string path,string text){Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,text,SafeFiles.Utf8);}
    static string MakeSimplePackage(string collection,string dllSource)
    {
        string package=Path.Combine(collection,"5550000000");Directory.CreateDirectory(Path.Combine(package,"hooks"));Directory.CreateDirectory(Path.Combine(package,"config"));File.Copy(dllSource,Path.Combine(package,"hooks","sample_plugin.dll"));
        Write(Path.Combine(package,"hooks","sample_plugin.ini"),"[general]\nenabled=1\nlimit=5\nmode=normal\n");
        Write(Path.Combine(package,"config","sample.launcher.ini"),"[launcher]\nlayout_version=1\nvisible=1\nid=example.sample\nconfig=sample_plugin.ini\nenabled_field=general/enabled\nicon=builtin:gear\n"+
            "[field:enabled]\nsection=general\nkey=enabled\nlabel=Enabled\ntype=boolean\ngroup=main\n"+
            "[field:limit]\nsection=general\nkey=limit\nlabel=Limit\ntype=integer\nminimum=0\nmaximum=20\ngroup=main\n"+
            "[field:mode]\nsection=general\nkey=mode\nlabel=Mode\ntype=choice\nchoices=normal|strict\ngroup=main\n"+
            "[tab:settings]\nlabel=Settings\n[group:main]\ntab=settings\nlabel=Main\n");
        Write(Path.Combine(package,"soviet.mod.ini"),"[mod]\nid=example.sample\nname=Sample Plugin\nversion=2.0-beta\nenabled=1\ntesmio_api_min=4\ntesmio_api_max=4\n[hooks]\ndll=hooks\\sample_plugin.dll\n"+
            "[configuration]\ndefaults=hooks\\sample_plugin.ini\nlauncher_schema=config\\sample.launcher.ini\nuser_config=sample_plugin.ini\n[autoload]\nformat=1\nkind=plugin\ntarget=sample_plugin\n");return package;
    }

    [STAThread] static int Main(string[] args)
    {
        try
        {
            if(args.Length!=2)return 2;string source=Path.GetFullPath(args[0]),root=Path.GetFullPath(args[1]);if(Directory.Exists(root))throw new IOException("Use a fresh UI test directory.");
            string collection=Path.Combine(root,"workshop"),package=Path.Combine(collection,"fixture_vehicle_materials"),loader=Path.Combine(root,"build");CopyTree(source,package);
            string simple=MakeSimplePackage(collection,Path.Combine(package,"hooks","vehicle_materials.dll"));string unsupported=Path.Combine(collection,"9876543210");Write(Path.Combine(unsupported,"soviet.mod.ini"),"[mod]\nid=example.other\nname=ZZ Other Mod\nversion=1.0\n[hooks]\ndll=one.dll\ndll=two.dll\n");
            string plugins=Path.Combine(loader,"plugins"),resourceIni=Path.Combine(plugins,"resources.ini"),resourceDll=Path.Combine(plugins,"resources.dll");Directory.CreateDirectory(plugins);File.WriteAllBytes(resourceDll,new byte[]{1,2,3});string needsDll=Path.Combine(plugins,"needs.dll");File.WriteAllBytes(needsDll,new byte[]{9,8,7});Write(Path.Combine(plugins,"needs.ini"),"[list]\nfurniture = eletronics, 1.0, advanced, 0.35, 0.010\nmedicine = eletronics, 0.5, none, 0.30, 0.008\n[needs]\nenabled = 1\ndemand = 1\nstorage = 1\nmax_demands = 7\nwhen_full = skip\nprobe = 1\nlog_seconds = 60\n");string needsDllHash=SafeFiles.HashFile(needsDll);string depositsDll=Path.Combine(plugins,"deposits.dll");File.WriteAllBytes(depositsDll,new byte[]{5,5,5});Write(Path.Combine(plugins,"deposits.ini"),"[deposits]\ncode_patch = 1\nminimap = 1\neditor = 1\n[copper]\ntoken = $TYPE_MINE_COPPER\ntype = 10\nmap = resourcemap2\ncomponent = 3\nradius = ore\nicon = copper_ore\nminimap = 1\neditor = copper\n[sand]\ntoken = $TYPE_MINE_SAND\ntype = 11\nmap = terrain\ncomponent = 1\nradius = gravel\nicon = sand\nminimap = 1\neditor = sand\n");string depositsDllHash=SafeFiles.HashFile(depositsDll);File.WriteAllBytes(Path.Combine(plugins,"buildings_plus.dll"),new byte[]{4,4,4});Write(Path.Combine(plugins,"buildings_plus.ini"),"[buildings_plus]\nenabled = 1\nprune = 0\n\n[example]\ndonor = shop_clothes\nobject = Pharmacy\nname = Pharmacy\n");
            Write(resourceIni,"[list]\nglass=aluminium, Glass\ncable=steel, Cable\nsand=bauxite, Sand\n[resources]\nhook=2\n");Write(Path.Combine(loader,"tesmioloader.ini"),"[tesmioloader]\nplugins=1\n[plugins]\nresources=1\nneeds=1\ndeposits=1\nvehicle_materials=1\nsample_plugin=1\n");
            string resourcesHash=SafeFiles.HashFile(resourceIni),resourceDllHash=SafeFiles.HashFile(resourceDll),profile=Path.Combine(root,"ui-state.ini");var defaults=new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=package,Language="de"};var notes=new List<string>();var store=new UiStateStore(profile);
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);

            using(var form=new MainForm(store.Load(defaults,notes),store,true))
            {
                HiddenShow(form);
                // Three Workshop packages from the fixture plus every keyed editor schema that ships next to rmm.exe; the local count follows settings_schemas, so adding or leaving out a schema (the public repo has fewer) does not break the test.
                int localEditors=Catalog.ScanLocalEditors(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"settings_schemas"),new List<string>()).Count;
                Check(localEditors>=6&&form.ModCount==3+localEditors,"startup lists the three Workshop packages and every schema-driven local editor ("+localEditors+" found next to rmm.exe, at least Resources, Resources Plus, Needs, Needs Plus, Deposits and Buildings Plus)");
                Check(form.SelectedSource==package&&form.HasEditor,"remembered package opens directly without package-specific code");
                Check(form.TabCount==4&&form.ActionCount==3,"tabs come from schema and footer keeps exactly three actions");
                Check(form.DisplayedValue("resources/count")=="0"&&form.DisplayedValue("general/enabled")=="0","Vehicle Materials starts disabled with an empty user-owned collection");Check(form.SwitchVisible&&!form.SwitchChecked&&form.SwitchNote.Contains("tesmioloader.ini"),"single plugin switch shows off while the INI switch is 0");
                form.TestEdit("general/enabled","1");Check(form.StatusDetailText.Contains("Reiter „Ressourcen“")&&form.StatusDetailText.Contains("unter „Materialien“")&&form.StatusDetailText.Contains("Mit +")&&!form.StatusDetailText.Contains("Sammlung"),"German validation names the schema-derived tab and group");form.TestEdit("general/enabled","0");
                Check(form.ResourceChoiceCount==3&&form.ResourceUsedCount==0,"provider catalogue is available while no material is preselected");
                Check(!File.Exists(Path.Combine(plugins,"vehicle_materials.dll"))&&!File.Exists(Path.Combine(plugins,"vehicle_materials.ini")),"viewing never deploys native files");
                Check(SafeFiles.HashFile(resourceIni)==resourcesHash,"viewing never edits provider INI");
                using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,form.Width,form.Height));image.Save(Path.Combine(root,"ui-empty-vehicle-materials.png"));}

                form.SelectTab("vehicles");
                Check(Children(form).OfType<Label>().Any(x=>x.Text.Contains("noch keine Materialien"))&&Children(form).OfType<NoticeIcon>().Count(x=>x.Warning)==1,"schema warning appears while the collection is empty");
                Check(Children(form).OfType<Card>().Single().HeaderText=="Materialverbrauch nach Fahrzeugklasse","group title is rendered in the full-width card header");
                var activeTab=Children(form).OfType<Button>().Single(x=>Convert.ToString(x.Tag)=="vehicles");Check(activeTab.BackColor==Theme.SelectionBlue&&activeTab.ForeColor==Color.White,"active tab uses the selected-plugin blue and white text");
                using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,form.Width,form.Height));image.Save(Path.Combine(root,"ui-empty-warning.png"));}

                form.SelectTab("resources");Check(Children(form).OfType<Card>().Single().HeaderText=="Materialien","Vehicle Materials uses the requested resource group title");var resourceAdd=Children(form).OfType<VectorButton>().Single(x=>x.Symbol=="plus"&&x.Height==38);Check(resourceAdd.Primary&&resourceAdd.BackColor==Theme.Blue,"collection add button remains a primary action");
                Check(resourceAdd.GlyphScale>=2.5f&&!Children(form).OfType<TextBox>().Any(x=>x.Text=="0"),"large add glyph and non-focusable readonly display are used");
                using(var dialog=form.TestResourceDialog())
                {
                    dialog.ShowInTaskbar=false;dialog.StartPosition=FormStartPosition.Manual;dialog.Location=new Point(-20000,-20000);dialog.Show();Application.DoEvents();var picker=Children(dialog).OfType<ResourcePickerList>().Single();
                    Check(picker.UsedCount==0&&picker.AvailableCount==3,"picker shows every provider entry as initially available");
                    Check(Children(dialog).OfType<NumberInput>().All(x=>x.Width<=165)&&Children(dialog).OfType<VectorButton>().Where(x=>x.Symbol=="plus"||x.Symbol=="minus").All(x=>x.Primary&&x.ForeColor==Color.White),"resource dialog uses compact inputs with blue spin buttons");
                    using(var image=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(image,new Rectangle(0,0,dialog.Width,dialog.Height));image.Save(Path.Combine(root,"ui-resource-dialog.png"));}
                    picker.SelectedIndex=2;Application.DoEvents();Check(picker.Choice!=null&&picker.Choice.Option.Id=="sand","unused entry is selectable");dialog.Close();
                }
                form.TestAddResource("sand","0.020","0.010","0.005","0.003");
                Check(form.IsDirty&&form.DisplayedValue("resources/count")=="1"&&form.DisplayedValue("resources/resource0")=="sand"&&form.DisplayedValue("road/sand")=="0.02","plus creates a fully schema-driven collection item");
                Check(form.ResourceUsedCount==1,"new item is immediately marked used");
                form.SelectTab("vehicles");Check(!Children(form).OfType<Label>().Any(x=>x.Text.Contains("noch keine Materialien")),"empty warning disappears immediately after the first item is added");
                form.TestEdit("general/enabled","1");Check(form.StatusValid,"plugin can be enabled after a positive item exists");
                Check(form.IsDirty&&form.StatusText=="Ungespeicherte Änderungen"&&form.SaveEnabled&&form.HeadingText.EndsWith(" *"),"unsaved state shows the amber heading, the primary save button and the heading mark (0.4.51)");Check(form.SwitchChecked,"single plugin switch follows the INI switch");
                form.TestCommit(()=>{});string user=Path.Combine(loader,"user_config","vehicle_materials.ini");Ini saved=new Ini(SafeFiles.Text(user));Check(saved.Get("resources","count")=="1"&&saved.Get("resources","resource0")=="sand"&&saved.Get("airplane","sand")=="0.003","save persists empty-package collection entirely as personal data");
                form.TestEdit("road/sand","0.0200");Check(!form.IsDirty,"numeric formatting-only edit is not reported as a change");
                form.TestEdit("road/sand","0.021");Check(form.TestApply(()=>{}),"generic Save deploys DLL and effective INI");
                Check(new Ini(SafeFiles.Text(Path.Combine(plugins,"vehicle_materials.ini"))).Get("road","sand")=="0.021"&&form.Activity(package),"activity light follows manifest target and enabled field");
                form.SelectTab("resources");Check(Children(form).OfType<VectorButton>().Count(x=>x.Symbol=="trash")==1,"user-owned item receives a delete button");
                string question="";form.ResourceRemovePrompt=text=>{question=text;return DialogResult.Cancel;};Check(!form.TestRemoveResource("sand")&&question.Contains("sand"),"delete confirmation names item and honours Cancel");
                form.ResourceRemovePrompt=text=>DialogResult.OK;Check(form.TestRemoveResource("sand")&&form.DisplayedValue("resources/count")=="0"&&form.DisplayedValue("road/sand")==null,"confirmed delete removes list item and every target value");
                form.SelectTab("vehicles");Check(Children(form).OfType<Label>().Any(x=>x.Text.Contains("noch keine Materialien")),"empty warning returns after the final item is removed");
                Check(form.DisplayedValue("general/enabled")=="0"&&form.StatusValid,"removing the final positive item safely disables the plugin");form.TestCommit(()=>{});
                Check(!form.IsDirty&&!form.SaveEnabled&&form.StatusText=="Konfiguration gültig"&&!form.HeadingText.EndsWith(" *"),"after the commit the heading mark and the save emphasis are gone (0.4.51)");

                form.TestSearch("Sample");form.SelectIndex(0);Application.DoEvents();Check(form.SelectedSource==simple&&form.HasEditor&&form.TabCount==1&&form.DisplayedValue("general/limit")=="5","unrelated second plugin renders from its own schema");
                form.TestEdit("general/limit","7");Check(form.TestApply(()=>{})&&new Ini(SafeFiles.Text(Path.Combine(plugins,"sample_plugin.ini"))).Get("general","limit")=="7","unrelated plugin saves to its manifest-derived filename");
                Check(form.SwitchVisible&&form.SwitchChecked&&form.SwitchNote.Contains("tesmioloader.ini"),"installed plugin shows the single plugin switch on");
                form.TestSwitch(false);Check(!form.SwitchChecked&&form.DisplayedValue("general/enabled")=="1"&&form.TestApply(()=>{})&&new Ini(SafeFiles.Text(Path.Combine(loader,"tesmioloader.ini"))).Get("plugins","sample_plugin")=="0","switching off writes only the loader entry and leaves the INI switch alone");
                form.TestSwitch(true);Check(form.SwitchChecked&&form.TestApply(()=>{})&&new Ini(SafeFiles.Text(Path.Combine(loader,"tesmioloader.ini"))).Get("plugins","sample_plugin")=="1","switching on restores the loader entry");
                form.TestEdit("general/enabled","0");Check(!form.SwitchChecked,"an INI switch set to 0 shows the header switch off although the loader entry is on");form.TestSwitch(true);Check(form.SwitchChecked&&form.DisplayedValue("general/enabled")=="1","switching on sets the INI switch back to 1");form.TestApply(()=>{});
                Check(SafeFiles.HashFile(resourceIni)==resourcesHash,"ordinary plugin operations leave the Resources provider INI byte-for-byte unchanged");
                form.TestSearch("Resources");form.SelectIndex(0);Application.DoEvents();Check(form.HasLocalResourceEditor&&form.LocalResourceCount==3&&form.Activity(form.SelectedSource),"local Resources editor is discovered from its launcher schema and shows the provider list");
                Check(Children(form).OfType<Card>().Single().HeaderText=="Ressourcendefinitionen"&&!Children(form).OfType<VectorButton>().Any(x=>x.Symbol=="trash")&&Children(form).OfType<ResourceDefinitionList>().Count()==1,"selected original resource has a lock-aware master list and no delete action");
                // 0.5.19: the record's fields are grouped under five headings (transport, record prices, market, cargo, fixed prices).
                {var heads=Children(form).OfType<Label>().Where(x=>(x.AccessibleName??"").StartsWith("heading:")).Select(x=>x.AccessibleName.Substring(8)).ToList();Check(heads.Count==5&&heads[0]=="Transport und Preisart"&&heads[4]=="Feste Preise","the resource detail area shows the five schema headings in order: "+String.Join(" | ",heads));}
                {int[] before=form.TestCardWidths();int wide=form.TestContentWidth;form.TestDeferredRebuild();Application.DoEvents();Application.DoEvents();int[] after=form.TestCardWidths();Check(after.Length==before.Length&&after.All(w=>w>=wide-28)&&before.All(w=>w>=wide-28),"a deferred rebuild (dropdown path) keeps every card at full width: "+String.Join(",",after)+" of "+wide+" (before "+String.Join(",",before)+"; content "+form.TestContentSize+")");}
                form.TestAddLocalResource("hydrogen","custom","Hydrogen","oil");form.TestSetLocalField("hydrogen","kind","1");form.TestSetLocalField("hydrogen","pinned_base_price","180, 150");form.TestSetLocalField("hydrogen","forced_price","200, 175");var localEffective=new LooseIni(form.LocalEffective());Check(form.LocalResourceCount==4&&form.TestLocalOwned("hydrogen")&&localEffective.Get("custom:hydrogen","transport")=="oil"&&localEffective.Get("base_price","hydrogen")=="180, 150"&&localEffective.Get("price","hydrogen")=="200, 175","master-detail editor generates custom and keyed price entries for a personal resource");
                Check(Children(form).OfType<VectorButton>().Any(x=>x.Symbol=="trash"&&x.Danger),"personal resource receives the red delete action");Check(form.TestSaveLocal(()=>{})&&SafeFiles.HashFile(resourceDll)==resourceDllHash,"Resources settings save without replacing the provider DLL");
                string warning="";form.ResourceRemovePrompt=text=>{warning=text;return DialogResult.Cancel;};form.TestRemoveLocalResource("hydrogen");Check(form.LocalResourceCount==4&&warning.Contains("Spielstand"),"resource deletion warns about saved-game compatibility and honours Cancel");form.ResourceRemovePrompt=text=>DialogResult.OK;form.TestRemoveLocalResource("hydrogen");Check(form.LocalResourceCount==3&&form.TestSaveLocal(()=>{}),"confirmed deletion removes only the personal resource and saves the clean overlay");
                using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,form.Width,form.Height));image.Save(Path.Combine(root,"ui-resource-master-detail.png"));}
                // 0.4.36: below 600 px of detail width the captions move above the inputs; wide again afterwards.
                form.TestSearch("Deposits");form.SelectIndex(0);Application.DoEvents();form.SelectTab("deposits");Application.DoEvents();
                form.MinimumSize=new Size(900,700);form.Width=1150;Application.DoEvents();Application.DoEvents();
                var narrow=Children(form).OfType<TableLayoutPanel>().Where(t=>Equals(t.Tag,"narrow")).ToList();
                Check(narrow.Count>0&&narrow.All(t=>t.Controls.Cast<Control>().All(c=>t.GetColumn(c)==0)),"narrow details stack captions above inputs (0.4.36)");
                form.Width=1600;Application.DoEvents();Application.DoEvents();
                Check(!Children(form).OfType<TableLayoutPanel>().Any(t=>Equals(t.Tag,"narrow")),"wide details return to two columns (0.4.36)");
                form.TestSearch("Needs");form.SelectIndex(0);Application.DoEvents();Check(form.HasLocalResourceEditor&&form.LocalResourceCount==2&&form.Activity(form.SelectedSource),"local Needs editor is discovered from its keyed_list schema and lists the original needs");
                Check(form.TabCount==2&&form.SelectedTabId=="needs"&&Children(form).OfType<Card>().Count()==1&&Children(form).OfType<Card>().First().HeaderText=="Bedürfnisse der Bürger"&&Children(form).OfType<ComboBox>().Any(x=>x.AccessibleName=="column:donor"),"Needs editor opens on its needs tab with the list card and column inputs");
                form.SelectTab("general");Check(form.SelectedTabId=="general"&&Children(form).OfType<Card>().Count()==1&&Children(form).OfType<Card>().First().HeaderText=="Plugin-Einstellungen"&&Children(form).OfType<ToggleSwitch>().Count(x=>(x.AccessibleName??"").StartsWith("global:"))==3&&!Children(form).OfType<ToggleSwitch>().Any(x=>x.AccessibleName=="global:enabled"),"general tab shows the plugin settings card without the enabled switch, which lives in the header");
                Check(form.SwitchVisible&&form.SwitchChecked&&form.SwitchNote.Contains("tesmioloader.ini"),"Needs editor shows the single plugin switch in the header");
                form.TestSwitch(false);Check(!form.SwitchChecked&&form.IsDirty,"switching Needs off marks the session dirty");form.TestSwitch(true);Check(form.SwitchChecked&&!form.IsDirty,"switching Needs back on clears the pending change");
                form.SelectTab("needs");
                Check(Children(form).OfType<VectorButton>().Count(x=>x.Symbol=="trash"&&x.Danger)==1,"original need offers the hide action");
                form.TestAddListItem("glass","clothes, 2, medium, 0.5, 0.01");form.TestSetListColumn("furniture",3,"0.5");form.TestSetGlobal("max_demands","5");var needsEffective=new LooseIni(form.LocalEffective());Check(form.LocalResourceCount==3&&form.TestLocalOwned("glass")&&needsEffective.Get("list","glass")=="clothes, 2, medium, 0.5, 0.01"&&needsEffective.Get("list","furniture")=="eletronics, 1, advanced, 0.5, 0.01"&&needsEffective.Get("needs","max_demands")=="5","Needs editor writes personal lines, column overrides and plugin switches into the effective needs.ini");
                string needsWarning="";form.ResourceRemovePrompt=text=>{needsWarning=text;return DialogResult.Cancel;};form.TestRemoveLocalResource("medicine");Check(form.LocalResourceCount==3&&needsWarning.Contains("Spielstand"),"hiding an original need warns about saved games and honours Cancel");form.ResourceRemovePrompt=text=>DialogResult.OK;form.TestRemoveLocalResource("medicine");Check(form.LocalResourceCount==2&&form.LocalHidden().Contains("medicine")&&Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="unhide:medicine"),"hidden original is listed with a show-again action");
                Check(form.TestSaveLocal(()=>{})&&new LooseIni(SafeFiles.Text(Path.Combine(plugins,"needs.ini"))).Get("list","medicine")==null&&SafeFiles.HashFile(Path.Combine(plugins,"needs.dll"))==needsDllHash,"Needs settings save the effective INI without replacing the DLL");
                form.TestSwitch(false);Check(form.TestSaveLocal(()=>{})&&new Ini(SafeFiles.Text(Path.Combine(loader,"tesmioloader.ini"))).Get("plugins","needs")=="0"&&!form.Activity(form.SelectedSource),"switching Needs off writes [plugins] needs = 0 and turns the activity light off");
                form.TestSwitch(true);Check(form.TestSaveLocal(()=>{})&&new Ini(SafeFiles.Text(Path.Combine(loader,"tesmioloader.ini"))).Get("plugins","needs")=="1"&&form.Activity(form.SelectedSource),"switching Needs back on restores the loader entry and the activity light");
                using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,form.Width,form.Height));image.Save(Path.Combine(root,"ui-needs-master-detail.png"));}
                form.TestSearch("Deposits");form.SelectIndex(0);Application.DoEvents();Check(form.HasLocalResourceEditor&&form.LocalResourceCount==2&&form.TabCount==2&&form.SelectedTabId=="deposits"&&form.SwitchVisible&&form.SwitchChecked&&form.Activity(form.SelectedSource),"local Deposits editor is discovered from its keyed_sections schema and opens on the deposits tab");
                Check(Children(form).OfType<Card>().Single().HeaderText=="Vorkommen"&&Children(form).OfType<ComboBox>().Any(x=>x.AccessibleName=="item:map")&&Children(form).OfType<ToggleSwitch>().Any(x=>x.AccessibleName=="item:minimap")&&Children(form).OfType<TextBox>().Any(x=>x.AccessibleName=="item:token")&&Children(form).OfType<VectorButton>().Count(x=>x.Symbol=="trash"&&x.Danger)==1,"deposit details show every key with the matching input and the hide action");
                Check(form.LocalNextValue("type")=="12","the add dialog proposes the next free type number");
                form.TestAddSection("glass",new Dictionary<string,string>{{"token","$TYPE_MINE_GLASS"},{"type","12"},{"map","auto"},{"radius","ore"},{"icon","glass"},{"minimap","1"},{"editor","glass"}});var depositsEffective=new LooseIni(form.LocalEffective());Check(form.LocalResourceCount==3&&form.TestLocalOwned("glass")&&depositsEffective.Get("glass","token")=="$TYPE_MINE_GLASS"&&depositsEffective.Get("glass","type")=="12"&&form.IsDirty,"a personal deposit becomes a new section in the effective deposits.ini");
                bool duplicateRejected=false;try{form.TestAddSection("dup",new Dictionary<string,string>{{"token","$TYPE_MINE_DUP"},{"type","12"}});}catch(Exception){duplicateRejected=true;}Check(duplicateRejected&&form.LocalResourceCount==3,"a taken type number is refused with an error");
                form.SelectTab("general");Check(Children(form).OfType<Card>().Single().HeaderText=="Plugin-Einstellungen"&&Children(form).OfType<ToggleSwitch>().Count(x=>(x.AccessibleName??"").StartsWith("global:"))==3&&Children(form).OfType<Label>().Any(x=>x.Text.Contains("Spielstand")),"general tab shows the three plugin switches and the saved-game notice");
                form.SelectTab("deposits");form.ResourceRemovePrompt=text=>DialogResult.OK;form.TestRemoveLocalResource("copper");Check(form.LocalResourceCount==2&&form.LocalHidden().Contains("copper"),"an original deposit can be hidden");
                Check(form.TestSaveLocal(()=>{})&&!new LooseIni(SafeFiles.Text(Path.Combine(plugins,"deposits.ini"))).HasSection("copper")&&new LooseIni(SafeFiles.Text(Path.Combine(plugins,"deposits.ini"))).Get("glass","type")=="12"&&SafeFiles.HashFile(Path.Combine(plugins,"deposits.dll"))==depositsDllHash,"Deposits settings save the effective INI without replacing the DLL");
                using(var image=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(image,new Rectangle(0,0,form.Width,form.Height));image.Save(Path.Combine(root,"ui-deposits-master-detail.png"));}

                // 0.5.5: the SML buildings tab. It lists what another generator wrote into
                // media_soviet\workshop_wip and offers the one write there is - the missing owner
                // id, the zero that makes the game report missing Workshop items on every load.
                string wipFolder=Path.Combine(root,"media_soviet","workshop_wip","9188000001"),wipConfig=Path.Combine(wipFolder,"workshopconfig.ini");
                Write(Path.Combine(wipFolder,"tesmioloader.stamp"),"tesmioloader plugins\\buildings.dll generated this folder.\r\nsection=salt_mine donor=bauxite_mine\r\nhash=1\r\n");
                Write(wipConfig,"$ITEM_ID 9188000001\r\n\r\n$OWNER_ID 0\r\n\r\n$OBJECT_BUILDING SaltMine\r\n\r\n$ITEM_NAME \"Salt Mine\"\r\n\r\n$END\r\n");
                WipBuildings.TestOwner="76561198017498697";
                form.TestSearch("Buildings Plus");form.SelectIndex(0);Application.DoEvents();
                Check(form.HasLocalResourceEditor&&form.TabCount==3,"Buildings Plus opens with General, Buildings and the SML buildings tab");

                // 0.5.8: the Buildings tab is a card list - one row per building with its own
                // switch, a state line and the buttons; the fields moved into the dialog.
                form.SelectTab("buildings");Application.DoEvents();
                var cardEdit=Children(form).OfType<Button>().FirstOrDefault(x=>(x.AccessibleName??"")=="card:edit:example");
                var cardState=Children(form).OfType<Label>().FirstOrDefault(x=>(x.AccessibleName??"")=="card:state:example");
                Check(cardEdit!=null&&cardState!=null&&Children(form).OfType<ToggleSwitch>().Any(x=>(x.AccessibleName??"")=="card:toggle:example")&&Children(form).OfType<VectorButton>().Any(x=>(x.AccessibleName??"")=="card:delete:example"),"the buildings tab lists a row per building with switch, state and buttons");
                Check(cardState.Text=="Spender nicht gefunden","a donor that is not on this machine is named in the row, long before the game says anything");
                Write(Path.Combine(root,"media_soviet","buildings_types","shop_clothes.ini"),"$NAME_STR \"Clothes shop\"\r\n$WORKERS_NEEDED 20\r\n");
                form.SelectTab("general");Application.DoEvents();form.SelectTab("buildings");Application.DoEvents();
                cardState=Children(form).OfType<Label>().First(x=>(x.AccessibleName??"")=="card:state:example");
                Check(cardState.Text.StartsWith("Wird beim n"),"with the donor in place the row says the folder is written at the next start");
                // The details tab carries the fields, the lines tab the editor: the donor's
                // building.ini beside your own lines, with what each of yours removes.
                int dialogTabs=0;bool editorThere=false,added=false,keepOffered=false,namedGone=false,takenOnce=false,resultReadOnly=false,noDuplicate=false,overwritten=false,keptBoth=false,cancelled=false,resultLocked=false;
                form.CardDialogPrompt=dialog=>
                {
                    dialogTabs=Children(dialog).OfType<Button>().Count(x=>(x.AccessibleName??"").StartsWith("card-tab:"));
                    var select=dialog.Tag as Action<string>;
                    if(select==null)return DialogResult.Cancel;
                    select("lines");Application.DoEvents();
                    var editor=form.TestDonorEditor;editorThere=editor!=null;
                    if(editor==null)return DialogResult.Cancel;
                    editor.TestType("$WORKERS_NEEDED 150");editor.TestAdd();Application.DoEvents();
                    added=editor.Lines.Contains("$WORKERS_NEEDED 150");
                    editor.TestSelectMine(0);Application.DoEvents();
                    keepOffered=editor.TestHasKeep;
                    // A picture of the dialog: it is never shown in the test, so it has to be
                    // laid out by hand before it can be drawn.
                    dialog.StartPosition=FormStartPosition.Manual;dialog.Location=new Point(-20000,-20000);dialog.Size=new Size(1120,780);
                    dialog.Show();Application.DoEvents();dialog.PerformLayout();Application.DoEvents();
                    using(var image=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(image,new Rectangle(0,0,dialog.Width,dialog.Height));image.Save(Path.Combine(root,"ui-donor-editor.png"));}
                    dialog.Hide();
                    // The name field alone removes the donor's name line, without a line of its own.
                    namedGone=editor.TestDropped("$NAME_STR \"Clothes shop\"");
                    // 0.5.8: taking a donor line that one of your lines already replaces overwrites
                    // that line instead of piling up a second and a third copy of the same token.
                    editor.TestSelectDonor(1);Application.DoEvents();
                    editor.TestTake();Application.DoEvents();
                    takenOnce=editor.Lines.Count()==1&&editor.Lines.Contains("$WORKERS_NEEDED 20");
                    // And the result view is read-only: it shows what will be written, nothing is taken there.
                    editor.TestView(true);Application.DoEvents();
                    editor.TestSelectDonor(0);Application.DoEvents();
                    int before=editor.Lines.Count();editor.TestTake();Application.DoEvents();
                    resultReadOnly=editor.Lines.Count()==before;
                    editor.TestView(false);Application.DoEvents();
                    int dupBefore=editor.Lines.Count();editor.TestType("$WORKERS_NEEDED 20");editor.TestAdd();Application.DoEvents();
                    noDuplicate=editor.Lines.Count()==dupBefore;
                    // 0.5.8: the same setting with another value is not a second line - the editor
                    // asks whether to overwrite, because only one of the two counts in the game.
                    editor.DoublePrompt = t => DialogResult.Yes;
                    editor.TestType("$WORKERS_NEEDED 8");editor.TestAdd();Application.DoEvents();
                    overwritten=editor.Lines.Count()==1&&editor.Lines.Contains("$WORKERS_NEEDED 8");
                    editor.DoublePrompt = t => DialogResult.No;
                    editor.TestType("$WORKERS_NEEDED 15");editor.TestAdd();Application.DoEvents();
                    keptBoth=editor.Lines.Count()==2&&editor.TestWarnings.Any(x=>x.Contains("donor_warn_double"));
                    editor.DoublePrompt = t => DialogResult.Cancel;
                    editor.TestType("$WORKERS_NEEDED 99");editor.TestAdd();Application.DoEvents();
                    cancelled=editor.Lines.Count()==2;
                    editor.DoublePrompt = null;
                    // The result view switches every editing button off, so none of them can report
                    // a reason that does not apply there.
                    editor.TestView(true);Application.DoEvents();
                    resultLocked=!editor.TestEditable;
                    dialog.Show();Application.DoEvents();dialog.PerformLayout();Application.DoEvents();
                    using(var view=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(view,new Rectangle(0,0,dialog.Width,dialog.Height));view.Save(Path.Combine(root,"ui-donor-result.png"));}
                    dialog.Hide();
                    editor.TestView(false);Application.DoEvents();
                    resultLocked=resultLocked&&editor.TestEditable;
                    return DialogResult.Cancel;
                };
                cardEdit.PerformClick();Application.DoEvents();form.CardDialogPrompt=null;
                Check(dialogTabs==2&&editorThere,"the change dialog has a details and a lines tab, and the second one is the line editor ["+dialogTabs+" tabs, editor "+editorThere+"]");
                Check(added&&keepOffered,"a line of yours is added and the editor offers to keep what it removes from the donor");
                Check(namedGone,"the name field alone removes the donor's own name line");
                Check(takenOnce,"taking a donor line twice overwrites your line instead of adding the same token again");
                Check(resultReadOnly,"the result view takes nothing: it only shows what will be written");
                Check(noDuplicate,"Add with a block that is already in the list picks the existing one instead of a copy");
                Check(overwritten,"the same setting with another value overwrites when you say so");
                Check(keptBoth,"a second line is allowed when you insist, and then it is reported");
                Check(cancelled,"Cancel in that question adds nothing");
                Check(resultLocked,"the result view switches the editing buttons off and the donor view switches them back on");


                // 0.5.8: the + opens the very same dialog, only empty - one way for adding and
                // changing instead of two. The entry exists as a placeholder while the dialog is
                // open and gets its name when Apply is pressed.
                bool idBoxThere=false,waitsForDonor=false;
                var addButton=Children(form).OfType<VectorButton>().FirstOrDefault(x=>(x.AccessibleName??"")=="card:add");
                Check(addButton!=null,"the card list carries the + in its header");
                form.CardDialogPrompt=dialog=>
                {
                    var select=dialog.Tag as Action<string>;
                    idBoxThere=Children(dialog).OfType<TextBox>().Any(x=>(x.AccessibleName??"")=="card-dialog:id");
                    select("lines");Application.DoEvents();
                    waitsForDonor=form.TestDonorEditor==null;
                    select("global");Application.DoEvents();
                    var box=Children(dialog).OfType<TextBox>().FirstOrDefault(x=>(x.AccessibleName??"")=="card-dialog:id");
                    if(box!=null)box.Text="Salt Mine";
                    dialog.StartPosition=FormStartPosition.Manual;dialog.Location=new Point(-20000,-20000);dialog.Size=new Size(1120,780);
                    dialog.Show();Application.DoEvents();dialog.PerformLayout();Application.DoEvents();
                    using(var shot=new Bitmap(dialog.Width,dialog.Height)){dialog.DrawToBitmap(shot,new Rectangle(0,0,dialog.Width,dialog.Height));shot.Save(Path.Combine(root,"ui-card-add.png"));}
                    dialog.Hide();
                    var ok=Children(dialog).OfType<Button>().First(x=>(x.AccessibleName??"")=="card-dialog:apply");
                    ((Action)ok.Tag)();
                    return dialog.DialogResult;
                };
                addButton.PerformTestClick();Application.DoEvents();form.CardDialogPrompt=null;
                Check(idBoxThere,"a new entry asks for its name in the dialog instead of in a second window");
                Check(waitsForDonor,"the lines tab waits for a donor instead of showing an empty editor");
                Check(Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="card:edit:salt_mine"),"Apply creates the section under the name that was typed");
                Check(!Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="card:edit:new_entry"),"the placeholder the dialog worked on is gone");
                addButton=Children(form).OfType<VectorButton>().First(x=>(x.AccessibleName??"")=="card:add");
                form.CardDialogPrompt=dialog=>DialogResult.Cancel;
                addButton.PerformTestClick();Application.DoEvents();form.CardDialogPrompt=null;
                Check(!Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="card:edit:new_entry"),"Cancel leaves no half-made entry behind");
                form.SelectTab("sml");Application.DoEvents();
                Check(Children(form).OfType<Card>().Single().HeaderText.StartsWith("Von Soviet Mod Loader")&&Children(form).OfType<Label>().Any(x=>x.Text.Contains("9188000001")&&x.Text.Contains("SaltMine")),"the SML tab lists the generated folder with id, object and origin");
                Check(Children(form).OfType<Label>().Any(x=>x.Text=="Salt Mine")&&Children(form).OfType<Label>().Any(x=>x.Text.Contains("kein Paket mehr")),"the name comes from the folder, and a folder no package declares is reported as stale");
                var fixOwner=Children(form).OfType<Button>().FirstOrDefault(x=>(x.AccessibleName??"")=="wip:fix-owner");
                Check(fixOwner!=null,"a missing owner id offers the fill-in button");
                fixOwner.PerformClick();Application.DoEvents();
                Check(SafeFiles.Text(wipConfig).Contains("$OWNER_ID 76561198017498697")&&SafeFiles.Text(wipConfig).Contains("$ITEM_NAME \"Salt Mine\"")&&!Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="wip:fix-owner"),"the button writes the owner, leaves the rest of the file alone and then has nothing left to do");
                Check(File.Exists(Path.Combine(wipFolder,"tesmioloader.stamp")),"the stamp of the other generator is never touched");

                // 0.5.5 step 3: personal changes to the generated building.ini, kept as operations.
                string wipBuilding=Path.Combine(wipFolder,"SaltMine","building.ini");
                // The game aligns its values with runs of spaces; the fixture does the same, so the
                // change list is measured the way it really looks.
                Write(wipBuilding,"; generated by tesmioloader plugins\\buildings.dll\r\n$NAME_STR \"Salt Mine\"\r\n$TYPE_FACTORY\r\n$WORKERS_NEEDED 10\r\n$CONSUMPTION eletric                0.250\r\n$PRODUCTION rocksalt              3.000\r\n");
                form.SelectTab("sml");Application.DoEvents();
                Check(Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="wip:edit:9188000001"),"every building offers the Change button");
                form.TestWipEdit("9188000001",window=>
                {
                    Check(window.LineCount==6&&window.ChangeCount==0,"the window lists the generator's lines and starts without changes");
                    window.TestReplace(3,"$WORKERS_NEEDED 25");window.TestAdd("$POLLUTION_SMALL");
                    Check(window.ChangeCount==2,"replace and add are recorded as two changes");
                    // 0.5.6: the column padding is squeezed for the eye, and only for the eye.
                    window.TestReplace(4,"$CONSUMPTION eletric                0.400");
                    Check(window.ChangeText(2)=="~ $CONSUMPTION eletric 0.250  ->  $CONSUMPTION eletric 0.400","the change list collapses the alignment spaces");
                    Check(window.ChangeExtent>0,"and the list can scroll sideways when a line is longer than the window");
                    window.TestSearch("PRODUCTION");Check(window.LineCount==1,"the search narrows the list of lines");
                    window.TestSearch("");
                    window.ShowInTaskbar=false;window.StartPosition=FormStartPosition.Manual;window.Location=new Point(-20000,-20000);window.Show();Application.DoEvents();
                    using(var shot=new Bitmap(window.Width,window.Height)){window.DrawToBitmap(shot,new Rectangle(0,0,window.Width,window.Height));shot.Save(Path.Combine(root,"ui-wip-editor.png"));}
                    window.Hide();
                });
                Application.DoEvents();
                string written=SafeFiles.Text(wipBuilding);
                Check(written.Contains("$WORKERS_NEEDED 25")&&!written.Contains("$WORKERS_NEEDED 10")&&written.TrimEnd().EndsWith("$POLLUTION_SMALL"),"applying writes the changed building.ini");
                Check(Children(form).OfType<Label>().Any(x=>x.Text.Contains("3 Änderungen von dir")),"the row says how many changes the building carries");
                // The generator writes anew, with an improvement of its own: RMM puts the changes
                // back in and keeps what the package changed.
                Write(wipBuilding,"; generated by tesmioloader plugins\\buildings.dll\r\n$NAME_STR \"Salt Mine\"\r\n$TYPE_FACTORY\r\n$WORKERS_NEEDED 10\r\n$PRODUCTION rocksalt 4.2\r\n");
                Write(Path.Combine(wipFolder,"tesmioloader.stamp"),"tesmioloader plugins\\buildings.dll generated this folder.\r\nsection=salt_mine donor=bauxite_mine\r\nhash=99\r\n");
                form.SelectTab("general");form.SelectTab("sml");Application.DoEvents();
                written=SafeFiles.Text(wipBuilding);
                Check(written.Contains("$WORKERS_NEEDED 25")&&written.Contains("$PRODUCTION rocksalt 4.2"),"after a regeneration the changes are back and the new recipe survived");
                Check(Children(form).OfType<Label>().Any(x=>x.Text.Contains("wieder drin")),"and the blue notice says it happened");
                form.TestWipEdit("9188000001",window=>window.TestDrop(0));
                form.TestWipEdit("9188000001",window=>window.TestDrop(0));
                form.TestWipEdit("9188000001",window=>window.TestDrop(0));
                Application.DoEvents();
                Check(SafeFiles.Text(wipBuilding).Contains("$WORKERS_NEEDED 10")&&!SafeFiles.Text(wipBuilding).Contains("$POLLUTION_SMALL"),"dropping every change puts the generator's file back");
                WipBuildings.TestOwner=null;
                form.TestSearch("Vehicle");form.SelectIndex(0);Application.DoEvents();form.SelectTab("general");Check(Children(form).OfType<Button>().Count(x=>(x.AccessibleName??"").StartsWith("link:"))==1&&Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"")=="link:readme"&&x.Enabled),"general tab shows exactly the guide of the active language");form.SelectTab("resources");Check(!Children(form).OfType<Button>().Any(x=>(x.AccessibleName??"").StartsWith("link:"))&&!Children(form).OfType<Card>().Any(x=>x.HeaderText=="Hinweise"),"other tabs carry neither guides nor the notices card");
                // 0.4.15: number inputs keep the reset button's column free at every window size (user report: cut off after resizing).
                {
                    Func<bool> inside=()=>{var boxes=Children(form).OfType<NumberInput>().Where(x=>x.Parent!=null).ToList();return boxes.Count>0&&boxes.All(x=>x.Right<=x.Parent.ClientSize.Width-30);};
                    form.SelectTab("general");Application.DoEvents();
                    Check(inside(),"number inputs leave room for the reset button after the tab is built");
                    form.Width+=300;Application.DoEvents();Check(inside(),"number inputs still fit after widening the window");
                    form.Width-=500;Application.DoEvents();Check(inside(),"number inputs still fit after narrowing the window");
                    form.WindowState=FormWindowState.Maximized;Application.DoEvents();Check(inside(),"number inputs still fit maximized");
                    form.WindowState=FormWindowState.Normal;Application.DoEvents();Check(inside(),"number inputs still fit after restoring");
                }
                form.TestSearch("ZZ Other");form.SelectIndex(0);Application.DoEvents();Check(form.SelectedSource==unsupported&&!form.HasEditor,"rejected Workshop package remains visible but cannot deploy");
            }

            var againStore=new UiStateStore(profile);using(var form=new MainForm(againStore.Load(defaults,notes),againStore,true))
            {HiddenShow(form);Check(form.SelectedSource==unsupported,"second launch remembers selected package tab");form.TestLanguage("en");Check(form.LanguageGlyph=="text:EN","language selection remains general application state");}

            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=package,Language="en"},null,false))
            {HiddenShow(form);form.PendingPrompt=()=>DialogResult.No;Check(form.DisplayedValue("resources/count")=="0","deleted collection remains empty after reopening");form.TestEdit("general/enabled","1");Check(form.StatusDetailText.Contains("“Resources” tab")&&form.StatusDetailText.Contains("under “Materials”")&&form.StatusDetailText.Contains("Use +")&&!form.StatusDetailText.Contains("Reiter"),"English validation names the translated schema-derived tab and group");form.TestEdit("general/enabled","0");bool started=false;Check(form.TestSaveAndLaunch(()=>{},()=>started=true)&&started&&!form.Visible,"Save + Start closes Settings after successful launch");}
            // 0.5.9: with "keep the window open" the very same path starts the game and leaves
            // the window standing - the footer says so instead of the program disappearing.
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=package,Language="en"},null,false))
            {
                HiddenShow(form);
                bool started=false; LauncherOptions.KeepOpen=true;
                try
                {
                    Check(form.TestSaveAndLaunch(()=>{},()=>started=true)&&started&&form.Visible,"Save + Start keeps the window when keep_open is on");
                    Check(form.StatusDetailText.Contains("stays open"),"and the footer says the window is waiting for you: "+form.StatusDetailText);
                }
                finally{LauncherOptions.KeepOpen=false;}
            }
            Check(SafeFiles.HashFile(resourceIni)==resourcesHash,"all collection operations leave provider INI byte-for-byte unchanged");
            using(var icon=Icon.ExtractAssociatedIcon(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"rmm.exe"))){Check(icon!=null,"EXE contains native application icon");}
            // 0.4.93: the options window. What it changes must land in the saved view (that layer
            // wins over rmm.ini), take effect at once, and say where the value differs from rmm.ini.
            {
                AppOptions.IniLauncherWindow="0"; AppOptions.IniWatchSeconds="15"; AppOptions.IniVersionCheck="1";
                LauncherOptions.ShowWindow=false; LauncherOptions.WatchSeconds=15; GameVersion.CheckEnabled=true;
                var view=new UiState{Build=loader,Language="de"};
                string report=null;
                using(var window=new OptionsWindow(new Language("de"),view,()=>"Republic Mod Manager 0.4.93\nSteam: Ready",new Font("Segoe UI",10),null))
                {
                    window.Copied=t=>report=t;
                    Check(window.TestNote("launcher_window")==""&&window.TestNote("watch_seconds")=="","no note while everything matches rmm.ini");
                    window.TestLauncherWindow=true;
                    Check(view.LauncherWindow=="1"&&LauncherOptions.ShowWindow&&window.Changed,"the launcher switch is staged and takes effect at once");
                    Check(window.TestNote("launcher_window").Contains("rmm.ini")&&window.TestNote("launcher_window").Contains("AUS"),"a value differing from rmm.ini names the shipped default");
                    window.TestWatch="0";
                    Check(view.WatchSeconds=="0"&&LauncherOptions.WatchSeconds==0&&window.TestNote("watch_seconds").Contains("15"),"the watch seconds are staged and the rmm.ini default is shown");
                    window.TestVersionCheck=false;
                    Check(view.VersionCheck=="0"&&!GameVersion.CheckEnabled,"the version check switch is staged and takes effect at once");
                    // 0.5.9: keep the window open - same three steps as every other option.
                    Check(window.TestNote("keep_open")=="","no note while the switch matches rmm.ini");
                    window.TestKeepOpen=true;
                    Check(view.KeepOpen=="1"&&LauncherOptions.KeepOpen&&window.TestNote("keep_open").Contains("rmm.ini"),"the keep-open switch is staged, takes effect at once and names the shipped default");
                    window.TestKeepOpen=false; LauncherOptions.KeepOpen=false;
                    // 0.5.20: the loader's log level - a four-position switch whose note says what
                    // the position means, and a write that goes into tesmioloader.ini, not rmm.ini.
                    {
                        string loaderIni=Path.Combine(loader,"tesmioloader.ini");
                        string keptIni=SafeFiles.Text(loaderIni);
                        Check(LoaderLog.Level(loader)==2&&LoaderLog.Repeat(loader)==3,"without the keys the loader defaults are read: level 2, repeat 3");
                        Check(window.TestLogLevel==2&&window.TestNote("log_level")=="Fehler und Warnungen","the switch starts on the current level and says what it means");
                        window.TestLogLevel=0;
                        Check(window.TestNote("log_level")=="Aus - nichts vom Spiel"&&window.Changed,"moving the knob changes the wording under it");
                        window.TestLogLevel=3;
                        Check(window.TestNote("log_level")=="Alles, auch der Ladefortschritt","the last position is everything");
                        window.TestLogLevel=1;
                        window.SaveLoaderLog();
                        Check(LoaderLog.Level(loader)==1&&LoaderLog.Repeat(loader)==3,"the level lands in tesmioloader.ini");
                        string now=SafeFiles.Text(loaderIni);
                        Check(now.Contains("[plugins]")&&now.Contains("log_game")&&now.Split('\n').Length>=keptIni.Split('\n').Length,"the launcher's own file keeps its sections");
                        window.SaveLoaderLog();
                        Check(SafeFiles.Text(loaderIni)==now,"saving again without a change rewrites nothing");
                    }
                    window.TestCopy();
                    Check(report!=null&&report.Contains("Republic Mod Manager"),"the report goes through the copy hook");
                    // 0.4.94: the reset asks before it touches anything, and the word window only
                    // opens the door when the word actually fits.
                    ResetPlan asked=null; bool applied=false;
                    window.Planner=everything=>{var p=new ResetPlan{Everything=everything};p.Files.Add("user_config\\x.ini");return p;};
                    window.Applier=p=>{applied=true;return "backup = "+Path.Combine(root,"reset-backup");};
                    window.ConfirmHook=p=>{asked=p;return false;};
                    window.TestReset(true);
                    Check(asked!=null&&asked.Everything&&!applied&&!window.ResetDone,"the reset asks first and does nothing when the answer is no");
                    window.ConfirmHook=p=>true;
                    window.TestReset(false);
                    Check(applied&&window.ResetDone,"a yes carries the plan out");
                }
                using(var word=new ResetWordWindow(new Language("de"),new Font("Segoe UI",10),null))
                {
                    Check(!word.TestReady,"the final button starts locked");
                    word.TestType("löschen");Check(word.TestReady,"the typed word unlocks it, upper or lower case");
                    word.TestType("lösche");Check(!word.TestReady,"a half word locks it again");
                }
                // Saved and read back: the view wins over the rmm.ini defaults.
                string viewPath=Path.Combine(root,"optionsview","state.ini"); Directory.CreateDirectory(Path.GetDirectoryName(viewPath));
                var optionStore=new UiStateStore(viewPath); optionStore.Load(new UiState(),new List<string>()); optionStore.Save(view);
                LauncherOptions.ShowWindow=false; LauncherOptions.WatchSeconds=15; GameVersion.CheckEnabled=true;
                var back=new UiStateStore(viewPath).Load(new UiState{Build=loader},new List<string>());
                AppOptions.Apply(back);
                Check(LauncherOptions.ShowWindow&&LauncherOptions.WatchSeconds==0&&!GameVersion.CheckEnabled,"the saved options win over the rmm.ini defaults");
                LauncherOptions.ShowWindow=false; LauncherOptions.WatchSeconds=15; GameVersion.CheckEnabled=true;
            }
            // 0.4.95: every dialog dims what is behind it. The backdrop is an owned form and it
            // has to be gone again the moment the dialog closes.
            {
                using(var owner=new Form{ShowInTaskbar=false,StartPosition=FormStartPosition.Manual,Location=new Point(-20000,-20000),Size=new Size(900,700)})
                {
                    owner.Show();Application.DoEvents();
                    bool dimmed=false;int behind=0;
                    using(var dialog=new Form{ShowInTaskbar=false,StartPosition=FormStartPosition.Manual,Location=new Point(-20000,-20000),Size=new Size(300,200)})
                    {
                        dialog.Shown+=(s,e)=>
                        {
                            foreach(Form owned in owner.OwnedForms) if(owned!=dialog&&owned.BackColor==Color.Black&&owned.Opacity<0.9) {dimmed=true;behind=owned.Bounds==owner.Bounds?1:0;}
                            dialog.DialogResult=DialogResult.OK;dialog.Close();
                        };
                        Check(Theme.Modal(owner,dialog)==DialogResult.OK,"the dimmed dialog returns its result");
                    }
                    Check(dimmed&&behind==1,"a dark backdrop covers the owner while the dialog is open");
                    Check(owner.OwnedForms.Length==0,"the backdrop is gone when the dialog closes");
                    owner.Close();
                }
            }
            // 0.5.8: the commands editor of a Vanilla Buildings rule set. Driven on its own, with a
            // target file handed in - the wiring into the dialog is the same as for the donor editor.
            {
                var file=new List<string>{"$NAME_STR \"Shop\"","$WORKERS_NEEDED 20","$STORAGE RESOURCE_TRANSPORT_COVERED 40","end"};
                using(var editor=new TargetLinesEditor(new Language("de"),new[]{@"buildings_types\shop.ini",@"2496571917\x\building.ini"},
                    f=>f.StartsWith("buildings_types")?file:null,new string[0],new string[0],new string[0],new string[0],new Font("Segoe UI",10)))
                {
                    Check(editor.TestEditable,"an existing target file leaves the buttons on");
                    editor.TestSelectLine(1);editor.TestType("$WORKERS_NEEDED 30");editor.TestReplace();
                    Check(editor.Replace.Count()==1&&editor.Replace.First()=="$WORKERS_NEEDED 20 | $WORKERS_NEEDED 30","picking the line and typing the new one writes the replace command");
                    editor.TestSelectLine(1);editor.TestType("$LIFESPAN 4000");editor.TestInsert(true);
                    Check(editor.Insert.First()=="1 | $WORKERS_NEEDED 20 | $LIFESPAN 4000","insert carries the position, the picked anchor and the new line");
                    editor.TestType("$CITIZEN_ABLE_SERVE 6");editor.TestAdd();
                    Check(editor.Add.First()=="$CITIZEN_ABLE_SERVE 6"&&!editor.TestProblems.Any(),"an appended line that is not in the file yet fits");
                    editor.TestType("$WORKERS_NEEDED 20");editor.TestAdd();
                    Check(editor.TestProblems.Any(x=>x=="target_problem_exists"),"appending a line the file already has is reported");
                    editor.TestSelectCommand(1);editor.TestDrop();
                    Check(!editor.Add.Contains("$WORKERS_NEEDED 20"),"a command can be thrown out again");
                    // The second view: the file the game will read, and nothing to change there.
                    editor.TestView(true);
                    Check(!editor.TestEditable,"the result view switches every button off");
                    Check(editor.TestResult.Contains("$WORKERS_NEEDED 30")&&!editor.TestResult.Contains("$WORKERS_NEEDED 20"),"the result carries the replaced line instead of the old one");
                    Check(editor.TestResult.Contains("$LIFESPAN 4000")&&editor.TestResult.Contains("$CITIZEN_ABLE_SERVE 6"),"the inserted and the appended line stand in the result");
                    Check(editor.TestResultCommand(editor.TestResult.ToList().IndexOf("$LIFESPAN 4000"))>=0,"a produced line says which command made it");
                    editor.TestView(false);
                    Check(editor.TestEditable,"back in the original view the buttons work again");
                    // A picture of it: the control has to sit in a window that was shown once.
                    using(var frame=new Form{StartPosition=FormStartPosition.Manual,Location=new Point(-20000,-20000),Size=new Size(1080,660),Font=new Font("Segoe UI",10),BackColor=Color.White})
                    {
                        editor.TestSelectCommand(0);frame.Controls.Add(editor);
                        frame.Show();Application.DoEvents();frame.PerformLayout();Application.DoEvents();
                        using(var shot=new Bitmap(frame.Width,frame.Height)){frame.DrawToBitmap(shot,new Rectangle(0,0,frame.Width,frame.Height));shot.Save(Path.Combine(root,"ui-target-editor.png"));}
                        editor.TestView(true);Application.DoEvents();
                        using(var shot=new Bitmap(frame.Width,frame.Height)){frame.DrawToBitmap(shot,new Rectangle(0,0,frame.Width,frame.Height));shot.Save(Path.Combine(root,"ui-target-result.png"));}
                        editor.TestView(false);Application.DoEvents();
                        frame.Hide();frame.Controls.Remove(editor);
                    }
                    editor.TestTarget(1);
                    Check(!editor.TestEditable,"a target file that is not on this machine switches the buttons off");
                }
                // 0.5.8: connections in the same editor, taken as a block; and the box that checks
                // every target instead of the chosen one.
                var linked=new List<string>{"$NAME_STR \"Shop\"","$CONNECTION_ROAD","-19.12 0.36 -27.02","-19.12 0.36 -30.02","$WORKERS_NEEDED 20","end"};
                var second=new List<string>{"$NAME_STR \"Other\"","$WORKERS_NEEDED 25","end"};
                using(var editor=new TargetLinesEditor(new Language("de"),new[]{"a.ini","b.ini"},
                    f=>f=="a.ini"?linked:second,new string[0],new string[0],new string[0],new string[0],new string[0],new string[0],new string[0],new Font("Segoe UI",10)))
                {
                    editor.TestSelectLine(2);
                    Check(editor.TestTyped.Contains("$CONNECTION_ROAD")&&editor.TestTyped.Contains("-19.12 0.36 -30.02"),"clicking a point line puts the whole connection block into the box");
                    editor.TestRemove();
                    Check(editor.RemoveLink.Count()==1&&editor.RemoveLink.First().StartsWith("$CONNECTION_ROAD | -19.12"),"Remove on a connection writes remove_connection with its points");
                    editor.TestType("$CONNECTION_PEDESTRIAN\n1 0 1\n2 0 2");editor.TestAdd();
                    Check(editor.AddLink.First()=="$CONNECTION_PEDESTRIAN | 1 0 1 | 2 0 2","a typed connection block becomes add_connection");
                    Check(!editor.TestProblems.Any(),"both connection commands fit this file");
                    // The rule set's second target has none of it: the box says so.
                    editor.TestEverywhere(true);
                    Check(editor.TestSpread("remove_connection",0)==1,"with the box ticked the command is counted against every target");
                }
            }
            // 0.23.0: profiles and restore points window, driven through its test hooks.
            using(var window=new ProfilesWindow(new Language("de"),loader,new Font("Segoe UI",10),null))
            {
                window.NamePrompt=()=>new KeyValuePair<string,string>("Testprofil","vor dem Umbau");window.Confirm=q=>DialogResult.Yes;window.Guard=()=>{};
                Check(window.ProfileCount==0&&window.PointCount>0&&window.ProfileInfoText.Contains("Noch kein Profil"),"window starts without profiles and lists the restore points of earlier saves");
                window.TestSave();Check(window.ProfileCount==1&&window.SummaryText.Contains("Testprofil")&&window.ProfileFileRows>0,"saving creates a profile from the loader configuration");
                string loaderIni=Path.Combine(loader,"tesmioloader.ini");string original=File.ReadAllText(loaderIni,SafeFiles.Utf8);Write(loaderIni,original+"zeta=1\n");
                Check(window.ProfileInfoText.Contains("abweichend: 0"),"details are refreshed only on demand");window.Reload();Check(window.ProfileInfoText.Contains("abweichend: 1"),"a changed file shows as differing");
                window.TestApply();Check(File.ReadAllText(loaderIni,SafeFiles.Utf8)==original&&window.Changed,"applying the profile writes the captured file back");
                window.TestShowPoints();window.TestSelectPoint("profile");Check(window.PointsShown&&window.PointCount>0,"the profile application left its own restore point");
                window.TestRestore();Check(File.ReadAllText(loaderIni,SafeFiles.Utf8)==original+"zeta=1\n","restoring the point brings the edited file back");
                Write(loaderIni,original);window.TestDelete();Check(window.ProfileCount==0,"deleting removes the profile");
            }
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=package,Language="de"},null,false))
            {HiddenShow(form);Check(Children(form).OfType<SidebarButton>().Count()==7,"sidebar offers folders, start check, log, profiles, language, settings and refresh");
             // 0.4.88: the state filter beside the search box. 0.4.90: an entry that does not match
             // is really gone - only unsaved work stays under every filter, marked by the amber dot.
             Check(Children(form).OfType<Label>().Count(x=>(x.AccessibleName??"").StartsWith("filter:"))==4,"the list offers four state filters");
             int all=form.ModCount; form.TestFilter("problems");
             Check(form.ModCount<all&&!form.ShownListed,"a filter narrows the list and drops the shown entry when it has no problem");
             form.TestFilter("all"); Check(form.ModCount==all&&form.ShownListed,"'all' brings everything back");
             form.TestEdit("general/debug","1"); form.TestFilter("problems");
             Check(form.ShownListed&&form.DirtyMarks==1,"an entry with unsaved changes stays listed under every filter");
             form.TestEdit("general/debug","0"); form.TestFilter("all");}
            // 0.32.0: the building picker, driven through its hooks against a fake game folder.
            {
                string game=Path.Combine(root,"fake-game"),ms=Path.Combine(game,"media_soviet");
                Write(Path.Combine(ms,"buildings_types","cement_plant.ini"),"$OBSOLETE\r\n$TYPE_FACTORY\r\n");Write(Path.Combine(ms,"buildings_types","cement_plant_v3.ini"),"$TYPE_FACTORY\r\n");Write(Path.Combine(ms,"buildings_types","flat.ini"),"$TYPE_LIVING\r\n");Write(Path.Combine(ms,"dlc3","buildings","ts_small","building.ini"),"$TYPE_GARBAGE_OFFICE\r\n");
                string fakeBuild=Path.Combine(game,"tesmioloader","build");Directory.CreateDirectory(fakeBuild);
                var used=new Dictionary<string,string>{{"buildings_types\\flat.ini","other_rules"}};
                using(var picker=new BuildingPickerWindow(new Language("de"),fakeBuild,"",new[]{"buildings_types\\cement_plant_v3.ini","2496571917\\gone\\building.ini"},used,new Font("Segoe UI",10),null))
                {
                    Check(picker.Total==4&&picker.SelectedCount==2&&picker.VisibleCount==2&&picker.CounterText.Contains("2 ausgewählt"),"picker lists the game buildings, keeps unknown targets and hides obsolete ones by default");
                    picker.TestObsolete(true);Check(picker.VisibleCount==3,"the obsolete filter reveals superseded versions");
                    picker.TestSearch("ts_");Check(picker.VisibleCount==1,"search narrows the available list");picker.TestSearch("");
                    picker.TestSelect("dlc3\\buildings\\ts_small\\building.ini");picker.TestUnselect("2496571917\\gone\\building.ini");
                    var result=picker.TestResult();Check(result.Count==2&&result[0]=="buildings_types\\cement_plant_v3.ini"&&result[1]=="dlc3\\buildings\\ts_small\\building.ini","selection keeps the existing order and appends new targets");
                }
                // 0.4.80: the donor variant - base-game buildings_types only, by plain name, one choice.
                using(var donors=new BuildingPickerWindow(new Language("de"),fakeBuild,"",new[]{"cement_plant_v3"},null,new Font("Segoe UI",10),null,true))
                {
                    Check(donors.Total==3&&donors.SelectedCount==1&&donors.TestResult()[0]=="cement_plant_v3","the donor picker lists only base-game buildings, by plain name");
                    donors.TestSelect("flat");var chosen=donors.TestResult();
                    Check(chosen.Count==1&&chosen[0]=="flat","picking a second donor replaces the first one");
                }
                // 0.4.80: the lines of a donor building.ini, for taking one over.
                using(var lines=new DonorLinesWindow(new Language("de"),fakeBuild,"cement_plant_v3",new Font("Segoe UI",10),null))
                {
                    Check(lines.LineCount==1,"the donor line window lists the donor's token lines");
                    lines.TestSearch("nothing");Check(lines.LineCount==0,"the search narrows the donor lines");lines.TestSearch("");
                    lines.TestChoose(0,"$TYPE_FACTORY_X");Check(lines.Result=="$TYPE_FACTORY_X","the edited line is what the field receives");
                }
            }
            // 0.4.21: the game-text picker on a synthetic language file.
            {
                string textGame=Path.Combine(root,"text-game");Directory.CreateDirectory(Path.Combine(textGame,"media_soviet"));
                var texts=new List<KeyValuePair<int,string>>{new KeyValuePair<int,string>(5,"kurz"),new KeyValuePair<int,string>(1970,"Zeigt den Bereich an, in dem ein moegliches Problem auf der Route besteht!\n(Das ist der naechstgelegene Punkt zum Ziel)"),new KeyValuePair<int,string>(2412,"Ein anderer sehr langer Hinweis, der ebenfalls aus dem Fenster laeuft und umgebrochen werden koennte")};
                File.WriteAllBytes(Path.Combine(textGame,"media_soviet","sovietGerman.btf"),GameTexts.Build(texts));File.WriteAllBytes(Path.Combine(textGame,"media_soviet","sovietEnglish.btf"),GameTexts.Build(texts.Take(1).ToList()));Write(Path.Combine(textGame,"media_soviet","config.ini"),"$TEXT LANGUAGE2 auto\n");
                using(var picker=new GameTextPickerWindow(new Language("de"),Path.Combine(textGame,"tesmioloader","build"),new[]{5},new Font("Segoe UI",10),null))
                {
                    Check(picker.SelectedLanguage=="German"&&picker.LoadedCount==3,"text picker loads the German file for the German UI");
                    Check(picker.VisibleCount==2,"text picker hides used ids and short lines at the default minimum of 60");
                    picker.TestSetFilter("",0);Check(picker.VisibleCount==2,"text picker shows every unused text at minimum 0");
                    picker.TestSetFilter("Route",0);Check(picker.VisibleCount==1,"text picker filters by text");
                    picker.TestSetFilter("2412",0);Check(picker.VisibleCount==1,"text picker finds an id typed into the search");
                    picker.TestSetFilter("",80);Check(picker.VisibleCount==1,"text picker filters by the longest line");
                    picker.TestSetFilter("",0);picker.TestSelect(1970);Check(picker.Result==1970,"text picker returns the chosen id");
                }
                using(var picker=new GameTextPickerWindow(new Language("en"),Path.Combine(textGame,"tesmioloader","build"),null,new Font("Segoe UI",10),null))
                    Check(picker.SelectedLanguage=="English"&&picker.LoadedCount==1,"text picker follows the English UI when the game says auto");
                using(var picker=new GameTextPickerWindow(new Language("de"),Path.Combine(root,"nowhere","tesmioloader","build"),null,new Font("Segoe UI",10),null))
                    Check(picker.LoadedCount==0&&picker.VisibleCount==0,"text picker without a game folder stays empty instead of failing");
            }
            // 0.4.42: the line picker composes edit commands from the lines of a Vanilla research block.
            {
                var entry=GameResearch.Parse(new[]{"$RESEARCH faculty_geology","$UNLOCK_RESEARCH uranium_study","$UNLOCK_RESEARCH bauxite_study","$COST 1500","$RESEARCH_ADD"})[0];
                string noGame=Path.Combine(root,"nowhere","tesmioloader","build");
                using(var lines=new ResearchLinesWindow(new Language("de"),noGame,"faculty_geology",entry,"line_anchor",null,new Font("Segoe UI",10),null))
                {Check(lines.LineCount==3,"line picker lists the editable lines of the block");lines.TestChoose(0,1,null);Check(lines.Result=="$UNLOCK_RESEARCH uranium_study | $UNLOCK_RESEARCH bauxite_study","line picker builds a move command from line and anchor");}
                using(var lines=new ResearchLinesWindow(new Language("de"),noGame,"faculty_geology",entry,"line_edit",null,new Font("Segoe UI",10),null))
                {lines.TestChoose(2,-1,"$COST 1800");Check(lines.Result=="$COST 1500 | $COST 1800","line picker builds a replace command from line and edited copy");}
                using(var lines=new ResearchLinesWindow(new Language("de"),noGame,"faculty_geology",entry,"edit",null,new Font("Segoe UI",10),null))
                {lines.TestChoose(-1,-1,"$UNLOCK_RESEARCH clay_study");Check(lines.Result=="$UNLOCK_RESEARCH clay_study","line picker returns a new line for add");}
                using(var lines=new ResearchLinesWindow(new Language("de"),noGame,"faculty_geology",entry,"line",null,new Font("Segoe UI",10),null))
                {lines.TestChoose(1,-1,null);Check(lines.Result=="$UNLOCK_RESEARCH bauxite_study","line picker returns the chosen line for remove");}
            }
            // 0.4.44: a number field reports the typed number as its Text, so the add dialogs keep it.
            using(var number=new NumberInput(1,100000,100)){number.Input.Text="1300";Check(number.Text=="1300","number input exposes the typed value as Text");number.Text="1800";Check(number.Input.Text=="1800","number input takes a value through Text");}
            // 0.4.45: +/- clicks raise Stepped and start an empty field at the minimum.
            using(var number=new NumberInput(1,100000,100)){int stepped=0;number.Stepped+=(s,e)=>stepped++;number.Input.Text="1300";number.TestStep(1);Check(number.Text=="1400"&&stepped==1,"plus click steps the value and raises Stepped");number.Input.Text="";number.TestStep(1);Check(number.Text=="1"&&stepped==2,"plus click on an empty field lands on the minimum");number.TestStep(-1);Check(number.Text=="1"&&stepped==2,"minus click below the minimum changes nothing");}
            // 0.4.71: edits in several entries survive switching and are written by one save.
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=package,Language="de"},null,false))
            {
                HiddenShow(form);form.PendingPrompt=()=>{throw new Exception("FAIL switching entries must not ask about unsaved changes (0.4.71)");};
                form.TestSearch("Sample");form.SelectIndex(0);Application.DoEvents();form.TestEdit("general/limit","9");Check(form.IsDirty&&form.DirtyCount==1&&form.DirtyMarks==1,"an edit in the first entry counts as unsaved");
                form.TestSearch("Needs");form.SelectIndex(0);Application.DoEvents();
                Check(form.HasLocalResourceEditor&&!form.IsDirty&&form.DirtyCount==1&&form.DirtyMarks==1&&form.SaveEnabled&&form.StatusText=="Ungespeicherte Änderungen"&&form.StatusDetailText.Contains("Sample Plugin"),"switching parks the first entry: footer names it, save stays enabled, its dot stays (0.4.71)");
                form.TestSetGlobal("max_demands","4");Check(form.IsDirty&&form.DirtyCount==2&&form.DirtyMarks==2&&form.StatusDetailText.Contains("Needs")&&form.StatusDetailText.Contains("Sample Plugin"),"the second entry adds to the unsaved entries");
                form.TestSearch("Sample");form.SelectIndex(0);Application.DoEvents();Check(form.DisplayedValue("general/limit")=="9"&&form.IsDirty&&form.DirtyCount==2&&form.HeadingText.EndsWith(" *"),"the parked edit comes back when the entry is shown again");
                Check(form.TestSaveAll(()=>{})&&!form.IsDirty&&form.DirtyCount==0&&form.DirtyMarks==0&&new Ini(SafeFiles.Text(Path.Combine(plugins,"sample_plugin.ini"))).Get("general","limit")=="9"&&new LooseIni(SafeFiles.Text(Path.Combine(plugins,"needs.ini"))).Get("needs","max_demands")=="4","one save writes every unsaved entry (0.4.71)");
                form.TestEdit("general/limit","x");form.TestSearch("Needs");form.SelectIndex(0);Application.DoEvents();Check(!form.SaveEnabled&&!form.StartEnabled&&form.StatusDetailText.Contains("Sample Plugin"),"an invalid parked entry locks Save and Save + Start and is named (0.4.71)");
                form.TestSearch("Sample");form.SelectIndex(0);Application.DoEvents();Check(form.HasEditor&&form.DisplayedValue("general/limit")=="x"&&!form.StatusValid,"the invalid parked entry comes back with its editor and the red footer");
                form.TestEdit("general/limit","11");form.TestSearch("Needs");form.SelectIndex(0);Application.DoEvents();form.TestSetGlobal("max_demands","6");Check(form.DirtyCount==2,"two entries are unsaved again");
                form.PendingPrompt=()=>DialogResult.Cancel;Check(!form.TestResolvePending()&&form.DirtyCount==2,"cancelling the unsaved-changes question keeps every parked edit");
                form.PendingPrompt=()=>DialogResult.No;Check(form.TestResolvePending()&&!form.IsDirty&&form.DirtyCount==0&&form.DirtyMarks==0,"discarding drops the parked edits and reloads the shown entry");
                form.TestSearch("Sample");form.SelectIndex(0);Application.DoEvents();Check(form.DisplayedValue("general/limit")=="9","a discarded parked edit is gone when the entry is shown again");
            }
            // 0.4.80: a content package has no editor - one switch provides its entries through the
            // target editors and its files into the vfs, and takes everything back again.
            string contentPkg=Path.Combine(collection,"salt_content");
            Write(Path.Combine(contentPkg,"soviet.mod.ini"),"[mod]\nid=example.uisalt\nname=AAA Salt Content\nversion=2.0\nenabled=1\n[content]\nresources=tesmio\\resources.ini\nassets=assets\n");
            Write(Path.Combine(contentPkg,"tesmio","resources.ini"),"[list]\nraw_salt = rawgravel, Raw Salt\n\n[custom:raw_salt]\ncargo = bulk\n");
            Write(Path.Combine(contentPkg,"assets","media_soviet","resources","raw_salt.png"),"png");
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=contentPkg,Language="de"},null,false))
            {
                HiddenShow(form);
                string vfsPng=Path.Combine(LocalEditorSpec.VfsRoot(loader),"media_soviet","resources","raw_salt.png");
                Check(form.SelectedSource==contentPkg&&form.HasEditor&&form.SwitchVisible&&!form.SwitchChecked&&!form.ContentProvided,"a content package opens with its provide switch off");
                Check(Children(form).OfType<Label>().Any(x=>x.Text.Contains("Plugin resources"))&&Children(form).OfType<Label>().Any(x=>x.Text.Contains("raw_salt")),"the page names the target plugin and the entries the package carries");
                form.TestSwitch(true);Application.DoEvents();
                Check(form.IsDirty&&form.DirtyCount==1&&form.SaveEnabled&&form.StatusValid,"switching the package on counts as an unsaved change");
                Check(form.TestSaveAll(()=>{}),"saving provides the content package");
                Check(form.ContentProvided&&!form.IsDirty&&File.Exists(vfsPng),"the receipt records the package and its file landed in the vfs");
                string providedIni=SafeFiles.Text(resourceIni);
                Check(providedIni.Contains("raw_salt = rawgravel, Raw Salt")&&providedIni.Contains("[custom:raw_salt]")&&providedIni.Contains("glass=aluminium, Glass"),"the entries are merged into the Resources INI beside the existing ones");
                form.TestSearch("Resources");form.SelectIndex(0);Application.DoEvents();
                Check(form.HasLocalResourceEditor&&form.LocalResourceCount==4&&!form.IsDirty,"the Resources editor shows the provided entry as an original");
                form.TestSearch("AAA Salt");form.SelectIndex(0);Application.DoEvents();
                form.TestSwitch(false);Application.DoEvents();Check(form.IsDirty&&form.SaveEnabled,"switching the package off counts as an unsaved change");
                Check(form.TestSaveAll(()=>{}),"saving removes the content package");
                string removedIni=SafeFiles.Text(resourceIni);
                Check(!form.ContentProvided&&!File.Exists(vfsPng)&&!removedIni.Contains("raw_salt")&&removedIni.Contains("glass=aluminium, Glass"),"removing takes entries and files away and leaves the rest untouched");
            }
            // 0.4.81: --save runs the Save button's path headlessly and declines every question.
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=simple,Language="de"},null,false))
            {
                HiddenShow(form);
                // With the DLL gone from plugins\ this save would have to write it, which is the one
                // thing the window asks about - so the headless save refuses instead.
                File.Delete(Path.Combine(plugins,"sample_plugin.dll"));
                form.TestEdit("general/limit","7");
                string dllAnswer=form.CliSave(()=>{});Check(dllAnswer.StartsWith("FAIL"),"--save refuses a save that would copy a DLL into plugins ["+dllAnswer+"]");
            }
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=collection,Language="de"},null,false))
            {
                HiddenShow(form);
                form.TestSearch("Needs");form.SelectIndex(0);Application.DoEvents();
                Check(form.HasLocalResourceEditor&&form.CliSave(()=>{}).StartsWith("PASS"),"--save on an unchanged entry says there is nothing to do");
                form.TestSetGlobal("max_demands","5");
                string answer=form.CliSave(()=>{});
                Check(answer.StartsWith("PASS")&&!form.IsDirty&&new LooseIni(SafeFiles.Text(Path.Combine(plugins,"needs.ini"))).Get("needs","max_demands")=="5","--save writes the change like the button does ["+answer+"]");
                Check(form.CliSave(()=>{}).StartsWith("PASS")&&SafeFiles.Text(Path.Combine(plugins,"needs.ini")).Contains("max_demands = 5"),"a second --save finds nothing to do and writes nothing new");
            }
            // 0.4.82: --activate moves the header switch headlessly, so --save can commit it.
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=contentPkg,Language="de"},null,false))
            {
                HiddenShow(form);
                string switchPng=Path.Combine(LocalEditorSpec.VfsRoot(loader),"media_soviet","resources","raw_salt.png");
                Check(form.CliActivate(false)==null&&!form.IsDirty,"--activate off on an entry that is already off changes nothing");
                Check(form.CliActivate(true)==null&&form.IsDirty&&form.SwitchChecked,"--activate on stages the provide switch");
                string provideAnswer=form.CliSave(()=>{});
                Check(provideAnswer.StartsWith("PASS")&&form.ContentProvided&&File.Exists(switchPng),"--save then provides the content package ["+provideAnswer+"]");
                Check(form.CliActivate(false)==null&&form.CliSave(()=>{}).StartsWith("PASS")&&!form.ContentProvided&&!File.Exists(switchPng),"--activate off followed by --save takes it away again");
            }
            // 0.5.9: a package whose ids the game already has is refused - red, with what to do.
            // But under Soviet Mod Loader there is no switch to press here at all, and the entry in
            // the way is usually the one SML built from this very package. The red box then only
            // makes RMM look broken, so it stays away.
            Write(resourceIni,SafeFiles.Text(resourceIni).Replace("[resources]","raw_salt = rawgravel, Raw Salt\r\n\r\n[resources]"));
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=contentPkg,Language="de"},null,false))
            {
                HiddenShow(form);
                Check(Children(form).OfType<NoticeBox>().Any(x=>x.Tone=="error"),"without SML a clashing id is reported in red");
            }
            File.WriteAllBytes(Path.Combine(plugins,"soviet_mod_loader.dll"),new byte[]{7,7,7});
            Write(Path.Combine(plugins,"soviet_mod_loader.ini"),"[loader]\nenabled = 1\n");
            Write(Path.Combine(loader,"tesmioloader.ini"),"[tesmioloader]\nplugins=1\n[plugins]\nsoviet_mod_loader=1\nresources=0\n");
            File.Delete(resourceDll);
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=contentPkg,Language="de"},null,false))
            {
                HiddenShow(form);
                Check(!Children(form).OfType<NoticeBox>().Any(x=>x.Tone=="error"),"with SML in charge the same clash raises no red box");
                Check(!form.SwitchVisible&&Children(form).OfType<Label>().Any(x=>x.Text.Contains("Soviet Mod Loader")),"the page says who is in charge instead");
            }
            // 0.5.16: a fork that SML pushes aside looks exactly like Deposits Plus under SML - no
            // switch in the header, amber dot, locked page with the way out.
            File.WriteAllBytes(Path.Combine(plugins,"resources_plus.dll"),new byte[]{7,7,7});
            Write(Path.Combine(plugins,"resources_plus.ini"),"[resources]\nhook = 2\n[list]\nunobtainium = aluminium, Unobtainium\nalloy_a = steel, Alloy A\nalloy_b = steel, Alloy B\nalloy_c = steel, Alloy C\nalloy_d = steel, Alloy D\nalloy_e = steel, Alloy E\nalloy_f = steel, Alloy F\nalloy_g = steel, Alloy G\n");
            Write(Path.Combine(loader,"tesmioloader.ini"),"[tesmioloader]\nplugins=1\n[plugins]\nsoviet_mod_loader=1\nresources=0\nresources_plus=1\n");
            using(var form=new MainForm(new UiState{Build=loader,WorkshopRoot=collection,SelectedSource=contentPkg,Language="de"},null,false))
            {
                HiddenShow(form);form.TestSearch("Resources Plus");form.SelectIndex(0);Application.DoEvents();
                Check(form.HasLocalResourceEditor&&Children(form).OfType<Button>().Any(x=>x.AccessibleName=="sml:unlock"),"the paused fork opens on its locked page with the way out");
                Check(!form.SwitchVisible,"the fork's header switch is hidden while SML hosts the capability");
                Check(form.ShownPaused,"and its dot is amber, not green");
                // 0.5.19: the plugin-wide switches of the General tab must not be rendered as per-resource fields.
                Check(!Children(form).OfType<Label>().Any(x=>x.Text=="Preise aus der INI anwenden"||x.Text=="Zollhaus-Zeilen protokollieren"),"global fields stay out of the resource detail area");
                // 0.5.19: eight entries bring a search box above the master list; typing filters the ListBox in place.
                {var box=Children(form).OfType<TextBox>().Single(x=>x.AccessibleName=="list-search");var lb=Children(form).OfType<ResourceDefinitionList>().Single();Check(lb.Items.Count==8&&!box.ReadOnly,"the master list has a search box that stays typeable on a paused page");box.Text="alloy_c";Application.DoEvents();Check(lb.Items.Count==1&&((LocalResourceItem)lb.Items[0]).Id=="alloy_c"&&lb.SelectedIndex==0,"the filter narrows the list in place and selects the match: "+lb.Items.Count);box.Text="";Application.DoEvents();Check(lb.Items.Count==8,"clearing the filter brings every entry back");}
                Check(!Children(form).OfType<Label>().Any(x=>x.Text=="Kein Eintrag – du hast diesen Wert nicht überschrieben"),"empty original fields carry no origin line any more");
                // 0.5.17: the status line lights no load-path dot for a paused plugin; an amber "paused" dot says why.
                var dots=Children(form).OfType<Panel>().Where(x=>(x.AccessibleName??"").StartsWith("status-dot:")).Select(x=>x.AccessibleName).ToList();
                Check(dots.Contains("status-dot:paused")&&!dots.Contains("status-dot:on"),"the status line shows the paused dot and no lit load path");
            }
            Console.WriteLine("RESULT "+passed+" UI assertions passed. "+root);return 0;
        }
        catch(Exception e){Console.Error.WriteLine(e);return 1;}
    }
}
