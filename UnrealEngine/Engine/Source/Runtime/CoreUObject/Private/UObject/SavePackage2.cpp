// Copyright Epic Games, Inc. All Rights Reserved.


#include "UObject/SavePackage.h"

#if UE_WITH_SAVEPACKAGE
#include "AssetRegistry/AssetData.h"
#include "Async/ParallelFor.h"
#include "Blueprint/BlueprintSupport.h"
#include "HAL/FileManager.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Internationalization/PackageLocalizationManager.h"
#include "IO/IoDispatcher.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/PathViews.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/SecureHash.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "Serialization/ArchiveStackTrace.h"
#include "Serialization/EditorBulkData.h"
#include "Serialization/Formatters/JsonArchiveOutputFormatter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PackageWriter.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Serialization/UnversionedPropertySerialization.h"
#include "UObject/DebugSerializationFlags.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/LinkerSave.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage/PackageHarvester.h"
#include "UObject/SavePackage/SaveContext.h"
#include "UObject/SavePackage/SavePackageUtilities.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"
#endif

// defined in UObjectGlobals.cpp
COREUOBJECT_API extern bool GOutputCookingWarnings;

namespace
{

int32 GFixupStandaloneFlags = 0;
static FAutoConsoleVariableRef CVar_FixupStandaloneFlags(
	TEXT("save.FixupStandaloneFlags"),
	GFixupStandaloneFlags,
	TEXT("If non-zero, when the UAsset of a package is missing RF_Standalone, the flag is added. If zero, the flags are not changed and the save fails.")
);

ESavePackageResult WriteAdditionalFiles(FSaveContext& SaveContext, FScopedSlowTask& SlowTask, int64 LinkerSize);

ESavePackageResult ReturnSuccessOrCancel()
{
	return !GWarn->ReceivedUserCancel() ? ESavePackageResult::Success : ESavePackageResult::Canceled;
}

ESavePackageResult ValidatePackage(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_ValidatePackage);

	// Platform can't save the package
	if (!FPlatformProperties::HasEditorOnlyData())
	{
		return ESavePackageResult::Error;
	}

	// Check recursive save package call
	if (GIsSavingPackage && !SaveContext.IsConcurrent())
	{
		ensureMsgf(false, TEXT("Recursive SavePackage() is not supported"));
		return ESavePackageResult::Error;
	}

	FString FilenameStr(SaveContext.GetFilename());

	/// Cooking checks
	if (SaveContext.IsCooking())
	{
#if WITH_EDITORONLY_DATA
		// if we strip editor only data, validate the package isn't referenced only by editor data
		// This check has to be done prior to validating the asset, because invalid state in the
		// package after stripping editoronly objects is okay if we're going to skip saving the whole package.
		if (SaveContext.IsStripEditorOnly())
		{
			// Don't save packages marked as editor-only.
			if (SaveContext.CanSkipEditorReferencedPackagesWhenCooking() && SaveContext.GetPackage()->IsLoadedByEditorPropertiesOnly())
			{
				UE_CLOG(SaveContext.IsGenerateSaveError(), LogSavePackage, Verbose, TEXT("Package loaded by editor-only properties: %s. Package will not be saved."), *SaveContext.GetPackage()->GetName());
				return ESavePackageResult::ReferencedOnlyByEditorOnlyData;
			}
			else if (SaveContext.GetPackage()->HasAnyPackageFlags(PKG_EditorOnly))
			{
				UE_CLOG(SaveContext.IsGenerateSaveError(), LogSavePackage, Verbose, TEXT("Package marked as editor-only: %s. Package will not be saved."), *SaveContext.GetPackage()->GetName());
				return ESavePackageResult::ReferencedOnlyByEditorOnlyData;
			}
		}
#endif
	}

	UObject* Asset = SaveContext.GetAsset();
	if (Asset)
	{
		// If an asset is provided, validate it is in the package
		if (!Asset->IsInPackage(SaveContext.GetPackage()))
		{
			if (SaveContext.IsGenerateSaveError() && SaveContext.GetError())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), FText::FromString(FilenameStr));
				FText ErrorText = FText::Format(NSLOCTEXT("SavePackage2", "AssetSaveNotInPackage", "The Asset '{Name}' being saved is not in the provided package."), Arguments);
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
			}
			return ESavePackageResult::Error;
		}

		// If An asset is provided, validate it has the requested TopLevelFlags. This is necessary to prevent dataloss, but only
		// when saving packages to the WorkspaceDomain
		EObjectFlags TopLevelFlags = SaveContext.GetTopLevelFlags();
		if (!SaveContext.IsCooking() && TopLevelFlags != RF_NoFlags && !Asset->HasAnyFlags(TopLevelFlags))
		{
			if (SaveContext.IsFixupStandaloneFlags() && !Asset->GetExternalPackage() && EnumHasAnyFlags(TopLevelFlags, RF_Standalone))
			{
				UE_LOG(LogSavePackage, Warning, TEXT("The Asset %s being saved is missing the RF_Standalone flag; adding it."), *Asset->GetPathName());
				Asset->SetFlags(RF_Standalone);
				check(Asset->HasAnyFlags(TopLevelFlags));
			}
			else
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), FText::FromString(Asset->GetPathName()));
				static_assert(sizeof(TopLevelFlags) <= sizeof(uint32), "Expect EObjectFlags to be uint32");
				Arguments.Add(TEXT("Flags"), FText::FromString(FString::Printf(TEXT("%x"), (uint32)TopLevelFlags)));
				FText ErrorText;
				if (!Asset->GetExternalPackage() && EnumHasAnyFlags(TopLevelFlags, RF_Standalone))
				{
					ErrorText = FText::Format(NSLOCTEXT("SavePackage2", "AssetSaveMissingStandaloneFlag", "The Asset {Name} being saved does not have any of the provided object flags (0x{Flags}); saving the package would cause data loss. Run with -dpcvars=save.FixupStandaloneFlags=1 to add the RF_Standalone flag."), 
						Arguments);
				}
				else
				{
					ErrorText = FText::Format(NSLOCTEXT("SavePackage2", "AssetSaveMissingTopLevelFlags", "The Asset {Name} being saved does not have any of the provided object flags (0x{Flags}); saving the package would cause data loss."),	
						Arguments);
				}
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
				return ESavePackageResult::Error;
			}
		}
	}

	// Make sure package is allowed to be saved.
	if (!SaveContext.IsCooking() && FCoreUObjectDelegates::IsPackageOKToSaveDelegate.IsBound())
	{
		bool bIsOKToSave = FCoreUObjectDelegates::IsPackageOKToSaveDelegate.Execute(SaveContext.GetPackage(), SaveContext.GetFilename(), SaveContext.GetError());
		if (!bIsOKToSave)
		{
			if (SaveContext.IsGenerateSaveError())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), FText::FromString(FilenameStr));
				FText FormatText = SaveContext.GetPackage()->ContainsMap() 
					? NSLOCTEXT("SavePackage2", "MapSaveNotAllowed", "Map '{Name}' is not allowed to save (see log for reason)") 
					: NSLOCTEXT("SavePackage2", "AssetSaveNotAllowed", "Asset '{Name}' is not allowed to save (see log for reason)");
				FText ErrorText = FText::Format(FormatText, Arguments);
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
			}
			return ESavePackageResult::Error;
		}
	}

	// Check if the package is fully loaded
	if (!SaveContext.GetPackage()->IsFullyLoaded())
	{
		if (SaveContext.IsGenerateSaveError())
		{
			// We cannot save packages that aren't fully loaded as it would clobber existing not loaded content.
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Name"), FText::FromString(FilenameStr));
			FText FormatText = SaveContext.GetPackage()->ContainsMap()
				? NSLOCTEXT("SavePackage2", "CannotSaveMapPartiallyLoaded", "Map '{Name}' cannot be saved as it has only been partially loaded")
				: NSLOCTEXT("SavePackage2", "CannotSaveAssetPartiallyLoaded", "Asset '{Name}' cannot be saved as it has only been partially loaded");
			FText ErrorText = FText::Format(FormatText, Arguments);
			SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorText.ToString());
		}
		return ESavePackageResult::Error;
	}

	// Warn about long package names, which may be bad for consoles with limited filename lengths.
	if (SaveContext.IsWarningLongFilename())
	{
		int32 MaxFilenameLength = FPlatformMisc::GetMaxPathLength();

		// If the name is of the form "_LOC_xxx.ext", remove the loc data before the length check
		FString BaseFilename = FPaths::GetBaseFilename(FilenameStr);
		FString CleanBaseFilename = BaseFilename;
		if (CleanBaseFilename.Find("_LOC_") == BaseFilename.Len() - 8)
		{
			CleanBaseFilename = BaseFilename.LeftChop(8);
		}
		if (CleanBaseFilename.Len() > MaxFilenameLength)
		{
			if (SaveContext.IsGenerateSaveError())
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("FileName"), FText::FromString(BaseFilename));
				Arguments.Add(TEXT("MaxLength"), FText::AsNumber(MaxFilenameLength));
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format(NSLOCTEXT("Core", "Error_FilenameIsTooLongForCooking", "Filename '{FileName}' is too long; this may interfere with cooking for consoles. Unreal filenames should be no longer than {MaxLength} characters."), Arguments).ToString());
			}
			else
			{
				UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf(TEXT("Filename is too long (%d characters); this may interfere with cooking for consoles. Unreal filenames should be no longer than %s characters. Filename value: %s"), BaseFilename.Len(), MaxFilenameLength, *BaseFilename));
			}
		}
	}
	return ReturnSuccessOrCancel();
}

FORCEINLINE void EnsurePackageLocalization(UPackage* InPackage)
{
#if USE_STABLE_LOCALIZATION_KEYS
	if (GIsEditor)
	{
		// We need to ensure that we have a package localization namespace as the package loading will need it
		// We need to do this before entering the GIsSavingPackage block as it may change the package meta-data
		TextNamespaceUtil::EnsurePackageNamespace(InPackage);
	}
#endif // USE_STABLE_LOCALIZATION_KEYS

	// Also make sure the localization cache is up to date, since updating it during the GIsSavingPackage won't allow object resolving
	FPackageLocalizationManager::Get().ConditionalUpdateCache();
}

void PreSavePackage(FSaveContext& SaveContext)
{
#if WITH_EDITOR
	// if the in memory package filename is different the filename we are saving it to,
	// regenerate a new persistent id for it.
	UPackage* Package = SaveContext.GetPackage();
	if (!SaveContext.IsCooking() && !SaveContext.IsFromAutoSave() && !Package->GetLoadedPath().IsEmpty() && Package->GetLoadedPath() != SaveContext.GetTargetPackagePath())
	{
		Package->SetPersistentGuid(FGuid::NewGuid());
	}
#endif //WITH_EDITOR
}

ESavePackageResult RoutePresave(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_RoutePresave);

	// Just route presave on all objects in the package while skipping unsaveable objects. 
	// This should be more efficient then trying to restrict to just the actual export, 
	// objects likely to not be export will probably not care about PreSave and should be mainly noop
	TArray<UObject*> ObjectsInPackage;
	GetObjectsWithPackage(SaveContext.GetPackage(), ObjectsInPackage);
	for (UObject* Object : ObjectsInPackage)
	{
		// Do not emit unsaveable warning while routing presave. 
		// this is to prevent warning on objects which won't be harvested later since they are unreferenced
		if (!SaveContext.IsUnsaveable(Object, false/*bEmitWarning*/))
		{
			if (SaveContext.IsCooking() && Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
			{
				FArchiveObjectCrc32NonEditorProperties CrcArchive;
				int32 Before = CrcArchive.Crc32(Object);
				UE::SavePackageUtilities::CallPreSave(Object, SaveContext.GetObjectSaveContext());
				int32 After = CrcArchive.Crc32(Object);

				if (Before != After)
				{
					UE_ASSET_LOG(
						LogSavePackage,
						Warning,
						Object,
						TEXT("Non-deterministic cook warning - PreSave() has modified %s '%s' - a resave may be required"),
						Object->HasAnyFlags(RF_ClassDefaultObject) ? TEXT("CDO") : TEXT("archetype"),
						*Object->GetName()
					);
				}
			}
			else
			{
				UE::SavePackageUtilities::CallPreSave(Object, SaveContext.GetObjectSaveContext());
			}
		}
	}

	return ReturnSuccessOrCancel();
}

ESavePackageResult HarvestPackage(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_HarvestPackage);

	FPackageHarvester Harvester(SaveContext);
	EObjectFlags TopLevelFlags = SaveContext.GetTopLevelFlags();
	UObject* Asset = SaveContext.GetAsset();

	auto TryHarvestRootObject = [&Harvester, &SaveContext](UObject* InRoot)
	{
		Harvester.TryHarvestExport(InRoot);
		// if we are automatically generating an optional package output, re-harvest objects with that realm as the default
		if (SaveContext.IsSaveAutoOptional())
		{
			//@todo: FH: the top level might have to be altered to use the editor one when auto harvesting optional data
			FSaveContext::FSetSaveRealmToSaveScope RealmScope(SaveContext, ESaveRealm::Optional);
			Harvester.TryHarvestExport(InRoot);
		}
	};

	// if no top level flags are passed, only use the provided package asset as root
	if (TopLevelFlags == RF_NoFlags)
	{
		TryHarvestRootObject(Asset);
	}
	// Otherwise use all objects which have the relevant flags
	else
	{
		ForEachObjectWithPackage(SaveContext.GetPackage(), [&TryHarvestRootObject, TopLevelFlags](UObject* InObject)
			{
				if (InObject->HasAnyFlags(TopLevelFlags))
				{
					TryHarvestRootObject(InObject);
				}
				return true;
			}, true/*bIncludeNestedObjects */, RF_Transient);
	}
	// Now process harvested roots
	while (FPackageHarvester::FExportWithContext ExportContext = Harvester.PopExportToProcess())
	{
		Harvester.ProcessExport(ExportContext);
	}

	// If we have a valid optional context and we are saving it,
	// transform any harvested non optional export into imports
	// Mark other optional import package as well
	if (!SaveContext.IsSaveAutoOptional() &&
		SaveContext.IsSaveOptional() &&
		SaveContext.IsCooking() &&
		SaveContext.GetHarvestedRealm(ESaveRealm::Optional).GetExports().Num() &&
		SaveContext.GetHarvestedRealm(ESaveRealm::Game).GetExports().Num())
	{
		bool bHasNonOptionalSelfReference = false;
		FHarvestedRealm& OptionalContext = SaveContext.GetHarvestedRealm(ESaveRealm::Optional);
		for (auto It = OptionalContext.GetExports().CreateIterator(); It; ++It)
		{
			if (!It->Obj->GetClass()->HasAnyClassFlags(CLASS_Optional))
			{
				// Make sure the export is found in the game context as well
				if (FTaggedExport* GameExport = SaveContext.GetHarvestedRealm(ESaveRealm::Game).GetExports().Find(It->Obj))
				{
					// Flag the export in the game context to generate it's public hash
					GameExport->bGeneratePublicHash = true;
					// Transform the export as an import
					OptionalContext.AddImport(It->Obj);
					// Flag the package itself to be an import
					bHasNonOptionalSelfReference = true;
				}
				// if not found in the game context and the reference directly came from an optional object, record an illegal reference
				else if (It->bFromOptionalReference)
				{
					SaveContext.RecordIllegalReference(nullptr, It->Obj, EIllegalRefReason::ReferenceFromOptionalToMissingGameExport);
				}
				It.RemoveCurrent();
			}
		}
		// Also add the current package itself as an import if we are referencing any non optional export
		if (bHasNonOptionalSelfReference)
		{
			OptionalContext.AddImport(SaveContext.GetPackage());
		}
	}

	// Trim PrestreamPackage list 
	TSet<UPackage*>& PrestreamPackages = SaveContext.GetPrestreamPackages();
	TSet<UPackage*> KeptPrestreamPackages;
	for (UPackage* Pkg : PrestreamPackages)
	{
		// If the prestream package hasn't been otherwise already marked as an import, keep it as such and mark it as an import
		if (!SaveContext.IsImport(Pkg))
		{
			KeptPrestreamPackages.Add(Pkg);
			SaveContext.AddImport(Pkg);
		}
	}
	Exchange(PrestreamPackages, KeptPrestreamPackages);

	// Harvest the PrestreamPackage class name if needed
	if (PrestreamPackages.Num() > 0)
	{
		Harvester.HarvestPackageHeaderName(SavePackageUtilities::NAME_PrestreamPackage);		
	}

	// if we have a WorldTileInfo, we need to harvest its dependencies as well, i.e. Custom Version
	if (FWorldTileInfo* WorldTileInfo = SaveContext.GetPackage()->GetWorldTileInfo())
	{
		Harvester << *WorldTileInfo;
	}

	// The Editor version is used as part of the check to see if a package is too old to use the gather cache, so we always have to add it if we have gathered loc for this asset
	// We need to set the editor custom version before we copy the version container to the summary, otherwise we may end up with corrupt assets
	// because we later do it on the Linker when actually gathering loc data
	if (!SaveContext.IsFilterEditorOnly())
	{
		Harvester.UsingCustomVersion(FEditorObjectVersion::GUID);
	}
	SaveContext.SetCustomVersions(Harvester.GetCustomVersions());

	return ReturnSuccessOrCancel();
}

static FNameEntryId NAME_UniqueObjectNameForCookingComparisonIndex = FName("UniqueObjectNameForCooking").GetComparisonIndex();

ESavePackageResult ValidateRealms(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_ValidateRealms);
	if (SaveContext.GetIllegalReferences().Num())
	{
		for (const FIllegalReference& Reference : SaveContext.GetIllegalReferences())
		{
			FString ErrorMessage;
			switch (Reference.Reason)
			{
			case EIllegalRefReason::ReferenceToOptional:
				ErrorMessage = FString::Printf(TEXT("Can't save %s: Non-optional object (%s) has a reference to optional object (%s). Only optional objects can refer to other optional objects."),
					SaveContext.GetFilename(),
					Reference.From ? *Reference.From->GetPathName() : TEXT("Unknown"),
					Reference.To ? *Reference.To->GetPathName() : TEXT("Unknown"));
				break;
			case EIllegalRefReason::ReferenceFromOptionalToMissingGameExport:
				ErrorMessage = FString::Printf(TEXT("Can't save %s: Optional object (%s) has a reference to cooked object (%s) which is missing. Non optional objects referenced by optional objects needs to be present in cooked data."),
					SaveContext.GetFilename(),
					Reference.From ? *Reference.From->GetPathName() : TEXT("Unknown"),
					Reference.To ? *Reference.To->GetPathName() : TEXT("Unknown"));
				break;
			case EIllegalRefReason::UnsaveableClass:
				ErrorMessage = FString::Printf(TEXT("Can't save %s: Object (%s) is an export but is an instance of class (%s) which is unsaveable: %s"),
					SaveContext.GetFilename(),
					Reference.From ? *Reference.From->GetPathName() : TEXT("Unknown"),
					Reference.To ? *Reference.To->GetPathName() : TEXT("Unknown"),
					*Reference.FormatStringArg);
				break;
			case EIllegalRefReason::UnsaveableOuter:
				ErrorMessage = FString::Printf(TEXT("Can't save %s: Object (%s) is an export but is a subobject of (%s) which is unsaveable: %s"),
					SaveContext.GetFilename(),
					Reference.From ? *Reference.From->GetPathName() : TEXT("Unknown"),
					Reference.To ? *Reference.To->GetPathName() : TEXT("Unknown"),
					*Reference.FormatStringArg);
				break;
			default:
				ErrorMessage = FString::Printf(TEXT("Can't save %s: Unknown Illegal reference from object (%s) to object (%s)"),
					SaveContext.GetFilename(),
					Reference.From ? *Reference.From->GetPathName() : TEXT("Unknown"),
					Reference.To ? *Reference.To->GetPathName() : TEXT("Unknown"));
			}

			if (SaveContext.IsGenerateSaveError())
			{
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorMessage);
			}
			else
			{
				UE_LOG(LogSavePackage, Error, TEXT("%s"), *ErrorMessage);
			}

		}
		return ESavePackageResult::Error;
	}
	return ReturnSuccessOrCancel();
}

ESavePackageResult ValidateExports(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_ValidateExports);

	/// Export Validation for optional realm
	if (SaveContext.GetCurrentHarvestingRealm() == ESaveRealm::Optional)
	{
		// return empty realm to skip processing if no export are found
		if (SaveContext.GetExports().Num() == 0)
		{
			return ESavePackageResult::EmptyRealm;
		}
		return ReturnSuccessOrCancel();
	}

	/// Export validation for game/editor realm
	// Check if we gathered any exports
	if (SaveContext.GetExports().Num() == 0)
	{
		UE_CLOG(SaveContext.IsGenerateSaveError(), LogSavePackage, Verbose, TEXT("No exports found (or all exports are editor-only) for %s. Package will not be saved."), SaveContext.GetFilename());
		return SaveContext.IsCooking() ? ESavePackageResult::ContainsEditorOnlyData : ESavePackageResult::Error;
	}

	// Validate that if an asset was provided it had the proper flags to be present in the exports.
	UObject* Asset = SaveContext.GetAsset();
	if (Asset)
	{
		if (!SaveContext.GetExports().Contains(FTaggedExport{ Asset }) &&
			SaveContext.GetTopLevelFlags() != RF_NoFlags && !Asset->HasAnyFlags(SaveContext.GetTopLevelFlags()))
		{
			FString ErrorMessage = FString::Printf(
				TEXT("The asset to save %s in package %s does not contain any of the provided object flags."),
				*Asset->GetName(), *SaveContext.GetPackage()->GetName());
			if (SaveContext.IsGenerateSaveError())
			{
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorMessage);
			}
			else
			{
				UE_LOG(LogSavePackage, Error, TEXT("%s"), *ErrorMessage);
			}
			return ESavePackageResult::Error;
		}
	}

#if WITH_EDITOR
	if (GOutputCookingWarnings)
	{
		// check the name list for UniqueObjectNameForCooking cooking
		if (SaveContext.NameExists(NAME_UniqueObjectNameForCookingComparisonIndex))
		{
			for (const FTaggedExport& Export : SaveContext.GetExports())
			{
				FName NameInUse = Export.Obj->GetFName();
				if (NameInUse.GetComparisonIndex() == NAME_UniqueObjectNameForCookingComparisonIndex)
				{
					UObject* Outer = Export.Obj->GetOuter();
					UE_LOG(LogSavePackage, Warning, TEXT("Saving object into cooked package %s which was created at cook time, Object Name %s, Full Path %s, Class %s, Outer %s, Outer class %s"), SaveContext.GetFilename(), *NameInUse.ToString(), *Export.Obj->GetFullName(), *Export.Obj->GetClass()->GetName(), Outer ? *Outer->GetName() : TEXT("None"), Outer ? *Outer->GetClass()->GetName() : TEXT("None"));
				}
			}
		}
	}
#endif

	// If this is a map package, make sure there is a world or level in the export map.
	if (SaveContext.GetPackage()->ContainsMap())
	{
		bool bContainsMap = false;
		for (const FTaggedExport& Export : SaveContext.GetExports())
		{
			UObject* Object = Export.Obj;
			// Consider redirectors to world/levels as map packages too.
			if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Object))
			{
				Object = Redirector->DestinationObject;
			}
			if (Object)
			{
				FName ClassName = Object->GetClass()->GetFName();
				bContainsMap |= (ClassName == SavePackageUtilities::NAME_World || ClassName == SavePackageUtilities::NAME_Level);
			}
		}
		if (!bContainsMap)
		{
			ensureMsgf(false, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *SaveContext.GetPackage()->GetName());
			UE_LOG(LogSavePackage, Error, TEXT("Attempting to save a map package '%s' that does not contain a map object."), *SaveContext.GetPackage()->GetName());

			if (SaveContext.IsGenerateSaveError())
			{
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format(NSLOCTEXT("Core", "SavePackageNoMap", "Attempting to save a map asset '{0}' that does not contain a map object"), FText::FromString(SaveContext.GetFilename())).ToString());
			}
			return ESavePackageResult::Error;
		}
	}

	// Cooking checks
	if (SaveContext.IsCooking())
	{
		// Add the exports for the cook checker
		if (FEDLCookCheckerThreadState* EDLCookChecker = SaveContext.GetEDLCookChecker())
		{
			// the package isn't actually in the export map, but that is ok, we add it as export anyway for error checking
			EDLCookChecker->AddExport(SaveContext.GetPackage());
			for (const FTaggedExport& Export : SaveContext.GetExports())
			{
				EDLCookChecker->AddExport(Export.Obj);
			}
		}
	}
	return ReturnSuccessOrCancel();
}

ESavePackageResult ValidateIllegalReferences(FSaveContext& SaveContext, TArray<UObject*>& PrivateObjects, TArray<UObject*>& ObjectsInOtherMaps)
{
	FFormatNamedArguments Args;

	// Illegal objects in other map warning
	if (ObjectsInOtherMaps.Num() > 0)
	{
		UObject* MostLikelyCulprit = nullptr;
		FString CulpritString = TEXT("Unknown");
		FString Referencer;
		SavePackageUtilities::FindMostLikelyCulprit(ObjectsInOtherMaps, MostLikelyCulprit, Referencer, &SaveContext);
		if (MostLikelyCulprit != nullptr)
		{
			CulpritString = FString::Printf(TEXT("%s (%s)"), *MostLikelyCulprit->GetFullName(), *Referencer);
		}

		FString ErrorMessage = FString::Printf(TEXT("Can't save %s: Graph is linked to object %s in external map"), SaveContext.GetFilename(), *CulpritString);
		if (SaveContext.IsGenerateSaveError())
		{
			SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *ErrorMessage);
		}
		else
		{
			UE_LOG(LogSavePackage, Error, TEXT("%s"), *ErrorMessage);
		}
		return ESavePackageResult::Error;
	}

	if (PrivateObjects.Num() > 0)
	{
		UObject* MostLikelyCulprit = nullptr;
		FString CulpritString = TEXT("Unknown");
		FString Referencer;
		SavePackageUtilities::FindMostLikelyCulprit(PrivateObjects, MostLikelyCulprit, Referencer, &SaveContext);
		CulpritString = FString::Printf(TEXT("%s (%s)"),
			(MostLikelyCulprit != nullptr) ? *MostLikelyCulprit->GetFullName() : TEXT("(unknown culprit)"),
			*Referencer);

		if (SaveContext.IsGenerateSaveError())
		{
			SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("Can't save %s: Graph is linked to external private object %s"), SaveContext.GetFilename(), *CulpritString);
		}
		return ESavePackageResult::Error;
	}
	return ReturnSuccessOrCancel();
}

ESavePackageResult ValidateImports(FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_ValidateImports);

	TArray<UObject*> TopLevelObjects;
	UPackage* Package = SaveContext.GetPackage();
	GetObjectsWithPackage(Package, TopLevelObjects, false);
	auto IsInAnyTopLevelObject = [&TopLevelObjects](UObject* InObject) -> bool
	{
		for (UObject* TopObject : TopLevelObjects)
		{
			if (InObject->IsInOuter(TopObject))
			{
				return true;
			}
		}
		return false;
	};
	auto AnyTopLevelObjectIsIn = [&TopLevelObjects](UObject* InObject) -> bool
	{
		for (UObject* TopObject : TopLevelObjects)
		{
			if (TopObject->IsInOuter(InObject))
			{
				return true;
			}
		}
		return false;
	};
	auto AnyTopLevelObjectHasSameOutermostObject = [&TopLevelObjects](UObject* InObject) -> bool
	{
		UObject* Outermost = InObject->GetOutermostObject();
		for (UObject* TopObject : TopLevelObjects)
		{
			if (TopObject->GetOutermostObject() == Outermost)
			{
				return true;
			}
		}
		return false;

	};
	auto IsSourcePackageReferenceAllowed = [](UPackage* InSourcePackage, UPackage* InImportPackage) -> bool
	{
#if WITH_EDITORONLY_DATA
		// Generated packages must have the same persistent GUID as their source package
		if (InSourcePackage->GetPersistentGuid() == InImportPackage->GetPersistentGuid())
		{
			// Generated packages can reference into their source package, or into other generated packages of the same source
			if (InSourcePackage->HasAnyPackageFlags(PKG_CookGenerated))
			{
				return true;
			}
		}
#endif
		return false;
	};

	auto IsMapReferenceAllowed = [&SaveContext](UObject* InImport) -> bool
	{
		if (!InImport->HasAnyFlags(RF_Public))
		{
			return false;
		}

		// If we have a public import from a map (i.e. the world) only redirector are allowed to have a hard reference
		for (const auto& Pair : SaveContext.GetObjectDependencies())
		{
			if (Pair.Key->GetClass() != UObjectRedirector::StaticClass())
			{
				if (Pair.Value.Contains(InImport))
				{
					return false;
				}
			}
		}
		return true;
	};

	FString PackageName = Package->GetName();

	// Warn for private objects & map object references
	TArray<UObject*> PrivateObjects;
	TArray<UObject*> ObjectsInOtherMaps;
	const TSet<UObject*>& Imports = SaveContext.GetImports();
	for (UObject* Import : Imports)
	{
		UPackage* ImportPackage = Import->GetPackage();
		// All names should be properly harvested at this point
		ensureAlwaysMsgf(SaveContext.NameExists(Import->GetFName().GetComparisonIndex())
			, TEXT("Missing import name %s while saving package %s. Did you rename an import during serialization?"), *Import->GetName(), *PackageName);
		ensureAlwaysMsgf(SaveContext.NameExists(ImportPackage->GetFName().GetComparisonIndex())
			, TEXT("Missing import package name %s while saving package %s. Did you rename an import during serialization?"), *ImportPackage->GetName(), *PackageName);
		ensureAlwaysMsgf(SaveContext.NameExists(Import->GetClass()->GetFName().GetComparisonIndex())
			, TEXT("Missing import class name %s while saving package %s"), *Import->GetClass()->GetName(), *PackageName);
		ensureAlwaysMsgf(SaveContext.NameExists(Import->GetClass()->GetOuter()->GetFName().GetComparisonIndex())
			, TEXT("Missing import class package name %s while saving package %s"), *Import->GetClass()->GetOuter()->GetName(), *PackageName);

		// if the import is marked as a prestream package, we dont need to validate further
		if (SaveContext.IsPrestreamPackage(ImportPackage))
		{
			ensureAlwaysMsgf(Import == ImportPackage, TEXT("Found an import refrence %s in a prestream package %s while saving package %s"), *Import->GetName(), *ImportPackage->GetName(), *PackageName);
			// These are not errors
			UE_LOG(LogSavePackage, Display, TEXT("Prestreaming package %s "), *ImportPackage->GetPathName()); //-V595
			continue;
		}

		// if an import outer is an export and that import doesn't have a specific package set then, there's an error
		const bool bWrongImport = Import->GetOuter() 
			&& Import->GetOuter()->IsInPackage(SaveContext.GetPackage()) 
			&& Import->GetExternalPackage() == nullptr
			// The optional context will have import that are actually in the same package, similar to external package
			&& SaveContext.GetCurrentHarvestingRealm() != ESaveRealm::Optional;
		if (bWrongImport)
		{
			if (!Import->HasAllFlags(RF_Transient) || !Import->IsNative())
			{
				UE_LOG(LogSavePackage, Warning, TEXT("Bad Object=%s"), *Import->GetFullName());
			}
			else
			{
				// if an object is marked RF_Transient and native, it is either an intrinsic class or
				// a property of an intrinsic class.  Only properties of intrinsic classes will have
				// an Outer that passes the check for "GetOuter()->IsInPackage(InOuter)" (thus ending up in this
				// block of code).  Just verify that the Outer for this property is also marked RF_Transient and Native
				check(Import->GetOuter()->HasAllFlags(RF_Transient) && Import->GetOuter()->IsNative());
			}
		}
		check(!bWrongImport || Import->HasAllFlags(RF_Transient) || Import->IsNative());

		// if this import shares a outer with top level object of this package then the reference is acceptable
		if ((!SaveContext.IsCooking() || SaveContext.GetCurrentHarvestingRealm() == ESaveRealm::Optional) &&
			(IsInAnyTopLevelObject(Import) || AnyTopLevelObjectIsIn(Import) || AnyTopLevelObjectHasSameOutermostObject(Import)))
		{
			continue;
		}

		// Allow private imports for split packages into their source package
		if (!IsSourcePackageReferenceAllowed(Package, ImportPackage))
		{
			// See whether the object we are referencing is in another map package and if it is allowed (i.e. from redirector).
			if (ImportPackage->ContainsMap() && !IsMapReferenceAllowed(Import))
			{
				ObjectsInOtherMaps.Add(Import);
			}

			if (!Import->HasAnyFlags(RF_Public) && (!SaveContext.IsCooking() || !ImportPackage->HasAnyPackageFlags(PKG_CompiledIn)))
			{
				PrivateObjects.Add(Import);
			}
		}

		// Enforce that Private content can only be referenced by something within the same Mount Point
		if (!ImportPackage->IsExternallyReferenceable())
		{
			FName MountPointName = FPackageName::GetPackageMountPoint(PackageName);

			FName ImportMountPointName = FPackageName::GetPackageMountPoint(ImportPackage->GetName());

			if (!MountPointName.IsNone() && !ImportMountPointName.IsNone())
			{
				if (MountPointName != ImportMountPointName)
				{
					PrivateObjects.Add(Import);
				}
			}
			else
			{
				PrivateObjects.Add(Import);
			}
		}
	}
	if (PrivateObjects.Num() > 0 || ObjectsInOtherMaps.Num() > 0)
	{
		return ValidateIllegalReferences(SaveContext, PrivateObjects, ObjectsInOtherMaps);
	}

	ISavePackageValidator* Validator = SaveContext.GetPackageValidator();
	if (Validator)
	{
		ESavePackageResult ValidatorResult = Validator->ValidateImports(Package, Imports);
		if (ValidatorResult != ESavePackageResult::Success)
		{
			return ValidatorResult;
		}
	}

	// Cooking checks
	// Currently do not use the edl checker for the optional context
	if (SaveContext.IsCooking() && SaveContext.GetCurrentHarvestingRealm() != ESaveRealm::Optional)
	{
		// Now that imports are validated add them to the cook checker if available
		if (FEDLCookCheckerThreadState* EDLCookChecker = SaveContext.GetEDLCookChecker())
		{
			for (UObject* Import : SaveContext.GetImports())
			{
				check(Import);
				EDLCookChecker->AddImport(Import, Package);
			}
		}
	}

	return ReturnSuccessOrCancel();
}

ESavePackageResult CreateLinker(FSaveContext& SaveContext)
{
	const FString BaseFilename = FPaths::GetBaseFilename(SaveContext.GetFilename());
	// Make temp file. CreateTempFilename guarantees unique, non-existing filename.
	// The temp file will be saved in the game save folder to not have to deal with potentially too long paths.
	// Since the temp filename may include a 32 character GUID as well, limit the user prefix to 32 characters.
	{
		SCOPED_SAVETIMER(UPackage_Save_CreateLinkerSave);

		IPackageWriter* PackageWriter = SaveContext.GetPackageWriter();
		if (PackageWriter || SaveContext.IsSaveToMemory())
		{
			// Allocate the linker with a memory writer, forcing byte swapping if wanted.
			TUniquePtr<FLargeMemoryWriter> ExportsArchive = PackageWriter ?
				PackageWriter->CreateLinkerArchive(SaveContext.GetPackage()->GetFName(), SaveContext.GetAsset()) :
				// The LargeMemoryWriter does not need to be persistent; the LinkerSave wraps it and reports Persistent=true
				TUniquePtr<FLargeMemoryWriter>(new FLargeMemoryWriter(0, false /* bIsPersistent */, *SaveContext.GetPackage()->GetName()));
			SaveContext.SetLinker(MakePimpl<FLinkerSave>(SaveContext.GetPackage(), ExportsArchive.Release(),
				SaveContext.IsForceByteSwapping(), SaveContext.IsSaveUnversionedNative()));
		}
		else
		{
			// Allocate the linker with a tempfile, forcing byte swapping if wanted.
			SaveContext.SetTempFilename(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32)));
			SaveContext.SetLinker(MakePimpl<FLinkerSave>(SaveContext.GetPackage(), *SaveContext.GetTempFilename().GetValue(), SaveContext.IsForceByteSwapping(), SaveContext.IsSaveUnversionedNative()));
			if (!SaveContext.GetLinker()->Saver)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Name"), FText::FromString(*SaveContext.GetTempFilename()));
				FText ErrorText = FText::Format(NSLOCTEXT("SavePackage", "CouldNotCreateSaveFile", "Could not create temporary save filename {Name}."), Arguments);
				UE_LOG(LogSavePackage, Error, TEXT("%s"), *ErrorText.ToString());
				if (SaveContext.IsGenerateSaveError())
				{
					SaveContext.GetError()->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorText.ToString());
				}
				return ESavePackageResult::Error;
			}
		}

		if (SaveContext.IsGenerateSaveError())
		{
			SaveContext.GetLinker()->SetOutputDevice(SaveContext.GetError());
		}
		SaveContext.GetLinker()->bUpdatingLoadedPath = SaveContext.IsUpdatingLoadedPath();
		SaveContext.GetLinker()->bProceduralSave = SaveContext.IsProceduralSave();

		SaveContext.GetLinker()->bRehydratePayloads = SaveContext.ShouldRehydratePayloads();
		
		if (UE::FPackageTrailer::IsEnabled())
		{
			// The package trailer is not supported for text based assets yet
			if (!SaveContext.IsTextFormat() && !SaveContext.IsProceduralSave())
			{		
				SaveContext.GetLinker()->PackageTrailerBuilder = MakeUnique<UE::FPackageTrailerBuilder>(SaveContext.GetPackage()->GetName());
			}
			else if ((SaveContext.GetSaveArgs().SaveFlags & SAVE_BulkDataByReference) != 0)
			{
				if (const FLinkerLoad* LinkerLoad = FLinkerLoad::FindExistingLinkerForPackage(SaveContext.GetPackage()))
				{
					if (const UE::FPackageTrailer* Trailer = LinkerLoad->GetPackageTrailer())
					{
						SaveContext.GetLinker()->PackageTrailerBuilder = UE::FPackageTrailerBuilder::CreateReferenceToTrailer(*Trailer, SaveContext.GetPackage()->GetName());
					}
				}
			}
		}

#if WITH_TEXT_ARCHIVE_SUPPORT
		if (SaveContext.IsTextFormat())
		{
			if (SaveContext.GetTempFilename().IsSet())
			{
				SaveContext.SetTextFormatTempFilename(SaveContext.GetTempFilename().GetValue() + FPackageName::GetTextAssetPackageExtension());
			}
			else
			{
				SaveContext.SetTextFormatTempFilename(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), *BaseFilename.Left(32)) + FPackageName::GetTextAssetPackageExtension());
			}
			SaveContext.SetTextFormatArchive(TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*SaveContext.GetTextFormatTempFilename().GetValue())));
			TUniquePtr<FJsonArchiveOutputFormatter> OutputFormatter = MakeUnique<FJsonArchiveOutputFormatter>(*SaveContext.GetTextFormatArchive());
			OutputFormatter->SetObjectIndicesMap(&SaveContext.GetLinker()->ObjectIndicesMap);
			SaveContext.SetFormatter(MoveTemp(OutputFormatter));
		}
		else
#endif
		{
			SaveContext.SetFormatter(MakeUnique<FBinaryArchiveFormatter>(*(FArchive*)SaveContext.GetLinker()));
		}
	}

	SaveContext.SetStructuredArchive(MakeUnique<FStructuredArchive>(*SaveContext.GetFormatter()));
	return ReturnSuccessOrCancel();
}

struct FNameEntryIdSortHelper
{
private:
	/** the linker that we're sorting names for */
	friend struct TDereferenceWrapper<FNameEntryId, FNameEntryIdSortHelper>;

	/** Comparison function used by Sort */
	FORCEINLINE bool operator()(const FName& A, const FName& B) const
	{
		return A.Compare(B) < 0;
	}

	/** Comparison function used by Sort */
	FORCEINLINE bool operator()(FNameEntryId A, FNameEntryId B) const
	{
		//@todo Could be implemented without constructing FName but need a would new FNameEntry comparison API
		return A != B && operator()(FName::CreateFromDisplayId(A, 0), FName::CreateFromDisplayId(B, 0));
	}
};

struct FObjectResourceSortHelper
{
private:
	friend struct TDereferenceWrapper<FObjectImport, FObjectResourceSortHelper>;
	friend struct TDereferenceWrapper<FObjectExport, FObjectResourceSortHelper>;

	/** Comparison function used by Sort */
	FORCEINLINE bool operator()(const FObjectResource& A, const FObjectResource& B) const
	{
		return A.ObjectName.Compare(B.ObjectName) < 0;
	}
};

ESavePackageResult BuildLinker(FSaveContext& SaveContext)
{
	// Setup Linker 
	{
		// Use the custom versions we harvested from the dependency harvesting pass
		SaveContext.GetLinker()->Summary.SetCustomVersionContainer(SaveContext.GetCustomVersions());

		SaveContext.GetLinker()->SetPortFlags(SaveContext.GetPortFlags());
		if (SaveContext.IsSaveAutoOptional() && SaveContext.GetCurrentHarvestingRealm() == ESaveRealm::Optional)
		{
			// Do not filter editor only data when automatically creating optional data using full uncooked objects
			SaveContext.GetLinker()->SetFilterEditorOnly(false);
		}
		else
		{
			SaveContext.GetLinker()->SetFilterEditorOnly(SaveContext.IsFilterEditorOnly());
		}
		SaveContext.GetLinker()->SetCookData(SaveContext.GetCookData());

		bool bUseUnversionedProperties = SaveContext.IsSaveUnversionedProperties();
		SaveContext.GetLinker()->SetUseUnversionedPropertySerialization(bUseUnversionedProperties);

#if WITH_EDITOR
		if (SaveContext.IsCooking())
		{
			SaveContext.GetLinker()->SetDebugSerializationFlags(DSF_EnableCookerWarnings | SaveContext.GetLinker()->GetDebugSerializationFlags());
		}
#endif
		// Make sure the package has the same version as the linker
		SaveContext.UpdatePackageLinkerVersions();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	if (SaveContext.GetSaveArgs().OutputPackageGuid)
	{
		SaveContext.GetLinker()->Summary.Guid = *SaveContext.GetSaveArgs().OutputPackageGuid;
		
	}
	else
	{
		SaveContext.GetLinker()->Summary.Guid = SaveContext.IsKeepGuid() ?
			SaveContext.GetPackage()->GetGuid() : SaveContext.GetPackage()->MakeNewGuid();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
#if WITH_EDITORONLY_DATA
	SaveContext.GetLinker()->Summary.PersistentGuid = SaveContext.GetPackage()->GetPersistentGuid();
#endif
	SaveContext.GetLinker()->Summary.Generations = TArray<FGenerationInfo>{ FGenerationInfo(0, 0) };

	FLinkerSave* Linker = SaveContext.GetLinker();

	// Build Name Map
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildNameMap);
		const TSet<FNameEntryId>& NamesReferencedFromExportData = SaveContext.GetNamesReferencedFromExportData();
		const TSet<FNameEntryId>& NamesReferencedFromPackageHeader = SaveContext.GetNamesReferencedFromPackageHeader();
		Linker->NameMap.Reserve(NamesReferencedFromExportData.Num() + NamesReferencedFromPackageHeader.Num());
		for (FNameEntryId NameEntryId : NamesReferencedFromExportData)
		{
			Linker->NameMap.Add(NameEntryId);
		}
		for (FNameEntryId NameEntryId : NamesReferencedFromPackageHeader)
		{
			if (!NamesReferencedFromExportData.Contains(NameEntryId))
			{
				Linker->NameMap.Add(NameEntryId);
			}
		}
		Linker->Summary.NameOffset = 0;
		Linker->Summary.NameCount = Linker->NameMap.Num();
		Linker->Summary.NamesReferencedFromExportDataCount = NamesReferencedFromExportData.Num();

		Sort(Linker->NameMap.GetData(), Linker->Summary.NamesReferencedFromExportDataCount, FNameEntryIdSortHelper());
		Sort(Linker->NameMap.GetData() + Linker->Summary.NamesReferencedFromExportDataCount,
			Linker->Summary.NameCount - Linker->Summary.NamesReferencedFromExportDataCount,
			FNameEntryIdSortHelper());

		if (!SaveContext.IsTextFormat())
		{
			for (int32 Index = 0; Index < Linker->NameMap.Num(); ++Index)
			{
				Linker->NameIndices.Add(Linker->NameMap[Index], Index);
			}
		}
	}

	// Build SoftObjectPathList
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildSoftObjectPathList);
		Linker->Summary.SoftObjectPathsOffset = 0;
		Linker->Summary.SoftObjectPathsCount = 0;

		// Do not serialize a soft object path list when cooking.
		// iostore for example does not keep that list as part of its header information
		if (!SaveContext.IsCooking())
		{
			Linker->SoftObjectPathList = SaveContext.GetSoftObjectPathList().Array();
		}

		if (!SaveContext.IsTextFormat())
		{
			for (int32 Index = 0; Index < Linker->SoftObjectPathList.Num(); ++Index)
			{
				Linker->SoftObjectPathIndices.Add(Linker->SoftObjectPathList[Index], Index);
			}
		}
	}

	// Build GatherableText
	{
		Linker->Summary.GatherableTextDataOffset = 0;
		Linker->Summary.GatherableTextDataCount = 0;
		if (!SaveContext.IsFilterEditorOnly())
		{
			SCOPED_SAVETIMER(UPackage_Save_BuildGatherableTextData);

			// Gathers from the given package
			SaveContext.GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;
			FPropertyLocalizationDataGatherer(Linker->GatherableTextDataMap, SaveContext.GetPackage(), SaveContext.GatherableTextResultFlags);
		}
	}

	// Build ImportMap
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildImportMap);

		for (UObject* Import : SaveContext.GetImports())
		{
			UClass* ImportClass = Import->GetClass();
			FName ReplacedName = NAME_None;
			FObjectImport& ObjectImport = Linker->ImportMap.Add_GetRef(FObjectImport(Import, ImportClass));

			// flag the import as optional
			if (Import->GetClass()->HasAnyClassFlags(CLASS_Optional))
			{
				ObjectImport.bImportOptional = true;
			}

			// If the package import is a prestream package, mark it as such by hacking its class name
			if (SaveContext.IsPrestreamPackage(Cast<UPackage>(Import)))
			{
				ObjectImport.ClassName = SavePackageUtilities::NAME_PrestreamPackage;
			}

			if (ReplacedName != NAME_None)
			{
				ObjectImport.ObjectName = ReplacedName;
			}
		}
		//Sort(&Linker->ImportMap[0], Linker->ImportMap.Num(), FObjectResourceSortHelper());

		// @todo: To stay consistent with the old save and prevent binary diff between the algo, use the old import sort for now
		// a future cvar could allow projects use the less expensive sort in their own time down the line
		FObjectImportSortHelper ImportSortHelper;
		{
			SCOPED_SAVETIMER(UPackage_Save_SortImports);
			ImportSortHelper.SortImports(Linker);
		}
		Linker->Summary.ImportCount = Linker->ImportMap.Num();
	}

	// Build ExportMap & Package Netplay data
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildExportMap);
		for (const FTaggedExport& TaggedExport : SaveContext.GetExports())
		{
			FObjectExport& Export = Linker->ExportMap.Add_GetRef(FObjectExport(TaggedExport.Obj, TaggedExport.bNotAlwaysLoadedForEditorGame));
			Export.bGeneratePublicHash = TaggedExport.bGeneratePublicHash;

			if (UPackage* Package = Cast<UPackage>(Export.Object))
			{
				Export.PackageFlags = Package->GetPackageFlags();
			}
		}
		//Sort(&Linker->ExportMap[0], Linker->ExportMap.Num(), FObjectResourceSortHelper());

		// @todo: To stay consistent with the old save and prevent binary diff between the algo, use the old import sort for now
		// a future cvar could allow projects use the less expensive sort in their own time down the line
		// Also, currently the export sort order matters in an incidental manner where it should be properly tracked with dependencies instead.
		// for example where FAnimInstanceProxy PostLoad actually depends on UAnimBlueprintGeneratedClass PostLoad to be properly initialized.
		FObjectExportSortHelper ExportSortHelper;
		{
			SCOPED_SAVETIMER(UPackage_Save_SortExports);
			ExportSortHelper.SortExports(Linker);
		}
		Linker->Summary.ExportCount = Linker->ExportMap.Num();
	}

	// Build Linker Reverse Mapping
	{
		for (int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex)
		{
			UObject* Object = Linker->ExportMap[ExportIndex].Object;
			check(Object);
			Linker->ObjectIndicesMap.Add(Object, FPackageIndex::FromExport(ExportIndex));
		}
		for (int32 ImportIndex = 0; ImportIndex < Linker->ImportMap.Num(); ++ImportIndex)
		{
			UObject* Object = Linker->ImportMap[ImportIndex].XObject;
			check(Object);
			Linker->ObjectIndicesMap.Add(Object, FPackageIndex::FromImport(ImportIndex));
		}
	}

	// Build DependsMap
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildExportDependsMap);

		Linker->DependsMap.AddZeroed(Linker->ExportMap.Num());
		for (int32 ExpIndex = 0; ExpIndex < Linker->ExportMap.Num(); ++ExpIndex)
		{
			UObject* Object = Linker->ExportMap[ExpIndex].Object;
			TArray<FPackageIndex>& DependIndices = Linker->DependsMap[ExpIndex];
			const TSet<UObject*>* SrcDepends = SaveContext.GetObjectDependencies().Find(Object);
			checkf(SrcDepends, TEXT("Couldn't find dependency map for %s"), *Object->GetFullName());
			DependIndices.Reserve(SrcDepends->Num());

			for (UObject* DependentObject : *SrcDepends)
			{
				FPackageIndex DependencyIndex = Linker->ObjectIndicesMap.FindRef(DependentObject);

				// if we didn't find it (FindRef returns 0 on failure, which is good in this case), then we are in trouble, something went wrong somewhere
				checkf(!DependencyIndex.IsNull(), TEXT("Failed to find dependency index for %s (%s)"), *DependentObject->GetFullName(), *Object->GetFullName());

				// add the import as an import for this export
				DependIndices.Add(DependencyIndex);
			}
		}
	}

	// Build SoftPackageReference & Searchable Name Map
	{
		Linker->SoftPackageReferenceList = SaveContext.GetSoftPackageReferenceList().Array();

		// Convert the searchable names map from UObject to packageindex
		for (TPair<UObject*, TArray<FName>>& SearchableNamePair : SaveContext.GetSearchableNamesObjectMap())
		{
			const FPackageIndex PackageIndex = Linker->MapObject(SearchableNamePair.Key);
			// This should always be in the imports already
			if (ensure(!PackageIndex.IsNull()))
			{
				Linker->SearchableNamesMap.FindOrAdd(PackageIndex) = MoveTemp(SearchableNamePair.Value);
			}
		}
		SaveContext.GetSearchableNamesObjectMap().Empty();
	}

	// Map Export Indices
	{
		SCOPED_SAVETIMER(UPackage_Save_MapExportIndices);
		for (FObjectExport& Export : Linker->ExportMap)
		{
			// Set class index.
			// If this is *exactly* a UClass, store null instead; for anything else, including UClass-derived classes, map it
			UClass* ObjClass = Export.Object->GetClass();
			if (ObjClass != UClass::StaticClass())
			{
				Export.ClassIndex = Linker->MapObject(ObjClass);
				// The class should be mappable because it was checked in FPackageHarvester::ProcessExport and the save early-exited if not
				checkf(!Export.ClassIndex.IsNull(), TEXT("Export %s class is not mapped when saving %s"), *Export.Object->GetFullName(), *Linker->LinkerRoot->GetName());
			}
			else
			{
				Export.ClassIndex = FPackageIndex();
			}

			if (SaveContext.IsCooking())
			{
				UObject* Archetype = Export.Object->GetArchetype();
				check(Archetype);
				check(Archetype->IsA(Export.Object->HasAnyFlags(RF_ClassDefaultObject) ? ObjClass->GetSuperClass() : ObjClass));
				Export.TemplateIndex = Linker->MapObject(Archetype);
				UE_CLOG(Export.TemplateIndex.IsNull(), LogSavePackage, Fatal, TEXT("%s was an archetype of %s but returned a null index mapping the object."), *Archetype->GetFullName(), *Export.Object->GetFullName());
				check(!Export.TemplateIndex.IsNull());
			}

			// Set the parent index, if this export represents a UStruct-derived object
			UStruct* Struct = Cast<UStruct>(Export.Object);
			if (Struct && Struct->GetSuperStruct() != nullptr)
			{
				Export.SuperIndex = Linker->MapObject(Struct->GetSuperStruct());
				checkf(!Export.SuperIndex.IsNull(),
					TEXT("Export Struct (%s) of type (%s) inheriting from (%s) of type (%s) has not mapped super struct."),
					*GetPathNameSafe(Struct),
					*(Struct->GetClass()->GetName()),
					*GetPathNameSafe(Struct->GetSuperStruct()),
					*(Struct->GetSuperStruct()->GetClass()->GetName())
				);
			}
			else
			{
				Export.SuperIndex = FPackageIndex();
			}

			// Set FPackageIndex for this export's Outer. If the export's Outer
			// is the UPackage corresponding to this package's LinkerRoot, leave it null
			Export.OuterIndex = Export.Object->GetOuter() != SaveContext.GetPackage() ? Linker->MapObject(Export.Object->GetOuter()) : FPackageIndex();

			// Only packages or object having the currently saved package as outer are allowed to have no outer
			ensureMsgf(Export.OuterIndex != FPackageIndex() || Export.Object->IsA(UPackage::StaticClass()) || Export.Object->GetOuter() == SaveContext.GetPackage(), TEXT("Export %s has no valid outer!"), *Export.Object->GetPathName());
		}

		for (FObjectImport& Import : Linker->ImportMap)
		{
			if (Import.XObject)
			{
				// Set the package index.
				if (Import.XObject->GetOuter())
				{
					Import.OuterIndex = Linker->MapObject(Import.XObject->GetOuter());

					// if the import has a package set, set it up
					if (UPackage* ImportPackage = Import.XObject->GetExternalPackage())
					{
						Import.SetPackageName(ImportPackage->GetFName());
					}

					if (SaveContext.IsCooking())
					{
						// Only package imports are allowed to have no outer
						ensureMsgf(Import.OuterIndex != FPackageIndex() || Import.ClassName == NAME_Package, TEXT("Import %s has no valid outer when cooking!"), *Import.XObject->GetPathName());
					}
				}
			}
			else
			{
				checkf(false, TEXT("NULL XObject for import - Object: %s Class: %s"), *Import.ObjectName.ToString(), *Import.ClassName.ToString());
			}
		}
	}
	return ReturnSuccessOrCancel();
}

void SavePreloadDependencies(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	FLinkerSave* Linker = SaveContext.GetLinker();
	auto IncludeObjectAsDependency = [Linker, &SaveContext](int32 CallSite, TSet<FPackageIndex>& AddTo, UObject* ToTest, UObject* ForObj, bool bMandatory, bool bOnlyIfInLinkerTable)
	{
		// Skip transient, editor only, and excluded client/server objects
		if (ToTest)
		{
			UPackage* Outermost = ToTest->GetOutermost();
			check(Outermost);
			if (Outermost->GetFName() == GLongCoreUObjectPackageName)
			{
				return; // We assume nothing in coreuobject ever loads assets in a constructor
			}
			FPackageIndex Index = Linker->MapObject(ToTest);
			if (Index.IsNull() && bOnlyIfInLinkerTable)
			{
				return;
			}
			if (!Index.IsNull() && (ToTest->HasAllFlags(RF_Transient) && !ToTest->IsNative()))
			{
				UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' is in the linker table, but is transient. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
			}
			if (!Index.IsNull() && !IsValid(ToTest))
			{
				UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' is in the linker table, but is pending kill or garbage. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
			}
			bool bNotFiltered = !SaveContext.IsExcluded(ToTest);
			if (bMandatory && !bNotFiltered)
			{
				UE_LOG(LogSavePackage, Warning, TEXT("A dependency '%s' of '%s' was filtered, but is mandatory. This indicates a problem with editor only stripping. We will keep the dependency anyway (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
				bNotFiltered = true;
			}
			if (bNotFiltered)
			{
				if (!Index.IsNull())
				{
					AddTo.Add(Index);
					return;
				}
				else if (!SaveContext.IsUnsaveable(ToTest))
				{
					UE_CLOG(Outermost->HasAnyPackageFlags(PKG_CompiledIn), LogSavePackage, Verbose, TEXT("A compiled in dependency '%s' of '%s' was not actually in the linker tables and so will be ignored (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
					UE_CLOG(!Outermost->HasAnyPackageFlags(PKG_CompiledIn), LogSavePackage, Fatal, TEXT("A dependency '%s' of '%s' was not actually in the linker tables and so will be ignored (%d)."), *ToTest->GetFullName(), *ForObj->GetFullName(), CallSite);
				}
			}
			check(!bMandatory);
		}
	};

	auto IncludeIndexAsDependency = [Linker](TSet<FPackageIndex>& AddTo, FPackageIndex Dep)
	{
		if (!Dep.IsNull())
		{
			UObject* ToTest = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
			if (ToTest)
			{
				UPackage* Outermost = ToTest->GetOutermost();
				if (Outermost && Outermost->GetFName() != GLongCoreUObjectPackageName) // We assume nothing in coreuobject ever loads assets in a constructor
				{
					AddTo.Add(Dep);
				}
			}
		}
	};

	Linker->Summary.PreloadDependencyOffset = (int32)Linker->Tell();
	Linker->Summary.PreloadDependencyCount = -1;

	if (SaveContext.IsCooking())
	{
		Linker->Summary.PreloadDependencyCount = 0;

		FStructuredArchive::FStream DepedenciesStream = StructuredArchiveRoot.EnterStream(TEXT("PreloadDependencies"));

		TArray<UObject*> Subobjects;
		TArray<UObject*> Deps;
		TSet<FPackageIndex> SerializationBeforeCreateDependencies;
		TSet<FPackageIndex> SerializationBeforeSerializationDependencies;
		TSet<FPackageIndex> CreateBeforeSerializationDependencies;
		TSet<FPackageIndex> CreateBeforeCreateDependencies;

		for (int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex)
		{
			FObjectExport& Export = Linker->ExportMap[ExportIndex];
			check(Export.Object);
			{
				SerializationBeforeCreateDependencies.Reset();
				IncludeIndexAsDependency(SerializationBeforeCreateDependencies, Export.ClassIndex);
				UObject* CDO = Export.Object->GetArchetype();
				IncludeObjectAsDependency(1, SerializationBeforeCreateDependencies, CDO, Export.Object, true, false);
				Subobjects.Reset();
				GetObjectsWithOuter(CDO, Subobjects);
				for (UObject* SubObj : Subobjects)
				{
					// Only include subobject archetypes
					if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
					{
						while (SubObj->HasAnyFlags(RF_Transient)) // transient components are stripped by the ICH, so find the one it will really use at runtime
						{
							UObject* SubObjArch = SubObj->GetArchetype();
							if (SubObjArch->GetClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
							{
								break;
							}
							SubObj = SubObjArch;
						}
						if (IsValid(SubObj))
						{
							IncludeObjectAsDependency(2, SerializationBeforeCreateDependencies, SubObj, Export.Object, false, false);
						}
					}
				}
			}
			{
				SerializationBeforeSerializationDependencies.Reset();
				Deps.Reset();
				Export.Object->GetPreloadDependencies(Deps);

				for (UObject* Obj : Deps)
				{
					IncludeObjectAsDependency(3, SerializationBeforeSerializationDependencies, Obj, Export.Object, false, true);
				}
				if (Export.Object->HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
				{
					UObject* Outer = Export.Object->GetOuter();
					if (!Outer->IsA(UPackage::StaticClass()))
					{
						IncludeObjectAsDependency(4, SerializationBeforeSerializationDependencies, Outer, Export.Object, true, false);
					}
				}
				if (Export.Object->IsA(UClass::StaticClass()))
				{
					// we need to load archetypes of our subobjects before we load the class
					UObject* CDO = CastChecked<UClass>(Export.Object)->GetDefaultObject();
					Subobjects.Reset();
					GetObjectsWithOuter(CDO, Subobjects);
					for (UObject* SubObj : Subobjects)
					{
						// Only include subobject archetypes
						if (SubObj->HasAnyFlags(RF_DefaultSubObject | RF_ArchetypeObject))
						{
							// Don't include the archetype of SubObjects that we excluded in the harvesting phase; we didn't add their archetypes
							if (SaveContext.IsExcluded(SubObj))
							{
								continue;
							}

							SubObj = SubObj->GetArchetype();
							while (SubObj->HasAnyFlags(RF_Transient)) // transient components are stripped by the ICH, so find the one it will really use at runtime
							{
								UObject* SubObjArch = SubObj->GetArchetype();
								if (SubObjArch->GetClass()->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
								{
									break;
								}
								SubObj = SubObjArch;
							}
							if (IsValid(SubObj))
							{
								IncludeObjectAsDependency(5, SerializationBeforeSerializationDependencies, SubObj, Export.Object, false, false);
							}
						}
					}
				}
			}
			{
				CreateBeforeSerializationDependencies.Reset();
				UClass* Class = Cast<UClass>(Export.Object);
				UObject* ClassCDO = Class ? Class->GetDefaultObject() : nullptr;
				{
					TArray<FPackageIndex>& Depends = Linker->DependsMap[ExportIndex];
					for (FPackageIndex Dep : Depends)
					{
						UObject* ToTest = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
						if (ToTest != ClassCDO)
						{
							IncludeIndexAsDependency(CreateBeforeSerializationDependencies, Dep);
						}
					}
				}
				{
					const TSet<UObject*>& NativeDeps = SaveContext.GetNativeObjectDependencies()[Export.Object];
					for (UObject* ToTest : NativeDeps)
					{
						if (ToTest != ClassCDO)
						{
							IncludeObjectAsDependency(6, CreateBeforeSerializationDependencies, ToTest, Export.Object, false, true);
						}
					}
				}
			}
			{
				CreateBeforeCreateDependencies.Reset();
				IncludeIndexAsDependency(CreateBeforeCreateDependencies, Export.OuterIndex);
				IncludeIndexAsDependency(CreateBeforeCreateDependencies, Export.SuperIndex);
			}
			// Currently do not validate the optional context with the edl checker
			FEDLCookCheckerThreadState* EDLCookChecker = SaveContext.GetCurrentHarvestingRealm() != ESaveRealm::Optional ? SaveContext.GetEDLCookChecker() : nullptr;
			auto AddArcForDepChecking = [&Linker, &Export, EDLCookChecker](bool bExportIsSerialize, FPackageIndex Dep, bool bDepIsSerialize)
			{
				check(Export.Object);
				check(!Dep.IsNull());
				UObject* DepObject = Dep.IsExport() ? Linker->Exp(Dep).Object : Linker->Imp(Dep).XObject;
				check(DepObject);

				Linker->DepListForErrorChecking.Add(Dep);
				if (EDLCookChecker)
				{
					EDLCookChecker->AddArc(DepObject, bDepIsSerialize, Export.Object, bExportIsSerialize);
				}
			};

			for (FPackageIndex Index : SerializationBeforeSerializationDependencies)
			{
				if (SerializationBeforeCreateDependencies.Contains(Index))
				{
					continue; // if the other thing must be serialized before we create, then this is a redundant dep
				}
				if (Export.FirstExportDependency == -1)
				{
					Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
					check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
				}
				Linker->Summary.PreloadDependencyCount++;
				Export.SerializationBeforeSerializationDependencies++;
				DepedenciesStream.EnterElement() << Index;
				AddArcForDepChecking(true, Index, true);
			}
			for (FPackageIndex Index : CreateBeforeSerializationDependencies)
			{
				if (SerializationBeforeCreateDependencies.Contains(Index))
				{
					continue; // if the other thing must be serialized before we create, then this is a redundant dep
				}
				if (SerializationBeforeSerializationDependencies.Contains(Index))
				{
					continue; // if the other thing must be serialized before we serialize, then this is a redundant dep
				}
				if (CreateBeforeCreateDependencies.Contains(Index))
				{
					continue; // if the other thing must be created before we are created, then this is a redundant dep
				}
				if (Export.FirstExportDependency == -1)
				{
					Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
					check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
				}
				Linker->Summary.PreloadDependencyCount++;
				Export.CreateBeforeSerializationDependencies++;
				DepedenciesStream.EnterElement() << Index;
				AddArcForDepChecking(true, Index, false);
			}
			for (FPackageIndex Index : SerializationBeforeCreateDependencies)
			{
				if (Export.FirstExportDependency == -1)
				{
					Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
					check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
				}
				Linker->Summary.PreloadDependencyCount++;
				Export.SerializationBeforeCreateDependencies++;
				DepedenciesStream.EnterElement() << Index;
				AddArcForDepChecking(false, Index, true);
			}
			for (FPackageIndex Index : CreateBeforeCreateDependencies)
			{
				if (Export.FirstExportDependency == -1)
				{
					Export.FirstExportDependency = Linker->Summary.PreloadDependencyCount;
					check(Export.SerializationBeforeSerializationDependencies == 0 && Export.CreateBeforeSerializationDependencies == 0 && Export.SerializationBeforeCreateDependencies == 0 && Export.CreateBeforeCreateDependencies == 0);
				}
				Linker->Summary.PreloadDependencyCount++;
				Export.CreateBeforeCreateDependencies++;
				DepedenciesStream.EnterElement() << Index;
				AddArcForDepChecking(false, Index, false);
			}
		}
		UE_LOG(LogSavePackage, Verbose, TEXT("Saved %d dependencies for %d exports."), Linker->Summary.PreloadDependencyCount, Linker->ExportMap.Num());
	}
}

void WriteGatherableText(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	FStructuredArchive::FStream Stream = StructuredArchiveRoot.EnterStream(TEXT("GatherableTextData"));
	// Do not gather text data during cooking since the data isn't only scrubbed off of editor packages
	if (!SaveContext.IsCooking()
		&& !SaveContext.IsFilterEditorOnly()
		// We can only cache packages that:
		//	1) Don't contain script data, as script data is very volatile and can only be safely gathered after it's been compiled (which happens automatically on asset load).
		//	2) Don't contain text keyed with an incorrect package localization ID, as these keys will be changed later during save.
		&& !EnumHasAnyFlags(SaveContext.GatherableTextResultFlags, EPropertyLocalizationGathererResultFlags::HasScript | EPropertyLocalizationGathererResultFlags::HasTextWithInvalidPackageLocalizationID))
	{
		FLinkerSave* Linker = SaveContext.GetLinker();

		// The Editor version is used as part of the check to see if a package is too old to use the gather cache, so we always have to add it if we have gathered loc for this asset
		// Note that using custom version here only works because we already added it to the export tagger before the package summary was serialized
		Linker->UsingCustomVersion(FEditorObjectVersion::GUID);

		Linker->Summary.GatherableTextDataOffset = (int32)Linker->Tell();
		Linker->Summary.GatherableTextDataCount = Linker->GatherableTextDataMap.Num();
		for (FGatherableTextData& GatherableTextData : Linker->GatherableTextDataMap)
		{
			Stream.EnterElement() << GatherableTextData;
		}
	}
}

ESavePackageResult WritePackageHeader(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	FLinkerSave* Linker = SaveContext.GetLinker();
#if WITH_EDITOR
	FArchiveStackTraceIgnoreScope IgnoreDiffScope(SaveContext.IsIgnoringHeaderDiff());
#endif
	TGuardValue<bool> Guard(Linker->bIsWritingHeader, true);

	// Write Dummy Summary
	{
		StructuredArchiveRoot.GetUnderlyingArchive() << Linker->Summary;
	}
	SaveContext.OffsetAfterPackageFileSummary = (int32)Linker->Tell();

	// Write Name Map
	Linker->Summary.NameOffset = SaveContext.OffsetAfterPackageFileSummary;
	{
		SCOPED_SAVETIMER(UPackage_Save_BuildNameMap);
		checkf(Linker->Summary.NameCount == Linker->NameMap.Num(), TEXT("Summary NameCount didn't match linker name map count when saving package header for '%s'"), *Linker->LinkerRoot->GetName());
		for (const FNameEntryId NameEntryId : Linker->NameMap)
		{
			FName::GetEntry(NameEntryId)->Write(*Linker);
		}
	}

	// Write Soft Object Paths
	{
		SCOPED_SAVETIMER(UPackage_Save_SaveSoftObjectPaths);
		// Save soft object paths references
		Linker->Summary.SoftObjectPathsOffset = (int32)Linker->Tell();
		Linker->Summary.SoftObjectPathsCount = Linker->SoftObjectPathList.Num();
		// Do not map soft object path during the table serialization itself
		FStructuredArchive::FStream SoftObjectPathListStream = StructuredArchiveRoot.EnterStream(TEXT("SoftObjectPathList"));
		for (FSoftObjectPath& Path : Linker->SoftObjectPathList)
		{
			SoftObjectPathListStream.EnterElement() << Path;
		}
	}

	// Write GatherableText
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteGatherableTextData);
		WriteGatherableText(StructuredArchiveRoot, SaveContext);
	}

	// Save Dummy Import Map, overwritten later.
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteDummyImportMap);
		Linker->Summary.ImportOffset = (int32)Linker->Tell();
		for (FObjectImport& Import : Linker->ImportMap)
		{
			StructuredArchiveRoot.GetUnderlyingArchive() << Import;
		}
	}
	SaveContext.OffsetAfterImportMap = (int32)Linker->Tell();

	// Save Dummy Export Map, overwritten later.
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteDummyExportMap);
		Linker->Summary.ExportOffset = (int32)Linker->Tell();
		for (FObjectExport& Export : Linker->ExportMap)
		{
			*Linker << Export;
		}
	}
	SaveContext.OffsetAfterExportMap = (int32)Linker->Tell();

	// Save Depend Map
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteDependsMap);

		FStructuredArchive::FStream DependsStream = StructuredArchiveRoot.EnterStream(TEXT("DependsMap"));
		Linker->Summary.DependsOffset = (int32)Linker->Tell();
		if (SaveContext.IsCooking())
		{
			//@todo optimization, this should just be stripped entirely from cooked packages
			TArray<FPackageIndex> Depends; // empty array
			for (int32 ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ++ExportIndex)
			{
				DependsStream.EnterElement() << Depends;
			}
		}
		else
		{
			// save depends map (no need for later patching)
			check(Linker->DependsMap.Num() == Linker->ExportMap.Num());
			for (TArray<FPackageIndex>& Depends : Linker->DependsMap)
			{
				DependsStream.EnterElement() << Depends;
			}
		}
	}

	// Write Soft Package references & Searchable Names
	if (!SaveContext.IsFilterEditorOnly())
	{
		SCOPED_SAVETIMER(UPackage_Save_SaveSoftPackagesAndSearchableNames);

		// Save soft package references
		Linker->Summary.SoftPackageReferencesOffset = (int32)Linker->Tell();
		Linker->Summary.SoftPackageReferencesCount = Linker->SoftPackageReferenceList.Num();
		{
			FStructuredArchive::FStream SoftReferenceStream = StructuredArchiveRoot.EnterStream(TEXT("SoftReferences"));
			for (FName& SoftPackageName : Linker->SoftPackageReferenceList)
			{
				SoftReferenceStream.EnterElement() << SoftPackageName;
			}

			// Save searchable names map
			Linker->Summary.SearchableNamesOffset = (int32)Linker->Tell();
			Linker->SerializeSearchableNamesMap(StructuredArchiveRoot.EnterField(TEXT("SearchableNames")));
		}
	}
	else
	{
		Linker->Summary.SoftPackageReferencesCount = 0;
		Linker->Summary.SoftPackageReferencesOffset = 0;
		Linker->Summary.SearchableNamesOffset = 0;
	}

	// Save thumbnails
	{
		SCOPED_SAVETIMER(UPackage_Save_SaveThumbnails);
		SavePackageUtilities::SaveThumbnails(SaveContext.GetPackage(), Linker, StructuredArchiveRoot.EnterField(TEXT("Thumbnails")));
	}
	{
		// Save asset registry data so the editor can search for information about assets in this package
		SCOPED_SAVETIMER(UPackage_Save_SaveAssetRegistryData);
		UE::AssetRegistry::WritePackageData(StructuredArchiveRoot, SaveContext.IsCooking(), SaveContext.GetPackage(), Linker, SaveContext.GetImportsUsedInGame(), SaveContext.GetSoftPackagesUsedInGame(), SaveContext.GetTargetPlatform());
	}
	// Save level information used by World browser
	{
		SCOPED_SAVETIMER(UPackage_Save_WorldLevelData);
		SavePackageUtilities::SaveWorldLevelInfo(SaveContext.GetPackage(), Linker, StructuredArchiveRoot);
	}

	// Write Preload Dependencies
	{
		SCOPED_SAVETIMER(UPackage_Save_PreloadDependencies);
		SavePreloadDependencies(StructuredArchiveRoot, SaveContext);
	}
	Linker->Summary.TotalHeaderSize = (int32)Linker->Tell();

	// Rather than check if an offset is truncated every time we assign one, we can just check the final TotalHeaderSize to see if it is truncated.
	// Checking every time an offset is assigned would let us fail quicker but a) relies that new code follows the convention b) bloats the code a fair bit.
	if (Linker->Tell() > MAX_int32)
	{
		UE_LOG(LogSavePackage, Error, TEXT("Package header for '%s' is too large (%" UINT64_FMT " bytes), some package file summary offsets will be truncated when stored as a int32"),
			*SaveContext.GetPackage()->GetName(), Linker->Tell());

		return ESavePackageResult::Error;
	}

	return ReturnSuccessOrCancel();
}

ESavePackageResult WritePackageTextHeader(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	FLinkerSave* Linker = SaveContext.GetLinker();

	// Write GatherableText
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteGatherableTextData);
		WriteGatherableText(StructuredArchiveRoot, SaveContext);
	}

	// Write ImportTable
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteImportTable);
		FStructuredArchive::FStream ImportTableStream = StructuredArchiveRoot.EnterStream(TEXT("ImportTable"));
		for (FObjectImport& Import : Linker->ImportMap)
		{
			ImportTableStream.EnterElement() << Import;
		}
	}

	// Write ExportTable
	{
		SCOPED_SAVETIMER(UPackage_Save_WriteExportTable);
		FStructuredArchive::FStream ExportTableStream = StructuredArchiveRoot.EnterStream(TEXT("ExportTable"));
		for (FObjectExport& Export : Linker->ExportMap)
		{
			ExportTableStream.EnterElement() << Export;
		}
	}

	// Save thumbnails
	{
		SCOPED_SAVETIMER(UPackage_Save_SaveThumbnails);
		SavePackageUtilities::SaveThumbnails(SaveContext.GetPackage(), Linker, StructuredArchiveRoot.EnterField(TEXT("Thumbnails")));
	}
	// Save level information used by World browser
	{
		SCOPED_SAVETIMER(UPackage_Save_WorldLevelData);
		SavePackageUtilities::SaveWorldLevelInfo(SaveContext.GetPackage(), Linker, StructuredArchiveRoot);
	}

	return ReturnSuccessOrCancel();
}

ESavePackageResult WriteExports(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	//COOK_STAT(FScopedDurationTimer SaveTimer(FSavePackageStats::SerializeExportsTimeSec));
	SCOPED_SAVETIMER(UPackage_Save_SaveExports);
	FLinkerSave* Linker = SaveContext.GetLinker();
	FScopedSlowTask SlowTask((float)Linker->ExportMap.Num(), FText(), SaveContext.IsUsingSlowTask());

	FStructuredArchive::FRecord ExportsRecord = StructuredArchiveRoot.EnterRecord(TEXT("Exports"));

	// Save exports.
	int32 LastExportSaveStep = 0;
	for (int32 i = 0; i < Linker->ExportMap.Num(); i++)
	{
		if (GWarn->ReceivedUserCancel())
		{
			return ESavePackageResult::Canceled;
		}
		SlowTask.EnterProgressFrame();

		FObjectExport& Export = Linker->ExportMap[i];
		if (Export.Object)
		{
			SCOPED_SAVETIMER(UPackage_Save_SaveExport);

			// Save the object data.
			Export.SerialOffset = Linker->Tell();
			Linker->CurrentlySavingExport = FPackageIndex::FromExport(i);

			FString ObjectName = Export.Object->GetPathName(SaveContext.GetPackage());
			FStructuredArchive::FSlot ExportSlot = ExportsRecord.EnterField(*ObjectName);

#if WITH_EDITOR
			bool bSupportsText = UClass::IsSafeToSerializeToStructuredArchives(Export.Object->GetClass());
#else
			bool bSupportsText = false;
#endif

			SCOPED_SAVETIMER_TEXT(*WriteToString<128>(GetClassTraceScope(Export.Object), TEXT("_SaveSerialize")));
			if (Export.Object->HasAnyFlags(RF_ClassDefaultObject))
			{
				if (bSupportsText)
				{
					Export.Object->GetClass()->SerializeDefaultObject(Export.Object, ExportSlot);
				}
				else
				{
					FArchiveUObjectFromStructuredArchive Adapter(ExportSlot);
					Export.Object->GetClass()->SerializeDefaultObject(Export.Object, Adapter.GetArchive());
					Adapter.Close();
				}
			}
			else
			{
				TGuardValue<UObject*> GuardSerializedObject(SaveContext.GetSerializeContext()->SerializedObject, Export.Object);

				if (bSupportsText)
				{
					FStructuredArchive::FRecord ExportRecord = ExportSlot.EnterRecord();
					Export.Object->Serialize(ExportRecord);
				}
				else
				{
					FArchiveUObjectFromStructuredArchive Adapter(ExportSlot);
					Export.Object->Serialize(Adapter.GetArchive());
					Adapter.Close();
				}

#if WITH_EDITOR
				if (Linker->IsCooking())
				{
					Export.Object->CookAdditionalFiles(SaveContext.GetFilename(), SaveContext.GetTargetPlatform(),
						[&SaveContext](const TCHAR* Filename, void* Data, int64 Size)
						{
							FLargeMemoryWriter& Writer = SaveContext.AdditionalFilesFromExports.Emplace_GetRef(0, true, Filename);
							Writer.Serialize(Data, Size);
						});
				}
#endif
			}
			Linker->CurrentlySavingExport = FPackageIndex();
			Export.SerialSize = Linker->Tell() - Export.SerialOffset;
		}
	}
	// if an error occurred on the linker while serializing exports, return an error
	return Linker->IsError() ? ESavePackageResult::Error : ReturnSuccessOrCancel();
}

[[nodiscard]] ESavePackageResult BuildAndWriteTrailer(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	SaveContext.GetLinker()->Summary.PayloadTocOffset = INDEX_NONE;

	if (SaveContext.GetLinker()->PackageTrailerBuilder.IsValid())
	{
		// At the moment we assume that we cannot have reference payloads in the trailer if SAVE_BulkDataByReference is not set and we
		// cannot have locally stored payloads if SAVE_BulkDataByReference is set.
		checkf((SaveContext.GetSaveArgs().SaveFlags & SAVE_BulkDataByReference) != 0 || SaveContext.GetLinker()->PackageTrailerBuilder->GetNumReferencedPayloads() == 0,
			TEXT("Attempting to build a package trailer with referenced payloads but the SAVE_BulkDataByReference flag is not set. '%s'"), *SaveContext.GetPackage()->GetName());

		checkf(	(SaveContext.GetSaveArgs().SaveFlags & SAVE_BulkDataByReference) == 0 || SaveContext.GetLinker()->PackageTrailerBuilder->GetNumLocalPayloads() == 0,
				TEXT("Attempting to build a package trailer with local payloads but the SAVE_BulkDataByReference flag is set. '%s'"), *SaveContext.GetPackage()->GetName());

		checkf(SaveContext.IsTextFormat() == false, TEXT("Attempting to build a package trailer for text based asset '%s', this is not supported!"), *SaveContext.GetPackage()->GetName());

		SaveContext.GetLinker()->Summary.PayloadTocOffset = SaveContext.GetLinker()->Tell();
		if (!SaveContext.GetLinker()->PackageTrailerBuilder->BuildAndAppendTrailer(SaveContext.GetLinker(), *SaveContext.GetLinker()))
		{
			return ESavePackageResult::Error;
		}

		SaveContext.GetLinker()->PackageTrailerBuilder.Reset();
	}

	return ESavePackageResult::Success;
}

ESavePackageResult WriteAdditionalExportFiles(FSaveContext& SaveContext)
{
	FSavePackageContext* SavePackageContext = SaveContext.GetSavePackageContext();

	if (SaveContext.IsCooking() && SaveContext.AdditionalFilesFromExports.Num() > 0)
	{
		checkf(SaveContext.GetCurrentHarvestingRealm() != ESaveRealm::Optional , TEXT("Addtional export files is currently unsupported with optional package multi output, Package %s"), *SaveContext.GetPackage()->GetName());
		IPackageWriter* PackageWriter = SavePackageContext ? SavePackageContext->PackageWriter : nullptr;
		checkf(PackageWriter, TEXT("Cooking requires a PackageWriter"));
		for (FLargeMemoryWriter& Writer : SaveContext.AdditionalFilesFromExports)
		{
			const int64 Size = Writer.TotalSize();
			SaveContext.TotalPackageSizeUncompressed += Size;

			IPackageWriter::FAdditionalFileInfo FileInfo;
			FileInfo.PackageName = SaveContext.GetPackage()->GetFName();
			FileInfo.Filename = *Writer.GetArchiveName();

			FIoBuffer FileData(FIoBuffer::AssumeOwnership, Writer.ReleaseOwnership(), Size);

			// This might not actuallly write the file, but instead add it to a queue to write later.
			// (See TPackageWriterToSharedBuffer).
			PackageWriter->WriteAdditionalFile(FileInfo, FileData);
		}
		SaveContext.AdditionalFilesFromExports.Empty();
	}
	return ReturnSuccessOrCancel();
}

ESavePackageResult UpdatePackageHeader(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_UpdatePackageHeader);

	FLinkerSave* Linker = SaveContext.GetLinker();
	int64 OffsetBeforeUpdates = Linker->Tell();
#if WITH_EDITOR
	FArchiveStackTraceIgnoreScope IgnoreDiffScope(SaveContext.IsIgnoringHeaderDiff());
#endif

	// Write Real Import Map
	if (!SaveContext.IsTextFormat())
	{
		Linker->Seek(Linker->Summary.ImportOffset);
		FStructuredArchive::FStream ImportTableStream = StructuredArchiveRoot.EnterStream(TEXT("ImportTable"));
		for (FObjectImport& Import : Linker->ImportMap)
		{
			ImportTableStream.EnterElement() << Import;
		}
	}
	// Write Real Export Map
	if (!SaveContext.IsTextFormat())
	{
		check(Linker->Tell() == SaveContext.OffsetAfterImportMap);
		Linker->Seek(Linker->Summary.ExportOffset);
		FStructuredArchive::FStream ExportTableStream = StructuredArchiveRoot.EnterStream(TEXT("ExportTable"));

		for (FObjectExport& Export : Linker->ExportMap)
		{
			ExportTableStream.EnterElement() << Export;
		}
		check(Linker->Tell() == SaveContext.OffsetAfterExportMap);
	}

	// Figure out if at leat one export is marked as an asset
	bool bContainsAsset = false;
	for (FObjectExport& Export : Linker->ExportMap)
	{
		bContainsAsset |= Export.bIsAsset;
	}

	// Update Summary
	// Write Real Summary
	{
		//@todo: remove ExportCount and NameCount - no longer used
		Linker->Summary.Generations.Last().ExportCount = Linker->Summary.ExportCount;
		Linker->Summary.Generations.Last().NameCount = Linker->Summary.NameCount;

		// create the package source (based on developer or user created)
#if (UE_BUILD_SHIPPING && WITH_EDITOR)
		Linker->Summary.PackageSource = FMath::Rand() * FMath::Rand();
#else
		Linker->Summary.PackageSource = FCrc::StrCrc_DEPRECATED(*FPaths::GetBaseFilename(SaveContext.GetFilename()).ToUpper());
#endif

		// Flag package as requiring localization gather if the archive requires localization gathering.
		Linker->LinkerRoot->ThisRequiresLocalizationGather(Linker->RequiresLocalizationGather());

		// Update package flags from package, in case serialization has modified package flags.
		uint32 PackageFlags = Linker->LinkerRoot->GetPackageFlags();
		if (!bContainsAsset)
		{
			PackageFlags |= PKG_ContainsNoAsset;
		}
		// Take the Linker FilterEditorOnlyData setting over the package flags to set this flag in the summary
		if (Linker->IsFilterEditorOnly())
		{
			PackageFlags |= PKG_FilterEditorOnly;
		}
		else
		{
			PackageFlags &= ~PKG_FilterEditorOnly;
		}
		Linker->Summary.SetPackageFlags(PackageFlags);

		// @todo: custom versions: when can this be checked?
		{
			// Verify that the final serialization pass hasn't added any new custom versions. Otherwise this will result in crashes when loading the package.
			bool bNewCustomVersionsUsed = false;
			for (const FCustomVersion& LinkerCustomVer : Linker->GetCustomVersions().GetAllVersions())
			{
				if (Linker->Summary.GetCustomVersionContainer().GetVersion(LinkerCustomVer.Key) == nullptr)
				{
					UE_LOG(LogSavePackage, Error,
						TEXT("Unexpected custom version \"%s\" found when saving %s. This usually happens when export tagging and final serialization paths differ. Package will not be saved."),
						*LinkerCustomVer.GetFriendlyName().ToString(), *Linker->LinkerRoot->GetName());
					bNewCustomVersionsUsed = true;
				}
			}
			if (bNewCustomVersionsUsed)
			{
				return ESavePackageResult::Error;
			}
		}

		if (!SaveContext.IsTextFormat())
		{
			Linker->Seek(0);
		}
		{
			StructuredArchiveRoot.EnterField(TEXT("Summary")) << Linker->Summary;
		}

		if (!SaveContext.IsTextFormat())
		{
			check(Linker->Tell() == SaveContext.OffsetAfterPackageFileSummary);
		}
	}
	if (!SaveContext.IsTextFormat())
	{
		Linker->Seek(OffsetBeforeUpdates); // Return Linker Pos to the end; some packagewriters need it there
	}
	return ReturnSuccessOrCancel();
}

ESavePackageResult FinalizeFile(FStructuredArchive::FRecord& StructuredArchiveRoot, FSaveContext& SaveContext)
{
	SCOPED_SAVETIMER(UPackage_Save_FinalizeFile);

	// In the concurrent case, it is called right after routing presave so it can be done in batch before going concurrent
	if (!SaveContext.IsConcurrent())
	{
		// If we're writing to the existing file call ResetLoaders on the Package so that we drop the handle to the file on disk and can write to it
		//COOK_STAT(FScopedDurationTimer SaveTimer(FSavePackageStats::ResetLoadersTimeSec));
		ResetLoadersForSave(SaveContext.GetPackage(), SaveContext.GetFilename());
	}

	FSavePackageContext* SavePackageContext = SaveContext.GetSavePackageContext();
	IPackageWriter* PackageWriter = SavePackageContext ? SavePackageContext->PackageWriter : nullptr;
	if (PackageWriter || SaveContext.IsSaveToMemory())
	{
		bool bIsOptionalRealm = SaveContext.GetCurrentHarvestingRealm() == ESaveRealm::Optional;
		FLinkerSave* Linker = SaveContext.GetLinker();
		UE_LOG(LogSavePackage, Verbose, TEXT("Async saving from memory to '%s'"), SaveContext.GetFilename());
		FLargeMemoryWriter* Writer = static_cast<FLargeMemoryWriter*>(Linker->Saver);

		if (PackageWriter)
		{
			IPackageWriter::FPackageInfo PackageInfo;
			PackageInfo.PackageName = SaveContext.GetPackage()->GetFName();
			// Adjust LooseFilePath if needed
			if (bIsOptionalRealm)
			{
				// Optional output have the form PackagePath.o.ext
				PackageInfo.LooseFilePath = FPathViews::ChangeExtension(SaveContext.GetFilename(), TEXT("o.") + FPaths::GetExtension(SaveContext.GetFilename()));
				PackageInfo.MultiOutputIndex = 1;
			}
			else
			{
				PackageInfo.LooseFilePath = SaveContext.GetFilename();
			}
			PackageInfo.HeaderSize = Linker->Summary.TotalHeaderSize;

			FPackageId PackageId = FPackageId::FromName(PackageInfo.PackageName);
			PackageInfo.ChunkId = CreateIoChunkId(PackageId.Value(), PackageInfo.MultiOutputIndex, EIoChunkType::ExportBundleData);

			PackageWriter->WritePackageData(PackageInfo, *Writer, Linker->FileRegions);
		}
		else
		{
			checkf(!SaveContext.IsCooking(), TEXT("Cooking requires a PackageWriter"));
			EAsyncWriteOptions WriteOptions(EAsyncWriteOptions::None);

			// Add the uasset file to the list of output files
			int64 DataSize = Writer->TotalSize();
			SaveContext.AdditionalPackageFiles.Emplace(SaveContext.GetFilename(),
				FLargeMemoryPtr(Writer->ReleaseOwnership()), Linker->FileRegions, DataSize);

			for (FSavePackageOutputFile& Entry : SaveContext.AdditionalPackageFiles)
			{
				SavePackageUtilities::AsyncWriteFile(WriteOptions, Entry);
			}
		}
		SaveContext.CloseLinkerArchives();
	}
	else
	{
		// Destroy archives used for saving, closing file handle.
		if (SaveContext.CloseLinkerArchives() == false)
		{
			UE_LOG(LogSavePackage, Error, TEXT("Error writing temp file '%s' for '%s'"),
				SaveContext.GetTempFilename().IsSet() ? *SaveContext.GetTempFilename().GetValue() : TEXT("UNKNOWN"), SaveContext.GetFilename());
			return ESavePackageResult::Error;
		}

		// Move file to its real destination
		checkf(SaveContext.GetTempFilename().IsSet(), TEXT("The package should've been saved to a tmp file first! (%s)"), *SaveContext.GetFilename());

		// When saving in text format we will have two temp files, so we need to manually delete the non-textbased one
		if (SaveContext.IsTextFormat())
		{
			check(SaveContext.GetTextFormatTempFilename().IsSet());
			IFileManager::Get().Delete(*SaveContext.GetTempFilename().GetValue());
			SaveContext.SetTempFilename(SaveContext.GetTextFormatTempFilename());
			SaveContext.SetTextFormatTempFilename(TOptional<FString>());
		}

		// Add the .uasset file to the list of output files (TODO: Fix the 0 size, it isn't used after this point but needs to be cleaned up)
		SaveContext.AdditionalPackageFiles.Emplace(SaveContext.GetFilename(), SaveContext.GetTempFilename().GetValue(), 0);

		ESavePackageResult FinalizeResult = SavePackageUtilities::FinalizeTempOutputFiles(SaveContext.GetTargetPackagePath(), SaveContext.AdditionalPackageFiles,
			SaveContext.GetFinalTimestamp());
		
		SaveContext.SetTempFilename(TOptional<FString>());

		if (FinalizeResult != ESavePackageResult::Success)
		{
			if (SaveContext.IsGenerateSaveError())
			{
				UE_LOG(LogSavePackage, Error, TEXT("%s"), *FString::Printf(TEXT("Error saving '%s'"), SaveContext.GetFilename()));
				SaveContext.GetError()->Logf(ELogVerbosity::Warning, TEXT("%s"), *FText::Format(NSLOCTEXT("Core", "SaveWarning", "Error saving '{0}'"), FText::FromString(FString(SaveContext.GetFilename()))).ToString());
			}
			else
			{
				UE_LOG(LogSavePackage, Warning, TEXT("%s"), *FString::Printf(TEXT("Error saving '%s'"), SaveContext.GetFilename()));
			}
			return FinalizeResult;
		}
	}

	return ESavePackageResult::Success;
}

ESavePackageResult BeginCachePlatformCookedData(FSaveContext& SaveContext)
{
#if WITH_EDITOR
	// TODO: Call IsCachedCookedPlatformDataLoaded on all the objects until it returns true. This is required for the contract.
	// We tried enabling this once, but it created knockon bugs: Textures created by landscape were not handling it correctly
	// (which we have subsequently fixed) and MaterialInstanceConstants created by landscape were not handling it correctly (which 
	// we have not yet diagnosed)
#define SAVEPACKAGE_CALL_ISCACHEDCOOKEDPLATFORMDATALOADED 0

	// TODO: Remove BeginCacheForCookedPlatformData from SavePackage; it is already called by the cooker.
	// Cache platform cooked data
	if (SaveContext.IsCooking() && !SaveContext.IsConcurrent())
	{
		const ITargetPlatform* TargetPlatform = SaveContext.GetTargetPlatform();
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithPackage(SaveContext.GetPackage(), ObjectsInPackage);
		for (TArray<UObject*>::TIterator Iter(ObjectsInPackage); Iter; ++Iter)
		{
			UObject* Object = *Iter;
			if (SaveContext.IsUnsaveable(Object))
			{
				Iter.RemoveCurrentSwap();
				continue;
			}
			Object->BeginCacheForCookedPlatformData(TargetPlatform);
#if SAVEPACKAGE_CALL_ISCACHEDCOOKEDPLATFORMDATALOADED
			if (Object->IsCachedCookedPlatformDataLoaded(TargetPlatform))
			{
				Iter.RemoveCurrentSwap();
				continue;
			}
#endif
		}
#if SAVEPACKAGE_CALL_ISCACHEDCOOKEDPLATFORMDATALOADED
		if (ObjectsInPackage.Num())
		{
			if (SaveContext.GetSaveArgs().SaveFlags & SAVE_AllowTimeout)
			{
				return ESavePackageResult::Timeout;
			}
			const float MaxWaitSeconds = 30.f;
			const double EndTimeSeconds = FPlatformTime::Seconds() + MaxWaitSeconds;
			while (ObjectsInPackage.Num())
			{
				if (FPlatformTime::Seconds() > EndTimeSeconds)
				{
					UE_LOG(LogSavePackage, Error, TEXT("Save of %s failed: timed out waiting for IsCachedCookedPlatformDataLoaded on %s."),
						SaveContext.GetFilename(), *ObjectsInPackage[0]->GetFullName());
					return ESavePackageResult::Error;
				}
				for (TArray<UObject*>::TIterator Iter(ObjectsInPackage); Iter; ++Iter)
				{
					if ((*Iter)->IsCachedCookedPlatformDataLoaded(TargetPlatform))
					{
						Iter.RemoveCurrentSwap();
						continue;
					}
				}
			}
		}
#endif
	}
#endif
	return ESavePackageResult::Success;
}

void ClearCachedPlatformCookedData(FSaveContext& SaveContext)
{
#if WITH_EDITOR
	if (SaveContext.IsCooking() && !SaveContext.IsConcurrent())
	{
		TArray<UObject*> ObjectsInPackage;
		GetObjectsWithPackage(SaveContext.GetPackage(), ObjectsInPackage);
		for (UObject* Object : ObjectsInPackage)
		{
			if (!SaveContext.IsUnsaveable(Object))
			{
				Object->ClearCachedCookedPlatformData(SaveContext.GetTargetPlatform());
			}
		}
	}
#endif
}

void PostSavePackage(FSaveContext& SaveContext)
{
	UPackage* Package = SaveContext.GetPackage();
	// Package has been saved, so unmark the NewlyCreated flag.
	Package->ClearPackageFlags(PKG_NewlyCreated);

	// Copy and modify the output SerializedPackageFlags from the PackageFlags written into the default realm summary
	uint32 SerializedPackageFlags = SaveContext.GetLinker()->Summary.GetPackageFlags();
	// Consider all output packages when reflecting PKG_ContainsNoAsset to the single entry in SerializedPackageFlags and the asset registry
	bool bContainsNoAsset = true;
	for (FLinkerSave* Linker : SaveContext.GetLinkers())
	{
		// GetLinkers shouldn't return null linker
		check(Linker);

		const bool bLinkerContainsNoAsset = Linker->Summary.GetPackageFlags() & PKG_ContainsNoAsset;
		bContainsNoAsset &= bLinkerContainsNoAsset;

		// Call the linker post save callbacks
		Linker->OnPostSave(SaveContext.GetTargetPackagePath(), FObjectPostSaveContext(SaveContext.GetObjectSaveContext()));
	}

	if (bContainsNoAsset)
	{
		SerializedPackageFlags |= PKG_ContainsNoAsset;
	}
	else
	{
		SerializedPackageFlags &= ~PKG_ContainsNoAsset;
	}
	SaveContext.SerializedPackageFlags = SerializedPackageFlags;

	// Notify the soft reference collector about our harvested soft references during save. 
	// This is currently needed only for cooking which does not require editor-only references 
#if WITH_EDITOR
	if (SaveContext.IsCooking())
	{
		GRedirectCollector.CollectSavedSoftPackageReferences(Package->GetFName(), SaveContext.GetSoftPackagesUsedInGame(), false);
	}
#endif


	// Send a message that the package was saved
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	UPackage::PackageSavedEvent.Broadcast(SaveContext.GetFilename(), Package);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
	UPackage::PackageSavedWithContextEvent.Broadcast(SaveContext.GetFilename(), Package, FObjectPostSaveContext(SaveContext.GetObjectSaveContext()));

	// update the internal package filename path if we're saving to a valid mounted path and we aren't currently cooking
#if WITH_EDITOR
	const FPackagePath& PackagePath = SaveContext.GetTargetPackagePath();
	if (SaveContext.IsUpdatingLoadedPath())
	{
		Package->SetLoadedPath(PackagePath);
	}
#endif
}

ESavePackageResult SaveHarvestedRealms(FSaveContext& SaveContext, ESaveRealm HarvestingContextToSave)
{
	// Set the current harvested context to save
	FSaveContext::FSetSaveRealmToSaveScope Scope(SaveContext, HarvestingContextToSave);

	// Create slow task dialog if needed
	const int32 TotalSaveSteps = 11;
	FScopedSlowTask SlowTask(TotalSaveSteps, FText(), SaveContext.IsUsingSlowTask());

	// Validate Exports
	SlowTask.EnterProgressFrame();
	SaveContext.Result = ValidateExports(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		// if we are skipping processing due to an empty realm, consider the save succesful since that only used internally for optional realm
		SaveContext.Result = SaveContext.Result == ESavePackageResult::EmptyRealm ? ESavePackageResult::Success : SaveContext.Result;
		return SaveContext.Result;
	}

	// Validate Imports
	SlowTask.EnterProgressFrame();
	SaveContext.Result = ValidateImports(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// Create Linker
	SlowTask.EnterProgressFrame();
	SaveContext.Result = CreateLinker(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// Build Linker
	SlowTask.EnterProgressFrame();
	SaveContext.Result = BuildLinker(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	FStructuredArchive::FRecord StructuredArchiveRoot = SaveContext.GetStructuredArchive()->Open().EnterRecord();
	StructuredArchiveRoot.GetUnderlyingArchive().SetSerializeContext(SaveContext.GetSerializeContext());

	// Write Header
	SlowTask.EnterProgressFrame();
	SaveContext.Result = !SaveContext.IsTextFormat() ? WritePackageHeader(StructuredArchiveRoot, SaveContext) : WritePackageTextHeader(StructuredArchiveRoot, SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// SHA Generation
	TArray<uint8>* ScriptSHABytes = nullptr;
	{
		// look for this package in the list of packages to generate script SHA for 
		ScriptSHABytes = FLinkerSave::PackagesToScriptSHAMap.Find(*FPaths::GetBaseFilename(SaveContext.GetFilename()));
		// if we want to generate the SHA key, start tracking script writes
		if (ScriptSHABytes)
		{
			SaveContext.GetLinker()->StartScriptSHAGeneration();
		}
	}

	// Write Exports
	SlowTask.EnterProgressFrame();
	SaveContext.Result = WriteExports(StructuredArchiveRoot, SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}
	// Get SHA Key
	{
		// if we want to generate the SHA key, get it out now that the package has finished saving
		if (ScriptSHABytes && SaveContext.GetLinker()->ContainsCode())
		{
			// make space for the 20 byte key
			ScriptSHABytes->Empty(20);
			ScriptSHABytes->AddUninitialized(20);

			// retrieve it
			SaveContext.GetLinker()->GetScriptSHAKey(ScriptSHABytes->GetData());
		}
	}

	IPackageWriter* PackageWriter = SaveContext.GetPackageWriter();
	if (PackageWriter)
	{
		const int64 ExportsSize = SaveContext.GetLinker()->Tell();
		SaveContext.Result = WriteAdditionalFiles(SaveContext, SlowTask, ExportsSize);
		checkf(SaveContext.GetLinker()->Tell() == ExportsSize, TEXT("The writing of additional files is not allowed to append to the LinkerSave when using a PackageWriter."));
		if (SaveContext.Result != ESavePackageResult::Success)
		{
			return SaveContext.Result;
		}
	}
	else
	{
		// AdditionalFiles are appended to the Linker's archive, and so must be appended before we can calculate the full size of the Package
		SaveContext.Result = WriteAdditionalFiles(SaveContext, SlowTask, -1);
		if (SaveContext.Result != ESavePackageResult::Success)
		{
			return SaveContext.Result;
		}
	}

	// Write out a tag to the end of the package
	if (PackageWriter == nullptr && !SaveContext.IsTextFormat())
	{
		uint32 Tag = PACKAGE_FILE_TAG;
		StructuredArchiveRoot.GetUnderlyingArchive() << Tag;
	}

	// Now that the package is written out we can write the package trailer that is appended
	// to the file. This should be the last thing written to the file!
	SlowTask.EnterProgressFrame();
	SaveContext.Result = BuildAndWriteTrailer(StructuredArchiveRoot, SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}	

	int64 ExportsSize = SaveContext.GetLinker()->Tell();
	if (PackageWriter)
	{
		PackageWriter->AddToExportsSize(ExportsSize);
	}
	// Store the package header and export size of the non optional realm
	if (SaveContext.GetCurrentHarvestingRealm() != ESaveRealm::Optional)
	{
		SaveContext.PackageHeaderAndExportSize = ExportsSize;
	}
	SaveContext.TotalPackageSizeUncompressed += ExportsSize;
	for (const FSavePackageOutputFile& File : SaveContext.AdditionalPackageFiles)
	{
		SaveContext.TotalPackageSizeUncompressed += File.DataSize;
	}

	// Update Package Header
	SlowTask.EnterProgressFrame();
	SaveContext.Result = UpdatePackageHeader(StructuredArchiveRoot, SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// Finalize File Write
	SlowTask.EnterProgressFrame();
	SaveContext.Result = FinalizeFile(StructuredArchiveRoot, SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	//COOK_STAT(FSavePackageStats::MBWritten += ((double)SaveContext.TotalPackageSizeUncompressed) / 1024.0 / 1024.0);
	{
		SCOPED_SAVETIMER(UPackage_Save_MarkExportLoaded);
		FLinkerSave* Linker = SaveContext.GetLinker();
		// Mark exports and the package as RF_Loaded after they've been serialized
		// This is to ensue that newly created packages are properly marked as loaded (since they now exist on disk and 
		// in memory in the exact same state).

		// Nobody should be touching those objects beside us while we are saving them here as this can potentially be executed from another thread
		for (auto& Export : Linker->ExportMap)
		{
			if (Export.Object)
			{
				Export.Object->SetFlags(RF_WasLoaded | RF_LoadCompleted);
			}
		}
	}
	return SaveContext.Result;
}

/**
 * InnerSave is the portion of Save that can be safely run concurrently
 */
ESavePackageResult InnerSave(FSaveContext& SaveContext)
{
	TRefCountPtr<FUObjectSerializeContext> SerializeContext(FUObjectThreadContext::Get().GetSerializeContext());
	SaveContext.SetSerializeContext(SerializeContext);
	SaveContext.SetEDLCookChecker(&FEDLCookCheckerThreadState::Get());

	// Create slow task dialog if needed
	const int32 TotalSaveSteps = 4;
	FScopedSlowTask SlowTask(TotalSaveSteps, FText(), SaveContext.IsUsingSlowTask());
	SlowTask.MakeDialogDelayed(3.0f, SaveContext.IsFromAutoSave());

	// Harvest Package
	SlowTask.EnterProgressFrame();
	SaveContext.Result = HarvestPackage(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	SlowTask.EnterProgressFrame();
	SaveContext.Result = ValidateRealms(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// @todo: Need to adjust GIsSavingPackage to properly prevent generating reference once, package harvesting is done
	// GIsSavingPackage is too harsh however, since it should be scope only to the current package
	FScopedSavingFlag IsSavingFlag(SaveContext.IsConcurrent(), SaveContext.GetPackage());

	// Split the save context into its harvested contexts,
	// This essentially mean that a package can produce multiple package output. 
	// This is different then the multiple file outputs a package can already produce
	// since each harvested context will produce those multiple file outputs i.e:
	// Input package -> Main cooked package -> .uasset
	//										-> .uexp
	//										-> .ubulk
	//										-> etc
	//					Sub cooked package	-> .o.uasset
	//										-> .o.uexp
	//										-> .o.ubulk
	//										-> etc
	SlowTask.EnterProgressFrame();
	for (ESaveRealm HarvestingContext : SaveContext.GetHarvestedRealmsToSave())
	{
		SaveContext.Result = SaveHarvestedRealms(SaveContext, HarvestingContext);
		if (SaveContext.Result != ESavePackageResult::Success)
		{
			return SaveContext.Result;
		}
	}

	SlowTask.EnterProgressFrame();
	{
		// Mark the Package RF_Loaded after its been serialized. 
		// This was already done for each object in the package in SaveHarvestedRealms
		SaveContext.GetPackage()->SetFlags(RF_WasLoaded | RF_LoadCompleted);

		// Clear dirty flag if desired
		if (!SaveContext.IsKeepDirty())
		{
			SaveContext.GetPackage()->SetDirtyFlag(false);
		}

		// Update package FileSize value
		if (SaveContext.IsUpdatingLoadedPath())
		{
			SaveContext.UpdatePackageFileSize(SaveContext.PackageHeaderAndExportSize);
		}
	}
	return SaveContext.Result;
}

/** WriteAdditionalPayloads writes BulkData, PayloadSidecarFiles, and other data that is separate from exports.
 * Depending on settings, these additional files may be appended to the LinkerSave after the exports, or they
 * may be written into separate sidecar files.
 * 
 * @param SaveContext The context for the overall save, including data about the additional payloads
 * @param LinkerSize If the Linker has finished writing, this is the size of the Linker's archive: Linker->Tell(). Otherwise it is -1.
 */
ESavePackageResult WriteAdditionalFiles(FSaveContext& SaveContext, FScopedSlowTask& SlowTask, int64 LinkerSize)
{
	// Save Bulk Data
	SlowTask.EnterProgressFrame();
	int64 DataStartOffset = LinkerSize >= 0 ? LinkerSize : SaveContext.GetLinker()->Tell();
	SaveContext.Result = SavePackageUtilities::SaveBulkData(SaveContext.GetLinker(), DataStartOffset,
		SaveContext.GetPackage(), SaveContext.GetFilename(), SaveContext.GetTargetPlatform(),
		SaveContext.GetSavePackageContext(), SaveContext.GetSaveArgs().SaveFlags, SaveContext.IsTextFormat(),
		SaveContext.TotalPackageSizeUncompressed,
		SaveContext.GetCurrentHarvestingRealm() == ESaveRealm::Optional);

	// Add any pending data blobs to the end of the file by invoking the callbacks
	ESavePackageResult Result = SavePackageUtilities::AppendAdditionalData(*SaveContext.GetLinker(), DataStartOffset, SaveContext.GetSavePackageContext());
	if (Result != ESavePackageResult::Success)
	{
		return Result;
	}

	// Create the payload side car file (if needed)
	Result = SavePackageUtilities::CreatePayloadSidecarFile(*SaveContext.GetLinker(), SaveContext.GetTargetPackagePath(), SaveContext.IsSaveToMemory(),
		SaveContext.AdditionalPackageFiles, SaveContext.GetSavePackageContext());
	if (Result != ESavePackageResult::Success)
	{
		return Result;
	}

	// Write Additional files from export
	SlowTask.EnterProgressFrame();
	Result = WriteAdditionalExportFiles(SaveContext);
	if (Result != ESavePackageResult::Success)
	{
		return Result;
	}
	return ESavePackageResult::Success;
}

FText GetSlowTaskStatusMessage(const FSaveContext& SaveContext)
{
	FString CleanFilename = FPaths::GetCleanFilename(SaveContext.GetFilename());
	FFormatNamedArguments Args;
	Args.Add(TEXT("CleanFilename"), FText::FromString(CleanFilename));
	return FText::Format(NSLOCTEXT("Core", "SavingFile", "Saving file: {CleanFilename}..."), Args);
}

} // end namespace

FSavePackageResultStruct UPackage::Save2(UPackage* InPackage, UObject* InAsset, const TCHAR* InFilename,
	const FSavePackageArgs& SaveArgs)
{
	COOK_STAT(FScopedDurationTimer FuncSaveTimer(FSavePackageStats::SavePackageTimeSec));
	COOK_STAT(FSavePackageStats::NumPackagesSaved++);
	SCOPED_SAVETIMER(UPackage_Save2);
	UE_SCOPED_COOK_STAT(InPackage->GetFName(), EPackageEventStatType::SavePackage);

	FSaveContext SaveContext(InPackage, InAsset, InFilename, SaveArgs);

	// Create the slow task dialog if needed
	const int32 TotalSaveSteps = 7;
	FScopedSlowTask SlowTask(TotalSaveSteps, GetSlowTaskStatusMessage(SaveContext), SaveContext.IsUsingSlowTask());
	SlowTask.MakeDialogDelayed(3.0f, SaveContext.IsFromAutoSave());

	SlowTask.EnterProgressFrame();
	SaveContext.Result = ValidatePackage(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	// Ensures
	SlowTask.EnterProgressFrame();
	EnsurePackageLocalization(SaveContext.GetPackage());
	{
		// FullyLoad the package's Loader, so that anything we need to serialize (bulkdata, thumbnails) is available
		//COOK_STAT(FScopedDurationTimer SaveTimer(FSavePackageStats::FullyLoadLoadersTimeSec));
		EnsureLoadingComplete(SaveContext.GetPackage());

		if (!SaveContext.IsConcurrent())
		{
			// We need to fulfill all pending streaming and async loading requests to then allow us to lock the global IO manager. 
			// The latter implies flushing all file handles which is a pre-requisite of saving a package. The code basically needs 
			// to be sure that we are not reading from a file that is about to be overwritten and that there is no way we might 
			// start reading from the file till we are done overwriting it.
			FlushAsyncLoading();
		}
		(*GFlushStreamingFunc)();
	}

	// PreSave Asset
	SlowTask.EnterProgressFrame();
	PreSavePackage(SaveContext);
	if (SaveContext.GetAsset() && !SaveContext.IsConcurrent())
	{
		FObjectSaveContextData& ObjectSaveContext = SaveContext.GetObjectSaveContext();
		UE::SavePackageUtilities::CallPreSaveRoot(SaveContext.GetAsset(), ObjectSaveContext);
		SaveContext.SetPostSaveRootRequired(true);
		SaveContext.SetPreSaveCleanup(ObjectSaveContext.bCleanupRequired);
	}

	// Route Presave only if not calling concurrently or if the PackageWriter claims already completed, in those case they should be handled separately already
	SlowTask.EnterProgressFrame();
	IPackageWriter* PackageWriter = SaveContext.GetPackageWriter();
	if (!SaveContext.IsConcurrent() && (!PackageWriter || !PackageWriter->IsPreSaveCompleted()))
	{
		SaveContext.Result = RoutePresave(SaveContext);
		if (SaveContext.Result != ESavePackageResult::Success)
		{
			return SaveContext.Result;
		}
	}

	// Trigger platform cooked data caching before package harvesting 
	// because it might modify some property and hence affect harvested property name for tagged property for example
	SaveContext.Result = BeginCachePlatformCookedData(SaveContext);
	if (SaveContext.Result != ESavePackageResult::Success)
	{
		return SaveContext.Result;
	}

	SlowTask.EnterProgressFrame();
	{
		// @todo: Once GIsSavingPackage is reworked we should reinstore the saving flag here for the gc lock
		//FScopedSavingFlag IsSavingFlag(SaveContext.IsConcurrent());
		SaveContext.Result = InnerSave(SaveContext);

		// in case of failure or cancellation, do not exit here, still run cleanup (PostSaveRoot/ClearCachedPlatformCookedData)
	}

	// PostSave Asset
	SlowTask.EnterProgressFrame();
	if (SaveContext.GetPostSaveRootRequired() && SaveContext.GetAsset())
	{
		UE::SavePackageUtilities::CallPostSaveRoot(SaveContext.GetAsset(), SaveContext.GetObjectSaveContext(), SaveContext.GetPreSaveCleanup());
		SaveContext.SetPostSaveRootRequired(false);
	}

	ClearCachedPlatformCookedData(SaveContext);

	// PostSave Package - edit in memory package and send events if save was successful
	SlowTask.EnterProgressFrame();
	if (SaveContext.Result == ESavePackageResult::Success)
	{
		PostSavePackage(SaveContext);
	}
	return SaveContext.GetFinalResult();
}


ESavePackageResult UPackage::SaveConcurrent(TArrayView<FPackageSaveInfo> InPackages, const FSavePackageArgs& SaveArgs, TArray<FSavePackageResultStruct>& OutResults)
{
	const int32 TotalSaveSteps = 4;
	FScopedSlowTask SlowTask(TotalSaveSteps, NSLOCTEXT("Core", "SavingFiles", "Saving files..."), SaveArgs.bSlowTask);
	SlowTask.MakeDialogDelayed(3.0f, !!(SaveArgs.SaveFlags & SAVE_FromAutosave));

	// Create all the package save context and run pre save
	SlowTask.EnterProgressFrame();
	TArray<FSaveContext> PackageSaveContexts;
	{
		SCOPED_SAVETIMER(UPackage_SaveConcurrent_PreSave);
		for (FPackageSaveInfo& PackageSaveInfo : InPackages)
		{
			FSaveContext& SaveContext = PackageSaveContexts.Emplace_GetRef(PackageSaveInfo.Package, PackageSaveInfo.Package->FindAssetInPackage(), *PackageSaveInfo.Filename, SaveArgs, nullptr);

			// Validation
			SaveContext.Result = ValidatePackage(SaveContext);
			if (SaveContext.Result != ESavePackageResult::Success)
			{
				continue;
			}

			// Ensures
			EnsurePackageLocalization(SaveContext.GetPackage());
			EnsureLoadingComplete(SaveContext.GetPackage()); //@todo: needed??

			// PreSave Asset
			PreSavePackage(SaveContext);
			if (SaveContext.GetAsset())
			{
				SCOPED_SAVETIMER(UPackage_SaveConcurrent_PreSaveRoot);
				FObjectSaveContextData& ObjectSaveContext = SaveContext.GetObjectSaveContext();
				UE::SavePackageUtilities::CallPreSaveRoot(SaveContext.GetAsset(), ObjectSaveContext);
				SaveContext.SetPreSaveCleanup(ObjectSaveContext.bCleanupRequired);
			}

			// Route Presave
			SaveContext.Result = RoutePresave(SaveContext);
			if (SaveContext.Result != ESavePackageResult::Success)
			{
				continue;
			}
		}
	}

	SlowTask.EnterProgressFrame();
	{
		// Flush async loading and reset loaders
		SCOPED_SAVETIMER(UPackage_SaveConcurrent_ResetLoadersForSave);
		ResetLoadersForSave(InPackages);
	}

	SlowTask.EnterProgressFrame();
	{
		SCOPED_SAVETIMER(UPackage_SaveConcurrent);
		// @todo: Once GIsSavingPackage is reworked we should reinstore the saving flag here for the gc lock
		// Passing in false here so that GIsSavingPackage is set to true on top of locking the GC
		//FScopedSavingFlag IsSavingFlag(false);

		// Concurrent Part
		ParallelFor(PackageSaveContexts.Num(), [&PackageSaveContexts](int32 PackageIdx)
			{
				InnerSave(PackageSaveContexts[PackageIdx]);
			});
	}

	// Run Post Concurrent Save
	SlowTask.EnterProgressFrame();
	{
		SCOPED_SAVETIMER(UPackage_SaveConcurrent_PostSave);
		for (FSaveContext& SaveContext : PackageSaveContexts)
		{
			// PostSave Asset
			if (SaveContext.GetAsset())
			{
				UE::SavePackageUtilities::CallPostSaveRoot(SaveContext.GetAsset(), SaveContext.GetObjectSaveContext(), SaveContext.GetPreSaveCleanup());
				SaveContext.SetPreSaveCleanup(false);
			}

			ClearCachedPlatformCookedData(SaveContext);

			// PostSave Package - edit in memory package and send events
			if (SaveContext.Result == ESavePackageResult::Success)
			{
				PostSavePackage(SaveContext);
			}
			OutResults.Add(SaveContext.GetFinalResult());
		}
	}
	
	return ESavePackageResult::Success;
}

#endif // UE_WITH_SAVEPACKAGE
