// Copyright Epic Games, Inc. All Rights Reserved.
#include "Views/MixerSourceDashboardViewFactory.h"

#include "Audio/AudioDebug.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsDashboardAssetCommands.h"
#include "AudioInsightsModule.h"
#include "AudioInsightsStyle.h"
#include "Editor.h"
#include "Internationalization/Text.h"
#include "IPropertyTypeCustomization.h"
#include "Providers/MixerSourceTraceProvider.h"
#include "SSimpleTimeSlider.h"
#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "AudioInsights"


namespace UE::Audio::Insights
{
	namespace MixerSourcePrivate
	{
		const FMixerSourceDashboardEntry& CastEntry(const IDashboardDataViewEntry& InData)
		{
			return static_cast<const FMixerSourceDashboardEntry&>(InData);
		};

		static const FText PlotColumnSelectDescription = LOCTEXT("AudioDashboard_MixerSources_SelectPlotColumnDescription", "Select a column from the table to plot.");
	} // namespace MixerSourcePrivate

	const double FMixerSourceDashboardViewFactory::MaxPlotHistorySeconds = 5.0;
	const int32 FMixerSourceDashboardViewFactory::MaxPlotSources = 32;

	FMixerSourceDashboardViewFactory::FMixerSourceDashboardViewFactory()
	{
		const FTraceModule& TraceModule = FAudioInsightsModule::GetChecked().GetTraceModule();
		Providers = TArray<TSharedPtr<FTraceProviderBase>>
		{
			TraceModule.FindAudioTraceProvider<FMixerSourceTraceProvider>()
		};
	}

	FName FMixerSourceDashboardViewFactory::GetName() const
	{
		return "MixerSources";
	}

	FText FMixerSourceDashboardViewFactory::GetDisplayName() const
	{
		return LOCTEXT("AudioDashboard_MixerSources_DisplayName", "Sources");
	}

	FSlateIcon FMixerSourceDashboardViewFactory::GetIcon() const
	{
		return FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Sources");
	}

	EDefaultDashboardTabStack FMixerSourceDashboardViewFactory::GetDefaultTabStack() const
	{
		return EDefaultDashboardTabStack::Analysis;
	}

	const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& FMixerSourceDashboardViewFactory::GetColumns() const
	{
		auto CreateColumnData = []()
		{

			return TMap<FName, FTraceTableDashboardViewFactory::FColumnData>
			{
				{
					"PlayOrder",
					{
						LOCTEXT("PlayOrder_PlayOrderColumnDisplayName", "Play Order"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).PlayOrder); },
						true /* bDefaultHidden */,
						0.08f /* FillWidth */
					}
				},
				{
					"Name",
					{
						LOCTEXT("Source_NameColumnDisplayName", "Name"),
						[](const IDashboardDataViewEntry& InData) { return FText::FromString(*FSoftObjectPath(MixerSourcePrivate::CastEntry(InData).Name).GetAssetName()); },
						false /* bDefaultHidden */,
						0.75f /* FillWidth */
					}
				},
				{
					"Amplitude",
					{
						LOCTEXT("Source_EnvColumnDisplayName", "Amp (Peak)"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).Envelope, FSlateStyle::Get().GetAmpFloatFormat()); },
						false /* bDefaultHidden */,
						0.12f /* FillWidth */
					}
				},
				{
					"Volume",
					{
						LOCTEXT("Source_VolumeColumnDisplayName", "Volume"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).Volume, FSlateStyle::Get().GetAmpFloatFormat()); },
						false /* bDefaultHidden */,
						0.07f /* FillWidth */
					}
				},
				{
					"DistanceAttenuation",
					{
						LOCTEXT("Source_AttenuationColumnDisplayName", "Distance Attenuation"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).DistanceAttenuation, FSlateStyle::Get().GetAmpFloatFormat()); },
						true  /* bDefaultHidden */,
						0.15f /* FillWidth */
					}
				},
				{
					"Pitch",
					{
						LOCTEXT("Source_PitchColumnDisplayName", "Pitch"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).Pitch, FSlateStyle::Get().GetPitchFloatFormat()); },
						false /* bDefaultHidden */,
						0.06f /* FillWidth */
					}
				},
				{
					"LPF",
					{
						LOCTEXT("Source_LPFColumnDisplayName", "LPF Freq (Hz)"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).LPFFreq, FSlateStyle::Get().GetFreqFloatFormat()); },
						true  /* bDefaultHidden */,
						0.1f /* FillWidth */
					}
				},
				{
					"HPF",
					{
						LOCTEXT("Source_HPFColumnDisplayName", "HPF Freq (Hz)"),
						[](const IDashboardDataViewEntry& InData) { return FText::AsNumber(MixerSourcePrivate::CastEntry(InData).HPFFreq, FSlateStyle::Get().GetFreqFloatFormat()); },
						true  /* bDefaultHidden */,
						0.1f /* FillWidth */
					}
				}
			};
		};
		static const TMap<FName, FTraceTableDashboardViewFactory::FColumnData> ColumnData = CreateColumnData();
		return ColumnData;
	}

	void FMixerSourceDashboardViewFactory::SortTable()
	{
		if (SortByColumn == "PlayOrder")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.PlayOrder < BData.PlayOrder;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.PlayOrder < AData.PlayOrder;
				});
			}
		}
		else if (SortByColumn == "Name")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.GetDisplayName().CompareToCaseIgnored(BData.GetDisplayName()) < 0;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.GetDisplayName().CompareToCaseIgnored(AData.GetDisplayName()) < 0;
				});
			}
		}
		else if (SortByColumn == "Amplitude")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.Envelope < BData.Envelope;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.Envelope < AData.Envelope;
				});
			}
		}
		else if (SortByColumn == "Volume")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.Volume < BData.Volume;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.Volume < AData.Volume;
				});
			}
		}
		else if (SortByColumn == "DistanceAttenuation")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.DistanceAttenuation < BData.DistanceAttenuation;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.DistanceAttenuation < AData.DistanceAttenuation;
				});
			}
		}
		else if (SortByColumn == "Pitch")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.Pitch < BData.Pitch;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.Pitch < AData.Pitch;
				});
			}
		}
		else if (SortByColumn == "LPF")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.LPFFreq < BData.LPFFreq;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.LPFFreq < AData.LPFFreq;
				});
			}
		}
		else if (SortByColumn == "HPF")
		{
			if (SortMode == EColumnSortMode::Ascending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return AData.HPFFreq < BData.HPFFreq;
				});
			}
			else if (SortMode == EColumnSortMode::Descending)
			{
				DataViewEntries.Sort([](const TSharedPtr<IDashboardDataViewEntry>& A, const TSharedPtr<IDashboardDataViewEntry>& B)
				{
					const FMixerSourceDashboardEntry& AData = MixerSourcePrivate::CastEntry(*A.Get());
					const FMixerSourceDashboardEntry& BData = MixerSourcePrivate::CastEntry(*B.Get());

					return BData.HPFFreq < AData.HPFFreq;
				});
			}
		}
	}

	void FMixerSourceDashboardViewFactory::ResetPlots()
	{
		for (const auto& KVP : PlotWidgetCurveIdToPointDataMapPerColumn)
		{
			const TSharedPtr<FPointDataPerCurveMap>& PointDataPerCurveMap = KVP.Value;
			PointDataPerCurveMap->Empty();
		}

		PlotWidgetMetadataPerCurve->Empty();

		BeginTimestamp = TNumericLimits<double>::Max();
		CurrentTimestamp = 0;
	}

	void FMixerSourceDashboardViewFactory::OnPIEStopped(bool bSimulating)
	{
		ResetPlots();
	}

#if AUDIO_INSIGHTS_SHOW_SOURCE_CONTEXT_MENU
	TSharedPtr<SWidget> FMixerSourceDashboardViewFactory::OnConstructContextMenu()
	{
		const FDashboardAssetCommands& Commands = FDashboardAssetCommands::Get();

		TSharedPtr<FUICommandList> CommandList = MakeShared<FUICommandList>();
		CommandList->MapAction(Commands.GetMuteCommand(), FExecuteAction::CreateRaw(this, &FMixerSourceDashboardViewFactory::MuteSound));
		CommandList->MapAction(Commands.GetSoloCommand(), FExecuteAction::CreateRaw(this, &FMixerSourceDashboardViewFactory::SoloSound));
		CommandList->MapAction(Commands.GetClearMuteSoloCommand(), FExecuteAction::CreateRaw(this, &FMixerSourceDashboardViewFactory::ClearMutesAndSolos));

		constexpr bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

		MenuBuilder.BeginSection("SoundActions", LOCTEXT("SoundActions_Header", "Sound Actions"));
		{
			MenuBuilder.AddMenuEntry(Commands.GetMuteCommand());
			MenuBuilder.AddMenuEntry(Commands.GetSoloCommand());
			MenuBuilder.AddMenuEntry(Commands.GetClearMuteSoloCommand());
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}
#endif // AUDIO_INSIGHTS_SHOW_SOURCE_CONTEXT_MENU 

	FSlateColor FMixerSourceDashboardViewFactory::GetRowColor(const TSharedPtr<IDashboardDataViewEntry>& InRowDataPtr)
	{
		FColor RowTextColor(255, 255, 255);

#if ENABLE_AUDIO_DEBUG
		if (const FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
		{
			const FSoundAssetDashboardEntry& SoundAssetDashboardEntry = *StaticCastSharedPtr<FSoundAssetDashboardEntry>(InRowDataPtr).Get();
			const FName SoundAssetName { SoundAssetDashboardEntry.Name };
			const bool bIsSolo = AudioDeviceManager->GetDebugger().IsSoloSoundWave(SoundAssetName);
			if (bIsSolo)
			{
				RowTextColor = FColor(255, 255, 0);
			}
			else
			{
				const bool bIsMute = AudioDeviceManager->GetDebugger().IsMuteSoundWave(SoundAssetName);
				if (bIsMute)
				{
					RowTextColor = FColor(255, 0, 0);
				}
			}
		}
#endif // ENABLE_AUDIO_DEBUG

		return FSlateColor(RowTextColor);
	}

	void FMixerSourceDashboardViewFactory::ToggleMuteForAllItems(ECheckBoxState NewState)
	{
#if ENABLE_AUDIO_DEBUG
		if (MuteState != NewState)
		{
			MuteState = NewState;
			UpdateSoloMuteState();
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::ToggleSoloForAllItems(ECheckBoxState NewState)
	{
#if ENABLE_AUDIO_DEBUG
		if (SoloState != NewState)
		{
			SoloState = NewState;
			UpdateSoloMuteState();
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::MuteSound()
	{
#if ENABLE_AUDIO_DEBUG
		if (FilteredEntriesListView.IsValid())
		{
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				const TArray<TSharedPtr<IDashboardDataViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();

				for (const TSharedPtr<IDashboardDataViewEntry>& SelectedItem : SelectedItems)
				{
					if (SelectedItem.IsValid())
					{
						const FSoundAssetDashboardEntry& SoundAssetDashboardEntry = *StaticCastSharedPtr<FSoundAssetDashboardEntry>(SelectedItem).Get();
						const FName SoundAssetDisplayName { SoundAssetDashboardEntry.Name };
						AudioDeviceManager->GetDebugger().ToggleMuteSoundWave(SoundAssetDisplayName);
					}
				}

				// Handle general Mute button state
				bool bIsAnySoundMuted = false;

				const TArrayView<const TSharedPtr<IDashboardDataViewEntry>> TableItems = FilteredEntriesListView->GetItems();

				for (const TSharedPtr<IDashboardDataViewEntry>& Item : TableItems)
				{
					if (!Item.IsValid())
					{
						continue;
					}

					const FSoundAssetDashboardEntry& SoundAssetDashboardEntry = *StaticCastSharedPtr<FSoundAssetDashboardEntry>(Item).Get();
					const FName SoundAssetDisplayName { SoundAssetDashboardEntry.Name };
					if (AudioDeviceManager->GetDebugger().IsMuteSoundWave(SoundAssetDisplayName))
					{
						bIsAnySoundMuted = true;
						break;
					}
				}

				MuteToggleButton->SetIsChecked(bIsAnySoundMuted ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
			}
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::SoloSound()
	{
#if ENABLE_AUDIO_DEBUG
		if (FilteredEntriesListView.IsValid())
		{
			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				const TArray<TSharedPtr<IDashboardDataViewEntry>> SelectedItems = FilteredEntriesListView->GetSelectedItems();

				for (const TSharedPtr<IDashboardDataViewEntry>& SelectedItem : SelectedItems)
				{
					if (SelectedItem.IsValid())
					{
						const FSoundAssetDashboardEntry& SoundAssetDashboardEntry = *StaticCastSharedPtr<FSoundAssetDashboardEntry>(SelectedItem).Get();
						const FName SoundAssetDisplayName { SoundAssetDashboardEntry.Name };
						AudioDeviceManager->GetDebugger().ToggleSoloSoundWave(SoundAssetDisplayName);
					}
				}

				// Handle general Solo button state
				bool bIsAnySoundSoloed = false;

				const TArrayView<const TSharedPtr<IDashboardDataViewEntry>> TableItems = FilteredEntriesListView->GetItems();

				for (const TSharedPtr<IDashboardDataViewEntry>& Item : TableItems)
				{
					if (!Item.IsValid())
					{
						continue;
					}

					const FSoundAssetDashboardEntry& SoundAssetDashboardEntry = *StaticCastSharedPtr<FSoundAssetDashboardEntry>(Item).Get();
					const FName SoundAssetDisplayName { SoundAssetDashboardEntry.Name };
					if (AudioDeviceManager->GetDebugger().IsSoloSoundWave(SoundAssetDisplayName))
					{
						bIsAnySoundSoloed = true;
						break;
					}
				}

				SoloToggleButton->SetIsChecked(bIsAnySoundSoloed ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
			}
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::ClearMutesAndSolos()
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
		{
			AudioDeviceManager->GetDebugger().ClearMutesAndSolos();

			MuteToggleButton->SetIsChecked(ECheckBoxState::Unchecked);
			SoloToggleButton->SetIsChecked(ECheckBoxState::Unchecked);
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::UpdateSoloMuteState()
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
		{
			FName CurrentFilterStringName = FName{ CurrentFilterString };
			if (MuteState == ECheckBoxState::Checked && !CurrentFilterString.IsEmpty())
			{
				AudioDeviceManager->GetDebugger().ToggleMuteSoundWave(CurrentFilterStringName, true);
			}
			else
			{
				AudioDeviceManager->GetDebugger().ToggleMuteSoundWave(NAME_None, true);
			}

			if (SoloState == ECheckBoxState::Checked && !CurrentFilterString.IsEmpty())
			{
				AudioDeviceManager->GetDebugger().ToggleSoloSoundWave(CurrentFilterStringName, true);
			}
			else
			{
				AudioDeviceManager->GetDebugger().ToggleSoloSoundWave(NAME_None, true);
			}
		}
#endif
	}

	void FMixerSourceDashboardViewFactory::UpdatePlotsWidgetsData()
	{
		if (DataViewEntries.Num() <= 0)
		{
			return;
		}

		// Process new data
		bool bHasNewMetadata = false;
		for (const TSharedPtr<IDashboardDataViewEntry>& DataEntry : DataViewEntries)
		{
			const FMixerSourceDashboardEntry& SourceDataPoint = MixerSourcePrivate::CastEntry(*DataEntry);
			const uint32 SourceId = SourceDataPoint.SourceId; 

			if (SourceId == INDEX_NONE)
			{
				break;
			}
			
			// Only add new sources if there are less than the max 
			const bool bCanAddNewSources = PlotWidgetMetadataPerCurve->Num() < MaxPlotSources;

			const double PointTime = SourceDataPoint.Timestamp;
			BeginTimestamp = FMath::Min(BeginTimestamp, PointTime);
			CurrentTimestamp = FMath::Max(CurrentTimestamp, PointTime);
			const double DataPointTime = SourceDataPoint.Timestamp - BeginTimestamp;

			// For each column, get the array for this data point's source id and add the value to that data array
			for (auto Iter = PlotWidgetCurveIdToPointDataMapPerColumn.CreateIterator(); Iter; ++Iter)
			{
				const FName& ColumnName = Iter.Key();
				auto DataFunc = GetPlotColumnDataFunc(ColumnName);
				TSharedPtr<FPointDataPerCurveMap>& DataMap = Iter.Value();

				// Add new data point array
				if (bCanAddNewSources && !DataMap->Contains(SourceId))
				{
					DataMap->Add(SourceId);
				}

				// Get the data point array for this source id, add new point
				TArray<FPlotCurvePoint>* DataPoints = DataMap->Find(SourceId);
				if (DataPoints)
				{
					const float Value = (DataFunc)(SourceDataPoint);
					DataPoints->Emplace(DataPointTime, Value);
				}
			}

			// Create metadata for this curve if necessary 
			if (bCanAddNewSources && !PlotWidgetMetadataPerCurve->Contains(SourceId))
			{
				FPlotCurveMetadata& NewMetadata = PlotWidgetMetadataPerCurve->Add(SourceId);
				NewMetadata.CurveId = SourceId;
				NewMetadata.CurveColor = FLinearColor(FColor::MakeRandomColor()); 
				NewMetadata.DisplayName = FText::FromString(*FSoftObjectPath(SourceDataPoint.Name).GetAssetName());
				bHasNewMetadata = true;
			}
		}

		// Set metadata for each widget if updated
		if (bHasNewMetadata)
		{
			for (TSharedPtr<SAudioCurveView> PlotWidget : PlotWidgets)
			{
				PlotWidget->SetCurvesMetadata(PlotWidgetMetadataPerCurve);
			}
		}

		// Remove points that are older than max history limit from the most recent timestamp
		const static auto RemoveOldCurvePoints = [this](TSharedPtr<FPointDataPerCurveMap> PlotWidgetPointDataPerCurve)
		{
			for (auto Iter = PlotWidgetPointDataPerCurve->CreateIterator(); Iter; ++Iter)
			{
				TArray<FPlotCurvePoint>& CurvePoints = Iter->Value;
				for (int32 i = CurvePoints.Num() - 1; i >= 0; --i)
				{
					const FPlotCurvePoint& Point = CurvePoints[i];
					if (Point.Key + BeginTimestamp < CurrentTimestamp - MaxPlotHistorySeconds)
					{
						CurvePoints.RemoveAt(i);
					}
				}
			}
		};

		// Remove old points and set curve data for each widget 
		for (int32 WidgetIndex = 0; WidgetIndex < NumPlotWidgets; ++WidgetIndex)
		{
			TSharedPtr<SAudioCurveView> PlotWidget = PlotWidgets[WidgetIndex];
			const FName& SelectedPlotColumn = SelectedPlotColumnNames[WidgetIndex];

			if (TSharedPtr<FPointDataPerCurveMap> CurveData = *PlotWidgetCurveIdToPointDataMapPerColumn.Find(SelectedPlotColumn))
			{
				RemoveOldCurvePoints(CurveData);
				PlotWidget->SetCurvesPointData(CurveData);
			}
		}
	}

	const TMap<FName, FMixerSourceDashboardViewFactory::FPlotColumnInfo>& FMixerSourceDashboardViewFactory::GetPlotColumnInfo()
	{
		auto CreatePlotColumnInfo = []()
		{
			return TMap<FName, FMixerSourceDashboardViewFactory::FPlotColumnInfo>
			{
				{
					"Amplitude",
					{
						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).Envelope; },
						FSlateStyle::Get().GetAmpFloatFormat()
					}
				},
				{
					"Volume",
					{

						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).Volume; },
						FSlateStyle::Get().GetAmpFloatFormat()
					}
				},
				{
					"DistanceAttenuation",
					{
						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).DistanceAttenuation; }, 
						FSlateStyle::Get().GetAmpFloatFormat()
					}
				},
				{
					"Pitch",
					{
						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).Pitch; },
						FSlateStyle::Get().GetPitchFloatFormat()
					}
				},
				{
					"LPF",
					{
						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).LPFFreq; },
							FSlateStyle::Get().GetFreqFloatFormat() 
					}
				},
				{
					"HPF",
					{
						[](const IDashboardDataViewEntry& InData) { return MixerSourcePrivate::CastEntry(InData).HPFFreq; },
						FSlateStyle::Get().GetFreqFloatFormat()
					}
				}
			};
		};
		static const TMap<FName, FMixerSourceDashboardViewFactory::FPlotColumnInfo> ColumnInfo = CreatePlotColumnInfo();
		return ColumnInfo;
	}

	const FNumberFormattingOptions* FMixerSourceDashboardViewFactory::GetPlotColumnNumberFormat(const FName& ColumnName)
	{
		if (const FMixerSourceDashboardViewFactory::FPlotColumnInfo* PlotColumnInfo = GetPlotColumnInfo().Find(ColumnName))
		{
			return PlotColumnInfo->FormatOptions;
		}
		return nullptr;
	}

	const TFunctionRef<float(const IDashboardDataViewEntry& InData)> FMixerSourceDashboardViewFactory::GetPlotColumnDataFunc(const FName& ColumnName)
	{
		return GetPlotColumnInfo().Find(ColumnName)->DataFunc;
	}

	const FText FMixerSourceDashboardViewFactory::GetPlotColumnDisplayName(const FName& ColumnName)
	{
		if (const FTraceTableDashboardViewFactory::FColumnData* ColumnInfo = GetColumns().Find(ColumnName))
		{
			return ColumnInfo->DisplayName;
		}
		return FText::GetEmpty();
	}

	TSharedRef<SWidget> FMixerSourceDashboardViewFactory::MakePlotsWidget()
	{
		// Initialize column options and initially selected columns 
		GetPlotColumnInfo().GenerateKeyArray(ColumnNames);
		if (SelectedPlotColumnNames.IsEmpty() && ColumnNames.Num() > 3)
		{
			SelectedPlotColumnNames.Add(ColumnNames[0]); // Amplitude
			SelectedPlotColumnNames.Add(ColumnNames[3]); // Pitch
		}

		// Initialize curve data and metadata
		if (!PlotWidgetMetadataPerCurve.IsValid())
		{
			PlotWidgetMetadataPerCurve = MakeShared<TMap<int32, SAudioCurveView::FCurveMetadata>>();
			for (const FName& ColumnName : ColumnNames)
			{
				TSharedPtr<FPointDataPerCurveMap> PointDataPerCurveMap = MakeShared<FPointDataPerCurveMap>();
				PlotWidgetCurveIdToPointDataMapPerColumn.Emplace(ColumnName, MoveTemp(PointDataPerCurveMap));
			}
		}

		// Create plot widgets
		auto GetViewRange = [this]()
		{
			return TRange<double>(FMath::Max(0, CurrentTimestamp - MaxPlotHistorySeconds - BeginTimestamp), CurrentTimestamp - BeginTimestamp);
		};

		if (PlotWidgets.IsEmpty())
		{
			PlotWidgets.AddDefaulted(NumPlotWidgets);
			for (int32 WidgetNum = 0; WidgetNum < NumPlotWidgets; ++WidgetNum)
			{
				SAssignNew(PlotWidgets[WidgetNum], SAudioCurveView)
					.ViewRange_Lambda(GetViewRange);
			}
		}

		// Create plot column combo box widgets  
		auto CreatePlotColumnComboBoxWidget = [this](int32 PlotWidgetIndex)
		{
			return SNew(SComboBox<FName>)
				.ToolTipText(MixerSourcePrivate::PlotColumnSelectDescription)
				.OptionsSource(&ColumnNames)
				.OnGenerateWidget_Lambda([this](const FName& ColumnName)
				{
					return SNew(STextBlock)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
					.Text(GetPlotColumnDisplayName(ColumnName));
				})
				.OnSelectionChanged_Lambda([this, PlotWidgetIndex](FName NewColumnName, ESelectInfo::Type)
				{
					SelectedPlotColumnNames[PlotWidgetIndex] = NewColumnName;
					if (TSharedPtr<FPointDataPerCurveMap>* DataMap = PlotWidgetCurveIdToPointDataMapPerColumn.Find(NewColumnName))
					{
						PlotWidgets[PlotWidgetIndex]->SetCurvesPointData(*DataMap);
						PlotWidgets[PlotWidgetIndex]->SetYValueFormattingOptions(*GetPlotColumnNumberFormat(NewColumnName));
					}
				})
				[
					SNew(STextBlock)
					.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
					.Text_Lambda([this, PlotWidgetIndex]()
					{
						return GetPlotColumnDisplayName(SelectedPlotColumnNames[PlotWidgetIndex]);
					})
				];
		};

		return SNew(SVerticalBox)
			.Clipping(EWidgetClipping::ClipToBounds)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SSimpleTimeSlider)
				.ViewRange_Lambda(GetViewRange)
				.ClampRangeHighlightSize(0.0f) // Hide clamp range
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				CreatePlotColumnComboBoxWidget(0)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				PlotWidgets[0].ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				CreatePlotColumnComboBoxWidget(1)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				PlotWidgets[1].ToSharedRef()
			];
	}

	TSharedRef<SWidget> FMixerSourceDashboardViewFactory::MakeMuteSoloWidget()
	{
		// Mute/Solo labels generation
		auto GenerateToggleButtonLabelWidget = [](const FText& InLabel = FText::GetEmpty(), const FName& InTextStyle = TEXT("ButtonText")) -> TSharedRef<SWidget>
		{
       		TSharedPtr<SHorizontalBox> HBox = SNew(SHorizontalBox);
			
       		if (!InLabel.IsEmpty())
       		{
       			HBox->AddSlot()	
				.Padding(0.0f, 0.5f, 0.0f, 0.0f)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle( &FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>(InTextStyle))
					.Justification(ETextJustify::Center)
					.Text(InLabel)
				];
       		}

       		return SNew(SBox)
				.HeightOverride(16.0f)
				[
					HBox.ToSharedRef()
				];
		};
		
		const FSlateColor WhiteColor(FColor::White);

		// Mute button style
		MuteToggleButtonStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBox");
		MuteToggleButtonStyle.BorderBackgroundColor = FSlateColor(FColor(200, 0, 0));

		MuteToggleButtonStyle.CheckedHoveredImage.TintColor = WhiteColor;
		MuteToggleButtonStyle.CheckedImage.TintColor        = WhiteColor;
		MuteToggleButtonStyle.CheckedPressedImage.TintColor = WhiteColor;

		// Solo button style
		SoloToggleButtonStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckBox");
		SoloToggleButtonStyle.BorderBackgroundColor = FSlateColor(FColor(255, 200, 0));

		SoloToggleButtonStyle.CheckedHoveredImage.TintColor = WhiteColor;
		SoloToggleButtonStyle.CheckedImage.TintColor        = WhiteColor;
		SoloToggleButtonStyle.CheckedPressedImage.TintColor = WhiteColor;

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Margin(FMargin(0.0, 2.0, 0.0, 0.0))
				.Text(LOCTEXT("TableDashboardView_GlobalMuteSoloText", "Global Mute/Solo:"))
			]
			+ SHorizontalBox::Slot()
			.MaxWidth(2.0f)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SAssignNew(MuteToggleButton, SCheckBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Style(&MuteToggleButtonStyle)
				.ToolTip(FSlateApplicationBase::Get().MakeToolTip(LOCTEXT("TableDashboardView_MuteButtonTooltipText", "Mute/Unmute all the items in the list.")))
				.IsChecked(ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &FMixerSourceDashboardViewFactory::ToggleMuteForAllItems)
				[
					GenerateToggleButtonLabelWidget(LOCTEXT("TableDashboardView_MuteButtonText", "M"), "SmallButtonText")
				]
			]
			+ SHorizontalBox::Slot()
			.MaxWidth(2.0f)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SAssignNew(SoloToggleButton, SCheckBox)
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Style(&SoloToggleButtonStyle)
				.ToolTip(FSlateApplicationBase::Get().MakeToolTip(LOCTEXT("TableDashboardView_SoloButtonTooltipText", "Enabled/Disable Solo on all the items in the list.")))
				.IsChecked(ECheckBoxState::Unchecked)
				.OnCheckStateChanged(this, &FMixerSourceDashboardViewFactory::ToggleSoloForAllItems)
				[
					GenerateToggleButtonLabelWidget(LOCTEXT("TableDashboardView_SoloButtonText", "S"), "SmallButtonText")
				]
			];
	}

	TSharedRef<SWidget> FMixerSourceDashboardViewFactory::MakeWidget()
	{
		FDashboardFactory::OnActiveAudioDeviceChanged.AddSP(this, &FMixerSourceDashboardViewFactory::ClearMutesAndSolos);
		FEditorDelegates::EndPIE.AddSP(this, &FMixerSourceDashboardViewFactory::OnPIEStopped);

		TSharedRef<SWidget> MuteSoloWidget = MakeMuteSoloWidget();
		TSharedRef<SWidget> TableDashboardWidget = FTraceTableDashboardViewFactory::MakeWidget();
		TSharedRef<SWidget> PlotsWidget = MakePlotsWidget();

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				MuteSoloWidget
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				// Dashboard and plots area
				SNew(SSplitter)
				.Orientation(Orient_Vertical)
				+ SSplitter::Slot()
				.Value(0.68f)
				[
					TableDashboardWidget
				]
				+ SSplitter::Slot()
				.Value(0.32f)
				[
					PlotsWidget
				]
			];
	}	

	void FMixerSourceDashboardViewFactory::ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason InReason)
	{
		const FString FilterString = GetSearchFilterText().ToString();
		FTraceTableDashboardViewFactory::FilterEntries<FMixerSourceTraceProvider>([&FilterString](const IDashboardDataViewEntry& Entry)
		{
			const FMixerSourceDashboardEntry& MixerSourceEntry = MixerSourcePrivate::CastEntry(Entry);
			if (MixerSourceEntry.GetDisplayName().ToString().Contains(FilterString))
			{
				return false;
			}

			return true;
		});

		UpdatePlotsWidgetsData();

#if ENABLE_AUDIO_DEBUG
		// Update the mute and solo states if the filter string changes
		if (CurrentFilterString != FilterString)
		{
			CurrentFilterString = FilterString;
			UpdateSoloMuteState();
		}
#endif
	}

#if WITH_EDITOR
	bool FMixerSourceDashboardViewFactory::IsDebugDrawEnabled() const
	{
		return false;
	}

	void FMixerSourceDashboardViewFactory::DebugDraw(float InElapsed, const IDashboardDataViewEntry& InEntry, ::Audio::FDeviceId DeviceId) const
	{
		// TODO: Get source position if 3d so debug draw works
// 		const FMixerSourceDashboardEntry& LoopData = static_cast<const FMixerSourceDashboardEntry&>(InEntry);
// 		const FRotator& Rotator = LoopData.Rotator;
// 		const FVector& Location = LoopData.Location;
// 		const FString Description = FString::Printf(TEXT("%s [Virt: %.2fs]"), *LoopData.Name, LoopData.TimeVirtualized);
// 
// 		const TArray<UWorld*> Worlds = FAudioDeviceManager::Get()->GetWorldsUsingAudioDevice(DeviceId);
// 		for (UWorld* World : Worlds)
// 		{
// 			DrawDebugSphere(World, Location, 30.0f, 8, FColor::Magenta, false, InElapsed, SDPG_Foreground);
// 			DrawDebugString(World, Location + FVector(0, 0, 32), *Description, nullptr, FColor::Magenta, InElapsed, false, 1.0f);
// 		}
	}
#endif // WITH_EDITOR
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
