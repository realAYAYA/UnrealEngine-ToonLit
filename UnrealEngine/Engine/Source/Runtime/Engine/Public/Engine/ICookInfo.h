// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "UObject/NameTypes.h"

#if WITH_EDITOR

namespace UE::Cook
{

/**
 * List the keywords that should be enum values of the EInstigator enum
 * The list is declared as a macro instead of ordinary C keywords in the enum declaration
 * so that we can reduce duplication in the functions that specify the string name and other properties for each
 * enum value; see the uses in ICookInfo.cpp.
 */
#define EINSTIGATOR_VALUES(CallbackMacro) \
	/** CallbackMacro(CPPToken Name, bool bAllowUnparameterized) */ \
	CallbackMacro(InvalidCategory, true) \
	CallbackMacro(NotYetRequested, true) \
	CallbackMacro(Unspecified, false) \
	CallbackMacro(StartupPackage, true) \
	CallbackMacro(AlwaysCookMap, true) \
	CallbackMacro(IniMapSection, false) \
	CallbackMacro(IniAllMaps, true) \
	CallbackMacro(CommandLinePackage, true) \
	CallbackMacro(CommandLineDirectory, true) \
	CallbackMacro(DirectoryToAlwaysCook, false) \
	CallbackMacro(FullDepotSearch, true) \
	CallbackMacro(GameDefaultObject, false) \
	CallbackMacro(InputSettingsIni, true) \
	CallbackMacro(StartupSoftObjectPath, true) \
	CallbackMacro(PackagingSettingsMapToCook, true) \
	CallbackMacro(CookModificationDelegate, true) \
	CallbackMacro(ModifyCookDelegate, true) \
	CallbackMacro(AssetManagerModifyCook, true) \
	CallbackMacro(AssetManagerModifyDLCCook, true) \
	CallbackMacro(TargetPlatformExtraPackagesToCook, true) \
	CallbackMacro(ConsoleCommand, true) \
	CallbackMacro(CookOnTheFly, true) \
	CallbackMacro(IterativeCook, true) \
	CallbackMacro(PreviousAssetRegistry, true) \
	CallbackMacro(RequestPackageFunction, true) \
	CallbackMacro(Dependency, false) \
	CallbackMacro(HardDependency, false) \
	CallbackMacro(SoftDependency, false) \
	CallbackMacro(Unsolicited, false) \
	CallbackMacro(GeneratedPackage, false) \


/** The different ways a package can be discovered by the cooker. */
enum class EInstigator : uint8
{
#define EINSTIGATOR_VALUE_CALLBACK(Name, bAllowUnparameterized) Name,
	EINSTIGATOR_VALUES(EINSTIGATOR_VALUE_CALLBACK)
#undef EINSTIGATOR_VALUE_CALLBACK
	Count,
};
ENGINE_API const TCHAR* LexToString(EInstigator Value);

/** Category and referencer for how a package was discovered by the cooker. */
struct FInstigator
{
	FName Referencer;
	EInstigator Category;

	FInstigator() : Category(EInstigator::InvalidCategory)
	{
	}
	FInstigator(EInstigator InCategory, FName InReferencer = NAME_None) : Referencer(InReferencer), Category(InCategory)
	{
	}

	ENGINE_API FString ToString() const;
};

/** Engine interface for information provided by UCookOnTheFlyServer in cook callbacks. */
class ICookInfo
{
public:
	/**
	 * Return the instigator that first caused the package to be requested by the cook.
	 * Returns category EInstigator::NotYetRequested if package is not yet known to the cook.
	 */
	virtual FInstigator GetInstigator(FName PackageName) = 0;
	/**
	 * Return the chain of instigators that caused the package to be requested by the cook.
	 * First element is the direct instigator of the package, last is the root instigator that started the chain.
	 */
	virtual TArray<FInstigator> GetInstigatorChain(FName PackageName) = 0;
};

}

#endif