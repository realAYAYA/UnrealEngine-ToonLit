// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelDetails.h"
#include "SNiagaraNamePropertySelector.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelDefinitions.h"

void FNiagaraDataChannelReferenceDetailsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ChannelHandle = PropertyHandle->GetChildHandle(TEXT("ChannelName"));

	TArray<TSharedPtr<FName>> DataChannelList;
	const TArray<UNiagaraDataChannelDefinitions*>& DataChannelDefs = UNiagaraDataChannelDefinitions::GetDataChannelDefinitions(false, true);
	for (const UNiagaraDataChannelDefinitions* Def : DataChannelDefs)
	{
		for (TObjectPtr<UNiagaraDataChannel> Channel : Def->DataChannels)
		{
			if(Channel)
			{
				DataChannelList.Add(MakeShared<FName>(Channel->GetChannelName()));
			}
		}
	}

	// Build Widget
	HeaderRow
		.NameContent()
		[
			ChannelHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(200.0f)
		[
			SNew(SNiagaraNamePropertySelector, ChannelHandle.ToSharedRef(), DataChannelList)
		];	
}

void FNiagaraDataChannelReferenceDetailsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}
