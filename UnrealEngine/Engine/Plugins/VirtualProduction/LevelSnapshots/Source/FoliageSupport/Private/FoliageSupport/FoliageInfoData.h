// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InstancedFoliage.h"

namespace UE::LevelSnapshots::Foliage::Private
{
	/** Data we'll send to FFoliageImpl::Serialize */
	class FFoliageInfoData
	{
		EFoliageImplType ImplType = EFoliageImplType::Unknown;
		FName ComponentName;
		
		/** Used before FSnapshotCustomVersion::CustomSubobjectSoftObjectPathRefactor */
		TArray<uint8> SerializedData_DEPRECATED;
		/** Added after FSnapshotCustomVersion::CustomSubobjectSoftObjectPathRefactor */
		int64 FoliageInfoArchiveSize = 0;

		FArchive& SerializeInternal(FArchive& Ar);
	
	public:

		void Save(FArchive& Archive, FFoliageInfo& DataToReadFrom);
		void ApplyTo(FArchive& Archive, FFoliageInfo& DataToWriteInto) const;

		EFoliageImplType GetImplType() const { return ImplType; }
		TOptional<FName> GetComponentName() const { return ImplType == EFoliageImplType::StaticMesh && !ComponentName.IsNone() ? TOptional<FName>(ComponentName) : TOptional<FName>(); }
		int64 GetFoliageInfoArchiveSize() const { return FoliageInfoArchiveSize; }
	
		friend FArchive& operator<<(FArchive& Ar, FFoliageInfoData& MeshInfo)
		{
			return MeshInfo.SerializeInternal(Ar);
		}
	};
}
