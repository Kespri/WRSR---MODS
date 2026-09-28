using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    // 0.4.88: what a saved game needs. The loader writes tesmioloader.save.ini beside every save:
    // the plugins that were loaded and the resources and deposits the world knows. Read-only here -
    // the manager never writes into a save - but it answers the one question a player has before
    // switching something off: "does one of my games still need this?"
    public sealed class SaveGame
    {
        public string Name="",Path="";
        public DateTime Written;
        public readonly List<string> Plugins=new List<string>(),Resources=new List<string>(),Deposits=new List<string>();
        public bool Knows(string kind,string id)
        {
            var list=kind=="plugins"?Plugins:kind=="deposits"?Deposits:Resources;
            return list.Contains(id,StringComparer.OrdinalIgnoreCase);
        }
    }

    public static class SaveGames
    {
        public const string Manifest="tesmioloader.save.ini";
        // save holds the player's games and the autosaves, saved_last the last exit, save_cloud the
        // Steam copies. save_backups is a copy of copies and save_terraineditor holds maps, not games.
        static readonly string[] Folders={"save","save_cloud","saved_last"};

        public static List<SaveGame> Scan(string build)
        {
            var result=new List<SaveGame>();
            string game=GameBuildings.GameRoot(build);
            if(game==null)return result;
            foreach(string folder in Folders)
            {
                string root=Path.Combine(game,"media_soviet",folder);
                if(!Directory.Exists(root))continue;
                // saved_last is one save itself; the others hold one folder per save.
                foreach(string dir in File.Exists(Path.Combine(root,Manifest))?new[]{root}:Directories(root))
                {
                    SaveGame save=Read(dir);
                    if(save!=null)result.Add(save);
                }
            }
            return result.OrderByDescending(x=>x.Written).ThenBy(x=>x.Name,StringComparer.CurrentCultureIgnoreCase).ToList();
        }
        static string[] Directories(string root)
        {
            try{return Directory.GetDirectories(root);}catch(Exception){return new string[0];}
        }
        static SaveGame Read(string dir)
        {
            string file=Path.Combine(dir,Manifest);
            if(!File.Exists(file))return null;
            var save=new SaveGame{Name=Path.GetFileName(dir.TrimEnd('\\')),Path=dir};
            try{save.Written=File.GetLastWriteTime(file);}catch(Exception){}
            try
            {
                // The tolerant reader on purpose: an older loader wrote broken keys into this file
                // ("rawiron, Copper Ore=1"), and a save must never be unreadable because of that.
                var ini=new LooseIni(SafeFiles.Text(file));
                foreach(var pair in ini.Entries("plugins"))Add(save.Plugins,pair.Key);
                foreach(var pair in ini.Entries("resources"))Add(save.Resources,pair.Key);
                foreach(var pair in ini.Entries("deposits"))Add(save.Deposits,pair.Key);
            }
            catch(Exception){return save;}
            return save;
        }
        static void Add(List<string> list,string key)
        {
            string id=(key??"").Trim();
            // Keys of the old broken manifests hold a whole list line ("steel, Cable"); the id in
            // front of the comma is still the one the save means.
            int comma=id.IndexOf(',');
            if(comma>0)id=id.Substring(0,comma).Trim();
            if(id.Length>0&&!list.Contains(id,StringComparer.OrdinalIgnoreCase))list.Add(id);
        }

        // The saves that know any of these ids, newest first.
        public static List<SaveGame> Using(IEnumerable<SaveGame> saves,string kind,IEnumerable<string> ids)
        {
            var wanted=new HashSet<string>(ids??new string[0],StringComparer.OrdinalIgnoreCase);
            if(wanted.Count==0)return new List<SaveGame>();
            return (saves??new List<SaveGame>()).Where(s=>wanted.Any(id=>s.Knows(kind,id))).ToList();
        }
        // "Sibirien 1985, Testmap und 3 weitere" - a line a player can read at a glance.
        public static string Names(IEnumerable<SaveGame> saves,int shown,string andMore)
        {
            var list=(saves??new List<SaveGame>()).Select(x=>x.Name).ToList();
            if(list.Count<=shown)return String.Join(", ",list);
            return String.Join(", ",list.Take(shown))+" "+String.Format(andMore,list.Count-shown);
        }
    }
}
