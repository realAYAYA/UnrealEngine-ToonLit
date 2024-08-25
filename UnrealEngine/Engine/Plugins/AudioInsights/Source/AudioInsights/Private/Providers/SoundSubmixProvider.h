// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"
#include "UObject/NameTypes.h"
#include "Views/SoundSubmixAssetDashboardEntry.h"

namespace UE::Audio::Insights
{
	class FSoundSubmixProvider : public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FSoundSubmixAssetDashboardEntry>>, public TSharedFromThis<FSoundSubmixProvider>
	{
	public:
		FSoundSubmixProvider();
		virtual ~FSoundSubmixProvider();

		static FName GetName_Static();

		virtual UE::Trace::IAnalyzer* ConstructAnalyzer() override;
		
		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmixAssetAdded, const TWeakObjectPtr<UObject> /*Asset*/);
		inline static FOnSubmixAssetAdded OnSubmixAssetAdded;

		DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubmixAssetRemoved, const TWeakObjectPtr<UObject> /*Asset*/);
		inline static FOnSubmixAssetRemoved OnSubmixAssetRemoved;

		DECLARE_MULTICAST_DELEGATE(FOnSubmixAssetListUpdated);
		inline static FOnSubmixAssetListUpdated OnSubmixAssetListUpdated;

	private:
		void OnAssetAdded(const FAssetData& InAssetData);
		void OnAssetRemoved(const FAssetData& InAssetData);
		void OnFilesLoaded();
		void OnActiveAudioDeviceChanged();

		void AddSubmixAsset(const FAssetData& InAssetData);
		void RemoveSubmixAsset(const FAssetData& InAssetData);

		void UpdateSubmixAssetNames();

		virtual bool ProcessMessages() override;

		bool bAreFilesLoaded = false;

		TArray<TSharedPtr<FSoundSubmixAssetDashboardEntry>> SubmixDataViewEntries;
	};
} // namespace UE::Audio::Insights
