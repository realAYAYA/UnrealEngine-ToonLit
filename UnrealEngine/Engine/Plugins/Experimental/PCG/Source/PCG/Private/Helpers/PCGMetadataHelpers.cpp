// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGMetadataHelpers.h"
#include "Metadata/PCGMetadata.h"

namespace PCGMetadataHelpers
{
	bool HasSameRoot(const UPCGMetadata* Metadata1, const UPCGMetadata* Metadata2)
	{
		return Metadata1 && Metadata2 && Metadata1->GetRoot() == Metadata2->GetRoot();
	}
}