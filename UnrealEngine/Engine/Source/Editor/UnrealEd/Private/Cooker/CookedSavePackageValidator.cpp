// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookedSavePackageValidator.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CookOnTheSide/CookLog.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Engine/ICookInfo.h"
#include "HAL/Platform.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/PackageName.h"
#include "ModuleDescriptor.h"
#include "PluginDescriptor.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

namespace UE::Cook
{

FCookedSavePackageValidator::FCookedSavePackageValidator(const ITargetPlatform* InTargetPlatform,
	UCookOnTheFlyServer& InCOTFS)
	: TargetPlatform(InTargetPlatform)
	, COTFS(InCOTFS)
{
	for (TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (!TargetPlatform->IsEnabledForPlugin(Plugin.Get()))
		{
			for (const FModuleDescriptor& Module : Plugin->GetDescriptor().Modules)
			{
				SuppressedNativeScriptPackages.Add(FPackageName::GetModuleScriptPackageName(Module.Name));
			}
		}
	}
}

ESavePackageResult FCookedSavePackageValidator::ValidateImports(const UPackage* Package, const TSet<UObject*>& Imports)
{
	for (UObject* Object : Imports)
	{
		UClass* Class = Cast<UClass>(Object);
		if (Class && Class->IsNative() && SuppressedNativeScriptPackages.Contains(Class->GetPackage()->GetFName()))
		{
			FInstigator Instigator = COTFS.GetInstigator(Package->GetFName());
			bool bIsError = true;
			if (Instigator.Category == EInstigator::StartupPackage)
			{
				// StartupPackages might be around just because of the editor;
				// if they're not available on client, ignore them without error
				bIsError = false;
			}

			// If you receive this message in a package that you do want to cook, you can remove the object of the
			// unavailable class by overriding UObject::NeedsLoadForTargetPlatform on that class to return false.
			if (bIsError)
			{
				UE_LOG(LogCook, Error, TEXT("Failed to cook %s for platform %s. It imports class %s, which is in a module that is not available on the platform."),
					*Package->GetName(), *TargetPlatform->PlatformName(), *Class->GetPathName());
				return ESavePackageResult::ValidatorError;
			}
			else
			{
				UE_LOG(LogCook, Display, TEXT("Skipping package %s for platform %s. It imports class %s, which is in a module that is not available on the platform."),
					*Package->GetName(), *TargetPlatform->PlatformName(), *Class->GetPathName());
				return ESavePackageResult::ValidatorSuppress;
			}
		}
	}
	return ESavePackageResult::Success;
}

}
