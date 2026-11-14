# LuxAI penceresinin ekran goruntusunu alir (GitHub Windows runner icin).
# Once PrintWindow ile pencereyi cizer; olmazsa tam ekrani kopyalar.
param(
  [string]$Out = "shot.png",
  [string]$TitleMatch = "LuxAI"
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class LuxWin {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern IntPtr FindWindow(string cls, string title);
}
"@

[void][LuxWin]::SetProcessDPIAware()

$dir = Split-Path -Parent $Out
if ($dir -and !(Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }

$proc = Get-Process | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like "*$TitleMatch*" } |
        Select-Object -First 1
$saved = $false

if ($proc) {
  $hwnd = $proc.MainWindowHandle
  Write-Host ("pencere: '{0}' hwnd={1}" -f $proc.MainWindowTitle, $hwnd)
  [void][LuxWin]::ShowWindow($hwnd, 5)          # SW_SHOW
  [void][LuxWin]::SetForegroundWindow($hwnd)
  Start-Sleep -Milliseconds 900

  $r = New-Object LuxWin+RECT
  [void][LuxWin]::GetWindowRect($hwnd, [ref]$r)
  $w = $r.Right - $r.Left
  $h = $r.Bottom - $r.Top
  if ($w -gt 100 -and $h -gt 100) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    $ok = [LuxWin]::PrintWindow($hwnd, $hdc, 2)   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
    if (-not $ok) {
      Write-Host "PrintWindow basarisiz, ekrandan kopyalaniyor"
      $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    }
    $g.Dispose()
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("kaydedildi: {0} ({1}x{2})" -f $Out, $w, $h)
    $saved = $true
  }
}

if (-not $saved) {
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $g.Dispose()
  $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Host ("tam ekran kaydedildi: {0} ({1}x{2})" -f $Out, $b.Width, $b.Height)
}
