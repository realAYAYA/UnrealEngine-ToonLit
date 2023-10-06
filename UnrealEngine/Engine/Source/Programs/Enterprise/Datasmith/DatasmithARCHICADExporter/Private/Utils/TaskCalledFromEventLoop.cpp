// Copyright Epic Games, Inc. All Rights Reserved.

#include "TaskCalledFromEventLoop.h"
#include "../ResourcesIDs.h"
#include "Error.h"

#include <atomic>

BEGIN_NAMESPACE_UE_AC

enum : GSType
{
	UEDirectLinkTask = 'DLTk'
};
enum : Int32
{
	CmdDoTask = 1
};

static std::atomic< int32 > SPendingTaskCount(0);

// Run the task if it's not already deleted
GSErrCode FTaskCalledFromEventLoop::DoTasks(GSHandle ParamHandle)
{
	if (ParamHandle)
	{
		Int32	  NbPars = 0;
		GSErrCode GSErr = ACAPI_Goodies(APIAny_GetMDCLParameterNumID, ParamHandle, &NbPars);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FTaskCalledFromEventLoop::DoTasks - APIAny_GetMDCLParameterNumID error %s\n",
						 GetErrorName(GSErr));
			return GSErr;
		}

		if (NbPars != 1)
		{
			UE_AC_DebugF("FTaskCalledFromEventLoop::DoTasks - Invalid number of parameters %d\n", NbPars);
			return APIERR_BADPARS;
		}

		API_MDCLParameter Param = {};
		Param.index = 1;
		GSErr = ACAPI_Goodies(APIAny_GetMDCLParameterID, ParamHandle, &Param);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FTaskCalledFromEventLoop::DoTasks - APIAny_GetMDCLParameterID 1 error %s\n",
						 GetErrorName(GSErr));
			return GSErr;
		}
		TSharedPtr< FTaskCalledFromEventLoop > TaskPtr;
		if (CHCompareCStrings(Param.name, "TaskSharedRef", CS_CaseSensitive) == 0 && Param.type == MDCLPar_pointer)
		{
			TSharedRef< FTaskCalledFromEventLoop >* TaskRefPtr =
				reinterpret_cast< TSharedRef< FTaskCalledFromEventLoop >* >(Param.ptr_par);
			TaskPtr = *TaskRefPtr;
			delete TaskRefPtr;
		}
		else if (CHCompareCStrings(Param.name, "TaskWeakPtr", CS_CaseSensitive) == 0 && Param.type == MDCLPar_pointer)
		{
			TWeakPtr< FTaskCalledFromEventLoop >* TaskWeakPtrPtr =
				reinterpret_cast< TWeakPtr< FTaskCalledFromEventLoop >* >(Param.ptr_par);
			TaskPtr = TaskWeakPtrPtr->Pin();
			delete TaskWeakPtrPtr;
		}
		else
		{
			UE_AC_DebugF("FTaskCalledFromEventLoop::DoTasks - Invalid parameters (type=%d) %s\n", Param.type,
						 Param.name);
			return APIERR_BADPARS;
		}
		--SPendingTaskCount;
		if (TaskPtr.IsValid())
		{
			TaskPtr->Run();
		}
		return NoError;
	}
	return ErrParam;
}

// Run the task if it's not already deleted
GSErrCode FTaskCalledFromEventLoop::DoTasksCallBack(GSHandle ParamHandle, GSPtr /* OutResultData */,
													bool /* bSilentMode */)
{
	GSErrCode GSErr = TryFunctionCatchAndLog("DoTasks", [&ParamHandle]() -> GSErrCode { return DoTasks(ParamHandle); });
	return GSErr;
}

// Schedule InTask to be executed on next event.
void FTaskCalledFromEventLoop::CallTaskFromEventLoop(const TSharedRef< FTaskCalledFromEventLoop >& InTask,
													 ERetainType								   InRetainType)
{
	GSHandle  ParamHandle = nullptr;
	GSErrCode GSErr = ACAPI_Goodies(APIAny_InitMDCLParameterListID, &ParamHandle);
	if (GSErr == NoError)
	{
		API_MDCLParameter Param;
		Zap(&Param);
		if (InRetainType == kSharedRef)
		{
			Param.name = "TaskSharedRef";
			Param.ptr_par = new TSharedRef< FTaskCalledFromEventLoop >(InTask);
		}
		else
		{
			Param.name = "TaskWeakPtr";
			Param.ptr_par = new TWeakPtr< FTaskCalledFromEventLoop >(InTask);
		}
		++SPendingTaskCount;
		Param.type = MDCLPar_pointer;
		GSErr = ACAPI_Goodies(APIAny_AddMDCLParameterID, ParamHandle, &Param);
		if (GSErr == NoError)
		{
			API_ModulID mdid;
			Zap(&mdid);
			mdid.developerID = kEpicGamesDevId;
			mdid.localID = kDatasmithExporterId;
			GSErr = ACAPI_Command_CallFromEventLoop(&mdid, UEDirectLinkTask, CmdDoTask, ParamHandle, false, nullptr);
			if (GSErr == NoError)
			{
				ParamHandle = nullptr;
			}
			else
			{
				UE_AC_DebugF(
					"FTaskCalledFromEventLoop::CallTaskFromEventLoop - ACAPI_Command_CallFromEventLoop error %s\n",
					GetErrorName(GSErr));
			}
		}
		else
		{
			UE_AC_DebugF("FTaskCalledFromEventLoop::CallTaskFromEventLoop - APIAny_AddMDCLParameterID error %s\n",
						 GetErrorName(GSErr));
		}

		if (ParamHandle != nullptr)
		{
			// Clean up
			if (InRetainType == kSharedRef)
			{
				delete reinterpret_cast< TSharedRef< FTaskCalledFromEventLoop >* >(Param.ptr_par);
			}
			else
			{
				delete reinterpret_cast< TWeakPtr< FTaskCalledFromEventLoop >* >(Param.ptr_par);
			}
			--SPendingTaskCount;

			GSErr = ACAPI_Goodies(APIAny_FreeMDCLParameterListID, &ParamHandle);
			if (GSErr != NoError)
			{
				UE_AC_DebugF(
					"FTaskCalledFromEventLoop::CallTaskFromEventLoop - APIAny_FreeMDCLParameterListID error %s\n",
					GetErrorName(GSErr));
			}
		}
	}
	else
	{
		UE_AC_DebugF("FTaskCalledFromEventLoop::CallTaskFromEventLoop - APIAny_InitMDCLParameterListID error %s\n",
					 GetErrorName(GSErr));
	}
}

// Register the task service
GSErrCode FTaskCalledFromEventLoop::Register()
{
	GSErrCode GSErr = ACAPI_Register_SupportedService(UEDirectLinkTask, CmdDoTask);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FTaskCalledFromEventLoop::Register - Error %d\n", GSErr);
	}
	return GSErr;
}

// Initialize
GSErrCode FTaskCalledFromEventLoop::Initialize()
{
	GSErrCode GSErr = ACAPI_Install_ModulCommandHandler(UEDirectLinkTask, CmdDoTask, DoTasksCallBack);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FTaskCalledFromEventLoop::Initialize - Error %d\n", GSErr);
	}
	return GSErr;
}

// Uninitialize the task service
void FTaskCalledFromEventLoop::Uninitialize()
{
	int PendingTaskCount = SPendingTaskCount;
	if (PendingTaskCount != 0)
	{
		UE_AC_DebugF("FTaskCalledFromEventLoop::Uninitialize - Pending tasks %d\n", PendingTaskCount);
	}
}

END_NAMESPACE_UE_AC
