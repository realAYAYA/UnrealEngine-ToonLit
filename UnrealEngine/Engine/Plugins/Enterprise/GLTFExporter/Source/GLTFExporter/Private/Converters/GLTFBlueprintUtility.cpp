// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBlueprintUtility.h"
#include "LevelSequenceActor.h"

FString FGLTFBlueprintUtility::GetClassPath(const AActor* Actor)
{
	UClass* Class = Actor->GetClass();
	if (Class != nullptr && Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return Class->GetPathName();
	}

	return TEXT("");
}

bool FGLTFBlueprintUtility::IsSkySphere(const FString& Path)
{
	// TODO: what if a blueprint inherits BP_Sky_Sphere?
	return Path.Equals(TEXT("/Engine/EngineSky/BP_Sky_Sphere.BP_Sky_Sphere_C"));
}

bool FGLTFBlueprintUtility::IsHDRIBackdrop(const FString& Path)
{
	// TODO: what if a blueprint inherits HDRIBackdrop?
	return Path.Equals(TEXT("/HDRIBackdrop/Blueprints/HDRIBackdrop.HDRIBackdrop_C"));
}
