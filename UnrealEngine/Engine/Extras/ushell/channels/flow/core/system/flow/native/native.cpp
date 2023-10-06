// Copyright Epic Games, Inc. All Rights Reserved.

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <Windows.h>

//------------------------------------------------------------------------------
static PyObject* isatty_impl(PyObject* self, PyObject** args, Py_ssize_t nargs)
{
    HANDLE Handle = nullptr;

    if (nargs > 0 && PyLong_Check(args[0]))
    {
        switch (PyLong_AsLong(args[0]))
        {
        case 0: Handle = GetStdHandle(STD_INPUT_HANDLE);    break;
        case 1: Handle = GetStdHandle(STD_OUTPUT_HANDLE);   break;
        case 2: Handle = GetStdHandle(STD_ERROR_HANDLE);    break;
        }
    }

    if (Handle == nullptr)
        Py_RETURN_NONE;

    DWORD Type = GetFileType(Handle);
    if (Type == FILE_TYPE_CHAR)
    {
        DWORD Mode;
        if (GetConsoleMode(Handle, &Mode))
            Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

//------------------------------------------------------------------------------
static PyMethodDef native_module_methods[] = {
    { "isatty", PyCFunction(isatty_impl), METH_FASTCALL, "Like isatty() but correctly reports false for NUL"},
    {},
};

//------------------------------------------------------------------------------
static PyModuleDef native_module = {
    PyModuleDef_HEAD_INIT,
    "native",
    nullptr,
    -1,
    native_module_methods,
};

//------------------------------------------------------------------------------
extern "C" __declspec(dllexport)
PyObject* PyInit_native()
{
    PyObject* module = PyModule_Create(&native_module);
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
