// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelViewViewModelModule.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Types/MVVMExecutionMode.h"

#define LOCTEXT_NAMESPACE "ModelViewViewModelModule"

void FModelViewViewModelModule::StartupModule()
{
	IConsoleVariable* CVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode"));
	if (ensure(CVarDefaultExecutionMode))
	{
		HandleDefaultExecutionModeChanged(CVarDefaultExecutionMode);
		CVarDefaultExecutionMode->OnChangedDelegate().AddRaw(this, &FModelViewViewModelModule::HandleDefaultExecutionModeChanged);
	}
}

void FModelViewViewModelModule::ShutdownModule()
{
	if (!IsEngineExitRequested())
	{
		if (IConsoleVariable* CVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode")))
		{
			CVarDefaultExecutionMode->OnChangedDelegate().RemoveAll(this);
		}
	}
}

void FModelViewViewModelModule::HandleDefaultExecutionModeChanged(IConsoleVariable* Variable)
{
	const int32 Value = Variable->GetInt();
	switch(Value)
	{
	case (int32)EMVVMExecutionMode::Delayed:
	case (int32)EMVVMExecutionMode::Immediate:
	case (int32)EMVVMExecutionMode::Tick:
	case (int32)EMVVMExecutionMode::DelayedWhenSharedElseImmediate:
		break;
	default:
		ensureMsgf(false, TEXT("MVVM.DefaultExecutionMode default value is not a valid value."));
		Variable->Set((int32)EMVVMExecutionMode::DelayedWhenSharedElseImmediate, (EConsoleVariableFlags)(Variable->GetFlags() & ECVF_SetByMask));
		break;
	}
}

IMPLEMENT_MODULE(FModelViewViewModelModule, ModelViewViewModel);

#undef LOCTEXT_NAMESPACE
