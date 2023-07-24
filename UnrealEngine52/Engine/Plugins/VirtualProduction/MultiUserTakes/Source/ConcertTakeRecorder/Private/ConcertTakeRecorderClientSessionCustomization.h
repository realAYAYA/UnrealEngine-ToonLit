// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "IDetailCustomization.h"


enum class EConcertClientStatus : uint8;
struct FConcertClientRecordSetting;
template <typename ItemType> class SListView;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertRecordSettingChanged, const FConcertClientRecordSetting&);

using SListViewClientRecordSetting = SListView<TSharedPtr<FConcertClientRecordSetting>>;

class FConcertTakeRecorderClientSessionCustomization : public IDetailCustomization
{
public:
	void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	void UpdateClientSettings(EConcertClientStatus Status, const FConcertClientRecordSetting& RecordSetting);
	void PopulateClientList();

	FOnConcertRecordSettingChanged& OnRecordSettingChanged()
	{
		return OnRecordSettingDelegate;
	}

private:
	void RecordSettingChange(const FConcertClientRecordSetting &RecordSetting);

	FOnConcertRecordSettingChanged	OnRecordSettingDelegate;

	TArray<TSharedPtr<FConcertClientRecordSetting>> Clients;
	TWeakPtr<SListViewClientRecordSetting>		    ClientsListViewWeak;
};
