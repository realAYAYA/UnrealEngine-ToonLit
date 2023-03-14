// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraVersionedObject.h"

#include "NiagaraTypes.h"

//------------ dummy implementations -------------------------

namespace NiagaraVersionedObject
{
	struct DummyObject : FNiagaraVersionDataAccessor
	{
		virtual FNiagaraAssetVersion& GetObjectVersion() override { return Version; }
		virtual FText& GetVersionChangeDescription() override { return Desc; }
		virtual ENiagaraPythonUpdateScriptReference& GetUpdateScriptExecutionType() override {return Type; }
		virtual FString& GetPythonUpdateScript() override {return Script; }
		virtual FFilePath& GetScriptAsset() override {return Path; }
		virtual bool& IsDeprecated() override { return bDeprecated; }

		FNiagaraAssetVersion Version;
		FText Desc;
		ENiagaraPythonUpdateScriptReference Type = ENiagaraPythonUpdateScriptReference::None;
		FString Script;
		FFilePath Path;
		bool bDeprecated;
	};

	static DummyObject Dummy;
}

FNiagaraAssetVersion& FNiagaraVersionDataAccessor::GetObjectVersion()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetObjectVersion();
}

FText& FNiagaraVersionDataAccessor::GetVersionChangeDescription()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetVersionChangeDescription();
}

bool& FNiagaraVersionDataAccessor::IsDeprecated()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.IsDeprecated();
}

FText& FNiagaraVersionDataAccessor::GetDeprecationMessage()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetVersionChangeDescription();
}

ENiagaraPythonUpdateScriptReference& FNiagaraVersionDataAccessor::GetUpdateScriptExecutionType()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetUpdateScriptExecutionType();
}

FString& FNiagaraVersionDataAccessor::GetPythonUpdateScript()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetPythonUpdateScript();
}

FFilePath& FNiagaraVersionDataAccessor::GetScriptAsset()
{
	ensure(false);
	return NiagaraVersionedObject::Dummy.GetScriptAsset();
}

TSharedPtr<FNiagaraVersionDataAccessor> FNiagaraVersionedObject::GetVersionDataAccessor(const FGuid& Version)
{
	ensure(false);
	return TSharedPtr<FNiagaraVersionDataAccessor>();
}

bool FNiagaraVersionedObject::IsVersioningEnabled() const
{
	ensure(false);
	return false;
}

TArray<FNiagaraAssetVersion> FNiagaraVersionedObject::GetAllAvailableVersions() const
{
	ensure(false);
	return TArray<FNiagaraAssetVersion>();
}

FNiagaraAssetVersion FNiagaraVersionedObject::GetExposedVersion() const
{
	ensure(false);
	return FNiagaraAssetVersion();
}

FNiagaraAssetVersion const* FNiagaraVersionedObject::FindVersionData(const FGuid& VersionGuid) const
{
	ensure(false);
	return nullptr;
}

FGuid FNiagaraVersionedObject::AddNewVersion(int32 MajorVersion, int32 MinorVersion)
{
	ensure(false);
	return FGuid();
}

void FNiagaraVersionedObject::DeleteVersion(const FGuid& VersionGuid)
{
	ensure(false);
}

void FNiagaraVersionedObject::ExposeVersion(const FGuid& VersionGuid)
{
	ensure(false);
}

void FNiagaraVersionedObject::EnableVersioning()
{
	ensure(false);
}

void FNiagaraVersionedObject::DisableVersioning(const FGuid& VersionGuidToUse)
{
	ensure(false);
}
