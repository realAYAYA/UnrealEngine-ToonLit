// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class AUsdStageActor;
class UPackage;
class UUsdStageImportOptions;

class USDSTAGEEDITORVIEWMODELS_API FUsdStageViewModel
{
public:
	void NewStage();
	void OpenStage(const TCHAR* FilePath);
	void ReloadStage();
	void ResetStage();
	void CloseStage();
	void SaveStage();
	/** Temporary until SaveAs feature is properly implemented, may be removed in a future release */
	void SaveStageAs(const TCHAR* FilePath);
	void ImportStage(const TCHAR* TargetContentFolder = nullptr, UUsdStageImportOptions* Options = nullptr);

public:
	TWeakObjectPtr<AUsdStageActor> UsdStageActor;
};
