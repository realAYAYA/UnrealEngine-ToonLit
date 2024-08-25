// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureAction_AddChunkOverride.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerSettings.h"
#include "GameFeatureData.h"
#include "Misc/MessageDialog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddChunkOverride)

#define LOCTEXT_NAMESPACE "GameFeatures"

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_AddChunkOverride

DEFINE_LOG_CATEGORY_STATIC(LogAddChunkOverride, Log, All);

namespace GameFeatureAction_AddChunkOverride
{
	static TMap<int32, FString> ChunkIdToPluginMap;
	static TMap<FString, int32> PluginToChunkId;
}

UGameFeatureAction_AddChunkOverride::FShouldAddChunkOverride UGameFeatureAction_AddChunkOverride::ShouldAddChunkOverride;

void UGameFeatureAction_AddChunkOverride::OnGameFeatureRegistering()
{
	const bool bShouldAddChunkOverride = ShouldAddChunkOverride.IsBound() ? ShouldAddChunkOverride.Execute(GetTypedOuter<UGameFeatureData>()) : true;
	if (bShouldAddChunkOverride)
	{
		AddChunkIdOverride();
	}
}

void UGameFeatureAction_AddChunkOverride::OnGameFeatureUnregistering()
{
	RemoveChunkIdOverride();
}

#if WITH_EDITOR
void UGameFeatureAction_AddChunkOverride::GetChunkForPackage(const FName PackageName, const int32 DefaultGameChunk, TArray<int32>& OutChunkList)
{
	TSet<FPrimaryAssetId> Managers;
	UAssetManager::Get().GetPackageManagers(PackageName, true, Managers);
	GetChunkForPackage(PackageName.ToString(), Managers, DefaultGameChunk, OutChunkList);
}

void UGameFeatureAction_AddChunkOverride::GetChunkForPackage(const FString& PackageName, const TSet<FPrimaryAssetId>& Managers, const int32 DefaultGameChunk, TArray<int32>& OutChunkList)
{
	if (GameFeatureAction_AddChunkOverride::PluginToChunkId.Num() == 0)
	{
		return;
	}

	auto ResolveMultiChunkDependecies = [PackageName, &Managers, DefaultGameChunk,  &OutChunkList]()
	{
		UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("%s was referenced by one than one chunk"), *PackageName);
		for (const int32 OutChunkId : OutChunkList)
		{
			UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("%s was referenced by chunk %d"), *PackageName, OutChunkId);
		}

		if (OutChunkList.Num() > 1 && !OutChunkList.Contains(0))
		{
			UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("Forcing %s into gameplay chunk %d. It was referend by multiple GFPs which might load at different times."), *PackageName, DefaultGameChunk);
			OutChunkList.Reset();
			OutChunkList.Add(DefaultGameChunk);
		}
		else
		{
			// If multiple package mangers exist for this package with different chunk IDs it should go into the default game chunk.
			UAssetManager& AssetManager = UAssetManager::Get();
			TSet<int32> ManagerChunkIds;
			for (const FPrimaryAssetId& PrimaryAssetId : Managers)
			{
				FPrimaryAssetRules Rules = AssetManager.GetPrimaryAssetRules(PrimaryAssetId);
				if (Rules.ChunkId != INDEX_NONE)
				{
					ManagerChunkIds.Add(Rules.ChunkId);
				}
			}
			if (ManagerChunkIds.Num() > 1)
			{
				UE_LOG(LogAddChunkOverride, Log, TEXT("Forcing %s into gameplay chunk %d. It was referend by multiple GFPs which might load at different times. Package managers with a valid chunkID might not have been registered for this type."), *PackageName, DefaultGameChunk);
				OutChunkList.Reset();
				OutChunkList.Add(DefaultGameChunk);
			}
		}
	};

	static const FString EngineDir(TEXT("/Engine/"));
	static const FString GameDir(TEXT("/Game/"));
	if (PackageName.StartsWith(EngineDir, ESearchCase::CaseSensitive))
	{
		return;
	}
	else if (PackageName.StartsWith(GameDir, ESearchCase::CaseSensitive))
	{
		ResolveMultiChunkDependecies();
	}
	else
	{
		TArray<FString> BrokenString;
		PackageName.ParseIntoArray(BrokenString, TEXT("/"));
		if (BrokenString.Num() > 0)
		{
			FString PluginName = BrokenString[0];
			if (GameFeatureAction_AddChunkOverride::PluginToChunkId.Contains(PluginName))
			{
				int32 ExpectedChunkId = GameFeatureAction_AddChunkOverride::PluginToChunkId[PluginName];
				if (OutChunkList.Contains(ExpectedChunkId) && OutChunkList.Num() > 1)
				{
					UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("%s is referenced by expected chunk %d but is also referend by other chunks."), *PackageName, ExpectedChunkId);
					if (LogAddChunkOverride.GetVerbosity() == ELogVerbosity::VeryVerbose)
					{
						for (const int32 ChunkId : OutChunkList)
						{
							UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("%s was referenced by chunk %d"), *PackageName, ChunkId);
						}
					}
				}
				else
				{
					UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("%s was expected to be in chunk %d but not found there. This could be because of a GameplayCue or this packages is only cooking because it is referend by another plugin"), *PackageName, ExpectedChunkId);
				}

				UE_LOG(LogAddChunkOverride, VeryVerbose, TEXT("Forcing %s into chunk %d"), *PackageName, ExpectedChunkId);
				OutChunkList.Reset();
				OutChunkList.Add(ExpectedChunkId);
			}
			else if (OutChunkList.Num() > 1)
			{
				ResolveMultiChunkDependecies();
			}
		}
	}
}

FString UGameFeatureAction_AddChunkOverride::GetPluginNameFromChunkID(int32 ChunkID)
{
	return GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap.FindRef(ChunkID);
}

void UGameFeatureAction_AddChunkOverride::PostRename(UObject* OldOuter, const FName OldName)
{
	Super::PostRename(OldOuter, OldName);

	// If OldOuter is not GetTransientPackage(), but GetOuter() is GetTransientPackage(), then you were trashed.
	const UObject* MyOuter = GetOuter();
	const UPackage* TransientPackage = GetTransientPackage();
	if (OldOuter != TransientPackage && MyOuter == TransientPackage)
	{
		RemoveChunkIdOverride();
	}
}

void UGameFeatureAction_AddChunkOverride::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UGameFeatureAction_AddChunkOverride, bShouldOverrideChunk))
	{
		RemoveChunkIdOverride();
		// Generate a new value if we have an invalid chunkId
		if (bShouldOverrideChunk && ChunkId < 0)
		{
			UE_LOG(LogAddChunkOverride, Log, TEXT("Detected invalid ChunkId autogenerating new ID based on PluginName"));
			ChunkId = GenerateUniqueChunkId();
		}
		if (ChunkId >= 0)
		{
			AddChunkIdOverride();
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UGameFeatureAction_AddChunkOverride, ChunkId))
	{
		RemoveChunkIdOverride();
		AddChunkIdOverride();
	}
}

int32 UGameFeatureAction_AddChunkOverride::GetLowestAllowedChunkId()
{
	if (const UGameFeatureAction_AddChunkOverride* Action = UGameFeatureAction_AddChunkOverride::StaticClass()->GetDefaultObject<UGameFeatureAction_AddChunkOverride>())
	{
		return Action->LowestAllowedChunkIndexForAutoGeneration;
	}
	else
	{
		ensureMsgf(false, TEXT("Unable to get class default object for UGameFeatureAction_AddChunkOverride"));
		return INDEX_NONE;
	}
}

#endif // WITH_EDITOR

void UGameFeatureAction_AddChunkOverride::AddChunkIdOverride()
{
#if WITH_EDITOR
	if (!bShouldOverrideChunk)
	{
		return;
	}
	if (ChunkId < 0)
	{
		UE_LOG(LogAddChunkOverride, Error, TEXT("ChunkId is negative. Unable to override to a negative chunk"));
		return;
	}
	if (GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap.Contains(ChunkId))
	{
		FString PluginName;
		if (UGameFeatureData* GameFeatureData = GetTypedOuter<UGameFeatureData>())
		{
			GameFeatureData->GetPluginName(PluginName);
		}
		UE_LOG(LogAddChunkOverride, Error, TEXT("ChunkId (%d) is already in use by %s. Manually resolve the conflict for %s"), ChunkId, *GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap[ChunkId], *PluginName);
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("AddChunkOverride_IdConflight", "Chunk Id is already in use by '{0}'."), FText::FromString(PluginName)));
		return;
	}

	if (UGameFeatureData* GameFeatureData = GetTypedOuter<UGameFeatureData>())
	{
		FString PluginName;
		GameFeatureData->GetPluginName(PluginName);
		GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap.Add(ChunkId, PluginName);
		GameFeatureAction_AddChunkOverride::PluginToChunkId.Add(PluginName, ChunkId);
		LastChunkIdUsed = ChunkId;
		UE_LOG(LogAddChunkOverride, Log, TEXT("Plugin(%s) will cook assets into chunk(%d)"), *PluginName, ChunkId);

		UAssetManager& Manager = UAssetManager::Get();

		FPrimaryAssetRules GFDRules;
		GFDRules.ChunkId = ChunkId;
		Manager.SetPrimaryAssetRules(GameFeatureData->GetPrimaryAssetId(), GFDRules);

		for (const FPrimaryAssetTypeInfo& AssetTypeInfo : GameFeatureData->GetPrimaryAssetTypesToScan())
		{
			FPrimaryAssetRulesCustomOverride Override;
			Override.PrimaryAssetType = FPrimaryAssetType(AssetTypeInfo.PrimaryAssetType);
			Override.FilterDirectory.Path = FString::Printf(TEXT("/%s"), *PluginName);
			Override.Rules.ChunkId = ChunkId;
			Manager.ApplyCustomPrimaryAssetRulesOverride(Override);
		}
	}
#endif // WITH_EDITOR
}

void UGameFeatureAction_AddChunkOverride::RemoveChunkIdOverride()
{
#if WITH_EDITOR
	if (LastChunkIdUsed < 0)
	{
		UE_LOG(LogAddChunkOverride, Verbose, TEXT("LastChunkIdUsed(%d) was invalid. Skipping override removal"), LastChunkIdUsed);
		return;
	}

	// Remove primary asset rules by setting the override the default.
	if (UGameFeatureData* GameFeatureData = GetTypedOuter<UGameFeatureData>())
	{
		FString PluginName;
		GameFeatureData->GetPluginName(PluginName);

		ensure(GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap.Remove(LastChunkIdUsed));
		ensure(GameFeatureAction_AddChunkOverride::PluginToChunkId.Remove(PluginName));
		UE_LOG(LogAddChunkOverride, Log, TEXT("Removing ChunkId override (%d) for Plugin (%s)"), LastChunkIdUsed, *PluginName);
		LastChunkIdUsed = -1;

		UAssetManager& Manager = UAssetManager::Get();

		Manager.SetPrimaryAssetRules(GameFeatureData->GetPrimaryAssetId(), FPrimaryAssetRules());
		for (const FPrimaryAssetTypeInfo& AssetTypeInfo : GameFeatureData->GetPrimaryAssetTypesToScan())
		{
			FPrimaryAssetRulesCustomOverride Override;
			Override.PrimaryAssetType = FPrimaryAssetType(AssetTypeInfo.PrimaryAssetType);
			Override.FilterDirectory.Path = FString::Printf(TEXT("/%s"), *PluginName);
			Manager.ApplyCustomPrimaryAssetRulesOverride(Override);
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
int32 UGameFeatureAction_AddChunkOverride::GenerateUniqueChunkId() const
{
	// Holdover auto-generation function until we can allow for Chunks to be specified by string name
	int32 NewChunkId = -1;
	UGameFeatureData* GameFeatureData = GetTypedOuter<UGameFeatureData>();
	if (ensure(GameFeatureData))
	{
		FString PluginName;
		GameFeatureData->GetPluginName(PluginName);

		uint32 NewId = GetTypeHash(PluginName);
		int16 SignedId = NewId;
		if (SignedId < 0)
		{
			SignedId = -SignedId;
		}
		NewChunkId = SignedId;
	}

	if (NewChunkId < LowestAllowedChunkIndexForAutoGeneration)
	{
		UE_LOG(LogAddChunkOverride, Warning, TEXT("Autogenerated ChunkId(%d) is lower than the config specified LowestAllowedChunkIndexForAutoGeneration(%d)"), NewChunkId, LowestAllowedChunkIndexForAutoGeneration);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddChunkOverride_InvalidId", "Autogenerated ChunkID is lower than config specified LowestAllowedChunkIndexForAutoGeneration. Please manually assign a valid Chunk Id"));
		NewChunkId = -1;
	}
	else if (GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap.Contains(NewChunkId))
	{
		UE_LOG(LogAddChunkOverride, Warning, TEXT("ChunkId(%d) is in use by %s. Unable to autogenerate unique id. Lowest allowed ChunkId(%d)"), NewChunkId, *GameFeatureAction_AddChunkOverride::ChunkIdToPluginMap[NewChunkId], LowestAllowedChunkIndexForAutoGeneration);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AddChunkOverride_UsedChunkId", "Unable to auto generate unique valid Chunk Id. Please manually assign a valid Chunk Id"));
		NewChunkId = -1;
	}

	return NewChunkId;
}
#endif // WITH_EDITOR

//////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

