using System;
using System.IO;

namespace TesmioAutoload
{
    // What a declared building of Buildings Plus is doing right now. The card list shows one
    // line of this per entry, and it answers the question the player really has before starting
    // the game: is this thing in there, and what happens at the next start?
    //
    // What can be known and what cannot: the folder under media_soviet\workshop_wip carries a
    // tesmioloader.stamp with section=, donor= and object=, so a changed donor or object name is
    // visible here. The stamp's hash is not - it covers the size and the write time of the donor
    // file as well, and recomputing it here would be a second implementation of the plugin's rule
    // that is bound to drift. A changed building.ini line is therefore invisible; the card says so
    // once, above the list, instead of claiming something per row.
    public sealed class BuildingRow
    {
        public string Id = "";       // the section name in the INI
        public string Number = "";   // the Workshop id, from the field or assigned by the plugin
        public string Donor = "";
        public string Object = "";
        public bool Enabled = true;
    }

    public static class BuildingStates
    {
        public sealed class Result
        {
            public string Text = "";
            // normal | muted | warn | error - the editor turns this into a colour.
            public string Tone = "muted";
        }

        public static Result Of(string build, BuildingRow row, bool prune, Language language)
        {
            var result = new Result();
            if (row == null) return result;
            string donor = (row.Donor ?? "").Trim();
            // No donor, or one that is not on this machine: the plugin refuses the section and
            // logs it. Worth a red line here, long before the game says anything.
            if (donor.Length == 0 || Safe(() => GameBuildings.DonorFile(build, donor)) == null)
            {
                result.Text = language.T("building_state_no_donor");
                result.Tone = "error";
                return result;
            }
            string number = (row.Number ?? "").Trim();
            string folder = null;
            if (number.Length > 0)
            {
                string root = Safe(() => WipBuildings.Root(build));
                if (root != null) folder = Path.Combine(root, number);
            }
            bool exists = folder != null && Safe(() => Directory.Exists(folder) ? folder : null) != null;
            if (!exists)
            {
                result.Text = language.T(row.Enabled ? "building_state_pending" : "building_state_off");
                return result;
            }
            WipBuilding stamped = Safe(() => WipBuildings.Read(folder));
            // A folder of that number written by somebody else. The plugin stops at this with
            // "refusing to touch it" and only the log says so - here it is a red line up front.
            if (stamped == null || stamped.Origin != "buildings_plus")
            {
                result.Text = language.T("building_state_foreign");
                result.Tone = "error";
                return result;
            }
            if (!row.Enabled)
            {
                result.Text = language.T(prune ? "building_state_off_pruned" : "building_state_off_kept");
                if (!prune) result.Tone = "warn";
                return result;
            }
            string want = (row.Object ?? "").Trim();
            if (want.Length == 0) want = row.Id;
            bool sameDonor = stamped.Donor.Length == 0 || stamped.Donor.Equals(donor, StringComparison.OrdinalIgnoreCase);
            // row.Object == null: the schema has no object field, so the object name is not ours to
            // compare. Only an explicit value is checked against the stamp.
            bool sameObject = row.Object == null || stamped.Object.Length == 0 || stamped.Object.Equals(want, StringComparison.OrdinalIgnoreCase);
            result.Text = language.T(sameDonor && sameObject ? "building_state_ready" : "building_state_rebuild");
            result.Tone = "normal";
            return result;
        }

        // Every lookup here touches the disk of a folder the user may have pulled away mid-read.
        // A state line is never worth an exception.
        static T Safe<T>(Func<T> read) where T : class
        {
            try { return read(); }
            catch (Exception) { return null; }
        }
    }
}
