// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXControlConsoleActorFactory.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleActorFactory"

UDMXControlConsoleActorFactory::UDMXControlConsoleActorFactory()
{
	DisplayName = LOCTEXT("DMXControlConsoleActorFactoryDisplayName", "Control Console Actor");
	NewActorClass = ADMXControlConsoleActor::StaticClass();
}

bool UDMXControlConsoleActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid())
	{
		const UClass* AssetClass = AssetData.GetClass();
		if (AssetClass && AssetClass == UDMXControlConsole::StaticClass())
		{
			return true;
		}
	}

	return false;
}

AActor* UDMXControlConsoleActorFactory::SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, const FActorSpawnParameters& InSpawnParams)
{
	const UDMXControlConsole* ControlConsole = Cast<UDMXControlConsole>(Asset);
	if (!ControlConsole)
	{
		return nullptr;
	}

	if (InSpawnParams.ObjectFlags & RF_Transient)
	{
		// This is a hack for drag and drop so that we don't spawn all the actors for the preview actor since it gets deleted right after.
		FActorSpawnParameters SpawnInfo(InSpawnParams);
		SpawnInfo.OverrideLevel = InLevel;
		AStaticMeshActor* DragActor = Cast<AStaticMeshActor>(InLevel->GetWorld()->SpawnActor(AStaticMeshActor::StaticClass(), &Transform, SpawnInfo));
		DragActor->GetStaticMeshComponent()->SetStaticMesh(Cast< UStaticMesh >(FSoftObjectPath(TEXT("StaticMesh'/Engine/EditorMeshes/EditorSphere.EditorSphere'")).TryLoad()));
		DragActor->SetActorScale3D(FVector(0.1f));

		return DragActor;
	}
	else 
	{
		if (!InLevel)
		{
			return nullptr;
		}

		UWorld* World = InLevel->GetWorld();
		if (!World)
		{
			return nullptr;
		}

		ADMXControlConsoleActor* ControlConsoleActor = World->SpawnActor<ADMXControlConsoleActor>(ADMXControlConsoleActor::StaticClass());
		if (ControlConsoleActor)
		{
			ControlConsoleActor->SetDMXControlConsoleData(ControlConsole->GetControlConsoleData());

			return ControlConsoleActor;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
