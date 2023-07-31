// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessages.h"
#include "IConcertSession.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "ConcertTakeRecorderMessages.h"
#include "IPropertyTypeCustomization.h"

#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"

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
