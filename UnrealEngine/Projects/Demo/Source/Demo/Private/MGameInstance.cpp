// Fill out your copyright notice in the Description page of Project Settings.


#include "MGameInstance.h"
#include "GameTablesModule.h"

void UMGameInstance::Init()
{
	Super::Init();
	
}

UMGameSession* UMGameInstance::GetMGameSession()
{
	return nullptr;
}

UGameTables* UMGameInstance::GetGameTables()
{
	return FGameTablesModule::Get().GetGameTables();
}
