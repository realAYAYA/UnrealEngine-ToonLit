// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureActionConvertContentBundleWorldPartitionBuilder.h"
#include "GameFeatureAction_AddWorldPartitionContent.h"
#include "GameFeatureAction_AddWPContent.h"
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleClient.h"
#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerInstance.h"
#include "WorldPartition/DataLayer/ExternalDataLayerManager.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "DataLayer/ExternalDataLayerFactory.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/MetaData.h"
#include "Commandlets/Commandlet.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureData.h"
#include "PackageSourceControlHelper.h"
#include "SourceControlHelpers.h"
#include "Misc/PathViews.h"
#include "AssetSelection.h"
#include "PackageTools.h"
#include "IAssetTools.h"
#include "Algo/Find.h"

DEFINE_LOG_CATEGORY_STATIC(LogConvertContentBundleBuilder, All, All);

UGameFeatureActionConvertContentBundleWorldPartitionBuilder::UGameFeatureActionConvertContentBundleWorldPartitionBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bReportOnly(false)
	, bSkipDelete(false)
{}

bool UGameFeatureActionConvertContentBundleWorldPartitionBuilder::PreRun(UWorld* World, FPackageSourceControlHelper& PackageHelper)
{
	TArray<FString> Tokens, Switches;
	TMap<FString, FString> CommandLineParams;
	UCommandlet::ParseCommandLine(*GetBuilderArgs(), Tokens, Switches, CommandLineParams);

	FString const* DestFolderPtr = CommandLineParams.Find(TEXT("DestinationFolder"));
	DestinationFolder = DestFolderPtr ? *DestFolderPtr : FString();
	bReportOnly = Switches.Contains(TEXT("ReportOnly"));
	bSkipDelete = Switches.Contains(TEXT("SkipDelete"));

	if (FString const* ContentBundlesToConvertString = CommandLineParams.Find(TEXT("ContentBundles")))
	{
		if ((*ContentBundlesToConvertString).ParseIntoArray(ContentBundlesToConvert, TEXT("+")) == 0)
		{
			UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to parse ContentBundles argument '%s'."), **ContentBundlesToConvertString);
			return false;
		}
	}

	return true;
}

bool UGameFeatureActionConvertContentBundleWorldPartitionBuilder::RunInternal(UWorld* World, const FCellInfo& InCellInfo, FPackageSourceControlHelper& PackageHelper)
{
	TStrongObjectPtr<UExternalDataLayerFactory> ExternalDataLayerFactory(NewObject<UExternalDataLayerFactory>(GetTransientPackage()));

	UContentBundleManager* ContentBundleManager = World->ContentBundleManager;
	if (!ContentBundleManager)
	{
		UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("World %s does not have content bundles."), *World->GetName());
		return false;
	}

	TArray<TSharedPtr<FContentBundleEditor>> ContentBundles;
	if (!ContentBundleManager->GetEditorContentBundle(ContentBundles))
	{
		UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("World %s does not have content bundles."), *World->GetName());
		return false;
	}

	UExternalDataLayerManager* ExternalDataLayerManager = UExternalDataLayerManager::GetExternalDataLayerManager(World);
	if (!ExternalDataLayerManager)
	{
		UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("World %s does not have an ExternalDataLayerManager"), *World->GetName());
		return false;
	}

	UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Found %d Content bundles in World %s"), ContentBundles.Num(), *World->GetName());

	TArray<TSharedPtr<FContentBundleEditor>> ContentBundlesToProcess;
	if (!ContentBundlesToConvert.IsEmpty())
	{
		for (TSharedPtr<FContentBundleEditor>& ContentBundle : ContentBundles)
		{
			if (!ContentBundlesToConvert.Contains(ContentBundle->GetDisplayName()))
			{
				UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Skipping %s: Not in the list of content bundles to convert."), *ContentBundle->GetDisplayName());
				continue;
			}
			ContentBundlesToProcess.Add(ContentBundle);
		}
	}
	else
	{
		ContentBundlesToProcess = ContentBundles;
	}

	bool bIsSuccess = true;
	for (TSharedPtr<FContentBundleEditor>& ContentBundle : ContentBundlesToProcess)
	{
		TSet<UPackage*> PackagesToSave;
		TSet<FString> PackagesToDelete;

		TSharedPtr<FContentBundleClient> ContentBundleClient = ContentBundle->GetClient().Pin();
		if (!ContentBundleClient.IsValid())
		{
			UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to access Client of Content Bundle %s"), *ContentBundle->GetDisplayName());
			bIsSuccess = false;
			continue;
		}

		if (ContentBundleClient->GetState() == EContentBundleClientState::Registered)
		{
			if (const FContentBundleBase* CB = ContentBundleManager->GetContentBundle(World, ContentBundle->GetDescriptor()->GetGuid()))
			{
				if (CB->GetStatus() != EContentBundleStatus::ContentInjected)
				{
					UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Requesting forced injection of Content Bundle %s for conversion purposes."), *ContentBundle->GetDisplayName());
					ContentBundleClient->RequestContentInjection();
				}
			}
		}

		if (const FContentBundleBase* CB = ContentBundleManager->GetContentBundle(World, ContentBundle->GetDescriptor()->GetGuid()))
		{
			if (CB->GetStatus() != EContentBundleStatus::ContentInjected)
			{
				UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Skipping %s: Not injected in this world."), *ContentBundle->GetDisplayName());
				continue;
			}
		}

		UActorDescContainerInstance* ContentBundleContainerInstance = ContentBundle->GetActorDescContainerInstance().Get();
		const UContentBundleDescriptor* ContentBundleDescriptor = ContentBundle->GetDescriptor();
		UGameFeatureAction_AddWPContent* OldAddWPContentAction = ContentBundleDescriptor->GetTypedOuter<UGameFeatureAction_AddWPContent>();
		UGameFeatureData* GameFeatureData = ContentBundleDescriptor->GetTypedOuter<UGameFeatureData>();
		if (!OldAddWPContentAction || !GameFeatureData || !ContentBundleContainerInstance)
		{
			UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Skipping invalid Content Bundle %s"), *ContentBundle->GetDisplayName());
			bIsSuccess = false;
			continue;
		}

		if (ContentBundleContainerInstance->IsEmpty())
		{
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Skipping conversion : Empty Content Bundle %s"), *ContentBundle->GetDisplayName());
			SkippedEmptyContentBundles.Add(ContentBundle);
			continue;
		}

		// Find or create EDL Asset for this Content Bundle
		UExternalDataLayerAsset* ExternalDataLayerAsset = GetOrCreateExternalDataLayerAsset(ContentBundleDescriptor, ExternalDataLayerFactory.Get(), PackagesToSave);
		UE_CLOG(ExternalDataLayerAsset, LogConvertContentBundleBuilder, Log, TEXT("Converting Content Bundle %s using External Data Layer Asset %s."), *ContentBundle->GetDisplayName(), *ExternalDataLayerAsset->GetPathName());
		if (!ExternalDataLayerAsset)
		{
			UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to retrieve or create External Data Layer Asset for Content Bundle %s"), *ContentBundle->GetDisplayName());
			bIsSuccess = false;
			continue;
		}

		// Allow injection of External Data Layer (only used by this builder)
		UExternalDataLayerEngineSubsystem::FForcedExternalDataLayerInjectionKey ForcedInjectionKey(World, ExternalDataLayerAsset);
		UExternalDataLayerEngineSubsystem::Get().ForcedAllowInjection.Add(ForcedInjectionKey);
		ON_SCOPE_EXIT { UExternalDataLayerEngineSubsystem::Get().ForcedAllowInjection.Remove(ForcedInjectionKey); };

		// Find existing GameFeatureAction_AddWorldPartitionContent for this EDL Asset
		UGameFeatureAction* const* ExistingAddWorldPartitionContent = Algo::FindByPredicate(GameFeatureData->GetActions(), [ExternalDataLayerAsset](UGameFeatureAction* Action) { return Action && Action->IsA<UGameFeatureAction_AddWorldPartitionContent>() && Cast<UGameFeatureAction_AddWorldPartitionContent>(Action)->GetExternalDataLayerAsset() == ExternalDataLayerAsset; });
		UGameFeatureAction_AddWorldPartitionContent* AddWorldPartitionContent = ExistingAddWorldPartitionContent ? Cast<UGameFeatureAction_AddWorldPartitionContent>(*ExistingAddWorldPartitionContent) : nullptr;
		if (!AddWorldPartitionContent)
		{
			// Create new GameFeatureAction_AddWorldPartitionContent for this EDL Asset
			AddWorldPartitionContent = NewObject<UGameFeatureAction_AddWorldPartitionContent>(GameFeatureData);
			GameFeatureData->GetMutableActionsInEditor().Add(AddWorldPartitionContent);
			AddWorldPartitionContent->ExternalDataLayerAsset = ExternalDataLayerAsset;
			PackagesToSave.Add(GameFeatureData->GetPackage());
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Added new Action of type 'GameFeatureAction_AddWorldPartitionContent' to GameFeatureData %s using External Data Layer Asset %s while converting Content Bundle %s."), *GameFeatureData->GetName(), *ExternalDataLayerAsset->GetPathName(), *ContentBundle->GetDisplayName());
		}
		
		// Manually call OnExternalDataLayerAssetChanged to register the newly created GameFeatureAction_AddWorldPartitionContent
		AddWorldPartitionContent->OnExternalDataLayerAssetChanged(nullptr, ExternalDataLayerAsset);

		// First try to find existing ExternalDataLayerInstance
		UExternalDataLayerInstance* ExternalDataLayerInstance = ExternalDataLayerManager->GetExternalDataLayerInstance(ExternalDataLayerAsset);
		if (!ExternalDataLayerInstance)
		{
			// Create AWorldDataLayers and the ExternalDataLayerInstance for this EDL Asset
			FDataLayerCreationParameters CreationParams;
			CreationParams.DataLayerAsset = ExternalDataLayerAsset;
			CreationParams.WorldDataLayers = World->GetWorldDataLayers();
			ExternalDataLayerInstance = Cast<UExternalDataLayerInstance>(UDataLayerEditorSubsystem::Get()->CreateDataLayerInstance(CreationParams));
			UE_CLOG(ExternalDataLayerInstance, LogConvertContentBundleBuilder, Log, TEXT("Create External Data Layer Instance %s while converting Content Bundle %s."), *ExternalDataLayerAsset->GetPathName(), *ContentBundle->GetDisplayName());
			if (!ExternalDataLayerInstance)
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to create External Data Layer Instance for External Data Layer Asset %s while converting Content Bundle %s"), *ExternalDataLayerAsset->GetName(), *ContentBundle->GetDisplayName());
				bIsSuccess = false;
				continue;
			}
			check(!ExternalDataLayerInstance->IsPackageExternal());
		}

		AWorldDataLayers* EDLWorldDataLayers = ExternalDataLayerInstance->GetDirectOuterWorldDataLayers();
		if (!EDLWorldDataLayers)
		{
			UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to find a valid WorldDataLayers actor for External Data Layer Asset %s while converting Content Bundle %s"), *ExternalDataLayerAsset->GetName(), *ContentBundle->GetDisplayName());
			bIsSuccess = false;
			continue;
		}

		// By default, newly create AWorldDataLayers will be marked as read-only which would block assignation of actors to the External Data Layer
		// Temporarily remove package flag
		bool bIsNewEDLWorldDataLayers = EDLWorldDataLayers->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated);
		if (bIsNewEDLWorldDataLayers)
		{
			EDLWorldDataLayers->GetPackage()->ClearPackageFlags(PKG_NewlyCreated);
			PackagesToSave.Add(EDLWorldDataLayers->GetPackage());
		}

		TArray<FGuid> ConvertedActorGuids;
		TArray<FWorldPartitionReference> ActorReferences;
		bool bContentBundleActorConversionSuccess = true;
		// Convert actors from Content Bundle to EDL
		for (UActorDescContainerInstance::TIterator<> It(ContentBundleContainerInstance); It; ++It)
		{
			FWorldPartitionReference ActorRef(ContentBundleContainerInstance, It->GetGuid());
			if (!ActorRef.IsValid())
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to load actor %s(%s). Actor won't be converted."), *It->GetActorLabelOrName().ToString(), *It->GetActorPackage().ToString());
				bContentBundleActorConversionSuccess = false;
				break;
			}

			ActorReferences.Add(ActorRef);
			AActor* Actor = ActorRef.GetActor();
			UPackage* OldActorPackage = Actor->GetExternalPackage();
			const FString OldActorPackageName = OldActorPackage->GetName();
			if (!bSkipDelete)
			{
				PackagesToDelete.Add(OldActorPackageName);
			}

			// Clear Actor CB Guid
			FSetActorContentBundleGuid(Actor, FGuid());

			// Verify that the actor can be assigned to the External Data Layer
			FText FailureReason;
			if (!ExternalDataLayerInstance->CanAddActor(Actor, &FailureReason))
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Can't create package for actor %s. %s"), *ExternalDataLayerAsset->GetName(), *Actor->GetActorNameOrLabel(), *FailureReason.ToString());
				bContentBundleActorConversionSuccess = false;
				break;
			}

			// Remove actor from it's old package
			Actor->SetPackageExternal(false);

			// Get all other dependant objects in the old actor package
			TArray<UObject*> DependantObjects;
			ForEachObjectWithPackage(OldActorPackage, [&DependantObjects](UObject* Object)
			{
				if (!Cast<UMetaData>(Object))
				{
					DependantObjects.Add(Object);
				}
				return true;
			}, false);

			// Create a new external package and assign it to the actor
			ULevel* DestinationLevel = Actor->GetLevel();
			UPackage* NewActorPackage = ULevel::CreateActorPackage(DestinationLevel->GetPackage(), DestinationLevel->GetActorPackagingScheme(), Actor->GetName(), ExternalDataLayerAsset);
			Actor->SetPackageExternal(true, true, NewActorPackage);

			// Validation
			check(NewActorPackage == Actor->GetExternalPackage());
			check(NewActorPackage->GetName() == ExternalDataLayerManager->GetActorPackageName(ExternalDataLayerAsset, DestinationLevel, Actor->GetPathName()));
			check(NewActorPackage->GetName() != OldActorPackageName);

			// Move dependant objects into the new actor package
			for (UObject* DependantObject : DependantObjects)
			{
				DependantObject->Rename(nullptr, NewActorPackage, REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_DoNotDirty);
			}
			
			// Set External Data Layer Asset
			if (!UDataLayerEditorSubsystem::Get()->AddActorToDataLayer(Actor, ExternalDataLayerInstance))
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to add actor %s(%s) to external data layer %s. Actor won't be converted."), *It->GetActorLabelOrName().ToString(), *It->GetActorPackage().ToString(), *ExternalDataLayerAsset->GetName());
				bContentBundleActorConversionSuccess = false;
				break;
			}

			check(Actor->GetExternalDataLayerAsset() == ExternalDataLayerAsset);
			check(!Actor->GetContentBundleGuid().IsValid());
			UPackage* NewPackage = Actor->GetPackage();
			PackagesToSave.Add(NewPackage);
			check(OldActorPackageName != NewPackage->GetName());
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Converted Actor %s(%s) to %s."), *Actor->GetName(), *NewPackage->GetName(), *ExternalDataLayerAsset->GetName());

			ConvertedActorGuids.Add(It->GetGuid());
		}

		// Remove actors from Content Bundle container (since we recycled the same Actor/guid)
		for (const FGuid& ActorGuid : ConvertedActorGuids)
		{
			ContentBundleContainerInstance->RemoveActor(ActorGuid);
		}
		
		if (bContentBundleActorConversionSuccess)
		{
			// Remove Content Bundle GameFeatureData action
			GameFeatureData->GetMutableActionsInEditor().Remove(OldAddWPContentAction);
			PackagesToSave.Add(GameFeatureData->GetPackage());
		}

		// Restore package flag
		if (bIsNewEDLWorldDataLayers)
		{
			EDLWorldDataLayers->GetPackage()->SetPackageFlags(PKG_NewlyCreated);
		}
		
		// Log
		for (UPackage* PackageToSave : PackagesToSave)
		{
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Package to save: %s"), *PackageToSave->GetPathName());
		}

		for (const FString& PackageToDelete : PackagesToDelete)
		{
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Package to delete: %s"), *PackageToDelete);
		}

		if (bContentBundleActorConversionSuccess && !bReportOnly)
		{
			if (!UWorldPartitionBuilder::SavePackages(PackagesToSave.Array(), PackageHelper))
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to save packages. Conversion sanity is not guaranteed. Consult log for details."));
				bContentBundleActorConversionSuccess = false;
			}
			else if (!UWorldPartitionBuilder::DeletePackages(PackagesToDelete.Array(), PackageHelper))
			{
				UE_LOG(LogConvertContentBundleBuilder, Error, TEXT("Failed to delete packages. Conversion sanity is not guaranteed. Consult log for details."));
				bContentBundleActorConversionSuccess = false;
			}
		}

		if (bContentBundleActorConversionSuccess)
		{
			ConvertedContentBundles.Add(ContentBundle);
			FinalReport.Reserve(FinalReport.Num() + PackagesToSave.Num() + PackagesToDelete.Num() + 5);
			FinalReport.Add(TEXT("------------------------------------------------------------------------"));
			FinalReport.Add(FString::Printf(TEXT("Converted Content Bundle '%s' (%s) to EDL Asset '%s'"), *ContentBundle->GetDisplayName(), *ContentBundle->GetDescriptor()->GetGuid().ToString(), *ExternalDataLayerAsset->GetPathName()));
			FinalReport.Add(TEXT("------------------------------------------------------------------------"));
			FinalReport.Add(FString::Printf(TEXT("[+] Added %d packages: "), PackagesToSave.Num()));
			for (UPackage* PackageToSave : PackagesToSave)
			{
				FinalReport.Add(FString::Printf(TEXT(" |- %s"), *PackageToSave->GetPathName()));
			}
			if (PackagesToDelete.Num())
			{
				FinalReport.Add(FString::Printf(TEXT("[+] Deleted %d packages: "), PackagesToDelete.Num()));
				for (const FString& PackageToDelete : PackagesToDelete)
				{
					FinalReport.Add(FString::Printf(TEXT(" |- %s"), *PackageToDelete));
				}
			}
		}
		else
		{
			bIsSuccess = false;
		}
	}

	if (SkippedEmptyContentBundles.Num())
	{
		FinalReport.Reserve(FinalReport.Num() + SkippedEmptyContentBundles.Num() + 4);
		FinalReport.Add(TEXT("------------------------------------------------------------------------"));
		FinalReport.Add(FString::Printf(TEXT("%d Skipped Empty Content Bundles: "), SkippedEmptyContentBundles.Num()));
		FinalReport.Add(TEXT("------------------------------------------------------------------------"));
		FinalReport.Add(TEXT("[+] Content Bundles: "));
		for (const TSharedPtr<FContentBundleEditor>& CB : SkippedEmptyContentBundles)
		{
			FinalReport.Add(FString::Printf(TEXT(" |- %s (%s)"), *CB->GetDisplayName(), *CB->GetDescriptor()->GetGuid().ToString()));
		}
	}

	TArray<TSharedPtr<FContentBundleEditor>> FailedContentBundles;
	for (TSharedPtr<FContentBundleEditor>& ContentBundle : ContentBundlesToProcess)
	{
		if (!ConvertedContentBundles.Contains(ContentBundle) && !SkippedEmptyContentBundles.Contains(ContentBundle))
		{
			FailedContentBundles.Add(ContentBundle);
		}
	}

	if (FailedContentBundles.Num())
	{
		FinalReport.Reserve(FinalReport.Num() + FailedContentBundles.Num() + 4);
		FinalReport.Add(TEXT("------------------------------------------------------------------------"));
		FinalReport.Add(FString::Printf(TEXT("%d Failed Content Bundle Conversions: "), FailedContentBundles.Num()));
		FinalReport.Add(TEXT("------------------------------------------------------------------------"));
		FinalReport.Add(TEXT("[+] Content Bundles: "));
		for (const TSharedPtr<FContentBundleEditor>& CB : FailedContentBundles)
		{
			FinalReport.Add(FString::Printf(TEXT(" |- %s (%s)"), *CB->GetDisplayName(), *CB->GetDescriptor()->GetGuid().ToString()));
		}
	}

	if (FinalReport.Num())
	{
		UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("================================================================================================="));
		UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("Content Bundle Conversion : Found(%d) | Converted(%d) | Empty(%d) | Failed(%d)"), ContentBundlesToProcess.Num(), ConvertedContentBundles.Num(), SkippedEmptyContentBundles.Num(), FailedContentBundles.Num());
		UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("================================================================================================="));
		for (const FString& Str : FinalReport)
		{
			UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("%s"), *Str);
		}
		UE_LOG(LogConvertContentBundleBuilder, Log, TEXT("================================================================================================="));
	}

	return bIsSuccess;
}

UExternalDataLayerAsset* UGameFeatureActionConvertContentBundleWorldPartitionBuilder::GetOrCreateExternalDataLayerAsset(const UContentBundleDescriptor* InContentBundleDescriptor, UExternalDataLayerFactory* InExternalDataLayerFactory, TSet<UPackage*>& OutPackagesToSave) const
{
	const FString ContentBundleDescriptorPackage = InContentBundleDescriptor->GetPackage()->GetName();
	const FString AssetPath = FString(FPathViews::GetMountPointNameFromPath(ContentBundleDescriptorPackage, nullptr, false)) / DestinationFolder;
	const FString AssetName = UPackageTools::SanitizePackageName(InContentBundleDescriptor->GetDisplayName());
	const FString PackageName = AssetPath / AssetName;
	const FSoftObjectPath Path(FTopLevelAssetPath(FName(PackageName), FName(AssetName)).ToString());
	const FString ObjectPath = Path.ToString();
	if (UExternalDataLayerAsset* ExistingExternalDataLayerAsset = LoadObject<UExternalDataLayerAsset>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn))
	{
		return ExistingExternalDataLayerAsset;
	}
	
	UObject* Asset = IAssetTools::Get().CreateAsset(AssetName, AssetPath, UExternalDataLayerAsset::StaticClass(), InExternalDataLayerFactory);
	UExternalDataLayerAsset* NewExternalDataLayerAsset = Asset ? CastChecked<UExternalDataLayerAsset>(Asset) : nullptr;
	UE_CLOG(!NewExternalDataLayerAsset, LogConvertContentBundleBuilder, Error, TEXT("Failed to create external data layer asset for %s."), *InContentBundleDescriptor->GetDisplayName());
	if (NewExternalDataLayerAsset)
	{
		OutPackagesToSave.Add(NewExternalDataLayerAsset->GetPackage());
	}
	return NewExternalDataLayerAsset;
}