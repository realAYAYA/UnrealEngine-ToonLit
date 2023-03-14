// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerModel.h"
#include "MLDeformerInputInfo.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerVizSettings.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "NeuralNetwork.h"
#include "Animation/AnimSequence.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MLDeformer
{
	void FVertexMapBuffer::InitRHI()
	{
		if (VertexMap.Num() > 0)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("UMLDeformerModel::FVertexMapBuffer"));

			VertexBufferRHI = RHICreateVertexBuffer(VertexMap.Num() * sizeof(uint32), BUF_Static | BUF_ShaderResource, CreateInfo);
			uint32* Data = reinterpret_cast<uint32*>(RHILockBuffer(VertexBufferRHI, 0, VertexMap.Num() * sizeof(uint32), RLM_WriteOnly));
			for (int32 Index = 0; Index < VertexMap.Num(); ++Index)
			{
				Data[Index] = static_cast<uint32>(VertexMap[Index]);
			}
			RHIUnlockBuffer(VertexBufferRHI);
			VertexMap.Empty();

			ShaderResourceViewRHI = RHICreateShaderResourceView(VertexBufferRHI, 4, PF_R32_UINT);
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
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerModel::Serialize)
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

		if (!Archive.IsCooking())
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
		InputInfo->OnPostLoad();
	}

	UNeuralNetwork* Network = GetNeuralNetwork();
	if (Network)
	{
		// If we run the neural network on the GPU.
		if (IsNeuralNetworkOnGPU())
		{
			Network->SetDeviceType(ENeuralDeviceType::GPU, ENeuralDeviceType::CPU, ENeuralDeviceType::GPU);
			if (Network->GetDeviceType() != ENeuralDeviceType::GPU || Network->GetOutputDeviceType() != ENeuralDeviceType::GPU || Network->GetInputDeviceType() != ENeuralDeviceType::CPU)
			{
				UE_LOG(LogMLDeformer, Error, TEXT("Neural net in ML Deformer '%s' cannot run on the GPU, it will not be active."), *GetDeformerAsset()->GetName());
			}
		}
		else // We run our neural network on the CPU.
		{
			Network->SetDeviceType(ENeuralDeviceType::CPU, ENeuralDeviceType::CPU, ENeuralDeviceType::CPU);
		}
	}
}

void UMLDeformerModel::SetNeuralNetwork(UNeuralNetwork* InNeuralNetwork)
{
	NeuralNetworkModifyDelegate.Broadcast();
	NeuralNetwork = InNeuralNetwork;
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
		NumBaseMeshVerts = UMLDeformerModel::ExtractNumImportedSkinnedVertices(GetSkeletalMesh());
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
