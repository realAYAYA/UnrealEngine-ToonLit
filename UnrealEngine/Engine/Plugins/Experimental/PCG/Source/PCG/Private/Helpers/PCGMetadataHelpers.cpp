// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGMetadataHelpers.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGMetadata.h"

namespace PCGMetadataHelpers
{
	bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2)
	{
		return Metadata1 && Metadata2 && Metadata1->GetRoot() == Metadata2->GetRoot();
	}

	const UPCGMetadata* GetParentMetadata(const UPCGMetadata* Metadata)
	{
		check(Metadata);
		TWeakObjectPtr<const UPCGMetadata> Parent = Metadata->GetParentPtr();

		// We're expecting the parent to either be null, or to be valid - if not, then it has been deleted
		// which is going to cause some issues.
		//check(Parent.IsExplicitlyNull() || Parent.IsValid());
		return Parent.Get();
	}

	const UPCGMetadata* GetConstMetadata(const UPCGData* InData)
	{
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InData))
		{
			return SpatialData->ConstMetadata();
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(InData))
		{
			return ParamData->ConstMetadata();
		}
		else
		{
			return nullptr;
		}
	}

	UPCGMetadata* GetMutableMetadata(UPCGData* InData)
	{
		if (UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InData))
		{
			return SpatialData->MutableMetadata();
		}
		else if (UPCGParamData* ParamData = Cast<UPCGParamData>(InData))
		{
			return ParamData->MutableMetadata();
		}
		else
		{
			return nullptr;
		}
	}
}