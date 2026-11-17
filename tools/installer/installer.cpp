// ---------------------------------------------------------------------------
// Tiny per-user installer for blur.
//
// The release zip is appended to this exe (stub + zip + footer). On run it:
//   1. asks where to install, with a real folder picker (defaults to
//      %LOCALAPPDATA%\Programs\blur, the user may choose any folder)
//   2. asks whether to create a desktop shortcut (on by default)
//   3. writes the appended zip to %TEMP%
//   4. extracts it with the bsdtar that ships with Windows 10/11 (no admin)
//   5. creates the start menu shortcut (+ desktop if wanted)
//   6. registers an entry in "Apps & features" (HKCU uninstall key)
//   7. starts blur-gui.exe
//
// No external runtime: built with /MT, only kernel32/user32/shell32/ole32/
// advapi32/comctl32 imports.
// ---------------------------------------------------------------------------
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <cstdio>
#include <string>
#include <vector>


namespace {
	constexpr char FOOTER_MAGIC[8] = { 'B', 'L', 'U', 'R', 'Z', 'I', 'P', '1' };
	constexpr unsigned long long FOOTER_SIZE = 16; // 8 byte offset + 8 byte magic

	const wchar_t* const APP_TITLE = L"blur 安装程序";
	const wchar_t* const APP_DISPLAY_NAME = L"blur 2.45 多语言版";
	const wchar_t* const APP_VERSION = L"2.45.0";
	const wchar_t* const UNINSTALL_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\blur";

	std::wstring get_module_path() {
		wchar_t buffer[MAX_PATH * 4] = {};
		GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
		return buffer;
	}

	std::wstring get_env(const wchar_t* name) {
		wchar_t buffer[MAX_PATH * 4] = {};
		GetEnvironmentVariableW(name, buffer, static_cast<DWORD>(std::size(buffer)));
		return buffer;
	}

	// ascii view of a path (the install location lives under %LOCALAPPDATA% and
	// only ever contains ascii, the uninstall script must stay ascii as well)
	std::string narrow(const std::wstring& wide) {
		std::string out;
		out.reserve(wide.size());
		for (wchar_t wc : wide)
			out.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');
		return out;
	}

	std::wstring default_install_dir() {
		return get_env(L"LOCALAPPDATA") + L"\\Programs\\blur";
	}

	// "/D=<path>" for unattended installs, like most windows installers
	std::wstring silent_install_dir() {
		int argc = 0;
		wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv == nullptr)
			return {};

		std::wstring result;
		for (int i = 1; i < argc; ++i) {
			if (_wcsnicmp(argv[i], L"/D=", 3) == 0)
				result = argv[i] + 3;
		}

		LocalFree(argv);
		return result;
	}

	// native folder picker (IFileDialog, folders only). returns empty when the
	// user cancels.
	std::wstring pick_folder(const std::wstring& initial_dir) {
		IFileOpenDialog* dialog = nullptr;
		if (FAILED(CoCreateInstance(
				CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)
			)))
			return {};

		DWORD options = 0;
		dialog->GetOptions(&options);
		dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
		dialog->SetTitle(L"选择 blur 的安装位置（会在该文件夹内创建 blur 目录）");
		dialog->SetOkButtonLabel(L"安装到这里");

		IShellItem* folder = nullptr;
		if (SUCCEEDED(SHCreateItemFromParsingName(initial_dir.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
			dialog->SetFolder(folder);
			dialog->SetDefaultFolder(folder);
			folder->Release();
		}

		std::wstring result;
		if (SUCCEEDED(dialog->Show(nullptr))) {
			IShellItem* item = nullptr;
			if (SUCCEEDED(dialog->GetResult(&item))) {
				PWSTR path = nullptr;
				if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
					result = path;
					CoTaskMemFree(path);
				}
				item->Release();
			}
		}

		dialog->Release();
		return result;
	}

	// the user picks the parent folder, blur goes into <parent>\blur
	std::wstring install_dir_from_parent(std::wstring parent) {
		if (parent.empty())
			return default_install_dir();

		while (!parent.empty() && (parent.back() == L'\\' || parent.back() == L'/'))
			parent.pop_back();

		const std::wstring lower = parent;
		const bool already_blur =
			lower.size() >= 5 && _wcsicmp(lower.c_str() + (lower.size() - 4), L"blur") == 0;

		return already_blur ? parent : parent + L"\\blur";
	}
	bool read_footer(const std::wstring& exe_path, unsigned long long& offset, unsigned long long& size) {
		HANDLE file = CreateFileW(exe_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (file == INVALID_HANDLE_VALUE)
			return false;

		LARGE_INTEGER file_size{};
		GetFileSizeEx(file, &file_size);

		if (file_size.QuadPart < static_cast<LONGLONG>(FOOTER_SIZE)) {
			CloseHandle(file);
			return false;
		}

		LARGE_INTEGER footer_pos{};
		footer_pos.QuadPart = file_size.QuadPart - static_cast<LONGLONG>(FOOTER_SIZE);
		SetFilePointerEx(file, footer_pos, nullptr, FILE_BEGIN);

		unsigned char footer[FOOTER_SIZE] = {};
		DWORD read = 0;
		const bool ok = ReadFile(file, footer, sizeof(footer), &read, nullptr) && read == sizeof(footer);
		CloseHandle(file);

		if (!ok || memcmp(footer + 8, FOOTER_MAGIC, 8) != 0)
			return false;

		memcpy(&offset, footer, 8);
		size = static_cast<unsigned long long>(file_size.QuadPart) - FOOTER_SIZE - offset;
		return true;
	}

	bool write_payload(
		const std::wstring& exe_path, unsigned long long offset, unsigned long long size, const std::wstring& target
	) {
		HANDLE src = CreateFileW(exe_path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (src == INVALID_HANDLE_VALUE)
			return false;

		HANDLE dst = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		if (dst == INVALID_HANDLE_VALUE) {
			CloseHandle(src);
			return false;
		}

		LARGE_INTEGER pos{};
		pos.QuadPart = static_cast<LONGLONG>(offset);
		SetFilePointerEx(src, pos, nullptr, FILE_BEGIN);

		std::vector<char> buffer(1 << 20);
		unsigned long long remaining = size;
		bool ok = true;

		while (remaining > 0) {
			DWORD to_read = static_cast<DWORD>(remaining < buffer.size() ? remaining : buffer.size());
			DWORD read = 0;

			if (!ReadFile(src, buffer.data(), to_read, &read, nullptr) || read == 0) {
				ok = false;
				break;
			}

			DWORD written = 0;
			if (!WriteFile(dst, buffer.data(), read, &written, nullptr) || written != read) {
				ok = false;
				break;
			}

			remaining -= read;
		}

		CloseHandle(src);
		CloseHandle(dst);
		return ok;
	}

	bool run_and_wait(const std::wstring& command_line) {
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};

		std::vector<wchar_t> cmd(command_line.begin(), command_line.end());
		cmd.push_back(L'\0');

		if (!CreateProcessW(
				nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process
			))
			return false;

		WaitForSingleObject(process.hProcess, INFINITE);

		DWORD exit_code = 1;
		GetExitCodeProcess(process.hProcess, &exit_code);
		CloseHandle(process.hProcess);
		CloseHandle(process.hThread);

		return exit_code == 0;
	}

	void create_shortcut(const std::wstring& link_path, const std::wstring& target, const std::wstring& workdir) {
		const std::wstring command = L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"$ws = New-Object "
		                              L"-ComObject WScript.Shell; $sc = $ws.CreateShortcut('" + link_path +
		                              L"'); $sc.TargetPath = '" + target + L"'; $sc.WorkingDirectory = '" + workdir +
		                              L"'; $sc.Description = 'blur - motion blur'; $sc.Save()\"";

		run_and_wait(command);
	}

	unsigned long long directory_size(const std::wstring& dir) {
		WIN32_FIND_DATAW data{};
		HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &data);
		if (find == INVALID_HANDLE_VALUE)
			return 0;

		unsigned long long total = 0;

		do {
			if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
				continue;

			const std::wstring child = dir + L"\\" + data.cFileName;

			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				total += directory_size(child);
			else
				total += (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) + data.nFileSizeLow;
		} while (FindNextFileW(find, &data));

		FindClose(find);
		return total;
	}

	void write_string(HKEY key, const wchar_t* name, const std::wstring& value) {
		RegSetValueExW(
			key,
			name,
			0,
			REG_SZ,
			reinterpret_cast<const BYTE*>(value.c_str()),
			static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))
		);
	}

	void write_dword(HKEY key, const wchar_t* name, DWORD value) {
		RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
	}

	void register_uninstall_entry(const std::wstring& install_dir, const std::wstring& uninstall_path) {
		HKEY key = nullptr;
		if (RegCreateKeyExW(
				HKEY_CURRENT_USER, UNINSTALL_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr
			) != ERROR_SUCCESS)
			return;

		std::wstring wchar_buf[64];

		write_string(key, L"DisplayName", APP_DISPLAY_NAME);
		write_string(key, L"DisplayVersion", APP_VERSION);
		write_string(key, L"Publisher", L"SORA123k（f0e/blur 社区分支）");
		write_string(key, L"InstallLocation", install_dir);
		write_string(key, L"DisplayIcon", install_dir + L"\\blur-gui.exe");
		write_string(key, L"UninstallString", L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + uninstall_path + L"\"");
		write_string(key, L"Comments", L"给视频加运动模糊的开源工具，GPL-3.0");

		const unsigned long long bytes = directory_size(install_dir);
		write_dword(key, L"EstimatedSize", static_cast<DWORD>(bytes / 1024));
		write_dword(key, L"NoModify", 1);
		write_dword(key, L"NoRepair", 1);

		RegCloseKey(key);
	}

	struct InstallOptions {
		bool desktop_shortcut = true;
		bool accepted = false;
		bool quiet = false;
		std::wstring install_dir;
	};

	// Unattended switches so the installer can be scripted:
	//   /S or --quiet              install without any dialog, desktop shortcut on
	//   --no-desktop-shortcut      skip the desktop shortcut
	//   --desktop-shortcut         force the desktop shortcut
	bool has_switch(const wchar_t* name) {
		int argc = 0;
		wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv == nullptr)
			return false;

		bool found = false;
		for (int i = 1; i < argc; ++i) {
			if (_wcsicmp(argv[i], name) == 0) {
				found = true;
				break;
			}
		}

		LocalFree(argv);
		return found;
	}

	InstallOptions ask_options() {
		InstallOptions options;
		options.install_dir = default_install_dir();

		const bool quiet = has_switch(L"/S") || has_switch(L"--quiet") || has_switch(L"/quiet");
		if (quiet) {
			options.quiet = true;
			options.accepted = true;
			options.desktop_shortcut = !has_switch(L"--no-desktop-shortcut");

			const auto custom = silent_install_dir();
			if (!custom.empty())
				options.install_dir = custom;

			return options;
		}

		// let the user choose where it goes; the picker starts at
		// %LOCALAPPDATA%\Programs and the app lands in <chosen>\blur
		const std::wstring default_parent = get_env(L"LOCALAPPDATA") + L"\\Programs";
		const auto picked = pick_folder(default_parent);

		if (picked.empty()) {
			// cancelled: no installation, and no "nothing happened" popup
			return options;
		}

		options.install_dir = install_dir_from_parent(picked);
		options.accepted = true;

		const int shortcut = MessageBoxW(
			nullptr,
			(L"安装到：\n" + options.install_dir + L"\n\n要在桌面创建 blur 的快捷方式吗？\n\n"
			 L"选「是」在桌面放一个图标（推荐），选「否」只创建开始菜单快捷方式。")
				.c_str(),
			APP_TITLE,
			MB_ICONQUESTION | MB_YESNO
		);

		options.desktop_shortcut = shortcut == IDYES;
		return options;
	}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	const std::wstring exe_path = get_module_path();

	unsigned long long offset = 0;
	unsigned long long size = 0;

	if (!read_footer(exe_path, offset, size) || size < 1024) {
		MessageBoxW(
			nullptr, L"这个文件不完整（找不到内嵌的安装包）。\n请重新下载安装程序。", APP_TITLE, MB_ICONERROR | MB_OK
		);
		CoUninitialize();
		return 1;
	}

	const InstallOptions options = ask_options();

	if (!options.accepted) {
		CoUninitialize();
		return 0;
	}

	const std::wstring install_dir = options.install_dir;

	{
	}
	// 1. write the appended zip to temp
	const std::wstring zip_path = get_env(L"TEMP") + L"\\blur-install-payload.zip";

	SetCursor(LoadCursorW(nullptr, IDC_WAIT));


	if (!write_payload(exe_path, offset, size, zip_path)) {
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		MessageBoxW(nullptr, L"写入临时文件失败，请检查磁盘空间。", APP_TITLE, MB_ICONERROR);
		CoUninitialize();
		return 1;
	}

	// 2. extract with the tar.exe that ships with Windows 10 1803+
	// the user may pick a nested folder that does not exist yet, so create the
	// whole path instead of only its last component
	const int created = SHCreateDirectoryExW(nullptr, install_dir.c_str(), nullptr);
	if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) {
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		MessageBoxW(nullptr, (L"无法创建安装目录：\n" + install_dir).c_str(), APP_TITLE, MB_ICONERROR);
		DeleteFileW(zip_path.c_str());
		CoUninitialize();
		return 1;
	}

	if (!run_and_wait(L"tar.exe -xf \"" + zip_path + L"\" -C \"" + install_dir + L"\"")) {
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
		MessageBoxW(
			nullptr,
			L"解压失败。\n\n如果系统里没有 tar.exe（Windows 10 1803 以前），请改用便携版 zip，"
			L"解压后直接运行 blur-gui.exe。",
			APP_TITLE,
			MB_ICONERROR
		);
		DeleteFileW(zip_path.c_str());
		CoUninitialize();
		return 1;
	}

	DeleteFileW(zip_path.c_str());

	// 3. uninstaller: powershell, so a non-ascii install path works too
	const std::wstring uninstall_path = install_dir + L"\\uninstall.ps1";
	{
		const std::wstring script =
			L"$ErrorActionPreference = 'SilentlyContinue'\r\n"
			L"$dir = '" + install_dir + L"'\r\n"
			L"Write-Host ''\r\n"
			L"Write-Host '  Uninstalling blur...' -ForegroundColor Cyan\r\n"
			L"# close a running copy first: its python / vapoursynth dlls lock files\r\n"
			L"Get-Process -Name 'blur-gui' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue\r\n"
			L"Start-Sleep -Milliseconds 800\r\n"
			L"foreach ($f in @([Environment]::GetFolderPath('Desktop'), [Environment]::GetFolderPath('Programs'))) {\r\n"
			L"    Remove-Item -Force -LiteralPath (Join-Path $f 'blur.lnk')\r\n"
			L"}\r\n"
			L"Remove-Item -Force -Recurse -LiteralPath 'HKCU:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\blur'\r\n"
			L"Set-Location $env:TEMP\r\n"
			L"for ($i = 0; $i -lt 12; $i++) {\r\n"
			L"    Remove-Item -Force -Recurse -LiteralPath $dir\r\n"
			L"    if (-not (Test-Path -LiteralPath $dir)) { break }\r\n"
			L"    Start-Sleep -Milliseconds 500\r\n"
			L"}\r\n"
			L"if (Test-Path -LiteralPath $dir) {\r\n"
			L"    Write-Host '  Some files were still in use, please delete the folder manually.' -ForegroundColor Yellow\r\n"
			L"}\r\n"
			L"Write-Host '  Done.' -ForegroundColor Green\r\n"
			L"Start-Sleep -Seconds 2\r\n";

		HANDLE file = CreateFileW(uninstall_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		if (file != INVALID_HANDLE_VALUE) {
			// powershell needs utf-8 (with bom) or utf-16; utf-8 keeps the file small
			// and still handles a non-ascii install path
			const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
			DWORD written = 0;
			WriteFile(file, bom, sizeof(bom), &written, nullptr);

			const int size_needed = WideCharToMultiByte(
				CP_UTF8, 0, script.data(), static_cast<int>(script.size()), nullptr, 0, nullptr, nullptr
			);
			std::string utf8(static_cast<size_t>(size_needed), '\0');
			WideCharToMultiByte(
				CP_UTF8,
				0,
				script.data(),
				static_cast<int>(script.size()),
				utf8.data(),
				size_needed,
				nullptr,
				nullptr
			);
			WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
			CloseHandle(file);
		}
	}

	// 4. shortcuts
	wchar_t desktop[MAX_PATH * 2] = {};
	wchar_t programs[MAX_PATH * 2] = {};
	SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop);
	SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, programs);

	const std::wstring target = install_dir + L"\\blur-gui.exe";

	if (options.desktop_shortcut)
		create_shortcut(std::wstring(desktop) + L"\\blur.lnk", target, install_dir);

	create_shortcut(std::wstring(programs) + L"\\blur.lnk", target, install_dir);

	// 5. "Apps & features" entry
	register_uninstall_entry(install_dir, uninstall_path);

	SetCursor(LoadCursorW(nullptr, IDC_ARROW));

	// 6. launch
	ShellExecuteW(nullptr, L"open", target.c_str(), nullptr, install_dir.c_str(), SW_SHOWNORMAL);

	if (!options.quiet) {
		const std::wstring done =
			std::wstring(L"安装完成，blur 已经启动。\n\n位置：" + install_dir + L"\n\n") +
			(options.desktop_shortcut ? L"已在桌面和开始菜单创建快捷方式。\n"
			                          : L"已在开始菜单创建快捷方式（按你的选择没有创建桌面快捷方式）。\n") +
			L"可以在「设置 → 应用 → 已安装的应用」里卸载。";

		MessageBoxW(nullptr, done.c_str(), APP_TITLE, MB_ICONINFORMATION);
	}

	CoUninitialize();
	return 0;
}