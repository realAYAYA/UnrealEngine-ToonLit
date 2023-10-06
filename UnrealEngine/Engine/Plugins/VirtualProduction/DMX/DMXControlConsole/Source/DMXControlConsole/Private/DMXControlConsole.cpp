// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsole.h"

#include "DMXControlConsoleData.h"
#include "Library/DMXLibrary.h"


#define LOCTEXT_NAMESPACE "DMXControlConsole"

UDMXControlConsole::UDMXControlConsole()
{
	ControlConsoleData = CreateDefaultSubobject<UDMXControlConsoleData>(TEXT("ControlConsoleData"));
}

void UDMXControlConsole::CopyControlConsoleData(UDMXControlConsoleData* InControlConsoleData)
{
	if (!InControlConsoleData)
	{
		return;
	}

	// Unsaved Control Consoles live in the Transient Package
	if (InControlConsoleData->GetPackage() == GetTransientPackage())
	{
		ControlConsoleData = InControlConsoleData;
	}
	else
	{
		ControlConsoleData = DuplicateObject<UDMXControlConsoleData>(InControlConsoleData, this);
	}

	ControlConsoleData->Rename(nullptr, this);
	ControlConsoleData->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
}

void UDMXControlConsole::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsSaving() && Ar.IsPersistent())
	{
		OnControlConsoleSaved.Broadcast(this);
	}
#endif // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
