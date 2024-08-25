// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserViewExtension.h"
#include "NNE.h"
#include "NNEDenoiserGenericDenoiser.h"
#include "NNEDenoiserIOProcessBase.h"
#include "NNEDenoiserIOProcessOidn.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserModelData.h"
#include "NNEDenoiserModelInstanceCPU.h"
#include "NNEDenoiserModelInstanceGPU.h"
#include "NNEDenoiserModelInstanceRDG.h"
#include "NNEDenoiserModelIOMappingData.h"
#include "NNEDenoiserPathTracingDenoiser.h"
#include "NNEDenoiserPathTracingSpatialTemporalDenoiser.h"
#include "NNEModelData.h"
#include "PathTracingDenoiser.h"

namespace UE::NNEDenoiser::Private
{

static TAutoConsoleVariable<bool> CVarNNEDenoiser(
	TEXT("NNEDenoiser"),
	true,
	TEXT("Enable the NNE Denoiser.")
);

static TAutoConsoleVariable<int32> CVarNNEDenoiserModelData(
	TEXT("NNEDenoiser.ModelData"),
	0,
	TEXT("Defines the model data of the denoiser model.\n")
	TEXT("  0: Use model data from Project Settings\n")
	TEXT("  1: OIDN Color\n")
	TEXT("  2: OIDN Color and Albedo\n")
	TEXT("  3: OIDN Color, Albedo and Normal\n")
	TEXT("  4: OIDN Color, Half\n")
	TEXT("  5: OIDN Color and Albedo, Half\n")
	TEXT("  6: OIDN Color, Albedo and Normal, Half")
);

static TAutoConsoleVariable<int32> CVarNNEDenoiserRuntimeType(
	TEXT("NNEDenoiser.Runtime.Type"),
	1,
	TEXT("Defines the runtime type to run the denoiser model.\n")
	TEXT("  0: CPU\n")
	TEXT("  1: GPU\n")
	TEXT("  2: RDG\n")
);

static TAutoConsoleVariable<FString> CVarNNEDenoiserRuntimeName(
	TEXT("NNEDenoiser.Runtime.Name"),
	FString(),
	TEXT("Defines the runtime name to run the denoiser model. Leave empty to use default.")
);

EDenoiserRuntimeType GetDenoiserRuntimeTypeFromCVar()
{
	const int32 Value = CVarNNEDenoiserRuntimeType.GetValueOnGameThread();
	const int32 Min = (int32)EDenoiserRuntimeType::CPU;
	const int32 Max = (int32)EDenoiserRuntimeType::RDG;
	return static_cast<EDenoiserRuntimeType>(FMath::Clamp(Value, Min, Max));
}

FString GetDenoiserModelDataNameFromCVarAndSettings(const UNNEDenoiserSettings* Settings)
{
	const int32 Idx = CVarNNEDenoiserModelData.GetValueOnGameThread();
	switch(Idx)
	{
		case 0: return !Settings->DenoiserModelData.IsNull() ? Settings->DenoiserModelData.ToString() : FString();
		case 1: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_Color_720.NNEDNN_Oidn2_Color_720");
		case 2: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_ColorAlbedo_720.NNEDNN_Oidn2_ColorAlbedo_720");
		case 3: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_ColorAlbedoNormal_720.NNEDNN_Oidn2_ColorAlbedoNormal_720");
		case 4: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_Color_720_Float16.NNEDNN_Oidn2_Color_720_Float16");
		case 5: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_ColorAlbedo_720_Float16.NNEDNN_Oidn2_ColorAlbedo_720_Float16");
		case 6: return TEXT("/NNEDenoiser/NNEDNN_Oidn2_ColorAlbedoNormal_720_Float16.NNEDNN_Oidn2_ColorAlbedoNormal_720_Float16");
	}
	check(false);
	return FString();
}

TUniquePtr<FGenericDenoiser> CreateNNEDenoiser(UNNEModelData& ModelData, EDenoiserRuntimeType RuntimeType, const FString& RuntimeNameOverride,
	TUniquePtr<IInputProcess> InputProcess, TUniquePtr<IOutputProcess> OutputProcess, FParameters Parameters)
{
	TUniquePtr<IModelInstance> ModelInstance;
	if (RuntimeType == EDenoiserRuntimeType::CPU)
	{
		ModelInstance = FModelInstanceCPU::Make(ModelData, RuntimeNameOverride);
	}
	else if (RuntimeType == EDenoiserRuntimeType::GPU)
	{
		ModelInstance = FModelInstanceGPU::Make(ModelData, RuntimeNameOverride);
	}
	else if (RuntimeType == EDenoiserRuntimeType::RDG)
	{
		ModelInstance = FModelInstanceRDG::Make(ModelData, RuntimeNameOverride);
	}

	if (!ModelInstance.IsValid())
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create denoiser!"));
		return {};
	}

	return MakeUnique<FGenericDenoiser>(MoveTemp(ModelInstance), MoveTemp(InputProcess), MoveTemp(OutputProcess), MoveTemp(Parameters));
}

static FResourceMappingList MakeTensorLayout(UDataTable* DataTable)
{
	TMap<int32, TMap<int32, FResourceInfo>> Map;
	DataTable->ForeachRow<FNNEDenoiserModelIOMappingData>("FResourceLayout", [&] (const FName &Key, const FNNEDenoiserModelIOMappingData &Value)
	{
		if (Value.TensorChannel < 0)
		{
			for (int32 I = 0; I < -Value.TensorChannel; I++)
			{
				Map.FindOrAdd(Value.TensorIndex).FindOrAdd(I) = FResourceInfo{Value.Resource, I, Value.FrameIndex};
			}
		}
		else
		{
			Map.FindOrAdd(Value.TensorIndex).FindOrAdd(Value.TensorChannel) = FResourceInfo{Value.Resource, Value.ResourceChannel, Value.FrameIndex};
		}
	});

	FResourceMappingList Result{};

	for (int32 I = 0; I < Map.Num(); I++)
	{
		checkf(Map.Contains(I), TEXT("Missing intput/output %d, must be continuous!"), I);

		const TMap<int32, FResourceInfo>& InnerMap = Map[I];
		FResourceMapping& Mapping = Result.Add_GetRef({});

		for (int32 J = 0; J < InnerMap.Num(); J++)
		{
			checkf(InnerMap.Contains(J), TEXT("Missing tensor info for channel %d, must be continuous!"), J);

			Mapping.Add(InnerMap[J]);
		}
	}

	return Result;
}

TUniquePtr<FGenericDenoiser> CreateNNEDenoiserFromAsset(const FString& AssetName, EDenoiserRuntimeType RuntimeType, const FString& RuntimeNameOverride)
{
	if (AssetName.IsEmpty())
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Asset name not set!"));
		return {};
	}

	UNNEDenoiserModelData *DenoiserModelData = LoadObject<UNNEDenoiserModelData>(nullptr, *AssetName);
	if (!DenoiserModelData)
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Could not load denoiser model data asset!"));
		return {};
	}

	UNNEModelData* ModelData = DenoiserModelData->ModelData.LoadSynchronous();
	if (!ModelData)
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Asset does not contain model data!"));
		return {};
	}
	UDataTable* InputMappingTable = DenoiserModelData->InputMapping.LoadSynchronous();
	if (InputMappingTable)
	{
		UE_LOG(LogNNEDenoiser, Log, TEXT("Loaded input mapping from %s"), *DenoiserModelData->InputMapping.GetAssetName());
	}
	UDataTable* OutputMappingTable = DenoiserModelData->OutputMapping.LoadSynchronous();
	if (OutputMappingTable)
	{
		UE_LOG(LogNNEDenoiser, Log, TEXT("Loaded output mapping from %s"), *DenoiserModelData->OutputMapping.GetAssetName());
	}

	FResourceMappingList InputLayout;
	if (InputMappingTable)
	{
		InputLayout = MakeTensorLayout(InputMappingTable);
	}

	FResourceMappingList OutputLayout;
	if (OutputMappingTable)
	{
		OutputLayout = MakeTensorLayout(OutputMappingTable);
	}

	const bool bIsOidnModel = AssetName.Contains(TEXT("oidn2"));
	const bool bIsInHouseModel = AssetName.Contains(TEXT("kpcn"));

	check(!bIsOidnModel || !bIsInHouseModel);

	TUniquePtr<IInputProcess> InputProcess;
	if (bIsOidnModel)
	{
		InputProcess = MakeUnique<FInputProcessOidn>(MoveTemp(InputLayout));
	}
	else
	{
		InputProcess = MakeUnique<FInputProcessBase>(MoveTemp(InputLayout));
	}

	TUniquePtr<IOutputProcess> OutputProcess;
	if (bIsOidnModel)
	{
		OutputProcess = MakeUnique<FOutputProcessOidn>(MoveTemp(OutputLayout));
	}
	else
	{
		OutputProcess = MakeUnique<FOutputProcessBase>(MoveTemp(OutputLayout));
	}

	return CreateNNEDenoiser(*ModelData, RuntimeType, RuntimeNameOverride, MoveTemp(InputProcess), MoveTemp(OutputProcess), {});
}

FViewExtension::FViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
#if WITH_EDITOR
	GetMutableDefault<UNNEDenoiserSettings>()->OnSettingChanged().AddRaw(this, &FViewExtension::OnDenoiserSettingsChanged);
#endif
}

FViewExtension::~FViewExtension()
{
#if WITH_EDITOR
	if(UObjectInitialized())
	{
		GetMutableDefault<UNNEDenoiserSettings>()->OnSettingChanged().RemoveAll(this);
	}
#endif

	GPathTracingDenoiserPlugin.Reset();
	GPathTracingSpatialTemporalDenoiserPlugin.Reset();
}

void FViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	ApplySettings(GetDefault<UNNEDenoiserSettings>());
}

void FViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (!bDenoiserEnabled)
	{
		GPathTracingDenoiserPlugin.Reset();
		GPathTracingSpatialTemporalDenoiserPlugin.Reset();
	}
	else
	{
		if (DenoiserToSwap.IsValid())
		{
			GPathTracingSpatialTemporalDenoiserPlugin.Reset();
			GPathTracingDenoiserPlugin = MoveTemp(DenoiserToSwap);
		}

		if (SpatialTemporalDenoiserToSwap.IsValid())
		{
			GPathTracingDenoiserPlugin.Reset();
			GPathTracingSpatialTemporalDenoiserPlugin = MoveTemp(SpatialTemporalDenoiserToSwap);
		}
	}
}

void FViewExtension::ApplySettings(const UNNEDenoiserSettings* Settings)
{
	// Check for changes
	bool bNeedsUpdate = false;
	bNeedsUpdate |= CVarNNEDenoiser.GetValueOnGameThread() != bDenoiserEnabled;
	bNeedsUpdate |= GetDenoiserRuntimeTypeFromCVar() != RuntimeType;
	bNeedsUpdate |= CVarNNEDenoiserRuntimeName.GetValueOnGameThread() != RuntimeName;
	bNeedsUpdate |= GetDenoiserModelDataNameFromCVarAndSettings(Settings) != ModelDataName;

	if (!bNeedsUpdate)
	{
		return;
	}

	bDenoiserEnabled = CVarNNEDenoiser.GetValueOnGameThread();

	UE_LOG(LogNNEDenoiser, Log, TEXT("ApplySettings: bDenoiserEnabled %d"), bDenoiserEnabled);

	if (!bDenoiserEnabled)
	{
		return;
	}

	RuntimeType = GetDenoiserRuntimeTypeFromCVar();
	RuntimeName = CVarNNEDenoiserRuntimeName.GetValueOnGameThread();
	ModelDataName = GetDenoiserModelDataNameFromCVarAndSettings(Settings);

	UE_LOG(LogNNEDenoiser, Log, TEXT("Create denoiser from asset %s..."), *ModelDataName);

	TUniquePtr<FGenericDenoiser> Denoiser = CreateNNEDenoiserFromAsset(ModelDataName, RuntimeType, RuntimeName);
	if (Denoiser.IsValid())
	{
		if (ModelDataName.Contains(TEXT("kpcn")))
		{
			SpatialTemporalDenoiserToSwap = MakeUnique<FPathTracingSpatialTemporalDenoiser>(MoveTemp(Denoiser));
		}
		else
		{
			DenoiserToSwap = MakeUnique<FPathTracingDenoiser>(MoveTemp(Denoiser));
		}
	}
	else
	{
		UE_LOG(LogNNEDenoiser, Error, TEXT("Could not create denoiser!"));
	}
}

#if WITH_EDITOR
void FViewExtension::OnDenoiserSettingsChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	UE_LOG(LogNNEDenoiser, Log, TEXT("Settings %s changed: %s"), *InObject->GetName(), *InPropertyChangedEvent.GetPropertyName().ToString());

	ApplySettings(CastChecked<UNNEDenoiserSettings>(InObject));
}
#endif

} // namespace UE::NNEDenoiser::Private