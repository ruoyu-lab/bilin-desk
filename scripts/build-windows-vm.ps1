# 在 Windows 虚拟机里做 x64 的安装包（scripts/build-windows-vm.sh 交过来运行）：源码整个换新，编译、做安装包，
# 再静默装一遍试试。哪一步不成就停；上一次的安装包先删掉，编不成就没有安装包可拿。
# 成了最后一行是 BUILD-OK <安装包的 SHA-256>
param([string]$Qt = 'C:\Qt\6.11.2\msvc2022_64')
$ProgressPreference = 'SilentlyContinue'
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$source = 'C:\src\bilin'
# 构建目录按 Qt 分开（ARM64、x64 的 Qt 不能共用一个）
$build = 'C:\src\bilin-build-' + (Split-Path $Qt -Leaf)
$out = 'C:\src\bilin-out'
$installer = "$out\Bilin-Setup.exe"

$tar = "C:\src\bilin-src-" + [guid]::NewGuid().ToString('N') + '.tar'
Copy-Item \\Mac\Home\Downloads\bilin-src.tar $tar
if (Test-Path $source) { Remove-Item -Recurse -Force $source }
New-Item -ItemType Directory $source | Out-Null
tar -xf $tar -C $source
if ($LASTEXITCODE -ne 0) { throw '解不开源码' }
Remove-Item $tar -Force
if (Test-Path $installer) { Remove-Item -Force $installer }
Select-String -Path "$source\CMakeLists.txt" -Pattern '^project' | % { $_.Line }

cmd /c "$source\scripts\build-windows-installer.cmd $Qt $build $out > C:\src\bilin-build.log 2>&1"
$built = $LASTEXITCODE
"installer exit: $built"
if ($built -ne 0) {
    Select-String -Path C:\src\bilin-build.log -Pattern ': error|FAILED|CMake Error' | Select -First 12 | % { $_.Line }
    exit 1
}

$install = Start-Process -FilePath $installer -ArgumentList '--quiet' -PassThru -Wait
"install exit: $($install.ExitCode)"
if ($install.ExitCode -ne 0) { exit 1 }
Start-Sleep -Seconds 8
Get-Process | Where-Object { $_.Name -match '^Bilin' } | Select Id, Name, SessionId | Format-Table -AutoSize | Out-String
'BUILD-OK ' + (Get-FileHash -Algorithm SHA256 $installer).Hash.ToLower()
