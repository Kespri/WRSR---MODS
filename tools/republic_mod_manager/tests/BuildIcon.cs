using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

static class BuildIcon
{
    static int Main(string[] args)
    {
        try
        {
            var frames = new List<byte[]>(); int[] sizes = {16,20,24,32,40,48,64,128,256};
            using (var source = new Bitmap(args[0])) foreach (int size in sizes)
            {
                using (var bitmap = new Bitmap(size,size,PixelFormat.Format32bppArgb))
                {
                    using (var g = Graphics.FromImage(bitmap)) { g.CompositingMode = CompositingMode.SourceCopy; g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.PixelOffsetMode = PixelOffsetMode.HighQuality; g.DrawImage(source,new Rectangle(0,0,size,size)); }
                    using (var ms = new MemoryStream()) using (var w = new BinaryWriter(ms))
                    {
                        if (size >= 128) bitmap.Save(ms,ImageFormat.Png);
                        else
                        {
                            int stride = ((size+31)/32)*4;
                            w.Write(40); w.Write(size); w.Write(size*2); w.Write((short)1); w.Write((short)32); w.Write(0); w.Write(size*size*4+stride*size); w.Write(0); w.Write(0); w.Write(0); w.Write(0);
                            for (int y=size-1;y>=0;y--) for (int x=0;x<size;x++) { var c=bitmap.GetPixel(x,y); w.Write(c.B); w.Write(c.G); w.Write(c.R); w.Write(c.A); }
                            for (int y=size-1;y>=0;y--) { var mask=new byte[stride]; for(int x=0;x<size;x++) if(bitmap.GetPixel(x,y).A==0) mask[x/8]|=(byte)(128>>(x%8)); w.Write(mask); }
                        }
                        frames.Add(ms.ToArray());
                    }
                }
            }
            using (var output = new BinaryWriter(File.Create(args[1])))
            {
                output.Write((short)0); output.Write((short)1); output.Write((short)sizes.Length); int offset=6+16*sizes.Length;
                for(int i=0;i<sizes.Length;i++) { int size=sizes[i]; output.Write((byte)(size==256?0:size)); output.Write((byte)(size==256?0:size)); output.Write((byte)0); output.Write((byte)0); output.Write((short)1); output.Write((short)32); output.Write(frames[i].Length); output.Write(offset); offset+=frames[i].Length; }
                foreach(byte[] data in frames) output.Write(data);
            }
            Console.WriteLine("PASS ICO: 16,20,24,32,40,48,64,128,256 px; original alpha retained."); return 0;
        }
        catch(Exception e) { Console.Error.WriteLine(e); return 1; }
    }
}
