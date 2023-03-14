// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyValueColor.h"

#include "VariantManagerContentLog.h"
#include "VariantObjectBinding.h"

#include "Components/LightComponent.h"
#include "CoreMinimal.h"
#include "HAL/UnrealMemory.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PropertyValueColor"

UPropertyValueColor::UPropertyValueColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<uint8> UPropertyValueColor::GetDataFromResolvedObject() const
{
	int32 PropertySizeBytes = GetValueSizeInBytes();
	TArray<uint8> CurrentData;
	CurrentData.SetNumZeroed(PropertySizeBytes);

	if (!HasValidResolve())
	{
		return CurrentData;
	}

	// Used by ULightComponent
	if (PropertySetterName == TEXT("SetLightColor"))
	{
		ULightComponent* ContainerObject = (ULightComponent*) ParentContainerAddress;
		if (!ContainerObject || !ContainerObject->IsValidLowLevel())
		{
			UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueColor '%s' does not have a ULightComponent as parent address!"), *GetFullDisplayString());
			return CurrentData;
		}

		FLinearColor Col = ContainerObject->GetLightColor();
		FMemory::Memcpy(CurrentData.GetData(), &Col, PropertySizeBytes);
	}

	return CurrentData;
}

UScriptStruct* UPropertyValueColor::GetStructPropertyStruct() const
{
	static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	static UScriptStruct* LinearColorScriptStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("LinearColor"));
	return LinearColorScriptStruct;
}

int32 UPropertyValueColor::GetValueSizeInBytes() const
{
	return sizeof(FLinearColor);
}

#undef LOCTEXT_NAMESPACE
