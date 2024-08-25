// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Insights/ViewModels/FilterConfiguratorNode.h"
#include "Insights/ViewModels/Filters.h"
#include "Insights/ViewModels/IFilterExecutor.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterConfigurator : public IFilterExecutor
{
public:
	FFilterConfigurator();

	FFilterConfigurator(const FFilterConfigurator& Other);
	FFilterConfigurator& operator=(const FFilterConfigurator& Other);

	bool operator==(const FFilterConfigurator& Other) const;
	bool operator!=(const FFilterConfigurator& Other) const { return !(*this == Other); }

	virtual ~FFilterConfigurator();

	bool IsEmpty() const { return RootNode->GetChildrenCount() == 0; }

	/** Called to update the internal state of some filters */
	void Update() { RootNode->Update(); }

	virtual bool ApplyFilters(const FFilterContext& Context) const override;

	bool IsKeyUsed(int32 Key) const;

	FFilterConfiguratorNodePtr GetRootNode() { return RootNode; }
	TSharedPtr<TArray<TSharedPtr<FFilter>>>& GetAvailableFilters() { return AvailableFilters; }

	void Add(TSharedPtr<FFilter> InFilter) { AvailableFilters->Add(InFilter); }

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// OnDestroyedEvent

public:
	/** The event to execute when an instance is destroyed. */
	DECLARE_MULTICAST_DELEGATE(FOnDestroyedEvent);
	FOnDestroyedEvent& GetOnDestroyedEvent() { return OnDestroyedEvent; }

private:
	FOnDestroyedEvent OnDestroyedEvent;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// OnChangesCommittedEvent

public:
	/** The event to execute when the changes to the Filter Widget are saved by clicking on the OK Button. */
	DECLARE_MULTICAST_DELEGATE(FOnChangesCommittedEvent);
	FOnChangesCommittedEvent& GetOnChangesCommittedEvent() { return OnChangesCommittedEvent; }

private:
	FOnChangesCommittedEvent OnChangesCommittedEvent;

	////////////////////////////////////////////////////////////////////////////////////////////////////

private:
	void ComputeUsedKeys();

private:
	FFilterConfiguratorNodePtr RootNode;

	TSharedPtr<TArray<TSharedPtr<FFilter>>> AvailableFilters;

	TSet<int32> KeysUsed;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
