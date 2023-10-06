// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementGeneralRegistration.h"

#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#define LOCTEXT_NAMESPACE "TypedElementsUI_GeneralRegistration"

const FName UTypedElementGeneralRegistrationFactory::CellPurpose(TEXT("General.Cell"));
const FName UTypedElementGeneralRegistrationFactory::CellDefaultPurpose(TEXT("General.Cell.Default"));
const FName UTypedElementGeneralRegistrationFactory::HeaderDefaultPurpose(TEXT("General.Header.Default"));

void UTypedElementGeneralRegistrationFactory::RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const
{
	DataStorageUi.RegisterWidgetPurpose(CellPurpose, ITypedElementDataStorageUiInterface::EPurposeType::UniqueByNameAndColumn,
		LOCTEXT("CellPurpose", "General purpose widgets that can be used as cells for specific columns or column combinations."));
	
	DataStorageUi.RegisterWidgetPurpose(HeaderDefaultPurpose, ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName,
		LOCTEXT("HeaderDefaultPurpose", "The default widget to use in headers if no other specialization is provided."));
	
	DataStorageUi.RegisterWidgetPurpose(CellDefaultPurpose, ITypedElementDataStorageUiInterface::EPurposeType::UniqueByName,
		LOCTEXT("CellDefaultPurpose", "The default widget to use in cells if no other specialization is provided."));
}

#undef LOCTEXT_NAMESPACE
