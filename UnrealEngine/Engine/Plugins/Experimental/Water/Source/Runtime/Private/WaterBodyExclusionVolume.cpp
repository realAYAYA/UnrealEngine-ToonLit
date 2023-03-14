// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodyExclusionVolume.h"
#include "Components/BrushComponent.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "EngineUtils.h"
#include "WaterSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "WaterBodyComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WaterBodyExclusionVolume)

#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#include "WaterIconHelper.h"
#include "WaterSubsystem.h"
#include "Modules/ModuleManager.h"
#include "WaterModule.h"
#endif

AWaterBodyExclusionVolume::AWaterBodyExclusionVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	ActorIcon = FWaterIconHelper::EnsureSpriteComponentCreated(this, TEXT("/Water/Icons/WaterBodyExclusionVolumeSprite"));
#endif
}

void AWaterBodyExclusionVolume::UpdateOverlappingWaterBodies()
{
	TArray<FOverlapResult> Overlaps;

	FBoxSphereBounds Bounds = GetBounds();
	GetWorld()->OverlapMultiByObjectType(Overlaps, Bounds.Origin, FQuat::Identity, FCollisionObjectQueryParams::AllObjects, FCollisionShape::MakeBox(Bounds.BoxExtent));

	// Find any new overlapping bodies and notify them that this exclusion volume influences them
	TSet<UWaterBodyComponent*> ExistingOverlappingBodies;
	TSet<TWeakObjectPtr<UWaterBodyComponent>> NewOverlappingBodies;

	TSoftObjectPtr<AWaterBodyExclusionVolume> SoftThis(this);

	// Fixup overlapping bodies (iterating on actors on post-load will fail, but this is fine as this exclusion volume should not yet be referenced by an existing water body upon loading) : 
	FWaterBodyManager::ForEachWaterBodyComponent(GetWorld(), [SoftThis, &ExistingOverlappingBodies](UWaterBodyComponent* WaterBodyComponent)
	{
		if (WaterBodyComponent->ContainsExclusionVolume(SoftThis))
		{
			ExistingOverlappingBodies.Add(WaterBodyComponent);
		}
		return true;
	});

	for (const FOverlapResult& Result : Overlaps)
	{
		AWaterBody* WaterBody = Result.OverlapObjectHandle.FetchActor<AWaterBody>();
		if (WaterBody && (bExcludeAllOverlappingWaterBodies || WaterBodiesToExclude.Contains(WaterBody)))
		{
			UWaterBodyComponent* WaterBodyComponent = WaterBody->GetWaterBodyComponent();
			NewOverlappingBodies.Add(WaterBodyComponent);
			// If the water body is not already overlapping then notify
			if (!ExistingOverlappingBodies.Contains(WaterBodyComponent))
			{
				WaterBodyComponent->AddExclusionVolume(this);
			}
		}
	}

	// Find existing bodies that are no longer overlapping and remove them
	for (UWaterBodyComponent* ExistingBody : ExistingOverlappingBodies)
	{
		if (ExistingBody && !NewOverlappingBodies.Contains(ExistingBody))
		{
			ExistingBody->RemoveExclusionVolume(this);
		}
	}
}

#if WITH_EDITOR
void AWaterBodyExclusionVolume::UpdateActorIcon()
{
	UTexture2D* IconTexture = ActorIcon->Sprite;
	IWaterModuleInterface& WaterModule = FModuleManager::GetModuleChecked<IWaterModuleInterface>("Water");
	if (const IWaterEditorServices* WaterEditorServices = WaterModule.GetWaterEditorServices())
	{
		IconTexture = WaterEditorServices->GetWaterActorSprite(GetClass());
	}
	FWaterIconHelper::UpdateSpriteComponent(this, IconTexture);
}
#endif // WITH_EDITOR

void AWaterBodyExclusionVolume::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Perform data deprecation: 
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::SupportMultipleWaterBodiesPerExclusionVolume)
	{
		if (WaterBodyToIgnore_DEPRECATED != nullptr)
		{
			WaterBodiesToIgnore_DEPRECATED.Add(WaterBodyToIgnore_DEPRECATED);
			WaterBodyToIgnore_DEPRECATED = nullptr;
		}
	}

	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WaterExclusionVolumeExcludeAllDefault)
	{
		WaterBodiesToExclude = MoveTemp(WaterBodiesToIgnore_DEPRECATED);

		// If the existing actor had selected specific water bodies to exclude then we set the new value to false by default.
		if (!bIgnoreAllOverlappingWaterBodies_DEPRECATED && WaterBodiesToExclude.Num() > 0)
		{
			bExcludeAllOverlappingWaterBodies = false;
		}
	}
#endif // WITH_EDITORONLY_DATA

	UpdateOverlappingWaterBodies();
}

void AWaterBodyExclusionVolume::Destroyed()
{
	Super::Destroyed();

	// No need for water bodies to keep a pointer to ourselves, even if a lazy one :
	// Use a TObjectRange here instead of the Manager for each because it may not be valid
	UWorld* World = GetWorld();
	for (UWaterBodyComponent* WaterBodyComponent : TObjectRange<UWaterBodyComponent>())
	{
		if (WaterBodyComponent && WaterBodyComponent->GetWorld() == World)
		{
			WaterBodyComponent->RemoveExclusionVolume(this);
		}
	}
}

#if WITH_EDITOR
void AWaterBodyExclusionVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	UpdateOverlappingWaterBodies();
}

void AWaterBodyExclusionVolume::PostEditUndo()
{
	Super::PostEditUndo();

	UpdateOverlappingWaterBodies();
}

void AWaterBodyExclusionVolume::PostEditImport()
{
	Super::PostEditImport();

	UpdateOverlappingWaterBodies();
}

void AWaterBodyExclusionVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateOverlappingWaterBodies();
}

FName AWaterBodyExclusionVolume::GetCustomIconName() const
{
	// override this to not inherit from ABrush::GetCustomIconName's behavior and use the class icon instead 
	return NAME_None;
}

#endif // WITH_EDITOR

