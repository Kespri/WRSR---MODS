using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    public sealed class ResourceCleanupPlan
    {
        public readonly List<string> Plugins=new List<string>();
        public readonly Dictionary<string,byte[]> Writes=new Dictionary<string,byte[]>(StringComparer.OrdinalIgnoreCase);
    }

    // Cross-plugin checks use only collection metadata declared by launcher schemas.
    // Unknown INI keys are never guessed to be resource references.
    public static class ResourceConsistency
    {
        static IEnumerable<Package> Packages(IEnumerable<CatalogEntry> entries)
        {
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var entry in entries.Where(x=>x.Supported&&!x.LocalEditor&&x.Problem.Length==0))
            {
                Package package;
                try{package=Package.Load(entry.Root);}catch{continue;}
                // A content package brings no DLL and no INI of its own, so it has neither a target nor a
                // config name; building plugins\<name> from an empty name would be an invalid path.
                if(package.Kind=="content")continue;
                if(seen.Add(package.Id))yield return package;
            }
        }
        public static HashSet<string> ResourceIds(string build)
        {
            string path=SafeFiles.Child(Path.GetFullPath(build),"plugins\\resources.ini");
            if(!File.Exists(path))throw new IOException(Msg.Key("err_plugins_resources_ini_fehlt"));
            var result=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var pair in new LooseIni(SafeFiles.Text(path)).Entries("list"))
                if(!CollectionRules.SafeItem(pair.Key)||!result.Add(pair.Key))throw new FormatException(Msg.Key("err_ungueltige_oder_doppelte_ressource", pair.Key));
            return result;
        }
        public static List<string> ValidateReferences(string build,IEnumerable<CatalogEntry> entries,ISet<string> resourceIds=null)
        {
            var ids=resourceIds==null?ResourceIds(build):new HashSet<string>(resourceIds,StringComparer.OrdinalIgnoreCase);
            var issues=new List<string>();string root=Path.GetFullPath(build);Ini loader;
            string loaderPath=SafeFiles.Child(root,"tesmioloader.ini");try{loader=File.Exists(loaderPath)?new Ini(SafeFiles.Text(loaderPath)):new Ini("");}catch(Exception e){issues.Add(Msg.Key("consistency_loader_ini",e.Message));return issues;}
            foreach(Package package in Packages(entries))
            {
                string localDll=SafeFiles.Child(root,"plugins\\"+package.Target+".dll"),localIni=SafeFiles.Child(root,"plugins\\"+package.ConfigName);
                if(!File.Exists(localDll)||!File.Exists(localIni)||loader.Get("plugins",package.Target,"1")=="0")continue;
                try
                {
                    var config=new Ini(SafeFiles.Text(localIni));
                    foreach(CollectionSpec collection in package.Collections.Where(x=>x.SourcePlugin.Equals("resources",StringComparison.OrdinalIgnoreCase)&&x.SourceSection.Equals("list",StringComparison.OrdinalIgnoreCase)))
                        foreach(string id in CollectionRules.Names(collection,config).Where(x=>!ids.Contains(x)))
                            issues.Add(Msg.Key("consistency_resource_missing",package.Name,id,collection.Id));
                }
                catch(Exception e){issues.Add(Msg.Key("consistency_check_failed",package.Name,e.Message));}
            }
            return issues;
        }
        public static ResourceCleanupPlan PlanRemoval(string build,IEnumerable<CatalogEntry> entries,string resourceId)
        {
            var plan=new ResourceCleanupPlan();string root=Path.GetFullPath(build);
            foreach(Package package in Packages(entries))
            {
                string localIni=SafeFiles.Child(root,"plugins\\"+package.ConfigName);if(!File.Exists(localIni))continue;
                Ini current;
                try{current=new Ini(SafeFiles.Text(localIni));}catch{continue;}
                bool changed=false;
                foreach(CollectionSpec collection in package.Collections.Where(x=>x.SourcePlugin.Equals("resources",StringComparison.OrdinalIgnoreCase)&&x.SourceSection.Equals("list",StringComparison.OrdinalIgnoreCase)))
                {
                    if(!CollectionRules.Names(collection,current).Any(x=>x.Equals(resourceId,StringComparison.OrdinalIgnoreCase)))continue;
                    if(!CollectionRules.CanRemove(package,collection,resourceId))throw new IOException(Msg.Key("err_enthaelt_als_nicht_loeschbaren", package.Name, resourceId));
                    current=CollectionRules.Remove(package,collection,current,resourceId);changed=true;
                }
                if(!changed)continue;
                plan.Plugins.Add(package.Name);
                var overrides=current.Values.Where(x=>!package.IsDefault(x.Key,x.Value)).ToDictionary(x=>x.Key,x=>x.Value,StringComparer.OrdinalIgnoreCase);
                string userIni=SafeFiles.Child(root,"user_config\\"+package.ConfigName);
                string personal=File.Exists(userIni)?new Ini(SafeFiles.Text(userIni)).RewriteValues(overrides):Ini.Sparse(overrides);
                plan.Writes[localIni]=SafeFiles.Utf8.GetBytes(current.RewriteValues(current.Values));
                plan.Writes[userIni]=SafeFiles.Utf8.GetBytes(personal);
                string localDll=SafeFiles.Child(root,"plugins\\"+package.Target+".dll");
                if(File.Exists(localDll))
                {
                    string receipt=SafeFiles.Child(root,"user_config\\.autoload\\"+package.Target+".receipt.ini");
                    plan.Writes[receipt]=SafeFiles.Utf8.GetBytes("[state]\r\nid = "+package.Id+"\r\ndll_hash = "+SafeFiles.HashFile(localDll)+"\r\nini_hash = "+SafeFiles.Hash(plan.Writes[localIni])+"\r\ndefaults_hash = "+package.InputHashes[package.DefaultsPath]+"\r\n");
                }
            }
            return plan;
        }
    }
}
