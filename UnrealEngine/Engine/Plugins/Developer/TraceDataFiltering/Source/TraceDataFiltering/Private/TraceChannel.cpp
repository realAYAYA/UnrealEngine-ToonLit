// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceChannel.h"
#include "Internationalization/Text.h"
#include "ISessionTraceFilterService.h"

FTraceChannel::FTraceChannel(FString InName, FString InParentName, uint32 InHash, bool bInEnabled, bool bInReadOnly, const TArray<TSharedPtr<ITraceObject>>& InChildObjects, TSharedPtr<ISessionTraceFilterService> InFilterService) : Name(InName), ParentName(InParentName), Hash(InHash), ChildObjects(InChildObjects), bFiltered(!bInEnabled), bIsPending(false), bReadOnly(bInReadOnly), FilterService(InFilterService)
{

}

FText FTraceChannel::GetDisplayText() const
{
	return FText::FromString(Name);
}

FString FTraceChannel::GetName() const
{
	return Name;
}

void FTraceChannel::SetPending()
{
	bIsPending = true;
}


bool FTraceChannel::IsReadOnly() const
{
	return bReadOnly;
}

void FTraceChannel::SetIsFiltered(bool bState)
{
	SetPending();
	FilterService->SetObjectFilterState(Name, !bState);
}

bool FTraceChannel::IsFiltered() const
{
	return bFiltered;
}

bool FTraceChannel::IsPending() const
{
	return bIsPending;
}

void FTraceChannel::GetSearchString(TArray<FString>& OutFilterStrings) const
{
	OutFilterStrings.Add(Name);
}

void FTraceChannel::GetChildren(TArray<TSharedPtr<ITraceObject>>& OutChildren) const
{
	OutChildren.Append(ChildObjects);
}

