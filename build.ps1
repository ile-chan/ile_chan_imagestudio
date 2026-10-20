param([ValidateSet('Release','Debug','ASan')][string]$Configuration='Release',[switch]$Test)
$ErrorActionPreference='Stop'
$vswherePath='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Visual Studio 2022 C++ tools are required.' }
$msbuildPath = & $vswherePath -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuildPath) { throw 'MSBuild was not found. Install the Desktop development with C++ workload.' }
& $msbuildPath (Join-Path $PSScriptRoot 'WkyImageStudio.sln') '/m' '/nologo' '/v:minimal' "/p:Configuration=$Configuration" '/p:Platform=x64'
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
if ($Test) {
    if ($Configuration -eq 'ASan') {
        $vsInstallPath = & $vswherePath -latest -products '*' -version '[17.0,18.0)' -property installationPath
        $runtimeFolder = Get-ChildItem -LiteralPath (Join-Path $vsInstallPath 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
        $env:PATH=(Join-Path $runtimeFolder.FullName 'bin\Hostx64\x64')+';'+$env:PATH
        $env:ASAN_OPTIONS='halt_on_error=1'
    }
    $testOutput=Join-Path $PSScriptRoot "tests\$Configuration"
    New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
    $testProcess=Start-Process -FilePath (Join-Path $PSScriptRoot "bin\$Configuration\ile chan的图像工坊.exe") -ArgumentList @('--self-test',('"'+$testOutput+'"')) -WindowStyle Hidden -PassThru -Wait
    Get-Content -LiteralPath (Join-Path $testOutput 'self-test.txt') -Encoding UTF8
    if ($testProcess.ExitCode -ne 0) { throw 'Self-tests failed.' }
}
