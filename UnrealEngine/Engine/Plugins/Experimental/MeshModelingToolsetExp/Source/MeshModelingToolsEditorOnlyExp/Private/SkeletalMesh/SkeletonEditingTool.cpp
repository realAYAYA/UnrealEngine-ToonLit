// Copyright Epic Games, Inc. All Rights Reserved.


#include "SkeletalMesh/SkeletonEditingTool.h"

#include "ContextObjectStore.h"
#include "InteractiveToolManager.h"
#include "ModelingToolTargetUtil.h"
#include "SkeletalDebugRendering.h"
#include "ToolTargetManager.h"
#include "BaseBehaviors/SingleClickBehavior.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealClient.h"
#include "HitProxies.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseGizmos/GizmoViewContext.h"

#include "Algo/Count.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "SkeletalMesh/SkeletonTransformProxy.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"

#define LOCTEXT_NAMESPACE "USkeletonEditingTool"

namespace SkeletonEditingTool
{
	
FRefSkeletonChange::FRefSkeletonChange(const USkeletonEditingTool* InTool)
	: FToolCommandChange()
	, PreChangeSkeleton(InTool->Modifier->GetReferenceSkeleton())
	, PreBoneTracker(InTool->Modifier->GetBoneIndexTracker())
	, PostChangeSkeleton(InTool->Modifier->GetReferenceSkeleton())
	, PostBoneTracker(InTool->Modifier->GetBoneIndexTracker())
{}

void FRefSkeletonChange::StoreSkeleton(const USkeletonEditingTool* InTool)
{
	PostChangeSkeleton = InTool->Modifier->GetReferenceSkeleton();
	PostBoneTracker = InTool->Modifier->GetBoneIndexTracker();
}

void FRefSkeletonChange::Apply(UObject* Object)
{ // redo
	USkeletonEditingTool* Tool = CastChecked<USkeletonEditingTool>(Object);
	Tool->Modifier->ExternalUpdate(PostChangeSkeleton, PostBoneTracker);
	Tool->UpdateGizmo();
}

void FRefSkeletonChange::Revert(UObject* Object)
{ // undo
	USkeletonEditingTool* Tool = CastChecked<USkeletonEditingTool>(Object);
	Tool->Modifier->ExternalUpdate(PreChangeSkeleton, PreBoneTracker);
	Tool->UpdateGizmo();
}

}

/**
 * USkeletonEditingToolBuilder
 */

bool USkeletonEditingToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1;
}

const FToolTargetTypeRequirements& USkeletonEditingToolBuilder::GetTargetRequirements() const
{
	static const FToolTargetTypeRequirements TypeRequirements(UPrimitiveComponentBackedTarget::StaticClass());
	return TypeRequirements;
}

UInteractiveTool* USkeletonEditingToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	USkeletonEditingTool* NewTool = NewObject<USkeletonEditingTool>(SceneState.ToolManager);
	
	UToolTarget* Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);
	NewTool->Init(SceneState);
	
	return NewTool;
}

/**
 * USkeletonEditingTool
 */

void USkeletonEditingTool::Init(const FToolBuilderState& InSceneState)
{
	TargetWorld = InSceneState.World;

	const UContextObjectStore* ContextObjectStore = InSceneState.ToolManager->GetContextObjectStore();
	ViewContext = ContextObjectStore->FindContext<UGizmoViewContext>();
	GizmoContext = ContextObjectStore->FindContext<USkeletalMeshGizmoContextObjectBase>();
	EditorContext = ContextObjectStore->FindContext<USkeletalMeshEditorContextObjectBase>();
}

void USkeletonEditingTool::Setup()
{
	Super::Setup();

	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	USkeletalMeshComponent* Component = Cast<USkeletalMeshComponent>(TargetComponent->GetOwnerComponent());

	USkeletalMesh* SkeletalMesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalMesh)
	{
		return;
	}

	// setup modifier
	Modifier = NewObject<USkeletonModifier>(this);
	Modifier->SetSkeletalMesh(SkeletalMesh);

	// setup current bone
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
	const int32 NumBones = RefSkeleton.GetNum();
	const FName& RootBoneName = NumBones ? RefSkeleton.GetBoneName(0) : NAME_None;

	if (NumBones)
	{
		Selection = {RootBoneName};
	}

	// setup preview
	{
		PreviewMesh = NewObject<UPreviewMesh>(this);
		PreviewMesh->bBuildSpatialDataStructure = true;
		PreviewMesh->CreateInWorld(TargetWorld.Get(), FTransform::Identity);

		PreviewMesh->SetTransform(UE::ToolTarget::GetLocalToWorldTransform(Target));

		PreviewMesh->ReplaceMesh(UE::ToolTarget::GetDynamicMeshCopy(Target));

		const FComponentMaterialSet MaterialSet = UE::ToolTarget::GetMaterialSet(Target);
		PreviewMesh->SetMaterials(MaterialSet.Materials);

		// hide the skeletal mesh component
		UE::ToolTarget::HideSourceObject(Target);
	}

	ToolPropertyObjects.Add(this);
	
	// setup properties
	{
		ProjectionProperties = NewObject<UProjectionProperties>();
		ProjectionProperties->Initialize(this, PreviewMesh);
		ProjectionProperties->RestoreProperties(this);
		AddToolPropertySource(ProjectionProperties);

		MirroringProperties = NewObject<UMirroringProperties>();
		MirroringProperties->Initialize(this);
		MirroringProperties->RestoreProperties(this);
		AddToolPropertySource(MirroringProperties);

		OrientingProperties = NewObject<UOrientingProperties>();
		OrientingProperties->Initialize(this);
		OrientingProperties->RestoreProperties(this);
		AddToolPropertySource(OrientingProperties);

		Properties = NewObject<USkeletonEditingProperties>();
		Properties->Initialize(this);
		Properties->Name = GetCurrentBone();
		Properties->RestoreProperties(this);
		AddToolPropertySource(Properties);
	}

	// setup drag & drop behaviour
	{
		UClickDragInputBehavior* ClickDragBehavior = NewObject<UClickDragInputBehavior>(this);
		ClickDragBehavior->Initialize(this);
		ClickDragBehavior->Modifiers.RegisterModifier(AddToSelectionModifier, FInputDeviceState::IsShiftKeyDown);
		ClickDragBehavior->Modifiers.RegisterModifier(ToggleSelectionModifier, FInputDeviceState::IsCtrlKeyDown);
		AddInputBehavior(ClickDragBehavior);
	}

	// setup gizmo
	if (GizmoContext.IsValid())
	{
		UGizmoLambdaStateTarget* NewTarget = NewObject<UGizmoLambdaStateTarget>(this);
		NewTarget->BeginUpdateFunction = [this]()
		{
			if (GizmoWrapper && GizmoWrapper->CanInteract())
			{
				TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
				BeginChange();
			}
		};
		NewTarget->EndUpdateFunction = [this]()
		{
			if (GizmoWrapper && GizmoWrapper->CanInteract())
			{
				TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
				EndChange();

				const IToolsContextQueriesAPI* ToolsContextQueries = GetToolManager()->GetPairedGizmoManager()->GetContextQueriesAPI();
				check(ToolsContextQueries);
				if (ToolsContextQueries->GetCurrentCoordinateSystem() == EToolContextCoordinateSystem::World)
				{
					UpdateGizmo();
				}
			}
		};
		
		GizmoWrapper = GizmoContext->GetNewWrapper(GetToolManager(), this, NewTarget);
		if (GizmoWrapper)
		{
			GizmoWrapper->Component = Component;
		}
	}

	// setup watchers
	SelectionWatcher.Initialize(
	[this]()
	{
		return this->Selection;
	},
	[this](TArray<FName> InBoneNames)
	{
		UpdateGizmo();
		if (NeedsNotification())
		{
			GetNotifier().Notify(InBoneNames, ESkeletalMeshNotifyType::BonesSelected);
		}
	},
	this->Selection);

	IToolsContextQueriesAPI* ToolsContextQueries = GetToolManager()->GetPairedGizmoManager()->GetContextQueriesAPI();
	CoordinateSystemWatcher.Initialize(
	[ToolsContextQueries]()
	{
		check(ToolsContextQueries);
		return ToolsContextQueries->GetCurrentCoordinateSystem();
	},
	[this](EToolContextCoordinateSystem)
	{
		UpdateGizmo();
	},
	ToolsContextQueries->GetCurrentCoordinateSystem());

	if (EditorContext.IsValid())
	{
		EditorContext->HideSkeleton();
		EditorContext->BindTo(this);
	}
}

void USkeletonEditingTool::UpdateGizmo() const
{	
	if (!GizmoWrapper)
	{
		return;
	}

	GizmoWrapper->Initialize();

	const bool bUseGizmo = Operation == EEditingOperation::Select || Operation == EEditingOperation::Transform; 
	if (Selection.IsEmpty() || !bUseGizmo)
	{
		return;
	}

	const IToolsContextQueriesAPI* ToolsContextQueries = GetToolManager()->GetPairedGizmoManager()->GetContextQueriesAPI();
	const bool bWorld = ToolsContextQueries->GetCurrentCoordinateSystem() == EToolContextCoordinateSystem::World;
	
	const TArray<int32> BoneIndexes = GetSelectedBoneIndexes();
	FTransform InitTransform = Modifier->GetTransform(BoneIndexes.Last(), true);
	if (bWorld)
	{
		InitTransform = FTransform(FQuat::Identity, InitTransform.GetTranslation(), InitTransform.GetScale3D());
	}
	GizmoWrapper->Initialize(InitTransform, ToolsContextQueries->GetCurrentCoordinateSystem());

	const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
	const TArray<FMeshBoneInfo>& BoneInfos = ReferenceSkeleton.GetRawRefBoneInfo();
	for (const int32 BoneIndex: BoneIndexes)
	{
		if (BoneInfos.IsValidIndex(BoneIndex))
		{
			const FName BoneName = BoneInfos[BoneIndex].Name;
			const int32 ParentBoneIndex = BoneInfos[BoneIndex].ParentIndex;
			const FTransform& ParentGlobal = Modifier->GetTransform(ParentBoneIndex, true);
		
			GizmoWrapper->HandleBoneTransform(
				[this, BoneIndex, bWorld]()
				{
					return Modifier->GetTransform(BoneIndex, bWorld);
				},
				[this, BoneName, ParentGlobal, bWorld](const FTransform& InNewTransform)
				{
					if (bWorld)
					{
						const FTransform NewLocal = InNewTransform.GetRelativeTransform(ParentGlobal);
						return Modifier->SetBoneTransform(BoneName, NewLocal, Properties->bUpdateChildren);
					}
					return Modifier->SetBoneTransform(BoneName, InNewTransform, Properties->bUpdateChildren);
				});
		}
	}
}


void USkeletonEditingTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (ShutdownType == EToolShutdownType::Accept)
	{
		GetToolManager()->BeginUndoTransaction(LOCTEXT("SkeletonEditingTool", "Commit Skeleton Editing"));
		Modifier->CommitSkeletonToSkeletalMesh();
		GetToolManager()->EndUndoTransaction();

		// to force to refresh the tree
		if (NeedsNotification())
		{
			GetNotifier().Notify({}, ESkeletalMeshNotifyType::BonesAdded);
		}
	}
	
	Super::Shutdown(ShutdownType);

	// remove preview mesh
	if (PreviewMesh != nullptr)
	{
		PreviewMesh->SetVisible(false);
		PreviewMesh->Disconnect();
		PreviewMesh = nullptr;
	}

	// show the skeletal mesh component
	UE::ToolTarget::ShowSourceObject(Target);

	// save properties	
	Properties->SaveProperties(this);
	ProjectionProperties->SaveProperties(this);
	MirroringProperties->SaveProperties(this);
	OrientingProperties->SaveProperties(this);

	// clear gizmo
	if (GizmoWrapper)
	{
		GizmoWrapper->Clear();
	}
	
	if (EditorContext.IsValid())
	{
		EditorContext->ShowSkeleton();
		EditorContext->UnbindFrom(this);
	}
}

void USkeletonEditingTool::RegisterActions(FInteractiveToolActionSet& ActionSet)
{
	Super::RegisterActions(ActionSet);

	int32 ActionId = static_cast<int32>(EStandardToolActions::BaseClientDefinedActionID) + 400;
	auto GetActionId = [&ActionId]
	{
		return ActionId++;
	};
	
	// register New key
	ActionSet.RegisterAction(this, GetActionId(), TEXT("CreateNewBone"),
		LOCTEXT("CreateNewBone", "Create New Bone"),
		LOCTEXT("CreateNewBoneDesc", "Create New Bone"),
		EModifierKey::None, EKeys::N,
		[this]()
		{
			Operation = EEditingOperation::Create;
			UpdateGizmo();
			GetToolManager()->DisplayMessage(LOCTEXT("Create", "Click & Drag to place a new bone."), EToolMessageLevel::UserNotification);
		});
	
	// register Delete key
	ActionSet.RegisterAction(this, GetActionId(), TEXT("DeleteSelectedBones"),
		LOCTEXT("DeleteSelectedBones", "Delete Selected Bone(s)"),
		LOCTEXT("DeleteSelectedBonesDesc", "Delete Selected Bone(s)"),
		EModifierKey::None, EKeys::Delete,
		[this]()
		{
			RemoveBones();
		});

	// register Select key
	ActionSet.RegisterAction(this, GetActionId(), TEXT("SelectBones"),
		LOCTEXT("SelectBone", "Select Bone"),
		LOCTEXT("SelectDesc", "Select Bone"),
		EModifierKey::None, EKeys::Escape,
		[this]()
		{
			if (Operation != EEditingOperation::Select)
			{
				Operation = EEditingOperation::Select;
				UpdateGizmo();
				GetToolManager()->DisplayMessage(LOCTEXT("Select", "Click on a bone to select it."), EToolMessageLevel::UserNotification);
			}
		});

	// register UnParent key
	ActionSet.RegisterAction(this, GetActionId(), TEXT("UnparentBones"),
		LOCTEXT("UnparentBones", "Unparent Bones"),
		LOCTEXT("UnparentBonesDesc", "Unparent Bones"),
		EModifierKey::Shift, EKeys::P,
		[this]()
		{
			UnParentBones();
		});
		
	// register Parent key
	ActionSet.RegisterAction(this, GetActionId(), TEXT("ParentBones"),
		LOCTEXT("ParentBones", "Parent Bones"),
		LOCTEXT("ParentBonesDesc", "Parent Bones"),
		EModifierKey::None, EKeys::B, // FIXME find another shortcut
		[this]()
		{
			Operation = EEditingOperation::Parent;
			UpdateGizmo();
			GetToolManager()->DisplayMessage(LOCTEXT("Parent", "Click on a bone to be set as the new parent."), EToolMessageLevel::UserNotification);
		});
}

void USkeletonEditingTool::CreateNewBone()
{
	if (Operation != EEditingOperation::Create)
	{
		return;
	}

	BeginChange();

	static const FName DefaultName("joint");
	const FName BoneName = Modifier->GetUniqueName(DefaultName);
	const FName ParentName = Selection.IsEmpty() ? NAME_None : Selection.Last(); 
	const bool bBoneAdded = Modifier->AddBone(BoneName, ParentName, Properties->Transform);
	if (bBoneAdded)
	{
		if (NeedsNotification())
		{
			GetNotifier().Notify({BoneName}, ESkeletalMeshNotifyType::BonesAdded);
		}
	
		Selection = {BoneName};
		Properties->Name = BoneName;

		EndChange();
		return;
	}

	CancelChange();
}

void USkeletonEditingTool::MirrorBones()
{
	TGuardValue OperationGuard(Operation, EEditingOperation::Mirror);
	BeginChange();

	const bool bBonesMirrored = Modifier->MirrorBones(GetSelectedBones(), MirroringProperties->Options);
	if (bBonesMirrored)
	{
		if (NeedsNotification())
		{
			GetNotifier().Notify({}, ESkeletalMeshNotifyType::HierarchyChanged);
		}
	
		EndChange();
		return;		
	}
	
	CancelChange();
}

void USkeletonEditingTool::RemoveBones()
{
	const TArray<FName> BonesToRemove = GetSelectedBones();
	
	TGuardValue OperationGuard(Operation, EEditingOperation::Remove);
	BeginChange();

	const bool bBonesRemoved = Modifier->RemoveBones(BonesToRemove, true);
	if (bBonesRemoved)
	{
		Selection.RemoveAll([&](const FName& BoneName)
		{
			return BonesToRemove.Contains(BoneName);
		});
		
		if (NeedsNotification())
		{
			GetNotifier().Notify(BonesToRemove, ESkeletalMeshNotifyType::BonesRemoved);
		}
		
		EndChange();
		return;
	}

	CancelChange();
}

void USkeletonEditingTool::UnParentBones()
{
	static const TArray<FName> Dummy;

	TGuardValue OperationGuard(Operation, EEditingOperation::Parent);
	BeginChange();

	const bool bBonesUnParented = Modifier->ParentBones(GetSelectedBones(), Dummy);
	if (bBonesUnParented)
	{
		GetToolManager()->DisplayMessage(LOCTEXT("Unparent", "Selected bones have been unparented."), EToolMessageLevel::UserNotification);

		if (NeedsNotification())
		{
			GetNotifier().Notify({}, ESkeletalMeshNotifyType::HierarchyChanged);
		}
		
		EndChange();
		return;
	}
	
	CancelChange();
}

void USkeletonEditingTool::ParentBones(const FName& InParentName)
{
	if (Operation != EEditingOperation::Parent)
	{
		return;
	}

	BeginChange();
	const bool bBonesParented = Modifier->ParentBones(GetSelectedBones(), {InParentName});
	if (bBonesParented)
	{
		if (NeedsNotification())
		{
			GetNotifier().Notify({}, ESkeletalMeshNotifyType::HierarchyChanged);
		}
		
		EndChange();
	}
	else
	{
		CancelChange();
	}
	Operation = EEditingOperation::Select;
}

void USkeletonEditingTool::MoveBones()
{
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();

	const TArray<FName> Bones = GetSelectedBones();
	const bool bHasValidBone = Bones.ContainsByPredicate([&](const FName& InBoneName)
	{
		return RefSkeleton.FindRawBoneIndex(InBoneName) > INDEX_NONE;
	});

	if (!bHasValidBone)
	{
		return;
	}

	TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
	BeginChange();

	const bool bBonesMoved = Modifier->SetBoneTransform(Bones[0], Properties->Transform, Properties->bUpdateChildren);
	if (bBonesMoved)
	{
		// if (NeedsNotification())
		// {
		// 	GetNotifier().Notify(Bones, ESkeletalMeshNotifyType::BonesMoved);
		// }
		
		EndChange();
		return;
	}

	CancelChange();
}

void USkeletonEditingTool::RenameBones()
{
	const TArray<int32> BoneIndices = GetSelectedBoneIndexes();
	if (BoneIndices.IsEmpty() || Properties->Name == NAME_None)
	{
		return;
	}
	
	const FName CurrentBone = GetCurrentBone();
	if (BoneIndices.Num() == 1 && CurrentBone == Properties->Name)
	{
		return;
	}

	TGuardValue OperationGuard(Operation, EEditingOperation::Rename);
	BeginChange();

	TArray<FName> ReversedSelection = Selection;
	Algo::Reverse(ReversedSelection);

	TArray<FName> NewNames;
	NewNames.Init(Properties->Name, Selection.Num());

	const bool bBoneRenamed = Modifier->RenameBones(ReversedSelection, NewNames);
	if (bBoneRenamed)
	{
		// update selection with new names
		const TArray<FMeshBoneInfo>& BoneInfos = Modifier->GetReferenceSkeleton().GetRawRefBoneInfo();
		for (int32 Index = 0; Index < BoneIndices.Num(); Index++)
		{
			if (BoneIndices[Index] != INDEX_NONE)
			{
				Selection[Index] = BoneInfos[BoneIndices[Index]].Name;
			}
		}

		const FName NewName = GetCurrentBone();
		Properties->Name = NewName;
		
		if (NeedsNotification())
		{
			GetNotifier().Notify( {NewName}, ESkeletalMeshNotifyType::BonesRenamed);
		}
		
		EndChange();
		return;
	}

	CancelChange();
}

void USkeletonEditingTool::OnClickPress(const FInputDeviceRay& InPressPos)
{
	if (PendingFunction)
	{
		PendingFunction();
		PendingFunction.Reset();
	}
	BeginChange();
}

void USkeletonEditingTool::OnClickDrag(const FInputDeviceRay& InDragPos)
{
	if (Operation != EEditingOperation::Create)
	{
		return;
	}
	
	FVector HitPoint;
	if (ProjectionProperties->GetProjectionPoint(InDragPos, HitPoint))
	{
		const FTransform& ParentGlobal = Modifier->GetTransform(ParentIndex, true);
		Properties->Transform.SetLocation(ParentGlobal.InverseTransformPosition(HitPoint));

		if (!ActiveChange)
		{
			TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
			BeginChange();
		}

		const bool bBoneMoved = Modifier->SetBoneTransform(GetCurrentBone(), Properties->Transform, Properties->bUpdateChildren);
		if (!bBoneMoved)
		{
			CancelChange();
			return;
		}

		const bool bOrient = Operation == EEditingOperation::Create && OrientingProperties->bAutoOrient;
		if (bOrient && ParentIndex != INDEX_NONE)
		{
			const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
			const FName ParentName = RefSkeleton.GetRawRefBoneInfo()[ParentIndex].Name;
			Modifier->OrientBone(ParentName, OrientingProperties->Options);
		}
	}
}

void USkeletonEditingTool::OrientBones()
{
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();

	const TArray<FName> Bones = GetSelectedBones();
	const bool bHasValidBone = Bones.ContainsByPredicate([&](const FName& InBoneName)
	{
		return RefSkeleton.FindRawBoneIndex(InBoneName) > INDEX_NONE;
	});

	if (!bHasValidBone)
	{
		return;
	}

	TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
	BeginChange();
	
	const bool bBoneOriented = Modifier->OrientBones(Bones, OrientingProperties->Options);
	if (bBoneOriented)
	{
		UpdateGizmo();
		// if (NeedsNotification())
		// {
		// 	GetNotifier().Notify(Bones, ESkeletalMeshNotifyType::BonesMoved);
		// }
		
		EndChange();
		return;
	}

	CancelChange();
}

void USkeletonEditingTool::OnUpdateModifierState(int InModifierID, bool bIsOn)
{
	auto UpdateFlag = [this, bIsOn](EBoneSelectionMode InFlag)
	{
		if (bIsOn)
		{
			EnumAddFlags(SelectionMode, InFlag);
		}
		else
		{
			EnumRemoveFlags(SelectionMode, InFlag);
		}		
	};
	
	if (InModifierID == AddToSelectionModifier)
	{
		UpdateFlag(EBoneSelectionMode::Additive);
	}
	else if (InModifierID == ToggleSelectionModifier)
	{
		UpdateFlag(EBoneSelectionMode::Toggle);
	}
}

EEditingOperation USkeletonEditingTool::GetOperation() const
{
	return Operation;	
}

void USkeletonEditingTool::SetOperation(const EEditingOperation InOperation, const bool bUpdateGizmo)
{
	Operation = InOperation;

	if (bUpdateGizmo)
	{
		UpdateGizmo();
	}
}

void USkeletonEditingTool::SelectBone(const FName& InBoneName)
{
	TArray<FName> NewSelection = Selection;

	if (EnumHasAnyFlags(SelectionMode, EBoneSelectionMode::Additive))
	{
		if (InBoneName != NAME_None)
		{
			NewSelection.AddUnique(InBoneName);
		}		
	}
	else if (EnumHasAnyFlags(SelectionMode, EBoneSelectionMode::Toggle))
	{
		const bool bSelected = NewSelection.Contains(InBoneName);
		if (bSelected)
		{
			NewSelection.Remove(InBoneName);
		}
		else
		{
			NewSelection.Add(InBoneName);
		}
	}
	else
	{
		NewSelection.Empty();
		if (InBoneName != NAME_None)
		{
			NewSelection.Add(InBoneName);
		}
	}

	Selection = MoveTemp(NewSelection);
	NormalizeSelection();
}

void USkeletonEditingTool::NormalizeSelection()
{
	if (Selection.Num() < 2)
	{
		return;
	}
	
	const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();

	// ensure they are known and unique
	TArray<int32> Indexes;
	for (const FName& BoneName: Selection)
	{
		const int32 BoneIndex = ReferenceSkeleton.FindRawBoneIndex(BoneName);
		if (BoneIndex != INDEX_NONE)
		{
			Indexes.AddUnique(BoneIndex);
		}
	}

	if (Indexes.IsEmpty())
	{
		Selection.Reset();
		return;
	}

	// sort them so that parents are placed after one of their children 
	const TArray<FMeshBoneInfo>& BoneInfos = ReferenceSkeleton.GetRawRefBoneInfo();
	Indexes.StableSort([&](const int32 Index0, const int32 Index1)
	{
		int32 P0 = BoneInfos[Index0].ParentIndex;
		while (P0 != INDEX_NONE)
		{
			if (P0 == Index1)
			{ // Index1 is a parent
				return true;
			}
			if (Indexes.Contains(P0))
			{ // parent is selected
				return true;
			}
			P0 = BoneInfos[P0].ParentIndex;
		}
		return false;
	});

	// transform back to names
	Selection.Reset();
	Algo::Transform(Indexes, Selection, [&](const int32 Index)
	{
		return BoneInfos[Index].Name;
	});	
}

void USkeletonEditingTool::OnClickRelease(const FInputDeviceRay& InReleasePos)
{
	TGuardValue OperationGuard(Operation, EEditingOperation::Transform);
	EndChange();
}

void USkeletonEditingTool::OnTerminateDragSequence()
{}

void USkeletonEditingTool::OnTick(float DeltaTime)
{
	if (PendingFunction)
	{
		PendingFunction();
		PendingFunction.Reset();
	}
	
	SelectionWatcher.CheckAndUpdate();
	CoordinateSystemWatcher.CheckAndUpdate();
}

FInputRayHit USkeletonEditingTool::CanBeginClickDragSequence(const FInputDeviceRay& InPressPos)
{
	PendingFunction.Reset();
	ParentIndex = INDEX_NONE;
	
	FViewport* Viewport = GetToolManager()->GetContextQueriesAPI()->GetFocusedViewport();
	if (!Viewport)
	{
		return FInputRayHit();
	}
	
	if (GizmoWrapper && GizmoWrapper->IsGizmoHit(InPressPos))
	{
		return FInputRayHit();
	}
	
	auto PickBone = [&]() -> int32
	{
		if (HHitProxy* HitProxy = Viewport->GetHitProxy(InPressPos.ScreenPosition.X, InPressPos.ScreenPosition.Y))
		{
			if (TOptional<FName> OptBoneName = GetBoneName(HitProxy))
			{
				const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
				return ReferenceSkeleton.FindRawBoneIndex(*OptBoneName);
			}
		}
		return INDEX_NONE;
	};
	
	// pick bone in viewport
	const int32 BoneIndex = PickBone();

	// update projection properties
	const FVector GlobalPosition = Modifier->GetTransform(BoneIndex, true).GetTranslation();
	ProjectionProperties->UpdatePlane(*ViewContext, GlobalPosition);
	GetToolManager()->GetContextQueriesAPI()->GetCurrentViewState(ProjectionProperties->CameraState);
	
	// if we picked a new bone
	if (BoneIndex > INDEX_NONE)
	{
		// parent selection without changing the selection
		if (Operation == EEditingOperation::Parent)
		{
			const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
			ParentBones(ReferenceSkeleton.GetBoneName(BoneIndex));
			return FInputRayHit();
		}
		
		// otherwise, update current selection
		PendingFunction = [this, BoneIndex]
		{
			const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
			SelectBone(ReferenceSkeleton.GetBoneName(BoneIndex));

			Properties->Name = GetCurrentBone();
			Properties->Transform = ReferenceSkeleton.GetRefBonePose()[BoneIndex];
			ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndex);
		};
	
		return FInputRayHit(0.0);
	}

	// if we didn't pick anything
	if (Operation == EEditingOperation::Select)
	{
		Selection.Empty();
		Properties->Name = GetCurrentBone();
		return FInputRayHit();
	}

	// if we're in creation mode then create a new bone
	if (Operation == EEditingOperation::Create)
	{
		FVector HitPoint;
		if (ProjectionProperties->GetProjectionPoint(InPressPos, HitPoint))
		{
			// CurrentBone is gonna be the parent
			PendingFunction = [this, HitPoint]
			{
				const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
				ParentIndex = ReferenceSkeleton.FindRawBoneIndex(GetCurrentBone());
				const FTransform& ParentGlobalTransform = Modifier->GetTransform(ParentIndex, true);

				// Create the new bone under mouse
				Properties->Transform.SetLocation(ParentGlobalTransform.InverseTransformPosition(HitPoint));
				CreateNewBone();
			};
			
			return FInputRayHit(0.0);
		}
	}
	
	return FInputRayHit();
}

TWeakObjectPtr<USkeletonModifier> USkeletonEditingTool::GetModifier() const
{
	return Modifier;
}

void USkeletonEditingTool::HandleSkeletalMeshModified(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType)
{
	if (GetNotifier().Notifying())
	{
		return;
	}

	TArray<FName> BoneNames(InBoneNames);
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
	BoneNames.RemoveAll([&](const FName& BoneName)
	{
		return RefSkeleton.FindRawBoneIndex(BoneName) == INDEX_NONE;
	});

	switch (InNotifyType)
	{
		case ESkeletalMeshNotifyType::BonesAdded:
			Selection = BoneNames;
			break;
		case ESkeletalMeshNotifyType::BonesRemoved:
			Selection.RemoveAll([&](const FName& BoneName)
			{
				return BoneNames.Contains(BoneName);
			});
			break;
		case ESkeletalMeshNotifyType::BonesMoved:
			break;
		case ESkeletalMeshNotifyType::BonesSelected:
			Selection.Reset();
			Selection = BoneNames;
			break;
		case ESkeletalMeshNotifyType::BonesRenamed:
			Selection = BoneNames;
			break;
		case ESkeletalMeshNotifyType::HierarchyChanged:
			break;
		default:
			break;
	}

	NormalizeSelection();
	
	Properties->Name = GetCurrentBone();
}

void USkeletonEditingTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	// FIXME many things could be caches here and updated lazilly
	if (!Target)
	{
		return;
	}

	static const FLinearColor DefaultBoneColor(0.0f,0.0f,0.025f,1.0f);
	static const FLinearColor SelectedBoneColor(0.2f,1.0f,0.2f,1.0f);
	static const FLinearColor AffectedBoneColor(1.0f,1.0f,1.0f,1.0f);
	static const FLinearColor ParentOfSelectedBoneColor(0.85f,0.45f,0.12f,1.0f);
	static FSkelDebugDrawConfig DrawConfig;
		DrawConfig.BoneDrawMode = EBoneDrawMode::Type::All;
		DrawConfig.BoneDrawSize = 1.f;
		DrawConfig.bAddHitProxy = true;
		DrawConfig.bForceDraw = false;
		DrawConfig.DefaultBoneColor = DefaultBoneColor;
		DrawConfig.AffectedBoneColor = AffectedBoneColor;
		DrawConfig.SelectedBoneColor = SelectedBoneColor;
		DrawConfig.ParentOfSelectedBoneColor = ParentOfSelectedBoneColor;
		DrawConfig.AxisConfig.Thickness = Properties->AxisThickness;
		DrawConfig.AxisConfig.Length = Properties->AxisLength;
	
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	const IPrimitiveComponentBackedTarget* TargetComponent = Cast<IPrimitiveComponentBackedTarget>(Target);
	const FTransform ComponentTransform = TargetComponent->GetWorldTransform();
	
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();

	const int32 NumBones = RefSkeleton.GetRawBoneNum();
	TArray<TRefCountPtr<HHitProxy>> HitProxies; HitProxies.Reserve(NumBones);
	TArray<FBoneIndexType> RequiredBones; RequiredBones.AddUninitialized(NumBones);
	TArray<FTransform> WorldTransforms; WorldTransforms.AddUninitialized(NumBones);
	TArray<FLinearColor> BoneColors; BoneColors.AddUninitialized(NumBones);
	
	for (int32 Index = 0; Index < NumBones; ++Index)
	{
		const FTransform& BoneTransform = Modifier->GetTransform(Index, true);
		WorldTransforms[Index] = BoneTransform;
		RequiredBones[Index] = Index;
		BoneColors[Index] = DefaultBoneColor;
		HitProxies.Add(new HBoneHitProxy(Index, RefSkeleton.GetBoneName(Index)));
	}

	SkeletalDebugRendering::DrawBones(
		PDI,
		ComponentTransform.GetLocation(),
		RequiredBones,
		RefSkeleton,
		WorldTransforms,
		GetSelectedBoneIndexes(),
		BoneColors,
		HitProxies,
		DrawConfig
	);
}

FBox USkeletonEditingTool::GetWorldSpaceFocusBox()
{
	const TArray<int32> BoneIndexes = GetSelectedBoneIndexes();
	if (!BoneIndexes.IsEmpty())
	{
		FBox Box(EForceInit::ForceInit);
		TSet<int32> AllChildren;

		const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
		for (const int32 BoneIndex: BoneIndexes)
		{
			Box += Modifier->GetTransform(BoneIndex, true).GetTranslation();

			// get direct children
			TArray<int32> Children;
			RefSkeleton.GetRawDirectChildBones(BoneIndex, Children);
			AllChildren.Append(Children);
		}

		for (const int32 ChildIndex: AllChildren)
		{
			Box += Modifier->GetTransform(ChildIndex, true).GetTranslation();
		}
		
		return Box;
	}

	if (PreviewMesh && PreviewMesh->GetActor())
	{
		return PreviewMesh->GetActor()->GetComponentsBoundingBox();
	}

	return USingleSelectionTool::GetWorldSpaceFocusBox();
}

TArray<FName> USkeletonEditingTool::GetSelectedBones() const
{
	return GetSelection();
}

const TArray<FName>& USkeletonEditingTool::GetSelection() const
{
	return Selection;
}

const FTransform& USkeletonEditingTool::GetTransform(const FName InBoneName, const bool bWorld) const
{
	if (Modifier)
	{
		const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
		return Modifier->GetTransform(RefSkeleton.FindRawBoneIndex(InBoneName), bWorld);
	}
	return FTransform::Identity;
}

void USkeletonEditingTool::SetTransforms(const TArray<FName>& InBones, const TArray<FTransform>& InTransforms, const bool bWorld) const
{
	const int32 NumBonesToMove = InBones.Num();
	if (NumBonesToMove == 0 || NumBonesToMove != InTransforms.Num())
	{
		return;
	}
	
	TArray<FTransform> NewTransforms = InTransforms;
	if (bWorld)
	{ // switch to local
		const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
		const TArray<FMeshBoneInfo>& BoneInfos = RefSkeleton.GetRawRefBoneInfo();
		
		for (int32 Index = 0; Index < InBones.Num(); ++Index)
		{
			const int32 BoneIndex = RefSkeleton.FindRawBoneIndex(InBones[Index]);
			check(BoneIndex != INDEX_NONE);

			const int32 ParentBoneIndex = BoneInfos[BoneIndex].ParentIndex;
			const int32 ParentToBeSet = ParentBoneIndex != INDEX_NONE ?
				InBones.IndexOfByKey(BoneInfos[ParentBoneIndex].Name) : INDEX_NONE;

			const FTransform& ParentGlobal = ParentToBeSet != INDEX_NONE ?
				InTransforms[ParentToBeSet] : Modifier->GetTransform(ParentBoneIndex, true);
			
			NewTransforms[Index] = NewTransforms[Index].GetRelativeTransform(ParentGlobal);
		}
	}

	const bool bBonesMoved = Modifier->SetBonesTransforms(InBones, NewTransforms, Properties->bUpdateChildren);
	if (bBonesMoved)
	{
		UpdateGizmo();
	}
}

FName USkeletonEditingTool::GetCurrentBone() const
{
	return Selection.IsEmpty() ? NAME_None : Selection.Last(); 
}

TArray<int32> USkeletonEditingTool::GetSelectedBoneIndexes() const
{
	TArray<int32> Indexes;
	
	const FReferenceSkeleton& ReferenceSkeleton = Modifier->GetReferenceSkeleton();
	Algo::Transform(Selection, Indexes, [&](const FName& BoneName)
	{
		return ReferenceSkeleton.FindRawBoneIndex(BoneName);
	});
	
	return Indexes;
}

void USkeletonEditingTool::BeginChange()
{
	if (Operation == EEditingOperation::Select)
	{
		return;
	}
	
	ensure( ActiveChange == nullptr );
	ActiveChange = MakeUnique<SkeletonEditingTool::FRefSkeletonChange>(this); 
}

void USkeletonEditingTool::EndChange()
{
	if (!ActiveChange.IsValid())
	{
		return;
	}

	if (Operation == EEditingOperation::Select)
	{
		return CancelChange();
	}
	
	ActiveChange->StoreSkeleton(this);

	static const UEnum* OperationEnum = StaticEnum<EEditingOperation>();
	const FName OperationName = OperationEnum->GetNameByValue(static_cast<int64>(Operation));
	const FText TransactionDesc = FText::Format(LOCTEXT("RefSkeletonChanged", "Skeleton Edit - {0}"), FText::FromName(OperationName));
	
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(TransactionDesc);
	ToolManager->EmitObjectChange(this, MoveTemp(ActiveChange), TransactionDesc);
	ToolManager->EndUndoTransaction();
}

void USkeletonEditingTool::CancelChange()
{
	ActiveChange.Reset();
}

void USkeletonEditingProperties::Initialize(USkeletonEditingTool* ParentToolIn)
{
	ParentTool = ParentToolIn;
}

#if WITH_EDITOR
void USkeletonEditingProperties::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(USkeletonEditingProperties, Name))
		{
			ParentTool->RenameBones();
		}
	}
}
#endif

void UMirroringProperties::Initialize(USkeletonEditingTool* ParentToolIn)
{
	ParentTool = ParentToolIn;
}

void UMirroringProperties::MirrorBones()
{
	ParentTool->MirrorBones();
}

void UOrientingProperties::Initialize(USkeletonEditingTool* ParentToolIn)
{
	ParentTool = ParentToolIn;
}

void UOrientingProperties::OrientBones()
{
	ParentTool->OrientBones();
}

#if WITH_EDITOR

void UOrientingProperties::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		auto CheckAxis = [&](const TEnumAsByte<EAxis::Type>& InRef, TEnumAsByte<EAxis::Type>& OutOther)
		{
			if (OutOther != InRef)
			{
				return;
			}

			switch (InRef)
			{
			case EAxis::X:
				OutOther = EAxis::Y;
				break;
			case EAxis::Y:
				OutOther = EAxis::Z;
				break;
			case EAxis::Z:
				OutOther = EAxis::X;
				break;
			default:
				break;
			}
		};
		
		const FName PropertyName = PropertyChangedEvent.GetPropertyName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FOrientOptions, Primary))
		{
			if (Options.Primary == EAxis::None)
			{
				Options.Primary = EAxis::X;
				Options.Secondary = EAxis::Y;
				return;
			}
			CheckAxis(Options.Primary, Options.Secondary);
			return;
		}
		
		if (PropertyName == GET_MEMBER_NAME_CHECKED(FOrientOptions, Secondary))
		{
			CheckAxis(Options.Secondary, Options.Primary);
			return;
		}
	}
}

#endif

void UProjectionProperties::Initialize(USkeletonEditingTool* ParentToolIn, TObjectPtr<UPreviewMesh> InPreviewMesh) 
{
	ParentTool = ParentToolIn;
	PreviewMesh = InPreviewMesh;
}

void UProjectionProperties::UpdatePlane(const UGizmoViewContext& InViewContext, const FVector& InOrigin)
{
	PlaneNormal = -InViewContext.GetViewDirection();
	PlaneOrigin = InOrigin;
}

bool UProjectionProperties::GetProjectionPoint(const FInputDeviceRay& InRay, FVector& OutHitPoint) const
{
	const FRay& WorldRay = InRay.WorldRay;

	if (PreviewMesh.IsValid())
	{
		if (ProjectionType == EProjectionType::OnMesh)
		{
			FHitResult Hit;
			if (PreviewMesh->FindRayIntersection(WorldRay, Hit))
			{
				OutHitPoint = WorldRay.Origin + WorldRay.Direction * Hit.Distance;
				return true;
			}
		}

		if (ProjectionType == EProjectionType::WithinMesh)
		{
			using namespace UE::Geometry;

			if (FDynamicMeshAABBTree3* MeshAABBTree = PreviewMesh->GetSpatial())
			{
				using HitResult = MeshIntersection::FHitIntersectionResult;
				TArray<HitResult> Hits;
				
				if (MeshAABBTree->FindAllHitTriangles(WorldRay, Hits))
				{
					if (Hits.Num() == 1)
					{
						OutHitPoint = WorldRay.Origin + WorldRay.Direction * Hits[0].Distance;
						return true;						
					}

					// const double AverageDistance = Algo::Accumulate(Distances, 0.0) / static_cast<double>(Distances.Num());
					// const int32 Index0 = Distances.IndexOfByPredicate([AverageDistance](double Distance) {return Distance <= AverageDistance;} );
					// const int32 Index1 = Distances.IndexOfByPredicate([AverageDistance](double Distance) {return Distance >= AverageDistance;} );

					static constexpr int32 Index0 = 0;
					static constexpr int32 Index1 = 1;

					const double d0 = Hits[Index0].Distance;
					const double d1 = Hits[Index1].Distance;
					OutHitPoint = WorldRay.Origin + WorldRay.Direction * ((d0+d1)*0.5);
					return true;
				}
			}

			FHitResult Hit;
			if (PreviewMesh->FindRayIntersection(WorldRay, Hit))
			{
				OutHitPoint = WorldRay.Origin + WorldRay.Direction * Hit.Distance;
				return true;
			}
		}
	}
	
	// if ray is parallel to plane, nothing has been hit
	if (FMath::IsNearlyZero(FVector::DotProduct(PlaneNormal, WorldRay.Direction)))
	{
		return false;
	}

	const FPlane Plane(PlaneOrigin, PlaneNormal);
	
	if (CameraState.bIsOrthographic)
	{
		OutHitPoint = FVector::PointPlaneProject(WorldRay.Origin, Plane);
		return true;
	}
	
	const double HitDepth = FMath::RayPlaneIntersectionParam(WorldRay.Origin, WorldRay.Direction, Plane);
	if (HitDepth < 0.0)
	{
		return false;
	}

	OutHitPoint = WorldRay.Origin + WorldRay.Direction * HitDepth;
	
	return true;
}

#undef LOCTEXT_NAMESPACE