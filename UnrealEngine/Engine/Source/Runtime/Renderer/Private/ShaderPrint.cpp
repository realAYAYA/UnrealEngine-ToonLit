// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderPrint.h"
#include "ShaderPrintParameters.h"

#include "ShaderParameterStruct.h"
#include "CommonRenderResources.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Engine/Engine.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "ScreenPass.h"
#include "SystemTextures.h"

namespace ShaderPrint
{
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Console variables

	static int32 GEnabled = false;
	static FAutoConsoleVariableRef CVarEnable(
		TEXT("r.ShaderPrint"),
		GEnabled,
		TEXT("ShaderPrint debugging toggle.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarFontSize(
		TEXT("r.ShaderPrint.FontSize"),
		8,
		TEXT("ShaderPrint font size.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarFontSpacingX(
		TEXT("r.ShaderPrint.FontSpacingX"),
		0,
		TEXT("ShaderPrint horizontal spacing between symbols.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarFontSpacingY(
		TEXT("r.ShaderPrint.FontSpacingY"),
		8,
		TEXT("ShaderPrint vertical spacing between symbols.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarMaxCharacterCount(
		TEXT("r.ShaderPrint.MaxCharacters"),
		2000,
		TEXT("ShaderPrint output buffer size.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarMaxWidgetCount(
		TEXT("r.ShaderPrint.MaxWidget"),
		32,
		TEXT("ShaderPrint max widget count.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarMaxLineCount(
		TEXT("r.ShaderPrint.MaxLine"),
		32,
		TEXT("ShaderPrint max line count.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarMaxTriangleCount(
		TEXT("r.ShaderPrint.MaxTriangle"),
		32,
		TEXT("ShaderPrint max triangle count.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	static TAutoConsoleVariable<int32> CVarDrawLock(
		TEXT("r.ShaderPrint.Lock"),
		0,
		TEXT("Lock the line drawing.\n"),
		ECVF_Cheat | ECVF_RenderThreadSafe);

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Global states

	static uint32 GWidgetRequestCount = 0;
	static uint32 GCharacterRequestCount = 0;
	static uint32 GLineRequestCount = 0;
	static uint32 GTriangleRequestCount = 0;
	static FViewInfo* GDefaultView = nullptr;
	static TArray<FFrozenShaderPrintData> GShaderPrintDataToRender;

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Struct & Functions

	static uint32 GetMaxValueCount()
	{
		return FMath::Max(CVarMaxCharacterCount.GetValueOnAnyThread() + int32(GCharacterRequestCount), 0);
	}

	static uint32 GetMaxWidgetCount()
	{
		return FMath::Max(CVarMaxWidgetCount.GetValueOnAnyThread() + int32(GWidgetRequestCount), 0);
	}

	static uint32 GetMaxLineCount()
	{
		return FMath::Max(CVarMaxLineCount.GetValueOnAnyThread() + int32(GLineRequestCount), 0);
	}

	static uint32 GetMaxTriangleCount()
	{
		return FMath::Max(CVarMaxTriangleCount.GetValueOnAnyThread() + int32(GTriangleRequestCount), 0);
	}

	// Returns the number of uints used for counters, a line element, and a triangle elements
	static uint32 GetCountersUintSize()       { return 4; }
	static uint32 GetPackedLineUintSize()     { return 8; }
	static uint32 GetPackedTriangleUintSize() { return 12; }
	static uint32 GetPackedSymbolUintSize()   { return 4; }

	// Get symbol buffer size
	// This is some multiple of the value buffer size to allow for maximum value->symbol expansion
	static uint32 GetMaxSymbolCountFromValueCount(uint32 MaxValueCount)
	{
		return MaxValueCount * 12u;
	}

	static bool IsDrawLocked()
	{
		return CVarDrawLock.GetValueOnRenderThread() > 0;
	}

	// Empty buffer for binding when ShaderPrint is disabled
	class FEmptyBuffer : public FBufferWithRDG
	{
	public:
		void InitRHI() override
		{
			Buffer = AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredDesc(4, GetCountersUintSize()), TEXT("ShaderPrint.EmptyValueBuffer"));
		}
	};

	FBufferWithRDG* GEmptyBuffer = new TGlobalResource<FEmptyBuffer>();

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Uniform buffer
	
	// ShaderPrint uniform buffer
	IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FShaderPrintCommonParameters, "ShaderPrintData");
	
	// Fill the uniform buffer parameters
	void GetParameters(FShaderPrintSetup const& InSetup, FShaderPrintCommonParameters& OutParameters)
	{
		const FVector2D ViewSize(FMath::Max(InSetup.ViewRect.Size().X, 1), FMath::Max(InSetup.ViewRect.Size().Y, 1));
		const float FontWidth = float(InSetup.FontSize.X) * InSetup.DPIScale / ViewSize.X;
		const float FontHeight = float(InSetup.FontSize.Y) * InSetup.DPIScale / ViewSize.Y;
		const float SpaceWidth = float(InSetup.FontSpacing.X) * InSetup.DPIScale / ViewSize.X;
		const float SpaceHeight = float(InSetup.FontSpacing.Y) * InSetup.DPIScale / ViewSize.Y;

		OutParameters.FontSize = FVector2f(FontWidth, FontHeight);
		OutParameters.FontSpacing = FVector2f(FontWidth + SpaceWidth, FontHeight + SpaceHeight);
		OutParameters.Resolution = InSetup.ViewRect.Size();
		OutParameters.CursorCoord = InSetup.CursorCoord;
		OutParameters.MaxValueCount = InSetup.MaxValueCount;
		OutParameters.MaxSymbolCount = GetMaxSymbolCountFromValueCount(InSetup.MaxValueCount);
		OutParameters.MaxStateCount = InSetup.MaxStateCount;
		OutParameters.MaxLineCount = InSetup.MaxLineCount;
		OutParameters.MaxTriangleCount = InSetup.MaxTriangleCount;
		OutParameters.TranslatedWorldOffset = FVector3f(InSetup.PreViewTranslation);
	}

	// Return a uniform buffer with values filled and with single frame lifetime
	static TUniformBufferRef<ShaderPrint::FShaderPrintCommonParameters> CreateUniformBuffer(const FShaderPrintSetup& InSetup)
	{		
		FShaderPrintCommonParameters Parameters;
		GetParameters(InSetup, Parameters);

		return TUniformBufferRef<ShaderPrint::FShaderPrintCommonParameters>::CreateUniformBufferImmediate(Parameters, UniformBuffer_SingleFrame);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Accessors

	// Fill the FShaderParameters parameters
	void SetParameters(FRDGBuilder& GraphBuilder, const FShaderPrintData& InData, FShaderParameters& OutParameters)
	{
		OutParameters.Common = InData.UniformBuffer;
		OutParameters.ShaderPrint_StateBuffer = GraphBuilder.CreateSRV(InData.ShaderPrintStateBuffer);
		OutParameters.ShaderPrint_RWEntryBuffer = GraphBuilder.CreateUAV(InData.ShaderPrintEntryBuffer);
	}

	void SetParameters(FRDGBuilder& GraphBuilder, FShaderParameters& OutParameters)
	{
		if (ensure(GDefaultView != nullptr))
		{
			SetParameters(GraphBuilder, GDefaultView->ShaderPrintData, OutParameters);
		}
	}

	void SetParameters(FRDGBuilder& GraphBuilder, const FViewInfo & View, FShaderParameters& OutParameters)
	{
		SetParameters(GraphBuilder, View.ShaderPrintData, OutParameters);
	}

	bool IsSupported(EShaderPlatform InShaderPlatform)
	{
		return !IsMobilePlatform(InShaderPlatform) && !IsHlslccShaderPlatform(InShaderPlatform);
	}

	bool IsEnabled()
	{
		return GEnabled != 0;
	}

	void SetEnabled(bool bInEnabled)
	{
		GEnabled = bInEnabled ? 1 : 0;
	}

	bool IsValid(FShaderPrintData const& InShaderPrintData)
	{
		// Assume that if UniformBuffer is valid then all other buffers are.
		return InShaderPrintData.UniformBuffer.IsValid();
	}

	bool IsEnabled(FShaderPrintData const& InShaderPrintData)
	{
		return InShaderPrintData.Setup.bEnabled;
	}

	bool IsDefaultViewValid()
	{
		return GDefaultView != nullptr && IsValid(GDefaultView->ShaderPrintData);
	}

	bool IsDefaultViewEnabled()
	{
		return GDefaultView != nullptr && IsEnabled(GDefaultView->ShaderPrintData);
	}

	void RequestSpaceForCharacters(uint32 InCount)
	{
		GCharacterRequestCount += InCount;
	}

	void RequestSpaceForLines(uint32 InCount)
	{
		GLineRequestCount += InCount;
	}

	void RequestSpaceForTriangles(uint32 InCount)
	{
		GTriangleRequestCount += InCount;
	}

	void SubmitShaderPrintData(FFrozenShaderPrintData& InData)
	{
		GShaderPrintDataToRender.Add(InData);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Widget/Characters Shaders
	 
	// Shader to initialize the output value buffer
	class FShaderPrintClearCounterCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FShaderPrintClearCounterCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderPrintClearCounterCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWValuesBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderPrintClearCounterCS, "/Engine/Private/ShaderPrintDraw.usf", "ClearCounterCS", SF_Compute);

	// Shader to fill the indirect parameter arguments ready for the value->symbol compute pass
	class FShaderBuildIndirectDispatchArgsCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FShaderBuildIndirectDispatchArgsCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderBuildIndirectDispatchArgsCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ValuesBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWSymbolsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectDispatchArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderBuildIndirectDispatchArgsCS, "/Engine/Private/ShaderPrintDraw.usf", "BuildIndirectDispatchArgsCS", SF_Compute);

	// Shader to clean & compact widget state
	class FShaderCompactStateBufferCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FShaderCompactStateBufferCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderCompactStateBufferCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, FrameIndex)
			SHADER_PARAMETER(uint32, FrameThreshold)
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWStateBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderCompactStateBufferCS, "/Engine/Private/ShaderPrintDraw.usf", "CompactStateBufferCS", SF_Compute);

	// Shader to read the values buffer and convert to the symbols buffer
	class FShaderBuildSymbolBufferCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FShaderBuildSymbolBufferCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderBuildSymbolBufferCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(uint32, FrameIndex)
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ValuesBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWSymbolsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWStateBuffer)
			RDG_BUFFER_ACCESS(IndirectDispatchArgsBuffer, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderBuildSymbolBufferCS, "/Engine/Private/ShaderPrintDraw.usf", "BuildSymbolBufferCS", SF_Compute);

	// Shader to fill the indirect parameter arguments ready for draw pass
	class FShaderBuildIndirectDrawArgsCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FShaderBuildIndirectDrawArgsCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderBuildIndirectDrawArgsCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SymbolsBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectDrawArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderBuildIndirectDrawArgsCS, "/Engine/Private/ShaderPrintDraw.usf", "BuildIndirectDrawArgsCS", SF_Compute);

	// Shader for draw pass to render each symbol
	class FShaderDrawSymbols : public FGlobalShader
	{
	public:
		FShaderDrawSymbols()
		{}

		FShaderDrawSymbols(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
			: FGlobalShader(Initializer)
		{}

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			RENDER_TARGET_BINDING_SLOTS()
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER_TEXTURE(Texture2D, MiniFontTexture)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SymbolsBuffer)
			RDG_BUFFER_ACCESS(IndirectDrawArgsBuffer, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(FGlobalShaderPermutationParameters const& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}
	};

	class FShaderDrawSymbolsVS : public FShaderDrawSymbols
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawSymbolsVS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawSymbolsVS, FShaderDrawSymbols);
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawSymbolsVS, "/Engine/Private/ShaderPrintDraw.usf", "DrawSymbolsVS", SF_Vertex);

	class FShaderDrawSymbolsPS : public FShaderDrawSymbols
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawSymbolsPS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawSymbolsPS, FShaderDrawSymbols);
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawSymbolsPS, "/Engine/Private/ShaderPrintDraw.usf", "DrawSymbolsPS", SF_Pixel);

	//////////////////////////////////////////////////////////////////////////
	// Line Shaders

	class FShaderDrawDebugCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugCopyCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, ElementBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, RWIndirectArgs)
			SHADER_PARAMETER(uint32, PrimitiveType)
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, ShaderPrintData)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_COPY_CS"), 1);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugCopyCS, "/Engine/Private/ShaderPrintDrawPrimitive.usf", "ShaderDrawDebugCopyCS", SF_Compute);

	class FShaderDrawDebugVS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugVS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugVS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_REF(FShaderPrintCommonParameters, Common)
			SHADER_PARAMETER(FVector3f, TranslatedWorldOffsetConversion)
			SHADER_PARAMETER(FMatrix44f, TranslatedWorldToClip)
			SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
			SHADER_PARAMETER_SRV(StructuredBuffer, LockedShaderDrawDebugPrimitive)
			SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, ShaderDrawDebugPrimitive)
			RDG_BUFFER_ACCESS(IndirectBuffer, ERHIAccess::IndirectArgs)
		END_SHADER_PARAMETER_STRUCT()

		class FPrimitiveType : SHADER_PERMUTATION_INT("PERMUTATION_PRIMITIVE_TYPE", 2);
		using FPermutationDomain = TShaderPermutationDomain<FPrimitiveType>;

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			FPermutationDomain PermutationVector(Parameters.PermutationId);
			if (PermutationVector.Get<FPrimitiveType>() == 0)
			{
				OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_LINE_VS"), 1);
			}
			else
			{
				OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_TRIANGLE_VS"), 1);
			}
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugVS, "/Engine/Private/ShaderPrintDrawPrimitive.usf", "ShaderDrawDebugVS", SF_Vertex);

	class FShaderDrawDebugPS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FShaderDrawDebugPS);
		SHADER_USE_PARAMETER_STRUCT(FShaderDrawDebugPS, FGlobalShader);
		
		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FVector2f, OutputInvResolution)
			SHADER_PARAMETER(FVector2f, OriginalViewRectMin)
			SHADER_PARAMETER(FVector2f, OriginalViewSize)
			SHADER_PARAMETER(FVector2f, OriginalBufferInvSize)
			SHADER_PARAMETER(uint32, bCheckerboardEnabled)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
			RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		using FPermutationDomain = TShaderPermutationDomain<>;

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsSupported(Parameters.Platform);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING"), 1);
			OutEnvironment.SetDefine(TEXT("GPU_DEBUG_RENDERING_PS"), 1);
		}
	};

	IMPLEMENT_GLOBAL_SHADER(FShaderDrawDebugPS, "/Engine/Private/ShaderPrintDrawPrimitive.usf", "ShaderDrawDebugPS", SF_Pixel);

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderDrawVSPSParameters , )
		SHADER_PARAMETER_STRUCT_INCLUDE(FShaderDrawDebugVS::FParameters, VS)
		SHADER_PARAMETER_STRUCT_INCLUDE(FShaderDrawDebugPS::FParameters, PS)
	END_SHADER_PARAMETER_STRUCT()

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Setup render data

	FShaderPrintSetup::FShaderPrintSetup(FIntRect InViewRect)
	{
		bEnabled = IsEnabled();

		ViewRect = InViewRect;

		FontSize = FIntPoint(FMath::Max(CVarFontSize.GetValueOnAnyThread(), 1), FMath::Max(CVarFontSize.GetValueOnAnyThread(), 1));
		FontSpacing = FIntPoint(FMath::Max(CVarFontSpacingX.GetValueOnAnyThread(), 1), FMath::Max(CVarFontSpacingY.GetValueOnAnyThread(), 1));
		MaxValueCount = bEnabled ? GetMaxValueCount() : 0;
		MaxStateCount = bEnabled ? GetMaxWidgetCount() : 0;
		MaxLineCount = bEnabled ? GetMaxLineCount() : 0;
		MaxTriangleCount = bEnabled ? GetMaxTriangleCount(): 0;
	}

	FShaderPrintSetup::FShaderPrintSetup(FSceneView const& View)
	{
		bEnabled = IsEnabled() && IsSupported(View.GetShaderPlatform()) && View.Family->EngineShowFlags.ShaderPrint;

		ViewRect = View.UnconstrainedViewRect;
		CursorCoord = View.CursorPos;
		PreViewTranslation = View.ViewMatrices.GetPreViewTranslation();
		DPIScale = View.Family->DebugDPIScale;

		FontSize = FIntPoint(FMath::Max(CVarFontSize.GetValueOnAnyThread(), 1), FMath::Max(CVarFontSize.GetValueOnAnyThread(), 1));
		FontSpacing = FIntPoint(FMath::Max(CVarFontSpacingX.GetValueOnAnyThread(), 1), FMath::Max(CVarFontSpacingY.GetValueOnAnyThread(), 1));
		MaxValueCount = bEnabled ? GetMaxValueCount() : 0;
		MaxStateCount = bEnabled ? GetMaxWidgetCount() : 0;
		MaxLineCount = bEnabled ? GetMaxLineCount() : 0;
		MaxTriangleCount = bEnabled ? GetMaxTriangleCount() : 0;
	}

	FShaderPrintData CreateShaderPrintData(FRDGBuilder& GraphBuilder, FShaderPrintSetup const& InSetup, FSceneViewState* InViewState)
	{
		FShaderPrintData ShaderPrintData;

		// Common uniform buffer
		ShaderPrintData.Setup = InSetup;
		ShaderPrintData.UniformBuffer = CreateUniformBuffer(InSetup);

		// Early out if system is disabled.
		// Note that we still bind dummy buffers.
		// This is in case some debug shader code is still active and accessing the buffer.
		if (!InSetup.bEnabled)
		{
			ShaderPrintData.ShaderPrintEntryBuffer = GraphBuilder.RegisterExternalBuffer(GEmptyBuffer->Buffer);
			ShaderPrintData.ShaderPrintStateBuffer = GraphBuilder.RegisterExternalBuffer(GEmptyBuffer->Buffer);
			return ShaderPrintData;
		}

		// Characters/Widgets/Primitives/Lines
		{
			const bool bLockBufferThisFrame = IsDrawLocked() && InViewState != nullptr && !InViewState->ShaderPrintStateData.bIsLocked;
			ERDGBufferFlags Flags = bLockBufferThisFrame ? ERDGBufferFlags::MultiFrame : ERDGBufferFlags::None;

			const uint32 UintElementCount = 
				GetCountersUintSize() + 
				GetPackedSymbolUintSize() * InSetup.MaxValueCount +
				GetPackedLineUintSize() * InSetup.MaxLineCount + 
				GetPackedTriangleUintSize() * InSetup.MaxTriangleCount;
			ShaderPrintData.ShaderPrintEntryBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(4, UintElementCount), TEXT("ShaderPrint.EntryBuffer"), Flags);

			// State buffer is retrieved from the view state, or created if it does not exist
			if (InViewState != nullptr)
			{
				if (InViewState->ShaderPrintStateData.StateBuffer)
				{
					ShaderPrintData.ShaderPrintStateBuffer = GraphBuilder.RegisterExternalBuffer(InViewState->ShaderPrintStateData.StateBuffer);
				}
				else
				{
					ShaderPrintData.ShaderPrintStateBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(4, (3 * InSetup.MaxStateCount) + 1), TEXT("ShaderPrint.StateBuffer"));
					AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShaderPrintData.ShaderPrintStateBuffer, PF_R32_UINT), 0u);
					InViewState->ShaderPrintStateData.StateBuffer = GraphBuilder.ConvertToExternalBuffer(ShaderPrintData.ShaderPrintStateBuffer);
				}
			}
			else
			{
				ShaderPrintData.ShaderPrintStateBuffer = GraphBuilder.RegisterExternalBuffer(GEmptyBuffer->Buffer);
			}

			// Clear counters
			{
				FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				TShaderMapRef<FShaderPrintClearCounterCS> ComputeShader(GlobalShaderMap);

				FShaderPrintClearCounterCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShaderPrintClearCounterCS::FParameters>();
				PassParameters->RWValuesBuffer = GraphBuilder.CreateUAV(ShaderPrintData.ShaderPrintEntryBuffer);
				ClearUnusedGraphResources(ComputeShader, PassParameters);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ShaderPrint::ClearCounters"),
					PassParameters,
					ERDGPassFlags::Compute,
					[PassParameters, ComputeShader](FRHICommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, FIntVector(1, 1, 1));
					});
			}

			if (InViewState != nullptr)
			{
				if (IsDrawLocked() && !InViewState->ShaderPrintStateData.bIsLocked)
				{
					InViewState->ShaderPrintStateData.EntryBuffer = GraphBuilder.ConvertToExternalBuffer(ShaderPrintData.ShaderPrintEntryBuffer);
					InViewState->ShaderPrintStateData.PreViewTranslation = InSetup.PreViewTranslation;
					InViewState->ShaderPrintStateData.bIsLocked = true;
				}

				if (!IsDrawLocked() && InViewState->ShaderPrintStateData.bIsLocked)
				{
					InViewState->ShaderPrintStateData.EntryBuffer = nullptr;
					InViewState->ShaderPrintStateData.PreViewTranslation = FVector::ZeroVector;
					InViewState->ShaderPrintStateData.bIsLocked = false;
				}
			}
		}

		return ShaderPrintData;
	}

	FShaderPrintData CreateShaderPrintData(FRDGBuilder& GraphBuilder, FShaderPrintSetup const& InSetup)
	{
		return CreateShaderPrintData(GraphBuilder, InSetup, nullptr);
	}

	FFrozenShaderPrintData FreezeShaderPrintData(FRDGBuilder& GraphBuilder, FShaderPrintData& ShaderPrintData)
	{
		FFrozenShaderPrintData Out;

		Out.Setup = ShaderPrintData.Setup;

		Out.ShaderPrintEntryBuffer = GraphBuilder.ConvertToExternalBuffer(ShaderPrintData.ShaderPrintEntryBuffer);
		Out.ShaderPrintStateBuffer = GraphBuilder.ConvertToExternalBuffer(ShaderPrintData.ShaderPrintStateBuffer);

		return Out;
	}

	FShaderPrintData UnFreezeShaderPrintData(FRDGBuilder& GraphBuilder, FFrozenShaderPrintData& FrozenShaderPrintData)
	{
		FShaderPrintData Out;

		Out.Setup = FrozenShaderPrintData.Setup;
		Out.UniformBuffer = CreateUniformBuffer(Out.Setup);

		Out.ShaderPrintEntryBuffer = GraphBuilder.RegisterExternalBuffer(FrozenShaderPrintData.ShaderPrintEntryBuffer);
		Out.ShaderPrintStateBuffer = GraphBuilder.RegisterExternalBuffer(FrozenShaderPrintData.ShaderPrintStateBuffer);

		return Out;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////
	// Drawing/Rendering API

	void BeginView(FRDGBuilder& GraphBuilder, FViewInfo& View)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ShaderPrint::BeginView);

		if (!IsSupported(View.GetShaderPlatform()))
		{
			View.ShaderPrintData = FShaderPrintData();
			return;
		}

		// Invalid to call begin twice for the same view.
		ensure(GDefaultView != &View);
		if (GDefaultView == nullptr)
		{
			GDefaultView = &View;
		}

		// Create the render data and store on the view.
		FShaderPrintSetup ShaderPrintSetup(View);
		View.ShaderPrintData = CreateShaderPrintData(GraphBuilder, ShaderPrintSetup, View.ViewState);

		// Reset counter which is read on the next BeginView().
		GCharacterRequestCount = 0;
		GWidgetRequestCount = 0;
		GLineRequestCount = 0;
		GTriangleRequestCount = 0;
	}

	static void InternalDrawView_Characters(
		FRDGBuilder& GraphBuilder, 
		FShaderPrintData const& ShaderPrintData, 
		FIntRect ViewRect, 
		int32 FrameNumber, 
		FScreenPassTexture OutputTexture)
	{
		// Initialize graph managed resources
		const uint32 UintElementCount = GetCountersUintSize() + GetPackedSymbolUintSize() * GetMaxSymbolCountFromValueCount(ShaderPrintData.Setup.MaxValueCount);
		FRDGBufferRef SymbolBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(4, UintElementCount), TEXT("ShaderPrint.SymbolBuffer"));
		FRDGBufferRef IndirectDispatchArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("ShaderPrint.IndirectDispatchArgs"));
		FRDGBufferRef IndirectDrawArgsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(5), TEXT("ShaderPrint.IndirectDrawArgs"));

		// Non graph managed resources
		FRDGBufferSRVRef ValueBuffer = GraphBuilder.CreateSRV(ShaderPrintData.ShaderPrintEntryBuffer);
		FRDGBufferSRVRef StateBuffer = GraphBuilder.CreateSRV(ShaderPrintData.ShaderPrintStateBuffer);
		FTextureRHIRef FontTexture = GSystemTextures.AsciiTexture->GetRHI();

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		// BuildIndirectDispatchArgs
		{
			typedef FShaderBuildIndirectDispatchArgsCS SHADER;
			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->Common = ShaderPrintData.UniformBuffer;
			PassParameters->ValuesBuffer = ValueBuffer;
			PassParameters->RWSymbolsBuffer = GraphBuilder.CreateUAV(SymbolBuffer);
			PassParameters->RWIndirectDispatchArgsBuffer = GraphBuilder.CreateUAV(IndirectDispatchArgsBuffer, EPixelFormat::PF_R32_UINT);

			FComputeShaderUtils::AddPass(
				GraphBuilder, 
				RDG_EVENT_NAME("ShaderPrint::BuildIndirectDispatchArgs"), 
				ComputeShader, PassParameters,
				FIntVector(1, 1, 1));
		}

		// BuildSymbolBuffer
		{
			typedef FShaderBuildSymbolBufferCS SHADER;
			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->FrameIndex = FrameNumber;
			PassParameters->Common = ShaderPrintData.UniformBuffer;
			PassParameters->ValuesBuffer = ValueBuffer;
			PassParameters->RWSymbolsBuffer = GraphBuilder.CreateUAV(SymbolBuffer);
			PassParameters->RWStateBuffer = GraphBuilder.CreateUAV(ShaderPrintData.ShaderPrintStateBuffer);
			PassParameters->IndirectDispatchArgsBuffer = IndirectDispatchArgsBuffer;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShaderPrint::BuildSymbolBuffer"),
				ComputeShader, PassParameters,
				IndirectDispatchArgsBuffer, 0);
		}

		// CompactStateBuffer
		#if 0
		{
			typedef FShaderCompactStateBufferCS SHADER;
			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->FrameIndex = FrameNumber;
			PassParameters->FrameThreshold = 300u;
			PassParameters->Common = ShaderPrintData.UniformBuffer;
			PassParameters->RWStateBuffer = GraphBuilder.CreateUAV(ShaderPrintData.ShaderPrintStateBuffer);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShaderPrint::CompactStateBuffer"),
				ComputeShader, PassParameters, FIntVector(1,1,1));
		}
		#endif

		// BuildIndirectDrawArgs
		{
			typedef FShaderBuildIndirectDrawArgsCS SHADER;
			TShaderMapRef<SHADER> ComputeShader(GlobalShaderMap);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->Common = ShaderPrintData.UniformBuffer;
			PassParameters->SymbolsBuffer = GraphBuilder.CreateSRV(SymbolBuffer);
			PassParameters->RWIndirectDrawArgsBuffer = GraphBuilder.CreateUAV(IndirectDrawArgsBuffer, EPixelFormat::PF_R32_UINT);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShaderPrint::BuildIndirectDrawArgs"),
				ComputeShader, PassParameters,
				FIntVector(1, 1, 1));
		}

		// DrawSymbols
		{
			typedef FShaderDrawSymbols SHADER;
			TShaderMapRef< FShaderDrawSymbolsVS > VertexShader(GlobalShaderMap);
			TShaderMapRef< FShaderDrawSymbolsPS > PixelShader(GlobalShaderMap);

			SHADER::FParameters* PassParameters = GraphBuilder.AllocParameters<SHADER::FParameters>();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture.Texture, ERenderTargetLoadAction::ELoad);
			PassParameters->Common = ShaderPrintData.UniformBuffer;
			PassParameters->MiniFontTexture = FontTexture;
			PassParameters->SymbolsBuffer = GraphBuilder.CreateSRV(SymbolBuffer);
			PassParameters->IndirectDrawArgsBuffer = IndirectDrawArgsBuffer;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShaderPrint::DrawSymbols"),
				PassParameters,
				ERDGPassFlags::Raster,
				[VertexShader, PixelShader, PassParameters, ViewRect](FRHICommandList& RHICmdList)
			{
				
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

				RHICmdList.DrawIndexedPrimitiveIndirect(GTwoTrianglesIndexBuffer.IndexBufferRHI, PassParameters->IndirectDrawArgsBuffer->GetIndirectRHICallBuffer(), 0);
			});
		}
	}

	static void InternalDrawView_Primitives(
		FRDGBuilder& GraphBuilder,
		const FShaderPrintData& ShaderPrintData,
		FRDGBufferRef ShaderPrintPrimitiveBuffer,
		const FIntRect& ViewRect,
		const FIntRect& UnscaledViewRect,
		const FMatrix & TranslatedWorldToClip,
		const FVector& TranslatedWorldOffsetConversion,
		const bool bLines,
		const bool bLocked,
		FRDGTextureRef OutputTexture,
		FRDGTextureRef DepthTexture)
	{
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		FRDGBufferRef IndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(1), TEXT("ShaderDraw.IndirectBuffer"), ERDGBufferFlags::None);
		{
			FShaderDrawDebugCopyCS::FParameters* Parameters = GraphBuilder.AllocParameters<FShaderDrawDebugCopyCS::FParameters>();
			Parameters->ElementBuffer = GraphBuilder.CreateSRV(ShaderPrintPrimitiveBuffer);
			Parameters->RWIndirectArgs = GraphBuilder.CreateUAV(IndirectBuffer, PF_R32_UINT);
			Parameters->ShaderPrintData = ShaderPrintData.UniformBuffer;
			Parameters->PrimitiveType = bLines ? 0u : 1u;

			TShaderMapRef<FShaderDrawDebugCopyCS> ComputeShader(GlobalShaderMap);
			ClearUnusedGraphResources(ComputeShader, Parameters);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShaderPrint::CopyLineArgs(%s%s)", bLines ? TEXT("Lines") : TEXT("Triangles"), bLocked ? TEXT(",Locked") : TEXT("")),
				Parameters,
				ERDGPassFlags::Compute,
				[Parameters, ComputeShader](FRHICommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, FIntVector(1, 1, 1));
				});
		}

		FShaderDrawDebugVS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShaderDrawDebugVS::FPrimitiveType>(bLines ? 0u : 1u);
		TShaderMapRef<FShaderDrawDebugVS> VertexShader(GlobalShaderMap, PermutationVector);
		TShaderMapRef<FShaderDrawDebugPS> PixelShader(GlobalShaderMap);

		// Create a transient depth texture which allows to depth test filled primitive between themselves. These primitives are not culled against the scene depth texture, but only 'checkerboarded'.
		FRDGTextureRef TransientDepthTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(OutputTexture->Desc.Extent, PF_DepthStencil, FClearValueBinding::DepthFar, TexCreate_DepthStencilTargetable | TexCreate_ShaderResource), TEXT("ShaderPrint.DepthTexture"));

		FShaderDrawVSPSParameters* PassParameters = GraphBuilder.AllocParameters<FShaderDrawVSPSParameters >();
		PassParameters->VS.TranslatedWorldOffsetConversion = FVector3f(TranslatedWorldOffsetConversion);
		PassParameters->VS.TranslatedWorldToClip = FMatrix44f(TranslatedWorldToClip);
		PassParameters->PS.RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->PS.RenderTargets.DepthStencil = FDepthStencilBinding(TransientDepthTexture, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop);
		PassParameters->PS.OutputInvResolution = FVector2f(1.f / UnscaledViewRect.Width(), 1.f / UnscaledViewRect.Height());
		PassParameters->PS.OriginalViewRectMin = FVector2f(ViewRect.Min);
		PassParameters->PS.OriginalViewSize = FVector2f(ViewRect.Width(), ViewRect.Height());
		PassParameters->PS.OriginalBufferInvSize = FVector2f(1.f / DepthTexture->Desc.Extent.X, 1.f / DepthTexture->Desc.Extent.Y);
		PassParameters->PS.DepthTexture = DepthTexture;
		PassParameters->PS.DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->PS.bCheckerboardEnabled = bLines ? 1u : 0u;
		PassParameters->VS.ShaderDrawDebugPrimitive = GraphBuilder.CreateSRV(ShaderPrintPrimitiveBuffer);
		PassParameters->VS.IndirectBuffer = IndirectBuffer;
		PassParameters->VS.Common = ShaderPrintData.UniformBuffer;

		ValidateShaderParameters(PixelShader, PassParameters->PS);
		ClearUnusedGraphResources(PixelShader, &PassParameters->PS, { IndirectBuffer });
		ValidateShaderParameters(VertexShader, PassParameters->VS);
		ClearUnusedGraphResources(VertexShader, &PassParameters->VS, { IndirectBuffer });

		const FIntRect Viewport = UnscaledViewRect;
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShaderPrint::Draw(%s%s)", bLines ? TEXT("Lines") : TEXT("Triangles"), bLocked ? TEXT(",Locked") : TEXT("")),
			PassParameters,
			ERDGPassFlags::Raster,
			[VertexShader, PixelShader, PassParameters, IndirectBuffer, Viewport, bLines](FRHICommandList& RHICmdList)
			{
				// Marks the indirect draw parameter as used by the pass, given it's not used directly by any of the shaders.
				PassParameters->VS.IndirectBuffer->MarkResourceAsUsed();

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI(); // Premultiplied-alpha composition
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, true>::GetRHI();
				GraphicsPSOInit.PrimitiveType = bLines ? PT_LineList : PT_TriangleList;
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				RHICmdList.SetViewport(Viewport.Min.X, Viewport.Min.Y, 0.0f, Viewport.Max.X, Viewport.Max.Y, 1.0f);
				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

				// Marks the indirect draw parameter as used by the pass, given it's not used directly by any of the shaders.
				FRHIBuffer* IndirectBufferRHI = PassParameters->VS.IndirectBuffer->GetIndirectRHICallBuffer();
				check(IndirectBufferRHI != nullptr);
				RHICmdList.DrawPrimitiveIndirect(IndirectBufferRHI, 0);
			});
	}

	void InternalDrawView(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FShaderPrintData& ShaderPrintData, const FScreenPassTexture& OutputTexture, const FScreenPassTexture& DepthTexture)
	{
		if (!ensure(OutputTexture.IsValid()))
		{
			return;
		}

		RDG_EVENT_SCOPE(GraphBuilder, "ShaderPrint::DrawView");

		const FIntRect SourceViewRect = View.ViewRect;
		const FIntRect OutputViewRect = OutputTexture.ViewRect;
		const FVector PreViewTranslation = View.ViewMatrices.GetPreViewTranslation() - ShaderPrintData.Setup.PreViewTranslation;

		// Lines
		{
			FRDGBufferRef DataBuffer = ShaderPrintData.ShaderPrintEntryBuffer;
			InternalDrawView_Primitives(GraphBuilder, ShaderPrintData, DataBuffer, SourceViewRect, OutputViewRect, View.ViewMatrices.GetTranslatedViewProjectionMatrix(), PreViewTranslation, true /*bLines*/, false /*bLocked*/, OutputTexture.Texture, DepthTexture.Texture);
		}

		// Triangles
		{
			FRDGBufferRef DataBuffer = ShaderPrintData.ShaderPrintEntryBuffer;
			InternalDrawView_Primitives(GraphBuilder, ShaderPrintData, DataBuffer, SourceViewRect, OutputViewRect, View.ViewMatrices.GetTranslatedViewProjectionMatrix(), PreViewTranslation, false /*bLines*/, false /*bLocked*/, OutputTexture.Texture, DepthTexture.Texture);
		}

		// Locked Lines/Triangles
		if (View.ViewState && View.ViewState->ShaderPrintStateData.bIsLocked)
		{
			const FVector LockedPreViewTranslation = View.ViewMatrices.GetPreViewTranslation() - View.ViewState->ShaderPrintStateData.PreViewTranslation;
			FRDGBufferRef DataBuffer = GraphBuilder.RegisterExternalBuffer(View.ViewState->ShaderPrintStateData.EntryBuffer);
			InternalDrawView_Primitives(GraphBuilder, ShaderPrintData, DataBuffer, SourceViewRect, OutputViewRect, View.ViewMatrices.GetTranslatedViewProjectionMatrix(), LockedPreViewTranslation, true  /*bLines*/, true/*bLocked*/, OutputTexture.Texture, DepthTexture.Texture);
			InternalDrawView_Primitives(GraphBuilder, ShaderPrintData, DataBuffer, SourceViewRect, OutputViewRect, View.ViewMatrices.GetTranslatedViewProjectionMatrix(), LockedPreViewTranslation, false /*bLines*/, true/*bLocked*/, OutputTexture.Texture, DepthTexture.Texture);
		}

		// Characters
		{
			const int32 FrameNumber = View.Family ? View.Family->FrameNumber : 0u;
			InternalDrawView_Characters(GraphBuilder, ShaderPrintData, OutputViewRect, FrameNumber, OutputTexture);
		}
	}

	void DrawView(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FScreenPassTexture& OutputTexture, const FScreenPassTexture& DepthTexture)
	{
		// Draw the shader print data for the view.
		InternalDrawView(GraphBuilder, View, View.ShaderPrintData, OutputTexture, DepthTexture);

		// Draw any externally enqueued shader print data.
		for (FFrozenShaderPrintData& ShaderPrintDataToRender : GShaderPrintDataToRender)
		{
			FShaderPrintData ShaderPrintData = UnFreezeShaderPrintData(GraphBuilder, ShaderPrintDataToRender);
			InternalDrawView(GraphBuilder, View, ShaderPrintData, OutputTexture, DepthTexture);
		}
	}

	void EndView(FViewInfo& View)
	{
		View.ShaderPrintData = FShaderPrintData();
		GDefaultView = nullptr;
		GShaderPrintDataToRender.Reset();
	}
}