// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceHalfEdge.h"

#include "Components/SkinnedMeshComponent.h"
#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDataDomain.h"
#include "RenderGraphBuilder.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceHalfEdge)

struct FEdgeKey
{
	FVector3f V0;
	FVector3f V1;

	bool operator==(FEdgeKey const& Rhs) const { return V0 == Rhs.V0 && V1 == Rhs.V1; }
};

static uint32 GetTypeHash(FEdgeKey const& Key)
{
	return HashCombine(GetTypeHash(Key.V0), GetTypeHash(Key.V1));
}

namespace
{
	/** Builds lookup from edge to twin edge. */
	class FTwinEdgeBuilder
	{
	public:
		FTwinEdgeBuilder(int32 InNumTriangles)
		{
			TwinEdges.SetNum(InNumTriangles * 3);
			for (int32& TwinEdge : TwinEdges)
			{
				TwinEdge = -1;
			}
		}

		int32 SmallestVector(FVector3f V0, FVector3f V1)
		{
			if (V0.X != V1.X) return V1.X > V0.X;
			if (V0.Y != V1.Y) return V1.Y > V0.Y;
			return V1.Z > V0.Z;
		}

		void Add(int32 InTriangleIndex, TStaticArray<FVector3f, 3>& InVertexPositions)
		{
			for (uint32 TriangleEdgeIndex = 0; TriangleEdgeIndex < 3; ++TriangleEdgeIndex)
			{
				FVector3f V[2] = { 
					InVertexPositions[TriangleEdgeIndex],
					InVertexPositions[(TriangleEdgeIndex + 1) % 3] };
				int32 VMin = SmallestVector(V[0], V[1]);
				const FEdgeKey Key({V[VMin], V[1 - VMin]});

				if (FEdgeDescription* EdgeDesc = EdgeMap.Find(Key))
				{
					if (EdgeDesc->E1 == -1)
					{
						// The second time that we've seen this edge.
						EdgeDesc->E1 = InTriangleIndex * 3 + TriangleEdgeIndex;
						TwinEdges[EdgeDesc->E0] = EdgeDesc->E1;
						TwinEdges[EdgeDesc->E1] = EdgeDesc->E0;
					}
					else
					{
						// Non-manifold geometry can end up here. Two options: 
						// (i) Don't store twins for this edge the first pair.
						// (ii) Reset the edge map entry and hope that good twins are met consecutively when walking the index buffer.
 						EdgeDesc->E0 = InTriangleIndex * 3 + TriangleEdgeIndex;
 						EdgeDesc->E1 = -1;
					}
				}
				else
				{
					// First time that we've seen this edge.
					EdgeMap.Add(Key).E0 = InTriangleIndex * 3 + TriangleEdgeIndex;
				}
			}
		}

		TArray<int32>& GetTwinEdges()
		{
			return TwinEdges;
		}

	private:
		struct FEdgeDescription
		{
			int32 E0 = -1;
			int32 E1 = -1;
		};

		TMap<FEdgeKey, FEdgeDescription> EdgeMap;

		TArray<int32> TwinEdges;
	};

	/** Builds lookup from (one) vertex to (many) edges. */
	class FVertexToEdgeBuilder
	{
	public:
		FVertexToEdgeBuilder(int32 InNumVertices, int32 InNumTriangles)
		{
			VertexToEdgeItems.Reserve(InNumTriangles * 3);
			VertexToEdgeItems.AddDefaulted(InNumVertices);
		}

		void Add(int32 InVertexIndex, int32 InEdgeIndex)
		{
			int32 ItemIndex = InVertexIndex;
			if (VertexToEdgeItems[ItemIndex].EdgeIndex == -1)
			{
				VertexToEdgeItems[ItemIndex].EdgeIndex = InEdgeIndex;
				return;
			}
			while (VertexToEdgeItems[ItemIndex].NextItem != -1)
			{
				ItemIndex = VertexToEdgeItems[ItemIndex].NextItem;
			}
			int32 NewItemIndex = VertexToEdgeItems.AddDefaulted();
			VertexToEdgeItems[ItemIndex].NextItem = NewItemIndex;
			VertexToEdgeItems[NewItemIndex].EdgeIndex = InEdgeIndex;
		}

		using FEdgeInlineArray = TArray<int32, TInlineAllocator<16>>;
		void Get(int32 InVertexIndex, FEdgeInlineArray& OutEdgeIndices) const
		{
			int32 ItemIndex = InVertexIndex;
			while (ItemIndex != -1)
			{
				OutEdgeIndices.Add(VertexToEdgeItems[ItemIndex].EdgeIndex);
				ItemIndex = VertexToEdgeItems[ItemIndex].NextItem;
			}
		}

	private:
		struct FVertexToEdgeItem
		{
			int32 EdgeIndex = -1;
			int32 NextItem = -1;
		};

		/** First NumVertices entries are allocated up front as start of per vertex linked lists. */
		TArray<FVertexToEdgeItem> VertexToEdgeItems;
	};

	static int32 GetHeadEdgeForVertex(int32 InVertexIndex, FVertexToEdgeBuilder const& InVertexToEdge, TArray<int32> const& InEdgeToTwinEdge)
	{
		FVertexToEdgeBuilder::FEdgeInlineArray EdgeIndicesScratchBuffer;
		InVertexToEdge.Get(InVertexIndex, EdgeIndicesScratchBuffer);

		// Iterate backwards around the edges until we find a border, or a non recognized edge (which may lead to a border).
		const int32 StartEdgeIndex = EdgeIndicesScratchBuffer[0];
		int32 EdgeIndex = StartEdgeIndex;
		while(1)
		{
			const int32 LastEdgeIndex = EdgeIndex;
			EdgeIndex = ((EdgeIndex / 3) * 3) + ((EdgeIndex + 2) % 3);
			EdgeIndex = InEdgeToTwinEdge[EdgeIndex];
			
			if (EdgeIndex == StartEdgeIndex)
			{
				break;
			}
			if (EdgeIndex == -1 || EdgeIndicesScratchBuffer.Find(EdgeIndex) == INDEX_NONE)
			{
				EdgeIndex = LastEdgeIndex;
				break;
			}
		}

		return EdgeIndex;
	}

	static void BuildHalfEdgeBuffers(const FSkeletalMeshLODRenderData& InLodRenderData, TArray<int32>& OutVertexToEdge, TArray<int32>& OutEdgeToTwinEdge)
	{
		const FRawStaticIndexBuffer16or32Interface* IndexBuffer = InLodRenderData.MultiSizeIndexContainer.GetIndexBuffer();
		const int32 IndexCount = IndexBuffer->Num();
		const int32 TriangleCount = IndexCount / 3;

		const FPositionVertexBuffer& VertexBuffer = InLodRenderData.StaticVertexBuffers.PositionVertexBuffer;
		const int32 VertexCount = InLodRenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();

		// Build edge to twin edge map.
		{
			FTwinEdgeBuilder TwinEdgeBuilder(TriangleCount);
			for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
			{
				TStaticArray<FVector3f, 3> VertexPositions;
				VertexPositions[0] = VertexBuffer.VertexPosition(IndexBuffer->Get(TriangleIndex * 3 + 0));
				VertexPositions[1] = VertexBuffer.VertexPosition(IndexBuffer->Get(TriangleIndex * 3 + 1));
				VertexPositions[2] = VertexBuffer.VertexPosition(IndexBuffer->Get(TriangleIndex * 3 + 2));
				TwinEdgeBuilder.Add(TriangleIndex, VertexPositions);
			}
			OutEdgeToTwinEdge = MoveTemp(TwinEdgeBuilder.GetTwinEdges());
		}

		// Build vertex to edge index map.
		FVertexToEdgeBuilder VertexToEdgeBuilder(VertexCount, TriangleCount);
		for (int32 Index = 0; Index < TriangleCount * 3; ++Index)
		{
			const int32 VertexIndex = IndexBuffer->Get(Index);
			const int32 EdgeIndex = Index;
			VertexToEdgeBuilder.Add(VertexIndex, EdgeIndex);
		}

		// VertexToEdgeBuilder gives us multiple edges per vertex.
		// These should form a linked list of edges. 
		// Usually that list will be a loop, but at border geometry there will be a head and end to the list.
		// For loops it doesn't matter where we start iteration. But at borders we want to use the head of the linked list to start iteration.
		// Non-manifold geometry will be strange, and potentially have multiple edge independent loops. We just do the best we can in that case.
		OutVertexToEdge.SetNumUninitialized(VertexCount);
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; ++VertexIndex)
		{
			OutVertexToEdge[VertexIndex] = GetHeadEdgeForVertex(VertexIndex, VertexToEdgeBuilder, OutEdgeToTwinEdge);
		}
	}
}

/*
 * Render resource containing the half edge buffers. 
 * todo[CF]: We need to move this to the skeletal mesh cooked mesh data to avoid the perf and memory hit everytime we instantiate a deformer.
 */ 
class FHalfEdgeBufferResources : public FRenderResource
{
public:
	FHalfEdgeBufferResources(TArray< TArray<int32> >&& InVertexToEdgeData, TArray< TArray<int32> >&& InEdgeToTwinEdgeData)
	{
		VertexToEdgeData = MoveTemp(InVertexToEdgeData);
		EdgeToTwinEdgeData = MoveTemp(InEdgeToTwinEdgeData);
	}

	void InitRHI(FRHICommandListBase&) override
	{
		FRDGBuilder GraphBuilder(FRHICommandListImmediate::Get());

		const int32 NumLods = VertexToEdgeData.Num();
		VertexToEdgeBuffers.SetNum(NumLods);
		EdgeToTwinEdgeBuffers.SetNum(NumLods);
		
		for (int32 LodIndex = 0; LodIndex < NumLods; ++LodIndex)
		{
			FRDGBuffer* VertexToEdgeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), VertexToEdgeData[LodIndex].Num()), TEXT("Optimus.VertexToEdge"));
			GraphBuilder.QueueBufferUpload(VertexToEdgeBuffer, VertexToEdgeData[LodIndex].GetData(), VertexToEdgeData[LodIndex].Num() * sizeof(int32));
			VertexToEdgeBuffers[LodIndex] = GraphBuilder.ConvertToExternalBuffer(VertexToEdgeBuffer);
			
			FRDGBuffer* EdgeToTwinEdgeBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), EdgeToTwinEdgeData[LodIndex].Num()), TEXT("Optimus.EdgeToTwinEdge"));
			GraphBuilder.QueueBufferUpload(EdgeToTwinEdgeBuffer, EdgeToTwinEdgeData[LodIndex].GetData(), EdgeToTwinEdgeData[LodIndex].Num() * sizeof(int32));
			EdgeToTwinEdgeBuffers[LodIndex] = GraphBuilder.ConvertToExternalBuffer(EdgeToTwinEdgeBuffer);
		}

		GraphBuilder.Execute();

		VertexToEdgeData.Reset();
		EdgeToTwinEdgeData.Reset();
	}

	void ReleaseRHI() override
	{
		VertexToEdgeBuffers.Reset();
		EdgeToTwinEdgeBuffers.Reset();
	}

	TRefCountPtr<FRDGPooledBuffer> GetVertexToEdgeBuffer(int32 InLodIndex)
	{
		return VertexToEdgeBuffers[InLodIndex];
	}

	TRefCountPtr<FRDGPooledBuffer> GetEdgeToTwinBuffer(int32 InLodIndex)
	{
		return EdgeToTwinEdgeBuffers[InLodIndex];
	}

private:
	TArray< TArray<int32> > VertexToEdgeData;
	TArray< TArray<int32> > EdgeToTwinEdgeData;
	TArray< TRefCountPtr<FRDGPooledBuffer> > VertexToEdgeBuffers;
	TArray< TRefCountPtr<FRDGPooledBuffer> > EdgeToTwinEdgeBuffers;
};


FString UOptimusHalfEdgeDataInterface::GetDisplayName() const
{
	return TEXT("HalfEdge");
}

TArray<FOptimusCDIPinDefinition> UOptimusHalfEdgeDataInterface::GetPinDefinitions() const
{
	FName Vertex(UOptimusSkinnedMeshComponentSource::Domains::Vertex);
	FName Triangle(UOptimusSkinnedMeshComponentSource::Domains::Triangle);

	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "EdgesPerVertex", "ReadEdge", Vertex, "ReadNumVertices" });
	Defs.Add({ "TwinEdges", "ReadTwinEdge", Triangle, 3, "ReadNumTriangles" });
	return Defs;
}

TSubclassOf<UActorComponent> UOptimusHalfEdgeDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}

void UOptimusHalfEdgeDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumTriangles"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadEdge"))
		.AddReturnType(EShaderFundamentalType::Int)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadTwinEdge"))
		.AddReturnType(EShaderFundamentalType::Int)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FHalfEdgeDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, NumTriangles)
	SHADER_PARAMETER(uint32, IndexBufferStart)
	SHADER_PARAMETER(uint32, InputStreamStart)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, VertexToEdgeBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, EdgeToTwinEdgeBuffer)
END_SHADER_PARAMETER_STRUCT()

void UOptimusHalfEdgeDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FHalfEdgeDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusHalfEdgeDataInterface::TemplateFilePath = TEXT("/Plugin/Optimus/Private/DataInterfaceHalfEdge.ush");

TCHAR const* UOptimusHalfEdgeDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusHalfEdgeDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusHalfEdgeDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusHalfEdgeDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusHalfEdgeDataProvider* Provider = NewObject<UOptimusHalfEdgeDataProvider>();

	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);

	if (Provider->SkinnedMesh != nullptr)
	{
		FSkeletalMeshRenderData const* SkeletalMeshRenderData = Provider->SkinnedMesh->GetSkeletalMeshRenderData();
		if (SkeletalMeshRenderData != nullptr)
		{
			// Build half edge data.
			TArray< TArray<int32> > VertexToEdgePerLod;
			VertexToEdgePerLod.SetNum(SkeletalMeshRenderData->NumInlinedLODs);
			TArray< TArray<int32> > EdgeToTwinEdgePerLod;
			EdgeToTwinEdgePerLod.SetNum(SkeletalMeshRenderData->NumInlinedLODs);

			for (int32 LodIndex = 0; LodIndex < SkeletalMeshRenderData->NumInlinedLODs; ++LodIndex)
			{
				FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData->LODRenderData[LodIndex];
				BuildHalfEdgeBuffers(*LodRenderData, VertexToEdgePerLod[LodIndex], EdgeToTwinEdgePerLod[LodIndex]);
			}

			// Upload data to a resource stored on the provider.
			Provider->HalfEdgeBuffers = MakeShared<FHalfEdgeBufferResources>(MoveTemp(VertexToEdgePerLod), MoveTemp(EdgeToTwinEdgePerLod));

			BeginInitResource(Provider->HalfEdgeBuffers.Get());
		}
	}

	return Provider;
}


void UOptimusHalfEdgeDataProvider::BeginDestroy()
{
	Super::BeginDestroy();
	if (HalfEdgeBuffers)
	{
		BeginReleaseResource(HalfEdgeBuffers.Get());
	}
	DestroyFence.BeginFence();
}

bool UOptimusHalfEdgeDataProvider::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && DestroyFence.IsFenceComplete();
}

FComputeDataProviderRenderProxy* UOptimusHalfEdgeDataProvider::GetRenderProxy()
{
	return new FOptimusHalfEdgeDataProviderProxy(SkinnedMesh, HalfEdgeBuffers);
}


FOptimusHalfEdgeDataProviderProxy::FOptimusHalfEdgeDataProviderProxy(
	USkinnedMeshComponent* InSkinnedMeshComponent, 
	TSharedPtr<FHalfEdgeBufferResources>& InHalfEdgeBuffers)
	: SkeletalMeshObject(InSkinnedMeshComponent != nullptr ? InSkinnedMeshComponent->MeshObject : nullptr)
	, HalfEdgeBuffers(InHalfEdgeBuffers)
{
}

bool FOptimusHalfEdgeDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (SkeletalMeshObject == nullptr || !HalfEdgeBuffers.IsValid())
	{
		return false;
	}
	if (SkeletalMeshObject->GetSkeletalMeshRenderData().LODRenderData[SkeletalMeshObject->GetLOD()].RenderSections.Num() != InValidationData.NumInvocations)
	{
		return false;
	}

	return true;
}

void FOptimusHalfEdgeDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();

	FRDGBuffer* VertexToEdgeBuffer = GraphBuilder.RegisterExternalBuffer(HalfEdgeBuffers->GetVertexToEdgeBuffer(LodIndex));
	VertexToEdgeBufferSRV = GraphBuilder.CreateSRV(VertexToEdgeBuffer);
	
	FRDGBuffer* EdgeToTwinEdgeBuffer = GraphBuilder.RegisterExternalBuffer(HalfEdgeBuffers->GetEdgeToTwinBuffer(LodIndex));
	EdgeToTwinEdgeBufferSRV = GraphBuilder.CreateSRV(EdgeToTwinEdgeBuffer);
}

void FOptimusHalfEdgeDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	FRHIShaderResourceView* NullSRVBinding = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI.GetReference();

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumVertices = InDispatchData.bUnifiedDispatch ? LodRenderData->GetNumVertices() : RenderSection.NumVertices;
		Parameters.NumTriangles = InDispatchData.bUnifiedDispatch ? LodRenderData->GetTotalFaces() : RenderSection.NumTriangles;
		Parameters.IndexBufferStart = InDispatchData.bUnifiedDispatch ? 0 :RenderSection.BaseIndex;
		Parameters.InputStreamStart = InDispatchData.bUnifiedDispatch ? 0 : RenderSection.BaseVertexIndex;
		Parameters.VertexToEdgeBuffer = VertexToEdgeBufferSRV;
		Parameters.EdgeToTwinEdgeBuffer = EdgeToTwinEdgeBufferSRV;
	}
}
