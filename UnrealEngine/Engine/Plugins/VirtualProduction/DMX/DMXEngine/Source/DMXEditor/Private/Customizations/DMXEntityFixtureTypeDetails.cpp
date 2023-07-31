// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXEntityFixtureTypeDetails.h"

#include "DMXEditorLog.h"
#include "DMXInitializeFixtureTypeFromGDTFHelper.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXEntityFixtureType.h"

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "DMXEntityFixtureTypeDetails"

TSharedRef<IDetailCustomization> FDMXEntityFixtureTypeDetails::MakeInstance()
{
	return MakeShared<FDMXEntityFixtureTypeDetails>();
}

void FDMXEntityFixtureTypeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UDMXEntityFixtureType, Modes));

	GDTFHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDMXEntityFixtureType, DMXImport));
	GDTFHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXEntityFixtureTypeDetails::OnDMXImportChanged));
}

void FDMXEntityFixtureTypeDetails::OnDMXImportChanged()
{
	UObject* DMXImportObject = nullptr;
	if (GDTFHandle->GetValue(DMXImportObject) != FPropertyAccess::Success)
	{
		return;
	}

	UDMXImportGDTF* DMXImportGDTF = Cast<UDMXImportGDTF>(DMXImportObject);
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyUtilities->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> WeakFixtureTypeObject : SelectedObjects)
	{
		if (UDMXEntityFixtureType* FixtureType = Cast<UDMXEntityFixtureType>(WeakFixtureTypeObject.Get()))
		{
			FixtureType->PreEditChange(nullptr);
			FixtureType->Modes.Reset();
			FixtureType->PostEditChange();
			
			if (!DMXImportGDTF)
			{
				continue;
			}

			// Try to use the work around that supports creation of matrices, otherwise setup the fixture type with the old implementation
			FixtureType->PreEditChange(nullptr);
			const bool bAdvancedImportSuccess = FDMXInitializeFixtureTypeFromGDTFHelper::GenerateModesFromGDTF(*FixtureType, *DMXImportGDTF);
			if (!bAdvancedImportSuccess)
			{
				UE_LOG(LogDMXEditor, Warning, TEXT("Failed to initialize Fixture Type '%s', falling back to legacy method that doesn't support matrix fixtures."), *FixtureType->GetName());
				FixtureType->SetModesFromDMXImport(DMXImportGDTF);
			}
			FixtureType->PostEditChange();
		}
	}
}

#undef LOCTEXT_NAMESPACE
