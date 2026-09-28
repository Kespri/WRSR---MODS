// Republic Mod Manager - the game's research tree (0.4.29): the ids, names and
// unlock lists of media_soviet\research\research.ini, so a research can be
// picked by name instead of typed, and an unlock anchor can be chosen from what
// the parent research really unlocks.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TesmioAutoload
{
    public sealed class ResearchEntry
    {
        public string Id = "", Name = ""; public int NameId; public bool Own;
        public readonly List<string> Unlocks = new List<string>();
        // 0.4.42: every line of the block as written, header and $RESEARCH_ADD included.
        public readonly List<string> Lines = new List<string>();
        public string Caption { get { return Name.Length > 0 ? Id + "  ·  " + Name : Id; } }
    }

    public static class GameResearch
    {
        public static string FilePath(string game) { return Path.Combine(game, "media_soviet", "research", "research.ini"); }

        // Every active research block of research.ini. A block whose header starts
        // with "-" is deactivated in the game and left out; deactivated lines inside
        // an active block are ignored too. Names come from the game texts of the
        // given language; a missing text leaves the name empty.
        public static List<ResearchEntry> Scan(string game, string language)
        {
            var result = new List<ResearchEntry>();
            if (game == null) return result;
            string path = FilePath(game);
            if (!File.Exists(path)) return result;
            var names = new Dictionary<int, string>();
            if (!String.IsNullOrEmpty(language))
            {
                try { foreach (GameTextEntry entry in GameTexts.Load(game, language)) names[entry.Id] = entry.FirstLine; }
                catch (Exception) { names.Clear(); }
            }
            result = Parse(File.ReadAllLines(path));
            foreach (ResearchEntry entry in result) { string name; if (entry.NameId > 0 && names.TryGetValue(entry.NameId, out name)) entry.Name = name; }
            return result;
        }

        // The lines an edit command may name: the block without its header, its $RESEARCH_ADD
        // and the deactivated ("-") lines the game ignores.
        public static List<string> EditableLines(ResearchEntry entry)
        {
            return entry.Lines.Where(l => !l.StartsWith("$RESEARCH ", StringComparison.Ordinal) && l != "$RESEARCH_ADD" && !l.StartsWith("-", StringComparison.Ordinal)).ToList();
        }

        public static List<ResearchEntry> Parse(IEnumerable<string> lines)
        {
            var result = new List<ResearchEntry>();
            ResearchEntry current = null; bool skipping = false;
            foreach (string raw in lines)
            {
                string line = raw.Trim();
                if (line.Length == 0) continue;
                if (line.StartsWith("-$RESEARCH ", StringComparison.Ordinal)) { skipping = true; current = null; continue; }
                if (line.StartsWith("$RESEARCH ", StringComparison.Ordinal))
                {
                    skipping = false;
                    current = new ResearchEntry { Id = line.Substring(10).Trim() };
                    if (current.Id.Length > 0) { result.Add(current); current.Lines.Add(line); } else current = null;
                    continue;
                }
                if (line == "$RESEARCH_ADD" || line == "-$RESEARCH_ADD") { if (current != null) current.Lines.Add(line); current = null; skipping = false; continue; }
                if (!skipping && current != null) current.Lines.Add(line);
                if (skipping || current == null || line.StartsWith("-", StringComparison.Ordinal)) continue;
                if (line.StartsWith("$NAME ", StringComparison.Ordinal)) { int id; if (Int32.TryParse(line.Substring(6).Trim(), out id)) current.NameId = id; continue; }
                if (line.StartsWith("$UNLOCK_RESEARCH ", StringComparison.Ordinal)) { string target = line.Substring(17).Trim(); if (target.Length > 0 && !current.Unlocks.Contains(target, StringComparer.OrdinalIgnoreCase)) current.Unlocks.Add(target); }
            }
            return result;
        }
    }
}
