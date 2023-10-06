// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel.h"

#include "NiagaraWorldManager.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelManager.h"
#include "Engine/Engine.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataChannel)

#define LOCTEXT_NAMESPACE "NiagaraDataChannels"

//////////////////////////////////////////////////////////////////////////

void FNiagaraDataChannelGameDataLayout::Init(const TArray<FNiagaraVariable>& Variables)
{
	VariableIndices.Reset();
	LwcConverters.Reserve(Variables.Num());
	for (const FNiagaraVariable& Var : Variables)
	{
		//Sigh.
		//We must convert from the variable stored var in the data channels definition as we currently cannot serialize/store actual LWC types in FNiagawraTypeDefinitions.
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
			//Write all data from the data channel into the data set. Converting into from LWC into the local LWC Tile space as we go.

			uint8* SrcData = SrcPropertyBase;

			//Positions are a special case that are stored as FVectors in game data but converted to an LWCTile local FVector3f in simulation data.
			if (DestStruct == FNiagaraTypeDefinition::GetPositionStruct())
			{	
				float* DestX = (float*)DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
				float* DestY = (float*)DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
				float* DestZ = (float*)DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
								
				for (int32 i = 0; i < NumInstances; ++i)
				{
					FVector* Src = (FVector*)((SrcData + i * SrcVarSize));

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
					int32 SrcOffset = SrcProperty->GetOffset_ForInternal();
					SrcData = SrcPropertyBase + SrcProperty->GetOffset_ForInternal();

					//Convert any LWC doubles to floats. //TODO: Insert LWCTile... probably need to explicitly check for vectors etc.
					if (SrcProperty->IsA(FDoubleProperty::StaticClass()))
					{
						check(DestProperty->IsA(FFloatProperty::StaticClass()));
						float* Dest = (float*)DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);
						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							double* Src = (double*)((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FFloatProperty::StaticClass()))
					{
						float* Dest = (float*)DestBuffer->GetInstancePtrFloat(FloatCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							float* Src = (float*)((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FUInt16Property::StaticClass()))
					{
						FFloat16* Dest = (FFloat16*)DestBuffer->GetInstancePtrHalf(HalfCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							FFloat16* Src = (FFloat16*)((SrcData + i * SrcVarSize));
							*Dest++ = *Src;
						}
					}
					else if (SrcProperty->IsA(FIntProperty::StaticClass()) || SrcProperty->IsA(FBoolProperty::StaticClass()))
					{
						int32* Dest = (int32*)DestBuffer->GetInstancePtrInt32(IntCompIdx++, DestStartIdx);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							int32* Src = (int32*)((SrcData + i * SrcVarSize));
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
	int32 OrigNumDataChannel = NumElements;
	NumElements += NumInstances;

	const FNiagaraDataChannelGameDataLayout& Layout = DataChannel->GetGameDataLayout();
	for (const TPair<FNiagaraVariableBase, int32>& VarIndexPair : Layout.VariableIndices)
	{
		FNiagaraVariableBase Var = VarIndexPair.Key;
		int32 VarIndex = VarIndexPair.Value;
		FNiagaraDataChannelVariableBuffer& VarBuffer = VariableData[VarIndex];

		VarBuffer.SetNum(NumElements);

		uint8* DestDataBase = VarBuffer.Data.GetData();
		
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
		ReadData = [&](UScriptStruct* SrcStruct, UScriptStruct* DestStruct, uint8* DestData)
		{
			//Read all data from the simulation and place in the game level buffers. Converting from LWC local tile space where needed.

			//Special case for writing to Niagara Positions. We must offset the data into the simulation local LWC space.
			if (SrcStruct == FNiagaraTypeDefinition::GetPositionStruct())
			{
				float* SrcX = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);
				float* SrcY = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);
				float* SrcZ = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);

				for (int32 i = 0; i < NumInstances; ++i)
				{
					FVector* Dest = (FVector*)(DestData + VarSize * i);
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
					DestData += DestProperty->GetOffset_ForInternal();
					if (DestPropertyIt->IsA(FDoubleProperty::StaticClass()))
					{
						double* Dest = (double*)(DestData);
						float* Src = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = (double*)(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FFloatProperty::StaticClass()))
					{
						float* Dest = (float*)(DestData);
						float* Src = (float*)SrcBuffer->GetComponentPtrFloat(FloatCompIdx++);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = (float*)(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FUInt16Property::StaticClass()))
					{
						FFloat16* Dest = (FFloat16*)(DestData);
						FFloat16* Src = (FFloat16*)SrcBuffer->GetComponentPtrHalf(HalfCompIdx++);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = (FFloat16*)(DestData + VarSize * i);
							*Dest = *Src++;
						}
					}
					else if (DestPropertyIt->IsA(FIntProperty::StaticClass()) || DestPropertyIt->IsA(FBoolProperty::StaticClass()))
					{
						int32* Dest = (int32*)(DestData);
						int32* Src = (int32*)SrcBuffer->GetComponentPtrInt32(IntCompIdx++);

						//Write all instances for this component.
						for (int32 i = 0; i < NumInstances; ++i)
						{
							Dest = (int32*)(DestData + VarSize * i);
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
	ConsumePublishRequests(Owner);

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

void FNiagaraDataChannelData::ConsumePublishRequests(UNiagaraDataChannelHandler* Owner)
{
	check(IsValid(Owner));

	//There should be no access on other threads at this point anyway but lock just to be safe.
	FScopeLock Lock(&PublishCritSec);

	if(PublishRequests.Num() == 0)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();
	check(IsValid(World));

	int32 GameDataOrigSize = GameData->Num();
	int32 CPUDataOrigSize = CPUSimData->GetCurrentData()->GetNumInstances();
	int32 NewGameDataChannel = GameDataOrigSize;
	int32 NewCPUDataChannel = CPUDataOrigSize;

	//Do a pass to gather the new total size for our DataChannel data.

	//Each DI that generates DataChannel can control whether it's pushed to Game/CPU/GPU.
	BuffersForGPU.Reserve(BuffersForGPU.Num() + PublishRequests.Num());

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

	GameData->SetNum(NewGameDataChannel);

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
		
		It.RemoveCurrentSwap();
	}

	CPUSimData->EndSimulate();

#if !UE_BUILD_SHIPPING
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

namespace NiagaraDataChannel
{
	FNiagaraTypeDefinition GetFVectorDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector"));
		return FNiagaraTypeDefinition(VectorStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
	
	FNiagaraTypeDefinition GetDoubleDef()
	{
		static UPackage* NiagaraPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/Niagara"));
		static UScriptStruct* DoubleStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraDouble"));
		return FNiagaraTypeDefinition(DoubleStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}

	FNiagaraTypeDefinition GetFQuatDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Quat"));
		return FNiagaraTypeDefinition(Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
	
	FNiagaraTypeDefinition GetFVector2DDef()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* VectorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector2D"));
		return FNiagaraTypeDefinition(VectorStruct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}

	FNiagaraTypeDefinition GetFVector4Def()
	{
		static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
		static UScriptStruct* Vector4Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector4"));
		return FNiagaraTypeDefinition(Vector4Struct, FNiagaraTypeDefinition::EAllowUnfriendlyStruct::Allow);
	}
}

void UNiagaraDataChannel::PostInitProperties()
{
	Super::PostInitProperties();
	INiagaraModule::RequestRefreshDataChannels();
}

void UNiagaraDataChannel::PostLoad()
{
	Super::PostLoad();

	for (int i = 0; i < Variables.Num(); i++)
	{
		// fix up variables deserialized with wrong type
		// TODO (mga) find a better solution than this
		FNiagaraVariable& Var = Variables[i];
		if (Var.GetType() == FNiagaraTypeDefinition::GetVec3Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVectorDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetFloatDef())
		{
			Var.SetType(NiagaraDataChannel::GetDoubleDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetQuatDef())
		{
			Var.SetType(NiagaraDataChannel::GetFQuatDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec2Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVector2DDef());
		}
		else if (Var.GetType() == FNiagaraTypeDefinition::GetVec4Def())
		{
			Var.SetType(NiagaraDataChannel::GetFVector4Def());
		}
	}

	//Init compiled data. These are not currently serialized as we have no mechanism to rebuild then on internal data format changes like those in scripts.
	GetCompiledData(ENiagaraSimTarget::CPUSim);
	GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

	//TODO: Can serialize?
	GameDataLayout.Init(Variables);

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
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	check(IsInGameThread());

	//Refresh compiled data
	CompiledData.Empty();
	CompiledDataGPU.Empty();
	GetCompiledData(ENiagaraSimTarget::CPUSim);
	GetCompiledData(ENiagaraSimTarget::GPUComputeSim);

	GameDataLayout.Init(Variables);

	INiagaraModule::RequestRefreshDataChannels();
}

#endif//WITH_EDITOR

const FNiagaraDataSetCompiledData& UNiagaraDataChannel::GetCompiledData(ENiagaraSimTarget SimTarget)const
{
	if(SimTarget == ENiagaraSimTarget::CPUSim)
	{
		if (CompiledData.Variables.Num() != Variables.Num())
		{
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledData.Empty();
			CompiledData.SimTarget = ENiagaraSimTarget::CPUSim;
			for (FNiagaraVariableBase Var : Variables)
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
		if (CompiledDataGPU.Variables.Num() != Variables.Num())
		{
			check(SimTarget == ENiagaraSimTarget::GPUComputeSim);
			//Build the compiled data from the current variables but convert to Simulation types.
			CompiledDataGPU.Empty();
			CompiledDataGPU.SimTarget = ENiagaraSimTarget::GPUComputeSim;
			for (FNiagaraVariableBase Var : Variables)
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
	return Variables.Num() > 0 && CompiledData.Variables.Num() == Variables.Num() && CompiledDataGPU.Variables.Num() == Variables.Num();
}

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelLibrary::UNiagaraDataChannelLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNiagaraDataChannelHandler* UNiagaraDataChannelLibrary::GetNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel)
{
	return GetNiagaraDataChannel(WorldContextObject, Channel->Get());
}

UNiagaraDataChannelWriter* UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU)
{
	return WriteToNiagaraDataChannel(WorldContextObject, Channel->Get(), SearchParams, Count, bVisibleToGame, bVisibleToCPU, bVisibleToGPU);
}

UNiagaraDataChannelReader* UNiagaraDataChannelLibrary::ReadFromNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame)
{
	return ReadFromNiagaraDataChannel(WorldContextObject, Channel->Get(), SearchParams, bReadPreviousFrame);
}

UNiagaraDataChannelHandler* UNiagaraDataChannelLibrary::GetNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel)
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

UNiagaraDataChannelWriter* UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU)
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
					if(Writer->InitWrite(SearchParams, Count, bVisibleToGame, bVisibleToCPU, bVisibleToGPU))
					{
						return Writer;
					}
				}
			}
		}
	}
	return nullptr;
}

UNiagaraDataChannelReader* UNiagaraDataChannelLibrary::ReadFromNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame)
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
	if(PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataChannelAsset, DataChannel))
	{
		CachedPreChangeDataChannel = DataChannel;
	}
}

void UNiagaraDataChannelAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(CachedPreChangeDataChannel)
	{
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
		UEngine::CopyPropertiesForUnrelatedObjects(CachedPreChangeDataChannel, DataChannel, Params);
		CachedPreChangeDataChannel = nullptr;
	}
}

#endif


//////////////////////////////////////////////////////////////////////////

#if !UE_BUILD_SHIPPING
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
#endif //!UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE

