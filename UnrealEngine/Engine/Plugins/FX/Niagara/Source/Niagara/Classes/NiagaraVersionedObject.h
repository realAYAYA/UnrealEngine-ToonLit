// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraTypes.h"

struct NIAGARA_API FNiagaraVersionDataAccessor
{
	virtual ~FNiagaraVersionDataAccessor() = default;

	virtual FNiagaraAssetVersion& GetObjectVersion();
	virtual FText& GetVersionChangeDescription();
	virtual bool& IsDeprecated();
	virtual FText& GetDeprecationMessage();
	virtual ENiagaraPythonUpdateScriptReference& GetUpdateScriptExecutionType();
	virtual FString& GetPythonUpdateScript();
	virtual FFilePath& GetScriptAsset();
};

class NIAGARA_API FNiagaraVersionedObject
{
public:
	virtual ~FNiagaraVersionedObject() = default;

	virtual TSharedPtr<FNiagaraVersionDataAccessor> GetVersionDataAccessor(const FGuid& Version); 

	/** If true then this script asset uses active version control to track changes. */
	virtual bool IsVersioningEnabled() const;
	
	/** Returns all available versions for this object. */
	virtual TArray<FNiagaraAssetVersion> GetAllAvailableVersions() const;

	/** Returns the version of the exposed version data (i.e. the version used when adding a module to the stack) */
	virtual FNiagaraAssetVersion GetExposedVersion() const;

	/** Returns the version data for the given guid, if it exists. Otherwise returns nullptr. */
	virtual FNiagaraAssetVersion const* FindVersionData(const FGuid& VersionGuid) const;
	
	/** Creates a new data entry for the given version number. The version must be > 1.0 and must not collide with an already existing version. The data will be a copy of the previous minor version. */
	virtual FGuid AddNewVersion(int32 MajorVersion, int32 MinorVersion);

	/** Deletes the version data for an existing version. The exposed version cannot be deleted and will result in an error. Does nothing if the guid does not exist in the object's version data. */
	virtual void DeleteVersion(const FGuid& VersionGuid);

	/** Changes the exposed version. Does nothing if the guid does not exist in the object's version data. */
	virtual void ExposeVersion(const FGuid& VersionGuid);

	/** Enables versioning for this object. */
	virtual void EnableVersioning();

	/** Disables versioning and keeps only the data from the given version guid. Note that this breaks ALL references from existing assets and should only be used when creating a copy of an object, as the effect is very destructive.  */
	virtual void DisableVersioning(const FGuid& VersionGuidToUse);
};
