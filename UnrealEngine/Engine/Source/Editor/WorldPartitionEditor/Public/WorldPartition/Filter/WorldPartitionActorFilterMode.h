// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISceneOutlinerMode.h"
#include "WorldPartition/Filter/WorldPartitionActorFilter.h"
#include "Containers/Map.h"
#include "Templates/Tuple.h"

class SSceneOutliner;

class WORLDPARTITIONEDITOR_API FWorldPartitionActorFilterMode : public ISceneOutlinerMode
{
public:
	// struct holding multi selection values that might be null if they differ between actors
	class WORLDPARTITIONEDITOR_API FFilter
	{
	public:
		FFilter(TSharedPtr<FWorldPartitionActorFilter> LevelFilter, const TArray<const FWorldPartitionActorFilter*>& SelectedFilters);
		
		void Apply(FWorldPartitionActorFilter* Result) const;
				
		struct FDataLayerFilter
		{
			FDataLayerFilter(){}
			FDataLayerFilter(bool bInOverride, bool bInIncluded) : bOverride(bInOverride), bIncluded(bInIncluded) {}

			TOptional<bool> bOverride;
			TOptional<bool> bIncluded;
		};

		using FDataLayerFilters = TMap<FSoftObjectPath, FDataLayerFilter>;
	private:
		void Initialize(const FWorldPartitionActorFilter* Filter);
		void Override(const FWorldPartitionActorFilter* Other);
		void Merge(const FWorldPartitionActorFilter* Other);
				
		// Unmodified Reference Level Filter
		TSharedPtr<FWorldPartitionActorFilter> LevelFilter;
		// Current values for Filter based Selection		
		TMap<const FWorldPartitionActorFilter*, FDataLayerFilters> FilterValues;

		friend class FWorldPartitionActorFilterMode;
	};

	FWorldPartitionActorFilterMode(SSceneOutliner* InSceneOutliner, TSharedPtr<FFilter> InFilter);
	
	//~ Begin ISceneOutlinerMode interface
	virtual void Rebuild() override;
	virtual int32 GetTypeSortPriority(const ISceneOutlinerTreeItem& Item) const override;
	//~ Begin ISceneOutlinerMode interface
		
	void Apply(FWorldPartitionActorFilter& InOutResult) const;
	
	FFilter::FDataLayerFilters& FindChecked(const FWorldPartitionActorFilter* InFilter) const
	{
		return Filter->FilterValues.FindChecked(InFilter);
	}

	const FWorldPartitionActorFilter* GetFilter() const
	{
		return Filter->LevelFilter.Get();
	}

protected:
	//~ Begin ISceneOutlinerMode interface
	virtual TUniquePtr<ISceneOutlinerHierarchy> CreateHierarchy() override;
	//~ End ISceneOutlinerMode interface
		
private:
	TSharedPtr<FFilter> Filter;
};