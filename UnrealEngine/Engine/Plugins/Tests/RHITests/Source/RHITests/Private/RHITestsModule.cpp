// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHITestsModule.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "RHIBufferTests.h"
#include "RHITextureTests.h"

#define LOCTEXT_NAMESPACE "FRHITestsModule"

static bool RunTests_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	bool bResult = true;

	// ------------------------------------------------
	// RHIClearUAVUint / RHIClearUAVFloat tests
	// ------------------------------------------------

	// Vertex/Structured Buffer
	{
		RUN_TEST(FRHIBufferTests::Test_RHIClearUAVUint_VertexBuffer(RHICmdList));
		RUN_TEST(FRHIBufferTests::Test_RHIClearUAVFloat_VertexBuffer(RHICmdList));

		RUN_TEST(FRHIBufferTests::Test_RHIClearUAVUint_StructuredBuffer(RHICmdList));
		RUN_TEST(FRHIBufferTests::Test_RHIClearUAVFloat_StructuredBuffer(RHICmdList));

		RUN_TEST(FRHIBufferTests::Test_RHICreateBuffer_Parallel(RHICmdList));
	}

	// Texture2D/3D
	{
		RUN_TEST(FRHITextureTests::Test_RHIClearUAV_Texture2D(RHICmdList));
		RUN_TEST(FRHITextureTests::Test_RHIClearUAV_Texture3D(RHICmdList));
		RUN_TEST(FRHITextureTests::Test_UpdateTexture2D(RHICmdList));
		RUN_TEST(FRHITextureTests::Test_MultipleLockTexture2D(RHICmdList));
	}

	{
		RUN_TEST(FRHITextureTests::Test_RHIFormats(RHICmdList));
	}

	{
		RUN_TEST(FRHITextureTests::Test_RHICopyTexture(RHICmdList));
	}

	// @todo - add more tests
	return bResult;
}

void FRHITestsModule::StartupModule()
{
	//FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("RHITests"))->GetBaseDir(), TEXT("Shaders"));
	//AddShaderSourceDirectoryMapping(TEXT("/Plugin/RHITests"), PluginShaderDir);
}

void FRHITestsModule::RunAllTests()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("rhiunittest")))
	{
		if (RunOnRenderThreadSynchronous(RunTests_RenderThread))
		{
			UE_LOG(LogRHIUnitTestCommandlet, Display, TEXT("RHI unit tests completed. All tests passed."));
		}
		else
		{
			UE_LOG(LogRHIUnitTestCommandlet, Error, TEXT("RHI unit tests completed. At least one test failed."));
		}
	}
}

void FRHITestsModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRHITestsModule, RHITests)
