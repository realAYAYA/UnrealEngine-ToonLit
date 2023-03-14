// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigBlueprintCompiler.h"
#include "ControlRig.h"
#include "KismetCompiler.h"
#include "ControlRigBlueprint.h"
#include "Units/RigUnit.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogControlRigCompiler, Log, All);
#define LOCTEXT_NAMESPACE "ControlRigBlueprintCompiler"

bool FControlRigBlueprintCompiler::CanCompile(const UBlueprint* Blueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UControlRig::StaticClass()))
	{
		return true;
	}

	return false;
}

void FControlRigBlueprintCompiler::Compile(UBlueprint* Blueprint, const FKismetCompilerOptions& CompileOptions, FCompilerResultsLog& Results)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FControlRigBlueprintCompilerContext Compiler(Blueprint, Results, CompileOptions);
	Compiler.Compile();
}

void FControlRigBlueprintCompilerContext::MarkCompilationFailed(const FString& Message)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		Blueprint->Status = BS_Error;
		Blueprint->MarkPackageDirty();
		UE_LOG(LogControlRigCompiler, Error, TEXT("%s"), *Message);
		MessageLog.Error(*Message);

#if WITH_EDITOR
		FNotificationInfo Info(FText::FromString(Message));
		Info.Image = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Error"));
		Info.bFireAndForget = true;
		Info.FadeOutDuration = 5.0f;
		Info.ExpireDuration = 5.0f;
		TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
#endif
	}
}

void FControlRigBlueprintCompilerContext::PostCompile()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		if (!ControlRigBlueprint->bIsRegeneratingOnLoad)
		{
			// todo: can we find out somehow if we are cooking?
			ControlRigBlueprint->RecompileVM();
		}
	}

	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER(FKismetCompilerContext::PostCompile)
		FKismetCompilerContext::PostCompile();
	}
}

void FControlRigBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FKismetCompilerContext::CopyTermDefaultsToDefaultObject(DefaultObject);

	UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	if (ControlRigBlueprint)
	{
		// here, CDO is initialized from BP,
		// and in UControlRig::InitializeFromCDO,
		// other Control Rig Instances are then initialized From the CDO 
		UControlRig* ControlRig = CastChecked<UControlRig>(DefaultObject);
		ControlRig->PostInitInstanceIfRequired();
		
		// copy hierarchy
		{
			TGuardValue<bool> Guard(ControlRig->GetHierarchy()->GetSuspendNotificationsFlag(), false);
			ControlRig->GetHierarchy()->CopyHierarchy(ControlRigBlueprint->Hierarchy);
			ControlRig->GetHierarchy()->ResetPoseToInitial(ERigElementType::All);

#if WITH_EDITOR
			// link CDO hierarchy to BP's hierarchy
			// so that whenever BP's hierarchy is modified, CDO's hierarchy is kept in sync
			// Other instances of Control Rig can make use of CDO to reset to the initial state
			ControlRigBlueprint->Hierarchy->ClearListeningHierarchy();
			ControlRigBlueprint->Hierarchy->RegisterListeningHierarchy(ControlRig->GetHierarchy());
#endif
		}

		// notify clients that the hierarchy has changed
		ControlRig->GetHierarchy()->Notify(ERigHierarchyNotification::HierarchyReset, nullptr);

		ControlRig->DrawContainer = ControlRigBlueprint->DrawContainer;
		ControlRig->Influences = ControlRigBlueprint->Influences;
	}
}

void FControlRigBlueprintCompilerContext::EnsureProperGeneratedClass(UClass*& TargetUClass)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if( TargetUClass && !((UObject*)TargetUClass)->IsA(UControlRigBlueprintGeneratedClass::StaticClass()) )
	{
		FKismetCompilerUtilities::ConsignToOblivion(TargetUClass, Blueprint->bIsRegeneratingOnLoad);
		TargetUClass = nullptr;
	}
}

void FControlRigBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	NewControlRigBlueprintGeneratedClass = FindObject<UControlRigBlueprintGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);

	if (NewControlRigBlueprintGeneratedClass == nullptr)
	{
		NewControlRigBlueprintGeneratedClass = NewObject<UControlRigBlueprintGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		FBlueprintCompileReinstancer::Create(NewControlRigBlueprintGeneratedClass);
	}
	NewClass = NewControlRigBlueprintGeneratedClass;
}

void FControlRigBlueprintCompilerContext::OnNewClassSet(UBlueprintGeneratedClass* ClassToUse)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	NewControlRigBlueprintGeneratedClass = CastChecked<UControlRigBlueprintGeneratedClass>(ClassToUse);
}

void FControlRigBlueprintCompilerContext::CleanAndSanitizeClass(UBlueprintGeneratedClass* ClassToClean, UObject*& InOldCDO)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FKismetCompilerContext::CleanAndSanitizeClass(ClassToClean, InOldCDO);

	// Make sure our typed pointer is set
	check(ClassToClean == NewClass && NewControlRigBlueprintGeneratedClass == NewClass);
}

void FControlRigBlueprintCompilerContext::PreCompileUpdateBlueprintOnLoad(UBlueprint* BP)
{
	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(BP))
	{
		RigBlueprint->CreateMemberVariablesOnLoad();
	}
}


#undef LOCTEXT_NAMESPACE
