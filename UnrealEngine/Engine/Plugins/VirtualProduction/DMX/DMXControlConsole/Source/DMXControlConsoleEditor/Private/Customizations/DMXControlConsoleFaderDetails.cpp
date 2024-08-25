// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFaderDetails.h"

#include "Algo/AllOf.h"
#include "Algo/Transform.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleRawFader.h"
#include "IPropertyUtilities.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderDetails"

namespace UE::DMX::Private
{
	FDMXControlConsoleFaderDetails::FDMXControlConsoleFaderDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakEditorModel(InWeakEditorModel)
	{}

	TSharedRef<IDetailCustomization> FDMXControlConsoleFaderDetails::MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		return MakeShared<FDMXControlConsoleFaderDetails>(InWeakEditorModel);
	}

	void FDMXControlConsoleFaderDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		PropertyUtilities = InDetailLayout.GetPropertyUtilities();

		// Value property handle
		const TSharedRef<IPropertyHandle> ValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetValuePropertyName());
		ValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersValueChanged));

		// MinValue property handle
		const TSharedRef<IPropertyHandle> MinValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetMinValuePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		MinValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersMinValueChanged));

		// MaxValue property handle
		const TSharedRef<IPropertyHandle> MaxValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetMaxValuePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		MaxValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersMaxValueChanged));

		// UniverseID property handle
		const TSharedRef<IPropertyHandle> UniverseIDHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetUniverseIDPropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		UniverseIDHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersUniverseIDChanged));

		// StartingAddress property handle
		const TSharedRef<IPropertyHandle> StartingAddressHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetStartingAddressPropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		StartingAddressHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersDataTypeChanged));

		// DataType property handle
		const TSharedRef<IPropertyHandle> DataTypeHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetDataTypePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		DataTypeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersDataTypeChanged));
		if (!HasOnlyRawFadersSelected())
		{
			InDetailLayout.HideProperty(DataTypeHandle);
		}

		IDetailCategoryBuilder& CategoryBuilder = InDetailLayout.EditCategory("DMX Fader");
		TArray<TSharedRef<IPropertyHandle>> Properties;
		CategoryBuilder.GetDefaultProperties(Properties);
		for (const TSharedRef<IPropertyHandle>& Handle : Properties)
		{
			if (Handle->GetProperty()->GetFName() == UDMXControlConsoleFaderBase::GetMaxValuePropertyName())
			{				
				// Customize the MaxValue property's reset to default behaviour
				MaxValueHandle->MarkResetToDefaultCustomized();

				const TAttribute<bool> MaxValueResetToDefaultVisibilityAttribute(this, &FDMXControlConsoleFaderDetails::GetMaxValueResetToDefaultVisibility);
				const FSimpleDelegate OnMaxValueResetToDefaultClickedDelegate = FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnMaxValueResetToDefaultClicked);

				CategoryBuilder.AddProperty(MaxValueHandle)
					.OverrideResetToDefault(FResetToDefaultOverride::Create(MaxValueResetToDefaultVisibilityAttribute, OnMaxValueResetToDefaultClickedDelegate));
			}
			else
			{
				CategoryBuilder.AddProperty(Handle);
			}
		}
	}

	bool FDMXControlConsoleFaderDetails::HasOnlyRawFadersSelected() const
	{
		TArray<UDMXControlConsoleFaderBase*> ValidSelectedFaders = GetValidFadersBeingEdited();
		// Remove Faders which don't match filtering
		ValidSelectedFaders.RemoveAll([](const UDMXControlConsoleFaderBase* SelectedFader)
			{
				return SelectedFader && !SelectedFader->IsMatchingFilter();
			});

		const bool bAreAllRawFaders = Algo::AllOf(ValidSelectedFaders, 
			[](const UDMXControlConsoleFaderBase* SelectedFader)
			{
				return IsValid(Cast<UDMXControlConsoleRawFader>(SelectedFader));
			});

		return bAreAllRawFaders;
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader)
			{
				continue;
			}

			const uint32 CurrentValue = Fader->GetValue();
			Fader->SetValue(CurrentValue); 
			
			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			const float NormalizedValue = CurrentValue / ValueRange;

			ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetValuePropertyName()));
			ElementController->SetValue(NormalizedValue);
			ElementController->PostEditChange();
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersMinValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader)
			{
				continue;
			}

			const uint32 CurrentMinValue = Fader->GetMinValue();
			Fader->SetMinValue(CurrentMinValue);

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			const float NormalizedMinValue = CurrentMinValue / ValueRange;

			ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetMinValuePropertyName()));
			ElementController->SetMinValue(NormalizedMinValue);
			ElementController->PostEditChange();
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersMaxValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader)
			{
				continue;
			}

			const uint32 CurrentMaxValue = Fader->GetMaxValue();
			Fader->SetMaxValue(CurrentMaxValue);

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			const float NormalizedMaxValue = CurrentMaxValue / ValueRange;

			ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetMaxValuePropertyName()));
			ElementController->SetMaxValue(NormalizedMaxValue);
			ElementController->PostEditChange();
		}
	}

	bool FDMXControlConsoleFaderDetails::GetMaxValueResetToDefaultVisibility() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader)
			{
				continue;
			}

			const uint32 MaxValue = GetMaxValueForSignalFormat(Fader->GetDataType());
			return Fader->GetMaxValue() != MaxValue;
		}

		return false;
	}

	void FDMXControlConsoleFaderDetails::OnMaxValueResetToDefaultClicked()
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader)
			{
				continue;
			}

			Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetMaxValuePropertyName()));

			const uint32 MaxValue = GetMaxValueForSignalFormat(Fader->GetDataType());
			Fader->SetMaxValue(MaxValue);

			Fader->PostEditChange();

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetMaxValuePropertyName()));

			constexpr float NormalizedMaxValue = 1.f;
			ElementController->SetMaxValue(NormalizedMaxValue);

			ElementController->PostEditChange();
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersUniverseIDChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader);
			if (!RawFader)
			{
				continue;
			}

			const int32 CurrentUniverseID = RawFader->GetUniverseID();
			RawFader->SetUniverseID(CurrentUniverseID);
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersDataTypeChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader);
			if (!RawFader)
			{
				continue;
			}

			RawFader->PreEditChange(nullptr);
			const EDMXFixtureSignalFormat CurrentDataType = RawFader->GetDataType();
			RawFader->SetDataType(CurrentDataType);
			const uint32 CurrentMaxValue = RawFader->GetMaxValue();
			RawFader->SetMaxValue(CurrentMaxValue);
			const uint32 CurrentValue = RawFader->GetValue();
			RawFader->SetValue(CurrentValue);
			RawFader->PostEditChange();

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(RawFader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const uint8 NumChannels = static_cast<uint8>(CurrentDataType) + 1;
			const float ValueRange = FMath::Pow(2.f, 8.f * NumChannels) - 1;
			const float NormalizedMaxValue = CurrentMaxValue / ValueRange;
			const float NormalizedValue = CurrentValue / ValueRange;

			ElementController->PreEditChange(nullptr);
			ElementController->SetMaxValue(NormalizedMaxValue);
			ElementController->SetValue(NormalizedValue);
			ElementController->PostEditChange();
		}

		PropertyUtilities->RequestRefresh();
	}

	uint32 FDMXControlConsoleFaderDetails::GetMaxValueForSignalFormat(EDMXFixtureSignalFormat SignalFormat) const
	{
		switch (SignalFormat)
		{
		case EDMXFixtureSignalFormat::E8Bit:
			return 0xFF;
			break;
		case EDMXFixtureSignalFormat::E16Bit:
			return 0xFFFF;
			break;
		case EDMXFixtureSignalFormat::E24Bit:
			return 0xFFFFFF;
			break;
		case EDMXFixtureSignalFormat::E32Bit:
			return 0xFFFFFFFF;
			break;
		default:
			checkf(0, TEXT("Unhandled signal format in %s"), ANSI_TO_TCHAR(__FUNCTION__));
		}

		return 0;
	}

	TArray<UDMXControlConsoleFaderBase*> FDMXControlConsoleFaderDetails::GetValidFadersBeingEdited() const
	{
		const TArray<TWeakObjectPtr<UObject>> EditedObjects = PropertyUtilities->GetSelectedObjects();
		TArray<UDMXControlConsoleFaderBase*> Result;
		Algo::TransformIf(EditedObjects, Result,
			[](TWeakObjectPtr<UObject> Object)
			{
				return IsValid(Cast<UDMXControlConsoleFaderBase>(Object.Get()));
			},
			[](TWeakObjectPtr<UObject> Object)
			{
				return Cast<UDMXControlConsoleFaderBase>(Object.Get());
			}
		);

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
