// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMDomain.h"
#include "DMPrivate.h"
#include "Engine/EngineTypes.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::DynamicMaterialEditor::Private
{
	static const TArray<EMaterialDomain> SupportedDomains =
	{
		EMaterialDomain::MD_Surface,
		EMaterialDomain::MD_PostProcess
	};

	const TArray<FName>& SupportedDomainNames()
	{
		static TArray<FName> OutNames;

		if (OutNames.IsEmpty())
		{
			const UEnum* const Enum = StaticEnum<EMaterialDomain>();

			if (ensure(IsValid(Enum)))
			{
				for (const EMaterialDomain Domain : SupportedDomains)
				{
					const FName Name = Enum->GetNameByValue(Domain);
					OutNames.Add(Name);
				}
			}
		}

		return OutNames;
	}
}

void SDMDomain::Construct(const FArguments& InArgs)
{
	SelectedItem = InArgs._SelectedItem;
	OnSelectedItemChanged = InArgs._OnSelectedItemChanged;

	DomainEnum = StaticEnum<EMaterialDomain>();

	const EMaterialDomain InitialDomain = InArgs._SelectedItem.Get();
	const FName InitialDomainName = DomainEnum->GetNameByValue(InitialDomain);

	ChildSlot
	[
		SNew(SComboBox<FName>)
			.InitiallySelectedItem(InitialDomainName)
			.OptionsSource(&UE::DynamicMaterialEditor::Private::SupportedDomainNames())
			.OnGenerateWidget(this, &SDMDomain::OnGenerateWidget)
			.OnSelectionChanged(this, &SDMDomain::OnSelectionChanged)
			[
				SNew(STextBlock)
				.Text(this, &SDMDomain::GetSelectedItemText)
			]
	];
}

TSharedRef<SWidget> SDMDomain::OnGenerateWidget(const FName InItem)
{
	if (!DomainEnum->IsValidEnumName(InItem))
	{
		return SNullWidget::NullWidget;
	}

	return
		SNew(STextBlock)
		.Text(GetDomainDisplayName(InItem));
}

void SDMDomain::OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType)
{
	const EMaterialDomain Domain = static_cast<EMaterialDomain>(DomainEnum->GetValueByName(InNewItem));
	OnSelectedItemChanged.ExecuteIfBound(Domain);
}

FText SDMDomain::GetSelectedItemText() const
{
	const FName DomainName = DomainEnum->GetNameByValue(SelectedItem.Get());

	if (DomainName == NAME_None)
	{
		return FText::GetEmpty();
	}

	return GetDomainDisplayName(DomainName);
}

FText SDMDomain::GetDomainDisplayName(const FName InDomainName) const
{
	const int64 DomainValue = DomainEnum->GetValueByName(InDomainName);

	if (DomainValue == INDEX_NONE)
	{
		return FText::GetEmpty();
	}

	return DomainEnum->GetDisplayNameTextByValue(DomainValue);
}
