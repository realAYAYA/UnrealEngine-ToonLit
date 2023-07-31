// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/DataLayer/WorldDataLayersCollection.h"

bool FWorldDataLayersCollection::RegisterWorldDataLayer(AWorldDataLayers* InWorldDataLayers)
{
	// Registration and Unregistration for WorldDataLayers occurs during PreRegisterAllComponents and PostUnregisterAllComponents.
	// Those 2 functions are not symmetrical, so we need to check contains to avoid regisering twice.
	if (!WorldDataLayers.Contains(InWorldDataLayers) && InWorldDataLayers != nullptr)
	{
		WorldDataLayers.Add(InWorldDataLayers);
		return true;
	}

	return false;
}

bool FWorldDataLayersCollection::UnregisterWorldDataLayer(AWorldDataLayers* InWorldDataLayers)
{
	int32 OldCount = WorldDataLayers.Num();
	int32 NewCount = WorldDataLayers.RemoveSwap(InWorldDataLayers);
	check(!Contains(InWorldDataLayers));
	return OldCount > NewCount;
}

bool FWorldDataLayersCollection::Contains(const AWorldDataLayers* InWorldDataLayers) const
{
	return WorldDataLayers.Contains(InWorldDataLayers);
}

void FWorldDataLayersCollection::ForEachWorldDataLayersBreakable(TFunctionRef<bool(AWorldDataLayers*)> Func, const ULevel* InLevelContext /* = nullptr */) const
{
	for (const TWeakObjectPtr<AWorldDataLayers>& WorldDataLayer : WorldDataLayers)
	{
		if (ensure(WorldDataLayer.IsValid()))
		{
			if (ShoudIterateWorldDataLayer(WorldDataLayer.Get(), InLevelContext))
			{
				if (!Func(WorldDataLayer.Get()))
				{
					break;
				}
			}
		}
	}
}

void FWorldDataLayersCollection::ForEachWorldDataLayersBreakable(TFunctionRef<bool(AWorldDataLayers*)> Func, const ULevel* InLevelContext /* = nullptr */)
{
	const_cast<const FWorldDataLayersCollection*>(this)->ForEachWorldDataLayersBreakable(Func, InLevelContext);
}

void FWorldDataLayersCollection::ForEachWorldDataLayers(TFunctionRef<void(AWorldDataLayers*)> Func, const ULevel* InLevelContext /* = nullptr */) const
{
	for (const TWeakObjectPtr<AWorldDataLayers>& WorldDataLayer : WorldDataLayers)
	{
		if (ensure(WorldDataLayer.IsValid()))
		{
			if (ShoudIterateWorldDataLayer(WorldDataLayer.Get(), InLevelContext))
			{
				Func(WorldDataLayer.Get());
			}
		}
	}
}

void FWorldDataLayersCollection::ForEachWorldDataLayers(TFunctionRef<void(AWorldDataLayers*)> Func, const ULevel* InLevelContext /* = nullptr */)
{
	const_cast<const FWorldDataLayersCollection*>(this)->ForEachWorldDataLayers(Func, InLevelContext);
}

void FWorldDataLayersCollection::ForEachDataLayerBreakable(TFunctionRef<bool(UDataLayerInstance*)> Func, const ULevel* InLevelContext /* = nullptr */) const
{
	bool bShouldContinue = false;
	auto CallAndSetContinueFunc = [Func, &bShouldContinue](UDataLayerInstance* DataLayerInstance)
	{
		bShouldContinue = Func(DataLayerInstance);
		return bShouldContinue;
	};

	auto BreakableWorldDataLayerFunc = [CallAndSetContinueFunc, &bShouldContinue](const AWorldDataLayers* WorldDataLayer)
	{
		WorldDataLayer->ForEachDataLayer(CallAndSetContinueFunc);
		return bShouldContinue;
	};

	ForEachWorldDataLayersBreakable(BreakableWorldDataLayerFunc, InLevelContext);
}

void FWorldDataLayersCollection::ForEachDataLayerBreakable(TFunctionRef<bool(UDataLayerInstance*)> Func, const ULevel* InLevelContext /* = nullptr */)
{
	const_cast<const FWorldDataLayersCollection*>(this)->ForEachDataLayerBreakable(Func, InLevelContext);
}

bool FWorldDataLayersCollection::ShoudIterateWorldDataLayer(const AWorldDataLayers* WorldDataLayer, const ULevel* InLevelContext) const
{
	return (InLevelContext == nullptr && !WorldDataLayer->IsSubWorldDataLayers())
		|| (InLevelContext != nullptr && WorldDataLayer->GetTypedOuter<ULevel>() == InLevelContext);
}