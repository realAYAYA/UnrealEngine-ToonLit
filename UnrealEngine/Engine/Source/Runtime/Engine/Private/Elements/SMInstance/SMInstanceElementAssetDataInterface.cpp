// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/SMInstance/SMInstanceElementAssetDataInterface.h"

#include "Elements/SMInstance/SMInstanceElementData.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SMInstanceElementAssetDataInterface)

FAssetData USMInstanceElementAssetDataInterface::GetAssetData(const FTypedElementHandle& InElementHandle)
{
	if (FSMInstanceManager SMInstance = SMInstanceElementDataUtil::GetSMInstanceFromHandle(InElementHandle))
	{
		if (UStaticMesh* StaticMesh = SMInstance.GetISMComponent()->GetStaticMesh())
		{
			return FAssetData(StaticMesh);
		}
	}

	return FAssetData();
}

