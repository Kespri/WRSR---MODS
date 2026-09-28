Add-Type -AssemblyName System.Drawing
$s = Split-Path -Parent $MyInvocation.MyCommand.Path
$navy = [System.Drawing.Color]::FromArgb(12,32,54); $blue = [System.Drawing.Color]::FromArgb(0,85,215); $blueDark = [System.Drawing.Color]::FromArgb(0,62,160); $amber = [System.Drawing.Color]::FromArgb(232,166,36); $white = [System.Drawing.Color]::White

function RoundRect([System.Drawing.Graphics]$g, [System.Drawing.Brush]$b, [float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath; $d = $r*2
    $p.AddArc($x,$y,$d,$d,180,90); $p.AddArc($x+$w-$d,$y,$d,$d,270,90); $p.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90); $p.AddArc($x,$y+$h-$d,$d,$d,90,90); $p.CloseFigure()
    $g.FillPath($b,$p); $p.Dispose()
}
function GearPath([float]$cx, [float]$cy, [float]$rOut, [float]$rIn, [int]$teeth, [float]$toothFrac) {
    $pts = New-Object System.Collections.Generic.List[System.Drawing.PointF]
    $step = 2*[Math]::PI/$teeth; $half = $step*$toothFrac/2; $taper = $half*0.35
    for ($i=0; $i -lt $teeth; $i++) {
        $a = $i*$step
        $angles = @(($a-$half), ($a-$half+$taper), ($a+$half-$taper), ($a+$half))
        $radii  = @($rIn, $rOut, $rOut, $rIn)
        for ($k=0; $k -lt 4; $k++) { $ang = [double]$angles[$k]; $r = [double]$radii[$k]; $pts.Add((New-Object System.Drawing.PointF(($cx + $r*[Math]::Cos($ang)), ($cy + $r*[Math]::Sin($ang))))) }
    }
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath; $path.AddPolygon($pts.ToArray()); return $path
}
function DrawB([int]$S) {
    $bmp = New-Object System.Drawing.Bitmap($S,$S); $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode='AntiAlias'; $g.PixelOffsetMode='HighQuality'; $g.Clear([System.Drawing.Color]::Transparent); $u=$S/256.0
    $rect = New-Object System.Drawing.RectangleF(0,0,$S,$S); $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $blue, $blueDark, 90.0)
    RoundRect $g $lg 0 0 $S $S (46*$u); $lg.Dispose()
    $gear = GearPath (128*$u) (128*$u) (106*$u) (86*$u) 8 0.5
    $shadow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(70,0,20,60)); $m = New-Object System.Drawing.Drawing2D.Matrix; $m.Translate((2*$u),(4*$u)); $gs = $gear.Clone(); $gs.Transform($m); $g.FillPath($shadow,$gs); $gs.Dispose(); $shadow.Dispose()
    $wb = New-Object System.Drawing.SolidBrush($white); $g.FillPath($wb,$gear); $gear.Dispose()
    $bb = New-Object System.Drawing.SolidBrush($blue); $g.FillEllipse($bb,(66*$u),(66*$u),(124*$u),(124*$u)); $bb.Dispose()
    $pen = New-Object System.Drawing.Pen($amber, (22*$u)); $pen.StartCap='Round'; $pen.EndCap='Round'; $pen.LineJoin='Round'
    $pts = New-Object 'System.Drawing.PointF[]' 3
    $pts[0] = New-Object System.Drawing.PointF((90*$u),(132*$u)); $pts[1] = New-Object System.Drawing.PointF((117*$u),(159*$u)); $pts[2] = New-Object System.Drawing.PointF((168*$u),(102*$u))
    $g.DrawLines($pen, $pts); $pen.Dispose(); $wb.Dispose(); $g.Dispose(); return $bmp
}
$big = DrawB 1024; $big.Save("$s\rmm_icon_B_1024.png")
$sheet = New-Object System.Drawing.Bitmap(1000, 520); $sg = [System.Drawing.Graphics]::FromImage($sheet)
$sg.Clear([System.Drawing.Color]::FromArgb(239,243,249)); $sg.InterpolationMode='HighQualityBicubic'; $sg.SmoothingMode='AntiAlias'; $sg.TextRenderingHint='AntiAliasGridFit'
$font = New-Object System.Drawing.Font("Segoe UI", 12); $tb = New-Object System.Drawing.SolidBrush($navy); $tw = New-Object System.Drawing.SolidBrush($white)
$sg.DrawImage($big, 24, 24, 400, 400)
$x = 470
foreach ($sz in 128,64,48,32,24,16) { $img = DrawB $sz; $sg.DrawImage($img, $x, 24 + (128-$sz)); $sg.DrawString("$sz",$font,$tb,$x,160); $x += $sz + 24; $img.Dispose() }
$sg.FillRectangle((New-Object System.Drawing.SolidBrush($white)), 470, 200, 500, 32); $t16 = DrawB 16; $sg.DrawImage($t16, 478, 208); $sg.DrawString("Republic Mod Manager 0.5.9  -  fuer TesmioLoader b0.3.6",$font,$tb,500,206)
$sg.FillRectangle((New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(32,32,32))), 470, 250, 500, 48); $t24 = DrawB 24; $sg.DrawImage($t24, 486, 262); $sg.DrawString("Taskleiste",$font,$tw,520,264)
$sg.FillRectangle((New-Object System.Drawing.SolidBrush($navy)), 470, 320, 500, 100); $t40 = DrawB 40; $sg.DrawImage($t40, 490, 350)
$f2 = New-Object System.Drawing.Font("Segoe UI", 15, [System.Drawing.FontStyle]::Bold); $sg.DrawString("Republic Mod Manager",$f2,$tw,540,344); $sg.DrawString("Plugins fuer TesmioLoader",$font,$tw,540,374)
$sg.DrawString("Farben: Aktionsblau 0,85,215 mit Verlauf nach 0,62,160; Weiss; Bernstein 232,166,36",$font,$tb,24,450)
$sg.DrawString("Form: 8 Zaehne, blaue Nabe, Haken mit runden Enden, leichter Schatten unter dem Zahnrad",$font,$tb,24,475)
$sheet.Save("$s\rmm_icon_B_sheet.png"); $sg.Dispose(); $sheet.Dispose(); $big.Dispose()
Write-Output "ok"
