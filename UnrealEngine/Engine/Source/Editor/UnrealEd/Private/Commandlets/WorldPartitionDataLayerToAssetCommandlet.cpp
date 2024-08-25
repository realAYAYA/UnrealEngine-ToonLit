// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 UDataLayerToAssetCommandlet.cpp: Commandlet used to convert a partitioned ULevel's data layers to assets
=============================================================================*/

#include "Commandlets/WorldPartitionDataLayerToAssetCommandLet.h"

#include "Algo/Copy.h"
#include "Algo/Accumulate.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "DataLayer/DataLayerFactory.h"
#include "Engine/World.h"
#include "Logging/LogVerbosity.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHelpers.h"

DEFINE_LOG_CATEGORY(LogDataLayerToAssetCommandlet);

void UDataLayerConversionInfo::SetDataLayerToConvert(const UDeprecatedDataLayerInstance* InDataLayerToConvert)
{
	DataLayerToConvert = InDataLayerToConvert;

	DataLayerAsset->DataLayerType = DataLayerToConvert->GetType();
	DataLayerAsset->DebugColor = DataLayerToConvert->GetDebugColor();
}

void UDataLayerConversionInfo::SetDataLayerInstance(UDataLayerInstanceWithAsset* InDataLayerInstance)
{
	DataLayerInstance = InDataLayerInstance;

	if (DataLayerToConvert != nullptr)
	{
		DataLayerInstance->DataLayerAsset = DataLayerAsset;
		DataLayerInstance->bIsVisible = DataLayerToConvert->bIsVisible;
		DataLayerInstance->bIsInitiallyVisible = DataLayerToConvert->bIsInitiallyVisible;
		DataLayerInstance->bIsInitiallyLoadedInEditor = DataLayerToConvert->bIsInitiallyLoadedInEditor;
		DataLayerInstance->bIsLocked = DataLayerToConvert->bIsLocked;
		DataLayerInstance->InitialRuntimeState = DataLayerToConvert->InitialRuntimeState;
	}
	else
	{
		// Check the DataLayerInstance was already properly converted
		check(DataLayerInstance->DataLayerAsset == DataLayerAsset)
	}
}

UDataLayerToAssetCommandletContext::UDataLayerToAssetCommandletContext(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::GetDataLayerConversionInfo(const UDeprecatedDataLayerInstance* DataLayer) const
{
	const TObjectPtr<UDataLayerConversionInfo>* Entry = DataLayerConversionInfo.FindByPredicate([&DataLayer](const TObjectPtr<UDataLayerConversionInfo>& Other) { return DataLayer == Other->DataLayerToConvert; });
	return Entry != nullptr ? Entry->Get() : nullptr;
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::GetDataLayerConversionInfo(const UDataLayerAsset* DataLayerAsset) const
{
	const TObjectPtr<UDataLayerConversionInfo>* Entry = DataLayerConversionInfo.FindByPredicate([&DataLayerAsset](const TObjectPtr<UDataLayerConversionInfo>& Other) { return DataLayerAsset == Other->DataLayerAsset; });
	return Entry != nullptr ? Entry->Get() : nullptr;
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::GetDataLayerConversionInfo(const UDataLayerInstanceWithAsset* DataLayerInstance) const
{
	const TObjectPtr<UDataLayerConversionInfo>* Entry = DataLayerConversionInfo.FindByPredicate([&DataLayerInstance](const TObjectPtr<UDataLayerConversionInfo>& Other) { return DataLayerInstance == Other->DataLayerInstance; });
	return Entry != nullptr ? Entry->Get() : nullptr;
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::GetDataLayerConversionInfo(const FActorDataLayer& ActorDataLayer) const
{
	const TObjectPtr<UDataLayerConversionInfo>* Entry = DataLayerConversionInfo.FindByPredicate([&ActorDataLayer](const TObjectPtr<UDataLayerConversionInfo>& Other)
	{ 
		return Other->DataLayerToConvert != nullptr && ActorDataLayer.Name == Other->DataLayerToConvert->GetDataLayerFName();
	});
	
	return Entry != nullptr ? Entry->Get() : nullptr;
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::StoreExistingDataLayer(FAssetData& AssetData)
{
	UDataLayerConversionInfo* ConversionInfo = NewObject<UDataLayerConversionInfo>();
	ConversionInfo->DataLayerAsset = CastChecked<UDataLayerAsset>(AssetData.GetAsset());

	UE_LOG(LogDataLayerToAssetCommandlet, Verbose, TEXT("Data Layer Asset %s discovered."),
		*ConversionInfo->DataLayerAsset->GetFullName());
	return DataLayerConversionInfo.Add_GetRef(ConversionInfo).Get();
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::StoreDataLayerAssetConversion(const UDeprecatedDataLayerInstance* DataLayerToConvert, UDataLayerAsset* NewDataLayerAsset)
{
	if (UDataLayerConversionInfo* ConversionInfo = GetDataLayerConversionInfo(DataLayerToConvert))
	{
		UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to assign asset %s to data Layer %s. The data layer is already associated to asset %s."), 
			*NewDataLayerAsset->GetFullName() , *DataLayerToConvert->GetDataLayerShortName() , *ConversionInfo->DataLayerAsset->GetFullName());
		return nullptr;
	}

	UDataLayerConversionInfo* ConversionInfo = GetDataLayerConversionInfo(NewDataLayerAsset);
	if (ConversionInfo != nullptr)
	{
		if (ConversionInfo->DataLayerToConvert != nullptr)
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to assign asset %s to data Layer %s. The asset is already associated to data layer %s."),
				*NewDataLayerAsset->GetFullName(), *DataLayerToConvert->GetDataLayerShortName(), *ConversionInfo->DataLayerToConvert->GetDataLayerShortName());
			return nullptr;
		}
	}
	else
	{
		ConversionInfo = NewObject<UDataLayerConversionInfo>();
		ConversionInfo->DataLayerAsset = NewDataLayerAsset;
		DataLayerConversionInfo.Add(ConversionInfo);
	}

	ConversionInfo->SetDataLayerToConvert(DataLayerToConvert);

	UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Data Layer Asset %s is associated to Data Layer %s for conversion"),
		*ConversionInfo->DataLayerAsset->GetFullName(), *ConversionInfo->DataLayerToConvert->GetDataLayerShortName());
	return ConvertingDataLayerInfo.Add_GetRef(ConversionInfo).Get();
}

UDataLayerConversionInfo* UDataLayerToAssetCommandletContext::StoreDataLayerInstanceConversion(const UDataLayerAsset* DataLayerAsset, UDataLayerInstanceWithAsset* NewDataLayerInstance)
{
	if (UDataLayerConversionInfo* ConversionInfo = GetDataLayerConversionInfo(NewDataLayerInstance))
	{
		UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to assign asset %s to data layer instance %s. The instance is already associated to asset %s."),
			*DataLayerAsset->GetFullName(), *NewDataLayerInstance->GetDataLayerFName().ToString(), *ConversionInfo->DataLayerAsset->GetFullName());
		return nullptr;
	}

	if (UDataLayerConversionInfo* ConversionInfo = GetDataLayerConversionInfo(DataLayerAsset)) 
	{
		check(ConversionInfo->DataLayerInstance == nullptr);
		ConversionInfo->SetDataLayerInstance(NewDataLayerInstance);

		UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Data Layer Instance %s is associated to Data Layer Asset %s for conversion"),
			*ConversionInfo->DataLayerInstance->GetDataLayerFName().ToString(), *ConversionInfo->DataLayerAsset->GetFullName());
		return ConversionInfo;
	}

	UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to assign asset %s to data layer instance %s. The asset was not retrieved in conversion data."),
		*DataLayerAsset->GetFullName(), *NewDataLayerInstance->GetDataLayerFName().ToString());
	return nullptr;
}

bool UDataLayerToAssetCommandletContext::SetPreviousConversions(UDataLayerConversionInfo* CurrentConversion, TArray<TWeakObjectPtr<UDataLayerConversionInfo>>&& PreviousConversions)
{
	check(CurrentConversion->CurrentConvertingInfo == nullptr);
	check(CurrentConversion->PreviousConversionsInfo.IsEmpty());

	CurrentConversion->PreviousConversionsInfo = MoveTemp(PreviousConversions);

	uint32 ErrorCount = 0;
	for (TWeakObjectPtr<UDataLayerConversionInfo>& PreviousConversion : CurrentConversion->PreviousConversionsInfo)
	{
		check(PreviousConversion->CurrentConvertingInfo == nullptr);
		check(PreviousConversion->PreviousConversionsInfo.IsEmpty());

		if (PreviousConversion->DataLayerInstance != nullptr)
		{
			ErrorCount++;
			UE_LOG(LogDataLayerToAssetCommandlet, Error,
				TEXT("DataLayer %s was already converted but is still to be converted. Re-Sync Data to a clean conversion or pre-conversion state and re-run the commandlet"),
				*CurrentConversion->DataLayerAsset->GetFullName());
		}

		PreviousConversion->CurrentConvertingInfo = CurrentConversion;
	}

	ConvertingDataLayerInfo.AddUnique(CurrentConversion);

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandletContext::FindDataLayerConversionInfos(FName DataLayerAssetName, TArray<TWeakObjectPtr<UDataLayerConversionInfo>>& OutConversionInfos) const
{
	OutConversionInfos.Empty();

	FString SanitizedAssetName = DataLayerAssetName.ToString();
	const TCHAR* InvalidPackageChar = INVALID_LONGPACKAGE_CHARACTERS;
	while (*InvalidPackageChar)
	{
		SanitizedAssetName.ReplaceCharInline(*InvalidPackageChar, TCHAR('_'), ESearchCase::CaseSensitive);
		++InvalidPackageChar;
	}

	for (TObjectPtr<UDataLayerConversionInfo> const&  ConversionInfo : DataLayerConversionInfo)
	{
		if (ConversionInfo->DataLayerAsset->GetFName() == FName(SanitizedAssetName))
		{
			OutConversionInfos.Add(ConversionInfo.Get());
		}
	}

	return !OutConversionInfos.IsEmpty();
}

void UDataLayerToAssetCommandletContext::LogConversionInfos() const
{
	if (LogDataLayerToAssetCommandlet.GetVerbosity() >= ELogVerbosity::Verbose)
	{
		for (const TObjectPtr<UDataLayerConversionInfo>& info : DataLayerConversionInfo)
		{
			FString ConflictingConversionString = TEXT("");
			if(!info->PreviousConversionsInfo.IsEmpty())
			{
				ConflictingConversionString  = FString::JoinBy(info->PreviousConversionsInfo, TEXT(", "), [](const TWeakObjectPtr<UDataLayerConversionInfo>& ConflictingInfo)
				{ 
						return ConflictingInfo->DataLayerAsset->GetFullName(); 
				});
			}
			

			UE_LOG(LogDataLayerToAssetCommandlet, Verbose, TEXT("[Conversion Info] Data Layer %s\t\tData Layer Asset: %s\t\t\t\t\t\tData Layer Instance: %s\t\tConverting By: %s\t\t\t\t\t\tConflicting Previous Conversion: %s"),
				info->DataLayerToConvert != nullptr ? *info->DataLayerToConvert->GetDataLayerShortName() : TEXT("None"),
				info->DataLayerAsset != nullptr ? *info->DataLayerAsset->GetFullName() : TEXT("None"),
				info->DataLayerInstance != nullptr ? *info->DataLayerInstance->GetDataLayerFName().ToString() : TEXT("None"),
				info->CurrentConvertingInfo != nullptr ? *info->CurrentConvertingInfo->DataLayerAsset->GetFullName() : TEXT("None"),
				*ConflictingConversionString);

		}
	}
}

UDataLayerToAssetCommandlet::UDataLayerToAssetCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	DataLayerFactory(NewObject<UDataLayerFactory>())
{}

int32 UDataLayerToAssetCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::Main);

	UE_SCOPED_TIMER(TEXT("Data Layer Conversion"), LogDataLayerToAssetCommandlet, Display);

	FPackageSourceControlHelper PackageHelper;

	ON_SCOPE_EXIT
	{
		if (MainWorld != nullptr)
		{
			const bool bBroadcastWorldDestroyedEvent = false;
			MainWorld->DestroyWorld(bBroadcastWorldDestroyedEvent);
		}
	};

	TArray<FString> Tokens, Switches;
	TMap<FString, FString> CommandLineParams;
	ParseCommandLine(*Params, Tokens, Switches, CommandLineParams);
	if (!InitializeFromCommandLine(Tokens, Switches, CommandLineParams))
	{
		return EReturnCode::CommandletInitializationError;
	}

	ConversionFolder = DestinationFolder + "/" + MainWorld->GetName() + "/";

	TStrongObjectPtr<UDataLayerToAssetCommandletContext> Context(NewObject<UDataLayerToAssetCommandletContext>());

	if (!BuildConversionInfos(Context, PackageHelper))
	{
		return EReturnCode::DataLayerConversionError;
	}

	if (!ResolvePreviousConversionsToCurrent(Context))
	{
		return EReturnCode::DataLayerConversionError;
	}

	Context->LogConversionInfos();

	if (!CreateDataLayerInstances(Context))
	{
		return EReturnCode::DataLayerConversionError;
	}

	if (!RemapActorDataLayersToAssets(Context, PackageHelper))
	{
		return EReturnCode::ActorDataLayerRemappingError;
	}

	if (!PerformProjectSpecificConversions(Context))
	{
		return EReturnCode::ProjectSpecificConversionError;
	}

	if (!DeletePreviousConversionsData(Context, PackageHelper))
	{
		return EReturnCode::DataLayerConversionError;
	}

	if (!CommitConversion(Context, PackageHelper))
	{
		return EReturnCode::DataLayerConversionError;
	}

	return EReturnCode::Success;
}

bool UDataLayerToAssetCommandlet::InitializeFromCommandLine(TArray<FString>& Tokens, TArray<FString> const& Switches, TMap<FString, FString> const& Params)
{
	constexpr TCHAR DestinationFolderSwitch[] = TEXT("DestinationFolder");
	if (FString const* DestFolderPtr = Params.Find(DestinationFolderSwitch))
	{
		DestinationFolder = *DestFolderPtr;
	}
	else
	{
		UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("No \"%s\" switch specified"), DestinationFolderSwitch);
		return false;
	}

	if (Switches.Contains(TEXT("Verbose")))
	{
		LogDataLayerToAssetCommandlet.SetVerbosity(ELogVerbosity::Verbose);
		WorldPartitionCommandletHelpers::LogWorldPartitionCommandletUtils.SetVerbosity(ELogVerbosity::Verbose);
	}

	bPerformSavePackages = Switches.Contains(TEXT("NoSave")) == false;
	bIgnoreActorLoadingErrors = Switches.Contains(TEXT("IgnoreActorLoadingErrors"));

	MainWorld = WorldPartitionCommandletHelpers::LoadAndInitWorld(Tokens[0]);
	if (!MainWorld)
	{
		UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to Load %s, Conversion will abort"), *Tokens[0]);
		return false;
	}

	return true;
}

bool UDataLayerToAssetCommandlet::BuildConversionInfos(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, FPackageSourceControlHelper& PackageHelper)
{
	UE_SCOPED_TIMER(TEXT("Retrieving Already Converted Data Layers"), LogDataLayerToAssetCommandlet, Display);

	TArray<FAssetData> ExistingDataLayerAssets;
	IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(UDataLayerAsset::StaticClass()), ExistingDataLayerAssets);
	for (FAssetData& AssetData : ExistingDataLayerAssets)
	{
		if (IsAssetInConversionFolder(AssetData.GetSoftObjectPath()))
		{
			CommandletContext->StoreExistingDataLayer(AssetData);
		}
	}

	AWorldDataLayers* WorldDataLayers = MainWorld->GetWorldDataLayers();
	TArray<UDataLayerInstance*> DataLayerInstances;
	Algo::Copy(WorldDataLayers->DataLayerInstances, DataLayerInstances);
	for (int32 i = DataLayerInstances.Num() -1; i >= 0; --i)
	{
		if(UDataLayerInstanceWithAsset* DataLayerInstance = Cast<UDataLayerInstanceWithAsset>(DataLayerInstances[i]))
		{
			if (const UDataLayerAsset* DataLayerAsset = DataLayerInstance->GetAsset())
			{
				if (UDataLayerConversionInfo* ConversionInfo = CommandletContext->GetDataLayerConversionInfo(DataLayerAsset))
				{
					CommandletContext->StoreDataLayerInstanceConversion(ConversionInfo->DataLayerAsset, DataLayerInstance);
				}
			}
		}
	}

	uint32 ErrorCount = 0;
	for (const UDataLayerInstance* DataLayerInstance : WorldDataLayers->DataLayerInstances)
	{
		if (const UDeprecatedDataLayerInstance* DataLayerToConvert = Cast<UDeprecatedDataLayerInstance>(DataLayerInstance))
		{
			if (!CreateConversionFromDataLayer(CommandletContext, DataLayerToConvert, PackageHelper))
			{
				ErrorCount++;
			}
		}
	}

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandlet::CreateConversionFromDataLayer(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, const UDeprecatedDataLayerInstance* DataLayer, FPackageSourceControlHelper &PackageHelper)
{
	if(TObjectPtr<UDataLayerAsset> DataLayerAsset = GetOrCreateDataLayerAssetForConversion(CommandletContext, FName(DataLayer->GetDataLayerShortName())))
	{
		if (UDataLayerConversionInfo* ConversionInfo = CommandletContext->StoreDataLayerAssetConversion(DataLayer, DataLayerAsset))
		{
			if (bPerformSavePackages)
			{
				return WorldPartitionCommandletHelpers::CheckoutSaveAdd(ConversionInfo->DataLayerAsset->GetPackage(), PackageHelper);
			}

			return true;
		}
	}
	
	return false;
}

TObjectPtr<UDataLayerAsset> UDataLayerToAssetCommandlet::GetOrCreateDataLayerAssetForConversion(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, FName DataLayerAssetName)
{
	TObjectPtr<UDataLayerAsset> DataLayerAsset = nullptr;

	TArray<TWeakObjectPtr<UDataLayerConversionInfo>> ConversionInfos;
	if (CommandletContext->FindDataLayerConversionInfos(DataLayerAssetName, ConversionInfos))
	{
		for (TWeakObjectPtr<UDataLayerConversionInfo>& ConversionInfo : ConversionInfos)
		{
			if (IsAssetInConversionFolder(ConversionInfo->DataLayerAsset))
			{
				check(DataLayerAsset == nullptr);
				return CastChecked<UDataLayerAsset>(ConversionInfo->DataLayerAsset);
			}
		}
	}

	UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Creating new Data Layer Asset %s in folder %s"),
		*DataLayerAssetName.ToString(), *ConversionFolder);

	FString SanitizedAssetName = DataLayerAssetName.ToString();
	const TCHAR* InvalidPackageChar = INVALID_LONGPACKAGE_CHARACTERS;
	while (*InvalidPackageChar)
	{
		SanitizedAssetName.ReplaceCharInline(*InvalidPackageChar, TCHAR('_'), ESearchCase::CaseSensitive);
		++InvalidPackageChar;
	}

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	if (UObject* Asset = AssetTools.CreateAsset(SanitizedAssetName, ConversionFolder, UDataLayerAsset::StaticClass(), DataLayerFactory))
	{
		DataLayerAsset = CastChecked<UDataLayerAsset>(Asset);
	}
	else
	{
		UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to create asset for %s in folder %s. Consult log for more details"),
			*DataLayerAssetName.ToString(), *ConversionFolder);
	}

	return DataLayerAsset;
}

bool UDataLayerToAssetCommandlet::ResolvePreviousConversionsToCurrent(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext)
{
	UE_SCOPED_TIMER(TEXT("Resolving Conflicting Conversions"), LogDataLayerToAssetCommandlet, Display);
	UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Resolving Conflicting Conversions"));

	uint32 ErrorCount = 0;
	for (const TObjectPtr<UDataLayerConversionInfo>& ConversionInfo : CommandletContext->GetDataLayerConversionInfos())
	{
		if(IsAssetInConversionFolder(ConversionInfo->DataLayerAsset))
		{
			TArray<TWeakObjectPtr<UDataLayerConversionInfo>> PreviousConversionInfos;
			if (CommandletContext->FindDataLayerConversionInfos(ConversionInfo->DataLayerAsset->GetFName(), PreviousConversionInfos))
			{
				PreviousConversionInfos.Remove(ConversionInfo);

				if (!PreviousConversionInfos.IsEmpty())
				{
					if (!CommandletContext->SetPreviousConversions(ConversionInfo, MoveTemp(PreviousConversionInfos)))
					{
						ErrorCount++;
					}
				}
			}
		}
	}

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandlet::RemapActorDataLayersToAssets(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, FPackageSourceControlHelper& PackageHelper)
{
	UE_SCOPED_TIMER(TEXT("Remapping Actors Data Layers"), LogDataLayerToAssetCommandlet, Display);
	UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Starting Actor Data Layer Remapping To Data Layer Asset. This can take a while."));

	uint32 ErrorCount = 0;
	FWorldPartitionHelpers::ForEachActorWithLoading(MainWorld->GetWorldPartition(), [&ErrorCount, &CommandletContext, this, &PackageHelper](const FWorldPartitionActorDescInstance* ActorDescInstance)
	{
		uint32 ActorConversionErrors = 0;
		if (AActor* Actor = ActorDescInstance->GetActor())
		{
			ActorConversionErrors += RemapDataLayersAssetsFromPreviousConversions(CommandletContext, Actor);
			ActorConversionErrors += RemapActorDataLayers(CommandletContext, Actor);

			if (!PerformAdditionalActorConversions(CommandletContext, Actor))
			{
				ActorConversionErrors++;
			}

			if (ActorConversionErrors == 0 && bPerformSavePackages)
			{
				if (bPerformSavePackages && Actor->GetExternalPackage()->IsDirty())
				{
					if (!WorldPartitionCommandletHelpers::CheckoutSaveAdd(Actor->GetExternalPackage(), PackageHelper))
					{
						ActorConversionErrors++;
					}
				}
			}
		}
		else
		{
			const FDataLayerInstanceNames& ActDescDataLayers = ActorDescInstance->GetDataLayerInstanceNames();
			if (!ActDescDataLayers.IsEmpty())
			{
				FString DataLayerString = FString::JoinBy(ActDescDataLayers.ToArray(), TEXT(", "), [](const FName& DataLayerName) { return DataLayerName.ToString(); });

				UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Actor %s failed to load. Its data layers %s will not be remapped to a data layer asset."),
					*ActorDescInstance->GetActorName().ToString(), *DataLayerString);
				if (!bIgnoreActorLoadingErrors)
				{
					ActorConversionErrors++;
				}
			}
		}

		ErrorCount += ActorConversionErrors;
		return true;
	});

	UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Actor Data Layer Remapping To Data Layer Asset Done."));

	return ErrorCount == 0;;
}

uint32 UDataLayerToAssetCommandlet::RemapActorDataLayers(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, AActor* Actor)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	uint32 ErrorCount = 0;

	const TArray<FActorDataLayer>& ActorDataLayers = Actor->GetActorDataLayers();
	for (int32 i = ActorDataLayers.Num() - 1; i >= 0; --i)
	{
		const FActorDataLayer& ActorDataLayer = ActorDataLayers[i];
		if (UDataLayerConversionInfo* DataLayerConversionInfo = CommandletContext->GetDataLayerConversionInfo(ActorDataLayer))
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Verbose, TEXT("Data layer %s in Actor %s remapped to data layer asset %s"),
				*ActorDataLayer.Name.ToString(), *Actor->GetName(), *DataLayerConversionInfo->DataLayerAsset->GetName());

			if (Actor->AddDataLayer(DataLayerConversionInfo->DataLayerInstance))
			{
				if (!Actor->RemoveDataLayer(DataLayerConversionInfo->DataLayerToConvert))
				{
					ErrorCount++;
					UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to remove data layer %s in Actor %s"),
						*DataLayerConversionInfo->DataLayerToConvert->GetDataLayerShortName(), *Actor->GetName());
				}
			}
			else
			{
				ErrorCount++;
				UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to add data layer asset %s in Actor %s"),
					*DataLayerConversionInfo->DataLayerAsset->GetFullName(), *Actor->GetName());
			}

		}
		else
		{
			ErrorCount++;
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Actor %s is referencing data layer %s but it does not match any data layer asset."),
				*Actor->GetName(), *ActorDataLayer.Name.ToString());
		}
	}

	return ErrorCount;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

// Multiple run of the commandlet can lead to Actors referencing Data Layer Assets in different folders. Always remap to the newly created asset.
uint32 UDataLayerToAssetCommandlet::RemapDataLayersAssetsFromPreviousConversions(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, AActor* Actor)
{
	uint32 ErrorCount = 0;

	TArray<const UDataLayerAsset*> ActorDataLayerAssets = Actor->GetDataLayerAssets();
	for (int32 i = ActorDataLayerAssets.Num() - 1; i >= 0; --i)
	{
		const TObjectPtr<const UDataLayerAsset>& ActorDataLayerAsset = ActorDataLayerAssets[i];
		if (UDataLayerConversionInfo* DataLayerConversionInfo = CommandletContext->GetDataLayerConversionInfo(ActorDataLayerAsset.Get()))
		{
			if (DataLayerConversionInfo->IsAPreviousConversion())
			{
				if (!DataLayerConversionInfo->GetCurrentConversion()->DataLayerInstance->AddActor(Actor))
				{
					ErrorCount++;
					UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to replace data layer asset %s in Actor %s"),
						*DataLayerConversionInfo->GetCurrentConversion()->DataLayerAsset->GetFullName(), *Actor->GetName());
				}
			}
		}
	}

	return ErrorCount;
}

bool UDataLayerToAssetCommandlet::CreateDataLayerInstances(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext)
{
	UE_SCOPED_TIMER(TEXT("Creating Data Layer Instances"), LogDataLayerToAssetCommandlet, Display);

	AWorldDataLayers* WorldDataLayers = MainWorld->GetWorldDataLayers();
	for (const TWeakObjectPtr<UDataLayerConversionInfo>& ConvertingInfo : CommandletContext->GetConvertingDataLayerConversionInfo())
	{
		if (ConvertingInfo->DataLayerInstance == nullptr)
		{
			UDataLayerInstanceWithAsset* DataLayerInstance = WorldDataLayers->CreateDataLayer<UDataLayerInstanceWithAsset>(ConvertingInfo->DataLayerAsset);
			UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Created new Data Layer Instance %s"),
				*DataLayerInstance->GetDataLayerFName().ToString());

			CommandletContext->StoreDataLayerInstanceConversion(ConvertingInfo->DataLayerAsset, DataLayerInstance);
		}

		WorldDataLayers->DeprecatedDataLayerNameToDataLayerInstance.Add(ConvertingInfo->DataLayerToConvert->GetDataLayerFName(), ConvertingInfo->DataLayerInstance);
	}

	uint32 ErrorCount = 0;

	if (!RebuildDataLayerHierarchies(CommandletContext))
	{
		ErrorCount++;
	}

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandlet::RebuildDataLayerHierarchies(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext)
{
	UE_SCOPED_TIMER(TEXT("Creating Data Layer Instances Hierarchy"), LogDataLayerToAssetCommandlet, Display);

	uint32 ErrorCount = 0;
	for (const TWeakObjectPtr<UDataLayerConversionInfo>& ConvertingInfo : CommandletContext->GetConvertingDataLayerConversionInfo())
	{
		UDataLayerInstance* Child = ConvertingInfo->DataLayerInstance;
		if (Child == nullptr)
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to retrieve data layer instance when re-building hiearchy for %s"),
				*ConvertingInfo->DataLayerAsset->GetFullName());
			ErrorCount++;
			continue;
		}

		const UDataLayerInstance* OldParent = ConvertingInfo->DataLayerToConvert->GetParent();
		if (OldParent == nullptr)
		{
			// No Parent, continue
			continue;
		}

		
		const UDeprecatedDataLayerInstance* DeprecatedParent = Cast<UDeprecatedDataLayerInstance>(OldParent);
		if (DeprecatedParent == nullptr)
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Deprecated Data Layer Instance %s has %s as a parent. But %s is not depcrated. This is not permitted"),
				*Child->GetDataLayerFullName(), *OldParent->GetDataLayerShortName(), *OldParent->GetDataLayerShortName());
			ErrorCount++;
			continue;
		}

		UDataLayerConversionInfo* ParentConversionInfo = CommandletContext->GetDataLayerConversionInfo(DeprecatedParent);
		if (ParentConversionInfo == nullptr)
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to find conversion info for parent %s of %s while rebuilding data layer hierarchy"),
				*DeprecatedParent->GetDataLayerFullName(), *Child->GetDataLayerShortName());
			ErrorCount++;
			continue;

		}

		if (UDataLayerInstance* NewParent = ParentConversionInfo->DataLayerInstance)
		{
			if (!Child->SetParent(NewParent))
			{
				UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to set %s as the parent of %s"),
					*NewParent->GetDataLayerFullName(), *Child->GetDataLayerShortName());
				ErrorCount++;
			}
		}
		else
		{
			UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to retrieve data layer instance for parent %s when re-building hiearchy for %s"),
				*DeprecatedParent->GetDataLayerFullName(), *Child->GetDataLayerShortName());
			ErrorCount++;
		}

	}

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandlet::DeletePreviousConversionsData(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, FPackageSourceControlHelper& PackageHelper)
{
	UE_SCOPED_TIMER(TEXT("Delete Conflicting Assets"), LogDataLayerToAssetCommandlet, Display);

	uint32 ErrorCount = 0;
	for (const TWeakObjectPtr<UDataLayerConversionInfo>& ConvertingInfo : CommandletContext->GetConvertingDataLayerConversionInfo())
	{
		for (const TWeakObjectPtr<UDataLayerConversionInfo>& PreviousConversion : ConvertingInfo->GetPreviousConversions())
		{
			if (bPerformSavePackages)
			{
				if (WorldPartitionCommandletHelpers::Delete(PreviousConversion->DataLayerAsset->GetPackage(), PackageHelper))
				{
					PreviousConversion->DataLayerAsset = nullptr;
				}
				else
				{
					UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to delete Data Layer Asset %s from previous conversion. (Conflicting with %s)"),
						*PreviousConversion->DataLayerAsset->GetFullName(), *ConvertingInfo->DataLayerAsset->GetFullName());
					ErrorCount++;
				}
			}
		}
	}

	return ErrorCount == 0;
}

bool UDataLayerToAssetCommandlet::CommitConversion(TStrongObjectPtr<UDataLayerToAssetCommandletContext>& CommandletContext, FPackageSourceControlHelper& PackageHelper)
{
	uint32 ErrorCount = 0;
	AWorldDataLayers* WorldDataLayers = MainWorld->GetWorldDataLayers();
	for (const TWeakObjectPtr<UDataLayerConversionInfo>& ConversionInfo : CommandletContext->GetConvertingDataLayerConversionInfo())
	{
		if (ConversionInfo->DataLayerToConvert != nullptr)
		{
			// Remove directly from DataLayerInstances as RemoveDataLayer method also cleans DeprecatedDataLayerNameToDataLayerInstance which is used for runtime conversion
			if (WorldDataLayers->DataLayerInstances.Remove(ConversionInfo->DataLayerToConvert))
			{
				UE_LOG(LogDataLayerToAssetCommandlet, Log, TEXT("Deleted old data layer %s, it is now converted to Data Layer Asset %s and Data Layer Instance %s"),
					*ConversionInfo->DataLayerToConvert->GetDataLayerShortName(), *ConversionInfo->DataLayerAsset->GetFullName(), *ConversionInfo->DataLayerInstance->GetDataLayerFName().ToString());
			}
			else
			{
				UE_LOG(LogDataLayerToAssetCommandlet, Error, TEXT("Failed to delete converting data layer %s."),
					*ConversionInfo->DataLayerToConvert->GetDataLayerShortName());
				ErrorCount++;
			}
		}
	}

	if (ErrorCount == 0 && bPerformSavePackages)
	{
		if (!WorldPartitionCommandletHelpers::CheckoutSaveAdd(WorldDataLayers->GetExternalPackage(), PackageHelper))
		{
			return false;
		}
	}

	return true;
}

bool UDataLayerToAssetCommandlet::IsAssetInConversionFolder(const FSoftObjectPath& DataLayerAsset)
{
	return DataLayerAsset.GetAssetPathString().StartsWith(ConversionFolder);
}