// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RHI.h"
#include "Misc/AutomationTest.h"
#include "Math/DoubleFloat.h"

#if WITH_DEV_AUTOMATION_TESTS || WITH_EDITOR

class FRayTracingTestbedBase : public FAutomationTestBase
{
public:
	FRayTracingTestbedBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	virtual bool CanRunInEnvironment(const FString& TestParams, FString* OutReason, bool* OutWarn) const override
	{
		if (!GRHISupportsRayTracing || !GRHISupportsRayTracingShaders)
		{
			if (OutReason)
			{
				*OutReason = TEXT("RHI does not support Ray Tracing and/or Ray Tracing Shaders.");
			}

			if (OutWarn)
			{
				*OutWarn = false;
			}

			return false;
		}

		return true;
	}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FRayTracingTestbed, FRayTracingTestbedBase, "System.Renderer.RayTracing.BasicRayTracing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::HighPriority | EAutomationTestFlags::EngineFilter)

#if RHI_RAYTRACING

#include "RayTracing/RayTracingBasicShaders.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RayTracingDefinitions.h"
#include "RayTracingPayloadType.h"
#include "GlobalShader.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RenderUtils.h"
#include "RHIUtilities.h"

// HINT: Execute this test via console command in editor: Automation RunTest System.Renderer.RayTracing.BasicRayTracing
bool RunRayTracingTestbed_RenderThread(const FString& Parameters)
{
	check(IsInRenderingThread());

	// The ray tracing testbed currently rquires full ray tracing pipeline support.
	if (!GRHISupportsRayTracing || !GRHISupportsRayTracingShaders)
	{
		//Return true so the test passes in DX11, until the testing framework allows to skip tests depending on defined preconditions
		return true;
	}

	FBufferRHIRef VertexBuffer;
	FRHICommandListImmediate& RHICmdList = FRHICommandListImmediate::Get();

	{
		TResourceArray<FVector3f> PositionData;
		PositionData.SetNumUninitialized(3);
		PositionData[0] = FVector3f( 1, -1, 0);
		PositionData[1] = FVector3f( 1,  1, 0);
		PositionData[2] = FVector3f(-1, -1, 0);

		FRHIResourceCreateInfo CreateInfo(TEXT("RayTracingTestbedVB"));
		CreateInfo.ResourceArray = &PositionData;

		VertexBuffer = RHICmdList.CreateVertexBuffer(PositionData.GetResourceDataSize(), BUF_Static, CreateInfo);
	}

	FBufferRHIRef IndexBuffer;

	{
		TResourceArray<uint16> IndexData;
		IndexData.SetNumUninitialized(3);
		IndexData[0] = 0;
		IndexData[1] = 1;
		IndexData[2] = 2;

		FRHIResourceCreateInfo CreateInfo(TEXT("RayTracingTestbedIB"));
		CreateInfo.ResourceArray = &IndexData;

		IndexBuffer = RHICmdList.CreateIndexBuffer(2, IndexData.GetResourceDataSize(), BUF_Static, CreateInfo);
	}

	static constexpr uint32 NumRays = 4;

	FBufferRHIRef RayBuffer;
	FShaderResourceViewRHIRef RayBufferView;

	{
		TResourceArray<FBasicRayTracingRay> RayData;
		RayData.SetNumUninitialized(NumRays);
		RayData[0] = FBasicRayTracingRay{ { 0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f}, 100000.0f }; // expected to hit
		RayData[1] = FBasicRayTracingRay{ { 0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f},      0.5f }; // expected to miss (short ray)
		RayData[2] = FBasicRayTracingRay{ { 0.75f, 0.0f,  1.0f}, 0xFFFFFFFF, {0.0f, 0.0f, -1.0f}, 100000.0f }; // expected to hit  (should hit back face)
		RayData[3] = FBasicRayTracingRay{ {-0.75f, 0.0f, -1.0f}, 0xFFFFFFFF, {0.0f, 0.0f,  1.0f}, 100000.0f }; // expected to miss (doesn't intersect)

		FRHIResourceCreateInfo CreateInfo(TEXT("RayBuffer"));
		CreateInfo.ResourceArray = &RayData;

		RayBuffer = RHICmdList.CreateBuffer(RayData.GetResourceDataSize(), 
			BUF_Static | BUF_ShaderResource | BUF_StructuredBuffer, 
			sizeof(FBasicRayTracingRay),
			ERHIAccess::SRVMask,
			CreateInfo
		);
		RayBufferView = RHICmdList.CreateShaderResourceView(RayBuffer, 
			FRHIViewDesc::CreateBufferSRV()
			.SetType(FRHIViewDesc::EBufferType::Structured)
			.SetStride(sizeof(FBasicRayTracingRay))
			.SetNumElements(NumRays)
		);
	}

	FBufferRHIRef OcclusionResultBuffer;
	FUnorderedAccessViewRHIRef OcclusionResultBufferView;

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("OcclusionResultBuffer"));
		OcclusionResultBuffer = RHICmdList.CreateBuffer(sizeof(uint32) * NumRays, 
			BUF_Static | BUF_UnorderedAccess | BUF_StructuredBuffer, 
			sizeof(uint32), 
			ERHIAccess::UAVMask, 
			CreateInfo
		);
		OcclusionResultBufferView = RHICmdList.CreateUnorderedAccessView(OcclusionResultBuffer, 
			FRHIViewDesc::CreateBufferUAV()
			.SetType(FRHIViewDesc::EBufferType::Structured)
			.SetStride(sizeof(uint32))
			.SetNumElements(NumRays)
		);
	}

	FBufferRHIRef IntersectionResultBuffer;
	FUnorderedAccessViewRHIRef IntersectionResultBufferView;

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("IntersectionResultBuffer"));
		IntersectionResultBuffer = RHICmdList.CreateBuffer(sizeof(FBasicRayTracingIntersectionResult) * NumRays, 
			BUF_Static | BUF_UnorderedAccess | BUF_StructuredBuffer, 
			sizeof(FBasicRayTracingIntersectionResult), 
			ERHIAccess::UAVMask, 
			CreateInfo
		);
		IntersectionResultBufferView = RHICmdList.CreateUnorderedAccessView(IntersectionResultBuffer, 
			FRHIViewDesc::CreateBufferUAV()
			.SetType(FRHIViewDesc::EBufferType::Structured)
			.SetStride(sizeof(FBasicRayTracingIntersectionResult))
			.SetNumElements(NumRays)
		);
	}

	FRayTracingGeometryInitializer GeometryInitializer;
	GeometryInitializer.DebugName = FName("DebugTriangle");
	GeometryInitializer.IndexBuffer = IndexBuffer;
	GeometryInitializer.GeometryType = RTGT_Triangles;
	GeometryInitializer.bFastBuild = false;
	FRayTracingGeometrySegment Segment;
	Segment.VertexBuffer = VertexBuffer;
	Segment.NumPrimitives = 1;
	Segment.MaxVertices = 3;
	GeometryInitializer.Segments.Add(Segment);
	GeometryInitializer.TotalPrimitiveCount = Segment.NumPrimitives;
	FRayTracingGeometryRHIRef Geometry = RHICmdList.CreateRayTracingGeometry(GeometryInitializer);

	static constexpr uint32 NumTransforms = 1;
	static constexpr uint32 NumInstances = 1;

	FRayTracingGeometryInstance Instances[NumInstances] = {};
	Instances[0].GeometryRHI = Geometry;
	Instances[0].NumTransforms = NumTransforms;
	Instances[0].Transforms = MakeArrayView(&FMatrix::Identity, 1);

	FRayTracingSceneWithGeometryInstances RayTracingScene = CreateRayTracingSceneWithGeometryInstances(
		Instances,
		1,
		RAY_TRACING_NUM_SHADER_SLOTS,
		1);

	const FRayTracingSceneInitializer2& SceneInitializer = RayTracingScene.Scene->GetInitializer();

	ERayTracingAccelerationStructureFlags SceneBuildFlags = ERayTracingAccelerationStructureFlags::FastTrace;
	FRayTracingAccelerationStructureSize SceneSizeInfo = RHICalcRayTracingSceneSize(1, SceneBuildFlags);
	FRHIResourceCreateInfo SceneBufferCreateInfo(TEXT("RayTracingTestBedSceneBuffer"));
	FBufferRHIRef SceneBuffer = RHICmdList.CreateBuffer(
		uint32(SceneSizeInfo.ResultSize),
		BUF_AccelerationStructure, 0 /*Stride*/, ERHIAccess::BVHWrite,
		SceneBufferCreateInfo);

	FRHIResourceCreateInfo ScratchBufferCreateInfo(TEXT("RayTracingTestBedScratchBuffer"));
	FBufferRHIRef ScratchBuffer = RHICmdList.CreateBuffer(
		uint32(SceneSizeInfo.BuildScratchSize),
		BUF_UnorderedAccess, GRHIRayTracingScratchBufferAlignment, ERHIAccess::UAVCompute,
		ScratchBufferCreateInfo);

	FRWBufferStructured InstanceBuffer;
	InstanceBuffer.Initialize(RHICmdList, TEXT("RayTracingTestBedInstanceBuffer"), GRHIRayTracingInstanceDescriptorSize, SceneInitializer.NumNativeInstancesPerLayer[0]);

	FByteAddressBuffer AccelerationStructureAddressesBuffer;
	AccelerationStructureAddressesBuffer.Initialize(RHICmdList, TEXT("RayTracingTestBedAccelerationStructureAddressesBuffer"), sizeof(FRayTracingAccelerationStructureAddress), BUF_Volatile);

	const uint32 InstanceUploadBufferSize = SceneInitializer.NumNativeInstancesPerLayer[0] * sizeof(FRayTracingInstanceDescriptorInput);
	FBufferRHIRef InstanceUploadBuffer;
	FShaderResourceViewRHIRef InstanceUploadSRV;
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("RayTracingTestBedInstanceUploadBuffer"));
		InstanceUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FRayTracingInstanceDescriptorInput), InstanceUploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
		InstanceUploadSRV = RHICmdList.CreateShaderResourceView(InstanceUploadBuffer);
	}

	const uint32 TransformUploadBufferSize = RayTracingScene.NumNativeCPUInstances * 3 * sizeof(FVector4f);
	FBufferRHIRef TransformUploadBuffer;
	FShaderResourceViewRHIRef TransformUploadSRV;
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("RayTracingTestBedInstanceUploadBuffer"));
		TransformUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), TransformUploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
		TransformUploadSRV = RHICmdList.CreateShaderResourceView(TransformUploadBuffer);
	}

	{
		FRayTracingInstanceDescriptorInput* InstanceUploadData = (FRayTracingInstanceDescriptorInput*)RHICmdList.LockBuffer(InstanceUploadBuffer, 0, InstanceUploadBufferSize, RLM_WriteOnly);
		FVector4f* TransformUploadData = (FVector4f*)RHICmdList.LockBuffer(TransformUploadBuffer, 0, TransformUploadBufferSize, RLM_WriteOnly);
		FillRayTracingInstanceUploadBuffer(
			RayTracingScene.Scene,
			FVector::ZeroVector,
			Instances,
			RayTracingScene.InstanceGeometryIndices,
			RayTracingScene.BaseUploadBufferOffsets,
			RayTracingScene.NumNativeGPUSceneInstances,
			RayTracingScene.NumNativeCPUInstances,
			MakeArrayView(InstanceUploadData, SceneInitializer.NumNativeInstancesPerLayer[0]),
			MakeArrayView(TransformUploadData, RayTracingScene.NumNativeCPUInstances * 3));
		RHICmdList.UnlockBuffer(TransformUploadBuffer);
		RHICmdList.UnlockBuffer(InstanceUploadBuffer);
	}

	RHICmdList.EnqueueLambda([&AccelerationStructureAddressesBuffer, &SceneInitializer](FRHICommandListImmediate& RHICmdList)
		{
			FRayTracingAccelerationStructureAddress* AddressesPtr = (FRayTracingAccelerationStructureAddress*)RHICmdList.LockBuffer(
				AccelerationStructureAddressesBuffer.Buffer, 
				0, 
				SceneInitializer.ReferencedGeometries.Num() * sizeof(FRayTracingAccelerationStructureAddress), RLM_WriteOnly);

			const uint32 NumGeometries = SceneInitializer.ReferencedGeometries.Num();
			for (uint32 GeometryIndex = 0; GeometryIndex < NumGeometries; ++GeometryIndex)
			{
				AddressesPtr[GeometryIndex] = SceneInitializer.ReferencedGeometries[GeometryIndex]->GetAccelerationStructureAddress(RHICmdList.GetGPUMask().ToIndex());
			}

			RHICmdList.UnlockBuffer(AccelerationStructureAddressesBuffer.Buffer);
		});

	BuildRayTracingInstanceBuffer(
		RHICmdList,
		nullptr,
		FDFVector3{},
		InstanceBuffer.UAV,
		InstanceUploadSRV,
		AccelerationStructureAddressesBuffer.SRV,
		TransformUploadSRV,
		RayTracingScene.NumNativeGPUSceneInstances,
		RayTracingScene.NumNativeCPUInstances,
		{},
		nullptr,
		nullptr);

	RHICmdList.BindAccelerationStructureMemory(RayTracingScene.Scene, SceneBuffer, 0);

	RHICmdList.BuildAccelerationStructure(Geometry);

	// #yuriy_todo: explicit transitions and state validation for BLAS
	// RHICmdList.Transition(FRHITransitionInfo(Geometry.GetReference(), ERHIAccess::BVHWrite, ERHIAccess::BVHRead));

	FRayTracingSceneBuildParams BuildParams;
	BuildParams.Scene = RayTracingScene.Scene;
	BuildParams.ScratchBuffer = ScratchBuffer;
	BuildParams.ScratchBufferOffset = 0;
	BuildParams.InstanceBuffer = InstanceBuffer.Buffer;
	BuildParams.InstanceBufferOffset = 0;

	RHICmdList.BuildAccelerationStructure(BuildParams);

	RHICmdList.Transition(FRHITransitionInfo(RayTracingScene.Scene.GetReference(), ERHIAccess::BVHWrite, ERHIAccess::BVHRead));

	FShaderResourceViewInitializer RayTracingSceneViewInitializer(SceneBuffer, RayTracingScene.Scene->GetLayerBufferOffset(0), 0);
	FShaderResourceViewRHIRef RayTracingSceneView = RHICmdList.CreateShaderResourceView(RayTracingSceneViewInitializer);

	DispatchBasicOcclusionRays(RHICmdList, RayTracingScene.Scene, RayTracingSceneView, RayBufferView, OcclusionResultBufferView, NumRays);
	DispatchBasicIntersectionRays(RHICmdList, RayTracingScene.Scene, RayTracingSceneView, RayBufferView, IntersectionResultBufferView, NumRays);

	const bool bValidateResults = true;
	bool bOcclusionTestOK = false;
	bool bIntersectionTestOK = false;

	if (bValidateResults)
	{
		GDynamicRHI->RHISubmitCommandsAndFlushGPU();
		GDynamicRHI->RHIBlockUntilGPUIdle();

		// Read back and validate occlusion trace results

		{
			auto MappedResults = (const uint32*)RHICmdList.LockBuffer(OcclusionResultBuffer, 0, sizeof(uint32)*NumRays, RLM_ReadOnly);

			check(MappedResults);

			check(MappedResults[0] != 0); // expect hit
			check(MappedResults[1] == 0); // expect miss
			check(MappedResults[2] != 0); // expect hit
			check(MappedResults[3] == 0); // expect miss

			RHICmdList.UnlockBuffer(OcclusionResultBuffer);

			bOcclusionTestOK = (MappedResults[0] != 0) && (MappedResults[1] == 0) && (MappedResults[2] != 0) && (MappedResults[3] == 0);
		}

		// Read back and validate intersection trace results

		{
			auto MappedResults = (const FBasicRayTracingIntersectionResult*)RHICmdList.LockBuffer(IntersectionResultBuffer, 0, sizeof(FBasicRayTracingIntersectionResult)*NumRays, RLM_ReadOnly);

			check(MappedResults);

			// expect hit primitive 0, instance 0, barycentrics {0.5, 0.125}
			check(MappedResults[0].HitT >= 0);
			check(MappedResults[0].PrimitiveIndex == 0);
			check(MappedResults[0].InstanceIndex == 0);
			check(FMath::IsNearlyEqual(MappedResults[0].Barycentrics[0], 0.5f));
			check(FMath::IsNearlyEqual(MappedResults[0].Barycentrics[1], 0.125f));

			check(MappedResults[1].HitT < 0); // expect miss
			check(MappedResults[2].HitT >= 0); // expect hit back face
			check(MappedResults[3].HitT < 0); // expect miss

			RHICmdList.UnlockBuffer(IntersectionResultBuffer);

			bIntersectionTestOK = (MappedResults[0].HitT >= 0) && (MappedResults[1].HitT < 0) && (MappedResults[2].HitT >= 0) && (MappedResults[3].HitT < 0);
		}
	}

	return (bOcclusionTestOK && bIntersectionTestOK);
}
 
// Dummy shader to test shader compilation and reflection.
class FTestRaygenShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTestRaygenShader, Global);

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static ERayTracingPayloadType GetRayTracingPayloadType(const int32 PermutationId)
	{
		return ERayTracingPayloadType::Minimal;
	}

	FTestRaygenShader() {}
	//virtual ~FTestRaygenShader() {}

	/** Initialization constructor. */
	FTestRaygenShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TLAS.Bind(Initializer.ParameterMap, TEXT("TLAS"));
		Rays.Bind(Initializer.ParameterMap, TEXT("Rays"));
		Output.Bind(Initializer.ParameterMap, TEXT("Output"));
	}

	/*bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << TLAS;
		Ar << Rays;
		Ar << Output;
		return bShaderHasOutdatedParameters;
	}*/

	LAYOUT_FIELD(FShaderResourceParameter, TLAS)   // SRV RaytracingAccelerationStructure
	LAYOUT_FIELD(FShaderResourceParameter, Rays)   // SRV StructuredBuffer<FBasicRayData>
	LAYOUT_FIELD(FShaderResourceParameter, Output) // UAV RWStructuredBuffer<uint>
};

IMPLEMENT_RT_PAYLOAD_TYPE(ERayTracingPayloadType::Minimal, 4);
IMPLEMENT_GLOBAL_SHADER(FTestRaygenShader, "/Engine/Private/RayTracing/RayTracingTest.usf", "TestMainRGS", SF_RayGen);


bool FRayTracingTestbed::RunTest(const FString& Parameters)
{
	bool bTestPassed = false;
	FlushRenderingCommands();

	ENQUEUE_RENDER_COMMAND(FRayTracingTestbed)(
		[&](FRHICommandListImmediate& RHICmdList)
	{
		bTestPassed = RunRayTracingTestbed_RenderThread(Parameters);
	}
	);  

	FlushRenderingCommands();

	return bTestPassed;
}

#else // RHI_RAYTRACING

bool FRayTracingTestbed::RunTest(const FString& Parameters)
{
	// Nothing to do when ray tracing is disabled
	return true;
}

#endif // RHI_RAYTRACING

#endif //WITH_DEV_AUTOMATION_TESTS
