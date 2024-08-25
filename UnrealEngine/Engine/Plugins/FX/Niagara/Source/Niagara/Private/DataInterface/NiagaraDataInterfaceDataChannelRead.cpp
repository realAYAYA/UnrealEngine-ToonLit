// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface/NiagaraDataInterfaceDataChannelRead.h"

#include "NiagaraCompileHashVisitor.h"
#include "ShaderCompilerCore.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "NiagaraModule.h"
#include "NiagaraShaderParametersBuilder.h"

#include "NiagaraCustomVersion.h"
#include "NiagaraCommon.h"
#include "NiagaraSystem.h"
#include "NiagaraWorldManager.h"

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelManager.h"

#include "NiagaraEmitterInstanceImpl.h"

#include "NiagaraRenderer.h"
#include "NiagaraGPUSystemTick.h"

#include "NiagaraDataInterfaceUtilities.h"

#if WITH_EDITOR
#include "INiagaraEditorOnlyDataUtlities.h"
#include "Modules/ModuleManager.h"
#include "NiagaraModule.h"
#endif

//////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceDataChannelRead"

DECLARE_CYCLE_STAT(TEXT("NDIDataChannelRead Read"), STAT_NDIDataChannelRead_Read, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelRead Consume"), STAT_NDIDataChannelRead_Consume, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelRead Spawn"), STAT_NDIDataChannelRead_Spawn, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelRead Tick"), STAT_NDIDataChannelRead_Tick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelRead PostTick"), STAT_NDIDataChannelRead_PostTick, STATGROUP_NiagaraDataChannels);

static int GNDCReadForceTG = -1;
static FAutoConsoleVariableRef CVarNDCReadForceTG(
	TEXT("fx.Niagara.DataChannels.ForceReadTickGroup"),
	GNDCReadForceTG,
	TEXT("When >= 0 this will force Niagara systems with NDC read DIs to tick in the given Tick Group."),
	ECVF_Default
);

static bool GNDCReadForcePrevFrame = false;
static FAutoConsoleVariableRef CVarNDCReadForcePrevFrame(
	TEXT("fx.Niagara.DataChannels.ForceReadPrevFrame"),
	GNDCReadForcePrevFrame,
	TEXT("When true this will force Niagara systems with NDC read DIs to read from the previous frame."),
	ECVF_Default
);

namespace NDIDataChannelReadLocal
{
	static const TCHAR* CommonShaderFile = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelCommon.ush");
	static const TCHAR* TemplateShaderFile_Common = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplateCommon.ush");
	static const TCHAR* TemplateShaderFile_Read = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplate_Read.ush");
	static const TCHAR* TemplateShaderFile_Consume = TEXT("/Plugin/FX/Niagara/Private/DataChannel/NiagaraDataInterfaceDataChannelTemplate_Consume.ush");

	//////////////////////////////////////////////////////////////////////////
	//Function definitions
	const FNiagaraFunctionSignature& GetFunctionSig_Num()
	{
		static FNiagaraFunctionSignature Sig;
		if(!Sig.IsValid())
		{
			Sig.Name = TEXT("Num");
#if WITH_EDITORONLY_DATA
			Sig.Description = LOCTEXT("NumFunctionDescription", "Returns the current number of DataChannel accessible by this interface.");
			NIAGARA_ADD_FUNCTION_SOURCE_INFO(Sig)
#endif
			Sig.bMemberFunction = true;
			Sig.bExperimental = true;
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), TEXT("DataChannel interface")));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num")));
		}		
		return Sig;
	}

	const FNiagaraFunctionSignature& GetFunctionSig_GetNDCSpawnData()
	{
		static FNiagaraFunctionSignature Sig;
		if (!Sig.IsValid())
		{
			Sig.Name = NDIDataChannelUtilities::GetNDCSpawnDataName;
#if WITH_EDITORONLY_DATA
			Sig.Description = LOCTEXT("GetNDCSpawnInfoFunctionDescription", "Returns useful data in relation the the NDC item that spawned this particle.");
			NIAGARA_ADD_FUNCTION_SOURCE_INFO(Sig)
#endif		
			Sig.bMemberFunction = true;
			Sig.bExperimental = true;
			Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::Particle;
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), TEXT("DataChannel interface")));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraEmitterID::StaticStruct()), TEXT("Emitter ID")), LOCTEXT("EmitterIDDesc", "ID of the emitter we'd like to spawn into. This can be obtained from Engine.Emitter.ID."));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Spawned Particle Exec Index")), LOCTEXT("GetNDCSpawnData_InExecIndexDesc","The execution index of the spawned particle."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NDC Index")), LOCTEXT("GetNDCSpawnData_OutNDCIndexDesc","Index of the NDC item that spawned this particle."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NDC Spawn Index")), LOCTEXT("GetNDCSpawnData_OutNDCSpawnIndexDesc","The index of this particle in relation to all the particle spawned by the same NDC item."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NDC Spawn Count")), LOCTEXT("GetNDCSpawnData_OutNDCSpawnCountDesc","The number of particles spawned by the same NDC item."));
		}
		return Sig;
	}

	const FNiagaraFunctionSignature& GetFunctionSig_Read()
	{
		static FNiagaraFunctionSignature Sig;
		if(!Sig.IsValid())
		{
			Sig.Name = TEXT("Read");
#if WITH_EDITORONLY_DATA
			Sig.Description = LOCTEXT("ReadFunctionDescription", "Reads DataChannel data at a specific index. Any values we read that are not in the DataChannel data are set to their default values. Returns success if there was a valid DataChannel to read from.");
			NIAGARA_ADD_FUNCTION_SOURCE_INFO(Sig)
#endif
			Sig.bMemberFunction = true;
			Sig.bExperimental = true;
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), TEXT("DataChannel interface")));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")), LOCTEXT("ConsumeIndexInputDesc", "The index to read."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")), LOCTEXT("ConsumeSuccessOutputDesc", "True if all reads succeeded."));
			Sig.RequiredOutputs = Sig.Outputs.Num();//The user defines what we read in the graph.			
		}
		return Sig;
	}

	const FNiagaraFunctionSignature& GetFunctionSig_Consume()
	{
		static FNiagaraFunctionSignature Sig;
		if(!Sig.IsValid())
		{
			Sig.Name = TEXT("Consume");
#if WITH_EDITORONLY_DATA
			Sig.Description = LOCTEXT("ConsumeFunctionDescription", "Consumes an DataChannel from the end of the DataChannel array and reads the specified values. Any values we read that are not in the DataChannel data are set to their default values. Returns success if an DataChannel was available to pop.");
			NIAGARA_ADD_FUNCTION_SOURCE_INFO(Sig)
#endif
			Sig.bMemberFunction = true;
			Sig.bExperimental = true;
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), TEXT("DataChannel interface")));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Consume")), LOCTEXT("ConsumeInputDesc", "True if this instance (particle/emitter etc) should consume data from the data channel in this call."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")), LOCTEXT("ConsumeSuccessOutputDesc", "True if all reads succeeded."));
			Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")), LOCTEXT("ConsumeIndexOutputDesc", "The index we actually read from. If reading failed this can be -1. This allows subsequent reads of the data channel at this index."));
			Sig.RequiredOutputs = Sig.Outputs.Num();//The user defines what we read in the graph.			
		}
		return Sig;
	}

	enum class FunctionVersion_SpawnConditional : uint32
	{
		Initial = 0,
		EmitterIDParameter = 1,
	};

	const FNiagaraFunctionSignature& GetFunctionSig_SpawnConditional()
	{
		static FNiagaraFunctionSignature Sig;
		if(!Sig.IsValid())
		{
			FNiagaraVariable EnabledVar(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Enable"));
			EnabledVar.SetValue(FNiagaraBool(true));

			Sig.Name = TEXT("SpawnConditional");
#if WITH_EDITORONLY_DATA
			NIAGARA_ADD_FUNCTION_SOURCE_INFO(Sig)
			Sig.Description = LOCTEXT("SpawnCustomFunctionDescription", "Will Spawn particles into the bound Emitter between Min and Max counts for every element in the bound Data Channel.\n\
		Can take optional additional parameters as conditions on spawning that will be compared against the contents of each data channel element.\n\
		For example you could spawn only for a particular value of an enum.\n\
		For compound data types that contain multiple component floats or ints, comparissons are done on a per component basis.\n\
		For example if you add a Vector condition parameter it will be compared against each component of the corresponding Vector in the Data Channel.\n\
		Result = (Param.X == ChannelValue.X) && (Param.Y == ChannelValue.Y) && (Param.Z == ChannelValue.Z)");

			Sig.FunctionVersion = (uint32)FunctionVersion_SpawnConditional::EmitterIDParameter;
#endif
			Sig.bMemberFunction = true;
			Sig.bExperimental = true;
			Sig.bRequiresExecPin = true;
			Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::Emitter | ENiagaraScriptUsageMask::System;
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceDataChannelRead::StaticClass()), TEXT("DataChannel interface")));
			Sig.AddInput(EnabledVar, LOCTEXT("SpawnEnableInputDesc", "Enable or disable this function call. If false, this call with have no effetcs."));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraEmitterID::StaticStruct()), TEXT("Emitter ID")), LOCTEXT("EmitterIDDesc", "ID of the emitter we'd like to spawn into. This can be obtained from Engine.Emitter.ID."));
			Sig.AddInput(FNiagaraVariable(StaticEnum<ENDIDataChannelSpawnMode>(), TEXT("Mode")), LOCTEXT("SpawnCondModeInputDesc", "A mode switch that controls how this funciton will behave."));
			Sig.AddInput(FNiagaraVariable(StaticEnum<ENiagaraConditionalOperator>(), TEXT("Operator")), LOCTEXT("SpawnCondOpInputDesc", "The comparison operator to use when comparing values in the data channel to conditional parameters."));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Min Spawn Count")), LOCTEXT("MinSpawnCountInputDesc", "Minimum number of particles to spawn for each element in the data channel."));
			Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Max Spawn Count")), LOCTEXT("MaxSpawnCountInputDesc", "Maximum number of particles to spawn for each element in the data channel."));
			Sig.RequiredInputs = Sig.Inputs.Num();
		}
		return Sig;
	}
	// Function definitions END
	//////////////////////////////////////////////////////////////////////////

	const TCHAR* GetFunctionTemplate(FName FunctionName)
	{
		if (FunctionName == GetFunctionSig_Read().Name) return TemplateShaderFile_Read;
		if (FunctionName == GetFunctionSig_Consume().Name) return TemplateShaderFile_Consume;

		return nullptr;
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER_SRV(Buffer<uint>, ParamOffsetTable)
		SHADER_PARAMETER_SRV(Buffer<float>, DataFloat)
		SHADER_PARAMETER_SRV(Buffer<int>, DataInt32)
		SHADER_PARAMETER_SRV(Buffer<float>, DataHalf)
		SHADER_PARAMETER(int32, ParameterOffsetTableIndex)
		SHADER_PARAMETER(int32, Num)
		SHADER_PARAMETER(int32, FloatStride)
		SHADER_PARAMETER(int32, Int32Stride)
		SHADER_PARAMETER(int32, HalfStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int32>, NDCSpawnDataBuffer)
	END_SHADER_PARAMETER_STRUCT()
}

/** Render thread copy of current instance data. */
struct FNDIDataChannelReadInstanceData_RT
{
	//RT proxy for game channel data from which we're reading.
	FNiagaraDataChannelDataProxy* ChannelDataRTProxy = nullptr;

	bool bReadPrevFrame = false;
	/**
	Table of all parameter offsets used by each GPU script using this DI.
	Each script has to have it's own section of this table as the offsets into this table are embedded in the hlsl.
	At hlsl gen time we only have the context of each script individually to generate these indexes.
	TODO: Can possible elevate this up to the LayoutManager and have a single layout buffer for all scripts
	*/
	TResourceArray<uint32> GPUScriptParameterOffsetTable;

	/**
	Offsets into the parameter table are embedded in the gpu script hlsl.
	At hlsl gen time we can only know which parameters are accessed by each script individually so each script must have it's own parameter binding table.
	We provide the offset into the above table via a shader param.
	TODO: Can just as easily be an offset into a global buffer in the Layout manager.
	*/
	TMap<FNiagaraCompileHash, uint32> GPUScriptParameterTableOffsets;

	/** Signal we have updated function binding data from the GT this frame we should process. */
	bool bHasFunctionBindingUpdate = false;

	/** Buffer containing packed data for all emitters NDC spawning data for use on the GPU. */
	TArray<int32> NDCSpawnData;
};

//////////////////////////////////////////////////////////////////////////

FNDIDataChannelReadInstanceData::~FNDIDataChannelReadInstanceData()
{

}

FNiagaraDataBuffer* FNDIDataChannelReadInstanceData::GetReadBufferCPU(bool bPrevFrame)
{
	if (DataChannelData)
	{
		return DataChannelData->GetCPUData(bPrevFrame);
	}

	//TODO: Local reads.
// 	if (SourceInstData)
// 	{
// 		if (SourceInstData->Data && SourceInstData->Data->GetSimTarget() == ENiagaraSimTarget::CPUSim)
// 		{
// 			return SourceInstData->Data->GetDestinationData();
// 		}
// 	}

	return nullptr;
}

bool FNDIDataChannelReadInstanceData::Init(UNiagaraDataInterfaceDataChannelRead* Interface, FNiagaraSystemInstance* Instance)
{
	bool bSuccess = Tick(Interface, Instance, true);
	bSuccess &= PostTick(Interface, Instance);
	return bSuccess;
}

bool FNDIDataChannelReadInstanceData::Tick(UNiagaraDataInterfaceDataChannelRead* Interface, FNiagaraSystemInstance* Instance, bool bIsInit)
{
	ConsumeIndex = 0;
	for(auto& EmitterInstDataPair : EmitterInstanceData)
	{
		EmitterInstDataPair.Value.Reset();
	}

	const FNDIDataChannelCompiledData& CompiledData = Interface->GetCompiledData();

	//Interface is never used so do not create any instance data.
	if (CompiledData.UsedByCPU() == false && CompiledData.UsedByGPU() == false)
	{
		//UE_LOG(LogNiagara, Warning, TEXT("Data Channel Interface is being initialized but it is never used.\nSystem: %s\nInterface: %s"), *Instance->GetSystem()->GetFullName(), *Interface->GetFullName());
		return true;
	}
	
	Owner = Instance;

	//In non test/shipping builds we gather and log and missing parameters that cause us to fail to find correct bindings.
	TArray<FNiagaraVariableBase> MissingParams;

	//TODO: Reads directly from a local writer DI.
	//For local readers, we find the source DI inside the same system and bind our functions to it's data layout.
// 		if (Interface->Scope == ENiagaraDataChannelScope::Local)
// 		{
// 			//Find our source DI and it's inst data if needed.
// 			//We have generated our compiled data already in PostCompile but we have to get the actual DI and inst data at runtime as they may not be the same as compile time.
// 			//Though the layout of the data should be the same etc.
// 			if (SourceDI.IsValid() == false && Interface->Source != NAME_None)
// 			{
// 				//Should not be able to bind to ourselves!
// 				check(Interface->GetFName() != Interface->Source);
// 
// 				UNiagaraDataInterface* FoundDI = nullptr;
// 				auto FindSourceDI = [&](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& Context)
// 				{
// 					if (Context.Variable.GetName() == Interface->Source)
// 					{
// 						check(FoundDI == nullptr);
// 						FoundDI = Context.DataInterface;
// 						return false;
// 					}
// 					return true;
// 				};
// 
// 				if (UNiagaraSystem* System = Interface->GetTypedOuter<UNiagaraSystem>())
// 				{
// 					FNiagaraDataInterfaceUtilities::FDataInterfaceSearchFilter Filter;
// 					Filter.bIncludeInternal = true;
// 					FNiagaraDataInterfaceUtilities::ForEachDataInterface(System, Filter, FindSourceDI);
// 
// 
// 					if (UNiagaraDataInterfaceDataChannelWrite* SrcWriter = Cast<UNiagaraDataInterfaceDataChannelWrite>(FoundDI))
// 					{
// 						SourceDI = SrcWriter;
// 						SourceInstData = static_cast<FNDIDataChannelWriteInstanceData*>(Instance->FindDataInterfaceInstanceData(FoundDI));
// 
// 						uint32 NumFuncs = CompiledData.GetFunctionInfo().Num();
// 						FuncToDataSetBindingInfo.Reset(NumFuncs);
// 						for(const FNDIDataChannelFunctionInfo& FuncInfo : CompiledData.GetFunctionInfo())
// 						{
// 							FuncToDataSetBindingInfo.Add(FNDIDataChannelLayoutManager::Get().GetLayoutInfo(FuncInfo, SourceDI->GetCompiledData().DataLayout));
// 						}
// 					}
// 					else
// 					{
// 						UE_LOG(LogNiagara, Warning, TEXT("Failed to find the Source DataChannel Writer %s for DataChannel Reader %s"), *Interface->Source.ToString(), *Interface->GetFName().ToString());
// 						return false;
// 					}
// 				}				
// 			}
// 		}
// 		else
	{
		//For external readers, we find the DataChannel channel in the current world and bind our functions to it's data layout so we can read directly from the channel data.
		//check(Interface->Scope == ENiagaraDataChannelScope::World);

		//Grab our DataChannel channel and init the compiled data if needed.
		UNiagaraDataChannelHandler* DataChannelPtr = DataChannel.Get();
		if (DataChannelPtr == nullptr)
		{
			DataChannelData = nullptr;
			ChachedDataSetLayoutHash = INDEX_NONE;
			UWorld* World = Instance->GetWorld();
			if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
			{
				if (UNiagaraDataChannelHandler* NewChannelHandler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Interface->Channel))
				{
					DataChannelPtr = NewChannelHandler;
					DataChannel = NewChannelHandler;
				}
				else
				{
					UE_LOG(LogNiagara, Warning, TEXT("Failed to find or add Naigara DataChannel Channel: %s"), *Interface->Channel.GetName());
					return false;
				}
			}
		}

		//Grab the world DataChannel data if we're reading from there.	
		if (DataChannelPtr)
		{
			if(DataChannelData == nullptr || Interface->bUpdateSourceDataEveryTick)
			{
				//TODO: Automatically modify tick group if we have DIs that require current frame info?
				FNiagaraDataChannelSearchParameters SearchParams(Instance->GetAttachComponent());
				DataChannelData = DataChannelPtr->FindData(SearchParams, ENiagaraResourceAccess::ReadOnly);//TODO: Maybe should have two paths, one for system instances and another for SceneComponents...
			}	

			if(const UNiagaraDataChannel* ChannelPtr = DataChannelPtr->GetDataChannel())
			{
				if(!bIsInit && ChannelPtr->ShouldEnforceTickGroupReadWriteOrder() && Interface->bReadCurrentFrame)
				{
					ETickingGroup CurrTG = DataChannelPtr->GetCurrentTickGroup();
					ETickingGroup MinTickGroup = Interface->CalculateTickGroup(nullptr);//We don't use the per instance data...
					if(CurrTG < MinTickGroup)
					{
						static UEnum* TGEnum = StaticEnum<ETickingGroup>();
						UE_LOG(LogNiagara, Warning, TEXT("NDC Read DI is required to tick on or after %s but is reading in %s. This may cause us to have incorrectly ordered reads and writes to this NDC and thereform miss data.")
						, *TGEnum->GetDisplayNameTextByValue((int32)MinTickGroup).ToString()
						, *TGEnum->GetDisplayNameTextByValue((int32)CurrTG).ToString());
					}
				}
			}

			const FNiagaraDataSetCompiledData& CPUSourceDataCompiledData = DataChannelPtr->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::CPUSim);
			const FNiagaraDataSetCompiledData& GPUSourceDataCompiledData = DataChannelPtr->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::GPUComputeSim);
			check(CPUSourceDataCompiledData.GetLayoutHash() && CPUSourceDataCompiledData.GetLayoutHash() == GPUSourceDataCompiledData.GetLayoutHash());			
			uint64 SourceDataLayoutHash = CPUSourceDataCompiledData.GetLayoutHash();
			bool bChanged = SourceDataLayoutHash != ChachedDataSetLayoutHash;

			//If our CPU or GPU source data has changed then regenerate our binding info.
			//TODO: Multi-source buffer support. 
			//TODO: Variable input layout support. i.e. allow source systems to publish their particle buffers without the need for a separate write.
			if (bChanged)
			{
				ChachedDataSetLayoutHash = SourceDataLayoutHash;

				//We can likely be more targeted here.
				//Could probably only update the RT when the GPU data changes and only update the bindings if the function hashes change etc.
				bUpdateFunctionBindingRTData = CompiledData.UsedByGPU();
				int32 NumFuncs = CompiledData.GetFunctionInfo().Num();
				FuncToDataSetBindingInfo.SetNum(NumFuncs);
				//FuncToDataSetLayoutKeys.SetNumZeroed(NumFuncs);
				for (int32 BindingIdx = 0; BindingIdx < NumFuncs; ++BindingIdx)
				{
					const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[BindingIdx];

					FNDIDataChannel_FuncToDataSetBindingPtr& BindingPtr = FuncToDataSetBindingInfo[BindingIdx];
					BindingPtr = FNDIDataChannelLayoutManager::Get().GetLayoutInfo(FuncInfo, CPUSourceDataCompiledData, MissingParams);
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	if(MissingParams.Num() > 0)
	{
		FString MissingParamsString;
		for (FNiagaraVariableBase& MissingParam : MissingParams)
		{
			MissingParamsString += FString::Printf(TEXT("%s %s\n"), *MissingParam.GetType().GetName(), *MissingParam.GetName().ToString());
		}

		UE_LOG(LogNiagara, Warning, TEXT("Niagara Data Channel Reader Interface is trying to read parameters that do not exist in this channel.\nIt's likely that the Data Channel Definition has been changed and this system needs to be updated.\nData Channel: %s\nSystem: %s\nComponent:%s\nMissing Parameters:\n%s\n")
			, *DataChannel->GetDataChannel()->GetName()
			, *Instance->GetSystem()->GetPathName()
			, *Instance->GetAttachComponent()->GetPathName()
			, *MissingParamsString);		
	}	
#endif

	if (!DataChannel.IsValid() /*&& !SourceDI.IsValid()*/)//TODO: Local reads
	{
		UE_LOG(LogNiagara, Warning, TEXT("Niagara Data Channel Reader Interface could not find a valid data channel.\nData Channel: %s\nSystem: %s\nComponent:%s\n")
			, Interface->Channel ? *Interface->Channel->GetName() : TEXT("None")
			, *Instance->GetSystem()->GetPathName()
			, *Instance->GetAttachComponent()->GetPathName());
		return false;
	}

	//Verify we have valid binding info. If not, we have to bail as we cannot properly parse the vm bytecode.
	if (FuncToDataSetBindingInfo.Num() != Interface->GetCompiledData().GetFunctionInfo().Num())
	{
		return false;
	}

	for (const FNDIDataChannel_FuncToDataSetBindingPtr& FuncBinding : FuncToDataSetBindingInfo)
	{
		if (FuncBinding.IsValid() == false || FuncBinding->IsValid() == false)
		{
			return false;
		}
	}

	return true;
}

bool FNDIDataChannelReadInstanceData::PostTick(UNiagaraDataInterfaceDataChannelRead* Interface, FNiagaraSystemInstance* Instance)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////

UNiagaraDataInterfaceDataChannelRead::UNiagaraDataInterfaceDataChannelRead(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNiagaraDataInterfaceProxy_DataChannelRead());
}

void UNiagaraDataInterfaceDataChannelRead::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject) && INiagaraModule::DataChannelsEnabled())
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceDataChannelRead::BeginDestroy()
{
	Super::BeginDestroy();
}

void UNiagaraDataInterfaceDataChannelRead::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
	const int32 NiagaraVersion = Ar.CustomVer(FNiagaraCustomVersion::GUID);

	//Before we serialize in the properties we will restore any old default values from previous versions.
	if(NiagaraVersion < FNiagaraCustomVersion::NDCSpawnGroupOverrideDisabledByDefault)
	{
		bOverrideSpawnGroupToDataChannelIndex = true;
	}

	Super::Serialize(Ar);
}

bool UNiagaraDataInterfaceDataChannelRead::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIDataChannelReadInstanceData* InstanceData = new (PerInstanceData) FNDIDataChannelReadInstanceData;

	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return false;
	}

	if (InstanceData->Init(this, SystemInstance) == false)
	{
		return false;
	}

	return true;
}

void UNiagaraDataInterfaceDataChannelRead::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIDataChannelReadInstanceData* InstanceData = static_cast<FNDIDataChannelReadInstanceData*>(PerInstanceData);
	InstanceData->~FNDIDataChannelReadInstanceData();

	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	(
		[RT_Proxy = GetProxyAs<FNiagaraDataInterfaceProxy_DataChannelRead>(), InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->SystemInstancesToProxyData_RT.Remove(InstanceID);
		}
	);
}

int32 UNiagaraDataInterfaceDataChannelRead::PerInstanceDataSize() const
{
	return sizeof(FNDIDataChannelReadInstanceData);
}

bool UNiagaraDataInterfaceDataChannelRead::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelRead_Tick);
	check(SystemInstance);

	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}
	
	FNDIDataChannelReadInstanceData* InstanceData = static_cast<FNDIDataChannelReadInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->Tick(this, SystemInstance) == false)
	{
		return true;
	}

	return false;
}

bool UNiagaraDataInterfaceDataChannelRead::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelRead_PostTick);
	check(SystemInstance);

	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}

	FNDIDataChannelReadInstanceData* InstanceData = static_cast<FNDIDataChannelReadInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->PostTick(this, SystemInstance) == false)
	{
		return true;
	}

	return false;
}

void UNiagaraDataInterfaceDataChannelRead::PostStageTick(FNDICpuPostStageContext& Context)
{
	FNDIDataChannelReadInstanceData* InstanceData = Context.GetPerInstanceData<FNDIDataChannelReadInstanceData>();

	check(InstanceData);
	check(Context.Usage == ENiagaraScriptUsage::EmitterUpdateScript || Context.Usage == ENiagaraScriptUsage::SystemUpdateScript);

	for (auto& EmitterInstanceDataPair : InstanceData->EmitterInstanceData)
	{
		FNDIDataChannelRead_EmitterInstanceData& EmitterInstData = EmitterInstanceDataPair.Value;

		TArray<int32> PerNDCSpawnCounts = MoveTemp(EmitterInstData.NDCSpawnCounts);
	
		EmitterInstData.Reset();

		if(FNiagaraEmitterInstance* TargetEmitter = EmitterInstanceDataPair.Key)
		{
			//-TODO:Stateless:
			if (FNiagaraEmitterInstanceImpl* StatefulEmitter = TargetEmitter->AsStateful())
			{
				if (bOverrideSpawnGroupToDataChannelIndex)
				{
					//If we're overriding the spawn group then we must submit one SpawnInfo per NDC entry.
					FNiagaraSpawnInfo NewSpawnInfo(0,0.0f,0.0f,0);
					for (int32 i = 0; i < PerNDCSpawnCounts.Num(); ++i)
					{
						int32 SpawnCount = PerNDCSpawnCounts[i];
						if (SpawnCount > 0)
						{
							NewSpawnInfo.Count = SpawnCount;
							NewSpawnInfo.SpawnGroup = i;
							StatefulEmitter->GetSpawnInfo().Emplace(NewSpawnInfo);
						}
					}
				}
				else 
				{
					//No need for indirection table but we're not overriding the spawn group either so still push a single combined spawn info.
					FNiagaraSpawnInfo NewSpawnInfo(0, 0.0f, 0.0f, 0);
					for (int32 i = 0; i < PerNDCSpawnCounts.Num(); ++i)
					{
						NewSpawnInfo.Count += PerNDCSpawnCounts[i];
					}
					StatefulEmitter->GetSpawnInfo().Emplace(NewSpawnInfo);
				}

				if(CompiledData.NeedSpawnDataTable())
				{
					//Build an indirection table that allows us to map from ExecIndex back to the NDCIndex that generated it.
					//The indirection table is arranged in power of two buckets.
					//An NDC that spawns say 37 particles would add an entry to the 32, 4 and 1 buckets.
					//This allows us to spawn any number of particles from each NDC and only have a max of 16 indirection table entries.
					//Vs the naive per particle approach of 1 entry per particle.
					//Buckets are processed in descending size order.

					//TODO: It should be possible to write this from the GPU too as long as we allocate fixed size buckets.
					int32* SpawnDataBuckets = EmitterInstData.NDCSpawnData.NDCSpawnDataBuckets;					

					TArray<int32>& NDCSpawnData = EmitterInstData.NDCSpawnData.NDCSpawnData;

					//Start of the buffer is the per NDC spawn counts.
					uint32 TotalNDCSpawnDataSize = PerNDCSpawnCounts.Num();

					for (int32 i = 0; i < PerNDCSpawnCounts.Num(); ++i)
					{						
						uint32 Count = PerNDCSpawnCounts[i];

						//First section is the per NDC counts.
						NDCSpawnData.Add(Count);

						for (uint32 Bucket = 0; Bucket < 16; ++Bucket)
						{
							uint32 BucketSize = (1<<15) >> Bucket;
							uint32 Mask = (0xFFFF >> (Bucket + 1));
							uint32 CountMasked = Count & ~Mask;
							Count &= Mask;
							uint32 NumBucketEntries = CountMasked / BucketSize;
							SpawnDataBuckets[Bucket] += NumBucketEntries;
							TotalNDCSpawnDataSize += NumBucketEntries;
						}
					}

					//Second part is the counts decomposed into power of two buckets that allows us to map ExecIndex at runtime to an NDCIndex entry in this table.
					for (int32 Bucket = 0; Bucket < 16; ++Bucket)
					{
						uint32 BucketSize = (1 << 15) >> Bucket;
						uint32 StartSize = NDCSpawnData.Num();
						for (int32 i = 0; i < PerNDCSpawnCounts.Num(); ++i)
						{
							int32& Count = PerNDCSpawnCounts[i];
							while ((uint32)Count >= BucketSize)
							{
								Count -= BucketSize;
								NDCSpawnData.Add(i);
							}
						}

						uint32 EndSize = NDCSpawnData.Num();
						check(EndSize - StartSize == SpawnDataBuckets[Bucket]);
					}
				}
			}
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	const FNDIDataChannelReadInstanceData& SourceData = *reinterpret_cast<const FNDIDataChannelReadInstanceData*>(PerInstanceData);
	FNDIDataChannelReadInstanceData_RT* TargetData = new(DataForRenderThread) FNDIDataChannelReadInstanceData_RT();

	//Always update the dataset, this may change without triggering a full update if it's layout is the same.
	TargetData->ChannelDataRTProxy = SourceData.DataChannelData ? SourceData.DataChannelData->GetRTProxy() : nullptr;

	bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	TargetData->bReadPrevFrame = bReadPrevFrame;

	if (SourceData.bUpdateFunctionBindingRTData && INiagaraModule::DataChannelsEnabled())
	{
		SourceData.bUpdateFunctionBindingRTData = false;

		TargetData->bHasFunctionBindingUpdate = true;
		
		const FNiagaraDataSetCompiledData& GPUCompiledData = SourceData.DataChannel->GetDataChannel()->GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

		//For every GPU script, we append it's parameter access info to the table.
		TargetData->GPUScriptParameterTableOffsets.Reset();
		constexpr int32 ElemsPerParam = 3;
		TargetData->GPUScriptParameterOffsetTable.Reset(CompiledData.GetTotalParams() * ElemsPerParam);
		for (auto& GPUParameterAccessInfoPair : CompiledData.GetGPUScriptParameterInfos())
		{
			const FNDIDataChannel_GPUScriptParameterAccessInfo& ParamAccessInfo = GPUParameterAccessInfoPair.Value;

			//First get the offset for this script in the table.
			TargetData->GPUScriptParameterTableOffsets.FindOrAdd(GPUParameterAccessInfoPair.Key) = TargetData->GPUScriptParameterOffsetTable.Num();

			//Now fill the table for this script
			for (const FNiagaraVariableBase& Param : ParamAccessInfo.SortedParameters)
			{
				if (const FNiagaraVariableLayoutInfo* LayoutInfo = GPUCompiledData.FindVariableLayoutInfo(Param))
				{
					TargetData->GPUScriptParameterOffsetTable.Add(LayoutInfo->GetNumFloatComponents() > 0 ? LayoutInfo->GetFloatComponentStart() : INDEX_NONE);
					TargetData->GPUScriptParameterOffsetTable.Add(LayoutInfo->GetNumInt32Components() > 0 ? LayoutInfo->GetInt32ComponentStart() : INDEX_NONE);
					TargetData->GPUScriptParameterOffsetTable.Add(LayoutInfo->GetNumHalfComponents() > 0 ? LayoutInfo->GetHalfComponentStart() : INDEX_NONE);
				}
				else
				{
					TargetData->GPUScriptParameterOffsetTable.Add(INDEX_NONE);
					TargetData->GPUScriptParameterOffsetTable.Add(INDEX_NONE);
					TargetData->GPUScriptParameterOffsetTable.Add(INDEX_NONE);
				}
			}
		}
	}

	//Always need to fill in the NDCSpawnData array as it will change every frame and be pushed into an RDG buffer.

	//New buffer is every emitter continuous NDCSpawnDataArray. We need to store an offset that we pass in as a uniform.
	//The buckets come first, then the per NDC SpawnCounts, Then the bucket back ptrs.

	//Do one pass to calculate size.
	auto GetEmitterNDCSpawnDataSize = [](const FNDIDataChannelRead_EmitterInstanceData& EmitterInstData)
	{
		return 16 + EmitterInstData.NDCSpawnCounts.Num() + EmitterInstData.NDCSpawnData.NDCSpawnData.Num();
	};

	uint32 NumEmitters = SourceData.EmitterInstanceData.Num();
	uint32 TotalPacckedNDCSpawnDataSize = 0;
	int32 MaxEmitterIndex = 0;
	for (const TPair<FNiagaraEmitterInstance*, FNDIDataChannelRead_EmitterInstanceData>& EmitterInstDataPair : SourceData.EmitterInstanceData)
	{
		if (const FNiagaraEmitterInstance* EmitterInst = EmitterInstDataPair.Key)
		{
			const FNDIDataChannelRead_EmitterInstanceData& EmitterInstData = EmitterInstDataPair.Value;

			TotalPacckedNDCSpawnDataSize += GetEmitterNDCSpawnDataSize(EmitterInstData);
			FNiagaraEmitterID ID = EmitterInst->GetEmitterID();
			MaxEmitterIndex = FMath::Max(MaxEmitterIndex, ID.ID);
		}
	}

	//First section of the NDCSpawnDataBuffer is an offset into the buffer for each emitter.
	TotalPacckedNDCSpawnDataSize += MaxEmitterIndex;

	TargetData->NDCSpawnData.Reset(TotalPacckedNDCSpawnDataSize);
	TArray<int32>& TargetNDCSpawnData = TargetData->NDCSpawnData;

	//First grab space for the per emitter offset table. We'll fill this in as we go.
	TargetNDCSpawnData.AddZeroed(MaxEmitterIndex + 1);

	uint32 CurrentSpawnDataOffset = TargetNDCSpawnData.Num();

	for (const TPair<FNiagaraEmitterInstance*, FNDIDataChannelRead_EmitterInstanceData>& EmitterInstDataPair : SourceData.EmitterInstanceData)
	{
		if (const FNiagaraEmitterInstance* EmitterInst = EmitterInstDataPair.Key)
		{
			const FNDIDataChannelRead_EmitterInstanceData& EmitterInstData = EmitterInstDataPair.Value;
			if(EmitterInst->GetGPUContext() == nullptr)
			{
				continue;
			}


			uint32 EmitterNDCSpawnDataSize = GetEmitterNDCSpawnDataSize(EmitterInstData);

			//First fill in the current offset for this emitter.
			FNiagaraEmitterID EmitterID = EmitterInst->GetEmitterID();
			TargetNDCSpawnData[EmitterID.ID] = CurrentSpawnDataOffset;

			CurrentSpawnDataOffset += EmitterNDCSpawnDataSize;

			//Next fill in bucket counts
			for (int32 i = 0; i < 16; ++i)
			{
				TargetNDCSpawnData.Add(EmitterInstData.NDCSpawnData.NDCSpawnDataBuckets[i]);
			}
			//Next the per NDC Spawn Counts
			TargetNDCSpawnData.Append(EmitterInstData.NDCSpawnCounts);
			//Finally the exec index to NDC index mapping table
			TargetNDCSpawnData.Append(EmitterInstData.NDCSpawnData.NDCSpawnData);
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies)const
{
	//TODO: Local support.
	// When reading directly from a local writer DI we modify the tick order so the readers come after writers.
	//Find our source DI and add a dependency for any emitter that writes to it.
// 	if (UNiagaraDataInterfaceDataChannelWrite* SourceDI = FindSourceDI())
// 	{
// 		auto HandleVMFunc = [&](const UNiagaraScript* Script, const FVMExternalFunctionBindingInfo& BindingInfo)
// 		{
// 			if (UNiagaraEmitter* Emitter = Script->GetTypedOuter<UNiagaraEmitter>())
// 			{
// 				if (Script->IsParticleScript())
// 				{
// 					for (const FNiagaraEmitterHandle& EmitterHandle : Asset->GetEmitterHandles())
// 					{
// 						FVersionedNiagaraEmitter EmitterInstance = EmitterHandle.GetInstance();
// 						if (EmitterInstance.Emitter.Get() == Emitter)
// 						{
// 							Dependencies.AddUnique(EmitterInstance);
// 						}
// 					}
// 				}
// 			}
// 			return true;
// 		};
// 		if (SourceDI)
// 		{
// 			FNiagaraDataInterfaceUtilities::ForEachVMFunctionEquals(SourceDI, Asset, HandleVMFunc);
// 		}
// 	}
}

bool UNiagaraDataInterfaceDataChannelRead::HasTickGroupPrereqs() const
{
	if (GNDCReadForceTG >= 0 && GNDCReadForceTG < (int32)ETickingGroup::TG_MAX)
	{
		return true;
	}
	else if (Channel && Channel->Get())
	{
		return Channel->Get()->ShouldEnforceTickGroupReadWriteOrder();
	}
	return false;
}

ETickingGroup UNiagaraDataInterfaceDataChannelRead::CalculateTickGroup(const void* PerInstanceData) const
{
	if(GNDCReadForceTG >= 0 && GNDCReadForceTG < (int32)ETickingGroup::TG_MAX)
	{
		return (ETickingGroup)GNDCReadForceTG;
	}
	else if(Channel && Channel->Get() && Channel->Get()->ShouldEnforceTickGroupReadWriteOrder())
	{
		return (ETickingGroup)((int32)Channel->Get()->GetFinalWriteTickGroup() + 1);
	}

	return NiagaraFirstTickGroup; 
}

#if WITH_EDITORONLY_DATA

void UNiagaraDataInterfaceDataChannelRead::PostCompile()
{
	UNiagaraSystem* OwnerSystem = GetTypedOuter<UNiagaraSystem>();
	CompiledData.Init(OwnerSystem, this);
}

#endif


#if WITH_EDITOR	

void UNiagaraDataInterfaceDataChannelRead::GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo)
{
	Super::GetFeedback(InAsset, InComponent, OutErrors, OutWarnings, OutInfo);

	INiagaraModule& NiagaraModule = FModuleManager::GetModuleChecked<INiagaraModule>("Niagara");
	const INiagaraEditorOnlyDataUtilities& EditorOnlyDataUtilities = NiagaraModule.GetEditorOnlyDataUtilities();
	UNiagaraDataInterface* RuntimeInstanceOfThis = InAsset && EditorOnlyDataUtilities.IsEditorDataInterfaceInstance(this)
		? EditorOnlyDataUtilities.GetResolvedRuntimeInstanceForEditorDataInterfaceInstance(*InAsset, *this)
		: this;

	UNiagaraDataInterfaceDataChannelRead* RuntimeReadDI = Cast<UNiagaraDataInterfaceDataChannelRead>(RuntimeInstanceOfThis);
	
	if(!RuntimeReadDI)
	{
		return;	
	}

	if (RuntimeReadDI->Channel == nullptr)
	{
		OutErrors.Emplace(LOCTEXT("DataChannelMissingFmt", "Data Channel Interface has no valid Data Channel."),
			LOCTEXT("DataChannelMissingErrorSummaryFmt", "Missing Data Channel."),
			FNiagaraDataInterfaceFix());

		return;
	}

	//if(Scope == ENiagaraDataChannelScope::World)
	{
		if (const UNiagaraDataChannel* DataChannel = RuntimeReadDI->Channel->Get())
		{
			//Ensure the data channel contains all the parameters this function is requesting.
			TConstArrayView<FNiagaraDataChannelVariable> ChannelVars = DataChannel->GetVariables();
			for (const FNDIDataChannelFunctionInfo& FuncInfo : RuntimeReadDI->GetCompiledData().GetFunctionInfo())
			{
				TArray<FNiagaraVariableBase> MissingParams;

				auto VerifyChannelContainsParams = [&](const TArray<FNiagaraVariableBase>& Parameters)
				{
					for (const FNiagaraVariableBase& FuncParam : Parameters)
					{
						bool bParamFound = false;
						for (const FNiagaraDataChannelVariable& ChannelVar : ChannelVars)
						{
							//We have to convert each channel var to SWC for comparison with the function variables as there is no reliable way to go back from the SWC function var to the originating LWC var.
							FNiagaraVariable SWCVar(ChannelVar);
							if(ChannelVar.GetType().IsEnum() == false)
							{
								UScriptStruct* ChannelSWCStruct = FNiagaraTypeHelper::GetSWCStruct(ChannelVar.GetType().GetScriptStruct());
								if (ChannelSWCStruct)
								{
									FNiagaraTypeDefinition SWCType(ChannelSWCStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Deny);
									SWCVar = FNiagaraVariable(SWCType, ChannelVar.GetName());
								}
							}
						
							if (SWCVar == FuncParam)
							{
								bParamFound = true;
								break;
							}
						}

						if (bParamFound == false)
						{
							MissingParams.Add(FuncParam);
						}
					}
				};
				VerifyChannelContainsParams(FuncInfo.Inputs);
				VerifyChannelContainsParams(FuncInfo.Outputs);

				if(MissingParams.Num() > 0)
				{
					FTextBuilder Builder;
					Builder.AppendLineFormat(LOCTEXT("FuncParamMissingFromDataChannelErrorFmt", "Accessing variables that do not exist in Data Channel {0}."), FText::FromString(Channel.GetName()));
					for(FNiagaraVariableBase& Param : MissingParams)
					{
						Builder.AppendLineFormat(LOCTEXT("FuncParamMissingFromDataChannelErrorLineFmt", "{0} {1}"), Param.GetType().GetNameText(), FText::FromName(Param.GetName()));
					}					
					
					OutErrors.Emplace(Builder.ToText(), LOCTEXT("FuncParamMissingFromDataChannelErrorSummaryFmt", "Data Channel DI function is accessing invalid parameters."), FNiagaraDataInterfaceFix());
				}
			}
		}
		else
		{
			OutErrors.Emplace(FText::Format(LOCTEXT("DataChannelDoesNotExistErrorFmt", "Data Channel {0} does not exist. It may have been deleted."), FText::FromString(Channel.GetName())),
				LOCTEXT("DataChannelDoesNotExistErrorSummaryFmt", "Data Channel DI is accesssinga a Data Channel that doesn't exist."),
				FNiagaraDataInterfaceFix());
		}
	}
	//TODO: Local support.
// 	else
// 	{
// 		check(Scope == ENiagaraDataChannelScope::Local);
// 		UNiagaraDataInterfaceDataChannelWrite* SourceDI = FindSourceDI();
// 		if (SourceDI == nullptr)
// 		{
// 			OutErrors.Emplace(FText::Format(LOCTEXT("DataChannelCouldNotFindSourceErrorFmt", "Could not find local Data Channel Writer DI with name {0}."), FText::FromName(Source)),
// 				LOCTEXT("DataChannelCouldNotFindSourceErrorSummaryFmt", "Could not find source Data Channel Writer DI for local Data Channel Reader DI."),
// 				FNiagaraDataInterfaceFix());
// 		}
// 	}
}

void UNiagaraDataInterfaceDataChannelRead::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	Super::ValidateFunction(Function, OutValidationErrors);

	//It would be great to be able to validate the parameters on the function calls here but this is only called on the DI CDO. We don't have the context of which data channel we'll be accessing.
	//The translator should have all the required data to use the actual DIs when validating functions. We just need to do some wrangling to pull it from the pre compiled data correctly.
	//This would probably also allow us to actually call hlsl generation functions on the actual DIs rather than their CDOs. Which would allow for a bunch of better optimized code gen for things like fluids.
	//TODO!!!
}

#endif


UNiagaraDataInterfaceDataChannelWrite* UNiagaraDataInterfaceDataChannelRead::FindSourceDI()const
{
	//TODO: Local Read/Write?
// 	if (Scope == ENiagaraDataChannelScope::Local)
// 	{
// 		UNiagaraDataInterface* FoundDI = nullptr;
// 		auto FindSourceDI = [Interface=this, &FoundDI](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& Context)
// 		{
// 			if (Context.Variable.GetName() == Interface->Source)
// 			{
// 				check(FoundDI == nullptr);				
// 				FoundDI = Context.DataInterface;
// 				return false;
// 			}
// 			return true;
// 		};
// 
// 		if (UNiagaraSystem* System = GetTypedOuter<UNiagaraSystem>())
// 		{
// 			FNiagaraDataInterfaceUtilities::FDataInterfaceSearchFilter Filter;
// 			Filter.bIncludeInternal = true;
// 			FNiagaraDataInterfaceUtilities::ForEachDataInterface(System, Filter, FindSourceDI);
// 
// 			if (UNiagaraDataInterfaceDataChannelWrite* SrcWriter = Cast<UNiagaraDataInterfaceDataChannelWrite>(FoundDI))
// 			{
// 				return SrcWriter;
// 			}
// 		}
// 	}
	return nullptr;
}

bool UNiagaraDataInterfaceDataChannelRead::Equals(const UNiagaraDataInterface* Other)const
{
	//UE_LOG(LogNiagara, Warning, TEXT("Checking Equality DCRead DI %s == %s"), *GetPathName(), *Other->GetPathName());
	if (const UNiagaraDataInterfaceDataChannelRead* OtherTyped = CastChecked<UNiagaraDataInterfaceDataChannelRead>(Other))
	{
		if (Super::Equals(Other) &&
			//Scope == OtherTyped->Scope &&
			//Source == OtherTyped->Source &&
			Channel == OtherTyped->Channel &&
			bReadCurrentFrame == OtherTyped->bReadCurrentFrame &&
			bUpdateSourceDataEveryTick == OtherTyped->bUpdateSourceDataEveryTick &&
			bOverrideSpawnGroupToDataChannelIndex == OtherTyped->bOverrideSpawnGroupToDataChannelIndex)
		{
			return true;
		}
	}

	return false;
}

bool UNiagaraDataInterfaceDataChannelRead::CopyToInternal(UNiagaraDataInterface* Destination)const
{
	//UE_LOG(LogNiagara, Warning, TEXT("Coping DCRead DI %s --> %s"), *GetPathName(), *Destination->GetPathName());
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	if (UNiagaraDataInterfaceDataChannelRead* DestTyped = CastChecked<UNiagaraDataInterfaceDataChannelRead>(Destination))
	{
		//DestTyped->Scope = Scope;
		//DestTyped->Source = Source;
		DestTyped->Channel = Channel;
		DestTyped->CompiledData = CompiledData;
		DestTyped->bReadCurrentFrame = bReadCurrentFrame;
		DestTyped->bUpdateSourceDataEveryTick = bUpdateSourceDataEveryTick;
		DestTyped->bOverrideSpawnGroupToDataChannelIndex = bOverrideSpawnGroupToDataChannelIndex;		
		return true;
	}

	return false;
}

#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceDataChannelRead::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const
{
	OutFunctions.Add(NDIDataChannelReadLocal::GetFunctionSig_Num());
	OutFunctions.Add(NDIDataChannelReadLocal::GetFunctionSig_GetNDCSpawnData());
	OutFunctions.Add(NDIDataChannelReadLocal::GetFunctionSig_Read());
	OutFunctions.Add(NDIDataChannelReadLocal::GetFunctionSig_Consume());
	OutFunctions.Add(NDIDataChannelReadLocal::GetFunctionSig_SpawnConditional());
}
#endif

void UNiagaraDataInterfaceDataChannelRead::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == NDIDataChannelReadLocal::GetFunctionSig_Num().Name)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->Num(Context); });
	}
	else if (BindingInfo.Name == NDIDataChannelReadLocal::GetFunctionSig_GetNDCSpawnData().Name)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->GetNDCSpawnData(Context); });
	}
	else
	{
		int32 FuncIndex = CompiledData.FindFunctionInfoIndex(BindingInfo.Name, BindingInfo.VariadicInputs, BindingInfo.VariadicOutputs);
		if (BindingInfo.Name == NDIDataChannelReadLocal::GetFunctionSig_Read().Name)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this, FuncIndex](FVectorVMExternalFunctionContext& Context) { this->Read(Context, FuncIndex); });
		}
		else if (BindingInfo.Name == NDIDataChannelReadLocal::GetFunctionSig_Consume().Name)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this, FuncIndex](FVectorVMExternalFunctionContext& Context) { this->Consume(Context, FuncIndex); });
		}
		else if (BindingInfo.Name == NDIDataChannelReadLocal::GetFunctionSig_SpawnConditional().Name)
		{
			OutFunc = FVMExternalFunction::CreateLambda([this, FuncIndex](FVectorVMExternalFunctionContext& Context) { this->SpawnConditional(Context, FuncIndex); });
		}
		else
		{
			UE_LOG(LogTemp, Display, TEXT("Could not find data interface external function in %s. Received Name: %s"), *GetPathNameSafe(this), *BindingInfo.Name.ToString());
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::Num(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIDataChannelReadInstanceData> InstData(Context);

	FNDIOutputParam<int32> OutNum(Context);

	bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	FNiagaraDataBuffer* Buffer = InstData->GetReadBufferCPU(bReadPrevFrame);

	int32 Num = 0;
	if (Buffer && INiagaraModule::DataChannelsEnabled())
	{
		Num = (int32)Buffer->GetNumInstances();
	}

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		OutNum.SetAndAdvance(Num);
	}
}

/** 
GetNDCSpawnData - Retrieves spawn data about the NDC that spawned a particular particle.
Uses an indirection table that decomposes each NDC spawn into power of 2 buckets of particles.
We do this to strike a balance between allowing many particles per NDC entry and allowing many NDC entires to spawn particles.
We have a max of 16 buckets with the highest being for spawns with 1<15 particles or more and the lowest being for individual particles.
An example with two NDC entries. The first spawning 10 and the second 8.
The first's spawn count decomposes into an entry in the 8 bucket and 2 bucket.
The seconds just has an entry in the 8 bucket.
We have 16 buckets so the bucket counts array looks like
0,0,0,0,0,0,0,0,0,0,0,0,2,0,1,0
This means the we have 2 buckets with data so the rest of our buffer is.
0,1,0,

We have two entires in the 8 bucket. 
As we spawn particles we use our bucket sizes counts and the exec index to see which bucket entry each exec index should use.
The first 8 particles processed, exec index 0-7 will lookup the first entry and so use NDC 0.
THe next 8 particles, exec index 8-15 will use the next and so use NDC 1.
Finally the last two particle spawned will use the next entry and so also use NDC 0.

So in total we do have 10 particles from NDC 0 and 8 from NDC 1.
However they will not be processed all together with their own spawning NDC.

In the worst case an NDC entry could add to all 16 buckets and so we'd have 16 entries for that NDC entry.
Which may seem like a lot but consider that is spawning 1<<15 particles so not all that bad really.

It also means the lookup does not need to search an arbitrary sized list.
It just has to loop over a size 16 array and do some math to get an index into the main buffer from which to retreive the NDC Index.

Once we have the NDCIndex we the do another similar pass and use the total spawn counts for that NDC to work out a SpawnIndex within the NDC.
*/
void UNiagaraDataInterfaceDataChannelRead::GetNDCSpawnData(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIDataChannelReadInstanceData> InstData(Context);

	FNDIInputParam<FNiagaraEmitterID> InEmitterID(Context);
	FNDIInputParam<int32> InExecIndex(Context);
	FNDIOutputParam<int32> OutNDCIndex(Context);
	FNDIOutputParam<int32> OutNDCSpawnIndex(Context);
	FNDIOutputParam<int32> OutNDCSpawnCount(Context);

	FNiagaraSystemInstance* SystemInstance = InstData->Owner;
	check(SystemInstance);

	bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	FNiagaraDataBuffer* Buffer = InstData->GetReadBufferCPU(bReadPrevFrame);

	if(Buffer && INiagaraModule::DataChannelsEnabled())
	{
		uint32 NumNDCEntries = Buffer->GetNumInstances();

		auto CalculateNDCSpawnInfo = [&](const FNDIDataChannelRead_EmitterInstanceData& EmitterInstanceData)
		{
			const int32* NDCSpawnBukets = EmitterInstanceData.NDCSpawnData.NDCSpawnDataBuckets;
			TConstArrayView<int32> NDCSpawnData(EmitterInstanceData.NDCSpawnData.NDCSpawnData);

			uint32 ExecIndex = InExecIndex.GetAndAdvance();
			uint32 NDCIndex = INDEX_NONE;

			//First we find which bucket this exec index is in.	
			uint32 MaxBucketExecIndex = 0;
			uint32 BucketEntryStart = NumNDCEntries;
			for (uint32 BucketIdx = 0; BucketIdx < 16; ++BucketIdx)
			{
				uint32 BucketSize = (1 << 15) >> BucketIdx;
				uint32 NumEntriesInBucket = NDCSpawnBukets[BucketIdx];
				uint32 MinBucketExecIndex = MaxBucketExecIndex;
				MaxBucketExecIndex += BucketSize * NumEntriesInBucket;
				if (ExecIndex < MaxBucketExecIndex)
				{
					//We found our bucket.
					//Now we need to find our NDCIndex Entry Index.
					uint32 NDCIndexEntry = (ExecIndex - MinBucketExecIndex) >> (15 - BucketIdx);

					NDCIndex = NDCSpawnData[BucketEntryStart + NDCIndexEntry];
					break;
				}

				BucketEntryStart += NumEntriesInBucket;
			}

			if (NDCIndex >= 0 && NDCIndex < NumNDCEntries)
			{
				OutNDCIndex.SetAndAdvance(NDCIndex);

				uint32 NDCSpawnCount = NDCSpawnData[NDCIndex];
				OutNDCSpawnCount.SetAndAdvance(NDCSpawnCount);

				//Do another pass to calculate our SpawnIndex for this NDC within the total count for this NDC.
				if (OutNDCSpawnIndex.IsValid())
				{
					uint32 NDCSpawnIndex = 0;
					uint32 Count = NDCSpawnCount;
					MaxBucketExecIndex = 0;
					for (int32 BucketIdx = 0; BucketIdx < 16; ++BucketIdx)
					{
						uint32 BucketSize = (1 << 15) >> BucketIdx;
						uint32 Mask = (0xFFFF >> (BucketIdx + 1));
						uint32 CountMasked = Count & ~Mask;
						Count &= Mask;
						uint32 NumNDCEntriesInBucket = CountMasked >> (15 - BucketIdx);
						uint32 NumEntriesInBucket = NDCSpawnBukets[BucketIdx];
						uint32 NumNDCInstancesInBucket = NumNDCEntriesInBucket * BucketSize;

						int32 MinBucketExecIndex = MaxBucketExecIndex;
						MaxBucketExecIndex += BucketSize * NumEntriesInBucket;
						if (ExecIndex < MaxBucketExecIndex && NumNDCInstancesInBucket > 0)
						{
							uint32 NDCIndexEntry = (ExecIndex - MinBucketExecIndex) >> (15 - BucketIdx);

							uint32 MinNDCBucketExecIndex = MinBucketExecIndex + (BucketSize * NDCIndexEntry);

							NDCSpawnIndex += (ExecIndex - MinNDCBucketExecIndex);
							break;
						}
						else
						{
							NDCSpawnIndex += NumNDCInstancesInBucket;
						}
					}
					OutNDCSpawnIndex.SetAndAdvance(NDCSpawnIndex);
				}
			}
			else
			{
				OutNDCIndex.SetAndAdvance(INDEX_NONE);
				OutNDCSpawnCount.SetAndAdvance(INDEX_NONE);
				OutNDCSpawnIndex.SetAndAdvance(INDEX_NONE);
			}
		};
		
		if(InEmitterID.IsConstant())
		{		
			//TODO: Can likely vectorize all this.
			const FNiagaraEmitterID EmitterID = InEmitterID.GetAndAdvance();
			FNiagaraEmitterInstance* EmitterInst = SystemInstance->GetEmitterByID(EmitterID);
			if(const FNDIDataChannelRead_EmitterInstanceData* EmitterInstData = InstData->EmitterInstanceData.Find(EmitterInst))
			{
				for (int32 i = 0; i < Context.GetNumInstances(); ++i)
				{
					CalculateNDCSpawnInfo(*EmitterInstData);
				}
			}
			else
			{
				for (int32 i = 0; i < Context.GetNumInstances(); ++i)
				{
					OutNDCIndex.SetAndAdvance(INDEX_NONE);
					OutNDCSpawnCount.SetAndAdvance(INDEX_NONE);
					OutNDCSpawnIndex.SetAndAdvance(INDEX_NONE);
				}
			}
		}
		else
		{
			for (int32 i = 0; i < Context.GetNumInstances(); ++i)
			{
				const FNiagaraEmitterID EmitterID = InEmitterID.GetAndAdvance();
				FNiagaraEmitterInstance* EmitterInst = SystemInstance->GetEmitterByID(EmitterID);
				if (const FNDIDataChannelRead_EmitterInstanceData* EmitterInstData = InstData->EmitterInstanceData.Find(EmitterInst))
				{
					CalculateNDCSpawnInfo(*EmitterInstData);
				}
				else
				{
					OutNDCIndex.SetAndAdvance(INDEX_NONE);
					OutNDCSpawnCount.SetAndAdvance(INDEX_NONE);
					OutNDCSpawnIndex.SetAndAdvance(INDEX_NONE);
				}
			}			
		}
	}	
	else
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			OutNDCIndex.SetAndAdvance(INDEX_NONE);
			OutNDCSpawnCount.SetAndAdvance(INDEX_NONE);
			OutNDCSpawnIndex.SetAndAdvance(INDEX_NONE);
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::Read(FVectorVMExternalFunctionContext& Context, int32 FuncIdx)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelRead_Read);
	VectorVM::FUserPtrHandler<FNDIDataChannelReadInstanceData> InstData(Context);
	FNDIInputParam<int32> InIndex(Context);

	FNDIOutputParam<FNiagaraBool> OutSuccess(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FunctionToDataSetBinding* BindingInfo = InstData->FuncToDataSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FuncToDataSetBindingInfo[FuncIdx].Get() : nullptr;
	FNDIVariadicOutputHandler<16> VariadicOutputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	FNiagaraDataBuffer* Data = InstData->GetReadBufferCPU(bReadPrevFrame);
	if (Data && BindingInfo && INiagaraModule::DataChannelsEnabled())
 	{
 		//FString Label = TEXT("NDIDataChannelRead::Read() - ");
 		//Data->Dump(0, Data->GetNumInstances(), Label);

		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			int32 Index = InIndex.GetAndAdvance();

			bool bProcess = (uint32)Index < Data->GetNumInstances();

			auto FloatFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<float>& FloatData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
				{
					float* Src = Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, (uint32)Index);
					*FloatData.GetDestAndAdvance() = *Src;
				}
			};
			auto IntFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<int32>& IntData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
				{
					int32* Src = Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, (uint32)Index);
					*IntData.GetDestAndAdvance() = *Src;
				}
			};
			auto HalfFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<FFloat16>& HalfData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
					*HalfData.GetDestAndAdvance() = *Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, (uint32)Index);
			};
			bool bSuccess = VariadicOutputs.Process(bProcess, 1, BindingInfo, FloatFunc, IntFunc, HalfFunc);

			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(bSuccess);
			}
		}
	}
	else
	{
		VariadicOutputs.Fallback(Context.GetNumInstances());
		if (OutSuccess.IsValid())
		{
			FMemory::Memzero(OutSuccess.Data.GetDest(), sizeof(int32) * Context.GetNumInstances());
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::Consume(FVectorVMExternalFunctionContext& Context, int32 FuncIdx)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelRead_Consume);
	VectorVM::FUserPtrHandler<FNDIDataChannelReadInstanceData> InstData(Context);
	FNDIInputParam<bool> InConsume(Context);

	FNDIOutputParam<bool> OutSuccess(Context);
	FNDIOutputParam<int32> OutIndex(Context);

	const FNDIDataChannelFunctionInfo& FuncInfo = CompiledData.GetFunctionInfo()[FuncIdx];
	const FNDIDataChannel_FunctionToDataSetBinding* BindingInfo = InstData->FuncToDataSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FuncToDataSetBindingInfo[FuncIdx].Get() : nullptr;
	FNDIVariadicOutputHandler<16> VariadicOutputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	//TODO: Optimize for constant bConsume.
	//TODO: Optimize for long runs of bConsume==true;
	bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	FNiagaraDataBuffer* Data = InstData->GetReadBufferCPU(bReadPrevFrame);
	if (Data && BindingInfo && INiagaraModule::DataChannelsEnabled())
	{
		for (int32 i = 0; i < Context.GetNumInstances(); ++i)
		{
			bool bConsume = InConsume.GetAndAdvance();

			bool bSuccess = false;
			int32 Index = InstData->ConsumeIndex;
			if (bConsume)
			{
				while (Index < (int32)Data->GetNumInstances() && InstData->ConsumeIndex.compare_exchange_strong(Index, Index + 1) == false)
				{
					Index = InstData->ConsumeIndex;
				}

				bConsume &= (uint32)Index < Data->GetNumInstances();

				auto FloatFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<float>& FloatData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*FloatData.GetDestAndAdvance() = *Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, (uint32)Index);
				};
				auto IntFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<int32>& IntData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*IntData.GetDestAndAdvance() = *Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, (uint32)Index);
				};
				auto HalfFunc = [Data, Index](const FNDIDataChannelRegisterBinding& VMBinding, VectorVM::FExternalFuncRegisterHandler<FFloat16>& HalfData)
				{
					if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
						*HalfData.GetDestAndAdvance() = *Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, (uint32)Index);
				};
				bSuccess = VariadicOutputs.Process(bConsume, 1, BindingInfo, FloatFunc, IntFunc, HalfFunc);
			}
			else
			{
				VariadicOutputs.Fallback(1);
			}

			if (OutSuccess.IsValid())
			{
				OutSuccess.SetAndAdvance(bSuccess);
			}
			if (OutIndex.IsValid())
			{
				OutIndex.SetAndAdvance(bSuccess ? Index : INDEX_NONE);
			}
		}
	}
	else
	{
		VariadicOutputs.Fallback(Context.GetNumInstances());

		if (OutSuccess.IsValid())
		{
			FMemory::Memzero(OutSuccess.Data.GetDest(), sizeof(int32) * Context.GetNumInstances());
		}

		if (OutIndex.IsValid())
		{
			FMemory::Memset(OutSuccess.Data.GetDest(), 0xFF, sizeof(int32) * Context.GetNumInstances());
		}
	}
}

void UNiagaraDataInterfaceDataChannelRead::SpawnConditional(FVectorVMExternalFunctionContext& Context, int32 FuncIdx)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelRead_Spawn);

	//UE_LOG(LogNiagara, Warning, TEXT("UNiagaraDataInterfaceDataChannelRead::SpawnConditional - %u"), FuncIdx);

	//This should only be called from emitter scripts and since it has per instance data then we process them individually.
	check(Context.GetNumInstances() == 1);

	VectorVM::FUserPtrHandler<FNDIDataChannelReadInstanceData> InstData(Context);

	FNiagaraSystemInstance* SystemInstance = InstData->Owner;
	check(SystemInstance);

	//Binding info can be null here as we can be spawning without any conditions, i.e. no variadic parameters to the function.
	const FNDIDataChannel_FunctionToDataSetBinding* BindingInfo = InstData->FuncToDataSetBindingInfo.IsValidIndex(FuncIdx) ? InstData->FuncToDataSetBindingInfo[FuncIdx].Get() : nullptr;

	FNDIInputParam<FNiagaraBool> InEnabled(Context);
	FNDIInputParam<FNiagaraEmitterID> InEmitterID(Context);	

	FNDIInputParam<int32> InMode(Context);
	FNDIInputParam<int32> InOp(Context);
	FNDIInputParam<int32> InSpawnMin(Context);
	FNDIInputParam<int32> InSpawnMax(Context);

	FNDIVariadicInputHandler<16> VariadicInputs(Context, BindingInfo);//TODO: Make static / avoid allocation

	const FNiagaraEmitterID EmitterID = InEmitterID.GetAndAdvance();
	const int32 NumEmitters = SystemInstance->GetEmitters().Num();
	FNiagaraEmitterInstance* EmitterInst = SystemInstance->GetEmitterByID(EmitterID);

	FNiagaraDataChannelData* DataChannelData = InstData->DataChannelData.Get();

	const bool bReadPrevFrame = bReadCurrentFrame == false || GNDCReadForcePrevFrame;
	FNiagaraDataBuffer* Data = DataChannelData ? DataChannelData->GetCPUData(bReadPrevFrame) : nullptr;

	const bool bSpawn = INiagaraModule::DataChannelsEnabled() && Data && Data->GetNumInstances() > 0 && EmitterInst && EmitterInst->IsActive() && InEnabled.GetAndAdvance();
	if(bSpawn)
	{
		FNDIRandomHelperFromStream RandHelper(Context);

		ENDIDataChannelSpawnMode Mode = (ENDIDataChannelSpawnMode)InMode.GetAndAdvance();
		ENiagaraConditionalOperator Op = (ENiagaraConditionalOperator)InOp.GetAndAdvance();
		int32 NumDataChannelInstances = Data->GetNumInstances();

		//Is mode none or invalid?
		if (NumDataChannelInstances == 0 || (int32)Mode == (int32)ENDIDataChannelSpawnMode::None || (int32)Mode < 0 || (int32)Mode >= (int32)ENDIDataChannelSpawnMode::Max)
		{
			return;
		}

		int32 SpawnMin = InSpawnMin.GetAndAdvance();
		int32 SpawnMax = InSpawnMax.GetAndAdvance();

		//Each data channel element has an additional spawn entry which accumulates across all spawning calls and can be nulled independently by a suppression call.
		FNDIDataChannelRead_EmitterInstanceData& EmitterInstData = InstData->EmitterInstanceData.FindOrAdd(EmitterInst);
		TArray<int32>& EmitterConditionalSpawns = EmitterInstData.NDCSpawnCounts;
		EmitterConditionalSpawns.SetNumZeroed(NumDataChannelInstances);

		for (int32 DataChannelIdx = 0; DataChannelIdx < NumDataChannelInstances; ++DataChannelIdx)
		{
			bool bConditionsPass = true;
			auto FloatFunc = [&bConditionsPass, Op, Data, DataChannelIdx](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<float>& FloatData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
					bConditionsPass &= EvalConditional(Op, FloatData.GetAndAdvance(), *Data->GetInstancePtrFloat(VMBinding.DataSetRegisterIndex, (uint32)DataChannelIdx));
			};
			auto IntFunc = [&bConditionsPass, Op, Data, DataChannelIdx](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<int32>& IntData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
					bConditionsPass &= EvalConditional(Op, IntData.GetAndAdvance(), *Data->GetInstancePtrInt32(VMBinding.DataSetRegisterIndex, (uint32)DataChannelIdx));
			};
			auto HalfFunc = [&bConditionsPass, Op, Data, DataChannelIdx](const FNDIDataChannelRegisterBinding& VMBinding, FNDIInputParam<FFloat16>& HalfData)
			{
				if (VMBinding.DataSetRegisterIndex != INDEX_NONE)
					bConditionsPass &= EvalConditional(Op, HalfData.GetAndAdvance(), *Data->GetInstancePtrHalf(VMBinding.DataSetRegisterIndex, (uint32)DataChannelIdx));
			};
			VariadicInputs.Process(true, 1, BindingInfo, FloatFunc, IntFunc, HalfFunc);
			VariadicInputs.Reset();

			if (bConditionsPass)
			{
				int32 Count = RandHelper.RandRange(DataChannelIdx, SpawnMin, SpawnMax);
				if (Mode == ENDIDataChannelSpawnMode::Accumulate)
				{
					EmitterConditionalSpawns[DataChannelIdx] += Count;
				}
				else if (Mode == ENDIDataChannelSpawnMode::Override)
				{
					EmitterConditionalSpawns[DataChannelIdx] = Count;
				}
			}
		}
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceDataChannelRead::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelCommon"), GetShaderFileHash(NDIDataChannelReadLocal::CommonShaderFile, EShaderPlatform::SP_PCD3D_SM5).ToString());
	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelRead_Common"), GetShaderFileHash(NDIDataChannelReadLocal::TemplateShaderFile_Common, EShaderPlatform::SP_PCD3D_SM5).ToString());
	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelRead_Read"), GetShaderFileHash(NDIDataChannelReadLocal::TemplateShaderFile_Read, EShaderPlatform::SP_PCD3D_SM5).ToString());
	bSuccess &= InVisitor->UpdateString(TEXT("UNiagaraDataInterfaceDataChannelRead_Consume"), GetShaderFileHash(NDIDataChannelReadLocal::TemplateShaderFile_Consume, EShaderPlatform::SP_PCD3D_SM5).ToString());

	bSuccess &= InVisitor->UpdateShaderParameters<NDIDataChannelReadLocal::FShaderParameters>();
	return bSuccess;
}

void UNiagaraDataInterfaceDataChannelRead::GetCommonHLSL(FString& OutHLSL)
{
	Super::GetCommonHLSL(OutHLSL);
	OutHLSL.Appendf(TEXT("#include \"%s\"\n"), NDIDataChannelReadLocal::CommonShaderFile);
}

bool UNiagaraDataInterfaceDataChannelRead::GetFunctionHLSL(FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL)
{
	return	HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelRead, Num) ||
		HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelRead, GetNDCSpawnData) ||
		HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelRead, Read) ||
		HlslGenContext.GetFunctionInfo().DefinitionName == GET_FUNCTION_NAME_CHECKED(UNiagaraDataInterfaceDataChannelRead, Consume);
}

void UNiagaraDataInterfaceDataChannelRead::GetParameterDefinitionHLSL(FNiagaraDataInterfaceHlslGenerationContext& HlslGenContext, FString& OutHLSL)
{
	//Desc:
	// This function iterates over all functions called for this DI in each script and builds the correct hlsl.
	// The main part of the complexity here is dealing with Variadic function parameters.
	// We must interrogate the script to see what parameters are actually accessed and generate hlsl accordingly.
	// Ideally at some future point we can refactor most of this out to a utility helper that will do most of the heavy lifting.
	// Allowing users to simply provide some details and lambdas etc to specify what exactly they'd like to do in the function body with each variadic param.

	//Initially we'll do some preamble, setting up various template strings and args etc.

	//Map of all arguments for various pieces of template code.
	//We have some common code that is shared by all functions.
	//Some code is unique for each function.
	//Some is unique per parameter to each function.
	//Finally there is some that is unique for each sub component of each parameter until we've hit a base type. float/2/3/4 etc.
	TMap<FString, FStringFormatArg> HlslTemplateArgs =
	{
		//Common args for all functions
		{TEXT("ParameterName"),							HlslGenContext.ParameterInfo.DataInterfaceHLSLSymbol},
		
		//Per function args. These will be changed with each function written
		{TEXT("FunctionSymbol"),						FString(TEXT("FunctionSymbol"))},//Function symbol which will be a mangled form from the translator.
		{TEXT("FunctionParameters"),					FString(TEXT("FunctionParameters"))},//Function parameters written into the function signature.
		{TEXT("PerFunctionParameterShaderCode"),		FString(TEXT("PerFunctionParameterShaderCode"))},//Combined string of all code dealing with each of the function parameters.
		//Per function parameter args. These will be changed with each parameter written
		{TEXT("FunctionParameterIndex"),				TEXT("FunctionParameterIndex")},//Function parameter index allowing us to look up the layout information for the correct parameter to the function.
		{TEXT("FunctionParameterName"),					FString(TEXT("FunctionParameterName"))},//Name of this function parameter.
		{TEXT("FunctionParameterType"),					FString(TEXT("FunctionParameterType"))},//Type of this function parameter.
		{TEXT("PerParamRWCode"),						FString(TEXT("PerParamRWCode"))},//Code that does the actual reading or writing to the data channel buffers.
		//Per component/base type args. These will change with every base type we I/O from the Data Channel.
		{TEXT("FunctionParameterComponentBufferType"),	FString(TEXT("FunctionParameterComponentBufferType"))},//The actual base data buffer type being accessed by a particular DataChannel access code line. Float, Int32, Half etc
		{TEXT("FunctionParameterComponentName"),		FString(TEXT("FunctionParameterComponentName"))},//The name/symbol of the actual member of a parameter that we can I/O from the DataChannel via a standard getter/setter.
		{TEXT("FunctionParameterComponentType"),		FString(TEXT("FunctionParameterComponentType"))},//The type of the actual member of a parameter that we can I/O from the DataChannel via a standard getter/setter.
	};

	//Grab refs to per function args we'll change with each function written.
	FStringFormatArg& FunctionSymbol = HlslTemplateArgs.FindChecked(TEXT("FunctionSymbol"));
	FStringFormatArg& FunctionParameters = HlslTemplateArgs.FindChecked(TEXT("FunctionParameters"));
	FStringFormatArg& PerFunctionParameterShaderCode = HlslTemplateArgs.FindChecked(TEXT("PerFunctionParameterShaderCode"));

	//Grab refs to per function args we'll change with each parameter written.
	FStringFormatArg& FunctionParameterIndex = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterIndex"));
	FStringFormatArg& FuncParamName = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterName"));
	FStringFormatArg& FuncParamType = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterType"));
	FStringFormatArg& PerParamRWCode = HlslTemplateArgs.FindChecked(TEXT("PerParamRWCode"));

	//Grab refs to per component/base type args we'll change for every parameter component we access in the Data Channel.
	FStringFormatArg& FunctionParameterComponentBufferType = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterComponentBufferType"));
	FStringFormatArg& FunctionParameterComponentName = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterComponentName"));
	FStringFormatArg& FunctionParameterComponentType = HlslTemplateArgs.FindChecked(TEXT("FunctionParameterComponentType"));

	//Template code for writing handling code for each of the function parameters.
	//Some preamble and an replacement arg into which we write all the actual I/O with the DataChannel buffers.
	FString PerFunctionParameterTemplate = TEXT("\n\
		//Handling for function parameter {FunctionParameterName}\n\
		int FloatRegisterIndex_{FunctionParameterName};\n\
		int Int32RegisterIndex_{FunctionParameterName};\n\
		int HalfRegisterIndex_{FunctionParameterName};\n\
		if(GetParameterLayoutInfo_{ParameterName}({FunctionParameterIndex}, FloatRegisterIndex_{FunctionParameterName}, Int32RegisterIndex_{FunctionParameterName}, HalfRegisterIndex_{FunctionParameterName}))\n\
		{\n\
{PerParamRWCode}\n\
		}\n\
		else\n\
		{\n\
			bOutSuccess = false;\n\
		}\n");

	//Template code for accessing data from the Data Channel's buffers.
	static const FString ReadDataTemplate = TEXT("			NiagaraDataChannelRead_{FunctionParameterComponentType}({ParameterName}_Data{FunctionParameterComponentBufferType}, {ParameterName}_{FunctionParameterComponentBufferType}Stride, {FunctionParameterComponentBufferType}RegisterIndex_{FunctionParameterName}, ElementIndex, {FunctionParameterComponentName});\n\t\t");
	static const FString WriteDataTemplate = TEXT("			NiagaraDataChannelWrite_{FunctionParameterComponentType}({ParameterName}_Data{FunctionParameterComponentBufferType}, {ParameterName}_{FunctionParameterComponentBufferType}Stride, {FunctionParameterComponentBufferType}RegisterIndex_{FunctionParameterName}, ElementIndex, {FunctionParameterComponentName});\n\t\t");

	//Map to store template shader code for each DI function as each is in it's own ush file. We'll load those as needed and place in here for ease of access.
	TMap<FName, FString> FunctionTemplateMap;

	//We may call the same function multiple times so avoid duplicating the same function impl.
	TMap<uint32, FString> FunctionHlslMap;

	//Now lets build some hlsl!
	// 
	//First build the shader code common to all functions.
	FString CommonTemplateFile;
	LoadShaderSourceFile(NDIDataChannelReadLocal::TemplateShaderFile_Common, EShaderPlatform::SP_PCD3D_SM5, &CommonTemplateFile, nullptr);
	OutHLSL += FString::Format(*CommonTemplateFile, HlslTemplateArgs);

	auto GetSignatureHash = [](const FNiagaraFunctionSignature& Sig)
	{
		uint32 Ret = GetTypeHash(Sig.Name);
		for (const FNiagaraVariable& Input : Sig.Inputs)
		{
			Ret = HashCombine(Ret, GetTypeHash(Input));
		}
		for (const FNiagaraVariable& Output : Sig.Outputs)
		{
			Ret = HashCombine(Ret, GetTypeHash(Output));
		}
		return Ret;
	};

	TArray<FNiagaraVariableBase> ParametersAccessed;
	//First iterate over the generated functions to gather all used parameters so we can generate the sorted parameter list for all functions called by this script.
	for (int32 FuncIdx = 0; FuncIdx < HlslGenContext.ParameterInfo.GeneratedFunctions.Num(); ++FuncIdx)
	{
		const FNiagaraDataInterfaceGeneratedFunction& Func = HlslGenContext.ParameterInfo.GeneratedFunctions[FuncIdx];
		const FNiagaraFunctionSignature& Signature = HlslGenContext.Signatures[FuncIdx];

		for (int32 InputIdx = Signature.NumRequiredInputs(); InputIdx < Signature.Inputs.Num(); ++InputIdx)
		{
			const FNiagaraVariable& InputParam = Signature.Inputs[InputIdx];
			ParametersAccessed.AddUnique(InputParam);
		}

		for (int32 OutputIdx = Signature.NumRequiredOutputs(); OutputIdx < Signature.Outputs.Num(); ++OutputIdx)
		{
			const FNiagaraVariable& OutputParam = Signature.Outputs[OutputIdx];
			ParametersAccessed.AddUnique(OutputParam);
		}
	}
	//Sort the parameters so that that generated hlsl can exactly match the runtime ordering of parameters.
	NDIDataChannelUtilities::SortParameters(ParametersAccessed);

	//Utility map for easy access of the right param offset during hlsl gen.
	TMap<FNiagaraVariableBase, uint32> ParamOffsets;
	for (int32 ParamIdx = 0; ParamIdx < ParametersAccessed.Num(); ++ParamIdx)
	{
		ParamOffsets.FindOrAdd(ParametersAccessed[ParamIdx]) = ParamIdx;
	}

	//Now to iterate on the functions and build the hlsl for each as needed.
	for (int32 FuncIdx = 0; FuncIdx < HlslGenContext.ParameterInfo.GeneratedFunctions.Num(); ++FuncIdx)
	{
		const FNiagaraDataInterfaceGeneratedFunction& Func = HlslGenContext.ParameterInfo.GeneratedFunctions[FuncIdx];
		const FNiagaraFunctionSignature& Signature = HlslGenContext.Signatures[FuncIdx];

		//Init/Reset our per function hlsl template args.
		FunctionSymbol.StringValue = HlslGenContext.GetFunctionSignatureSymbol(Signature);
		FunctionParameters.StringValue.Reset();//Reset function parameters ready to rebuild when we iterate over the parameters.
		PerFunctionParameterShaderCode.StringValue.Reset(); //Reset function parameters ready to rebuild when we iterate over the parameters.

		//Generate the function hlsl if we've not done so already for this signature.
		uint32 FuncHash = GetSignatureHash(Signature);
		FString& FunctionHlsl = FunctionHlslMap.FindOrAdd(FuncHash);
		if (FunctionHlsl.IsEmpty())
		{
			//Function that will recurse down a parameter's type and generate the appropriate IO code for all of it's members.
			TFunction<void(bool, UScriptStruct*, FString&)> GenerateRWParameterCode = [&](bool bRead, UScriptStruct* Struct, FString& OutCode)
			{
				//Intercept positions and replace with FVector3fs
				if(Struct == FNiagaraPosition::StaticStruct())
				{
					Struct = FNiagaraTypeDefinition::GetVec3Struct();
				}

				if (Struct == FNiagaraTypeDefinition::GetFloatStruct() || Struct == FNiagaraTypeDefinition::GetVec2Struct() || Struct == FNiagaraTypeDefinition::GetVec3Struct()
					|| Struct == FNiagaraTypeDefinition::GetVec4Struct() || Struct == FNiagaraTypeDefinition::GetColorStruct() || Struct == FNiagaraTypeDefinition::GetQuatStruct())
				{
					FunctionParameterComponentBufferType = TEXT("Float");
					FunctionParameterComponentType = HlslGenContext.GetStructHlslTypeName(FNiagaraTypeDefinition(Struct));
					OutCode += FString::Format(bRead ? *ReadDataTemplate : *WriteDataTemplate, HlslTemplateArgs);
				}
				else if (Struct == FNiagaraTypeDefinition::GetIntStruct())
				{
					FunctionParameterComponentBufferType = TEXT("Int32");
					FunctionParameterComponentType = HlslGenContext.GetStructHlslTypeName(FNiagaraTypeDefinition(Struct));
					OutCode += FString::Format(bRead ? *ReadDataTemplate : *WriteDataTemplate, HlslTemplateArgs);
				}
				else if (Struct == FNiagaraTypeDefinition::GetBoolStruct())
				{
					FunctionParameterComponentBufferType = TEXT("Int32");
					FunctionParameterComponentType = HlslGenContext.GetStructHlslTypeName(FNiagaraTypeDefinition::GetIntDef());
					OutCode += FString::Format(bRead ? *ReadDataTemplate : *WriteDataTemplate, HlslTemplateArgs);
				}
				else
				{
					FString PropertyBaseName = FunctionParameterComponentName.StringValue;

					for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
					{
						FunctionParameterComponentName.StringValue = PropertyBaseName;
						const FProperty* Property = *PropertyIt;

						if (Property->IsA(FFloatProperty::StaticClass()))
						{
							FunctionParameterComponentBufferType = TEXT("Float");
							FunctionParameterComponentType = HlslGenContext.GetStructHlslTypeName(FNiagaraTypeDefinition::GetFloatDef());
							FunctionParameterComponentName.StringValue += FString::Printf(TEXT(".%s"), *HlslGenContext.GetSanitizedSymbolName(Property->GetName()));
							OutCode += FString::Format(bRead ? *ReadDataTemplate : *WriteDataTemplate, HlslTemplateArgs);
						}
						else if (Property->IsA(FIntProperty::StaticClass()))
						{
							FunctionParameterComponentBufferType = TEXT("Int32");
							FunctionParameterComponentType = HlslGenContext.GetStructHlslTypeName(FNiagaraTypeDefinition::GetIntDef());
							FunctionParameterComponentName.StringValue += FString::Printf(TEXT(".%s"), *HlslGenContext.GetSanitizedSymbolName(Property->GetName()));
							OutCode += FString::Format(bRead ? *ReadDataTemplate : *WriteDataTemplate, HlslTemplateArgs);
						}
						else if (const FStructProperty* StructProp = CastFieldChecked<const FStructProperty>(Property))
						{
							FunctionParameterComponentName.StringValue += FString::Printf(TEXT(".%s"), *HlslGenContext.GetSanitizedSymbolName(Property->GetName()));
							FunctionParameterComponentType = HlslGenContext.GetPropertyHlslTypeName(Property);
							GenerateRWParameterCode(bRead, StructProp->Struct, OutCode);
						}
						else
						{
							check(false);
							OutCode += FString::Printf(TEXT("Error! - DataChannel Interface encountered an invalid type when generating it's function hlsl. %s"), *Property->GetName());
						}
					}
				}
			};

			int32 CurrParamIdx = 0;
			int32 CurrVariadicParamIdx = 0;//Track current variadic param index so the shader code can tell which param's layout info to lookup.

			//Iterate through the variadic inputs and output parameters building the function signature and the per parameter function body as we go.
			static FString FirstParamPrefix = TEXT("");
			static FString ParamPrefix = TEXT(", ");
			static FString OutputParamPrefix = TEXT("out");
			static FString InputParamPrefix = TEXT("in");
			auto HandleParameters = [&](TConstArrayView<FNiagaraVariable> Parameters, int32 StartParamIdx, bool bOutput)
			{
				for (int32 ParamIdx = StartParamIdx; ParamIdx < Parameters.Num(); ++ParamIdx)
				{
					const FNiagaraVariable& Param = Parameters[ParamIdx];

					//Reset or init per parameter hlsl template args.

					//Grab the offset for this parameter and embed in the hlsl directly.
					//This will correspond to an entry in the ParameterLayoutBuffer passed to the shader.
					int32 ParamOffset = ParamOffsets.FindChecked(Param);
					FunctionParameterIndex = LexToString(ParamOffset);
					FuncParamName = Param.GetName().ToString();
					FuncParamType = HlslGenContext.GetStructHlslTypeName(Param.GetType());
					PerParamRWCode.StringValue.Reset();

					bool bFirstParam = CurrParamIdx++ == 0;
					FunctionParameters.StringValue += FString::Printf(TEXT("%s%s %s %s"),
						bFirstParam ? *FirstParamPrefix : *ParamPrefix,
						bOutput ? *OutputParamPrefix : *InputParamPrefix,
						*FuncParamType.StringValue, *FuncParamName.StringValue);

					//Reset per component args and build the code for per component/base type Data Channel access.
					FunctionParameterComponentName.StringValue.Reset();
					FunctionParameterComponentType.StringValue.Reset();
					FunctionParameterComponentBufferType.StringValue.Reset();

					//Generate the per component/base type IO code that will be used in the subsequent per paramters shader code template.
					FunctionParameterComponentName = HlslGenContext.GetSanitizedSymbolName(Param.GetName().ToString());
					GenerateRWParameterCode(bOutput, Param.GetType().GetScriptStruct(), PerParamRWCode.StringValue);

					//Generate the whole code for this parameter and append it to the function's per parameter code.
					//This will be used at the end in the per function template.
					PerFunctionParameterShaderCode.StringValue += FString::Format(*PerFunctionParameterTemplate, HlslTemplateArgs);

					++CurrVariadicParamIdx;
				}
			};

			HandleParameters(Signature.Inputs, Signature.NumRequiredInputs(), false);
			HandleParameters(Signature.Outputs, Signature.NumRequiredOutputs(), true);

			//Grab the template file for the function.
			FString& FunctionTemplateFile = FunctionTemplateMap.FindOrAdd(Signature.Name);
			if (FunctionTemplateFile.IsEmpty())
			{
				//Load it if we have not previously.
				LoadShaderSourceFile(NDIDataChannelReadLocal::GetFunctionTemplate(Signature.Name), EShaderPlatform::SP_PCD3D_SM5, &FunctionTemplateFile, nullptr);
			}

			//Finally generate the final code for this function and add it to the final hlsl.
			OutHLSL += FString::Format(*FunctionTemplateFile, HlslTemplateArgs);
			OutHLSL += TEXT("\n");
		}
	}
}

bool UNiagaraDataInterfaceDataChannelRead::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	TArray<FNiagaraFunctionSignature> Funcs;
	GetFunctionsInternal(Funcs);

	for (const FNiagaraFunctionSignature& Func : Funcs)
	{
		if (Func.Name == FunctionSignature.Name && Func.FunctionVersion > FunctionSignature.FunctionVersion)
		{
			//We need to add back any variadic params from the source signature.
			TArray<FNiagaraVariableBase> VariadicInputs;
			FunctionSignature.GetVariadicInputs(VariadicInputs);
			TArray<FNiagaraVariableBase> VariadicOutputs;
			FunctionSignature.GetVariadicOutputs(VariadicOutputs);

			FunctionSignature = Func;
			for (FNiagaraVariableBase& Param : VariadicInputs)
			{
				FunctionSignature.AddInput(Param);
			}
			for (FNiagaraVariableBase& Param : VariadicOutputs)
			{
				FunctionSignature.AddOutput(Param);
			}
			return true;
		}
	}

	return false;
}

#endif

void UNiagaraDataInterfaceDataChannelRead::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<NDIDataChannelReadLocal::FShaderParameters>();
}

void UNiagaraDataInterfaceDataChannelRead::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxy_DataChannelRead& DataInterfaceProxy = Context.GetProxy<FNiagaraDataInterfaceProxy_DataChannelRead>();
	FNiagaraDataInterfaceProxy_DataChannelRead::FInstanceData* InstanceData = DataInterfaceProxy.SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());

	NDIDataChannelReadLocal::FShaderParameters* InstParameters = Context.GetParameterNestedStruct<NDIDataChannelReadLocal::FShaderParameters>();

	bool bSuccess = false;
	if (InstanceData)
	{
		//Find the start offset in the parameter table for this script.
		const FNiagaraCompileHash& ScriptCompileHash = Context.GetComputeInstanceData().Context->GPUScript_RT->GetBaseCompileHash();
		uint32 ParameterOffsetTableIndex = INDEX_NONE;
		if (uint32* ParameterOffsetTableIndexPtr = InstanceData->GPUScriptParameterTableOffsets.Find(ScriptCompileHash))
		{
			ParameterOffsetTableIndex = *ParameterOffsetTableIndexPtr;
		}

		if (InstanceData->ChannelDataRTProxy && ParameterOffsetTableIndex != INDEX_NONE)
		{
			FNiagaraDataBuffer* Data = InstanceData->bReadPrevFrame ? InstanceData->ChannelDataRTProxy->PrevFrameData.GetReference() : InstanceData->ChannelDataRTProxy->GPUDataSet->GetCurrentData();
			if (Data)
			{
				const FReadBuffer& ParameterLayoutBuffer = InstanceData->ParameterLayoutBuffer;

				FRDGBufferSRVRef NDCSpawnDataBufferSRV = Context.GetGraphBuilder().CreateSRV(InstanceData->NDCSpawnDataBuffer, PF_R32_SINT);
				if (NDCSpawnDataBufferSRV && ParameterLayoutBuffer.SRV.IsValid() && ParameterLayoutBuffer.NumBytes > 0)
				{
					InstParameters->ParamOffsetTable = ParameterLayoutBuffer.SRV.IsValid() ? ParameterLayoutBuffer.SRV.GetReference() : FNiagaraRenderer::GetDummyUIntBuffer();
					InstParameters->DataFloat = Data->GetGPUBufferFloat().SRV.IsValid() ? Data->GetGPUBufferFloat().SRV.GetReference() : FNiagaraRenderer::GetDummyFloatBuffer();
					InstParameters->DataInt32 = Data->GetGPUBufferInt().SRV.IsValid() ? Data->GetGPUBufferInt().SRV.GetReference() : FNiagaraRenderer::GetDummyIntBuffer();
					InstParameters->DataHalf = Data->GetGPUBufferHalf().SRV.IsValid() ? Data->GetGPUBufferHalf().SRV.GetReference() : FNiagaraRenderer::GetDummyHalfBuffer();
					InstParameters->ParameterOffsetTableIndex = ParameterOffsetTableIndex;
					InstParameters->Num = Data->GetNumInstances();
					InstParameters->FloatStride = Data->GetFloatStride() / sizeof(float);
					InstParameters->Int32Stride = Data->GetInt32Stride() / sizeof(int32);
					InstParameters->HalfStride = Data->GetHalfStride() / sizeof(FFloat16);

					InstParameters->NDCSpawnDataBuffer = NDCSpawnDataBufferSRV;
					bSuccess = true;
				}
			}
		}
	}

	if (bSuccess == false)
	{
		InstParameters->ParamOffsetTable = FNiagaraRenderer::GetDummyUIntBuffer();
		InstParameters->DataFloat = FNiagaraRenderer::GetDummyFloatBuffer();
		InstParameters->DataInt32 = FNiagaraRenderer::GetDummyIntBuffer();
		InstParameters->DataHalf = FNiagaraRenderer::GetDummyHalfBuffer();
		InstParameters->ParameterOffsetTableIndex = INDEX_NONE;
		InstParameters->Num = 0;
		InstParameters->FloatStride = 0;
		InstParameters->Int32Stride = 0;
		InstParameters->HalfStride = 0;
		
		FRDGBufferRef DummyBuffer = GSystemTextures.GetDefaultBuffer(Context.GetGraphBuilder(), 4, 0u);
		InstParameters->NDCSpawnDataBuffer = Context.GetGraphBuilder().CreateSRV(DummyBuffer);		
	}
}

void FNiagaraDataInterfaceProxy_DataChannelRead::PreStage(const FNDIGpuComputePreStageContext& Context)
{
	FNiagaraDataInterfaceProxy_DataChannelRead::FInstanceData* InstanceData = SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());

	if(InstanceData->NDCSpawnDataBuffer == nullptr)
	{
		InstanceData->NDCSpawnDataBuffer = CreateUploadBuffer<int32>(
			Context.GetGraphBuilder(),
			TEXT("Niagara_NDCReadDI_NDCSpawnData"),
			InstanceData->NDCSpawnData);
	}
}

void FNiagaraDataInterfaceProxy_DataChannelRead::PostSimulate(const FNDIGpuComputePostSimulateContext& Context)
{
	if (Context.IsFinalPostSimulate())
	{
		FNiagaraDataInterfaceProxy_DataChannelRead::FInstanceData* InstanceData = SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());
		InstanceData->NDCSpawnDataBuffer = nullptr;
	}
}

void FNiagaraDataInterfaceProxy_DataChannelRead::ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance)
{
	FNDIDataChannelReadInstanceData_RT& SourceData = *reinterpret_cast<FNDIDataChannelReadInstanceData_RT*>(PerInstanceData);
	FInstanceData& InstData = SystemInstancesToProxyData_RT.FindOrAdd(Instance);

	FRHICommandListBase& RHICmdList = FRHICommandListImmediate::Get();

	InstData.ChannelDataRTProxy = SourceData.ChannelDataRTProxy;
	InstData.bReadPrevFrame = SourceData.bReadPrevFrame;

	InstData.NDCSpawnData = SourceData.NDCSpawnData;
	InstData.NDCSpawnDataBuffer = nullptr;//Clear the RDG buffer ready for re-up to the GPU.

	if (SourceData.bHasFunctionBindingUpdate) 
	{
		//Take the offset map from the source data.
		//This maps from GPU script to that scripts offset into the ParameterLayoutBuffer.
		//Allows us to look up and pass in at SetShaderParameters time.
		InstData.GPUScriptParameterTableOffsets = MoveTemp(SourceData.GPUScriptParameterTableOffsets);

		//Now generate the ParameterLayoutBuffer
		//This contains a table of all parameters used by each GPU script that uses this DI.
		//TODO: This buffer can likely be shared among many instances and stored in the layout manager or in the DI proxy.
		{
			if (InstData.ParameterLayoutBuffer.NumBytes > 0)
			{
				InstData.ParameterLayoutBuffer.Release();
			}

			if(SourceData.GPUScriptParameterOffsetTable.Num() > 0)
			{
				InstData.ParameterLayoutData = SourceData.GPUScriptParameterOffsetTable;
				InstData.ParameterLayoutBuffer.Initialize(RHICmdList, TEXT("NDIDataChannel_ParameterLayoutBuffer"), sizeof(uint32), SourceData.GPUScriptParameterOffsetTable.Num(), EPixelFormat::PF_R32_UINT, BUF_Static, &InstData.ParameterLayoutData);
			}
		}
	}

	SourceData.~FNDIDataChannelReadInstanceData_RT();
}

int32 FNiagaraDataInterfaceProxy_DataChannelRead::PerInstanceDataPassedToRenderThreadSize() const
{
	return sizeof(FNDIDataChannelReadInstanceData_RT);
}

void FNiagaraDataInterfaceProxy_DataChannelRead::GetDispatchArgs(const FNDIGpuComputeDispatchArgsGenContext& Context)
{
	const FNiagaraDataInterfaceProxy_DataChannelRead::FInstanceData* InstanceData = SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());
	if (InstanceData && InstanceData->ChannelDataRTProxy)
	{
		FNiagaraDataBuffer* Data = InstanceData->bReadPrevFrame ? InstanceData->ChannelDataRTProxy->PrevFrameData.GetReference() : InstanceData->ChannelDataRTProxy->GPUDataSet->GetCurrentData();
		if (Data)
		{
			Context.SetDirect(Data->GetNumInstances());
		}
	}
}

#undef LOCTEXT_NAMESPACE
