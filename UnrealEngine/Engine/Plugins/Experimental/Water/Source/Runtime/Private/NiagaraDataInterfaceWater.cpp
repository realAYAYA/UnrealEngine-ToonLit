// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceWater.h"

#include "NiagaraSystemInstance.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"
#include "WaterModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceWater)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceWater"


namespace WaterFunctionNames
{
	const FName GetWaterDataAtPointName(TEXT("GetWaterDataAtPoint"));

	const FName GetWaveParamLookupTableName(TEXT("GetWaveParamLookupTableOffset"));
}

struct FNiagaraWaterDIFunctionVersion
{
	enum Type
	{
		InitialVersion = 0,
		LargeWorldCoordinates = 1,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
};

void UNiagaraDataInterfaceWater::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceWater::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = WaterFunctionNames::GetWaterDataAtPointName;

		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Water")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("WorldPosition")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("WaveHeight")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Depth")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("SurfacePosition")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SurfaceNormal")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bExperimental = true;
		Sig.SetDescription(LOCTEXT("DataInterfaceWater_GetWaterDataAtPoint", "Get the water data at the provided world position and time"));
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = WaterFunctionNames::GetWaveParamLookupTableName;

		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Water")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Offset")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.bExperimental = true;
		Sig.SetDescription(LOCTEXT("DataInterfaceWater_GetWaveParamLookupTableOffset", "Get the lookup table offset into the wave data texture for the data interface's water body"));
		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceWater, GetWaterDataAtPoint);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceWater, GetWaveParamLookupTableOffset);

struct FNDIWater_InstanceData
{
	//Cached ptr to component we sample from. 
	TWeakObjectPtr<UWaterBodyComponent> WaterBodyComponent;
	FNiagaraLWCConverter LWCConverter;
};

void UNiagaraDataInterfaceWater::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == WaterFunctionNames::GetWaterDataAtPointName)
	{
		if(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 11)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceWater, GetWaterDataAtPoint)::Bind(this, OutFunc);
		}
	}
	else if (BindingInfo.Name == WaterFunctionNames::GetWaveParamLookupTableName)
	{
		if (BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1)
		{
			NDI_FUNC_BINDER(UNiagaraDataInterfaceWater, GetWaveParamLookupTableOffset)::Bind(this, OutFunc);
		}
	}
}

bool UNiagaraDataInterfaceWater::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceWater* OtherTyped = CastChecked<const UNiagaraDataInterfaceWater>(Other);
	return OtherTyped->SourceBodyComponent == SourceBodyComponent;
}

bool UNiagaraDataInterfaceWater::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceWater* OtherTyped = CastChecked<UNiagaraDataInterfaceWater>(Destination);
	OtherTyped->SourceBodyComponent = SourceBodyComponent;

	return true;
}

int32 UNiagaraDataInterfaceWater::PerInstanceDataSize() const
{
	return sizeof(FNDIWater_InstanceData);
}

bool UNiagaraDataInterfaceWater::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIWater_InstanceData* InstData = new (PerInstanceData) FNDIWater_InstanceData();

	InstData->WaterBodyComponent = SourceBodyComponent;
	InstData->LWCConverter = SystemInstance->GetLWCConverter();

	return true;
}

void UNiagaraDataInterfaceWater::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIWater_InstanceData* InstData = static_cast<FNDIWater_InstanceData*>(PerInstanceData);
	InstData->~FNDIWater_InstanceData();
}

bool UNiagaraDataInterfaceWater::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	check(SystemInstance);
	FNDIWater_InstanceData* InstData = static_cast<FNDIWater_InstanceData*>(PerInstanceData);

	if (InstData->WaterBodyComponent != SourceBodyComponent)
	{
		InstData->WaterBodyComponent = SourceBodyComponent;
	}

	return false;
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceWater::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	bool bChanged = false;
	
	// upgrade from lwc changes, only parameter types changed there
	if (FunctionSignature.FunctionVersion < FNiagaraWaterDIFunctionVersion::LargeWorldCoordinates)
	{
		if (FunctionSignature.Name == WaterFunctionNames::GetWaterDataAtPointName && ensure(FunctionSignature.Inputs.Num() == 3) && ensure(FunctionSignature.Outputs.Num() == 5))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[3].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
	}
	FunctionSignature.FunctionVersion = FNiagaraWaterDIFunctionVersion::LatestVersion;

	return bChanged;
}
#endif

void UNiagaraDataInterfaceWater::GetWaterDataAtPoint(FVectorVMExternalFunctionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(NiagaraDataInterfaceWater_GetWaterDataAtPoint);

	VectorVM::FUserPtrHandler<FNDIWater_InstanceData> InstData(Context);
	
	// Inputs
	FNDIInputParam<FNiagaraPosition> WorldPos(Context);
	FNDIInputParam<float> Time(Context);

	// Outputs
	FNDIOutputParam<float> OutHeight(Context);
	FNDIOutputParam<float> OutDepth(Context);
	FNDIOutputParam<FVector3f> OutVelocity(Context);
	FNDIOutputParam<FNiagaraPosition> OutSurfacePos(Context);
	FNDIOutputParam<FVector3f> OutSurfaceNormal(Context);

	UWaterBodyComponent* Component = InstData->WaterBodyComponent.Get();
	if (Component == nullptr)
	{
		UE_LOG(LogWater, Warning, TEXT("NiagaraDataInterfaceWater: GetWaterData called with no water body component set"));
	}

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		FWaterBodyQueryResult QueryResult;
		
		bool bIsValid = false;
		if (Component != nullptr)
		{
			FVector QueryPos = InstData->LWCConverter.ConvertSimulationPositionToWorld(WorldPos.GetAndAdvance());
			QueryResult = Component->QueryWaterInfoClosestToWorldLocation(QueryPos,
				EWaterBodyQueryFlags::ComputeLocation
				| EWaterBodyQueryFlags::ComputeVelocity
				| EWaterBodyQueryFlags::ComputeNormal
				| EWaterBodyQueryFlags::ComputeDepth
				| EWaterBodyQueryFlags::IncludeWaves);
			bIsValid = !QueryResult.IsInExclusionVolume();
		}

		OutHeight.SetAndAdvance(bIsValid ? QueryResult.GetWaveInfo().Height : 0.0f);
		OutDepth.SetAndAdvance(bIsValid ? QueryResult.GetWaterSurfaceDepth() : 0.0f);
		OutVelocity.SetAndAdvance( bIsValid ? FVector3f(QueryResult.GetVelocity()) : FVector3f::ZeroVector);		// LWC_TODO: Precision loss

		// Note we assume X and Y are in water by the time this is queried
		const FVector& AdjustedSurfaceLoc = bIsValid ? QueryResult.GetWaterSurfaceLocation() : FVector::ZeroVector;
		OutSurfacePos.SetAndAdvance( InstData->LWCConverter.ConvertWorldToSimulationPosition(AdjustedSurfaceLoc));

		OutSurfaceNormal.SetAndAdvance(bIsValid ? FVector3f(QueryResult.GetWaterSurfaceNormal()) : FVector3f::UpVector);

		Time.GetAndAdvance();
	}
}

void UNiagaraDataInterfaceWater::GetWaveParamLookupTableOffset(FVectorVMExternalFunctionContext& Context)
{
	// Inputs
	VectorVM::FUserPtrHandler<FNDIWater_InstanceData> InstData(Context);

	// Outputs
	VectorVM::FExternalFuncRegisterHandler<int> OutLookupTableOffset(Context);
	if (UWaterBodyComponent* Component = InstData->WaterBodyComponent.Get())
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			*OutLookupTableOffset.GetDestAndAdvance() = Component->GetWaterBodyIndex();
		}
	}
	else
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			*OutLookupTableOffset.GetDestAndAdvance() = 0;
		}
	}
}

#undef LOCTEXT_NAMESPACE

