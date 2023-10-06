// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "Internationalization/Text.h"
#include "UObject/UnrealType.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/DataLayer/DataLayerUtils.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationErrorHandler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerInstance)

#define LOCTEXT_NAMESPACE "DataLayer"

UDataLayerInstance::UDataLayerInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, bIsVisible(true)
	, bIsInitiallyVisible(true)
	, bIsInitiallyLoadedInEditor(true)
	, bIsLoadedInEditor(true)
	, bIsLocked(false)
#endif
	, InitialRuntimeState(EDataLayerRuntimeState::Unloaded)
{

}

void UDataLayerInstance::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Initialize bIsVisible with persistent flag bIsInitiallyVisible
	bIsVisible = bIsInitiallyVisible;
#endif

	if (Parent)
	{
		Parent->AddChild(this);
	}
}

UWorld* UDataLayerInstance::GetOuterWorld() const
{
	return UObject::GetTypedOuter<UWorld>();
}

AWorldDataLayers* UDataLayerInstance::GetOuterWorldDataLayers() const
{
	return GetOuterWorld()->GetWorldDataLayers();
}

#if WITH_EDITOR
void UDataLayerInstance::PreEditUndo()
{
	Super::PreEditUndo();
	bUndoIsLoadedInEditor = bIsLoadedInEditor;
}

void UDataLayerInstance::PostEditUndo()
{
	Super::PostEditUndo();
	if (bIsLoadedInEditor != bUndoIsLoadedInEditor)
	{
		IWorldPartitionActorLoaderInterface::RefreshLoadedState(true);
	}
}

void UDataLayerInstance::SetVisible(bool bInIsVisible)
{
	if (bIsVisible != bInIsVisible)
	{
		Modify(/*bAlwaysMarkDirty*/false);
		bIsVisible = bInIsVisible;
	}
}

void UDataLayerInstance::SetIsInitiallyVisible(bool bInIsInitiallyVisible)
{
	if (bIsInitiallyVisible != bInIsInitiallyVisible)
	{
		Modify();
		bIsInitiallyVisible = bInIsInitiallyVisible;
	}
}

void UDataLayerInstance::SetIsLoadedInEditor(bool bInIsLoadedInEditor, bool bInFromUserChange)
{
	if (bIsLoadedInEditor != bInIsLoadedInEditor)
	{
		Modify(false);
		bIsLoadedInEditor = bInIsLoadedInEditor;
		bIsLoadedInEditorChangedByUserOperation |= bInFromUserChange;
	}
}

bool UDataLayerInstance::IsEffectiveLoadedInEditor() const
{
	bool bResult = IsLoadedInEditor();

	const UDataLayerInstance* ParentDataLayer = GetParent();
	while (ParentDataLayer && bResult)
	{
		bResult = bResult && ParentDataLayer->IsLoadedInEditor();
		ParentDataLayer = ParentDataLayer->GetParent();
	}
	return bResult;
}

bool UDataLayerInstance::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	if (IsReadOnly())
	{
		return false;
	}

	if (!IsRuntime() && (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDataLayerInstance, InitialRuntimeState)))
	{
		return false;
	}

	return true;
}

bool UDataLayerInstance::IsLocked() const
{
	if (bIsLocked)
	{
		return true;
	}

	return IsRuntime() && !GetOuterWorldDataLayers()->GetAllowRuntimeDataLayerEditing();
}

bool UDataLayerInstance::IsReadOnly() const
{
	return GetWorld()->IsGameWorld();
}

const TCHAR* UDataLayerInstance::GetDataLayerIconName() const
{
	return FDataLayerUtils::GetDataLayerIconName(GetType());
}

bool UDataLayerInstance::CanUserAddActors() const
{
	return !IsLocked();
}

bool UDataLayerInstance::CanAddActor(AActor* InActor) const
{
	return InActor != nullptr && InActor->CanAddDataLayer(this);
}

bool UDataLayerInstance::AddActor(AActor* InActor) const
{
	if (CanAddActor(InActor))
	{
		return PerformAddActor(InActor);
	}

	return false;
}

bool UDataLayerInstance::CanUserRemoveActors() const
{
	return !IsLocked();
}

bool UDataLayerInstance::CanRemoveActor(AActor* InActor) const
{
	return InActor->GetDataLayerInstances().Contains(this) || InActor->GetDataLayerInstancesForLevel().Contains(this);
}

bool UDataLayerInstance::RemoveActor(AActor* InActor) const
{
	if (CanRemoveActor(InActor))
	{
		return PerformRemoveActor(InActor);
	}

	return false;
}

bool UDataLayerInstance::CanBeChildOf(const UDataLayerInstance* InParent, FText* OutReason) const
{
	auto AssignReason = [OutReason](FText&& Reason)
	{
		if(OutReason != nullptr)
		{
			*OutReason = MoveTemp(Reason);
		}
	};

	if (this == InParent || Parent == InParent)
	{
		AssignReason(LOCTEXT("SameParentOrSameDataLayer", "Data Layer already has this parent"));
		return false;
	}

	if (InParent == nullptr)
	{
		// nullptr is considered a valid parent
		return true;
	}

	if (!CanHaveParentDataLayer())
	{
		AssignReason(FText::Format(LOCTEXT("ParentDataLayerUnsupported", "Data Layer \"{0}\" does not support parent data layers"), FText::FromString(GetDataLayerShortName())));
		return false;
	}

	if (!InParent->CanHaveChildDataLayers())
	{
		AssignReason(FText::Format(LOCTEXT("ChildDataLayerUnsuported", "Data Layer \"{0}\" does not support child data layers"), FText::FromString(InParent->GetDataLayerShortName())));
		return false;
	}

	if (!IsParentDataLayerTypeCompatible(InParent))
	{
		AssignReason(FText::Format(LOCTEXT("IncompatibleChildType", "{0} Data Layer cannot have {1} child Data Layers"), UEnum::GetDisplayValueAsText(GetType()), UEnum::GetDisplayValueAsText(InParent->GetType())));
		return false;
	}

	if (InParent->GetOuterWorldDataLayers() != GetOuterWorldDataLayers())
	{
		AssignReason(LOCTEXT("DifferentOuterWorldDataLayer", "Parent WorldDataLayers is a different from child WorldDataLayers"));
		return false;
	}

	return true;
}

bool UDataLayerInstance::IsParentDataLayerTypeCompatible(const UDataLayerInstance* InParent) const
{
	if (InParent == nullptr)
	{
		return false;
	}

	EDataLayerType ParentDataLayerType = InParent->GetType();

	return GetType() != EDataLayerType::Unknown
		&& ParentDataLayerType != EDataLayerType::Unknown
		&& (ParentDataLayerType == EDataLayerType::Editor || GetType() == EDataLayerType::Runtime);
}

bool UDataLayerInstance::SetParent(UDataLayerInstance* InParent)
{
	if (!CanBeChildOf(InParent))
	{
		return false;
	}

	check(CanHaveParentDataLayer());

	Modify();

	// If we find ourself in the parent chain of the provided parent
	UDataLayerInstance* CurrentInstance = InParent;
	while (CurrentInstance)
	{
		if (CurrentInstance == this)
		{
			// Detach the parent from its parent
			InParent->SetParent(nullptr);
			break;
		}
		CurrentInstance = CurrentInstance->GetParent();
	};

	if (Parent)
	{
		Parent->RemoveChild(this);
	}
	Parent = InParent;
	if (Parent)
	{
		Parent->AddChild(this);
	}

	return true;
}

void UDataLayerInstance::SetChildParent(UDataLayerInstance* InParent)
{
	if (this == InParent)
	{
		return;
	}

	check(!InParent || InParent->CanHaveChildDataLayers());

	Modify();
	while (Children.Num())
	{
		Children[0]->SetParent(InParent);
	};
}

void UDataLayerInstance::RemoveChild(UDataLayerInstance* InDataLayer)
{
	Modify();
	check(Children.Contains(InDataLayer));
	Children.RemoveSingle(InDataLayer);
}

FText UDataLayerInstance::GetDataLayerText(const UDataLayerInstance* InDataLayer)
{
	return InDataLayer ? FText::FromString(InDataLayer->GetDataLayerShortName()) : LOCTEXT("InvalidDataLayerShortName", "<None>");
}

bool UDataLayerInstance::Validate(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	if (GetParent() != nullptr && !IsParentDataLayerTypeCompatible(GetParent()))
	{
		ErrorHandler->OnDataLayerHierarchyTypeMismatch(this, GetParent());
		return false;
	}

	return true;
}

bool UDataLayerInstance::CanBeInActorEditorContext() const
{
	return !IsLocked();
}

bool UDataLayerInstance::IsInActorEditorContext() const
{
	return GetOuterWorldDataLayers()->IsInActorEditorContext(this);
}

bool UDataLayerInstance::AddToActorEditorContext()
{
	return GetOuterWorldDataLayers()->AddToActorEditorContext(this);
}

bool UDataLayerInstance::RemoveFromActorEditorContext()
{
	return GetOuterWorldDataLayers()->RemoveFromActorEditorContext(this);
}

#endif

bool UDataLayerInstance::IsInitiallyVisible() const
{
#if WITH_EDITOR
	return bIsInitiallyVisible;
#else
	return false;
#endif
}

bool UDataLayerInstance::IsVisible() const
{
#if WITH_EDITOR
	return bIsVisible;
#else
	return false;
#endif
}

bool UDataLayerInstance::IsEffectiveVisible() const
{
#if WITH_EDITOR
	bool bResult = IsVisible();
	const UDataLayerInstance* ParentDataLayer = GetParent();
	while (ParentDataLayer && bResult)
	{
		bResult = bResult && ParentDataLayer->IsVisible();
		ParentDataLayer = ParentDataLayer->GetParent();
	}
	return bResult && IsEffectiveLoadedInEditor();
#else
	return false;
#endif
}

void UDataLayerInstance::ForEachChild(TFunctionRef<bool(const UDataLayerInstance*)> Operation) const
{
	for (const UDataLayerInstance* Child : Children)
	{
		if (!Operation(Child))
		{
			break;
		}
	}
}

void UDataLayerInstance::AddChild(UDataLayerInstance* InDataLayer)
{
	check(InDataLayer->GetOuterWorldDataLayers() == GetOuterWorldDataLayers())
	check(CanHaveChildDataLayers());
	Modify();
	checkSlow(!Children.Contains(InDataLayer));
	Children.Add(InDataLayer);
}

EDataLayerRuntimeState UDataLayerInstance::GetRuntimeState() const
{
	return GetOuterWorldDataLayers()->GetDataLayerRuntimeStateByName(GetDataLayerFName());
}

EDataLayerRuntimeState UDataLayerInstance::GetEffectiveRuntimeState() const
{
	return GetOuterWorldDataLayers()->GetDataLayerEffectiveRuntimeStateByName(GetDataLayerFName());
}

bool UDataLayerInstance::SetRuntimeState(EDataLayerRuntimeState InState, bool bInIsRecursive) const
{
	if (GetOuterWorldDataLayers()->HasAuthority())
	{
		GetOuterWorldDataLayers()->SetDataLayerRuntimeState(this, InState, bInIsRecursive);
		return true;
	}
	UE_LOG(LogWorldPartition, Error, TEXT("SetDataLayerRuntimeState can only execute on authority"));
	return false;
}

#undef LOCTEXT_NAMESPACE
