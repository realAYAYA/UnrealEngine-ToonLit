// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogContentBundle, Log, All);

class FContentBundleBase;	
class FContentBundleClient;
class FContentBundleContainer;
class UContentBundleDescriptor;
class UWorld;

namespace ContentBundle
{
	namespace Log
	{
		ENGINE_API FString MakeDebugInfoString(const FContentBundleBase& ContentBundle);
		ENGINE_API FString MakeDebugInfoString(const FContentBundleClient& ContentBundleClient);
		ENGINE_API FString MakeDebugInfoString(const FContentBundleContainer& ContentBundleContainer);
		ENGINE_API FString MakeDebugInfoString(const FContentBundleClient& ContentBundleClient, UWorld* World);
	}
}