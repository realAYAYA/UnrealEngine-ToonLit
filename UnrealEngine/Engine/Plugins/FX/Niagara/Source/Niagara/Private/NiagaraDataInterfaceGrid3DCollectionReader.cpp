// Copyright Epic Games, Inc. All Rights Reserved.
#include "NiagaraDataInterfaceGrid3DCollectionReader.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "NiagaraSystemInstance.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "NiagaraSettings.h"
#include "NiagaraConstants.h"
#include "NiagaraComputeExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceGrid3DCollectionReader)


#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceGrid3DCollectionReader"

UNiagaraDataInterfaceGrid3DCollectionReader::UNiagaraDataInterfaceGrid3DCollectionReader(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNiagaraDataInterfaceProxyGrid3DCollectionProxy());

	FNiagaraTypeDefinition Def(UTextureRenderTarget::StaticClass());
	RenderTargetUserParameter.Parameter.SetType(Def);
}

bool UNiagaraDataInterfaceGrid3DCollectionReader::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceGrid3DCollectionReader* OtherTyped = CastChecked<const UNiagaraDataInterfaceGrid3DCollectionReader>(Other);

	return OtherTyped != nullptr && OtherTyped->EmitterName == EmitterName && OtherTyped->DIName == DIName;
}

bool UNiagaraDataInterfaceGrid3DCollectionReader::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceGrid3DCollectionReader* OtherTyped = CastChecked<UNiagaraDataInterfaceGrid3DCollectionReader>(Destination);
	OtherTyped->EmitterName = EmitterName;
	OtherTyped->DIName = DIName;
	return true;
}

bool UNiagaraDataInterfaceGrid3DCollectionReader::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	check(Proxy);
	
	FGrid3DCollectionRWInstanceData_GameThread* InstanceData = new (PerInstanceData) FGrid3DCollectionRWInstanceData_GameThread();
	SystemInstancesToProxyData_GT.Emplace(SystemInstance->GetId(), InstanceData);

	TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> RT_EmitterInstance;

	
	FNiagaraEmitterInstance* EmitterInstanceToUse = nullptr;
	for (TSharedPtr<FNiagaraEmitterInstance, ESPMode::ThreadSafe> EmitterInstance : SystemInstance->GetEmitters())
	{
		UNiagaraEmitter* Emitter = EmitterInstance->GetCachedEmitter().Emitter;
		if (Emitter == nullptr)
		{
			continue;
		}

		if (EmitterName == Emitter->GetUniqueEmitterName())
		{
			EmitterInstanceToUse = EmitterInstance.Get();
			break;
		}
	}

	// Look for proxy we are going to use
	FNiagaraDataInterfaceProxy* ProxyToUse = nullptr;
	UNiagaraDataInterfaceGrid3DCollection* GridInterface = nullptr;

	if (EmitterInstanceToUse != nullptr)
	{
		FNiagaraComputeExecutionContext* ExecContext = EmitterInstanceToUse->GetGPUContext();
		if ((ExecContext != nullptr) && (ExecContext->GPUScript != nullptr))
		{
			const TArray<FNiagaraScriptDataInterfaceCompileInfo>& DataInterfaceInfo = ExecContext->GPUScript->GetVMExecutableData().DataInterfaceInfo;
			const TArray<UNiagaraDataInterface*>& DataInterfaces = ExecContext->CombinedParamStore.GetDataInterfaces();

			FString FullName = FString("Emitter.") + DIName;
			int Index = 0;

			// #todo(dmp): we are looking at the UObjects that define the DIs here 
			
			for (UNiagaraDataInterface* Interface : DataInterfaces)
			{
				if (DataInterfaceInfo[Index].Name.GetPlainNameString() == FullName)
				{
					GridInterface = static_cast<UNiagaraDataInterfaceGrid3DCollection*>(Interface);
					ProxyToUse = Interface->GetProxy();
					break;
				}
				++Index;
			}
		}

		if (GridInterface && ProxyToUse)
		{

			FGrid3DCollectionRWInstanceData_GameThread* Grid3DInstanceData = GridInterface->GetSystemInstancesToProxyData_GT().FindRef(SystemInstance->GetId());
			InstanceData->OtherDI = GridInterface;
			InstanceData->OtherInstanceData = Grid3DInstanceData;

			// Push Updates to Proxy.
			FNiagaraDataInterfaceProxyGrid3DCollectionProxy* RT_Proxy = GetProxyAs<FNiagaraDataInterfaceProxyGrid3DCollectionProxy>();
			ENQUEUE_RENDER_COMMAND(FUpdateData)(
				[RT_Proxy, InstanceID = SystemInstance->GetId(), RT_ProxyToUse = ProxyToUse](FRHICommandListImmediate& RHICmdList)
			{
				check(!RT_Proxy->SystemInstancesToProxyData_RT.Contains(InstanceID));
				//check(RT_ProxyToUse->SystemInstancesToProxyData_RT.Contains(InstanceID));
				//FGrid3DCollectionRWInstanceData_RenderThread* DataToRead = RT_ProxyToUse->SystemInstancesToProxyData_RT.Find(InstanceID);

				FGrid3DCollectionRWInstanceData_RenderThread* TargetData = &RT_Proxy->SystemInstancesToProxyData_RT.Add(InstanceID);
				TargetData->OtherProxy = RT_ProxyToUse;
			});
			return true;
		}
	}

	return false;
}

void UNiagaraDataInterfaceGrid3DCollectionReader::GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies) const
{
	if (!Asset)
	{
		return;
	}

	for (const FNiagaraEmitterHandle& EmitterHandle : Asset->GetEmitterHandles())
	{
		UNiagaraEmitter* EmitterInstance = EmitterHandle.GetInstance().Emitter;
		if (EmitterInstance && EmitterInstance->GetUniqueEmitterName() == EmitterName)
		{
			Dependencies.Add(EmitterHandle.GetInstance());
			return;
		}
	}
}

void UNiagaraDataInterfaceGrid3DCollectionReader::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	int32 NumFunctionsBefore = OutFunctions.Num();

	Super::GetFunctions(OutFunctions);
	TSet<FName> FunctionsToRemove = 
	{
		ClearCellFunctionName,
		CopyPreviousToCurrentForCellFunctionName,
		CopyMaskedPreviousToCurrentForCellFunctionName,
		SetValueFunctionName,
		SetFullGridValueFunctionName,
		SetVector4ValueFunctionName,
		SetVector3ValueFunctionName,
		SetVector2ValueFunctionName,
		SetFloatValueFunctionName,
		GetPreviousVector4ValueFunctionName,
		SetVectorValueFunctionName,
		SetVector2DValueFunctionName,
		SetNumCellsFunctionName,
		SetNumCellsFunctionName
	};
	
	for (int32 ItemIndex = NumFunctionsBefore; ItemIndex < OutFunctions.Num();)
	{
		if (FunctionsToRemove.Find(OutFunctions[ItemIndex].Name) != nullptr)
		{
			OutFunctions.RemoveAtSwap(ItemIndex, 1);
		}
		else
		{
			++ItemIndex;
		}
	}	
}

bool UNiagaraDataInterfaceGrid3DCollectionReader::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	FGrid3DCollectionRWInstanceData_GameThread* InstanceData = SystemInstancesToProxyData_GT.FindRef(SystemInstance->GetId());
	
	// we can run into the case where depending on the ordering of DI initialization, we might have not been able to grab the other grid's InstanceData
	// in InitPerInstanceData.  If this is the case, we ensure it is correct here.
	if (InstanceData->OtherInstanceData == nullptr && InstanceData->OtherDI)
	{
		UNiagaraDataInterfaceGrid3DCollection*  OtherGridDI = static_cast<UNiagaraDataInterfaceGrid3DCollection*>(InstanceData->OtherDI);
		FGrid3DCollectionRWInstanceData_GameThread* Grid3DInstanceData = OtherGridDI->GetSystemInstancesToProxyData_GT().FindRef(SystemInstance->GetId());

		InstanceData->OtherInstanceData = Grid3DInstanceData;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

