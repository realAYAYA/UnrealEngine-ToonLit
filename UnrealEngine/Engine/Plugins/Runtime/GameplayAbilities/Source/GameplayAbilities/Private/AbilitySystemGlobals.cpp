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

	template<typename ClassToFind>
	TSubclassOf<ClassToFind> FuzzyFindClass(FString SearchString);
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

void UAbilitySystemGlobals::AddAttributeDefaultTables(const TArray<FSoftObjectPath>& AttribDefaultTableNames)
{
	for (const FSoftObjectPath& TableName : AttribDefaultTableNames)
	{
		GlobalAttributeSetDefaultsTableNames.AddUnique(TableName);
	}

	InitAttributeDefaults();
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

namespace UE::AbilitySystemGlobals
{
	/**
	 * Common logic used to fuzzy-find a requested class (or alternatively, a passed-in asset path).
	 */
	template<typename ClassToFind>
	TSubclassOf<ClassToFind> FuzzyFindClass(FString SearchString)
	{
		TSubclassOf<ClassToFind> FoundClass;

		// See if we passed in a class name of a Class that already exists in memory.
		// If we passed-in Default__, just remove that part since we're looking for Classes, not CDO names.
		SearchString.RemoveFromStart(TEXT("Default__"));
		const int SearchStringLen = SearchString.Len();
		int BestClassMatchLen = INT_MAX;
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			const bool bClassMatches = ClassIt->IsChildOf(ClassToFind::StaticClass());
			if (!bClassMatches)
			{
				continue;
			}

			// Class name search
			const FString ClassName = ClassIt->GetName();
			const int ClassNameLen = ClassName.Len();
			if (ClassNameLen < BestClassMatchLen && ClassNameLen >= SearchStringLen)
			{
				bool bContains = ClassName.Contains(SearchString);
				if (bContains)
				{
					FoundClass = *ClassIt;
					BestClassMatchLen = ClassNameLen;
				}
			}
		}

		// If it wasn't a class name, then perhaps it was the path to a specific asset
		if (!FoundClass)
		{
			FSoftObjectPath SoftObjectPath{ SearchString };
			if (UObject* ReferencedObject = SoftObjectPath.ResolveObject())
			{
				if (UPackage* ReferencedPackage = Cast<UPackage>(ReferencedObject))
				{
					ReferencedObject = ReferencedPackage->FindAssetInPackage();
				}

				if (UBlueprint* ReferencedBlueprint = Cast<UBlueprint>(ReferencedObject))
				{
					FoundClass = ReferencedBlueprint->GeneratedClass;
				}
				else if (ClassToFind* ReferencedGA = Cast<ClassToFind>(ReferencedObject))
				{
					FoundClass = ReferencedGA->GetClass();
				}
			}
		}

		return FoundClass;
	}
}

//
// AbilitySystem.Ability Debug Commands
//

FAutoConsoleCommand DebugAbilitySystemAbilityListGrantedCommand(TEXT("AbilitySystem.Ability.Grant"), TEXT("Include a param [ClassName/AssetName] to Grant an Ability to the Player.  Omit parameters to lists all of the Gameplay Abilities currently granted to the Player."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

		APlayerController* PC = World->GetFirstPlayerController();
		UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
		if (!ASC)
		{
			OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
			return;
		}

		if (Args.Num() > 1)
		{
			OutputDevice.Logf(TEXT("Expected a single parameter (ClassName or AssetName).  Use no parameters to list all granted abilities."));
			return;
		}

		// List already granted
		if (Args.Num() < 1)
		{
			const UEnum* ExecutionEnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/GameplayAbilities.EGameplayAbilityNetExecutionPolicy"), true);
			check(ExecutionEnumPtr&& TEXT("Couldn't locate EGameplayAbilityNetExecutionPolicy enum!"));

			const UEnum* SecurityEnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/GameplayAbilities.EGameplayAbilityNetSecurityPolicy"), true);
			check(SecurityEnumPtr&& TEXT("Couldn't locate EGameplayAbilityNetSecurityPolicy enum!"));

			OutputDevice.Logf(TEXT("Granted abilities to %s (ASC: '%s'):"), *PC->GetName(), *ASC->GetFullName());

			for (FGameplayAbilitySpec& Activatable : ASC->GetActivatableAbilities())
			{
				const TCHAR* ActiveText = Activatable.IsActive() ? TEXT("**ACTIVE**") : TEXT("");
				OutputDevice.Logf(TEXT("   %s (%s - %s) %s"), *Activatable.Ability->GetName(), *ExecutionEnumPtr->GetDisplayNameTextByIndex(Activatable.Ability->GetNetExecutionPolicy()).ToString(), *SecurityEnumPtr->GetDisplayNameTextByIndex(Activatable.Ability->GetNetSecurityPolicy()).ToString(), ActiveText);
			}

			return;
		}

		FString SearchString = Args[0];

		// We couldn't find anything the user was searching for, so early out
		TSubclassOf<UGameplayAbility> ActivateAbilityClass = UE::AbilitySystemGlobals::FuzzyFindClass<UGameplayAbility>(SearchString);
		if (!ActivateAbilityClass)
		{
			OutputDevice.Logf(TEXT("Could not find a valid Gameplay Ability based on Search String '%s'"), *SearchString);
			return;
		}

		// Check if it's already granted
		if (const FGameplayAbilitySpec* ExistingSpec = ASC->FindAbilitySpecFromClass(ActivateAbilityClass))
		{
			OutputDevice.Logf(TEXT("Existing Ability Spec '%s' on Player '%s' (It is already granted)."), *GetNameSafe(*ActivateAbilityClass), *GetNameSafe(PC));
			return;
		}

		// If we're not the authority, we need to send the command to the server because we can't grant locally.
		if (!ASC->IsOwnerActorAuthoritative())
		{
			const FString ServerCommand = FString::Printf(TEXT("AbilitySystem.Ability.Grant %s"), *SearchString);
			PC->ServerExec(ServerCommand);

			OutputDevice.Logf(TEXT("Sent Command '%s' from Player '%s' to Server (Reason: Cannot Grant if not Authority)."), *ServerCommand, *GetNameSafe(PC));
			return;
		}

		// It wasn't granted, let's grant it now.
		FGameplayAbilitySpec AbilitySpec{ ActivateAbilityClass };
		FGameplayAbilitySpecHandle SpecHandle = ASC->GiveAbility(AbilitySpec);
		if (SpecHandle.IsValid())
		{
			OutputDevice.Logf(TEXT("Successfully Granted '%s' on Player '%s'."), *GetNameSafe(ActivateAbilityClass.Get()), *GetNameSafe(PC));
		}
		else
		{
			OutputDevice.Logf(TEXT("Failed to Grant '%s' on Player '%s'."), *GetNameSafe(ActivateAbilityClass.Get()), *GetNameSafe(PC));
		}
		}), ECVF_Cheat);

FAutoConsoleCommand DebugAbilitySystemAbilityCancelCommand(TEXT("AbilitySystem.Ability.Cancel"), TEXT("<Name>. Cancels (prematurely Ends) a currently executing Gameplay Ability"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
		{
			UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

			APlayerController* PC = World->GetFirstPlayerController();
			UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
			if (!ASC)
			{
				OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
				return;
			}

			if (Args.Num() < 1)
			{
				if (IConsoleObject* GetGrantedCommand = IConsoleManager::Get().FindConsoleObject(TEXT("AbilitySystem.Ability.ListGranted"), false))
				{
					GetGrantedCommand->AsCommand()->Execute(Args, World, OutputDevice);
				}
				return;
			}

			FString SearchString = Args[0];
			SearchString.RemoveFromStart(TEXT("Default__"));

			bool bFound = false;
			for (FGameplayAbilitySpec& GASpec : ASC->GetActivatableAbilities())
			{
				if (GASpec.IsActive())
				{
					if (!GASpec.Ability || !GASpec.Ability->GetName().Contains(SearchString))
					{
						continue;
					}

					TArray<UGameplayAbility*> ActiveAbilities = GASpec.GetAbilityInstances();
					if (ActiveAbilities.Num() > 0)
					{
						for (UGameplayAbility* Instance : ActiveAbilities)
						{
							if (Instance)
							{
								OutputDevice.Logf(TEXT("%s (%s): Cancelling (instanced) %s"), *PC->GetName(), *ASC->GetName(), *Instance->GetName());
								Instance->CancelAbility(GASpec.Handle, ASC->AbilityActorInfo.Get(), GASpec.ActivationInfo, true);
								bFound = true;
							}
						}
					}
					else
					{
						OutputDevice.Logf(TEXT("%s (%s): Cancelling (non-instanced) %s"), *PC->GetName(), *ASC->GetName(), *GASpec.Ability->GetName());
						GASpec.Ability->CancelAbility(GASpec.Handle, ASC->AbilityActorInfo.Get(), GASpec.ActivationInfo, true);
						bFound = true;
					}
				}
			}

			if (!bFound)
			{
				OutputDevice.Logf(TEXT("Found no Active Gameplay Abilities on %s (%s) that matched '%s'"), *PC->GetName(), *ASC->GetName(), *SearchString);
			}
	}), ECVF_Cheat);

FAutoConsoleCommand DebugAbilitySystemAbilityActivateCommand(TEXT("AbilitySystem.Ability.Activate"), TEXT("<TagName/ClassName/AssetName>. Activate a Gameplay Ability.\nSubstring name matching works for Activation Tags (on already granted abilities), Asset Paths (on non-granted abilities), or Class Names on both."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

		APlayerController* PC = World->GetFirstPlayerController();
		UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
		if (!ASC)
		{
			OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
			return;
		}

		if (Args.Num() < 1)
		{
			if (IConsoleObject* GetGrantedCommand = IConsoleManager::Get().FindConsoleObject(TEXT("AbilitySystem.Ability.Grant"), false))
			{
				GetGrantedCommand->AsCommand()->Execute(Args, World, OutputDevice);
			}

			return;
		}

		const FString SearchString = Args[0];

		// Start by figuring out if we're trying to execute by GameplayTag.
		const FName SearchName{ TCHAR_TO_ANSI(*SearchString), FNAME_Find };
		if (SearchName != NAME_None)
		{
			const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(SearchName, false);
			if (GameplayTag.IsValid())
			{
				FGameplayTagContainer GameplayTagContainer{ GameplayTag };
				TArray<FGameplayAbilitySpec*> MatchingSpecs;
				constexpr bool bTagRequirementsMustMatch = false;
				ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(GameplayTagContainer, MatchingSpecs, bTagRequirementsMustMatch);
				if (MatchingSpecs.Num() > 0)
				{
					FString MatchingAbilitiesString = FString::JoinBy(MatchingSpecs, TEXT(", "), [](FGameplayAbilitySpec* Item) { return Item->GetDebugString(); });

					bool bSuccess = ASC->TryActivateAbilitiesByTag(GameplayTagContainer);
					if (bSuccess)
					{
						OutputDevice.Logf(TEXT("Requested Tag '%s' successfully executed one of: %s."), *SearchName.ToString(), *MatchingAbilitiesString);
					}
					else
					{
						OutputDevice.Logf(TEXT("Requested Tag '%s' was expected to execute one of: %s. But it failed to do so due to tag requirements."), *SearchName.ToString(), *MatchingAbilitiesString);
					}
				}
				else
				{
					OutputDevice.Logf(TEXT("Requested Tag '%s' matched no given Gameplay Abilities to %s."), *SearchName.ToString(), *GetNameSafe(PC));
				}

				return;
			}
		}

		// We couldn't find anything the user was searching for, so early out
		TSubclassOf<UGameplayAbility> ActivateAbilityClass = UE::AbilitySystemGlobals::FuzzyFindClass<UGameplayAbility>(SearchString);
		if (!ActivateAbilityClass)
		{
			OutputDevice.Logf(TEXT("Could not find a valid Gameplay Ability based on Search String '%s'"), *SearchString);
			return;
		}

		// If we're not the authority, we should check if we need to send the command to the server
		if (!ASC->IsOwnerActorAuthoritative())
		{
			const UGameplayAbility* Ability = ActivateAbilityClass.GetDefaultObject();
			const bool bSendToServer = Ability && (Ability->GetNetExecutionPolicy() >= EGameplayAbilityNetExecutionPolicy::ServerInitiated);
			if (bSendToServer)
			{
				const FString ServerCommand = FString::Printf(TEXT("AbilitySystem.Ability.Activate %s"), *SearchString);
				PC->ServerExec(ServerCommand);

				OutputDevice.Logf(TEXT("Sent Command '%s' from Player '%s' to Server (Reason: Net Execution Policy)."), *ServerCommand, *GetNameSafe(PC), *GetNameSafe(ActivateAbilityClass.Get()));
				return;
			}
		}

		// We found what the user was searching for, so let's try to activate it.  First, let's assume the Ability was already granted.
		bool bSuccess = ASC->TryActivateAbilityByClass(ActivateAbilityClass);
		if (bSuccess)
		{
			OutputDevice.Logf(TEXT("Successfully Activated Granted Ability '%s' on Player '%s'."), *GetNameSafe(ActivateAbilityClass.Get()), *GetNameSafe(PC));
			return;
		}

		// It wasn't granted (or we failed to activate it even though it was granted).  We can't really differentiate those two, so let's grant it, then activate it.
		FGameplayAbilitySpec AbilitySpec{ ActivateAbilityClass };
		FGameplayAbilitySpecHandle SpecHandle = ASC->GiveAbilityAndActivateOnce(AbilitySpec);
		if (SpecHandle.IsValid())
		{
			OutputDevice.Logf(TEXT("Successfully Granted, then Activated '%s' on Player '%s'."), *GetNameSafe(ActivateAbilityClass.Get()), *GetNameSafe(PC));
		}
		else
		{
			OutputDevice.Logf(TEXT("Failed to Grant & Activate '%s' on Player '%s'."), *GetNameSafe(ActivateAbilityClass.Get()), *GetNameSafe(PC));
		}
	}), ECVF_Cheat);


//
// AbilitySystem.Effect Debug Commands
//

FAutoConsoleCommand DebugAbilitySystemEffectListActiveCommand(TEXT("AbilitySystem.Effect.ListActive"), TEXT("Lists all of the Gameplay Effects currently active on the Player"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

		APlayerController* PC = World->GetFirstPlayerController();
		UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
		if (!ASC)
		{
			OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
			return;
		}

		OutputDevice.Logf(TEXT("Active Gameplay Effects on %s (%s):"), *PC->GetName(), *ASC->GetName());

		const FActiveGameplayEffectsContainer& ActiveGEContainer = ASC->GetActiveGameplayEffects();
		for (const FActiveGameplayEffectHandle& ActiveGEHandle : ActiveGEContainer.GetAllActiveEffectHandles())
		{
			if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveGEHandle))
			{
				const UGameplayEffect* GESpecDef = ActiveGE->Spec.Def;
				const FString ActiveState = ActiveGE->bIsInhibited ? FString(TEXT("Inhibited")) : FString::Printf(TEXT("Active (Stack: %d)"), ActiveGE->Spec.GetStackCount());

				OutputDevice.Logf(TEXT(" [%u] %s: %s"), GetTypeHash(ActiveGEHandle), *GetNameSafe(GESpecDef), *ActiveState);
			}
		}
	}), ECVF_Cheat);

FAutoConsoleCommand DebugAbilitySystemEffectRemoveCommand(TEXT("AbilitySystem.Effect.Remove"), TEXT("<Handle/Name>. Remove a Gameplay Effect that is currently active on the Player"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
		{
			UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

			APlayerController* PC = World->GetFirstPlayerController();
			UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
			if (!ASC)
			{
				OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
				return;
			}

			if (Args.Num() < 1)
			{
				if (IConsoleObject* ListActiveCommand = IConsoleManager::Get().FindConsoleObject(TEXT("AbilitySystem.Effect.ListActive"), false))
				{
					ListActiveCommand->AsCommand()->Execute(Args, World, OutputDevice);
				}

				return;
			}

			uint32 SearchHandleValue = 0;
			FString SearchString = Args[0];
			if (SearchString.IsNumeric())
			{
				int64 SearchHash64 = FCString::Atoi64(*SearchString);
				if (SearchHash64 > 0)
				{
					SearchHandleValue = static_cast<uint32>(SearchHash64 & UINT_MAX);
				}
			}

			bool bAnyMatch = false;
			const FActiveGameplayEffectsContainer& ActiveGEContainer = ASC->GetActiveGameplayEffects();
			for (const FActiveGameplayEffectHandle& ActiveGEHandle : ActiveGEContainer.GetAllActiveEffectHandles())
			{
				if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ActiveGEHandle))
				{
					const UGameplayEffect* GESpecDef = ActiveGE->Spec.Def;
					const uint32 ActiveGEHandleValue = GetTypeHash(ActiveGEHandle);

					const bool bMatch = (SearchHandleValue > 0) ? (ActiveGEHandleValue == SearchHandleValue) : GESpecDef->GetName().Contains(SearchString);
					if (bMatch)
					{
						OutputDevice.Logf(TEXT("Removing Active GE Handle %u (%s)"), ActiveGEHandleValue, *GESpecDef->GetName());
						ASC->RemoveActiveGameplayEffect(ActiveGEHandle);
						bAnyMatch = true;
					}
				}
			}

			if (!bAnyMatch)
			{
				OutputDevice.Logf(TEXT("Found no Active Gameplay Effects on %s (%s) that matched '%s'"), *PC->GetName(), *ASC->GetName(), *SearchString);
			}
		}), ECVF_Cheat);

FAutoConsoleCommand DebugAbilitySystemEffectApply(TEXT("AbilitySystem.Effect.Apply"), TEXT("<Class/AssetName> [Level]. Apply a Gameplay Effect on the Player.  Substring name matching works for Asset Tags, Asset Paths, or Class Names."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();

		APlayerController* PC = World->GetFirstPlayerController();
		UAbilitySystemComponent* ASC = PC ? AbilitySystemGlobals.GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
		if (!ASC)
		{
			OutputDevice.Logf(TEXT("Could not find Player (%s) with AbilitySystemComponent in World (%s)"), *GetNameSafe(PC), *GetNameSafe(World));
			return;
		}

		// We couldn't find anything the user was searching for, so early out
		const FString SearchString = Args[0];
		TSubclassOf<UGameplayEffect> GameplayEffectClass = UE::AbilitySystemGlobals::FuzzyFindClass<UGameplayEffect>(SearchString);
		if (!GameplayEffectClass)
		{
			OutputDevice.Logf(TEXT("Could not find a valid Gameplay Effect based on Search String '%s'"), *SearchString);
			return;
		}

		// Create a GameplayEffectSpec that executes the passed-in parameters
		FGameplayEffectContextHandle GEContextHandle = ASC->MakeEffectContext();
		const float Level = FMath::Max(FGameplayEffectConstants::INVALID_LEVEL, (Args.Num() > 1) ? FPlatformString::Atof(*Args[1]) : FGameplayEffectConstants::INVALID_LEVEL);
		FGameplayEffectSpec GESpec{ GameplayEffectClass.GetDefaultObject(), GEContextHandle, Level };

		// We need to create a new valid prediction key (as if we just started activating an ability) so the GE will fire even if we are not the authority.
		FPredictionKey FakePredictionKey = (PC->GetLocalRole() == ENetRole::ROLE_Authority) ? FPredictionKey() : FPredictionKey::CreateNewPredictionKey(ASC);
		FActiveGameplayEffectHandle ActiveGEHandle = ASC->ApplyGameplayEffectSpecToSelf(GESpec, FakePredictionKey);
		if (ActiveGEHandle.IsValid())
		{
			OutputDevice.Logf(TEXT("Successfully Applied Gameplay Effect '%s' on Player '%s'."), *GameplayEffectClass->GetName(), *PC->GetName());
		}
		else
		{
			OutputDevice.Logf(TEXT("Failed to Apply Gameplay Effect '%s' on Player '%s'."), *GameplayEffectClass->GetName(), *PC->GetName());
		}
	}), ECVF_Cheat);
