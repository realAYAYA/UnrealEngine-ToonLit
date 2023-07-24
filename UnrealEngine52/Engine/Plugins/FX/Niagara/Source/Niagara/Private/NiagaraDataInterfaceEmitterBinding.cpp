// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceEmitterBinding.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceEmitterBinding)

FNiagaraEmitterInstance* FNiagaraDataInterfaceEmitterBinding::Resolve(FNiagaraSystemInstance* SystemInstance, UNiagaraDataInterface* DataInterface)
{
	if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Self)
	{
		// If we came from a particle script our outer will be a UNiagaraEmitter
		if (UNiagaraEmitter* OwnerEmitter = DataInterface->GetTypedOuter<UNiagaraEmitter>())
		{
			for (TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstance : SystemInstance->GetEmitters())
			{
				if (EmitterInstance->GetCachedEmitter().Emitter == OwnerEmitter)
				{
					return EmitterInstance.Get();
				}
			}
		}
		// If we came from an emitter script we need to look over the default data interfaces to find ourself
		else
		{
			auto FindDataInterfaceName =
				[DataInterface](UNiagaraSystem* NiagaraSystem) -> FName
				{
					for (UNiagaraScript* NiagaraScript : { NiagaraSystem->GetSystemUpdateScript(), NiagaraSystem->GetSystemSpawnScript() })
					{
						if (NiagaraScript != nullptr)
						{
							for (const FNiagaraScriptDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetCachedDefaultDataInterfaces())
							{
								if (DataInterfaceInfo.DataInterface == DataInterface)
								{
									return DataInterfaceInfo.RegisteredParameterMapWrite.IsNone() ? DataInterfaceInfo.Name : DataInterfaceInfo.RegisteredParameterMapWrite;
								}
							}
						}
					}
					return NAME_None;
				};

			const FName DataInterfaceName = FindDataInterfaceName(SystemInstance->GetSystem());
			if (!DataInterfaceName.IsNone())
			{
				FNameBuilder SelfEmitterString;
				DataInterfaceName.ToString(SelfEmitterString);
				FStringView SelfEmitterView = SelfEmitterString.ToView();
				int32 DotIndex;
				if (SelfEmitterView.FindChar('.', DotIndex))
				{
					SelfEmitterView = SelfEmitterView.Mid(0, DotIndex);
					for (TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstance : SystemInstance->GetEmitters())
					{
						if (const UNiagaraEmitter* CachedEmitter = EmitterInstance->GetCachedEmitter().Emitter)
						{
							if (SelfEmitterView.Equals(CachedEmitter->GetUniqueEmitterName()))
							{
								return EmitterInstance.Get();
							}
						}
					}
				}
			}
		}
		if ( FNiagaraUtilities::LogVerboseWarnings() )
		{
			UE_LOG(LogNiagara, Warning, TEXT("EmitterBinding failed to find self emitter"));
		}
	}
	else
	{
		if ( EmitterName.IsNone() )
		{
			if ( FNiagaraUtilities::LogVerboseWarnings() )
			{
				UE_LOG(LogNiagara, Warning, TEXT("EmitterName has not been set but we are in Other mode"));
			}
			return nullptr;
		}

		FNameBuilder EmitterNameString;
		EmitterName.ToString(EmitterNameString);
		FStringView EmitterNameStringView = EmitterNameString.ToView();

		for (TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstance : SystemInstance->GetEmitters())
		{
			if (const UNiagaraEmitter* CachedEmitter = EmitterInstance->GetCachedEmitter().Emitter)
			{
				//-TODO: UniqueEmitterName should probably be a FName?
				if (EmitterNameStringView.Equals(CachedEmitter->GetUniqueEmitterName()) )
				{
					return EmitterInstance.Get();
				}
			}
		}

		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("EmitterBinding failed to find emitter '%s' it might not exist or has been cooked out"), EmitterNameStringView.GetData());
		}
	}
	return nullptr;
}

UNiagaraEmitter* FNiagaraDataInterfaceEmitterBinding::Resolve(UNiagaraSystem* NiagaraSystem)
{
	if ( BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Other )
	{
		if (EmitterName.IsNone())
		{
			return nullptr;
		}

		FNameBuilder EmitterNameString;
		EmitterName.ToString(EmitterNameString);
		FStringView EmitterNameStringView = EmitterNameString.ToView();

		for ( const FNiagaraEmitterHandle& EmitterHandle : NiagaraSystem->GetEmitterHandles() )
		{
			if (UNiagaraEmitter* NiagaraEmitter = EmitterHandle.GetInstance().Emitter)
			{
				//-TODO: UniqueEmitterName should probably be a FName?
				if (EmitterNameStringView.Equals(NiagaraEmitter->GetUniqueEmitterName()))
				{
					return NiagaraEmitter;
				}
			}
		}
	}
	return nullptr;
}

