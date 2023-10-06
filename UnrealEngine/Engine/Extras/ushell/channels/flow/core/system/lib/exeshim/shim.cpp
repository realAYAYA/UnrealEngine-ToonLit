// Copyright Epic Games, Inc. All Rights Reserved.

#include <cinttypes>
#include <Windows.h>
#include <winternl.h>

// {{{1 misc -------------------------------------------------------------------
static void close_handle(void*)
{
    /* do nothing - let the kernel clean up after us... */
}

[[noreturn]] static void quit(int exit_code)
{
    ExitProcess(exit_code);
}

static void spam(const char* format, ...)
{
    HANDLE conout = CreateFileW(L"CONOUT$", GENERIC_WRITE|GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    static char buffer[128];

    va_list args;
    va_start(args, format);
    int count = wvsprintfA(buffer, format, args);
    va_end(args);

    DWORD written;
    WriteFile(conout, buffer, count, &written, nullptr);

    CloseHandle(conout);
};

#if 0
#   define SPAM(format, ...) spam(format "\n", ##__VA_ARGS__)
#else
#   define SPAM(format, ...)
#endif

// {{{1 constants --------------------------------------------------------------
static const uint16_t LENGTH_ID = 0x493;
static const uint16_t DATA_ID   = 493;

// {{{1 vt100 ------------------------------------------------------------------
class Vt100Scope
{
public:
            Vt100Scope();
            ~Vt100Scope();

private:
    DWORD   _con_mode;
    HANDLE  _stdout_handle;
};

//------------------------------------------------------------------------------
Vt100Scope::Vt100Scope()
{
    // Enable Windows 10's VT100/ECMA-48 support
    _con_mode = 0;
    HANDLE handle = CreateFileW(L"CONOUT$", GENERIC_WRITE|GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (intptr_t(handle) <= 0)
        return;

    if (GetFileType(handle) != FILE_TYPE_CHAR)
        return;

    if (!GetConsoleMode(handle, &_con_mode))
        return;

    _con_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, _con_mode);

    _stdout_handle = handle;
}

//------------------------------------------------------------------------------
Vt100Scope::~Vt100Scope()
{
    if (intptr_t(_stdout_handle) <= 0)
        return;

    // Restore modifications to the terminal's mode
    SetConsoleMode(_stdout_handle, _con_mode);
}

// {{{1 py ---------------------------------------------------------------------
#if SHIM_BUILD == 1

#pragma section(".shim", read, write)

struct PyData
{
    uint16_t    dll_length;
    wchar_t     dll_path[1];
};

#if defined(DEBUG)
#   define PY_DLL L"C:/Users/martin.ridgers/AppData/Local/ushell/.working/python/3.11.0/python311.dll"
    static wchar_t _data[2048] = {
        (sizeof(PY_DLL) / sizeof(PY_DLL[0])),
        PY_DLL L"\0" L"-u\0" L"-E\0",
    };
#else
    static __declspec(allocate(".shim")) PyData _data = {
        LENGTH_ID,
        { DATA_ID },
    };
#endif

//------------------------------------------------------------------------------
template <int EnableSingleQuote>
static int read_arg_word(wchar_t* start)
{
    // note to self; the escaping of quotes doesn't quite match what MS does.

    auto is_quote = [] (int c) {
        return (c == '\"') | (EnableSingleQuote & (c == '\''));
    };

    wchar_t* write = start;
    const wchar_t* read = start;
    int c;
    int quote = 0;
    for (; c = *read; ++read, ++write)
    {
        *write = *read;

        if (quote)
        {
            if (c == '\\' & read[1] == quote)
            {
                ++read;
                *write = wchar_t(quote);
                continue;
            }

            quote = (c == quote) ? 0 : quote;
            write -= !quote;
            continue;
        }

        if (c == '\\' & is_quote(read[1]))
        {
            ++read;
            *write = '\"';
            continue;
        }
        if (quote = (is_quote(c) ? c : 0))
        {
            --write;
            continue;
        }

        if (c <= 0x20)
            break;
    }

    for (; c = *read, (!!c & unsigned(c) <= 0x20); ++read);

    *write = '\0';

    return int(ptrdiff_t(read - start));
}

//------------------------------------------------------------------------------
void __cdecl py(void*)
{
    auto* data = (PyData*)(&_data);

    // Load Python DLL and find Py_Main entry point
    SPAM("loading dll; %S", data->dll_path);
    HMODULE py_dll = LoadLibraryW(data->dll_path);
    if (py_dll == nullptr)
        quit(63);
    SPAM("loaded at %p", py_dll);

    FARPROC py_main_addr = GetProcAddress(py_dll, "Py_Main");
    if (py_main_addr == nullptr)
        quit(62);
    SPAM("Py_Main address; %p", py_main_addr);

    // Convert DLL path into a .exe path
    wchar_t* cursor = data->dll_path + data->dll_length - 5; // 5 skips ".dll\0"
    for (; cursor[-1] != 'n'; --cursor);
    cursor[0] = '.';
    cursor[1] = 'e';
    cursor[2] = 'x';
    cursor[3] = 'e';
    cursor[4] = '\0';

    // Setting this makes Python behave as if the shim is python.exe
    SetEnvironmentVariableW(L"PYTHONEXECUTABLE", data->dll_path);

    // Build argv arguments
    // We'll use memory after arguments for argv[]
    cursor = data->dll_path + data->dll_length + 2;
    for (; cursor[-2] || cursor[-1]; ++cursor);
    wchar_t** argv = (wchar_t**)cursor;

    wchar_t* cmd_line = GetCommandLineW();
    for (int c; c = *cmd_line, (!!c & unsigned(c) <= 0x20); ++cmd_line);
    argv[0] = cmd_line;
    cmd_line += read_arg_word<0>(cmd_line);

    int argv_i = 1;
    const int argv_n = 256;

    cursor = data->dll_path + data->dll_length;
    for (; argv_i < argv_n && *cursor; ++argv_i)
    {
        argv[argv_i] = cursor;
        for (cursor += 2; cursor[-1] != '\0'; ++cursor);
    }

    for (int i = 1; argv_i < argv_n && cmd_line[0] != '\0'; ++argv_i, ++i)
    {
        argv[argv_i] = cmd_line;
        cmd_line += read_arg_word<1>(cmd_line);
    }

    // Launch Python
    int ret = 0;
    {
        Vt100Scope _;

        for (int i = 0; i < argv_i; ++i)
            SPAM("arg %d %S", i, argv[i]);

        auto* py_main = (int (*)(int, wchar_t**))py_main_addr;
        ret = py_main(argv_i, argv);
    }

    quit(ret);
}

#endif // SHIM_BUILD

// {{{1 shim -------------------------------------------------------------------
#if SHIM_BUILD == 2

#pragma section(".shim", read, write)

struct ShimData
{
    uint16_t    path_length;
    uint16_t    args_length;
    wchar_t     path[1];
};

#if defined(DEBUG)
#   define PATH L"c:/windows/system32/cmd.exe"
#   define ARGS L"/c dir c:\\"
    static wchar_t _data[2048] = {
        (sizeof(PATH) / sizeof(PATH[0])),
        (sizeof(ARGS) / sizeof(ARGS[0])),
        PATH L"\0" ARGS,
    };
#else
    static __declspec(allocate(".shim")) ShimData _data = {
        LENGTH_ID,
        LENGTH_ID,
        { DATA_ID },
    };
#endif

//------------------------------------------------------------------------------
void __cdecl shim(void*)
{
    auto* data = (ShimData*)(&_data);

    // What type of subsystem are we to launch into?
    int subsystem = 0;
    uintptr_t base = uintptr_t("") & ~0xffff;
    auto& dos = *(const IMAGE_DOS_HEADER*)base;
    auto& nt32 = *(const IMAGE_NT_HEADERS32*)(base + dos.e_lfanew);
    if (nt32.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64)
    {
        auto& nt64 = (const IMAGE_NT_HEADERS64&)nt32;
        subsystem = nt64.OptionalHeader.Subsystem;
    }
    else
        subsystem = nt32.OptionalHeader.Subsystem;

    // As this is just a shim we don't want it to interfere with Ctrl-C/Break.
    if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
    {
        auto ctrlc_handler = [] (DWORD type) -> BOOL {
            return (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT);
        };
        SetConsoleCtrlHandler(ctrlc_handler, TRUE);
    }

    // Work out how to treat the executable location
    bool slashed = false;
    for (const wchar_t* i = data->path; !slashed && *i; ++i)
        slashed = (*i == '/');

    enum style_e { style_path, style_absolute, style_relative };

    style_e style = style_path;
    if (data->path[1] == ':')
        style = style_absolute;
    else if (slashed)
        style = style_relative;

    // Build path to executable.
    wchar_t* __restrict exe = data->path;
    wchar_t* __restrict args = exe + data->path_length;
    wchar_t* __restrict buffer = args + data->args_length;

    if (style == style_relative)
    {
        exe = buffer;
        wchar_t* __restrict write = exe + GetModuleFileNameW(nullptr, exe, 0x4000);
        do { --write; } while (write[-1] != '\\');
        const wchar_t* read = data->path;
        do { *write++ = *read++; } while (write[-1]);
        buffer = write;
    }

    // Build arguments.
    wchar_t* __restrict cmd = buffer;
    wchar_t* __restrict write = cmd;

    const wchar_t* cmd_line = GetCommandLineW();
    if (*cmd_line == '\"')
    {
        int i = 2;
        do {
            *write++ = *cmd_line++;
        } while ((i -= cmd_line[-1] == '\"') && *cmd_line);
        if (i)
            *write++ = '\"';
    }
    else
    {
        do { *write++ = *cmd_line; } while (*cmd_line++ > ' ');
        --write;
        --cmd_line;
    }

    do { *write++ = *args; } while (*args++);
    --write;
    do { *write++ = *cmd_line++; } while (*cmd_line);

    int ret = 0;
    { Vt100Scope _;

    // Launch the main process
    PROCESS_INFORMATION pi;
    STARTUPINFOW si;
    si.cb = sizeof(si);
    BOOL ok = CreateProcessW(exe, cmd, nullptr, nullptr, FALSE,
        INHERIT_PARENT_AFFINITY, nullptr, nullptr, &si, &pi);
    if (ok == TRUE)
    {
        if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
        {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD process_ret = 0;
            GetExitCodeProcess(pi.hProcess, &process_ret);
            ret = int(process_ret);
        }
    }
    else
        ret = 9009; // cmd.exe uses this if a command doesn't exist

    close_handle(pi.hThread);
    close_handle(pi.hProcess);

    } // vt100

    quit(ret);
}

#endif // SHIM_BUILD

// {{{1 dump -------------------------------------------------------------------
#if SHIM_BUILD == 3

static int dump(const char* tag, const char* exe_path)
{
    HANDLE pe_file = CreateFile(exe_path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pe_file == INVALID_HANDLE_VALUE)
        return 1;

    void* pe_data = VirtualAlloc(nullptr, 0x40000, MEM_COMMIT, PAGE_READWRITE);
    DWORD pe_size = 0;
    ReadFile(pe_file, pe_data, 0x8000, &pe_size, nullptr);

    auto& dos = *(IMAGE_DOS_HEADER*)pe_data;
    auto& nt = *(IMAGE_NT_HEADERS64*)(uintptr_t(pe_data) + dos.e_lfanew);
    if (nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
        return 1;

    IMAGE_SECTION_HEADER* shim_section = IMAGE_FIRST_SECTION(&nt);
    auto* end_section = shim_section + nt.FileHeader.NumberOfSections;
    for (; shim_section != end_section; ++shim_section)
        if (*(int*)(shim_section->Name) == 'ihs.')
            break;

    if (shim_section == end_section)
        return 1;

    // This gives us a large buffer following .shim. segment when the PE's loaded
    nt.OptionalHeader.SizeOfImage = 0x4000;

    // Validate shim starts with a run of known constants
    auto* shim_data = (uint16_t*)(uintptr_t(pe_data) + shim_section->PointerToRawData);
    for (int i = 0; i < 16; ++i)
    {
        uint16_t value = shim_data[i];
        if (value == DATA_ID)   break;
        if (value != LENGTH_ID) return 1;
    }

    auto& offset_to = [=] (const void* addr) {
        return int(uintptr_t(addr) - uintptr_t(pe_data));
    };

    char* buffer = (char*)(uintptr_t(pe_data) + pe_size);

    HANDLE out_file = CreateFile("shim.py", GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (out_file == INVALID_HANDLE_VALUE)
        return 1;
    SetFilePointer(out_file, 0, nullptr, FILE_END);

    auto& print = [=] (const char* format, ...) {
        va_list args;
        va_start(args, format);
        int count = wvsprintfA(buffer, format, args);
        va_end(args);

        DWORD written;
        WriteFile(out_file, buffer, count, &written, nullptr);
    };

    print("    %s_section = %d\n", tag, offset_to(shim_section));
    print("    %s_subsystem = %d\n", tag, offset_to(&nt.OptionalHeader.Subsystem));
    print("    %s_alignment = %d\n", tag, nt.OptionalHeader.SectionAlignment);
    print("    %s_data = %d\n", tag, shim_section->PointerToRawData);

    print("    %s_payload = (\n", tag);
    pe_size = shim_section->PointerToRawData;
    for (auto* pe_read = (unsigned char*)pe_data; pe_size;)
    {
        print("        b\"");
        for (int i = 0; pe_size && i < 64; ++i, --pe_size)
            print("\\x%02x", *pe_read++);
        print("\"\n");
    }
    print("    )\n");

    CloseHandle(out_file);
    return 0;
}

//------------------------------------------------------------------------------
void __cdecl dump(void*)
{
    DeleteFile("shim.py");

    int ret = 0;
    ret += dump("py", "py.exe");
    ret += dump("shim", "shim.exe");

    quit(ret);
}

#endif // SHIM_BUILD

// vim: foldlevel=1
