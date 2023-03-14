// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/GeometrySelectionManager.h"
#include "Selection/DynamicMeshSelector.h"
#include "Selection/SelectionEditInteractiveCommand.h"
#include "ToolContextInterfaces.h"
#include "ToolDataVisualizer.h"

// for debug drawing
#include "SceneManagement.h"		
#include "DynamicMeshBuilder.h"
#include "ToolSetupUtil.h"

#include "Selection/PersistentMeshSelection.h"


using namespace UE::Geometry;


#define LOCTEXT_NAMESPACE "UGeometrySelectionManager"


void UGeometrySelectionManager::Initialize( UInteractiveToolsContext* ToolsContextIn, IToolsContextTransactionsAPI* TransactionsAPIIn )
{
	ToolsContext = ToolsContextIn;
	TransactionsAPI = TransactionsAPIIn;
}

void UGeometrySelectionManager::RegisterSelectorFactory(TUniquePtr<IGeometrySelectorFactory> Factory)
{
	Factories.Add(MoveTemp(Factory));
	ResetTargetCache();
}

void UGeometrySelectionManager::Shutdown()
{
	OnSelectionModified.Clear();
	ToolsContext = nullptr;
	TransactionsAPI = nullptr;

	ResetTargetCache();
	ActiveTargetReferences.Reset();
	ActiveTargetMap.Reset();
	UpdateSelectionRenderCacheOnTargetChange();
}

bool UGeometrySelectionManager::HasBeenShutDown() const
{
	return (ToolsContext == nullptr);
}


class FGeometrySelectionManager_SelectionTypeChange : public FToolCommandChange
{
public:
	EGeometryElementType FromElementType = EGeometryElementType::Face;
	EGeometryElementType ToElementType = EGeometryElementType::Face;

	UGeometrySelectionManager::EMeshTopologyMode FromTopologyMode = UGeometrySelectionManager::EMeshTopologyMode::None;
	UGeometrySelectionManager::EMeshTopologyMode ToTopologyMode = UGeometrySelectionManager::EMeshTopologyMode::None;

	/** Makes the change to the object */
	virtual void Apply(UObject* Object) override
	{
		if (FromElementType != ToElementType)
		{
			CastChecked<UGeometrySelectionManager>(Object)->SetSelectionElementTypeInternal(ToElementType);
		}
		if (FromTopologyMode != ToTopologyMode)
		{
			CastChecked<UGeometrySelectionManager>(Object)->SetMeshTopologyModeInternal(ToTopologyMode);
		}
	}

	/** Reverts change to the object */
	virtual void Revert(UObject* Object) override
	{
		if (FromElementType != ToElementType)
		{
			CastChecked<UGeometrySelectionManager>(Object)->SetSelectionElementTypeInternal(FromElementType);
		}
		if (FromTopologyMode != ToTopologyMode)
		{
			CastChecked<UGeometrySelectionManager>(Object)->SetMeshTopologyModeInternal(FromTopologyMode);
		}
	}

	/** Describes this change (for debugging) */
	virtual FString ToString() const override { return TEXT("FGeometrySelectionManager_SelectionTypeChange"); }

	virtual bool HasExpired(UObject* Object) const override
	{
		UGeometrySelectionManager* Manager = Cast<UGeometrySelectionManager>(Object);
		return (Manager == nullptr || IsValid(Manager) == false || Manager->HasBeenShutDown());
	}
};



void UGeometrySelectionManager::SetSelectionElementTypeInternal(EGeometryElementType NewElementType)
{
	if (SelectionElementType != NewElementType)
	{
		SelectionElementType = NewElementType;

		for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
		{
			Target->Selection.ElementType = SelectionElementType;
			// do we need to update/rebuild Editors here? They currently do not have state...
		}
	}
}


void UGeometrySelectionManager::SetSelectionElementType(EGeometryElementType NewElementType)
{
	if (SelectionElementType != NewElementType)
	{
		GetTransactionsAPI()->BeginUndoTransaction(LOCTEXT("ChangeElementType", "Selection Type"));

		if (HasSelection())
		{
			ClearSelection();
		}

		// We have to undo/redo the change to the selection type because if we want to 'undo' this later and restore
		// the current selection, we need the active element type to be correct. Note that it goes *after* the Clear
		// so that when we undo, we change to the correct type before we restore
		TUniquePtr<FGeometrySelectionManager_SelectionTypeChange> TypeChange = MakeUnique<FGeometrySelectionManager_SelectionTypeChange>();
		TypeChange->FromElementType = SelectionElementType;
		TypeChange->ToElementType = NewElementType;
		GetTransactionsAPI()->AppendChange(this, MoveTemp(TypeChange), LOCTEXT("ChangeElementType", "Selection Type"));

		SetSelectionElementTypeInternal(NewElementType);

		GetTransactionsAPI()->EndUndoTransaction();
	}
}




void UGeometrySelectionManager::SetMeshTopologyModeInternal(EMeshTopologyMode NewTopologyMode)
{
	if (MeshTopologyMode != NewTopologyMode)
	{
		MeshTopologyMode = NewTopologyMode;

		for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
		{
			Target->Selection.TopologyType = GetSelectionTopologyType();
			// do we need to update/rebuild Editors here? They currently do not have state...
		}
	}
}


void UGeometrySelectionManager::SetMeshTopologyMode(EMeshTopologyMode NewTopologyMode)
{
	if (MeshTopologyMode != NewTopologyMode)
	{
		GetTransactionsAPI()->BeginUndoTransaction(LOCTEXT("ChangeSelectionMode", "Selection Mode"));

		if (HasSelection())
		{
			ClearSelection();
		}

		// We have to undo/redo the change to the selection type because if we want to 'undo' this later and restore
		// the current selection, we need the active element type to be correct. Note that it goes *after* the Clear
		// so that when we undo, we change to the correct type before we restore
		TUniquePtr<FGeometrySelectionManager_SelectionTypeChange> TypeChange = MakeUnique<FGeometrySelectionManager_SelectionTypeChange>();
		TypeChange->FromTopologyMode = MeshTopologyMode;
		TypeChange->FromTopologyMode = NewTopologyMode;
		GetTransactionsAPI()->AppendChange(this, MoveTemp(TypeChange),LOCTEXT("ChangeSelectionMode", "Selection Mode"));

		SetMeshTopologyModeInternal(NewTopologyMode);

		GetTransactionsAPI()->EndUndoTransaction();
	}
}



EGeometryTopologyType UGeometrySelectionManager::GetSelectionTopologyType() const
{
	if (MeshTopologyMode == EMeshTopologyMode::Polygroup)
	{
		return EGeometryTopologyType::Polygroup;
	}
	else
	{
		return EGeometryTopologyType::Triangle;
	}
}



bool UGeometrySelectionManager::HasSelection() const
{
	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		if (Target->Selection.IsEmpty() == false)
		{
			return true;
		}
	}
	return false;
}





class FGeometrySelectionManager_ActiveTargetsChange : public FToolCommandChange
{
public:
	TArray<FGeometryIdentifier> TargetsBefore;
	TArray<FGeometryIdentifier> TargetsAfter;

	virtual void Apply(UObject* Object) override
	{
		CastChecked<UGeometrySelectionManager>(Object)->SetTargetsOnUndoRedo(TargetsAfter);
	}

	virtual void Revert(UObject* Object) override
	{
		CastChecked<UGeometrySelectionManager>(Object)->SetTargetsOnUndoRedo(TargetsBefore);
	}

	virtual FString ToString() const override { return TEXT("FGeometrySelectionManager_ActiveTargetsChange"); }

	virtual bool HasExpired(UObject* Object) const override
	{
		UGeometrySelectionManager* Manager = Cast<UGeometrySelectionManager>(Object);
		return (Manager == nullptr || IsValid(Manager) == false || Manager->HasBeenShutDown());
	}
};





bool UGeometrySelectionManager::HasActiveTargets() const
{
	return (ActiveTargetReferences.Num() > 0);
}



void UGeometrySelectionManager::ClearActiveTargets()
{
	// generally at this point is it too late to clear the selection, because it will emit an
	// undo that cannot be redone later, because on redo the Targets will not exist yet
	// (one possibility would be to emit separate changes for when the target set is modified?? would that work w/ delete?? )
	ensure(HasSelection() == false);

	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		SleepOrShutdownTarget(Target.Get(), false);
	}

	ActiveTargetReferences.Reset();
	ActiveTargetMap.Reset();

	UpdateSelectionRenderCacheOnTargetChange();

	OnSelectionModified.Broadcast();
}

bool UGeometrySelectionManager::AddActiveTarget(FGeometryIdentifier TargetIdentifier)
{
	if (ActiveTargetMap.Contains(TargetIdentifier))
	{
		return false;
	}

	// need to have a selector factory that can build for this target
	const IGeometrySelectorFactory* UseFactory = nullptr;
	for (const TUniquePtr<IGeometrySelectorFactory>& Factory : Factories)
	{
		if (Factory->CanBuildForTarget(TargetIdentifier))
		{
			UseFactory = Factory.Get();
			break;
		}
	}
	if (UseFactory == nullptr)
	{
		return false;
	}

	TSharedPtr<FGeometrySelectionTarget> SelectionTarget = GetCachedTarget(TargetIdentifier, UseFactory);
	check(SelectionTarget != nullptr);

	ActiveTargetMap.Add(TargetIdentifier, SelectionTarget);
	ActiveTargetReferences.Add(SelectionTarget);

	SelectionTarget->OnGeometryModifiedHandle = 
		SelectionTarget->Selector->GetOnGeometryModifed().AddUObject(this, &UGeometrySelectionManager::OnTargetGeometryModified);

	UpdateSelectionRenderCacheOnTargetChange();

	return true;
}


void UGeometrySelectionManager::SynchronizeActiveTargets(
	const TArray<FGeometryIdentifier>& DesiredActiveSet,
	TFunctionRef<void()> WillChangeActiveTargetsCallback)
{
	TArray<FGeometryIdentifier> Before = GetCurrentTargetIdentifiers();

	// currently only support single selection
	if (DesiredActiveSet.Num() == 1)
	{
		// if we do not already have this target, select it
		if ( ActiveTargetMap.Contains(DesiredActiveSet[0]) == false )
		{
			WillChangeActiveTargetsCallback();
			ClearActiveTargets();
			AddActiveTarget(DesiredActiveSet[0]);
		}
	}
	else
	{
		WillChangeActiveTargetsCallback();
		ClearActiveTargets();
	}

	TArray<FGeometryIdentifier> After = GetCurrentTargetIdentifiers();
	if (Before != After)
	{
		TUniquePtr<FGeometrySelectionManager_ActiveTargetsChange> Change = MakeUnique<FGeometrySelectionManager_ActiveTargetsChange>();
		Change->TargetsBefore = Before;
		Change->TargetsAfter = After;
		GetTransactionsAPI()->AppendChange(this, MoveTemp(Change), LOCTEXT("Change Targets", "Change Targets"));
	}
}

TArray<FGeometryIdentifier> UGeometrySelectionManager::GetCurrentTargetIdentifiers() const
{
	TArray<FGeometryIdentifier> Result;
	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		Result.Add(Target->TargetIdentifier);
	}
	return Result;
}

void UGeometrySelectionManager::SetTargetsOnUndoRedo(TArray<FGeometryIdentifier> NewTargets)
{
	check(HasSelection() == false);
	ClearActiveTargets();
	for (FGeometryIdentifier Identifier : NewTargets)
	{
		AddActiveTarget(Identifier);
	}
}



void UGeometrySelectionManager::SleepOrShutdownTarget(FGeometrySelectionTarget* Target, bool bForceShutdown)
{
	if (Target->Selector->SupportsSleep() && bForceShutdown == false)
	{
		bool bOK = Target->Selector->Sleep();
		check(bOK);
	}
	else
	{
		Target->Selector->GetOnGeometryModifed().Remove(Target->OnGeometryModifiedHandle);
		Target->Selector->Shutdown();
	}
}

TSharedPtr<UGeometrySelectionManager::FGeometrySelectionTarget> UGeometrySelectionManager::GetCachedTarget(FGeometryIdentifier TargetIdentifier, const IGeometrySelectorFactory* UseFactory)
{
	if (TargetCache.Contains(TargetIdentifier))
	{
		TSharedPtr<FGeometrySelectionTarget> FoundTarget = TargetCache[TargetIdentifier];
		FoundTarget->Selection.Reset();
		FoundTarget->Selector->Restore();

		// ensure these are current, as they may have changed while Target was asleep
		FoundTarget->Selection.ElementType = GetSelectionElementType();
		FoundTarget->Selection.TopologyType = GetSelectionTopologyType();

		return FoundTarget;
	}

	// if we are in a situation where we don't have a cache, currently we need the Factory to exist?
	check(UseFactory != nullptr);

	// selector has to be built properly
	TUniquePtr<IGeometrySelector> Selector = UseFactory->BuildForTarget(TargetIdentifier);
	// not going to handle this for now...
	check(Selector.IsValid());

	TSharedPtr<FGeometrySelectionTarget> SelectionTarget = MakeShared<FGeometrySelectionTarget>();
	SelectionTarget->Selector = MoveTemp(Selector);
	SelectionTarget->TargetIdentifier = TargetIdentifier;
	SelectionTarget->SelectionIdentifer = SelectionTarget->Selector->GetIdentifier();
	SelectionTarget->Selection.ElementType = GetSelectionElementType();
	SelectionTarget->Selection.TopologyType = GetSelectionTopologyType();

	SelectionTarget->SelectionEditor = MakeUnique<FGeometrySelectionEditor>();
	SelectionTarget->SelectionEditor->Initialize(&SelectionTarget->Selection);

	if (SelectionTarget->Selector->SupportsSleep())
	{
		TargetCache.Add(TargetIdentifier, SelectionTarget);
	}

	return SelectionTarget;
}

void UGeometrySelectionManager::ResetTargetCache()
{
	for (TPair<FGeometryIdentifier, TSharedPtr<FGeometrySelectionTarget>> Pair : TargetCache)
	{
		SleepOrShutdownTarget(Pair.Value.Get(), true);
	}
	TargetCache.Reset();
}



bool UGeometrySelectionManager::RayHitTest(
	const FRay3d& WorldRay,
	FInputRayHit& HitResultOut )
{
	HitResultOut = FInputRayHit();
	if (ActiveTargetReferences.Num() == 0)
	{
		return false;
	}

	bool bHit = ActiveTargetReferences[0]->Selector->RayHitTest(WorldRay, HitResultOut);
	if (bHit)
	{
		HitResultOut.HitOwner = ActiveTargetReferences[0].Get();
		HitResultOut.HitObject = (ActiveTargetReferences[0]->TargetIdentifier.TargetType == FGeometryIdentifier::ETargetType::PrimitiveComponent) ?
			ActiveTargetReferences[0]->TargetIdentifier.TargetObject : nullptr;
	}

	// currently only going to support one object, not sure how to support more yet...
	return bHit;
}


void UGeometrySelectionManager::ClearSelection()
{
	if (!HasSelection())
	{
		return;
	}

	GetTransactionsAPI()->BeginUndoTransaction(LOCTEXT("ClearSelection", "Clear Selection"));

	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		FGeometrySelectionDelta ClearDelta;
		Target->SelectionEditor->ClearSelection(ClearDelta);
		if (ClearDelta.IsEmpty() == false)
		{
			TUniquePtr<FGeometrySelectionDeltaChange> ClearChange = MakeUnique<FGeometrySelectionDeltaChange>();
			ClearChange->Identifier = Target->SelectionIdentifer;
			ClearChange->Delta = MoveTemp(ClearDelta);
			GetTransactionsAPI()->AppendChange(this, MoveTemp(ClearChange), LOCTEXT("ClearSelection", "Clear Selection"));
		}
	}

	GetTransactionsAPI()->EndUndoTransaction();

	bSelectionRenderCachesDirty = true;
	OnSelectionModified.Broadcast();
}


void UGeometrySelectionManager::UpdateSelectionViaRaycast(
	const FRay3d& WorldRay,
	const FGeometrySelectionUpdateConfig& UpdateConfig,
	FGeometrySelectionUpdateResult& ResultOut
)
{
	ResultOut.bSelectionModified = false;

	if (ActiveTargetReferences.Num() == 0)
	{
		return;
	}

	// currently only going to support one object, not sure how to support more yet...
	FGeometrySelectionTarget* Target = ActiveTargetReferences[0].Get();

	Target->Selector->UpdateSelectionViaRaycast(
		WorldRay, *Target->SelectionEditor, UpdateConfig, ResultOut
	);

	if (ResultOut.bSelectionModified)
	{
		TUniquePtr<FGeometrySelectionDeltaChange> DeltaChange = MakeUnique<FGeometrySelectionDeltaChange>();
		DeltaChange->Identifier = Target->SelectionIdentifer;
		DeltaChange->Delta = ResultOut.SelectionDelta;

		GetTransactionsAPI()->BeginUndoTransaction(LOCTEXT("UpdateSelectionViaRaycast", "Change Selection"));
		GetTransactionsAPI()->AppendChange(this, MoveTemp(DeltaChange), LOCTEXT("UpdateSelectionViaRaycast", "Change Selection"));
		GetTransactionsAPI()->EndUndoTransaction();

		bSelectionRenderCachesDirty = true;
		OnSelectionModified.Broadcast();
	}
	else if (ResultOut.bSelectionMissed && UpdateConfig.ChangeType == EGeometrySelectionChangeType::Replace)
	{
		ClearSelection();
	}
}





bool UGeometrySelectionManager::GetSelectionBounds(FGeometrySelectionBounds& BoundsOut) const
{
	BoundsOut = FGeometrySelectionBounds();

	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		Target->Selector->AccumulateSelectionBounds(Target->Selection, BoundsOut, true);
	}

	return (BoundsOut.WorldBounds.IsEmpty() == false);
}


void UGeometrySelectionManager::GetSelectionWorldFrame(FFrame3d& SelectionFrame) const
{
	SelectionFrame = FFrame3d();
	if (HasSelection())
	{
		// only handling this case for now
		//if (ActiveTargetReferences.Num() == 1)
		TSharedPtr<FGeometrySelectionTarget> Target = ActiveTargetReferences[0];
		Target->Selector->GetSelectionFrame(Target->Selection, SelectionFrame, true);
	}
}


bool UGeometrySelectionManager::HasSelectionForComponent(UPrimitiveComponent* Component) const
{
	if (HasSelection())
	{
		for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
		{
			if (Target->TargetIdentifier.TargetObject == Component)
			{
				return Target->Selection.IsEmpty();
			}
		}
	}
	return false;
}

bool UGeometrySelectionManager::GetSelectionForComponent(UPrimitiveComponent* Component, FGeometrySelection& SelectionOut) const
{
	if (HasSelection())
	{
		for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
		{
			if (Target->TargetIdentifier.TargetObject == Component)
			{
				SelectionOut = Target->Selection;
				return ! Target->Selection.IsEmpty();
			}
		}
	}
	return false;
}



bool UGeometrySelectionManager::BeginTransformation()
{
	if (!ensure(IsInActiveTransformation() == false))
	{
		return false;
	}
	if (HasSelection() == false)
	{
		return false;
	}

	bool bHaveTransformers = false;
	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		IGeometrySelectionTransformer* Transformer = Target->Selector->InitializeTransformation(Target->Selection);
		if (Transformer != nullptr)
		{
			Transformer->BeginTransform(Target->Selection);
			ActiveTransformations.Add(Transformer);
			bHaveTransformers = true;
		}
		else
		{
			ActiveTransformations.Add(nullptr);
		}
	}

	if (!bHaveTransformers)
	{
		ActiveTransformations.Reset();
		return false;
	}

	return true;
}

void UGeometrySelectionManager::UpdateTransformation( 
	TFunctionRef<FVector3d(int32 VertexID, const FVector3d&, const FTransform&)> PositionTransformFunc )
{
	if ( ! ensure(IsInActiveTransformation()) ) return;

	for (int32 k = 0; k < ActiveTargetReferences.Num(); ++k)
	{
		if (ActiveTransformations[k] != nullptr)
		{
			ActiveTransformations[k]->UpdateTransform(PositionTransformFunc);
		}
	}

	bSelectionRenderCachesDirty = true;
}

void UGeometrySelectionManager::EndTransformation()
{
	if ( ! ensure(IsInActiveTransformation()) ) return;

	GetTransactionsAPI()->BeginUndoTransaction(LOCTEXT("EndTransformation", "Transform Selection"));

	for (int32 k = 0; k < ActiveTargetReferences.Num(); ++k)
	{
		if (ActiveTransformations[k] != nullptr)
		{
			ActiveTransformations[k]->EndTransform(GetTransactionsAPI());

			ActiveTargetReferences[k]->Selector->ShutdownTransformation(ActiveTransformations[k]);
		}
	}
	ActiveTransformations.Reset();

	GetTransactionsAPI()->EndUndoTransaction();

	bSelectionRenderCachesDirty = true;
}





bool UGeometrySelectionManager::CanExecuteSelectionCommand(UGeometrySelectionEditCommand* Command)
{
	if (SelectionArguments == nullptr)
	{
		SelectionArguments = NewObject<UGeometrySelectionEditCommandArguments>();
	}

	bool bCanExecute = true;
	bool bHaveSelections = false;
	ProcessActiveSelections([&](FGeometrySelectionHandle Handle)
	{
		SelectionArguments->SelectionHandle = Handle;
		SelectionArguments->SetTransactionsAPI(TransactionsAPI);
		bCanExecute = bCanExecute && Command->CanExecuteCommand(SelectionArguments);
		bHaveSelections = true;
	});

	return bHaveSelections && bCanExecute;
}

void UGeometrySelectionManager::ExecuteSelectionCommand(UGeometrySelectionEditCommand* Command)
{
	if (SelectionArguments == nullptr)
	{
		SelectionArguments = NewObject<UGeometrySelectionEditCommandArguments>();
	}

	// open transaction to wrap the entire set of Commands and selection changes
	FText CommandText = Command->GetCommandShortString();
	GetTransactionsAPI()->BeginUndoTransaction(CommandText);

	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		if (Target->Selection.IsEmpty()) continue;

		// When initially executing the command, we do not clear the selection, because we pass it to the command.
		// However, when we later *undo* any changes emitted by the command, we need to restore the selection aftewards.
		// So we emit a clearing change here, so that undo un-clears. 
		// When we later Redo, it is also necessary to Clear as otherwise an invalid Selection might hang around.
		// Note that this must happen *before* the Command. The Command will not be re-executed, only its emitted Changes,
		// so it will not be holding onto the active Selection on Redo later
		// (if that becomes necessary, this sequence of changes will need to become more complicated....)
		TUniquePtr<FGeometrySelectionReplaceChange> ClearChange = MakeUnique<FGeometrySelectionReplaceChange>();
		ClearChange->Identifier = Target->Selector->GetIdentifier();
		ClearChange->Before = Target->Selection;
		ClearChange->After.InitializeTypes(ClearChange->Before);
		GetTransactionsAPI()->AppendChange(this, MoveTemp(ClearChange), LOCTEXT("ClearSelection", "Clear Selection"));

		// q: we could clear the selection here, and pass the Handle a copy. Perhaps safer?

		SelectionArguments->SelectionHandle = FGeometrySelectionHandle{ Target->Selector->GetIdentifier(), &Target->Selection };
		SelectionArguments->SetTransactionsAPI(TransactionsAPI);
		Command->ExecuteCommand(SelectionArguments);

		// clear selection after executing command
		FGeometrySelectionDelta ClearDelta;
		Target->SelectionEditor->ClearSelection(ClearDelta);

		// 
		// if commands could emit new selections, this is where we would set them up and emit changes!
	}

	GetTransactionsAPI()->EndUndoTransaction();

	// assume this is true for now
	bSelectionRenderCachesDirty = true;
	OnSelectionModified.Broadcast();
}


void UGeometrySelectionManager::ProcessActiveSelections(TFunctionRef<void(FGeometrySelectionHandle)> ProcessFunc)
{
	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		if (Target->Selection.IsEmpty() == false)
		{
			FGeometrySelectionHandle Handle;
			Handle.Selection = & Target->Selection;
			Handle.Identifier = Target->Selector->GetIdentifier();
			ProcessFunc(Handle);
		}
	}
}









void UGeometrySelectionManager::ApplyChange(IGeometrySelectionChange* Change)
{
	// We should not get here because selection changes should have been expired.
	if ( ! ensure(HasBeenShutDown() == false) )
	{
		return;
	}

	FGeometryIdentifier Identifer = Change->GetIdentifier();

	for (int32 k = 0; k < ActiveTargetReferences.Num(); ++k)
	{
		if (ActiveTargetReferences[k]->SelectionIdentifer == Identifer) 
		{
			FGeometrySelectionDelta ApplyDelta;
			Change->ApplyChange( ActiveTargetReferences[k]->SelectionEditor.Get(), ApplyDelta);

			if (ApplyDelta.IsEmpty() == false)
			{
				bSelectionRenderCachesDirty = true;
				OnSelectionModified.Broadcast();
			}

			break;
		}
	}
}


void UGeometrySelectionManager::RevertChange(IGeometrySelectionChange* Change)
{
	// We should not get here because selection changes should have been expired.
	if ( ! ensure(HasBeenShutDown() == false) )
	{
		return;
	}

	FGeometryIdentifier Identifer = Change->GetIdentifier();

	for (int32 k = 0; k < ActiveTargetReferences.Num(); ++k)
	{
		if (ActiveTargetReferences[k]->SelectionIdentifer == Identifer) 
		{
			FGeometrySelectionDelta RevertDelta;
			Change->RevertChange( ActiveTargetReferences[k]->SelectionEditor.Get(), RevertDelta);

			if (RevertDelta.IsEmpty() == false)
			{
				bSelectionRenderCachesDirty = true;
				OnSelectionModified.Broadcast();
			}

			break;
		}
	}
}


void UGeometrySelectionManager::OnTargetGeometryModified(IGeometrySelector* Selector)
{
	bSelectionRenderCachesDirty = true;
}


void UGeometrySelectionManager::UpdateSelectionRenderCacheOnTargetChange()
{
	CachedSelectionRenderElements.Reset();
	CachedSelectionRenderElements.SetNum(ActiveTargetReferences.Num());
	bSelectionRenderCachesDirty = true;
}

void UGeometrySelectionManager::RebuildSelectionRenderCaches()
{
	if (bSelectionRenderCachesDirty == false)
	{
		return;
	}

	check(ActiveTargetReferences.Num() == CachedSelectionRenderElements.Num());
	for (int32 k = 0; k < ActiveTargetReferences.Num(); ++k)
	{
		TSharedPtr<FGeometrySelectionTarget> Target = ActiveTargetReferences[k];

		FGeometrySelectionElements& Elements = CachedSelectionRenderElements[k];
		Elements.Reset();

		Target->Selector->AccumulateSelectionElements(Target->Selection, Elements, true);
	}

	bSelectionRenderCachesDirty = false;
}


void UGeometrySelectionManager::DebugPrintSelection()
{
	if (ActiveTargetReferences.Num() == 0)
	{
		UE_LOG(LogGeometry, Warning, TEXT("[SelectionManager] No Active Selection"));
		return;
	}
	
	int32 NumSelected = 0;
	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		NumSelected += Target->Selection.Num();
	}
	UE_LOG(LogGeometry, Warning, TEXT("[SelectionManager] %d selected items in %d active targets"), NumSelected, ActiveTargetReferences.Num());
}


void UGeometrySelectionManager::DebugRender(IToolsContextRenderAPI* RenderAPI)
{
	// disable selection during xform to avoid overhead
	if (IsInActiveTransformation())
	{
		return;
	}

	//const UMaterialInterface* TriangleMaterial = ToolSetupUtil::GetSelectionMaterial(FLinearColor(1.0f, 0, 0, 0.5f), nullptr, 0.5f);

	RebuildSelectionRenderCaches();

	for ( const FGeometrySelectionElements& Elements : CachedSelectionRenderElements )
	{
		// batch render all the triangles, vastly more efficient than drawing one by one!

		FPrimitiveDrawInterface* CurrentPDI = RenderAPI->GetPrimitiveDrawInterface();
		FDynamicMeshBuilder MeshBuilder(CurrentPDI->View->GetFeatureLevel());
		int32 DepthPriority = SDPG_World; // SDPG_Foreground;  // SDPG_World
		FVector2f UVs[3] = { FVector2f(0,0), FVector2f(0,1), FVector2f(1,1)	};
		FVector3f Normal = FVector3f(0, 0, 1);
		FVector3f Tangent = FVector3f(1, 0, 0);	
		for (const FTriangle3d& Triangle : Elements.Triangles)
		{
			int32 V0 = MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)Triangle.V[0], Tangent, Normal, UVs[0], FColor::White));
			int32 V1 = MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)Triangle.V[1], Tangent, Normal, UVs[1], FColor::White));
			int32 V2 = MeshBuilder.AddVertex(FDynamicMeshVertex((FVector3f)Triangle.V[2], Tangent, Normal, UVs[2], FColor::White));
			MeshBuilder.AddTriangle(V0, V1, V2);
		}
		//FMaterialRenderProxy* MaterialRenderProxy = TriangleMaterial->GetRenderProxy();		// currently does not work, material does not render
		FMaterialRenderProxy* MaterialRenderProxy = GEngine->ConstraintLimitMaterialX->GetRenderProxy();
		MeshBuilder.Draw(CurrentPDI, FMatrix::Identity, MaterialRenderProxy, DepthPriority, false, false);


		FToolDataVisualizer Visualizer;
		Visualizer.bDepthTested = false;
		Visualizer.BeginFrame(RenderAPI);

		Visualizer.SetLineParameters(FLinearColor(0,1,0,1), 2.0f);
		for (const FSegment3d& Segment : Elements.Segments)
		{
			Visualizer.DrawLine(Segment.StartPoint(), Segment.EndPoint());
		}

		Visualizer.SetPointParameters(FLinearColor(0,0,1,1), 10.0f);
		for (const FVector3d& Point : Elements.Points)
		{
			Visualizer.DrawPoint(Point);
		}

		Visualizer.EndFrame();
	}

}



UPersistentMeshSelection* UGeometrySelectionManager::GetActiveSingleSelectionConverted_Legacy(UPrimitiveComponent* ForComponent)
{
	OldSelection = nullptr;

	if (HasSelection() == false)
	{
		return nullptr;
	}

	for (TSharedPtr<FGeometrySelectionTarget> Target : ActiveTargetReferences)
	{
		if (Target->TargetIdentifier.TargetObject == ForComponent)
		{
			const FGeometrySelection& GeoSelection = Target->Selection;

			OldSelection = NewObject<UPersistentMeshSelection>(this);
			FGenericMeshSelection Selection;
			Selection.SourceComponent = ForComponent;

			if (GeoSelection.TopologyType == EGeometryTopologyType::Triangle)
			{
				Selection.TopologyType = FGenericMeshSelection::ETopologyType::FTriangleGroupTopology;
				if (GeoSelection.ElementType == EGeometryElementType::Face)
				{
					for (uint64 tid : GeoSelection.Selection)
					{
						Selection.FaceIDs.Add((int32)tid);
					}
				}
			}
			else
			{
				Selection.TopologyType = FGenericMeshSelection::ETopologyType::FGroupTopology;
				if (GeoSelection.ElementType == EGeometryElementType::Face)
				{
					for (uint64 EncodedID : GeoSelection.Selection)
					{
						FGeoSelectionID SelectionID(EncodedID);
						Selection.FaceIDs.Add((int32)SelectionID.TopologyID);
					}
				}
			}

			
			OldSelection->SetSelection(Selection);
		}
	}

	return OldSelection;
}


#undef LOCTEXT_NAMESPACE