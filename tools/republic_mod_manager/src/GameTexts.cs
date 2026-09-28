// Republic Mod Manager - the game's language files (0.4.21).
// media_soviet\soviet<Language>.btf: big-endian u32 count, u32 payload size,
// u32 payload units, then count x { u32 id, u32 offset, u16 length } and a
// UTF-16BE payload; offsets and lengths are in code units. The ids are the
// same in every language, which is what a text-wrap list keys on.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TesmioAutoload
{
    public sealed class GameTextEntry
    {
        public int Id; public string Text = "", FirstLine = ""; public int Lines, Longest;
    }

    public static class GameTexts
    {
        static readonly Regex LanguageSetting = new Regex(@"^\s*\$TEXT\s+LANGUAGE2\s+(\S+)", RegexOptions.Multiline);

        public static string MediaFolder(string game) { return Path.Combine(game, "media_soviet"); }
        public static string FilePath(string game, string language) { return Path.Combine(MediaFolder(game), "soviet" + language + ".btf"); }

        // Language names as the files spell them (German, English, ...), sorted; the
        // developer comments file is not a language.
        public static List<string> Languages(string game)
        {
            var names = new List<string>();
            string media = MediaFolder(game);
            if (game == null || !Directory.Exists(media)) return names;
            foreach (string file in Directory.GetFiles(media, "soviet*.btf"))
            {
                string name = Path.GetFileNameWithoutExtension(file).Substring(6);
                if (name.Length == 0 || name.Equals("Comments", StringComparison.OrdinalIgnoreCase)) continue;
                names.Add(name);
            }
            names.Sort(StringComparer.OrdinalIgnoreCase);
            return names;
        }

        // The language chosen in media_soviet\config.ini ($TEXT LANGUAGE2 <name>); null
        // for "auto" (the game then follows Steam) or when nothing is set.
        public static string ConfiguredLanguage(string game)
        {
            try
            {
                string config = Path.Combine(MediaFolder(game), "config.ini");
                if (!File.Exists(config)) return null;
                var match = LanguageSetting.Match(File.ReadAllText(config));
                if (!match.Success) return null;
                string value = match.Groups[1].Value.Trim();
                return value.Equals("auto", StringComparison.OrdinalIgnoreCase) ? null : value;
            }
            catch (Exception) { return null; }
        }

        // The configured language when its file exists, otherwise the file matching the
        // UI language (de -> German, en -> English), otherwise the first file found.
        public static string DefaultLanguage(string game, string uiLanguage)
        {
            var available = Languages(game);
            if (available.Count == 0) return null;
            string configured = ConfiguredLanguage(game);
            if (configured != null) { string hit = available.FirstOrDefault(x => x.Equals(configured, StringComparison.OrdinalIgnoreCase)); if (hit != null) return hit; }
            string wanted = (uiLanguage ?? "").StartsWith("de", StringComparison.OrdinalIgnoreCase) ? "German" : "English";
            return available.FirstOrDefault(x => x.Equals(wanted, StringComparison.OrdinalIgnoreCase)) ?? available[0];
        }

        public static List<GameTextEntry> Load(string game, string language)
        {
            string path = FilePath(game, language);
            if (!File.Exists(path)) throw new FileNotFoundException(Msg.Key("err_btf_missing", path));
            try { return Parse(File.ReadAllBytes(path)); }
            catch (FormatException) { throw new FormatException(Msg.Key("err_btf_format", path)); }
        }

        public static List<GameTextEntry> Parse(byte[] data)
        {
            if (data == null || data.Length < 12) throw new FormatException("btf");
            uint count = ReadU32(data, 0);
            long payload = 12L + count * 10L;
            if (payload > data.Length) throw new FormatException("btf");
            var result = new List<GameTextEntry>((int)Math.Min(count, 100000));
            int position = 12;
            for (uint i = 0; i < count; i++)
            {
                uint id = ReadU32(data, position); uint offset = ReadU32(data, position + 4); int length = ReadU16(data, position + 8); position += 10;
                long start = payload + offset * 2L;
                if (start < payload || start + length * 2L > data.Length) throw new FormatException("btf");
                var text = new StringBuilder(length);
                for (int k = 0; k < length; k++) text.Append((char)((data[start + 2 * k] << 8) | data[start + 2 * k + 1]));
                result.Add(Entry((int)id, text.ToString()));
            }
            return result;
        }

        // The inverse of Parse, for tests and tools: one table entry per pair, payload in
        // the given order without sharing.
        public static byte[] Build(IList<KeyValuePair<int, string>> texts)
        {
            var payload = new List<byte>();
            var table = new List<byte>();
            foreach (var pair in texts)
            {
                int offset = payload.Count / 2;
                WriteU32(table, (uint)pair.Key); WriteU32(table, (uint)offset); table.Add((byte)(pair.Value.Length >> 8)); table.Add((byte)(pair.Value.Length & 0xFF));
                foreach (char c in pair.Value) { payload.Add((byte)(c >> 8)); payload.Add((byte)(c & 0xFF)); }
            }
            var file = new List<byte>();
            WriteU32(file, (uint)texts.Count); WriteU32(file, (uint)payload.Count); WriteU32(file, (uint)(payload.Count / 2));
            file.AddRange(table); file.AddRange(payload);
            return file.ToArray();
        }

        static GameTextEntry Entry(int id, string text)
        {
            var entry = new GameTextEntry { Id = id, Text = text };
            string[] lines = text.Replace("\r", "").Split('\n');
            entry.Lines = lines.Length;
            entry.Longest = lines.Max(l => l.Length);
            entry.FirstLine = lines[0];
            return entry;
        }

        static uint ReadU32(byte[] d, int at) { return ((uint)d[at] << 24) | ((uint)d[at + 1] << 16) | ((uint)d[at + 2] << 8) | d[at + 3]; }
        static int ReadU16(byte[] d, int at) { return (d[at] << 8) | d[at + 1]; }
        static void WriteU32(List<byte> to, uint value) { to.Add((byte)(value >> 24)); to.Add((byte)(value >> 16)); to.Add((byte)(value >> 8)); to.Add((byte)value); }
    }
}
