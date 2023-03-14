// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterInstance.h"

#include "NiagaraComponentSettings.generated.h"

USTRUCT(meta = (DisplayName = "Emitter Name Settings Reference"))
struct FNiagaraEmitterNameSettingsRef
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)
	FName SystemName;

	UPROPERTY(EditAnywhere, Category = Parameters)
	FString EmitterName;

	FORCEINLINE bool operator==(const FNiagaraEmitterNameSettingsRef& Other)const
	{
		return SystemName == Other.SystemName && EmitterName == Other.EmitterName;
	}

	FORCEINLINE bool operator!=(const FNiagaraEmitterNameSettingsRef& Other)const
	{
		return !(*this == Other);
	}
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraEmitterNameSettingsRef& Var)
{
	return HashCombine(GetTypeHash(Var.SystemName), GetTypeHash(Var.EmitterName));
}

UCLASS(config=Game, defaultconfig)
class NIAGARA_API UNiagaraComponentSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	FORCEINLINE static bool ShouldSuppressActivation(const UNiagaraSystem* System)
	{
		check(System != nullptr);
		if ( bAllowSuppressActivation )
		{
			if (const UNiagaraComponentSettings* ComponentSettings = GetDefault<UNiagaraComponentSettings>())
			{
				if (ComponentSettings->SuppressActivationList.Contains(System->GetFName()))
				{
					return true;
				}
			}
		}
		return false;
	}

	FORCEINLINE static bool ShouldForceAutoPooling(const UNiagaraSystem* System)
	{
		check(System != nullptr);
		if (bAllowForceAutoPooling)
		{
			if (const UNiagaraComponentSettings* ComponentSettings = GetDefault<UNiagaraComponentSettings>())
			{
				if (ComponentSettings->ForceAutoPooolingList.Contains(System->GetFName()))
				{
					return true;
				}
			}
		}
		return false;
	}

	FORCEINLINE static bool ShouldSuppressEmitterActivation(const FNiagaraEmitterInstance* EmitterInstance)
	{
		if ( bUseSuppressEmitterList || bUseGpuEmitterAllowList || bUseGpuDataInterfaceDenyList)
		{
			const UNiagaraComponentSettings* ComponentSettings = GetDefault<UNiagaraComponentSettings>();

			FNiagaraEmitterNameSettingsRef EmitterRef;
			if (const UNiagaraSystem* ParentSystem = EmitterInstance->GetParentSystemInstance()->GetSystem())
			{
				EmitterRef.SystemName = ParentSystem->GetFName();
			}
			FVersionedNiagaraEmitter CachedEmitter = EmitterInstance->GetCachedEmitter();
			EmitterRef.EmitterName = CachedEmitter.Emitter->GetUniqueEmitterName();

			if (bUseSuppressEmitterList && ComponentSettings->SuppressEmitterList.Contains(EmitterRef))
			{
				return true;
			}

			FVersionedNiagaraEmitterData* EmitterData = CachedEmitter.GetEmitterData();
			if (bUseGpuEmitterAllowList && (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim) && !ComponentSettings->GPUEmitterAllowList.Contains(EmitterRef))
			{
				return true;
			}

			if ( bUseGpuDataInterfaceDenyList && (EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim) )
			{
				if (const UNiagaraScript* GPUComputeScript = EmitterData->GetGPUComputeScript())
				{
					for (const FNiagaraScriptDataInterfaceInfo& DefaultDIInfo : GPUComputeScript->GetCachedDefaultDataInterfaces())
					{
						if (ComponentSettings->GpuDataInterfaceDenyList.Contains(DefaultDIInfo.Type.GetFName()))
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	UPROPERTY(config)
	TSet<FName> SuppressActivationList;

	UPROPERTY(config)
	TSet<FName> ForceAutoPooolingList;

	/** 
		Config file to tweak individual emitters being disabled. Syntax is as follows for the config file:
		[/Script/Niagara.NiagaraComponentSettings]
		SuppressEmitterList=((SystemName="BasicSpriteSystem",EmitterName="BasicSprite001"))
		+GpuDataInterfaceDenyList=("NiagaraDataInterfaceCollisionQuery")
	*/
	UPROPERTY(config)
	TSet<FNiagaraEmitterNameSettingsRef> SuppressEmitterList;

	UPROPERTY(config)
	TSet<FNiagaraEmitterNameSettingsRef> GPUEmitterAllowList;

	UPROPERTY(config)
	TSet<FName> GpuDataInterfaceDenyList;

	static int32 bAllowSuppressActivation;
	static int32 bAllowForceAutoPooling;
	static int32 bUseSuppressEmitterList;
	static int32 bUseGpuEmitterAllowList;
	static int32 bUseGpuDataInterfaceDenyList;
};
