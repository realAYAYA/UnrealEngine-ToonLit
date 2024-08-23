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
		GameTables->AddToRoot();// 保证不会被GC
		GameTables->Init();
	}

	return GameTables;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGameTablesModule, GameTables)

