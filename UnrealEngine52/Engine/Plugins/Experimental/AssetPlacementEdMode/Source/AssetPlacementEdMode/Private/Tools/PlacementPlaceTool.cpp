// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/PlacementPlaceTool.h"
#include "UObject/Object.h"
#include "AssetPlacementSettings.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Math/UnrealMathUtility.h"
#include "Editor.h"
#include "InstancedFoliage.h"
#include "InteractiveToolManager.h"
#include "Modes/PlacementModeSubsystem.h"
#include "PlacementPaletteItem.h"

#include "Instances/InstancedPlacementClientInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementPlaceTool)

constexpr TCHAR UPlacementModePlacementTool::ToolName[];

UPlacementBrushToolBase* UPlacementModePlacementToolBuilder::FactoryToolInstance(UObject* Outer) const
{
	return NewObject<UPlacementModePlacementTool>(Outer);
}

void UPlacementModePlacementTool::OnBeginDrag(const FRay& Ray)
{
	Super::OnBeginDrag(Ray);

	GetToolManager()->BeginUndoTransaction(NSLOCTEXT("AssetPlacementEdMode", "PaintAssets", "Paint Assets"));
}

void UPlacementModePlacementTool::OnEndDrag(const FRay& Ray)
{
	GetToolManager()->EndUndoTransaction();

	Super::OnEndDrag(Ray);
}

void UPlacementModePlacementTool::OnTick(float DeltaTime)
{
	if (!bInBrushStroke)
	{
		return;
	}

	UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>();
	const UAssetPlacementSettings* PlacementSettings = PlacementModeSubsystem ? PlacementModeSubsystem->GetModeSettingsObject() : nullptr;
	if (BrushProperties && PlacementSettings)
	{
		TArrayView<const TObjectPtr<UPlacementPaletteClient>> PaletteItems = PlacementSettings->GetActivePaletteItems();
		if (PaletteItems.Num())
		{
			FVector BrushLocation = LastBrushStamp.WorldPosition;

			// Assume a default density of 100 * whatever the user has selected as brush size.
			float DefaultDensity = 100.0f * BrushProperties->BrushSize;
			// This is the total set of instances disregarding parameters like slope, height or layer.
			float DesiredInstanceCountFloat = DefaultDensity /** todo: individual item's Density*/ * BrushProperties->BrushStrength;
			// Allow a single instance with a random chance, if the brush is smaller than the density
			int32 DesiredInstanceCount = DesiredInstanceCountFloat > 1.f ? FMath::RoundToInt(DesiredInstanceCountFloat) : FMath::FRand() < DesiredInstanceCountFloat ? 1 : 0;

			// Todo: save the instance hash per tile for the editor, so that paints don't continually add instances and surpass desired density (partition actor?)
			FFoliageInstanceHash PotentialInstanceHash(7);
			TArray<FVector> PotentialInstanceLocations;

			TArray<FAssetPlacementInfo> DesiredInfoSpawnLocations;
			DesiredInfoSpawnLocations.Reserve(DesiredInstanceCount);
			PotentialInstanceLocations.Empty(DesiredInstanceCount);
			for (int32 Index = 0; Index < DesiredInstanceCount; ++Index)
			{
				FVector Start, End;
				GetRandomVectorInBrush(Start, End);
				FHitResult AdjustedHitResult;
				if (FindHitResultWithStartAndEndTraceVectors(AdjustedHitResult, Start, End))
				{
					FVector SpawnLocation = AdjustedHitResult.ImpactPoint;
					FVector SpawnNormal = AdjustedHitResult.ImpactNormal;

					bool bValidInstance = true;
					auto TempInstances = PotentialInstanceHash.GetInstancesOverlappingBox(FBox::BuildAABB(BrushLocation, FVector(BrushProperties->BrushRadius)));
					for (int32 InstanceIndex : TempInstances)
					{
						if ((PotentialInstanceLocations[InstanceIndex] - BrushLocation).SizeSquared() < FMath::Square(BrushProperties->BrushRadius))
						{
							bValidInstance = false;
							break;
						}
					}

					if (!bValidInstance)
					{
						continue;
					}

					int32 PotentialIdx = PotentialInstanceLocations.Add(SpawnLocation);
					PotentialInstanceHash.InsertInstance(SpawnLocation, PotentialIdx);

					int32 ItemIndex = FMath::RandHelper(PaletteItems.Num());
					if (const UPlacementPaletteClient* ItemToPlace = PaletteItems[ItemIndex])
					{
						FAssetPlacementInfo NewInfo;
						NewInfo.AssetToPlace = ItemToPlace->AssetPath.TryLoad();
						NewInfo.FactoryOverride = ItemToPlace->FactoryInterface;
						NewInfo.ItemGuid = ItemToPlace->ClientGuid;
						NewInfo.PreferredLevel = GEditor->GetEditorWorldContext().World()->GetCurrentLevel();
						NewInfo.FinalizedTransform = GenerateTransformFromHitLocationAndNormal(SpawnLocation, SpawnNormal);
						NewInfo.SettingsObject = ItemToPlace->SettingsObject;

						DesiredInfoSpawnLocations.Emplace(NewInfo);
					}
				}
			}

			UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>();
			if (PlacementSubsystem)
			{
				FPlacementOptions PlacementOptions;
				PlacementOptions.bPreferBatchPlacement = true;
				PlacementOptions.InstancedPlacementGridGuid = PlacementSettings->GetActivePaletteGuid();
				PlacementSubsystem->PlaceAssets(DesiredInfoSpawnLocations, PlacementOptions);
			}
		}
	}
}

void UPlacementModePlacementTool::GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd)
{
	if (!BrushProperties)
	{
		return;
	}
	FVector BrushNormal = LastBrushStamp.WorldNormal;
	FVector BrushLocation = LastBrushStamp.WorldPosition;
	float BrushRadius = BrushProperties->BrushRadius;

	// Find Rx and Ry inside the unit circle
	float Ru = (2.f * FMath::FRand() - 1.f);
	float Rv = (2.f * FMath::FRand() - 1.f) * FMath::Sqrt(1.f - FMath::Square(Ru));

	// find random point in circle through brush location on the same plane to brush location hit surface normal
	FVector U, V;
	BrushNormal.FindBestAxisVectors(U, V);
	FVector Point = Ru * U + Rv * V;

	// find distance to surface of sphere brush from this point
	FVector Rw = FMath::Sqrt(FMath::Max(1.f - (FMath::Square(Ru) + FMath::Square(Rv)), 0.001f)) * BrushNormal;

	OutStart = BrushLocation + BrushRadius * (Point + Rw);
	OutEnd = BrushLocation + BrushRadius * (Point - Rw);
}

