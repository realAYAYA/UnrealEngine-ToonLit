// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithVREDImportData.h"

struct FDatasmithFBXSceneAnimClip;
struct FDatasmithFBXSceneAnimNode;
struct FDatasmithFBXSceneLight;
struct FDatasmithFBXSceneMaterial;
struct FVREDCppVariant;

class FDatasmithVREDImportMatsResult
{
public:
	TArray<FDatasmithFBXSceneMaterial> Mats;
};

class FDatasmithVREDImportVariantsResult
{
public:
	TArray<FVREDCppVariant> VariantSwitches;
	TSet<FString> SwitchObjects;
	TSet<FString> SwitchMaterialObjects;
	TSet<FString> TransformVariantObjects;
};

class FDatasmithVREDImportLightsResult
{
public:
	TArray<FDatasmithFBXSceneLight> Lights;
};

class FDatasmithVREDImportClipsResult
{
public:
	TArray<FDatasmithFBXSceneAnimNode> AnimNodes;
	TArray<FDatasmithFBXSceneAnimClip> AnimClips;
	float KeyTime = FLT_MAX;
	float BaseTime = 24.0f;
	float PlaybackSpeed = 24.0f;
};

namespace FDatasmithVREDAuxFiles
{
	FDatasmithVREDImportMatsResult ParseMatsFile(const FString& InFilePath);
	FDatasmithVREDImportVariantsResult ParseVarFile(const FString& InFilePath);
	FDatasmithVREDImportLightsResult ParseLightsFile(const FString& InFilePath);
	FDatasmithVREDImportClipsResult ParseClipsFile(const FString& InFilePath);
};
