// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithMVRNativeTranslator.h"

#include "DatasmithMVRImportOptions.h"
#include "Factories/DMXLibraryFromMVRFactory.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "MVR/Types/DMXMVRFixtureNode.h"

#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DatasmithNativeTranslator.h"
#include "DatasmithSceneFactory.h"
#include "IDatasmithSceneElements.h"
#include "Subsystems/ImportSubsystem.h"


DECLARE_LOG_CATEGORY_CLASS(LogDatasmithMVRNativeTranslator, Log, All);

bool FDatasmithMVRNativeTranslator::LoadScene(TSharedRef<IDatasmithScene> InOutScene)
{
	const bool bNativeTranslatorSuccess = FDatasmithNativeTranslator::LoadScene(InOutScene);
	if (!bNativeTranslatorSuccess)
	{
		return false;
	}

	if (ImportOptions.IsValid() && !ImportOptions->bImportMVR)
	{
		// Importing MVR is turned off in options. Acknowledge the scene was successfully imported by the native translator.
		return true;
	}

	FString MVRFilePathAndName;
	if (!FindMVRFile(MVRFilePathAndName))
	{
		// If there is no occurence of MVR, acknowledge the scene was successfully imported by the native translator.
		return true;
	}

	InOutScene->SetHost(TEXT("DatasmithMVRNativeTranslator"));
	InOutScene->SetProductName(TEXT("DatasmithMVRNativeTranslator"));

	UDMXLibrary* DMXLibrary = CreateDMXLibraryFromMVR(MVRFilePathAndName);
	if (!DMXLibrary)
	{
		const FString MVRFilename = FPaths::GetCleanFilename(MVRFilePathAndName);
		UE_LOG(LogDatasmithMVRNativeTranslator, Warning, TEXT("Failed to create DMX Library for %s. See previous errors for more info. Creating native datasmith scene instead."), *MVRFilename);
		return true;
	}

	ReplaceMVRActorsWithMVRSceneActor(InOutScene, DMXLibrary);

	return true;
}

void FDatasmithMVRNativeTranslator::GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	FDatasmithNativeTranslator::GetSceneImportOptions(Options);

	if (!ImportOptions.IsValid())
	{
		const FString& FilePath = GetSource().GetSourceFile();

		ImportOptions = Datasmith::MakeOptions<UDatasmithMVRImportOptions>();
	}

	Options.Add(ImportOptions.Get());
}

void FDatasmithMVRNativeTranslator::SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options)
{
	FDatasmithNativeTranslator::SetSceneImportOptions(Options);

	for (const TObjectPtr<UDatasmithOptionsBase>& OptionPtr : Options)
	{
		if (UDatasmithMVRImportOptions* InImportOptions = Cast<UDatasmithMVRImportOptions>(OptionPtr))
		{
			ImportOptions.Reset(InImportOptions);
		}
	}
}

bool FDatasmithMVRNativeTranslator::FindMVRFile(FString& OutMVRFilePathAndName) const
{
	const FString DatasmithFilePathAndName = GetSource().GetSourceFile();
	const FString DatasmithFileName = FPaths::GetBaseFilename(DatasmithFilePathAndName);
	const FString DatasmithPath = FPaths::GetPath(DatasmithFilePathAndName);
	const FString DatasmithAssetsPath = FPaths::GetPath(DatasmithFilePathAndName) / DatasmithFileName + TEXT("_Assets");

	const FString MVRFilename = FPaths::GetBaseFilename(DatasmithFilePathAndName) + TEXT(".mvr");

	if (FPaths::FileExists(DatasmithPath / MVRFilename))
	{
		OutMVRFilePathAndName = DatasmithPath / MVRFilename;
		return true;
	}
	else if (FPaths::FileExists(DatasmithAssetsPath / MVRFilename))
	{
		OutMVRFilePathAndName = DatasmithAssetsPath / MVRFilename;
		return true;;
	}

	return false;
}

UDMXLibrary* FDatasmithMVRNativeTranslator::CreateDMXLibraryFromMVR(const FString& MVRFilePathAndName) const
{
	if (!ensureMsgf(FPaths::FileExists(MVRFilePathAndName), TEXT("Tried to create a DMX Library from MVR, but the MVR file doesn't exist anymore.")))
	{
		return nullptr;
	}

	UDMXLibraryFromMVRFactory* DMXLibraryFromMVRFactory = NewObject<UDMXLibraryFromMVRFactory>();

	const FString SceneName = GetSource().GetSceneName();
	const FString DMXLibraryName = FPaths::GetBaseFilename(MVRFilePathAndName);
	const FString PackageName = TEXT("/Game") / SceneName;
	UPackage* DMXLibraryAssetPackage = CreatePackage(*PackageName);
	DMXLibraryAssetPackage->FullyLoad();

	bool bOperationCanceled;
	UObject* NewDMXLibraryObject = DMXLibraryFromMVRFactory->FactoryCreateFile(UDMXLibrary::StaticClass(), DMXLibraryAssetPackage, *DMXLibraryName, RF_Public | RF_Standalone | RF_Transactional, MVRFilePathAndName, nullptr, GWarn, bOperationCanceled);
	if (bOperationCanceled)
	{
		return nullptr;
	}

	if (UDMXLibrary* DMXLibrary = Cast<UDMXLibrary>(NewDMXLibraryObject))
	{
		FAssetRegistryModule::AssetCreated(DMXLibrary);
		DMXLibrary->MarkPackageDirty();

		return DMXLibrary;
	}

	return nullptr;
}

void FDatasmithMVRNativeTranslator::ReplaceMVRActorsWithMVRSceneActor(TSharedRef<IDatasmithScene>& InOutScene, UDMXLibrary* DMXLibrary) const
{
	if (!ensureMsgf(DMXLibrary, TEXT("Tried to create an MVR scene in the datasmith scene, but DMXLibrary is null.")))
	{
		return;
	}

	// Create the custom MVR Scene Actor element
	static const FString MVRSceneActorName = TEXT("MVR Scene Actor");
	static const FString MVRSceneActorPath = TEXT("/Script/DMXRuntime.DMXMVRSceneActor");

	const TSharedRef<IDatasmithCustomActorElement> CustomMVRSceneActorElement = FDatasmithSceneFactory::CreateCustomActor(*MVRSceneActorName);
	InOutScene->AddActor(CustomMVRSceneActorElement);
	CustomMVRSceneActorElement->SetClassOrPathName(*MVRSceneActorPath);
	CustomMVRSceneActorElement->SetIsAComponent(false);
	CustomMVRSceneActorElement->SetRotation(FQuat::Identity);
	CustomMVRSceneActorElement->SetScale(FVector::OneVector);
	CustomMVRSceneActorElement->SetTranslation(FVector::ZeroVector);

	// Set the DMX Library path as metadata of the custom actor element
	const TSoftObjectPtr<UDMXEntityFixturePatch> SoftDMXLibraryObject = DMXLibrary;
	const TSharedRef<IDatasmithKeyValueProperty> FixturePatchKeyValueProperty = FDatasmithSceneFactory::CreateKeyValueProperty(TEXT("DMXLibraryPath"));
	FixturePatchKeyValueProperty->SetValue(*SoftDMXLibraryObject.ToString());

	const TSharedRef<IDatasmithMetaDataElement> CustomActorMetaDataElement = FDatasmithSceneFactory::CreateMetaData(CustomMVRSceneActorElement->GetName());
	CustomActorMetaDataElement->AddProperty(FixturePatchKeyValueProperty);
	CustomActorMetaDataElement->SetAssociatedElement(CustomMVRSceneActorElement);

	InOutScene->AddMetaData(CustomActorMetaDataElement);

	// Remove native datasmith elements that correspond to an MVR Fixtures
	TArray<TSharedRef<IDatasmithMetaDataElement>> MetaDataElementsToRemove;
	TArray<TSharedRef<IDatasmithActorElement>> ActorElementsToRemove;

	const int32 NumMetaDatas = InOutScene->GetMetaDataCount();

	DMXLibrary->UpdateGeneralSceneDescription();
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = DMXLibrary->GetLazyGeneralSceneDescription();
	if (!ensureAlwaysMsgf(GeneralSceneDescription, TEXT("Trying to remove MVR scene elements from datasmith scene, but the DMX Library that was created for it does not provide a General Scene Description.")))
	{
		return;
	}

	TArray<UDMXMVRFixtureNode*> FixtureNodes;
	GeneralSceneDescription->GetFixtureNodes(FixtureNodes);
	for (const UDMXMVRFixtureNode* MVRFixture : FixtureNodes)
	{
		for (int32 MetaDataIndex = 0; MetaDataIndex < NumMetaDatas; MetaDataIndex++)
		{
			const TSharedPtr<IDatasmithMetaDataElement> MetaDataElement = InOutScene->GetMetaData(MetaDataIndex);

			// Try to find an MVR ID in MetaData. Note Vectorworks uses VW_UUID as that, so we search for that too.
			static const FString Generic_MVRUUIDName = TEXT("MVR_UUID");
			static const FString VW_MVRUUIDName = TEXT("VW_UUID");

			const TSharedPtr<IDatasmithKeyValueProperty> DatasmithKeyValueProperty = [&MetaDataElement]()
			{
				TSharedPtr<IDatasmithKeyValueProperty> Result = MetaDataElement->GetPropertyByName(*Generic_MVRUUIDName);
				if (!Result.IsValid())
				{
					Result = MetaDataElement->GetPropertyByName(*VW_MVRUUIDName);
				}

				return Result;
			}();

			if (!DatasmithKeyValueProperty.IsValid())
			{
				continue;
			}

			// Try to find the node that corresponds to the MVR node
			const FString MetaDataMVRUUIDString = FString(DatasmithKeyValueProperty->GetValue());
			FGuid MetaDataMVRUUID;
			if (!FGuid::Parse(MetaDataMVRUUIDString, MetaDataMVRUUID))
			{
				continue;
			}

			if (MetaDataMVRUUID == MVRFixture->UUID)
			{
				const TSharedPtr<IDatasmithElement> AssociatedElementToRemove = MetaDataElement->GetAssociatedElement();

				const bool bValidElementType = AssociatedElementToRemove.IsValid() || AssociatedElementToRemove->IsA(EDatasmithElementType::Actor);
				if (!ensureAlwaysMsgf(bValidElementType, TEXT("DatasmithMVRNativeTranslator unexpected Element type when trying to remove MVR element in favor of an MVR Scene Actor."), AssociatedElementToRemove->GetName()))
				{
					continue;
				}

				const TSharedRef<IDatasmithActorElement> ActorElementToRemove = StaticCastSharedRef<IDatasmithActorElement>(AssociatedElementToRemove.ToSharedRef());
				MetaDataElementsToRemove.Add(MetaDataElement.ToSharedRef());
				ActorElementsToRemove.Add(ActorElementToRemove);
			}
		}
	}

	for (const TSharedRef<IDatasmithMetaDataElement>& MetaDataElementToRemove : MetaDataElementsToRemove)
	{
		InOutScene->RemoveMetaData(MetaDataElementToRemove);
	}
	for (const TSharedRef<IDatasmithActorElement>& ActorElementToRemove : ActorElementsToRemove)
	{
		InOutScene->RemoveActor(ActorElementToRemove, EDatasmithActorRemovalRule::RemoveChildren);
	}
}
