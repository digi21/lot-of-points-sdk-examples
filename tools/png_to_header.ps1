# Writes PNG bytes to a C header for lop_command::icon_png.
# Usage: png_to_header.ps1 <icon.png> <header.h> <array name>
param(
    [Parameter(Mandatory)] [string] $Png,
    [Parameter(Mandatory)] [string] $Header,
    [Parameter(Mandatory)] [string] $Name
)

$bytes = [IO.File]::ReadAllBytes((Resolve-Path $Png))
$lines = for ($i = 0; $i -lt $bytes.Length; $i += 16) {
    $last = [math]::Min($i + 15, $bytes.Length - 1)
    '    ' + (($bytes[$i..$last] | ForEach-Object { '0x{0:x2},' -f $_ }) -join ' ')
}
$text = @(
    "/* $(Split-Path $Header -Leaf) - $(Split-Path $Png -Leaf) as bytes, written by tools/png_to_header.ps1. */"
    "static unsigned char const $Name[] ="
    '{'
    $lines
    '};'
    ''
) -join "`r`n"
# Resolves Header relative to PowerShell's current location, as Resolve-Path does for Png.
[IO.File]::WriteAllText($ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Header), $text, [Text.UTF8Encoding]::new($false))
