// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "NiagaraSimCache.h"
#include "NiagaraSystemInstanceController.h"

struct FNiagaraSimCacheHelper
{
	explicit FNiagaraSimCacheHelper(FNiagaraSystemInstance* InSystemInstance)
	{
		SystemInstance = InSystemInstance;
		SystemSimulation = SystemInstance->GetSystemSimulation();
		check(SystemSimulation);
		SystemSimulationDataBuffer = SystemSimulation->MainDataSet.GetCurrentData();
		NiagaraSystem = SystemSimulation->GetSystem();
	}

	explicit FNiagaraSimCacheHelper(UNiagaraComponent* NiagaraComponent)
	{
		if ( NiagaraComponent == nullptr )
		{
			return;
		}

		NiagaraSystem = NiagaraComponent->GetAsset();
		if ( NiagaraSystem == nullptr )
		{
			return;
		}

		FNiagaraSystemInstanceControllerPtr SystemInstanceController = NiagaraComponent->GetSystemInstanceController();
		if (SystemInstanceController.IsValid() == false)
		{
			return;
		}

		SystemInstance = SystemInstanceController->GetSystemInstance_Unsafe();
		if (SystemInstance == nullptr)
		{
			return;
		}

		SystemSimulation = SystemInstance->GetSystemSimulation();
		if (SystemSimulation == nullptr)
		{
			return;
		}

		SystemSimulationDataBuffer = SystemSimulation->MainDataSet.GetCurrentData();
		if (SystemSimulationDataBuffer == nullptr)
		{
			return;
		}
	}

	FNiagaraDataSet& GetSystemSimulationDataSet() { return SystemSimulation->MainDataSet; }

	bool HasValidSimulation() const { return SystemSimulation != nullptr; }
	bool HasValidSimulationData() const { return SystemSimulationDataBuffer != nullptr; }

	void BuildCacheLayout(FNiagaraSimCacheDataBuffersLayout& CacheLayout, const FNiagaraDataSetCompiledData& CompiledData, FName LayoutName, TArray<FName> InRebaseVariableNames, TConstArrayView<FName> ExplicitCaptureAttributes) const
	{
		CacheLayout.LayoutName = LayoutName;
		CacheLayout.SimTarget = CompiledData.SimTarget;

		int32 TotalCacheComponents = 0;
		TArray<int32> CacheToDataSetVariables;

		CacheToDataSetVariables.Reserve(CompiledData.Variables.Num());
		for (int32 i = 0; i < CompiledData.Variables.Num(); ++i)
		{
			const FNiagaraVariableLayoutInfo& DataSetVariableLayout = CompiledData.VariableLayouts[i];
			const FNiagaraVariable& DataSetVariable = CompiledData.Variables[i];
			if (ExplicitCaptureAttributes.Num() == 0 || ExplicitCaptureAttributes.Contains(DataSetVariable.GetName()))
			{
				CacheToDataSetVariables.Add(i);
				TotalCacheComponents += DataSetVariableLayout.GetNumFloatComponents() + DataSetVariableLayout.GetNumHalfComponents() + DataSetVariableLayout.GetNumInt32Components();
			}
		}

		const int32 NumCacheVariables = CacheToDataSetVariables.Num();

		CacheLayout.Variables.AddDefaulted(NumCacheVariables);

		CacheLayout.ComponentMappingsFromDataBuffer.Empty(TotalCacheComponents);
		CacheLayout.ComponentMappingsFromDataBuffer.AddDefaulted(TotalCacheComponents);
		CacheLayout.RebaseVariableNames = MoveTemp(InRebaseVariableNames);

		for ( int32 iCacheVariable=0; iCacheVariable < NumCacheVariables; ++iCacheVariable)
		{
			const int32 iDataSetVariable = CacheToDataSetVariables[iCacheVariable];
			const FNiagaraVariableLayoutInfo& DataSetVariableLayout = CompiledData.VariableLayouts[iDataSetVariable];
			FNiagaraSimCacheVariable& CacheVariable = CacheLayout.Variables[iCacheVariable];

			CacheVariable.Variable = CompiledData.Variables[iDataSetVariable];
			CacheVariable.FloatOffset = DataSetVariableLayout.GetNumFloatComponents() > 0 ? CacheLayout.FloatCount : INDEX_NONE;
			CacheVariable.FloatCount = DataSetVariableLayout.GetNumFloatComponents();
			CacheVariable.HalfOffset = DataSetVariableLayout.GetNumHalfComponents() > 0 ? CacheLayout.HalfCount : INDEX_NONE;
			CacheVariable.HalfCount = DataSetVariableLayout.GetNumHalfComponents();
			CacheVariable.Int32Offset = DataSetVariableLayout.GetNumInt32Components() > 0 ? CacheLayout.Int32Count : INDEX_NONE;
			CacheVariable.Int32Count = DataSetVariableLayout.GetNumInt32Components();

			CacheLayout.FloatCount += DataSetVariableLayout.GetNumFloatComponents();
			CacheLayout.HalfCount += DataSetVariableLayout.GetNumHalfComponents();
			CacheLayout.Int32Count += DataSetVariableLayout.GetNumInt32Components();
		}

		// Build write mappings we will build read mappings in a separate path
		int32 FloatOffset = 0;
		int32 HalfOffset = CacheLayout.FloatCount;
		int32 Int32Offset = HalfOffset + CacheLayout.HalfCount;
		for (int32 iCacheVariable = 0; iCacheVariable < NumCacheVariables; ++iCacheVariable)
		{
			const int32 iDataSetVariable = CacheToDataSetVariables[iCacheVariable];
			const FNiagaraVariableLayoutInfo& DataSetVariableLayout = CompiledData.VariableLayouts[iDataSetVariable];
			FNiagaraSimCacheVariable& CacheVariable = CacheLayout.Variables[iCacheVariable];

			for (int32 iComponent=0; iComponent < CacheVariable.FloatCount; ++iComponent)
			{
				CacheLayout.ComponentMappingsFromDataBuffer[FloatOffset] = DataSetVariableLayout.FloatComponentStart + iComponent;
				++FloatOffset;
			}

			for (int32 iComponent=0; iComponent < CacheVariable.HalfCount; ++iComponent)
			{
				CacheLayout.ComponentMappingsFromDataBuffer[HalfOffset] = DataSetVariableLayout.HalfComponentStart + iComponent;
				++HalfOffset;
			}

			for (int32 iComponent=0; iComponent < CacheVariable.Int32Count; ++iComponent)
			{
				CacheLayout.ComponentMappingsFromDataBuffer[Int32Offset] = DataSetVariableLayout.Int32ComponentStart + iComponent;
				++Int32Offset;
			}
		}

		// Slightly inefficient but we can share the code between the paths
		BuildCacheReadMappings(CacheLayout, CompiledData);
	}

	void BuildCacheLayoutForSystem(const FNiagaraSimCacheCreateParameters& CreateParameters, FNiagaraSimCacheDataBuffersLayout& CacheLayout)
	{
		const FNiagaraDataSetCompiledData& SystemCompileData = NiagaraSystem->GetSystemCompiledData().DataSetCompiledData;

		TArray<FName> RebaseVariableNames;
		if ( CreateParameters.bAllowRebasing )
		{
			TArray<FString, TInlineAllocator<8>> LocalSpaceEmitters;
			for ( int32 i=0; i < NiagaraSystem->GetNumEmitters(); ++i )
			{
				const FNiagaraEmitterHandle& EmitterHandle = NiagaraSystem->GetEmitterHandle(i);
				if (EmitterHandle.GetIsEnabled() )
				{
					const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetInstance().GetEmitterData();
					if (EmitterData && EmitterData->bLocalSpace)
					{
						LocalSpaceEmitters.Add(EmitterHandle.GetUniqueInstanceName());
					}
				}
			}

			for (const FNiagaraVariable& Variable : SystemCompileData.Variables)
			{
				if (Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef())
				{
					// If this is an emitter variable we need to check if it's local space or not
					bool bIsLocalSpace = false;
					for ( const FString& LocalSpaceEmitter : LocalSpaceEmitters )
					{
						if ( Variable.IsInNameSpace(LocalSpaceEmitter) )
						{
							bIsLocalSpace = true;
							break;
						}
					}

					if ( bIsLocalSpace == false && CreateParameters.RebaseExcludeAttributes.Contains(Variable.GetName()) == false )
					{
						RebaseVariableNames.AddUnique(Variable.GetName());
					}
				}
				else if ( CanRebaseVariable(Variable) && CreateParameters.RebaseIncludeAttributes.Contains(Variable.GetName()) )
				{
					RebaseVariableNames.AddUnique(Variable.GetName());
				}
			}
		}

		BuildCacheLayout(CacheLayout, SystemCompileData, NiagaraSystem->GetFName(), MoveTemp(RebaseVariableNames), CreateParameters.ExplicitCaptureAttributes);
	}

	void BuildCacheLayoutForEmitter(const FNiagaraSimCacheCreateParameters& CreateParameters, FNiagaraSimCacheDataBuffersLayout& CacheLayout, int EmitterIndex)
	{
		const FNiagaraEmitterHandle& EmitterHandle = NiagaraSystem->GetEmitterHandle(EmitterIndex);
		const FNiagaraEmitterCompiledData& EmitterCompiledData = NiagaraSystem->GetEmitterCompiledData()[EmitterIndex].Get();
		const FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetInstance().GetEmitterData();
		if (EmitterHandle.GetIsEnabled() == false || EmitterData == nullptr)
		{
			return;
		}

		// Find potential candidates for re-basing
		CacheLayout.bLocalSpace = EmitterData->bLocalSpace;

		TArray<FName> RebaseVariableNames;
		if ( CreateParameters.bAllowRebasing && CacheLayout.bLocalSpace == false )
		{
			// Build list of include / exclude names
			TArray<FName> ForceIncludeNames;
			TArray<FName> ForceExcludeNames;
			if ( CreateParameters.RebaseIncludeAttributes.Num() > 0 || CreateParameters.RebaseExcludeAttributes.Num() > 0 )
			{
				const FString EmitterName = EmitterHandle.GetUniqueInstanceName();
				for (FName RebaseName : CreateParameters.RebaseIncludeAttributes)
				{
					FNiagaraVariableBase BaseVar(FNiagaraTypeDefinition::GetFloatDef(), RebaseName);
					if (BaseVar.RemoveRootNamespace(EmitterName))
					{
						ForceIncludeNames.Add(BaseVar.GetName());
					}
				}

				for (FName RebaseName : CreateParameters.RebaseExcludeAttributes)
				{
					FNiagaraVariableBase BaseVar(FNiagaraTypeDefinition::GetFloatDef(), RebaseName);
					if (BaseVar.RemoveRootNamespace(EmitterName))
					{
						ForceExcludeNames.Add(BaseVar.GetName());
					}
				}
			}

		#if WITH_EDITORONLY_DATA
			// Look for renderer attributes bound to Quat / Matrix types are we will want to rebase those
			// We will add all Position types after this so no need to add them here
			EmitterData->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* RenderProperties)
				{
					for (FNiagaraVariable BoundAttribute : RenderProperties->GetBoundAttributes())
					{
						if ( (BoundAttribute.GetType() == FNiagaraTypeDefinition::GetQuatDef()) ||
							 (BoundAttribute.GetType() == FNiagaraTypeDefinition::GetMatrix4Def()) )
						{
							if (BoundAttribute.RemoveRootNamespace(FNiagaraConstants::ParticleAttributeNamespaceString))
							{
								if (EmitterCompiledData.DataSetCompiledData.Variables.Contains(BoundAttribute) && ForceExcludeNames.Contains(BoundAttribute.GetName()) == false )
								{
									RebaseVariableNames.AddUnique(BoundAttribute.GetName());
								}
							}
						}
					}
				}
			);
		#endif

			// Look for regular attributes that we are forcing to rebase or can rebase like positions
			for (const FNiagaraVariable& Variable : EmitterCompiledData.DataSetCompiledData.Variables)
			{
				if ( Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef() )
				{
					if ( ForceExcludeNames.Contains(Variable.GetName()) == false )
					{
						RebaseVariableNames.AddUnique(Variable.GetName());
					}
				}
				else if ( ForceIncludeNames.Contains(Variable.GetName()) && CanRebaseVariable(Variable) )
				{
					RebaseVariableNames.AddUnique(Variable.GetName());
				}
			}
		}
		TArray<FName> ExplicitCaptureAttributes;
		if ( CreateParameters.ExplicitCaptureAttributes.Num() > 0 )
		{
			const FString EmitterName = EmitterHandle.GetUniqueInstanceName();
			for ( FName AttributeName : CreateParameters.ExplicitCaptureAttributes)
			{
				FNiagaraVariableBase AttributeVar(FNiagaraTypeDefinition::GetFloatDef(), AttributeName);
				if (AttributeVar.RemoveRootNamespace(EmitterName))
				{
					if (AttributeVar.RemoveRootNamespace(FNiagaraConstants::ParticleAttributeNamespaceString))
					{
						ExplicitCaptureAttributes.Add(AttributeVar.GetName());
					}
				}
			}
		}

		BuildCacheLayout(CacheLayout, EmitterCompiledData.DataSetCompiledData, EmitterHandle.GetName(), MoveTemp(RebaseVariableNames), ExplicitCaptureAttributes);
	}

	static bool BuildCacheReadMappings(FNiagaraSimCacheDataBuffersLayout& CacheLayout, const FNiagaraDataSetCompiledData& CompiledData)
	{
		const int32 CacheTotalComponents = CacheLayout.FloatCount + CacheLayout.HalfCount + CacheLayout.Int32Count;
		CacheLayout.ComponentMappingsToDataBuffer.Empty(CacheTotalComponents);
		CacheLayout.ComponentMappingsToDataBuffer.AddDefaulted(CacheTotalComponents);
		CacheLayout.VariableMappingsToDataBuffer.Empty(0);

		int32 FloatOffset = 0;
		int32 HalfOffset = CacheLayout.FloatCount;
		int32 Int32Offset = HalfOffset + CacheLayout.HalfCount;
		for (const FNiagaraSimCacheVariable& SourceVariable : CacheLayout.Variables)
		{
			// Find variable, if it doesn't exist that's ok as the cache contains more data than is required
			const int32 DataSetVariableIndex = CompiledData.Variables.IndexOfByPredicate([&](const FNiagaraVariableBase& DataSetVariable) { return DataSetVariable == SourceVariable.Variable; });
			const FNiagaraVariableLayoutInfo* DestVariableLayout = nullptr;
			if (DataSetVariableIndex != INDEX_NONE)
			{
				DestVariableLayout = &CompiledData.VariableLayouts[DataSetVariableIndex];

				// If the variable exists but types not match the cache is invalid
				if ((DestVariableLayout->GetNumFloatComponents() != SourceVariable.FloatCount) ||
					(DestVariableLayout->GetNumHalfComponents() != SourceVariable.HalfCount) ||
					(DestVariableLayout->GetNumInt32Components() != SourceVariable.Int32Count))
				{
					return false;
				}
			}

			// Is this a type that requires conversion / re-basing?
			if (DestVariableLayout != nullptr)
			{
				if ( CacheLayout.RebaseVariableNames.Contains(SourceVariable.Variable.GetName()) )
				{
					if (SourceVariable.Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef())
					{
						check(SourceVariable.FloatCount == 3);
						CacheLayout.VariableMappingsToDataBuffer.Emplace(FloatOffset, DestVariableLayout->FloatComponentStart, &FNiagaraSimCacheHelper::CopyPositions);
						DestVariableLayout = nullptr;
					}
					else if (SourceVariable.Variable.GetType() == FNiagaraTypeDefinition::GetQuatDef())
					{
						check(SourceVariable.FloatCount == 4);
						CacheLayout.VariableMappingsToDataBuffer.Emplace(FloatOffset, DestVariableLayout->FloatComponentStart, &FNiagaraSimCacheHelper::CopyQuaternions);
						DestVariableLayout = nullptr;
					}
					else if (SourceVariable.Variable.GetType() == FNiagaraTypeDefinition::GetMatrix4Def())
					{
						check(SourceVariable.FloatCount == 16);
						CacheLayout.VariableMappingsToDataBuffer.Emplace(FloatOffset, DestVariableLayout->FloatComponentStart, &FNiagaraSimCacheHelper::CopyMatrices);
						DestVariableLayout = nullptr;
					}
				}
			}

			for (int32 i = 0; i < SourceVariable.FloatCount; ++i)
			{
				CacheLayout.ComponentMappingsToDataBuffer[FloatOffset++] = DestVariableLayout ? DestVariableLayout->FloatComponentStart + i : INDEX_NONE;
			}

			for (int32 i = 0; i < SourceVariable.HalfCount; ++i)
			{
				CacheLayout.ComponentMappingsToDataBuffer[HalfOffset++] = DestVariableLayout ? DestVariableLayout->HalfComponentStart + i : INDEX_NONE;
			}

			for (int32 i = 0; i < SourceVariable.Int32Count; ++i)
			{
				CacheLayout.ComponentMappingsToDataBuffer[Int32Offset++] = DestVariableLayout ? DestVariableLayout->Int32ComponentStart + i : INDEX_NONE;
			}
		}

		return true;
	}

	static void CheckedMemcpy(TConstArrayView<uint8> DstArray, uint8* Dst, TConstArrayView<uint8> SrcArray, const uint8* Src, uint32 Size)
	{
		checkf(Src >= SrcArray.GetData() && Src + Size <= SrcArray.GetData() + SrcArray.Num(), TEXT("Source %p-%p is out of bounds, start %p end %p"), Src, Src + Size, SrcArray.GetData(), SrcArray.GetData() + SrcArray.Num());
		checkf(Dst >= DstArray.GetData() && Dst + Size <= DstArray.GetData() + DstArray.Num(), TEXT("Dest %p-%p is out of bounds, start %p end %p"), Dst, Dst + Size, DstArray.GetData(), DstArray.GetData() + DstArray.Num());
		FMemory::Memcpy(Dst, Src, Size);
	}

	void WriteDataBuffer(const FNiagaraDataBuffer& DataBuffer, const FNiagaraSimCacheDataBuffersLayout& CacheLayout, FNiagaraSimCacheDataBuffers& CacheBuffer, int32 FirstInstance, int32 NumInstances)
	{
		if ( NumInstances == 0 )
		{
			return;
		}

		CacheBuffer.NumInstances = NumInstances;

		int32 iComponent = 0;

		// Copy Float
		CacheBuffer.FloatData.AddDefaulted(CacheLayout.FloatCount * NumInstances * sizeof(float));
		for ( uint32 i=0; i < CacheLayout.FloatCount; ++i )
		{
			const uint32 Component = CacheLayout.ComponentMappingsFromDataBuffer[iComponent++];
			const uint8* Source = DataBuffer.GetComponentPtrFloat(Component) + (FirstInstance * sizeof(float));
			uint8* Dest = CacheBuffer.FloatData.GetData() + (i * NumInstances * sizeof(float));
			CheckedMemcpy(CacheBuffer.FloatData, Dest, DataBuffer.GetFloatBuffer(), Source, sizeof(float) * NumInstances);
			//FMemory::Memcpy(Dest, Source, sizeof(float) * NumInstances);
		}
		
		// Copy Half
		CacheBuffer.HalfData.AddDefaulted(CacheLayout.HalfCount * NumInstances * sizeof(FFloat16));
		for (uint32 i = 0; i < CacheLayout.HalfCount; ++i)
		{
			const uint32 Component = CacheLayout.ComponentMappingsFromDataBuffer[iComponent++];
			const uint8* Source = DataBuffer.GetComponentPtrHalf(Component) + (FirstInstance * sizeof(FFloat16));
			uint8* Dest = CacheBuffer.HalfData.GetData() + (i * NumInstances * sizeof(FFloat16));
			CheckedMemcpy(CacheBuffer.HalfData, Dest, DataBuffer.GetHalfBuffer(), Source, sizeof(FFloat16) * NumInstances);
			//FMemory::Memcpy(Dest, Source, sizeof(FFloat16) * NumInstances);
		}

		// Copy Int32
		CacheBuffer.Int32Data.AddDefaulted(CacheLayout.Int32Count * NumInstances * sizeof(int32));
		for (uint32 i = 0; i < CacheLayout.Int32Count; ++i)
		{
			const uint32 Component = CacheLayout.ComponentMappingsFromDataBuffer[iComponent++];
			const uint8* Source = DataBuffer.GetComponentPtrInt32(Component) + (FirstInstance * sizeof(int32));
			uint8* Dest = CacheBuffer.Int32Data.GetData() + (i * NumInstances * sizeof(int32));
			CheckedMemcpy(CacheBuffer.Int32Data, Dest, DataBuffer.GetInt32Buffer(), Source, sizeof(int32) * NumInstances);
			//FMemory::Memcpy(Dest, Source, sizeof(int32) * NumInstances);
		}

		// Copy ID to Index Table
		CacheBuffer.IDToIndexTable = DataBuffer.GetIDTable();
		CacheBuffer.IDAcquireTag = DataBuffer.GetIDAcquireTag();
	}

	void WriteDataBufferGPU(FNiagaraEmitterInstance& EmitterInstance, const FNiagaraDataBuffer& DataBuffer, const FNiagaraSimCacheDataBuffersLayout& CacheLayout, FNiagaraSimCacheDataBuffers& CacheBuffer)
	{
		//-TODO: Make async
		TSharedRef<FNiagaraDataSetReadback> ReadbackRequest = MakeShared<FNiagaraDataSetReadback>();
		ReadbackRequest->ImmediateReadback(&EmitterInstance);
		if ( FNiagaraDataBuffer* CurrentData = ReadbackRequest->GetDataSet().GetCurrentData() )
		{
			WriteDataBuffer(*CurrentData, CacheLayout, CacheBuffer, 0, CurrentData->GetNumInstances());
		}
	}

	void ReadDataBuffer(const FTransform& RebaseTransform, const FNiagaraSimCacheDataBuffersLayout& CacheLayout, const FNiagaraSimCacheDataBuffers& CacheBuffer, FNiagaraDataSet& DataSet)
	{
		FNiagaraDataBuffer& DataBuffer = DataSet.BeginSimulate();
		DataBuffer.Allocate(CacheBuffer.NumInstances);
		DataBuffer.SetNumInstances(CacheBuffer.NumInstances);
		if ( CacheBuffer.NumInstances > 0 )
		{
			int32 iComponent = 0;
			const int32 NumInstances = CacheBuffer.NumInstances;

			// Copy Float
			for (uint32 i=0; i < CacheLayout.FloatCount; ++i)
			{
				const uint32 Component = CacheLayout.ComponentMappingsToDataBuffer[iComponent++];
				if (Component == InvalidComponent)
				{
					continue;
				}
				const uint8* Source = CacheBuffer.FloatData.GetData() + (i * NumInstances * sizeof(float));
				uint8* Dest = DataBuffer.GetComponentPtrFloat(Component);
				CheckedMemcpy(DataBuffer.GetFloatBuffer(), Dest, CacheBuffer.FloatData, Source, sizeof(float) * NumInstances);
				//FMemory::Memcpy(Dest, Source, sizeof(float) * NumInstances);
			}

			// Copy Half
			for (uint32 i = 0; i < CacheLayout.HalfCount; ++i)
			{
				const uint32 Component = CacheLayout.ComponentMappingsToDataBuffer[iComponent++];
				if (Component == InvalidComponent)
				{
					continue;
				}
				const uint8* Source = CacheBuffer.HalfData.GetData() + (i * NumInstances * sizeof(FFloat16));
				uint8* Dest = DataBuffer.GetComponentPtrHalf(Component);
				CheckedMemcpy(DataBuffer.GetHalfBuffer(), Dest, CacheBuffer.HalfData, Source, sizeof(FFloat16) * NumInstances);
				//FMemory::Memcpy(Dest, Source, sizeof(FFloat16) * NumInstances);
			}

			// Copy Int32
			for (uint32 i = 0; i < CacheLayout.Int32Count; ++i)
			{
				const uint32 Component = CacheLayout.ComponentMappingsToDataBuffer[iComponent++];
				if (Component == InvalidComponent)
				{
					continue;
				}
				const uint8* Source = CacheBuffer.Int32Data.GetData() + (i * NumInstances * sizeof(int32));
				uint8* Dest = DataBuffer.GetComponentPtrInt32(Component);
				CheckedMemcpy(DataBuffer.GetInt32Buffer(), Dest, CacheBuffer.Int32Data, Source, sizeof(int32) * NumInstances);
				//FMemory::Memcpy(Dest, Source, sizeof(int32) * NumInstances);
			}

			// Copy variables that require processing
			for ( const FNiagaraSimCacheDataBuffersLayout::FVariableCopyInfo& VariableCopyInfo : CacheLayout.VariableMappingsToDataBuffer )
			{
				const uint32 SrcStride = uint32(NumInstances) * sizeof(float);
				const uint8* Src = CacheBuffer.FloatData.GetData() + (VariableCopyInfo.ComponentFrom * SrcStride);
				uint8* Dst = DataBuffer.GetComponentPtrFloat(VariableCopyInfo.ComponentTo);
				VariableCopyInfo.CopyFunc(Dst, DataBuffer.GetFloatStride(), Src, SrcStride, uint32(NumInstances), RebaseTransform);
			}
		}

		//-TODO:DestinationDataBuffer.SetIDTable(CacheBuffer.IDToIndexTable);
		DataBuffer.SetIDAcquireTag(CacheBuffer.IDAcquireTag);

		DataSet.EndSimulate();
	}

	void ReadDataBufferGPU(const FTransform& InRebaseTransform, FNiagaraEmitterInstance& EmitterInstance, const FNiagaraSimCacheDataBuffersLayout& InCacheLayout, const FNiagaraSimCacheDataBuffers& InCacheBuffer, FNiagaraDataSet& InDataSet, std::atomic<int32>& InPendingCommandsCounter)
	{
		if (EmitterInstance.IsDisabled())
		{
			return;
		}

		++InPendingCommandsCounter;

		check(EmitterInstance.GetGPUContext());

		FNiagaraGpuComputeDispatchInterface* DispathInterface = EmitterInstance.GetParentSystemInstance()->GetComputeDispatchInterface();
		ENQUEUE_RENDER_COMMAND(NiagaraSimCacheGpuReadFrame)(
			[DispathInterface, GPUExecContext=EmitterInstance.GetGPUContext(), RebaseTransform=InRebaseTransform, CacheLayout=&InCacheLayout, CacheBuffer=&InCacheBuffer, DataSet=&InDataSet, PendingCommandsCounter=&InPendingCommandsCounter](FRHICommandListImmediate& RHICmdList)
			{
				const int32 NumInstances = CacheBuffer->NumInstances;

				// Set Instance Count
				{
					FNiagaraGPUInstanceCountManager& CountManager = DispathInterface->GetGPUInstanceCounterManager();
					if (GPUExecContext->CountOffset_RT == INDEX_NONE)
					{
						GPUExecContext->CountOffset_RT = CountManager.AcquireOrAllocateEntry(RHICmdList);
					}

					const FRWBuffer& CountBuffer = CountManager.GetInstanceCountBuffer();
					const TPair<uint32, int32> DataToSet(GPUExecContext->CountOffset_RT, NumInstances);
					RHICmdList.Transition(FRHITransitionInfo(CountBuffer.UAV, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState, ERHIAccess::UAVCompute));
					NiagaraClearCounts::ClearCountsInt(RHICmdList, CountBuffer.UAV, MakeArrayView(&DataToSet, 1));
					RHICmdList.Transition(FRHITransitionInfo(CountBuffer.UAV, ERHIAccess::UAVCompute, FNiagaraGPUInstanceCountManager::kCountBufferDefaultState));
				}

				// Copy instance counts
				FNiagaraDataBuffer& DataBuffer = DataSet->GetCurrentDataChecked();
				DataBuffer.AllocateGPU(RHICmdList, NumInstances, DispathInterface->GetFeatureLevel(), TEXT("NiagaraSimCache"));
				DataBuffer.SetNumInstances(NumInstances);
				DataBuffer.SetGPUDataReadyStage(ENiagaraGpuComputeTickStage::PreInitViews);
				GPUExecContext->SetDataToRender(&DataBuffer);

				if (CacheBuffer->NumInstances > 0 )
				{
					int32 iComponent = 0;

					// Copy Float
					if ( CacheLayout->FloatCount > 0 )
					{
						FRWBuffer& RWBuffer = DataBuffer.GetGPUBufferFloat();
						const int32 RWComponentStride = DataBuffer.GetFloatStride();
						uint8* RWBufferMemory = reinterpret_cast<uint8*>(RHILockBuffer(RWBuffer.Buffer, 0, RWBuffer.NumBytes, RLM_WriteOnly));

						for (uint32 i=0; i < CacheLayout->FloatCount; ++i)
						{
							const uint32 Component = CacheLayout->ComponentMappingsToDataBuffer[iComponent++];
							if (Component == InvalidComponent)
							{
								continue;
							}
							const uint8* Source = CacheBuffer->FloatData.GetData() + (i * NumInstances * sizeof(float));
							uint8* Dest = RWBufferMemory + (uint32(Component) * RWComponentStride);
							CheckedMemcpy(MakeArrayView(RWBufferMemory, RWBuffer.NumBytes), Dest, CacheBuffer->FloatData, Source, sizeof(float) * NumInstances);
							//FMemory::Memcpy(Dest, Source, sizeof(float) * NumInstances);
						}

						// Copy variables that require processing
						for (const FNiagaraSimCacheDataBuffersLayout::FVariableCopyInfo& VariableCopyInfo : CacheLayout->VariableMappingsToDataBuffer)
						{
							const uint32 SrcStride = uint32(NumInstances) * sizeof(float);
							const uint8* Src = CacheBuffer->FloatData.GetData() + (VariableCopyInfo.ComponentFrom * SrcStride);
							uint8* Dst = RWBufferMemory + (uint32(VariableCopyInfo.ComponentTo) * RWComponentStride);
							VariableCopyInfo.CopyFunc(Dst, DataBuffer.GetFloatStride(), Src, SrcStride, uint32(NumInstances), RebaseTransform);
						}

						RHIUnlockBuffer(RWBuffer.Buffer);
					}

					// Copy Half
					if (CacheLayout->HalfCount > 0)
					{
						FRWBuffer& RWBuffer = DataBuffer.GetGPUBufferHalf();
						const int32 RWComponentStride = DataBuffer.GetHalfStride();
						uint8* RWBufferMemory = reinterpret_cast<uint8*>(RHILockBuffer(RWBuffer.Buffer, 0, RWBuffer.NumBytes, RLM_WriteOnly));

						for (uint32 i = 0; i < CacheLayout->HalfCount; ++i)
						{
							const uint32 Component = CacheLayout->ComponentMappingsToDataBuffer[iComponent++];
							if (Component == InvalidComponent)
							{
								continue;
							}
							const uint8* Source = CacheBuffer->HalfData.GetData() + (i * NumInstances * sizeof(FFloat16));
							uint8* Dest = RWBufferMemory + (uint32(Component) * RWComponentStride);
							CheckedMemcpy(MakeArrayView(RWBufferMemory, RWBuffer.NumBytes), Dest, CacheBuffer->HalfData, Source, sizeof(FFloat16) * NumInstances);
							//FMemory::Memcpy(Dest, Source, sizeof(FFloat16) * NumInstances);
						}

						RHIUnlockBuffer(RWBuffer.Buffer);
					}


					// Copy Int32
					if (CacheLayout->Int32Count > 0)
					{
						FRWBuffer& RWBuffer = DataBuffer.GetGPUBufferInt();
						const int32 RWComponentStride = DataBuffer.GetInt32Stride();
						uint8* RWBufferMemory = reinterpret_cast<uint8*>(RHILockBuffer(RWBuffer.Buffer, 0, RWBuffer.NumBytes, RLM_WriteOnly));

						for (uint32 i = 0; i < CacheLayout->Int32Count; ++i)
						{
							const uint32 Component = CacheLayout->ComponentMappingsToDataBuffer[iComponent++];
							if (Component == InvalidComponent)
							{
								continue;
							}
							const uint8* Source = CacheBuffer->Int32Data.GetData() + (i * NumInstances * sizeof(int32));
							uint8* Dest = RWBufferMemory + (uint32(Component) * RWComponentStride);
							CheckedMemcpy(MakeArrayView(RWBufferMemory, RWBuffer.NumBytes), Dest, CacheBuffer->Int32Data, Source, sizeof(int32) * NumInstances);
							//FMemory::Memcpy(Dest, Source, sizeof(int32) * NumInstances);
						}

						RHIUnlockBuffer(RWBuffer.Buffer);
					}
				}

				//-TODO:DestinationDataBuffer.SetIDTable(CacheBuffer.IDToIndexTable);
				DataBuffer.SetIDAcquireTag(CacheBuffer->IDAcquireTag);

				// Ensure we decrement our counter so the GameThread knows the state of things
				--(*PendingCommandsCounter);
			}
		);
	}

	static bool CanRebaseVariable(const FNiagaraVariableBase& Variable)
	{
		return	
			(Variable.GetType() == FNiagaraTypeDefinition::GetQuatDef()) ||
			(Variable.GetType() == FNiagaraTypeDefinition::GetMatrix4Def()) ||
			(Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef());
	}

	static void CopyPositions(uint8* Dst, uint32 DstStride, const uint8* Src, uint32 SrcStride, uint32 NumInstances, const FTransform& RebaseTransform)
	{
		float* DstFloats = reinterpret_cast<float*>(Dst);
		DstStride = DstStride >> 2;

		const float* SrcFloats = reinterpret_cast<const float*>(Src);
		SrcStride = SrcStride >> 2;

		for (uint32 i = 0; i < NumInstances; ++i)
		{
			const FVector CachePosition(
				SrcFloats[i + (SrcStride * 0)],
				SrcFloats[i + (SrcStride * 1)],
				SrcFloats[i + (SrcStride * 2)]
			);
			const FVector RebasedPosition = RebaseTransform.TransformPosition(CachePosition);
			DstFloats[i + (DstStride * 0)] = RebasedPosition.X;
			DstFloats[i + (DstStride * 1)] = RebasedPosition.Y;
			DstFloats[i + (DstStride * 2)] = RebasedPosition.Z;
		}
	}

	static void CopyQuaternions(uint8* Dst, uint32 DstStride, const uint8* Src, uint32 SrcStride, uint32 NumInstances, const FTransform& RebaseTransform)
	{
		float* DstFloats = reinterpret_cast<float*>(Dst);
		DstStride = DstStride >> 2;

		const float* SrcFloats = reinterpret_cast<const float*>(Src);
		SrcStride = SrcStride >> 2;

		for (uint32 i = 0; i < NumInstances; ++i)
		{
			const FQuat4f CacheRotation(
				SrcFloats[i + (SrcStride * 0)],
				SrcFloats[i + (SrcStride * 1)],
				SrcFloats[i + (SrcStride * 2)],
				SrcFloats[i + (SrcStride * 3)]
			);
			const FQuat4f RebasedQuat = CacheRotation * FQuat4f(RebaseTransform.GetRotation());
			DstFloats[i + (DstStride * 0)] = RebasedQuat.X;
			DstFloats[i + (DstStride * 1)] = RebasedQuat.Y;
			DstFloats[i + (DstStride * 2)] = RebasedQuat.Z;
			DstFloats[i + (DstStride * 3)] = RebasedQuat.W;
		}
	}

	static void CopyMatrices(uint8* Dst, uint32 DstStride, const uint8* Src, uint32 SrcStride, uint32 NumInstances, const FTransform& RebaseTransform)
	{
		float* DstFloats = reinterpret_cast<float*>(Dst);
		DstStride = DstStride >> 2;

		const float* SrcFloats = reinterpret_cast<const float*>(Src);
		SrcStride = SrcStride >> 2;

		const FMatrix44d RebaseMatrix = RebaseTransform.ToMatrixWithScale();
		for (uint32 i = 0; i < NumInstances; ++i)
		{
			FMatrix44d CacheMatrix;
			for (int32 j = 0; j < 16; ++j)
			{
				CacheMatrix.M[j >> 2][j & 0x3] = double(SrcFloats[i + (SrcStride * j)]);
			}

			CacheMatrix = CacheMatrix * RebaseMatrix;

			for (int32 j = 0; j < 16; ++j)
			{
				DstFloats[i + (DstStride * j)] = float(CacheMatrix.M[j >> 2][j & 0x3]);
			}
		}
	}

	UNiagaraSystem*						NiagaraSystem = nullptr;
	FNiagaraSystemInstance*				SystemInstance = nullptr;
	FNiagaraSystemSimulationPtr			SystemSimulation = nullptr;
	FNiagaraDataBuffer*					SystemSimulationDataBuffer = nullptr;

	static constexpr uint16				InvalidComponent = INDEX_NONE;
};
