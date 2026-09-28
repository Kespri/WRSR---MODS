// Presentation metadata never changes plugin keys, limits, defaults or calculations.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;

namespace TesmioAutoload
{
    public static class Embedded
    {
        public static string Text(string name)
        {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("Tesmio." + name))
            { if (stream == null) throw new IOException(Msg.Key("err_missing_application_resource", name)); using (var reader = new StreamReader(stream, SafeFiles.Utf8)) return reader.ReadToEnd(); }
        }
    }
    public sealed class Language
    {
        public readonly string Code;
        readonly Ini english, selected;
        public static bool ValidCode(string code) { return Regex.IsMatch(code, "^[a-z]{2,3}(-[A-Za-z]{2,4})?$"); }
        public Language(string requested)
        {
            string code = requested == "auto" ? CultureInfo.CurrentUICulture.TwoLetterISOLanguageName : requested;
            Code = ValidCode(code ?? "") ? code : "en";
            english = Load("en"); selected = Load(Code) ?? english;
        }
        static Ini Load(string code)
        {
            string path = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "languages", code + ".ini");
            try { if (File.Exists(path)) { var external=new Ini(SafeFiles.Text(path)); return code=="de" || code=="en" ? new Ini(new Ini(Embedded.Text("languages."+code)).Render(external.Values)) : external; } } catch (Exception) { /* Invalid optional translations fall back to built-in text. */ }
            return code == "de" || code == "en" ? new Ini(Embedded.Text("languages." + code)) : null;
        }
        public string T(string key) { return selected.Get("strings", key, english.Get("strings", key, key)); }
        public string Format(string key, params object[] arguments) { return String.Format(CultureInfo.CurrentCulture,T(key),arguments); }
        // Core messages arrive as keys with arguments (Msg.Key); everything else passes through.
        public string Localize(string text) { if (!Msg.IsKey(text)) return text; try { return Format(Msg.KeyOf(text), Msg.ArgsOf(text).Select(a => (object)Localize(a)).ToArray()); } catch (FormatException) { return Msg.Plain(text); } }
        public static List<KeyValuePair<string, string>> Available()
        {
            var list = new Dictionary<string, string> { { "auto", "Auto" }, { "de", "Deutsch" }, { "en", "English" } };
            string root = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "languages");
            if (Directory.Exists(root)) try
            {
                SafeFiles.NoLinks(root);
                foreach (string file in Directory.GetFiles(root, "*.ini"))
                {
                    string code = Path.GetFileNameWithoutExtension(file); if (!ValidCode(code)) continue;
                    try { list[code] = new Ini(SafeFiles.Text(file)).Get("language", "name", code); } catch (Exception) { }
                }
            } catch (Exception) { }
            return list.Select(x => new KeyValuePair<string, string>(x.Key, x.Value)).ToList();
        }
    }
    public sealed class TabSpec { public string Id, Label; public int Order; }
    public sealed class GroupSpec { public string Id, Tab, Label, Description, Notice, NoticeStyle = "", EmptyNotice, EmptyNoticeStyle, Layout; public int Order; }
    public sealed class FieldSpec
    {
        public Field Field;
        public string Group, Label, Description, Icon, Row, RowLabel, Column, ColumnLabel, Unit;
        public decimal Step;
        public int Order;
    }
    // A button that opens a file of the package (its README) with the file's own
    // application; Language limits it to one UI language.
    public sealed class LinkSpec { public string Id, File, Label, Language = ""; public int Order; }
    // 0.4.53: a labelled row with one button that runs a command of the manager (bridge_prune).
    public sealed class ActionSpec { public string Id, Group, Label, Description, Button, Command; public int Order; }
    // 0.4.56: the subfolders of one or more paths as rows - name, a short text from the language file
    // (<DescriptionPrefix>.<folder>), and where the folder lives (package / local / both).
    public sealed class FolderListSpec { public string Id, Group, DescriptionPrefix, Note; public string[] Paths; public int Order; }
    public sealed class Presentation
    {
        public string Description, Icon, EnabledField, DefaultTab;
        public readonly List<LinkSpec> Links = new List<LinkSpec>();
        public string LinksTab = "", LinksLabel = "", LinksRoot = "."; string schemaDirectory = "";
        public string LinkPath(LinkSpec link)
        {
            if (schemaDirectory.Length == 0) throw new FormatException(Msg.Key("err_kein_schema_ordner_fuer"));
            string root = Path.GetFullPath(Path.Combine(schemaDirectory, LinksRoot));
            string path = SafeFiles.Child(root, link.File); string ext = Path.GetExtension(path).ToLowerInvariant();
            if (ext != ".md" && ext != ".txt" && ext != ".html" && ext != ".pdf") throw new FormatException(Msg.Key("err_nur_md_txt_html", link.File));
            return path;
        }
        public readonly List<ActionSpec> Actions = new List<ActionSpec>();   // 0.4.53
        public readonly List<FolderListSpec> FolderLists = new List<FolderListSpec>();   // 0.4.56
        // A path spec of a folder list: package:<rel> (the package root, like [links]), build:<rel>, vfs:<rel>.
        public string ResolveFolder(string spec, string build)
        {
            int colon = spec.IndexOf(':'); if (colon <= 0) throw new FormatException(Msg.Key("err_ungueltiger_pfadeintrag", spec));
            string prefix = spec.Substring(0, colon).Trim().ToLowerInvariant(), rel = spec.Substring(colon + 1).Trim(); string root;
            if (prefix == "package") { if (schemaDirectory.Length == 0) throw new FormatException(Msg.Key("err_kein_schema_ordner_fuer")); root = Path.GetFullPath(Path.Combine(schemaDirectory, LinksRoot)); }
            else if (prefix == "build") root = Path.GetFullPath(build);
            else if (prefix == "vfs") root = LocalEditorSpec.VfsRoot(build);
            else throw new FormatException(Msg.Key("err_ungueltiger_pfadeintrag", spec));
            try { return SafeFiles.Child(root, rel); } catch (IOException) { throw new FormatException(Msg.Key("err_ungueltiger_pfadeintrag", spec)); }
        }
        public static bool IsPackagePath(string spec) { return spec.TrimStart().StartsWith("package:", StringComparison.OrdinalIgnoreCase); }
        // The short text of one folder from the language files, "" when there is none.
        public string FolderText(string prefix, string folder)
        {
            if (String.IsNullOrEmpty(prefix)) return "";
            string id = prefix + "." + folder;
            return (selected == null ? null : selected.Get("strings", id)) ?? (english == null ? null : english.Get("strings", id)) ?? "";
        }
        public List<LinkSpec> LinksFor(Language language)
        {
            string code = (language.Code ?? "").Split('-')[0].ToLowerInvariant();
            var matching = Links.Where(x => x.Language.Length == 0 || x.Language == code).ToList();
            if (!matching.Any(x => x.Language.Length > 0) && Links.Any(x => x.Language.Length > 0)) matching.AddRange(Links.Where(x => x.Language.Length > 0));
            return matching;
        }
        public readonly List<TabSpec> Tabs = new List<TabSpec>();
        public readonly List<GroupSpec> Groups = new List<GroupSpec>();
        public readonly List<FieldSpec> Fields = new List<FieldSpec>();
        public readonly List<CollectionSpec> Collections = new List<CollectionSpec>();
        public readonly List<string> Notes = new List<string>();
        // [launcher] notice / notice_key / notice_style: a package-wide note shown in the
        // "Notes" card of the first tab, next to dependency and bridge notes.
        public string Notice = "", NoticeStyle = "info";
        public string Info = "";   // [launcher] info (0.4.10): second, always blue box below the notice
        readonly Ini meta, english, selected;
        int Order(string section) { int n; return Int32.TryParse(meta.Get(section, "order", "0"), out n) ? n : 0; }
        string Text(string section, string key, string fallback)
        {
            string id = meta.Get(section, key + "_key", "");
            return (selected == null ? null : selected.Get("strings", id)) ?? (english == null ? null : english.Get("strings", id)) ?? meta.Get(section, key, fallback);
        }
        string Translated(string key,string fallback)
        {return key.Length==0?fallback:(selected==null?null:selected.Get("strings",key))??(english==null?null:english.Get("strings",key))??fallback;}
        // A schema value cannot contain a newline; the two-character sequence \n stands for one.
        static string Lines(string s) { return s == null ? null : s.Replace("\\n", "\n"); }
        Ini Translation(string root, string relative, string code)
        {
            if (relative.Length == 0 || !Language.ValidCode(code)) return null;
            try { string path = SafeFiles.Child(root, relative + "\\" + code + ".ini"); return File.Exists(path) ? new Ini(SafeFiles.Text(path)) : null; }
            catch (Exception e) { Notes.Add(Msg.Key("translation_ignored", e.Message)); return null; }
        }
        public Presentation(Package package, Language language, Ini effective = null, IDictionary<string,ResourceRegistry> registries = null)
        {
            meta = package.Schema;
            if (meta.Get("launcher", "layout_version", "1") != "1") throw new FormatException(Msg.Key("err_unsupported_presentation_layout_version"));
            string directory = meta.Get("launcher", "language_directory", "");
            english = Translation(package.Root, directory, "en"); selected = Translation(package.Root, directory, language.Code);
            Description = Lines(Text("launcher", "description", "")); Icon = meta.Get("launcher", "icon", "builtin:gear");
            Notice = Lines(Text("launcher", "notice", "")); NoticeStyle = meta.Get("launcher", "notice_style", "info");
            Info = Lines(Text("launcher", "info", ""));
            EnabledField = meta.Get("launcher", "enabled_field", ""); DefaultTab = meta.Get("launcher", "default_tab", "");
            if (EnabledField.Length > 0 && !package.Fields.Any(f => f.Id == EnabledField && f.Type == "boolean")) throw new FormatException(Msg.Key("err_enabled_field_must_refer"));
            foreach (string s in meta.Sections.Where(s => s.StartsWith("tab:", StringComparison.OrdinalIgnoreCase)))
                Tabs.Add(new TabSpec { Id = s.Substring(4), Label = Text(s, "label", s.Substring(4)), Order = Order(s) });
            schemaDirectory = package.SchemaPath == null ? "" : Path.GetDirectoryName(package.SchemaPath);
            LinksTab = meta.Get("links", "tab", ""); LinksLabel = Text("links", "label", ""); LinksRoot = meta.Get("links", "root", ".");
            if (LinksRoot.Any(c => c == ':' || c == '\0') || LinksRoot.Split('\\', '/').Any(part => part != ".." && part != "." && part.Length > 0 && !part.All(c => Char.IsLetterOrDigit(c) || c == '_' || c == '-'))) throw new FormatException(Msg.Key("err_ungueltiger_links_abschnitt"));
            foreach (string s in meta.Sections.Where(s => s.StartsWith("link:", StringComparison.OrdinalIgnoreCase)))
            {
                var link = new LinkSpec { Id = s.Substring(5), File = meta.Get(s, "file", ""), Label = Text(s, "label", s.Substring(5)), Language = meta.Get(s, "language", "").Trim().ToLowerInvariant(), Order = Order(s) };
                if (link.File.Length == 0 || link.File.Any(c => c == ':' || c == '\0') || link.File.Contains("..") || link.Language.Length > 0 && !Language.ValidCode(link.Language)) throw new FormatException(Msg.Key("err_ungueltige_verknuepfung", s));
                Links.Add(link);
            }
            Links.Sort((a, b) => a.Order != b.Order ? a.Order.CompareTo(b.Order) : String.CompareOrdinal(a.Id, b.Id));
            foreach (string s in meta.Sections.Where(s => s.StartsWith("action:", StringComparison.OrdinalIgnoreCase)))
            {
                var action = new ActionSpec { Id = s.Substring(7), Group = meta.Get(s, "group", "settings"), Label = Text(s, "label", s.Substring(7)), Description = Lines(Text(s, "description", "")), Button = Text(s, "button", s.Substring(7)), Command = meta.Get(s, "command", "").Trim().ToLowerInvariant(), Order = Order(s) };
                if (action.Command != "bridge_prune") throw new FormatException(Msg.Key("err_unbekannte_aktion", s));
                Actions.Add(action);
            }
            foreach (string s in meta.Sections.Where(s => s.StartsWith("group:", StringComparison.OrdinalIgnoreCase)))
            {
                string layout = meta.Get(s, "layout", "fields");
                if (layout != "fields" && layout != "matrix") throw new FormatException(Msg.Key("err_unknown_group_layout", s));
                Groups.Add(new GroupSpec { Id = s.Substring(6), Tab = meta.Get(s, "tab", "settings"), Label = Text(s, "label", s.Substring(6)), Description = Lines(Text(s, "description", "")), Notice = Lines(Text(s, "notice", "")), NoticeStyle = meta.Get(s, "notice_style", ""), EmptyNotice = "", EmptyNoticeStyle = "warning", Layout = layout, Order = Order(s) });
            }
            Collections.AddRange(package.Collections);
            foreach(CollectionSpec collection in Collections)if(!Groups.Any(g=>g.Id==collection.ResourceGroup)||!Groups.Any(g=>g.Id==collection.MatrixGroup&&g.Layout=="matrix"))
                throw new FormatException(Msg.Key("err_collection_references_unknown_or", collection.Id));
            foreach (Field field in package.Fields)
            {
                string section = meta.Sections.FirstOrDefault(s => s.StartsWith("field:", StringComparison.OrdinalIgnoreCase) && Ini.Id(meta.Get(s, "section", ""), meta.Get(s, "key", "")) == field.Id);
                section = section ?? "_missing";
                decimal step; if (!Decimal.TryParse(meta.Get(section, "step", field.Type == "integer" ? "1" : "0.001"), NumberStyles.Float, CultureInfo.InvariantCulture, out step) || step <= 0) throw new FormatException(Msg.Key("err_invalid_ui_step", field.Id));
                string group = meta.Get(section, "group", "settings");
                if (!Groups.Any(g => g.Id == group))
                {
                    if (group != "settings") throw new FormatException(Msg.Key("err_field_references_unknown_group", field.Id, group));
                    Groups.Add(new GroupSpec { Id = group, Tab = "settings", Label = language.T("settings"), Description = "", Notice = "", EmptyNotice = "", EmptyNoticeStyle = "warning", Layout = "fields", Order = 10000 });
                }
                Fields.Add(new FieldSpec { Field = field, Group = group, Label = Text(section, "label", field.Label), Description = Lines(Text(section, "description", field.Description)), Icon = meta.Get(section, "icon", ""), Row = meta.Get(section, "row", field.Section), RowLabel = Text(section, "row_label", field.Section), Column = meta.Get(section, "column", field.Key), ColumnLabel = Text(section, "column_label", field.Key), Unit = Text(section, "unit", ""), Step = step, Order = Order(section) });
            }
            foreach(var collection in Collections)
            {
                Ini config=effective??package.Defaults; string raw=config.Required(collection.CountSection,collection.CountKey); int count;
                if(!Int32.TryParse(raw,NumberStyles.None,CultureInfo.InvariantCulture,out count)||count<0||count>collection.MaximumItems)
                    throw new FormatException(Msg.Key("err_invalid_collection_count", collection.Id));
                if(count==0)
                {
                    GroupSpec matrixGroup=Groups.First(g=>g.Id==collection.MatrixGroup);
                    matrixGroup.EmptyNotice=Translated(collection.EmptyNoticeKey,collection.EmptyNotice);
                    matrixGroup.EmptyNoticeStyle=collection.EmptyNoticeStyle;
                }
                for(int i=0;i<count;i++)
                {
                    string itemKey=collection.ItemPrefix+i, name=config.Required(collection.CountSection,itemKey);
                    bool removable=CollectionRules.CanRemove(package,collection,name);
                    if(!Fields.Any(x=>x.Field.Id==Ini.Id(collection.CountSection,itemKey)))
                    {
                        string itemLabel=Translated(collection.ItemLabelKey,collection.ItemLabel);
                        string itemDescription=Translated(collection.ItemDescriptionKey,language.T("registered_resource_help"));
                        var field=new Field {Section=collection.CountSection,Key=itemKey,Label=itemLabel+" "+(i+1),Description=itemDescription,Type="readonly",DefaultValue=name,HasPackageDefault=package.Defaults.Values.ContainsKey(Ini.Id(collection.CountSection,itemKey)),CollectionId=collection.Id,CollectionItem=name,RemovableCollectionItem=removable};
                        Fields.Add(new FieldSpec {Field=field,Group=collection.ResourceGroup,Label=field.Label,Description=field.Description,Row=collection.CountSection,RowLabel=collection.CountSection,Column=itemKey,ColumnLabel=name,Unit="",Step=1,Order=1000+i});
                    }
                    ResourceRegistry registry=null;if(registries!=null)registries.TryGetValue(collection.Id,out registry);
                    ResourceOption option=registry==null?null:registry.Find(name); string display=option==null?name:option.Display;
                    for(int targetIndex=0;targetIndex<collection.TargetSections.Length;targetIndex++)
                    {
                        string target=collection.TargetSections[targetIndex], id=Ini.Id(target,name);
                        if(Fields.Any(x=>x.Field.Id==id)) continue;
                        string rowLabel=Translated(collection.TargetLabelKeys[targetIndex],collection.TargetLabels[targetIndex].Length==0?target:collection.TargetLabels[targetIndex]);
                        string unit=Translated(collection.UnitKey,collection.Unit.Length==0?language.T("coefficient"):collection.Unit);
                        string coefficientHelp=Translated(collection.CoefficientDescriptionKey,language.T("coefficient_help"));
                        var field=new Field {Section=target,Key=name,Label=display,Description=coefficientHelp,Type=collection.Type,Minimum=collection.Minimum,Maximum=collection.Maximum,DefaultValue=package.Defaults.Get(target,name,collection.DefaultValue),HasPackageDefault=package.Defaults.Values.ContainsKey(id),CollectionId=collection.Id,CollectionItem=name};
                        Fields.Add(new FieldSpec {Field=field,Group=collection.MatrixGroup,Label=display,Description=field.Description,Icon=collection.TargetIcons[targetIndex],
                            Row=target,RowLabel=rowLabel,Column=name,ColumnLabel=display,Unit=unit,
                            Step=collection.Step,Order=2000+i*collection.TargetSections.Length+targetIndex});
                    }
                }
            }
            foreach (GroupSpec group in Groups)
            {
                if (!Tabs.Any(t => t.Id == group.Tab))
                {
                    if (group.Tab != "settings") throw new FormatException(Msg.Key("err_group_references_unknown_tab", group.Id, group.Tab));
                    Tabs.Add(new TabSpec { Id = "settings", Label = language.T("settings"), Order = 10000 });
                }
                if (group.Layout == "matrix" && Fields.Where(f => f.Group == group.Id && f.Field.Id != EnabledField).GroupBy(f => f.Row + "/" + f.Column).Any(g => g.Count() > 1)) throw new FormatException(Msg.Key("err_duplicate_matrix_cell", group.Id));
            }
            Tabs.Sort((a,b) => a.Order != b.Order ? a.Order.CompareTo(b.Order) : String.CompareOrdinal(a.Id,b.Id));
            Groups.Sort((a,b) => a.Order != b.Order ? a.Order.CompareTo(b.Order) : String.CompareOrdinal(a.Id,b.Id));
            foreach (ActionSpec action in Actions) if (!Groups.Any(g => g.Id == action.Group)) throw new FormatException(Msg.Key("err_unbekannte_aktion", action.Id));
            foreach (string s in meta.Sections.Where(s => s.StartsWith("folder_list:", StringComparison.OrdinalIgnoreCase)))
            {
                var list = new FolderListSpec { Id = s.Substring(12), Group = meta.Get(s, "group", "settings"), DescriptionPrefix = meta.Get(s, "description_prefix", "").Trim(), Note = Lines(Text(s, "note", "")), Paths = meta.Get(s, "paths", "").Split('|').Select(x => x.Trim()).Where(x => x.Length > 0).ToArray(), Order = Order(s) };
                if (list.Paths.Length == 0 || !Groups.Any(g => g.Id == list.Group)) throw new FormatException(Msg.Key("err_ungueltiger_pfadeintrag", s));
                foreach (string spec in list.Paths) ResolveFolder(spec, Path.GetTempPath());   // syntax check only
                FolderLists.Add(list);
            }
            Fields.Sort((a,b) => a.Order != b.Order ? a.Order.CompareTo(b.Order) : String.CompareOrdinal(a.Field.Id,b.Field.Id));
        }
    }
}
