// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConnectionRemapUtilsImpl.h"

#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "UI/VCamConnectionStructs.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::VCamCoreEditor::Private
{
	FConnectionRemapUtilsImpl::FConnectionRemapUtilsImpl(TSharedRef<IDetailLayoutBuilder> Builder)
		: Builder(MoveTemp(Builder))
	{}

	void FConnectionRemapUtilsImpl::AddConnection(FAddConnectionArgs Args)
	{
		TSharedPtr<IDetailLayoutBuilder> BuilderPin =  Builder.Pin();
		if (!BuilderPin)
		{
			return;
		}
		
		TSharedPtr<TStructOnScope<FConnectionContainerDummy>> StructData;
		if (const TSharedPtr<TStructOnScope<FConnectionContainerDummy>>* ExistingStructData = AddedConnections.Find(Args.ConnectionName))
		{
			StructData = *ExistingStructData;
		}
		else
		{
			FConnectionContainerDummy DummyContainer { Args.ConnectionData };
			// Overwrite bManuallyConfigureConnection because otherwise its EditCondition will hide the property!
			DummyContainer.Connection.bManuallyConfigureConnection = true;
			TStructOnScope<FConnectionContainerDummy> StructOnScope(DummyContainer);
			StructData = MakeShared<TStructOnScope<FConnectionContainerDummy>>(MoveTemp(StructOnScope));
			AddedConnections.Add(Args.ConnectionName, StructData);
		}

		const FName TargetSettingsPropertyName = GET_MEMBER_NAME_CHECKED(FConnectionContainerDummy, Connection);
		const TSharedPtr<IPropertyHandle> ConnectionHandle = BuilderPin->AddStructurePropertyData(StructData, TargetSettingsPropertyName);
		if (!ensure(ConnectionHandle))
		{
			return;
		}
		const TSharedPtr<IPropertyHandle> TargetConnectionSettings = ConnectionHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVCamConnection, ConnectionTargetSettings));
		if (!ensure(TargetConnectionSettings))
		{
			return;
		}
		
		TargetConnectionSettings->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateLambda([TargetConnectionSettings, Callback = MoveTemp(Args.OnTargetSettingsChangedDelegate)]()
		{
			void* Data;
			if (ensure(TargetConnectionSettings->GetValueData(Data) == FPropertyAccess::Success))
			{
				Callback.Execute(*(FVCamConnectionTargetSettings*)Data);
			}
		}));

		Args.DetailGroup.AddPropertyRow(TargetConnectionSettings.ToSharedRef())
			.DisplayName(FText::FromName(Args.ConnectionName)) 
			.CustomWidget(true)
			.NameContent()
			[
				SNew(STextBlock)
				.Font(Args.Font)
				.Text(FText::FromName(Args.ConnectionName))
			]
			.ValueContent()
			[
				TargetConnectionSettings->CreatePropertyValueWidget()
			];
	}

	FSlateFontInfo FConnectionRemapUtilsImpl::GetRegularFont() const
	{
		if (TSharedPtr<IDetailLayoutBuilder> BuilderPin = Builder.Pin())
		{
			return BuilderPin->GetDetailFont();
		}
		return FSlateFontInfo{};
	}

	void FConnectionRemapUtilsImpl::ForceRefreshProperties() const
	{
		if (TSharedPtr<IDetailLayoutBuilder> BuilderPin = Builder.Pin())
		{
			BuilderPin->ForceRefreshDetails();
		}
	}
}

