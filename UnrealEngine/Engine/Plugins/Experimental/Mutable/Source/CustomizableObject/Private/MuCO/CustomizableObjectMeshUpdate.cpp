// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
CustomizableObjectMeshUpdate.cpp: Helpers to stream in CustomizableObject skeletal mesh LODs.
=============================================================================*/


#include "MuCO/CustomizableObjectMeshUpdate.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSkeletalMesh.h"
#include "MuCO/UnrealConversionUtils.h"

#include "MuR/Model.h"
#include "MuR/MeshBufferSet.h"

#include "Engine/SkeletalMesh.h"
#include "Streaming/RenderAssetUpdate.inl"
#include "Rendering/SkeletalMeshRenderData.h"


#include "BusyWaits_Deprecated.h"

template class TRenderAssetUpdate<FSkelMeshUpdateContext>;

#define UE_MUTABLE_UPDATE_MESH_REGION		TEXT("Task_Mutable_UpdateMesh")

FCustomizableObjectMeshStreamIn::FCustomizableObjectMeshStreamIn(const USkeletalMesh* InMesh, bool bInHighPrio, bool bInRenderThread)
	: FSkeletalMeshStreamIn(InMesh),
	bHighPriority(bInHighPrio),
	bRenderThread(bInRenderThread)
{
	OperationData = MakeShared<FMutableMeshOperationData>();

	// This must run in the mutable thread.
	check(UCustomizableObjectSystem::IsCreated());
	OperationData->System = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem;

	const UCustomizableObjectSkeletalMesh* TypedMesh = CastChecked<UCustomizableObjectSkeletalMesh>(InMesh);
	OperationData->Model = TypedMesh->Model;
	OperationData->Parameters = TypedMesh->Parameters;
	OperationData->State = TypedMesh->State;

	OperationData->MeshIDs = TypedMesh->MeshIDs;
	OperationData->Meshes.SetNum(TypedMesh->MeshIDs.Num());

	OperationData->CurrentFirstLODIdx = CurrentFirstLODIdx;
	OperationData->PendingFirstLODIdx = PendingFirstLODIdx;

	PushTask(FContext(InMesh, TT_None), TT_Async, SRA_UPDATE_CALLBACK(DoInitiate), TT_None, nullptr);
}

void FCustomizableObjectMeshStreamIn::OnUpdateMeshFinished()
{
	check(TaskSynchronization.GetValue() > 0)

	// At this point task synchronization would hold the number of pending requests.
	TaskSynchronization.Decrement();

	// The tick here is intended to schedule the success or cancel callback.
	// Using TT_None ensure gets which could create a dead lock.
	Tick(FSkeletalMeshUpdate::TT_None);
}

void FCustomizableObjectMeshStreamIn::DoInitiate(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);

	if (IsCancelled())
	{
		return;
	}

	// Launch MutableTask
	RequestMeshUpdate(Context);

	PushTask(Context, TT_Async, SRA_UPDATE_CALLBACK(DoConvertResources), TT_Async, SRA_UPDATE_CALLBACK(DoCancelMeshUpdate));
}

void FCustomizableObjectMeshStreamIn::DoConvertResources(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);

	ConvertMesh(Context);

	const EThreadType TThread = bRenderThread ? TT_Render : TT_Async;
	const EThreadType CThread = (EThreadType)Context.CurrentThread;
	PushTask(Context, TThread, SRA_UPDATE_CALLBACK(DoCreateBuffers), CThread, SRA_UPDATE_CALLBACK(DoCancel));
}

void FCustomizableObjectMeshStreamIn::DoCreateBuffers(const FContext& Context)
{
	if (bRenderThread)
	{
		CreateBuffers_RenderThread(Context);
	}
	else
	{
		CreateBuffers_Async(Context);
	}
	check(!TaskSynchronization.GetValue());

	// We cannot cancel once DoCreateBuffers has started executing, as there's an RHICmdList that must be submitted.
	// Pass the same callback for both task and cancel.
	PushTask(Context, 
		TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate),
		TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate));
}

void FCustomizableObjectMeshStreamIn::DoCancelMeshUpdate(const FContext& Context)
{
	CancelMeshUpdate(Context);
	PushTask(Context, TT_None, nullptr, (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(DoCancel));
}


namespace impl
{
	void Task_Mutable_UpdateMesh_End(const TSharedPtr<FMutableMeshOperationData> OperationData, TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task, mu::Instance::ID InstanceID)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh_End);

		// End update
		OperationData->System->EndUpdate(InstanceID);
		OperationData->System->ReleaseInstance(InstanceID);

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			OperationData->System->ClearWorkingMemory();
		}

		Task->OnUpdateMeshFinished();
		
		TRACE_END_REGION(UE_MUTABLE_UPDATE_MESH_REGION);
	}

	void Task_Mutable_UpdateMesh_Loop(
		const TSharedPtr<FMutableMeshOperationData> OperationData,
		TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task,
		mu::Instance::ID InstanceID,
		int32 LODIndex)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh_Loop);

		if (LODIndex == OperationData->CurrentFirstLODIdx)
		{
			Task_Mutable_UpdateMesh_End(OperationData, Task, InstanceID);
			return;
		}

		const mu::FResourceID MeshID = OperationData->MeshIDs[LODIndex];
		UE::Tasks::TTask<mu::Ptr<const mu::Mesh>> GetMeshTask = OperationData->System->GetMesh(InstanceID, MeshID);

		UE::Tasks::AddNested(UE::Tasks::Launch(TEXT("Task_MutableGetMeshes_GetMesh_Post"), [=]() mutable
			{
				OperationData->Meshes[LODIndex] = GetMeshTask.GetResult();

				Task_Mutable_UpdateMesh_Loop(OperationData, Task, InstanceID, LODIndex + 1);
			},
			GetMeshTask));
	}

	void Task_Mutable_UpdateMesh(const TSharedPtr<FMutableMeshOperationData> OperationData, TRefCountPtr<FCustomizableObjectMeshStreamIn>& Task)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_UpdateMesh);
		TRACE_BEGIN_REGION(UE_MUTABLE_UPDATE_MESH_REGION);

		mu::SystemPtr System = OperationData->System;
		const TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = OperationData->Model;

#if WITH_EDITOR
		// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
		if (!Model || !Model->IsValid())
		{
			TRACE_END_REGION(UE_MUTABLE_UPDATE_MESH_REGION);
			return;
		}
#endif

		// For now, we are forcing the recreation of mutable-side instances with every update.
		mu::Instance::ID InstanceID = System->NewInstance(Model);
		UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for a mesh update"), InstanceID);

		// LOD mask, set to all ones to build all LODs
		const uint32 LODMask = 0xFFFFFFFF;

		// Main instance generation step
		const mu::Instance* Instance = System->BeginUpdate(InstanceID, OperationData->Parameters, OperationData->State, LODMask);
		check(Instance);

		Task_Mutable_UpdateMesh_Loop(OperationData, Task, InstanceID, OperationData->PendingFirstLODIdx);
	}
	
} // namespace


void FCustomizableObjectMeshStreamIn::RequestMeshUpdate(const FContext& Context)
{
	if (!UCustomizableObjectSystem::IsActive())
	{
		Abort();
		return;
	}

	check(OperationData.IsValid());

#if WITH_EDITOR
	// Recompiling a CO in the editor will invalidate the previously generated Model. Check that it is valid before accessing the streamed data.
	if (!OperationData->Model || !OperationData->Model->IsValid())
	{
		Abort();
		return;
	}
#endif

	UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
	
	TaskSynchronization.Increment();

	TRefCountPtr<FCustomizableObjectMeshStreamIn> RefThis = this;
	TSharedPtr<FMutableMeshOperationData> SharedOperationData = OperationData;


	MutableTaskId = CustomizableObjectSystem->MutableTaskGraph.AddMutableThreadTaskLowPriority(
		TEXT("Mutable_MeshUpdate"),
		[SharedOperationData, RefThis]() mutable
		{
			if (CVarEnableNewSplitMutableTask.GetValueOnAnyThread())
			{
				impl::Task_Mutable_UpdateMesh(SharedOperationData, RefThis);
			}
			else
			{
				using namespace CustomizableObjectMeshUpdate;
				ImplDeprecated::Task_Mutable_UpdateMesh(SharedOperationData);
				
				RefThis->OnUpdateMeshFinished();
			}
		});
}


void FCustomizableObjectMeshStreamIn::CancelMeshUpdate(const FContext& Context)
{
	UCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
	if (CustomizableObjectSystem && MutableTaskId != FMutableTaskGraph::INVALID_ID)
	{
		// Cancel task if not launched yet.
		const bool bMutableTaskCancelledBeforeRun = CustomizableObjectSystem->MutableTaskGraph.CancelMutableThreadTaskLowPriority(MutableTaskId);
		if (bMutableTaskCancelledBeforeRun)
		{
			// Clear MeshUpdate data
			OperationData = nullptr;

			// At this point task synchronization would hold the number of pending requests.
			TaskSynchronization.Decrement();
			check(TaskSynchronization.GetValue() == 0);
		}
	}
	else
	{
		check(TaskSynchronization.GetValue() == 0);
	}

	// The tick here is intended to schedule the success or cancel callback.
	// Using TT_None ensure gets which could create a dead lock.
	Tick(FSkeletalMeshUpdate::TT_None);
}


void FCustomizableObjectMeshStreamIn::ConvertMesh(const FContext& Context)
{
	MUTABLE_CPUPROFILER_SCOPE(ConvertMesh);

	check(!TaskSynchronization.GetValue());

	const USkeletalMesh* Mesh = Context.Mesh;
	FSkeletalMeshRenderData* RenderData = Context.RenderData;
	if (IsCancelled() || !Mesh || !RenderData)
	{
		return;
	}

	for (int32 LODIndex = PendingFirstLODIdx; LODIndex < CurrentFirstLODIdx; ++LODIndex)
	{
		mu::MeshPtrConst MutableMesh = OperationData->Meshes[LODIndex];

		if (!MutableMesh)
		{
			check(false);
			Abort();
			return;
		}

		FSkeletalMeshLODRenderData& LODResource = *Context.LODResourcesView[LODIndex];
		UnrealConversionUtils::CopyMutableVertexBuffers(LODResource, MutableMesh, true);
		UnrealConversionUtils::CopyMutableIndexBuffers(LODResource, MutableMesh);

		// TODO PERE: Implement missing buffers
		// SkinWeightProfilesData
		// MorphTargets (MorphTargetVertexInfoBuffers)
		// Clothing (ClothVertexBuffer)

		UnrealConversionUtils::UpdateSkeletalMeshLODRenderDataBuffersSize(LODResource);
	}

	// Clear MeshUpdate data
	OperationData = nullptr;
}