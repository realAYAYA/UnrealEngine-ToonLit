// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookingUtils.h"

#if WITH_EDITOR
#include "LogVCamCore.h"
#include "Modifier/ModifierStackEntry.h"
#include "Output/VCamOutputProviderBase.h"

#include "Algo/IndexOf.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/ObjectPtr.h"
#endif

namespace UE::VCamCore::CookingUtils::Private
{
#if WITH_EDITOR
	static UClass& FindFirstNativeClass(UObject& Object)
	{
		UClass* Current = Object.GetClass();
		for (; Current && !Current->IsNative(); Current = Current->GetSuperClass())
		{}
		check(Current);
		return *Current;
	}

	void RemoveUnsupportedOutputProviders(TArray<TObjectPtr<UVCamOutputProviderBase>>& OutputProviders, const FObjectPreSaveContext& SaveContext)
	{
		for (auto OutputProviderIt = OutputProviders.CreateIterator(); OutputProviderIt; ++OutputProviderIt)
		{
			UVCamOutputProviderBase* OutputProvider = *OutputProviderIt;
			if (!OutputProvider || !CanIncludeInCookedGame(*OutputProvider, SaveContext))
			{
				UE_CLOG(OutputProvider, LogVCamCore, Display, TEXT("Excluding output provider \"%s\" because it is not supported by the target platform"), *OutputProvider->GetPathName());
				OutputProviderIt.RemoveCurrent();
			}
		}
	}

	void RemoveUnsupportedModifiers(TArray<FModifierStackEntry>& ModifierStack, const FObjectPreSaveContext& SaveContext)
	{
		for (auto ModifierIt = ModifierStack.CreateIterator(); ModifierIt; ++ModifierIt)
		{
			UVCamModifier* Modifier = ModifierIt->GeneratedModifier;
			if (!Modifier || !CanIncludeInCookedGame(*Modifier, SaveContext))
			{
				UE_CLOG(Modifier, LogVCamCore, Display, TEXT("Excluding modifier \"%s\" because it is not supported by the target platform"), *Modifier->GetPathName());
				ModifierIt.RemoveCurrent();
			}
		}
	}

	bool CanIncludeInCookedGame(UObject& Object, const FObjectPreSaveContext& SaveContext)
	{
		if (Object.IsEditorOnly())
		{
			return false;
		}

		UClass& FirstNativeParent = FindFirstNativeClass(Object);
		// Will have the pattern "/Script/[Module].[ClassName]", e.g. "/Script/VCamCore.VCamOutputViewport"
		const FString PackagePathName = FirstNativeParent.GetPathName();
		constexpr uint32 PrefixLastIndex = 7;
		int32 ModuleSeparatorIndex;
		const bool bFoundSeparator = PackagePathName.FindChar('.', ModuleSeparatorIndex);
		if (!ensureMsgf(bFoundSeparator, TEXT("Class path %s does not follow expected class path pattern!"), *PackagePathName))
		{
			return true; 
		}
		const FName ModuleName = *PackagePathName.Left(ModuleSeparatorIndex).Right(PrefixLastIndex);
		const IPluginManager& PluginManager = IPluginManager::Get();
		const TSharedPtr<IPlugin> OwningPlugin = PluginManager.GetModuleOwnerPlugin(ModuleName);
		if (UNLIKELY(!OwningPlugin))
		{
			// This should almost be a warning... it is highly unlikely that an engine module directly references VCamCore to create subclass of UVCamOutputProviderBase or UVCamModifier
			return true;
		}

		const TArray<FModuleDescriptor>& Modules = OwningPlugin->GetDescriptor().Modules;
		const int32 ModuleIndex = Algo::IndexOfByPredicate(Modules, [ModuleName](const FModuleDescriptor& Descriptor){ return Descriptor.Name == ModuleName; });
		check(Modules.IsValidIndex(ModuleIndex));
		
		const FModuleDescriptor& ModuleDescriptor = Modules[ModuleIndex];
		const FString& PlatformName = SaveContext.GetTargetPlatform()->PlatformName();
		const bool bIsAllowedPlatform = ModuleDescriptor.PlatformAllowList.Contains(PlatformName) && !ModuleDescriptor.PlatformDenyList.Contains(PlatformName);
		const TSet<EHostType::Type> AllowedHostTypes = { EHostType::Runtime, EHostType::CookedOnly, EHostType::ClientOnly };
		const bool bIsSupportedTarget = AllowedHostTypes.Contains(ModuleDescriptor.Type);
		
		const bool bCanInclude = bIsAllowedPlatform && bIsSupportedTarget;
		return bCanInclude;
	}
#endif
}