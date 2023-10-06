// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "EditorDomainSaveCommandlet.generated.h"

/** Runs the EditorDomainSave server for client Editor and CookCommandlet processes. */
UCLASS(config = Editor)
class UEditorDomainSaveCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& CmdLineParams) override;
	//~ End UCommandlet Interface
};

