// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEClonerComponent.h"

#include "Async/Async.h"
#include "Cloner/CEClonerActor.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "Components/BrushComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "DynamicMeshEditor.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "Materials/Material.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"
#include "ProceduralMeshComponent.h"
#include "StaticMeshOperations.h"
#include "UDynamicMesh.h"
#include "Subsystems/CEClonerSubsystem.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Framework/Notifications/NotificationManager.h"
#include "Materials/Material.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogCEClonerComponent, Log, All);

#define LOCTEXT_NAMESPACE "CEClonerComponent"

UCEClonerComponent::FOnClonerMeshUpdated UCEClonerComponent::OnClonerMeshUpdated;
UCEClonerComponent::FOnClonerSystemLoaded UCEClonerComponent::OnClonerSystemLoaded;

UCEClonerComponent::UCEClonerComponent()
	: UNiagaraComponent()
{
	CastShadow = true;
	bReceivesDecals = true;
	bAutoActivate = true;
	bHiddenInGame = false;

#if WITH_EDITOR
	// Do not show bounding box around cloner for better visibility
	SetIsVisualizationComponent(true);

	// Disable use of bounds to focus to avoid de-zoom
	SetIgnoreBoundsForEditorFocus(true);
#endif

	bIsEditorOnly = false;

	// Show sprite for this component to visualize it when empty
#if WITH_EDITORONLY_DATA
	bVisualizeComponent = true;
#endif

	if (!IsTemplate())
	{
		// Bind to delegate to detect material changes
#if WITH_EDITOR
		FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UCEClonerComponent::OnActorPropertyChanged);
#endif
	}
}

void UCEClonerComponent::PostLoad()
{
	SetAsset(nullptr);

	Super::PostLoad();
}

void UCEClonerComponent::UpdateClonerRenderState()
{
	/**
	 * Perform a mesh update when asset is valid,
	 * An update is not already ongoing,
	 * Meshes are out of date after an attachment tree update,
	 * Tree is up to date
	 */
	if (!GetAsset()
		|| IsGarbageCollectingAndLockingUObjectHashTables()
		|| bClonerMeshesUpdating
		|| !bClonerMeshesDirty
		|| ClonerTree.Status != ECEClonerAttachmentStatus::Updated)
	{
		return;
	}

	UpdateDirtyMeshesAsync();
}

void UCEClonerComponent::RefreshUserParameters() const
{
	if (UNiagaraSystem* ActiveSystem = GetAsset())
	{
		FNiagaraUserRedirectionParameterStore& UserParameterStore = ActiveSystem->GetExposedParameters();
		UserParameterStore.PostGenericEditChange();
	}
}

void UCEClonerComponent::OnRenderStateDirty(UActorComponent& InComponent)
{
	const AActor* OwningActor = InComponent.GetOwner();
	const ACEClonerActor* ClonerActor = GetOuterACEClonerActor();

	if (!IsValid(OwningActor) || !IsValid(ClonerActor))
	{
		return;
	}

	if (OwningActor->GetWorld() != ClonerActor->GetWorld())
	{
		return;
	}

	/* Owning actor is updated and is attached to cloner
		or owning actor is detached from cloner */
	if (OwningActor->IsAttachedTo(ClonerActor)
		|| ClonerTree.ItemAttachmentMap.Contains(OwningActor))
	{
		UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Detected attachment change for %s"), *ClonerActor->GetActorNameOrLabel(), *OwningActor->GetActorNameOrLabel());

		// Only update attachment tree
		UpdateClonerAttachmentTree();
	}
}

void UCEClonerComponent::UpdateClonerAttachmentTree(bool bInReset)
{
	if (ClonerTree.Status == ECEClonerAttachmentStatus::Updated)
	{
		ClonerTree.Status = ECEClonerAttachmentStatus::Outdated;
	}

	if (bInReset)
	{
		ClonerTree.Reset();
		ClonerTree.Status = ECEClonerAttachmentStatus::Outdated;
	}

	UpdateAttachmentTree();
}

void UCEClonerComponent::UpdateAttachmentTree()
{
	if (ClonerTree.Status != ECEClonerAttachmentStatus::Outdated)
	{
		return;
	}
	ClonerTree.Status = ECEClonerAttachmentStatus::Updating;

	// Invalidate all, to see what is outdated and what is still invalid
	for (TPair<TWeakObjectPtr<AActor>, FCEClonerAttachmentItem>& AttachmentPair : ClonerTree.ItemAttachmentMap)
	{
		AttachmentPair.Value.Status = ECEClonerAttachmentStatus::Invalid;
	}

	// Update root attachment items
	TArray<AActor*> RootChildren;
	GetOrderedRootActors(RootChildren);

	TArray<TObjectPtr<UStaticMesh>> NewCombinesMeshes;
	TArray<TWeakObjectPtr<AActor>> NewRootActors;
	for (int32 RootIdx = 0; RootIdx < RootChildren.Num(); RootIdx++)
	{
		AActor* RootChild = RootChildren[RootIdx];

		UpdateActorAttachment(RootChild, nullptr);

		// Lets find the old root idx
		const int32 OldIdx = ClonerTree.RootActors.Find(RootChild);
		TObjectPtr<UStaticMesh> CombineBakedMesh = nullptr;
		if (OldIdx != INDEX_NONE)
		{
			CombineBakedMesh = ClonerTree.MergedBakedMeshes[OldIdx];

			// Did we rearrange stuff ?
			if (RootIdx != OldIdx)
			{
				bClonerMeshesDirty = true;
			}
		}
		NewCombinesMeshes.Add(CombineBakedMesh);
		NewRootActors.Add(RootChild);
	}

	// Did we remove any root actors ?
	if (ClonerTree.RootActors.Num() != NewRootActors.Num())
	{
		bClonerMeshesDirty = true;
	}

	// Did we need to update meshes
	TArray<TWeakObjectPtr<AActor>> ClonedActors;
	ClonerTree.ItemAttachmentMap.GenerateKeyArray(ClonedActors);
	for (const TWeakObjectPtr<AActor>& ClonedActor : ClonedActors)
	{
		FCEClonerAttachmentItem* ClonedItem = ClonerTree.ItemAttachmentMap.Find(ClonedActor);

		if (!ClonedItem)
		{
			continue;
		}

		if (ClonedItem->Status == ECEClonerAttachmentStatus::Invalid)
		{
			InvalidateBakedStaticMesh(ClonedActor.Get());
			UnbindActorDelegates(ClonedActor.Get());
			ClonerTree.ItemAttachmentMap.Remove(ClonedItem->ItemActor);
		}
		else if (ClonedItem->Status == ECEClonerAttachmentStatus::Outdated)
		{
			if (ClonedItem->MeshStatus == ECEClonerAttachmentStatus::Outdated)
			{
				DirtyItemAttachments.Add(ClonedItem);
				InvalidateBakedStaticMesh(ClonedActor.Get());
			}
			bClonerMeshesDirty = true;
			ClonedItem->Status = ECEClonerAttachmentStatus::Updated;
		}
	}

	// Did we remove an attachment ?
	if (ClonedActors.Num() != ClonerTree.ItemAttachmentMap.Num())
	{
		bClonerMeshesDirty = true;
	}

	if (!DirtyItemAttachments.IsEmpty())
	{
		bClonerMeshesDirty = true;
	}

	// Use default meshes if nothing is attached
	if (NewRootActors.IsEmpty() && NewCombinesMeshes.IsEmpty())
	{
		if (const ACEClonerActor* ClonerActor = GetOuterACEClonerActor())
		{
			const TArray<TObjectPtr<UStaticMesh>>& DefaultMeshes = ClonerActor->GetDefaultMeshes();

			if (ClonerTree.MergedBakedMeshes.Num() != DefaultMeshes.Num())
			{
				bClonerMeshesDirty = true;
			}

			for (const TObjectPtr<UStaticMesh>& DefaultMesh : DefaultMeshes)
			{
				const int32 Idx = NewCombinesMeshes.Add(DefaultMesh);

				// Only update when there is a change
				if (!ClonerTree.MergedBakedMeshes.IsValidIndex(Idx)
					|| DefaultMesh != ClonerTree.MergedBakedMeshes[Idx])
				{
					bClonerMeshesDirty = true;
				}
			}
		}
	}

	ClonerTree.RootActors = NewRootActors;
	ClonerTree.MergedBakedMeshes = NewCombinesMeshes;
	ClonerTree.Status = ECEClonerAttachmentStatus::Updated;
}

void UCEClonerComponent::UpdateActorAttachment(AActor* InActor, AActor* InParent)
{
	if (!InActor)
	{
		return;
	}

	const AActor* ClonerActor = GetOwner();
	const FTransform& ClonerTransform = ClonerActor->GetActorTransform();

	// Here order is not important
	TArray<AActor*> ChildrenActors;
	InActor->GetAttachedActors(ChildrenActors, true, false);

	FCEClonerAttachmentItem* AttachmentItem = ClonerTree.ItemAttachmentMap.Find(InActor);
	if (AttachmentItem)
	{
		AttachmentItem->Status = ECEClonerAttachmentStatus::Updated;

		// Check Root is the same
		const bool bIsRoot = InParent == nullptr;
		if (AttachmentItem->bRootItem != bIsRoot)
		{
			InvalidateBakedStaticMesh(InActor);
			AttachmentItem->bRootItem = bIsRoot;
			AttachmentItem->Status = ECEClonerAttachmentStatus::Outdated;
		}

		// Check parent is the same
		if (AttachmentItem->ParentActor.Get() != InParent)
		{
			InvalidateBakedStaticMesh(InParent);
			InvalidateBakedStaticMesh(AttachmentItem->ParentActor.Get());
			AttachmentItem->ParentActor = InParent;
			AttachmentItem->Status = ECEClonerAttachmentStatus::Outdated;
		}

		// Check transform is the same
		const FTransform ActorTransform = InActor->GetActorTransform().GetRelativeTransform(ClonerTransform);
		if (!ActorTransform.Equals(AttachmentItem->ActorTransform))
		{
			// invalidate if not root, else change transform in mesh renderer
			if (!bIsRoot)
			{
				InvalidateBakedStaticMesh(InActor);
			}
			AttachmentItem->ActorTransform = ActorTransform;
			AttachmentItem->Status = ECEClonerAttachmentStatus::Outdated;
		}
	}
	else
	{
		AttachmentItem = &ClonerTree.ItemAttachmentMap.Add(InActor, FCEClonerAttachmentItem());
		AttachmentItem->ItemActor = InActor;
		AttachmentItem->ParentActor = InParent;
		AttachmentItem->ActorTransform = InActor->GetActorTransform().GetRelativeTransform(ClonerTransform);
		AttachmentItem->MeshStatus = ECEClonerAttachmentStatus::Outdated;
		AttachmentItem->bRootItem = InParent == nullptr;
		AttachmentItem->Status = ECEClonerAttachmentStatus::Outdated;
		InvalidateBakedStaticMesh(InActor);
		BindActorDelegates(InActor);
	}

	if (AttachmentItem->ChildrenActors.Num() != ChildrenActors.Num())
	{
		InvalidateBakedStaticMesh(InActor);
	}

	AttachmentItem->ChildrenActors.Empty(ChildrenActors.Num());
	for (AActor* ChildActor : ChildrenActors)
	{
		AttachmentItem->ChildrenActors.Add(ChildActor);
		UpdateActorAttachment(ChildActor, InActor);
	}
}

void UCEClonerComponent::BindActorDelegates(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

#if WITH_EDITOR
	// Hide new actor when attached to cloner
	InActor->SetIsTemporarilyHiddenInEditor(true);
#endif
	InActor->SetActorHiddenInGame(true);

	InActor->OnDestroyed.RemoveAll(this);
	InActor->OnDestroyed.AddDynamic(this, &UCEClonerComponent::OnActorDestroyed);

#if WITH_EDITOR
	// Detect static mesh change
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	InActor->GetComponents(StaticMeshComponents, false);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (!StaticMeshComponent->OnStaticMeshChanged().IsBoundToObject(this))
		{
			StaticMeshComponent->OnStaticMeshChanged().AddUObject(this, &UCEClonerComponent::OnMeshChanged, InActor);
		}
	}
#endif

	// Detect dynamic mesh change
	TArray<UDynamicMeshComponent*> DynamicMeshComponents;
	InActor->GetComponents(DynamicMeshComponents, false);
	for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshComponents)
	{
		if (!DynamicMeshComponent->OnMeshChanged.IsBoundToObject(this))
		{
			UStaticMeshComponent* NullComponent = nullptr;
			DynamicMeshComponent->OnMeshChanged.AddUObject(this, &UCEClonerComponent::OnMeshChanged, NullComponent, InActor);
		}
	}
}

void UCEClonerComponent::UnbindActorDelegates(AActor* InActor) const
{
	if (!InActor)
	{
		return;
	}

#if WITH_EDITOR
	// Show new actor when detached to cloner
	InActor->SetIsTemporarilyHiddenInEditor(false);
#endif
	InActor->SetActorHiddenInGame(false);

	InActor->OnDestroyed.RemoveAll(this);

#if WITH_EDITOR
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	InActor->GetComponents(StaticMeshComponents, false);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		StaticMeshComponent->OnStaticMeshChanged().RemoveAll(this);
	}
#endif

	TArray<UDynamicMeshComponent*> DynamicMeshComponents;
	InActor->GetComponents(DynamicMeshComponents, false);
	for (UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshComponents)
	{
		DynamicMeshComponent->OnMeshChanged.RemoveAll(this);
	}
}

void UCEClonerComponent::OnActorDestroyed(AActor* InDestroyedActor)
{
	if (ClonerTree.ItemAttachmentMap.Contains(InDestroyedActor))
	{
		InvalidateBakedStaticMesh(InDestroyedActor);
		UnbindActorDelegates(InDestroyedActor);
		ClonerTree.ItemAttachmentMap.Remove(InDestroyedActor);
		bClonerMeshesDirty = true;
	}
}

#if WITH_EDITOR
void UCEClonerComponent::OnActorPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	OnMaterialChanged(InObject);
}
#endif

void UCEClonerComponent::OnMaterialChanged(UObject* InObject)
{
	if (!IsValid(InObject))
	{
		return;
	}

	const AActor* ClonerActor = GetOwner();

	if (!ClonerActor)
	{
		return;
	}

	AActor* ActorChanged = Cast<AActor>(InObject);
	ActorChanged = ActorChanged ? ActorChanged : InObject->GetTypedOuter<AActor>();

	if (!ActorChanged)
	{
		return;
	}

	FCEClonerAttachmentItem* AttachmentItem = ClonerTree.ItemAttachmentMap.Find(ActorChanged);

	if (!AttachmentItem)
	{
		return;
	}

	int32 MatIdx = 0;
	TArray<TWeakObjectPtr<UMaterialInterface>> NewMaterials;
	bool bMaterialChanged = false;

	TArray<UDynamicMeshComponent*> DynamicMeshComponents;
	ActorChanged->GetComponents(DynamicMeshComponents, false);
	for (const UDynamicMeshComponent* DynamicMeshComponent : DynamicMeshComponents)
	{
		for (UMaterialInterface* Material : DynamicMeshComponent->GetMaterials())
		{
			if (!AttachmentItem->BakedMaterials.IsValidIndex(MatIdx) || AttachmentItem->BakedMaterials[MatIdx] != Material)
			{
				bMaterialChanged = true;
			}
			NewMaterials.Add(Material);
			MatIdx++;
		}
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
	ActorChanged->GetComponents(SkeletalMeshComponents, false);
	for (const USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		for (UMaterialInterface* Material : SkeletalMeshComponent->GetMaterials())
		{
			if (!AttachmentItem->BakedMaterials.IsValidIndex(MatIdx) || AttachmentItem->BakedMaterials[MatIdx] != Material)
			{
				bMaterialChanged = true;
			}
			NewMaterials.Add(Material);
			MatIdx++;
		}
	}

	TArray<UBrushComponent*> BrushComponents;
	ActorChanged->GetComponents(BrushComponents, false);
	for (const UBrushComponent* BrushComponent : BrushComponents)
	{
		for (int32 Idx = 0; Idx < BrushComponent->GetNumMaterials(); Idx++)
		{
			if (!AttachmentItem->BakedMaterials.IsValidIndex(MatIdx) || AttachmentItem->BakedMaterials[MatIdx] != BrushComponent->GetMaterial(Idx))
			{
				bMaterialChanged = true;
			}
			NewMaterials.Add(BrushComponent->GetMaterial(Idx));
			MatIdx++;
		}
	}

	TArray<UProceduralMeshComponent*> ProceduralMeshComponents;
	ActorChanged->GetComponents(ProceduralMeshComponents, false);
	for (const UProceduralMeshComponent* ProceduralMeshComponent : ProceduralMeshComponents)
	{
		if (ProceduralMeshComponent->GetNumSections() == 0)
		{
			continue;
		}
		for (UMaterialInterface* Material : ProceduralMeshComponent->GetMaterials())
		{
			if (!AttachmentItem->BakedMaterials.IsValidIndex(MatIdx) || AttachmentItem->BakedMaterials[MatIdx] != Material)
			{
				bMaterialChanged = true;
			}
			NewMaterials.Add(Material);
			MatIdx++;
		}
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	ActorChanged->GetComponents(StaticMeshComponents, false);
	for (const UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		for (UMaterialInterface* Material : StaticMeshComponent->GetMaterials())
		{
			if (!AttachmentItem->BakedMaterials.IsValidIndex(MatIdx) || AttachmentItem->BakedMaterials[MatIdx] != Material)
			{
				bMaterialChanged = true;
			}
			NewMaterials.Add(Material);
			MatIdx++;
		}
	}

	if (bMaterialChanged)
	{
		UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Detected material change for %s"), *ClonerActor->GetActorNameOrLabel(), *ActorChanged->GetActorNameOrLabel());

		if (NewMaterials.Num() == AttachmentItem->BakedMaterials.Num())
		{
			AttachmentItem->BakedMaterials = NewMaterials;
		}
		else
		{
			AttachmentItem->MeshStatus = ECEClonerAttachmentStatus::Outdated;
		}

		InvalidateBakedStaticMesh(ActorChanged);
	}
}

void UCEClonerComponent::OnTransformUpdated(USceneComponent* InUpdatedComponent, EUpdateTransformFlags InUpdateTransformFlags, ETeleportType InTeleport)
{
	InvalidateBakedStaticMesh(InUpdatedComponent->GetOwner());
}

void UCEClonerComponent::OnMeshChanged(UStaticMeshComponent*, AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	const AActor* ClonerActor = GetOwner();

	if (!ClonerActor)
	{
		return;
	}

	if (FCEClonerAttachmentItem* Item = ClonerTree.ItemAttachmentMap.Find(InActor))
	{
		UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Detected mesh change for %s"), *ClonerActor->GetActorNameOrLabel(), *InActor->GetActorNameOrLabel());

		Item->MeshStatus = ECEClonerAttachmentStatus::Outdated;
        InvalidateBakedStaticMesh(InActor);
		DirtyItemAttachments.Add(Item);
	}
}

void UCEClonerComponent::GetOrderedRootActors(TArray<AActor*>& OutActors) const
{
	const AActor* ClonerActor = GetOwner();
	if (!ClonerActor)
	{
		return;
	}

	UCEClonerSubsystem* ClonerSubsystem = UCEClonerSubsystem::Get();
	if (!ClonerSubsystem)
	{
		return;
	}

	const UCEClonerSubsystem::FOnGetOrderedActors& CustomActorResolver = ClonerSubsystem->GetCustomActorResolver();

	if (CustomActorResolver.IsBound())
	{
		OutActors = CustomActorResolver.Execute(ClonerActor);
	}
	else
	{
		ClonerActor->GetAttachedActors(OutActors, true, false);
	}
}

AActor* UCEClonerComponent::GetRootActor(AActor* InActor) const
{
	if (InActor == nullptr)
	{
		return nullptr;
	}
	if (const FCEClonerAttachmentItem* Item = ClonerTree.ItemAttachmentMap.Find(InActor))
	{
		if (Item->bRootItem)
		{
			return InActor;
		}

		return GetRootActor(Item->ParentActor.Get());
	}
	return nullptr;
}

void UCEClonerComponent::InvalidateBakedStaticMesh(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	if (const FCEClonerAttachmentItem* FoundItem = ClonerTree.ItemAttachmentMap.Find(InActor))
	{
		if (FoundItem->bRootItem || !FoundItem->ParentActor.IsValid())
		{
			const int32 RootIdx = ClonerTree.RootActors.Find(InActor);
			if (ClonerTree.MergedBakedMeshes.IsValidIndex(RootIdx))
			{
				ClonerTree.MergedBakedMeshes[RootIdx] = nullptr;
				bClonerMeshesDirty = true;
			}
		}
		else
		{
			InvalidateBakedStaticMesh(FoundItem->ParentActor.Get());
		}
	}
}

void UCEClonerComponent::UpdateDirtyMeshesAsync()
{
	if (bClonerMeshesUpdating)
	{
		return;
	}

	bClonerMeshesUpdating = true;
	TSet<FCEClonerAttachmentItem*> UpdateItems = DirtyItemAttachments;
	DirtyItemAttachments.Empty();

	// Update baked dynamic meshes on other thread
	TWeakObjectPtr<UCEClonerComponent> ThisWeak(this);
	Async(EAsyncExecution::TaskGraph, [ThisWeak, UpdateItems]()
	{
		UCEClonerComponent* This = ThisWeak.Get();

		if (!This)
		{
			return;
		}

		// update actor baked dynamic meshes
		bool bSuccess = true;
		for (FCEClonerAttachmentItem* Item : UpdateItems)
		{
			if (!Item || !Item->ItemActor.IsValid())
			{
				continue;
			}

			if (IsGarbageCollectingAndLockingUObjectHashTables())
			{
				bSuccess = false;
				This->DirtyItemAttachments.Add(Item);
				continue;
			}

			This->UpdateActorBakedDynamicMesh(Item->ItemActor.Get());
		}

		// Create baked static mesh on main thread (required)
		Async(EAsyncExecution::TaskGraphMainThread, [ThisWeak, &bSuccess]()
		{
			UCEClonerComponent* This = ThisWeak.Get();

			if (!This)
			{
				return;
			}

			if (!bSuccess)
			{
				This->OnDirtyMeshesUpdated(false);
				return;
			}

			// Update actors baked static mesh
			for (int32 Idx = 0; Idx < This->ClonerTree.RootActors.Num(); Idx++)
			{
				if (IsGarbageCollectingAndLockingUObjectHashTables())
				{
					bSuccess = false;
					break;
				}

				const UStaticMesh* RootStaticMesh = This->ClonerTree.MergedBakedMeshes[Idx].Get();

				if (!RootStaticMesh)
				{
					AActor* RootActor = This->ClonerTree.RootActors[Idx].Get();
					This->UpdateRootActorBakedStaticMesh(RootActor);
				}
			}

			// update niagara asset
			This->OnDirtyMeshesUpdated(bSuccess);
		});
	});
}

void UCEClonerComponent::OnDirtyMeshesUpdated(bool bInSuccess)
{
	bClonerMeshesUpdating = false;

	// Update niagara parameters
	if (bInSuccess)
	{
		UpdateClonerMeshes();
	}
}

void UCEClonerComponent::UpdateActorBakedDynamicMesh(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	FCEClonerAttachmentItem* AttachmentItem = ClonerTree.ItemAttachmentMap.Find(InActor);

	if (!AttachmentItem || AttachmentItem->MeshStatus != ECEClonerAttachmentStatus::Outdated)
	{
		return;
	}

	AActor* ClonerActor = GetOwner();

	if (!ClonerActor)
	{
		return;
	}

	AttachmentItem->MeshStatus = ECEClonerAttachmentStatus::Updating;

	UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Updating baked actor mesh %s"), *ClonerActor->GetActorNameOrLabel(), *InActor->GetActorNameOrLabel());

	using namespace UE::Geometry;

	TArray<FDynamicMesh3> ConvertedMeshes;
	TArray<TWeakObjectPtr<UMaterialInterface>> MeshesMaterials;

	const FTransform& SourceTransform = InActor->GetActorTransform();
	UDynamicMesh* OutputDynamicMesh = NewObject<UDynamicMesh>();

	static FGeometryScriptCopyMeshFromAssetOptions OutputMeshOptions;
	OutputMeshOptions.bIgnoreRemoveDegenerates = false;
	OutputMeshOptions.bRequestTangents = false;
	OutputMeshOptions.bApplyBuildSettings = false;

	// Dynamic mesh components
	{
		TArray<UDynamicMeshComponent*> Components;
		InActor->GetComponents(Components, false);
		for (UDynamicMeshComponent* Component : Components)
		{
			const UDynamicMesh* DynamicMesh = Component->GetDynamicMesh();
			// Transform the new mesh relative to the component
			const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
			MeshesMaterials.Append(Component->GetMaterials());
			// Create a copy
			DynamicMesh->ProcessMesh([&ConvertedMeshes, RelativeTransform](const FDynamicMesh3& EditMesh)
			{
				FDynamicMesh3 CopyMesh = EditMesh;
				MeshTransforms::ApplyTransform(CopyMesh, RelativeTransform);
				ConvertedMeshes.Add(MoveTemp(CopyMesh));
			});
		}
	}

	// Skeletal mesh components
	{
		static FGeometryScriptMeshReadLOD SkeletalMeshLOD;
		SkeletalMeshLOD.LODType = EGeometryScriptLODType::SourceModel;

		TArray<USkeletalMeshComponent*> Components;
		InActor->GetComponents(Components, false);
		for (USkeletalMeshComponent* Component : Components)
		{
			USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset();

			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(SkeletalMesh, OutputDynamicMesh, OutputMeshOptions, SkeletalMeshLOD, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				MeshesMaterials.Append(Component->GetMaterials());
				OutputDynamicMesh->EditMesh([&ConvertedMeshes, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					ConvertedMeshes.Add(MoveTemp(EditMesh));
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);
			}
		}
	}

	// Brush components
	{
		static const FGeometryScriptCopyMeshFromComponentOptions Options;
		static FTransform Transform;

		TArray<UBrushComponent*> Components;
		InActor->GetComponents(Components, false);
		for (UBrushComponent* Component : Components)
		{
			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;
			UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(Component, OutputDynamicMesh, Options, false, Transform, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				for (int32 MatIdx = 0; MatIdx < Component->GetNumMaterials(); MatIdx++)
				{
					MeshesMaterials.Add(Component->GetMaterial(MatIdx));
				}
				OutputDynamicMesh->EditMesh([&ConvertedMeshes, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					ConvertedMeshes.Add(MoveTemp(EditMesh));
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);
			}
		}
	}

	// Procedural mesh components
	{
		TArray<UProceduralMeshComponent*> Components;
		InActor->GetComponents(Components, false);
		for (UProceduralMeshComponent* Component : Components)
		{
			const int32 SectionCount = Component->GetNumSections();
			if (SectionCount == 0)
			{
				continue;
			}

			// Transform the new mesh relative to the component
			const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);

			FDynamicMesh3 Mesh;
			Mesh.EnableAttributes();
			Mesh.Attributes()->EnablePrimaryColors();
			Mesh.Attributes()->EnableMaterialID();
			Mesh.Attributes()->SetNumNormalLayers(1);
			Mesh.Attributes()->SetNumUVLayers(1);
			Mesh.Attributes()->SetNumPolygroupLayers(1);
			Mesh.Attributes()->EnableTangents();

			FDynamicMeshColorOverlay* ColorOverlay = Mesh.Attributes()->PrimaryColors();
			FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
			FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
			FDynamicMeshMaterialAttribute* MaterialAttr = Mesh.Attributes()->GetMaterialID();
			FDynamicMeshPolygroupAttribute* GroupAttr = Mesh.Attributes()->GetPolygroupLayer(0);
			FDynamicMeshNormalOverlay* TangentOverlay = Mesh.Attributes()->PrimaryTangents();

			for (int32 SectionIdx = 0; SectionIdx < SectionCount; SectionIdx++)
			{
				if (FProcMeshSection* Section = Component->GetProcMeshSection(SectionIdx))
				{
					if (Section->bSectionVisible)
					{
						TArray<int32> VtxIds;
						TArray<int32> NormalIds;
						TArray<int32> ColorIds;
						TArray<int32> UVIds;
						TArray<int32> TaIds;

						// copy vertices data (position, normal, color, UV, tangent)
						for (FProcMeshVertex& SectionVertex : Section->ProcVertexBuffer)
						{
							int32 VId = Mesh.AppendVertex(SectionVertex.Position);
							VtxIds.Add(VId);

							int32 NId = NormalOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Normal));
							NormalIds.Add(NId);

							int32 CId = ColorOverlay->AppendElement(static_cast<FVector4f>(SectionVertex.Color));
							ColorIds.Add(CId);

							int32 UVId = UVOverlay->AppendElement(static_cast<FVector2f>(SectionVertex.UV0));
							UVIds.Add(UVId);

							int32 TaId = TangentOverlay->AppendElement(static_cast<FVector3f>(SectionVertex.Tangent.TangentX));
							TaIds.Add(TaId);
						}

						// copy tris data
						if (Section->ProcIndexBuffer.Num() % 3 != 0)
						{
							continue;
						}
						for (int32 Idx = 0; Idx < Section->ProcIndexBuffer.Num(); Idx+=3)
						{
							int32 VIdx1 = Section->ProcIndexBuffer[Idx];
							int32 VIdx2 = Section->ProcIndexBuffer[Idx + 1];
							int32 VIdx3 = Section->ProcIndexBuffer[Idx + 2];

							int32 VId1 = VtxIds[VIdx1];
							int32 VId2 = VtxIds[VIdx2];
							int32 VId3 = VtxIds[VIdx3];

							int32 TId = Mesh.AppendTriangle(VId1, VId2, VId3, SectionIdx);
							if (TId < 0)
							{
								continue;
							}

							NormalOverlay->SetTriangle(TId, FIndex3i(NormalIds[VIdx1], NormalIds[VIdx2], NormalIds[VIdx3]), true);
							ColorOverlay->SetTriangle(TId, FIndex3i(ColorIds[VIdx1], ColorIds[VIdx2], ColorIds[VIdx3]), true);
							UVOverlay->SetTriangle(TId, FIndex3i(UVIds[VIdx1], UVIds[VIdx2], UVIds[VIdx3]), true);
							TangentOverlay->SetTriangle(TId, FIndex3i(TaIds[VIdx1], TaIds[VIdx2], TaIds[VIdx3]), true);

							MaterialAttr->SetValue(TId, SectionIdx);
							GroupAttr->SetValue(TId, SectionIdx);
						}
					}
				}
			}

			if (Mesh.TriangleCount() > 0)
			{
				MeshTransforms::ApplyTransform(Mesh, RelativeTransform);
				MeshesMaterials.Append(Component->GetMaterials());
				ConvertedMeshes.Add(MoveTemp(Mesh));
			}
		}
	}

	// Static mesh components
	{
		static FGeometryScriptMeshReadLOD StaticMeshLOD;
		StaticMeshLOD.LODType = EGeometryScriptLODType::RenderData;

		TArray<UStaticMeshComponent*> Components;
		InActor->GetComponents(Components, false);
		for (UStaticMeshComponent* Component : Components)
		{
			UStaticMesh* StaticMesh = Component->GetStaticMesh();
			// convert to dynamic mesh
			EGeometryScriptOutcomePins OutResult;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, OutputDynamicMesh, OutputMeshOptions, StaticMeshLOD, OutResult);
			if (OutResult == EGeometryScriptOutcomePins::Success)
			{
				// Transform the new mesh relative to the component
				const FTransform RelativeTransform = Component->GetComponentTransform().GetRelativeTransform(SourceTransform);
				MeshesMaterials.Append(Component->GetMaterials());
				OutputDynamicMesh->EditMesh([&ConvertedMeshes, RelativeTransform](FDynamicMesh3& EditMesh)
				{
					MeshTransforms::ApplyTransform(EditMesh, RelativeTransform);
					ConvertedMeshes.Add(MoveTemp(EditMesh));
					// replace by empty mesh
					FDynamicMesh3 EmptyMesh;
					EditMesh = MoveTemp(EmptyMesh);
				}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);
			}
		}
	}

	// Lets combine all meshes components from this actor together
	OutputDynamicMesh->EditMesh([&ConvertedMeshes](FDynamicMesh3& InMergedMesh)
	{
		InMergedMesh.Clear();
		InMergedMesh.EnableAttributes();
		InMergedMesh.Attributes()->SetNumNormalLayers(1);

		FDynamicMeshEditor Editor(&InMergedMesh);
		FGeometryScriptAppendMeshOptions AppendOptions;
		AppendOptions.CombineMode = EGeometryScriptCombineAttributesMode::EnableAllMatching;
		int32 MaterialCount = 0;

		// Convert meshes
		for (int32 MeshIdx = 0; MeshIdx < ConvertedMeshes.Num(); MeshIdx++)
		{
			const FDynamicMesh3& ConvertedMesh = ConvertedMeshes[MeshIdx];

			// Enable matching attributes & append mesh
			FMeshIndexMappings TmpMappings;
			AppendOptions.UpdateAttributesForCombineMode(InMergedMesh, ConvertedMesh);
			Editor.AppendMesh(&ConvertedMesh, TmpMappings);

			// Fix triangles materials linking
			if (ConvertedMesh.HasAttributes() && ConvertedMesh.Attributes()->HasMaterialID())
			{
				const FDynamicMeshMaterialAttribute* FromMaterialIDAttrib = ConvertedMesh.Attributes()->GetMaterialID();
				FDynamicMeshMaterialAttribute* ToMaterialIDAttrib = InMergedMesh.Attributes()->GetMaterialID();
				TMap<int32, int32> MaterialMap;
				for (const TPair<int32, int32>& FromToTId : TmpMappings.GetTriangleMap().GetForwardMap())
				{
					const int32 FromMatId = FromMaterialIDAttrib->GetValue(FromToTId.Key);
					const int32 ToMatId = FromMatId + MaterialCount;
					MaterialMap.Add(FromMatId, ToMatId);
					ToMaterialIDAttrib->SetNewValue(FromToTId.Value, ToMatId);
				}
				MaterialCount += MaterialMap.Num();
			}
		}

		if (InMergedMesh.TriangleCount() > 0)
		{
			// Merge shared edges
			FMergeCoincidentMeshEdges WeldOp(&InMergedMesh);
			WeldOp.Apply();
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

	AttachmentItem->BakedMesh = OutputDynamicMesh;
	AttachmentItem->BakedMaterials = MeshesMaterials;
	// Was the mesh invalidated during the update process ?
	AttachmentItem->MeshStatus = AttachmentItem->MeshStatus == ECEClonerAttachmentStatus::Outdated ? ECEClonerAttachmentStatus::Outdated : ECEClonerAttachmentStatus::Updated;
	InvalidateBakedStaticMesh(AttachmentItem->ItemActor.Get());
}

void UCEClonerComponent::UpdateRootActorBakedStaticMesh(AActor* InRootActor)
{
	if (!InRootActor)
	{
		return;
	}

	const int32 RootIdx = ClonerTree.RootActors.Find(InRootActor);

	if (RootIdx == INDEX_NONE)
	{
		return;
	}

	const FCEClonerAttachmentItem* RootAttachmentItem = ClonerTree.ItemAttachmentMap.Find(InRootActor);

	if (!RootAttachmentItem)
	{
		return;
	}

	const AActor* ClonerActor = GetOwner();

	if (!ClonerActor)
	{
		return;
	}

	UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Updating root merged baked mesh %s"), *ClonerActor->GetActorNameOrLabel(), *InRootActor->GetActorNameOrLabel());

	using namespace UE::Geometry;

#if WITH_EDITOR
	TSet<int32> MaterialsMissingNiagaraUsageFlag;
#endif

	TArray<FCEClonerAttachmentItem*> AttachmentItems;
	GetActorAttachmentItems(InRootActor, AttachmentItems);

	// Lets combine all dynamic meshes from item attachments and self into one mesh
	UDynamicMesh* MergedAttachmentMesh = NewObject<UDynamicMesh>();
	TArray<TObjectPtr<UMaterialInterface>> CombineMaterials;
	MergedAttachmentMesh->EditMesh([&AttachmentItems, &CombineMaterials, this
#if WITH_EDITOR
		, &MaterialsMissingNiagaraUsageFlag
#endif
		](FDynamicMesh3& InMergedMesh)
	{
		InMergedMesh.Clear();
		InMergedMesh.EnableAttributes();
		InMergedMesh.Attributes()->SetNumNormalLayers(1);

		FDynamicMeshEditor Editor(&InMergedMesh);
		FGeometryScriptAppendMeshOptions AppendOptions;
		AppendOptions.CombineMode = EGeometryScriptCombineAttributesMode::EnableAllMatching;
		int32 MaterialCount = 0;

		// Convert meshes
		for (int32 MeshIdx = 0; MeshIdx < AttachmentItems.Num(); MeshIdx++)
		{
			FCEClonerAttachmentItem* AttachmentItem = AttachmentItems[MeshIdx];
			if (!AttachmentItem)
			{
				continue;
			}

			const UDynamicMesh* BakedDynamicMesh = AttachmentItem->BakedMesh;
			if (!BakedDynamicMesh)
			{
				continue;
			}

			BakedDynamicMesh->ProcessMesh([&AttachmentItem, &CombineMaterials, &MaterialCount, &AppendOptions, &InMergedMesh, &Editor, this
#if WITH_EDITOR
				, &MaterialsMissingNiagaraUsageFlag
#endif
				](const FDynamicMesh3& InProcessMesh)
			{
				FDynamicMesh3 ConvertedMesh = InProcessMesh;

				if (AttachmentItem->ParentActor.IsValid())
				{
					const FTransform& ParentTransform = AttachmentItem->ParentActor->GetTransform();
					const FTransform ParentChildTransform = AttachmentItem->ItemActor->GetTransform().GetRelativeTransform(ParentTransform);
					MeshTransforms::ApplyTransform(ConvertedMesh, ParentChildTransform);
				}
				else
				{
					MeshTransforms::ApplyTransform(ConvertedMesh, FTransform::Identity);
				}

				// Enable matching attributes & append mesh
				FMeshIndexMappings TmpMappings;
				AppendOptions.UpdateAttributesForCombineMode(InMergedMesh, ConvertedMesh);
				Editor.AppendMesh(&ConvertedMesh, TmpMappings);

				// Fix triangles materials linking
				if (ConvertedMesh.HasAttributes() && ConvertedMesh.Attributes()->HasMaterialID())
				{
					const FDynamicMeshMaterialAttribute* FromMaterialIDAttrib = ConvertedMesh.Attributes()->GetMaterialID();
					FDynamicMeshMaterialAttribute* ToMaterialIDAttrib = InMergedMesh.Attributes()->GetMaterialID();
					TMap<int32, int32> MaterialMap;
					for (const TPair<int32, int32>& FromToTId : TmpMappings.GetTriangleMap().GetForwardMap())
					{
						const int32 FromMatId = FromMaterialIDAttrib->GetValue(FromToTId.Key);
						const int32 ToMatId = FromMatId + MaterialCount;
						MaterialMap.Add(FromMatId, ToMatId);
						ToMaterialIDAttrib->SetNewValue(FromToTId.Value, ToMatId);
					}
					MaterialCount += MaterialMap.Num();

					for (TWeakObjectPtr<UMaterialInterface>& BakedMaterial : AttachmentItem->BakedMaterials)
					{
						UMaterialInterface* MaterialInterface = BakedMaterial.Get();
						int32 MatIdx = CombineMaterials.Add(MaterialInterface);
#if WITH_EDITOR
						if (MaterialInterface)
						{
							const UMaterial* Material = MaterialInterface->GetMaterial();
							if (Material && !Material->GetUsageByFlag(EMaterialUsage::MATUSAGE_NiagaraMeshParticles))
							{
								MaterialsMissingNiagaraUsageFlag.Add(MatIdx);
							}
						}
#endif
					}
				}
			});
		}
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, true);

#if WITH_EDITOR
	if (!MaterialsMissingNiagaraUsageFlag.IsEmpty())
	{
		int32 ReadOnlyMaterialCount = 0;

		for (const int32 MaterialIdx : MaterialsMissingNiagaraUsageFlag)
		{
			UMaterialInterface* Material = CombineMaterials[MaterialIdx];
			if (!Material)
			{
				continue;
			}

			if (!IsMaterialDirtyable(Material))
			{
				ReadOnlyMaterialCount++;
				CombineMaterials[MaterialIdx] = nullptr;
				UE_LOG(LogCEClonerComponent, Warning, TEXT("%s : The following materials (%s) on actor (%s) does not have the required usage flag (bUsedWithNiagaraMeshParticles) to work with the cloner, this material cannot be dirtied due to its read-only location, skipping material and proceeding"), *ClonerActor->GetActorNameOrLabel(), Material ? *Material->GetMaterial()->GetPathName() : TEXT("Invalid Material"), *InRootActor->GetActorNameOrLabel());
			}
			else
			{
				UE_LOG(LogCEClonerComponent, Log, TEXT("%s : The following materials (%s) on actor (%s) does not have the required usage flag (bUsedWithNiagaraMeshParticles) to work with the cloner, dirtying material and proceeding, please save the asset for working runtime result"), *ClonerActor->GetActorNameOrLabel(), Material ? *Material->GetMaterial()->GetPathName() : TEXT("Invalid Material"), *InRootActor->GetActorNameOrLabel());
			}
		}

		if (ReadOnlyMaterialCount > 0)
		{
			FNotificationInfo NotificationInfo(FText::Format(LOCTEXT("MaterialsMissingUsageFlag", "Detected {0} read-only material(s) with missing niagara usage flag required to work properly with cloner (See logs)"), ReadOnlyMaterialCount));
			NotificationInfo.ExpireDuration = 5.f;
			NotificationInfo.bFireAndForget = true;
			NotificationInfo.Image = FAppStyle::GetBrush("Icons.WarningWithColor");
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
	}
#endif

	// Convert merged mesh to static mesh
	UStaticMesh* BakedAttachmentMesh = NewObject<UStaticMesh>();
	if (MergedAttachmentMesh->GetTriangleCount())
	{
		// export options
		FGeometryScriptCopyMeshToAssetOptions AssetOptions;
		AssetOptions.bReplaceMaterials = true;
		AssetOptions.bEnableRecomputeNormals = false;
		AssetOptions.bEnableRecomputeTangents = false;
		AssetOptions.bEnableRemoveDegenerates = true;
		AssetOptions.bEmitTransaction = false;
		AssetOptions.bApplyNaniteSettings = false;
		AssetOptions.bDeferMeshPostEditChange = false;
		AssetOptions.NewMaterials = CombineMaterials;
		// LOD options
		static FGeometryScriptMeshWriteLOD TargetLOD;
		TargetLOD.LODIndex = 0;
		TargetLOD.bWriteHiResSource = false;
		// result
		EGeometryScriptOutcomePins OutResult;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(MergedAttachmentMesh, BakedAttachmentMesh, AssetOptions, TargetLOD, OutResult);

		// Fix camera culling and avoid killing particles when bounds partially in view
		const float NewRadiusBounds = BakedAttachmentMesh->GetBounds().SphereRadius * 2;
		BakedAttachmentMesh->SetExtendedBounds(FBox(-FVector(NewRadiusBounds), FVector(NewRadiusBounds)));

#if WITH_EDITORONLY_DATA
		// Compute normals and tangents
		FMeshDescription* MeshDescription = BakedAttachmentMesh->GetMeshDescription(0);
		FStaticMeshOperations::ComputeTriangleTangentsAndNormals(*MeshDescription);
		FStaticMeshOperations::ComputeTangentsAndNormals(*MeshDescription, EComputeNTBsFlags::Normals);
#endif

		bClonerMeshesDirty = true;
	}

	MergedAttachmentMesh->MarkAsGarbage();
	ClonerTree.MergedBakedMeshes[RootIdx] = BakedAttachmentMesh;
}

void UCEClonerComponent::GetActorAttachmentItems(AActor* InActor, TArray<FCEClonerAttachmentItem*>& OutAttachmentItems)
{
	if (!InActor)
	{
		return;
	}

	FCEClonerAttachmentItem* AttachmentItem = ClonerTree.ItemAttachmentMap.Find(InActor);

	if (!AttachmentItem)
	{
		return;
	}

	OutAttachmentItems.Add(AttachmentItem);

	for (TWeakObjectPtr<AActor>& ChildActor : AttachmentItem->ChildrenActors)
	{
		if (ChildActor.IsValid())
		{
			GetActorAttachmentItems(ChildActor.Get(), OutAttachmentItems);
		}
	}
}

bool UCEClonerComponent::IsAllMergedMeshesValid() const
{
	for (const TObjectPtr<UStaticMesh>& MergedMesh : ClonerTree.MergedBakedMeshes)
	{
		if (!MergedMesh)
		{
			return false;
		}
	}
	return true;
}

bool UCEClonerComponent::IsMaterialDirtyable(const UMaterialInterface* InMaterial) const
{
	const UMaterial* BaseMaterial = InMaterial->GetMaterial();
	const FString ContentFolder = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());

	const UPackage* MaterialPackage = BaseMaterial->GetPackage();
	const FPackagePath& LoadedPath = MaterialPackage->GetLoadedPath();
	const FString PackagePath = FPaths::ConvertRelativePathToFull(LoadedPath.GetLocalFullPath());
	const FString MaterialPath = BaseMaterial->GetPathName();

	const bool bTransientPackage = MaterialPackage == GetTransientPackage() || MaterialPath.StartsWith("/Temp/");
	const bool bContentFolder = PackagePath.StartsWith(ContentFolder);

	return bTransientPackage || bContentFolder;
}

void UCEClonerComponent::UpdateClonerMeshes()
{
	const AActor* ClonerActor = GetOwner();
	if (!ClonerActor)
	{
		return;
	}

	if (!bClonerMeshesDirty)
	{
		return;
	}

	if (!GetAsset() || !ActiveLayout)
	{
		return;
	}

	UNiagaraMeshRendererProperties* MeshRenderer = ActiveLayout->GetMeshRenderer();

	if (!MeshRenderer)
	{
		UE_LOG(LogCEClonerComponent, Warning, TEXT("%s : Invalid mesh renderer for cloner system"), *ClonerActor->GetActorNameOrLabel());
		return;
	}

	// Set new number of meshes in renderer
	static const FName MeshNumName(TEXT("MeshNum"));
	SetIntParameter(MeshNumName, ClonerTree.MergedBakedMeshes.Num());

	// Resize mesh array properly
	if (MeshRenderer->Meshes.Num() > ClonerTree.MergedBakedMeshes.Num())
	{
		MeshRenderer->Meshes.SetNum(ClonerTree.MergedBakedMeshes.Num());
	}

	// Set baked meshes in mesh renderer array
	for (int32 Idx = 0; Idx < ClonerTree.MergedBakedMeshes.Num(); Idx++)
	{
		UStaticMesh* StaticMesh = ClonerTree.MergedBakedMeshes[Idx];
		FNiagaraMeshRendererMeshProperties& MeshProperties = !MeshRenderer->Meshes.IsValidIndex(Idx) ? MeshRenderer->Meshes.AddDefaulted_GetRef() : MeshRenderer->Meshes[Idx];
		MeshProperties.Mesh = StaticMesh && StaticMesh->GetNumTriangles(0) > 0 ? StaticMesh : nullptr;

		if (ClonerTree.RootActors.IsValidIndex(Idx))
		{
			if (const FCEClonerAttachmentItem* Item = ClonerTree.ItemAttachmentMap.Find(ClonerTree.RootActors[Idx]))
			{
				MeshProperties.Rotation = Item->ActorTransform.Rotator();
				MeshProperties.Scale = Item->ActorTransform.GetScale3D();
			}
		}
	}

#if WITH_EDITORONLY_DATA
	MeshRenderer->OnMeshChanged();
#endif

	bClonerMeshesDirty = !DirtyItemAttachments.IsEmpty();

	UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Cloner mesh updated %i"), *ClonerActor->GetActorNameOrLabel(), ClonerTree.MergedBakedMeshes.Num());

	OnClonerMeshUpdated.Broadcast(this);
}

bool UCEClonerComponent::SetClonerActiveLayout(UCEClonerLayoutBase* InLayout)
{
	if (!InLayout)
	{
		return false;
	}

	const AActor* ClonerActor = GetOwner();
	if (!ClonerActor)
	{
		return false;
	}

	if (!InLayout->IsLayoutLoaded())
	{
		// Load system
		if (!InLayout->LoadLayout())
		{
			UE_LOG(LogCEClonerComponent, Warning, TEXT("%s : Cloner layout system failed to load %s - %s"), *GetOwner()->GetActorNameOrLabel(), *InLayout->GetLayoutName().ToString(), *InLayout->GetLayoutAssetPath());
			return false;
		}

		UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Cloner layout system loaded %s - %s"), *GetOwner()->GetActorNameOrLabel(), *InLayout->GetLayoutName().ToString(), *InLayout->GetLayoutAssetPath());

		OnClonerSystemLoaded.Broadcast(this, InLayout);
	}

	// Copy data interfaces to new layout
	if (ActiveLayout && ActiveLayout->IsLayoutLoaded())
	{
		ActiveLayout->CopyTo(InLayout);
	}

	// Deactivate previous layout
	if (ActiveLayout && ActiveLayout->IsLayoutActive())
	{
		ActiveLayout->DeactivateLayout();
	}

	// Activate new layout
	InLayout->ActivateLayout();

	ActiveLayout = InLayout;
	bClonerMeshesDirty = true;

	UE_LOG(LogCEClonerComponent, Log, TEXT("%s : Cloner layout system changed %s - %s"), *ClonerActor->GetActorNameOrLabel(), *InLayout->GetLayoutName().ToString(), *InLayout->GetLayoutAssetPath());

	return true;
}

#undef LOCTEXT_NAMESPACE
