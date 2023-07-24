// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPCameraBase.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Editor/EditorEngine.h"
#include "IVREditorModule.h"
#include "Misc/App.h"
#include "ScopedTransaction.h"
#include "SLevelViewport.h"
#include "VREditorMode.h"
#include "UObject/ObjectSaveContext.h"
#endif

#define LOCTEXT_NAMESPACE "VPCameraUIBase"

AVPCameraBase::AVPCameraBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetActorTickEnabled(true);

	CurrentInstanceId = FApp::GetInstanceId()/*.ToString(EGuidFormats::Digits)*/;
}

void AVPCameraBase::ResetPreview()
{
#if WITH_EDITOR
	const FScopedTransaction Transaction(LOCTEXT("TransactionRemoveUser", "RemoveUser"));
	Modify();
	SelectedByUsers.Reset();
	RemovePreview();
#endif
}

#if WITH_EDITOR
bool AVPCameraBase::ShouldTickIfViewportsOnly() const
{
	return true;
}

void AVPCameraBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bIsInSelectionList = SelectedByUsers.Contains(CurrentInstanceId);
	bool bAddToSelection = false;
	bool bRemoveToSelection = false;

	if (IsSelectedInEditor())
	{
		bAddToSelection = !bIsInSelectionList;
	}
	else
	{
		bRemoveToSelection = bIsInSelectionList;
	}

	if (bAddToSelection)
	{
		AddUser();
	}
	else if (bRemoveToSelection)
	{
		RemoveUser(false);
	}

	if (SelectedByUsers.Num() > 0)
	{
		AddPreview();
	}
	else
	{
		RemovePreview();
	}
}

void AVPCameraBase::BeginDestroy()
{
	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		RemoveUser(true);
	}
	Super::BeginDestroy();
}

void AVPCameraBase::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void AVPCameraBase::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	RemoveUser(true);
	Super::PreSave(ObjectSaveContext);
}

void AVPCameraBase::AddPreview()
{
	if (IVREditorModule::IsAvailable())
	{
		if (UVREditorMode* VRMode = IVREditorModule::Get().GetVRMode())
		{
			VRMode->GetLevelViewportPossessedForVR().SetActorAlwaysPreview(this, true);
		}
	}
}

void AVPCameraBase::RemovePreview()
{
	if (IVREditorModule::IsAvailable())
	{
		if (UVREditorMode* VRMode = IVREditorModule::Get().GetVRMode())
		{
			VRMode->GetLevelViewportPossessedForVR().SetActorAlwaysPreview(this, false);
		}
	}
}

void AVPCameraBase::AddUser()
{
	{
		const FScopedTransaction Transaction(LOCTEXT("TransactionAddUser", "AddUser"));
		Modify();
		SelectedByUsers.Add(CurrentInstanceId);
		SelectedByUsers.Sort();
	}
	AddPreview();
}

void AVPCameraBase::RemoveUser(bool bForceRemovePreview)
{
	if (SelectedByUsers.Contains(CurrentInstanceId))
	{
		const FScopedTransaction Transaction(LOCTEXT("TransactionRemoveUser", "RemoveUser"));
		Modify();
		SelectedByUsers.RemoveSingle(CurrentInstanceId);
		if (SelectedByUsers.Num() == 0)
		{
			bForceRemovePreview = true;
		}
	}

	if (bForceRemovePreview)
	{
		RemovePreview();
	}
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE