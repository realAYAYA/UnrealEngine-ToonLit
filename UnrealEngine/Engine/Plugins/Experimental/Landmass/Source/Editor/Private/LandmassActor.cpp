// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandmassActor.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandmassActor)


ALandmassActor::ALandmassActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	USceneComponent* SceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = SceneComp;
	SceneComp->Mobility = EComponentMobility::Static;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.SetTickFunctionEnable(true);

	if (!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddUObject(this, &ALandmassActor::HandleActorSelectionChanged);
	}
}

void ALandmassActor::Tick(float DeltaSeconds)
{
	this->CustomTick(DeltaSeconds);
	//UE_LOG(LogTemp, Warning, TEXT("Actor Tick was called"));
}

void ALandmassActor::CustomTick_Implementation(float DeltaSeconds)
{

}

bool ALandmassActor::ShouldTickIfViewportsOnly() const
{
	return EditorTickIsEnabled;
}

void ALandmassActor::HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (!IsTemplate())
	{
		bool bUpdateActor = false;
		if (bWasSelected && !NewSelection.Contains(this))
		{
			bWasSelected = false;
			bUpdateActor = true;
		}
		if (!bWasSelected && NewSelection.Contains(this))
		{
			bWasSelected = true;
			bUpdateActor = true;
		}
		if (bUpdateActor)
		{
			ActorSelectionChanged(bWasSelected);
		}
	}
}

void ALandmassActor::ActorSelectionChanged_Implementation(bool bSelected)
{

}

