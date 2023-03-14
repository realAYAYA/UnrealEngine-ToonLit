// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "IFilterPreset.h"
#include "Templates/SharedPointer.h"

struct FTraceObjectInfo
{
	FString Name;
	bool bEnabled;
	bool bReadOnly;
	uint32 Hash;
	uint32 OwnerHash;
};

/** Filtering service, representing the state and data for a specific TraceServices::IAnalysisSession */
class ISessionTraceFilterService : public TSharedFromThis<ISessionTraceFilterService>
{
public:
	virtual ~ISessionTraceFilterService() {}

	/** Returns the root level set of objects */
	virtual void GetRootObjects(TArray<FTraceObjectInfo>& OutObjects) const = 0;

	/** Returns the contained objects for the specific object hash */
	virtual void GetChildObjects(uint32 InObjectHash, TArray<FTraceObjectInfo>& OutChildObjects) const = 0;

	/** Set the filtered state for an individual object by its hash */
	virtual void SetObjectFilterState(const FString& InObjectName, const bool bFilterState) = 0;
	
	/** Set timestamp for last processed update (data change) */
	virtual const FDateTime& GetTimestamp() = 0;

	/** Update filter preset */
	virtual void UpdateFilterPreset(const TSharedPtr<IFilterPreset> InPreset, bool IsEnabled) = 0;
};
