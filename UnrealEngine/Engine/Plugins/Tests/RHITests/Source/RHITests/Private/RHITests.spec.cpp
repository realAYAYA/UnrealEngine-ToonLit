// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "CoreMinimal.h"
#include "RenderUtils.h"
#include "RenderingThread.h"
#include "RHIBufferTests.h"
#include "RHITextureTests.h"


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
			bool bResult = RunOnRenderThreadSynchronous(FRHITextureTests::Test_UpdateTexture2D);
			TestEqual("RHI Update Texture2D failed", bResult, 1);
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
}
