// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "Views/SoundSubmixAssetDashboardEntry.h"
#include "Views/TableDashboardViewFactory.h"
#include "Widgets/Input/SCheckBox.h"

class USoundSubmix;

namespace UE::Audio::Insights
{
	class FSubmixesDashboardViewFactory : public FTraceObjectTableDashboardViewFactory
	{
	public:
		FSubmixesDashboardViewFactory();
		virtual ~FSubmixesDashboardViewFactory();

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSubmixAssetChecked, const bool /*bIsChecked*/, TWeakObjectPtr<USoundSubmix> /*SoundSubmix*/);
		inline static FOnSubmixAssetChecked OnSubmixAssetChecked;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmixSelectionChanged, const TWeakObjectPtr<USoundSubmix> /*SoundSubmix*/);
		inline static FOnSubmixSelectionChanged OnSubmixSelectionChanged;

	protected:
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& InColumnName) override;
		virtual void ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;

		virtual void SortTable() override;

		virtual void OnSelectionChanged(TSharedPtr<IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo) override;

		void RequestListRefresh();

		void HandleOnSubmixAssetListUpdated(const TWeakObjectPtr<UObject> InAsset);

		TMap<const TWeakObjectPtr<USoundSubmix>, bool> SubmixCheckboxCheckedStates;
	};
} // namespace UE::Audio::Insights
