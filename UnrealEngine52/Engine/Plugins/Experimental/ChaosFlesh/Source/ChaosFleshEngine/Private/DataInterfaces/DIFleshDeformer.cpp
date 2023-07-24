// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaces/DIFleshDeformer.h"

#include "Components/SkeletalMeshComponent.h"
#include "Containers/Set.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "GenericPlatform/GenericPlatformMath.h"
#include "GeometryCollection/Facades/CollectionTetrahedralBindingsFacade.h"
#include "OptimusDataDomain.h"
#include "ShaderParameterMetadataBuilder.h"
#include "RenderGraphBuilder.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "SkeletalRenderPublic.h"

DEFINE_LOG_CATEGORY(LogFleshDeformer);

//
// Interface
//

FString
UDIFleshDeformer::GetDisplayName() const
{
	return TEXT("Flesh Deformer");
}

TArray<FOptimusCDIPinDefinition>
UDIFleshDeformer::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "Mask", "ReadMask", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "Parents", "ReadParents", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "Weights", "ReadWeights", Optimus::DomainName::Vertex, "ReadNumVertices"  });
	Defs.Add({ "Offset", "ReadOffset", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "EmbeddedPos", "GetEmbeddedPos", Optimus::DomainName::Vertex, "ReadNumVertices" });
	return Defs;
}

TSubclassOf<UActorComponent> 
UDIFleshDeformer::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}

void
UDIFleshDeformer::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadMask"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadParents"))
		.AddReturnType(EShaderFundamentalType::Int, 4) // int4
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadWeights"))
		.AddReturnType(EShaderFundamentalType::Float, 4) // float4
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadOffset"))
		.AddReturnType(EShaderFundamentalType::Float, 3) // float4
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("GetEmbeddedPos"))
		.AddReturnType(EShaderFundamentalType::Float, 3) // float3
		.AddParam(EShaderFundamentalType::Uint);

}

BEGIN_SHADER_PARAMETER_STRUCT(FFleshDeformerDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, BaseVertexIndex)
	SHADER_PARAMETER(uint32, NumTetRestVertexBuffer)
	SHADER_PARAMETER(uint32, NumTetVertexBuffer)
	SHADER_PARAMETER(uint32, NumParentsBuffer)
	SHADER_PARAMETER(uint32, NumWeightsBuffer)
	SHADER_PARAMETER(uint32, NumOffsetBuffer)
	SHADER_PARAMETER(uint32, NumMaskBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, TetRestVertexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, TetVertexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, ParentsBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, WeightsBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, OffsetBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, MaskBuffer)
END_SHADER_PARAMETER_STRUCT()

void
UDIFleshDeformer::GetShaderParameters(
	TCHAR const* UID,
	FShaderParametersMetadataBuilder& InOutBuilder,
	FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FFleshDeformerDataInterfaceParameters>(UID);
}

void
UDIFleshDeformer::GetPermutations(FComputeKernelPermutationVector& OutPermutationVector) const
{
	OutPermutationVector.AddPermutation(TEXT("ENABLE_DEFORMER_FLESH"), 2);
}

void
UDIFleshDeformer::GetShaderHash(FString& InOutKey) const
{
	// Module registers "ChaosFlesh/Source/ChaosFleshEngine/Shaders/Private" as virtual shader 
	// path: "/Plugin/ChaosFleshEngine".
	GetShaderFileHash(
		TEXT("/Plugin/ChaosFleshEngine/DIFleshDeformer.ush"),
		EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void
UDIFleshDeformer::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(
		TEXT("/Plugin/ChaosFleshEngine/DIFleshDeformer.ush"),
		EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider*
UDIFleshDeformer::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UDIFleshDeformerDataProvider* Provider = NewObject<UDIFleshDeformerDataProvider>();

	if ((Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding)))
	{
		AActor* Actor = Provider->SkinnedMesh->GetOwner();
		const TSet<UActorComponent*>& Components = Actor->GetComponents();

		// Count the number of flesh components we have.
		int32 NumFleshComponents = 0;
		for (TSet<UActorComponent*>::TConstIterator It = Components.CreateConstIterator(); It; ++It)
		{
			NumFleshComponents += const_cast<UFleshComponent*>(Cast<UFleshComponent>(*It)) != nullptr;
		}

		// Find the flesh component that is associated with this skeletal mesh.
		for (TSet<UActorComponent*>::TConstIterator It = Components.CreateConstIterator(); It; ++It)
		{
			if (const UActorComponent* Component = *It)
			{
				if (UFleshComponent* FleshComponent = const_cast<UFleshComponent*>(Cast<UFleshComponent>(Component)))
				{
					if (const UFleshAsset* FleshAsset = FleshComponent->GetRestCollection())
					{
						//if ((USkinnedMeshComponent*)FleshComponent->TargetSkeletalMesh == Provider->SkinnedMesh)
						{
							if (FleshAsset->TargetSkeletalMesh.Equals(Provider->SkinnedMesh->GetName()))
							{
								Provider->FleshMesh = FleshComponent;
								break;
							}
							else if (FleshAsset->TargetSkeletalMesh.IsEmpty() && NumFleshComponents == 1)
							{
								// The typical case is probably that we only have 1 skeletal mesh and 1 flesh
								// component.  So, in an effort to keep the easy case easy, if the TargetSkeletalMesh
								// isn't set, just do the obvious thing and use it.
								UE_LOG(LogFleshDeformer, Display,
									TEXT("FleshAsset '%s' has no value for 'TargetSkeletalMesh', "
										"using it for skeletal mesh '%s' anyway as it's the only one available."),
									*FleshAsset->GetName(),
									*Provider->SkinnedMesh->GetName());
								Provider->FleshMesh = FleshComponent;
								break;
							}
							else if (FleshAsset->TargetSkeletalMesh.IsEmpty())
							{
								UE_LOG(LogFleshDeformer, Warning,
									TEXT("FleshAsset '%s' has no value for 'TargetSkeletalMesh', "
										"not using it for skeletal mesh '%s' as there are other candidates."),
									*FleshAsset->GetName(),
									*Provider->SkinnedMesh->GetName());
							}
						}
					}
				}
			}
		}
	}
	// TODO: Someday, if/when Optimus supports deforming static meshes...
	//if (Provider->StaticMesh = Cast<UStaticMeshComponent>(InBinding))
	//{}

	Provider->FleshDeformerParameters = FleshDeformerParameters;
	return Provider;
}

//
// DataProvider
//

bool
UDIFleshDeformerDataProvider::IsValid() const
{
	return (SkinnedMesh && FleshMesh) || (StaticMesh && FleshMesh);
}

FComputeDataProviderRenderProxy*
UDIFleshDeformerDataProvider::GetRenderProxy()
{
	return new FDIFleshDeformerProviderProxy(SkinnedMesh, FleshMesh, FleshDeformerParameters);
}

//
// Proxy
//

FDIFleshDeformerProviderProxy::FDIFleshDeformerProviderProxy(
	USkinnedMeshComponent* SkinnedMeshComponentIn, 
	UFleshComponent* FleshComponentIn,
	FFleshDeformerParameters const& FleshDeformerParametersIn)
	: SkinnedMeshComponent(SkinnedMeshComponentIn)
	, SkeletalMeshObject(SkinnedMeshComponentIn ? SkinnedMeshComponentIn->MeshObject : nullptr)
	, FleshComponent(FleshComponentIn)
	, FleshDeformerParameters(FleshDeformerParametersIn)
{
	LodIndex = SkeletalMeshObject ? SkeletalMeshObject->GetLOD() : 0;
	if (SkinnedMeshComponent)
	{
		if (USkinnedAsset* SkinnedAsset = SkinnedMeshComponent->GetSkinnedAsset())
		{
			FPrimaryAssetId Id = SkinnedAsset->GetPrimaryAssetId();
			MeshId = Id.IsValid() ? Id.ToString() : SkinnedAsset->GetName();
		}
	}
}

bool 
FDIFleshDeformerProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (SkinnedMeshComponent == nullptr || SkeletalMeshObject == nullptr || FleshComponent == nullptr)
	{
		return false;
	}

	const UFleshAsset* RestCollectionAsset = FleshComponent->GetRestCollection();
	if (!RestCollectionAsset)
	{
		return false;
	}

	const FFleshCollection* FleshCollection = RestCollectionAsset->GetCollection();
	if (!FleshCollection)
	{
		return false;
	}

	// Note: tet bindings will be invalid until initialized to a bindings group.
	// We're just testing for the existence of such a group here.
	GeometryCollection::Facades::FTetrahedralBindings TetBindings(*FleshCollection);
	FName MeshName(*MeshId, MeshId.Len());
	int32 TetMeshIdx = TetBindings.GetTetMeshIndex(MeshName, LodIndex);
	if (TetMeshIdx == INDEX_NONE)
	{
		UE_LOG(LogFleshDeformer, Error, 
			TEXT("FleshDeformer: Failed to find tet mesh index associated to mesh '%s' LOD: %d"), 
			*MeshId, LodIndex);
		return false;
	}
	bool bValid = TetBindings.ContainsBindingsGroup(TetMeshIdx, MeshName, LodIndex);
	return bValid;
}

struct FFleshDeformerDataInterfacePermutationIds
{
	uint32 EnableDeformerFlesh = 0;
	uint32 EnableSurfaceOffsets = 0;

	FFleshDeformerDataInterfacePermutationIds(FComputeKernelPermutationVector const& PermutationVector)
	{
		{
			static FString Name(TEXT("ENABLE_DEFORMER_FLESH"));
			static uint32 Hash = GetTypeHash(Name);
			EnableDeformerFlesh = PermutationVector.GetPermutationBits(Name, Hash, 1);
		}
	}
};

void
FDIFleshDeformerProviderProxy::GatherPermutations(FPermutationData& InOutPermutationData) const
{
	FFleshDeformerDataInterfacePermutationIds PermutationIds(InOutPermutationData.PermutationVector);
	for (int32 InvocationIndex = 0; InvocationIndex < InOutPermutationData.NumInvocations; ++InvocationIndex)
	{
		InOutPermutationData.PermutationIds[InvocationIndex] |= PermutationIds.EnableDeformerFlesh;
	}
}

void
FDIFleshDeformerProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	//
	// Rest Vertices from FleshComponent's rest collection.
	//

	check(FleshComponent);
	const UFleshAsset* RestCollectionAsset = FleshComponent->GetRestCollection();
	const FFleshCollection* FleshCollection = RestCollectionAsset->GetCollection();
	NumTetRestVertices = 0;
	NumTetVertices = 0;

	const TManagedArray<FVector3f>* RestVertex = nullptr;
	if (FleshCollection)
	{
		RestVertex = &FleshCollection->Vertex;
		NumTetRestVertices = RestVertex->Num();
	}

	TetRestVertexBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(float), FGenericPlatformMath::Max((uint32)1, NumTetRestVertices * 3)),
		TEXT("FleshDeformer.RestVertex"));
	TetRestVertexBufferSRV = GraphBuilder.CreateSRV(TetRestVertexBuffer);

	if (NumTetRestVertices)
	{
		const FVector3f* Vert0 = RestVertex->GetData();
		const float* Vert00 = &Vert0->X;

		GraphBuilder.QueueBufferUpload(
			TetRestVertexBuffer,
			Vert00,
			NumTetRestVertices * 3 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			TetRestVertexBuffer,
			&NullFloatBuffer,
			1 * sizeof(float),
			ERDGInitialDataFlags::None);
	}

	//
	// Vertices from FleshComponent's dynamic collection, if they exist; rest
	// collection vertices otherwise.
	//

	const TManagedArray<FVector3f>* Vertex = nullptr;
	if (UFleshDynamicAsset* DynamicCollectionAsset = FleshComponent->GetDynamicCollection())
	{
		Vertex = &DynamicCollectionAsset->GetPositions();
		NumTetVertices = Vertex->Num();
	}
	if (!NumTetVertices && FleshCollection)
	{
		Vertex = &FleshCollection->Vertex;
		NumTetVertices = Vertex->Num();
	}

	TetVertexBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(float), FGenericPlatformMath::Max((uint32)1, NumTetVertices * 3)),
		TEXT("FleshDeformer.Vertex"));
	TetVertexBufferSRV = GraphBuilder.CreateSRV(TetVertexBuffer);

	if (NumTetVertices)
	{
		const FVector3f* Vert0 = Vertex->GetData();
		const float* Vert00 = &Vert0->X;

		GraphBuilder.QueueBufferUpload(
			TetVertexBuffer,
			Vert00,
			NumTetVertices * 3 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			TetVertexBuffer,
			&NullFloatBuffer,
			1 * sizeof(float),
			ERDGInitialDataFlags::None);
	}

	//
	// Bindings data from FleshComponent's rest collection.
	//

	GeometryCollection::Facades::FTetrahedralBindings* TetBindings = nullptr;
	if (FleshCollection)
	{
		TetBindings = new GeometryCollection::Facades::FTetrahedralBindings(*FleshCollection);
		FName MeshName(*MeshId, MeshId.Len());
		TetIndex = TetBindings->GetTetMeshIndex(MeshName, LodIndex);
		if (TetIndex != INDEX_NONE)
		{
			if (!TetBindings->ReadBindingsGroup(TetIndex, MeshName, LodIndex))
			{
				// Warn?
			}
		}
		if (!TetBindings->IsValid())
		{
			delete TetBindings;
			TetBindings = nullptr;
		}
	}
	const TManagedArrayAccessor<FIntVector4>* Parents = TetBindings ? TetBindings->GetParentsRO() : nullptr;
	const TManagedArrayAccessor<FVector4f>* Weights = TetBindings ? TetBindings->GetWeightsRO() : nullptr;
	const TManagedArrayAccessor<FVector3f>* Offset = TetBindings ? TetBindings->GetOffsetsRO() : nullptr;
	const TManagedArrayAccessor<float>* Mask = TetBindings ? TetBindings->GetMaskRO() : nullptr;

	NumParents = (Parents ? Parents->Num() : 0);
	ParentsBuffer =
		GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(int32), FGenericPlatformMath::Max((uint32)1, NumParents * 4)),
			TEXT("FleshDeformer.Parents"));
	ParentsBufferSRV = GraphBuilder.CreateSRV(ParentsBuffer);

	NumWeights = (Weights ? Weights->Num() : 0);
	WeightsBuffer =
		GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(float), FGenericPlatformMath::Max((uint32)1, NumWeights * 4)),
			TEXT("FleshDeformer.Weights"));
	WeightsBufferSRV = GraphBuilder.CreateSRV(WeightsBuffer);

	NumOffset = (Offset ? Offset->Num() : 0);
	OffsetBuffer =
		GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(float), FGenericPlatformMath::Max((uint32)1, NumOffset * 3)),
			TEXT("FleshDeformer.Offset"));
	OffsetBufferSRV = GraphBuilder.CreateSRV(OffsetBuffer);

	NumMask = (Mask ? Mask->Num() : 0);
	MaskBuffer =
		GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(float), FGenericPlatformMath::Max((uint32)1, NumMask)),
			TEXT("FleshDeformer.Mask"));
	MaskBufferSRV = GraphBuilder.CreateSRV(MaskBuffer);

	if (Parents && Parents->Num())
	{
		const TManagedArray<FIntVector4>& ParentsValues = Parents->Get();
		GraphBuilder.QueueBufferUpload(
			ParentsBuffer,
			&ParentsValues[0][0],
			ParentsValues.Num() * 4 * sizeof(int32),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			ParentsBuffer,
			&NullIntBuffer,
			1 * sizeof(int32),
			ERDGInitialDataFlags::None);
	}
	if (Weights && Weights->Num())
	{
		const TManagedArray<FVector4f>& WeightsValues = Weights->Get();
		GraphBuilder.QueueBufferUpload(
			WeightsBuffer,
			&WeightsValues.GetData()->X,
			WeightsValues.Num() * 4 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			WeightsBuffer,
			&NullFloatBuffer,
			1 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	if (Offset && Offset->Num())
	{
		const TManagedArray<FVector3f>& OffsetValues = Offset->Get();
		GraphBuilder.QueueBufferUpload(
			OffsetBuffer,
			&OffsetValues.GetData()->X,
			OffsetValues.Num() * 3 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			OffsetBuffer,
			&NullFloatBuffer,
			1 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	if (Mask && Mask->Num())
	{
		const TManagedArray<float>& MaskValues = Mask->Get();
		GraphBuilder.QueueBufferUpload(
			MaskBuffer,
			&MaskValues[0],
			MaskValues.Num() * sizeof(float),
			ERDGInitialDataFlags::None);
	}
	else
	{
		GraphBuilder.QueueBufferUpload(
			MaskBuffer,
			&NullFloatBuffer,
			1 * sizeof(float),
			ERDGInitialDataFlags::None);
	}
}

void
FDIFleshDeformerProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);

	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];
		
		FParameters& Parameters = ParameterArray[InvocationIndex];

		Parameters.NumVertices = RenderSection.GetNumVertices();
		Parameters.BaseVertexIndex = RenderSection.BaseVertexIndex;

		Parameters.NumTetRestVertexBuffer = NumTetRestVertices;
		Parameters.NumTetVertexBuffer = NumTetVertices;

		Parameters.NumParentsBuffer = NumParents;
		Parameters.NumWeightsBuffer = NumWeights;
		Parameters.NumOffsetBuffer = NumOffset;
		Parameters.NumMaskBuffer = NumMask;

		Parameters.TetRestVertexBuffer = TetRestVertexBufferSRV;
		Parameters.TetVertexBuffer = TetVertexBufferSRV;

		Parameters.ParentsBuffer = ParentsBufferSRV;
		Parameters.WeightsBuffer = WeightsBufferSRV;
		Parameters.OffsetBuffer = OffsetBufferSRV;
		Parameters.MaskBuffer = MaskBufferSRV;
	}
}
