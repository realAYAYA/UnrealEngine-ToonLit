// Copyright Epic Games, Inc. All Rights Reserved.

#include "ARKitTrackables.h"
#include "AppleARKitModule.h"
#include "AppleARKitSystem.h"
#include "AppleARKitMeshData.h"
#include "AppleARKitConversion.h"


bool UARKitMeshGeometry::GetObjectClassificationAtLocation(const FVector& InWorldLocation, EARObjectClassification& OutClassification, FVector& OutClassificationLocation, float MaxLocationDiff)
{
#if SUPPORTS_ARKIT_3_5
	if (auto ARKitSystem = FAppleARKitModule::GetARKitSystem())
	{
		FGuid MyGuid;
		if (ARKitSystem->GetGuidForGeometry(this, MyGuid))
		{
			if (auto MeshData = FARKitMeshData::GetMeshData(MyGuid))
			{
				uint8 Classification = 0;
				if (MeshData->GetClassificationAtLocation(InWorldLocation, GetLocalToWorldTransform(), Classification, OutClassificationLocation, MaxLocationDiff))
				{
					OutClassification = FAppleARKitConversion::ToEARObjectClassification((ARMeshClassification)Classification);
					return true;
				}
			}
		}
	}
#endif
	
	return false;
}
