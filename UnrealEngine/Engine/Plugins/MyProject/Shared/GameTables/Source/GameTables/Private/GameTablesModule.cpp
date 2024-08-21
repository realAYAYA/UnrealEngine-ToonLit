#include "GameTablesModule.h"

#include "GameTables.h"

#define LOCTEXT_NAMESPACE "FGameTablesModule"

void FGameTablesModule::StartupModule()
{
}

void FGameTablesModule::ShutdownModule()
{
}

UGameTables* FGameTablesModule::GetGameTables()
{
	if (!GameTables)
	{
		GameTables = NewObject<UGameTables>();
		GameTables->Init();
	}

	return GameTables;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGameTablesModule, GameTables)

