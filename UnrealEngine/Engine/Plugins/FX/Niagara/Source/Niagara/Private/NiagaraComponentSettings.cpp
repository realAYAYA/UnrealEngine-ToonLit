// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponentSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraComponentSettings)

int32 UNiagaraComponentSettings::bAllowSuppressActivation = 0;
int32 UNiagaraComponentSettings::bAllowForceAutoPooling = 0;
int32 UNiagaraComponentSettings::bUseSuppressEmitterList = 0;
int32 UNiagaraComponentSettings::bUseGpuEmitterAllowList = 0;
int32 UNiagaraComponentSettings::bUseGpuDataInterfaceDenyList = 0;

static FAutoConsoleVariableRef CVarNiagaraUseSupressActivateList(
	TEXT("fx.Niagara.UseSupressActivateList"),
	UNiagaraComponentSettings::bAllowSuppressActivation,
	TEXT("When a component is activated we will check the surpession list."),
	ECVF_Default
);

static FAutoConsoleVariableRef CVarNiagaraForceAutoPooling(
	TEXT("fx.Niagara.ForceAutoPooling"),
	UNiagaraComponentSettings::bAllowForceAutoPooling,
	TEXT("Forces auto pooling to be enabled on spawned components."),
	ECVF_Default
);

static FAutoConsoleVariableRef CVarNiagaraUseEmitterSupressList(
	TEXT("fx.Niagara.UseEmitterSuppressList"),
	UNiagaraComponentSettings::bUseSuppressEmitterList,
	TEXT("When an emitter is activated we will check the surpession list."),
	ECVF_Default
);

static FAutoConsoleVariableRef CVarNiagaraUseGpuEmitterAllowList(
	TEXT("fx.Niagara.UseGpuEmitterAllowList"),
	UNiagaraComponentSettings::bUseGpuEmitterAllowList,
	TEXT("When enabled only GPU emitters on the allow list are allowed to run."),
	ECVF_Default
);

static FAutoConsoleVariableRef CVarNiagaraUseGpuDataInterfaceDenyList(
	TEXT("fx.Niagara.UseGpuDataInterfaceDenyList"),
	UNiagaraComponentSettings::bUseGpuDataInterfaceDenyList,
	TEXT("When enabled GPU emitters will be disabled if they use a data interface on the deny list."),
	ECVF_Default
);

UNiagaraComponentSettings::UNiagaraComponentSettings(FObjectInitializer const& ObjectInitializer)
{
}

