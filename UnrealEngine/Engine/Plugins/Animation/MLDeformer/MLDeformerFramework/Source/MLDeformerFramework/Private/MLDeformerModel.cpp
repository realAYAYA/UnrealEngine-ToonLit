// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerVizSettings.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "MLDeformerObjectVersion.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Animation/AnimSequence.h"
#include "RHICommandList.h"
#include "UObject/UObjectGlobals.h"
#include "RHICommandList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerModel)

namespace UE::MLDeformer
{
	void FVertexMapBuffer::InitRHI(FRHICommandListBase& RHICmdList)
	{
		if (VertexMap.Num() > 0)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("UMLDeformerModel::FVertexMapBuffer"));

			VertexBufferRHI = RHICmdList.CreateVertexBuffer(VertexMap.Num() * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo);
			uint32* Data = reinterpret_cast<uint32*>(RHICmdList.LockBuffer(VertexBufferRHI, 0, VertexMap.Num() * sizeof(uint32), RLM_WriteOnly));
			for (int32 Index = 0; Index < VertexMap.Num(); ++Index)
			{
				Data[Index] = static_cast<uint32>(VertexMap[Index]);
			}
			RHICmdList.UnlockBuffer(VertexBufferRHI);
			VertexMap.Empty();

			ShaderResourceViewRHI = RHICmdList.CreateShaderResourceView(VertexBufferRHI, 4, PF_R32_UINT);
		}
		else
		{
			VertexBufferRHI = nullptr;
			ShaderResourceViewRHI = nullptr;
		}
	}
}	// namespace UE::MLDeformer

FString UMLDeformerModel::GetDisplayName() const
{ 
	return GetClass()->GetFName().ToString();
}

UMLDeformerInputInfo* UMLDeformerModel::CreateInputInfo()
{ 
	return NewObject<UMLDeformerInputInfo>(this);
}

UMLDeformerModelInstance* UMLDeformerModel::CreateModelInstance(UMLDeformerComponent* Component)
{ 
	return NewObject<UMLDeformerModelInstance>(Component);
}

void UMLDeformerModel::Init(UMLDeformerAsset* InDeformerAsset) 
{ 
	check(InDeformerAsset);
	DeformerAsset = InDeformerAsset; 
	if (InputInfo == nullptr)
	{
		InputInfo = CreateInputInfo();
	}
}

void UMLDeformerModel::Serialize(FArchive& Archive)
{
	#if WITH_EDITOR
		if (Archive.IsSaving())
		{
			if (Archive.IsCooking())
			{
				AnimSequence = nullptr;
				VizSettings = nullptr;
			}

			InitVertexMap();
		}

		if (!Archive.IsCooking() && Archive.IsLoading())
		{
			// This also triggers the target mesh to be loaded, don't do that while cooking.
			UpdateCachedNumVertices();
		}
	#endif

	Super::Serialize(Archive);
}

UMLDeformerAsset* UMLDeformerModel::GetDeformerAsset() const
{ 
	return DeformerAsset.Get(); 
}

void UMLDeformerModel::PostLoad()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModel::PostLoad)

	Super::PostLoad();

	#if WITH_EDITOR
		InitVertexMap();
	#endif

	InitGPUData();

	UMLDeformerAsset* MLDeformerAsset = Cast<UMLDeformerAsset>(GetOuter());
	Init(MLDeformerAsset);

	if (InputInfo)
	{
		// If we are dealing with an input info that doesn't have a skeletal mesh, then use the 
		// currently set skeletal mesh. This is for backward compatibility, from before we put the skeletal mesh inside the input info.
		if (InputInfo->GetSkeletalMesh() == nullptr)
		{
			InputInfo->SetSkeletalMesh(SkeletalMesh);
		}

		InputInfo->OnPostLoad();
	}

	#if WITH_EDITOR
		UpdateMemoryUsage();
	#endif
}

// Used for the FBoenReference, so it knows what skeleton to pick bones from.
USkeleton* UMLDeformerModel::GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle)
{
	bInvalidSkeletonIsError = false;
	USkeletalMesh* SkelMesh = GetSkeletalMesh();
	return (SkelMesh != nullptr) ? SkelMesh->GetSkeleton() : nullptr;
}

void UMLDeformerModel::BeginDestroy()
{
	PostEditPropertyDelegate.RemoveAll(this);

	BeginReleaseResource(&VertexMapBuffer);
	RenderResourceDestroyFence.BeginFence();
	Super::BeginDestroy();
}

bool UMLDeformerModel::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && RenderResourceDestroyFence.IsFenceComplete();
}

void UMLDeformerModel::InitGPUData()
{
	BeginReleaseResource(&VertexMapBuffer);
	VertexMapBuffer.Init(VertexMap);
	BeginInitResource(&VertexMapBuffer);
}

void UMLDeformerModel::FloatArrayToVector3Array(const TArray<float>& FloatArray, TArray<FVector3f>& OutVectorArray)
{
	check(FloatArray.Num() % 3 == 0);
	const int32 NumVerts = FloatArray.Num() / 3;
	OutVectorArray.Reset();
	OutVectorArray.SetNumUninitialized(NumVerts);
	for (int32 VertexIndex = 0; VertexIndex < NumVerts; ++VertexIndex)
	{
		const int32 FloatBufferOffset = VertexIndex * 3;
		OutVectorArray[VertexIndex] = FVector3f(FloatArray[FloatBufferOffset + 0], FloatArray[FloatBufferOffset + 1], FloatArray[FloatBufferOffset + 2]);
	}
}

#if WITH_EDITOR
	void UMLDeformerModel::UpdateNumTargetMeshVertices()
	{ 
		NumTargetMeshVerts = 0;
	}

	void UMLDeformerModel::UpdateNumBaseMeshVertices()
	{
		USkeletalMesh* SkelMesh = GetSkeletalMesh();
		if (SkelMesh)
		{
			NumBaseMeshVerts = UMLDeformerModel::ExtractNumImportedSkinnedVertices(SkelMesh);
		}
	}

	void UMLDeformerModel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
	{
		const FProperty* Property = PropertyChangedEvent.Property;
		if (Property == nullptr)
		{
			return;
		}

		OnPostEditChangeProperty().Broadcast(PropertyChangedEvent);
	}

	void UMLDeformerModel::UpdateCachedNumVertices()
	{
		UpdateNumBaseMeshVertices();
		UpdateNumTargetMeshVertices();
	}

	int32 UMLDeformerModel::ExtractNumImportedSkinnedVertices(const USkeletalMesh* SkeletalMesh)
	{
		return SkeletalMesh ? SkeletalMesh->GetNumImportedVertices() : 0;
	}

	void UMLDeformerModel::InvalidateMemUsage()
	{
		bInvalidateMemUsage = true;
	}

	uint64 UMLDeformerModel::GetMemUsageInBytes(UE::MLDeformer::EMemUsageRequestFlags Flags) const
	{
		return (Flags == UE::MLDeformer::EMemUsageRequestFlags::Cooked) ? CookedMemUsageInBytes : MemUsageInBytes;
	}

	uint64 UMLDeformerModel::GetGPUMemUsageInBytes() const
	{
		return GPUMemUsageInBytes;
	}

	bool UMLDeformerModel::IsMemUsageInvalidated() const
	{
		return bInvalidateMemUsage;
	}

	void UMLDeformerModel::UpdateMemoryUsage()
	{
		bInvalidateMemUsage = false;
		MemUsageInBytes = 0;
		GPUMemUsageInBytes = 0;

		// Get the size of the model to get an approximate size.
		UMLDeformerModel* Model = const_cast<UMLDeformerModel*>(this);
		MemUsageInBytes += static_cast<uint64>(Model->GetResourceSizeBytes(EResourceSizeMode::Type::EstimatedTotal));

		// Init the cooked size. We can subtract bytes from this to simulate a cook.
		CookedMemUsageInBytes = MemUsageInBytes;

		// Add GPU memory.
		if (VertexMapBuffer.VertexBufferRHI.IsValid())
		{
			GPUMemUsageInBytes += VertexMapBuffer.VertexBufferRHI->GetSize();
		}
	}
#endif	// #if WITH_EDITOR

#if WITH_EDITORONLY_DATA
	void UMLDeformerModel::InitVertexMap()
	{
		VertexMap.Empty();
		const USkeletalMesh* SkelMesh = GetSkeletalMesh();
		if (SkelMesh)
		{
			const FSkeletalMeshModel* SkeletalMeshModel = SkelMesh->GetImportedModel();
			if (SkeletalMeshModel && SkeletalMeshModel->LODModels.IsValidIndex(0))
			{
				VertexMap = SkeletalMeshModel->LODModels[0].MeshToImportVertexMap;
			}
		}
	}
#endif	// WITH_EDITORDATA_ONLY
