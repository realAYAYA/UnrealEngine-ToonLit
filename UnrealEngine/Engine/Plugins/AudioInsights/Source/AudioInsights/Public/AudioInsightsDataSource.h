// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"


namespace UE::Audio::Insights
{
	class AUDIOINSIGHTS_API IDashboardDataViewEntry : public TSharedFromThis<IDashboardDataViewEntry>
	{
	public:
		virtual ~IDashboardDataViewEntry() = default;

		virtual bool IsValid() const = 0;
	};

	class AUDIOINSIGHTS_API FAudioDataTableView
	{ 
	public:
		FAudioDataTableView()
			: Entries(MakeShared<TArray<TSharedPtr<IDashboardDataViewEntry>>>())
		{
		}

		FAudioDataTableView(TSharedRef<TArray<TSharedPtr<IDashboardDataViewEntry>>> InEntries)
			: Entries(InEntries)
		{
		}

		virtual ~FAudioDataTableView() = default;

		TSharedRef<const TArray<TSharedPtr<IDashboardDataViewEntry>>> GetEntries() const
		{
			return Entries;
		}

		virtual bool FilterViewEntries() = 0;

		virtual void ResetViewEntries()
		{
			Entries->Empty();
			OnViewEntriesUpdatedDelegate.Broadcast();
		}

		DECLARE_MULTICAST_DELEGATE(FOnViewEntriesUpdatedDelegate);
		FOnViewEntriesUpdatedDelegate OnViewEntriesUpdatedDelegate;

	protected:
		TSharedRef<TArray<TSharedPtr<IDashboardDataViewEntry>>> Entries;
	};
} // namespace UE::Audio::Insights
