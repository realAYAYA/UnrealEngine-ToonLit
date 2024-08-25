// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Views/AudioBusAssetDashboardEntry.h"
#include "Views/AudioBusAssetDashboardEntry.h"
#include "Views/TableDashboardViewFactory.h"
#include "Widgets/Input/SCheckBox.h"

class UAudioBus;

namespace UE::Audio::Insights
{
	class FAudioBusesDashboardViewFactory : public FTraceObjectTableDashboardViewFactory
	{
	public:
		FAudioBusesDashboardViewFactory();
		virtual ~FAudioBusesDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAudioBusAssetChecked, const bool /*bIsChecked*/, const TWeakObjectPtr<UAudioBus> /*AudioBus*/);
		inline static FOnAudioBusAssetChecked OnAudioBusAssetChecked;

	protected:
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName) override;
		virtual void ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;

		void RequestListRefresh();

		void HandleOnAudioBusAssetListUpdated(const TWeakObjectPtr<UObject> InAsset);

		TMap<const TWeakObjectPtr<UAudioBus>, bool> AudioBusCheckboxCheckedStates;
	};
} // namespace UE::Audio::Insights
