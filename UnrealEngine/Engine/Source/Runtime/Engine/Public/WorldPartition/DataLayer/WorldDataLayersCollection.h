// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"

class ENGINE_API FWorldDataLayersCollection
{
public:
	bool RegisterWorldDataLayer(AWorldDataLayers* InWorldDataLayers);
	bool UnregisterWorldDataLayer(AWorldDataLayers* InWorldDataLayers);
	bool Contains(const AWorldDataLayers* InWorldDataLayers) const;

	void ForEachWorldDataLayersBreakable(TFunctionRef<bool(AWorldDataLayers*)> Func, const ULevel* InLevelContext = nullptr);
	void ForEachWorldDataLayersBreakable(TFunctionRef<bool(AWorldDataLayers*)> Func, const ULevel* InLevelContext = nullptr) const;

	void ForEachWorldDataLayers(TFunctionRef<void(AWorldDataLayers*)> Func, const ULevel* InLevelContext = nullptr);
	void ForEachWorldDataLayers(TFunctionRef<void(AWorldDataLayers*)> Func, const ULevel* InLevelContext = nullptr) const;

	void ForEachDataLayerBreakable(TFunctionRef<bool(UDataLayerInstance*)> Func, const ULevel* InLevelContext = nullptr);
	void ForEachDataLayerBreakable(TFunctionRef<bool(UDataLayerInstance*)> Func, const ULevel* InLevelContext = nullptr) const;

	bool IsEmpty() const { return WorldDataLayers.IsEmpty(); }

	void Empty() { return WorldDataLayers.Empty(); }

	template<class T>
	UDataLayerInstance* GetDataLayerInstance(const T& InDataLayerIdentifier, const ULevel* InLevelContext = nullptr) const;

	template<class T>
	TArray<FName> GetDataLayerInstanceNames(const TArray<T>& InDataLayerIdentifiers, const ULevel* InLevelContext = nullptr) const;

	template<class T>
	TArray<const UDataLayerInstance*> GetDataLayerInstances(const TArray<T>& InDataLayerIdentifiers, const ULevel* InLevelContext = nullptr) const;

private:
	bool ShoudIterateWorldDataLayer(const AWorldDataLayers* WorldDataLayers, const ULevel* InLevelContext) const;

	TArray<TWeakObjectPtr<AWorldDataLayers>> WorldDataLayers;
};

template<class T>
UDataLayerInstance* FWorldDataLayersCollection::GetDataLayerInstance(const T& InDataLayerIdentifier, const ULevel* InLevelContext /* = nullptr */) const
{
	const UDataLayerInstance* DataLayerInstance = nullptr;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ForEachWorldDataLayersBreakable([&InDataLayerIdentifier, &DataLayerInstance](AWorldDataLayers* InWorldDataLayers)
	{
		DataLayerInstance = InWorldDataLayers->GetDataLayerInstance(InDataLayerIdentifier);
		return DataLayerInstance == nullptr;

	}, InLevelContext);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	return const_cast<UDataLayerInstance*>(DataLayerInstance);
}

template<class T>
TArray<FName> FWorldDataLayersCollection::GetDataLayerInstanceNames(const TArray<T>& InDataLayerIdentifiers, const ULevel* InLevelContext /* = nullptr */) const
{
	TArray<FName> DataLayerInstanceNames;
	DataLayerInstanceNames.Reserve(InDataLayerIdentifiers.Num());

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ForEachWorldDataLayersBreakable([&InDataLayerIdentifiers, &DataLayerInstanceNames](AWorldDataLayers* InWorldDataLayers)
	{
		DataLayerInstanceNames.Append(InWorldDataLayers->GetDataLayerInstanceNames(InDataLayerIdentifiers));
		return DataLayerInstanceNames.Num() != InDataLayerIdentifiers.Num();

	}, InLevelContext);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return DataLayerInstanceNames;
}

template<class T>
TArray<const UDataLayerInstance*> FWorldDataLayersCollection::GetDataLayerInstances(const TArray<T>& InDataLayerIdentifiers, const ULevel* InLevelContext /* = nullptr */) const
{
	TArray<const UDataLayerInstance*> DataLayerInstances;
	DataLayerInstances.Reserve(InDataLayerIdentifiers.Num());

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ForEachWorldDataLayersBreakable([&InDataLayerIdentifiers, &DataLayerInstances](AWorldDataLayers* InWorldDataLayers)
	{
		DataLayerInstances.Append(InWorldDataLayers->GetDataLayerInstances(InDataLayerIdentifiers));
		return DataLayerInstances.Num() != InDataLayerIdentifiers.Num();

	}, InLevelContext);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return DataLayerInstances;
}