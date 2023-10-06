// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "NiagaraScriptValidationCommandlet.generated.h"

class UNiagaraScript;

UCLASS(config=Editor)
class UNiagaraScriptValidationCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

	virtual int32 Main(const FString& Params) override;

	/** The folder in which the commandlet's output files will be stored */
	FString AuditOutputFolder;

	/** Only assets in this collection will be considered. If this is left blank, no assets will be filtered by collection */
	FString FilterCollection;

	/** Package paths to include */
	TArray<FName> PackagePaths;

private:
	void ProcessNiagaraScripts();
};