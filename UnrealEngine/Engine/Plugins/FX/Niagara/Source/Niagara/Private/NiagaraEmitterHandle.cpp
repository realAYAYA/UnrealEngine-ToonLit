// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraCommon.h"
#include "NiagaraCustomVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraEmitterHandle)

const FNiagaraEmitterHandle FNiagaraEmitterHandle::InvalidHandle;

FNiagaraEmitterHandle::FNiagaraEmitterHandle() 
	: bIsEnabled(true)
#if WITH_EDITORONLY_DATA
	, Source_DEPRECATED(nullptr)
	, LastMergedSource_DEPRECATED(nullptr)
	, bIsolated(false)
	, Instance_DEPRECATED(nullptr)
#endif
{
}

#if WITH_EDITORONLY_DATA
FNiagaraEmitterHandle::FNiagaraEmitterHandle(UNiagaraEmitter& InEmitter, const FGuid& Version)
	: Name(*InEmitter.GetUniqueEmitterName())
	, Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(true)
	, Source_DEPRECATED(nullptr)
	, LastMergedSource_DEPRECATED(nullptr)
	, bIsolated(false)
	, Instance_DEPRECATED(nullptr)
	, VersionedInstance(FVersionedNiagaraEmitter(&InEmitter, Version))
	, EmitterMode(ENiagaraEmitterMode::Standard)
{
}

FNiagaraEmitterHandle::FNiagaraEmitterHandle(const FVersionedNiagaraEmitter& InEmitter)
	: Name(*InEmitter.Emitter->GetUniqueEmitterName())
	, Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(true)
	, Source_DEPRECATED(nullptr)
	, LastMergedSource_DEPRECATED(nullptr)
	, bIsolated(false)
	, Instance_DEPRECATED(nullptr)
	, VersionedInstance(InEmitter)
	, EmitterMode(ENiagaraEmitterMode::Standard)
{
}

//-TODO:Stateless
FNiagaraEmitterHandle::FNiagaraEmitterHandle(UNiagaraStatelessEmitter& InEmitter)
	: Name(*InEmitter.GetUniqueEmitterName())
	, Id(FGuid::NewGuid())
	, IdName(*Id.ToString())
	, bIsEnabled(true)
	, Source_DEPRECATED(nullptr)
	, LastMergedSource_DEPRECATED(nullptr)
	, bIsolated(false)
	, Instance_DEPRECATED(nullptr)
	, StatelessEmitter(&InEmitter)
	, EmitterMode(ENiagaraEmitterMode::Stateless)
{
}
//-TODO:Stateless
#endif

bool FNiagaraEmitterHandle::IsValid() const
{
	return Id.IsValid();
}

FGuid FNiagaraEmitterHandle::GetId() const
{
	return Id;
}

FName FNiagaraEmitterHandle::GetIdName() const
{
	return IdName;
}

FName FNiagaraEmitterHandle::GetName() const
{
	return Name;
}

void FNiagaraEmitterHandle::SetName(FName InName, UNiagaraSystem& InOwnerSystem)
{
	FName SanitizedName = *FNiagaraUtilities::SanitizeNameForObjectsAndPackages(InName.ToString());
	if (SanitizedName.IsEqual(Name, ENameCase::CaseSensitive, false))
	{
		return;
	}

	TSet<FName> OtherEmitterNames;
	for (const FNiagaraEmitterHandle& OtherEmitterHandle : InOwnerSystem.GetEmitterHandles())
	{
		if (OtherEmitterHandle.GetId() != GetId())
		{
			OtherEmitterNames.Add(OtherEmitterHandle.GetName());
		}
	}
	FName UniqueName = FNiagaraUtilities::GetUniqueName(SanitizedName, OtherEmitterNames);

	Name = UniqueName;

	if (StatelessEmitter != nullptr)
	{
		StatelessEmitter->SetUniqueEmitterName(Name.ToString());
	}

	if (VersionedInstance.Emitter && VersionedInstance.Emitter->SetUniqueEmitterName(Name.ToString()))
	{
 #if WITH_EDITOR
		if (InOwnerSystem.GetSystemSpawnScript() && InOwnerSystem.GetSystemSpawnScript()->GetLatestSource())
		{
			// Just invalidate the system scripts here. The emitter scripts have their important variables 
			// changed in the SetUniqueEmitterName method above.
			InOwnerSystem.GetSystemSpawnScript()->GetLatestSource()->MarkNotSynchronized(TEXT("EmitterHandleRenamed"));
		}
#endif
	}
}

bool FNiagaraEmitterHandle::GetIsEnabled() const
{
	return bIsEnabled;
}

bool FNiagaraEmitterHandle::SetIsEnabled(bool bInIsEnabled, UNiagaraSystem& InOwnerSystem, bool bRecompileIfChanged)
{
	if (bIsEnabled != bInIsEnabled)
	{
		bIsEnabled = bInIsEnabled;

#if WITH_EDITOR
		if (InOwnerSystem.GetSystemSpawnScript() && InOwnerSystem.GetSystemSpawnScript()->GetLatestSource())
		{
			// We need to get the NiagaraNodeEmitters to update their enabled state based on what happened.
			InOwnerSystem.GetSystemSpawnScript()->GetLatestSource()->RefreshFromExternalChanges();

			// Need to cause us to recompile in the future if necessary...
			FString InvalidateReason = TEXT("Emitter enabled changed.");
			InOwnerSystem.GetSystemSpawnScript()->InvalidateCompileResults(InvalidateReason);
			InOwnerSystem.GetSystemUpdateScript()->InvalidateCompileResults(InvalidateReason);

			// Clean out the emitter's compile results for cleanliness.
			FVersionedNiagaraEmitterData* EmitterData = VersionedInstance.GetEmitterData();
			if (EmitterData)
			{
				EmitterData->InvalidateCompileResults();
			}

			// In some cases we may do the recompile now.
			if (bRecompileIfChanged)
			{
				InOwnerSystem.RequestCompile(false);
			}
		}
#endif
		return true;
	}
	return false;
}

#if WITH_EDITORONLY_DATA
void FNiagaraEmitterHandle::SetEmitterMode(UNiagaraSystem& InOwningSystem, ENiagaraEmitterMode InEmitterMode)
{
	if (EmitterMode != InEmitterMode)
	{
		EmitterMode = InEmitterMode;
		if (EmitterMode == ENiagaraEmitterMode::Stateless && StatelessEmitter == nullptr)
		{
			StatelessEmitter = NewObject<UNiagaraStatelessEmitter>(&InOwningSystem, "StatelessEmitter", RF_Transactional);
			StatelessEmitter->SetUniqueEmitterName(*Name.ToString());
			StatelessEmitter->CacheFromCompiledData();
		}
		InOwningSystem.RequestCompile(false);
		OnEmitterModeChangedDelegate.Broadcast();
	}
}
#endif

FVersionedNiagaraEmitter FNiagaraEmitterHandle::GetInstance() const
{
	return VersionedInstance;
}

FVersionedNiagaraEmitter& FNiagaraEmitterHandle::GetInstance()
{
	return VersionedInstance;
}

FVersionedNiagaraEmitterData* FNiagaraEmitterHandle::GetEmitterData() const
{
	return VersionedInstance.GetEmitterData();
}

FString FNiagaraEmitterHandle::GetUniqueInstanceName() const
{
	// We might not have an instance if this is a cooked object that we're loading in the editor
	if (VersionedInstance.Emitter)
	{
		return VersionedInstance.Emitter->GetUniqueEmitterName();
	}
	//-TODO:Stateless: We need an emitter data adapter / base class
	else if (StatelessEmitter)
	{
		return StatelessEmitter->GetUniqueEmitterName();
	}
	return FString();
}

#if WITH_EDITORONLY_DATA

bool FNiagaraEmitterHandle::NeedsRecompile() const
{
	if (GetIsEnabled())
	{
		TArray<UNiagaraScript*> Scripts;
		VersionedInstance.GetEmitterData()->GetScripts(Scripts);

		for (UNiagaraScript* Script : Scripts)
		{
			if (Script->IsCompilable() && !Script->AreScriptAndSourceSynchronized())
			{
				return true;
			}
		}
	}
	return false;
}

void FNiagaraEmitterHandle::ConditionalPostLoad(int32 NiagaraCustomVersion)
{
#if WITH_EDITORONLY_DATA
	if (Instance_DEPRECATED != nullptr)
	{
		VersionedInstance.Emitter = Instance_DEPRECATED;
		VersionedInstance.Version = Instance_DEPRECATED->IsVersioningEnabled() ? Instance_DEPRECATED->GetExposedVersion().VersionGuid : FGuid(); 
	}
#endif

	if (UNiagaraEmitter* InstanceEmitter = VersionedInstance.Emitter)
	{
		InstanceEmitter->ConditionalPostLoad();
		if (NiagaraCustomVersion < FNiagaraCustomVersion::MoveInheritanceDataFromTheEmitterHandleToTheEmitter)
		{
			FVersionedNiagaraEmitterData* EmitterData = GetEmitterData();
			if (Source_DEPRECATED && EmitterData)
			{
				Source_DEPRECATED->ConditionalPostLoad();
				EmitterData->VersionedParent = FVersionedNiagaraEmitter(Source_DEPRECATED, Source_DEPRECATED->GetExposedVersion().VersionGuid);
				Source_DEPRECATED = nullptr;
			}
			if (LastMergedSource_DEPRECATED && EmitterData)
			{
				LastMergedSource_DEPRECATED->ConditionalPostLoad();
				EmitterData->VersionedParentAtLastMerge = FVersionedNiagaraEmitter(LastMergedSource_DEPRECATED, LastMergedSource_DEPRECATED->GetExposedVersion().VersionGuid);
				EmitterData->VersionedParentAtLastMerge.Emitter->Rename(nullptr, InstanceEmitter, REN_ForceNoResetLoaders);
				LastMergedSource_DEPRECATED = nullptr;
			}
		}

		FText Reason;
		if (InstanceEmitter->GetFName().IsValidObjectName(Reason) == false)
		{
			UNiagaraSystem* OwningSystem = InstanceEmitter->GetTypedOuter<UNiagaraSystem>();
			if (OwningSystem != nullptr)
			{
				// If the name isn't a valid object name, set the name again so that it will be properly sanitized.
				SetName(Name, *OwningSystem);
			}
		}

		if (bIsEnabled == false)
		{
			FVersionedNiagaraEmitterData* EmitterData = VersionedInstance.GetEmitterData();
			if (EmitterData != nullptr)
			{
				EmitterData->InvalidateCompileResults();
			}
		}
	}
}

bool FNiagaraEmitterHandle::UsesEmitter(const FVersionedNiagaraEmitter& InEmitter) const
{
	if (ensure(InEmitter.Emitter))
	{
		if (InEmitter.Emitter->IsVersioningEnabled())
		{
			return VersionedInstance == InEmitter || (VersionedInstance.GetEmitterData() && VersionedInstance.GetEmitterData()->UsesEmitter(*InEmitter.Emitter));
		}
		else
		{
			return FNiagaraEmitterHandle::UsesEmitter(*InEmitter.Emitter);
		}
	}
	return false;
}

bool FNiagaraEmitterHandle::UsesEmitter(const UNiagaraEmitter& InEmitter) const
{
	return VersionedInstance.Emitter == &InEmitter || (VersionedInstance.GetEmitterData() && VersionedInstance.GetEmitterData()->UsesEmitter(InEmitter));
}

void FNiagaraEmitterHandle::ClearEmitter()
{
	VersionedInstance = FVersionedNiagaraEmitter();
	Instance_DEPRECATED = nullptr;
	Source_DEPRECATED = nullptr;
	LastMergedSource_DEPRECATED = nullptr;
}


#endif
