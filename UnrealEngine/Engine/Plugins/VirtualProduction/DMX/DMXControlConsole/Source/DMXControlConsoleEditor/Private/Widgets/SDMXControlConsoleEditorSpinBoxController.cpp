// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorSpinBoxController.h"

#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxVertical.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorSpinBoxController"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorSpinBoxController::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
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

		const FSpinBoxStyle& SpinBoxStyle = ElementControllerModel->HasSingleElement() ?
			FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("DMXControlConsole.SingleFader") :
			FDMXControlConsoleEditorStyle::Get().GetWidgetStyle<FSpinBoxStyle>("DMXControlConsole.MultiFader");

		ChildSlot
			[
				SNew(SBorder)
				.BorderImage(this, &SDMXControlConsoleEditorSpinBoxController::GetSpinBoxBorderImage)
				[
					SAssignNew(ElementControllerSpinBox, SDMXControlConsoleEditorSpinBoxVertical<float>)
					.Value(this, &SDMXControlConsoleEditorSpinBoxController::GetValue)
					.ValueText(this, &SDMXControlConsoleEditorSpinBoxController::GetValueAsText)
					.MinValue(this, &SDMXControlConsoleEditorSpinBoxController::GetMinValue)
					.MaxValue(this, &SDMXControlConsoleEditorSpinBoxController::GetMaxValue)
					.MinSliderValue(this, &SDMXControlConsoleEditorSpinBoxController::GetMinValue)
					.MaxSliderValue(this, &SDMXControlConsoleEditorSpinBoxController::GetMaxValue)
					.OnBeginSliderMovement(this, &SDMXControlConsoleEditorSpinBoxController::OnBeginValueChange)
					.OnValueChanged(this, &SDMXControlConsoleEditorSpinBoxController::HandleValueChanged)
					.OnValueCommitted(this, &SDMXControlConsoleEditorSpinBoxController::OnValueCommitted)
					.IsActive(this, &SDMXControlConsoleEditorSpinBoxController::IsElementControllerSpinBoxActive)
					.Style(&SpinBoxStyle)
					.ToolTipText(this, &SDMXControlConsoleEditorSpinBoxController::GetToolTipText)
					.MinDesiredWidth(40.0f)
				]
			];
	}

	UDMXControlConsoleElementController* SDMXControlConsoleEditorSpinBoxController::GetElementController() const
	{
		return ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
	}

	void SDMXControlConsoleEditorSpinBoxController::CommitValue(float NewValue)
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		if (ElementController)
		{
			PreCommittedValue = ElementController->GetValue();
			OnValueCommitted(NewValue, ETextCommit::Default);
		}
	}

	FReply SDMXControlConsoleEditorSpinBoxController::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot handle selection correctly.")))
		{
			return FReply::Unhandled();
		}

		if (!ensureMsgf(ElementControllerSpinBox.IsValid(), TEXT("Invalid fader widget, cannot handle selection correctly.")))
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
				if (!IsSelected() || !ElementControllerSpinBox->IsHovered())
				{
					constexpr bool bNotifySelectionChange = false;
					SelectionHandler->ClearSelection(bNotifySelectionChange);
					SelectionHandler->AddToSelection(ElementController);
				}
			}

			return FReply::Handled()
				.PreventThrottling();
		}

		return FReply::Unhandled();
	}

	bool SDMXControlConsoleEditorSpinBoxController::IsSelected() const
	{
		UDMXControlConsoleElementController* ElementController = GetElementController();
		if (ElementController && EditorModel.IsValid())
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			return SelectionHandler->IsSelected(ElementController);
		}

		return false;
	}

	float SDMXControlConsoleEditorSpinBoxController::GetValue() const
	{
		if (!ElementControllerModel.IsValid() ||
			(ElementControllerModel->HasUniformDataType() && 
			!ElementControllerModel->HasUniformValue()))
		{
			return 1.f;
		}

		const UDMXControlConsoleElementController* ElementController = ElementControllerModel->GetElementController();
		return ElementController ? ElementController->GetValue() : 1.f;
	}

	FText SDMXControlConsoleEditorSpinBoxController::GetValueAsText() const
	{
		if (!ElementControllerModel.IsValid())
		{
			return FText::GetEmpty();
		}

		if (ElementControllerModel->HasUniformDataType() && !ElementControllerModel->HasUniformValue())
		{
			return FText::Format(LOCTEXT("MultipleValues", "Multiple{0}Values"), FText::FromString(LINE_TERMINATOR));
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

		return FText::FromString(FString::SanitizeFloat(Value, 2).Left(4));
	}

	TOptional<float> SDMXControlConsoleEditorSpinBoxController::GetMinValue() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		return ElementController ? ElementController->GetMinValue() : TOptional<float>();
	}

	TOptional<float> SDMXControlConsoleEditorSpinBoxController::GetMaxValue() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		return ElementController ? ElementController->GetMaxValue() : TOptional<float>();
	}

	void SDMXControlConsoleEditorSpinBoxController::HandleValueChanged(float NewValue)
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot set element controller value correctly.")))
		{
			return;
		}

		if (!ensureMsgf(ElementControllerSpinBox.IsValid(), TEXT("Invalid element controller widget, cannot set element contorller value correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ensureMsgf(EditorData, TEXT("Invalid control console editor data, cannot set element controller value correctly.")))
		{
			return;
		}

		const EDMXControlConsoleEditorControlMode ControlMode = EditorData->GetControlMode();
		const float Range = ElementController->GetMaxValue() - ElementController->GetMinValue();
		const float FaderSpinBoxValue = ElementControllerSpinBox->GetValue();

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();
		for (const TWeakObjectPtr<UObject>& SelectedElementControllerObject : SelectedElementControllers)
		{
			UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
			if (!SelectedElementController ||
				!SelectedElementController->IsMatchingFilter() ||
				SelectedElementController->IsLocked())
			{
				continue;
			}

			const float SelectedElementControllerRange = SelectedElementController->GetMaxValue() - SelectedElementController->GetMinValue();
			float SelectedElementControllerPercentage = 0.f;
			float SelectedElementControllerValue = 0.f;

			if (ControlMode == EDMXControlConsoleEditorControlMode::Relative)
			{
				// Relative percentage
				SelectedElementControllerPercentage = (NewValue - FaderSpinBoxValue) / Range;
				SelectedElementControllerValue = FMath::Clamp(SelectedElementController->GetValue() + SelectedElementControllerRange * SelectedElementControllerPercentage, 0.f, 1.f);

			}
			else if (ControlMode == EDMXControlConsoleEditorControlMode::Absolute)
			{
				// Absolute percentage
				SelectedElementControllerPercentage = (NewValue - ElementController->GetMinValue()) / Range;
				SelectedElementControllerValue = SelectedElementController->GetMinValue() + SelectedElementControllerRange * SelectedElementControllerPercentage;
			}
			else
			{
				checkf(0, TEXT("Undefined enum value"));
			}

			SelectedElementController->SetValue(SelectedElementControllerValue);
		}
	}

	void SDMXControlConsoleEditorSpinBoxController::OnBeginValueChange()
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot set element controller value correctly.")))
		{
			return;
		}

		PreCommittedValue = ElementController->GetValue();
	}

	void SDMXControlConsoleEditorSpinBoxController::OnValueCommitted(float NewValue, ETextCommit::Type CommitType)
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot set element controller value correctly.")))
		{
			return;
		}

		if (!ensureMsgf(ElementControllerSpinBox.IsValid(), TEXT("Invalid element controller widget, cannot set element controller value correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!ensureMsgf(EditorData, TEXT("Invalid control console editor data, cannot set element controller value correctly.")))
		{
			return;
		}

		const FScopedTransaction FaderValueCommittedTransaction(LOCTEXT("FaderValueCommittedTransaction", "Edit Fader Value"));

		const float Range = ElementController->GetMaxValue() - ElementController->GetMinValue();
		const float ElementcontrollerSpinBoxValue = ElementControllerSpinBox->GetValue();
		float PreCommittedPercentage = 0.f;
		float Percentage = 0.f;

		const EDMXControlConsoleEditorControlMode ControlMode = EditorData->GetControlMode();
		if (ControlMode == EDMXControlConsoleEditorControlMode::Relative)
		{
			// Relative percentages
			PreCommittedPercentage = (PreCommittedValue - ElementcontrollerSpinBoxValue) / Range;
			Percentage = (NewValue - ElementcontrollerSpinBoxValue) / Range;
		}
		else if (ControlMode == EDMXControlConsoleEditorControlMode::Absolute)
		{
			// Absolute percentages
			PreCommittedPercentage = (PreCommittedValue - ElementController->GetMinValue()) / Range;
			Percentage = (NewValue - ElementController->GetMinValue()) / Range;
		}
		else
		{
			checkf(0, TEXT("Undefined enum value"));
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();
		for (const TWeakObjectPtr<UObject>& SelectedElementControllerObject : SelectedElementControllers)
		{
			UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
			if (!SelectedElementController ||
				!SelectedElementController->IsMatchingFilter() ||
				SelectedElementController->IsLocked())
			{
				continue;
			}

			const float SelectedElementControllerRange = SelectedElementController->GetMaxValue() - SelectedElementController->GetMinValue();
			float SelectedElementControllerPreCommittedValue = 0.f;
			float SelectedElementControllerValue = 0.f;

			if (ControlMode == EDMXControlConsoleEditorControlMode::Relative)
			{
				SelectedElementControllerPreCommittedValue = FMath::Clamp(SelectedElementController->GetValue() + SelectedElementControllerRange * PreCommittedPercentage, 0.f, 1.f);
				SelectedElementControllerValue = FMath::Clamp(SelectedElementController->GetValue() + SelectedElementControllerRange * Percentage, 0.f, 1.f);
			}
			else if (ControlMode == EDMXControlConsoleEditorControlMode::Absolute)
			{
				SelectedElementControllerPreCommittedValue = SelectedElementController->GetMinValue() + SelectedElementControllerRange * PreCommittedPercentage;
				SelectedElementControllerValue = SelectedElementController->GetMinValue() + SelectedElementControllerRange * Percentage;
			}
			else
			{
				checkf(0, TEXT("Undefined enum value"));
			}

			// Reset to PreCommittedValue to handle transactions
			SelectedElementController->SetValue(SelectedElementControllerPreCommittedValue);

			// Ensure that each fader in the selected controller is registered to the transaction
			for (UDMXControlConsoleFaderBase* Fader : SelectedElementController->GetFaders())
			{
				if (Fader)
				{
					Fader->Modify();
				}
			}

			SelectedElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetValuePropertyName()));
			SelectedElementController->SetValue(SelectedElementControllerValue);
			SelectedElementController->PostEditChange();
		}
	}

	bool SDMXControlConsoleEditorSpinBoxController::IsElementControllerSpinBoxActive() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		const bool bIsControllerEnabled = ElementController && ElementController->GetEnabledState() == ECheckBoxState::Checked;
		return bIsControllerEnabled;
	}

	FText SDMXControlConsoleEditorSpinBoxController::GetToolTipText() const
	{
		const UDMXControlConsoleElementController* ElementController = GetElementController();
		if (ElementController)
		{
			const FString& ElementControllerName = ElementController->GetUserName();
			const FString ElementControllerValueAsString = FString::SanitizeFloat(ElementController->GetValue());
			const FString ElementControllerMaxValueAsString = FString::SanitizeFloat(ElementController->GetMaxValue());
			const FString ToolTipString = FString::Format(TEXT("{0}{1}/{2}"), { ElementControllerName + LINE_TERMINATOR, ElementControllerValueAsString, ElementControllerMaxValueAsString });
			return FText::FromString(ToolTipString);
		}

		return FText::GetEmpty();
	}

	const FSlateBrush* SDMXControlConsoleEditorSpinBoxController::GetSpinBoxBorderImage() const
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		if (!ElementController)
		{
			return nullptr;
		}

		const bool bIsControllerEnabled = ElementController->GetEnabledState() == ECheckBoxState::Checked;
		if (ElementControllerModel->IsLocked() || !bIsControllerEnabled)
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
		}

		if (IsHovered() || IsSelected())
		{
			return
				ElementControllerModel->HasSingleElement() ?
				FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder_SingleHovered") :
				FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder_MultiHovered");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.SpinBoxBorder");
		}
	}
}

#undef LOCTEXT_NAMESPACE
