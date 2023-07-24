// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataLayerAsset)

UDataLayerAsset::UDataLayerAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DataLayerType(EDataLayerType::Editor)
	, DebugColor(FColor::Black)
{}

#if WITH_EDITOR
void UDataLayerAsset::PostLoad()
{
	if (DebugColor == FColor::Black)
	{
		DebugColor = FColor::MakeRandomSeededColor(GetTypeHash(GetName()));
	}

	Super::PostLoad();
}
#endif
