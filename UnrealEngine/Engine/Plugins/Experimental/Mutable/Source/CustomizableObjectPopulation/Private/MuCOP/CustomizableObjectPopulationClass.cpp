// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOP/CustomizableObjectPopulationClass.h"

#include "MuCOP/CustomizableObjectPopulationCharacteristic.h"
#include "MuCOP/CustomizableObjectPopulationCustomVersion.h"
#include "Serialization/Archive.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectPopulationClass"

using namespace CustomizableObjectPopulation;

void UCustomizableObjectPopulationClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectPopulationCustomVersion::GUID);
}

#undef LOCTEXT_NAMESPACE