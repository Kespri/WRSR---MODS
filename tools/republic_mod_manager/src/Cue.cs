// Republic Mod Manager - grey hint text in an empty TextBox (0.4.31): the Win32
// cue banner, shown while the box is empty, with or without focus.
using System;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace TesmioAutoload
{
    static class Cue
    {
        [DllImport("user32.dll", CharSet = CharSet.Unicode)]
        static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, string lParam);
        const int EM_SETCUEBANNER = 0x1501;

        public static void Set(TextBox box, string text)
        {
            if (box == null || String.IsNullOrEmpty(text)) return;
            EventHandler apply = (s, e) => SendMessage(box.Handle, EM_SETCUEBANNER, (IntPtr)1, text);
            if (box.IsHandleCreated) apply(box, EventArgs.Empty); else box.HandleCreated += apply;
        }
    }
}
