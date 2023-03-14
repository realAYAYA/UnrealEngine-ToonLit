// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FoliageSupport/SubobjectFoliageInfoData.h"
#include "FoliageSupport/FoliageInfoData.h"
#include "UObject/SoftObjectPtr.h"

class UFoliageType;
struct FPropertySelectionMap;

namespace UE::LevelSnapshots::Foliage::Private
{
	class FInstancedFoliageActorData;
	FArchive& operator<<(FArchive& Ar, FInstancedFoliageActorData& MeshInfo);
	
	/** Handles saving of AInstancedFoliageActor::FoliageInfos */
	class FInstancedFoliageActorData
	{
		/**
		* Foliage type asset references
		* Consists of foliage type assets added via the green Add button or drag-drop a foliage type.
		*/
		TMap<TSoftObjectPtr<UFoliageType>, FFoliageInfoData> FoliageAssets;

		/**
		* Saves foliage type subobjects.
		* Consists of foliage types created when you drag-drop a static mesh or add Blueprint derived foliage types.
		*/
		TArray<FSubobjectFoliageInfoData> SubobjectData;

		FArchive& SerializeInternal(FArchive& Ar);
	
	public:

		/** Saves foliage actor into this object */
		void Save(FArchive& Archive, AInstancedFoliageActor* FoliageActor);
		
		/**
		* Looks up saved foliage types and restores the passed in foliage actor.
		* The caller must ensure that the required instances already exist by using FindFoliageType or FindOrCreateFoliageType.
		*/
		void ApplyTo(FArchive& Archive, AInstancedFoliageActor* FoliageActor, const FPropertySelectionMap& SelectedProperties, bool bWasRecreated) const;
		
		friend FArchive& operator<<(FArchive& Ar, FInstancedFoliageActorData& MeshInfo)
		{
			return MeshInfo.SerializeInternal(Ar);
		}
	};
}
