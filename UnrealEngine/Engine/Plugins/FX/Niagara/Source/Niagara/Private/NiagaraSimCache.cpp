// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCache.h"

#include "NiagaraClearCounts.h"
#include "NiagaraConstants.h"
#include "NiagaraComponent.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraDataSetReadback.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraGPUInstanceCountManager.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraSimCacheAttributeReaderHelper.h"
#include "NiagaraSimCacheHelper.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSimCache)

UNiagaraSimCache::FOnCacheBeginWrite	UNiagaraSimCache::OnCacheBeginWrite;
UNiagaraSimCache::FOnCacheEndWrite		UNiagaraSimCache::OnCacheEndWrite;

UNiagaraSimCache::UNiagaraSimCache(const FObjectInitializer& ObjectInitializer)
{
}

bool UNiagaraSimCache::IsReadyForFinishDestroy()
{
	return PendingCommandsInFlight == 0;
}

bool UNiagaraSimCache::BeginWrite(FNiagaraSimCacheCreateParameters InCreateParameters, UNiagaraComponent* NiagaraComponent)
{
	check(PendingCommandsInFlight == 0);

	OnCacheBeginWrite.Broadcast(this);

	FNiagaraSimCacheHelper Helper(NiagaraComponent);
	if (Helper.HasValidSimulation() == false)
	{
		return false;
	}

	Modify();
#if WITH_EDITOR
	PostEditChange();
#endif

	// Reset to defaults
	SoftNiagaraSystem = Helper.NiagaraSystem;
	CreateParameters = InCreateParameters;
	StartSeconds = 0.0f;
	DurationSeconds = 0.0f;
	CacheLayout = FNiagaraSimCacheLayout();
	CacheFrames.Empty();
	CaptureTickCount = INDEX_NONE;

	for ( auto it=DataInterfaceStorage.CreateIterator(); it; ++it )
	{
		it->Value->MarkAsGarbage();
	}
	DataInterfaceStorage.Empty();

	// Not explicit capture mode?  Empty the list as internally we reuse this list when in rendering only mode
	if (CreateParameters.AttributeCaptureMode == ENiagaraSimCacheAttributeCaptureMode::ExplicitAttributes)
	{
		// Invalid as the user specified nothing to actual capture
		if (CreateParameters.ExplicitCaptureAttributes.IsEmpty())
		{
			SoftNiagaraSystem.Reset();
			return false;
		}
	}
	else
	{
		CreateParameters.ExplicitCaptureAttributes.Empty();
	}

	// When in rendering only mode ask all renderers which attributes we need to capture
	if ( CreateParameters.AttributeCaptureMode == ENiagaraSimCacheAttributeCaptureMode::RenderingOnly )
	{
	#if WITH_EDITORONLY_DATA
		for ( const FNiagaraEmitterHandle& EmitterHandle : Helper.NiagaraSystem->GetEmitterHandles() )
		{
			EmitterHandle.GetInstance().GetEmitterData()->ForEachEnabledRenderer(
				[&](UNiagaraRendererProperties* RenderProperties)
				{
					for (FNiagaraVariableBase BoundAttribute : RenderProperties->GetBoundAttributes())
					{
						FNameBuilder NameBuilder;
						NameBuilder.Append(EmitterHandle.GetUniqueInstanceName());
						NameBuilder.AppendChar(TEXT('.'));
						BoundAttribute.GetName().AppendString(NameBuilder);

						CreateParameters.ExplicitCaptureAttributes.AddUnique(NameBuilder.ToString());
					}
				}
			);
		}
	#endif
		for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& EmitterInstance : Helper.SystemInstance->GetEmitters())
		{
			const FNiagaraParameterStore& RendererBindings = EmitterInstance->GetRendererBoundVariables();
			for ( const FNiagaraVariableWithOffset& Variable : RendererBindings.ReadParameterVariables() )
			{
				CreateParameters.ExplicitCaptureAttributes.AddUnique(Variable.GetName());
			}
		}

		FNiagaraDataInterfaceUtilities::ForEachDataInterface(
			Helper.SystemInstance,
			[&](const FNiagaraDataInterfaceUtilities::FDataInterfaceUsageContext& UsageContext)
			{
				for ( const FNiagaraVariableBase& PreserveAttribute : UsageContext.DataInterface->GetSimCacheRendererAttributes(UsageContext.OwnerObject) )
				{
					CreateParameters.ExplicitCaptureAttributes.AddUnique(PreserveAttribute.GetName());
				}
				return true;
			}
		);
	}

	// Build new layout for system / emitters
	Helper.BuildCacheLayoutForSystem(CreateParameters, CacheLayout.SystemLayout);

	const int32 NumEmitters = Helper.NiagaraSystem->GetEmitterHandles().Num();
	CacheLayout.EmitterLayouts.AddDefaulted(NumEmitters);
	for ( int32 i=0; i < NumEmitters; ++i )
	{
		Helper.BuildCacheLayoutForEmitter(CreateParameters, CacheLayout.EmitterLayouts[i], i);
	}

	// Find data interfaces we may want to cache
	if ( CreateParameters.bAllowDataInterfaceCaching )
	{
		FNiagaraDataInterfaceUtilities::ForEachDataInterface(
			Helper.SystemInstance,
			[&](const FNiagaraVariableBase& Variable, UNiagaraDataInterface* DataInterface)
			{
				if ( (CreateParameters.ExplicitCaptureAttributes.Num() == 0) || CreateParameters.ExplicitCaptureAttributes.Contains(Variable.GetName()) )
				{
					const void* PerInstanceData = Helper.SystemInstance->FindDataInterfaceInstanceData(DataInterface);
					if (UObject* DICacheStorage = DataInterface->SimCacheBeginWrite(this, Helper.SystemInstance, PerInstanceData))
					{
						DataInterfaceStorage.FindOrAdd(Variable) = DICacheStorage;
					}
				}
				return true;
			}
		);
	}
	return true;
}

bool UNiagaraSimCache::WriteFrame(UNiagaraComponent* NiagaraComponent)
{
	FNiagaraSimCacheHelper Helper(NiagaraComponent);
	if (Helper.HasValidSimulationData() == false)
	{
		SoftNiagaraSystem.Reset();
		return false;
	}

	if ( SoftNiagaraSystem.Get() != Helper.NiagaraSystem )
	{
		SoftNiagaraSystem.Reset();
		return false;
	}

	// Simulation is complete nothing to cache
	if ( Helper.SystemInstance->IsComplete() )
	{
		return false;
	}

	// Is the simulation running?  If not nothing to cache yet
	if ( Helper.SystemInstance->SystemInstanceState != ENiagaraSystemInstanceState::Running )
	{
		return false;
	}

	// First frame we are about to cache?
	if (CaptureTickCount == INDEX_NONE)
	{
		StartSeconds = Helper.SystemInstance->GetAge();
		CaptureTickCount = Helper.SystemInstance->GetTickCount();
	}
	// If our tick counter hasn't moved then we won't capture a frame as there's nothing new to process
	else if (CaptureTickCount == Helper.SystemInstance->GetTickCount())
	{
		return false;
	}

	// If the tick counter is lower than the previous value then the system was reset so we won't capture
	if ( Helper.SystemInstance->GetTickCount() < CaptureTickCount )
	{
		SoftNiagaraSystem.Reset();
		return false;
	}

	DurationSeconds = Helper.SystemInstance->GetAge() - StartSeconds;

	// Cache frame
	FNiagaraSimCacheFrame& CacheFrame = CacheFrames.AddDefaulted_GetRef();
	CacheFrame.LocalToWorld = Helper.SystemInstance->GatheredInstanceParameters.ComponentTrans;
	CacheFrame.LWCTile = Helper.SystemInstance->GetLWCTile();

	CacheFrame.SystemData.LocalBounds = Helper.SystemInstance->GetLocalBounds();

	const int32 NumEmitters = CacheLayout.EmitterLayouts.Num();
	CacheFrame.EmitterData.AddDefaulted(NumEmitters);

	Helper.WriteDataBuffer(*Helper.SystemSimulationDataBuffer, CacheLayout.SystemLayout, CacheFrame.SystemData.SystemDataBuffers, Helper.SystemInstance->GetSystemInstanceIndex(), 1);

	bool bNeedsGpuTickFlush = true;
	for ( int32 i=0; i < NumEmitters; ++i )
	{
		FNiagaraSimCacheEmitterFrame& CacheEmitterFrame = CacheFrame.EmitterData[i];
		FNiagaraEmitterInstance& EmitterInstance = Helper.SystemInstance->GetEmitters()[i].Get();
		FNiagaraDataBuffer* EmitterCurrentData = EmitterInstance.GetData().GetCurrentData();
		if (EmitterInstance.IsComplete() || !EmitterCurrentData)
		{
			continue;
		}

		CacheEmitterFrame.LocalBounds = EmitterInstance.GetBounds();
		CacheEmitterFrame.TotalSpawnedParticles = EmitterInstance.GetTotalSpawnedParticles();
		if (CacheLayout.EmitterLayouts[i].SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			// First time we encounter a GPU emitter we need to process we also need to flush EOF updates / the compute process to ensure we get latest data
			if (bNeedsGpuTickFlush)
			{
				UWorld* World = NiagaraComponent->GetWorld();
				if (ensure(World))
				{
					World->SendAllEndOfFrameUpdates();

					FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World);
					if (ensureMsgf(ComputeDispatchInterface, TEXT("Compute dispatch interface was invalid when flushing commands")))
					{
						ComputeDispatchInterface->FlushPendingTicks_GameThread();
					}
				}

				bNeedsGpuTickFlush = false;
			}

			Helper.WriteDataBufferGPU(EmitterInstance, *EmitterCurrentData, CacheLayout.EmitterLayouts[i], CacheEmitterFrame.ParticleDataBuffers);
		}
		else
		{
			Helper.WriteDataBuffer(*EmitterCurrentData, CacheLayout.EmitterLayouts[i], CacheEmitterFrame.ParticleDataBuffers, 0, EmitterCurrentData->GetNumInstances());
		}
	}

	// Store data interface data
	//-OPT: We shouldn't need to search all the time here
	if (DataInterfaceStorage.IsEmpty() == false)
	{
		const int FrameIndex = CacheFrames.Num() - 1;
		bool bDataInterfacesSucess = true;

		FNiagaraDataInterfaceUtilities::ForEachDataInterface(
			Helper.SystemInstance,
			[&](const FNiagaraVariableBase& Variable, UNiagaraDataInterface* DataInterface)
			{
				if ( UObject* StorageObject = DataInterfaceStorage.FindRef(Variable) )
				{
					const void* PerInstanceData = Helper.SystemInstance->FindDataInterfaceInstanceData(DataInterface);
					bDataInterfacesSucess &= DataInterface->SimCacheWriteFrame(StorageObject, FrameIndex, Helper.SystemInstance, PerInstanceData);
				}
				return true;
			}
		);

		// A data interface failed to write information
		if (bDataInterfacesSucess == false)
		{
			SoftNiagaraSystem.Reset();
			return false;
		}
	}
	return true;
}

bool UNiagaraSimCache::EndWrite()
{
	check(PendingCommandsInFlight == 0);
	if ( CacheFrames.Num() == 0 )
	{
		SoftNiagaraSystem.Reset();
	}

	if (DataInterfaceStorage.IsEmpty() == false)
	{
		bool bDataInterfacesSucess = true;
		for ( auto it=DataInterfaceStorage.CreateIterator(); it; ++it )
		{
			UClass* DataInterfaceClass = it.Key().GetType().GetClass();
			check(DataInterfaceClass != nullptr);
			UNiagaraDataInterface* DataInterface = CastChecked<UNiagaraDataInterface>(DataInterfaceClass->GetDefaultObject());
			bDataInterfacesSucess &= DataInterface->SimCacheEndWrite(it.Value());
		}

		if (bDataInterfacesSucess == false)
		{
			SoftNiagaraSystem.Reset();
		}
	}

	OnCacheEndWrite.Broadcast(this);
	return IsCacheValid();
}

bool UNiagaraSimCache::CanRead(UNiagaraSystem* NiagaraSystem)
{
	check(IsInGameThread());

	if ( NiagaraSystem != SoftNiagaraSystem.Get() )
	{
		return false;
	}

	if ( NiagaraSystem->IsReadyToRun() == false )
	{
		return false;
	}

	// Uncooked platforms can recompile the system so we need to detect if a recache is required
	//-OPT: This should use the changed notification delegate to avoid checks
#if WITH_EDITORONLY_DATA
	if ( !bNeedsReadComponentMappingRecache )
	{
		uint32 CacheVMIndex = 0;
		NiagaraSystem->ForEachScript(
			[&](UNiagaraScript* Script)
			{
				if (CachedScriptVMIds.IsValidIndex(CacheVMIndex))
				{
					bNeedsReadComponentMappingRecache |= CachedScriptVMIds[CacheVMIndex] != Script->GetVMExecutableDataCompilationId();
				}
				else
				{
					bNeedsReadComponentMappingRecache = true;
				}
			}
		);
	}
#endif

	if (bNeedsReadComponentMappingRecache)
	{
		const int32 NumEmitters = NiagaraSystem->GetEmitterHandles().Num();
		if (NumEmitters != CacheLayout.EmitterLayouts.Num())
		{
			return false;
		}

		bool bCacheValid = true;
		bCacheValid &= FNiagaraSimCacheHelper::BuildCacheReadMappings(CacheLayout.SystemLayout, NiagaraSystem->GetSystemCompiledData().DataSetCompiledData);

		for ( int32 i=0; i < NumEmitters; ++i )
		{
			const FNiagaraEmitterCompiledData& EmitterCompiledData = NiagaraSystem->GetEmitterCompiledData()[i].Get();
			FNiagaraSimCacheDataBuffersLayout& EmitterLayout = CacheLayout.EmitterLayouts[i];
			bCacheValid &= FNiagaraSimCacheHelper::BuildCacheReadMappings(EmitterLayout, EmitterCompiledData.DataSetCompiledData);
			bCacheValid &= EmitterLayout.SimTarget == EmitterCompiledData.DataSetCompiledData.SimTarget;
		}

		if (bCacheValid == false)
		{
			return false;
		}

#if WITH_EDITORONLY_DATA
		// Gather all the CachedScriptVMIds
		CachedScriptVMIds.Empty();
		NiagaraSystem->ForEachScript(
			[&](UNiagaraScript* Script)
			{
				CachedScriptVMIds.Add(Script->GetVMExecutableDataCompilationId());
			}
		);
		CachedScriptVMIds.Shrink();
#endif
		bNeedsReadComponentMappingRecache = false;
	}

	return true;
}

bool UNiagaraSimCache::Read(float TimeSeconds, FNiagaraSystemInstance* SystemInstance) const
{
	const float RelativeTime = FMath::Max(TimeSeconds - StartSeconds, 0.0f);
	if ( RelativeTime > DurationSeconds )
	{
		// Complete
		return false;
	}

	const float FrameTime		= DurationSeconds > 0.0f ? (RelativeTime / DurationSeconds) * float(CacheFrames.Num() - 1) : 0.0f;
	const float FrameIndex		= FMath::Floor(FrameTime);
	const float FrameFraction	= FrameTime - float(FrameIndex);

	return ReadFrame(FrameIndex, FrameFraction, SystemInstance);
}

bool UNiagaraSimCache::ReadFrame(int32 FrameIndex, float FrameFraction, FNiagaraSystemInstance* SystemInstance) const
{
	FNiagaraSimCacheHelper Helper(SystemInstance);
	if ( !Helper.HasValidSimulation() )
	{
		return false;
	}

	const FNiagaraSimCacheFrame& CacheFrame = CacheFrames[FrameIndex];

	FTransform RebaseTransform = FTransform::Identity;
	if ( USceneComponent* AttachComponent = SystemInstance->GetAttachComponent() )
	{
		RebaseTransform = AttachComponent->GetComponentToWorld();
		RebaseTransform.AddToTranslation(FVector(SystemInstance->GetLWCTile()) * -FLargeWorldRenderScalar::GetTileSize());
		RebaseTransform = RebaseTransform * CacheFrame.LocalToWorld.Inverse();
	}

	Helper.SystemInstance->LocalBounds = CacheFrame.SystemData.LocalBounds;
	Helper.ReadDataBuffer(RebaseTransform, CacheLayout.SystemLayout, CacheFrame.SystemData.SystemDataBuffers, Helper.GetSystemSimulationDataSet());

	const int32 NumEmitters = CacheLayout.EmitterLayouts.Num();
	for (int32 i=0; i < NumEmitters; ++i)
	{
		const FNiagaraSimCacheEmitterFrame& CacheEmitterFrame = CacheFrame.EmitterData[i];
		FNiagaraEmitterInstance& EmitterInstance = Helper.SystemInstance->GetEmitters()[i].Get();
		EmitterInstance.CachedBounds = CacheEmitterFrame.LocalBounds;
		EmitterInstance.TotalSpawnedParticles = CacheEmitterFrame.TotalSpawnedParticles;

		if (CacheLayout.EmitterLayouts[i].SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			Helper.ReadDataBufferGPU(RebaseTransform, EmitterInstance, CacheLayout.EmitterLayouts[i], CacheEmitterFrame.ParticleDataBuffers, EmitterInstance.GetData(), PendingCommandsInFlight);
		}
		else
		{
			Helper.ReadDataBuffer(RebaseTransform, CacheLayout.EmitterLayouts[i], CacheEmitterFrame.ParticleDataBuffers, EmitterInstance.GetData());
		}
	}

	// Store data interface data
	//-OPT: We shouldn't need to search all the time here
	if (DataInterfaceStorage.IsEmpty() == false)
	{
		const int NextFrameIndex = FMath::Min(FrameIndex + 1, CacheFrames.Num() - 1);
		bool bDataInterfacesSucess = true;

		FNiagaraDataInterfaceUtilities::ForEachDataInterface(
			Helper.SystemInstance,
			[&](const FNiagaraVariableBase& Variable, UNiagaraDataInterface* DataInterface)
			{
				if (UObject* StorageObject = DataInterfaceStorage.FindRef(Variable))
				{
					void* PerInstanceData = Helper.SystemInstance->FindDataInterfaceInstanceData(DataInterface);
					bDataInterfacesSucess &= DataInterface->SimCacheReadFrame(StorageObject, FrameIndex, NextFrameIndex, FrameFraction, Helper.SystemInstance, PerInstanceData);
				}
				return true;
			}
		);


		if (bDataInterfacesSucess == false)
		{
			return false;
		}
	}

	//-TODO: This should loop over all DataInterfaces that register not just ones with instance data
	for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& DataInterfacePair : SystemInstance->DataInterfaceInstanceDataOffsets)
	{
		if (UNiagaraDataInterface* Interface = DataInterfacePair.Key.Get())
		{
			Interface->SimCachePostReadFrame(&SystemInstance->DataInterfaceInstanceData[DataInterfacePair.Value], SystemInstance);
		}
	}
	return true;
}

int UNiagaraSimCache::GetEmitterIndex(FName EmitterName) const
{
	if ( IsCacheValid() )
	{
		for ( int i=0; i < CacheLayout.EmitterLayouts.Num(); ++i )
		{
			if ( CacheLayout.EmitterLayouts[i].LayoutName == EmitterName )
			{
				return i;
			}
		}
	}
	return INDEX_NONE;
}

TArray<FName> UNiagaraSimCache::GetEmitterNames() const
{
	TArray<FName> EmitterNames;
	if ( IsCacheValid() )
	{
		EmitterNames.Reserve(CacheLayout.EmitterLayouts.Num());
		for (const FNiagaraSimCacheDataBuffersLayout& EmitterLayout : CacheLayout.EmitterLayouts)
		{
			EmitterNames.Add(EmitterLayout.LayoutName);
		}
	}

	return EmitterNames;
}

int UNiagaraSimCache::GetEmitterNumInstances(int32 EmitterIndex, int32 FrameIndex) const
{
	if ( CacheFrames.IsValidIndex(FrameIndex) )
	{
		if ( EmitterIndex == INDEX_NONE )
		{
			return CacheFrames[FrameIndex].SystemData.SystemDataBuffers.NumInstances;
		}
		else if ( CacheFrames[FrameIndex].EmitterData.IsValidIndex(EmitterIndex) )
		{
			return CacheFrames[FrameIndex].EmitterData[EmitterIndex].ParticleDataBuffers.NumInstances;
		}
	}
	return 0;
}

void UNiagaraSimCache::ForEachEmitterAttribute(int32 EmitterIndex, TFunction<bool(const FNiagaraSimCacheVariable&)> Function) const
{
	if (EmitterIndex == INDEX_NONE)
	{
		for (const FNiagaraSimCacheVariable& CacheVariable : CacheLayout.SystemLayout.Variables)
		{
			if ( Function(CacheVariable) == false )
			{
				break;
			}
		}
	}
	else if (CacheLayout.EmitterLayouts.IsValidIndex(EmitterIndex))
	{
		for (const FNiagaraSimCacheVariable& CacheVariable : CacheLayout.EmitterLayouts[EmitterIndex].Variables)
		{
			if (Function(CacheVariable) == false)
			{
				break;
			}
		}
	}
}

void UNiagaraSimCache::ReadAttribute(TArray<float>& OutFloats, TArray<FFloat16>& OutHalfs, TArray<int32>& OutInts, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if ( AttributeReader.IsValid() )
	{
		const int32 OutFloatOffset = OutFloats.AddUninitialized(AttributeReader.Variable->FloatCount * AttributeReader.GetNumInstances());
		for ( int32 i=0; i < AttributeReader.Variable->FloatCount; ++i )
		{
			AttributeReader.CopyComponentFloats(i, &OutFloats[OutFloatOffset + (i * AttributeReader.GetNumInstances())]);
		}

		const int32 OutHalfOffset = OutHalfs.AddUninitialized(AttributeReader.Variable->HalfCount * AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.Variable->HalfCount; ++i)
		{
			AttributeReader.CopyComponentHalfs(i, &OutHalfs[OutHalfOffset + (i * AttributeReader.GetNumInstances())]);
		}

		const int32 OutIntOffset = OutInts.AddUninitialized(AttributeReader.Variable->Int32Count * AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.Variable->Int32Count; ++i)
		{
			AttributeReader.CopyComponentInts(i, &OutInts[OutIntOffset + (i * AttributeReader.GetNumInstances())]);
		}
	}
}

void UNiagaraSimCache::ReadIntAttribute(TArray<int32>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetIntDef())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = AttributeReader.ReadInt(i);
		}
	}
}

void UNiagaraSimCache::ReadFloatAttribute(TArray<float>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetFloatDef())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = AttributeReader.ReadFloat(i);
		}
	}
}

void UNiagaraSimCache::ReadVector2Attribute(TArray<FVector2D>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetVec2Def())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = FVector2D(AttributeReader.ReadFloat2f(i));
		}
	}
}

void UNiagaraSimCache::ReadVectorAttribute(TArray<FVector>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetVec3Def())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i=0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = FVector(AttributeReader.ReadFloat3f(i));
		}
	}
}

void UNiagaraSimCache::ReadVector4Attribute(TArray<FVector4>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetVec4Def())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = FVector4(AttributeReader.ReadFloat4f(i));
		}
	}
}

void UNiagaraSimCache::ReadColorAttribute(TArray<FLinearColor>& OutValues, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetColorDef())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			OutValues[OutValueOffset + i] = AttributeReader.ReadColor(i);
		}
	}
}

void UNiagaraSimCache::ReadPositionAttribute(TArray<FVector>& OutValues, FName AttributeName, FName EmitterName, bool bLocalSpaceToWorld, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef())
	{
		FTransform LocalToWorld;
		if ( bLocalSpaceToWorld && AttributeReader.DataBuffersLayout->bLocalSpace )
		{
			LocalToWorld = AttributeReader.CacheFrame->LocalToWorld;
		}
		else
		{
			LocalToWorld = FTransform::Identity;
			LocalToWorld.SetTranslation(FVector(AttributeReader.CacheFrame->LWCTile) * FLargeWorldRenderScalar::GetTileSize());
		}

		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			const FVector Position = FVector(AttributeReader.ReadFloat3f(i));
			OutValues[OutValueOffset + i] = LocalToWorld.TransformPosition(Position);
		}
	}
}

void UNiagaraSimCache::ReadPositionAttributeWithRebase(TArray<FVector>& OutValues, FTransform Transform, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetPositionDef())
	{
		const FTransform LocalToWorld = AttributeReader.DataBuffersLayout->bLocalSpace ? Transform : Transform * AttributeReader.CacheFrame->LocalToWorld.Inverse();
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			const FVector Position = FVector(AttributeReader.ReadFloat3f(i));
			OutValues[OutValueOffset + i] = LocalToWorld.TransformPosition(Position);
		}
	}
}

void UNiagaraSimCache::ReadQuatAttribute(TArray<FQuat>& OutValues, FName AttributeName, FName EmitterName, bool bLocalSpaceToWorld, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetQuatDef())
	{
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		if (bLocalSpaceToWorld && AttributeReader.DataBuffersLayout->bLocalSpace)
		{
			for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
			{
				const FQuat Quat = FQuat(AttributeReader.ReadQuat4f(i));
				OutValues[OutValueOffset + i] = Quat * AttributeReader.CacheFrame->LocalToWorld.GetRotation();
			}
		}
		else
		{
			for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
			{
				const FQuat Quat = FQuat(AttributeReader.ReadQuat4f(i));
				OutValues[OutValueOffset + i] = Quat;
			}
		}
	}
}

void UNiagaraSimCache::ReadQuatAttributeWithRebase(TArray<FQuat>& OutValues, FQuat Quat, FName AttributeName, FName EmitterName, int FrameIndex) const
{
	FNiagaraSimCacheAttributeReaderHelper AttributeReader(this, EmitterName, AttributeName, FrameIndex);
	if (AttributeReader.IsValid() && AttributeReader.Variable->Variable.GetType() == FNiagaraTypeDefinition::GetQuatDef())
	{
		const bool bRebase = AttributeReader.DataBuffersLayout->bLocalSpace == false && AttributeReader.DataBuffersLayout->RebaseVariableNames.Contains(AttributeName);

		const FQuat LocalToWorld = bRebase ? Quat * AttributeReader.CacheFrame->LocalToWorld.GetRotation().Inverse() : Quat;
		const int32 OutValueOffset = OutValues.AddUninitialized(AttributeReader.GetNumInstances());
		for (int32 i = 0; i < AttributeReader.GetNumInstances(); ++i)
		{
			const FQuat Rotation = FQuat(AttributeReader.ReadQuat4f(i));
			OutValues[OutValueOffset + i] = Rotation * LocalToWorld;
		}
	}
}

