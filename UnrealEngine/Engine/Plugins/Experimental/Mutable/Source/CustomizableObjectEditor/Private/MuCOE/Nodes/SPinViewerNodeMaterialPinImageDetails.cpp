// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/SPinViewerNodeMaterialPinImageDetails.h"

#include "EdGraph/EdGraphPin.h"
#include "Framework/Views/TableViewMetadata.h"
#include "ISinglePropertyView.h"
#include "Modules/ModuleManager.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/SMutableTextComboBox.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SSpinBox.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


FText UVLayoutModeToText(const EUVLayoutMode Mode)
{
	switch(Mode)
	{
	case EUVLayoutMode::Default:
		return LOCTEXT("EUVLayoutMode::Default","From Material");

	case EUVLayoutMode::Ignore:
		return LOCTEXT("EUVLayoutMode::Ignore","Ignore");

	case EUVLayoutMode::Index:
		return LOCTEXT("EUVLayoutMode::Index","Index");

	default:
		check(false); // Case not contemplated.
		return FText();	
	}
}


EUVLayoutMode UVLayoutToUVLayoutMode(const int32 UVLayout)
{
	if (UVLayout >= 0)
	{
		return EUVLayoutMode::Index;
	}
	else
	{
		switch (UVLayout)
		{
		case UCustomizableObjectNodeMaterialPinDataImage::UV_LAYOUT_DEFAULT:
			return EUVLayoutMode::Default;

		case UCustomizableObjectNodeMaterialPinDataImage::UV_LAYOUT_IGNORE:
			return EUVLayoutMode::Ignore;

		default:
			check(false); // Case not contemplated.
			return EUVLayoutMode::Index; // Fake return.
		}
	}
}


int32 UVLayoutModeToUVLayout(const EUVLayoutMode Mode, const int32 UVLayout)
{
	switch (Mode)
	{
	case EUVLayoutMode::Default:
		return UCustomizableObjectNodeMaterialPinDataImage::UV_LAYOUT_DEFAULT;

	case EUVLayoutMode::Ignore:
		return UCustomizableObjectNodeMaterialPinDataImage::UV_LAYOUT_IGNORE;

	case EUVLayoutMode::Index:
		return UVLayout;

	default:
		check(false); // Case not contemplated.
		return 0;
	}
}


void SPinViewerNodeMaterialPinImageDetails::Construct(const FArguments& InArgs)
{
	SPinViewerPinDetails::Construct(SPinViewerPinDetails::FArguments());

	Pin = InArgs._Pin;
	PinData = InArgs._PinData;
	
	{
		PinModeOptions.Add(MakeShared<EPinMode>(EPinMode::Default));
		PinModeOptions.Add(MakeShared<EPinMode>(EPinMode::Mutable));
		PinModeOptions.Add(MakeShared<EPinMode>(EPinMode::Passthrough));

		const TSharedPtr<EPinMode> SelectedItem = *PinModeOptions.FindByPredicate([this](const TSharedPtr<EPinMode>& Element)
		{
			return *Element == PinData->GetPinMode();
		});

		bool bIsLinked = FollowInputPin(*Pin) != nullptr;
		
		TSharedPtr<SWidget> Widget;
		SAssignNew(Widget, SMutableTextComboBox<TSharedPtr<EPinMode>>)
			.Options(&PinModeOptions)
			.InitiallySelectedItem(SelectedItem)
			.OnSelectionChanged(this, &SPinViewerNodeMaterialPinImageDetails::PinModeOnSelectionChanged)
			.Translate_Lambda([this](const TSharedPtr<EPinMode> Option)
			{
				return EPinModeToText(*Option);
			})
			.IsEnabled(!bIsLinked)
			.ToolTipText_Lambda([bIsLinked]()
			{
				if (bIsLinked)
				{
					return LOCTEXT("SPinViewer_PinModeComboboxTooltip", "Value being overridden due to pin linked to a node.");
				}

				return LOCTEXT("SPinViewer_PinModeComboboxTooltipDisabled", "");
			});

		const FText Tooltip = LOCTEXT("PinModeTooltip", "Texture Parameter mode.");
		AddRow(LOCTEXT("PinMode", "Mode"), Widget.ToSharedRef(), &Tooltip);
	}

	const EUVLayoutMode UVLayoutMode = UVLayoutToUVLayoutMode(PinData->UVLayout);
	
	{
		UVLayoutModeOptions.Add(MakeShared<EUVLayoutMode>(EUVLayoutMode::Default));
		UVLayoutModeOptions.Add(MakeShared<EUVLayoutMode>(EUVLayoutMode::Ignore));
		UVLayoutModeOptions.Add(MakeShared<EUVLayoutMode>(EUVLayoutMode::Index));

		const TSharedPtr<EUVLayoutMode> SelectedItem = *UVLayoutModeOptions.FindByPredicate([&](const TSharedPtr<EUVLayoutMode>& Element)
		{
			return *Element == UVLayoutMode;
		});
		
		TSharedPtr<SWidget> Widget;
		SAssignNew(Widget, SMutableTextComboBox<TSharedPtr<EUVLayoutMode>>)
			.Options(&UVLayoutModeOptions)
			.InitiallySelectedItem(SelectedItem)
			.OnSelectionChanged(this, &SPinViewerNodeMaterialPinImageDetails::UVLayoutModeOnSelectionChanged)
			.Translate_Lambda([this](const TSharedPtr<EUVLayoutMode> Option)
			{
				return UVLayoutModeToText(*Option);
			});

		const FText Tooltip = LOCTEXT("UVLayoutModeTooltip", "Specify how UV Layuout is defined.");
		AddRow(LOCTEXT("UVLayoutMode", "UV Layout Mode"), Widget.ToSharedRef(), &Tooltip);
	}
	
	{
		SAssignNew(UVLayoutSSpinBox, SSpinBox<int32>)
			.MinValue(0)
			.OnValueChanged(this, &SPinViewerNodeMaterialPinImageDetails::UVLayoutOnValueChanged)
			.Value_Lambda([this]
			{
				return FMath::Max(0, PinData->UVLayout);
			});

		UVLayout = AddRow(LOCTEXT("UVLayout", "UV Layout Index"), UVLayoutSSpinBox.ToSharedRef());
		UVLayoutVisibility(UVLayoutMode, UVLayout);
	}

	{
		FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FSinglePropertyParams SingleDetails;
		SingleDetails.NamePlacement = EPropertyNamePlacement::Hidden;

		const TSharedPtr<ISinglePropertyView> Widget = PropPlugin.CreateSingleProperty(PinData,
			GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeMaterialPinDataImage, ReferenceTexture), SingleDetails);

		AddRow(LOCTEXT("ReferenceTexture", "Reference Texture"), Widget.ToSharedRef());
	}
}


void SPinViewerNodeMaterialPinImageDetails::PinModeOnSelectionChanged(const TSharedPtr<EPinMode> PinMode, ESelectInfo::Type Arg)
{
	FScopedTransaction Transaction(LOCTEXT("ChangedPinModeTransaction", "Changed Pin Mode"));
	PinData->Modify();
	
	PinData->SetPinMode(*PinMode);
}


void SPinViewerNodeMaterialPinImageDetails::UVLayoutModeOnSelectionChanged(const TSharedPtr<EUVLayoutMode> LayoutMode, ESelectInfo::Type Arg)
{
	FScopedTransaction Transaction(LOCTEXT("ChangedUVLayoutModeTransaction", "Changed UV Layout Mode"));
	PinData->Modify();

	PinData->UVLayout = UVLayoutModeToUVLayout(*LayoutMode, UVLayoutSSpinBox->GetValue());
	UVLayoutVisibility(*LayoutMode, UVLayout);
}


void SPinViewerNodeMaterialPinImageDetails::UVLayoutVisibility(const EUVLayoutMode LayoutMode, const TSharedPtr<SWidget> Widget)
{
	switch (LayoutMode)
	{
	case EUVLayoutMode::Default:
	case EUVLayoutMode::Ignore:
		Widget->SetVisibility(EVisibility::Collapsed);
		break;
	
	case EUVLayoutMode::Index:
		Widget->SetVisibility(EVisibility::Visible);
		break;

	default:
		check(false); // Case not contemplated.
	}
}


void SPinViewerNodeMaterialPinImageDetails::UVLayoutOnValueChanged(int32 Value)
{
	FScopedTransaction Transaction(LOCTEXT("ChangedUVLayoutTransaction", "Changed UV Layout Index"));
	PinData->Modify();
	
	PinData->UVLayout = Value;
}

#undef LOCTEXT_NAMESPACE
