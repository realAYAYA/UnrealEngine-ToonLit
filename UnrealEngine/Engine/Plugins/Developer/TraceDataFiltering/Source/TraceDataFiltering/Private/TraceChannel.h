// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITraceObject.h"

class ISessionTraceFilterService;

class FTraceChannel : public ITraceObject
{
public:
	FTraceChannel(FString InName, FString InParentName, uint32 InHash, bool bInEnabled, bool bInReadOnly, const TArray<TSharedPtr<ITraceObject>>& InChildObjects, TSharedPtr<ISessionTraceFilterService> InFilterService);

	/** Begin ITraceObject overrides */
	virtual FText GetDisplayText() const override;;
	virtual FString GetName() const;
	virtual void SetPending() override;
	virtual bool IsReadOnly() const;
	virtual void SetIsFiltered(bool bState) override;
	virtual bool IsFiltered() const override;
	virtual bool IsPending() const override;
	virtual void GetSearchString(TArray<FString>& OutFilterStrings) const override;
	virtual void GetChildren(TArray<TSharedPtr<ITraceObject>>& OutChildren) const override;
	/** End ITraceObject overrides */

protected:
	/** This channel's name */
	FString Name;
	/** Channel's parent (group) name */
	FString ParentName;
	/** Hash representing this channel uniquely */
	uint32 Hash;
	TArray<TSharedPtr<ITraceObject>> ChildObjects;
	
	/** Whether or not this channel is filtered out, true = filtered; false = not filtered */
	bool bFiltered;
	bool bIsPending;	
	bool bReadOnly;

	TSharedPtr<ISessionTraceFilterService> FilterService;
};

