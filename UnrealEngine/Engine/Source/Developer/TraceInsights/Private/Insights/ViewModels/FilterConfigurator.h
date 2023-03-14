// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Insights/ViewModels/IFilterExecutor.h"
#include "Insights/ViewModels/Filters.h"
#include "Insights/ViewModels/FilterConfiguratorNode.h"

namespace Insights
{
 
////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterConfigurator : public IFilterExecutor
{
public:
	FFilterConfigurator();

	FFilterConfigurator(const FFilterConfigurator& Other);
	FFilterConfigurator& operator=(const FFilterConfigurator& Other);

	bool operator==(const FFilterConfigurator& Other);

	bool operator!=(const FFilterConfigurator& Other) { return !(*this == Other); }

	virtual ~FFilterConfigurator();

	FFilterConfiguratorNodePtr GetRootNode() { return RootNode; }

	virtual bool ApplyFilters(const FFilterContext& Context) const override;

	bool IsKeyUsed(int32 Key) const;

	TSharedPtr<TArray<TSharedPtr<struct FFilter>>>& GetAvailableFilters() { return AvailableFilters; }

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// OnDestroyedEvent
public:
	/** The event to execute when an instance is destroyed. */
	DECLARE_MULTICAST_DELEGATE(FOnDestroyedEvent);
	FOnDestroyedEvent& GetOnDestroyedEvent() { return OnDestroyedEvent; }

private:
	FOnDestroyedEvent OnDestroyedEvent;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// OnChangesCommitedEvent
public:
	/** The event to execute when the changes to the Filter Widget are saved by clicking on the OK Button. */
	DECLARE_MULTICAST_DELEGATE(FOnChangesCommitedEvent);
	FOnChangesCommitedEvent& GetOnChangesCommitedEvent() { return OnChangesCommitedEvent; }

private:
	FOnChangesCommitedEvent OnChangesCommitedEvent;

	////////////////////////////////////////////////////////////////////////////////////////////////////

private:

	void ComputeUsedKeys();

	FFilterConfiguratorNodePtr RootNode;

	TSharedPtr<TArray<TSharedPtr<struct FFilter>>> AvailableFilters;

	TSet<int32> KeysUsed;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights