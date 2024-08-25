// Copyright Epic Games, Inc. All Rights Reserved.

#include "EngineUtils.h"
#include "Engine/World.h"

#include "Editor.h"
#include "Editor/UnrealEd/Public/PackageTools.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include "PCGVolume.h"
#include "PCGComponent.h"
#include "Tests/PCGTestsCommon.h"
#include "Tests/AutomationEditorCommon.h"


#if WITH_AUTOMATION_TESTS

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGComponentInNewPCGVolumes, FPCGTestBaseClass, "Plugins.PCG.Display.PCGComponentInNewPCGVolumes", PCGTestsCommon::TestFlags)

bool FPCGComponentInNewPCGVolumes::RunTest(const FString& Parameters)
{
	UTEST_NOT_NULL(TEXT("Failed to get editor!"), GEditor);
	// Condition for static analysis.
	check(GEditor);

	// Get current world
	UWorld* World = GEditor->GetEditorWorldContext().World();
	UTEST_NOT_NULL(TEXT("Failed to get editor world context!"), World);

	// Condition for static analysis.
	check(World);

	TSubclassOf<APCGVolume> PCGVolumeClass = APCGVolume::StaticClass();
	UActorFactory* PCGVolumeFactory = GEditor->FindActorFactoryForActorClass(PCGVolumeClass);

	UTEST_NOT_NULL(TEXT("Failed to find PCGVolume actor factory."), PCGVolumeFactory);
	check(PCGVolumeFactory);

	APCGVolume* VolumeActor = nullptr;

	if (GCurrentLevelEditingViewportClient)
	{
		FTransform ActorTransform;
		VolumeActor = Cast<APCGVolume>(GEditor->UseActorFactory(PCGVolumeFactory, FAssetData(PCGVolumeClass), &ActorTransform));
	}

	UTEST_NOT_NULL(TEXT("Failed to add PCGVolume actor."), VolumeActor);
	// Condition for static analysis.
	check(VolumeActor);

	VolumeActor->SetFlags(RF_Transient);

	UPCGComponent* PCGComponent = VolumeActor->FindComponentByClass<UPCGComponent>();
	UTEST_NOT_NULL(TEXT("PCGVolume actor does not contain a PCGComponent!"), PCGComponent);
	// Condition for static analysis.
	check(PCGComponent);

	PCGComponent->ClearPCGLink(APCGVolume::StaticClass());

	// Cleanup the VolumeActor
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	FAssetData PCGVolumeAssetData(VolumeActor);
	AssetRegistry.AssetDeleted(PCGVolumeAssetData.GetAsset());

	bool bSuccessful = ObjectTools::DeleteSingleObject(VolumeActor, true);
	if (!bSuccessful)
	{
		//Clear references to the object so we can delete it
		FAutomationEditorCommonUtils::NullReferencesToObject(VolumeActor);
		bSuccessful = ObjectTools::DeleteSingleObject(VolumeActor, false);
		TestTrue(TEXT("References to PCGVolume component could not be cleaned"), bSuccessful);
	}

	return true;
};

#endif