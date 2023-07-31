// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamObjectWithInputFactory.h"

#include "AssetThumbnail.h"
#include "Editor.h"
#include "PropertyCustomizationHelpers.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "VCamObjectFactories"

bool UVCamObjectWithInputFactory::ConfigureProperties()
{
	class FVCamWidgetFactoryUI : public TSharedFromThis<FVCamWidgetFactoryUI>
	{
	public:
		FReply OnCreate()
		{
			if (PickerWindow.IsValid())
			{
				PickerWindow->RequestDestroyWindow();
			}
			bWasCanceled= false;
			return FReply::Handled();
		}

		bool WasCanceled() const
		{
			return bWasCanceled;
		}

		void OpenMappingSelector(UVCamObjectWithInputFactory* Factory)
		{
			AssetThumbnailPool = MakeShared<FAssetThumbnailPool>(1);
			PickerWindow = SNew(SWindow)
				.Title(LOCTEXT("PickerTitle", "Select Input Mapping Context"))
				.ClientSize(FVector2D(450, 150))
				.SupportsMinimize(false)
				.SupportsMaximize(false)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Menu.Background"))
					.Padding(10)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DialogText", "Your class needs an Input Mapping Context if you want to accept hardware input. You can add or change this selection later in the Class Defaults."))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(FMargin(0, 0, 20, 0))
							[
								SNew(STextBlock)
								.Text(LOCTEXT("InputMappingContext", "Input Mapping Context"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.f)
							.VAlign(VAlign_Center)
							[
								SNew(SObjectPropertyEntryBox)
								.DisplayThumbnail(true)
								.ThumbnailPool(AssetThumbnailPool)
								.AllowClear(true)
								.DisplayUseSelected(false)
								.DisplayBrowse(false)
								.AllowedClass(UInputMappingContext::StaticClass())
								.ObjectPath_Lambda([Factory]() -> FString
								{
									return (Factory && Factory->InputMappingContext) ? Factory->InputMappingContext->GetPathName() : TEXT("None");
								})
								.OnObjectChanged_Lambda([Factory](const FAssetData& AssetData) -> void
								{
									if (Factory)
									{
										Factory->InputMappingContext = Cast<UInputMappingContext>(AssetData.GetAsset());
									}
								})
							]
						]
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Bottom)
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(5,0))
							[
								SNew(SButton)
								.Text(LOCTEXT("OK", "OK"))
								.ToolTipText(LOCTEXT("OkTooltip", "Continues creation using the specified Input Mapping Context"))
								.OnClicked(this, &FVCamWidgetFactoryUI::OnCreate)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(5,0))
							[
								SNew(SButton)
								.Text(LOCTEXT("Skip", "Skip"))
								.ToolTipText(LOCTEXT("SkipTooltip", "Continues creation without assigning an Input Mapping Context"))
								.OnClicked_Lambda([Factory, this]() -> FReply
								{
									// If we skipped then create the widget but with no IMC set
									if (Factory)
									{
										Factory->InputMappingContext = nullptr;
									}
									return OnCreate();
								})
							]
						]
					]
				];
			if (GEditor)
			{
				GEditor->EditorAddModalWindow(PickerWindow.ToSharedRef());
			}
			PickerWindow.Reset();
		}

	private:
		TSharedPtr<SWindow> PickerWindow;
		TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
		bool bWasCanceled = true;
	};

	const TSharedRef<FVCamWidgetFactoryUI> InputMappingSelector = MakeShared<FVCamWidgetFactoryUI>();
	InputMappingSelector->OpenMappingSelector(this);
	
	return !InputMappingSelector->WasCanceled();
}

#undef LOCTEXT_NAMESPACE