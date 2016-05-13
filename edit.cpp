// Copyright (C) 2016 Michael Trenholm-Boyle. See the LICENSE file for more.
#include "stdafx.h"
#include "unique_handle.h"
#include "json/json.h"

static bool find_atom_dir(std::wstring * atom_dir);
static bool pipe_stdin_to_temp_file(std::wstring * temp_file);
static bool delete_atom_application_initial_paths();
static int launch_atom(const wchar_t * atom_dir, const wchar_t * arg, bool wait = false);
static int update_atom(const wchar_t * atom_dir, bool wait = false);
static bool program_name_implies_wait();
static bool wcsiendswith(const wchar_t * str, const wchar_t * suffix);

int wmain(int argc, wchar_t ** argv) {
    std::wstring atom_dir;
    if(!find_atom_dir(&atom_dir)) {
        MessageBox(NULL, L"Atom not installed", L"edit", MB_OK|MB_ICONSTOP);
        return 1;
    }
    delete_atom_application_initial_paths();
    int n = 0;
    bool tried = false;
	bool wait = program_name_implies_wait();
	SetEnvironmentVariable(L"ELECTRON_NO_ATTACH_CONSOLE", L"YES");
    for (int i = 1; i < argc; ++i) {
		if (wcscmp(argv[i], L"--wait") == 0) {
			wait = true;
		} else {
	        tried = true;
			n = std::max(n, launch_atom(atom_dir.c_str(), argv[i], wait));
		}
    }
	if (!tried) {
		std::wstring temp_file;
		if (pipe_stdin_to_temp_file(&temp_file)) {
			tried = true;
			n = std::max(n, launch_atom(atom_dir.c_str(), temp_file.c_str(), wait));
		}
	}
    if(!tried) {
        n = std::max(n, update_atom(atom_dir.c_str(), wait));
    }
    return n;
}

#ifndef PATH_MAX
#define PATH_MAX (32768-8)
#endif

template<typename T>
struct free_delete {
    inline void operator()(T * p) const {
        if (p) {
            free(p);
        }
    }
};

struct findclose_delete : public std::handle_delete<HANDLE> {
public:
	void operator()(HANDLE h) const {
		FindClose(h);
	}
};

static bool pipe_stdin_to_temp_file(std::wstring * temp_file) {
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	if(!std::handle_traits<HANDLE>().is_valid_value(h)) {
		return false;
	}
    const size_t BUFLEN = 4096;
    std::unique_ptr<wchar_t, free_delete<wchar_t> > tmp(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
    if (!GetTempPath(PATH_MAX + 1, tmp.get())) {
        return false;
    }
    std::unique_ptr<wchar_t, free_delete<wchar_t> > tmpfile(reinterpret_cast<wchar_t* >(calloc(PATH_MAX + 1, 2)));
    if (!GetTempFileName(tmp.get(), L"TMP", 0, tmpfile.get())) {
        return false;
    }
    size_t len = 0;
    {
        std::unique_handle<HANDLE> f(CreateFile(tmpfile.get(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
        if (!f) {
            return false;
        }
        std::unique_ptr<char, free_delete<char> > cbuf(reinterpret_cast<char *>(calloc(BUFLEN, 1)));
		bool first = true;
        while (true) {
            DWORD r = 0;
			if (first) {
				PeekNamedPipe(h, cbuf.get(), static_cast<DWORD>(BUFLEN), &r, nullptr, nullptr);
				if (!r) {
					break;
				}
				first = false;
			}
            BOOL b = ReadFile(h, cbuf.get(), static_cast<DWORD>(BUFLEN), &r, nullptr);
            if (!b || !r) {
                break;
            }
            DWORD w = 0;
            b = WriteFile(f.get(), cbuf.get(), r, &w, nullptr);
            if (!b || (r != w)) {
                break;
            }
            len += w;
        }
    }
    if (!len) {
        DeleteFile(tmpfile.get());
        return false;
    }
    *temp_file = std::wstring(tmpfile.get());
    return true;
}

static bool find_atom_dir(std::wstring * atom_dir) {
    std::unique_ptr<wchar_t, free_delete<wchar_t> > dir(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
    if (!GetEnvironmentVariable(L"LOCALAPPDATA", dir.get(), PATH_MAX - 80)) {
        return false;
    }
    wcscat_s(dir.get(), PATH_MAX, L"\\atom");
    WIN32_FIND_DATA fd = { 0 };
    std::unique_handle<HANDLE, findclose_delete> hf;
    {
        std::unique_ptr<wchar_t, free_delete<wchar_t> > pat(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
        wcscpy_s(pat.get(), PATH_MAX, dir.get());
        wcscat_s(pat.get(), PATH_MAX, L"\\app-*");
        hf.reset(FindFirstFile(pat.get(), &fd));
    }
    if (!hf) {
        return false;
    }
    int major = -1, minor = -1, build = -1, patch = -1;
    while (true) {
        if (FILE_ATTRIBUTE_DIRECTORY == (FILE_ATTRIBUTE_DIRECTORY & fd.dwFileAttributes)) {
            int m = -1, n = -1, b = -1, p = -1;
            if (4 != swscanf_s(fd.cFileName, L"app-%d.%d.%d.%d", &m, &n, &b, &p)) {
                p = -1;
                if (3 != swscanf_s(fd.cFileName, L"app-%d.%d.%d", &m, &n, &b)) {
                    b = -1;
                    if (2 != swscanf_s(fd.cFileName, L"app-%d.%d", &m, &n)) {
                        n = -1;
                        if (1 != swscanf_s(fd.cFileName, L"app-%d", &m)) {
                            m = -1;
                        }
                    }
                }
            }
            if (m >= 0) {
                if (m > major) {
                    major = m;
                    minor = n;
                    build = b;
                    patch = p;
                } else if (m == major) {
                    if (n > minor) {
                        minor = n;
                        build = b;
                        patch = p;
                    } else if (n == minor) {
                        if (b > build) {
                            build = b;
                            patch = p;
                        } else if (b == build) {
                            if (p > patch) {
                                patch = p;
                            }
                        }
                    }
                }
            }
        }
        if (!FindNextFile(hf.get(), &fd)) {
            break;
        }
    }
    if (major < 0) {
        return false;
    }
    wchar_t buf[50] = { 0 };
    const wchar_t * pat = L"\\app-%d";
    if (minor >= 0) {
        pat = L"\\app-%d.%d";
        if (build >= 0) {
            pat = L"\\app-%d.%d.%d";
            if (patch >= 0) {
                pat = L"\\app-%d.%d.%d.%d";
            }
        }
    }
    swprintf_s(buf, pat, major, minor, build, patch);
    wcscat_s(dir.get(), PATH_MAX, buf);
    *atom_dir = std::wstring(dir.get());
    return true;
}

static bool delete_atom_application_initial_paths() {
    std::unique_ptr<wchar_t, free_delete<wchar_t> > path;
    path.reset(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
    if (!GetEnvironmentVariable(L"USERPROFILE", path.get(), PATH_MAX)) {
        return false;
    }
    wcscat_s(path.get(), PATH_MAX, L"\\.atom\\storage\\application.json");
    if(GetFileAttributes(path.get()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    Json::Value application;
    {
        std::ifstream fin(path.get(), std::ios::binary);
        if (!fin.is_open()) {
            return false;
        }
        fin >> application;
    }
    if (application.type() != Json::arrayValue || application.size() < 1 || application[0].type() != Json::objectValue || !application[0].isMember("initialPaths") || application[0]["initialPaths"].type() != Json::arrayValue) {
        return false;
    }
    application[0]["initialPaths"].clear();
    {
        std::ofstream fout(path.get(), std::ios::binary | std::ios::trunc);
        if (!fout.is_open()) {
            return false;
        }
        fout << application;
    }
    return true;
}

static int launch_atom(const wchar_t * atom_dir, const wchar_t * arg, bool wait) {
	std::unique_ptr<wchar_t, free_delete<wchar_t> > atom_exe, cli;
    atom_exe.reset(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
    wcscpy_s(atom_exe.get(), PATH_MAX, atom_dir);
    wcscat_s(atom_exe.get(), PATH_MAX, L"\\atom.exe");
    size_t len = wcslen(atom_exe.get()) + wcslen(arg ? arg : L"") + 9;
    cli.reset(reinterpret_cast<wchar_t *>(calloc(len + 1, 2)));
    swprintf_s(cli.get(), len, (arg && *arg) ? L"\"%s\" \"%s\"" : L"\"%s\"", atom_exe.get(), arg);
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
	si.dwFlags = STARTF_FORCEONFEEDBACK;
	PROCESS_INFORMATION pi = { 0 };
    if(!CreateProcess(atom_exe.get(), cli.get(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, L".", &si, &pi)) {
        return 2;
    }
	if (wait) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	} else {
		WaitForInputIdle(pi.hProcess, 7000);
	}
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

static int update_atom(const wchar_t * atom_dir, bool wait) {
    std::unique_ptr<wchar_t, free_delete<wchar_t> > update_exe, cli;
    update_exe.reset(reinterpret_cast<wchar_t *>(calloc(PATH_MAX + 1, 2)));
    if(!GetEnvironmentVariable(L"LOCALAPPDATA", update_exe.get(), PATH_MAX - 16)) {
        return 1;
    }
    wcscat_s(update_exe.get(), PATH_MAX, L"\\atom\\Update.exe");
	static const wchar_t * url = L"https://github.com/atom/atom/releases";
    size_t len = wcslen(update_exe.get()) + 14 + wcslen(url);
    cli.reset(reinterpret_cast<wchar_t *>(calloc(len + 1, 2)));
    swprintf_s(cli.get(), len, L"\"%s\" --update=%s", update_exe.get(), url);
    STARTUPINFO si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_FORCEONFEEDBACK;
    PROCESS_INFORMATION pi = { 0 };
    if(!CreateProcess(update_exe.get(), cli.get(), nullptr, nullptr, FALSE, wait ? CREATE_NO_WINDOW : DETACHED_PROCESS, nullptr, atom_dir, &si, &pi)) {
        return 2;
    }
    CloseHandle(pi.hThread);
	if (wait) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	} else {
		WaitForInputIdle(pi.hProcess, 7000);
	}
    CloseHandle(pi.hProcess);
    return launch_atom(atom_dir, nullptr, wait);
}

static bool program_name_implies_wait() {
	wchar_t buf[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, buf, sizeof(buf) / sizeof(buf[0]) - 1);
	return (_wcsicmp(buf, L"editor.exe") == 0) || wcsiendswith(buf, L"\\editor.exe");
}


static bool wcsiendswith(const wchar_t * string, const wchar_t * suffix) {
	size_t stringlen = wcslen(string);
	size_t suffixlen = wcslen(suffix);
	return stringlen >= suffixlen && _wcsicmp(string + stringlen - suffixlen, suffix) == 0;
}