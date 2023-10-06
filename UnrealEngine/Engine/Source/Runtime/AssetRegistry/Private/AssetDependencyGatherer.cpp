// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/AssetDependencyGatherer.h"

#if WITH_EDITOR

namespace UE::AssetDependencyGatherer::Private
{

static TLinkedList<FRegisteredAssetDependencyGatherer*>* GRegisteredAssetDependencyGathererList = nullptr;
FRegisteredAssetDependencyGatherer::FOnAssetDependencyGathererRegistered FRegisteredAssetDependencyGatherer::OnAssetDependencyGathererRegistered;

FRegisteredAssetDependencyGatherer::FRegisteredAssetDependencyGatherer()
	: GlobalListLink(this)
{
	GlobalListLink.LinkHead(GetRegisteredList());
	OnAssetDependencyGathererRegistered.Broadcast();
}

FRegisteredAssetDependencyGatherer::~FRegisteredAssetDependencyGatherer()
{
	GlobalListLink.Unlink();
}

TLinkedList<FRegisteredAssetDependencyGatherer*>*& FRegisteredAssetDependencyGatherer::GetRegisteredList()
{
	return GRegisteredAssetDependencyGathererList;
}

void FRegisteredAssetDependencyGatherer::ForEach(TFunctionRef<void(FRegisteredAssetDependencyGatherer*)> Func)
{
	for (TLinkedList<FRegisteredAssetDependencyGatherer*>::TIterator It(GetRegisteredList()); It; It.Next())
	{
		Func(*It);
	}
}
	
}

#endif