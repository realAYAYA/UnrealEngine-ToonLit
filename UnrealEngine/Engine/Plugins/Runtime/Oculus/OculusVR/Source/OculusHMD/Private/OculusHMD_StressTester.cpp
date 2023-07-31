// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_StressTester.h"

#if OCULUS_STRESS_TESTS_ENABLED
#include "OculusHMD.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "OculusShaders.h"
#include "SceneUtils.h" // for SCOPED_DRAW_EVENT()

DECLARE_STATS_GROUP(TEXT("Oculus"), STATGROUP_Oculus, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("GPUStressRendering"), STAT_GPUStressRendering, STATGROUP_Oculus);

//-------------------------------------------------------------------------------------------------
// Uniform buffers
//-------------------------------------------------------------------------------------------------

//This buffer should contain variables that never, or rarely change
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FOculusPixelShaderConstantParameters, )
//SHADER_PARAMETER(FVector4f, Name)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FOculusPixelShaderConstantParameters, "PSConstants");

typedef TUniformBufferRef<FOculusPixelShaderConstantParameters> FOculusPixelShaderConstantParametersRef;


//This buffer is for variables that change very often (each frame for example)
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FOculusPixelShaderVariableParameters, )
SHADER_PARAMETER(int, IterationsMultiplier)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FOculusPixelShaderVariableParameters, "PSVariables");

typedef TUniformBufferRef<FOculusPixelShaderVariableParameters> FOculusPixelShaderVariableParametersRef;

namespace OculusHMD
{


//-------------------------------------------------------------------------------------------------
// FTextureVertexDeclaration
//-------------------------------------------------------------------------------------------------

struct FTextureVertex
{
	FVector4f	Position;
	FVector2f	UV;
};

inline FBufferRHIRef CreateTempOcculusVertexBuffer()
{
	FRHIResourceCreateInfo CreateInfo(TEXT("TempOcculusVertexBuffer"));
	FBufferRHIRef VertexBufferRHI = RHICreateVertexBuffer(sizeof(FTextureVertex) * 4, BUF_Volatile, CreateInfo);
	void* VoidPtr = RHILockBuffer(VertexBufferRHI, 0, sizeof(FTextureVertex) * 4, RLM_WriteOnly);

	FTextureVertex* Vertices = (FTextureVertex*)VoidPtr;
	Vertices[0].Position = FVector4f(-1.0f, 1.0f, 0, 1.0f);
	Vertices[1].Position = FVector4f(1.0f, 1.0f, 0, 1.0f);
	Vertices[2].Position = FVector4f(-1.0f, -1.0f, 0, 1.0f);
	Vertices[3].Position = FVector4f(1.0f, -1.0f, 0, 1.0f);
	Vertices[0].UV = FVector2f(0, 0);
	Vertices[1].UV = FVector2f(1, 0);
	Vertices[2].UV = FVector2f(0, 1);
	Vertices[3].UV = FVector2f(1, 1);
	RHIUnlockBuffer(VertexBufferRHI);

	return VertexBufferRHI;
}

class FTextureVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FTextureVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

static TGlobalResource<FTextureVertexDeclaration> GOculusTextureVertexDeclaration;


//-------------------------------------------------------------------------------------------------
// FStressTester
//-------------------------------------------------------------------------------------------------

TSharedPtr<FStressTester, ESPMode::ThreadSafe> FStressTester::SharedInstance;

TSharedRef<class FStressTester, ESPMode::ThreadSafe> FStressTester::Get()
{
	CheckInGameThread();
	if (!SharedInstance.IsValid())
	{
		SharedInstance = TSharedPtr<class FStressTester, ESPMode::ThreadSafe>(new FStressTester());
		check(SharedInstance.IsValid());
	}
	return SharedInstance.ToSharedRef();
}

FStressTester::FStressTester()
	: Mode(STM_None)
	, CPUSpinOffInSeconds(0.011 / 3.) // one third of the frame (default value)
	, PDsTimeLimitInSeconds(10.) // 10 secs
	, CPUsTimeLimitInSeconds(10.)// 10 secs
	, GPUsTimeLimitInSeconds(10.)// 10 secs
	, GPUIterationsMultiplier(0.)
	, CPUStartTimeInSeconds(0.)
	, GPUStartTimeInSeconds(0.)
	, PDStartTimeInSeconds(0.)
{

}

// multiple masks could be set, see EStressTestMode
void FStressTester::SetStressMode(uint32 InStressMask)
{
	check((InStressMask & (~STM__All)) == 0);
	Mode = InStressMask;

	for (uint32 m = 1; m < STM__All; m <<= 1)
	{
		if (InStressMask & m)
		{
			switch (m)
			{
			case STM_EyeBufferRealloc: UE_LOG(LogHMD, Log, TEXT("PD of EyeBuffer stress test is started")); break;
			case STM_CPUSpin: UE_LOG(LogHMD, Log, TEXT("CPU stress test is started")); break;
			case STM_GPU: UE_LOG(LogHMD, Log, TEXT("GPU stress test is started")); break;
			}
		}
	}
}

void FStressTester::DoTickCPU_GameThread(FOculusHMD* pPlugin)
{
	CheckInGameThread();

	if (Mode & STM_EyeBufferRealloc)
	{
		// Change PixelDensity every frame within MinPixelDensity..MaxPixelDensity range
		if (PDStartTimeInSeconds == 0.)
		{
			PDStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - PDStartTimeInSeconds >= PDsTimeLimitInSeconds)
			{
				PDStartTimeInSeconds = 0.;
				Mode &= ~STM_EyeBufferRealloc;
				UE_LOG(LogHMD, Log, TEXT("PD of EyeBuffer stress test is finished"));
			}
		}

		const int divisor = int((MaxPixelDensity - MinPixelDensity)*10.f);
		float NewPD = float(uint64(FPlatformTime::Seconds()*1000) % divisor) / 10.f + MinPixelDensity;

		pPlugin->SetPixelDensity(NewPD);
	}

	if (Mode & STM_CPUSpin)
	{
		// Simulate heavy CPU load within specified time limits

		if (CPUStartTimeInSeconds == 0.)
		{
			CPUStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - CPUStartTimeInSeconds >= CPUsTimeLimitInSeconds)
			{
				CPUStartTimeInSeconds = 0.;
				Mode &= ~STM_CPUSpin;
				UE_LOG(LogHMD, Log, TEXT("CPU stress test is finished"));
			}
		}

		const double StartSeconds = FPlatformTime::Seconds();
		int i, num = 1, primes = 0;

		bool bFinish = false;
		while (!bFinish)
		{
			i = 2;
			while (i <= num)
			{
				if (num % i == 0)
				{
					break;
				}
				i++;
				const double NowSeconds = FPlatformTime::Seconds();
				if (NowSeconds - StartSeconds >= CPUSpinOffInSeconds)
				{
					bFinish = true;
				}
			}
			if (i == num)
			{
				++primes;
			}

			++num;
		}
	}

	if (Mode & STM_GPU)
	{
		// Simulate heavy CPU load within specified time limits

		if (GPUStartTimeInSeconds == 0.)
		{
			GPUStartTimeInSeconds = FPlatformTime::Seconds();
		}
		else
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - GPUStartTimeInSeconds >= GPUsTimeLimitInSeconds)
			{
				GPUStartTimeInSeconds = 0.;
				Mode &= ~STM_GPU;
				UE_LOG(LogHMD, Log, TEXT("GPU stress test is finished"));
			}
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Console commands for managing the stress tester:
//-------------------------------------------------------------------------------------------------

static void StressGPUCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_GPU | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const int GpuMult = FCString::Atoi(*Args[0]);
		StressTester->SetGPULoadMultiplier(GpuMult);
	}
	if (Args.Num() > 1)
	{
		const float GpuTimeLimit = FCString::Atof(*Args[1]);
		StressTester->SetGPUsTimeLimitInSeconds(GpuTimeLimit);
	}
}

static FAutoConsoleCommand CStressGPUCmd(
	TEXT("vr.oculus.Stress.GPU"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressGPU", "Initiates a GPU stress test.\n Usage: vr.oculus.Stress.GPU [LoadMultiplier [TimeLimit]]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressGPUCmdHandler));

static void StressCPUCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_CPUSpin | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const float CpuLimit = FCString::Atof(*Args[0]);
		StressTester->SetCPUSpinOffPerFrameInSeconds(CpuLimit);
	}
	if (Args.Num() > 1)
	{
		const float CpuTimeLimit = FCString::Atof(*Args[1]);
		StressTester->SetCPUsTimeLimitInSeconds(CpuTimeLimit);
	}
}

static FAutoConsoleCommand CStressCPUCmd(
	TEXT("vr.oculus.Stress.CPU"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressCPU", "Initiates a CPU stress test.\n Usage: vr.oculus.Stress.CPU [PerFrameTime [TotalTimeLimit]]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressCPUCmdHandler));

static void StressPDCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(FStressTester::STM_EyeBufferRealloc | StressTester->GetStressMode());
	if (Args.Num() > 0)
	{
		const float TimeLimit = FCString::Atof(*Args[0]);
		StressTester->SetPDsTimeLimitInSeconds(TimeLimit);
	}
}

static FAutoConsoleCommand CStressPDCmd(
	TEXT("vr.oculus.Stress.PD"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressPD", "Initiates a pixel density stress test wher pixel density is changed every frame for TotalTimeLimit seconds.\n Usage: vr.oculus.Stress.PD [TotalTimeLimit]").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressPDCmdHandler));

static void StressResetCmdHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	auto StressTester = FStressTester::Get();
	StressTester->SetStressMode(0);
}

static FAutoConsoleCommand CStressResetCmd(
	TEXT("vr.oculus.Stress.Reset"),
	*NSLOCTEXT("OculusRift", "CCommandText_StressReset", "Resets the stress tester and stops all currently running stress tests.\n Usage: vr.oculus.Stress.Reset").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(StressResetCmdHandler));


} // namespace OculusHMD

#endif // #if OCULUS_STRESS_TESTS_ENABLED