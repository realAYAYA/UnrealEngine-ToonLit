// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomFactory.h"
#include "GroomAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GroomFactory)

UGroomFactory::UGroomFactory()
{
	SupportedClass = UGroomAsset::StaticClass();

	bCreateNew = true;
	bEditAfterNew = true;
	bText = false;
	bEditorImport = true;
}

UObject* UGroomFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UGroomAsset* Groom = NewObject<UGroomAsset>(InParent, InName, Flags | RF_Transactional);

	// Groom has to have at least one group to edit with one cards LOD
	Groom->SetNumGroup(1);
	Groom->HairGroupsLOD[0].LODs[0].GeometryType = EGroomGeometryType::Cards;
	Groom->HairGroupsInterpolation[0].InterpolationSettings.bUseUniqueGuide = true;

	Groom->UpdateHairGroupsInfo();
	Groom->UpdateCachedSettings();
	Groom->InitResources();

	return Groom;
}

