<#
.SYNOPSIS
	Local-only helper: rebase the current branch onto `source/dev`, then
	reconfigure (and optionally rebuild the Qt 6 / third-party libraries) so the
	Visual Studio solution is regenerated against Qt 6.11.1.

.DESCRIPTION
	Telegram Desktop's build scripts (Telegram\build\qt_version.py) OVERRIDE the
	`QT` environment variable when passed the `qt6` argument, setting it to the
	pinned Qt 6 version. This script always passes `qt6` so you stay on Qt 6.

	Important: that override only lives inside configure.py's own process. Any
	later implicit CMake reconfigure - which happens whenever an upstream merge
	touches CMakeLists.txt, either from `cmake --build` or from Visual Studio -
	re-reads the ambient `QT` variable. The Visual Studio Qt extension commonly
	leaves `QT=6.9.0` / `QTDIR=C:\Qt\6.9.0\...` in the environment, and
	cmake/external/qt/package.cmake does `set(qt_requested $ENV{QT} ... FORCE)`,
	producing:

		Configured Qt version 6.11.1 does not match requested version 6.9.0.

	So this script pins `QT` for the whole session and clears the stale `QTDIR`.

	Typical flow after a new merge from upstream:
		.\tools\rebuild-qt6.ps1                 # sync + configure (fast)
		.\tools\rebuild-qt6.ps1 -Build          # also compile a Release exe
		.\tools\rebuild-qt6.ps1 -Prepare        # also rebuild Qt/libs (slow!)

	The Qt/library rebuild (-Prepare) compiles Qt from source and can take
	several hours. You normally only need it when the pinned Qt version or the
	third-party libraries actually change.

.PARAMETER Prepare
	Also run Telegram\build\prepare\win.bat qt6 (rebuilds Qt 6 + libraries).
	Slow (multi-hour). Skip it for ordinary merges that don't bump Qt.

.PARAMETER SkipRebase
	Skip the fetch + integrate step and only reconfigure/prepare.

.PARAMETER Strategy
	How to integrate source/dev: 'merge' (default, preserves local commits) or
	'rebase' (rewrites them).

	Use 'merge' for the normal flow. Once dev has been pushed to origin - which
	it has, and which PR merges land on - rebasing rewrites published history
	and forces a --force-with-lease push, breaking any other clone of the fork.
	Only use 'rebase' on a local branch nobody else has pulled.

.PARAMETER Build
	After configuring, compile the Release Telegram.exe via cmake --build.

.PARAMETER SkipSubmodules
	Skip `git submodule update --init --recursive`. Not recommended: upstream
	adds new submodules periodically (e.g. libcbor/libfido2) and configure fails
	with a missing-file error until they are cloned.

.PARAMETER Arch
	Target architecture: x64 (default), x86 or arm.

.PARAMETER ApiId
	Telegram api_id. Overrides the value from the .env file / environment.

.PARAMETER ApiHash
	Telegram api_hash. Overrides the value from the .env file / environment.

.PARAMETER EnvFile
	Path to a .env file with TDESKTOP_API_ID / TDESKTOP_API_HASH.
	Defaults to ".env" in the repository root (already gitignored).

.PARAMETER VcVarsVer
	MSVC toolset version passed to vcvars (default 14.44, per docs\building-win.md).

.EXAMPLE
	# .env file in the repository root:
	#   TDESKTOP_API_ID=12345
	#   TDESKTOP_API_HASH=abcdef0123456789abcdef0123456789
	.\tools\rebuild-qt6.ps1 -Build
#>
[CmdletBinding()]
param(
	[switch]$Prepare,
	[switch]$SkipRebase,
	[switch]$SkipSubmodules,
	[switch]$Build,
	[ValidateSet('merge', 'rebase')]
	[string]$Strategy = 'merge',
	[ValidateSet('x64', 'x86', 'arm')]
	[string]$Arch = 'x64',
	[string]$ApiId,
	[string]$ApiHash,
	[string]$EnvFile,
	[string]$VcVarsVer = '14.44'
)

$ErrorActionPreference = 'Stop'

# Resolve the repository root from git rather than from this script's own
# location, so the script keeps working if it is moved or invoked from
# anywhere. Falls back to the parent of the script directory (tools/..).
function Get-RepoRoot([string]$scriptRoot) {
	$fallback = Split-Path -Parent $scriptRoot
	try {
		$top = (& git -C $scriptRoot rev-parse --show-toplevel 2>$null)
		if ($LASTEXITCODE -eq 0 -and $top) {
			return (Resolve-Path $top.Trim()).Path
		}
	} catch {
		# git missing or not a repository - fall through.
	}
	return $fallback
}

$RepoRoot = Get-RepoRoot $PSScriptRoot
$BuildPath = Split-Path -Parent $RepoRoot   # e.g. C:\TBuild (where win.bat runs from)

if (-not (Test-Path (Join-Path $RepoRoot 'Telegram\configure.bat'))) {
	throw "Could not locate the tdesktop checkout. Resolved repo root as '$RepoRoot', but Telegram\configure.bat is not there."
}

# The Qt 6 version pinned by Telegram\build\qt_version.py. Parsed rather than
# hardcoded so an upstream Qt bump does not silently desync this script.
function Get-PinnedQtVersion([string]$repoRoot) {
	$path = Join-Path $repoRoot 'Telegram\build\qt_version.py'
	if (Test-Path $path) {
		$content = Get-Content -LiteralPath $path -Raw
		# Take the version assigned in the win32 / qt6 branch.
		$found = [regex]::Matches($content, "os\.environ\['QT'\]\s*=\s*'(6\.[0-9.]+)'")
		if ($found.Count -gt 0) {
			return $found[$found.Count - 1].Groups[1].Value
		}
	}
	throw "Could not determine the pinned Qt 6 version from $path."
}
function Write-Step($message) {
	Write-Host ""
	Write-Host "==> $message" -ForegroundColor Cyan
}

# --- Load KEY=VALUE pairs from a .env file into the process environment --------
function Import-DotEnv([string]$path) {
	if (-not (Test-Path $path)) { return }
	Write-Step "Loading secrets from $path"
	foreach ($rawLine in Get-Content -LiteralPath $path) {
		$line = $rawLine.Trim()
		if (-not $line -or $line.StartsWith('#')) { continue }
		$eq = $line.IndexOf('=')
		if ($eq -lt 1) { continue }
		$key = $line.Substring(0, $eq).Trim()
		$value = $line.Substring($eq + 1).Trim()
		# Strip optional surrounding single/double quotes.
		if ($value.Length -ge 2 -and
			(($value.StartsWith('"') -and $value.EndsWith('"')) -or
			 ($value.StartsWith("'") -and $value.EndsWith("'")))) {
			$value = $value.Substring(1, $value.Length - 2)
		}
		Set-Item -Path ("Env:" + $key) -Value $value
	}
}

function Invoke-Native($file, [string[]]$arguments, $workingDir) {
	Push-Location $workingDir
	try {
		& $file @arguments
		if ($LASTEXITCODE -ne 0) {
			throw "$file $($arguments -join ' ') failed with exit code $LASTEXITCODE"
		}
	} finally {
		Pop-Location
	}
}

# --- Import the Visual Studio developer environment (needed by cmake/prepare) ---
function Import-VsDevEnv([string]$targetArch, [string]$vcvarsVer) {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
	if (-not (Test-Path $vswhere)) {
		$vswhere = Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
	}
	if (-not (Test-Path $vswhere)) {
		throw "vswhere.exe not found; is Visual Studio installed?"
	}

	$vsPath = & $vswhere -latest -prerelease -property installationPath | Select-Object -First 1
	if (-not $vsPath) { throw "Could not locate a Visual Studio installation." }

	$vcvarsall = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
	if (-not (Test-Path $vcvarsall)) { throw "vcvarsall.bat not found at $vcvarsall" }

	$vcvarsArch = switch ($targetArch) {
		'x64' { 'x64' }
		'x86' { 'x86' }
		'arm' { 'x64_arm64' }
	}

	Write-Step "Importing Visual Studio environment ($vcvarsArch, toolset $vcvarsVer)"
	$line = "`"$vcvarsall`" $vcvarsArch -vcvars_ver=$vcvarsVer && set"
	cmd.exe /c $line | ForEach-Object {
		if ($_ -match '^([^=]+)=(.*)$') {
			Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
		}
	}
}

# --- 0. Pin the Qt version for this whole session ------------------------------
# Must happen before ANY cmake invocation, including the implicit reconfigure
# that `cmake --build` triggers after an upstream merge. See .DESCRIPTION.
$PinnedQt = Get-PinnedQtVersion $RepoRoot
Write-Step "Pinning QT=$PinnedQt for this session"
if ($env:QT -and $env:QT -ne $PinnedQt) {
	Write-Host "Overriding inherited QT=$($env:QT) (likely from the Visual Studio Qt extension)."
}
$env:QT = $PinnedQt
# A stale QTDIR pointing at a different Qt poisons CMAKE_PREFIX_PATH.
if ($env:QTDIR -and $env:QTDIR -notmatch [regex]::Escape($PinnedQt)) {
	Write-Host "Clearing stale QTDIR=$($env:QTDIR)."
	Remove-Item Env:QTDIR -ErrorAction SilentlyContinue
}

# --- 1. Integrate source/dev ---------------------------------------------------
if (-not $SkipRebase) {
	Write-Step "Integrating source/dev via $Strategy"

	# --untracked-files=no: build outputs and local-only files (.env, CMake user
	# presets) are expected to be present and must not block the integration.
	$status = (& git -C $RepoRoot status --porcelain --untracked-files=no)
	if ($status) {
		throw "Tracked files have uncommitted changes. Commit or stash them first.`n$status"
	}

	$branch = (& git -C $RepoRoot rev-parse --abbrev-ref HEAD).Trim()
	Write-Host "Current branch: $branch"

	Invoke-Native 'git' @('-C', $RepoRoot, 'fetch', 'source') $RepoRoot
	if ($Strategy -eq 'merge') {
		try {
			Invoke-Native 'git' @('-C', $RepoRoot, 'merge', 'source/dev', '--no-edit') $RepoRoot
		} catch {
			throw "Merge failed, likely a conflict. Resolve it, run 'git commit', then re-run with -SkipRebase. To back out: git merge --abort`n$_"
		}
	} else {
		try {
			Invoke-Native 'git' @('-C', $RepoRoot, 'rebase', 'source/dev') $RepoRoot
		} catch {
			throw "Rebase failed, likely a conflict. Resolve it and run 'git rebase --continue', then re-run with -SkipRebase. To back out: git rebase --abort`n$_"
		}
	}
} else {
	Write-Step "Skipping source/dev integration (-SkipRebase)"
}

# --- 1b. Sync submodules -------------------------------------------------------
# Upstream adds submodules periodically (libcbor/libfido2 landed recently).
# Without this, configure dies on e.g. a missing libcbor configuration.h.in.
if (-not $SkipSubmodules) {
	Write-Step "Syncing git submodules"
	Invoke-Native 'git' @('-C', $RepoRoot, 'submodule', 'update', '--init', '--recursive') $RepoRoot
} else {
	Write-Step "Skipping submodule sync (-SkipSubmodules)"
}

# --- 2. Import MSVC environment ------------------------------------------------
Import-VsDevEnv -targetArch $Arch -vcvarsVer $VcVarsVer

# Sanitize PSModulePath: the prepare scripts invoke Windows PowerShell 5.1
# (powershell.exe) for steps like "Expand-Archive". If this script is run from
# PowerShell 7 (pwsh), PSModulePath contains PS7's module dirs, and the child
# 5.1 process tries to load PS7's incompatible Microsoft.PowerShell.Archive,
# failing with "the module could not be loaded". Reset it to only the 5.1
# system module path so child powershell.exe uses its own modules.
$env:PSModulePath = Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\Modules'

# --- 3. (Optional) rebuild Qt 6 + libraries -----------------------------------
if ($Prepare) {
	Write-Step "Rebuilding Qt 6 + third-party libraries (this can take hours)"
	# 'silent' auto-rebuilds stale stages instead of blocking on the interactive
	# "(r)ebuild, (a)ll, (s)kip, (p)rint, (q)uit?" prompt (there is no stdin here).
	$winBat = Join-Path $RepoRoot 'Telegram\build\prepare\win.bat'
	Invoke-Native $winBat @('qt6', 'silent') $BuildPath
} else {
	Write-Step "Skipping library prepare (pass -Prepare to rebuild Qt/libs)"
}

# --- 4. Reconfigure the solution against Qt 6 ---------------------------------
Write-Step "Configuring solution (qt6)"

# Resolve secrets: explicit params win, otherwise fall back to .env / environment.
if (-not $EnvFile) { $EnvFile = Join-Path $RepoRoot '.env' }
Import-DotEnv $EnvFile

if (-not $ApiId)   { $ApiId   = $env:TDESKTOP_API_ID }
if (-not $ApiHash) { $ApiHash = $env:TDESKTOP_API_HASH }

$configureArgs = @($Arch, 'qt6')
if ($ApiId)   { $configureArgs += "-D"; $configureArgs += "TDESKTOP_API_ID=$ApiId" }
if ($ApiHash) { $configureArgs += "-D"; $configureArgs += "TDESKTOP_API_HASH=$ApiHash" }

if (-not $ApiId -or -not $ApiHash) {
	Write-Warning "TDESKTOP_API_ID / TDESKTOP_API_HASH not found. Add them to $EnvFile (KEY=VALUE per line) or pass -ApiId/-ApiHash. configure may fail or use test credentials."
}

$configureBat = Join-Path $RepoRoot 'Telegram\configure.bat'
Invoke-Native $configureBat $configureArgs (Join-Path $RepoRoot 'Telegram')

# --- 5. (Optional) compile a Release exe --------------------------------------
if ($Build) {
	Write-Step "Building Release Telegram.exe"
	# Use --parallel, not a raw '-- -m -v:m' passthrough: PowerShell splits the
	# latter into '-v: m' and MSBuild fails with MSB1016.
	Invoke-Native 'cmake' @('--build', (Join-Path $RepoRoot 'out'), '--config', 'Release', '--target', 'Telegram', '--parallel') $RepoRoot
	Write-Step "Done. Built $RepoRoot\out\Release\Telegram.exe"
} else {
	Write-Step "Done. Open $RepoRoot\out\Telegram.slnx in Visual Studio and build, or re-run with -Build."
}
