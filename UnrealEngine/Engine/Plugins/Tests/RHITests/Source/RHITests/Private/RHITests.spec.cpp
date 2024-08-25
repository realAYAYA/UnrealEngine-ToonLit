// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CoreMinimal.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "RHIBufferTests.h"
#include "RHITextureTests.h"
#include "RHIDrawTests.h"
#include "RHIReadbackTests.h"
#include "RHIReservedResourceTests.h"

BEGIN_DEFINE_SPEC(FAutomationRHITest, "Rendering.RHI", EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NonNullRHI)
END_DEFINE_SPEC(FAutomationRHITest)
void FAutomationRHITest::Define()
{
	Describe("Test RHI Clear", [this]()
	{
		It("RHI Clear UINT VertexBuffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIBufferTests::Test_RHIClearUAVUint_VertexBuffer);
			TestEqual("Clear UINT VertexBuffer failed", bResult, 1);
		});

		It("RHI Clear Float VertexBuffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIBufferTests::Test_RHIClearUAVFloat_VertexBuffer);
			TestEqual("Clear Float VertexBuffer failed", bResult, 1);
		});

		It("RHI Clear UINT StructuredBuffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIBufferTests::Test_RHIClearUAVUint_StructuredBuffer);
			TestEqual("Clear UINT StructuredBuffer failed", bResult, 1);
		});

		It("RHI Clear Float StructuredBuffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIBufferTests::Test_RHIClearUAVFloat_StructuredBuffer);
			TestEqual("Clear Float StructuredBuffer failed", bResult, 1);
		});

		It("RHI Clear Texture2D", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_RHIClearUAV_Texture2D);
			TestEqual("Clear Texture2D failed", bResult, 1);
		});

		It("RHI Clear Texture3D", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_RHIClearUAV_Texture3D);
			TestEqual("Clear Texture3D failed", bResult, 1);
		});

		It("RHI Clear Render Targets", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_ClearRenderTargets);
			TestEqual("Clear Render Targets failed", bResult, 1);
		});
	});

	Describe("Test RHI Pixel Format", [this]()
	{
		It("RHI Target Formats", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_RHIFormats);
			TestEqual("RHI Target Formats failed", bResult, 1);
		});
	});

	Describe("Test RHI Resource Update", [this]()
	{
		It("RHI Update Texture2D", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_UpdateTexture);
			TestEqual("RHI Update Texture failed", bResult, 1);
		});
		
		It("RHI Multiple Lock Texture2D", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_MultipleLockTexture2D);
			TestEqual("RHI Multiple Lock Texture2D failed", bResult, 1);
		});
	});

	Describe("Test RHI Copy", [this]
	{
		It("RHICopyTexture", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_RHICopyTexture);
			TestEqual("RHICopyTexture", bResult, 1);
		});
	});

	Describe("Test RHI Readback", [this]
	{
		It("Buffer Readback", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReadbackTests::Test_BufferReadback);
			TestEqual("BufferReadback", bResult, 1);
		});

		It("Texture Readback", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReadbackTests::Test_TextureReadback);
			TestEqual("TextureReadback", bResult, 1);
		});
	});

	Describe("Test RHI Create Buffer Parallel", [this]
	{
		It("RHICreateBuffer_Parallel", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIBufferTests::Test_RHICreateBuffer_Parallel);
			TestEqual("RHICreateBuffer_Parallel", bResult, 1);
		});
	});

	Describe("Test RHI Draw", [this]()
	{
		It("RHI DrawBaseVertexAndInstanceDirect", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIDrawTests::Test_DrawBaseVertexAndInstanceDirect);
			TestEqual("RHI DrawBaseVertexAndInstanceDirect", bResult, 1);
		});
		It("RHI DrawBaseVertexAndInstanceIndirect", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIDrawTests::Test_DrawBaseVertexAndInstanceIndirect);
			TestEqual("RHI DrawBaseVertexAndInstanceIndirect", bResult, 1);
		});
		It("RHI MultiDrawIndirect", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIDrawTests::Test_MultiDrawIndirect);
			TestEqual("RHI MultiDrawIndirect", bResult, 1);
		});
	});

	Describe("Test RHI Reserved Resource", [this]()
	{
		It("Create Reserved Volume Texture", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReservedResourceTests::Test_ReservedResource_CreateVolumeTexture);
			TestEqual("Create Reserved Volume Texture failed", bResult, 1);
		});

		It("Create Reserved Texture", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReservedResourceTests::Test_ReservedResource_CreateTexture);
			TestEqual("Create Reserved Texture failed", bResult, 1);
		});

		It("Create Reserved Buffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReservedResourceTests::Test_ReservedResource_CreateBuffer);
			TestEqual("Create Reserved Buffer failed", bResult, 1);
		});

		It("Commit Reserved Buffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReservedResourceTests::Test_ReservedResource_CommitBuffer);
			TestEqual("Commit Reserved Buffer failed", bResult, 1);
		});

		It("Decommit Reserved Buffer", [this]()
		{
			bool bResult = RunOnRenderThreadSynchronous(FRHIReservedResourceTests::Test_ReservedResource_DecommitBuffer);
			TestEqual("Decommit Reserved Buffer failed", bResult, 1);
		});
	});
}
