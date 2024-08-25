// Copyright Epic Games, Inc. All Rights Reserved.

#include "Providers/AudioBusProvider.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioInsightsDashboardFactory.h"
#include "Sound/AudioBus.h"

namespace UE::Audio::Insights
{
	FAudioBusProvider::FAudioBusProvider()
		: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FAudioBusAssetDashboardEntry>>(GetName_Static())
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FAudioBusProvider::OnAssetAdded);
		AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FAudioBusProvider::OnAssetRemoved);
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FAudioBusProvider::OnFilesLoaded);

		FDashboardFactory::OnActiveAudioDeviceChanged.AddRaw(this, &FAudioBusProvider::OnActiveAudioDeviceChanged);
	}
	
	FAudioBusProvider::~FAudioBusProvider()
	{
		if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
		{
			AssetRegistryModule->Get().OnAssetAdded().RemoveAll(this);
			AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
			AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
		}

		FDashboardFactory::OnActiveAudioDeviceChanged.RemoveAll(this);
	}

	FName FAudioBusProvider::GetName_Static()
	{
		return "AudioBusProvider";
	}

	void FAudioBusProvider::OnAssetAdded(const FAssetData& InAssetData)
	{
		if (bAreFilesLoaded && InAssetData.AssetClassPath == FTopLevelAssetPath(UAudioBus::StaticClass()))
		{
			AddAudioBusAsset(InAssetData);
		}
	}

	void FAudioBusProvider::OnAssetRemoved(const FAssetData& InAssetData)
	{
		if (InAssetData.AssetClassPath == FTopLevelAssetPath(UAudioBus::StaticClass()))
		{
			RemoveAudioBusAsset(InAssetData);
		}
	}

	void FAudioBusProvider::OnFilesLoaded()
	{
		bAreFilesLoaded = true;
		UpdateAudioBusAssetNames();
	}

	void FAudioBusProvider::OnActiveAudioDeviceChanged()
	{
		UpdateAudioBusAssetNames();
	}

	void FAudioBusProvider::AddAudioBusAsset(const FAssetData& InAssetData)
	{
		const bool bIsAudioBusAssetAlreadAdded = AudioBusDataViewEntries.ContainsByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusAssetDashboardEntry)
			{
				return AudioBusAssetDashboardEntry->Name == AssetName;
			});

		if (!bIsAudioBusAssetAlreadAdded)
		{
			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const ::Audio::FDeviceId DeviceId = InsightsModule.GetDeviceId();

			TSharedPtr<FAudioBusAssetDashboardEntry> AudioBusAssetDashboardEntry = MakeShared<FAudioBusAssetDashboardEntry>();
			AudioBusAssetDashboardEntry->DeviceId = DeviceId;
			AudioBusAssetDashboardEntry->Name     = InAssetData.GetObjectPathString();
			AudioBusAssetDashboardEntry->AudioBus = Cast<UAudioBus>(InAssetData.GetAsset());

			AudioBusDataViewEntries.Add(MoveTemp(AudioBusAssetDashboardEntry));

			OnAudioBusAssetAdded.Broadcast(InAssetData.GetAsset());
			++LastUpdateId;
		}
	}

	void FAudioBusProvider::RemoveAudioBusAsset(const FAssetData& InAssetData)
	{
		const int32 FoundAudioBusAssetNameIndex = AudioBusDataViewEntries.IndexOfByPredicate(
			[AssetName = InAssetData.GetObjectPathString()](const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusAssetDashboardEntry)
			{
				return AudioBusAssetDashboardEntry->Name == AssetName;
			});

		if (FoundAudioBusAssetNameIndex != INDEX_NONE)
		{
			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const ::Audio::FDeviceId DeviceId = InsightsModule.GetDeviceId();

			RemoveDeviceEntry(DeviceId, AudioBusDataViewEntries[FoundAudioBusAssetNameIndex]->AudioBus->GetUniqueID());

			AudioBusDataViewEntries.RemoveAt(FoundAudioBusAssetNameIndex);

			OnAudioBusAssetRemoved.Broadcast(InAssetData.GetAsset());
			++LastUpdateId;
		}
	}

	void FAudioBusProvider::UpdateAudioBusAssetNames()
	{
		// Get all UAudioBus assets
		TArray<FAssetData> AssetDataArray;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().GetAssetsByClass(FTopLevelAssetPath(UAudioBus::StaticClass()), AssetDataArray);

		// Build AudioBusDataViewEntries
		Reset();
		AudioBusDataViewEntries.Empty();

		for (const FAssetData& AssetData : AssetDataArray)
		{
			AddAudioBusAsset(AssetData);
		}

		AudioBusDataViewEntries.Sort([](const TSharedPtr<FAudioBusAssetDashboardEntry>& A, const TSharedPtr<FAudioBusAssetDashboardEntry>& B)
		{
			return A->GetDisplayName().CompareToCaseIgnored(B->GetDisplayName()) < 0;
		});

		OnAudioBusAssetListUpdated.Broadcast();
	}

	bool FAudioBusProvider::ProcessMessages()
	{
		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		const ::Audio::FDeviceId DeviceId = InsightsModule.GetDeviceId();

		for (uint32 Index = 0;
			 const TSharedPtr<FAudioBusAssetDashboardEntry>& AudioBusDataViewEntry : AudioBusDataViewEntries)
		{
			UpdateDeviceEntry(DeviceId, AudioBusDataViewEntry->AudioBus->GetUniqueID(), [&AudioBusDataViewEntry](TSharedPtr<FAudioBusAssetDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = AudioBusDataViewEntry;
				}
			});

			++Index;
		}

		return true;
	}

	UE::Trace::IAnalyzer* FAudioBusProvider::ConstructAnalyzer()
	{
		return nullptr;
	}
} // namespace UE::Audio::Insights
