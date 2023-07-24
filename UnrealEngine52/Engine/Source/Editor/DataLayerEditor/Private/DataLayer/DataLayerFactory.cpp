// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataLayer/DataLayerFactory.h"

#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Templates/SubclassOf.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

class FFeedbackContext;
class UClass;
class UObject;

UDataLayerFactory::UDataLayerFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UDataLayerAsset::StaticClass();
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UObject* UDataLayerFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDataLayerAsset* DataLayerAsset = NewObject<UDataLayerAsset>(InParent, InName, Flags);
	DataLayerAsset->SetDebugColor(FColor::MakeRandomSeededColor(GetTypeHash(DataLayerAsset->GetFullName())));
	return DataLayerAsset;
}