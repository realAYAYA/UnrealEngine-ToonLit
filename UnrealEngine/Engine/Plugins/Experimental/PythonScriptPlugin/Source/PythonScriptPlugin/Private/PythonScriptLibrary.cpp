// Copyright Epic Games, Inc. All Rights Reserved.

#include "PythonScriptLibrary.h"
#include "PythonScriptPlugin.h"
#include "PyGIL.h"
#include "PyUtil.h"
#include "PyGenUtil.h"
#include "PyConversion.h"
#include "PyWrapperTypeRegistry.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PythonScriptLibrary)

bool UPythonScriptLibrary::IsPythonAvailable()
{
	return IPythonScriptPlugin::Get()->IsPythonAvailable();
}

bool UPythonScriptLibrary::ExecutePythonCommand(const FString& PythonCommand)
{
	return IPythonScriptPlugin::Get()->ExecPythonCommand(*PythonCommand);
}

bool UPythonScriptLibrary::ExecutePythonCommandEx(const FString& PythonCommand, FString& CommandResult, TArray<FPythonLogOutputEntry>& LogOutput, const EPythonCommandExecutionMode ExecutionMode, const EPythonFileExecutionScope FileExecutionScope)
{
	FPythonCommandEx PythonCommandEx;
	PythonCommandEx.Command = PythonCommand;
	PythonCommandEx.ExecutionMode = ExecutionMode;
	PythonCommandEx.FileExecutionScope = FileExecutionScope;

	const bool bResult = IPythonScriptPlugin::Get()->ExecPythonCommandEx(PythonCommandEx);
		
	CommandResult = MoveTemp(PythonCommandEx.CommandResult);
	LogOutput = MoveTemp(PythonCommandEx.LogOutput);
		
	return bResult;
}

bool UPythonScriptLibrary::ExecutePythonScript(const FString& PythonScript, const TArray<FString>& PythonInputs, const TArray<FString>& PythonOutputs)
{
	// We should never hit this!
	check(0);
	return false;
}

DEFINE_FUNCTION(UPythonScriptLibrary::execExecutePythonScript)
{
	auto ExecuteCustomPythonScriptImpl = [&]() -> bool
	{
		const FString FunctionErrorName = Stack.Node->GetName();
		const FString FunctionErrorCtxt = Stack.Node->GetOutermost()->GetName();

#if WITH_PYTHON
		// Local Python context used when executing this script
		// Has the inputs written into it prior to execution, and the outputs read from it after execution
		FPyObjectPtr PyTempGlobalDict = FPyObjectPtr::StealReference(PyDict_Copy(FPythonScriptPlugin::Get()->GetDefaultGlobalDict()));
		FPyObjectPtr PyTempLocalDict = PyTempGlobalDict;
#endif	// WITH_PYTHON

		// Read the standard function arguments
		P_GET_PROPERTY_REF(FStrProperty, PythonScript);
		P_GET_TARRAY_REF(FString, PythonInputs);
		P_GET_TARRAY_REF(FString, PythonOutputs);

		// Read the input values and write them to the Python context
		bool bHasValidInputValues = true;
		{
			for (const FString& PythonInput : PythonInputs)
			{
				// Note: Medium term the Blueprint interpreter will change to provide us with a list of properties and 
				// instance pointers, rather than forcing us to jump in and out of its interpreter loop (via StepCompiledIn)
				Stack.StepCompiledIn<FProperty>(nullptr);
				check(Stack.MostRecentProperty && Stack.MostRecentPropertyAddress);

#if WITH_PYTHON
				FPyObjectPtr PyInput;
				if (PyConversion::PythonizeProperty_Direct(Stack.MostRecentProperty, Stack.MostRecentPropertyAddress, PyInput.Get()))
				{
					PyDict_SetItemString(PyTempLocalDict, TCHAR_TO_UTF8(*PythonInput), PyInput);
				}
				else
				{
					PyUtil::SetPythonError(PyExc_TypeError, *FunctionErrorCtxt, *FString::Printf(TEXT("Failed to convert input property '%s' (%s) to attribute '%s' when calling function '%s' on '%s'"), *Stack.MostRecentProperty->GetName(), *Stack.MostRecentProperty->GetClass()->GetName(), *PythonInput, *FunctionErrorName, *P_THIS_OBJECT->GetName()));
					bHasValidInputValues = false;
				}
#endif	// WITH_PYTHON
			}
		}

		struct FPyOutputParam
		{
			FProperty* Property = nullptr;
			uint8* PropAddr = nullptr;
			const TCHAR* OutputName = nullptr;
		};

		// Read the output values and store them to write to later from the Python context
		TArray<FPyOutputParam, TInlineAllocator<4>> OutParms;
		for (const FString& PythonOutput : PythonOutputs)
		{
			// Note: Medium term the Blueprint interpreter will change to provide us with a list of properties and 
			// instance pointers, rather than forcing us to jump in and out of its interpreter loop (via StepCompiledIn)
			Stack.StepCompiledIn<FProperty>(nullptr);
			check(Stack.MostRecentProperty && Stack.MostRecentPropertyAddress);

			FPyOutputParam& OutParam = OutParms.AddDefaulted_GetRef();
			OutParam.Property = Stack.MostRecentProperty;
			OutParam.PropAddr = Stack.MostRecentPropertyAddress;
			OutParam.OutputName = *PythonOutput;
		}

		P_FINISH;

#if	WITH_PYTHON
		// If we already failed during the parameter processing phase, then just bail now
		if (!bHasValidInputValues)
		{
			return false;
		}

		// Execute the Python command
		FPyObjectPtr PyResult = FPyObjectPtr::StealReference(FPythonScriptPlugin::Get()->EvalString(*PythonScript, TEXT("<string>"), Py_file_input, PyTempGlobalDict, PyTempGlobalDict));
		Py_BEGIN_ALLOW_THREADS
		FPyWrapperTypeReinstancer::Get().ProcessPending();
		Py_END_ALLOW_THREADS

		// Read the output values from the Python context
		if (PyResult)
		{
			for (const FPyOutputParam& OutParam : OutParms)
			{
				PyObject* PyOutput = PyDict_GetItemString(PyTempLocalDict, TCHAR_TO_UTF8(OutParam.OutputName));
				if (!PyOutput)
				{
					PyUtil::SetPythonError(PyExc_TypeError, *FunctionErrorCtxt, *FString::Printf(TEXT("Failed to find attribute '%s' for output property '%s' (%s) when calling function '%s' on '%s'"), OutParam.OutputName, *OutParam.Property->GetName(), *OutParam.Property->GetClass()->GetName() , *FunctionErrorName, *P_THIS_OBJECT->GetName()));
					return false;
				}

				if (!PyConversion::NativizeProperty_Direct(PyOutput, OutParam.Property, OutParam.PropAddr))
				{
					PyUtil::SetPythonError(PyExc_TypeError, *FunctionErrorCtxt, *FString::Printf(TEXT("Failed to convert output property '%s' (%s) from attribute '%s' when calling function '%s' on '%s'"), *OutParam.Property->GetName(), *OutParam.Property->GetClass()->GetName(), OutParam.OutputName, *FunctionErrorName, *P_THIS_OBJECT->GetName()));
					return false;
				}
			}

			return true;
		}
#endif	// WITH_PYTHON

		return false;
	};

#if WITH_PYTHON
	// Execute Python code within this block
	{
		FPyScopedGIL GIL;
		if (ExecuteCustomPythonScriptImpl())
		{
			*(bool*)RESULT_PARAM = true;
		}
		else
		{
			*(bool*)RESULT_PARAM = PyUtil::ReThrowPythonError();
		}
	}
#else	// WITH_PYTHON
	// We still need to call this function to step the bytecode correctly...
	ExecuteCustomPythonScriptImpl();
	*(bool*)RESULT_PARAM = false;
#endif	// WITH_PYTHON
}

