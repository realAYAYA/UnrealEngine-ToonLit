// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "AudioInsightsDataSource.h"
#include "Engine/Attenuation.h"
#include "Engine/World.h"
#include "Templates/SharedPointer.h"
#include "Views/DashboardViewFactory.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"


namespace UE::Audio::Insights
{
	class AUDIOINSIGHTS_API IObjectDashboardEntry : public IDashboardDataViewEntry
	{
	public:
		virtual ~IObjectDashboardEntry() = default;

		virtual FText GetDisplayName() const = 0;
		virtual const UObject* GetObject() const = 0;
		virtual UObject* GetObject() = 0;
	};

	struct AUDIOINSIGHTS_API FSoundAssetDashboardEntry : public IObjectDashboardEntry
	{
		virtual ~FSoundAssetDashboardEntry() = default;

		virtual FText GetDisplayName() const override;
		virtual const UObject* GetObject() const override;
		virtual UObject* GetObject() override;
		virtual bool IsValid() const override;

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 PlayOrder = INDEX_NONE;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
		double Timestamp = 0.0;
		FString Name;
	};

	class AUDIOINSIGHTS_API FTraceTableDashboardViewFactory : public FTraceDashboardViewFactoryBase, public TSharedFromThis<FTraceTableDashboardViewFactory>
	{
	public:
		FTraceTableDashboardViewFactory();
		virtual ~FTraceTableDashboardViewFactory();

	protected:
		struct SRowWidget : public SMultiColumnTableRow<TSharedPtr<IDashboardDataViewEntry>>
		{
			SLATE_BEGIN_ARGS(SRowWidget) { }
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<IDashboardDataViewEntry> InData, TSharedRef<FTraceTableDashboardViewFactory> InFactory);
			virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& Column) override;

			TSharedPtr<IDashboardDataViewEntry> Data;
			TSharedPtr<FTraceTableDashboardViewFactory> Factory;
		};

	public:
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& Column);
		virtual FReply OnDataRowKeyInput(const FGeometry& Geometry, const FKeyEvent& KeyEvent) const;
		virtual TSharedRef<SWidget> MakeWidget() override;
		virtual void RebuildFilteredEntriesListView();

		enum class EProcessReason : uint8
		{
			None,
			FilterUpdated,
			EntriesUpdated
		};

	protected:
		void Tick(float InElapsed);

		template<typename TableProviderType>
		bool FilterEntries(TFunctionRef<bool(const IDashboardDataViewEntry&)> IsFiltered)
		{
			const TSharedPtr<const TableProviderType> Provider = FindProvider<const TableProviderType>();
			if (Provider.IsValid())
			{
				if (const typename TableProviderType::FDeviceData* DeviceData = Provider->FindFilteredDeviceData())
				{
					DataViewEntries.Reset();

					auto TransformEntry = [](const typename TableProviderType::FEntryPair& Pair)
					{
						return StaticCastSharedPtr<IDashboardDataViewEntry>(Pair.Value);
					};

					if (SearchBoxFilterText.IsEmpty())
					{
						Algo::Transform(*DeviceData, DataViewEntries, TransformEntry);
					}
					else
					{
						auto FilterEntry = [this, &IsFiltered](const typename TableProviderType::FEntryPair& Pair)
						{
							return !IsFiltered((const IDashboardDataViewEntry&)(*Pair.Value));
						};
						Algo::TransformIf(*DeviceData, DataViewEntries, FilterEntry, TransformEntry);
					}

					// Temp fix for UE-201431 (SoundAsset entries with empty name are displayed in the Sources tab)
					DataViewEntries.RemoveAll([](const TSharedPtr<IDashboardDataViewEntry>& Item)
					{
						const TSharedPtr<FSoundAssetDashboardEntry> SoundAssetDashboardEntry = StaticCastSharedPtr<FSoundAssetDashboardEntry>(Item);

						return SoundAssetDashboardEntry.IsValid() && SoundAssetDashboardEntry->Name.IsEmpty();
					});
					
					// Sort list
					RequestSort();

					return true;
				}
				else
				{
					if (!DataViewEntries.IsEmpty())
					{
						DataViewEntries.Empty();
						return true;
					}
				}
			}

			return false;
		}

#if WITH_EDITOR
		virtual bool IsDebugDrawEnabled() const { return false; }
		virtual void DebugDraw(float InElapsed, const IDashboardDataViewEntry& InDashboardEntry, ::Audio::FDeviceId DeviceId) const { }
#endif // WITH_EDITOR

		struct FColumnData
		{
			const FText DisplayName;
			const TFunction<FText(const IDashboardDataViewEntry&)> GetDisplayValue;
			bool bDefaultHidden = false;
			const float FillWidth = 1.0f;
			const EHorizontalAlignment Alignment = HAlign_Left;
		};

		virtual const TMap<FName, FColumnData>& GetColumns() const = 0;
		virtual void ProcessEntries(EProcessReason Reason) = 0;

		const FText& GetSearchFilterText() const;

		virtual void SortTable() = 0;

		virtual TSharedPtr<SWidget> OnConstructContextMenu();
		virtual void OnSelectionChanged(TSharedPtr<IDashboardDataViewEntry> SelectedItem, ESelectInfo::Type SelectInfo);
		virtual FSlateColor GetRowColor(const TSharedPtr<IDashboardDataViewEntry>& InRowDataPtr);

		EProcessReason UpdateFilterReason = EProcessReason::None;
		FTSTicker::FDelegateHandle TickerHandle;

		FDelegateHandle OnEntriesUpdatedHandle;

		TArray<TSharedPtr<IDashboardDataViewEntry>> DataViewEntries;
		TMap<FName, uint64> UpdateIds;

		TSharedPtr<SWidget> DashboardWidget;
		TSharedPtr<SListView<TSharedPtr<IDashboardDataViewEntry>>> FilteredEntriesListView;

		FName SortByColumn;
		EColumnSortMode::Type SortMode;

	private:
		TSharedRef<SHeaderRow> MakeHeaderRowWidget();
		void SetSearchBoxFilterText(const FText& NewText);

		EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
		void RequestSort();
		void OnColumnSortModeChanged(const EColumnSortPriority::Type InSortPriority, const FName& InColumnId, const EColumnSortMode::Type InSortMode);

		TSharedPtr<SSearchBox> SearchBoxWidget;
		FText SearchBoxFilterText;
	};

	class AUDIOINSIGHTS_API FTraceObjectTableDashboardViewFactory : public FTraceTableDashboardViewFactory
	{
	public:
		virtual ~FTraceObjectTableDashboardViewFactory() = default;

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(TSharedRef<IDashboardDataViewEntry> InRowData, const FName& Column) override;
		virtual FReply OnDataRowKeyInput(const FGeometry& Geometry, const FKeyEvent& KeyEvent) const override;
		virtual TSharedRef<SWidget> MakeWidget() override;

	protected:
		virtual TSharedRef<SWidget> MakeAssetMenuBar() const;

	private:
		TArray<UObject*> GetSelectedEditableAssets() const;

		bool OpenAsset() const;
		bool BrowseToAsset() const;
	};

	class AUDIOINSIGHTS_API FSoundAttenuationVisualizer
	{
		mutable TMultiMap<EAttenuationShape::Type, FBaseAttenuationSettings::AttenuationShapeDetails> ShapeDetailsMap;
		mutable uint32 LastObjectId = INDEX_NONE;

	public:
		const FColor Color { 155, 155, 255 };

	public:
		FSoundAttenuationVisualizer(const FColor& InColor)
			: Color(InColor)
		{
		}

		void Draw(float InDeltaTime, const FTransform& InTransform, const UObject& InObject, const UWorld& InWorld) const;
	};
} // namespace UE::Audio::Insights
