// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DatasmithAssetImportData.h"
#include "DatasmithDeltaGenImportData.h"
#include "DatasmithDeltaGenLog.h"

struct FDeltaGenVarDataVariantSwitch;

class FDatasmithDeltaGenImportVariantsResult
{
public:
	TArray<FDeltaGenVarDataVariantSwitch> VariantSwitches;
	TSet<FName> SwitchObjects;
	TSet<FName> ToggleObjects;
	TSet<FName> ObjectSetObjects;
};

class FDatasmithDeltaGenImportTmlResult
{
public:
	TArray<FDeltaGenTmlDataTimeline> Timelines;
	TSet<FName> AnimatedObjects;
};

class FDatasmithDeltaGenImportPosResult
{
public:
	TArray<FDeltaGenPosDataState> PosStates;
	TSet<FName> StateObjects;
	TSet<FName> SwitchObjects;
	TSet<FName> SwitchMaterialObjects;
};

class FDatasmithDeltaGenAuxFiles
{
public:
	static FDatasmithDeltaGenImportVariantsResult ParseVarFile(const FString& InFilePath);
	static FDatasmithDeltaGenImportPosResult ParsePosFile(const FString& InFilePath);
	static FDatasmithDeltaGenImportTmlResult ParseTmlFile(const FString& InFilePath);
};


