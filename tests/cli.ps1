param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\pather.exe')
)

$ErrorActionPreference = 'Stop'
$executablePath = (Resolve-Path -LiteralPath $Executable).Path

function Invoke-Pather {
    param([string[]]$CliArguments)

    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $executablePath
    $start.Arguments = $CliArguments -join ' '
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $start.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    $process = [System.Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        return [pscustomobject]@{
            Code = $process.ExitCode
            Stdout = $stdout.Result.Replace("`r`n", "`n")
            Stderr = $stderr.Result.Replace("`r`n", "`n")
        }
    } finally {
        $process.Dispose()
    }
}

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-ExpectedListing {
    param([string[]]$Scopes, [bool]$PathOnly)

    $lines = [System.Collections.Generic.List[string]]::new()
    foreach ($scope in $Scopes) {
        if ($scope -eq 'user') {
            $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment')
        } else {
            $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
                'SYSTEM\CurrentControlSet\Control\Session Manager\Environment')
        }
        try {
            Assert-True ($null -ne $key) "Cannot read $scope environment"
            [string[]]$names = $key.GetValueNames()
            [Array]::Sort($names, [StringComparer]::Ordinal)
            foreach ($name in $names) {
                if ($PathOnly -and $name -ine 'Path') { continue }
                $kind = $key.GetValueKind($name)
                if ($kind -ne [Microsoft.Win32.RegistryValueKind]::String -and
                    $kind -ne [Microsoft.Win32.RegistryValueKind]::ExpandString) { continue }
                $value = $key.GetValue($name, '',
                    [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
                if ($PathOnly) {
                    foreach ($entry in $value.Split(';')) {
                        if ($entry.Length -gt 0) { $lines.Add("[$scope] $name=$entry") }
                    }
                } else {
                    $lines.Add("[$scope] $name=$value")
                }
            }
        } finally {
            if ($null -ne $key) { $key.Dispose() }
        }
    }
    if ($lines.Count -eq 0) { return '' }
    return ($lines -join "`n") + "`n"
}

$helpResult = Invoke-Pather @('-help')
Assert-True ($helpResult.Code -eq 0) '-help must succeed'
Assert-True ($helpResult.Stderr -ceq '') '-help must write only to stdout'
Assert-True ($helpResult.Stdout.Contains('|  _ \')) '-help must include ASCII art'
Assert-True ($helpResult.Stdout.Contains('Pather - Windows environment variable manager')) `
    '-help must identify Pather'
foreach ($command in @('-help', '-ls Path', '-rm -n', '-rm -p', '-c PATH')) {
    Assert-True ($helpResult.Stdout.Contains($command)) "Help must document $command"
}
$mixedCaseHelp = Invoke-Pather @('-HeLp')
Assert-True ($mixedCaseHelp.Code -eq 0 -and $mixedCaseHelp.Stdout -ceq $helpResult.Stdout) `
    'Help must be case-insensitive'

$cases = @(
    @{ Arguments = @('-ls', 'Path'); Scopes = @('user', 'sys'); PathOnly = $true },
    @{ Arguments = @('-LS', 'pAtH', '-e', 'user'); Scopes = @('user'); PathOnly = $true },
    @{ Arguments = @('-ls', 'Path', '-e', 'sys'); Scopes = @('sys'); PathOnly = $true },
    @{ Arguments = @('-ls', 'Path', '-e', 'system'); Scopes = @('sys'); PathOnly = $true },
    @{ Arguments = @('-ls', 'Path', '-e', 'all'); Scopes = @('user', 'sys'); PathOnly = $true },
    @{ Arguments = @('-ls'); Scopes = @('user', 'sys'); PathOnly = $false },
    @{ Arguments = @('-ls', '-e', 'user'); Scopes = @('user'); PathOnly = $false }
)
foreach ($case in $cases) {
    $result = Invoke-Pather $case.Arguments
    $label = $case.Arguments -join ' '
    Assert-True ($result.Code -eq 0) "$label must succeed"
    Assert-True ($result.Stderr -ceq '') "$label must not write to stderr"
    $expected = Get-ExpectedListing $case.Scopes $case.PathOnly
    Assert-True ($result.Stdout -ceq $expected) "$label must match registry values exactly"
}

$invalidCases = @(
    @{ Arguments = @() },
    @{ Arguments = @('-help', 'extra') },
    @{ Arguments = @('-help', '-e', 'user') },
    @{ Arguments = @('-ls', 'OtherVariable') },
    @{ Arguments = @('-ls', 'Path', 'extra') },
    @{ Arguments = @('-ls', 'Path', '-e') },
    @{ Arguments = @('-ls', 'Path', '-e', 'invalid') },
    @{ Arguments = @('-ls', 'Path', '-e', 'user', '-e', 'sys') }
)
foreach ($case in $invalidCases) {
    $result = Invoke-Pather $case.Arguments
    Assert-True ($result.Code -eq 2) 'Invalid arguments must return exit code 2'
    Assert-True ($result.Stdout -ceq '') 'Invalid arguments must not write to stdout'
    Assert-True ($result.Stderr.Contains('Usage:')) 'Invalid arguments must show usage on stderr'
}

Write-Output 'CLI checks passed: help, Path listing, scopes, existing listing, and invalid arguments.'
