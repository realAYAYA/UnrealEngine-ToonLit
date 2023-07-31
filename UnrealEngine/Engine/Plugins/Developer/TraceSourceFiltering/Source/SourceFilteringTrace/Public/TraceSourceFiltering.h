// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

#include "DataSourceFilter.h"

class UTraceSourceFilteringSettings;
class USourceFilterCollection;

/** Object managing the currently active UDataSourceFilter instances and UTraceSourceFilteringSettings */
class SOURCEFILTERINGTRACE_API FTraceSourceFiltering : public FGCObject
{
public:
	static void Initialize();
	static FTraceSourceFiltering& Get();

	/** Returns the running instance its Filter Collection, containing active set of filters */
	USourceFilterCollection* GetFilterCollection();
	/** Returns the running instance its Filtering Settings */
	UTraceSourceFilteringSettings* GetSettings();

	/** Processes an received filtering command, altering the Filter Collection and or Settings accordingly */
	void ProcessRemoteCommand(const FString& Command, const TArray<FString>& Arguments);

	/** Begin FGCObject overrides */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FTraceSourceFiltering");
	}
	/** End FGCObject overrides */
protected:
	FTraceSourceFiltering();
	void PopulateRemoteTraceCommands();

protected:
	UTraceSourceFilteringSettings* Settings;
	USourceFilterCollection* FilterCollection;

	/** Structure representing a remotely 'callable' filter command */
	struct FFilterCommand
	{
		TFunction<void(const TArray<FString>&)> Function;
		int32 NumExpectedArguments;
	};

	/** Mapping for all filtering commands from their name to respective FFilterCommand object */
	TMap<FString, FFilterCommand> CommandMap;
};