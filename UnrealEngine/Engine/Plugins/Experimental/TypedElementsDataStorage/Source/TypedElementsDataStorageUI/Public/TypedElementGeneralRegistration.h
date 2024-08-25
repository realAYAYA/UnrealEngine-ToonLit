// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementGeneralRegistration.generated.h"

UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementGeneralRegistrationFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	static const FName CellPurpose;
	static const FName HeaderPurpose;
	static const FName CellDefaultPurpose;
	static const FName HeaderDefaultPurpose;

	~UTypedElementGeneralRegistrationFactory() override = default;

	void RegisterWidgetPurposes(ITypedElementDataStorageUiInterface& DataStorageUi) const override;
};
