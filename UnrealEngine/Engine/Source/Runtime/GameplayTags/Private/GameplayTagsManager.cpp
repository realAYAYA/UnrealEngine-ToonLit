// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsManager.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectArray.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectThreadContext.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/CoreDelegates.h"
#include "HAL/IConsoleManager.h"
#include "NativeGameplayTags.h"
#include "AutoRTFM/AutoRTFM.h"

#if WITH_EDITOR
#include "SourceControlHelpers.h"
#include "ISourceControlModule.h"
#include "Editor.h"
#include "PropertyHandle.h"
FSimpleMulticastDelegate UGameplayTagsManager::OnEditorRefreshGameplayTagTree;
#endif

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include "Templates/Function.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/Async.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayTagsManager)

const FName UGameplayTagsManager::NAME_Categories("Categories");
const FName UGameplayTagsManager::NAME_GameplayTagFilter("GameplayTagFilter");

#define LOCTEXT_NAMESPACE "GameplayTagManager"

DECLARE_CYCLE_STAT(TEXT("Load Gameplay Tags"), STAT_GameplayTags_LoadGameplayTags, STATGROUP_GameplayTags);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Add Tag *.ini Search Path"), STAT_GameplayTags_AddTagIniSearchPath, STATGROUP_GameplayTags);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Remove Tag *.ini Search Path"), STAT_GameplayTags_RemoveTagIniSearchPath, STATGROUP_GameplayTags);

#if !(UE_BUILD_SHIPPING)

static FAutoConsoleCommand PrintReplicationIndicesCommand(
	TEXT("GameplayTags.PrintReplicationIndicies"),
	TEXT("Prints the index assigned to each tag for fast network replication."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UGameplayTagsManager::Get().PrintReplicationIndices();
	})
);

#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static FAutoConsoleCommand PrintReplicationFrequencyReportCommand(
	TEXT("GameplayTags.PrintReplicationFrequencyReport"),
	TEXT("Prints the frequency each tag is replicated."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		UGameplayTagsManager::Get().PrintReplicationFrequencyReport();
	})
);

#endif

struct FCompareFGameplayTagNodeByTag
{
	FORCEINLINE bool operator()(const TSharedPtr<FGameplayTagNode>& A, const TSharedPtr<FGameplayTagNode>& B) const
	{
		// Note: GetSimpleTagName() is not good enough here. The individual tag nodes are share frequently (E.g, Dog.Tail, Cat.Tail have sub nodes with the same simple tag name)
		// Compare with equal FNames will look at the backing number/indices to the FName. For FNames used elsewhere, like "A" for example, this can cause non determinism in platforms
		// (For example if static order initialization differs on two platforms, the "version" of the "A" FName that two places get could be different, causing this comparison to also be)
		return (A->GetCompleteTagName().Compare(B->GetCompleteTagName())) < 0;
	}
};

namespace GameplayTagUtil
{
	static void GetRestrictedConfigsFromIni(const FString& IniFilePath, TArray<FRestrictedConfigInfo>& OutRestrictedConfigs)
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(IniFilePath);

		TArray<FString> IniConfigStrings;
		if (ConfigFile.GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("RestrictedConfigFiles"), IniConfigStrings))
		{
			for (const FString& ConfigString : IniConfigStrings)
			{
				FRestrictedConfigInfo Config;
				if (FRestrictedConfigInfo::StaticStruct()->ImportText(*ConfigString, &Config, nullptr, PPF_None, nullptr, FRestrictedConfigInfo::StaticStruct()->GetName()))
				{
					OutRestrictedConfigs.Add(Config);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////
// FGameplayTagSource

static const FName NAME_Native = FName(TEXT("Native"));
static const FName NAME_DefaultGameplayTagsIni("DefaultGameplayTags.ini");

FString FGameplayTagSource::GetConfigFileName() const
{
	if (SourceTagList)
	{
		return SourceTagList->ConfigFileName;
	}
	if (SourceRestrictedTagList)
	{
		return SourceRestrictedTagList->ConfigFileName;
	}

	return FString();
}

FName FGameplayTagSource::GetNativeName()
{
	return NAME_Native;
}

FName FGameplayTagSource::GetDefaultName()
{
	return NAME_DefaultGameplayTagsIni;
}

#if WITH_EDITOR
static const FName NAME_TransientEditor("TransientEditor");

FName FGameplayTagSource::GetFavoriteName()
{
	return GetDefault<UGameplayTagsDeveloperSettings>()->FavoriteTagSource;
}

void FGameplayTagSource::SetFavoriteName(FName TagSourceToFavorite)
{
	UGameplayTagsDeveloperSettings* MutableSettings = GetMutableDefault<UGameplayTagsDeveloperSettings>();

	if (MutableSettings->FavoriteTagSource != TagSourceToFavorite)
	{
		MutableSettings->Modify();
		MutableSettings->FavoriteTagSource = TagSourceToFavorite;

		FPropertyChangedEvent ChangeEvent(MutableSettings->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UGameplayTagsDeveloperSettings, FavoriteTagSource)), EPropertyChangeType::ValueSet);
		MutableSettings->PostEditChangeProperty(ChangeEvent);
		
		MutableSettings->SaveConfig();
	}
}

FName FGameplayTagSource::GetTransientEditorName()
{
	return NAME_TransientEditor;
}
#endif

//////////////////////////////////////////////////////////////////////
// UGameplayTagsManager

UGameplayTagsManager* UGameplayTagsManager::SingletonManager = nullptr;

UGameplayTagsManager::UGameplayTagsManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFastReplication = false;
	bShouldWarnOnInvalidTags = true;
	bShouldClearInvalidTags = false;
	bDoneAddingNativeTags = false;
	bShouldAllowUnloadingTags = false;
	NetIndexFirstBitSegment = 16;
	NetIndexTrueBitNum = 16;
	NumBitsForContainerSize = 6;
	NetworkGameplayTagNodeIndexHash = 0;
}

// Enable to turn on detailed startup logging
#define GAMEPLAYTAGS_VERBOSE 0

#if STATS && GAMEPLAYTAGS_VERBOSE
#define SCOPE_LOG_GAMEPLAYTAGS(Name) SCOPE_LOG_TIME_IN_SECONDS(Name, nullptr)
#else
#define SCOPE_LOG_GAMEPLAYTAGS(Name)
#endif

void UGameplayTagsManager::LoadGameplayTagTables(bool bAllowAsyncLoad)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayTags_LoadGameplayTags);

	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
	GameplayTagTables.Empty();

#if !WITH_EDITOR
	// If we're a cooked build and in a safe spot, start an async load so we can pipeline it
	if (bAllowAsyncLoad && !IsLoading() && MutableDefault->GameplayTagTableList.Num() > 0)
	{
		for (FSoftObjectPath DataTablePath : MutableDefault->GameplayTagTableList)
		{
			LoadPackageAsync(DataTablePath.GetLongPackageName());
		}

		return;
	}
#endif // !WITH_EDITOR

	SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::LoadGameplayTagTables"));
	for (FSoftObjectPath DataTablePath : MutableDefault->GameplayTagTableList)
	{
		UDataTable* TagTable = LoadObject<UDataTable>(nullptr, *DataTablePath.ToString(), nullptr, LOAD_None, nullptr);

		// Handle case where the module is dynamically-loaded within a LoadPackage stack, which would otherwise
		// result in the tag table not having its RowStruct serialized in time. Without the RowStruct, the tags manager
		// will not be initialized correctly.
		if (TagTable)
		{
			FLinkerLoad* TagLinker = TagTable->GetLinker();
			if (TagLinker)
			{
				TagTable->GetLinker()->Preload(TagTable);
			}
		}
		GameplayTagTables.Add(TagTable);
	}
}

void UGameplayTagsManager::AddTagIniSearchPath(const FString& RootDir)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GameplayTags_AddTagIniSearchPath);

	FGameplayTagSearchPathInfo* PathInfo = RegisteredSearchPaths.Find(RootDir);

	if (!PathInfo)
	{
		PathInfo = &RegisteredSearchPaths.FindOrAdd(RootDir);
	}

	if (!PathInfo->bWasSearched)
	{
		PathInfo->Reset();
		
		// Read all tags from the ini
		TArray<FString> FilesInDirectory;
		IFileManager::Get().FindFilesRecursive(FilesInDirectory, *RootDir, TEXT("*.ini"), true, false);

		if (FilesInDirectory.Num() > 0)
		{
			FilesInDirectory.Sort();

			for (const FString& IniFilePath : FilesInDirectory)
			{
				const FName TagSource = FName(*FPaths::GetCleanFilename(IniFilePath));
				PathInfo->SourcesInPath.Add(TagSource);
				PathInfo->TagIniList.Add(FConfigCacheIni::NormalizeConfigIniPath(IniFilePath));
			}
		}
		
		PathInfo->bWasSearched = true;
	}

	if (!PathInfo->bWasAddedToTree)
	{
		for (const FString& IniFilePath : PathInfo->TagIniList)
		{
			TArray<FRestrictedConfigInfo> IniRestrictedConfigs;
			GameplayTagUtil::GetRestrictedConfigsFromIni(IniFilePath, IniRestrictedConfigs);
			const FString IniDirectory = FPaths::GetPath(IniFilePath);
			for (const FRestrictedConfigInfo& Config : IniRestrictedConfigs)
			{
				const FString RestrictedFileName = FString::Printf(TEXT("%s/%s"), *IniDirectory, *Config.RestrictedConfigName);
				AddRestrictedGameplayTagSource(RestrictedFileName);
			}
		}

		AddTagsFromAdditionalLooseIniFiles(PathInfo->TagIniList);

		PathInfo->bWasAddedToTree = true;

		HandleGameplayTagTreeChanged(false);
	}
}

bool UGameplayTagsManager::RemoveTagIniSearchPath(const FString& RootDir)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GameplayTags_RemoveTagIniSearchPath);

	if (!ShouldUnloadTags())
	{
		// Can't unload at all
		return false;
	}

	FGameplayTagSearchPathInfo* PathInfo = RegisteredSearchPaths.Find(RootDir);

	if (PathInfo)
	{
		// Clear out the path and then recreate the tree
		RegisteredSearchPaths.Remove(RootDir);

		HandleGameplayTagTreeChanged(true);

		return true;
	}
	return false;
}

void UGameplayTagsManager::GetTagSourceSearchPaths(TArray<FString>& OutPaths)
{
	OutPaths.Reset();
	RegisteredSearchPaths.GenerateKeyArray(OutPaths);
}

int32 UGameplayTagsManager::GetNumTagSourceSearchPaths()
{
	return RegisteredSearchPaths.Num();
}

void UGameplayTagsManager::AddRestrictedGameplayTagSource(const FString& FileName)
{
	FName TagSource = FName(*FPaths::GetCleanFilename(FileName));
	if (TagSource == NAME_None)
	{
		return;
	}

	if (RestrictedGameplayTagSourceNames.Contains(TagSource))
	{
		// Was already added on this pass
		return;
	}

	RestrictedGameplayTagSourceNames.Add(TagSource);
	FGameplayTagSource* FoundSource = FindOrAddTagSource(TagSource, EGameplayTagSourceType::RestrictedTagList);

	// Make sure we have regular tag sources to match the restricted tag sources but don't try to read any tags from them yet.
	FindOrAddTagSource(TagSource, EGameplayTagSourceType::TagList);

	if (FoundSource && FoundSource->SourceRestrictedTagList)
	{
		FoundSource->SourceRestrictedTagList->LoadConfig(URestrictedGameplayTagsList::StaticClass(), *FileName);

#if WITH_EDITOR
		if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenario since this would break compat with noneditor cooked builds
		{
			FoundSource->SourceRestrictedTagList->SortTags();
		}
#endif
		for (const FRestrictedGameplayTagTableRow& TableRow : FoundSource->SourceRestrictedTagList->RestrictedGameplayTagList)
		{
			AddTagTableRow(TableRow, TagSource, true);
		}
	}
}

void UGameplayTagsManager::AddTagsFromAdditionalLooseIniFiles(const TArray<FString>& IniFileList)
{
	// Read all tags from the ini
	for (const FString& IniFilePath : IniFileList)
	{
		const FName TagSource = FName(*FPaths::GetCleanFilename(IniFilePath));

		// skip the restricted tag files
		if (RestrictedGameplayTagSourceNames.Contains(TagSource))
		{
			continue;
		}

		FGameplayTagSource* FoundSource = FindOrAddTagSource(TagSource, EGameplayTagSourceType::TagList);

		UE_CLOG(GAMEPLAYTAGS_VERBOSE, LogGameplayTags, Display, TEXT("Loading Tag File: %s"), *IniFilePath);

		if (FoundSource && FoundSource->SourceTagList)
		{
			FoundSource->SourceTagList->ConfigFileName = IniFilePath;

			// Check deprecated locations
			TArray<FString> Tags;
			if (GConfig->GetArray(TEXT("UserTags"), TEXT("GameplayTags"), Tags, IniFilePath))
			{
				for (const FString& Tag : Tags)
				{
					FoundSource->SourceTagList->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*Tag)));
				}
			}
			else
			{
				// Load from new ini
				FoundSource->SourceTagList->LoadConfig(UGameplayTagsList::StaticClass(), *IniFilePath);
			}

#if WITH_EDITOR
			if (GIsEditor || IsRunningCommandlet()) // Sort tags for UI Purposes but don't sort in -game scenario since this would break compat with noneditor cooked builds
			{
				FoundSource->SourceTagList->SortTags();
			}
#endif

			for (const FGameplayTagTableRow& TableRow : FoundSource->SourceTagList->GameplayTagList)
			{
				AddTagTableRow(TableRow, TagSource);
			}
		}
	}
}

void UGameplayTagsManager::ConstructGameplayTagTree()
{
	SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree"));
	TGuardValue<bool> GuardRebuilding(bIsConstructingGameplayTagTree, true);
	if (!GameplayRootTag.IsValid())
	{
		GameplayRootTag = MakeShareable(new FGameplayTagNode());

		UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

		// Copy invalid characters, then add internal ones
		InvalidTagCharacters = MutableDefault->InvalidTagCharacters;
		InvalidTagCharacters.Append(TEXT("\r\n\t"));

		// Add prefixes first
		if (ShouldImportTagsFromINI())
		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: ImportINI prefixes"));

			TArray<FString> RestrictedGameplayTagFiles;
			GetRestrictedTagConfigFiles(RestrictedGameplayTagFiles);
			RestrictedGameplayTagFiles.Sort();

			for (const FString& FileName : RestrictedGameplayTagFiles)
			{
				AddRestrictedGameplayTagSource(FileName);
			}
		}

		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Add native tags"));
			// Add native tags before other tags
			for (FName TagToAdd : LegacyNativeTags)
			{
				AddTagTableRow(FGameplayTagTableRow(TagToAdd), FGameplayTagSource::GetNativeName());
			}

			for (const class FNativeGameplayTag* NativeTag : FNativeGameplayTag::GetRegisteredNativeTags())
			{
				AddTagTableRow(NativeTag->GetGameplayTagTableRow(), NativeTag->GetModuleName());
			}
		}

		// If we didn't load any tables it might be async loading, so load again with a flush
		if (GameplayTagTables.Num() == 0)
		{
			LoadGameplayTagTables(false);
		}
		
		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Construct from data asset"));
			for (UDataTable* DataTable : GameplayTagTables)
			{
				if (DataTable)
				{
					PopulateTreeFromDataTable(DataTable);
				}
			}
		}

		// Create native source
		FindOrAddTagSource(FGameplayTagSource::GetNativeName(), EGameplayTagSourceType::Native);

		if (ShouldImportTagsFromINI())
		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: ImportINI tags"));

			// Copy from deprecated list in DefaultEngine.ini
			TArray<FString> EngineConfigTags;
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("+GameplayTags"), EngineConfigTags, GEngineIni);
			
			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*EngineConfigTag)));
			}

			// Copy from deprecated list in DefaultGamplayTags.ini
			EngineConfigTags.Empty();
			GConfig->GetArray(TEXT("/Script/GameplayTags.GameplayTagsSettings"), TEXT("+GameplayTags"), EngineConfigTags, MutableDefault->GetDefaultConfigFilename());

			for (const FString& EngineConfigTag : EngineConfigTags)
			{
				MutableDefault->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*EngineConfigTag)));
			}

#if WITH_EDITOR
			MutableDefault->SortTags();
#endif

			FName TagSource = FGameplayTagSource::GetDefaultName();
			FGameplayTagSource* DefaultSource = FindOrAddTagSource(TagSource, EGameplayTagSourceType::DefaultTagList);

			for (const FGameplayTagTableRow& TableRow : MutableDefault->GameplayTagList)
			{
				AddTagTableRow(TableRow, TagSource);
			}

			// Make sure default config list is added
			FString DefaultPath = FPaths::ProjectConfigDir() / TEXT("Tags");
			AddTagIniSearchPath(DefaultPath);

			// Refresh any other search paths that need it
			for (TPair<FString, FGameplayTagSearchPathInfo>& Pair : RegisteredSearchPaths)
			{
				if (!Pair.Value.IsValid())
				{
					AddTagIniSearchPath(Pair.Key);
				}
			}
		}

#if WITH_EDITOR
		// Add any transient editor-only tags
		for (FName TransientTag : TransientEditorTags)
		{
			AddTagTableRow(FGameplayTagTableRow(TransientTag), FGameplayTagSource::GetTransientEditorName());
		}
#endif
		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Request common tags"));
			// Grab the commonly replicated tags
			CommonlyReplicatedTags.Empty();
			for (FName TagName : MutableDefault->CommonlyReplicatedTags)
			{
				if (TagName.IsNone())
				{
					// Still being added to the UI
					continue;
				}

				FGameplayTag Tag = RequestGameplayTag(TagName, false);
				if (Tag.IsValid())
				{
					CommonlyReplicatedTags.Add(Tag);
				}
				else
				{
					UE_LOG(LogGameplayTags, Warning, TEXT("%s was found in the CommonlyReplicatedTags list but doesn't appear to be a valid tag!"), *TagName.ToString());
				}
			}

			bUseFastReplication = MutableDefault->FastReplication;
			bShouldWarnOnInvalidTags = MutableDefault->WarnOnInvalidTags;
			bShouldClearInvalidTags = MutableDefault->ClearInvalidTags;
			NumBitsForContainerSize = MutableDefault->NumBitsForContainerSize;
			NetIndexFirstBitSegment = MutableDefault->NetIndexFirstBitSegment;

#if WITH_EDITOR
			if (GIsEditor)
			{
				bShouldAllowUnloadingTags = MutableDefault->AllowEditorTagUnloading;
			}
			else
#endif
			{
				bShouldAllowUnloadingTags = MutableDefault->AllowGameTagUnloading;
			}

		}

		if (ShouldUseFastReplication())
		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: Reconstruct NetIndex"));
			InvalidateNetworkIndex();
		}

		{
			SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::ConstructGameplayTagTree: GameplayTagTreeChangedEvent.Broadcast"));
			BroadcastOnGameplayTagTreeChanged();
		}
	}
}

int32 PrintNetIndiceAssignment = 0;
static FAutoConsoleVariableRef CVarPrintNetIndiceAssignment(TEXT("GameplayTags.PrintNetIndiceAssignment"), PrintNetIndiceAssignment, TEXT("Logs GameplayTag NetIndice assignment"), ECVF_Default );
void UGameplayTagsManager::ConstructNetIndex()
{
	FScopeLock Lock(&GameplayTagMapCritical);

	bNetworkIndexInvalidated = false;

	NetworkGameplayTagNodeIndex.Empty();

	GameplayTagNodeMap.GenerateValueArray(NetworkGameplayTagNodeIndex);

	NetworkGameplayTagNodeIndex.Sort(FCompareFGameplayTagNodeByTag());

	check(CommonlyReplicatedTags.Num() <= NetworkGameplayTagNodeIndex.Num());

	// Put the common indices up front
	for (int32 CommonIdx=0; CommonIdx < CommonlyReplicatedTags.Num(); ++CommonIdx)
	{
		int32 BaseIdx=0;
		FGameplayTag& Tag = CommonlyReplicatedTags[CommonIdx];

		bool Found = false;
		for (int32 findidx=0; findidx < NetworkGameplayTagNodeIndex.Num(); ++findidx)
		{
			if (NetworkGameplayTagNodeIndex[findidx]->GetCompleteTag() == Tag)
			{
				NetworkGameplayTagNodeIndex.Swap(findidx, CommonIdx);
				Found = true;
				break;
			}
		}

		// A non fatal error should have been thrown when parsing the CommonlyReplicatedTags list. If we make it here, something is seriously wrong.
		checkf( Found, TEXT("Tag %s not found in NetworkGameplayTagNodeIndex"), *Tag.ToString() );
	}

	InvalidTagNetIndex = IntCastChecked<uint16, int32>(NetworkGameplayTagNodeIndex.Num() + 1);
	NetIndexTrueBitNum = FMath::CeilToInt(FMath::Log2(static_cast<float>(InvalidTagNetIndex)));
	
	// This should never be smaller than NetIndexTrueBitNum
	NetIndexFirstBitSegment = FMath::Min<int32>(GetDefault<UGameplayTagsSettings>()->NetIndexFirstBitSegment, NetIndexTrueBitNum);

	// This is now sorted and it should be the same on both client and server
	if (NetworkGameplayTagNodeIndex.Num() >= INVALID_TAGNETINDEX)
	{
		ensureMsgf(false, TEXT("Too many tags in dictionary for networking! Remove tags or increase tag net index size"));

		NetworkGameplayTagNodeIndex.SetNum(INVALID_TAGNETINDEX - 1);
	}

	UE_CLOG(PrintNetIndiceAssignment, LogGameplayTags, Display, TEXT("Assigning NetIndices to %d tags."), NetworkGameplayTagNodeIndex.Num() );

	NetworkGameplayTagNodeIndexHash = 0;

	for (FGameplayTagNetIndex i = 0; i < NetworkGameplayTagNodeIndex.Num(); i++)
	{
		if (NetworkGameplayTagNodeIndex[i].IsValid())
		{
			NetworkGameplayTagNodeIndex[i]->NetIndex = i;

			NetworkGameplayTagNodeIndexHash = FCrc::StrCrc32(*NetworkGameplayTagNodeIndex[i]->GetCompleteTagString().ToLower(), NetworkGameplayTagNodeIndexHash);

			UE_CLOG(PrintNetIndiceAssignment, LogGameplayTags, Display, TEXT("Assigning NetIndex (%d) to Tag (%s)"), i, *NetworkGameplayTagNodeIndex[i]->GetCompleteTag().ToString());
		}
		else
		{
			UE_LOG(LogGameplayTags, Warning, TEXT("TagNode Indice %d is invalid!"), i);
		}
	}

	UE_LOG(LogGameplayTags, Log, TEXT("NetworkGameplayTagNodeIndexHash is %x"), NetworkGameplayTagNodeIndexHash);
}

FName UGameplayTagsManager::GetTagNameFromNetIndex(FGameplayTagNetIndex Index) const
{
	VerifyNetworkIndex();

	if (Index >= NetworkGameplayTagNodeIndex.Num())
	{
		// Ensure Index is the invalid index. If its higher than that, then something is wrong.
		ensureMsgf(Index == InvalidTagNetIndex, TEXT("Received invalid tag net index %d! Tag index is out of sync on client!"), Index);
		return NAME_None;
	}
	return NetworkGameplayTagNodeIndex[Index]->GetCompleteTagName();
}

FGameplayTagNetIndex UGameplayTagsManager::GetNetIndexFromTag(const FGameplayTag &InTag) const
{
	VerifyNetworkIndex();

	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(InTag);

	if (GameplayTagNode.IsValid())
	{
		return GameplayTagNode->GetNetIndex();
	}

	return InvalidTagNetIndex;
}

void UGameplayTagsManager::PushDeferOnGameplayTagTreeChangedBroadcast()
{
	++bDeferBroadcastOnGameplayTagTreeChanged;
}

void UGameplayTagsManager::PopDeferOnGameplayTagTreeChangedBroadcast()
{
	if (!--bDeferBroadcastOnGameplayTagTreeChanged && bShouldBroadcastDeferredOnGameplayTagTreeChanged)
	{
		bShouldBroadcastDeferredOnGameplayTagTreeChanged = false;
		IGameplayTagsModule::OnGameplayTagTreeChanged.Broadcast();
	}
}

bool UGameplayTagsManager::ShouldImportTagsFromINI() const
{
	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

	// Deprecated path
	bool ImportFromINI = false;
	if (GConfig->GetBool(TEXT("GameplayTags"), TEXT("ImportTagsFromConfig"), ImportFromINI, GEngineIni))
	{
		if (ImportFromINI)
		{
			MutableDefault->ImportTagsFromConfig = ImportFromINI;
			UE_LOG(LogGameplayTags, Log, TEXT("ImportTagsFromConfig is in a deprecated location, open and save GameplayTag settings to fix"));
		}
		return ImportFromINI;
	}

	return MutableDefault->ImportTagsFromConfig;
}

bool UGameplayTagsManager::ShouldUnloadTags() const
{
#if WITH_EDITOR
	if (bShouldAllowUnloadingTags && GIsEditor && GEngine)
	{
		// Check if we have an active PIE index without linking GEditor, and compare to game setting
		FWorldContext* PIEWorldContext = GEngine->GetWorldContextFromPIEInstance(0);
		UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

		if (PIEWorldContext && !MutableDefault->AllowGameTagUnloading)
		{
			UE_LOG(LogGameplayTags, Warning, TEXT("Ignoring request to unload tags during Play In Editor because AllowGameTagUnloading=false"));
			return false;
		}
	}
#endif

	if (ShouldAllowUnloadingTagsOverride.IsSet())
	{
		return ShouldAllowUnloadingTagsOverride.GetValue();
	}

	return bShouldAllowUnloadingTags;
}

void UGameplayTagsManager::SetShouldUnloadTagsOverride(bool bShouldUnloadTags)
{
	ShouldAllowUnloadingTagsOverride = bShouldUnloadTags;
}

void UGameplayTagsManager::ClearShouldUnloadTagsOverride()
{
	ShouldAllowUnloadingTagsOverride.Reset();
}

void UGameplayTagsManager::SetShouldDeferGameplayTagTreeRebuilds(bool bShouldDeferRebuilds)
{
	ShouldDeferGameplayTagTreeRebuilds = bShouldDeferRebuilds;
}

void UGameplayTagsManager::ClearShouldDeferGameplayTagTreeRebuilds(bool bRebuildTree)
{
	ShouldDeferGameplayTagTreeRebuilds.Reset();

	if (bRebuildTree)
	{
		HandleGameplayTagTreeChanged(true);
	}
}

void UGameplayTagsManager::GetRestrictedTagConfigFiles(TArray<FString>& RestrictedConfigFiles) const
{
	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

	if (MutableDefault)
	{
		for (const FRestrictedConfigInfo& Config : MutableDefault->RestrictedConfigFiles)
		{
			RestrictedConfigFiles.Add(FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *Config.RestrictedConfigName));
		}
	}

	for (const TPair<FString, FGameplayTagSearchPathInfo>& Pair : RegisteredSearchPaths)
	{
		for (const FString& IniFilePath : Pair.Value.TagIniList)
		{
			TArray<FRestrictedConfigInfo> IniRestrictedConfigs;
			GameplayTagUtil::GetRestrictedConfigsFromIni(IniFilePath, IniRestrictedConfigs);
			for (const FRestrictedConfigInfo& Config : IniRestrictedConfigs)
			{
				RestrictedConfigFiles.Add(FString::Printf(TEXT("%s/%s"), *FPaths::GetPath(IniFilePath), *Config.RestrictedConfigName));
			}
		}
	}
}

void UGameplayTagsManager::GetRestrictedTagSources(TArray<const FGameplayTagSource*>& Sources) const
{
	for (const TPair<FName, FGameplayTagSource>& Pair : TagSources)
	{
		if (Pair.Value.SourceType == EGameplayTagSourceType::RestrictedTagList)
		{
			Sources.Add(&Pair.Value);
		}
	}
}

void UGameplayTagsManager::GetOwnersForTagSource(const FString& SourceName, TArray<FString>& OutOwners) const
{
	UGameplayTagsSettings* MutableDefault = GetMutableDefault<UGameplayTagsSettings>();

	if (MutableDefault)
	{
		for (const FRestrictedConfigInfo& Config : MutableDefault->RestrictedConfigFiles)
		{
			if (Config.RestrictedConfigName.Equals(SourceName))
			{
				OutOwners = Config.Owners;
				return;
			}
		}
	}
}

void UGameplayTagsManager::GameplayTagContainerLoaded(FGameplayTagContainer& Container, FProperty* SerializingProperty) const
{
	RedirectTagsForContainer(Container, SerializingProperty);

	if (OnGameplayTagLoadedDelegate.IsBound())
	{
		for (const FGameplayTag& Tag : Container)
		{
			OnGameplayTagLoadedDelegate.Broadcast(Tag);
		}
	}
}

void UGameplayTagsManager::SingleGameplayTagLoaded(FGameplayTag& Tag, FProperty* SerializingProperty) const
{
	RedirectSingleGameplayTag(Tag, SerializingProperty);

	OnGameplayTagLoadedDelegate.Broadcast(Tag);
}

void UGameplayTagsManager::RedirectTagsForContainer(FGameplayTagContainer& Container, FProperty* SerializingProperty) const
{
	TArray<FName> NamesToRemove;
	TArray<const FGameplayTag*> TagsToAdd;

	// First populate the NamesToRemove and TagsToAdd sets by finding tags in the container that have redirects
	for (auto TagIt = Container.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName TagName = TagIt->GetTagName();
		const FGameplayTag* NewTag = FGameplayTagRedirectors::Get().RedirectTag(TagName);
		if (NewTag)
		{
			NamesToRemove.Add(TagName);
			if (NewTag->IsValid())
			{
				TagsToAdd.Add(NewTag);
			}
		}
#if WITH_EDITOR
		else if (SerializingProperty)
		{
			// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
			FGameplayTag OldTag = RequestGameplayTag(TagName, false);
			if (!OldTag.IsValid())
			{
				if (ShouldWarnOnInvalidTags())
				{
					FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
					UObject* LoadingObject = LoadContext ? LoadContext->SerializedObject : nullptr;
					UE_ASSET_LOG(LogGameplayTags, Warning, *GetPathNameSafe(LoadingObject), TEXT("Invalid GameplayTag %s found in property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
				}

				if (ShouldClearInvalidTags())
				{
					NamesToRemove.Add(TagName);
				}
			}
		}
#endif
	}

	// Remove all tags from the NamesToRemove set
	for (FName RemoveName : NamesToRemove)
	{
		Container.RemoveTag(FGameplayTag(RemoveName));
	}

	// Add all tags from the TagsToAdd set
	for (const FGameplayTag* AddTag : TagsToAdd)
	{
		Container.AddTag(*AddTag);
	}
}

void UGameplayTagsManager::RedirectSingleGameplayTag(FGameplayTag& Tag, FProperty* SerializingProperty) const
{
	const FName TagName = Tag.GetTagName();
	if (const FGameplayTag* NewTag = FGameplayTagRedirectors::Get().RedirectTag(TagName))
	{
		if (NewTag->IsValid())
		{
			Tag = *NewTag;
		}
	}
#if WITH_EDITOR
	else if (!TagName.IsNone() && SerializingProperty)
	{
		// Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds
		FGameplayTag OldTag = RequestGameplayTag(TagName, false);
		if (!OldTag.IsValid())
		{
			if (ShouldWarnOnInvalidTags())
			{
				FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
				UObject* LoadingObject = LoadContext ? LoadContext->SerializedObject : nullptr;
				UE_ASSET_LOG(LogGameplayTags, Warning, *GetPathNameSafe(LoadingObject), TEXT("Invalid GameplayTag %s found in property %s."), *TagName.ToString(), *GetPathNameSafe(SerializingProperty));
			}
			
			if (ShouldClearInvalidTags())
			{
				Tag.TagName = NAME_None;
			}
		}
	}
#endif
}

bool UGameplayTagsManager::ImportSingleGameplayTag(FGameplayTag& Tag, FName ImportedTagName, bool bImportFromSerialize) const
{
	// None is always valid, no need to do any real work.
	if (ImportedTagName == NAME_None)
	{
		return true;
	}

	bool bRetVal = false;
	if (const FGameplayTag* RedirectedTag = FGameplayTagRedirectors::Get().RedirectTag(ImportedTagName))
	{
		Tag = *RedirectedTag;
		bRetVal = true;
	}
	else if (ValidateTagCreation(ImportedTagName))
	{
		// The tag name is valid
		Tag.TagName = ImportedTagName;
		bRetVal = true;
	}

	if (!bRetVal && bImportFromSerialize && !ImportedTagName.IsNone())
	{
#if WITH_EDITOR
		if (ShouldWarnOnInvalidTags())
		{
			FUObjectSerializeContext* LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
			UObject* LoadingObject = LoadContext ? LoadContext->SerializedObject : nullptr;
			if (LoadingObject)
			{
				// If this is a serialize with a real object and it failed to find the tag, warn about it
				UE_ASSET_LOG(LogGameplayTags, Warning, *GetPathNameSafe(LoadingObject), TEXT("Invalid GameplayTag %s found in object %s."), *ImportedTagName.ToString(), *LoadingObject->GetName());
			}
		}

		// Always keep invalid tags in cooked game to be consistent with properties
		if (!ShouldClearInvalidTags())
#endif
		{
			// For imported tags that are part of a serialize, leave invalid ones the same way normal serialization does to avoid data loss
			Tag.TagName = ImportedTagName;
			bRetVal = true;
		}
	}

	if (bRetVal)
	{
		OnGameplayTagLoadedDelegate.Broadcast(Tag);
	}
	else
	{
		// No valid tag established in this attempt
		Tag.TagName = NAME_None;
	}

	return bRetVal;
}

void UGameplayTagsManager::InitializeManager()
{
	check(!SingletonManager);
	SCOPED_BOOT_TIMING("UGameplayTagsManager::InitializeManager");
	SCOPE_LOG_TIME_IN_SECONDS(TEXT("UGameplayTagsManager::InitializeManager"), nullptr);

	SingletonManager = NewObject<UGameplayTagsManager>(GetTransientPackage(), NAME_None);
	SingletonManager->AddToRoot();

	UGameplayTagsSettings* MutableDefault = nullptr;
	{
		SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::InitializeManager: Load settings"));
		MutableDefault = GetMutableDefault<UGameplayTagsSettings>();
	}

	{
		SCOPE_LOG_GAMEPLAYTAGS(TEXT("UGameplayTagsManager::InitializeManager: Load deprecated"));

		TArray<FString> GameplayTagTablePaths;
		GConfig->GetArray(TEXT("GameplayTags"), TEXT("+GameplayTagTableList"), GameplayTagTablePaths, GEngineIni);

		// Report deprecation
		if (GameplayTagTablePaths.Num() > 0)
		{
			UE_LOG(LogGameplayTags, Log, TEXT("GameplayTagTableList is in a deprecated location, open and save GameplayTag settings to fix"));
			for (const FString& DataTable : GameplayTagTablePaths)
			{
				MutableDefault->GameplayTagTableList.AddUnique(DataTable);
			}
		}
	}

	SingletonManager->LoadGameplayTagTables(true);
	SingletonManager->ConstructGameplayTagTree();

	// Bind to end of engine init to be done adding native tags
	FCoreDelegates::OnPostEngineInit.AddUObject(SingletonManager, &UGameplayTagsManager::DoneAddingNativeTags);
}

void UGameplayTagsManager::PopulateTreeFromDataTable(class UDataTable* InTable)
{
	checkf(GameplayRootTag.IsValid(), TEXT("ConstructGameplayTagTree() must be called before PopulateTreeFromDataTable()"));
	static const FString ContextString(TEXT("UGameplayTagsManager::PopulateTreeFromDataTable"));
	
	TArray<FGameplayTagTableRow*> TagTableRows;
	InTable->GetAllRows<FGameplayTagTableRow>(ContextString, TagTableRows);

	FName SourceName = InTable->GetOutermost()->GetFName();

	FGameplayTagSource* FoundSource = FindOrAddTagSource(SourceName, EGameplayTagSourceType::DataTable);

	for (const FGameplayTagTableRow* TagRow : TagTableRows)
	{
		if (TagRow)
		{
			AddTagTableRow(*TagRow, SourceName);
		}
	}
}

void UGameplayTagsManager::AddTagTableRow(const FGameplayTagTableRow& TagRow, FName SourceName, bool bIsRestrictedTag)
{
	TSharedPtr<FGameplayTagNode> CurNode = GameplayRootTag;
	TArray<TSharedPtr<FGameplayTagNode>> AncestorNodes;
	bool bAllowNonRestrictedChildren = true;

	const FRestrictedGameplayTagTableRow* RestrictedTagRow = static_cast<const FRestrictedGameplayTagTableRow*>(&TagRow);
	if (bIsRestrictedTag && RestrictedTagRow)
	{
		bAllowNonRestrictedChildren = RestrictedTagRow->bAllowNonRestrictedChildren;
	}

	// Split the tag text on the "." delimiter to establish tag depth and then insert each tag into the gameplay tag tree
	// We try to avoid as many FString->FName conversions as possible as they are slow
	FName OriginalTagName = TagRow.Tag;
	FString FullTagString = OriginalTagName.ToString();

#if WITH_EDITOR
	{
		// In editor builds, validate string
		// These must get fixed up cooking to work properly
		FText ErrorText;
		FString FixedString;

		if (!IsValidGameplayTagString(FullTagString, &ErrorText, &FixedString))
		{
			if (FixedString.IsEmpty())
			{
				// No way to fix it
				UE_LOG(LogGameplayTags, Error, TEXT("Invalid tag %s from source %s: %s!"), *FullTagString, *SourceName.ToString(), *ErrorText.ToString());
				return;
			}
			else
			{
				UE_LOG(LogGameplayTags, Error, TEXT("Invalid tag %s from source %s: %s! Replacing with %s, you may need to modify InvalidTagCharacters"), *FullTagString, *SourceName.ToString(), *ErrorText.ToString(), *FixedString);
				FullTagString = FixedString;
				OriginalTagName = FName(*FixedString);
			}
		}
	}
#endif

	TArray<FString> SubTags;
	FullTagString.ParseIntoArray(SubTags, TEXT("."), true);

	// We will build this back up as we go
	FullTagString.Reset();

	int32 NumSubTags = SubTags.Num();
	bool bHasSeenConflict = false;

	for (int32 SubTagIdx = 0; SubTagIdx < NumSubTags; ++SubTagIdx)
	{
		bool bIsExplicitTag = (SubTagIdx == (NumSubTags - 1));
		FName ShortTagName = *SubTags[SubTagIdx];
		FName FullTagName;

		if (bIsExplicitTag)
		{
			// We already know the final name
			FullTagName = OriginalTagName;
		}
		else if (SubTagIdx == 0)
		{
			// Full tag is the same as short tag, and start building full tag string
			FullTagName = ShortTagName;
			FullTagString = SubTags[SubTagIdx];
		}
		else
		{
			// Add .Tag and use that as full tag
			FullTagString += TEXT(".");
			FullTagString += SubTags[SubTagIdx];

			FullTagName = FName(*FullTagString);
		}
			
		TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();
		int32 InsertionIdx = InsertTagIntoNodeArray(ShortTagName, FullTagName, CurNode, ChildTags, SourceName, TagRow.DevComment, bIsExplicitTag, bIsRestrictedTag, bAllowNonRestrictedChildren);

		CurNode = ChildTags[InsertionIdx];

		// Tag conflicts only affect the editor so we don't look for them in the game
#if WITH_EDITORONLY_DATA
		if (bIsRestrictedTag)
		{
			CurNode->bAncestorHasConflict = bHasSeenConflict;

			// If the sources don't match and the tag is explicit and we should've added the tag explicitly here, we have a conflict
			if (CurNode->GetFirstSourceName() != SourceName && (CurNode->bIsExplicitTag && bIsExplicitTag))
			{
				// mark all ancestors as having a bad descendant
				for (TSharedPtr<FGameplayTagNode> CurAncestorNode : AncestorNodes)
				{
					CurAncestorNode->bDescendantHasConflict = true;
				}

				// mark the current tag as having a conflict
				CurNode->bNodeHasConflict = true;

				// append source names
				CurNode->SourceNames.Add(SourceName);

				// mark all current descendants as having a bad ancestor
				MarkChildrenOfNodeConflict(CurNode);
			}

			// mark any children we add later in this function as having a bad ancestor
			if (CurNode->bNodeHasConflict)
			{
				bHasSeenConflict = true;
			}

			AncestorNodes.Add(CurNode);
		}
#endif
	}
}

void UGameplayTagsManager::MarkChildrenOfNodeConflict(TSharedPtr<FGameplayTagNode> CurNode)
{
#if WITH_EDITORONLY_DATA
	TArray< TSharedPtr<FGameplayTagNode> >& ChildTags = CurNode.Get()->GetChildTagNodes();
	for (TSharedPtr<FGameplayTagNode> ChildNode : ChildTags)
	{
		ChildNode->bAncestorHasConflict = true;
		MarkChildrenOfNodeConflict(ChildNode);
	}
#endif
}

void UGameplayTagsManager::BroadcastOnGameplayTagTreeChanged()
{
	if (bDeferBroadcastOnGameplayTagTreeChanged)
	{
		bShouldBroadcastDeferredOnGameplayTagTreeChanged = true;
	}
	else
	{
		IGameplayTagsModule::OnGameplayTagTreeChanged.Broadcast();
	}
}

void UGameplayTagsManager::HandleGameplayTagTreeChanged(bool bRecreateTree)
{
	// Don't do anything during a reconstruct or before initial native tags are done loading
	if (!bIsConstructingGameplayTagTree && bDoneAddingNativeTags)
	{
		if (bRecreateTree && (!ShouldDeferGameplayTagTreeRebuilds.IsSet() || !ShouldDeferGameplayTagTreeRebuilds.GetValue()))
		{
#if WITH_EDITOR
			if (GIsEditor)
			{
				// In the editor refresh everything
				EditorRefreshGameplayTagTree();
				return;
			}
#endif
			DestroyGameplayTagTree();
			ConstructGameplayTagTree();
		}
		else
		{
			// Refresh if we're done adding tags
			if (ShouldUseFastReplication())
			{
				InvalidateNetworkIndex();
			}

			BroadcastOnGameplayTagTreeChanged();
		}
	}
}

UGameplayTagsManager::~UGameplayTagsManager()
{
	DestroyGameplayTagTree();
	SingletonManager = nullptr;
}

void UGameplayTagsManager::DestroyGameplayTagTree()
{
	FScopeLock Lock(&GameplayTagMapCritical);

	if (GameplayRootTag.IsValid())
	{
		GameplayRootTag->ResetNode();
		GameplayRootTag.Reset();
		GameplayTagNodeMap.Reset();
	}
	RestrictedGameplayTagSourceNames.Reset();

	for (TPair<FString, FGameplayTagSearchPathInfo>& Pair : RegisteredSearchPaths)
	{
		Pair.Value.bWasAddedToTree = false;
	}
}

int32 UGameplayTagsManager::InsertTagIntoNodeArray(FName Tag, FName FullTag, TSharedPtr<FGameplayTagNode> ParentNode, TArray< TSharedPtr<FGameplayTagNode> >& NodeArray, FName SourceName, const FString& DevComment, bool bIsExplicitTag, bool bIsRestrictedTag, bool bAllowNonRestrictedChildren)
{
	int32 FoundNodeIdx = INDEX_NONE;
	int32 WhereToInsert = INDEX_NONE;

	// See if the tag is already in the array

	// LowerBoundBy returns Position of the first element >= Value, may be position after last element in range
	int32 LowerBoundIndex = Algo::LowerBoundBy(NodeArray, Tag,
		[](const TSharedPtr<FGameplayTagNode>& N) -> FName { return N->GetSimpleTagName(); },
		[](const FName& A, const FName& B) { return A != B && UE::ComparisonUtility::CompareWithNumericSuffix(A, B) < 0; });

	if (LowerBoundIndex < NodeArray.Num())
	{
		FGameplayTagNode* CurrNode = NodeArray[LowerBoundIndex].Get();
		if (CurrNode->GetSimpleTagName() == Tag)
		{
			FoundNodeIdx = LowerBoundIndex;
#if WITH_EDITORONLY_DATA
			// If we are explicitly adding this tag then overwrite the existing children restrictions with whatever is in the ini
			// If we restrict children in the input data, make sure we restrict them in the existing node. This applies to explicit and implicitly defined nodes
			if (bAllowNonRestrictedChildren == false || bIsExplicitTag)
			{
				// check if the tag is explicitly being created in more than one place.
				if (CurrNode->bIsExplicitTag && bIsExplicitTag)
				{
					// restricted tags always get added first
					// 
					// There are two possibilities if we're adding a restricted tag. 
					// If the existing tag is non-restricted the restricted tag should take precedence. This may invalidate some child tags of the existing tag.
					// If the existing tag is restricted we have a conflict. This is explicitly not allowed.
					if (bIsRestrictedTag)
					{

					}
				}
				CurrNode->bAllowNonRestrictedChildren = bAllowNonRestrictedChildren;
				CurrNode->bIsExplicitTag = CurrNode->bIsExplicitTag || bIsExplicitTag;
			}
#endif				
		}
		else
		{
			// Insert new node before this
			WhereToInsert = LowerBoundIndex;
		}
	}

	if (FoundNodeIdx == INDEX_NONE)
	{
		if (WhereToInsert == INDEX_NONE)
		{
			// Insert at end
			WhereToInsert = NodeArray.Num();
		}

		// Don't add the root node as parent
		TSharedPtr<FGameplayTagNode> TagNode = MakeShareable(new FGameplayTagNode(Tag, FullTag, ParentNode != GameplayRootTag ? ParentNode : nullptr, bIsExplicitTag, bIsRestrictedTag, bAllowNonRestrictedChildren));

		// Add at the sorted location
		FoundNodeIdx = NodeArray.Insert(TagNode, WhereToInsert);

		FGameplayTag GameplayTag = TagNode->GetCompleteTag();

		// These should always match
		ensure(GameplayTag.GetTagName() == FullTag);

		{
			// This critical section is to handle an issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
			// This function is not generically threadsafe.
			FScopeLock Lock(&GameplayTagMapCritical);
			GameplayTagNodeMap.Add(GameplayTag, TagNode);
		}
	}

#if WITH_EDITOR
	// Set/update editor only data
	NodeArray[FoundNodeIdx]->SourceNames.AddUnique(SourceName);

	if (NodeArray[FoundNodeIdx]->DevComment.IsEmpty() && !DevComment.IsEmpty())
	{
		NodeArray[FoundNodeIdx]->DevComment = DevComment;
	}
#endif

	return FoundNodeIdx;
}

void UGameplayTagsManager::PrintReplicationIndices()
{
	VerifyNetworkIndex();

	UE_LOG(LogGameplayTags, Display, TEXT("::PrintReplicationIndices (TOTAL %d)"), GameplayTagNodeMap.Num());

	FScopeLock Lock(&GameplayTagMapCritical);

	for (auto It : GameplayTagNodeMap)
	{
		FGameplayTag Tag = It.Key;
		TSharedPtr<FGameplayTagNode> Node = It.Value;

		UE_LOG(LogGameplayTags, Display, TEXT("Tag %s NetIndex: %d"), *Tag.ToString(), Node->GetNetIndex());
	}

}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UGameplayTagsManager::PrintReplicationFrequencyReport()
{
	VerifyNetworkIndex();

	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
	UE_LOG(LogGameplayTags, Warning, TEXT("Gameplay Tags Replication Report"));

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated solo:"));
	ReplicationCountMap_SingleTags.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_SingleTags)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}
	
	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nTags replicated in containers:"));
	ReplicationCountMap_Containers.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap_Containers)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	// ---------------------------------------

	UE_LOG(LogGameplayTags, Warning, TEXT("\nAll Tags replicated:"));
	ReplicationCountMap.ValueSort(TGreater<int32>());
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("%s - %d"), *It.Key.ToString(), It.Value);
	}

	TMap<int32, int32> SavingsMap;
	int32 BaselineCost = 0;
	for (int32 Bits=1; Bits < NetIndexTrueBitNum; ++Bits)
	{
		int32 TotalSavings = 0;
		BaselineCost = 0;

		FGameplayTagNetIndex ExpectedNetIndex=0;
		for (auto& It : ReplicationCountMap)
		{
			int32 ExpectedCostBits = 0;
			bool FirstSeg = ExpectedNetIndex < FMath::Pow(2.f, Bits);
			if (FirstSeg)
			{
				// This would fit in the first Bits segment
				ExpectedCostBits = Bits+1;
			}
			else
			{
				// Would go in the second segment, so we pay the +1 cost
				ExpectedCostBits = NetIndexTrueBitNum+1;
			}

			int32 Savings = (NetIndexTrueBitNum - ExpectedCostBits) * It.Value;
			BaselineCost += NetIndexTrueBitNum * It.Value;

			//UE_LOG(LogGameplayTags, Warning, TEXT("[Bits: %d] Tag %s would save %d bits"), Bits, *It.Key.ToString(), Savings);
			ExpectedNetIndex++;
			TotalSavings += Savings;
		}

		SavingsMap.FindOrAdd(Bits) = TotalSavings;
	}

	SavingsMap.ValueSort(TGreater<int32>());
	int32 BestBits = 0;
	for (auto& It : SavingsMap)
	{
		if (BestBits == 0)
		{
			BestBits = It.Key;
		}

		UE_LOG(LogGameplayTags, Warning, TEXT("%d bits would save %d (%.2f)"), It.Key, It.Value, (float)It.Value / (float)BaselineCost);
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("\nSuggested config:"));

	// Write out a nice copy pastable config
	int32 Count=0;
	for (auto& It : ReplicationCountMap)
	{
		UE_LOG(LogGameplayTags, Warning, TEXT("+CommonlyReplicatedTags=%s"), *It.Key.ToString());

		if (Count == FMath::Pow(2.f, BestBits))
		{
			// Print a blank line out, indicating tags after this are not necessary but still may be useful if the user wants to manually edit the list.
			UE_LOG(LogGameplayTags, Warning, TEXT(""));
		}

		if (Count++ >= FMath::Pow(2.f, BestBits+1))
		{
			break;
		}
	}

	UE_LOG(LogGameplayTags, Warning, TEXT("NetIndexFirstBitSegment=%d"), BestBits);

	UE_LOG(LogGameplayTags, Warning, TEXT("================================="));
}

void UGameplayTagsManager::NotifyTagReplicated(FGameplayTag Tag, bool WasInContainer)
{
	ReplicationCountMap.FindOrAdd(Tag)++;

	if (WasInContainer)
	{
		ReplicationCountMap_Containers.FindOrAdd(Tag)++;
	}
	else
	{
		ReplicationCountMap_SingleTags.FindOrAdd(Tag)++;
	}
	
}
#endif

#if WITH_EDITOR

static void RecursiveRootTagSearch(const FString& InFilterString, const TArray<TSharedPtr<FGameplayTagNode> >& GameplayRootTags, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray)
{
	FString CurrentFilter, RestOfFilter;
	if (!InFilterString.Split(TEXT("."), &CurrentFilter, &RestOfFilter))
	{
		CurrentFilter = InFilterString;
	}

	for (int32 iTag = 0; iTag < GameplayRootTags.Num(); ++iTag)
	{
		FString RootTagName = GameplayRootTags[iTag].Get()->GetSimpleTagName().ToString();

		if (RootTagName == CurrentFilter)
		{
			if (RestOfFilter.IsEmpty())
			{
				// We've reached the end of the filter, add tags
				OutTagArray.Add(GameplayRootTags[iTag]);
			}
			else
			{
				// Recurse into our children
				RecursiveRootTagSearch(RestOfFilter, GameplayRootTags[iTag]->GetChildTagNodes(), OutTagArray);
			}
		}		
	}
}

void UGameplayTagsManager::GetFilteredGameplayRootTags(const FString& InFilterString, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const
{
	TArray<FString> PreRemappedFilters;
	TArray<FString> Filters;
	TArray<TSharedPtr<FGameplayTagNode>>& GameplayRootTags = GameplayRootTag->GetChildTagNodes();

	OutTagArray.Empty();
	if( InFilterString.ParseIntoArray( PreRemappedFilters, TEXT( "," ), true ) > 0 )
	{
		const UGameplayTagsSettings* CDO = GetDefault<UGameplayTagsSettings>();
		for (FString& Str : PreRemappedFilters)
		{
			bool Remapped = false;
			for (const FGameplayTagCategoryRemap& RemapInfo : CDO->CategoryRemapping)
			{
				if (RemapInfo.BaseCategory == Str)
				{
					Remapped = true;
					Filters.Append(RemapInfo.RemapCategories);
				}
			}
			if (Remapped == false)
			{
				Filters.Add(Str);
			}
		}		

		// Check all filters in the list
		for (int32 iFilter = 0; iFilter < Filters.Num(); ++iFilter)
		{
			RecursiveRootTagSearch(Filters[iFilter], GameplayRootTags, OutTagArray);
		}

		if (OutTagArray.Num() == 0)
		{
			// We had filters but nothing matched. Ignore the filters.
			// This makes sense to do with engine level filters that games can optionally specify/override.
			// We never want to impose tag structure on projects, but still give them the ability to do so for their project.
			OutTagArray = GameplayRootTags;
		}

	}
	else
	{
		// No Filters just return them all
		OutTagArray = GameplayRootTags;
	}
}

FString UGameplayTagsManager::GetCategoriesMetaFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	// Global delegate override. Useful for parent structs that want to override tag categories based on their data (e.g. not static property meta data)
	FString DelegateOverrideString;
	OnGetCategoriesMetaFromPropertyHandle.Broadcast(PropertyHandle, DelegateOverrideString);
	if (DelegateOverrideString.IsEmpty() == false)
	{
		return DelegateOverrideString;
	}

	return StaticGetCategoriesMetaFromPropertyHandle(PropertyHandle);
}

FString UGameplayTagsManager::StaticGetCategoriesMetaFromPropertyHandle(TSharedPtr<class IPropertyHandle> PropertyHandle)
{
	FString Categories;

	while(PropertyHandle.IsValid())
	{
		if (FProperty* Property = PropertyHandle->GetProperty())
		{
			/**
			 *	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories="GameplayCue"))
			 *	FGameplayTag GameplayCueTag;
			 */
			Categories = GetCategoriesMetaFromField(Property);
			if (!Categories.IsEmpty())
			{
				break;
			}

			/**
			 *	USTRUCT(meta=(Categories="EventKeyword"))
			 *	struct FGameplayEventKeywordTag : public FGameplayTag
			 */
			if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				Categories = GetCategoriesMetaFromField<UScriptStruct>(StructProperty->Struct);
				if (!Categories.IsEmpty())
				{
					break;
				}
			}

			/**	TArray<FGameplayEventKeywordTag> QualifierTagTestList; */
			if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				Categories = GetCategoriesMetaFromField(ArrayProperty->Inner);
				if (!Categories.IsEmpty())
				{
					break;
				}
			}

			/**	TMap<FGameplayTag, ValueType> GameplayTagMap; */
			if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
			{
				Categories = GetCategoriesMetaFromField(MapProperty->KeyProp);
				if (!Categories.IsEmpty())
				{
					break;
				}
			}
		}

		TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle();
		
		if (ParentHandle.IsValid())
		{
			/** Check if the parent handle's base class is of the same class. It's possible the current child property is from a subobject which in that case we probably want to ignore
			  * any meta category restrictions coming from any parent properties. A subobject's gameplay tag property without any declared meta categories should stay that way. */
			if (PropertyHandle->GetOuterBaseClass() != ParentHandle->GetOuterBaseClass())
			{
				break;
			}
		}

		PropertyHandle = ParentHandle;
	}
	
	return Categories;
}

FString UGameplayTagsManager::GetCategoriesMetaFromFunction(const UFunction* ThisFunction, FName ParamName /** = NAME_None */)
{
	FString FilterString;
	if (ThisFunction)
	{
		// If a param name was specified, check it first for UPARAM metadata
		if (!ParamName.IsNone())
		{
			FProperty* ParamProp = FindFProperty<FProperty>(ThisFunction, ParamName);
			if (ParamProp)
			{
				FilterString = GetCategoriesMetaFromField(ParamProp);
			}
		}

		// No filter found so far, fall back to UFUNCTION-level
		if (FilterString.IsEmpty())
		{
			FilterString = GetCategoriesMetaFromField(ThisFunction);
		}
	}

	return FilterString;
}

void UGameplayTagsManager::GetAllTagsFromSource(FName TagSource, TArray< TSharedPtr<FGameplayTagNode> >& OutTagArray) const
{
	FScopeLock Lock(&GameplayTagMapCritical);

	for (const TPair<FGameplayTag, TSharedPtr<FGameplayTagNode>>& NodePair : GameplayTagNodeMap)
	{
		if (NodePair.Value->SourceNames.Contains(TagSource))
		{
			OutTagArray.Add(NodePair.Value);
		}
	}
}

bool UGameplayTagsManager::IsDictionaryTag(FName TagName) const
{
	TSharedPtr<FGameplayTagNode> Node = FindTagNode(TagName);
	if (Node.IsValid() && Node->bIsExplicitTag)
	{
		return true;
	}

	return false;
}

bool UGameplayTagsManager::GetTagEditorData(FName TagName, FString& OutComment, FName& OutFirstTagSource, bool& bOutIsTagExplicit, bool &bOutIsRestrictedTag, bool &bOutAllowNonRestrictedChildren) const
{
	TSharedPtr<FGameplayTagNode> Node = FindTagNode(TagName);
	if (Node.IsValid())
	{
		OutComment = Node->DevComment;
		OutFirstTagSource = Node->GetFirstSourceName();
		bOutIsTagExplicit = Node->bIsExplicitTag;
		bOutIsRestrictedTag = Node->bIsRestrictedTag;
		bOutAllowNonRestrictedChildren = Node->bAllowNonRestrictedChildren;
		return true;
	}
	return false;
}

bool UGameplayTagsManager::GetTagEditorData(FName TagName, FString& OutComment, TArray<FName>& OutTagSources, bool& bOutIsTagExplicit, bool &bOutIsRestrictedTag, bool &bOutAllowNonRestrictedChildren) const
{
	TSharedPtr<FGameplayTagNode> Node = FindTagNode(TagName);
	if (Node.IsValid())
	{
		OutComment = Node->DevComment;
		OutTagSources = Node->GetAllSourceNames();
		bOutIsTagExplicit = Node->bIsExplicitTag;
		bOutIsRestrictedTag = Node->bIsRestrictedTag;
		bOutAllowNonRestrictedChildren = Node->bAllowNonRestrictedChildren;
		return true;
	}
	return false;
}

#if WITH_EDITOR

void UGameplayTagsManager::EditorRefreshGameplayTagTree()
{
	if (!EditorRefreshGameplayTagTreeSuspendTokens.IsEmpty())
	{
		bEditorRefreshGameplayTagTreeRequestedDuringSuspend = true;
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UGameplayTagsManager::EditorRefreshGameplayTagTree)

	// Clear out source path info so it will reload off disk
	for (TPair<FString, FGameplayTagSearchPathInfo>& Pair : RegisteredSearchPaths)
	{
		Pair.Value.bWasSearched = false;
	}

	DestroyGameplayTagTree();
	LoadGameplayTagTables(false);
	ConstructGameplayTagTree();

	OnEditorRefreshGameplayTagTree.Broadcast();
}

void UGameplayTagsManager::SuspendEditorRefreshGameplayTagTree(FGuid SuspendToken)
{
	EditorRefreshGameplayTagTreeSuspendTokens.Add(SuspendToken);
}

void UGameplayTagsManager::ResumeEditorRefreshGameplayTagTree(FGuid SuspendToken)
{
	EditorRefreshGameplayTagTreeSuspendTokens.Remove(SuspendToken);
	if (EditorRefreshGameplayTagTreeSuspendTokens.IsEmpty() && bEditorRefreshGameplayTagTreeRequestedDuringSuspend)
	{
		bEditorRefreshGameplayTagTreeRequestedDuringSuspend = false;
		EditorRefreshGameplayTagTree();
;	}
}

#endif //if WITH_EDITOR

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagChildrenInDictionary(const FGameplayTag& GameplayTag) const
{
	// Note this purposefully does not include the passed in GameplayTag in the container.
	FGameplayTagContainer TagContainer;

	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		AddChildrenTags(TagContainer, GameplayTagNode, true, true);
	}
	return TagContainer;
}

#if WITH_EDITORONLY_DATA
FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagDirectDescendantsInDictionary(const FGameplayTag& GameplayTag, EGameplayTagSelectionType SelectionType) const
{
	bool bIncludeRestrictedTags = (SelectionType == EGameplayTagSelectionType::RestrictedOnly || SelectionType == EGameplayTagSelectionType::All);
	bool bIncludeNonRestrictedTags = (SelectionType == EGameplayTagSelectionType::NonRestrictedOnly || SelectionType == EGameplayTagSelectionType::All);

	// Note this purposefully does not include the passed in GameplayTag in the container.
	FGameplayTagContainer TagContainer;

	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		TArray< TSharedPtr<FGameplayTagNode> >& ChildrenNodes = GameplayTagNode->GetChildTagNodes();
		int32 CurrArraySize = ChildrenNodes.Num();
		for (int32 Idx = 0; Idx < CurrArraySize; ++Idx)
		{
			TSharedPtr<FGameplayTagNode> ChildNode = ChildrenNodes[Idx];
			if (ChildNode.IsValid())
			{
				// if the tag isn't in the dictionary, add its children to the list
				if (ChildNode->GetFirstSourceName() == NAME_None)
				{
					TArray< TSharedPtr<FGameplayTagNode> >& GrandChildrenNodes = ChildNode->GetChildTagNodes();
					ChildrenNodes.Append(GrandChildrenNodes);
					CurrArraySize = ChildrenNodes.Num();
				}
				else
				{
					// this tag is in the dictionary so add it to the list
					if ((ChildNode->bIsRestrictedTag && bIncludeRestrictedTags) ||
						(!ChildNode->bIsRestrictedTag && bIncludeNonRestrictedTags))
					{
						TagContainer.AddTag(ChildNode->GetCompleteTag());
					}
				}
			}
		}
	}
	return TagContainer;
}
#endif // WITH_EDITORONLY_DATA

void UGameplayTagsManager::NotifyGameplayTagDoubleClickedEditor(FString TagName)
{
	FGameplayTag Tag = RequestGameplayTag(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherGameplayTagDoubleClickedEditor.Broadcast(Tag, Delegate);
		Delegate.Broadcast();
	}
}

bool UGameplayTagsManager::ShowGameplayTagAsHyperLinkEditor(FString TagName)
{
	FGameplayTag Tag = RequestGameplayTag(FName(*TagName), false);
	if(Tag.IsValid())
	{
		FSimpleMulticastDelegate Delegate;
		OnGatherGameplayTagDoubleClickedEditor.Broadcast(Tag, Delegate);
		return Delegate.IsBound();
	}
	return false;
}

#endif // WITH_EDITOR

const FGameplayTagSource* UGameplayTagsManager::FindTagSource(FName TagSourceName) const
{
	return TagSources.Find(TagSourceName);
}

FGameplayTagSource* UGameplayTagsManager::FindTagSource(FName TagSourceName)
{
	return TagSources.Find(TagSourceName);
}

void UGameplayTagsManager::FindTagsWithSource(FStringView PackageNameOrPath, TArray<FGameplayTag>& OutTags) const
{
	for (const auto& TagSourceEntry : TagSources)
	{
		const FGameplayTagSource& Source = TagSourceEntry.Value;
		
		FString SourcePackagePath;
		switch (Source.SourceType)
		{
		case EGameplayTagSourceType::TagList:
			if (Source.SourceTagList)
			{
				const FString ContentFilePath = FPaths::GetPath(Source.SourceTagList->ConfigFileName) / TEXT("../../Content/");
				FString RootContentPath;
				if (FPackageName::TryConvertFilenameToLongPackageName(ContentFilePath, RootContentPath))
				{
					SourcePackagePath = *RootContentPath;
				}
			}
			break;
		case EGameplayTagSourceType::DataTable:
			SourcePackagePath = Source.SourceName.ToString();
			break;
		case EGameplayTagSourceType::Native:
			SourcePackagePath = Source.SourceName.ToString();
		default:
			break;
		}

		if (SourcePackagePath.StartsWith(PackageNameOrPath))
		{
			if (Source.SourceTagList)
			{
				for (const FGameplayTagTableRow& Row : Source.SourceTagList->GameplayTagList)
				{
					OutTags.Add(FGameplayTag(Row.Tag));
				}
			}
		}
	}
}

void UGameplayTagsManager::FindTagSourcesWithType(EGameplayTagSourceType TagSourceType, TArray<const FGameplayTagSource*>& OutArray) const
{
	for (auto TagSourceIt = TagSources.CreateConstIterator(); TagSourceIt; ++TagSourceIt)
	{
		if (TagSourceIt.Value().SourceType == TagSourceType)
		{
			OutArray.Add(&TagSourceIt.Value());
		}
	}
}

FGameplayTagSource* UGameplayTagsManager::FindOrAddTagSource(FName TagSourceName, EGameplayTagSourceType SourceType, const FString& RootDirToUse)
{
	FGameplayTagSource* FoundSource = FindTagSource(TagSourceName);
	if (FoundSource)
	{
		if (SourceType == FoundSource->SourceType)
		{
			return FoundSource;
		}

		return nullptr;
	}

	// Need to make a new one

	FGameplayTagSource* NewSource = &TagSources.Add(TagSourceName, FGameplayTagSource(TagSourceName, SourceType));

	if (SourceType == EGameplayTagSourceType::Native)
	{
		NewSource->SourceTagList = NewObject<UGameplayTagsList>(this, TagSourceName, RF_Transient);
	}
	else if (SourceType == EGameplayTagSourceType::DefaultTagList)
	{
		NewSource->SourceTagList = GetMutableDefault<UGameplayTagsSettings>();
	}
	else if (SourceType == EGameplayTagSourceType::TagList)
	{
		NewSource->SourceTagList = NewObject<UGameplayTagsList>(this, TagSourceName, RF_Transient);
		if (RootDirToUse.IsEmpty())
		{
			NewSource->SourceTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
		}
		else
		{
			// Use custom root and add the root to the search list for later refresh
			NewSource->SourceTagList->ConfigFileName = RootDirToUse / *TagSourceName.ToString();
			RegisteredSearchPaths.FindOrAdd(RootDirToUse);
		}
		if (GUObjectArray.IsDisregardForGC(this))
		{
			NewSource->SourceTagList->AddToRoot();
		}
	}
	else if (SourceType == EGameplayTagSourceType::RestrictedTagList)
	{
		NewSource->SourceRestrictedTagList = NewObject<URestrictedGameplayTagsList>(this, TagSourceName, RF_Transient);
		if (RootDirToUse.IsEmpty())
		{
			NewSource->SourceRestrictedTagList->ConfigFileName = FString::Printf(TEXT("%sTags/%s"), *FPaths::SourceConfigDir(), *TagSourceName.ToString());
		}
		else
		{			
			// Use custom root and add the root to the search list for later refresh
			NewSource->SourceRestrictedTagList->ConfigFileName = RootDirToUse / *TagSourceName.ToString();
			RegisteredSearchPaths.FindOrAdd(RootDirToUse);
		}
		if (GUObjectArray.IsDisregardForGC(this))
		{
			NewSource->SourceRestrictedTagList->AddToRoot();
		}
	}

	return NewSource;
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::RequestGameplayTag"), STAT_UGameplayTagsManager_RequestGameplayTag, STATGROUP_GameplayTags);

void UGameplayTagsManager::RequestGameplayTagContainer(const TArray<FString>& TagStrings, FGameplayTagContainer& OutTagsContainer, bool bErrorIfNotFound/*=true*/) const
{
	for (const FString& CurrentTagString : TagStrings)
	{
		FGameplayTag RequestedTag = RequestGameplayTag(FName(*(CurrentTagString.TrimStartAndEnd())), bErrorIfNotFound);
		if (RequestedTag.IsValid())
		{
			OutTagsContainer.AddTag(RequestedTag);
		}
	}
}

FGameplayTag UGameplayTagsManager::RequestGameplayTag(FName TagName, bool ErrorIfNotFound) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_RequestGameplayTag);

	// This critical section is to handle an issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&GameplayTagMapCritical);

	// Check if there are redirects for this tag. If so and the redirected tag is in the node map, return it.
	// Redirects take priority, even if the tag itself may exist.
	if (const FGameplayTag* RedirectedTag = FGameplayTagRedirectors::Get().RedirectTag(TagName))
	{
		// Check if the redirected tag exists in the node map
		if (GameplayTagNodeMap.Contains(*RedirectedTag))
		{
			return *RedirectedTag;
		}

		// The tag that was redirected to was not found. Error if that was requested.
		if (ErrorIfNotFound)
		{
			static TSet<FName> MissingRedirectedTagNames;
			if (!MissingRedirectedTagNames.Contains(TagName))
			{
				const FString RedirectedToName = RedirectedTag->GetTagName().ToString();
				ensureAlwaysMsgf(false, TEXT("Requested Gameplay Tag %s was redirected to %s but %s was not found. Fix or remove the redirect from config."), *TagName.ToString(), *RedirectedToName, *RedirectedToName);
				MissingRedirectedTagNames.Add(TagName);
			}
		}
		
		// TagName got redirected to a non-existent tag. We'll return an empty tag rather than falling through
		// and trying to resolve the original tag name. Stale redirects should be fixed.
		return FGameplayTag();
	}

	// Check if the tag itself exists in the node map. If so, return it.
	const FGameplayTag PossibleTag(TagName);
	if (GameplayTagNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}

	// The tag is not found. Error if that was requested.
	if (ErrorIfNotFound)
	{
		static TSet<FName> MissingTagName;
		if (!MissingTagName.Contains(TagName))
		{
			ensureAlwaysMsgf(false, TEXT("Requested Gameplay Tag %s was not found, tags must be loaded from config or registered as a native tag"), *TagName.ToString());
			MissingTagName.Add(TagName);
		}
	}

	return FGameplayTag();
}

bool UGameplayTagsManager::IsValidGameplayTagString(const FString& TagString, FText* OutError, FString* OutFixedString)
{
	bool bIsValid = true;
	FString FixedString = TagString;
	FText ErrorText;

	if (FixedString.IsEmpty())
	{
		ErrorText = LOCTEXT("EmptyStringError", "Tag is empty");
		bIsValid = false;
	}

	while (FixedString.StartsWith(TEXT("."), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("StartWithPeriod", "Tag starts with .");
		FixedString.RemoveAt(0);
		bIsValid = false;
	}

	while (FixedString.EndsWith(TEXT("."), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("EndWithPeriod", "Tag ends with .");
		FixedString.RemoveAt(FixedString.Len() - 1);
		bIsValid = false;
	}

	while (FixedString.StartsWith(TEXT(" "), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("StartWithSpace", "Tag starts with space");
		FixedString.RemoveAt(0);
		bIsValid = false;
	}

	while (FixedString.EndsWith(TEXT(" "), ESearchCase::CaseSensitive))
	{
		ErrorText = LOCTEXT("EndWithSpace", "Tag ends with space");
		FixedString.RemoveAt(FixedString.Len() - 1);
		bIsValid = false;
	}

	FText TagContext = LOCTEXT("GameplayTagContext", "Tag");
	if (!FName::IsValidXName(TagString, InvalidTagCharacters, &ErrorText, &TagContext))
	{
		for (TCHAR& TestChar : FixedString)
		{
			for (TCHAR BadChar : InvalidTagCharacters)
			{
				if (TestChar == BadChar)
				{
					TestChar = TEXT('_');
				}
			}
		}

		bIsValid = false;
	}

	if (OutError)
	{
		*OutError = ErrorText;
	}
	if (OutFixedString)
	{
		*OutFixedString = FixedString;
	}

	return bIsValid;
}

FGameplayTag UGameplayTagsManager::FindGameplayTagFromPartialString_Slow(FString PartialString) const
{
	// This critical section is to handle an issue where tag requests come from another thread when async loading from a background thread in FGameplayTagContainer::Serialize.
	// This function is not generically threadsafe.
	FScopeLock Lock(&GameplayTagMapCritical);

	// Exact match first
	FGameplayTag PossibleTag(*PartialString);
	if (GameplayTagNodeMap.Contains(PossibleTag))
	{
		return PossibleTag;
	}

	// Find shortest tag name that contains the match string
	FGameplayTag FoundTag;
	FGameplayTagContainer AllTags;
	RequestAllGameplayTags(AllTags, false);

	int32 BestMatchLength = MAX_int32;
	for (FGameplayTag MatchTag : AllTags)
	{
		FString Str = MatchTag.ToString();
		if (Str.Contains(PartialString))
		{
			if (Str.Len() < BestMatchLength)
			{
				FoundTag = MatchTag;
				BestMatchLength = Str.Len();
			}
		}
	}
	
	return FoundTag;
}

FGameplayTag UGameplayTagsManager::AddNativeGameplayTag(FName TagName, const FString& TagDevComment)
{
	if (TagName.IsNone())
	{
		return FGameplayTag();
	}

	// Unsafe to call after done adding
	if (ensure(!bDoneAddingNativeTags))
	{
		FGameplayTag NewTag = FGameplayTag(TagName);

		if (!LegacyNativeTags.Contains(TagName))
		{
			LegacyNativeTags.Add(TagName);
		}

		AddTagTableRow(FGameplayTagTableRow(TagName, TagDevComment), FGameplayTagSource::GetNativeName());

		return NewTag;
	}

	return FGameplayTag();
}

void UGameplayTagsManager::AddNativeGameplayTag(FNativeGameplayTag* TagSource)
{
	const FGameplayTagSource* NativeSource = FindOrAddTagSource(TagSource->GetModuleName(), EGameplayTagSourceType::Native);
	NativeSource->SourceTagList->GameplayTagList.Add(TagSource->GetGameplayTagTableRow());
	
	// This adds it to the temporary tree, but expects the caller to add it to FNativeGameplayTag::GetRegisteredNativeTags for later refreshes
	AddTagTableRow(TagSource->GetGameplayTagTableRow(), NativeSource->SourceName);

	HandleGameplayTagTreeChanged(false);
}

void UGameplayTagsManager::RemoveNativeGameplayTag(const FNativeGameplayTag* TagSource)
{
	if (!ShouldUnloadTags())
	{
		// Ignore if not allowed right now
		return;
	}

	// ~FNativeGameplayTag already removed the tag from the global list, so recreate the tree
	HandleGameplayTagTreeChanged(true);
}

void UGameplayTagsManager::CallOrRegister_OnDoneAddingNativeTagsDelegate(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (bDoneAddingNativeTags)
	{
		Delegate.Execute();
	}
	else
	{
		bool bAlreadyBound = Delegate.GetUObject() != nullptr ? OnDoneAddingNativeTagsDelegate().IsBoundToObject(Delegate.GetUObject()) : false;
		if (!bAlreadyBound)
		{
			OnDoneAddingNativeTagsDelegate().Add(Delegate);
		}
	}
}

FSimpleMulticastDelegate& UGameplayTagsManager::OnDoneAddingNativeTagsDelegate()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

FSimpleMulticastDelegate& UGameplayTagsManager::OnLastChanceToAddNativeTags()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}

void UGameplayTagsManager::DoneAddingNativeTags()
{
	// Safe to call multiple times, only works the first time, must be called after the engine
	// is initialized (DoneAddingNativeTags is bound to PostEngineInit to cover anything that's skipped).
	if (GEngine && !bDoneAddingNativeTags)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UGameplayTagsManager::DoneAddingNativeTags);

		UE_CLOG(GAMEPLAYTAGS_VERBOSE, LogGameplayTags, Display, TEXT("UGameplayTagsManager::DoneAddingNativeTags. DelegateIsBound: %d"), (int32)OnLastChanceToAddNativeTags().IsBound());
		OnLastChanceToAddNativeTags().Broadcast();
		bDoneAddingNativeTags = true;

		// We may add native tags that are needed for redirectors, so reconstruct the GameplayTag tree
		DestroyGameplayTagTree();
		ConstructGameplayTagTree();

		OnDoneAddingNativeTagsDelegate().Broadcast();
	}
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagParents(const FGameplayTag& GameplayTag) const
{
	FScopeLock Lock(&GameplayTagMapCritical);

	const FGameplayTagContainer* ParentTags = GetSingleTagContainerPtr(GameplayTag);

	if (ParentTags)
	{
		return ParentTags->GetGameplayTagParents();
	}
	return FGameplayTagContainer();
}

// If true, verify that the node lookup and manual methods give identical results
#define VALIDATE_EXTRACT_PARENT_TAGS 0

bool UGameplayTagsManager::ExtractParentTags(const FGameplayTag& GameplayTag, TArray<FGameplayTag>& UniqueParentTags) const
{
	// This gets called during GameplayTagContainer serialization so needs to be efficient
	if (!GameplayTag.IsValid())
	{
		return false;
	}

	TArray<FGameplayTag> ValidationCopy;

	if constexpr (0 != VALIDATE_EXTRACT_PARENT_TAGS)
	{
		ValidationCopy = UniqueParentTags;
	}

	int32 OldSize = UniqueParentTags.Num();
	FName RawTag = GameplayTag.GetTagName();

	// Need to run in the open as it takes a lock. 
	UE_AUTORTFM_OPEN(
		{
			FScopeLock Lock(&GameplayTagMapCritical);

			// This code does not check redirectors because that was already handled by GameplayTagContainerLoaded
			const TSharedPtr<FGameplayTagNode>*Node = GameplayTagNodeMap.Find(GameplayTag);
			if (Node)
			{
				// Use the registered tag container if it exists
				const FGameplayTagContainer& SingleContainer = (*Node)->GetSingleTagContainer();
				for (const FGameplayTag& ParentTag : SingleContainer.ParentTags)
				{
					UniqueParentTags.AddUnique(ParentTag);
				}

				if constexpr (0 != VALIDATE_EXTRACT_PARENT_TAGS)
				{
					GameplayTag.ParseParentTags(ValidationCopy);
					ensureAlwaysMsgf(ValidationCopy == UniqueParentTags, TEXT("ExtractParentTags results are inconsistent for tag %s"), *GameplayTag.ToString());
				}
			}
			else if (!ShouldClearInvalidTags())
			{
				// If we don't clear invalid tags, we need to extract the parents now in case they get registered later
				GameplayTag.ParseParentTags(UniqueParentTags);
			}
		});

	return UniqueParentTags.Num() != OldSize;
}

void UGameplayTagsManager::RequestAllGameplayTags(FGameplayTagContainer& TagContainer, bool OnlyIncludeDictionaryTags) const
{
	FScopeLock Lock(&GameplayTagMapCritical);

	for (const TPair<FGameplayTag, TSharedPtr<FGameplayTagNode>>& NodePair : GameplayTagNodeMap)
	{
		const TSharedPtr<FGameplayTagNode>& TagNode = NodePair.Value;
		if (!OnlyIncludeDictionaryTags || TagNode->IsExplicitTag())
		{
			TagContainer.AddTagFast(TagNode->GetCompleteTag());
		}
	}
}

FGameplayTagContainer UGameplayTagsManager::RequestGameplayTagChildren(const FGameplayTag& GameplayTag) const
{
	FGameplayTagContainer TagContainer;
	// Note this purposefully does not include the passed in GameplayTag in the container.
	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		AddChildrenTags(TagContainer, GameplayTagNode, true, false);
	}
	return TagContainer;
}

FGameplayTag UGameplayTagsManager::RequestGameplayTagDirectParent(const FGameplayTag& GameplayTag) const
{
	TSharedPtr<FGameplayTagNode> GameplayTagNode = FindTagNode(GameplayTag);
	if (GameplayTagNode.IsValid())
	{
		TSharedPtr<FGameplayTagNode> Parent = GameplayTagNode->GetParentTagNode();
		if (Parent.IsValid())
		{
			return Parent->GetCompleteTag();
		}
	}
	return FGameplayTag();
}

void UGameplayTagsManager::AddChildrenTags(FGameplayTagContainer& TagContainer, TSharedPtr<FGameplayTagNode> GameplayTagNode, bool RecurseAll, bool OnlyIncludeDictionaryTags) const
{
	if (GameplayTagNode.IsValid())
	{
		TArray< TSharedPtr<FGameplayTagNode> >& ChildrenNodes = GameplayTagNode->GetChildTagNodes();
		for (TSharedPtr<FGameplayTagNode> ChildNode : ChildrenNodes)
		{
			if (ChildNode.IsValid())
			{
				bool bShouldInclude = true;

#if WITH_EDITORONLY_DATA
				if (OnlyIncludeDictionaryTags && ChildNode->GetFirstSourceName() == NAME_None)
				{
					// Only have info to do this in editor builds
					bShouldInclude = false;
				}
#endif	
				if (bShouldInclude)
				{
					TagContainer.AddTag(ChildNode->GetCompleteTag());
				}

				if (RecurseAll)
				{
					AddChildrenTags(TagContainer, ChildNode, true, OnlyIncludeDictionaryTags);
				}
			}

		}
	}
}

void UGameplayTagsManager::SplitGameplayTagFName(const FGameplayTag& Tag, TArray<FName>& OutNames) const
{
	TSharedPtr<FGameplayTagNode> CurNode = FindTagNode(Tag);
	while (CurNode.IsValid())
	{
		OutNames.Insert(CurNode->GetSimpleTagName(), 0);
		CurNode = CurNode->GetParentTagNode();
	}
}

int32 UGameplayTagsManager::GameplayTagsMatchDepth(const FGameplayTag& GameplayTagOne, const FGameplayTag& GameplayTagTwo) const
{
	using FTagsArray = TArray<FName, TInlineAllocator<32>>;

	auto GetTags = [this](FTagsArray& Tags, const FGameplayTag& GameplayTag)
	{
		for (TSharedPtr<FGameplayTagNode> TagNode = FindTagNode(GameplayTag); TagNode.IsValid(); TagNode = TagNode->GetParentTagNode())
		{
			Tags.Add(TagNode->Tag);
		}
	};

	FTagsArray Tags1, Tags2;
	GetTags(Tags1, GameplayTagOne);
	GetTags(Tags2, GameplayTagTwo);

	// Get Tags returns tail to head, so compare in reverse order
	int32 Index1 = Tags1.Num() - 1;
	int32 Index2 = Tags2.Num() - 1;

	int32 Depth = 0;

	for (; Index1 >= 0 && Index2 >= 0; --Index1, --Index2)
	{
		if (Tags1[Index1] == Tags2[Index2])
		{
			++Depth;
		}
		else
		{
			break;
		}
	}

	return Depth;
}

int32 UGameplayTagsManager::GetNumberOfTagNodes(const FGameplayTag& GameplayTag) const
{
	int32 Count = 0;

	TSharedPtr<FGameplayTagNode> TagNode = FindTagNode(GameplayTag);
	while (TagNode.IsValid())
	{
		++Count;								// Increment the count of valid tag nodes.
		TagNode = TagNode->GetParentTagNode();	// Continue up the chain of parents.
	}

	return Count;
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::GetAllParentNodeNames"), STAT_UGameplayTagsManager_GetAllParentNodeNames, STATGROUP_GameplayTags);

void UGameplayTagsManager::GetAllParentNodeNames(TSet<FName>& NamesList, TSharedPtr<FGameplayTagNode> GameplayTag) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_GetAllParentNodeNames);

	NamesList.Add(GameplayTag->GetCompleteTagName());
	TSharedPtr<FGameplayTagNode> Parent = GameplayTag->GetParentTagNode();
	if (Parent.IsValid())
	{
		GetAllParentNodeNames(NamesList, Parent);
	}
}

DECLARE_CYCLE_STAT(TEXT("UGameplayTagsManager::ValidateTagCreation"), STAT_UGameplayTagsManager_ValidateTagCreation, STATGROUP_GameplayTags);

bool UGameplayTagsManager::ValidateTagCreation(FName TagName) const
{
	SCOPE_CYCLE_COUNTER(STAT_UGameplayTagsManager_ValidateTagCreation);

	return FindTagNode(TagName).IsValid();
}

FGameplayTagTableRow::FGameplayTagTableRow(FGameplayTagTableRow const& Other)
{
	*this = Other;
}

FGameplayTagTableRow& FGameplayTagTableRow::operator=(FGameplayTagTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Tag = Other.Tag;
	DevComment = Other.DevComment;

	return *this;
}

bool FGameplayTagTableRow::operator==(FGameplayTagTableRow const& Other) const
{
	return (Tag == Other.Tag);
}

bool FGameplayTagTableRow::operator!=(FGameplayTagTableRow const& Other) const
{
	return (Tag != Other.Tag);
}

bool FGameplayTagTableRow::operator<(FGameplayTagTableRow const& Other) const
{
	return UE::ComparisonUtility::CompareWithNumericSuffix(Tag, Other.Tag) < 0;
}

FRestrictedGameplayTagTableRow::FRestrictedGameplayTagTableRow(FRestrictedGameplayTagTableRow const& Other)
{
	*this = Other;
}

FRestrictedGameplayTagTableRow& FRestrictedGameplayTagTableRow::operator=(FRestrictedGameplayTagTableRow const& Other)
{
	// Guard against self-assignment
	if (this == &Other)
	{
		return *this;
	}

	Super::operator=(Other);
	bAllowNonRestrictedChildren = Other.bAllowNonRestrictedChildren;

	return *this;
}

bool FRestrictedGameplayTagTableRow::operator==(FRestrictedGameplayTagTableRow const& Other) const
{
	if (bAllowNonRestrictedChildren != Other.bAllowNonRestrictedChildren)
	{
		return false;
	}

	if (Tag != Other.Tag)
	{
		return false;
	}

	return true;
}

bool FRestrictedGameplayTagTableRow::operator!=(FRestrictedGameplayTagTableRow const& Other) const
{
	if (bAllowNonRestrictedChildren == Other.bAllowNonRestrictedChildren)
	{
		return false;
	}

	if (Tag == Other.Tag)
	{
		return false;
	}

	return true;
}

FGameplayTagNode::FGameplayTagNode(FName InTag, FName InFullTag, TSharedPtr<FGameplayTagNode> InParentNode, bool InIsExplicitTag, bool InIsRestrictedTag, bool InAllowNonRestrictedChildren)
	: Tag(InTag)
	, ParentNode(InParentNode)
	, NetIndex(INVALID_TAGNETINDEX)
{
	// Manually construct the tag container as we want to bypass the safety checks
	CompleteTagWithParents.GameplayTags.Add(FGameplayTag(InFullTag));

	FGameplayTagNode* RawParentNode = ParentNode.Get();
	if (RawParentNode && RawParentNode->GetSimpleTagName() != NAME_None)
	{
		// Our parent nodes are already constructed, and must have it's tag in GameplayTags[0]
		const FGameplayTagContainer ParentContainer = RawParentNode->GetSingleTagContainer();

		CompleteTagWithParents.ParentTags.Add(ParentContainer.GameplayTags[0]);
		CompleteTagWithParents.ParentTags.Append(ParentContainer.ParentTags);
	}
	
#if WITH_EDITORONLY_DATA
	bIsExplicitTag = InIsExplicitTag;
	bIsRestrictedTag = InIsRestrictedTag;
	bAllowNonRestrictedChildren = InAllowNonRestrictedChildren;

	bDescendantHasConflict = false;
	bNodeHasConflict = false;
	bAncestorHasConflict = false;
#endif 
}

void FGameplayTagNode::ResetNode()
{
	Tag = NAME_None;
	CompleteTagWithParents.Reset();
	NetIndex = INVALID_TAGNETINDEX;

	for (int32 ChildIdx = 0; ChildIdx < ChildTags.Num(); ++ChildIdx)
	{
		ChildTags[ChildIdx]->ResetNode();
	}

	ChildTags.Empty();
	ParentNode.Reset();

#if WITH_EDITORONLY_DATA
	SourceNames.Reset();
	DevComment = "";
	bIsExplicitTag = false;
	bIsRestrictedTag = false;
	bAllowNonRestrictedChildren = false;
	bDescendantHasConflict = false;
	bNodeHasConflict = false;
	bAncestorHasConflict = false;
#endif 
}

#undef LOCTEXT_NAMESPACE

