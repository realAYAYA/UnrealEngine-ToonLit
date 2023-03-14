// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxAssetImportData.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectHash.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Misc/ConfigCacheIni.h"
#include "FbxImporter.h"

UFbxAssetImportData::UFbxAssetImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ImportTranslation(0)
	, ImportRotation(0)
	, ImportUniformScale(1.0f)
	, bConvertScene(true)
	, bForceFrontXAxis(false)
	, bConvertSceneUnit(false)
	, bImportAsScene(false)
	, FbxSceneImportDataReference(nullptr)
{
	
}
