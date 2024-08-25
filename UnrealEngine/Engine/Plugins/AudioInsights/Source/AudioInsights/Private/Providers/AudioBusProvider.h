// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "UObject/NameTypes.h"
#include "Views/AudioBusAssetDashboardEntry.h"

namespace UE::Audio::Insights
{
	class FAudioBusProvider : public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FAudioBusAssetDashboardEntry>>, public TSharedFromThis<FAudioBusProvider>
	{
	public:
		FAudioBusProvider();
		virtual ~FAudioBusProvider();

		static FName GetName_Static();

		virtual UE::Trace::IAnalyzer* ConstructAnalyzer() override;
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioBusAssetAdded, const TWeakObjectPtr<UObject> /*Asset*/);
		inline static FOnAudioBusAssetAdded OnAudioBusAssetAdded;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioBusAssetRemoved, const TWeakObjectPtr<UObject> /*Asset*/);
		inline static FOnAudioBusAssetRemoved OnAudioBusAssetRemoved;

		DECLARE_MULTICAST_DELEGATE(FOnAudioBusAssetListUpdated);
		inline static FOnAudioBusAssetListUpdated OnAudioBusAssetListUpdated;

	private:
		void OnAssetAdded(const FAssetData& InAssetData);
		void OnAssetRemoved(const FAssetData& InAssetData);
		void OnFilesLoaded();
		void OnActiveAudioDeviceChanged();

		void AddAudioBusAsset(const FAssetData& InAssetData);
		void RemoveAudioBusAsset(const FAssetData& InAssetData);

		void UpdateAudioBusAssetNames();

		virtual bool ProcessMessages() override;

		bool bAreFilesLoaded = false;

		TArray<TSharedPtr<FAudioBusAssetDashboardEntry>> AudioBusDataViewEntries;
	};
} // namespace UE::Audio::Insights
