// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SourceControlHelpers.h"

//This class should not be used as a base class because it can force a connection to the SCM through FScopedSourceControl. Connection errors can result in failing automation processes that are unrelated. 
//The same can happen if the class is used as a member of another class. It should only be used as a local variable in a method so the connection is only attempted if required. 
class UNREALED_API FPackageSourceControlHelper final
{
public:
	bool UseSourceControl() const;
	bool Delete(const FString& PackageName) const;
	bool Delete(const TArray<FString>& PackageNames, bool bErrorsAsWarnings = false) const;
	bool Delete(UPackage* Package) const;
	bool Delete(const TArray<UPackage*>& Packages) const;
	bool AddToSourceControl(UPackage* Package) const;
	bool AddToSourceControl(const TArray<FString>& PackageNames, bool bErrorsAsWarnings = false) const;
	bool Checkout(UPackage* Package) const;
	bool Checkout(const TArray<FString>& PackageNames, bool bErrorsAsWarnings = false) const;
	bool GetDesiredStatesForModification(const TArray<FString>& PackageNames, TArray<FString>& OutPackagesToCheckout, TArray<FString>& OutPackagesToAdd, bool bErrorsAsWarnings = false) const;
private:
	ISourceControlProvider& GetSourceControlProvider() const;
	FScopedSourceControl SourceControl;	
};
