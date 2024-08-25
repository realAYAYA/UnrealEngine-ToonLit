// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"

#include "DetailLayoutBuilder.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"


void FChaosVDDetailsCustomizationUtils::HideAllCategories(IDetailLayoutBuilder& DetailBuilder, const TSet<FName>& AllowedCategories)
{
	// Hide everything as the only thing we want to show in these actors is the Recorded debug data
	TArray<FName> CurrentCategoryNames;
	DetailBuilder.GetCategoryNames(CurrentCategoryNames);
	for (const FName& CategoryToHide : CurrentCategoryNames)
	{
		if (!AllowedCategories.Contains(CategoryToHide))
		{
			DetailBuilder.HideCategory(CategoryToHide);
		}
	}
}

void FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(TConstArrayView<TSharedPtr<IPropertyHandle>> InPropertyHandles)
{
	if (InPropertyHandles.Num() == 0)
	{
		return;
	}

	for (const TSharedPtr<IPropertyHandle>& Handle : InPropertyHandles)
	{
		bool bIsParticleDataStruct;
		if (Handle && !HasValidCVDWrapperData(Handle, bIsParticleDataStruct))
		{
			// TODO: This doesn't work in all cases. It seems this just sets the IsCustom flag on, and that is why it is hidden but depends on how it is being customized
			// We need to find a more reliable way of hiding it
			Handle->MarkHiddenByCustomization();
		}
	}
}

void FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(TConstArrayView<TSharedRef<IPropertyHandle>> InPropertyHandles, IDetailLayoutBuilder& DetailBuilder)
{
	for (const TSharedRef<IPropertyHandle>& PropertyHandle : InPropertyHandles)
	{
		bool bIsParticleDataStruct = false;
		if (!HasValidCVDWrapperData(PropertyHandle, bIsParticleDataStruct))
		{
			if (bIsParticleDataStruct)
			{
				DetailBuilder.HideProperty(PropertyHandle);
			}
		}
	}
}

bool FChaosVDDetailsCustomizationUtils::HasValidCVDWrapperData(const TSharedPtr<IPropertyHandle>& InPropertyHandle, bool& bOutIsCVDBaseDataStruct)
{
	if (FProperty* Property = InPropertyHandle ? InPropertyHandle->GetProperty() : nullptr)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (StructProperty && StructProperty->Struct && StructProperty->Struct->IsChildOf(FChaosVDWrapperDataBase::StaticStruct()))
		{
			bOutIsCVDBaseDataStruct = true;

			void* Data = nullptr;
			InPropertyHandle->GetValueData(Data);
			if (Data)
			{
				const FChaosVDWrapperDataBase* DataViewer = static_cast<const FChaosVDWrapperDataBase*>(Data);

				// The Particle Data viewer struct has several fields that will have default values if there was no recorded data for them in the trace file
				// As these do not represent any real value, we should hide them in the details panel
				return DataViewer->HasValidData();
			}
		}
	}

	return true;
}
