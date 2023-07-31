// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterVertexFactory.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"
#include "WaterInstanceDataBuffer.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryParameters, "WaterVF");
IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FWaterVertexFactoryRaytracingParameters, "WaterRaytracingVF");

/**
 * Shader parameters for water vertex factory.
 */
template <bool bWithWaterSelectionSupport>
class TWaterVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(TWaterVertexFactoryShaderParameters<bWithWaterSelectionSupport>, NonVirtual);

public:
	using WaterVertexFactoryType = TWaterVertexFactory<bWithWaterSelectionSupport>;
	using WaterMeshUserDataType = TWaterMeshUserData<bWithWaterSelectionSupport>;
	using WaterInstanceDataBuffersType = TWaterInstanceDataBuffers<bWithWaterSelectionSupport>;

	void Bind(const FShaderParameterMap& ParameterMap)
	{
	}

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const class FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const class FVertexFactory* InVertexFactory,
		const struct FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		WaterVertexFactoryType* VertexFactory = (WaterVertexFactoryType*)InVertexFactory;

		const WaterMeshUserDataType* WaterMeshUserData = (const WaterMeshUserDataType*)BatchElement.UserData;

		const WaterInstanceDataBuffersType* InstanceDataBuffers = WaterMeshUserData->InstanceDataBuffers;

		const int32 InstanceOffsetValue = BatchElement.UserIndex;

		ShaderBindings.Add(Shader->GetUniformBufferParameter<FWaterVertexFactoryParameters>(), VertexFactory->GetWaterVertexFactoryUniformBuffer(WaterMeshUserData->RenderGroupType));

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			ShaderBindings.Add(Shader->GetUniformBufferParameter<FWaterVertexFactoryRaytracingParameters>(), WaterMeshUserData->WaterVertexFactoryRaytracingVFUniformBuffer);
		}
#endif

		if (VertexStreams.Num() > 0)
		{
			for (int32 i = 0; i < WaterInstanceDataBuffersType::NumBuffers; ++i)
			{
				FVertexInputStream* InstanceInputStream = VertexStreams.FindByPredicate([i](const FVertexInputStream& InStream) { return InStream.StreamIndex == i+1; });
				check(InstanceInputStream);
				
				// Bind vertex buffer
				check(InstanceDataBuffers->GetBuffer(i));
				InstanceInputStream->VertexBuffer = InstanceDataBuffers->GetBuffer(i);
			}

			if (InstanceOffsetValue > 0)
			{
				VertexFactory->OffsetInstanceStreams(InstanceOffsetValue, InputStreamType, VertexStreams);
			}
		}
	}
};

// ----------------------------------------------------------------------------------

// Always implement the basic vertex factory so that it's there for both editor and non-editor builds :
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false>);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ false>, SF_Vertex, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false>);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ false>, SF_Compute, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false>);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ false>, SF_RayHitGroup, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ false>);
#endif // RHI_RAYTRACING
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, TWaterVertexFactory</*bWithWaterSelectionSupport = */ false>, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

#if WITH_WATER_SELECTION_SUPPORT

// In editor builds, also implement the vertex factory that supports water selection:
IMPLEMENT_TEMPLATE_TYPE_LAYOUT(template<>, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true>);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ true>, SF_Vertex, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true>);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ true>, SF_Compute, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true>);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(TWaterVertexFactory</*bWithWaterSelectionSupport = */ true>, SF_RayHitGroup, TWaterVertexFactoryShaderParameters</*bWithWaterSelectionSupport = */ true>);
#endif
IMPLEMENT_TEMPLATE_VERTEX_FACTORY_TYPE(template<>, TWaterVertexFactory</*bWithWaterSelectionSupport = */ true>, "/Plugin/Water/Private/WaterMeshVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsPSOPrecaching
);

#endif // WITH_WATER_SELECTION_SUPPORT

const FVertexFactoryType* GetWaterVertexFactoryType(bool bWithWaterSelectionSupport)
{
#if WITH_WATER_SELECTION_SUPPORT
	if (bWithWaterSelectionSupport)
	{
		return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ true>::StaticType;
	}
	else
#endif // WITH_WATER_SELECTION_SUPPORT
	{
		check(!bWithWaterSelectionSupport);
		return &TWaterVertexFactory</*bWithWaterSelectionSupport = */ false>::StaticType;
	}
}
