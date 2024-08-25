// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel.h"

#include "NiagaraWorldManager.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelManager.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "Misc/LazySingleton.h"

#if WITH_NIAGARA_DEBUGGER
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataChannel)

#define LOCTEXT_NAMESPACE "NiagaraDataChannels"

namespace NDCCVars
{
	bool bEmitWarningsOnLateNDCWrites = !UE_BUILD_SHIPPING;
	static FAutoConsoleVariableRef CVarEmitWarningsOnLateNDCWrites(TEXT("fx.Niagara.DataChannels.WarnOnLateWrites"), bEmitWarningsOnLateNDCWrites, TEXT("If true, late writes to NDCs will generate warnings. Late meaning after their final allowed tick group."), ECVF_Default);

	int32 LogWritesToOutputLog = 0;
	static FAutoConsoleVariableRef CVarLogWritesToOutputLog(TEXT("fx.Niagara.DataChannels.LogWritesToOutputLog"), LogWritesToOutputLog, TEXT("0=Disabled, 1=Log write summary, 2=Also write data; If >0, the NDC debugger will print all data channel writes to the output log."), ECVF_Default);

	int32 FrameDataToCapture = 0;
	static FAutoConsoleVariableRef CVarFrameDataToCapture(TEXT("fx.Niagara.DataChannels.FrameDataToCapture"), FrameDataToCapture, TEXT("The number of frames the debugger will capture for write requests."), ECVF_Default);

#if WITH_NIAGARA_DEBUGGER
	FAutoConsoleCommand CmdDumpWriteLog(
		TEXT("fx.Niagara.DataChannels.DumpWriteLog"),
		TEXT("Dump all the currently stored writes to the log (see fx.Niagara.DataChannels.FrameDataToCapture on how many frames are captured)"),
		FConsoleCommandDelegate::CreateStatic(FNiagaraDataChannelDebugUtilities::DumpAllWritesToLog)
	);
#endif
};

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameDataLayout::Init(const TArray<FNiagaraDataChannelVariable>& Variables)
{
	VariableIndices.Reset();
	LwcConverters.Reserve(Variables.Num());
	for (const FNiagaraDataChannelVariable& Var : Variables)
	{
		//Sigh.
		//We must convert from the variable stored var in the data channels definition as we currently cannot serialize/store actual LWC types in FNiagaraTypeDefinitions.
		FNiagaraTypeDefinition LWCType = FNiagaraTypeHelper::GetLWCType(Var.GetType());
		FNiagaraVariableBase LWCVar(LWCType, Var.GetName());

		int32& VarIdx = VariableIndices.Add(LWCVar);
		VarIdx = VariableIndices.Num() - 1;

		FNiagaraLwcStructConverter& Converter = LwcConverters.AddDefaulted_GetRef();
		Converter = FNiagaraTypeRegistry::GetStructConverter(LWCType);
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameData::Init(const UNiagaraDataChannel* InDataChannel)
{
	check(InDataChannel);
	DataChannel = InDataChannel;

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	VariableData.Empty();
	VariableData.SetNum(Layout.VariableIndices.Num());
	for(const auto& Pair : Layout.VariableIndices)
	{
		int32 Index = Pair.Value;
		VariableData[Index].Init(Pair.Key);
	}
}

void FNiagaraDataChannelGameData::Empty()
{
	NumElements = 0;
	for (FNiagaraDataChannelVariableBuffer& VarData : VariableData)
	{
		VarData.Empty();
	}
}

void FNiagaraDataChannelGameData::BeginFrame()
{
	bool bKeepPrevious = DataChannel->KeepPreviousFrameData();
	PrevNumElements = bKeepPrevious ? NumElements : 0;
	NumElements = 0;
	for (FNiagaraDataChannelVariableBuffer& VarData : VariableData)
	{
		VarData.BeginFrame(bKeepPrevious);
	}
}

void FNiagaraDataChannelGameData::SetNum(int32 NewNum)
{
	NumElements = NewNum;
	for (auto& Buffer : VariableData)
	{
		Buffer.SetNum(NewNum);
	}
}

void FNiagaraDataChannelGameData::Reserve(int32 NewNum)
{
	//NumElements = NewNum;
	for (auto& Buffer : VariableData)
	{
		Buffer.Reserve(NewNum);
	}
}

FNiagaraDataChannelVariableBuffer* FNiagaraDataChannelGameData::FindVariableBuffer(const FNiagaraVariableBase& Var)
{
	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	const FNiagaraTypeDefinition& VarType = Var.GetType();
	for(auto& Pair : Layout.VariableIndices)
	{
		const FNiagaraVariableBase& LayoutVar = Pair.Key;
		const FNiagaraTypeDefinition& LayoutVarType = LayoutVar.GetType();
		int32 Index = Pair.Value;
		
		if(Var.GetName() == LayoutVar.GetName())
		{
			//For enum variables we'll hack things a little so that correctly named ints also match. This gets around some limitations in calling code not being able to provide the correct enum types.
			if(VarType == LayoutVarType || (LayoutVarType.IsEnum() && VarType == FNiagaraTypeDefinition::GetIntDef()))
			{
				return &VariableData[Index];
			}
		}
	}
	return nullptr;
}

void FNiagaraDataChannelGameData::WriteToDataSet(FNiagaraDataBuffer* DestBuffer, int32 DestStartIdx, FVector3f SimulationLwcTile)
{
	const FNiagaraDataSetCompiledData& CompiledData = DestBuffer->GetOwner()->GetCompiledData();

	static TArray<uint8> LWCConversionBuffer;

	int32 NumInstances = NumElements;

	if (NumInstances == 0)
	{
		return;
	}
	
	DestBuffer->SetNumInstances(DestStartIdx + NumInstances);

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();

	for (const TPair<FNiagaraVariableBase, int32>& VarIndexPair : Layout.VariableIndices)
	{
		FNiagaraVariableBase Var = VarIndexPair.Key;
		int32 VarIndex = VarIndexPair.Value;
		FNiagaraDataChannelVariableBuffer& VarBuffer = VariableData[VarIndex];
		uint8* SrcDataBase = VarBuffer.Data.GetData();
		
		FNiagaraVariableBase SimVar = Var;

		//Convert from LWC types to Niagara Simulation Types where required.
		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
		{
			SimVar.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(CastChecked<UScriptStruct>(Var.GetType().GetStruct()), ENiagaraStructConversion::Simulation)));
		}

		//Niagara Positions are a special case where we're actually storing them as FVectors in the game level data and must convert down to actual Positions/FVector3f in the sim data.
		if(Var.GetType() == FNiagaraTypeDefinition::GetPositionDef())
		{
			Var.SetType(FNiagaraTypeHelper::GetVectorDef());
		}

		int32 SimVarIndex = CompiledData.Variables.IndexOfByKey(SimVar);
		if (SimVarIndex == INDEX_NONE)
		{
			continue; //Did not find this variable in the dataset. Warn?
		}

		int32 SrcVarSize = Var.GetSizeInBytes();
		int32 DestVarSize = SimVar.GetSizeInBytes();
		const FNiagaraVariableLayoutInfo& SimLayout = CompiledData.VariableLayouts[SimVarIndex];

		int32 FloatCompIdx = SimLayout.GetFloatComponentStart();
		int32 IntCompIdx = SimLayout.GetInt32ComponentStart();
		int32 HalfCompIdx = SimLayout.GetHalfComponentStart();

		TFunction<void(UScriptStruct*, UScriptStruct*, uint8*)> WriteData;
		WriteData = [&](UScriptStruct* SrcStruct, UScriptStruct* DestStruct, uint8* SrcPropertyBase)
		{
			//TODO: why doesn't this just use FNiagaraLwcStructConverter?
			
			//Write all data from the data channel into the data set. Converting into from LWC into the local LWC Tile space as we go.

			uint8* SrcData = SrcPropertyBase;

			//Positions are a special case that are stored as FVectors in game data but converted to an LWCTile local FVector3f in simulation data.
			if (DestStruct == FNiagaraTypeDefinition::GetPositionStruct())
			{	
				float* DestX = DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
				float* DestY = DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
				float* DestZ = DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
								
				for (int32 i = 0; i < NumInstances; ++i)
				{
					FVector* Src = reinterpret_cast<FVector*>((SrcData + i * SrcVarSize));

					FVector3f SimLocalSWC = FVector3f((*Src) - FVector(SimulationLwcTile) * FLargeWorldRenderScalar::GetTileSize());
					*DestX++ = SimLocalSWC.X;
					*DestY++ = SimLocalSWC.Y;
					*DestZ++ = SimLocalSWC.Z;
				}
			}
			else
			{
				TFieldIterator<FProperty> SrcPropertyIt(SrcStruct, EFieldIteratorFlags::IncludeSuper);
				TFieldIterator<FProperty> DestPropertyIt(DestStruct, EFieldIteratorFlags::IncludeSuper);
				for (; SrcPropertyIt; ++SrcPropertyIt, ++DestPropertyIt)
				{
					FProperty* SrcProperty = *SrcPropertyIt;
					FProperty* DestProperty = *DestPropertyIt;
					SrcData = SrcPropertyBase + SrcProperty->GetOffset_ForInternal();

					//Convert any LWC doubles to floats. //TODO: Insert LWCTile... probably need to explicitly check for vectors etc.
					if (SrcProperty->IsA(FDoubleProperty::StaticClass()))
					{
						check(DestProperty->IsA(FFloatProperty::StaticClass()));
						float* Dest = DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							double* Src = reinterpret_cast<double*>((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FFloatProperty::StaticClass()))
					{
						float* Dest = DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							float* Src = reinterpret_cast<float*>((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FUInt16Property::StaticClass()))
					{
						FFloat16* Dest = DestBuffer->GetInstancePtrHalf(HalfCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							FFloat16* Src = reinterpret_cast<FFloat16*>((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FIntProperty::StaticClass()) || SrcProperty->IsA(FBoolProperty::StaticClass()))
					{
						int32* Dest = DestBuffer->GetInstancePtrInt32(IntCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							int32* Src = reinterpret_cast<int32*>((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					//Should be able to support double easily enough
					else if (FStructProperty* StructProp = CastField<FStructProperty>(SrcProperty))
					{
						FStructProperty* DestStructProp = CastField<FStructProperty>(DestProperty);
						WriteData(StructProp->Struct, DestStructProp->Struct, SrcData);
					}
					else
					{
						checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *SrcProperty->GetName(), *SrcProperty->GetClass()->GetName());
					}
				}
			}
		};
		WriteData(Var.GetType().GetScriptStruct(), SimVar.GetType().GetScriptStruct(), SrcDataBase);
	}
}
void FNiagaraDataChannelGameData::AppendFromGameData(const FNiagaraDataChannelGameData& GameData)
{
	check(GetDataChannel() == GameData.GetDataChannel());

	NumElements += GameData.Num();
	TConstArrayView<FNiagaraDataChannelVariableBuffer> SrcBuffers = GameData.GetVariableBuffers();
	for(int32 i=0; i < SrcBuffers.Num(); ++i)
	{
		const FNiagaraDataChannelVariableBuffer& SrcBuffer = SrcBuffers[i];
		FNiagaraDataChannelVariableBuffer& DestBuffer = VariableData[i];
		DestBuffer.Data.Append(SrcBuffer.Data);
	}
}

void FNiagaraDataChannelGameData::AppendFromDataSet(const FNiagaraDataBuffer* SrcBuffer, FVector3f SimulationLwcTile)
{
	const FNiagaraDataSetCompiledData& CompiledData = SrcBuffer->GetOwner()->GetCompiledData();

	static TArray<uint8> LWCConversionBuffer;

	int32 NumInstances = SrcBuffer->GetNumInstances();
	int32 StartIndex = NumElements;
	NumElements += NumInstances;

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	for (const TPair<FNiagaraVariableBase, int32>& VarIndexPair : Layout.VariableIndices)
	{
		FNiagaraVariableBase Var = VarIndexPair.Key;
		int32 VarIndex = VarIndexPair.Value;
		FNiagaraDataChannelVariableBuffer& VarBuffer = VariableData[VarIndex];

		VarBuffer.SetNum(NumElements);
		
		FNiagaraVariableBase SimVar = Var;
		if (FNiagaraTypeHelper::IsLWCType(Var.GetType()))
		{
			SimVar.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
		}

		//Niagara Positions are a special case where we're actually storing them as FVectors in the game level data and must convert down to actual Positions/FVector3f in the sim data and visa versa
		if(Var.GetType() == FNiagaraTypeDefinition::GetPositionDef())
		{
			Var.SetType(FNiagaraTypeHelper::GetVectorDef());
		}
		int32 VarSize = Var.GetSizeInBytes();

		uint8* DestDataBase = VarBuffer.Data.GetData() + StartIndex * VarSize;
		
		int32 SimVarIndex = CompiledData.Variables.IndexOfByKey(SimVar);
		if (SimVarIndex == INDEX_NONE)
		{
			continue; //Did not find this variable in the dataset. Warn?
		}

		const FNiagaraVariableLayoutInfo& SimLayout = CompiledData.VariableLayouts[SimVarIndex];
		
		int32 FloatCompIdx = SimLayout.GetFloatComponentStart();
		int32 IntCompIdx = SimLayout.GetInt32ComponentStart();
		int32 HalfCompIdx = SimLayout.GetHalfComponentStart();
		
		TFunction<void(UScriptStruct*, UScriptStruct*, uint8*)> ReadData;
		ReadData = [&](UScriptStruct* SrcStruct, UScriptStruct* DestStruct, uint8* DestDataBase)
		{
			//Read all data from the simulation and place in the game level buffers. Converting from LWC local tile space where needed.

			//Special case for writing to Niagara Positions. We must offset the data into the simulation local LWC space.
			if (SrcStruct == FNiagaraTypeDefinition::GetPositionStruct())
			{
				const float* SrcX = reinterpret_cast<const float*>(SrcBuffer->GetComponentPtrFloat(FloatCompIdx++));
				const float* SrcY = reinterpret_cast<const float*>(SrcBuffer->GetComponentPtrFloat(FloatCompIdx++));
				const float* SrcZ = reinterpret_cast<const float*>(SrcBuffer->GetComponentPtrFloat(FloatCompIdx++));

				for (int32 i = 0; i < NumInstances; ++i)
				{
					FVector* Dest = reinterpret_cast<FVector*>(DestDataBase + VarSize * i);
					*Dest = FVector(*SrcX++, *SrcY++, *SrcZ++) + FVector(SimulationLwcTile) * FLargeWorldRenderScalar::GetTileSize();
				}
			}
			else
			{
				TFieldIterator<FProperty> SrcPropertyIt(SrcStruct, EFieldIteratorFlags::IncludeSuper);
				TFieldIterator<FProperty> DestPropertyIt(DestStruct, EFieldIteratorFlags::IncludeSuper);
				for (; SrcPropertyIt; ++SrcPropertyIt, ++DestPropertyIt)
				{
					FProperty* SrcProperty = *SrcPropertyIt;
					FProperty* DestProperty = *DestPropertyIt;
					int32 DestOffset = DestProperty->GetOffset_ForInternal();
					uint8* DestData = DestDataBase + DestOffset;
					if (DestPropertyIt->IsA(FDoubleProperty::StaticClass()))
					{
						double* Dest = reinterpret_cast<double*>(DestData);
						const float* Src = reinterpret_cast<const float*>(SrcBuffer->GetComponentPtrFloat(FloatCompIdx++));

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = reinterpret_cast<double*>(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FFloatProperty::StaticClass()))
					{
						float* Dest = reinterpret_cast<float*>(DestData);
						const float* Src = reinterpret_cast<const float*>(SrcBuffer->GetComponentPtrFloat(FloatCompIdx++));

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = reinterpret_cast<float*>(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FUInt16Property::StaticClass()))
					{
						FFloat16* Dest = reinterpret_cast<FFloat16*>(DestData);
						const FFloat16* Src = reinterpret_cast<const FFloat16*>(SrcBuffer->GetComponentPtrHalf(HalfCompIdx++));

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = reinterpret_cast<FFloat16*>(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FIntProperty::StaticClass()) || DestPropertyIt->IsA(FBoolProperty::StaticClass()))
					{
						int32* Dest = reinterpret_cast<int32*>(DestData);
						const int32* Src = reinterpret_cast<const int32*>(SrcBuffer->GetComponentPtrInt32(IntCompIdx++));

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = reinterpret_cast<int32*>(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (FStructProperty* SrcStructProp = CastField<FStructProperty>(SrcProperty))
					{
						FStructProperty* DestStructProp = CastField<FStructProperty>(DestProperty);
						ReadData(SrcStructProp->Struct, DestStructProp->Struct, DestData);
					}
					else
					{
						checkf(false, TEXT("Property(%s) Class(%s) is not a supported type"), *SrcProperty->GetName(), *SrcProperty->GetClass()->GetName());
					}
				}
			}
		};
		ReadData(SimVar.GetType().GetScriptStruct(), Var.GetType().GetScriptStruct(), DestDataBase);
	}
}

void FNiagaraDataChannelGameData::SetFromSimCache(const FNiagaraVariableBase& SourceVar, TConstArrayView<uint8> Data, int32 Size)
{
	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	if (const int* Index = Layout.VariableIndices.Find(SourceVar))
	{
		if (VariableData[*Index].Size == Size)
		{
			VariableData[*Index].Data = Data;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelDataProxy::BeginFrame(bool bKeepPreviousFrameData)
{
	check(GPUDataSet);
	check(GPUDataSet->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim);

}

void FNiagaraDataChannelDataProxy::EndFrame(FNiagaraGpuComputeDispatchInterface* DispathInterface, FRHICommandListImmediate& CmdList, const TArray<FNiagaraDataBufferRef>& BuffersForGPU)
{
	check(GPUDataSet);
	check(GPUDataSet->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim);

	PrevFrameData = GPUDataSet->GetCurrentData();

	uint32 NumInstance = 0;
	for (auto& Buffer : BuffersForGPU)
	{
		NumInstance += Buffer->GetNumInstances();
	}

	GPUDataSet->BeginSimulate();
	FNiagaraDataBuffer* DestBuffer = GPUDataSet->GetDestinationData();	
	DestBuffer->PushCPUBuffersToGPU(BuffersForGPU, true, CmdList, DispathInterface->GetFeatureLevel(), GetDebugName());
	GPUDataSet->EndSimulate();

	//For now we need not deal with the instance count manager but when we do GPU->GPU writes we will
	//DestBuffer->SetGPUInstanceCountBufferOffset()
}

void FNiagaraDataChannelDataProxy::Reset()
{
	PrevFrameData = nullptr;
}

//////////////////////////////////////////////////////////////////////////

FNiagaraDataChannelData::FNiagaraDataChannelData(UNiagaraDataChannelHandler* Owner)
{
	const UNiagaraDataChannel* DataChannel = Owner->GetDataChannel();
	check(DataChannel);

	GameData = DataChannel->CreateGameData();

	CPUSimData = new FNiagaraDataSet();
	GPUSimData = new FNiagaraDataSet();


	const FNiagaraDataSetCompiledData& CompiledData = DataChannel->GetCompiledData(ENiagaraSimTarget::CPUSim);
	const FNiagaraDataSetCompiledData& CompiledDataGPU = DataChannel->GetCompiledData(ENiagaraSimTarget::GPUComputeSim);
	CPUSimData->Init(&CompiledData);
	GPUSimData->Init(&CompiledDataGPU);

	//TODO: Send game data to GPU direct without staging.
	GameDataStaging = new FNiagaraDataSet();
	GameDataStaging->Init(&CompiledData);

	CPUSimData->BeginSimulate();
	CPUSimData->EndSimulate();

	RTProxy.Reset(new FNiagaraDataChannelDataProxy());
	RTProxy->GPUDataSet = GPUSimData;
	#if !UE_BUILD_SHIPPING
	RTProxy->DebugName = FString::Printf(TEXT("%s__GPUData"), *DataChannel->GetName());
	#endif
}

FNiagaraDataChannelData::~FNiagaraDataChannelData()
{
	GameData.Reset();

	/** We defer the deletion of the dataset to the RT to be sure all in-flight RT commands have finished using it.*/
	ENQUEUE_RENDER_COMMAND(FDeleteContextCommand)(
		[CPUDataChannelDataSet = CPUSimData, GPUDataChannelDataSet = GPUSimData, ReleasedRTProxy = RTProxy.Release()](FRHICommandListImmediate& RHICmdList)
	{
		if (CPUDataChannelDataSet != nullptr)
		{
			delete CPUDataChannelDataSet;
		}
		if (GPUDataChannelDataSet != nullptr)
		{
			delete GPUDataChannelDataSet;
		}
		if (ReleasedRTProxy)
		{
			delete ReleasedRTProxy;
		}
	}
	);
	CPUSimData = nullptr;
	GPUSimData = nullptr;
}

void FNiagaraDataChannelData::Reset()
{
	FScopeLock Lock(&PublishCritSec);
	PublishRequests.Reset();
	BuffersForGPU.Reset();
	
	PrevCPUSimData = nullptr;	

	GameData->Empty();
	CPUSimData->ResetBuffers();
	GPUSimData->ResetBuffers();
	GameDataStaging->ResetBuffers();
	ENQUEUE_RENDER_COMMAND(FResetProxyCommand)(
		[RT_Proxy = RTProxy.Get()](FRHICommandListImmediate& RHICmdList)
	{
		if (RT_Proxy)
		{
			RT_Proxy->Reset();
		}
	}
	);
}

void FNiagaraDataChannelData::BeginFrame(UNiagaraDataChannelHandler* Owner)
{
	GameData->BeginFrame();

	bool bRequirePreviousData = Owner->GetDataChannel()->KeepPreviousFrameData();
	if (bRequirePreviousData)
	{
		PrevCPUSimData = CPUSimData->GetCurrentData();
	}

	//Grab a new buffer to store the CPU data.
	CPUSimData->BeginSimulate();
	CPUSimData->EndSimulate();
	
	check(RTProxy);
	if (FNiagaraGpuComputeDispatchInterface* DispathInterface = FNiagaraGpuComputeDispatchInterface::Get(Owner->GetWorld()))
	{
		ENQUEUE_RENDER_COMMAND(FDataChannelProxyBeginFrame) (
			[RT_Proxy = RTProxy.Get(), bRequirePreviousData](FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->BeginFrame(bRequirePreviousData);
		});
	}
}

void FNiagaraDataChannelData::EndFrame(UNiagaraDataChannelHandler* Owner)
{
	//We must do one final tick to process any final items generated by the last things to tick this frame.
	ConsumePublishRequests(Owner, TG_LastDemotable);

	check(RTProxy);
	if (FNiagaraGpuComputeDispatchInterface* DispathInterface = FNiagaraGpuComputeDispatchInterface::Get(Owner->GetWorld()))
	{
		ENQUEUE_RENDER_COMMAND(FDataChannelProxyEndFrame) (
			[DispathInterface, RT_Proxy = RTProxy.Get(), RT_BuffersForGPU = MoveTemp(BuffersForGPU)](FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->EndFrame(DispathInterface, CmdList, RT_BuffersForGPU);
		});
	}
	BuffersForGPU.Reset();
}

int32 FNiagaraDataChannelData::ConsumePublishRequests(UNiagaraDataChannelHandler* Owner, const ETickingGroup& TickGroup)
{
	check(IsValid(Owner));

	//There should be no access on other threads at this point anyway but lock just to be safe.
	FScopeLock Lock(&PublishCritSec);

	const UNiagaraDataChannel* DataChannel = Owner->GetDataChannel();
	if(PublishRequests.Num() == 0 || DataChannel == nullptr)
	{
		return 0;
	}
	
	if(NDCCVars::bEmitWarningsOnLateNDCWrites && DataChannel->ShouldEnforceTickGroupReadWriteOrder())
	{
		ETickingGroup PublishSourceTG = static_cast<ETickingGroup>(FMath::Clamp(static_cast<int32>(Owner->GetCurrentTickGroup()) - 1, 0, static_cast<int32>(ETickingGroup::TG_MAX) - 1));//We're consuming from the previous TG.
		ETickingGroup FinalWriteTG = DataChannel->GetFinalWriteTickGroup();

		//TODO: Possibly allow late writes to be deferred to the next frame?
		if(PublishSourceTG > FinalWriteTG)
		{
			static UEnum* TGEnum = StaticEnum<ETickingGroup>();
			UE_LOG(LogNiagara, Warning, TEXT("Data Channel %s is being written to in Tick Group %s which is after it's final write tick group %s. This may cause incorrect read / write ordering and missed data.")
			, *DataChannel->GetAsset()->GetName()
			, *TGEnum->GetDisplayNameTextByValue((int32)PublishSourceTG).ToString()
			, *TGEnum->GetDisplayNameTextByValue((int32)FinalWriteTG).ToString());
		}
	}

	UWorld* World = Owner->GetWorld();
	check(IsValid(World));

	int32 GameDataOrigSize = GameData->Num();
	int32 CPUDataOrigSize = CPUSimData->GetCurrentData()->GetNumInstances();
	int32 NewGameDataChannel = GameDataOrigSize;
	int32 NewCPUDataChannel = CPUDataOrigSize;

	//Do a pass to gather the new total size for our DataChannel data.

	//Each DI that generates DataChannel can control whether it's pushed to Game/CPU/GPU.
	int32 RequestCount = PublishRequests.Num();
	BuffersForGPU.Reserve(BuffersForGPU.Num() + RequestCount);

	for (FNiagaraDataChannelPublishRequest& PublishRequest : PublishRequests)
	{
		uint32 NumInsts = 0;

		if (FNiagaraDataChannelGameData* RequestGameData = PublishRequest.GameData.Get())
		{
			NumInsts = RequestGameData->Num();
			if (RequestGameData->Num() > 0 && PublishRequest.bVisibleToGPUSims)
			{
				//We stage the game data into a data set to facilitate easier copy over to the GPU. TODO: Send and copy the game data directly.			
				FNiagaraDataBuffer* StagingBuf = &GameDataStaging->BeginSimulate();
				StagingBuf->Allocate(NumInsts);				
				RequestGameData->WriteToDataSet(StagingBuf, 0, LwcTile);
				GameDataStaging->EndSimulate();				
				BuffersForGPU.Emplace(StagingBuf);	
			}
		}
		else if (ensure(PublishRequest.Data))
		{
			NumInsts = PublishRequest.Data->GetNumInstances();

			if (NumInsts > 0 && PublishRequest.bVisibleToGPUSims)
			{
				BuffersForGPU.Add(PublishRequest.Data);
			}
		}

		if (PublishRequest.bVisibleToGame)
		{
			NewGameDataChannel += NumInsts;
		}
		if (PublishRequest.bVisibleToCPUSims)
		{
			NewCPUDataChannel += NumInsts;
		}
	}

	//Allocate Sim buffers ready for gather.
	CPUSimData->BeginSimulate();
	CPUSimData->Allocate(NewCPUDataChannel, true);

	//Now do the actual data collection.

	GameData->Reserve(NewGameDataChannel);

	for (auto It = PublishRequests.CreateIterator(); It ; ++It)
	{
		FNiagaraDataChannelPublishRequest& PublishRequest = *It;
		if (FNiagaraDataChannelGameData* RequestGameData = PublishRequest.GameData.Get())
		{
			if (RequestGameData->Num())
			{
				if(PublishRequest.bVisibleToGame)
				{
					GameData->AppendFromGameData(*RequestGameData);
				}
				if(PublishRequest.bVisibleToCPUSims)
				{
					RequestGameData->WriteToDataSet(CPUSimData->GetDestinationData(), CPUSimData->GetDestinationDataChecked().GetNumInstances(), LwcTile);
				}
			}
		}

		if (PublishRequest.Data)
		{
			const ENiagaraSimTarget SimTarget = PublishRequest.Data->GetOwner()->GetSimTarget();
			if (SimTarget == ENiagaraSimTarget::CPUSim)
			{
				if (PublishRequest.bVisibleToGame)
				{
					GameData->AppendFromDataSet(PublishRequest.Data, PublishRequest.LwcTile);
				}
				if (PublishRequest.bVisibleToCPUSims)
				{
					PublishRequest.Data->CopyToUnrelated(CPUSimData->GetDestinationDataChecked(), 0, CPUSimData->GetDestinationDataChecked().GetNumInstances(), PublishRequest.Data->GetNumInstances());
				}
			}
			else if (SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				//TODO: GPU->GPU handling will be done all RT side. GPU->CPU may be done here in future but we'll have to pull the data from the GPU on the RT and pass beck to GT here.
			}
			else
			{
				check(0);
			}
		}
#if WITH_NIAGARA_DEBUGGER
		FNiagaraDataChannelDebugUtilities::LogWrite(PublishRequest, Owner->GetDataChannel(), TickGroup);
#endif
		
		It.RemoveCurrentSwap();
	}

	CPUSimData->EndSimulate();

#if WITH_NIAGARA_DEBUGGER
	if(Owner->GetDataChannel()->GetVerboseLogging())
	{
		FString Label = FString::Printf(TEXT("Data Channel %s - CURR"), *Owner->GetDataChannel()->GetName());
		CPUSimData->GetCurrentData()->Dump(0, CPUSimData->GetCurrentData()->GetNumInstances(), Label);

		if(PrevCPUSimData)
		{
			FString LabelPrev = FString::Printf(TEXT("Data Channel %s - PREV"), *Owner->GetDataChannel()->GetName());
			PrevCPUSimData->Dump(0, PrevCPUSimData->GetNumInstances(), LabelPrev);
		}
	}
#endif

	PublishRequests.Reset();
	return RequestCount;
}

FNiagaraDataBufferRef FNiagaraDataChannelData::GetCPUData(bool bPreviousFrame)
{
	if(bPreviousFrame)
	{
		return PrevCPUSimData;
	}
	else
	{
		return CPUSimData->GetCurrentData();
	}
}

FNiagaraDataChannelGameData* FNiagaraDataChannelData::GetGameData()
{
	return GameData.Get();
}

void FNiagaraDataChannelData::Publish(const FNiagaraDataChannelPublishRequest& Request)
{
	FScopeLock Lock(&PublishCritSec);

	PublishRequests.Add(Request);
}

void FNiagaraDataChannelData::RemovePublishRequests(const FNiagaraDataSet* DataSet)
{
	FScopeLock Lock(&PublishCritSec);
	//TODO: Have to ensure lifetime of anything we're holding
	for (auto It = PublishRequests.CreateIterator(); It; ++It)
	{
		const FNiagaraDataChannelPublishRequest& PublishRequest = *It;

		//First remove any buffers we've queued up to push to the GPU
		for (auto BufferIt = BuffersForGPU.CreateIterator(); BufferIt; ++BufferIt)
		{
			FNiagaraDataBuffer* Buffer = *BufferIt;
			if (Buffer == nullptr || Buffer->GetOwner() == DataSet)
			{
				BufferIt.RemoveCurrentSwap();
			}
		}

		if (PublishRequest.Data && PublishRequest.Data->GetOwner() == DataSet)
		{
			It.RemoveCurrentSwap();
		}
	}
}

const FNiagaraDataSetCompiledData& FNiagaraDataChannelData::GetCompiledData(ENiagaraSimTarget SimTarget)
{
	check(CPUSimData && GPUSimData)
	return SimTarget == ENiagaraSimTarget::CPUSim ? CPUSimData->GetCompiledData() : GPUSimData->GetCompiledData();
}

//////////////////////////////////////////////////////////////////////////

void UNiagaraDataChannel::PostInitProperties()
{
	Super::PostInitProperties();
	INiagaraModule::RequestRefreshDataChannels();
}

void AddVarToHash(const FNiagaraVariable& Var, FBlake3& Builder)
{
	FNameBuilder VarName(Var.GetName());
	FStringView Name = VarName.ToView();
	uint32 ClassHash = GetTypeHash(Var.GetType().ClassStructOrEnum);
	Builder.Update(Name.GetData(), Name.Len());
	Builder.Update(&ClassHash, sizeof(uint32));
	Builder.Update(&Var.GetType().UnderlyingType, sizeof(uint16));
}

void UNiagaraDataChannel::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	static FGuid BaseVersion(TEXT("182b8dd3-f963-477f-a57d-70a449d922d8"));
	for (const FNiagaraVariable& Var : Variables_DEPRECATED)
	{
		FNiagaraDataChannelVariable ChannelVar;
		ChannelVar.SetName(Var.GetName());
		ChannelVar.SetType(FNiagaraDataChannelVariable::ToDataChannelType(Var.GetType()));

		FBlake3 VarHashBuilder;
		VarHashBuilder.Update(&BaseVersion, sizeof(FGuid));
		AddVarToHash(Var, VarHashBuilder);
		FBlake3Hash VarHash = VarHashBuilder.Finalize();
		ChannelVar.Version = FGuid::NewGuidFromHash(VarHash);
		
		ChannelVariables.Add(ChannelVar);
	}
	Variables_DEPRECATED.Empty();
	
	if (!VersionGuid.IsValid())
	{
		// If we don't have a guid yet we create one by hashing the existing variables to get a deterministic start guid
		FBlake3 Builder;
		Builder.Update(&BaseVersion, sizeof(FGuid));
		for (const FNiagaraDataChannelVariable& Var : ChannelVariables)
		{
			AddVarToHash(Var, Builder);
		}

		FBlake3Hash Hash = Builder.Finalize();
		VersionGuid = FGuid::NewGuidFromHash(Hash);
	}
#endif

	//Init compiled data. These are not currently serialized as we have no mechanism to rebuild then on internal data format changes like those in scripts.
	GetCompiledData(ENiagaraSimTarget::CPUSim);
	GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

	//TODO: Can serialize?
	GameDataLayout.Init(ChannelVariables);

	INiagaraModule::RequestRefreshDataChannels();
}

void UNiagaraDataChannel::BeginDestroy()
{
	Super::BeginDestroy();
	INiagaraModule::RequestRefreshDataChannels();
}

#if WITH_EDITOR

void UNiagaraDataChannel::PreEditChange(FProperty* PropertyAboutToChange)
{
	FNiagaraWorldManager::ForAllWorldManagers(
		[DataChannel = this](FNiagaraWorldManager& WorldMan)
		{
			WorldMan.RemoveDataChannel(DataChannel);
		});
}

void UNiagaraDataChannel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName VariablesMemberName = GET_MEMBER_NAME_CHECKED(UNiagaraDataChannel, ChannelVariables);
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd && PropertyChangedEvent.GetPropertyName() == VariablesMemberName)
	{
		TSet<FName> ExistingNames;
		for (const FNiagaraDataChannelVariable& Var : ChannelVariables)
		{
			ExistingNames.Add(Var.GetName());
		}
		FName UniqueName = FNiagaraUtilities::GetUniqueName(FName("MyNewVar"), ExistingNames);
		ChannelVariables.Last().SetName(UniqueName);
	}
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate && PropertyChangedEvent.GetPropertyName() == VariablesMemberName)
	{
		int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(VariablesMemberName.ToString());
		if (ChannelVariables.IsValidIndex(ArrayIndex + 1))
		{
			TSet<FName> ExistingNames;
			for (const FNiagaraDataChannelVariable& Var : ChannelVariables)
			{
				ExistingNames.Add(Var.GetName());
			}
			FNiagaraDataChannelVariable& NewEntry = ChannelVariables[ArrayIndex + 1];
			FName UniqueName = FNiagaraUtilities::GetUniqueName(NewEntry.GetName(), ExistingNames);
			NewEntry.SetName(UniqueName);
			NewEntry.Version = FGuid::NewGuid();
		}
	}
	if (PropertyChangedEvent.GetPropertyName() == VariablesMemberName || PropertyChangedEvent.GetMemberPropertyName() == VariablesMemberName)
	{
		VersionGuid = FGuid::NewGuid();
		// the guid of the variable is updated by the details customization, as we don't want to change it when just renaming a variable
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	check(IsInGameThread());

	//Refresh compiled data
	CompiledData.Empty();
	CompiledDataGPU.Empty();
	GetCompiledData(ENiagaraSimTarget::CPUSim);
	GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

	GameDataLayout.Init(ChannelVariables);

	INiagaraModule::RequestRefreshDataChannels();
}

#endif//WITH_EDITOR

const FNiagaraDataSetCompiledData& UNiagaraDataChannel::GetCompiledData(ENiagaraSimTarget SimTarget)const
{
	if(SimTarget == ENiagaraSimTarget::CPUSim)
	{
		if (CompiledData.Variables.Num() != ChannelVariables.Num())
		{
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledData.Empty();
			CompiledData.SimTarget = ENiagaraSimTarget::CPUSim;
			for (FNiagaraVariableBase Var : ChannelVariables)
			{
				if(Var.GetType().IsEnum() == false)
				{
					Var.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
				}
				CompiledData.Variables.Add(Var);
			}
			CompiledData.BuildLayout();
		}
		return CompiledData;
	}
	else
	{
		if (CompiledDataGPU.Variables.Num() != ChannelVariables.Num())
		{
			check(SimTarget == ENiagaraSimTarget::GPUComputeSim);
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledDataGPU.Empty();
			CompiledDataGPU.SimTarget = ENiagaraSimTarget::GPUComputeSim;
			for (FNiagaraVariableBase Var : ChannelVariables)
			{
				if (Var.GetType().IsEnum() == false)
				{
					Var.SetType(FNiagaraTypeDefinition(FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(Var.GetType().GetScriptStruct(), ENiagaraStructConversion::Simulation)));
				}
				CompiledDataGPU.Variables.Add(Var);
			}
			CompiledDataGPU.BuildLayout();
		}
		return CompiledDataGPU;
	}
}

FNiagaraDataChannelGameDataPtr UNiagaraDataChannel::CreateGameData()const
{
	FNiagaraDataChannelGameDataPtr NewData = MakeShared<FNiagaraDataChannelGameData>();
	NewData->Init(this);
	return NewData;
}

bool UNiagaraDataChannel::IsValid()const
{
	return ChannelVariables.Num() > 0 && CompiledData.Variables.Num() == ChannelVariables.Num() && CompiledDataGPU.Variables.Num() == ChannelVariables.Num();
}

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelLibrary::UNiagaraDataChannelLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNiagaraDataChannelHandler* UNiagaraDataChannelLibrary::GetNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel)
{
	if (!Channel)
	{
		return nullptr;
	}
	return FindDataChannelHandler(WorldContextObject, Channel->Get());
}

UNiagaraDataChannelWriter* UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU, const FString& DebugSource)
{
	if (!Channel)
	{
		return nullptr;
	}
	return CreateDataChannelWriter(WorldContextObject, Channel->Get(), SearchParams, Count, bVisibleToGame, bVisibleToCPU, bVisibleToGPU, DebugSource);
}

UNiagaraDataChannelReader* UNiagaraDataChannelLibrary::ReadFromNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame)
{
	if (!Channel)
	{
		return nullptr;
	}
	return CreateDataChannelReader(WorldContextObject, Channel->Get(), SearchParams, bReadPreviousFrame);
}

int32 UNiagaraDataChannelLibrary::GetDataChannelElementCount(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame)
{
	if (Channel && Channel->Get())
	{
		if (UNiagaraDataChannelReader* Reader = CreateDataChannelReader(WorldContextObject, Channel->Get(), SearchParams, bReadPreviousFrame))
		{
			return Reader->Num();
		}
	}
	return 0;
}

void UNiagaraDataChannelLibrary::ReadFromNiagaraDataChannelSingle(const UObject*, const UNiagaraDataChannelAsset*, int32, FNiagaraDataChannelSearchParameters, bool, ENiagartaDataChannelReadResult&)
{
	// this function is just a placeholder and calls into CreateDataChannelReader and its individual read functions from the BP node
}

void UNiagaraDataChannelLibrary::WriteToNiagaraDataChannelSingle(const UObject*, const UNiagaraDataChannelAsset*, FNiagaraDataChannelSearchParameters, bool, bool, bool)
{
	// this function is just a placeholder and calls into CreateDataChannelWriter and its individual write functions from the BP node
}

UNiagaraDataChannelHandler* UNiagaraDataChannelLibrary::FindDataChannelHandler(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel)
{
	UWorld* World = (WorldContextObject != nullptr) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World)
	{
		if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
		{
			return WorldMan->GetDataChannelManager().FindDataChannelHandler(Channel);
		}
	}
	return nullptr;
}

UNiagaraDataChannelWriter* UNiagaraDataChannelLibrary::CreateDataChannelWriter(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU, const FString& DebugSource)
{
	check(IsInGameThread());
	UWorld* World = (WorldContextObject != nullptr) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World && Count > 0)
	{
		if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
		{
			if(UNiagaraDataChannelHandler* Handler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Channel))
			{
				if(UNiagaraDataChannelWriter* Writer = Handler->GetDataChannelWriter())
				{
					if(Writer->InitWrite(SearchParams, Count, bVisibleToGame, bVisibleToCPU, bVisibleToGPU, DebugSource))
					{
						return Writer;
					}
				}
			}
		}
	}
	return nullptr;
}

UNiagaraDataChannelReader* UNiagaraDataChannelLibrary::CreateDataChannelReader(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame)
{
	UWorld* World = (WorldContextObject != nullptr) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (World)
	{
		if (FNiagaraWorldManager* WorldMan = FNiagaraWorldManager::Get(World))
		{
			if (UNiagaraDataChannelHandler* Handler = WorldMan->GetDataChannelManager().FindDataChannelHandler(Channel))
			{
				if (UNiagaraDataChannelReader* Reader = Handler->GetDataChannelReader())
				{
					if(Reader->InitAccess(SearchParams, bReadPreviousFrame))
					{
						return Reader;
					}
				}
			}
		}
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR

void UNiagaraDataChannelAsset::PreEditChange(class FProperty* PropertyAboutToChange)
{
	if(PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataChannelAsset, DataChannel) && DataChannel)
	{
		CachedPreChangeDataChannel = DataChannel;
	}
}

void UNiagaraDataChannelAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(CachedPreChangeDataChannel && DataChannel)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		UEngine::CopyPropertiesForUnrelatedObjects(CachedPreChangeDataChannel, DataChannel, Params);
		CachedPreChangeDataChannel = nullptr;
	}
}

#endif

bool FNiagaraDataChannelVariable::Serialize(FArchive& Ar)
{
	if (FNiagaraVariableBase::Serialize(Ar))
	{
		if (Ar.IsLoading())
		{
			// fix up variables serialized with wrong type
			// this happens because we only save swc types
			SetType(ToDataChannelType(GetType()));
		}
		return true;
	}
	return false;
}

#if WITH_EDITORONLY_DATA
bool FNiagaraDataChannelVariable::IsAllowedType(const FNiagaraTypeDefinition& Type)
{
	return !(Type.IsDataInterface() || Type.GetClass() || Type == FNiagaraTypeDefinition::GetParameterMapDef() || Type == FNiagaraTypeDefinition::GetGenericNumericDef() || Type == FNiagaraTypeDefinition::GetHalfDef() || Type == FNiagaraTypeDefinition::GetMatrix4Def());
}
#endif

FNiagaraTypeDefinition FNiagaraDataChannelVariable::ToDataChannelType(const FNiagaraTypeDefinition& Type)
{
	if (Type == FNiagaraTypeDefinition::GetVec3Def())
	{
		return FNiagaraTypeHelper::GetVectorDef();
	}
	if (Type == FNiagaraTypeDefinition::GetFloatDef())
	{
		return FNiagaraTypeHelper::GetDoubleDef();
	}
	if (Type == FNiagaraTypeDefinition::GetQuatDef())
	{
		return FNiagaraTypeHelper::GetQuatDef();
	}
	if (Type == FNiagaraTypeDefinition::GetVec2Def())
	{
		return FNiagaraTypeHelper::GetVector2DDef();
	}
	if (Type == FNiagaraTypeDefinition::GetVec4Def())
	{
		return FNiagaraTypeHelper::GetVector4Def();
	}
	return Type;
}

//////////////////////////////////////////////////////////////////////////

#if WITH_NIAGARA_DEBUGGER
void FNiagaraDataChannelDebugUtilities::BeginFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds)
{
	check(WorldMan);
	WorldMan->GetDataChannelManager().BeginFrame(DeltaSeconds);
}

void FNiagaraDataChannelDebugUtilities::EndFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds)
{
	check(WorldMan);
	WorldMan->GetDataChannelManager().EndFrame(DeltaSeconds);
}

void FNiagaraDataChannelDebugUtilities::Tick(FNiagaraWorldManager* WorldMan, float DeltaSeconds, ETickingGroup TickGroup)
{
	check(WorldMan);
	WorldMan->GetDataChannelManager().Tick(DeltaSeconds, TickGroup);
}

UNiagaraDataChannelHandler* FNiagaraDataChannelDebugUtilities::FindDataChannelHandler(FNiagaraWorldManager* WorldMan, UNiagaraDataChannel* DataChannel)
{
	check(WorldMan);
	return WorldMan->GetDataChannelManager().FindDataChannelHandler(DataChannel);
}

void FNiagaraDataChannelDebugUtilities::LogWrite(const FNiagaraDataChannelPublishRequest& WriteRequest, const UNiagaraDataChannel* DataChannel, const ETickingGroup& TickGroup)
{
	FNiagaraDataChannelDebugUtilities& Debugger = Get();
	if (NDCCVars::FrameDataToCapture > 0)
	{
		if (Debugger.FrameData.Num() > NDCCVars::FrameDataToCapture)
		{
			Debugger.FrameData.SetNum(NDCCVars::FrameDataToCapture);
		}
		FFrameDebugData* Data;
		if (Debugger.FrameData.Num() && Debugger.FrameData.Last().FrameNumber == GFrameCounter)
		{
			Data = &Debugger.FrameData.Last();
		} 
		else
		{
			if (Debugger.FrameData.Num() == NDCCVars::FrameDataToCapture)
			{
				Debugger.FrameData.RemoveAt(0);
			}
			Data = &Debugger.FrameData.AddDefaulted_GetRef();
			Data->FrameNumber = GFrameCounter;
		}
		FChannelWriteRequest DebugData;
		DebugData.DebugSource = WriteRequest.DebugSource;
		DebugData.bVisibleToGame = WriteRequest.bVisibleToGame;
		DebugData.bVisibleToCPUSims = WriteRequest.bVisibleToCPUSims;
		DebugData.bVisibleToGPUSims = WriteRequest.bVisibleToGPUSims;
		DebugData.TickGroup = TickGroup;
		if (WriteRequest.GameData.IsValid())
		{
			DebugData.Data = WriteRequest.GameData;
		}
		else if (ensure(WriteRequest.Data))
		{
			DebugData.Data = MakeShared<FNiagaraDataChannelGameData>();
			DebugData.Data->Init(DataChannel);
			DebugData.Data->AppendFromDataSet(WriteRequest.Data, WriteRequest.LwcTile);
		}
		Data->WriteRequests.Add(DebugData);
	}
	else
	{
		if (Debugger.FrameData.Num())
		{
			Debugger.FrameData.Empty();
		}
	}
	
	if (NDCCVars::LogWritesToOutputLog > 0)
	{
		FString DataString;
		uint32 NumInsts = 0;
		if (FNiagaraDataChannelGameData* RequestGameData = WriteRequest.GameData.Get())
		{
			NumInsts = RequestGameData->Num();
			if (NDCCVars::LogWritesToOutputLog > 1)
			{
				DataString = ToJson(RequestGameData);
			}
		}
		else if (ensure(WriteRequest.Data))
		{
			NumInsts = WriteRequest.Data->GetNumInstances();
			if (NDCCVars::LogWritesToOutputLog > 1)
			{
				FNiagaraDataChannelGameData TempData;
				TempData.Init(DataChannel);
				TempData.AppendFromDataSet(WriteRequest.Data, WriteRequest.LwcTile);
				DataString = ToJson(&TempData);
			}
		}
		
		UE_LOG(LogNiagara, Log, TEXT("Frame %llu, TG %s, NDC write by %s (BP[%s]/CPU[%s]/GPU[%s]): %i entries to data channel %s %s%s")
			, GFrameCounter
			, *TickGroupToString(TickGroup)
			, *WriteRequest.DebugSource
			, WriteRequest.bVisibleToGame ? TEXT("X") : TEXT(" ")
			, WriteRequest.bVisibleToCPUSims ? TEXT("X") : TEXT(" ")
			, WriteRequest.bVisibleToGPUSims ? TEXT("X") : TEXT(" ")
			, NumInsts
			, *GetPathNameSafe(DataChannel)
			, DataString.IsEmpty() ? TEXT("") : TEXT("\n")
			, *DataString);
	}
}

void FNiagaraDataChannelDebugUtilities::DumpAllWritesToLog()
{
	FNiagaraDataChannelDebugUtilities& Debugger = Get();
	if (Debugger.FrameData.Num() == 0)
	{
		UE_LOG(LogNiagara, Log, TEXT("No writes are currently stored in the log. fx.Niagara.DataChannels.FrameDataToCapture = %i"), NDCCVars::FrameDataToCapture);
		return;
	}
	UE_LOG(LogNiagara, Log, TEXT("Current Frame is %llu, logging data from oldest to newest:"), GFrameCounter);
	for (const FFrameDebugData& FrameData : Debugger.FrameData)
	{
		UE_LOG(LogNiagara, Log, TEXT("Frame %llu: %i entries"), FrameData.FrameNumber, FrameData.WriteRequests.Num());
		for (const FChannelWriteRequest& Request : FrameData.WriteRequests)
		{
			FString DataString = ToJson(Request.Data.Get());
			UE_LOG(LogNiagara, Log, TEXT("Write by %s (BP[%s]/CPU[%s]/GPU[%s], TG %s): %i entries to data channel %s \n%s")
			, *Request.DebugSource
			, Request.bVisibleToGame ? TEXT("X") : TEXT(" ")
			, Request.bVisibleToCPUSims ? TEXT("X") : TEXT(" ")
			, Request.bVisibleToGPUSims ? TEXT("X") : TEXT(" ")
			, *TickGroupToString(Request.TickGroup)
			, Request.Data->Num()
			, *GetPathNameSafe(Request.Data->GetDataChannel())
			, *DataString);
		}
		UE_LOG(LogNiagara, Log, TEXT("----------------------------------------------"));
	} 
}

FNiagaraDataChannelDebugUtilities& FNiagaraDataChannelDebugUtilities::Get()
{
	return TLazySingleton<FNiagaraDataChannelDebugUtilities>::Get();
}

void FNiagaraDataChannelDebugUtilities::TearDown()
{
	return TLazySingleton<FNiagaraDataChannelDebugUtilities>::TearDown();
}

FString FNiagaraDataChannelDebugUtilities::ToJson(FNiagaraDataChannelGameData* Data)
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	JsonWriter->WriteArrayStart();

	TConstArrayView<FNiagaraDataChannelVariable> ChannelVariables = Data->GetDataChannel()->GetVariables();
	for (int32 i = 0; i < Data->Num(); i++)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		for (const FNiagaraDataChannelVariable& Var : ChannelVariables)
		{
			FString VarName = Var.GetName().ToString();
			FNiagaraDataChannelVariableBuffer* Buffer = Data->FindVariableBuffer(Var);
			if (Var.GetType() == FNiagaraTypeHelper::GetDoubleDef() && Buffer)
			{
				double Value;
				Buffer->Read<double>(i, Value, false);
				JsonObject->SetNumberField(VarName, Value);
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetBoolDef() && Buffer)
			{
				FNiagaraBool Value;
				Buffer->Read<FNiagaraBool>(i, Value, false);
				JsonObject->SetBoolField(VarName, Value);
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetIntDef() && Buffer)
			{
				int32 Value;
				Buffer->Read<int32>(i, Value, false);
				JsonObject->SetNumberField(VarName, Value);
			}
			else if ((Var.GetType() == FNiagaraTypeHelper::GetVectorDef() || Var.GetType() == FNiagaraTypeDefinition::GetPositionDef()) && Buffer)
			{
				FVector Value;
				Buffer->Read<FVector>(i, Value, false);
				JsonObject->SetStringField(VarName, Value.ToString());
			}
			else if (Var.GetType() == FNiagaraTypeHelper::GetVector2DDef() && Buffer)
			{
				FVector2D Value;
				Buffer->Read<FVector2D>(i, Value, false);
				JsonObject->SetStringField(VarName, Value.ToString());
			}
			else if (Var.GetType() == FNiagaraTypeHelper::GetVector4Def() && Buffer)
			{
				FVector4 Value;
				Buffer->Read<FVector4>(i, Value, false);
				JsonObject->SetStringField(VarName, Value.ToString());
			}
			else if (Var.GetType() == FNiagaraTypeHelper::GetQuatDef() && Buffer)
			{
				FQuat Value;
				Buffer->Read<FQuat>(i, Value, false);
				JsonObject->SetStringField(VarName, Value.ToString());
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetColorDef() && Buffer)
			{
				FLinearColor Value;
				Buffer->Read<FLinearColor>(i, Value, false);
				JsonObject->SetStringField(VarName, Value.ToString());
			}
			else if (Var.GetType() == FNiagaraTypeDefinition::GetIDDef() && Buffer)
			{
				FNiagaraID Value;
				Buffer->Read<FNiagaraID>(i, Value, false);
				JsonObject->SetStringField(VarName, FString::FromInt(Value.Index) + "/" + FString::FromInt(Value.AcquireTag));
			}
			else if (Var.GetType().IsEnum() && Buffer)
			{
				int32 Value;
				Buffer->Read<int32>(i, Value, false);
				JsonObject->SetStringField(VarName, Var.GetType().GetEnum()->GetNameByValue(Value).ToString());
			}
			else
			{
				JsonObject->SetStringField(VarName, "???");
			}
		}
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);
	}
	JsonWriter->WriteArrayEnd();
	JsonWriter->Close();
	return JsonString;
}

FString FNiagaraDataChannelDebugUtilities::TickGroupToString(const ETickingGroup& TickGroup)
{
	static UEnum* TGEnum = StaticEnum<ETickingGroup>();
	return TGEnum->GetDisplayNameTextByValue(TickGroup).ToString();
}
#endif //WITH_NIAGARA_DEBUGGER

#undef LOCTEXT_NAMESPACE

