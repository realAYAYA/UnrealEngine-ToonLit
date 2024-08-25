// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceEmitterBinding.h"
#include "NiagaraConstants.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceEmitterBinding)

FNiagaraEmitterInstance* FNiagaraDataInterfaceEmitterBinding::Resolve(const FNiagaraSystemInstance* SystemInstance, const UNiagaraDataInterface* DataInterface) const
{
	if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Self)
	{
		// If we came from a particle script our outer will be a UNiagaraEmitter
		if (UNiagaraEmitter* OwnerEmitter = DataInterface->GetTypedOuter<UNiagaraEmitter>())
		{
			for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
			{
				if (EmitterInstance->GetEmitter() == OwnerEmitter)
				{
					return &EmitterInstance.Get();
				}
			}
		}
		// If we came from an emitter script we need to look over the default data interfaces to find ourself
		else
		{
			FString SourceEmitterName;
			UNiagaraSystem* NiagaraSystem = SystemInstance->GetSystem();
			for (UNiagaraScript* NiagaraScript : { NiagaraSystem->GetSystemUpdateScript(), NiagaraSystem->GetSystemSpawnScript() })
			{
				if (NiagaraScript != nullptr)
				{
					const FNiagaraScriptResolvedDataInterfaceInfo* MatchingResolvedDataInterface = NiagaraScript->GetResolvedDataInterfaces().FindByPredicate(
						[&DataInterface](const FNiagaraScriptResolvedDataInterfaceInfo& ResolvedDataInterface) { return ResolvedDataInterface.ResolvedDataInterface == DataInterface; });
					if (MatchingResolvedDataInterface != nullptr)
					{
						SourceEmitterName = MatchingResolvedDataInterface->ResolvedSourceEmitterName;
						break;
					}
				}
			}

			if (SourceEmitterName.IsEmpty() == false)
			{
				for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
				{
					if (const UNiagaraEmitter* CachedEmitter = EmitterInstance->GetEmitter())
					{
						if (CachedEmitter->GetUniqueEmitterName() == SourceEmitterName)
						{
							return &EmitterInstance.Get();
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

		for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
		{
			if (const UNiagaraEmitter* CachedEmitter = EmitterInstance->GetEmitter())
			{
				//-TODO: UniqueEmitterName should probably be a FName?
				if (EmitterNameStringView.Equals(CachedEmitter->GetUniqueEmitterName(), ESearchCase::IgnoreCase) )
				{
					return &EmitterInstance.Get();
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

const FNiagaraEmitterHandle* FNiagaraDataInterfaceEmitterBinding::ResolveHandle(const UNiagaraDataInterface* DataInterface) const
{
	UNiagaraSystem* OwnerSystem = DataInterface->GetTypedOuter<UNiagaraSystem>();
	if (OwnerSystem == nullptr)
	{
		return nullptr;
	}

	if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Self)
	{
		if (UNiagaraEmitter* OwnerEmitter = DataInterface->GetTypedOuter<UNiagaraEmitter>())
		{
			for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
			{
				if (OwnerEmitter == EmitterHandle.GetInstance().Emitter)
				{
					return &EmitterHandle;
				}
			}
		}
		else
		{
			for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
			{
				FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
				if (EmitterHandle.GetIsEnabled() && EmitterData)
				{
					bool bFoundDI = false;
					bool bIsEmitterNamespace = false;
					EmitterData->ForEachScript(
						[DataInterface, &bFoundDI, &bIsEmitterNamespace](const UNiagaraScript* Script)
						{
							for (const FNiagaraScriptResolvedDataInterfaceInfo& ResolvedDI : Script->GetResolvedDataInterfaces())
							{
								if (ResolvedDI.ResolvedDataInterface == DataInterface)
								{
									bFoundDI = true;
									bIsEmitterNamespace = FNiagaraVariableBase::IsInNameSpace(FNiagaraConstants::EmitterNamespaceString, ResolvedDI.CompileName);
									return;
								}
							}
						}
					);

					if (bFoundDI)
					{
						return bIsEmitterNamespace ? &EmitterHandle : nullptr;
					}
				}
			}
		}
	}
	else if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Other)
	{
		if (!EmitterName.IsNone())
		{
			FNameBuilder EmitterNameString;
			EmitterName.ToString(EmitterNameString);
			FStringView EmitterNameStringView = EmitterNameString.ToView();

			for (const FNiagaraEmitterHandle& EmitterHandle : OwnerSystem->GetEmitterHandles())
			{
				if (UNiagaraEmitter* NiagaraEmitter = EmitterHandle.GetInstance().Emitter)
				{
					//-TODO: UniqueEmitterName should probably be a FName?
					if (EmitterNameStringView.Equals(NiagaraEmitter->GetUniqueEmitterName(), ESearchCase::IgnoreCase))
					{
						return &EmitterHandle;
					}
				}
			}
		}
	}
	return nullptr;
}

UNiagaraEmitter* FNiagaraDataInterfaceEmitterBinding::Resolve(const UNiagaraDataInterface* DataInterface) const
{
	const FNiagaraEmitterHandle* EmitterHandle = ResolveHandle(DataInterface);
	return EmitterHandle ? EmitterHandle->GetInstance().Emitter : nullptr;
}

FString FNiagaraDataInterfaceEmitterBinding::ResolveUniqueName(const UNiagaraDataInterface* DataInterface) const
{
	if (UNiagaraEmitter* ResolvedEmitter = Resolve(DataInterface))
	{
		return ResolvedEmitter->GetUniqueEmitterName();
	}
	else
	{
		if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Self)
		{
			return FString(TEXT("Self"));
		}
		else if (BindingMode == ENiagaraDataInterfaceEmitterBindingMode::Other)
		{
			return EmitterName.ToString();
		}
	}
	return FString();
}
