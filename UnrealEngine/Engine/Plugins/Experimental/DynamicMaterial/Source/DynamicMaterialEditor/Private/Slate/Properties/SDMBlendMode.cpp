// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMBlendMode.h"
#include "DMPrivate.h"
#include "Engine/EngineTypes.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::DynamicMaterialEditor::Private
{
	static const TArray<EBlendMode> SupportedBlendModes =
	{
		EBlendMode::BLEND_Opaque,
		EBlendMode::BLEND_Masked,
		EBlendMode::BLEND_Translucent,
		EBlendMode::BLEND_Additive,
		EBlendMode::BLEND_Modulate
	};

	const TArray<FName>& SupportedBlendModeNames()
	{
		static TArray<FName> OutNames;

		if (OutNames.IsEmpty())
		{
			const UEnum* const Enum = StaticEnum<EBlendMode>();

			if (ensure(IsValid(Enum)))
			{
				for (const EBlendMode BlendMode : SupportedBlendModes)
				{
					const FName Name = Enum->GetNameByValue(BlendMode);
					OutNames.Add(Name);
				}
			}
		}

		return OutNames;
	}
}

void SDMBlendMode::Construct(const FArguments& InArgs)
{
	SelectedItem = InArgs._SelectedItem;
	OnSelectedItemChanged = InArgs._OnSelectedItemChanged;

	BlendModeEnum = StaticEnum<EBlendMode>();

	const EBlendMode InitialBlendMode = InArgs._SelectedItem.Get();
	const FName InitialBlendModeName = BlendModeEnum->GetNameByValue(InitialBlendMode);

	ChildSlot
	[
		SNew(SComboBox<FName>)
		.InitiallySelectedItem(InitialBlendModeName)
		.OptionsSource(&UE::DynamicMaterialEditor::Private::SupportedBlendModeNames())
		.OnGenerateWidget(this, &SDMBlendMode::OnGenerateWidget)
		.OnSelectionChanged(this, &SDMBlendMode::OnSelectionChanged)
		[
			SNew(STextBlock)
			.Text(this, &SDMBlendMode::GetSelectedItemText)
		]
	];
}

TSharedRef<SWidget> SDMBlendMode::OnGenerateWidget(const FName InItem)
{
	if (!BlendModeEnum->IsValidEnumName(InItem))
	{
		return SNullWidget::NullWidget;
	}

	return 
		SNew(STextBlock)
		.Text(GetBlendModeDisplayName(InItem));
}

void SDMBlendMode::OnSelectionChanged(const FName InNewItem, const ESelectInfo::Type InSelectInfoType)
{
	const EBlendMode BlendMode = static_cast<EBlendMode>(BlendModeEnum->GetValueByName(InNewItem));
	OnSelectedItemChanged.ExecuteIfBound(BlendMode);
}

FText SDMBlendMode::GetSelectedItemText() const
{
	const FName BlendModeName = BlendModeEnum->GetNameByValue(SelectedItem.Get());

	if (BlendModeName == NAME_None)
	{
		return FText::GetEmpty();
	}

	return GetBlendModeDisplayName(BlendModeName);
}

FText SDMBlendMode::GetBlendModeDisplayName(const FName InBlendModeName) const
{
	const int64 BlendModeValue = BlendModeEnum->GetValueByName(InBlendModeName);

	if (BlendModeValue == INDEX_NONE)
	{
		return FText::GetEmpty();
	}

	return BlendModeEnum->GetDisplayNameTextByValue(BlendModeValue);
}
