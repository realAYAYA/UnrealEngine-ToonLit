// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorElementControllerView.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxController.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorElementControllerView"

namespace UE::DMX::Private
{
	namespace DMXControlConsoleEditorElementControllerView
	{
		namespace Private
		{
			constexpr float CollapsedViewModeHeight = 230.f;
			constexpr float ExpandedViewModeHeight = 310.f;
		}
	}

	void SDMXControlConsoleEditorElementControllerView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct element controller widget correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InElementControllerModel.IsValid(), TEXT("Invalid element controller model, cannot create element controller widget correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		ElementControllerModel = InElementControllerModel;

		ChildSlot
			[
				SNew(SBox)
				.WidthOverride(80.f)
				.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::GetElementControllerHeightByViewMode))
				.Padding(InArgs._Padding)
				[
					SNew(SBorder)
					.BorderImage(this, &SDMXControlConsoleEditorElementControllerView::GetBorderImage)
					.Padding(0.f, 4.f)
					[
						SNew(SVerticalBox)

						// Top section
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.Padding(0.f, 8.f)
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							// Element Controller Name
							+ SHorizontalBox::Slot()
							.MaxWidth(50.f)
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(this, &SDMXControlConsoleEditorElementControllerView::GetElementControllerNameText)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							]
						]

						// Middle section
						+ SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						[
							SNew(SVerticalBox)

							// Element Controller Max Value
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.Padding(6.f, 2.f, 6.f, 4.f)
							.AutoHeight()
							[
								SNew(SEditableTextBox)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.IsReadOnly_Lambda([this]() { return ElementControllerModel.IsValid() && ElementControllerModel->IsLocked(); })
								.Justification(ETextJustify::Center)
								.MinDesiredWidth(20.f)
								.OnTextCommitted(this, &SDMXControlConsoleEditorElementControllerView::OnMaxValueTextCommitted)
								.Text(this, &SDMXControlConsoleEditorElementControllerView::GetMaxValueAsText)
								.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::GetExpandedViewModeVisibility))
							]

							// Element Controller Spin Box
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.MaxWidth(40.f)
								[
									SNew(SOverlay)
									// Spin Box layer
									+ SOverlay::Slot()
									[
										SAssignNew(SpinBoxControllerWidget, SDMXControlConsoleEditorSpinBoxController, ElementControllerModel, EditorModel.Get())
									]

									// Lock Button layer
									+ SOverlay::Slot()
									[
										SNew(SVerticalBox)
										+ SVerticalBox::Slot()
										[
											SNew(SBox)
										]

										+ SVerticalBox::Slot()
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.Padding(0.f, 4.f)
										.AutoHeight()
										[
											GenerateLockButtonWidget()
										]
									]
								]
							]

							// Element Controller Value
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.Padding(6.f, 4.f)
							.AutoHeight()
							[
								SNew(SEditableTextBox)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.FocusedForegroundColor(FLinearColor::White)
								.ForegroundColor(FLinearColor::FromSRGBColor(FColor::FromHex("0088f7")))
								.IsReadOnly_Lambda([this]() { return ElementControllerModel.IsValid() && ElementControllerModel->IsLocked(); })
								.Justification(ETextJustify::Center)
								.OnTextCommitted(this, &SDMXControlConsoleEditorElementControllerView::OnValueTextCommitted)
								.MinDesiredWidth(20.f)
								.Text(this, &SDMXControlConsoleEditorElementControllerView::GetValueAsText)
								.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::GetExpandedViewModeVisibility))
							]

							// Element Controller Min Value
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.Padding(6.f, 4.f)
							.AutoHeight()
							[
								SNew(SEditableTextBox)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.IsReadOnly_Lambda([this]() { return ElementControllerModel.IsValid() && ElementControllerModel->IsLocked(); })
								.Justification(ETextJustify::Center)
								.MinDesiredWidth(20.f)
								.OnTextCommitted(this, &SDMXControlConsoleEditorElementControllerView::OnMinValueTextCommitted)
								.Text(this, &SDMXControlConsoleEditorElementControllerView::GetMinValueAsText)
								.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::GetExpandedViewModeVisibility))
							]

							// Mute CheckBox section
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.Padding(6.f, 10.f)
							.AutoHeight()
							[
								SNew(SCheckBox)
								.IsChecked(this, &SDMXControlConsoleEditorElementControllerView::IsEnableChecked)
								.OnCheckStateChanged(this, &SDMXControlConsoleEditorElementControllerView::OnEnableToggleChanged)
							]
						]
					]
				]
			];
	}

	UDMXControlConsoleElementController* SDMXControlConsoleEditorElementControllerView::GetElementController() const
	{
		return ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
	}

	FReply SDMXControlConsoleEditorElementControllerView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot handle selection correctly.")))
		{
			return FReply::Unhandled();
		}

		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			UDMXControlConsoleElementController* ElementController = GetElementController();
			if (!ElementController)
			{
				return FReply::Unhandled();
			}

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			if (MouseEvent.IsLeftShiftDown())
			{
				SelectionHandler->Multiselect(ElementController);
			}
			else if (MouseEvent.IsControlDown())
			{
				if (IsSelected())
				{
					SelectionHandler->RemoveFromSelection(ElementController);
				}
				else
				{
					SelectionHandler->AddToSelection(ElementController);
				}
			}
			else
			{
				if (!IsSelected() || !SpinBoxControllerWidget->IsHovered())
				{
					constexpr bool bNotifySelectionChange = false;
					SelectionHandler->ClearSelection(bNotifySelectionChange);
					SelectionHandler->AddToSelection(ElementController);
				}
			}

			return FReply::Handled();
		}

		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ElementControllerModel.IsValid())
		{
			const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateElementControllerContextMenuWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorElementControllerView::GenerateLockButtonWidget()
	{
		const TSharedRef<SWidget> MuteButtonWidget =
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.MinDesiredWidth(12.f)
			.MinDesiredHeight(12.f)
			.Padding(2.f)
			[
				SAssignNew(LockButton, SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.ClickMethod(EButtonClickMethod::MouseDown)
				.OnClicked(this, &SDMXControlConsoleEditorElementControllerView::OnLockClicked)
				.ToolTipText(LOCTEXT("FaderLockButtonToolTipText", "Locked"))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::GetLockButtonVisibility))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Lock"))
					.ColorAndOpacity(this, &SDMXControlConsoleEditorElementControllerView::GetLockButtonColor)
				]
			];

		return MuteButtonWidget;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorElementControllerView::GenerateElementControllerContextMenuWidget()
	{
		constexpr bool bShouldCloseWindowAfterClosing = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("Options", LOCTEXT("FaderOptionsCategory", "Options"));
		{
			constexpr bool bEnableController = true;
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("EnableLabel", "Enable"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnEnableElementController, bEnableController)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("DisableLabel", "Disable"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnEnableElementController, !bEnableController)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("RemoveLabel", "Remove"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnRemoveElementController),
					FCanExecuteAction::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && ElementControllerModel->HasOnlyRawFaders();
						})
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Controls", LOCTEXT("FaderControlsCategory", "Controls"));
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ResetToDefaultLabel", "Reset to Default"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.ResetToDefault"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnResetElementController),
					FCanExecuteAction::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && ElementControllerModel->HasSingleElement(); 
						}),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && ElementControllerModel->HasSingleElement(); 
						})
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("LockLabel", "Lock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnLockElementController, true),
					FCanExecuteAction::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && !ElementControllerModel->IsLocked(); 
						}),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && !ElementControllerModel->IsLocked(); 
						})
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("UnlockLabel", "Unlock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorElementControllerView::OnLockElementController, false),
					FCanExecuteAction::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && ElementControllerModel->IsLocked(); 
						}),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() 
						{ 
							return ElementControllerModel.IsValid() && ElementControllerModel->IsLocked(); 
						})
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	bool SDMXControlConsoleEditorElementControllerView::IsSelected() const
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (ElementController && EditorModel.IsValid())
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			return SelectionHandler->IsSelected(ElementController);
		}

		return false;
	}

	FString SDMXControlConsoleEditorElementControllerView::GetElementControllerName() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		return ElementController ? ElementController->GetUserName() : FString();
	}

	FText SDMXControlConsoleEditorElementControllerView::GetElementControllerNameText() const
	{
		const FString ElementControllerName = ElementControllerModel.IsValid() ? ElementControllerModel->GetRelativeControllerName() : FString();
		return FText::FromString(ElementControllerName);
	}

	FText SDMXControlConsoleEditorElementControllerView::GetValueAsText() const
	{
		if (!ElementControllerModel.IsValid())
		{
			return FText::GetEmpty();
		}

		if (!ElementControllerModel->HasUniformValue())
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}

		const UDMXControlConsoleElementController* ElementController = ElementControllerModel->GetElementController();
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ElementController || !ControlConsoleEditorData)
		{
			return FText::GetEmpty();
		}

		float Value = 0.f;
		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte)
		{
			Value = ElementControllerModel->GetRelativeValue();
			if (ElementControllerModel->HasUniformDataType())
			{
				return FText::FromString(FString::FromInt(Value));
			}
		}
		else
		{
			Value = ElementController->GetValue();
		}

		return FText::FromString(FString::SanitizeFloat(Value));
	}

	void SDMXControlConsoleEditorElementControllerView::OnValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ControlConsoleEditorData || !ElementController || NewText.IsEmpty())
		{
			return;
		}

		float NewValue = -1.f;
		if (!LexTryParseString(NewValue, *NewText.ToString()) || NewValue < 0.f)
		{
			return;
		}

		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte &&
			ElementControllerModel->HasUniformDataType())
		{
			// Normalize the input value
			const UDMXControlConsoleFaderBase* FirstFader = ElementControllerModel->GetFirstAvailableFader();
			if (ElementControllerModel->HasUniformDataType() && FirstFader)
			{
				const uint8 NumBytes = static_cast<uint8>(FirstFader->GetDataType()) + 1;
				const float ValueRange = FMath::Pow(2.f, 8.f * NumBytes) - 1;
				NewValue /= ValueRange;
			}
		}

		if (SpinBoxControllerWidget.IsValid())
		{
			SpinBoxControllerWidget->CommitValue(NewValue);
		}
	}

	TOptional<float> SDMXControlConsoleEditorElementControllerView::GetMinValue() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		return ElementController ? ElementController->GetMinValue() : TOptional<float>();
	}

	FText SDMXControlConsoleEditorElementControllerView::GetMinValueAsText() const
	{
		if (!ElementControllerModel.IsValid())
		{
			return FText::GetEmpty();
		}

		if (!ElementControllerModel->HasUniformMinValue())
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}

		const UDMXControlConsoleElementController* ElementController = ElementControllerModel->GetElementController();
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ElementController || !ControlConsoleEditorData)
		{
			return FText::GetEmpty();
		}

		float MinValue = 0.f;
		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte)
		{
			MinValue = ElementControllerModel->GetRelativeMinValue();
			if (ElementControllerModel->HasUniformDataType())
			{
				return FText::FromString(FString::FromInt(MinValue));
			}
		}
		else
		{
			MinValue = ElementController->GetMinValue();
		}

		return FText::FromString(FString::SanitizeFloat(MinValue));
	}

	void SDMXControlConsoleEditorElementControllerView::OnMinValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ControlConsoleEditorData || !ElementController || NewText.IsEmpty())
		{
			return;
		}

		float NewValue = -1.f;
		if (!LexTryParseString(NewValue, *NewText.ToString()) || NewValue < 0.f)
		{
			return;
		}

		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte &&
			ElementControllerModel->HasUniformDataType())
		{
			// Normalize the input value
			const UDMXControlConsoleFaderBase* FirstFader = ElementControllerModel->GetFirstAvailableFader();
			if (ElementControllerModel->HasUniformDataType() && FirstFader)
			{
				const uint8 NumBytes = static_cast<uint8>(FirstFader->GetDataType()) + 1;
				const float ValueRange = FMath::Pow(2.f, 8.f * NumBytes) - 1;
				NewValue /= ValueRange;
			}
		}

		const FScopedTransaction ElementControllerMinValueEditedTransaction(LOCTEXT("ElementControllerMinValueEditedTransaction", "Edit Min Value"));
		// Ensure that each fader in the controller is registered to the transaction
		for (UDMXControlConsoleFaderBase* Fader : ElementController->GetFaders())
		{
			if (Fader)
			{
				Fader->Modify();
			}
		}
		
		ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetMinValuePropertyName()));
		ElementController->SetMinValue(NewValue);
		ElementController->PostEditChange();
	}

	TOptional<float> SDMXControlConsoleEditorElementControllerView::GetMaxValue() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		return ElementController ? ElementController->GetMaxValue() : TOptional<float>();
	}

	FText SDMXControlConsoleEditorElementControllerView::GetMaxValueAsText() const
	{
		if (!ElementControllerModel.IsValid())
		{
			return FText::GetEmpty();
		}

		if (!ElementControllerModel->HasUniformMaxValue())
		{
			return LOCTEXT("MultipleValues", "Multiple Values");
		}

		const UDMXControlConsoleElementController* ElementController = ElementControllerModel->GetElementController();
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ElementController || !ControlConsoleEditorData)
		{
			return FText::GetEmpty();
		}

		float MaxValue = 0.f;
		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte)
		{
			MaxValue = ElementControllerModel->GetRelativeMaxValue();
			if (ElementControllerModel->HasUniformDataType())
			{
				return FText::FromString(FString::FromInt(MaxValue));
			}
		}
		else
		{
			MaxValue = ElementController->GetMaxValue();
		}

		return FText::FromString(FString::SanitizeFloat(MaxValue));
	}

	void SDMXControlConsoleEditorElementControllerView::OnMaxValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ControlConsoleEditorData || !ElementController || NewText.IsEmpty())
		{
			return;
		}

		float NewValue = -1.f;
		if (!LexTryParseString(NewValue, *NewText.ToString()) || NewValue < 0.f)
		{
			return;
		}

		const EDMXControlConsoleEditorValueType ValueType = ControlConsoleEditorData->GetValueType();
		if (ValueType == EDMXControlConsoleEditorValueType::Byte &&
			ElementControllerModel->HasUniformDataType())
		{
			// Normalize the input value
			const UDMXControlConsoleFaderBase* FirstFader = ElementControllerModel->GetFirstAvailableFader();
			if (ElementControllerModel->HasUniformDataType() && FirstFader)
			{
				const uint8 NumBytes = static_cast<uint8>(FirstFader->GetDataType()) + 1;
				const float ValueRange = FMath::Pow(2.f, 8.f * NumBytes) - 1;
				NewValue /= ValueRange;
			}
		}

		const FScopedTransaction ElementControllerMinValueEditedTransaction(LOCTEXT("ElementControllerMinValueEditedTransaction", "Edit Min Value"));
		
		// Ensure that each fader in the controller is registered to the transaction
		for (UDMXControlConsoleFaderBase* Fader : ElementController->GetFaders())
		{
			if (Fader)
			{
				Fader->Modify();
			}
		}
		
		ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetMaxValuePropertyName()));
		ElementController->SetMaxValue(NewValue);
		ElementController->PostEditChange();
	}

	void SDMXControlConsoleEditorElementControllerView::OnEnableElementController(bool bEnable) const
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (!ElementController)
		{
			return;
		}
		const FScopedTransaction EnableElementControllerOptionTransaction(LOCTEXT("EnableElementControllerOptionTransaction", "Edit Enable state"));
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = ElementController->GetElements();
		for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
		{
			UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
			if (Fader)
			{
				Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsEnabledPropertyName()));
				Fader->SetEnabled(bEnable);
				Fader->PostEditChange();
			}
		}
	}

	void SDMXControlConsoleEditorElementControllerView::OnRemoveElementController() const
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (!ElementController)
		{
			return;
		}

		const FScopedTransaction RemoveElementControllerOptionTransaction(LOCTEXT("RemoveElementControllerOptionTransaction", "Fader removed"));
		
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = ElementController->GetElements();
		for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
		{
			if (Element)
			{
				Element->Destroy();
			}
		}

		ElementController->PreEditChange(nullptr);
		ElementController->Destroy();
		ElementController->PostEditChange();
	}

	void SDMXControlConsoleEditorElementControllerView::OnResetElementController() const
	{
		UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ElementController)
		{
			return;
		}

		const FScopedTransaction ResetElementControllerOptionTransaction(LOCTEXT("ResetElementControllerOptionTransaction", "Fader reset to default"));
			
		// Ensure that each fader in the controller is registered to the transaction
		for (UDMXControlConsoleFaderBase* Fader : ElementController->GetFaders())
		{
			if (Fader)
			{
				Fader->Modify();
			}
		}
			
		ElementController->PreEditChange(nullptr);
		ElementController->ResetToDefault();
		ElementController->PostEditChange();
	}

	void SDMXControlConsoleEditorElementControllerView::OnLockElementController(bool bLock) const
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (ElementController)
		{
			const FScopedTransaction LockElementControllerOptionTransaction(LOCTEXT("LockElementControllerOptionTransaction", "Edit Fader lock state"));
			ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetIsLockedPropertyName()));
			ElementController->SetLocked(bLock);
			ElementController->PostEditChange();
		}
	}

	FReply SDMXControlConsoleEditorElementControllerView::OnLockClicked()
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (!EditorModel.IsValid() || !ElementController)
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction ElementControllerLockStateEditedtTransaction(LOCTEXT("ElementControllerLockStateEditedtTransaction", "Edit Lock state"));
		ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetIsLockedPropertyName()));
		ElementController->SetLocked(!ElementController->IsLocked());
		ElementController->PostEditChange();

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();
		if (!SelectedElementControllers.IsEmpty() && SelectedElementControllers.Contains(ElementController))
		{
			for (const TWeakObjectPtr<UObject>& SelectElementControllerObject : SelectedElementControllers)
			{
				UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectElementControllerObject);
				if (!SelectedElementController || !SelectedElementController->IsMatchingFilter())
				{
					continue;
				}

				SelectedElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetIsLockedPropertyName()));
				SelectedElementController->SetLocked(ElementController->IsLocked());
				SelectedElementController->PostEditChange();
			}
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorElementControllerView::OnEnableToggleChanged(ECheckBoxState CheckState)
	{
		UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!EditorModel.IsValid() || !ElementController)
		{
			return;
		}

		const FScopedTransaction SetEnabledTransaction(LOCTEXT("SetEnabledTransaction", "Edit Enable state"));
		
		const bool bIsControllerEnabled = CheckState == ECheckBoxState::Checked;
		const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = ElementController->GetElements();
		for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
		{
			UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
			if (Fader)
			{
				Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsEnabledPropertyName()));
				Fader->SetEnabled(bIsControllerEnabled);
				Fader->PostEditChange();
			}
		}

		// If the controller is selected, set the enable state of all the other selected controllers
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();
		if (SelectedElementControllers.Contains(ElementController))
		{
			for (const TWeakObjectPtr<UObject>& SelectElementControllerObject : SelectedElementControllers)
			{
				UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectElementControllerObject);
				if (!SelectedElementController || !SelectedElementController->IsMatchingFilter())
				{
					continue;
				}

				const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& SelectedElements = SelectedElementController->GetElements();
				for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& SelectedElement : SelectedElements)
				{
					UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(SelectedElement.GetObject());
					if (Fader)
					{
						Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetIsEnabledPropertyName()));
						Fader->SetEnabled(bIsControllerEnabled);
						Fader->PostEditChange();
					}
				}
			}
		}
	}

	ECheckBoxState SDMXControlConsoleEditorElementControllerView::IsEnableChecked() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		if (!ElementController)
		{
			return ECheckBoxState::Undetermined;
		}

		const UDMXControlConsoleFaderGroupController& OwnerFaderGroupController = ElementController->GetOwnerFaderGroupControllerChecked();
		const ECheckBoxState FaderGroupControllerEnableState = OwnerFaderGroupController.GetEnabledState();
		const ECheckBoxState ElementControllerEnableState = ElementController->GetEnabledState();
		if (FaderGroupControllerEnableState == ECheckBoxState::Checked || ElementControllerEnableState == ECheckBoxState::Unchecked)
		{
			return ElementControllerEnableState;
		}

		return ECheckBoxState::Undetermined;
	}

	FOptionalSize SDMXControlConsoleEditorElementControllerView::GetElementControllerHeightByViewMode() const
	{
		using namespace DMXControlConsoleEditorElementControllerView::Private;
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const EDMXControlConsoleEditorViewMode ViewMode = EditorData->GetFadersViewMode();
			return ViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
		}

		return CollapsedViewModeHeight;
	}

	FSlateColor SDMXControlConsoleEditorElementControllerView::GetLockButtonColor() const
	{
		if (LockButton.IsValid())
		{
			return LockButton->IsHovered() ? FStyleColors::AccentWhite : FLinearColor(1.f, 1.f, 1.f, .4f);
		}

		return FLinearColor::White;
	}

	EVisibility SDMXControlConsoleEditorElementControllerView::GetExpandedViewModeVisibility() const
	{
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;

		const bool bIsVisible =
			ElementControllerModel.IsValid() &&
			EditorData &&
			EditorData->GetFadersViewMode() == EDMXControlConsoleEditorViewMode::Expanded;

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorElementControllerView::GetLockButtonVisibility() const
	{
		const bool bIsVisible = ElementControllerModel.IsValid() && ElementControllerModel->IsLocked();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* SDMXControlConsoleEditorElementControllerView::GetBorderImage() const
	{
		if (!ElementControllerModel.IsValid())
		{
			return nullptr;
		}

		if (IsHovered())
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Highlighted");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Hovered");
			}
		}
		else
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Selected");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader");
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
