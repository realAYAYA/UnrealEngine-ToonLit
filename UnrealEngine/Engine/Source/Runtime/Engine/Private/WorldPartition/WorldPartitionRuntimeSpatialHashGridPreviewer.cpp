// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeSpatialHashGridPreviewer.h"
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "WorldPartition/RuntimeSpatialHash/RuntimeSpatialHashGridHelper.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WorldPartitionRuntimeSpatialHashGridPreviewer)

/*
 * FWorldPartitionRuntimeSpatialHashGridPreviewer
 */

FWorldPartitionRuntimeSpatialHashGridPreviewer::FWorldPartitionRuntimeSpatialHashGridPreviewer()
#if WITH_EDITORONLY_DATA
	: MID(nullptr)
	, Volume(nullptr)
#endif
{
#if WITH_EDITORONLY_DATA
	Material = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorMaterials/WorldPartition/WorldPartitionSpatialHashGridPreviewMaterial"));
#endif
}

#if WITH_EDITOR
void FWorldPartitionRuntimeSpatialHashGridPreviewer::Draw(UWorld* World, const TArray<FSpatialHashRuntimeGrid>& Grids, bool bEnabled)
{
	if (bEnabled && Material)
	{
		if (!Volume)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags |= RF_Transient;
			SpawnParameters.ObjectFlags &= ~RF_Transactional;
			Volume = World->SpawnActor<APostProcessVolume>(SpawnParameters);
		}

		if (!MID && Volume)
		{
			MID = UMaterialInstanceDynamic::Create(Material, Volume);
			Volume->Settings.WeightedBlendables.Array.Emplace(1.0f, MID);
			Volume->bUnbound = true;
		}

		if (MID)
		{
			for (int i = 0; i < MAX_PREVIEW_GRIDS; ++i)
			{
				bool bGridEnabled = Grids.IsValidIndex(i) && Grids[i].CellSize > 0;
				FGridParametersCache& CachedParameters = GridParameters[i];

				if (CachedParameters.bEnabled != bGridEnabled)
				{
					MID->SetScalarParameterValue(*FString::Printf(TEXT("Grid%d_Enabled"), i), bGridEnabled ? 1.0f : 0.f);
					CachedParameters.bEnabled = bGridEnabled;
				}
				if (bGridEnabled)
				{
					const FSpatialHashRuntimeGrid& Grid = Grids[i];

					if (CachedParameters.CellSize != Grid.CellSize)
					{
						MID->SetScalarParameterValue(*FString::Printf(TEXT("Grid%d_CellSize"), i), (float)Grid.CellSize);
						CachedParameters.CellSize = Grid.CellSize;
					}

					if (CachedParameters.LoadingRange != Grid.LoadingRange)
					{
						MID->SetScalarParameterValue(*FString::Printf(TEXT("Grid%d_LoadingRange"), i), Grid.LoadingRange);
						CachedParameters.LoadingRange = Grid.LoadingRange;
					}

					if (CachedParameters.GridColor != Grid.DebugColor)
					{
						MID->SetVectorParameterValue(*FString::Printf(TEXT("Grid%d_Color"), i), Grid.DebugColor);
						CachedParameters.GridColor = Grid.DebugColor;
					}

					FVector GridOffset = GRuntimeSpatialHashUseAlignedGridLevels ? FVector(0.5 * Grid.CellSize) : FVector::ZeroVector;
					if (CachedParameters.GridOffset != GridOffset)
					{
						MID->SetVectorParameterValue(*FString::Printf(TEXT("Grid%d_Offset"), i), GridOffset);
						CachedParameters.GridOffset = GridOffset;
					}
				}
			}
		}
	}

	if (Volume)
	{
		Volume->bEnabled = bEnabled;
	}
}
#endif
