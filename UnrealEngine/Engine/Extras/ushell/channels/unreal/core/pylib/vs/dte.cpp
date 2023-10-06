// Copyright Epic Games, Inc. All Rights Reserved.

#define _HAS_EXCEPTIONS 0
#define PY_SSIZE_T_CLEAN

#include <objbase.h>
#include <olectl.h>
#include <Python.h>
#include <new>

#include <dte.h>
#include <dte80.h>

//------------------------------------------------------------------------------
template <typename T>
class ComPtr
{
public:
                ComPtr()            = default;
                ComPtr(void* t)     { _ptr = (T*)t; }
                ~ComPtr()           { if (_ptr != nullptr) _ptr->Release(); }
    explicit    operator bool ()    { return _ptr != nullptr; }
                operator void** ()  { return (void**)&_ptr; }
                operator T** ()     { return &_ptr; }
                operator T* ()      { return _ptr; }
    T*          operator -> ()      { return _ptr; }
    T*          get()               { return _ptr; }
    T*          detach()            { T* ret = _ptr; forget(); return ret; }
    void        forget()            { _ptr = nullptr; }

private:
    T*          _ptr = nullptr;

                ComPtr(const ComPtr&) = delete;
                ComPtr(const ComPtr&&) = delete;
    ComPtr&     operator = (const ComPtr&) = delete;
    ComPtr&     operator = (const ComPtr&&) = delete;
};

//------------------------------------------------------------------------------
class BStr
{
public:
                BStr(const wchar_t* in) { _str = SysAllocString(in); }
                ~BStr()                 { SysFreeString(_str); }
                operator BSTR& ()       { return _str; }

private:
    BSTR        _str;
                BStr() = delete;
                BStr(const BStr&) = delete;
                BStr(const BStr&&) = delete;
    BStr&       operator = (const BStr&) = delete;
    BStr&       operator = (const BStr&&) = delete;
};



//------------------------------------------------------------------------------
class Dte
    : public PyObject
{
public:
                    Dte(_DTE* dte);
    void            shutdown();
    const wchar_t*  get_sln_path() const;
    const wchar_t*  get_version() const;
    bool            attach(int pid, const wchar_t* transport, const wchar_t* host) const;
    bool            attach(const wchar_t* name, const wchar_t* transport, const wchar_t* host) const;
    bool            open_file(const wchar_t* path, int line=-1) const;
    bool            activate() const;

private:
    template <typename T>
    bool            attach_impl(T name, const wchar_t* transport, const wchar_t* host) const;
    _DTE*           _dte;
};

//------------------------------------------------------------------------------
Dte::Dte(_DTE* dte)
: _dte(dte)
{
    _dte->AddRef();
}

//------------------------------------------------------------------------------
void Dte::shutdown()
{
    _dte->Release();
}

//------------------------------------------------------------------------------
const wchar_t* Dte::get_sln_path() const
{
    Solution* outer;
    if (FAILED(_dte->get_Solution(&outer)))
        return nullptr;

    ComPtr<_Solution> solution(outer);

    BSTR out;
    return SUCCEEDED(solution->get_FullName(&out)) ? out : nullptr;
}

//------------------------------------------------------------------------------
const wchar_t* Dte::get_version() const
{
    BSTR out;
    return SUCCEEDED(_dte->get_Version(&out)) ? out : nullptr;
}

//------------------------------------------------------------------------------
bool Dte::attach(int pid, const wchar_t* transport, const wchar_t* host) const
{
    return attach_impl<int>(pid, transport, host);
}

//------------------------------------------------------------------------------
bool Dte::attach(const wchar_t* name, const wchar_t* transport, const wchar_t* host) const
{
    return attach_impl<const wchar_t*>(name, transport, host);
}

//------------------------------------------------------------------------------
template <typename T>
bool Dte::attach_impl(T pid, const wchar_t* transport, const wchar_t* host) const
{
    transport = transport ? transport : L"Default";

    // Get the debugger interface
    ComPtr<Debugger> debugger;
    if (FAILED(_dte->get_Debugger(debugger)))
        return false;

    ComPtr<Debugger2> debugger2;
    if (FAILED(debugger->QueryInterface(__uuidof(Debugger2), debugger2)))
        return false;

    // Find the transport
    ComPtr<Transports> transports;
    debugger2->get_Transports(transports);

    long n;
    transports->get_Count(&n);
    VARIANT index = { VT_I4 };
    for (long i = 1; i <= n; ++i)
    {
        index.lVal = i;
        ComPtr<Transport> candidate;
        if (FAILED(transports->Item(index, candidate)))
            continue;

        BSTR candidate_name;
        if (FAILED(candidate->get_Name(&candidate_name)))
            continue;

        if (lstrcmpiW(transport, candidate_name) != 0)
            continue;

        // Now find the process within the transport at attach to it.
        ComPtr<Processes> processes;
        if (FAILED(debugger2->GetProcesses(candidate, BStr(host), processes)))
            return false;

        struct
        {
            const wchar_t* preprocess(const wchar_t* input) const
            {
                int len = lstrlenW(input);
                for (const wchar_t* c = input + len; c > input; --c)
                    if (c[-1] == '\\' || c[-1] == '/')
                        return c;

                return input;
            }

            int preprocess(int input) const
            {
                return input;
            }

            bool operator () (Process* process, int pid) const
            {
                long candidate_pid;
                if (FAILED(process->get_ProcessID(&candidate_pid)))
                    return false;

                return candidate_pid == pid;
            }

            bool operator () (Process* process, const wchar_t* name) const
            {
                BSTR candidate_name;
                if (FAILED(process->get_Name(&candidate_name)))
                    return false;

                return lstrcmpiW(name, preprocess(candidate_name)) == 0;
            }
        } is_target_process;

        pid = is_target_process.preprocess(pid);

        long m;
        processes->get_Count(&m);
        VARIANT index = { VT_I4 };
        for (long j = 1; j <= m; ++j)
        {
            index.lVal = j;
            ComPtr<Process> process;
            if (SUCCEEDED(processes->Item(index, process)))
                if (is_target_process(process, pid))
                    return SUCCEEDED(process->Attach());
        }

        return false;
    }

    return false;
}

//------------------------------------------------------------------------------
bool Dte::open_file(const wchar_t* path, int line) const
{
    ComPtr<ItemOperations> item_ops;
    if (FAILED(_dte->get_ItemOperations(item_ops)))
        return false;

    const wchar_t* view_kind = L"{7651A703-06E5-11D1-8EBD-00A0C90F26EA}"; // from vsViewKindTextView

    ComPtr<Window> window;
    if (FAILED(item_ops->OpenFile(BStr(path), BStr(view_kind), window)))
        return false;

    if (line < 0)
        return true;

    ComPtr<Document> document;
    if (SUCCEEDED(_dte->get_ActiveDocument(document)))
    {
        ComPtr<IDispatch> selection;
        if (SUCCEEDED(document->get_Selection(selection)))
        {
            ComPtr<TextSelection> text_selection;
            if (SUCCEEDED(selection->QueryInterface(__uuidof(TextSelection), text_selection)))
            {
                text_selection->GotoLine(line, FALSE);
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------
bool Dte::activate() const
{
    ComPtr<Window> main_window;
    if (FAILED(_dte->get_MainWindow(main_window)))
        return false;

    return SUCCEEDED(main_window->Activate());
}



//------------------------------------------------------------------------------
static PyTypeObject dte_type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
};

//------------------------------------------------------------------------------
void dte_free(PyObject* object)
{
    auto* dte = (Dte*)object;
    dte->shutdown();
    PyMem_Free(object);
}

//------------------------------------------------------------------------------
PyObject* dte_get_sln_path(PyObject* object, PyObject* args)
{
    auto* dte = (Dte*)object;
    if (const wchar_t* name = dte->get_sln_path())
        return PyUnicode_FromWideChar(name, -1);

    Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* dte_get_version(PyObject* object, PyObject* args)
{
    auto* dte = (Dte*)object;

    if (const wchar_t* version = dte->get_version())
        return PyUnicode_FromWideChar(version, -1);

    Py_RETURN_NONE;
}

//------------------------------------------------------------------------------
PyObject* dte_attach(PyObject* object, PyObject* args)
{
    auto* dte = (Dte*)object;

    int argc = PyTuple_Size(args);

    if (argc < 1)
    {
        PyErr_SetString(PyExc_TypeError, "Pid expected");
        return nullptr;
    }

    // transport argument
    wchar_t* transport = nullptr;
    PyObject* py_transport = (argc > 1) ? PyTuple_GetItem(args, 1) : nullptr;
    if (py_transport != nullptr && py_transport != Py_None)
    {
        if (!PyUnicode_Check(py_transport))
            return PyErr_SetString(PyExc_TypeError, "Argument 1 must be a string (transport)"), nullptr;

        transport = PyUnicode_AsWideCharString(py_transport, nullptr);
    }

    // host argument
    wchar_t* host = nullptr;
    PyObject* py_host = (argc > 2) ? PyTuple_GetItem(args, 2) : nullptr;
    if (py_host != nullptr && py_host != Py_None)
    {
        if (!PyUnicode_Check(py_host))
            return PyErr_SetString(PyExc_TypeError, "Argument 2 must be a string (host)"), nullptr;

        host = PyUnicode_AsWideCharString(py_host, nullptr);
    }

    // process argument
    bool ret = false;
    PyObject* py_process = PyTuple_GetItem(args, 0);
    if (PyLong_Check(py_process))
    {
        int pid = PyLong_AsLong(py_process);
        ret = dte->attach(pid, transport, host);
    }
    else if (PyUnicode_Check(py_process))
    {
        wchar_t* name = PyUnicode_AsWideCharString(py_process, nullptr);
        ret = dte->attach(name, transport, host);
        PyMem_Free(name);
    }
    else
        return PyErr_SetString(PyExc_TypeError, "Argument 0 must be an integer (pid) or string (name)"), nullptr;

    PyMem_Free(host);
    PyMem_Free(transport);

    if (ret)
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

//------------------------------------------------------------------------------
PyObject* dte_open_file(PyObject* object, PyObject* args)
{
    int argc = PyTuple_Size(args);

    wchar_t* path = nullptr;
    if (argc > 0)
    {
        PyObject* py_path = PyTuple_GetItem(args, 0);
        if (!PyUnicode_Check(py_path))
            return PyErr_SetString(PyExc_TypeError, "Argument 1 must be a string"), nullptr;

        path = PyUnicode_AsWideCharString(py_path, nullptr);
    }
    else
        Py_RETURN_FALSE;

    int line = -1;
    if (argc > 1)
    {
        PyObject* py_line = PyTuple_GetItem(args, 1);
        if (!PyLong_Check(py_line))
            return PyErr_SetString(PyExc_TypeError, "Argument 2 must be an integer"), nullptr;

        line = PyLong_AsLong(py_line);
    }

    auto* dte = (Dte*)object;
    bool ret = dte->open_file(path, line);

    PyMem_Free(path);

    if (ret)
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

//------------------------------------------------------------------------------
PyObject* dte_activate(PyObject* object, PyObject* args)
{
    auto* dte = (Dte*)object;
    if (dte->activate())
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

//------------------------------------------------------------------------------
static PyMethodDef dte_type_methods[] = {
    { "get_sln_path", dte_get_sln_path, METH_VARARGS, "Returns the path to the loaded .sln" },
    { "get_version",  dte_get_version,  METH_VARARGS, "Returns VS' version" },
    { "attach",       dte_attach,       METH_VARARGS, "Attaches to a process on a given transport" },
    { "activate",     dte_activate,     METH_VARARGS, "Brings the VS instance to the foreground" },
    { "open_file",    dte_open_file,    METH_VARARGS, "Opens a file, and optionally moves the cursor to a given line number" },
    {},
};



//------------------------------------------------------------------------------
class DteSet
{
public:
    bool                    add(_DTE* dte);
    _DTE*                   get(unsigned index) const   { return index < _count ? _dtes[index] : nullptr; }
    unsigned                get_count() const           { return _count; }

private:
    static const unsigned   _MAX = 16;
    unsigned                _count = 0;
    _DTE*                   _dtes[_MAX];
};

//------------------------------------------------------------------------------
bool DteSet::add(_DTE* dte)
{
    if (_count >= _MAX)
        return false;

    for (unsigned i = 0; i < get_count(); ++i)
        if (_dtes[i] == dte)
            return false;

    _dtes[_count++] = dte;
    return true;
}

//------------------------------------------------------------------------------
class RunningIter
    : public PyObject
{
public:
                RunningIter();
    void        shutdown();
    _DTE*       next();

private:
    DteSet      _dtes;
    unsigned    _iter = 0;
};

//------------------------------------------------------------------------------
RunningIter::RunningIter()
{
    ComPtr<IRunningObjectTable> running_object_table;
    GetRunningObjectTable(0, running_object_table);
    if (!running_object_table)
        return;

    ComPtr<IEnumMoniker> enum_moniker;
    running_object_table->EnumRunning(enum_moniker);
    if (!enum_moniker)
        return;

    ComPtr<IBindCtx> bind_ctx;
    CreateBindCtx(0, bind_ctx);
    if (!bind_ctx)
        return;

    while (true)
    {
        ComPtr<IMoniker> moniker;
        if (enum_moniker->Next(1, moniker, nullptr) == S_FALSE)
            break;

        ComPtr<IUnknown> unknown;
        running_object_table->GetObject(moniker, unknown);

        ComPtr<_DTE> dte;
        unknown->QueryInterface(__uuidof(_DTE), dte);

        if (dte && _dtes.add(dte))
            dte.forget();
    }
}

//------------------------------------------------------------------------------
void RunningIter::shutdown()
{
    for (int i = 0, n = _dtes.get_count(); i < n; ++i)
        _dtes.get(i)->Release();
}

//------------------------------------------------------------------------------
_DTE* RunningIter::next()
{
    if (_iter >= _dtes.get_count())
        return nullptr;

    _DTE* dte = _dtes.get(_iter);
    _iter++;

    return dte;
}



//------------------------------------------------------------------------------
static PyTypeObject running_iter_type = {
    //PyVarObject_HEAD_INIT(nullptr, 0)
};

//------------------------------------------------------------------------------
static PyObject* running_iter_new(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    auto* iter = PyMem_New(RunningIter, sizeof(RunningIter));
    new (iter) RunningIter();
    return PyObject_Init(iter, &running_iter_type);
};

//------------------------------------------------------------------------------
static void running_iter_free(PyObject* object)
{
    auto* iter = (RunningIter*)object;
    iter->shutdown();
    PyMem_Free(object);
};

//------------------------------------------------------------------------------
static PyObject* running_iter_next(PyObject* object)
{
    auto* iter = (RunningIter*)object;
    _DTE* dte = iter->next();
    if (dte == nullptr)
        return nullptr;

    auto* py_dte = PyMem_New(Dte, sizeof(Dte));
    new (py_dte) Dte(dte);
    return PyObject_Init(py_dte, &dte_type);
}



//------------------------------------------------------------------------------
static int (WINAPI *NtResumeProcess)(HANDLE);
static int (WINAPI *NtSuspendProcess)(HANDLE);

//------------------------------------------------------------------------------
static void suspend_resume_init()
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll");
    NtResumeProcess = decltype(NtResumeProcess)(GetProcAddress(ntdll, "NtResumeProcess"));
    NtSuspendProcess = decltype(NtSuspendProcess)(GetProcAddress(ntdll, "NtSuspendProcess"));
}

//------------------------------------------------------------------------------
static PyObject* suspend_resume(bool is_suspend, PyObject** args, int nargs)
{
    if (nargs < 1)
        Py_RETURN_FALSE;

    PyObject* py_pid = args[0];
    if (!PyLong_Check(py_pid))
        Py_RETURN_FALSE;

    unsigned pid = PyLong_AsLong(py_pid);

    HANDLE handle = OpenProcess(PROCESS_SUSPEND_RESUME, FALSE, pid);
    if (handle == nullptr)
        Py_RETURN_FALSE;

    int ok = is_suspend ? NtSuspendProcess(handle) : NtResumeProcess(handle);
    CloseHandle(handle);

    if (ok < 0)
        Py_RETURN_FALSE;

    Py_RETURN_TRUE;
}

//------------------------------------------------------------------------------
static PyObject* suspend_process(PyObject* self, PyObject** args, Py_ssize_t nargs)
{
    return suspend_resume(true, args, int(nargs));
}

//------------------------------------------------------------------------------
static PyObject* resume_process(PyObject* self, PyObject** args, Py_ssize_t nargs)
{
    return suspend_resume(false, args, int(nargs));
}



//------------------------------------------------------------------------------
static PyMethodDef dte_module_methods[] = {
    { "suspend_process", PyCFunction(suspend_process), METH_FASTCALL, "Suspend a process by PID"},
    { "resume_process",  PyCFunction(resume_process),  METH_FASTCALL, "Resume a process by PID"},
    {},
};

//------------------------------------------------------------------------------
static PyModuleDef dte_module = {
    PyModuleDef_HEAD_INIT,
    "dte",
    nullptr,
    -1,
    dte_module_methods,
};

//------------------------------------------------------------------------------
extern "C" __declspec(dllexport)
PyObject* PyInit_dte()
{
    CoInitialize(nullptr);
    suspend_resume_init();

    //&PyType_Type
    running_iter_type.tp_name = "read_running";
    running_iter_type.tp_doc = "Iterates running instances of Visual Studio";
    running_iter_type.tp_basicsize = sizeof(RunningIter);
    running_iter_type.tp_flags = Py_TPFLAGS_DEFAULT;
    running_iter_type.tp_new = running_iter_new;
    running_iter_type.tp_dealloc = running_iter_free;
    running_iter_type.tp_iter = PyObject_SelfIter;
    running_iter_type.tp_iternext = running_iter_next;
    if (PyType_Ready(&running_iter_type) < 0)
        return nullptr;

    //&PyType_Type
    dte_type.tp_name = "dte";
    dte_type.tp_doc = "Class that wraps a _DTE COM object";
    dte_type.tp_basicsize = sizeof(Dte);
    dte_type.tp_flags = Py_TPFLAGS_DEFAULT;
    dte_type.tp_dealloc = dte_free;
    dte_type.tp_methods = dte_type_methods;
    if (PyType_Ready(&dte_type) < 0)
        return nullptr;

    dte_module.m_free = [] (void*) { CoUninitialize(); };
    PyObject* module = PyModule_Create(&dte_module);
    PyModule_AddObject(module, "running", (PyObject*)&running_iter_type);
    return module;
}


//------------------------------------------------------------------------------
BOOL WINAPI
_DllMainCRTStartup(
        HANDLE  hDllHandle,
        DWORD   dwReason,
        LPVOID  lpreserved
        )
{
    return TRUE;
}
