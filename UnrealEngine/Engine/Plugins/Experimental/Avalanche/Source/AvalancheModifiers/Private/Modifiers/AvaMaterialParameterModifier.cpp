// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaMaterialParameterModifier.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "AvaOutlinerSubsystem.h"
#include "Engine/World.h"
#endif

#define LOCTEXT_NAMESPACE "AvaMaterialParameterModifier"

void FAvaMaterialParameterMap::MatchKeys(const FAvaMaterialParameterMap& InParameterMap)
{
	// Remove key not tracked anymore, preserve other values
	for (TMap<FName, float>::TIterator It = ScalarParameters.CreateIterator(); It; ++It)
	{
		if (!InParameterMap.ScalarParameters.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
	// add newly tracked key, preserve other values
	for (const TPair<FName, float>& Pair : InParameterMap.ScalarParameters)
	{
		ScalarParameters.FindOrAdd(Pair.Key);
	}

	// Remove key not tracked anymore, preserve other values
	for (TMap<FName, FLinearColor>::TIterator It = VectorParameters.CreateIterator(); It; ++It)
	{
		if (!InParameterMap.VectorParameters.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
	// add newly tracked key, preserve other values
	for (const TPair<FName, FLinearColor>& Pair : InParameterMap.VectorParameters)
	{
		VectorParameters.FindOrAdd(Pair.Key);
	}

	// Remove key not tracked anymore, preserve other values
	for (TMap<FName, TObjectPtr<UTexture>>::TIterator It = TextureParameters.CreateIterator(); It; ++It)
	{
		if (!InParameterMap.TextureParameters.Contains(It->Key))
		{
			It.RemoveCurrent();
		}
	}
	// add newly tracked key, preserve other values
	for (const TPair<FName, TObjectPtr<UTexture>>& Pair : InParameterMap.TextureParameters)
	{
		TextureParameters.FindOrAdd(Pair.Key);
	}
}

void FAvaMaterialParameterMap::Set(UMaterialInstanceDynamic* InMaterial)
{
	if (!IsValid(InMaterial))
	{
		return;
	}

	// Apply scalar
	for (const TPair<FName, float>& Pair : ScalarParameters)
	{
		InMaterial->SetScalarParameterValue(Pair.Key, Pair.Value);
	}

	// Apply color
	for (const TPair<FName, FLinearColor>& Pair : VectorParameters)
	{
		InMaterial->SetVectorParameterValue(Pair.Key, Pair.Value);
	}

	// Apply TextureParameters
	for (const TPair<FName, TObjectPtr<UTexture>>& Pair : TextureParameters)
	{
		InMaterial->SetTextureParameterValue(Pair.Key, Pair.Value);
	}
}

void FAvaMaterialParameterMap::Get(UMaterialInstanceDynamic* InMaterial)
{
	if (!IsValid(InMaterial))
	{
		return;
	}

	// Get scalar
	for (TPair<FName, float>& Pair : ScalarParameters)
	{
		Pair.Value = InMaterial->K2_GetScalarParameterValue(Pair.Key);
	}

	// Get Color
	for (TPair<FName, FLinearColor>& Pair : VectorParameters)
	{
		Pair.Value = InMaterial->K2_GetVectorParameterValue(Pair.Key);
	}

	// Get Texture
	for (TPair<FName, TObjectPtr<UTexture>>& Pair : TextureParameters)
	{
		Pair.Value = InMaterial->K2_GetTextureParameterValue(Pair.Key);
	}
}

UAvaMaterialParameterModifier::UAvaMaterialParameterModifier()
{
	MaterialClass = UMaterialInstanceDynamic::StaticClass();
}

void UAvaMaterialParameterModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("MaterialParameter"));
	InMetadata.SetCategory(TEXT("Rendering"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Sets specified dynamic materials parameters on an actor and its children"));
#endif

	InMetadata.SetCompatibilityRule([this](const AActor* InActor)->bool
	{
		if (!IsValid(InActor))
		{
			return false;
		}

		// Check actor component has at least one dynamic instance material in its components or children
		const bool bResult = ForEachComponent<UPrimitiveComponent>([](UPrimitiveComponent* InComponent)
		{
			if (InComponent)
			{
				for (int32 Idx = 0; Idx < InComponent->GetNumMaterials(); Idx++)
				{
					const UMaterialInterface* Mat = InComponent->GetMaterial(Idx);
					if (!IsValid(Mat) || !Mat->IsA<UMaterialInstanceDynamic>())
					{
						continue;
					}

					return false;
				}
			}

			return true;
		}
		, EActorModifierCoreComponentType::All
		, EActorModifierCoreLookup::SelfAndAllChildren
		, InActor);

		return !bResult;
	});
}

void UAvaMaterialParameterModifier::OnModifierEnabled(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierEnabled(InReason);

#if WITH_EDITOR
	/** Bind to delegate to detect material changes */
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UAvaMaterialParameterModifier::OnActorPropertyChanged);
#endif
}

void UAvaMaterialParameterModifier::OnModifierDisabled(EActorModifierCoreDisableReason InReason)
{
	Super::OnModifierDisabled(InReason);

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
#endif
}

void UAvaMaterialParameterModifier::OnModifiedActorTransformed()
{
	// Overwrite parent class behaviour don't do anything when moved
}

void UAvaMaterialParameterModifier::RestorePreState()
{
	RestoreMaterialParameters();
}

void UAvaMaterialParameterModifier::Apply()
{
	const AActor* ActorModified = GetModifiedActor();
	if (!ActorModified)
	{
		return;
	}

	// Scan for materials in actors
	ScanActorMaterials();

	SaveMaterialParameters();

	// Set new parameter value to them
	for (TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FAvaMaterialParameterMap>& MaterialParameterPair : SavedMaterialParameters)
	{
		if (UMaterialInstanceDynamic* MID = MaterialParameterPair.Key.Get())
		{
			// Set State
			MaterialParameters.Set(MID);
		}
	}

	Next();
}

#if WITH_EDITOR
void UAvaMaterialParameterModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	static const FName MaterialParametersName = GET_MEMBER_NAME_CHECKED(UAvaMaterialParameterModifier, MaterialParameters);
	static const FName UpdateChildrenName = GET_MEMBER_NAME_CHECKED(UAvaMaterialParameterModifier, bUpdateChildren);

	if (MemberName == MaterialParametersName)
	{
		OnMaterialParametersChanged();
	}
	else if (MemberName == UpdateChildrenName)
	{
		OnUpdateChildrenChanged();
	}
}

void UAvaMaterialParameterModifier::OnActorPropertyChanged(UObject* InObject, FPropertyChangedEvent& InChangeEvent)
{
	if (!InObject)
	{
		return;
	}

	if (const AActor* Actor = Cast<AActor>(InObject))
	{
		if (IsActorSupported(Actor))
		{
			MarkModifierDirty();
		}
	}
	else if (const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InObject))
	{
		if (!GetComponentDynamicMaterials(PrimitiveComponent).IsEmpty())
		{
			MarkModifierDirty();
		}
	}
}
#endif

bool UAvaMaterialParameterModifier::IsActorSupported(const AActor* InActor) const
{
	if (!InActor)
	{
		return false;
	}

	const AActor* ActorModified = GetModifiedActor();
	if (!ActorModified)
	{
		return false;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	InActor->GetComponents(PrimitiveComponents, false);
	const bool bAttachedToModifiedActor = InActor->IsAttachedTo(ActorModified);
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		for (UMaterialInstanceDynamic* MID : GetComponentDynamicMaterials(PrimitiveComponent))
		{
			if (bAttachedToModifiedActor)
			{
				return true;
			}
			else if (SavedMaterialParameters.Contains(MID))
			{
				return true;
			}
		}
	}

	return false;
}

TSet<UMaterialInstanceDynamic*> UAvaMaterialParameterModifier::GetComponentDynamicMaterials(const UPrimitiveComponent* InComponent) const
{
	TSet<UMaterialInstanceDynamic*> Materials;
	if (!InComponent)
	{
		return Materials;
	}

	for (int32 Idx = 0; Idx < InComponent->GetNumMaterials(); Idx++)
	{
		UMaterialInterface* Mat = InComponent->GetMaterial(Idx);
		if (!IsValid(Mat) || !Mat->IsA(MaterialClass))
		{
			continue;
		}

		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Mat))
		{
			Materials.Add(MID);
		}
	}

	return Materials;
}

void UAvaMaterialParameterModifier::SetMaterialParameters(const FAvaMaterialParameterMap& InParameterMap)
{
	MaterialParameters = InParameterMap;
	OnMaterialParametersChanged();
}

void UAvaMaterialParameterModifier::SetUpdateChildren(bool bInUpdateChildren)
{
	if (bUpdateChildren == bInUpdateChildren)
	{
		return;
	}

	bUpdateChildren = bInUpdateChildren;
	OnUpdateChildrenChanged();
}

void UAvaMaterialParameterModifier::OnSceneTreeTrackedActorChildrenChanged(int32 InIdx,
	const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	Super::OnSceneTreeTrackedActorChildrenChanged(InIdx, InPreviousChildrenActors, InNewChildrenActors);

	if (!bUpdateChildren)
	{
		return;
	}

	MarkModifierDirty();
}

void UAvaMaterialParameterModifier::OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx,
	const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors,
	const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors)
{
	// Overwrite parent class behaviour don't do anything when direct children changed
}

void UAvaMaterialParameterModifier::SaveMaterialParameters()
{
	// Save original values
	for (TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FAvaMaterialParameterMap>& MaterialParameterPair : SavedMaterialParameters)
	{
		if (UMaterialInstanceDynamic* MID = MaterialParameterPair.Key.Get())
		{
			MaterialParameterPair.Value.Get(MID);
		}
	}
}

void UAvaMaterialParameterModifier::RestoreMaterialParameters()
{
	// Set original saved values back
	for (TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FAvaMaterialParameterMap>& MaterialParameterPair : SavedMaterialParameters)
	{
		if (UMaterialInstanceDynamic* MID = MaterialParameterPair.Key.Get())
		{
			MaterialParameterPair.Value.Set(MID);
		}
	}
}

void UAvaMaterialParameterModifier::ScanActorMaterials()
{
	// Used to compare with current one to remove materials not tracked anymore
	TSet<UMaterialInstanceDynamic*> PrevScannedMaterials;
	for (TPair<TWeakObjectPtr<UMaterialInstanceDynamic>, FAvaMaterialParameterMap>& MaterialParameterPair : SavedMaterialParameters)
	{
		PrevScannedMaterials.Add(MaterialParameterPair.Key.Get());
	}

	ForEachComponent<UPrimitiveComponent>([this, &PrevScannedMaterials](UPrimitiveComponent* InComponent)->bool
	{
#if WITH_EDITOR
		if (InComponent->IsVisualizationComponent())
		{
			return true;
		}
#endif

		for (UMaterialInstanceDynamic* MID : GetComponentDynamicMaterials(InComponent))
		{
			const bool bMaterialAdded = !SavedMaterialParameters.Contains(MID);
			FAvaMaterialParameterMap& ParameterMap = SavedMaterialParameters.FindOrAdd(MID);

			// Removed untracked keys
			ParameterMap.MatchKeys(MaterialParameters);

			if (bMaterialAdded)
			{
				// Save original values
                ParameterMap.Get(MID);
				OnActorMaterialAdded(MID);
			}

			PrevScannedMaterials.Remove(MID);
		}

		return true;
	}
	, EActorModifierCoreComponentType::All
	, bUpdateChildren ? EActorModifierCoreLookup::SelfAndAllChildren : EActorModifierCoreLookup::Self);

	// Remove materials not tracked anymore
	for (UMaterialInstanceDynamic* MID : PrevScannedMaterials)
	{
		SavedMaterialParameters.Remove(MID);
		OnActorMaterialRemoved(MID);
	}
}

void UAvaMaterialParameterModifier::OnMaterialParametersChanged()
{
	MarkModifierDirty();
}

void UAvaMaterialParameterModifier::OnUpdateChildrenChanged()
{
	MarkModifierDirty();
}

#undef LOCTEXT_NAMESPACE
