// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelViewViewModelModule.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Types/MVVMExecutionMode.h"

#define LOCTEXT_NAMESPACE "ModelViewViewModelModule"

void FModelViewViewModelModule::StartupModule()
{
	if (IConsoleVariable* CVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode")))
	{
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
	EMVVMExecutionMode Value = (EMVVMExecutionMode)Variable->GetInt();
	if (Value != EMVVMExecutionMode::Delayed
		&& Value != EMVVMExecutionMode::Immediate
		&& Value != EMVVMExecutionMode::Tick
		&& Value != EMVVMExecutionMode::DelayedWhenSharedElseImmediate)
	{
		Variable->Set((int32)EMVVMExecutionMode::Immediate, (EConsoleVariableFlags)(Variable->GetFlags() & ECVF_SetByMask));
	}
}

IMPLEMENT_MODULE(FModelViewViewModelModule, ModelViewViewModel);

#undef LOCTEXT_NAMESPACE
