// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/EditorDomainSaveCommandlet.h"

#include "EditorDomainSave.h"

UEditorDomainSaveCommandlet::UEditorDomainSaveCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UEditorDomainSaveCommandlet::Main(const FString& CmdLineParams)
{
	FEditorDomainSaveServer Server;
	return Server.Run();
}
