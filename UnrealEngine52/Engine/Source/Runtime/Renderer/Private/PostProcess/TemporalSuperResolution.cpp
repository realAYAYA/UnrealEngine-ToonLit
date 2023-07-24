// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/TemporalAA.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PostProcess/PostProcessing.h"
#include "SceneTextureParameters.h"
#include "PixelShaderUtils.h"
#include "ScenePrivate.h"
#include "RendererModule.h"

#define COMPILE_TSR_DEBUG_PASSES (!UE_BUILD_SHIPPING)

namespace
{

TAutoConsoleVariable<float> CVarTSRHistorySampleCount(
	TEXT("r.TSR.History.SampleCount"), 16.0f,
	TEXT("Maximum number sample for each output pixel in the history. Higher values means more stability on highlights on static images, ")
	TEXT("but may introduce additional ghosting on firefliers style of VFX. Minimum value supported is 8.0 as TSR was in 5.0 and 5.1. ")
	TEXT("Maximum value possible due to the encoding of the TSR.History.Metadata is 32.0. Defaults to 16.0."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRHistorySP(
	TEXT("r.TSR.History.ScreenPercentage"), 100.0f,
	TEXT("Resolution multiplier of the history of TSR based of output resolution. While increasing the resolution adds runtime cost ")
	TEXT("to TSR, it allows to maintain a better sharpness and stability of the details stored in history through out the reprojection.\n")
	TEXT("\n")
	TEXT("Setting to 200 brings on a very particular property relying on NyQuist-Shannon sampling theorem that establishes a sufficient ")
	TEXT("condition for the sample rate of the accumulated details in the history. As a result only values between 100 and 200 are supported.\n")
	TEXT("It is controlled by default in the anti-aliasing scalability group set to 200 on Epic and Cinematic, 100 otherwise."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRR11G11B10History(
	TEXT("r.TSR.History.R11G11B10"), 1,
	TEXT("Select the bitdepth of the history. r.TSR.History.R11G11B10=1 Saves memory bandwidth that is of particular interest of the TSR's ")
	TEXT("UpdateHistory's runtime performance by saving memory both at previous frame's history reprojection and write out of the output and ")
	TEXT("new history.\n")
	TEXT("This optimisation is unsupported with r.PostProcessing.PropagateAlpha=1.\n")
	TEXT("\n")
	TEXT("Please also not that increasing r.TSR.History.ScreenPercentage=200 adds 2 additional implicit encoding bits in the history compared to the TSR.Output's bitdepth thanks to the downscaling pass from TSR history resolution to TSR output resolution."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRHistoryUpdateQuality(
	TEXT("r.TSR.History.UpdateQuality"), 3,
	TEXT("Selects shader permutation of the quality of the update of the history in the TSR HistoryUpdate pass currently driven by the sg.AntiAliasingQuality scalability group. ")
	TEXT("For further details about what each offers, you are invited to look at DIM_UPDATE_QUALITY in TSRUpdateHistory.usf and customise to your need."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRHistorySeparateTranslucency(
	TEXT("r.TSR.History.SeparateTranslucency"), 0,
	TEXT("Whether separate translucency should be accumulated separatly."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRHistoryGrandReprojection(
	TEXT("r.TSR.History.GrandReprojection"), 1,
	TEXT("Experimental functionality to keep higher sharpness in TSR's history in motion."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRWaveOps(
	TEXT("r.TSR.WaveOps"), 1,
	TEXT("Whether to use wave ops in the shading rejection heuristics to speeds up convolutions.\n")
	TEXT("\n")
	TEXT("The shading rejection heuristic optimisation can be particularily hard for shader compiler and hit bug in them causing corruption/quality loss.\n")
	TEXT("\n")
	TEXT("Note this optimisation is currently disabled on SPIRV platforms (mainly Vulkan and Metal) due to 5min+ compilation times in SPIRV ")
	TEXT("backend of DXC which is not great for editor startup."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRHistoryRejectionSampleCount(
	TEXT("r.TSR.ShadingRejection.SampleCount"), 2.0f,
	TEXT("Maximum number of sample in each output pixel of the history after total shading rejection.\n")
	TEXT("\n")
	TEXT("Lower values means higher clarity of the image after shading rejection of the history, but at the trade of higher instability ")
	TEXT("of the pixel on following frames accumulating new details which can be distracting to the human eye (Defaults to 2.0)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRFlickeringEnable(
	TEXT("r.TSR.ShadingRejection.Flickering"), 1,
	TEXT("Instability in TSR output 99% of the time coming from instability of the shading rejection, for different reasons:\n")
	TEXT(" - One first source of instability is most famously moire pattern between structured geometry and the rendering pixel grid changing ")
	TEXT("every frame due to the offset of the jittering pixel grid offset;\n")
	TEXT(" - Another source of instability can happen on extrem geometric complexity due to temporal history's chicken-and-egg problem that can ")
	TEXT("not be overcome by other mechanisms in place in TSR's RejectHistory pass: ")
	TEXT("how can the history be identical to rendered frame if the amount of details you have in the rendered frame is not in history? ")
	TEXT("how can the history accumulate details if the history is too different from the rendered frame?\n")
	TEXT("\n")
	TEXT("When enabled, this heuristic monitor how the luminance of the scene right before any translucency drawing stored in the ")
	TEXT("TSR.Moire.Luma resource how it involves over successive frames. And if it is detected to constantly flicker regularily above a certain ")
	TEXT("threshold defined with this r.TSR.ShadingRejection.Flickering.* cvars, the heuristic attempts to stabilize the image by letting ghost within ")
	TEXT("luminance boundary tied to the amplititude of flickering.\n")
	TEXT("\n")
	TEXT("One particular caveat of this heuristic is that any opaque geometry with incorrect motion vector can make a pixel look identically flickery ")
	TEXT("quicking this heuristic in and leaving undesired ghosting effects on the said geometry. When that happens, it is highly encourage to ")
	TEXT("verify the motion vector through the VisualizeMotionBlur show flag and how these motion vectors are able to reproject previous frame ")
	TEXT("with the VisualizeReprojection show flag.\n")
	TEXT("\n")
	TEXT("The variable to countrol the frame frequency at which a pixel is considered flickery and needs to be stabilized with this heuristic is defined ")
	TEXT("with the r.TSR.ShadingRejection.Flickering.Period in frames. For instance, a value r.TSR.ShadingRejection.Flickering.Period=3, it means any ")
	TEXT("pixel that have its luminance changing of variation every more often than every frames is considered flickering.\n")
	TEXT("\n")
	TEXT("However another caveats on this boundary between flickering pixel versus animated pixel is that: flickering ")
	TEXT("happens regardless of frame rate, whereas a visual effects that are/should be based on time and are therefore independent of the frame rate. This mean that ")
	TEXT("a visual effect that looks smooth at 60hz might appear to 'flicker' at lower frame rates, like 24hz for instance.\nTo make sure a visual ")
	TEXT("effect authored by an artists doesn't start to ghost of frame rate, r.TSR.ShadingRejection.Flickering.AdjustToFrameRate is enabled by default ")
	TEXT("such that this frame frequency boundary is automatically when the frame rate drops below a refresh rate below r.TSR.ShadingRejection.Flickering.FrameRateCap.\n")
	TEXT("\n")
	TEXT("While r.TSR.ShadingRejection.Flickering is controled based of scalability settings turn on/off this heuristic on lower/high-end GPU ")
	TEXT("the other r.TSR.ShadingRejection.Flickering.* can be set orthogonally in the Project's DefaultEngine.ini for a consistent behavior ")
	TEXT("across all platforms.\n")
	TEXT("\n")
	TEXT("It is enabled by default in the anti-aliasing scalability group High, Epic and Cinematic."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRFlickeringFrameRateCap(
	TEXT("r.TSR.ShadingRejection.Flickering.FrameRateCap"), 60,
	TEXT("Framerate cap in hertz at which point there is automatic adjustment of r.TSR.ShadingRejection.Flickering.Period when the rendering frame rate is lower. ")
	TEXT("Please read r.TSR.ShadingRejection.Flickering's help for further details. (Default to 60hz)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRFlickeringAdjustToFrameRate(
	TEXT("r.TSR.ShadingRejection.Flickering.AdjustToFrameRate"), 1,
	TEXT("Whether r.TSR.ShadingRejection.Flickering.Period settings should adjust to frame rate when below r.TSR.ShadingRejection.Flickering.FrameRateCap. ")
	TEXT("Please read r.TSR.ShadingRejection.Flickering's help for further details. (Enabled by default)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRFlickeringPeriod(
	TEXT("r.TSR.ShadingRejection.Flickering.Period"), 3.0f,
	TEXT("Periode in frames in which luma oscilations at equal or greater frequency is considered flickering and should ghost to stabilize the image ")
	TEXT("Please read r.TSR.ShadingRejection.Flickering's help for further details. (Default to 3 frames)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRFlickeringMaxParralaxVelocity(
	TEXT("r.TSR.ShadingRejection.Flickering.MaxParallaxVelocity"), 10.0,
	TEXT("Some material might for instance might do something like parallax occlusion mapping such as CitySample's buildings' window's interiors. ")
	TEXT("This often can not render accurately a motion vector of this fake interior geometry and therefore make the heuristic believe it is in fact flickering.\n")
	TEXT("\n")
	TEXT("This variable define the parallax velocity in 1080p pixel at frame rate defined by r.TSR.ShadingRejection.Flickering.FrameRateCap at which point the ")
	TEXT("heuristic should be disabled to not ghost. ")
	TEXT("\n")
	TEXT("(Default to 10 pixels 1080p).\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRRejectionAntiAliasingQuality(
	TEXT("r.TSR.RejectionAntiAliasingQuality"), 3,
	TEXT("Controls the quality of TSR's built-in spatial anti-aliasing technology when the history needs to be rejected. ")
	TEXT("While this may not be critical when the rendering resolution is not much lowered than display resolution, ")
	TEXT("this technic however becomes essential to hide lower rendering resolution rendering because of two reasons:\n")
	TEXT(" - the screen space size of aliasing is inverse proportional to rendering resolution;\n")
	TEXT(" - rendering at lower resolution means need more frame to reach at least 1 rendered pixel per display pixel.")
	TEXT("")
	TEXT("By default, it is only disabled by default in the low anti-aliasing scalability group."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRAsyncCompute(
	TEXT("r.TSR.AsyncCompute"), 0,
	TEXT("Controls how TSR run on async compute. Some TSR passes can overlap with previous passes.\n")
	TEXT(" 0: Disabled (default);\n")
	TEXT(" 1: Run on async compute only passes that are completly independent from any intermediary resource of this frame, namely ClearPrevTextures and ForwardScatterDepth passes;\n")
	TEXT(" 2: Run on async compute only passes that are completly independent or only dependent on the depth and velocity buffer which can overlap for instance with translucency or DOF. Any passes on critical path remains on the graphics queue;\n")
	TEXT(" 3: Run all passes on async compute;"),
	ECVF_RenderThreadSafe);

// TODO: remove this dead code
//TAutoConsoleVariable<float> CVarTSRTranslucencyHighlightLuminance(
//	TEXT("r.TSR.Translucency.HighlightLuminance"), -1.0f,
//	TEXT("Sets the luminance at which translucency is considered an highlights (default=-1.0)."),
//	ECVF_RenderThreadSafe);

// TODO: remove this dead code
//TAutoConsoleVariable<int32> CVarTSRTranslucencyPreviousFrameRejection(
//	TEXT("r.TSR.Translucency.PreviousFrameRejection"), 0,
//	TEXT("Enable heuristic to reject Separate translucency based on previous frame translucency."),
//	ECVF_RenderThreadSafe);

// TODO: currently dead code, that is technically relevent for 5.0's r.TSR.History.SeparateTranslucency=1 but that has too many problems on translucency contour around opaque geometry.
//TAutoConsoleVariable<int32> CVarTSREnableResponiveAA(
//	TEXT("r.TSR.Translucency.EnableResponiveAA"), 1,
//	TEXT("Whether the responsive AA should keep history fully clamped."),
//	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRWeightClampingSampleCount(
	TEXT("r.TSR.Velocity.WeightClampingSampleCount"), 4.0f,
	TEXT("Number of sample to count to in history pixel to clamp history to when output pixel velocity reach r.TSR.Velocity.WeightClampingPixelSpeed. ")
	TEXT("Higher value means higher stability on movement, but at the expense of additional blur due to successive convolution of each history reprojection.\n")
	TEXT("\n")
	TEXT("It is possible to visualize the number of sample in TSR history with the console command `vis TSR.History.Metadata`.\n")
	TEXT("\n")
	TEXT("Please note this clamp the sample count in history pixel, not output pixel, and therefore lower values are by designed less ")
	TEXT("noticeable with higher r.TSR.History.ScreenPercentage. This is done so such that increasing r.TSR.History.ScreenPercentage uniterally & automatically ")
	TEXT("give more temporal stability and maintaining sharpness of the details reprojection at the expense of that extra runtime cost regardless of this setting.\n")
	TEXT("\n")
	TEXT("A story telling game might preferer to keep this 4.0 for a 'cinematic look' whereas a competitive game like Fortnite would preferer to lower that to 2.0. ")
	TEXT("(Default = 4.0f)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRWeightClampingPixelSpeed(
	TEXT("r.TSR.Velocity.WeightClampingPixelSpeed"), 1.0f,
	TEXT("Defines the output pixel velocity at which the the high frequencies of the history get's their contributing weight clamped. ")
	TEXT("It's basically to lerp the effect of r.TSR.Velocity.WeightClampingSampleCount when the pixel velocity get smaller than r.TSR.Velocity.WeightClampingPixelSpeed. ")
	TEXT("(Default = 1.0f)."),
	ECVF_RenderThreadSafe);

// TODO: improve CONFIG_VELOCITY_EXTRAPOLATION in TSRDilateVelcity.usf that is disabled at the moment.
//TAutoConsoleVariable<float> CVarTSRVelocityExtrapolation(
//	TEXT("r.TSR.Velocity.Extrapolation"), 1.0f,
//	TEXT("Defines how much the velocity should be extrapolated on geometric discontinuities (Default = 1.0f)."),
//	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRSubpixelMethod(
	TEXT("r.TSR.Subpixel.Method"), 2,
	TEXT("One particular challenge of Nanite amount of details is that sometimes these details can be thiner than a rendering pixel in which case ")
	TEXT("they only render in some frames. When that happens, it means neither depth or velocity buffer to be able to reproject them. This is for instance ")
	TEXT("visible with the `vis SceneDepthZ` command.")
	TEXT("\n")
	TEXT("This settings control the method to reproject and/or discard the subpixel details.\n")
	TEXT(" 0: disable subpixel details accumulation in history which means all this these subpixel features may ghost; \n")
	TEXT(" 1: accumulate how much subpixel detail can paralax under movement of their background for history rejection which allow minimise ")
	TEXT("amount of ghosting but at the expense of some image stability on these very thin geometry.\n")
	TEXT(" 2: accumulate subpixel details' closest depth to be able to reproject them even when not drawing in depth/velocity buffer which works great for static geometry but not so much for any moving geometry (default)\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRSubpixelDepthMaxAge(
	TEXT("r.TSR.Subpixel.DepthMaxAge"), 3,
	TEXT("Maximum age in frames of subpixel's depth kept in history for their self reprojection (default to 3 frames)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRSubpixelIncludeMovingDepth(
	TEXT("r.TSR.Subpixel.IncludeMovingDepth"), 0,
	TEXT("Whether the depth of moving subpixel detail should also be included in the subpixel depth history for their reprojection. This is a really bad idea to turn this on ")
	TEXT("because it is impossible how a moving object's velocity involves overtime when it's only occasionally drawing its velocity. (disabled by default)."),
	ECVF_RenderThreadSafe);

#if COMPILE_TSR_DEBUG_PASSES

TAutoConsoleVariable<int32> CVarTSRDebugArraySize(
	TEXT("r.TSR.Debug.ArraySize"), 1,
	TEXT("Size of array for the TSR.Debug.* RDG textures"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRSetupDebugPasses(
	TEXT("r.TSR.Debug.SetupExtraPasses"), 0,
	TEXT("Whether to enable the debug passes"),
	ECVF_RenderThreadSafe);

#endif

BEGIN_SHADER_PARAMETER_STRUCT(FTSRCommonParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputInfo)
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, HistoryInfo)

	SHADER_PARAMETER(FIntPoint, InputPixelPosMin)
	SHADER_PARAMETER(FIntPoint, InputPixelPosMax)
	SHADER_PARAMETER(FScreenTransform, InputPixelPosToScreenPos)

	SHADER_PARAMETER(FVector2f, InputJitter)
	SHADER_PARAMETER(int32, bCameraCut)
	SHADER_PARAMETER(FVector2f, ScreenVelocityToInputPixelVelocity)
	SHADER_PARAMETER(FVector2f, InputPixelVelocityToScreenVelocity)

	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRHistoryArrayIndices, )
	SHADER_PARAMETER(int32, HighFrequency)
	SHADER_PARAMETER(int32, Translucency)

	SHADER_PARAMETER(int32, PrevHighFrequency)
	SHADER_PARAMETER(int32, PrevHighFrequencyResultant)
	SHADER_PARAMETER(int32, HighFrequencyOverblur)

	SHADER_PARAMETER(int32, Size)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRHistoryTextures, )
	SHADER_PARAMETER_STRUCT(FTSRHistoryArrayIndices, ArrayIndices)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Output)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, ColorArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Metadata)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SubpixelDetails)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SubpixelDepth)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TranslucencyAlpha)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Guide)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Moire)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRHistorySRVs, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, HighFrequency)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Metadata)

	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PrevHighFrequency)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, PrevHighFrequencyResultant)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, HighFrequencyOverblur)

	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, Translucency)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TranslucencyAlpha)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Guide)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Moire)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRHistoryUAVs, )
	SHADER_PARAMETER_STRUCT(FTSRHistoryArrayIndices, ArrayIndices)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, ColorArray)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, Metadata)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, TranslucencyAlpha)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRPrevHistoryParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevHistoryInfo)
	SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevHistoryBufferUV)
	SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevSubpixelDetails)
	SHADER_PARAMETER(FScreenTransform, ScreenPosToGrandPrevHistoryBufferUV)
	SHADER_PARAMETER(FVector2f, PrevSubpixelDetailsExtent)
	SHADER_PARAMETER(float, HistoryPreExposureCorrection)
END_SHADER_PARAMETER_STRUCT()

enum class ETSRHistoryFormatBits : uint32
{
	None = 0,
	Translucency = 1 << 0,
	Moire = 1 << 1,
	GrandReprojection = 1 << 2,
};
ENUM_CLASS_FLAGS(ETSRHistoryFormatBits);

enum class ETSRSubpixelMethod
{
	Disabled,
	ParallaxFactor,
	ClosestDepth,
};

bool IsOutputDifferentThanHighFrequency(ETSRHistoryFormatBits HistoryFormatBits)
{
	return EnumHasAnyFlags(HistoryFormatBits, ETSRHistoryFormatBits::Translucency);
}

FTSRHistoryArrayIndices TranslateHistoryFormatBitsToArrayIndices(ETSRHistoryFormatBits HistoryFormatBits)
{
	FTSRHistoryArrayIndices ArrayIndices;
	ArrayIndices.Size = 0;
	ArrayIndices.HighFrequency = -1;
	ArrayIndices.Translucency = -1;
	ArrayIndices.PrevHighFrequency = -1;
	ArrayIndices.PrevHighFrequencyResultant = -1;
	ArrayIndices.HighFrequencyOverblur = -1;

	if (IsOutputDifferentThanHighFrequency(HistoryFormatBits))
	{
		ArrayIndices.HighFrequency = ArrayIndices.Size++;
	}

	if (EnumHasAnyFlags(HistoryFormatBits, ETSRHistoryFormatBits::Translucency))
	{
		ArrayIndices.Translucency = ArrayIndices.Size++;
	}

	if (EnumHasAnyFlags(HistoryFormatBits, ETSRHistoryFormatBits::GrandReprojection))
	{
		ArrayIndices.PrevHighFrequency = ArrayIndices.Size++;
		//ArrayIndices.PrevHighFrequencyResultant = ArrayIndices.Size++;
		//ArrayIndices.HighFrequencyOverblur = ArrayIndices.Size++;
	}

	return ArrayIndices;
}

FTSRHistorySRVs CreateSRVs(FRDGBuilder& GraphBuilder, const FTSRHistoryTextures& Textures)
{
	FTSRHistorySRVs SRVs;
	if (Textures.ArrayIndices.HighFrequency >= 0)
	{
		SRVs.HighFrequency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(Textures.ColorArray, Textures.ArrayIndices.HighFrequency));
	}
	else
	{
		SRVs.HighFrequency = GraphBuilder.CreateSRV(Textures.Output);
	}
	SRVs.Metadata = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Textures.Metadata));
	if (Textures.ArrayIndices.Translucency >= 0)
	{
		SRVs.Translucency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(Textures.ColorArray, Textures.ArrayIndices.Translucency));
		SRVs.TranslucencyAlpha = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Textures.TranslucencyAlpha));
	}
	if (Textures.ArrayIndices.PrevHighFrequency >= 0)
	{
		SRVs.PrevHighFrequency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(Textures.ColorArray, Textures.ArrayIndices.PrevHighFrequency));
		//SRVs.PrevHighFrequencyResultant = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(Textures.ColorArray, Textures.ArrayIndices.PrevHighFrequencyResultant));
		//SRVs.HighFrequencyOverblur = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(Textures.ColorArray, Textures.ArrayIndices.HighFrequencyOverblur));
	}

	SRVs.Guide = Textures.Guide;
	SRVs.Moire = Textures.Moire;
	return SRVs;
}

FTSRHistoryUAVs CreateUAVs(FRDGBuilder& GraphBuilder, const FTSRHistoryTextures& Textures)
{
	FTSRHistoryUAVs UAVs;
	UAVs.ArrayIndices = Textures.ArrayIndices;
	if (Textures.ArrayIndices.Size > 0)
	{
		UAVs.ColorArray = GraphBuilder.CreateUAV(Textures.ColorArray);
	}
	UAVs.Metadata = GraphBuilder.CreateUAV(Textures.Metadata);
	if (Textures.TranslucencyAlpha)
	{
		UAVs.TranslucencyAlpha = GraphBuilder.CreateUAV(Textures.TranslucencyAlpha);
	}
	return UAVs;
}

class FTSRShader : public FGlobalShader
{
public:
	static constexpr int32 kSupportMinWaveSize = 32;
	static constexpr int32 kSupportMaxWaveSize = 64;
	
	FTSRShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	FTSRShader()
	{ }

	static ERHIFeatureSupport SupportsWaveOps(EShaderPlatform Platform)
	{
		// UE-161125: SPIRV ends up with 5min+ compilation times, and D3D11 needs SPIRV backend for HLSL2021 features on FXC.
		if (FDataDrivenShaderPlatformInfo::GetIsSPIRV(Platform) || Platform == SP_PCD3D_SM5)
		{
			return ERHIFeatureSupport::Unsupported;
		}

		return FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Platform);
	}

	static bool SupportsLDS(EShaderPlatform Platform)
	{
		// Always support LDS if wave ops are not guarenteed
		if (SupportsWaveOps(Platform) != ERHIFeatureSupport::RuntimeGuaranteed)
		{
			return true;
		}

		// Do not support LDS if shader supported wave size are guarenteed to support the platform.
		if (FDataDrivenShaderPlatformInfo::GetMinimumWaveSize(Platform) >= kSupportMinWaveSize &&
			FDataDrivenShaderPlatformInfo::GetMaximumWaveSize(Platform) <= kSupportMaxWaveSize)
		{
			return false;
		}

		return true;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return SupportsTSR(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Parameters.Platform) == ERHIFeatureSupport::RuntimeGuaranteed)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
	}
}; // class FTemporalSuperResolutionShader

class FTSRComputeMoireLumaCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRComputeMoireLumaCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRComputeMoireLumaCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputInfo)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, MoireLumaOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRComputeMoireLumaCS

class FTSRClearPrevTexturesCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRClearPrevTexturesCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRClearPrevTexturesCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PrevUseCountOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PrevClosestDepthOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SubpixelDepthOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRClearPrevTexturesCS

class FTSRForwardScatterDepthCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRForwardScatterDepthCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRForwardScatterDepthCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FIntPoint, PrevSubpixelDepthViewportMin)
		SHADER_PARAMETER(FIntPoint, PrevSubpixelDepthViewportMax)
		SHADER_PARAMETER(FScreenTransform, InputPixelPosToPrevScreenPosition)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToOutputPixelPos)
		SHADER_PARAMETER(int32, MaxDepthAge)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevSubpixelDepthTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SubpixelDepthOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRForwardScatterDepthCS

class FTSRDilateVelocityCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRDilateVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRDilateVelocityCS, FTSRShader);

	class FMotionBlurDirectionsDim : SHADER_PERMUTATION_INT("DIM_MOTION_BLUR_DIRECTIONS", 3);
	class FSubpixelDepthDim : SHADER_PERMUTATION_BOOL("DIM_SUBPIXEL_DEPTH");
	using FPermutationDomain = TShaderPermutationDomain<FMotionBlurDirectionsDim, FSubpixelDepthDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVelocityFlattenParameters, VelocityFlattenParameters)

		SHADER_PARAMETER(FMatrix44f, RotationalClipToPrevClip)
		SHADER_PARAMETER(FVector2f, PrevOutputBufferUVMin)
		SHADER_PARAMETER(FVector2f, PrevOutputBufferUVMax)
		SHADER_PARAMETER(float, WorldDepthToDepthError)
		SHADER_PARAMETER(float, VelocityExtrapolationMultiplier)
		SHADER_PARAMETER(float, InvFlickeringMaxParralaxVelocity)
		SHADER_PARAMETER(int32, bOutputIsMovingTexture)
		SHADER_PARAMETER(int32, bIncludeDynamicDepthInSubpixelDetails)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevScatteredSubpixelDepthTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DilatedVelocityOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ClosestDepthOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PrevUseCountOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, PrevClosestDepthOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, R8Output)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, SubpixelDepthOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityFlattenOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityTileOutput0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityTileOutput1)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// There isn't enough bindable UAV for outputing subpixel depth and motion blur render targets.
		if (PermutationVector.Get<FSubpixelDepthDim>())
		{
			PermutationVector.Set<FMotionBlurDirectionsDim>(0);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector != RemapPermutation(PermutationVector))
		{
			return false;
		}

		return FTSRShader::ShouldCompilePermutation(Parameters);
	}
}; // class FTSRDilateVelocityCS

class FTSRDecimateHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRDecimateHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRDecimateHistoryCS, FTSRShader);

	class FMoireReprojectionDim : SHADER_PERMUTATION_BOOL("DIM_MOIRE_REPROJECTION");
	class FGrandReprojectionDim : SHADER_PERMUTATION_BOOL("DIM_GRAND_REPROJECTION");
	class FSubpixelParallaxFactorDim : SHADER_PERMUTATION_BOOL("DIM_SUBPIXEL_PARALLAX_FACTOR");
	using FPermutationDomain = TShaderPermutationDomain<FMoireReprojectionDim, FGrandReprojectionDim, FSubpixelParallaxFactorDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FMatrix44f, RotationalClipToPrevClip)
		SHADER_PARAMETER(FVector2f, CurrentToPrevInputJitter)
		SHADER_PARAMETER(float, WorldDepthToPixelWorldRadius)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevUseCountTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevClosestDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ParallaxFactorTexture)

		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRPrevHistoryParameters, PrevHistoryParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistorySubpixelDetails)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryGuide)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryMoire)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevVelocityTexture)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevGuideInfo)
		SHADER_PARAMETER(FScreenTransform, InputPixelPosToReprojectScreenPos)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevHistoryGuideBufferUV)
		SHADER_PARAMETER(FVector3f, HistoryGuideQuantizationError)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ReprojectedHistoryGuideOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ReprojectedHistoryMoireOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HoleFilledVelocityOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HoleFilledVelocityMaskOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, GrandHoleFilledVelocityOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ParallaxRejectionMaskOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistorySubpixelDetailsOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRDecimateHistoryCS

class FTSRCompareTranslucencyCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRCompareTranslucencyCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRCompareTranslucencyCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, TranslucencyInfo)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevTranslucencyInfo)
		SHADER_PARAMETER(float, PrevTranslucencyPreExposureCorrection)
		SHADER_PARAMETER(float, TranslucencyHighlightLuminance)

		SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevTranslucencyTextureUV)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TranslucencyTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevTranslucencyTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, TranslucencyRejectionOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRCompareTranslucencyCS

class FTSRRejectShadingCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRRejectShadingCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRRejectShadingCS, FTSRShader);

	class FWaveSizeOps : SHADER_PERMUTATION_SPARSE_INT("DIM_WAVE_SIZE", 0, 32, 64);
	class FFlickeringDetectionDim : SHADER_PERMUTATION_BOOL("DIM_FLICKERING_DETECTION");

	using FPermutationDomain = TShaderPermutationDomain<FWaveSizeOps, FFlickeringDetectionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FScreenTransform, InputPixelPosToTranslucencyTextureUV)
		SHADER_PARAMETER(FVector2f, TranslucencyTextureUVMin)
		SHADER_PARAMETER(FVector2f, TranslucencyTextureUVMax)
		SHADER_PARAMETER(FVector3f, HistoryGuideQuantizationError)
		SHADER_PARAMETER(float, FlickeringFramePeriod)
		SHADER_PARAMETER(float, TheoricBlendFactor)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputMoireLumaTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneTranslucencyTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReprojectedHistoryGuideTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ReprojectedHistoryMoireTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ParallaxRejectionMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, IsMovingMaskTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryGuideOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryMoireOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryRejectionOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, InputSceneColorLdrLumaOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FTSRShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		int32 WaveSize = PermutationVector.Get<FWaveSizeOps>();

		ERHIFeatureSupport WaveOpsSupport = FTSRShader::SupportsWaveOps(Parameters.Platform);
		if (WaveSize != 0)
		{
			if (WaveOpsSupport == ERHIFeatureSupport::Unsupported)
			{
				return false;
			}

			if (WaveSize < int32(FDataDrivenShaderPlatformInfo::GetMinimumWaveSize(Parameters.Platform)) ||
				WaveSize > int32(FDataDrivenShaderPlatformInfo::GetMaximumWaveSize(Parameters.Platform)))
			{
				return false;
			}
		}
		else // if (WaveSize == LDS fallback)
		{
			return FTSRShader::SupportsLDS(Parameters.Platform);
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		if (PermutationVector.Get<FWaveSizeOps>() != 0)
		{
			if (PermutationVector.Get<FWaveSizeOps>() == 32)
			{
				OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
			}
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
}; // class FTSRRejectShadingCS

class FTSRSpatialAntiAliasingCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRSpatialAntiAliasingCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRSpatialAntiAliasingCS, FTSRShader);

	class FQualityDim : SHADER_PERMUTATION_INT("DIM_QUALITY_PRESET", 3);

	using FPermutationDomain = TShaderPermutationDomain<FQualityDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorLdrLumaTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, AntiAliasingOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, NoiseFilteringOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// There is no Quality=0 because the pass doesn't get setup.
		if (PermutationVector.Get<FQualityDim>() == 0)
		{
			return false;
		}

		return FTSRShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
}; // class FTSRSpatialAntiAliasingCS

class FTSRFilterAntiAliasingCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRFilterAntiAliasingCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRFilterAntiAliasingCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AntiAliasingTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, AntiAliasingOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRFilterAntiAliasingCS

class FTSRUpdateHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRUpdateHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRUpdateHistoryCS, FTSRShader);

	enum class EQuality
	{
		Low,
		Medium,
		High,
		Epic,
		MAX
	};

	class FQualityDim : SHADER_PERMUTATION_ENUM_CLASS("DIM_UPDATE_QUALITY", EQuality);
	class FSeparateTranslucencyDim : SHADER_PERMUTATION_BOOL("DIM_SEPARATE_TRANSLUCENCY");
	class FGrandReprojectionDim : SHADER_PERMUTATION_BOOL("DIM_GRAND_REPROJECTION");

	using FPermutationDomain = TShaderPermutationDomain<FQualityDim, FSeparateTranslucencyDim, FGrandReprojectionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputSceneStencilTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneTranslucencyTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryRejectionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TranslucencyRejectionTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, GrandDilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ParallaxFactorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ParallaxRejectionMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AntiAliasingTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NoiseFilteringTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HoleFilledVelocityMaskTexture)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, TranslucencyInfo)
		SHADER_PARAMETER(FIntPoint, TranslucencyPixelPosMin)
		SHADER_PARAMETER(FIntPoint, TranslucencyPixelPosMax)

		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToScreenPos)
		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToInputPPCo)
		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToTranslucencyPPCo)

		SHADER_PARAMETER(FVector3f, HistoryQuantizationError)
		SHADER_PARAMETER(float, MinTranslucencyRejection)
		SHADER_PARAMETER(float, HistorySampleCount)
		SHADER_PARAMETER(float, HistoryHisteresis)
		SHADER_PARAMETER(float, WeightClampingRejection)
		SHADER_PARAMETER(float, WeightClampingPixelSpeedAmplitude)
		SHADER_PARAMETER(float, InvWeightClampingPixelSpeed)
		SHADER_PARAMETER(float, InputToHistoryFactor)
		SHADER_PARAMETER(float, InputContributionMultiplier)
		SHADER_PARAMETER(float, GrandPrevPreExposureCorrection)
		SHADER_PARAMETER(int32, ResponsiveStencilMask)
		SHADER_PARAMETER(int32, bGenerateOutputMip1)
		SHADER_PARAMETER(int32, bGenerateOutputMip2)
		SHADER_PARAMETER(int32, bHasSeparateTranslucency)

		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRPrevHistoryParameters, PrevHistoryParameters)
		SHADER_PARAMETER_STRUCT(FTSRHistorySRVs, PrevHistory)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, GrandPrevColorTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip1)
		SHADER_PARAMETER_STRUCT(FTSRHistoryUAVs, HistoryOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		// Only compile the grand reprojection when translucency isn't accumulated separately.
		if (PermutationVector.Get<FGrandReprojectionDim>() && PermutationVector.Get<FSeparateTranslucencyDim>())
		{
			return false;
		}

		return FTSRShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
}; // class FTSRUpdateHistoryCS

class FTSRResolveHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRResolveHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRResolveHistoryCS, FTSRShader);

	using FPermutationDomain = TShaderPermutationDomain<FTSRUpdateHistoryCS::FSeparateTranslucencyDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FScreenTransform, DispatchThreadToHistoryPixelPos)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMin)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMax)
		SHADER_PARAMETER(int32, bGenerateOutputMip1)
		SHADER_PARAMETER(float, HistoryValidityMultiply)

		SHADER_PARAMETER_STRUCT(FTSRHistorySRVs, History)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
	}
}; // class FTSRResolveHistoryCS

class FTSRDebugHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRDebugHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRDebugHistoryCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT(FTSRHistorySRVs, History)
		SHADER_PARAMETER_STRUCT(FTSRHistorySRVs, PrevHistory)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRDebugHistoryCS

IMPLEMENT_GLOBAL_SHADER(FTSRComputeMoireLumaCS,      "/Engine/Private/TemporalSuperResolution/TSRComputeMoireLuma.usf",      "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRClearPrevTexturesCS,     "/Engine/Private/TemporalSuperResolution/TSRClearPrevTextures.usf",     "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRForwardScatterDepthCS,   "/Engine/Private/TemporalSuperResolution/TSRForwardScatterDepth.usf",   "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRDilateVelocityCS,        "/Engine/Private/TemporalSuperResolution/TSRDilateVelocity.usf",        "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRDecimateHistoryCS,       "/Engine/Private/TemporalSuperResolution/TSRDecimateHistory.usf",       "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRCompareTranslucencyCS,   "/Engine/Private/TemporalSuperResolution/TSRCompareTranslucency.usf",   "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRRejectShadingCS,         "/Engine/Private/TemporalSuperResolution/TSRRejectShading.usf",         "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRSpatialAntiAliasingCS,   "/Engine/Private/TemporalSuperResolution/TSRSpatialAntiAliasing.usf",   "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRFilterAntiAliasingCS,    "/Engine/Private/TemporalSuperResolution/TSRFilterAntiAliasing.usf",    "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRUpdateHistoryCS,         "/Engine/Private/TemporalSuperResolution/TSRUpdateHistory.usf",         "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRResolveHistoryCS,        "/Engine/Private/TemporalSuperResolution/TSRResolveHistory.usf",        "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRDebugHistoryCS,          "/Engine/Private/TemporalSuperResolution/TSRDebugHistory.usf",          "MainCS", SF_Compute);

DECLARE_GPU_STAT(TemporalSuperResolution)

} //! namespace

FVector3f ComputePixelFormatQuantizationError(EPixelFormat PixelFormat);

bool ComposeSeparateTranslucencyInTSR(const FViewInfo& View)
{
	return true;
}

static FRDGTextureUAVRef CreateDummyUAV(FRDGBuilder& GraphBuilder, EPixelFormat PixelFormat)
{
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
		FIntPoint(1, 1),
		PixelFormat,
		FClearValueBinding::None,
		/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureRef DummyTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.DummyOutput"));
	GraphBuilder.RemoveUnusedTextureWarning(DummyTexture);

	return GraphBuilder.CreateUAV(DummyTexture);
};

FScreenPassTexture AddTSRComputeMoireLuma(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FScreenPassTexture SceneColor)
{
	check(SceneColor.Texture)
;	RDG_GPU_STAT_SCOPE(GraphBuilder, TemporalSuperResolution);

	FScreenPassTexture MoireLuma;
	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			SceneColor.Texture->Desc.Extent,
			PF_R8,
			FClearValueBinding::None,
			/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

		MoireLuma.Texture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Moire.Luma"));
		MoireLuma.ViewRect = SceneColor.ViewRect;
	}

	FTSRComputeMoireLumaCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRComputeMoireLumaCS::FParameters>();
	PassParameters->InputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
		SceneColor.Texture->Desc.Extent, SceneColor.ViewRect));
	PassParameters->SceneColorTexture = SceneColor.Texture;
	PassParameters->MoireLumaOutput = GraphBuilder.CreateUAV(MoireLuma.Texture);

	TShaderMapRef<FTSRComputeMoireLumaCS> ComputeShader(ShaderMap);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TSR ComputeMoireLuma %dx%d", SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height()),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(MoireLuma.ViewRect.Size(), 8 * 2));

	return MoireLuma;
}

ITemporalUpscaler::FOutputs AddTemporalSuperResolutionPasses(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const ITemporalUpscaler::FPassInputs& PassInputs)
{
	const FTSRHistory& InputHistory = View.PrevViewInfo.TSRHistory;

#if COMPILE_TSR_DEBUG_PASSES
	const bool bSetupDebugPasses = CVarTSRSetupDebugPasses.GetValueOnRenderThread() != 0;
#endif

	// Whether to use wave ops optimizations.
	const ERHIFeatureSupport WaveOpsSupport = FTSRShader::SupportsWaveOps(View.GetShaderPlatform());
	const bool bSupportsLDS = FTSRShader::SupportsLDS(View.GetShaderPlatform());
	const bool bUseWaveOps = (CVarTSRWaveOps.GetValueOnRenderThread() != 0 && GRHISupportsWaveOperations && bSupportsLDS && (WaveOpsSupport == ERHIFeatureSupport::RuntimeDependent || WaveOpsSupport == ERHIFeatureSupport::RuntimeGuaranteed)) || !bSupportsLDS;

	// Whether should accumulate sub pixel depth.
	const ETSRSubpixelMethod SubpixelMethod = ETSRSubpixelMethod(FMath::Clamp(CVarTSRSubpixelMethod.GetValueOnRenderThread(), 0, 2));

	// Whether alpha channel is supported.
	const bool bSupportsAlpha = IsPostProcessingWithAlphaChannelSupported();

	// Whether separate translucency should be temporarily accumulated separatly.
	const bool bAccumulateTranslucencySeparately = CVarTSRHistorySeparateTranslucency.GetValueOnRenderThread() != 0;

	const bool bGrandReprojection = !bAccumulateTranslucencySeparately && CVarTSRHistoryGrandReprojection.GetValueOnRenderThread() != 0;

	const float RefreshRateToFrameRateCap = (View.Family->Time.GetDeltaRealTimeSeconds() > 0.0f && CVarTSRFlickeringAdjustToFrameRate.GetValueOnRenderThread())
		? View.Family->Time.GetDeltaRealTimeSeconds() * CVarTSRFlickeringFrameRateCap.GetValueOnRenderThread() : 1.0f;

	// Maximum number sample for each output pixel in the history
	const float MaxHistorySampleCount = FMath::Clamp(CVarTSRHistorySampleCount.GetValueOnRenderThread(), 8.0f, 32.0f);

	// whether TSR passes can run on async compute.
	int32 AsyncComputePasses = GSupportsEfficientAsyncCompute ? CVarTSRAsyncCompute.GetValueOnRenderThread() : 0;

	// period at which history changes is considered too distracting.
	const float FlickeringFramePeriod = CVarTSRFlickeringEnable.GetValueOnRenderThread() ? (CVarTSRFlickeringPeriod.GetValueOnRenderThread() / FMath::Max(RefreshRateToFrameRateCap, 1.0f)) : 0.0f;

	ETSRHistoryFormatBits HistoryFormatBits = ETSRHistoryFormatBits::None;
	{
		if (bAccumulateTranslucencySeparately)
		{
			HistoryFormatBits |= ETSRHistoryFormatBits::Translucency;
		}

		if (FlickeringFramePeriod > 0)
		{
			HistoryFormatBits |= ETSRHistoryFormatBits::Moire;
		}

		if (bGrandReprojection)
		{
			HistoryFormatBits |= ETSRHistoryFormatBits::GrandReprojection;
		}
	}

	const bool bIsOutputDifferentThanHighFrequency = IsOutputDifferentThanHighFrequency(HistoryFormatBits);

	// Whether to use camera cut shader permutation or not.
	bool bCameraCut = !InputHistory.IsValid() || View.bCameraCut || ETSRHistoryFormatBits(InputHistory.FormatBit) != HistoryFormatBits;

	FTSRUpdateHistoryCS::EQuality UpdateHistoryQuality = FTSRUpdateHistoryCS::EQuality(FMath::Clamp(CVarTSRHistoryUpdateQuality.GetValueOnRenderThread(), 0, int32(FTSRUpdateHistoryCS::EQuality::MAX) - 1));

	bool bIsSeperateTranslucyTexturesValid = PassInputs.PostDOFTranslucencyResources.IsValid();

	const bool bRejectSeparateTranslucency = false; // bIsSeperateTranslucyTexturesValid && CVarTSRTranslucencyPreviousFrameRejection.GetValueOnRenderThread() != 0;

	EPixelFormat ColorFormat = bSupportsAlpha ? PF_FloatRGBA : PF_FloatR11G11B10;
	EPixelFormat HistoryColorFormat = (CVarTSRR11G11B10History.GetValueOnRenderThread() != 0 && !bSupportsAlpha) ? PF_FloatR11G11B10 : PF_FloatRGBA;

	int32 RejectionAntiAliasingQuality = FMath::Clamp(CVarTSRRejectionAntiAliasingQuality.GetValueOnRenderThread(), 1, 2);
	if (UpdateHistoryQuality == FTSRUpdateHistoryCS::EQuality::Low)
	{
		RejectionAntiAliasingQuality = 0; 
	}

	FIntPoint InputExtent = PassInputs.SceneColorTexture->Desc.Extent;
	FIntRect InputRect = View.ViewRect;

	FIntPoint OutputExtent;
	FIntRect OutputRect;
	if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		OutputRect.Min = FIntPoint(0, 0);
		OutputRect.Max = View.GetSecondaryViewRectSize();

		FIntPoint QuantizedPrimaryUpscaleViewSize;
		QuantizeSceneBufferSize(OutputRect.Max, QuantizedPrimaryUpscaleViewSize);

		OutputExtent = FIntPoint(
			FMath::Max(InputExtent.X, QuantizedPrimaryUpscaleViewSize.X),
			FMath::Max(InputExtent.Y, QuantizedPrimaryUpscaleViewSize.Y));
	}
	else
	{
		OutputRect.Min = FIntPoint(0, 0);
		OutputRect.Max = View.ViewRect.Size();
		OutputExtent = InputExtent;
	}

	FIntPoint HistoryExtent;
	FIntPoint HistorySize;
	{
		float MaxHistoryUpscaleFactor = FMath::Max(float(GMaxTextureDimensions) / float(FMath::Max(OutputRect.Width(), OutputRect.Height())), 1.0f);

		float HistoryUpscaleFactor = FMath::Clamp(CVarTSRHistorySP.GetValueOnRenderThread() / 100.0f, 1.0f, 2.0f);
		if (HistoryUpscaleFactor > MaxHistoryUpscaleFactor)
		{
			HistoryUpscaleFactor = 1.0f;
		}
		
		HistorySize = FIntPoint(
			FMath::CeilToInt(OutputRect.Width() * HistoryUpscaleFactor),
			FMath::CeilToInt(OutputRect.Height() * HistoryUpscaleFactor));

		FIntPoint QuantizedHistoryViewSize;
		QuantizeSceneBufferSize(HistorySize, QuantizedHistoryViewSize);

		HistoryExtent = FIntPoint(
			FMath::Max(InputExtent.X, QuantizedHistoryViewSize.X),
			FMath::Max(InputExtent.Y, QuantizedHistoryViewSize.Y));
	}
	float OutputToHistoryResolutionFraction = float(HistorySize.X) / float(OutputRect.Width());
	float OutputToHistoryResolutionFractionSquare = OutputToHistoryResolutionFraction * OutputToHistoryResolutionFraction;

	float InputToHistoryResolutionFraction = float(HistorySize.X) / float(InputRect.Width());
	float InputToHistoryResolutionFractionSquare = InputToHistoryResolutionFraction * InputToHistoryResolutionFraction;

	float OutputToInputResolutionFraction = float(InputRect.Width()) / float(OutputRect.Width());
	float OutputToInputResolutionFractionSquare = OutputToInputResolutionFraction * OutputToInputResolutionFraction;

	RDG_EVENT_SCOPE(GraphBuilder, "TemporalSuperResolution(%s) %dx%d -> %dx%d",
		bSupportsAlpha ? TEXT("Alpha") : TEXT(""),
		InputRect.Width(), InputRect.Height(),
		OutputRect.Width(), OutputRect.Height());
	RDG_GPU_STAT_SCOPE(GraphBuilder, TemporalSuperResolution);

	FRDGTextureRef BlackUintDummy = GSystemTextures.GetZeroUIntDummy(GraphBuilder);
	FRDGTextureRef BlackDummy = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef BlackAlphaOneDummy = GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
	FRDGTextureRef WhiteDummy = GSystemTextures.GetWhiteDummy(GraphBuilder);

	FIntRect SeparateTranslucencyRect = FIntRect(0, 0, 1, 1);
	FRDGTextureRef SeparateTranslucencyTexture = BlackAlphaOneDummy;
	bool bHasSeparateTranslucency = PassInputs.PostDOFTranslucencyResources.IsValid();
#if WITH_EDITOR
	// Do not composite translucency if we are visualizing a buffer, unless it is the overview mode.
	static FName OverviewName = FName(TEXT("Overview"));
	bHasSeparateTranslucency &= 
		   (!View.Family->EngineShowFlags.VisualizeBuffer    || (View.Family->EngineShowFlags.VisualizeBuffer    && View.CurrentBufferVisualizationMode == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeNanite    || (View.Family->EngineShowFlags.VisualizeNanite    && View.CurrentNaniteVisualizationMode == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeLumen     || (View.Family->EngineShowFlags.VisualizeLumen     && View.CurrentLumenVisualizationMode  == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeSubstrate || (View.Family->EngineShowFlags.VisualizeSubstrate && View.CurrentStrataVisualizationMode == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeGroom     || (View.Family->EngineShowFlags.VisualizeGroom     && View.CurrentGroomVisualizationMode  == OverviewName));
#endif
	if (bHasSeparateTranslucency)
	{
		SeparateTranslucencyTexture = PassInputs.PostDOFTranslucencyResources.ColorTexture.Resolve;
		SeparateTranslucencyRect = PassInputs.PostDOFTranslucencyResources.ViewRect;
	}

	FMatrix44f RotationalClipToPrevClip;
	{
		const FViewMatrices& ViewMatrices = View.ViewMatrices;
		const FViewMatrices& PrevViewMatrices = View.PrevViewInfo.ViewMatrices;

		FMatrix RotationalInvViewProj = ViewMatrices.ComputeInvProjectionNoAAMatrix() * (ViewMatrices.GetTranslatedViewMatrix().RemoveTranslation().GetTransposed());
		FMatrix RotationalPrevViewProj = (PrevViewMatrices.GetTranslatedViewMatrix().RemoveTranslation()) * PrevViewMatrices.ComputeProjectionNoAAMatrix();

		RotationalClipToPrevClip = FMatrix44f(RotationalInvViewProj * RotationalPrevViewProj);
	}

	FTSRCommonParameters CommonParameters;
	{
		CommonParameters.InputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
			InputExtent, InputRect));
		CommonParameters.InputPixelPosMin = CommonParameters.InputInfo.ViewportMin;
		CommonParameters.InputPixelPosMax = CommonParameters.InputInfo.ViewportMax - 1;
		CommonParameters.InputPixelPosToScreenPos = (FScreenTransform::Identity + 0.5f) * FScreenTransform::ChangeTextureBasisFromTo(FScreenPassTextureViewport(
			InputExtent, InputRect), FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ScreenPosition);
		CommonParameters.ScreenVelocityToInputPixelVelocity = (FScreenTransform::Identity / CommonParameters.InputPixelPosToScreenPos).Scale;
		CommonParameters.InputPixelVelocityToScreenVelocity = CommonParameters.InputPixelPosToScreenPos.Scale.GetAbs();

		CommonParameters.HistoryInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
			HistoryExtent, FIntRect(FIntPoint(0, 0), HistorySize)));

		CommonParameters.InputJitter = FVector2f(View.TemporalJitterPixels);
		CommonParameters.bCameraCut = bCameraCut;
		CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	}

	auto CreateDebugUAV = [&](const FIntPoint& Extent, const TCHAR* DebugName)
	{
#if COMPILE_TSR_DEBUG_PASSES
		uint16 ArraySize = uint16(FMath::Clamp(CVarTSRDebugArraySize.GetValueOnRenderThread(), 1, GMaxTextureArrayLayers));
#else
		const uint16 ArraySize = 1;
#endif

		FRDGTextureDesc DebugDesc = FRDGTextureDesc::Create2DArray(
			Extent,
			PF_FloatRGBA,
			FClearValueBinding::None,
			/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
			ArraySize);

		FRDGTextureRef DebugTexture = GraphBuilder.CreateTexture(DebugDesc, DebugName);

		return GraphBuilder.CreateUAV(DebugTexture);
	};

	// Create new history.
	FRDGTextureRef UpdateHistoryOutputTexture;
	FRDGTextureRef SceneColorOutputTexture;
	FRDGTextureRef SceneColorOutputHalfResTexture = nullptr;
	FRDGTextureRef SceneColorOutputQuarterResTexture = nullptr;
	FTSRHistoryTextures History;
	{
		check(!(PassInputs.bGenerateOutputMip1 && (PassInputs.bGenerateSceneColorHalfRes || PassInputs.bGenerateSceneColorQuarterRes)));
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			HistoryExtent,
			bIsOutputDifferentThanHighFrequency ? ColorFormat : HistoryColorFormat,
			FClearValueBinding::None,
			/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
			/* NumMips = */ PassInputs.bGenerateOutputMip1 ? 2 : 1);

		UpdateHistoryOutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Output"));
		History.Output = UpdateHistoryOutputTexture;

		Desc.Format = ColorFormat;

		if (OutputRect.Size() != HistorySize)
		{
			Desc.Extent = OutputExtent;
			SceneColorOutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Output"));
		}
		else
		{
			SceneColorOutputTexture = UpdateHistoryOutputTexture;
		}

		// Generate quarter res output if only one needed, otherwise only output half res and let caller downscale to quarter res. 
		// This is motivated to saves UAV slots on FTSRUpdateHistoryCS
		if (PassInputs.bGenerateSceneColorQuarterRes && !PassInputs.bGenerateSceneColorHalfRes && OutputRect.Size() == HistorySize)
		{
			FRDGTextureDesc QuarterResDesc = Desc;
			QuarterResDesc.Extent /= 4;
			SceneColorOutputQuarterResTexture = GraphBuilder.CreateTexture(QuarterResDesc, TEXT("TSR.QuarterResOutput"));
		}
		else if (PassInputs.bGenerateSceneColorHalfRes || PassInputs.bGenerateSceneColorQuarterRes)
		{
			FRDGTextureDesc HalfResDesc = Desc;
			HalfResDesc.Extent /= 2;
			SceneColorOutputHalfResTexture = GraphBuilder.CreateTexture(HalfResDesc, TEXT("TSR.HalfResOutput"));
		}
	}
	{
		History.ArrayIndices = TranslateHistoryFormatBitsToArrayIndices(HistoryFormatBits);

		if (History.ArrayIndices.Size > 0)
		{
			FRDGTextureDesc ArrayDesc = FRDGTextureDesc::Create2DArray(
				HistoryExtent,
				HistoryColorFormat,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV,
				History.ArrayIndices.Size);

			History.ColorArray = GraphBuilder.CreateTexture(ArrayDesc, TEXT("TSR.History.ColorArray"));
		}
	}
	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			HistoryExtent,
			HistoryColorFormat,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		if (bGrandReprojection && History.ArrayIndices.PrevHighFrequencyResultant == -1)
		{
			Desc.Format = PF_R8G8;
		}
		else
		{
			Desc.Format = PF_R8;
		}
		History.Metadata = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Metadata"));

		Desc.Format = PF_R8;
		if (bAccumulateTranslucencySeparately)
		{
			History.TranslucencyAlpha = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.TranslucencyAlpha"));
		}
	}

	if (SubpixelMethod == ETSRSubpixelMethod::ParallaxFactor)
	{
		FRDGTextureDesc SubpixelDetailsDesc = FRDGTextureDesc::Create2D(
			InputExtent,
			PF_R16_UINT,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);
		History.SubpixelDetails = GraphBuilder.CreateTexture(SubpixelDetailsDesc, TEXT("TSR.History.SubpixelInfo"));
	}
	else if (SubpixelMethod == ETSRSubpixelMethod::ClosestDepth)
	{
		FRDGTextureDesc SubpixelDepthDesc = FRDGTextureDesc::Create2D(
			InputExtent,
			PF_R32_UINT,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);
		History.SubpixelDepth = GraphBuilder.CreateTexture(SubpixelDepthDesc, TEXT("TSR.History.SubpixelDepth"));
	}

	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			InputExtent,
			bSupportsAlpha ? PF_FloatRGBA : PF_A2B10G10R10,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV);

		History.Guide = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Guide"));
		Desc.Format = PF_R8G8B8A8;
		History.Moire = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Moire"));
	}

	// Setup a dummy history
	FTSRHistorySRVs DummyHistorySRVs;
	{
		DummyHistorySRVs.HighFrequency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.Metadata = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.Translucency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.TranslucencyAlpha = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(WhiteDummy));
		DummyHistorySRVs.PrevHighFrequency = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.PrevHighFrequencyResultant = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.HighFrequencyOverblur = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(BlackDummy));
		DummyHistorySRVs.Guide = BlackDummy;
		DummyHistorySRVs.Moire = BlackDummy;
	}

	// Setup the previous frame history
	FRDGTextureRef PrevVelocity = BlackDummy;
	FRDGTextureSRVRef GrandPrevColorTexture = GraphBuilder.CreateSRV(BlackDummy);
	FTSRHistoryTextures PrevHistory;
	FTSRHistorySRVs PrevHistorySRVs;
	if (!bCameraCut)
	{
		PrevHistory.ArrayIndices = History.ArrayIndices;

		// Register filterable history
		PrevHistory.Output = InputHistory.Output.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.Output, TEXT("TSR.PrevHistory.Output")) : nullptr;
		PrevHistory.ColorArray = InputHistory.ColorArray.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.ColorArray, TEXT("TSR.PrevHistory.ColorArray")) : nullptr;
		PrevHistory.Metadata = GraphBuilder.RegisterExternalTexture(InputHistory.Metadata, TEXT("TSR.PrevHistory.Metadata"));
		PrevHistory.TranslucencyAlpha = InputHistory.TranslucencyAlpha.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.TranslucencyAlpha, TEXT("TSR.PrevHistory.TranslucencyAlpha")) : DummyHistorySRVs.TranslucencyAlpha->Desc.Texture;
		PrevHistory.Guide = GraphBuilder.RegisterExternalTexture(InputHistory.Guide, TEXT("TSR.PrevHistory.Guide"));
		PrevHistory.Moire = InputHistory.Moire.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.Moire, TEXT("TSR.PrevHistory.Moire")) : DummyHistorySRVs.Moire;

		// Register non-filterable history
		PrevHistory.SubpixelDetails = InputHistory.SubpixelDetails.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.SubpixelDetails, TEXT("TSR.PrevHistory.SubpixelInfo")) : nullptr;
		PrevHistory.SubpixelDepth = InputHistory.SubpixelDepth.IsValid() ? GraphBuilder.RegisterExternalTexture(InputHistory.SubpixelDepth, TEXT("TSR.PrevHistory.SubpixelDepth")) : nullptr;

		PrevHistorySRVs = CreateSRVs(GraphBuilder, PrevHistory);

		if (bGrandReprojection)
		{
			PrevVelocity = GraphBuilder.RegisterExternalTexture(InputHistory.Velocity);
			if (InputHistory.PrevColorArray.IsValid() && bIsOutputDifferentThanHighFrequency)
			{
				GrandPrevColorTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(GraphBuilder.RegisterExternalTexture(InputHistory.PrevColorArray, TEXT("TSR.GrandPrevHistory.ColorArray")), PrevHistory.ArrayIndices.HighFrequency));
			}
			else if (InputHistory.PrevOutput.IsValid() && !bIsOutputDifferentThanHighFrequency)
			{
				GrandPrevColorTexture = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalTexture(InputHistory.PrevOutput, TEXT("TSR.GrandPrevHistory.Output")));
			}
		}
	}
	else
	{
		PrevHistorySRVs = DummyHistorySRVs;
	}

	// Setup the shader parameters for previous frame history
	FTSRPrevHistoryParameters PrevHistoryParameters;
	{
		// Setup prev history parameters.
		FScreenPassTextureViewport PrevHistoryViewport(PrevHistorySRVs.HighFrequency->Desc.Texture->Desc.Extent, InputHistory.OutputViewportRect);
		FScreenPassTextureViewport PrevSubpixelDetailsViewport(PrevHistorySRVs.Guide->Desc.Extent, InputHistory.InputViewportRect);
		FScreenPassTextureViewport GrandPrevSubpixelDetailsViewport(GrandPrevColorTexture->Desc.Texture->Desc.Extent, InputHistory.PrevOutputViewportRect);

		if (bCameraCut)
		{
			PrevHistoryViewport.Extent = FIntPoint(1, 1);
			PrevHistoryViewport.Rect = FIntRect(FIntPoint(0, 0), FIntPoint(1, 1));
			PrevSubpixelDetailsViewport.Extent = FIntPoint(1, 1);
			PrevSubpixelDetailsViewport.Rect = FIntRect(FIntPoint(0, 0), FIntPoint(1, 1));
			GrandPrevSubpixelDetailsViewport.Extent = FIntPoint(1, 1);
			GrandPrevSubpixelDetailsViewport.Rect = FIntRect(FIntPoint(0, 0), FIntPoint(1, 1));
		}

		PrevHistoryParameters.PrevHistoryInfo = GetScreenPassTextureViewportParameters(PrevHistoryViewport);
		PrevHistoryParameters.ScreenPosToPrevHistoryBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
			PrevHistoryViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
		PrevHistoryParameters.ScreenPosToPrevSubpixelDetails = FScreenTransform::ChangeTextureBasisFromTo(
			PrevSubpixelDetailsViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
		PrevHistoryParameters.ScreenPosToGrandPrevHistoryBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
			GrandPrevSubpixelDetailsViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
		PrevHistoryParameters.PrevSubpixelDetailsExtent = PrevSubpixelDetailsViewport.Extent;
		PrevHistoryParameters.HistoryPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
	}

	// Clear atomic scattered texture.
	FRDGTextureRef PrevUseCountTexture;
	FRDGTextureRef PrevClosestDepthTexture;
	FRDGTextureRef PrevScatteredSubpixelDepthTexture = nullptr;
	{
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R32_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible);

			PrevUseCountTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.PrevUseCountTexture"));
			PrevClosestDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.PrevClosestDepthTexture"));
		}

		FTSRClearPrevTexturesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRClearPrevTexturesCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->PrevUseCountOutput = GraphBuilder.CreateUAV(PrevUseCountTexture);
		PassParameters->PrevClosestDepthOutput = GraphBuilder.CreateUAV(PrevClosestDepthTexture);
		if (History.SubpixelDepth && PrevHistory.SubpixelDepth)
		{
			FRDGTextureDesc Desc = History.SubpixelDepth->Desc;
			Desc.Flags |= TexCreate_AtomicCompatible;

			PrevScatteredSubpixelDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.PrevScatteredSubpixelDepth"));
			PassParameters->SubpixelDepthOutput = GraphBuilder.CreateUAV(PrevScatteredSubpixelDepthTexture);
		}
		else
		{
			PassParameters->SubpixelDepthOutput = CreateDummyUAV(GraphBuilder, PF_R32_UINT);
		}

		TShaderMapRef<FTSRClearPrevTexturesCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR ClearPrevTextures %dx%d", InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 1 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8 * 2));
	}

	// Forward scatter previous frame sub pixel depths.
	if (PrevScatteredSubpixelDepthTexture)
	{
		FIntRect ScatterDepthViewport = InputHistory.InputViewportRect;

		FTSRForwardScatterDepthCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRForwardScatterDepthCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->PrevSubpixelDepthViewportMin = ScatterDepthViewport.Min;
		PassParameters->PrevSubpixelDepthViewportMax = ScatterDepthViewport.Max - 1;
		PassParameters->InputPixelPosToPrevScreenPosition = (FScreenTransform::Identity + 0.5f) * FScreenTransform::ChangeTextureBasisFromTo(
			PrevHistory.SubpixelDepth->Desc.Extent, ScatterDepthViewport,
			FScreenTransform::ETextureBasis::TexelPosition, FScreenTransform::ETextureBasis::ScreenPosition);
		PassParameters->ScreenPosToOutputPixelPos = FScreenTransform::ChangeTextureBasisFromTo(
			InputExtent, InputRect,
			FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TexelPosition);
		PassParameters->MaxDepthAge = FMath::Clamp(CVarTSRSubpixelDepthMaxAge.GetValueOnRenderThread(), 1, (1 << 7) - 1);

		PassParameters->PrevSubpixelDepthTexture = PrevHistory.SubpixelDepth;
		PassParameters->SubpixelDepthOutput = GraphBuilder.CreateUAV(PrevScatteredSubpixelDepthTexture);
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.ForwardScatterDepth"));

		TShaderMapRef<FTSRForwardScatterDepthCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR ForwardScatterDepth %dx%d", ScatterDepthViewport.Width(), ScatterDepthViewport.Height()),
			AsyncComputePasses >= 1 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(ScatterDepthViewport.Size(), 8));
	}

	// Dilate the velocity texture & scatter reprojection into previous frame
	FRDGTextureRef DilatedVelocityTexture;
	FRDGTextureRef ClosestDepthTexture;
	FRDGTextureSRVRef ParallaxFactorTexture;
	FRDGTextureSRVRef IsMovingMaskTexture = nullptr;
	FVelocityFlattenTextures VelocityFlattenTextures;
	{
		const bool bOutputIsMovingTexture = FlickeringFramePeriod > 0.0f;

		FRDGTextureRef R8OutputTexture;

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_G16R16,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			DilatedVelocityTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Velocity.Dilated"));

			Desc.Format = PF_R16F;
			ClosestDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ClosestDepthTexture"));
		}
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				InputExtent,
				PF_R8_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				bOutputIsMovingTexture ? 2 : 1);

			R8OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ParallaxFactor"));
			ParallaxFactorTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(R8OutputTexture, 0));
			if (bOutputIsMovingTexture)
			{
				IsMovingMaskTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(R8OutputTexture, 1));
			}
		}

		int32 TileSize = 8;
		FTSRDilateVelocityCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRDilateVelocityCS::FSubpixelDepthDim>(SubpixelMethod == ETSRSubpixelMethod::ClosestDepth);

		FTSRDilateVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRDilateVelocityCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->RotationalClipToPrevClip = RotationalClipToPrevClip;
		PassParameters->PrevOutputBufferUVMin = CommonParameters.InputInfo.UVViewportBilinearMin - CommonParameters.InputInfo.ExtentInverse;
		PassParameters->PrevOutputBufferUVMax = CommonParameters.InputInfo.UVViewportBilinearMax + CommonParameters.InputInfo.ExtentInverse;
		{
			float TanHalfFieldOfView = View.ViewMatrices.GetInvProjectionMatrix().M[0][0];

			// Should be multiplied 0.5* for the diameter to radius, and by 2.0 because GetTanHalfFieldOfView() cover only half of the pixels.
			float WorldDepthToPixelWorldRadius = TanHalfFieldOfView / float(View.ViewRect.Width());

			PassParameters->WorldDepthToDepthError = WorldDepthToPixelWorldRadius * 2.0f;
		}
		PassParameters->VelocityExtrapolationMultiplier = 0.0; //FMath::Clamp(CVarTSRVelocityExtrapolation.GetValueOnRenderThread(), 0.0f, 1.0f);
		{
			float FlickeringMaxParralaxVelocity = RefreshRateToFrameRateCap * CVarTSRFlickeringMaxParralaxVelocity.GetValueOnRenderThread() * float(View.ViewRect.Width()) / 1920.0f;
			PassParameters->InvFlickeringMaxParralaxVelocity = 1.0f / FlickeringMaxParralaxVelocity;
		}
		PassParameters->bOutputIsMovingTexture = bOutputIsMovingTexture;
		PassParameters->bIncludeDynamicDepthInSubpixelDetails = SubpixelMethod == ETSRSubpixelMethod::ClosestDepth && CVarTSRSubpixelIncludeMovingDepth.GetValueOnRenderThread();

		PassParameters->SceneDepthTexture = PassInputs.SceneDepthTexture;
		PassParameters->SceneVelocityTexture = PassInputs.SceneVelocityTexture;
		if (PrevScatteredSubpixelDepthTexture)
		{
			PassParameters->PrevScatteredSubpixelDepthTexture = PrevScatteredSubpixelDepthTexture;
		}
		else
		{
			PassParameters->PrevScatteredSubpixelDepthTexture = BlackUintDummy;
		}

		PassParameters->DilatedVelocityOutput = GraphBuilder.CreateUAV(DilatedVelocityTexture);
		PassParameters->ClosestDepthOutput = GraphBuilder.CreateUAV(ClosestDepthTexture);
		PassParameters->PrevUseCountOutput = GraphBuilder.CreateUAV(PrevUseCountTexture);
		PassParameters->PrevClosestDepthOutput = GraphBuilder.CreateUAV(PrevClosestDepthTexture);
		PassParameters->R8Output = GraphBuilder.CreateUAV(R8OutputTexture);
		if (SubpixelMethod == ETSRSubpixelMethod::ClosestDepth)
		{
			PassParameters->SubpixelDepthOutput = GraphBuilder.CreateUAV(History.SubpixelDepth);
		}

		// Setup up the motion blur's velocity flatten pass.
		if (PassInputs.bGenerateVelocityFlattenTextures && SubpixelMethod != ETSRSubpixelMethod::ClosestDepth)
		{
			const int32 MotionBlurDirections = GetMotionBlurDirections();
			PermutationVector.Set<FTSRDilateVelocityCS::FMotionBlurDirectionsDim>(MotionBlurDirections);
			TileSize = FVelocityFlattenTextures::kTileSize;

			{
				FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
					InputExtent,
					PF_FloatR11G11B10,
					FClearValueBinding::None,
					GFastVRamConfig.VelocityFlat | TexCreate_ShaderResource | TexCreate_UAV);

				VelocityFlattenTextures.VelocityFlatten.Texture = GraphBuilder.CreateTexture(Desc, TEXT("MotionBlur.VelocityTile"));
				VelocityFlattenTextures.VelocityFlatten.ViewRect = InputRect;
			}

			{
				FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
					FIntPoint::DivideAndRoundUp(InputRect.Size(), FVelocityFlattenTextures::kTileSize),
					PF_FloatRGBA,
					FClearValueBinding::None,
					GFastVRamConfig.MotionBlur | TexCreate_ShaderResource | TexCreate_UAV);

				VelocityFlattenTextures.VelocityTile[0].Texture = GraphBuilder.CreateTexture(Desc, TEXT("MotionBlur.VelocityTile"));
				VelocityFlattenTextures.VelocityTile[0].ViewRect = FIntRect(FIntPoint::ZeroValue, Desc.Extent);

				Desc.Format = PF_G16R16F;
				VelocityFlattenTextures.VelocityTile[1].Texture = GraphBuilder.CreateTexture(Desc, TEXT("MotionBlur.VelocityTile"));
				VelocityFlattenTextures.VelocityTile[1].ViewRect = FIntRect(FIntPoint::ZeroValue, Desc.Extent);
			}

			PassParameters->VelocityFlattenParameters = GetVelocityFlattenParameters(View);
			PassParameters->VelocityFlattenOutput = GraphBuilder.CreateUAV(VelocityFlattenTextures.VelocityFlatten.Texture);
			PassParameters->VelocityTileOutput0 = GraphBuilder.CreateUAV(VelocityFlattenTextures.VelocityTile[0].Texture);
			PassParameters->VelocityTileOutput1 = GraphBuilder.CreateUAV(VelocityFlattenTextures.VelocityTile[1].Texture);
		}

		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.DilateVelocity"));

		check(PermutationVector == FTSRDilateVelocityCS::RemapPermutation(PermutationVector));
		TShaderMapRef<FTSRDilateVelocityCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR DilateVelocity(MotionBlurDirections=%d%s%s) %dx%d",
				int32(PermutationVector.Get<FTSRDilateVelocityCS::FMotionBlurDirectionsDim>()),
				bOutputIsMovingTexture ? TEXT(" OutputIsMoving") : TEXT(""),
				PermutationVector.Get<FTSRDilateVelocityCS::FSubpixelDepthDim>() ? TEXT(" SubpixelDepth") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 2 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), TileSize));
	}

	// Decimate input to flicker at same frequency as input.
	FRDGTextureRef ReprojectedHistoryGuideTexture = nullptr;
	FRDGTextureRef ReprojectedHistoryMoireTexture = nullptr;
	FRDGTextureRef ParallaxRejectionMaskTexture = nullptr;
	FRDGTextureRef HoleFilledVelocityMaskTexture = nullptr;
	FRDGTextureRef GrandHoleFilledVelocityTexture = BlackDummy;
	{
		FRDGTextureRef HoleFilledVelocityTexture;
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			ParallaxRejectionMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ParallaxRejectionMask"));

			Desc.Format = PF_G16R16;
			HoleFilledVelocityTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Velocity.HoleFilled"));
			GrandHoleFilledVelocityTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Velocity.GrandVelocity"));

			Desc.Format = PF_R8G8;
			HoleFilledVelocityMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.velocity.HoleFillMask"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				History.Guide->Desc.Format,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			ReprojectedHistoryGuideTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ReprojectedHistoryGuide"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				History.Moire->Desc.Format,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			ReprojectedHistoryMoireTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ReprojectedHistoryMoire"));
		}

		FTSRDecimateHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRDecimateHistoryCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->RotationalClipToPrevClip = RotationalClipToPrevClip;
		PassParameters->CurrentToPrevInputJitter = InputHistory.PrevTemporalJitterPixels - CommonParameters.InputJitter;
		{
			float TanHalfFieldOfView = View.ViewMatrices.GetInvProjectionMatrix().M[0][0];

			// Should be multiplied 0.5* for the diameter to radius, and by 2.0 because GetTanHalfFieldOfView() cover only half of the pixels.
			PassParameters->WorldDepthToPixelWorldRadius = TanHalfFieldOfView / float(View.ViewRect.Width());
		}

		PassParameters->InputSceneColorTexture = PassInputs.SceneColorTexture;
		PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
		PassParameters->ClosestDepthTexture = ClosestDepthTexture;
		PassParameters->PrevUseCountTexture = PrevUseCountTexture;
		PassParameters->PrevClosestDepthTexture = PrevClosestDepthTexture;
		PassParameters->ParallaxFactorTexture = ParallaxFactorTexture;

		PassParameters->PrevHistoryParameters = PrevHistoryParameters;
		PassParameters->PrevHistorySubpixelDetails = PrevHistory.SubpixelDetails ? PrevHistory.SubpixelDetails : BlackUintDummy;
		PassParameters->PrevVelocityTexture = PrevVelocity;

		{
			FScreenPassTextureViewport PrevHistoryColorViewport(PrevHistorySRVs.Guide->Desc.Extent, InputHistory.InputViewportRect);
			PassParameters->PrevHistoryGuide = PrevHistorySRVs.Guide;
			PassParameters->PrevHistoryMoire = PrevHistorySRVs.Moire;
			PassParameters->PrevGuideInfo = GetScreenPassTextureViewportParameters(PrevHistoryColorViewport);
			PassParameters->InputPixelPosToReprojectScreenPos = ((FScreenTransform::Identity - InputRect.Min + 0.5f) / InputRect.Size()) * FScreenTransform::ViewportUVToScreenPos;
			PassParameters->ScreenPosToPrevHistoryGuideBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
				PrevHistoryColorViewport,
				FScreenTransform::ETextureBasis::ScreenPosition,
				FScreenTransform::ETextureBasis::TextureUV);
			PassParameters->HistoryGuideQuantizationError = ComputePixelFormatQuantizationError(PrevHistorySRVs.Guide->Desc.Format);
		}

		PassParameters->ReprojectedHistoryGuideOutput = GraphBuilder.CreateUAV(ReprojectedHistoryGuideTexture);
		PassParameters->ReprojectedHistoryMoireOutput = GraphBuilder.CreateUAV(ReprojectedHistoryMoireTexture);
		PassParameters->HoleFilledVelocityOutput = GraphBuilder.CreateUAV(HoleFilledVelocityTexture);
		PassParameters->HoleFilledVelocityMaskOutput = GraphBuilder.CreateUAV(HoleFilledVelocityMaskTexture);
		PassParameters->GrandHoleFilledVelocityOutput = GraphBuilder.CreateUAV(GrandHoleFilledVelocityTexture);
		PassParameters->ParallaxRejectionMaskOutput = GraphBuilder.CreateUAV(ParallaxRejectionMaskTexture);
		if (History.SubpixelDetails)
		{
			PassParameters->HistorySubpixelDetailsOutput = GraphBuilder.CreateUAV(History.SubpixelDetails);
		}
		else
		{
			PassParameters->HistorySubpixelDetailsOutput = CreateDummyUAV(GraphBuilder, PF_R16_UINT);
		}
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.DecimateHistory"));

		FTSRDecimateHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRDecimateHistoryCS::FMoireReprojectionDim>(FlickeringFramePeriod > 0.0f);
		PermutationVector.Set<FTSRDecimateHistoryCS::FGrandReprojectionDim>(bGrandReprojection);
		PermutationVector.Set<FTSRDecimateHistoryCS::FSubpixelParallaxFactorDim>(SubpixelMethod == ETSRSubpixelMethod::ParallaxFactor);

		TShaderMapRef<FTSRDecimateHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR DecimateHistory(%s%s%s) %dx%d",
				PermutationVector.Get<FTSRDecimateHistoryCS::FMoireReprojectionDim>() ? TEXT("ReprojectMoire") : TEXT(""),
				PermutationVector.Get<FTSRDecimateHistoryCS::FGrandReprojectionDim>() ? TEXT(" GrandReprojection") : TEXT(""),
				PermutationVector.Get<FTSRDecimateHistoryCS::FSubpixelParallaxFactorDim>() ? TEXT(" SubpixelParallaxFactor") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 2 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));

		DilatedVelocityTexture = HoleFilledVelocityTexture;
	}

	FRDGTextureRef TranslucencyRejectionTexture = nullptr;
	if (bRejectSeparateTranslucency && View.PrevViewInfo.SeparateTranslucency != nullptr)
	{
		FRDGTextureRef PrevTranslucencyTexture;
		FScreenPassTextureViewport PrevTranslucencyViewport;

		if (View.PrevViewInfo.SeparateTranslucency)
		{

			PrevTranslucencyTexture = GraphBuilder.RegisterExternalTexture(View.PrevViewInfo.SeparateTranslucency);
			PrevTranslucencyViewport = FScreenPassTextureViewport(PrevTranslucencyTexture->Desc.Extent, View.PrevViewInfo.SeparateTranslucencyViewRect);

		}
		else
		{
			PrevTranslucencyTexture = BlackAlphaOneDummy;
			PrevTranslucencyViewport = FScreenPassTextureViewport(FIntPoint(1, 1), FIntRect(FIntPoint(0, 0), FIntPoint(1, 1)));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			TranslucencyRejectionTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.TranslucencyRejection"));
		}

		FTSRCompareTranslucencyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRCompareTranslucencyCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;

		PassParameters->TranslucencyInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
			SeparateTranslucencyTexture->Desc.Extent, SeparateTranslucencyRect));
		PassParameters->PrevTranslucencyInfo = GetScreenPassTextureViewportParameters(PrevTranslucencyViewport);
		PassParameters->PrevTranslucencyPreExposureCorrection = PrevHistoryParameters.HistoryPreExposureCorrection;
		PassParameters->TranslucencyHighlightLuminance = -1.0; //CVarTSRTranslucencyHighlightLuminance.GetValueOnRenderThread();

		PassParameters->ScreenPosToPrevTranslucencyTextureUV = FScreenTransform::ChangeTextureBasisFromTo(
			PrevTranslucencyViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);

		PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
		PassParameters->TranslucencyTexture = SeparateTranslucencyTexture;
		PassParameters->PrevTranslucencyTexture = PrevTranslucencyTexture;

		PassParameters->TranslucencyRejectionOutput = GraphBuilder.CreateUAV(TranslucencyRejectionTexture);
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.CompareTranslucency"));

		TShaderMapRef<FTSRCompareTranslucencyCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR CompareTranslucency %dx%d", InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));
	}

	// Reject the history with frequency decomposition.
	FRDGTextureRef HistoryRejectionTexture;
	FRDGTextureRef InputSceneColorLdrLumaTexture = nullptr;
	{
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			InputSceneColorLdrLumaTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.SceneColorLdrLuma"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8G8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			HistoryRejectionTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.HistoryRejection"));
		}

		FScreenPassTextureViewport TranslucencyViewport(
			SeparateTranslucencyTexture->Desc.Extent, SeparateTranslucencyRect);

		FTSRRejectShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRRejectShadingCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->InputPixelPosToTranslucencyTextureUV =
			((FScreenTransform::Identity + 0.5f - InputRect.Min) / InputRect.Size()) *
			FScreenTransform::ChangeTextureBasisFromTo(TranslucencyViewport, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
		PassParameters->TranslucencyTextureUVMin = GetScreenPassTextureViewportParameters(TranslucencyViewport).UVViewportBilinearMin;
		PassParameters->TranslucencyTextureUVMax = GetScreenPassTextureViewportParameters(TranslucencyViewport).UVViewportBilinearMax;
		PassParameters->HistoryGuideQuantizationError = ComputePixelFormatQuantizationError(History.Guide->Desc.Format);
		PassParameters->FlickeringFramePeriod = FlickeringFramePeriod;
		PassParameters->TheoricBlendFactor = 1.0f / (1.0f + MaxHistorySampleCount / OutputToInputResolutionFractionSquare);

		PassParameters->InputTexture = PassInputs.SceneColorTexture;
		if (PassInputs.MoireInputTexture.IsValid())
		{
			ensure(InputRect == PassInputs.MoireInputTexture.ViewRect);
			PassParameters->InputMoireLumaTexture = PassInputs.MoireInputTexture.Texture;
		}
		else
		{
			PassParameters->InputMoireLumaTexture = BlackDummy;
		}
		if (bAccumulateTranslucencySeparately)
		{
			PassParameters->InputSceneTranslucencyTexture = BlackAlphaOneDummy;
		}
		else
		{
			PassParameters->InputSceneTranslucencyTexture = SeparateTranslucencyTexture;
		}
		PassParameters->ReprojectedHistoryGuideTexture = ReprojectedHistoryGuideTexture;
		PassParameters->ReprojectedHistoryMoireTexture = ReprojectedHistoryMoireTexture;
		PassParameters->ParallaxRejectionMaskTexture = ParallaxRejectionMaskTexture;
		PassParameters->IsMovingMaskTexture = IsMovingMaskTexture;

		PassParameters->HistoryGuideOutput = GraphBuilder.CreateUAV(History.Guide);
		PassParameters->HistoryMoireOutput = GraphBuilder.CreateUAV(History.Moire);
		PassParameters->HistoryRejectionOutput = GraphBuilder.CreateUAV(HistoryRejectionTexture);
		PassParameters->InputSceneColorLdrLumaOutput = GraphBuilder.CreateUAV(InputSceneColorLdrLumaTexture);
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.RejectShading"));

		FTSRRejectShadingCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRRejectShadingCS::FWaveSizeOps>(bUseWaveOps && GRHIMinimumWaveSize >= 32 && GRHIMinimumWaveSize <= 64 ? GRHIMinimumWaveSize : 0);
		PermutationVector.Set<FTSRRejectShadingCS::FFlickeringDetectionDim>(FlickeringFramePeriod > 0.0f);

		TShaderMapRef<FTSRRejectShadingCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR RejectShading(WaveSize=%d FlickeringFramePeriod=%f %s) %dx%d",
				int32(PermutationVector.Get<FTSRRejectShadingCS::FWaveSizeOps>()),
				FlickeringFramePeriod,
				!bAccumulateTranslucencySeparately ? TEXT(" ComposeTranslucency") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 12));
	}

	// Spatial anti-aliasing when doing history rejection.
	FRDGTextureRef AntiAliasingTexture = nullptr;
	FRDGTextureRef NoiseFilteringTexture = nullptr;
	if (RejectionAntiAliasingQuality > 0)
	{
		FRDGTextureRef RawAntiAliasingTexture;
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			RawAntiAliasingTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.AntiAliasing.Raw"));
			AntiAliasingTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.AntiAliasing.Filtered"));

			Desc.Format = PF_R8;
			NoiseFilteringTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.AntiAliasing.Noise"));
		}

		{
			FTSRSpatialAntiAliasingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRSpatialAntiAliasingCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->InputSceneColorTexture = PassInputs.SceneColorTexture;
			PassParameters->InputSceneColorLdrLumaTexture = InputSceneColorLdrLumaTexture;
			PassParameters->AntiAliasingOutput = GraphBuilder.CreateUAV(RawAntiAliasingTexture);
			PassParameters->NoiseFilteringOutput = GraphBuilder.CreateUAV(NoiseFilteringTexture);
			PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.SpatialAntiAliasing"));

			FTSRSpatialAntiAliasingCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTSRSpatialAntiAliasingCS::FQualityDim>(RejectionAntiAliasingQuality);

			TShaderMapRef<FTSRSpatialAntiAliasingCS> ComputeShader(View.ShaderMap, PermutationVector);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TSR SpatialAntiAliasing(Quality=%d) %dx%d",
					RejectionAntiAliasingQuality,
					InputRect.Width(), InputRect.Height()),
				AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));
		}

		{
			FTSRFilterAntiAliasingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRFilterAntiAliasingCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->AntiAliasingTexture = RawAntiAliasingTexture;
			PassParameters->AntiAliasingOutput = GraphBuilder.CreateUAV(AntiAliasingTexture);
			PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.FilterAntiAliasing"));

			TShaderMapRef<FTSRFilterAntiAliasingCS> ComputeShader(View.ShaderMap);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TSR FilterAntiAliasing %dx%d", InputRect.Width(), InputRect.Height()),
				AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));
		}
	}

	// Update temporal history.
	{
		static const TCHAR* const kUpdateQualityNames[] = {
			TEXT("Low"),
			TEXT("Medium"),
			TEXT("High"),
			TEXT("Epic"),
		};
		static_assert(UE_ARRAY_COUNT(kUpdateQualityNames) == int32(FTSRUpdateHistoryCS::EQuality::MAX), "Fix me!");

		FTSRUpdateHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRUpdateHistoryCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->InputSceneColorTexture = PassInputs.SceneColorTexture;
		PassParameters->InputSceneStencilTexture = GraphBuilder.CreateSRV(
			FRDGTextureSRVDesc::CreateWithPixelFormat(PassInputs.SceneDepthTexture, PF_X24_G8));
		PassParameters->InputSceneTranslucencyTexture = SeparateTranslucencyTexture;
		PassParameters->HistoryRejectionTexture = HistoryRejectionTexture;
		PassParameters->TranslucencyRejectionTexture = TranslucencyRejectionTexture ? TranslucencyRejectionTexture : BlackDummy;

		PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
		PassParameters->GrandDilatedVelocityTexture = GrandHoleFilledVelocityTexture;
		PassParameters->ParallaxFactorTexture = ParallaxFactorTexture;
		PassParameters->ParallaxRejectionMaskTexture = ParallaxRejectionMaskTexture;
		PassParameters->AntiAliasingTexture = AntiAliasingTexture;
		PassParameters->NoiseFilteringTexture = NoiseFilteringTexture;
		PassParameters->HoleFilledVelocityMaskTexture = HoleFilledVelocityMaskTexture;

		PassParameters->TranslucencyInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
			PassParameters->TranslucencyRejectionTexture->Desc.Extent, SeparateTranslucencyRect));
		PassParameters->TranslucencyPixelPosMin = PassParameters->TranslucencyInfo.ViewportMin;
		PassParameters->TranslucencyPixelPosMax = PassParameters->TranslucencyInfo.ViewportMax - 1;

		FScreenTransform HistoryPixelPosToViewportUV = (FScreenTransform::Identity + 0.5f) * CommonParameters.HistoryInfo.ViewportSizeInverse;
		PassParameters->HistoryPixelPosToScreenPos = HistoryPixelPosToViewportUV * FScreenTransform::ViewportUVToScreenPos;
		PassParameters->HistoryPixelPosToInputPPCo = HistoryPixelPosToViewportUV * CommonParameters.InputInfo.ViewportSize + CommonParameters.InputJitter + CommonParameters.InputPixelPosMin;
		PassParameters->HistoryPixelPosToTranslucencyPPCo = HistoryPixelPosToViewportUV * PassParameters->TranslucencyInfo.ViewportSize + CommonParameters.InputJitter * PassParameters->TranslucencyInfo.ViewportSize / CommonParameters.InputInfo.ViewportSize + SeparateTranslucencyRect.Min;
		PassParameters->HistoryQuantizationError = ComputePixelFormatQuantizationError((History.ColorArray ? History.ColorArray : History.Output)->Desc.Format);
		PassParameters->MinTranslucencyRejection = TranslucencyRejectionTexture == nullptr ? 1.0 : 0.0;

		// All parameters to control the sample count in history.
		PassParameters->HistorySampleCount = MaxHistorySampleCount / OutputToHistoryResolutionFractionSquare;
		PassParameters->HistoryHisteresis = 1.0f / PassParameters->HistorySampleCount;
		PassParameters->WeightClampingRejection = (CVarTSRHistoryRejectionSampleCount.GetValueOnRenderThread() / OutputToHistoryResolutionFractionSquare) * PassParameters->HistoryHisteresis;
		PassParameters->WeightClampingPixelSpeedAmplitude = FMath::Clamp(1.0f - CVarTSRWeightClampingSampleCount.GetValueOnRenderThread() * PassParameters->HistoryHisteresis, 0.0f, 1.0f);
		PassParameters->InvWeightClampingPixelSpeed = 1.0f / (CVarTSRWeightClampingPixelSpeed.GetValueOnRenderThread() * OutputToHistoryResolutionFraction);
		
		PassParameters->InputToHistoryFactor = float(HistorySize.X) / float(InputRect.Width());
		PassParameters->InputContributionMultiplier = OutputToHistoryResolutionFractionSquare; 
		PassParameters->GrandPrevPreExposureCorrection = bCameraCut ? 1.0f : View.PreExposure / InputHistory.PrevSceneColorPreExposure;
		//PassParameters->ResponsiveStencilMask = CVarTSREnableResponiveAA.GetValueOnRenderThread() ? (STENCIL_TEMPORAL_RESPONSIVE_AA_MASK) : 0;
		PassParameters->ResponsiveStencilMask = 0;
		PassParameters->bGenerateOutputMip1 = false;
		PassParameters->bGenerateOutputMip2 = false;
		PassParameters->bHasSeparateTranslucency = bHasSeparateTranslucency;

		PassParameters->PrevHistoryParameters = PrevHistoryParameters;
		PassParameters->PrevHistory = PrevHistorySRVs;
		PassParameters->GrandPrevColorTexture = GrandPrevColorTexture;

		PassParameters->HistoryOutput = CreateUAVs(GraphBuilder, History);
		if (HistorySize != OutputRect.Size() && bIsOutputDifferentThanHighFrequency)
		{
			PassParameters->SceneColorOutputMip0 = CreateDummyUAV(GraphBuilder, PF_FloatR11G11B10);
		}
		else
		{
			PassParameters->SceneColorOutputMip0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(UpdateHistoryOutputTexture, /* InMipLevel = */ 0));
		}
		
		if (PassInputs.bGenerateOutputMip1 && HistorySize == OutputRect.Size())
		{
			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputTexture, /* InMipLevel = */ 1));
		}
		else if (SceneColorOutputHalfResTexture && HistorySize == OutputRect.Size())
		{
			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputHalfResTexture));
		}
		else if (SceneColorOutputQuarterResTexture && HistorySize == OutputRect.Size())
		{
			PassParameters->bGenerateOutputMip2 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputQuarterResTexture));
		}
		else
		{
			PassParameters->SceneColorOutputMip1 = CreateDummyUAV(GraphBuilder, PF_FloatR11G11B10);
		}
		PassParameters->DebugOutput = CreateDebugUAV(HistoryExtent, TEXT("Debug.TSR.UpdateHistory"));

		FTSRUpdateHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRUpdateHistoryCS::FQualityDim>(UpdateHistoryQuality);
		PermutationVector.Set<FTSRUpdateHistoryCS::FSeparateTranslucencyDim>(bAccumulateTranslucencySeparately);
		PermutationVector.Set<FTSRUpdateHistoryCS::FGrandReprojectionDim>(bGrandReprojection);

		TShaderMapRef<FTSRUpdateHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR UpdateHistory(Quality=%s%s%s%s%s) %dx%d",
				kUpdateQualityNames[int32(PermutationVector.Get<FTSRUpdateHistoryCS::FQualityDim>())],
				PermutationVector.Get<FTSRUpdateHistoryCS::FSeparateTranslucencyDim>() ? TEXT(" SeparateTranslucency") : TEXT(""),
				PermutationVector.Get<FTSRUpdateHistoryCS::FGrandReprojectionDim>() ? TEXT(" GrandReprojection") : TEXT(""),
				(History.ColorArray ? History.ColorArray : History.Output)->Desc.Format == PF_FloatR11G11B10 ? TEXT(" R11G11B10") : TEXT(""),
				PassParameters->bGenerateOutputMip2 ? TEXT(" OutputMip2") : (PassParameters->bGenerateOutputMip1 ? TEXT(" OutputMip1") : TEXT("")),
				HistorySize.X, HistorySize.Y),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(HistorySize, 8));
	}

	// Debug the history.
	#if COMPILE_TSR_DEBUG_PASSES
	if (bSetupDebugPasses)
	{
		const int32 kHistoryUpscalingFactor = 2;

		FTSRDebugHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRDebugHistoryCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->History = CreateSRVs(GraphBuilder, History);
		PassParameters->PrevHistory = PrevHistorySRVs;
		PassParameters->DebugOutput = CreateDebugUAV(HistoryExtent * kHistoryUpscalingFactor, TEXT("Debug.TSR.History"));

		TShaderMapRef<FTSRDebugHistoryCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR DebugHistory %dx%d", HistorySize.X, HistorySize.Y),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(HistorySize * kHistoryUpscalingFactor, 8));
	}
	#endif

	// If we upscaled the history buffer, downsize back to the secondary screen percentage size.
	if (HistorySize != OutputRect.Size())
	{
		check(!SceneColorOutputQuarterResTexture);
		FTSRResolveHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRResolveHistoryCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->DispatchThreadToHistoryPixelPos = (
			FScreenTransform::DispatchThreadIdToViewportUV(OutputRect) *
			FScreenTransform::ChangeTextureBasisFromTo(
				HistoryExtent, FIntRect(FIntPoint(0, 0), HistorySize),
				FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TexelPosition));
		PassParameters->OutputViewRectMin = OutputRect.Min;
		PassParameters->OutputViewRectMax = OutputRect.Max;
		PassParameters->bGenerateOutputMip1 = false;
		PassParameters->HistoryValidityMultiply = float(HistorySize.X * HistorySize.Y) / float(OutputRect.Width() * OutputRect.Height());

		PassParameters->History = CreateSRVs(GraphBuilder, History);
		
		PassParameters->SceneColorOutputMip0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputTexture, /* InMipLevel = */ 0));
		if (PassInputs.bGenerateOutputMip1)
		{
			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputTexture, /* InMipLevel = */ 1));
		}
		else if (SceneColorOutputHalfResTexture)
		{
			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputHalfResTexture));
		}
		else
		{
			PassParameters->SceneColorOutputMip1 = CreateDummyUAV(GraphBuilder, PF_FloatR11G11B10);
		}
		PassParameters->DebugOutput = CreateDebugUAV(HistoryExtent, TEXT("Debug.TSR.ResolveHistory"));

		FTSRResolveHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRUpdateHistoryCS::FSeparateTranslucencyDim>(bAccumulateTranslucencySeparately);

		TShaderMapRef<FTSRResolveHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR ResolveHistory %dx%d", OutputRect.Width(), OutputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputRect.Size(), 8));
	}

	// Extract all resources for next frame.
	if (!View.bStatePrevViewInfoIsReadOnly)
	{
		FTSRHistory& OutputHistory = View.ViewState->PrevFrameViewInfo.TSRHistory;
		OutputHistory.InputViewportRect = InputRect;
		OutputHistory.OutputViewportRect = FIntRect(FIntPoint(0, 0), HistorySize);
		OutputHistory.FormatBit = uint32(HistoryFormatBits);
		OutputHistory.PrevSceneColorPreExposure = View.PrevViewInfo.SceneColorPreExposure;
		OutputHistory.PrevTemporalJitterPixels = CommonParameters.InputJitter;
		OutputHistory.PrevOutputViewportRect = InputHistory.OutputViewportRect;

		// Extract filterable history
		if (!bIsOutputDifferentThanHighFrequency)
		{
			GraphBuilder.QueueTextureExtraction(History.Output, &OutputHistory.Output);
		}
		if (History.ColorArray)
		{
			GraphBuilder.QueueTextureExtraction(History.ColorArray, &OutputHistory.ColorArray);
		}
		GraphBuilder.QueueTextureExtraction(History.Metadata, &OutputHistory.Metadata);
		if (bAccumulateTranslucencySeparately)
		{
			GraphBuilder.QueueTextureExtraction(History.TranslucencyAlpha, &OutputHistory.TranslucencyAlpha);
		}

		// Extract non-filterable history
		if (History.SubpixelDetails)
		{
			GraphBuilder.QueueTextureExtraction(History.SubpixelDetails, &OutputHistory.SubpixelDetails);
		}
		else if (History.SubpixelDepth)
		{
			GraphBuilder.QueueTextureExtraction(History.SubpixelDepth, &OutputHistory.SubpixelDepth);
		}

		// Extract history guide
		GraphBuilder.QueueTextureExtraction(History.Guide, &OutputHistory.Guide);

		if (FlickeringFramePeriod > 0.0f)
		{
			GraphBuilder.QueueTextureExtraction(History.Moire, &OutputHistory.Moire);
		}

		if (bGrandReprojection)
		{
			GraphBuilder.QueueTextureExtraction(DilatedVelocityTexture, &OutputHistory.Velocity);
			if (!bIsOutputDifferentThanHighFrequency)
			{
				if (PrevHistory.Output)
				{
					GraphBuilder.QueueTextureExtraction(PrevHistory.Output, &OutputHistory.PrevOutput);
				}
				else
				{
					GraphBuilder.QueueTextureExtraction(History.Output, &OutputHistory.PrevOutput);
				}
			}
			else
			{
				if (PrevHistory.ColorArray)
				{
					GraphBuilder.QueueTextureExtraction(PrevHistory.ColorArray, &OutputHistory.PrevColorArray);
				}
				else
				{
					GraphBuilder.QueueTextureExtraction(History.ColorArray, &OutputHistory.PrevColorArray);
				}
			}
		}

		// Extract the translucency buffer to compare it with next frame
		if (bRejectSeparateTranslucency)
		{
			GraphBuilder.QueueTextureExtraction(
				SeparateTranslucencyTexture, &View.ViewState->PrevFrameViewInfo.SeparateTranslucency);
			View.ViewState->PrevFrameViewInfo.SeparateTranslucencyViewRect = InputRect;
		}

		// Extract the output for next frame SSR so that separate translucency shows up in SSR.
		{
			GraphBuilder.QueueTextureExtraction(
				SceneColorOutputTexture, &View.ViewState->PrevFrameViewInfo.CustomSSRInput.RT[0]);

			View.ViewState->PrevFrameViewInfo.CustomSSRInput.ViewportRect = OutputRect;
			View.ViewState->PrevFrameViewInfo.CustomSSRInput.ReferenceBufferSize = OutputExtent;
		}
	}


	ITemporalUpscaler::FOutputs Outputs;
	Outputs.FullRes.Texture = SceneColorOutputTexture;
	Outputs.FullRes.ViewRect = OutputRect;
	if (SceneColorOutputHalfResTexture)
	{
		Outputs.HalfRes.Texture = SceneColorOutputHalfResTexture;
		Outputs.HalfRes.ViewRect.Min = OutputRect.Min / 2;
		Outputs.HalfRes.ViewRect.Max = Outputs.HalfRes.ViewRect.Min + FIntPoint::DivideAndRoundUp(OutputRect.Size(), 2);
	}
	if (SceneColorOutputQuarterResTexture)
	{
		Outputs.QuarterRes.Texture = SceneColorOutputQuarterResTexture;
		Outputs.QuarterRes.ViewRect.Min = OutputRect.Min / 4;
		Outputs.QuarterRes.ViewRect.Max = Outputs.HalfRes.ViewRect.Min + FIntPoint::DivideAndRoundUp(OutputRect.Size(), 4);
	}
	Outputs.VelocityFlattenTextures = VelocityFlattenTextures;
	return Outputs;
} // AddTemporalSuperResolutionPasses()
