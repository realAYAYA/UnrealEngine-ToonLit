// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementEditorAssetDataInterface.h"

#include "AssetRegistry/AssetData.h"
#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformCrt.h"

TArray<FAssetData> UActorElementEditorAssetDataInterface::GetAllReferencedAssetDatas(const FTypedElementHandle& InElementHandle)
{
	TArray<FAssetData> AssetDatas;

	if (AActor* RawActorPtr = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		TArray<UObject*> ReferencedContentObjects;
		RawActorPtr->GetReferencedContentObjects(ReferencedContentObjects);
		for (const UObject* ContentObject : ReferencedContentObjects)
		{
			FAssetData ObjectAssetData = FAssetData(ContentObject);
			if (ObjectAssetData.IsValid())
			{
				AssetDatas.Emplace(ObjectAssetData);
			}
		}

		AssetDatas.Emplace(FAssetData(RawActorPtr));
	}

	return AssetDatas;
}
