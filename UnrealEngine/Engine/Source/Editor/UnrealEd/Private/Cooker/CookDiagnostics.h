// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"

class ITargetPlatform;
class UCookOnTheFlyServer;
namespace UE::Cook { enum class EInstigator : uint8; }
namespace UE::Cook { struct FPackageData; }

namespace UE::Cook
{

class FDiagnostics
{
public:
	/**
	 * Compares packages that would be added to the cook when using legacy WhatGetsCookedRules to the packages that
	 * are added to the cook using OnlyEditorOnly rules. Silently ignores expected differences, but logs a message
	 * with diagnostics for unexpected differences.
	 */
	static void AnalyzeHiddenDependencies(UCookOnTheFlyServer& COTFS, FPackageData& PackageData,
		TMap<FPackageData*, EInstigator>&& UnsolicitedForPackage, TSet<FPackageData*>& SaveReferences,
		TConstArrayView<const ITargetPlatform*> ReachablePlatforms, bool bOnlyEditorOnlyDebug,
		bool bHiddenDependenciesDebug);

};

}