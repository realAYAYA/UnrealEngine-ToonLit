// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeUIDetails.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeSettings.h"
#include "Layout/Visibility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/MessageDialog.h"
#include "Misc/Optional.h"
#include "PropertyHandle.h"
#include "Styling/SlateTypes.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/ScopedSlowTask.h"

class UObject;

#define LOCTEXT_NAMESPACE "FLandscapeUIDetails"

FLandscapeUIDetails::FLandscapeUIDetails()
{
}

FLandscapeUIDetails::~FLandscapeUIDetails()
{
}

TSharedRef<IDetailCustomization> FLandscapeUIDetails::MakeInstance()
{
	return MakeShareable( new FLandscapeUIDetails);
}

void FLandscapeUIDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	if (EditingObjects.Num() == 1)
	{
		TWeakObjectPtr<ALandscape> Landscape(Cast<ALandscape>(EditingObjects[0].Get()));
		if (!Landscape.IsValid())
		{
			return;
		}

		TSharedRef<IPropertyHandle> CanHaveLayersPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ALandscape, bCanHaveLayersContent));
		DetailBuilder.HideProperty(CanHaveLayersPropertyHandle);
		const FText DisplayAndFilterText(LOCTEXT("LandscapeToggleLayerName", "Enable Edit Layers"));
		const FText ToolTipText(LOCTEXT("LandscapeToggleLayerToolTip", "Toggle whether or not to support edit layers on this Landscape. Toggling this will clear the undo stack."));
		DetailBuilder.AddCustomRowToCategory(CanHaveLayersPropertyHandle, DisplayAndFilterText)
		.RowTag(TEXT("EnableEditLayers"))
		.NameContent()
		[
			CanHaveLayersPropertyHandle->CreatePropertyNameWidget(DisplayAndFilterText, ToolTipText)
		]
		.Visibility(MakeAttributeLambda([]()
		{
			const ULandscapeSettings* Settings = GetDefault<ULandscapeSettings>();
			return Settings->InRestrictiveMode() ? EVisibility::Hidden : EVisibility::Visible;
		}))
		.ValueContent()
		[
			SNew(SCheckBox)
			.ToolTipText(ToolTipText)
			.Type(ESlateCheckBoxType::CheckBox)
			.IsChecked_Lambda([=]()
			{
				return Landscape.IsValid() && Landscape->CanHaveLayersContent() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, Landscape](ECheckBoxState NewState)
			{
				bool bChecked = (NewState == ECheckBoxState::Checked);
				if (Landscape.IsValid() && (Landscape->CanHaveLayersContent() != bChecked))
				{
					ToggleCanHaveLayersContent(Landscape.Get());
				}
			})
		];
	}
}

void FLandscapeUIDetails::ToggleCanHaveLayersContent(ALandscape* Landscape)
{
	bool bToggled = false;

	if (Landscape->bCanHaveLayersContent)
	{
		bool bHasHiddenLayers = false;
		for (int32 i = 0; i < Landscape->GetLayerCount(); ++i)
		{
			const FLandscapeLayer* Layer = Landscape->GetLayer(i);
			check(Layer != nullptr);

			if (!Layer->bVisible)
			{
				bHasHiddenLayers = true;
				break;
			}
		}
				
		FText Reason;

		if (bHasHiddenLayers)
		{
			Reason = LOCTEXT("LandscapeDisableLayers_HiddenLayers", "Are you sure you want to disable the edit layers on this Landscape?\n\nDoing so, will result in losing the data stored for each edit layer, but the current visual output will be kept. Be aware that some edit layers are currently hidden, continuing will result in their data being lost. Undo/redo buffer will also be cleared.");
		}
		else
		{
			Reason = LOCTEXT("LandscapeDisableLayers", "Are you sure you want to disable the edit layers on this Landscape?\n\nDoing so, will result in losing the data stored for each edit layers, but the current visual output will be kept. Undo/redo buffer will also be cleared.");
		}

		bToggled = FMessageDialog::Open(EAppMsgType::YesNo, Reason) == EAppReturnType::Yes;
	}
	else
	{
		
		bToggled = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("LandscapeEnableLayers", "Are you sure you want to enable edit layers on this landscape? Doing so will clear the undo/redo buffer.")) == EAppReturnType::Yes;
	}

	if (bToggled)
	{
		Landscape->ToggleCanHaveLayersContent();
		if (GEditor)
		{
			GEditor->ResetTransaction(LOCTEXT("ToggleLanscapeLayers", "Toggling Landscape Edit Layers"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
