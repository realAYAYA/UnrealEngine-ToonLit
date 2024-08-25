// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ClientName/SClientName.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SClientName"

namespace UE::ConcertClientSharedSlate
{
	void SClientName::Construct(const FArguments& InArgs)
	{
		ClientInfoAttribute = InArgs._ClientInfo;
		DisplayAsLocalClientAttribute = InArgs._DisplayAsLocalClient;
		check(ClientInfoAttribute.IsSet() || ClientInfoAttribute.IsBound());
		check(DisplayAsLocalClientAttribute.IsSet() || DisplayAsLocalClientAttribute.IsBound());
		
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			[
				SNew(STextBlock)
				.Font(InArgs._Font)
				.Text(TAttribute<FText>::CreateSP(this, &SClientName::GetClientDisplayName))
				.HighlightText(InArgs._HighlightText)
			]
		];
	}

	FText SClientName::GetDisplayText(const FConcertClientInfo& Info, bool bDisplayAsLocalClient)
	{
		if (bDisplayAsLocalClient)
		{
			return FText::Format(
				LOCTEXT("ClientDisplayNameFmt", "{0} (me)"),
				FText::FromString(Info.DisplayName)
				);
		}
		
		return FText::FromString(Info.DisplayName);
	}

	FText SClientName::GetClientDisplayName() const
	{
		const FConcertClientInfo* ClientInfo = ClientInfoAttribute.Get();
		check(ClientInfo);
		return GetDisplayText(*ClientInfo, DisplayAsLocalClientAttribute.Get());
	}
}

#undef LOCTEXT_NAMESPACE