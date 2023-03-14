// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "ISourceControlProvider.h"
#include "StateTreeCompileAllCommandlet.generated.h"

class UStateTree;

/**
 * Commandlet to recompile all StateTree assets in the project
 */
UCLASS()
class UStateTreeCompileAllCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UStateTreeCompileAllCommandlet(const FObjectInitializer& ObjectInitializer);

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

protected:
	bool CompileAndSaveStateTree(UStateTree& StateTree) const;

	ISourceControlProvider* SourceControlProvider = nullptr;
};


