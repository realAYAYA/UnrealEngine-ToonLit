// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceUncookedOnlyUtils.h"
#include "DataInterfaceGraph.h"
#include "DataInterfaceGraph_EditorData.h"
#include "DataInterfaceGraph_EdGraph.h"
#include "RigVMCompiler/RigVMCompiler.h"
#include "RigVMCore/RigVM.h"
#include "Units/RigUnitContext.h"

namespace UE::DataInterfaceGraphUncookedOnly
{

void FUtils::Compile(UDataInterfaceGraph* InGraph)
{
	check(InGraph);
	
	UDataInterfaceGraph_EditorData* EditorData = GetEditorData(InGraph);
	
	if(EditorData->bIsCompiling)
	{
		return;
	}
	
	TGuardValue<bool> CompilingGuard(EditorData->bIsCompiling, true);
	
	EditorData->bErrorsDuringCompilation = false;

	EditorData->RigGraphDisplaySettings.MinMicroSeconds = EditorData->RigGraphDisplaySettings.LastMinMicroSeconds = DBL_MAX;
	EditorData->RigGraphDisplaySettings.MaxMicroSeconds = EditorData->RigGraphDisplaySettings.LastMaxMicroSeconds = (double)INDEX_NONE;
	
	TGuardValue<bool> ReentrantGuardSelf(EditorData->bSuspendModelNotificationsForSelf, true);
	TGuardValue<bool> ReentrantGuardOthers(EditorData->bSuspendModelNotificationsForOthers, true);

	RecreateVM(InGraph);

	InGraph->VMRuntimeSettings = EditorData->VMRuntimeSettings;

	FRigNameCache TempNameCache;
	FRigUnitContext InitContext;
	InitContext.State = EControlRigState::Init;
	InitContext.NameCache = &TempNameCache;

	FRigUnitContext UpdateContext = InitContext;
	UpdateContext.State = EControlRigState::Update;

	void* InitContextPtr = &InitContext;
	void* UpdateContextPtr = &UpdateContext;

	TArray<FRigVMUserDataArray> UserData;
	UserData.Add(FRigVMUserDataArray(&InitContextPtr, 1));
	UserData.Add(FRigVMUserDataArray(&UpdateContextPtr, 1));

	EditorData->CompileLog.Messages.Reset();
	EditorData->CompileLog.NumErrors = EditorData->CompileLog.NumWarnings = 0;

	URigVMCompiler* Compiler = URigVMCompiler::StaticClass()->GetDefaultObject<URigVMCompiler>();
	Compiler->Settings = (EditorData->bCompileInDebugMode) ? FRigVMCompileSettings::Fast() : EditorData->VMCompileSettings;
	URigVMController* RootController = EditorData->GetRigVMClient()->GetOrCreateController(EditorData->GetRigVMClient()->GetDefaultModel());
	Compiler->Compile(EditorData->GetRigVMClient()->GetAllModels(false, false), RootController, InGraph->RigVM, InGraph->GetRigVMExternalVariables(), UserData, &EditorData->PinToOperandMap);

	if (EditorData->bErrorsDuringCompilation)
	{
		if(Compiler->Settings.SurpressErrors)
		{
			Compiler->Settings.Reportf(EMessageSeverity::Info, InGraph,TEXT("Compilation Errors may be suppressed for Data Interface Graph: %s. See VM Compile Settings for more Details"), *InGraph->GetName());
		}
	}

	EditorData->bVMRecompilationRequired = false;
	if(InGraph->RigVM)
	{
		EditorData->VMCompiledEvent.Broadcast(InGraph, InGraph->RigVM);
	}

#if WITH_EDITOR
//	RefreshBreakpoints(EditorData);
#endif
}

void FUtils::RecreateVM(UDataInterfaceGraph* InGraph)
{
	InGraph->RigVM = NewObject<URigVM>(InGraph, TEXT("VM"), RF_NoFlags);
	
	// Cooked platforms will load these pointers from disk
	if (!FPlatformProperties::RequiresCookedData())
	{
		// We dont support ERigVMMemoryType::Work memory as we dont operate on an instance
	//	InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Work, true);
		InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Literal, true);
		InGraph->RigVM->GetMemoryByType(ERigVMMemoryType::Debug, true);
	}

	InGraph->RigVM->Reset();
}

UDataInterfaceGraph_EditorData* FUtils::GetEditorData(const UDataInterfaceGraph* InDataInterfaceGraph)
{
	check(InDataInterfaceGraph);
	
	return CastChecked<UDataInterfaceGraph_EditorData>(InDataInterfaceGraph->EditorData);
}

}
