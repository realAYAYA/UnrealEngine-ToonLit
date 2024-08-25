// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemStats.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueInterface.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemLog.h"
#include "HAL/LowLevelMemTracker.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"
#include "GameplayTagResponseTable.h"
#include "GameplayTagsManager.h"
#include "Engine/Engine.h"
#include "UObject/UObjectIterator.h"

#if UE_WITH_IRIS
#include "Serialization/GameplayAbilityTargetDataHandleNetSerializer.h"
#include "Serialization/GameplayEffectContextHandleNetSerializer.h"
#include "Serialization/PredictionKeyNetSerializer.h"
#endif // UE_WITH_IRIS

#if WITH_EDITOR
#include "Editor.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilitySystemGlobals)

namespace UE::AbilitySystemGlobals
{
	bool bIgnoreAbilitySystemCooldowns = false;
	bool bIgnoreAbilitySystemCosts = false;

	FAutoConsoleVariableRef CVarAbilitySystemIgnoreCooldowns(TEXT("AbilitySystem.IgnoreCooldowns"), bIgnoreAbilitySystemCooldowns, TEXT("Ignore cooldowns for all Gameplay Abilities."), ECVF_Cheat);
	FAutoConsoleVariableRef CVarAbilitySystemIgnoreCosts(TEXT("AbilitySystem.IgnoreCosts"), bIgnoreAbilitySystemCosts, TEXT("Ignore costs for all Gameplay Abilities."), ECVF_Cheat);
}

UAbilitySystemGlobals::UAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	AbilitySystemGlobalsClassName = FSoftClassPath(TEXT("/Script/GameplayAbilities.AbilitySystemGlobals"));

	bUseDebugTargetFromHud = false;

	PredictTargetGameplayEffects = true;

	ReplicateActivationOwnedTags = true;

	MinimalReplicationTagCountBits = 5;

	bAllowGameplayModEvaluationChannels = false;

#if WITH_EDITORONLY_DATA
	RegisteredReimportCallback = false;
#endif // #if WITH_EDITORONLY_DATA
}

bool UAbilitySystemGlobals::IsAbilitySystemGlobalsInitialized() const
{
	return bInitialized;
}

void UAbilitySystemGlobals::InitGlobalData()
{
	// Make sure the user didn't try to initialize the system again (we call InitGlobalData automatically in UE5.3+).
	if (IsAbilitySystemGlobalsInitialized())
	{
		return;
	}
	bInitialized = true;

	LLM_SCOPE(TEXT("AbilitySystem"));
	GetGlobalCurveTable();
	GetGlobalAttributeMetaDataTable();
	
	InitAttributeDefaults();

	GetGameplayCueManager();
	GetGameplayTagResponseTable();
	InitGlobalTags();

	InitTargetDataScriptStructCache();

	// Register for PreloadMap so cleanup can occur on map transitions
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UAbilitySystemGlobals::HandlePreLoadMap);

#if WITH_EDITOR
	// Register in editor for PreBeginPlay so cleanup can occur when we start a PIE session
	if (GIsEditor)
	{
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UAbilitySystemGlobals::OnPreBeginPIE);
	}
#endif
}


UCurveTable * UAbilitySystemGlobals::GetGlobalCurveTable()
{
	if (!GlobalCurveTable && GlobalCurveTableName.IsValid())
	{
		GlobalCurveTable = Cast<UCurveTable>(GlobalCurveTableName.TryLoad());
	}
	return GlobalCurveTable;
}

UDataTable * UAbilitySystemGlobals::GetGlobalAttributeMetaDataTable()
{
	if (!GlobalAttributeMetaDataTable && GlobalAttributeMetaDataTableName.IsValid())
	{
		GlobalAttributeMetaDataTable = Cast<UDataTable>(GlobalAttributeMetaDataTableName.TryLoad());
	}
	return GlobalAttributeMetaDataTable;
}

bool UAbilitySystemGlobals::DeriveGameplayCueTagFromAssetName(FString AssetName, FGameplayTag& GameplayCueTag, FName& GameplayCueName)
{
	FGameplayTag OriginalTag = GameplayCueTag;
	
	// In the editor, attempt to infer GameplayCueTag from our asset name (if there is no valid GameplayCueTag already).
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (GameplayCueTag.IsValid() == false)
		{
			AssetName.RemoveFromStart(TEXT("Default__"));
			AssetName.RemoveFromStart(TEXT("REINST_"));
			AssetName.RemoveFromStart(TEXT("SKEL_"));
			AssetName.RemoveFromStart(TEXT("GC_"));		// allow GC_ prefix in asset name
			AssetName.RemoveFromEnd(TEXT("_c"));

			AssetName.ReplaceInline(TEXT("_"), TEXT("."), ESearchCase::CaseSensitive);

			if (!AssetName.Contains(TEXT("GameplayCue")))
			{
				AssetName = FString(TEXT("GameplayCue.")) + AssetName;
			}

			GameplayCueTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*AssetName), false);
		}
		GameplayCueName = GameplayCueTag.GetTagName();
	}
#endif
	return (OriginalTag != GameplayCueTag);
}

bool UAbilitySystemGlobals::ShouldAllowGameplayModEvaluationChannels() const
{
	return bAllowGameplayModEvaluationChannels;
}

bool UAbilitySystemGlobals::IsGameplayModEvaluationChannelValid(EGameplayModEvaluationChannel Channel) const
{
	// Only valid if channels are allowed and the channel has a game-specific alias specified or if not using channels and the channel is Channel0
	const bool bAllowChannels = ShouldAllowGameplayModEvaluationChannels();
	return bAllowChannels ? (!GetGameplayModEvaluationChannelAlias(Channel).IsNone()) : (Channel == EGameplayModEvaluationChannel::Channel0);
}

const FName& UAbilitySystemGlobals::GetGameplayModEvaluationChannelAlias(EGameplayModEvaluationChannel Channel) const
{
	return GetGameplayModEvaluationChannelAlias(static_cast<int32>(Channel));
}

const FName& UAbilitySystemGlobals::GetGameplayModEvaluationChannelAlias(int32 Index) const
{
	check(Index >= 0 && Index < UE_ARRAY_COUNT(GameplayModEvaluationChannelAliases));
	return GameplayModEvaluationChannelAliases[Index];
}

void UAbilitySystemGlobals::AddGameplayCueNotifyPath(const FString& InPath)
{
	GameplayCueNotifyPaths.AddUnique(InPath);
}

int32 UAbilitySystemGlobals::RemoveGameplayCueNotifyPath(const FString& InPath)
{
	return GameplayCueNotifyPaths.Remove(InPath);
}

#if WITH_EDITOR

void UAbilitySystemGlobals::OnTableReimported(UObject* InObject)
{
	if (GIsEditor && !IsRunningCommandlet() && InObject)
	{
		UCurveTable* ReimportedCurveTable = Cast<UCurveTable>(InObject);
		if (ReimportedCurveTable && GlobalAttributeDefaultsTables.Contains(ReimportedCurveTable))
		{
			ReloadAttributeDefaults();
		}
	}	
}

#endif

FGameplayAbilityActorInfo * UAbilitySystemGlobals::AllocAbilityActorInfo() const
{
	return new FGameplayAbilityActorInfo();
}

FGameplayEffectContext* UAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FGameplayEffectContext();
}

/** Helping function to avoid having to manually cast */
UAbilitySystemComponent* UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const AActor* Actor, bool LookForComponent)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
	if (ASI)
	{
		return ASI->GetAbilitySystemComponent();
	}

	if (LookForComponent)
	{
		// Fall back to a component search to better support BP-only actors
		return Actor->FindComponentByClass<UAbilitySystemComponent>();
	}

	return nullptr;
}

bool UAbilitySystemGlobals::ShouldPredictTargetGameplayEffects() const
{
	return PredictTargetGameplayEffects;
}

bool UAbilitySystemGlobals::ShouldReplicateActivationOwnedTags() const
{
	return ReplicateActivationOwnedTags;
}

// --------------------------------------------------------------------

UFunction* UAbilitySystemGlobals::GetGameplayCueFunction(const FGameplayTag& ChildTag, UClass* Class, FName &MatchedTag)
{
	SCOPE_CYCLE_COUNTER(STAT_GetGameplayCueFunction);

	// A global cached map to lookup these functions might be a good idea. Keep in mind though that FindFunctionByName
	// is fast and already gives us a reliable map lookup.
	// 
	// We would get some speed by caching off the 'fully qualified name' to 'best match' lookup. E.g. we can directly map
	// 'GameplayCue.X.Y' to the function 'GameplayCue.X' without having to look for GameplayCue.X.Y first.
	// 
	// The native remapping (Gameplay.X.Y to Gameplay_X_Y) is also annoying and slow and could be fixed by this as well.
	// 
	// Keep in mind that any UFunction* cacheing is pretty unsafe. Classes can be loaded (and unloaded) during runtime
	// and will be regenerated all the time in the editor. Just doing a single pass at startup won't be enough,
	// we'll need a mechanism for registering classes when they are loaded on demand.
	
	FGameplayTagContainer TagAndParentsContainer = ChildTag.GetGameplayTagParents();

	for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
	{
		FName CueName = InnerTagIt->GetTagName();
		if (UFunction* Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName;
			return Func;
		}

		// Native functions cant be named with ".", so look for them with _. 
		FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
		if (UFunction* Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper))
		{
			MatchedTag = CueName; // purposefully returning the . qualified name.
			return Func;
		}
	}

	return nullptr;
}

void UAbilitySystemGlobals::InitTargetDataScriptStructCache()
{
	TargetDataStructCache.InitForType(FGameplayAbilityTargetData::StaticStruct());
	EffectContextStructCache.InitForType(FGameplayEffectContext::StaticStruct());
}

// --------------------------------------------------------------------

void UAbilitySystemGlobals::InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectSpecForRPC& Spec)
{
	CueParameters.AggregatedSourceTags = Spec.AggregatedSourceTags;
	CueParameters.AggregatedTargetTags = Spec.AggregatedTargetTags;
	CueParameters.GameplayEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetAbilityLevel();
	InitGameplayCueParameters(CueParameters, Spec.GetContext());
}

void UAbilitySystemGlobals::InitGameplayCueParameters_GESpec(FGameplayCueParameters& CueParameters, const FGameplayEffectSpec& Spec)
{
	CueParameters.AggregatedSourceTags = *Spec.CapturedSourceTags.GetAggregatedTags();
	CueParameters.AggregatedTargetTags = *Spec.CapturedTargetTags.GetAggregatedTags();

	// Look for a modified attribute magnitude to pass to the CueParameters
	for (const FGameplayEffectCue& CueDef : Spec.Def->GameplayCues)
	{	
		bool FoundMatch = false;
		if (CueDef.MagnitudeAttribute.IsValid())
		{
			for (const FGameplayEffectModifiedAttribute& ModifiedAttribute : Spec.ModifiedAttributes)
			{
				if (ModifiedAttribute.Attribute == CueDef.MagnitudeAttribute)
				{
					CueParameters.RawMagnitude = ModifiedAttribute.TotalMagnitude;
					FoundMatch = true;
					break;
				}
			}
			if (FoundMatch)
			{
				break;
			}
		}
	}

	CueParameters.GameplayEffectLevel = Spec.GetLevel();
	CueParameters.AbilityLevel = Spec.GetEffectContext().GetAbilityLevel();

	InitGameplayCueParameters(CueParameters, Spec.GetContext());
}

void UAbilitySystemGlobals::InitGameplayCueParameters(FGameplayCueParameters& CueParameters, const FGameplayEffectContextHandle& EffectContext)
{
	if (EffectContext.IsValid())
	{
		// Copy Context over wholesale. Projects may want to override this and not copy over all data
		CueParameters.EffectContext = EffectContext;
	}
}

// --------------------------------------------------------------------

void UAbilitySystemGlobals::StartAsyncLoadingObjectLibraries()
{
	if (GlobalGameplayCueManager != nullptr)
	{
		GlobalGameplayCueManager->InitializeRuntimeObjectLibrary();
	}
}

// --------------------------------------------------------------------

/** Initialize FAttributeSetInitter. This is virtual so projects can override what class they use */
void UAbilitySystemGlobals::AllocAttributeSetInitter()
{
	GlobalAttributeSetInitter = TSharedPtr<FAttributeSetInitter>(new FAttributeSetInitterDiscreteLevels());
}

FAttributeSetInitter* UAbilitySystemGlobals::GetAttributeSetInitter() const
{
	check(GlobalAttributeSetInitter.IsValid());
	return GlobalAttributeSetInitter.Get();
}

void UAbilitySystemGlobals::AddAttributeDefaultTables(const FName OwnerName, const TArray<FSoftObjectPath>& AttribDefaultTableNames)
{
	for (const FSoftObjectPath& TableName : AttribDefaultTableNames)
	{
		if (TArray<FName>* Found = GlobalAttributeSetDefaultsTableNamesWithOwners.Find(TableName))
		{
			Found->Add(OwnerName);
		}
		else
		{
			TArray<FName> Owners = { OwnerName };
			GlobalAttributeSetDefaultsTableNamesWithOwners.Add(TableName, MoveTemp(Owners));
		}
	}

	InitAttributeDefaults();
}

void UAbilitySystemGlobals::RemoveAttributeDefaultTables(const FName OwnerName, const TArray<FSoftObjectPath>& AttribDefaultTableNames)
{
	bool bModified = false;

	for (const FSoftObjectPath& TableName : AttribDefaultTableNames)
	{
		if (TableName.IsValid())
		{
			if (TArray<FName>* Found = GlobalAttributeSetDefaultsTableNamesWithOwners.Find(TableName))
			{
				Found->RemoveSingle(OwnerName);

				// If no references remain, clear the pointer in GlobalAttributeDefaultsTables to allow GC
				if (Found->IsEmpty())
				{
					GlobalAttributeSetDefaultsTableNamesWithOwners.Remove(TableName);

					// Only if not listed in config file
					if (!GlobalAttributeSetDefaultsTableNames.Contains(TableName))
					{
						// Remove reference to allow GC so package can be unloaded
						if (UCurveTable* AttribTable = Cast<UCurveTable>(TableName.ResolveObject()))
						{
							if (GlobalAttributeDefaultsTables.Remove(AttribTable) > 0)
							{
								bModified = true;
							}
						}
					}
				}
			}
		}
	}

	if (bModified)
	{
		ReloadAttributeDefaults();
	}
}

void UAbilitySystemGlobals::InitAttributeDefaults()
{
 	bool bLoadedAnyDefaults = false;
 
	// Handle deprecated, single global table name
	if (GlobalAttributeSetDefaultsTableName.IsValid())
	{
		UCurveTable* AttribTable = Cast<UCurveTable>(GlobalAttributeSetDefaultsTableName.TryLoad());
		if (AttribTable)
		{
			GlobalAttributeDefaultsTables.AddUnique(AttribTable);
			bLoadedAnyDefaults = true;
		}
	}

	// Handle array of global curve tables for attribute defaults
 	for (const FSoftObjectPath& AttribDefaultTableName : GlobalAttributeSetDefaultsTableNames)
 	{
		if (AttribDefaultTableName.IsValid())
		{
			UCurveTable* AttribTable = Cast<UCurveTable>(AttribDefaultTableName.TryLoad());
			if (AttribTable)
			{
				GlobalAttributeDefaultsTables.AddUnique(AttribTable);
				bLoadedAnyDefaults = true;
			}
		}
 	}
	
	// Handle global curve tables for attribute defaults not defined by config file (registered by plugins or other systems calling AddAttributeDefaultTables)
	for (const TPair<FSoftObjectPath, TArray<FName>>& It : GlobalAttributeSetDefaultsTableNamesWithOwners)
	{
		const FSoftObjectPath& AttribDefaultTableName = It.Key;
		if (AttribDefaultTableName.IsValid() && !GlobalAttributeSetDefaultsTableNames.Contains(AttribDefaultTableName))
		{
			UCurveTable* AttribTable = Cast<UCurveTable>(AttribDefaultTableName.TryLoad());
			if (AttribTable)
			{
				GlobalAttributeDefaultsTables.AddUnique(AttribTable);
				bLoadedAnyDefaults = true;
			}
		}
	}

	if (bLoadedAnyDefaults)
	{
		// Subscribe for reimports if in the editor
#if WITH_EDITOR
		if (GIsEditor && !RegisteredReimportCallback)
		{
			GEditor->GetEditorSubsystem<UImportSubsystem>()->OnAssetReimport.AddUObject(this, &UAbilitySystemGlobals::OnTableReimported);
			RegisteredReimportCallback = true;
		}
#endif


		ReloadAttributeDefaults();
	}
}

void UAbilitySystemGlobals::ReloadAttributeDefaults()
{
	AllocAttributeSetInitter();
	GlobalAttributeSetInitter->PreloadAttributeSetData(GlobalAttributeDefaultsTables);
}

// --------------------------------------------------------------------

UGameplayCueManager* UAbilitySystemGlobals::GetGameplayCueManager()
{
	if (GlobalGameplayCueManager == nullptr)
	{
		// Load specific gameplaycue manager object if specified
		if (GlobalGameplayCueManagerName.IsValid())
		{
			GlobalGameplayCueManager = LoadObject<UGameplayCueManager>(nullptr, *GlobalGameplayCueManagerName.ToString(), nullptr, LOAD_None, nullptr);
			if (GlobalGameplayCueManager == nullptr)
			{
				ABILITY_LOG(Error, TEXT("Unable to Load GameplayCueManager %s"), *GlobalGameplayCueManagerName.ToString() );
			}
		}

		// Load specific gameplaycue manager class if specified
		if ( GlobalGameplayCueManager == nullptr && GlobalGameplayCueManagerClass.IsValid() )
		{
			UClass* GCMClass = LoadClass<UObject>(NULL, *GlobalGameplayCueManagerClass.ToString(), NULL, LOAD_None, NULL);
			if (GCMClass)
			{
				GlobalGameplayCueManager = NewObject<UGameplayCueManager>(this, GCMClass, NAME_None);
			}
		}

		if ( GlobalGameplayCueManager == nullptr)
		{
			// Fallback to base native class
			GlobalGameplayCueManager = NewObject<UGameplayCueManager>(this, UGameplayCueManager::StaticClass(), NAME_None);
		}

		GlobalGameplayCueManager->OnCreated();

		if (GameplayCueNotifyPaths.Num() == 0)
		{
			GameplayCueNotifyPaths.Add(TEXT("/Game"));
			ABILITY_LOG(Warning, TEXT("No GameplayCueNotifyPaths were specified in DefaultGame.ini under [/Script/GameplayAbilities.AbilitySystemGlobals]. Falling back to using all of /Game/. This may be slow on large projects. Consider specifying which paths are to be searched."));
		}
		
		if (GlobalGameplayCueManager->ShouldAsyncLoadObjectLibrariesAtStart())
		{
			StartAsyncLoadingObjectLibraries();
		}
	}

	check(GlobalGameplayCueManager);
	return GlobalGameplayCueManager;
}

UGameplayTagReponseTable* UAbilitySystemGlobals::GetGameplayTagResponseTable()
{
	if (GameplayTagResponseTable == nullptr && GameplayTagResponseTableName.IsValid())
	{
		GameplayTagResponseTable = LoadObject<UGameplayTagReponseTable>(nullptr, *GameplayTagResponseTableName.ToString(), nullptr, LOAD_None, nullptr);
	}

	return GameplayTagResponseTable;
}

void UAbilitySystemGlobals::GlobalPreGameplayEffectSpecApply(FGameplayEffectSpec& Spec, UAbilitySystemComponent* AbilitySystemComponent)
{

}

bool UAbilitySystemGlobals::ShouldIgnoreCooldowns() const
{
	return UE::AbilitySystemGlobals::bIgnoreAbilitySystemCooldowns;
}

bool UAbilitySystemGlobals::ShouldIgnoreCosts() const
{
	return UE::AbilitySystemGlobals::bIgnoreAbilitySystemCosts;
}

#if WITH_EDITOR
void UAbilitySystemGlobals::OnPreBeginPIE(const bool bIsSimulatingInEditor)
{
	ResetCachedData();
}
#endif // WITH_EDITOR

void UAbilitySystemGlobals::ResetCachedData()
{
	IGameplayCueInterface::ClearTagToFunctionMap();
	FActiveGameplayEffectHandle::ResetGlobalHandleMap();
}

void UAbilitySystemGlobals::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	// We don't want to reset for PIE since this is shared memory (which would have received OnPreBeginPIE).
	if (WorldContext.PIEInstance > 0)
	{
		return;
	}

	// If we are preloading a map but coming from an existing map, then we should wait until the previous map is cleaned up,
	// otherwise we'll end up stomping FActiveGameplayEffectHandle map.
	if (const UWorld* InWorld = WorldContext.World())
	{
		FWorldDelegates::OnPostWorldCleanup.AddWeakLambda(InWorld, [InWorld](UWorld* WorldParam, bool bSessionEnded, bool bCleanupResources)
			{
				if (WorldParam == InWorld)
				{
					ResetCachedData();
				}
			});

		return;
	}

	ResetCachedData();
}

void UAbilitySystemGlobals::Notify_OpenAssetInEditor(FString AssetName, int AssetType)
{
	AbilityOpenAssetInEditorCallbacks.Broadcast(AssetName, AssetType);
}

void UAbilitySystemGlobals::Notify_FindAssetInEditor(FString AssetName, int AssetType)
{
	AbilityFindAssetInEditorCallbacks.Broadcast(AssetName, AssetType);
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
float AbilitySystemGlobalScaler = 1.f;
static FAutoConsoleVariableRef CVarOrionGlobalScaler(TEXT("AbilitySystem.GlobalAbilityScale"), AbilitySystemGlobalScaler, TEXT("Global rate for scaling ability stuff like montages and root motion tasks. Used only for testing/iteration, never for shipping."), ECVF_Cheat );
#endif

void UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(float& Rate)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Rate *= AbilitySystemGlobalScaler;
#endif
}

void UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(float& Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (AbilitySystemGlobalScaler > 0.f)
	{
		Duration /= AbilitySystemGlobalScaler;
	}
#endif
}

void FNetSerializeScriptStructCache::InitForType(UScriptStruct* InScriptStruct)
{
	ScriptStructs.Reset();

	// Find all script structs of this type and add them to the list
	// (not sure of a better way to do this but it should only happen once at startup)
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->IsChildOf(InScriptStruct))
		{
			ScriptStructs.Add(*It);
		}
	}
	
	ScriptStructs.Sort([](const UScriptStruct& A, const UScriptStruct& B) { return A.GetName().ToLower() > B.GetName().ToLower(); });
}

bool FNetSerializeScriptStructCache::NetSerialize(FArchive& Ar, UScriptStruct*& Struct)
{
	if (Ar.IsSaving())
	{
		int32 idx;
		if (ScriptStructs.Find(Struct, idx))
		{
			check(idx < (1 << 8));
			uint8 b = idx;
			Ar.SerializeBits(&b, 8);
			return true;
		}
		ABILITY_LOG(Error, TEXT("Could not find %s in ScriptStructCache"), *GetNameSafe(Struct));
		return false;
	}
	else
	{
		uint8 b = 0;
		Ar.SerializeBits(&b, 8);
		if (ScriptStructs.IsValidIndex(b))
		{
			Struct = ScriptStructs[b];
			return true;
		}

		ABILITY_LOG(Error, TEXT("Could not find script struct at idx %d"), b);
		return false;
	}
}
