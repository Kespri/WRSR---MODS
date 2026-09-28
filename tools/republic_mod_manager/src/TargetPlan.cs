using System;
using System.Collections.Generic;
using System.Linq;

namespace TesmioAutoload
{
    // What Vanilla Buildings will do to ONE target file, worked out here so the editor can show it
    // before the game does. This is a second implementation of the rule in vanilla_buildings.cpp
    // (ValidateOperations, ValidateSingleLine, IsBlockToken, CountLine) and must not drift: the same
    // cases are checked in the plugin's own self tests and in CoreTests, and both sides carry this note.
    //
    // The rule, in full, and every command is checked against the UNCHANGED original:
    //   add     = <line>                     -> the line must NOT be in the file, and no $COST_ token
    //   replace = <old> | <new>              -> old exactly once, old and new must differ
    //   remove  = <line>                     -> exactly once, and never a $COST_WORK phase header
    //   insert  = 0|1 | <anchor> | <line>    -> anchor exactly once, or produced by an EARLIER
    //                                           add / insert / replace of the same rule set
    // Lines are compared trimmed and case sensitive. A block token ($CONNECTION..., $PARTICLE...)
    // has its own commands and is refused here.
    public sealed class TargetCommand
    {
        public string Kind = "";      // add | replace | remove | insert
        public int Index;             // position inside its own field
        public string Anchor = "";    // the line it looks for ("" for add)
        public string Value = "";     // the line it produces ("" for remove)
        public bool After;            // insert: behind the anchor
        public int At = -1;           // where the anchor sits in the target, -1 = nowhere
        public int End = -1;          // last line of the matched block (a connection spans up to three)
        public int InsertAt = -1;     // where a produced line goes, -1 = nowhere of its own
        public int AnchorOp = -1;     // insert: the earlier command whose produced line is the anchor
        public string[] Parts;        // connection commands: the fields of the command
        public bool One;              // connection commands: the one-point form
        public bool FromOwn;          // insert: the anchor is a line an earlier command produces
        public string Problem = "";   // "" = fine, otherwise a language key with arguments
        public string Text
        {
            get
            {
                if (Kind == "replace") return Anchor + "  |  " + Value;
                if (Kind == "insert") return (After ? "1" : "0") + "  |  " + Anchor + "  |  " + Value;
                return Kind == "remove" || Kind.EndsWith("_connection", StringComparison.Ordinal) ? Anchor : Value;
            }
        }
    }

    public sealed class TargetLineMark
    {
        public string Text = "";
        public int Command = -1;      // which command touches this line, -1 = none
    }

    public sealed class TargetOutcome
    {
        public readonly List<TargetCommand> Commands = new List<TargetCommand>();
        public readonly List<TargetLineMark> Lines = new List<TargetLineMark>();
        public bool Any { get { return Commands.Any(x => x.Problem.Length > 0); } }
    }

    // One line of the file the game will read. Command is the command that produced it, -1 for a
    // line that comes from the original.
    public sealed class TargetResultLine
    {
        public string Text = "";
        public int Command = -1;
    }

    // A connection of the target file: the token line plus one or two point lines, or the whole
    // thing written inline ("$CONNECTION_ROAD_DEAD x y z").
    public sealed class ConnectionBlock
    {
        public int Start, End;
        public string Token = "";
        public double[] First, Second;
        public int Points = 2;
    }

    public static class TargetPlan
    {
        static readonly string[] Blocks =
        {
            "$CONNECTION", "$TEXT_CAPTION", "$VEHICLE_", "$STATION_", "$RESOURCE_VISUALIZATION",
            "$RESOURCE_INCREASE", "$PARTICLE", "$MOVEABLE_DOOR", "$TURNPIKE", "$WORKER_RENDERING_AREA",
            "$UNDERGROUND", "$SHIP_STATION", "$AIRPLANE_STATION", "$HELIPORT", "$CONVEYOR"
        };
        public static string Token(string line) { return DonorPlan.FirstToken(line); }
        public static bool ConnectionToken(string token) { return token.StartsWith("$CONNECTION", StringComparison.Ordinal); }
        static bool AllowPass(string token) { return token.IndexOf("_ALLOWPASS", StringComparison.OrdinalIgnoreCase) >= 0; }
        // The plugin's SameNumber: a relative tolerance, never an exact string compare.
        static bool Same(double a, double b)
        {
            double scale = Math.Max(Math.Abs(a), Math.Abs(b)); if (scale < 1.0) scale = 1.0;
            return Math.Abs(a - b) <= 1e-6 * scale;
        }
        static bool Same(double[] a, double[] b)
        { return a != null && b != null && Same(a[0], b[0]) && Same(a[1], b[1]) && Same(a[2], b[2]); }
        public static double[] Point(string text)
        {
            string[] parts = (text ?? "").Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 3) return null;
            var point = new double[3];
            for (int i = 0; i < 3; i++)
                if (!double.TryParse(parts[i], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out point[i])) return null;
            return point;
        }
        // The plugin's FindConnections, line for line.
        public static List<ConnectionBlock> Connections(IList<string> lines)
        {
            var found = new List<ConnectionBlock>();
            for (int i = 0; i < lines.Count; i++)
            {
                string line = lines[i].Trim(), token = Token(line);
                if (!ConnectionToken(token)) continue;
                var block = new ConnectionBlock { Start = i, Token = token };
                if (line == token)
                {
                    double[] first = i + 1 < lines.Count ? Point(lines[i + 1]) : null;
                    double[] second = i + 2 < lines.Count ? Point(lines[i + 2]) : null;
                    if (first != null && second != null)
                    { block.First = first; block.Second = second; block.End = i + 2; block.Points = 2; found.Add(block); i += 2; continue; }
                    if (first != null)
                    { block.First = first; block.End = i + 1; block.Points = 1; found.Add(block); i += 1; }
                    continue;
                }
                double[] inline = Point(line.Substring(token.Length));
                if (inline == null) continue;
                block.First = inline; block.End = i; block.Points = 1; found.Add(block);
            }
            return found;
        }
        static bool Block(string token) { return Blocks.Any(p => token.StartsWith(p, StringComparison.Ordinal)); }
        // The plugin's ValidateSingleLine. forAdd also refuses every $COST_ token.
        static string Single(string line, bool forAdd)
        {
            string token = Token(line);
            if (token.Length < 2) return "target_problem_no_token";
            if (Block(token)) return "target_problem_block";
            if (token.StartsWith("$COST_WORK_", StringComparison.Ordinal)) return "target_problem_cost_work_geometry";
            if (forAdd && token.StartsWith("$COST_", StringComparison.Ordinal)) return "target_problem_cost";
            return "";
        }
        static int Count(IList<string> lines, string wanted, out int at)
        {
            at = -1; int count = 0; string want = (wanted ?? "").Trim();
            for (int i = 0; i < lines.Count; i++) if (lines[i].Trim() == want) { count++; at = i; }
            return count;
        }
        // The commands in the order the plugin sees them: the effective INI writes the fields in
        // schema order, so add comes before replace, remove and insert - and only an EARLIER
        // command can produce the anchor of an insert.
        public static TargetOutcome Check(IEnumerable<string> target, IEnumerable<string> adds,
            IEnumerable<string> replaces, IEnumerable<string> removes, IEnumerable<string> inserts)
        { return Check(target, adds, replaces, removes, inserts, null, null, null); }
        public static TargetOutcome Check(IEnumerable<string> target, IEnumerable<string> adds,
            IEnumerable<string> replaces, IEnumerable<string> removes, IEnumerable<string> inserts,
            IEnumerable<string> addLinks, IEnumerable<string> replaceLinks, IEnumerable<string> removeLinks)
        {
            var lines = (target ?? Enumerable.Empty<string>()).Select(x => x ?? "").ToList();
            var outcome = new TargetOutcome();
            foreach (string line in lines) outcome.Lines.Add(new TargetLineMark { Text = line });
            Collect(outcome, "add", adds);
            Collect(outcome, "replace", replaces);
            Collect(outcome, "remove", removes);
            Collect(outcome, "insert", inserts);
            Collect(outcome, "add_connection", addLinks);
            Collect(outcome, "replace_connection", replaceLinks);
            Collect(outcome, "remove_connection", removeLinks);
            var blocks = Connections(lines);
            // Where a produced line goes, exactly as the plugin works it out: a new plain line goes
            // in front of the closing "end", a new connection behind the last connection block (or
            // in front of the first $COST_WORK phase when the file has none).
            int lastEnd = -1;
            for (int i = 0; i < lines.Count; i++) if (lines[i].Trim() == "end") lastEnd = i;
            int linkAt = lastEnd;
            if (blocks.Count > 0) linkAt = blocks[blocks.Count - 1].End + 1;
            else for (int i = 0; i < lines.Count; i++) if (Token(lines[i]) == "$COST_WORK") { linkAt = i; break; }
            for (int i = 0; i < outcome.Commands.Count; i++)
                if (outcome.Commands[i].Kind.EndsWith("_connection", StringComparison.Ordinal)) Link(outcome, i, blocks, linkAt);
                else Judge(outcome, i, lines, lastEnd);
            // A file without its closing "end" is refused as a whole by the plugin, before any
            // command is even looked at.
            if (lines.Count > 0 && lastEnd < 0)
                foreach (TargetCommand command in outcome.Commands) command.Problem = "target_problem_no_end";
            return outcome;
        }
        // The file the game will read. A rule set is all or nothing: one command that does not fit
        // makes the plugin drop the WHOLE set for this target (BuildOverlays -> Fail), so as long
        // as anything is red the answer is the unchanged file.
        public static List<TargetResultLine> Result(TargetOutcome outcome, IList<string> lines)
        {
            var made = new List<TargetResultLine>();
            if (outcome == null) return made;
            if (outcome.Any)
            {
                foreach (TargetLineMark mark in outcome.Lines) made.Add(new TargetResultLine { Text = mark.Text });
                return made;
            }
            var all = outcome.Commands;
            for (int i = 0; i < lines.Count; )
            {
                // Lines that follow an anchor come first, so they stay next to it when several
                // commands share the same slot.
                for (int pass = 0; pass < 2; pass++)
                    for (int oi = 0; oi < all.Count; oi++)
                    {
                        TargetCommand op = all[oi];
                        if (op.InsertAt != i) continue;
                        if ((op.Kind == "insert" && op.After) != (pass == 0)) continue;
                        foreach (string text in Produces(op, lines)) made.Add(new TargetResultLine { Text = text, Command = oi });
                    }
                int action = -1;
                for (int oi = 0; oi < all.Count && action < 0; oi++) if (all[oi].At == i && all[oi].InsertAt < 0) action = oi;
                if (action < 0) { made.Add(new TargetResultLine { Text = lines[i] }); i++; continue; }
                TargetCommand acting = all[action];
                foreach (string text in Produces(acting, lines)) made.Add(new TargetResultLine { Text = text, Command = action });
                // remove and remove_connection deliberately produce nothing.
                i = Math.Max(acting.End, acting.At) + 1;
            }
            foreach (TargetCommand op in all)
                if (op.InsertAt >= lines.Count)
                    foreach (string text in Produces(op, lines)) made.Add(new TargetResultLine { Text = text, Command = all.IndexOf(op) });
            // Inserts anchored on a line an earlier command produces, in command order so a chain works.
            for (int oi = 0; oi < all.Count; oi++)
            {
                TargetCommand op = all[oi];
                if (op.Kind != "insert" || op.AnchorOp < 0) continue;
                for (int k = 0; k < made.Count; k++)
                {
                    if (made[k].Command != op.AnchorOp) continue;
                    made.Insert(op.After ? k + 1 : k, new TargetResultLine { Text = op.Value.Trim(), Command = oi });
                    break;
                }
            }
            return made;
        }
        // What one command writes into the result: nothing for the two remove kinds.
        static IEnumerable<string> Produces(TargetCommand op, IList<string> lines)
        {
            string[] parts = op.Parts ?? new string[0];
            switch (op.Kind)
            {
                case "add":
                case "insert": return new[] { op.Value.Trim() };
                case "replace": return new[] { op.Value.Trim() };
                case "add_connection":
                    return op.One ? new[] { parts[0] + " " + parts[1] } : new[] { parts[0], parts[1], parts[2] };
                case "replace_connection":
                    if (op.One)
                    {
                        string point = parts.Length == 4 ? parts[3]
                            : op.End > op.At ? lines[op.At + 1].Trim()
                            : lines[op.At].Trim().Substring(Token(lines[op.At]).Length).Trim();
                        return new[] { parts[2] + " " + point };
                    }
                    bool moves = parts.Length == 6;
                    return new[] { parts[3], moves ? parts[4] : lines[op.At + 1], moves ? parts[5] : lines[op.At + 2] };
                default: return Enumerable.Empty<string>();
            }
        }
        // The three connection commands. A one-point form is told apart the way the plugin does it:
        // by the number of fields, and for replace by whether the third field is a token.
        static void Link(TargetOutcome outcome, int at, List<ConnectionBlock> blocks, int linkAt)
        {
            TargetCommand command = outcome.Commands[at];
            string[] parts = command.Anchor.Split('|').Select(x => x.Trim()).ToArray();
            command.Parts = parts;
            if (parts.Length < 2) { command.Problem = "target_problem_link_form"; return; }
            string token = parts[0];
            if (!ConnectionToken(token)) { command.Problem = "target_problem_link_token"; return; }
            bool one = command.Kind == "replace_connection"
                ? parts.Length == 3 || (parts.Length == 4 && parts[2].StartsWith("$", StringComparison.Ordinal))
                : parts.Length == 2;
            command.One = one;
            double[] first = Point(parts[1]), second = one ? null : (parts.Length > 2 ? Point(parts[2]) : null);
            if (first == null || (!one && second == null)) { command.Problem = "target_problem_link_point"; return; }
            if (command.Kind == "add_connection")
            {
                if (AllowPass(token)) { command.Problem = "target_problem_link_allowpass"; return; }
                if (!one && Same(first, second)) { command.Problem = "target_problem_link_equal"; return; }
                if (blocks.Any(b => b.Token == token && b.Points == (one ? 1 : 2) && Same(b.First, first) && (one || Same(b.Second, second))))
                { command.Problem = "target_problem_link_exists"; return; }
                command.InsertAt = linkAt;
                return;
            }
            var hits = blocks.Where(b => b.Token == token && b.Points == (one ? 1 : 2) && Same(b.First, first) && (one || Same(b.Second, second))).ToList();
            if (hits.Count != 1) { command.Problem = hits.Count == 0 ? "target_problem_link_missing" : Msg.Key("target_problem_many", hits.Count); return; }
            command.At = hits[0].Start; command.End = hits[0].End;
            for (int i = hits[0].Start; i <= hits[0].End && i < outcome.Lines.Count; i++) outcome.Lines[i].Command = at;
            if (command.Kind != "replace_connection") return;
            string fresh = one ? (parts.Length > 2 ? parts[2] : "") : (parts.Length > 3 ? parts[3] : "");
            if (!ConnectionToken(fresh)) { command.Problem = "target_problem_link_new_token"; return; }
            if (AllowPass(fresh)) command.Problem = "target_problem_link_allowpass";
        }
        static void Collect(TargetOutcome outcome, string kind, IEnumerable<string> raw)
        {
            int index = 0;
            foreach (string entry in (raw ?? Enumerable.Empty<string>()))
            {
                string value = (entry ?? "").Trim();
                if (value.Length == 0) continue;
                var command = new TargetCommand { Kind = kind, Index = index++ };
                string[] parts = value.Split('|').Select(x => x.Trim()).ToArray();
                if (kind.EndsWith("_connection", StringComparison.Ordinal)) command.Anchor = value;
                else if (kind == "add") command.Value = value;
                else if (kind == "remove") command.Anchor = value;
                else if (kind == "replace")
                {
                    command.Anchor = parts.Length > 0 ? parts[0] : "";
                    command.Value = parts.Length > 1 ? parts[1] : "";
                    if (parts.Length != 2) command.Problem = "target_problem_replace_form";
                }
                else
                {
                    if (parts.Length != 3) command.Problem = "target_problem_insert_form";
                    else
                    {
                        if (parts[0] != "0" && parts[0] != "1") command.Problem = "target_problem_position";
                        command.After = parts[0] == "1";
                        command.Anchor = parts[1]; command.Value = parts[2];
                    }
                }
                outcome.Commands.Add(command);
            }
        }
        static void Judge(TargetOutcome outcome, int at, IList<string> lines, int lastEnd)
        {
            TargetCommand command = outcome.Commands[at];
            if (command.Problem.Length > 0) return;
            int where;
            if (command.Kind == "add")
            {
                command.Problem = Single(command.Value, true);
                if (command.Problem.Length > 0) return;
                if (Count(lines, command.Value, out where) != 0) { command.Problem = "target_problem_exists"; return; }
                command.InsertAt = lastEnd;
                return;
            }
            if (command.Kind == "replace" || command.Kind == "remove")
            {
                command.Problem = Single(command.Anchor, false);
                if (command.Problem.Length > 0) return;
                if (command.Kind == "remove" && Token(command.Anchor) == "$COST_WORK") { command.Problem = "target_problem_cost_work"; return; }
                int count = Count(lines, command.Anchor, out where);
                if (count != 1) { command.Problem = count == 0 ? "target_problem_missing" : Msg.Key("target_problem_many", count); return; }
                command.At = where; command.End = where; outcome.Lines[where].Command = at;
                if (command.Kind != "replace") return;
                command.Problem = Single(command.Value, false);
                if (command.Problem.Length == 0 && command.Anchor.Trim() == command.Value.Trim()) command.Problem = "target_problem_same";
                return;
            }
            // insert
            command.Problem = Single(command.Value, false);
            if (command.Problem.Length > 0) return;
            if (Token(command.Value) == "$COST_RESOURCE_AUTO" && Token(command.Anchor) != "$COST_WORK")
            { command.Problem = "target_problem_cost_anchor"; return; }
            if (command.Anchor == "end")
            {
                if (command.After) { command.Problem = "target_problem_end_after"; return; }
                if (Count(lines, "end", out where) == 1) { command.At = where; command.InsertAt = where; outcome.Lines[where].Command = at; }
                return;
            }
            if (Token(command.Anchor).Length < 2) { command.Problem = "target_problem_no_token"; return; }
            int hits = Count(lines, command.Anchor, out where);
            if (hits == 1)
            {
                command.At = where; command.InsertAt = command.After ? where + 1 : where;
                outcome.Lines[where].Command = at; return;
            }
            if (hits > 1) { command.Problem = Msg.Key("target_problem_many", hits); return; }
            // Not in the file: an earlier add, insert or replace of this rule set may produce it.
            for (int i = 0; i < at; i++)
            {
                TargetCommand other = outcome.Commands[i];
                if (other.Problem.Length > 0) continue;
                string made = other.Kind == "remove" ? "" : other.Value.Trim();
                if (made.Length > 0 && made == command.Anchor.Trim()) { command.FromOwn = true; command.AnchorOp = i; return; }
            }
            command.Problem = "target_problem_missing";
        }
    }
}
