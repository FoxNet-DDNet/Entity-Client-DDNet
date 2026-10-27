#include "eclient_updater.h"

#if defined(CONF_ECLIENT_AUTOUPDATE)

#include <base/fs.h>
#include <base/hash.h>
#include <base/io.h>
#include <base/log.h>
#include <base/process.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/version.h>

#include <array>
#include <cctype>
#include <cstdlib>
#include <iterator>

namespace
{
	constexpr const char *RELEASE_URL = "https://api.github.com/repos/FoxNet-DDNet/Entity-Client-DDNet/releases/latest";
	constexpr const char *ASSET_URL_PREFIX = "https://github.com/FoxNet-DDNet/Entity-Client-DDNet/releases/download/";
	constexpr const char *ERROR_FILE = "eclient-update/error.txt";

#if defined(CONF_FAMILY_WINDOWS)
	constexpr const char *ASSET_NAME = "E-Client-windows.zip";
	constexpr const char *ARCHIVE_FILE = "eclient-update/package.zip";
	constexpr const char *INSTALLER_FILE = "eclient-update/install.ps1";
	constexpr const char *INSTALLER_SCRIPT = R"PS1(param(
    [Parameter(Mandatory=$true)][string]$Archive,
    [Parameter(Mandatory=$true)][string]$Install,
    [Parameter(Mandatory=$true)][int]$ParentPid,
    [Parameter(Mandatory=$true)][string]$ErrorFile
)
$ErrorActionPreference = 'Stop'
$stage = Join-Path ([IO.Path]::GetTempPath()) ('EClientUpdate-' + [guid]::NewGuid().ToString('N'))
try {
    Wait-Process -Id $ParentPid -ErrorAction SilentlyContinue
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($Archive, $stage)
    $package = Get-ChildItem -LiteralPath $stage -Directory | Where-Object { $_.Name -like 'DDNet-*' } | Select-Object -First 1
    if (!$package -or !(Test-Path -LiteralPath (Join-Path $package.FullName 'DDNet.exe'))) { throw 'Invalid E-Client release package' }
    Get-ChildItem -LiteralPath $package.FullName -Force | Where-Object { $_.Name -ne 'storage.cfg' } | Copy-Item -Destination $Install -Recurse -Force
    Remove-Item -LiteralPath $ErrorFile -ErrorAction SilentlyContinue
    Start-Process -FilePath (Join-Path $Install 'DDNet.exe')
} catch {
    $_ | Out-String | Set-Content -LiteralPath $ErrorFile
    Start-Process -FilePath (Join-Path $Install 'DDNet.exe') -ErrorAction SilentlyContinue
} finally {
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
}
)PS1";
#elif defined(CONF_PLATFORM_MACOS)
	constexpr const char *ASSET_NAME = "E-Client-macOS.dmg";
	constexpr const char *ARCHIVE_FILE = "eclient-update/package.dmg";
	constexpr const char *INSTALLER_FILE = "eclient-update/install.sh";
	constexpr const char *INSTALLER_SCRIPT = R"SH(#!/bin/sh
archive=$1
app=$2
parent=$3
error_file=$4
while kill -0 "$parent" 2>/dev/null; do sleep 1; done
mount_dir=$(mktemp -d) || exit 1
backup="${app}.update-backup.$$"
fail() { echo "$1" > "$error_file"; [ -d "$app" ] && open "$app"; exit 1; }
cleanup() { hdiutil detach "$mount_dir" -quiet >/dev/null 2>&1 || true; rmdir "$mount_dir" 2>/dev/null || true; }
trap cleanup EXIT
hdiutil attach -readonly -nobrowse -quiet -mountpoint "$mount_dir" "$archive" || fail 'Could not mount the update DMG'
[ -x "$mount_dir/DDNet.app/Contents/MacOS/DDNet" ] || fail 'Invalid E-Client release package'
mv "$app" "$backup" || fail 'Could not move the installed app'
if ! ditto "$mount_dir/DDNet.app" "$app" || [ ! -x "$app/Contents/MacOS/DDNet" ]; then
    rm -rf "$app"
    mv "$backup" "$app"
    fail 'Could not install the new app'
fi
rm -rf "$backup"
rm -f "$error_file"
open "$app"
)SH";
#elif defined(CONF_PLATFORM_LINUX)
	constexpr const char *ASSET_NAME = "E-Client-ubuntu.tar.xz";
	constexpr const char *ARCHIVE_FILE = "eclient-update/package.tar.xz";
	constexpr const char *INSTALLER_FILE = "eclient-update/install.sh";
	constexpr const char *INSTALLER_SCRIPT = R"SH(#!/bin/sh
archive=$1
install=$2
parent=$3
error_file=$4
while kill -0 "$parent" 2>/dev/null; do sleep 1; done
stage=$(mktemp -d) || exit 1
trap 'rm -rf "$stage"' EXIT
fail() { echo "$1" > "$error_file"; if [ -x "$install/DDNet" ]; then "$install/DDNet" >/dev/null 2>&1 & fi; exit 1; }
tar -xJf "$archive" -C "$stage" || fail 'Could not extract the update'
package=$(find "$stage" -mindepth 1 -maxdepth 1 -type d -name 'DDNet-*' -print -quit)
[ -n "$package" ] && [ -x "$package/DDNet" ] || fail 'Invalid E-Client release package'
for item in "$package"/* "$package"/.[!.]* "$package"/..?*; do
    [ -e "$item" ] || continue
    [ "${item##*/}" = storage.cfg ] && continue
    cp -a "$item" "$install/" || fail 'Could not install the update'
done
rm -f "$error_file"
"$install/DDNet" >/dev/null 2>&1 &
)SH";
#else
#error E-Client updater is only supported on Windows, macOS and Linux
#endif

	bool ParseVersion(const char *pText, std::array<int, 3> &Version)
	{
		if(*pText == 'v' || *pText == 'V')
			++pText;
		for(size_t Part = 0; Part < Version.size(); ++Part)
		{
			if(!std::isdigit(static_cast<unsigned char>(*pText)))
				return false;
			int Value = 0;
			while(std::isdigit(static_cast<unsigned char>(*pText)))
			{
				Value = Value * 10 + (*pText++ - '0');
				if(Value > 1000000)
					return false;
			}
			Version[Part] = Value;
			if(*pText == '\0')
				return true;
			if(*pText++ != '.')
				return false;
		}
		return *pText == '\0';
	}

	bool WriteInstaller(IStorage *pStorage, char *pPath, unsigned PathSize)
	{
		IOHANDLE File = pStorage->OpenFile(INSTALLER_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE, pPath, PathSize);
		if(!File)
			return false;
		const unsigned Length = str_length(INSTALLER_SCRIPT);
		const bool Success = io_write(File, INSTALLER_SCRIPT, Length) == Length;
		io_close(File);
		return Success;
	}

	bool IsReleaseInstallation(IStorage *pStorage)
	{
		char aPath[IO_MAX_PATH_LENGTH];
#if defined(CONF_PLATFORM_MACOS)
		pStorage->GetBinaryPathAbsolute("DDNet", aPath, sizeof(aPath));
		return fs_parent_dir(aPath) == 0 && fs_parent_dir(aPath) == 0 && fs_parent_dir(aPath) == 0 && str_endswith(aPath, ".app");
#else
		pStorage->GetBinaryPathAbsolute("storage.cfg", aPath, sizeof(aPath));
		return fs_is_file(aPath) != 0;
#endif
	}
} // namespace

void CEClientUpdater::Init()
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pHttp = Kernel()->RequestInterface<IHttp>();
	char *pError = m_pStorage->ReadFileStr(ERROR_FILE, IStorage::TYPE_SAVE);
	if(pError)
	{
		log_error("eclient_updater", "Previous update failed: %s", pError);
		free(pError);
		m_pStorage->RemoveFile(ERROR_FILE, IStorage::TYPE_SAVE);
	}
	if(IsReleaseInstallation(m_pStorage))
		Check();
}

void CEClientUpdater::Check()
{
	if(m_State == EState::DOWNLOADING || m_State == EState::READY || m_State == EState::CHECKING)
		return;
	m_AssetUrl.clear();
	m_AssetDigest.clear();
	m_aLatestVersion[0] = '\0';
	m_pRequest = HttpGet(RELEASE_URL);
	m_pRequest->HeaderString("Accept", "application/vnd.github+json");
	m_pRequest->HeaderString("X-GitHub-Api-Version", "2022-11-28");
	m_pRequest->MaxResponseSize(1024 * 1024);
	m_pRequest->Timeout(CTimeout{5000, 15000, 500, 5});
	m_pRequest->SkipByFileTime(false);
	m_State = EState::CHECKING;
	m_pHttp->Run(m_pRequest);
}

void CEClientUpdater::FinishCheck()
{
	if(m_pRequest->State() != EHttpState::DONE)
	{
		log_error("eclient_updater", "Could not check GitHub Releases");
		m_State = EState::FAILED;
		return;
	}
	json_value *pJson = m_pRequest->ResultJson();
	if(!pJson || pJson->type != json_object)
	{
		log_error("eclient_updater", "Invalid GitHub release response");
		json_value_free(pJson);
		m_State = EState::FAILED;
		return;
	}
	const char *pTag = json_string_get(json_object_get(pJson, "tag_name"));
	std::array<int, 3> CurrentVersion{}, LatestVersion{};
	if(!pTag || !ParseVersion(pTag, LatestVersion) || !ParseVersion(ECLIENT_VERSION, CurrentVersion))
	{
		log_error("eclient_updater", "Release tag or client version is not numeric");
		json_value_free(pJson);
		m_State = EState::FAILED;
		return;
	}
	if(LatestVersion <= CurrentVersion)
	{
		json_value_free(pJson);
		m_State = EState::IDLE;
		return;
	}
	const json_value *pAssets = json_object_get(pJson, "assets");
	if(pAssets && pAssets->type == json_array)
	{
		for(int Index = 0; Index < json_array_length(pAssets); ++Index)
		{
			const json_value *pAsset = json_array_get(pAssets, Index);
			if(!pAsset || pAsset->type != json_object)
				continue;
			const char *pName = json_string_get(json_object_get(pAsset, "name"));
			if(!pName || str_comp(pName, ASSET_NAME) != 0)
				continue;
			const char *pUrl = json_string_get(json_object_get(pAsset, "browser_download_url"));
			const char *pDigest = json_string_get(json_object_get(pAsset, "digest"));
			if(pUrl && str_startswith(pUrl, ASSET_URL_PREFIX) && pDigest && str_startswith(pDigest, "sha256:") && str_length(pDigest + 7) == 64)
			{
				SHA256_DIGEST ParsedDigest;
				if(sha256_from_str(&ParsedDigest, pDigest + 7) == 0)
				{
					m_AssetUrl = pUrl;
					m_AssetDigest = pDigest + 7;
					str_copy(m_aLatestVersion, pTag);
				}
			}
			break;
		}
	}
	json_value_free(pJson);
	if(m_AssetUrl.empty())
	{
		log_error("eclient_updater", "Latest release has no verified %s asset", ASSET_NAME);
		m_State = EState::FAILED;
	}
	else
		m_State = EState::AVAILABLE;
}

void CEClientUpdater::Download()
{
	if(m_State != EState::AVAILABLE)
		return;
	SHA256_DIGEST Digest;
	if(sha256_from_str(&Digest, m_AssetDigest.c_str()) != 0)
	{
		m_State = EState::FAILED;
		return;
	}
	m_pRequest = HttpGetFile(m_AssetUrl.c_str(), m_pStorage, ARCHIVE_FILE, IStorage::TYPE_SAVE);
	m_pRequest->ExpectSha256(Digest);
	m_pRequest->SkipByFileTime(false);
	m_Progress = 0;
	m_State = EState::DOWNLOADING;
	m_pHttp->Run(m_pRequest);
}

void CEClientUpdater::FinishDownload()
{
	if(m_pRequest->State() != EHttpState::DONE)
	{
		log_error("eclient_updater", "Could not download or verify %s", ASSET_NAME);
		m_State = EState::FAILED;
		return;
	}
	m_State = EState::READY;
	if(m_pClient->State() != IClient::STATE_ONLINE && !m_pClient->EditorHasUnsavedData())
		m_pClient->Restart();
}

void CEClientUpdater::Update()
{
	if(m_State == EState::CHECKING && m_pRequest && m_pRequest->Done())
		FinishCheck();
	else if(m_State == EState::DOWNLOADING && m_pRequest)
	{
		m_Progress = m_pRequest->Progress();
		if(m_pRequest->Done())
			FinishDownload();
	}
}

bool CEClientUpdater::LaunchInstaller()
{
	if(m_State != EState::READY)
		return false;
	char aScript[IO_MAX_PATH_LENGTH];
	if(!WriteInstaller(m_pStorage, aScript, sizeof(aScript)))
	{
		log_error("eclient_updater", "Could not write the update installer");
		return false;
	}
	char aArchive[IO_MAX_PATH_LENGTH];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, ARCHIVE_FILE, aArchive, sizeof(aArchive));
	char aError[IO_MAX_PATH_LENGTH];
	m_pStorage->GetCompletePath(IStorage::TYPE_SAVE, ERROR_FILE, aError, sizeof(aError));
	char aExecutable[IO_MAX_PATH_LENGTH];
	m_pStorage->GetBinaryPathAbsolute(
#if defined(CONF_FAMILY_WINDOWS)
		"DDNet.exe",
#else
		"DDNet",
#endif
		aExecutable, sizeof(aExecutable));
	char aInstall[IO_MAX_PATH_LENGTH];
	str_copy(aInstall, aExecutable);
	if(fs_parent_dir(aInstall) != 0)
		return false;
#if defined(CONF_PLATFORM_MACOS)
	if(fs_parent_dir(aInstall) != 0 || fs_parent_dir(aInstall) != 0 || !str_endswith(aInstall, ".app"))
		return false;
#endif
	char aPid[32];
	str_format(aPid, sizeof(aPid), "%d", process_id());
#if defined(CONF_FAMILY_WINDOWS)
	const char *apArguments[] = {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", aScript, aArchive, aInstall, aPid, aError};
	const PROCESS Process = process_execute("powershell.exe", EShellExecuteWindowState::BACKGROUND, apArguments, std::size(apArguments));
#else
	const char *apArguments[] = {aScript, aArchive, aInstall, aPid, aError};
	const PROCESS Process = process_execute("/bin/sh", EShellExecuteWindowState::BACKGROUND, apArguments, std::size(apArguments));
#endif
	if(Process == INVALID_PROCESS)
	{
		log_error("eclient_updater", "Could not launch the update installer");
		return false;
	}
	return true;
}

#endif // CONF_ECLIENT_AUTOUPDATE
