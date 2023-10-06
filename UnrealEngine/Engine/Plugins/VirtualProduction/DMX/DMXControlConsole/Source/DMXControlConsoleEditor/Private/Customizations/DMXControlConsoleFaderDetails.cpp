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
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFaderDetails"

namespace UE::DMXControlConsole
{
	void FDMXControlConsoleFaderDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		PropertyUtilities = InDetailLayout.GetPropertyUtilities();

		// Value property handle
		const TSharedPtr<IPropertyHandle> ValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetValuePropertyName());
		ValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersValueChanged));

		// MinValue property handle
		const TSharedPtr<IPropertyHandle> MinValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetMinValuePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		MinValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersMinValueChanged));

		// MaxValue property handle
		const TSharedPtr<IPropertyHandle> MaxValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetMaxValuePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		MaxValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersMaxValueChanged));

		// UniverseID property handle
		const TSharedPtr<IPropertyHandle> UniverseIDHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetUniverseIDPropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		UniverseIDHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersUniverseIDChanged));

		// StartingAddress property handle
		const TSharedPtr<IPropertyHandle> StartingAddressHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetStartingAddressPropertyName(), UDMXControlConsoleFaderBase::StaticClass());
		StartingAddressHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleFaderDetails::OnSelectedFadersDataTypeChanged));

		// DataType property handle
		const TSharedPtr<IPropertyHandle> DataTypeHandle = InDetailLayout.GetProperty(UDMXControlConsoleFaderBase::GetDataTypePropertyName(), UDMXControlConsoleFaderBase::StaticClass());
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

	void FDMXControlConsoleFaderDetails::ForceRefresh() const
	{
		if (!PropertyUtilities.IsValid())
		{
			return;
		}

		PropertyUtilities->ForceRefresh();
	}

	bool FDMXControlConsoleFaderDetails::HasOnlyRawFadersSelected() const
	{
		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		TArray<TWeakObjectPtr<UObject>> SelectedFaderObjects = SelectionHandler->GetSelectedFaders();
		// Remove Faders which don't match filtering
		SelectedFaderObjects.RemoveAll([](const TWeakObjectPtr<UObject>& SelectedFaderObject)
			{
				const UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedFaderObject);
				return SelectedFader && !SelectedFader->IsMatchingFilter();
			});


		auto AreAllRawFadersLambda = [](const TWeakObjectPtr<UObject>& SelectedFaderObject)
		{
			const UDMXControlConsoleRawFader* SelectedRawFader = Cast<UDMXControlConsoleRawFader>(SelectedFaderObject);
			if (SelectedRawFader)
			{
				return true;
			}

			return false;
		};

		return Algo::AllOf(SelectedFaderObjects, AreAllRawFadersLambda);
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader || !Fader->IsMatchingFilter())
			{
				continue;
			}

			const uint32 CurrentValue = Fader->GetValue();
			Fader->SetValue(CurrentValue);
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersMinValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader || !Fader->IsMatchingFilter())
			{
				continue;
			}

			const uint32 CurrentMinValue = Fader->GetMinValue();
			Fader->SetMinValue(CurrentMinValue);
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersMaxValueChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader || !Fader->IsMatchingFilter())
			{
				continue;
			}

			const uint32 CurrentMaxValue = Fader->GetMaxValue();
			Fader->SetMaxValue(CurrentMaxValue);
		}
	}

	bool FDMXControlConsoleFaderDetails::GetMaxValueResetToDefaultVisibility() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			if (!Fader || !Fader->IsMatchingFilter())
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
			if (!Fader || !Fader->IsMatchingFilter())
			{
				continue;
			}

			Fader->PreEditChange(UDMXControlConsoleFaderBase::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderBase::GetMaxValuePropertyName()));

			const uint32 MaxValue = GetMaxValueForSignalFormat(Fader->GetDataType());
			Fader->SetMaxValue(MaxValue);

			Fader->PostEditChange();
		}
	}

	void FDMXControlConsoleFaderDetails::OnSelectedFadersUniverseIDChanged() const
	{
		for (UDMXControlConsoleFaderBase* Fader : GetValidFadersBeingEdited())
		{
			UDMXControlConsoleRawFader* RawFader = Cast<UDMXControlConsoleRawFader>(Fader);
			if (!RawFader || !RawFader->IsMatchingFilter())
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
			if (!RawFader || !RawFader->IsMatchingFilter())
			{
				continue;
			}

			const EDMXFixtureSignalFormat CurrentDataType = RawFader->GetDataType();
			RawFader->SetDataType(CurrentDataType);
			const uint32 CurrentValue = RawFader->GetValue();
			RawFader->SetValue(CurrentValue);
		}
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
		});

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
