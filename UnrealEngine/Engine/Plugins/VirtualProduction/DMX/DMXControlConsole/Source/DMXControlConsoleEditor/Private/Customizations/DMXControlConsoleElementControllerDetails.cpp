// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleElementControllerDetails.h"

#include "Algo/Transform.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleElementControllerDetails"

namespace UE::DMX::Private
{
	FDMXControlConsoleElementControllerDetails::FDMXControlConsoleElementControllerDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		: WeakEditorModel(InWeakEditorModel)
	{}

	TSharedRef<IDetailCustomization> FDMXControlConsoleElementControllerDetails::MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	{
		return MakeShared<FDMXControlConsoleElementControllerDetails>(InWeakEditorModel);
	}

	void FDMXControlConsoleElementControllerDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
	{
		PropertyUtilities = InDetailLayout.GetPropertyUtilities();

		InDetailLayout.HideCategory("DMX Controller");

		IDetailCategoryBuilder& ElementControllerCategory = InDetailLayout.EditCategory("DMX Element Controller", FText::GetEmpty());
		ElementControllerCategory.AddProperty(UDMXControlConsoleElementController::GetUserNamePropertyName());

		// Value property handle
		const TSharedRef<IPropertyHandle> ValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleElementController::GetValuePropertyName(), UDMXControlConsoleElementController::StaticClass());
		ValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersValueChanged));
		ElementControllerCategory.AddProperty(ValueHandle);

		// MinValue property handle
		const TSharedRef<IPropertyHandle> MinValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleElementController::GetMinValuePropertyName(), UDMXControlConsoleElementController::StaticClass());
		MinValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersMinValueChanged));
		ElementControllerCategory.AddProperty(MinValueHandle);

		// MaxValue property handle
		const TSharedRef<IPropertyHandle> MaxValueHandle = InDetailLayout.GetProperty(UDMXControlConsoleElementController::GetMaxValuePropertyName(), UDMXControlConsoleElementController::StaticClass());
		MaxValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersMaxValueChanged));
		ElementControllerCategory.AddProperty(MaxValueHandle);

		// bIsLocked property handle
		const TSharedRef<IPropertyHandle> LockStateHandle = InDetailLayout.GetProperty(UDMXControlConsoleControllerBase::GetIsLockedPropertyName(), UDMXControlConsoleControllerBase::StaticClass());
		ElementControllerCategory.AddProperty(LockStateHandle);
		LockStateHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersLockStateChanged));
	}

	void FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersValueChanged() const
	{
		for (UDMXControlConsoleElementController* ElementController : GetValidElementControllersBeingEdited())
		{
			if (!ElementController)
			{
				continue;
			}

			const float CurrentValue = ElementController->GetValue();
			ElementController->SetValue(CurrentValue);
		}
	}

	void FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersMinValueChanged() const
	{
		for (UDMXControlConsoleElementController* ElementController : GetValidElementControllersBeingEdited())
		{
			if (!ElementController)
			{
				continue;
			}

			const float CurrentMinValue = ElementController->GetMinValue();
			ElementController->SetMinValue(CurrentMinValue);
		}
	}

	void FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersMaxValueChanged() const
	{
		for (UDMXControlConsoleElementController* ElementController : GetValidElementControllersBeingEdited())
		{
			if (!ElementController)
			{
				continue;
			}

			const float CurrentMaxValue = ElementController->GetMaxValue();
			ElementController->SetMaxValue(CurrentMaxValue);
		}
	}

	void FDMXControlConsoleElementControllerDetails::OnSelectedElementControllersLockStateChanged() const
	{
		for (UDMXControlConsoleElementController* ElementController : GetValidElementControllersBeingEdited())
		{
			if (!ElementController)
			{
				continue;
			}

			const bool bIsLocked = ElementController->IsLocked();
			ElementController->SetLocked(bIsLocked);
		}
	}

	TArray<UDMXControlConsoleElementController*> FDMXControlConsoleElementControllerDetails::GetValidElementControllersBeingEdited() const
	{
		const TArray<TWeakObjectPtr<UObject>>& EditedObjects = PropertyUtilities->GetSelectedObjects();
		TArray<UDMXControlConsoleElementController*> Result;
		Algo::TransformIf(EditedObjects, Result,
			[](TWeakObjectPtr<UObject> Object)
			{
				return IsValid(Cast<UDMXControlConsoleElementController>(Object.Get()));
			},
			[](TWeakObjectPtr<UObject> Object)
			{
				return Cast<UDMXControlConsoleElementController>(Object.Get());
			});

		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
