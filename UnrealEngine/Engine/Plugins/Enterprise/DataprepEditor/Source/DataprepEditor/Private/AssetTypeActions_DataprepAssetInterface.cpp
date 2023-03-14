// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DataprepAssetInterface.h"

#include "DataprepAsset.h"
#include "DataprepAssetInstance.h"
#include "DataprepAssetProducers.h"
#include "DataprepCoreUtils.h"
#include "DataprepEditorModule.h"
#include "DataprepFactories.h"

#include "Developer/AssetTools/Public/IAssetTools.h"
#include "Developer/AssetTools/Public/AssetToolsModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

// UI
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DataprepAssetInterface"

uint32 FAssetTypeActions_DataprepAssetInterface::GetCategories()
{
	return IDataprepEditorModule::DataprepCategoryBit;
}

FText FAssetTypeActions_DataprepAssetInterface::GetName() const
{
	return LOCTEXT( "Name", "Dataprep Interface" );
}

UClass* FAssetTypeActions_DataprepAssetInterface::GetSupportedClass() const
{
	return UDataprepAssetInterface::StaticClass();
}

void FAssetTypeActions_DataprepAssetInterface::CreateInstance(TArray<TWeakObjectPtr<UDataprepAssetInterface>> DataprepAssetInterfaces)
{
	// Code is inspired from FAssetTypeActions_MaterialInterface::ExecuteNewMIC
	const FString DefaultSuffix = TEXT("_Inst");

	if(DataprepAssetInterfaces.Num() == 1)
	{
		if(UDataprepAsset* DataprepAsset = Cast<UDataprepAsset>(DataprepAssetInterfaces[0].Get()))
		{
			// Determine an appropriate and unique name 
			FString Name;
			FString PackageName;
			CreateUniqueAssetName(DataprepAsset->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

			// Create the factory used to generate the asset
			UDataprepAssetInstanceFactory* Factory = NewObject<UDataprepAssetInstanceFactory>();
			Factory->Parent = DataprepAsset;

			// Create asset in 
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackageName), UDataprepAssetInstance::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> AssetsToSync;
		for(auto AssetIt = DataprepAssetInterfaces.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			if(UDataprepAsset* DataprepAsset = Cast<UDataprepAsset>((*AssetIt).Get()))
			{
				// Determine an appropriate name
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(DataprepAsset->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				UDataprepAssetInstanceFactory* Factory = NewObject<UDataprepAssetInstanceFactory>();
				Factory->Parent = DataprepAsset;

				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
				UObject* NewAsset = AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UDataprepAssetInstance::StaticClass(), Factory);

				if(NewAsset)
				{
					AssetsToSync.Add(NewAsset);
				}
			}
		}

		if(AssetsToSync.Num() > 0)
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, /*bAllowLockedBrowsers=*/true);
		}
	}
}

void FAssetTypeActions_DataprepAssetInterface::ExecuteDataprepAssets(TArray<TWeakObjectPtr<UDataprepAssetInterface>> DataprepAssetInterfaces)
{
	for(TWeakObjectPtr<UDataprepAssetInterface>& DataprepAssetInterfacePtr : DataprepAssetInterfaces)
	{
		if( UDataprepAssetInterface* DataprepAssetInterface = DataprepAssetInterfacePtr.Get() )
		{
			// Nothing to do if the Dataprep asset does not have any inputs
			if(DataprepAssetInterface->GetProducers()->GetProducersCount() > 0)
			{
				FDataprepCoreUtils::ExecuteDataprep( DataprepAssetInterface
					, MakeShared<FDataprepCoreUtils::FDataprepLogger>()
					, MakeShared<FDataprepCoreUtils::FDataprepProgressUIReporter>() );
			}
		}
	}
}

void FAssetTypeActions_DataprepAssetInterface::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto DataprepAssetInterfaces = GetTypedWeakObjectPtrs<UDataprepAssetInterface>(InObjects);

	if(DataprepAssetInterfaces.Num() == 0)
	{
		return;
	}

	// #ueent_remark: An instance of an instance is not supported for 4.24.
	// Do not expose 'Create Instance' menu entry if at least one Dataprep asset is an instance
	bool bContainsAnInstance  = false;
	for (UObject* Object : InObjects)
	{
		if (Object && Object->GetClass() == UDataprepAssetInstance::StaticClass())
		{
			bContainsAnInstance = true;
			break;
		}
	}

	// Disable execute if any of the input assets has no consumer
	bool bCanExecute = true;
	for (UObject* Object : InObjects)
	{
		if (UDataprepAssetInterface* DataprepAsset = Cast<UDataprepAssetInterface>(Object))
		{
			if (nullptr == DataprepAsset->GetConsumer())
			{
				bCanExecute = false;
				break;
			}
		}
	}

	if (!bContainsAnInstance)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateInstance", "Create Instance"),
			LOCTEXT("CreateInstanceTooltip", "Creates a parameterized Dataprep asset using this Dataprep asset as a base."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_DataprepAssetInterface::CreateInstance, DataprepAssetInterfaces),
				FCanExecuteAction()
			)
		);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("RunAsset", "Execute"),
		LOCTEXT("RunAssetTooltip", "Runs the Dataprep asset's producers, execute its recipe, finally runs the consumer"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_DataprepAssetInterface::ExecuteDataprepAssets, DataprepAssetInterfaces),
			FCanExecuteAction::CreateLambda([bCanExecute](){ return bCanExecute; })
		)
	);
}

#undef LOCTEXT_NAMESPACE