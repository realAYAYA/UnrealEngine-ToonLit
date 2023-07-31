// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraEmitter.h"
#include "Misc/Guid.h"
#include "NiagaraScript.h"
#include "NiagaraEmitterHandle.generated.h"

class UNiagaraSystem;
class UNiagaraEmitter;

/** 
 * Stores emitter information within the context of a System.
 */
USTRUCT()
struct NIAGARA_API FNiagaraEmitterHandle
{
	GENERATED_USTRUCT_BODY()
public:
	/** Creates a new invalid emitter handle. */
	FNiagaraEmitterHandle();

#if WITH_EDITORONLY_DATA
	/** Creates a new emitter handle from an emitter. */
	FNiagaraEmitterHandle(UNiagaraEmitter& InEmitter, const FGuid& Version);
	
	/** Creates a new emitter handle from an emitter. */
	FNiagaraEmitterHandle(const FVersionedNiagaraEmitter& InEmitter);
#endif

	/** Whether or not this is a valid emitter handle. */
	bool IsValid() const;

	/** Gets the unique id for this handle. */
	FGuid GetId() const;

	// HACK!  Data sets used to use the emitter name, but this isn't guaranteed to be unique.  This is a temporary hack
	// to allow the data sets to continue work with using names, but that code needs to be refactored to use the id defined here.
	FName GetIdName() const;

	/** Gets the display name for this emitter in the System. */
	FName GetName() const;

	/** Sets the display name for this emitter in the System. The system is needed here in order to ensure uniqueness of the name. */
	void SetName(FName InName, UNiagaraSystem& InOwnerSystem);

	/** Gets whether or not this emitter is enabled within the System.  Disabled emitters aren't simulated. */
	bool GetIsEnabled() const;

	/** Sets whether or not this emitter is enabled within the System.  Disabled emitters aren't simulated. Returns whether or not the enabled state changed. */
	bool SetIsEnabled(bool bInIsEnabled, UNiagaraSystem& InOwnerSystem, bool bRecompileIfChanged);

#if WITH_EDITORONLY_DATA
	bool IsIsolated() const {	return bIsolated; }
	void SetIsolated(bool bInIsolated) { bIsolated = bInIsolated; }
#endif

	/** Gets the copied instance of the emitter this handle references. */
	FVersionedNiagaraEmitter GetInstance() const;
	FVersionedNiagaraEmitter& GetInstance();

	FVersionedNiagaraEmitterData* GetEmitterData() const;

	/** Gets a unique name for this emitter instance for use in scripts and parameter stores etc.*/
	FString GetUniqueInstanceName()const;

#if WITH_EDITORONLY_DATA
	/** Determine whether or not the Instance script is in synch with its graph.*/
	bool NeedsRecompile() const;

	/** Calls conditional post load on all sub-objects this handle references. */
	void ConditionalPostLoad(int32 NiagaraCustomVersion);

	/** Whether or not this handle uses the supplied emitter. */
	bool UsesEmitter(const FVersionedNiagaraEmitter& InEmitter) const;
	bool UsesEmitter(const UNiagaraEmitter& InEmitter) const;

	void ClearEmitter();

#endif
public:
	/** A static const invalid handle. */
	static const FNiagaraEmitterHandle InvalidHandle;

private:
	/** The id of this emitter handle. */
	UPROPERTY(VisibleAnywhere, Category="Emitter ID")
	FGuid Id;

	// HACK!  Data sets used to use the emitter name, but this isn't guaranteed to be unique.  This is a temporary hack
	// to allow the data sets to continue work with using names, but that code needs to be refactored to use the id defined here.
	UPROPERTY(VisibleAnywhere, Category = "Emitter ID")
	FName IdName;

	/** Whether or not this emitter is enabled within the System.  Disabled emitters aren't simulated. */
	UPROPERTY()
	bool bIsEnabled;
	
	/** The display name for this emitter in the System. */
	UPROPERTY()
	FName Name;

#if WITH_EDITORONLY_DATA
	/** The source emitter this emitter handle was built from. */
	UPROPERTY()
	TObjectPtr<UNiagaraEmitter> Source_DEPRECATED;

	/** An unmodified copy of the emitter this handle references for use when merging change from the source emitter. */
	UPROPERTY()
	TObjectPtr<UNiagaraEmitter> LastMergedSource_DEPRECATED;

	UPROPERTY(Transient)
	bool bIsolated;

#endif

	/** The copied instance of the emitter this handle references. */
	UPROPERTY()
	TObjectPtr<UNiagaraEmitter> Instance_DEPRECATED;

	/** The copied instance of the emitter this handle references. */
	UPROPERTY()
	FVersionedNiagaraEmitter VersionedInstance;
};
