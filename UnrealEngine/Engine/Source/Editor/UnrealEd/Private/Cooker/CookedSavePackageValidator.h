// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

class ITargetPlatform;
class UCookOnTheFlyServer;
class UObject;

namespace UE::Cook
{

/** A SavePackageValidator that tests that the package is valid for the given TargetPlatform. */
class FCookedSavePackageValidator : public ISavePackageValidator
{
public:
	FCookedSavePackageValidator(const ITargetPlatform* InTargetPlatform, UCookOnTheFlyServer& InCOTFS);
	virtual ESavePackageResult ValidateImports(const UPackage* Package, const TSet<UObject*>& Imports) override;

private:
	const ITargetPlatform* TargetPlatform;
	UCookOnTheFlyServer& COTFS;
	TSet<FName> SuppressedNativeScriptPackages;
};

}