using System;
using System.Collections.Generic;
using System.Linq;

namespace TesmioAutoload
{
    // What Buildings Plus will write, worked out here so the editor can show it before the game
    // does. This is a second implementation of the rule in buildings_plus.cpp (DonorLineIsReplaced,
    // Replaces, WriteBuildingIni) and the one thing that must never drift: the same cases are
    // checked in the plugin's own self tests and in CoreTests, and both sides carry this note.
    //
    // The rule, in full:
    //   the same token                                  -> the donor's line goes
    //   $TYPE_*   against $TYPE_*                        -> goes
    //   $NAME*    against $NAME*                         -> goes (the name field counts as $NAME_STR)
    //   $STORAGE* against $STORAGE*                      -> goes, and so does $RESOURCE_VISUALIZATION
    //   $PRODUCTION / $CONSUMPTION / $CONSUMPTION_PER_SECOND are one recipe
    //   a line without a token belongs to the token above it and goes with it,
    //   until a blank line, a "-" or a ";"
    // Not symmetrical: a $STORAGE line of yours removes the donor's $RESOURCE_VISUALIZATION,
    // the other way round it does not.
    public sealed class DonorLine
    {
        public string Text = "";
        public bool Dropped;
        // Which of your entries removed it: "line:<n>", "strip:<n>" or "name".
        public string DroppedBy = "";
        public bool Data;        // a line without a token of its own
        public int Storage = -1; // the index a $RESOURCE_VISUALIZATION would use, counted in the result
    }

    public sealed class DonorOutcome
    {
        public readonly List<DonorLine> Donor = new List<DonorLine>();
        public readonly List<DonorLine> Result = new List<DonorLine>();
        public readonly List<string> Warnings = new List<string>();
    }

    public static class DonorPlan
    {
        public static string FirstToken(string line)
        {
            if (line == null) return "";
            int at = line.IndexOf('$');
            if (at < 0) return "";
            var token = "$";
            for (int i = at + 1; i < line.Length; i++)
            {
                char c = line[i];
                if (c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_') token += c;
                else break;
            }
            return token.Length > 1 ? token : "";
        }
        static bool Recipe(string token)
        {
            return token == "$PRODUCTION" || token == "$CONSUMPTION" || token == "$CONSUMPTION_PER_SECOND";
        }
        public static bool Replaces(string mine, string theirs)
        {
            if (mine == theirs) return true;
            if (mine.StartsWith("$TYPE_", StringComparison.Ordinal) && theirs.StartsWith("$TYPE_", StringComparison.Ordinal)) return true;
            if (mine.StartsWith("$NAME", StringComparison.Ordinal) && theirs.StartsWith("$NAME", StringComparison.Ordinal)) return true;
            if (mine.StartsWith("$STORAGE", StringComparison.Ordinal) && (theirs.StartsWith("$STORAGE", StringComparison.Ordinal) || theirs == "$RESOURCE_VISUALIZATION")) return true;
            return Recipe(mine) && Recipe(theirs);
        }
        // The name field is written as one $NAME_STR / $NAME line and removes the donor's.
        static string Blame(string token, IList<string> mine, IList<string> strips, bool named)
        {
            if (named && Replaces("$NAME_STR", token)) return "name";
            for (int i = 0; i < mine.Count; i++)
            {
                string own = FirstToken(mine[i]);
                if (own.Length > 0 && Replaces(own, token)) return "line:" + i;
            }
            for (int i = 0; i < strips.Count; i++)
                if (Replaces((strips[i] ?? "").Trim(), token)) return "strip:" + i;
            return "";
        }

        // donor: the donor's building.ini, line by line. mine: the `line =` values in order.
        // strips: the `strip =` tokens. nameLine: the $NAME line the name field produces, or "".
        public static DonorOutcome Build(IEnumerable<string> donor, IEnumerable<string> mine, IEnumerable<string> strips, string nameLine)
        {
            var own = (mine ?? Enumerable.Empty<string>()).Select(x => x ?? "").ToList();
            var strip = (strips ?? Enumerable.Empty<string>()).Select(x => x ?? "").Where(x => x.Trim().Length > 0).ToList();
            bool named = !String.IsNullOrEmpty(nameLine);
            var outcome = new DonorOutcome();
            if (named) outcome.Result.Add(new DonorLine { Text = nameLine, DroppedBy = "name" });
            for (int i = 0; i < own.Count; i++) outcome.Result.Add(new DonorLine { Text = own[i], DroppedBy = "line:" + i, Data = FirstToken(own[i]).Length == 0 });
            outcome.Result.Add(new DonorLine { Text = "" });
            string blame = "";
            foreach (string raw in donor ?? Enumerable.Empty<string>())
            {
                string line = raw ?? "";
                string token = FirstToken(line);
                var entry = new DonorLine { Text = line, Data = token.Length == 0 };
                if (token.Length > 0) blame = Blame(token, own, strip, named);
                else if (blame.Length > 0)
                {
                    // Data lines follow their token. A blank line, a separator or a comment ends the block.
                    string bare = line.Trim();
                    if (bare.Length == 0 || bare[0] == '-' || bare[0] == ';') blame = "";
                }
                if (blame.Length > 0) { entry.Dropped = true; entry.DroppedBy = blame; }
                outcome.Donor.Add(entry);
                if (!entry.Dropped) outcome.Result.Add(new DonorLine { Text = line, Data = entry.Data });
            }
            Number(outcome);
            Doubles(outcome, own);
            return outcome;
        }
        // Two of your own lines that mean the same setting: only one of them counts in the game,
        // and which one is not documented anywhere. Report it instead of writing dead weight.
        static void Doubles(DonorOutcome outcome, IList<string> own)
        {
            var seen = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var told = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (string line in own)
            {
                string key = SettingKey(line);
                if (key.Length == 0) continue;
                if (!seen.ContainsKey(key)) { seen[key] = 1; continue; }
                if (told.Add(key)) outcome.Warnings.Add(Msg.Key("donor_warn_double", FirstToken(line)));
            }
        }
        // Two lines mean the same setting when their token and every word that is not a number
        // match. "$WORKERS_NEEDED 8" and "$WORKERS_NEEDED 15" are one setting; "$PRODUCTION cable
        // 0.06" and "$PRODUCTION steel 0.2" are two recipes, and a storage keeps its resource, which
        // stands at the END of the line. Tabs separate like spaces - $COST_RESOURCE_AUTO uses one.
        // An empty key means "this token may stand several times", see Repeats.
        public static string SettingKey(string line)
        {
            string token = FirstToken(line);
            if (token.Length == 0 || Repeats(token)) return "";
            string key = token;
            string[] words = (line ?? "").Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
            for (int i = 1; i < words.Length; i++) if (!IsNumber(words[i])) key += " " + words[i];
            return key;
        }
        static bool IsNumber(string word)
        {
            if (String.IsNullOrEmpty(word)) return false;
            int at = word[0] == '-' || word[0] == '+' ? 1 : 0;
            bool digit = false, dot = false;
            for (int i = at; i < word.Length; i++)
            {
                if (word[i] >= '0' && word[i] <= '9') { digit = true; continue; }
                if (word[i] == '.' && !dot) { dot = true; continue; }
                return false;
            }
            return digit;
        }
        // Tokens the game itself writes several times in one building with different values: there
        // the payload is geometry, an index or a particle, never a value that could overwrite
        // another one. Measured over the 1021 building.ini of the game and its DLCs - the two cable
        // yards are both "$RESOURCE_VISUALIZATION 3", the difference is in the lines below them.
        // $STORAGE is in here for a second reason: the index of $RESOURCE_VISUALIZATION counts the
        // storages, so folding two of them together moves every yard behind them onto the wrong one.
        public static bool Repeats(string token)
        {
            if (String.IsNullOrEmpty(token)) return true;
            string t = token.ToUpperInvariant();
            if (t.StartsWith("$CONNECTION", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$COST", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$PARTICLE", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$STORAGE", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$MOVEABLE_DOOR", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$VEHICLE", StringComparison.Ordinal)) return true;
            if (t.StartsWith("$MONUMENT_GOVERNMENT_LOYALTY", StringComparison.Ordinal)) return true;
            if (t.IndexOf("POINT", StringComparison.Ordinal) >= 0) return true;
            if (t.IndexOf("PARKING", StringComparison.Ordinal) >= 0) return true;
            if (t.IndexOf("STATION", StringComparison.Ordinal) >= 0) return true;
            if (t.IndexOf("RENDERING_AREA", StringComparison.Ordinal) >= 0) return true;
            return t == "$RESOURCE_VISUALIZATION" || t == "$TOKEN_POS_ROT_SCA" || t == "$OFFSET_CONNECTION_XYZW"
                || t == "$TURNPIKE" || t == "$AMBIENT_SFX" || t == "$MENU_SFX" || t == "$HORSE_WORKPLACE";
        }
        // Storage numbering: $RESOURCE_VISUALIZATION <n> counts the $STORAGE* lines of the finished
        // file from 0. Getting that wrong is invisible in the game - the pile simply never appears.
        static void Number(DonorOutcome outcome)
        {
            int next = 0;
            var water = new List<int>();
            foreach (DonorLine line in outcome.Result)
            {
                string token = FirstToken(line.Text);
                if (!token.StartsWith("$STORAGE", StringComparison.Ordinal)) continue;
                line.Storage = next;
                if (line.Text.IndexOf("RESOURCE_TRANSPORT_WATER", StringComparison.OrdinalIgnoreCase) >= 0
                    || line.Text.IndexOf("RESOURCE_TRANSPORT_SEWAGE", StringComparison.OrdinalIgnoreCase) >= 0) water.Add(next);
                next++;
            }
            foreach (DonorLine line in outcome.Result)
            {
                if (FirstToken(line.Text) != "$RESOURCE_VISUALIZATION") continue;
                string[] parts = line.Text.Trim().Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                int want;
                if (parts.Length < 2 || !Int32.TryParse(parts[1], out want)) continue;
                if (want >= next) { outcome.Warnings.Add(Msg.Key("donor_warn_no_storage", want, next)); continue; }
                // Water and sewage are only created when the game has them switched on, so a
                // storage that is visualised must never sit behind them. No base-game building
                // does it, and the one that did cost an evening.
                if (water.Any(x => x < want)) outcome.Warnings.Add(Msg.Key("donor_warn_water_first", want));
            }
        }
    }
}
