// Fill out your copyright notice in the Description page of Project Settings.


#include "MGameInstance.h"
#include "GameTablesModule.h"
#include "Demo/Net/MGameClient.h"

void UMGameInstance::Init()
{
	Super::Init();
	
}

UMGameSession* UMGameInstance::GetMGameSession()
{
	if (!GameSession)
		GameSession = NewObject<UMGameSession>();
	
	return GameSession;
}

UGameTables* UMGameInstance::GetGameTables()
{
	return FGameTablesModule::Get().GetGameTables();
}
