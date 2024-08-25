// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/TemporalAA.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PostProcess/PostProcessTonemap.h"
#include "PostProcess/PostProcessing.h"
#include "ClearQuad.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneTextureParameters.h"
#include "PixelShaderUtils.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "ShaderPlatformCachedIniValue.h"
#include "PostProcess/PostProcessVisualizeBuffer.h"
#include "DynamicResolutionState.h"

#define COMPILE_TSR_DEBUG_PASSES (!UE_BUILD_SHIPPING)

namespace
{
	
TAutoConsoleVariable<int32> CVarTSRAlphaChannel(
	TEXT("r.TSR.AplhaChannel"), -1,
	TEXT("Controls whether TSR should process the scene color's alpha channel.\n")
	TEXT(" -1: based of r.PostProcessing.PropagateAlpha (default);\n")
	TEXT("  0: disabled;\n")
	TEXT("  1: enabled.\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRHistorySampleCount(
	TEXT("r.TSR.History.SampleCount"), 16.0f,
	TEXT("Maximum number sample for each output pixel in the history. Higher values means more stability on highlights on static images, ")
	TEXT("but may introduce additional ghosting on firefliers style of VFX. Minimum value supported is 8.0 as TSR was in 5.0 and 5.1. ")
	TEXT("Maximum value possible due to the encoding of the TSR.History.Metadata is 32.0. Defaults to 16.0.\n")
	TEXT("\n")
	TEXT("Use \"r.TSR.Visualize 0\" command to see how many samples where accumulated in TSR history on areas of the screen."),
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

TAutoConsoleVariable<int32> CVarTSRWaveOps(
	TEXT("r.TSR.WaveOps"), 1,
	TEXT("Whether to use wave ops in the shading rejection heuristics to speeds up convolutions.\n")
	TEXT("\n")
	TEXT("The shading rejection heuristic optimisation can be particularily hard for shader compiler and hit bug in them causing corruption/quality loss.\n")
	TEXT("\n")
	TEXT("Note this optimisation is currently disabled on SPIRV platforms (mainly Vulkan and Metal) due to 5min+ compilation times in SPIRV ")
	TEXT("backend of DXC which is not great for editor startup."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRWaveSize(
	TEXT("r.TSR.WaveSize"), 0,
	TEXT("Overrides the WaveSize to use.\n")
	TEXT(" 0: Automatic (default);\n")
	TEXT(" 16: WaveSizeOps 16;\n")
	TEXT(" 32: WaveSizeOps 32;\n")
	TEXT(" 64: WaveSizeOps 64;\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSR16BitVALU(
	TEXT("r.TSR.16BitVALU"), 1,
	TEXT("Whether to use 16bit VALU on platform that have bSupportsRealTypes=RuntimeDependent"),
	ECVF_RenderThreadSafe);

#if PLATFORM_DESKTOP

TAutoConsoleVariable<int32> CVarTSR16BitVALUOnAMD(
	TEXT("r.TSR.16BitVALU.AMD"), 1,
	TEXT("Overrides whether to use 16bit VALU on AMD desktop GPUs"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSR16BitVALUOnIntel(
	TEXT("r.TSR.16BitVALU.Intel"), 1,
	TEXT("Overrides whether to use 16bit VALU on Intel desktop GPUs"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSR16BitVALUOnNvidia(
	TEXT("r.TSR.16BitVALU.Nvidia"), 0,
	TEXT("Overrides whether to use 16bit VALU on Nvidia desktop GPUs"),
	ECVF_RenderThreadSafe);

#endif // PLATFORM_DESKTOP

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
	TEXT("When enabled, this flickering temporal analysis monitor how the luminance of the scene right before any translucency drawing stored in the ")
	TEXT("TSR.Flickering.Luminance resource how it involves over successive frames. And if it is detected to constantly flicker regularily above a certain ")
	TEXT("threshold defined with this r.TSR.ShadingRejection.Flickering.* cvars, the heuristic attempts to stabilize the image by letting ghost within ")
	TEXT("luminance boundary tied to the amplititude of flickering.\n")
	TEXT("\n")
	TEXT("Use \"r.TSR.Visualize 7\" command to see on screen where this heuristic quicks in orange and red. Pink is where it is disabled.\n")
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
	TEXT("r.TSR.ShadingRejection.Flickering.Period"), 2.0f,
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

TAutoConsoleVariable<int32> CVarTSRShadingTileOverscan(
	TEXT("r.TSR.ShadingRejection.TileOverscan"), 3,
	TEXT("The shading rejection run a network of convolutions on the GPU all in single 32x32 without roundtrip to main video memory. ")
	TEXT("However chaining many convlutions in this tiles means that some convolutions on the edge arround are becoming corrupted ")
	TEXT("and therefor need to overlap the tile by couple of padding to hide it. Higher means less prones to tiling artifacts, but performance loss."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRShadingExposureOffset(
	TEXT("r.TSR.ShadingRejection.ExposureOffset"), 3.0,
	TEXT("The shading rejection needs to have a representative idea how bright a linear color pixel ends up displayed to the user. ")
	TEXT("And the shading rejection detect if a color become to changed to be visible in the back buffer by comparing to MeasureBackbufferLDRQuantizationError().\n")
	TEXT("\n")
	TEXT("It is important to have TSR's MeasureBackbufferLDRQuantizationError() ends up distributed uniformly across ")
	TEXT("the range of color intensity or it could otherwise disregard some subtle VFX causing ghostin.\n")
	TEXT("\n")
	TEXT("This controls adjusts the exposure of the linear color space solely in the TSR's rejection heuristic, such that higher value ")
	TEXT("lifts the shadows's LDR intensity, meaning MeasureBackbufferLDRQuantizationError() is decreased in these shadows and increased in ")
	TEXT("the highlights, control directly.\n")
	TEXT("\n")
	TEXT("The best TSR internal buffer to verify this is TSR.Flickering.Luminance, either with the \"show VisualizeTemporalUpscaler\" command or in DumpGPU ")
	TEXT("with the RGB Linear[0;1] source color space against the Tonemaper's output in sRGB source color space.\n"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRRejectionAntiAliasingQuality(
	TEXT("r.TSR.RejectionAntiAliasingQuality"), 3,
	TEXT("Controls the quality of TSR's built-in spatial anti-aliasing technology when the history needs to be rejected. ")
	TEXT("While this may not be critical when the rendering resolution is not much lowered than display resolution, ")
	TEXT("this technic however becomes essential to hide lower rendering resolution rendering because of two reasons:\n")
	TEXT(" - the screen space size of aliasing is inverse proportional to rendering resolution;\n")
	TEXT(" - rendering at lower resolution means need more frame to reach at least 1 rendered pixel per display pixel.\n")
	TEXT("\n")
	TEXT("Use \"r.TSR.Visualize 6\" command to see on screen where the spatial anti-aliaser quicks in green.\n")
	TEXT("\n")
	TEXT("By default, it is only disabled by default in the low anti-aliasing scalability group."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRResurrectionEnable(
	TEXT("r.TSR.Resurrection"), 0,
	TEXT("Allows TSR to resurrect previously discarded details from many frames ago.\n")
	TEXT("\n")
	TEXT("When enabled, the entire frames of the TSR are stored in a same unique Texture2DArray including a configurable ")
	TEXT("number of persistent frame (defined by r.TSR.Resurrection.PersistentFrameCount) that are occasionally recorded ")
	TEXT("(defined by r.TSR.Resurrection.PersistentFrameInterval).")
	TEXT("\n")
	TEXT("Then every frame, TSR will attempt to reproject either previous frame, or the oldest persistent frame available based ")
	TEXT("which matches best the current frames. The later option will happen when something previously seen by TSR shows up ")
	TEXT("again (no matter through parallax disocclusion, shading changes, translucent VFX moving) which will have the advantage ")
	TEXT("bypass the need to newly accumulate a second time by simply resurrected the previously accumulated details.\n")
	TEXT("\n")
	TEXT("Command \"r.TSR.Visualize 4\" too see parts of the screen is being resurrected by TSR in green.\n")
	TEXT("Command \"r.TSR.Visualize 5\" too see the oldest frame being possibly resurrected.\n")
	TEXT("\n")
	TEXT("Currently experimental and disabled by default."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRResurrectionPersistentFrameCount(
	TEXT("r.TSR.Resurrection.PersistentFrameCount"), 2,
	TEXT("Configures the number of persistent frame to record in history for futur history resurrection. ")
	TEXT("This will increase the memory footprint of the entire TSR history. ")
	TEXT("Must be an even number greater or equal to 2. (default=2)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRResurrectionPersistentFrameInterval(
	TEXT("r.TSR.Resurrection.PersistentFrameInterval"), 31,
	TEXT("Configures in number of frames how often persistent frame should be recorded in history for futur history resurrection. ")
	TEXT("This has no implication on memory footprint of the TSR history. Must be an odd number greater or equal to 1. ")
	TEXT("Uses the VisualizeTSR show flag and r.TSR.Visualize=5 to tune this parameter to your content. ")
	TEXT("(default=31)"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarTSRAsyncCompute(
	TEXT("r.TSR.AsyncCompute"), 2,
	TEXT("Controls how TSR run on async compute. Some TSR passes can overlap with previous passes.\n")
	TEXT(" 0: Disabled;\n")
	TEXT(" 1: Run on async compute only passes that are completly independent from any intermediary resource of this frame, namely ClearPrevTextures and ForwardScatterDepth passes;\n")
	TEXT(" 2: Run on async compute only passes that are completly independent or only dependent on the depth and velocity buffer which can overlap for instance with translucency or DOF. Any passes on critical path remains on the graphics queue (default);\n")
	TEXT(" 3: Run all passes on async compute;"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarTSRWeightClampingSampleCount(
	TEXT("r.TSR.Velocity.WeightClampingSampleCount"), 4.0f,
	TEXT("Number of sample to count to in history pixel to clamp history to when output pixel velocity reach r.TSR.Velocity.WeightClampingPixelSpeed. ")
	TEXT("Higher value means higher stability on movement, but at the expense of additional blur due to successive convolution of each history reprojection.\n")
	TEXT("\n")
	TEXT("Use \"r.TSR.Visualize 0\" command to see how many samples where accumulated in TSR history on areas of the screen.\n")
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

#if !UE_BUILD_OPTIMIZED_SHOWFLAGS

TAutoConsoleVariable<int32> CVarTSRVisualize(
	TEXT("r.TSR.Visualize"), -1,
	TEXT("Selects the TSR internal visualization mode.\n")
	TEXT(" -2: Display an overview grid based regardless of VisualizeTSR show flag;\n")
	TEXT(" -1: Display an overview grid based on the VisualizeTSR show flag (default, opened with the `show VisualizeTSR` command at runtime or Show > Visualize > TSR in editor viewports);\n")
	TEXT("  0: Number of accumulated samples in the history, particularily interesting to tune r.TSR.ShadingRejection.SampleCount and r.TSR.Velocity.WeightClampingSampleCount;\n")
	TEXT("  1: Parallax disocclusion based of depth and velocity buffers;\n")
	TEXT("  2: Mask where the history is rejected;\n")
	TEXT("  3: Mask where the history is clamped;\n")
	TEXT("  4: Mask where the history is resurrected (with r.TSR.Resurrection=1);\n")
	TEXT("  5: Mask where the history is resurrected in the resurrected frame (with r.TSR.Resurrection=1), particularily interesting to tune r.TSR.Resurrection.PersistentFrameInterval;\n")
	TEXT("  6: Mask where spatial anti-aliasing is being computed;\n")
	TEXT("  7: Mask where the flickering temporal analysis heuristic is taking effects (with r.TSR.ShadingRejection.Flickering=1);\n"),
	ECVF_RenderThreadSafe);

#endif

#if COMPILE_TSR_DEBUG_PASSES

TAutoConsoleVariable<int32> CVarTSRDebugArraySize(
	TEXT("r.TSR.Debug.ArraySize"), 1,
	TEXT("Size of array for the TSR.Debug.* RDG textures"),
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
	SHADER_PARAMETER(int32, Size)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRHistoryTextures, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, ColorArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, MetadataArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, GuideArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, MoireArray)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FTSRPrevHistoryParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevHistoryInfo)
	SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevHistoryBufferUV)
	SHADER_PARAMETER(float, HistoryPreExposureCorrection)
	SHADER_PARAMETER(float, ResurrectionPreExposureCorrection)
END_SHADER_PARAMETER_STRUCT()

enum class ETSRHistoryFormatBits : uint32
{
	None = 0,
	Moire = 1 << 0,
	AlphaChannel = 1 << 1,
};
ENUM_CLASS_FLAGS(ETSRHistoryFormatBits);

FTSRHistoryArrayIndices TranslateHistoryFormatBitsToArrayIndices(ETSRHistoryFormatBits HistoryFormatBits)
{
	FTSRHistoryArrayIndices ArrayIndices;
	ArrayIndices.Size = 1;
	ArrayIndices.HighFrequency = 0;
	return ArrayIndices;
}

class FTSRShader : public FGlobalShader
{
public:
	static constexpr int32 kSupportMinWaveSize = 32;
	static constexpr int32 kSupportMaxWaveSize = 64;

	class F16BitVALUDim : SHADER_PERMUTATION_BOOL("DIM_16BIT_VALU");
	class FAlphaChannelDim : SHADER_PERMUTATION_BOOL("DIM_ALPHA_CHANNEL");

	FTSRShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{ }

	FTSRShader()
	{ }

	static ERHIFeatureSupport Supports16BitVALU(EShaderPlatform Platform)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsRealTypes(Platform);
	}

	static bool ShouldCompile32or16BitPermutation(EShaderPlatform Platform, bool bIs16BitVALUPermutation)
	{
		// Always compile the 32bit permutations for the alpha channel
		if (!bIs16BitVALUPermutation)
		{
			return true;
		}

		const ERHIFeatureSupport Support = FTSRShader::Supports16BitVALU(Platform);
		return Support != ERHIFeatureSupport::Unsupported;
	}

	static ERHIFeatureSupport SupportsWaveOps(EShaderPlatform Platform)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Platform);
	}

	static bool SupportsLDS(EShaderPlatform Platform)
	{
		// Always support LDS on preview platform 
		if (FDataDrivenShaderPlatformInfo::GetIsPreviewPlatform(Platform))
		{
			return true;
		}

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
		if (FTSRShader::Supports16BitVALU(Parameters.Platform) == ERHIFeatureSupport::RuntimeGuaranteed)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
		OutEnvironment.CompilerFlags.Add(CFLAG_WarningsAsErrors);
		OutEnvironment.CompilerFlags.Add(CFLAG_HLSL2021);
		OutEnvironment.CompilerFlags.Add(CFLAG_ForceOptimization);
	}
}; // class FTemporalSuperResolutionShader

class FTSRConvolutionNetworkShader : public FTSRShader
{
public:
	class FWaveSizeOps : SHADER_PERMUTATION_SPARSE_INT("DIM_WAVE_SIZE", 0, 16, 32, 64);

	using FPermutationDomain = TShaderPermutationDomain<FWaveSizeOps, FTSRShader::F16BitVALUDim, FTSRShader::FAlphaChannelDim>;

	FTSRConvolutionNetworkShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FTSRShader(Initializer)
	{ }

	FTSRConvolutionNetworkShader()
	{ }

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Only compile the alpha channel with 32bit ops, as this is mostly targeting enterprise uses on Quadro GPUs
		if (PermutationVector.Get<FTSRShader::FAlphaChannelDim>())
		{
			PermutationVector.Set<FTSRShader::F16BitVALUDim>(false);
		}

		// Optimising register pressure with 16bit for waveops that is 1 pixel/lane is pointless.
		if (PermutationVector.Get<FWaveSizeOps>() == 0)
		{
			PermutationVector.Set<FTSRShader::F16BitVALUDim>(false);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters, FPermutationDomain PermutationVector)
	{
		if (!FTSRShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}

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

		if (!FTSRShader::ShouldCompile32or16BitPermutation(Parameters.Platform, PermutationVector.Get<FTSRShader::F16BitVALUDim>()))
		{
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, const FPermutationDomain& PermutationVector, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		if (PermutationVector.Get<FWaveSizeOps>() != 0)
		{
			if (PermutationVector.Get<FWaveSizeOps>() == 32)
			{
				OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
			}
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}

		if (PermutationVector.Get<FTSRShader::F16BitVALUDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
}; // FTSRConvolutionNetworkShader

class FTSRMeasureFlickeringLumaCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRMeasureFlickeringLumaCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRMeasureFlickeringLumaCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, InputInfo)
		SHADER_PARAMETER(float, PerceptionAdd)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, FlickeringLumaOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRMeasureFlickeringLumaCS

class FTSRClearPrevTexturesCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRClearPrevTexturesCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRClearPrevTexturesCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, PrevAtomicOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRClearPrevTexturesCS

class FTSRDilateVelocityCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRDilateVelocityCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRDilateVelocityCS, FTSRShader);

	class FMotionBlurDirectionsDim : SHADER_PERMUTATION_INT("DIM_MOTION_BLUR_DIRECTIONS", 3);
	using FPermutationDomain = TShaderPermutationDomain<FMotionBlurDirectionsDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVelocityFlattenParameters, VelocityFlattenParameters)

		SHADER_PARAMETER(FMatrix44f, RotationalClipToPrevClip)
		SHADER_PARAMETER(FVector2f, PrevOutputBufferUVMin)
		SHADER_PARAMETER(FVector2f, PrevOutputBufferUVMax)
		SHADER_PARAMETER(float, VelocityExtrapolationMultiplier)
		SHADER_PARAMETER(float, InvFlickeringMaxParralaxVelocity)
		SHADER_PARAMETER(int32, bOutputIsMovingTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneVelocityTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DilatedVelocityOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ClosestDepthOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, PrevAtomicOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, R8Output)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, VelocityFlattenOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, VelocityTileArrayOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector != RemapPermutation(PermutationVector))
		{
			return false;
		}

		if (!FTSRShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}
		return true;
	}
}; // class FTSRDilateVelocityCS

class FTSRDecimateHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRDecimateHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRDecimateHistoryCS, FTSRShader);

	class FMoireReprojectionDim : SHADER_PERMUTATION_BOOL("DIM_MOIRE_REPROJECTION");
	class FResurrectionReprojectionDim : SHADER_PERMUTATION_BOOL("DIM_RESURRECTION_REPROJECTION");
	using FPermutationDomain = TShaderPermutationDomain<FMoireReprojectionDim, FResurrectionReprojectionDim, FTSRShader::F16BitVALUDim, FTSRShader::FAlphaChannelDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FMatrix44f, RotationalClipToPrevClip)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, PrevAtomicTextureArray)

		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRPrevHistoryParameters, PrevHistoryParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryGuide)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PrevHistoryMoire)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, PrevGuideInfo)
		SHADER_PARAMETER(FScreenTransform, InputPixelPosToReprojectScreenPos)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToPrevHistoryGuideBufferUV)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToResurrectionGuideBufferUV)
		SHADER_PARAMETER(FVector2f, ResurrectionGuideUVViewportBilinearMin)
		SHADER_PARAMETER(FVector2f, ResurrectionGuideUVViewportBilinearMax)
		SHADER_PARAMETER(FVector3f, HistoryGuideQuantizationError)
		SHADER_PARAMETER(float, PerceptionAdd)
		SHADER_PARAMETER(float, ResurrectionFrameIndex)
		SHADER_PARAMETER(float, PrevFrameIndex)
		SHADER_PARAMETER(FMatrix44f, ClipToResurrectionClip)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, ReprojectedHistoryGuideOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, ReprojectedHistoryMoireOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HoleFilledVelocityOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DecimateMaskOutput)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FTSRShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		if (!FTSRShader::ShouldCompile32or16BitPermutation(Parameters.Platform, PermutationVector.Get<FTSRShader::F16BitVALUDim>()))
		{
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FTSRShader::F16BitVALUDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
}; // class FTSRDecimateHistoryCS

class FTSRRejectShadingCS : public FTSRConvolutionNetworkShader
{
	DECLARE_GLOBAL_SHADER(FTSRRejectShadingCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRRejectShadingCS, FTSRConvolutionNetworkShader);

	class FFlickeringDetectionDim : SHADER_PERMUTATION_BOOL("DIM_FLICKERING_DETECTION");
	class FHistoryResurrectionDim : SHADER_PERMUTATION_BOOL("DIM_HISTORY_RESURRECTION");

	using FPermutationDomain = TShaderPermutationDomain<
		FTSRConvolutionNetworkShader::FPermutationDomain,
		FFlickeringDetectionDim,
		FHistoryResurrectionDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FScreenTransform, InputPixelPosToTranslucencyTextureUV)
		SHADER_PARAMETER(FVector2f, TranslucencyTextureUVMin)
		SHADER_PARAMETER(FVector2f, TranslucencyTextureUVMax)
		SHADER_PARAMETER(FMatrix44f, ClipToResurrectionClip)
		SHADER_PARAMETER(FVector3f, HistoryGuideQuantizationError)
		SHADER_PARAMETER(float, FlickeringFramePeriod)
		SHADER_PARAMETER(float, TheoricBlendFactor)
		SHADER_PARAMETER(int32, TileOverscan)
		SHADER_PARAMETER(float, PerceptionAdd)
		SHADER_PARAMETER(int32, bEnableResurrection)
		SHADER_PARAMETER(int32, bEnableFlickeringHeuristic)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputMoireLumaTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneTranslucencyTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ReprojectedHistoryGuideTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ReprojectedHistoryGuideMetadataTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ReprojectedHistoryMoireTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ResurrectedHistoryGuideTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ResurrectedHistoryGuideMetadataTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DecimateMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, IsMovingMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestDepthTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, HistoryGuideOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, HistoryMoireOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, HistoryRejectionOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DilatedVelocityOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, InputSceneColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, InputSceneColorLdrLumaOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, AntiAliasMaskOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Remap redondant convolution permutations.
		PermutationVector.Set<FTSRConvolutionNetworkShader::FPermutationDomain>(
			FTSRConvolutionNetworkShader::RemapPermutation(PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>()));

		// Register pressure is identical between all these permutation with 16bit
		if (PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>().Get<FTSRShader::F16BitVALUDim>())
		{
			PermutationVector.Set<FFlickeringDetectionDim>(true);
			PermutationVector.Set<FHistoryResurrectionDim>(true);
		}

		// Flickering detection is on sg.AntiAliasQuality>=2 which also have resurrection.
		if (PermutationVector.Get<FFlickeringDetectionDim>())
		{
			PermutationVector.Set<FHistoryResurrectionDim>(true);
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

		if (!FTSRConvolutionNetworkShader::ShouldCompilePermutation(Parameters, PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>()))
		{
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FTSRConvolutionNetworkShader::ModifyCompilationEnvironment(
			Parameters,
			PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>(),
			OutEnvironment);
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
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AntiAliasMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorLdrLumaTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, AntiAliasingOutput)
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

	using FPermutationDomain = TShaderPermutationDomain<FQualityDim, FTSRShader::F16BitVALUDim, FTSRShader::FAlphaChannelDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneTranslucencyTexture)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryRejectionTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AntiAliasingTexture)

		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, TranslucencyInfo)
		SHADER_PARAMETER(FIntPoint, TranslucencyPixelPosMin)
		SHADER_PARAMETER(FIntPoint, TranslucencyPixelPosMax)

		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToScreenPos)
		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToInputPPCo)
		SHADER_PARAMETER(FScreenTransform, HistoryPixelPosToTranslucencyPPCo)

		SHADER_PARAMETER(FVector3f, HistoryQuantizationError)
		SHADER_PARAMETER(float, HistorySampleCount)
		SHADER_PARAMETER(float, HistoryHisteresis)
		SHADER_PARAMETER(float, WeightClampingRejection)
		SHADER_PARAMETER(float, WeightClampingPixelSpeedAmplitude)
		SHADER_PARAMETER(float, InvWeightClampingPixelSpeed)
		SHADER_PARAMETER(float, InputToHistoryFactor)
		SHADER_PARAMETER(float, InputContributionMultiplier)
		SHADER_PARAMETER(float, ResurrectionFrameIndex)
		SHADER_PARAMETER(float, PrevFrameIndex)
		SHADER_PARAMETER(int32, bGenerateOutputMip1)
		SHADER_PARAMETER(int32, bGenerateOutputMip2)
		SHADER_PARAMETER(int32, bGenerateOutputMip3)
		SHADER_PARAMETER(int32, bHasSeparateTranslucency)

		SHADER_PARAMETER_STRUCT(FTSRHistoryArrayIndices, HistoryArrayIndices)
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRPrevHistoryParameters, PrevHistoryParameters)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray, PrevHistoryColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray, PrevHistoryMetadataTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, HistoryColorOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, HistoryMetadataOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, SceneColorOutputMip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!FTSRShader::ShouldCompile32or16BitPermutation(Parameters.Platform, PermutationVector.Get<FTSRShader::F16BitVALUDim>()))
		{
			return false;
		}

		return FTSRShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FTSRShader::F16BitVALUDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
}; // class FTSRUpdateHistoryCS

class FTSRResolveHistoryCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRResolveHistoryCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRResolveHistoryCS, FTSRShader);

	class FNyquistDim : SHADER_PERMUTATION_SPARSE_INT("DIM_NYQUIST_WAVE_SIZE", 0, 16, 32);

	using FPermutationDomain = TShaderPermutationDomain<FNyquistDim, FTSRShader::F16BitVALUDim, FTSRShader::FAlphaChannelDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER(FScreenTransform, DispatchThreadToHistoryPixelPos)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMin)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMax)
		SHADER_PARAMETER(int32, bGenerateOutputMip1)
		SHADER_PARAMETER(float, HistoryValidityMultiply)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, UpdateHistoryOutputTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, SceneColorOutputMip1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		int32 WaveSize = PermutationVector.Get<FNyquistDim>();

		// WaveSize=16 is for Intel Arc GPU which also supports 16bits ops, so compiling WaveSize=16 32bit ops is useless and should instead fall back to WaveSize=0.
		if (WaveSize == 16 && !PermutationVector.Get<FTSRShader::F16BitVALUDim>())
		{
			PermutationVector.Set<FNyquistDim>(0);
		}

		if (WaveSize == 0)
		{
			PermutationVector.Set<FTSRShader::F16BitVALUDim>(false);
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

		if (PermutationVector.Get<FNyquistDim>())
		{
			int32 WaveSize = PermutationVector.Get<FNyquistDim>();

			if (FTSRShader::SupportsWaveOps(Parameters.Platform) == ERHIFeatureSupport::Unsupported)
			{
				return false;
			}

			if (WaveSize < int32(FDataDrivenShaderPlatformInfo::GetMinimumWaveSize(Parameters.Platform)) ||
				WaveSize > int32(FDataDrivenShaderPlatformInfo::GetMaximumWaveSize(Parameters.Platform)))
			{
				return false;
			}
		}

		if (!FTSRShader::ShouldCompile32or16BitPermutation(Parameters.Platform, PermutationVector.Get<FTSRShader::F16BitVALUDim>()))
		{
			return false;
		}

		return FTSRShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		FTSRShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		if (PermutationVector.Get<FNyquistDim>() != 0)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}

		if (PermutationVector.Get<FTSRShader::F16BitVALUDim>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
		}
	}
}; // class FTSRResolveHistoryCS

class FTSRVisualizeCS : public FTSRShader
{
	DECLARE_GLOBAL_SHADER(FTSRVisualizeCS);
	SHADER_USE_PARAMETER_STRUCT(FTSRVisualizeCS, FTSRShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRCommonParameters, CommonParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FTSRPrevHistoryParameters, PrevHistoryParameters)
		SHADER_PARAMETER(FScreenTransform, OutputPixelPosToScreenPos)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToHistoryUV)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToInputPixelPos)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToInputUV)
		SHADER_PARAMETER(FScreenTransform, ScreenPosToMoireHistoryUV)
		SHADER_PARAMETER(FVector2f, MoireHistoryUVBilinearMin)
		SHADER_PARAMETER(FVector2f, MoireHistoryUVBilinearMax)
		SHADER_PARAMETER(FMatrix44f, ClipToResurrectionClip)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMin)
		SHADER_PARAMETER(FIntPoint, OutputViewRectMax)
		SHADER_PARAMETER(int32, VisualizeId)
		SHADER_PARAMETER(int32, bCanResurrectHistory)
		SHADER_PARAMETER(int32, bCanSpatialAntiAlias)
		SHADER_PARAMETER(float, MaxHistorySampleCount)
		SHADER_PARAMETER(float, OutputToHistoryResolutionFractionSquare)
		SHADER_PARAMETER(float, FlickeringFramePeriod)
		SHADER_PARAMETER(float, PerceptionAdd)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputMoireLumaTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputSceneTranslucencyTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneColorTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DilatedVelocityTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, IsMovingMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryRejectionTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, MoireHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AntiAliasMaskTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, HistoryMetadataTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ResurrectedHistoryColorTexture)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, Output)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, DebugOutput)
	END_SHADER_PARAMETER_STRUCT()
}; // class FTSRVisualizeCS

IMPLEMENT_GLOBAL_SHADER(FTSRMeasureFlickeringLumaCS, "/Engine/Private/TemporalSuperResolution/TSRMeasureFlickeringLuma.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRClearPrevTexturesCS,     "/Engine/Private/TemporalSuperResolution/TSRClearPrevTextures.usf",     "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRDilateVelocityCS,        "/Engine/Private/TemporalSuperResolution/TSRDilateVelocity.usf",        "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRDecimateHistoryCS,       "/Engine/Private/TemporalSuperResolution/TSRDecimateHistory.usf",       "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRRejectShadingCS,         "/Engine/Private/TemporalSuperResolution/TSRRejectShading.usf",         "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRSpatialAntiAliasingCS,   "/Engine/Private/TemporalSuperResolution/TSRSpatialAntiAliasing.usf",   "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRUpdateHistoryCS,         "/Engine/Private/TemporalSuperResolution/TSRUpdateHistory.usf",         "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRResolveHistoryCS,        "/Engine/Private/TemporalSuperResolution/TSRResolveHistory.usf",        "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTSRVisualizeCS,             "/Engine/Private/TemporalSuperResolution/TSRVisualize.usf",             "MainCS", SF_Compute);

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

static FRDGTextureUAVRef CreateDummyUAVArray(FRDGBuilder& GraphBuilder, EPixelFormat PixelFormat)
{
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
		FIntPoint(1, 1),
		PixelFormat,
		FClearValueBinding::None,
		/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
		/* ArraySize = */ 1);

	FRDGTextureRef DummyTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.DummyOutput"));
	GraphBuilder.RemoveUnusedTextureWarning(DummyTexture);

	return GraphBuilder.CreateUAV(DummyTexture);
};

struct FTSRHistorySliceSequence
{
	static constexpr int32 kTransientSliceCount = 2;

	int32 FrameStorageCount = 1;
	int32 FrameStoragePeriod = 1;

	bool Check() const
	{
		check(FrameStorageCount == 1 || ((FrameStorageCount >= 4) && (FrameStorageCount % 2) == 0));
		check((FrameStoragePeriod % 2) == 1);
		return true;
	}
	
	/** Returns the total number of rolling indices. */
	int32 GetRollingIndexCount() const
	{
		if (FrameStorageCount == 1)
		{
			check(FrameStoragePeriod == 1);
			return 1;
		}
		else if (FrameStoragePeriod == 1)
		{
			return FrameStorageCount;
		}

		const int32 TransientIndexCount = kTransientSliceCount;
		const int32 PersistentIndexCount = FrameStorageCount - TransientIndexCount;

		return PersistentIndexCount * FrameStoragePeriod;
	}

	/** Returns a rolling index incremented by one. */
	int32 IncrementFrameRollingIndex(int32 PrevFrameRollingIndex) const
	{
		return (PrevFrameRollingIndex + 1) % GetRollingIndexCount();
	}

	/** Returns a rolling index incremented by one. */
	int32 DecrementFrameRollingIndex(int32 CurrentFrameRollingIndex) const
	{
		return (CurrentFrameRollingIndex + GetRollingIndexCount() - 1) % GetRollingIndexCount();
	}

	/** Returns a rolling index incremented by one. */
	int32 RollingIndexToSliceIndex(int32 FrameRollingIndex) const
	{
		if (FrameStorageCount == 1)
		{
			check(FrameRollingIndex == 0);
			check(FrameStoragePeriod == 1);
			return 0;
		}
		else if (FrameStoragePeriod == 1)
		{
			return (FrameRollingIndex % 2) * (FrameStorageCount / 2) + (FrameRollingIndex / 2) % (FrameStorageCount / 2);
		}

		const int32 TransientIndexCount = kTransientSliceCount;
		const int32 PersistentIndexCount = FrameStorageCount - TransientIndexCount;
		//const int32 FrameRollingIndexCount = PersistentIndexCount * FrameStoragePeriod;
		//check(FrameRollingIndex >= 0 && FrameRollingIndex < FrameRollingIndexCount);

		const bool bIsPersistentRollingIndex = (FrameRollingIndex % FrameStoragePeriod) == 0;
		if (bIsPersistentRollingIndex)
		{
			const int32 PersistentIndex = FrameRollingIndex / FrameStoragePeriod;

			return (PersistentIndex % 2)
				? ((FrameStorageCount / 2) + (PersistentIndex / 2))
				: ((FrameStorageCount / 2) - (PersistentIndex / 2) - 1);
		}
		else
		{
			return (FrameRollingIndex % 2) ? (FrameStorageCount - 1) : 0;
		}
	}

	int32 GetResurrectionFrameRollingIndex(int32 AccumulatedFrameCount, int32 LastFrameRollingIndex) const
	{
		const int32 RollingIndexCount = GetRollingIndexCount();

		if (FrameStorageCount == 1)
		{
			check(FrameStoragePeriod == 1);
			return 0;
		}
		else if (FrameStoragePeriod == 1)
		{
			return (RollingIndexCount + LastFrameRollingIndex - FMath::DivideAndRoundUp(FMath::Max(AccumulatedFrameCount - 2, 0), 2) * 2) % RollingIndexCount;
		}
		
		if (AccumulatedFrameCount < RollingIndexCount)
		{
			return 0;
		}

		return (FMath::DivideAndRoundUp(LastFrameRollingIndex + FrameStoragePeriod, FrameStoragePeriod) * FrameStoragePeriod) % RollingIndexCount;
	}

	FRHIRange16 GetSRVSliceRange(int32 CurrentFrameSliceIndex, int32 PrevFrameSliceIndex) const
	{
		check(CurrentFrameSliceIndex != PrevFrameSliceIndex);
		return (PrevFrameSliceIndex > CurrentFrameSliceIndex)
			? FRHIRange16(CurrentFrameSliceIndex + 1, FrameStorageCount - CurrentFrameSliceIndex - 1)
			: FRHIRange16(0, CurrentFrameSliceIndex);
	}
};

bool NeedTSRMoireLuma(const FViewInfo& View)
{
	return GetMainTAAPassConfig(View) == EMainTAAPassConfig::TSR;
}

bool IsVisualizeTSREnabled(const FViewInfo& View)
#if UE_BUILD_OPTIMIZED_SHOWFLAGS
{
	return false;
}
#else
{
	int32 VisualizeSettings = CVarTSRVisualize.GetValueOnRenderThread();
	return GetMainTAAPassConfig(View) == EMainTAAPassConfig::TSR && (View.Family->EngineShowFlags.VisualizeTSR || VisualizeSettings != -1);
}
#endif

FScreenPassTexture AddTSRMeasureFlickeringLuma(FRDGBuilder& GraphBuilder, FGlobalShaderMap* ShaderMap, FScreenPassTexture SceneColor)
{
	check(SceneColor.Texture)
	RDG_GPU_STAT_SCOPE(GraphBuilder, TemporalSuperResolution);

	FScreenPassTexture FlickeringLuma;
	{
		FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
			SceneColor.Texture->Desc.Extent,
			PF_R8,
			FClearValueBinding::None,
			/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

		FlickeringLuma.Texture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Flickering.Luminance"));
		FlickeringLuma.ViewRect = SceneColor.ViewRect;
	}

	FTSRMeasureFlickeringLumaCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRMeasureFlickeringLumaCS::FParameters>();
	PassParameters->InputInfo = GetScreenPassTextureViewportParameters(FScreenPassTextureViewport(
		SceneColor.Texture->Desc.Extent, SceneColor.ViewRect));
	PassParameters->PerceptionAdd = FMath::Pow(0.5f, CVarTSRShadingExposureOffset.GetValueOnRenderThread());
	PassParameters->SceneColorTexture = SceneColor.Texture;
	PassParameters->FlickeringLumaOutput = GraphBuilder.CreateUAV(FlickeringLuma.Texture);

	TShaderMapRef<FTSRMeasureFlickeringLumaCS> ComputeShader(ShaderMap);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TSR MeasureFlickeringLuma %dx%d", SceneColor.ViewRect.Width(), SceneColor.ViewRect.Height()),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(FlickeringLuma.ViewRect.Size(), 8 * 2));

	return FlickeringLuma;
}

FDefaultTemporalUpscaler::FOutputs AddTemporalSuperResolutionPasses(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FDefaultTemporalUpscaler::FInputs& PassInputs)
{
	const FTSRHistory& InputHistory = View.PrevViewInfo.TSRHistory;

	// Number of frames stored in the history.
	FTSRHistorySliceSequence HistorySliceSequence;
	if (CVarTSRResurrectionEnable.GetValueOnRenderThread())
	{
		HistorySliceSequence.FrameStorageCount = FMath::Clamp(
			FTSRHistorySliceSequence::kTransientSliceCount + FMath::DivideAndRoundUp(CVarTSRResurrectionPersistentFrameCount.GetValueOnRenderThread(), 2) * 2,
			4,
			GMaxTextureArrayLayers);
		HistorySliceSequence.FrameStoragePeriod = FMath::Clamp(CVarTSRResurrectionPersistentFrameInterval.GetValueOnRenderThread() | 0x1, 1, 1024);
	}
	check(HistorySliceSequence.Check());

	// Whether to use wave ops optimizations.
	const ERHIFeatureSupport WaveOpsSupport = FTSRShader::SupportsWaveOps(View.GetShaderPlatform());
	const bool bUseWaveOps = (CVarTSRWaveOps.GetValueOnRenderThread() != 0 && GRHISupportsWaveOperations && (WaveOpsSupport == ERHIFeatureSupport::RuntimeDependent || WaveOpsSupport == ERHIFeatureSupport::RuntimeGuaranteed));
	const int32 WaveSizeOverride = bUseWaveOps ? CVarTSRWaveSize.GetValueOnAnyThread() : 0;
	
	// Whether to use 16bit VALU
	const ERHIFeatureSupport VALU16BitSupport = FTSRShader::Supports16BitVALU(View.GetShaderPlatform());
	bool bUse16BitVALU = (CVarTSR16BitVALU.GetValueOnRenderThread() != 0 && GRHIGlobals.SupportsNative16BitOps && VALU16BitSupport == ERHIFeatureSupport::RuntimeDependent) || VALU16BitSupport == ERHIFeatureSupport::RuntimeGuaranteed;

	// Controls whether to use 16bit ops on per GPU vendor in mean time each driver matures.
#if PLATFORM_DESKTOP
	if ((GRHIGlobals.SupportsNative16BitOps && VALU16BitSupport == ERHIFeatureSupport::RuntimeDependent) || VALU16BitSupport == ERHIFeatureSupport::RuntimeGuaranteed)
	{
		if (IsRHIDeviceAMD())
		{
			bUse16BitVALU = CVarTSR16BitVALUOnAMD.GetValueOnRenderThread() != 0;
		}
		else if (IsRHIDeviceIntel())
		{
			bUse16BitVALU = CVarTSR16BitVALUOnIntel.GetValueOnRenderThread() != 0;
		}
		else if (IsRHIDeviceNVIDIA())
		{
			bUse16BitVALU = CVarTSR16BitVALUOnNvidia.GetValueOnRenderThread() != 0;
		}
	}
#endif // PLATFORM_DESKTOP

	// Whether alpha channel is supported.
	const bool bSupportsAlpha = CVarTSRAlphaChannel.GetValueOnRenderThread() >= 0 ? (CVarTSRAlphaChannel.GetValueOnRenderThread() > 0) : IsPostProcessingWithAlphaChannelSupported();

	const float RefreshRateToFrameRateCap = (View.Family->Time.GetDeltaRealTimeSeconds() > 0.0f && CVarTSRFlickeringAdjustToFrameRate.GetValueOnRenderThread())
		? View.Family->Time.GetDeltaRealTimeSeconds() * CVarTSRFlickeringFrameRateCap.GetValueOnRenderThread() : 1.0f;

	// Maximum number sample for each output pixel in the history
	const float MaxHistorySampleCount = FMath::Clamp(CVarTSRHistorySampleCount.GetValueOnRenderThread(), 8.0f, 32.0f);

	// Whether the view is orthographic view
	const bool bIsOrthoProjection = !View.IsPerspectiveProjection();

	// whether TSR passes can run on async compute.
	int32 AsyncComputePasses = GSupportsEfficientAsyncCompute ? CVarTSRAsyncCompute.GetValueOnRenderThread() : 0;

	// period at which history changes is considered too distracting.
	const float FlickeringFramePeriod = CVarTSRFlickeringEnable.GetValueOnRenderThread() ? (CVarTSRFlickeringPeriod.GetValueOnRenderThread() / FMath::Max(RefreshRateToFrameRateCap, 1.0f)) : 0.0f;

	ETSRHistoryFormatBits HistoryFormatBits = ETSRHistoryFormatBits::None;
	{
		if (FlickeringFramePeriod > 0)
		{
			HistoryFormatBits |= ETSRHistoryFormatBits::Moire;
		}

		if (bSupportsAlpha)
		{
			HistoryFormatBits |= ETSRHistoryFormatBits::AlphaChannel;
		}
	}
	FTSRHistoryArrayIndices HistoryArrayIndices = TranslateHistoryFormatBitsToArrayIndices(HistoryFormatBits);

	FTSRUpdateHistoryCS::EQuality UpdateHistoryQuality = FTSRUpdateHistoryCS::EQuality(FMath::Clamp(CVarTSRHistoryUpdateQuality.GetValueOnRenderThread(), 0, int32(FTSRUpdateHistoryCS::EQuality::MAX) - 1));

	bool bIsSeperateTranslucyTexturesValid = PassInputs.PostDOFTranslucencyResources.IsValid();

	EPixelFormat ColorFormat = bSupportsAlpha ? PF_FloatRGBA : PF_FloatR11G11B10;
	EPixelFormat HistoryColorFormat = (CVarTSRR11G11B10History.GetValueOnRenderThread() != 0 && !bSupportsAlpha) ? PF_FloatR11G11B10 : PF_FloatRGBA;

	int32 RejectionAntiAliasingQuality = FMath::Clamp(CVarTSRRejectionAntiAliasingQuality.GetValueOnRenderThread(), 1, 2);
	if (UpdateHistoryQuality == FTSRUpdateHistoryCS::EQuality::Low)
	{
		RejectionAntiAliasingQuality = 0; 
	}

	FIntPoint InputExtent = PassInputs.SceneColor.Texture->Desc.Extent;
	FIntRect InputRect = View.ViewRect;

	FIntPoint OutputExtent;
	FIntRect OutputRect;
	if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
	{
		OutputRect.Min = FIntPoint(0, 0);
		OutputRect.Max = View.GetSecondaryViewRectSize();

		FIntPoint QuantizedPrimaryUpscaleViewSize;
		QuantizeSceneBufferSize(OutputRect.Max, QuantizedPrimaryUpscaleViewSize);

		if (GIsEditor)
		{
			OutputExtent = FIntPoint(
				FMath::Max(InputExtent.X, QuantizedPrimaryUpscaleViewSize.X),
				FMath::Max(InputExtent.Y, QuantizedPrimaryUpscaleViewSize.Y));
		}
		else
		{
			OutputExtent = QuantizedPrimaryUpscaleViewSize;
		}
	}
	else
	{
		OutputRect.Min = FIntPoint(0, 0);
		OutputRect.Max = View.ViewRect.Size();
		OutputExtent = InputExtent;
	}

	FIntPoint HistoryGuideExtent;
	{
		// Compute final resolution fraction uper bound.
		float ResolutionFractionUpperBound = 1.f;
		if (ISceneViewFamilyScreenPercentage const* ScreenPercentageInterface = View.Family->GetScreenPercentageInterface())
		{
			DynamicRenderScaling::TMap<float> DynamicResolutionUpperBounds = ScreenPercentageInterface->GetResolutionFractionsUpperBound();
			const float PrimaryResolutionFractionUpperBound = DynamicResolutionUpperBounds[GDynamicPrimaryResolutionFraction];
			ResolutionFractionUpperBound = PrimaryResolutionFractionUpperBound * View.Family->SecondaryViewFraction;
		}

		FIntPoint MaxRenderingViewSize = FSceneRenderer::ApplyResolutionFraction(*View.Family, View.UnconstrainedViewRect.Size(), ResolutionFractionUpperBound);

		FIntPoint QuantizedMaxGuideSize;
		QuantizeSceneBufferSize(MaxRenderingViewSize, QuantizedMaxGuideSize);

		if (GIsEditor)
		{
			HistoryGuideExtent = FIntPoint(
				FMath::Max(InputExtent.X, QuantizedMaxGuideSize.X),
				FMath::Max(InputExtent.Y, QuantizedMaxGuideSize.Y));
		}
		else
		{
			HistoryGuideExtent = QuantizedMaxGuideSize;
		}
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

		if (GIsEditor)
		{
			HistoryExtent = FIntPoint(
				FMath::Max(InputExtent.X, QuantizedHistoryViewSize.X),
				FMath::Max(InputExtent.Y, QuantizedHistoryViewSize.Y));
		}
		else
		{
			HistoryExtent = QuantizedHistoryViewSize;
		}
	}
	float OutputToHistoryResolutionFraction = float(HistorySize.X) / float(OutputRect.Width());
	float OutputToHistoryResolutionFractionSquare = OutputToHistoryResolutionFraction * OutputToHistoryResolutionFraction;

	float InputToHistoryResolutionFraction = float(HistorySize.X) / float(InputRect.Width());
	float InputToHistoryResolutionFractionSquare = InputToHistoryResolutionFraction * InputToHistoryResolutionFraction;

	float OutputToInputResolutionFraction = float(InputRect.Width()) / float(OutputRect.Width());
	float OutputToInputResolutionFractionSquare = OutputToInputResolutionFraction * OutputToInputResolutionFraction;

	// Whether to use camera cut shader permutation or not.
	const bool bCameraCut =
		!InputHistory.IsValid() ||
		View.bCameraCut ||
		ETSRHistoryFormatBits(InputHistory.FormatBit) != HistoryFormatBits ||
		false;

	static auto CVarAntiAliasingQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.AntiAliasingQuality"));
	check(CVarAntiAliasingQuality);
	
	RDG_EVENT_SCOPE(GraphBuilder, "TemporalSuperResolution(sg.AntiAliasingQuality=%d%s) %dx%d -> %dx%d",
		CVarAntiAliasingQuality->GetInt(),
		bSupportsAlpha ? TEXT(" AlphaChannel") : TEXT(""),
		InputRect.Width(), InputRect.Height(),
		OutputRect.Width(), OutputRect.Height());
	RDG_GPU_STAT_SCOPE(GraphBuilder, TemporalSuperResolution);

	FRDGTextureRef BlackUintDummy = GSystemTextures.GetZeroUIntDummy(GraphBuilder);
	FRDGTextureRef BlackDummy = GSystemTextures.GetBlackDummy(GraphBuilder);
	FRDGTextureRef BlackArrayDummy = GSystemTextures.GetBlackArrayDummy(GraphBuilder);
	FRDGTextureRef BlackAlphaOneDummy = GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
	FRDGTextureRef WhiteDummy = GSystemTextures.GetWhiteDummy(GraphBuilder);

	FIntRect SeparateTranslucencyRect = FIntRect(0, 0, 1, 1);
	FRDGTextureRef SeparateTranslucencyTexture = BlackAlphaOneDummy;
	bool bHasSeparateTranslucency = PassInputs.PostDOFTranslucencyResources.IsValid();
#if WITH_EDITOR
	// Do not composite translucency if we are visualizing a buffer, unless it is the overview mode.
	static FName OverviewName = FName(TEXT("Overview"));
	static FName PerformanceOverviewName = FName(TEXT("PerformanceOverview"));
	bHasSeparateTranslucency &= 
		   (!View.Family->EngineShowFlags.VisualizeBuffer    || (View.Family->EngineShowFlags.VisualizeBuffer    && View.CurrentBufferVisualizationMode == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeNanite    || (View.Family->EngineShowFlags.VisualizeNanite    && View.CurrentNaniteVisualizationMode == OverviewName))
		&& (!View.Family->EngineShowFlags.VisualizeLumen     || (View.Family->EngineShowFlags.VisualizeLumen     && (View.CurrentLumenVisualizationMode  == OverviewName || View.CurrentLumenVisualizationMode == PerformanceOverviewName)))
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

	auto SelectWaveSize = [&](const TArray<int32>& WaveSizeDomain)
	{
		check(!WaveSizeDomain.IsEmpty());
		int32 WaveSizeOps = 0;

		if (bUseWaveOps)
		{
			if (WaveSizeOverride != 0 && WaveSizeDomain.Contains(WaveSizeOverride) && WaveSizeOverride >= GRHIMinimumWaveSize && WaveSizeOverride <= GRHIMaximumWaveSize)
			{
				WaveSizeOps = WaveSizeOverride;
			}
			else
			{
				const int32 MinimumWaveSizeWithPermutation = FMath::Max(GRHIMinimumWaveSize, WaveSizeDomain[0]);
				WaveSizeOps = MinimumWaveSizeWithPermutation >= WaveSizeDomain[0] && MinimumWaveSizeWithPermutation <= WaveSizeDomain.Last() ? MinimumWaveSizeWithPermutation : 0;
			}
		}

		return WaveSizeOps;
	};

	// Allocate a new history
	FTSRHistoryTextures History;
	const int32 HistoryColorGuideSliceCountWithoutResurrection = bSupportsAlpha ? 2 : 1;
	{
		{
			bool bRequires2Mips = HistorySize == OutputRect.Size() && PassInputs.bGenerateOutputMip1;

			FRDGTextureDesc ArrayDesc = FRDGTextureDesc::Create2DArray(
				HistoryExtent,
				HistoryColorFormat,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV,
				HistoryArrayIndices.Size * HistorySliceSequence.FrameStorageCount,
				/* NumMips = */ bRequires2Mips ? 2 : 1);
			History.ColorArray = GraphBuilder.CreateTexture(ArrayDesc, TEXT("TSR.History.Color"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				HistoryExtent,
				PF_R8,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV,
				HistorySliceSequence.FrameStorageCount);
			History.MetadataArray = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Metadata"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				HistoryGuideExtent,
				bSupportsAlpha ? PF_FloatRGBA : PF_A2B10G10R10,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV,
				HistorySliceSequence.FrameStorageCount * HistoryColorGuideSliceCountWithoutResurrection);
			History.GuideArray = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Guide"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				HistoryGuideExtent,
				PF_R8G8B8A8,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV,
				/* ArraySize = */ 1);
			History.MoireArray = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Moire"));
		}
	}

	// Whether to camera cut the history Resurrection
	const bool bCameraCutResurrection =
		bCameraCut ||
		HistorySliceSequence.GetRollingIndexCount() == 1 ||
		InputHistory.OutputViewportRect != FIntRect(FIntPoint(0, 0), HistorySize) ||
		InputHistory.FrameStorageCount != HistorySliceSequence.FrameStorageCount ||
		InputHistory.FrameStoragePeriod != HistorySliceSequence.FrameStoragePeriod ||
		History.ColorArray->Desc.Extent != InputHistory.ColorArray->GetDesc().Extent ||
		History.ColorArray->Desc.Format != InputHistory.ColorArray->GetDesc().Format ||
		History.ColorArray->Desc.NumMips != InputHistory.ColorArray->GetDesc().NumMips ||
		History.ColorArray->Desc.ArraySize != InputHistory.ColorArray->GetDesc().ArraySize ||
		History.GuideArray->Desc.Extent != InputHistory.GuideArray->GetDesc().Extent ||
		History.GuideArray->Desc.Format != InputHistory.GuideArray->GetDesc().Format ||
		false;

	// Current and previous frame histories
	int32 ResurrectionFrameSliceIndex = 0;
	int32 PrevFrameSliceIndex = 0;
	int32 CurrentFrameSliceIndex = 0;
	int32 CurrentFrameRollingIndex = 0;
	FTSRHistoryTextures PrevHistory;
	FTSRHistorySliceSequence PrevHistorySliceSequence;
	if (bCameraCut)
	{
		PrevHistory.ColorArray = BlackArrayDummy;
		PrevHistory.MetadataArray = BlackArrayDummy;
		PrevHistory.GuideArray = BlackArrayDummy;
		PrevHistory.MoireArray = BlackArrayDummy;

		if (HistorySliceSequence.GetRollingIndexCount() > 1)
		{
			check(bCameraCutResurrection);

			int32 PrevFrameRollingIndex = HistorySliceSequence.DecrementFrameRollingIndex(CurrentFrameRollingIndex);

			ResurrectionFrameSliceIndex = HistorySliceSequence.RollingIndexToSliceIndex(PrevFrameRollingIndex);
			PrevFrameSliceIndex        = HistorySliceSequence.RollingIndexToSliceIndex(PrevFrameRollingIndex);
			CurrentFrameSliceIndex     = HistorySliceSequence.RollingIndexToSliceIndex(CurrentFrameRollingIndex);
		}
	}
	else
	{
		PrevHistorySliceSequence.FrameStorageCount = InputHistory.FrameStorageCount;
		PrevHistorySliceSequence.FrameStoragePeriod = InputHistory.FrameStoragePeriod;
		check(PrevHistorySliceSequence.Check());

		// Register filterable history
		PrevHistory.ColorArray = GraphBuilder.RegisterExternalTexture(InputHistory.ColorArray);
		PrevHistory.MetadataArray = GraphBuilder.RegisterExternalTexture(InputHistory.MetadataArray);
		PrevHistory.GuideArray = GraphBuilder.RegisterExternalTexture(InputHistory.GuideArray);
		PrevHistory.MoireArray = InputHistory.MoireArray.IsValid()
			? GraphBuilder.RegisterExternalTexture(InputHistory.MoireArray)
			: BlackArrayDummy;

		int32 ResurrectionFrameRollingIndex = 0;
		int32 PrevFrameRollingIndex = 0;
		if (PrevHistorySliceSequence.GetRollingIndexCount() == 1)
		{
			// NOP
		}
		else if (bCameraCutResurrection)
		{
			ResurrectionFrameRollingIndex = InputHistory.LastFrameRollingIndex;
			PrevFrameRollingIndex = InputHistory.LastFrameRollingIndex;
		}
		else
		{
			// Reuse same history so all frames of the history are in the same Texture2DArray for
			// history resurrection without branching on texture fetches.
			if (!View.bStatePrevViewInfoIsReadOnly)
			{
				History.ColorArray = PrevHistory.ColorArray;
				History.MetadataArray = PrevHistory.MetadataArray;
				History.GuideArray = PrevHistory.GuideArray;
				History.MoireArray = PrevHistory.MoireArray;
			}

			ResurrectionFrameRollingIndex = PrevHistorySliceSequence.GetResurrectionFrameRollingIndex(InputHistory.AccumulatedFrameCount, InputHistory.LastFrameRollingIndex);
			PrevFrameRollingIndex = InputHistory.LastFrameRollingIndex;
			CurrentFrameRollingIndex = PrevHistorySliceSequence.IncrementFrameRollingIndex(InputHistory.LastFrameRollingIndex);
		}

		// Translate rolling indices to slice indices to work arround D3D limitation that prevents writing to a Texture2DArray slice when
		// the array is entirely bound.
		ResurrectionFrameSliceIndex = PrevHistorySliceSequence.RollingIndexToSliceIndex(ResurrectionFrameRollingIndex);
		PrevFrameSliceIndex = PrevHistorySliceSequence.RollingIndexToSliceIndex(PrevFrameRollingIndex);
		CurrentFrameSliceIndex = HistorySliceSequence.RollingIndexToSliceIndex(CurrentFrameRollingIndex);
	}

	// Whether history Resurrection is possible at all 
	const bool bCanResurrectHistory = ResurrectionFrameSliceIndex != PrevFrameSliceIndex;

	FMatrix44f ClipToResurrectionClip = FMatrix44f::Identity;
	FScreenPassTextureViewport ResurrectionGuideViewport(FIntPoint(1, 1), FIntRect(0, 0, 1, 1));
	if (bCanResurrectHistory)
	{
		const FViewMatrices& InViewMatrices = View.ViewMatrices;
		const FViewMatrices& InPrevViewMatrices = InputHistory.ViewMatrices[ResurrectionFrameSliceIndex];

		FVector DeltaTranslation = InPrevViewMatrices.GetPreViewTranslation() - InViewMatrices.GetPreViewTranslation();
		FMatrix InvViewProj = InViewMatrices.ComputeInvProjectionNoAAMatrix() * InViewMatrices.GetTranslatedViewMatrix().GetTransposed();
		FMatrix PrevViewProj = FTranslationMatrix(DeltaTranslation) * InPrevViewMatrices.GetTranslatedViewMatrix() * InPrevViewMatrices.ComputeProjectionNoAAMatrix();

		ClipToResurrectionClip = FMatrix44f(InvViewProj * PrevViewProj);
		ResurrectionGuideViewport = FScreenPassTextureViewport(PrevHistory.GuideArray->Desc.Extent, InputHistory.InputViewportRects[ResurrectionFrameSliceIndex]);
		ResurrectionGuideViewport.Rect = ResurrectionGuideViewport.Rect - ResurrectionGuideViewport.Rect.Min;
	}

	// Setup the shader parameters for previous frame history
	FTSRPrevHistoryParameters PrevHistoryParameters;
	{
		// Setup prev history parameters.
		FScreenPassTextureViewport PrevHistoryViewport(PrevHistory.MetadataArray->Desc.Extent, InputHistory.OutputViewportRect);
		if (bCameraCut)
		{
			PrevHistoryViewport.Extent = FIntPoint(1, 1);
			PrevHistoryViewport.Rect = FIntRect(FIntPoint(0, 0), FIntPoint(1, 1));
		}

		PrevHistoryParameters.PrevHistoryInfo = GetScreenPassTextureViewportParameters(PrevHistoryViewport);
		PrevHistoryParameters.ScreenPosToPrevHistoryBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
			PrevHistoryViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
		PrevHistoryParameters.HistoryPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		PrevHistoryParameters.ResurrectionPreExposureCorrection = bCanResurrectHistory ? View.PreExposure / InputHistory.SceneColorPreExposures[ResurrectionFrameSliceIndex] : 1.0f;
	}

	// Clear atomic scattered texture.
	FRDGTextureRef PrevAtomicTextureArray;
	{
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				InputExtent,
				PF_R32_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV | TexCreate_AtomicCompatible,
				/* ArraySize = */ bIsOrthoProjection ? 3 : 2);

			PrevAtomicTextureArray = GraphBuilder.CreateTexture(Desc, TEXT("TSR.PrevAtomics"));
		}

		FTSRClearPrevTexturesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRClearPrevTexturesCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->PrevAtomicOutput = GraphBuilder.CreateUAV(PrevAtomicTextureArray);

		TShaderMapRef<FTSRClearPrevTexturesCS> ComputeShader(View.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR ClearPrevTextures %dx%d", InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 1 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8 * 2));
	}

	// Dilate the velocity texture & scatter reprojection into previous frame
	FRDGTextureRef DilatedVelocityTexture;
	FRDGTextureRef ClosestDepthTexture;
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
		}

		{
			EPixelFormat ClosestDepthFormat;
			if (bIsOrthoProjection)
			{
				ClosestDepthFormat = bCanResurrectHistory ? PF_G32R32F : PF_R32_FLOAT;
			}
			else
			{
				ClosestDepthFormat = bCanResurrectHistory ? PF_G16R16F : PF_R16F;
			}
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				ClosestDepthFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			ClosestDepthTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ClosestDepthTexture"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				InputExtent,
				PF_R8_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				1);

			R8OutputTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ParallaxFactor"));
			if (bOutputIsMovingTexture)
			{
				IsMovingMaskTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(R8OutputTexture, 0));
			}
		}

		int32 TileSize = 8;
		FTSRDilateVelocityCS::FPermutationDomain PermutationVector;

		FTSRDilateVelocityCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRDilateVelocityCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->RotationalClipToPrevClip = RotationalClipToPrevClip;
		PassParameters->PrevOutputBufferUVMin = CommonParameters.InputInfo.UVViewportBilinearMin - CommonParameters.InputInfo.ExtentInverse;
		PassParameters->PrevOutputBufferUVMax = CommonParameters.InputInfo.UVViewportBilinearMax + CommonParameters.InputInfo.ExtentInverse;
		PassParameters->VelocityExtrapolationMultiplier = 0.0; //FMath::Clamp(CVarTSRVelocityExtrapolation.GetValueOnRenderThread(), 0.0f, 1.0f);
		{
			float FlickeringMaxParralaxVelocity = RefreshRateToFrameRateCap * CVarTSRFlickeringMaxParralaxVelocity.GetValueOnRenderThread() * float(View.ViewRect.Width()) / 1920.0f;
			PassParameters->InvFlickeringMaxParralaxVelocity = 1.0f / FlickeringMaxParralaxVelocity;
		}
		PassParameters->bOutputIsMovingTexture = bOutputIsMovingTexture;
		
		PassParameters->SceneDepthTexture = PassInputs.SceneDepth.Texture;
		PassParameters->SceneVelocityTexture = PassInputs.SceneVelocity.Texture;

		PassParameters->DilatedVelocityOutput = GraphBuilder.CreateUAV(DilatedVelocityTexture);
		PassParameters->ClosestDepthOutput = GraphBuilder.CreateUAV(ClosestDepthTexture);
		PassParameters->PrevAtomicOutput = GraphBuilder.CreateUAV(PrevAtomicTextureArray);
		PassParameters->R8Output = GraphBuilder.CreateUAV(R8OutputTexture);

		// Setup up the motion blur's velocity flatten pass.
		if (PassInputs.bGenerateVelocityFlattenTextures)
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

				VelocityFlattenTextures.VelocityFlatten.Texture = GraphBuilder.CreateTexture(Desc, TEXT("MotionBlur.VelocityFlatten"));
				VelocityFlattenTextures.VelocityFlatten.ViewRect = InputRect;
			}

			{
				FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
					FIntPoint::DivideAndRoundUp(InputRect.Size(), FVelocityFlattenTextures::kTileSize),
					PF_FloatRGBA,
					FClearValueBinding::None,
					GFastVRamConfig.MotionBlur | TexCreate_ShaderResource | TexCreate_UAV,
					/* ArraySize = */ MotionBlurDirections);

				VelocityFlattenTextures.VelocityTileArray.Texture = GraphBuilder.CreateTexture(Desc, TEXT("MotionBlur.VelocityTile"));
				VelocityFlattenTextures.VelocityTileArray.ViewRect = FIntRect(FIntPoint::ZeroValue, Desc.Extent);
			}

			PassParameters->VelocityFlattenParameters = GetVelocityFlattenParameters(View);
			PassParameters->VelocityFlattenOutput = GraphBuilder.CreateUAV(VelocityFlattenTextures.VelocityFlatten.Texture);
			PassParameters->VelocityTileArrayOutput = GraphBuilder.CreateUAV(VelocityFlattenTextures.VelocityTileArray.Texture);
		}

		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.DilateVelocity"));

		check(PermutationVector == FTSRDilateVelocityCS::RemapPermutation(PermutationVector));
		TShaderMapRef<FTSRDilateVelocityCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR DilateVelocity(#%d MotionBlurDirections=%d%s) %dx%d",
				PermutationVector.ToDimensionValueId(),
				int32(PermutationVector.Get<FTSRDilateVelocityCS::FMotionBlurDirectionsDim>()),
				bOutputIsMovingTexture ? TEXT(" OutputIsMoving") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 2 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), TileSize));
	}

	// Decimate input to flicker at same frequency as input.
	FRDGTextureRef ReprojectedHistoryGuideTexture = nullptr;
	FRDGTextureRef ReprojectedHistoryMoireTexture = nullptr;
	FRDGTextureRef DecimateMaskTexture = nullptr;
	{
		FRDGTextureRef HoleFilledVelocityTexture;
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8G8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			Desc.Format = PF_R8G8;
			DecimateMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.DecimateMask"));

			Desc.Format = PF_G16R16;
			HoleFilledVelocityTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.Velocity.HoleFilled"));
		}

		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				InputExtent,
				History.GuideArray->Desc.Format,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* InArraySize = */ (bCanResurrectHistory ? 2 : 1) * HistoryColorGuideSliceCountWithoutResurrection);
			ReprojectedHistoryGuideTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ReprojectedHistoryGuide"));
		}

		if (FlickeringFramePeriod > 0.0f)
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
				InputExtent,
				History.MoireArray->Desc.Format,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* InArraySize = */ 1);
			ReprojectedHistoryMoireTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.ReprojectedHistoryMoire"));
		}

		FTSRDecimateHistoryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRDecimateHistoryCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->RotationalClipToPrevClip = RotationalClipToPrevClip;

		PassParameters->InputSceneColorTexture = PassInputs.SceneColor.Texture;
		PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
		PassParameters->ClosestDepthTexture = ClosestDepthTexture;
		PassParameters->PrevAtomicTextureArray = PrevAtomicTextureArray;

		PassParameters->PrevHistoryParameters = PrevHistoryParameters;

		{
			FScreenPassTextureViewport PrevHistoryGuideViewport(PrevHistory.GuideArray->Desc.Extent, InputHistory.InputViewportRect - InputHistory.InputViewportRect.Min);
			PassParameters->PrevHistoryGuide = PrevHistory.GuideArray;
			PassParameters->PrevHistoryMoire = PrevHistory.MoireArray;
			PassParameters->PrevGuideInfo = GetScreenPassTextureViewportParameters(PrevHistoryGuideViewport);
			PassParameters->InputPixelPosToReprojectScreenPos = ((FScreenTransform::Identity - InputRect.Min + 0.5f) / InputRect.Size()) * FScreenTransform::ViewportUVToScreenPos;
			PassParameters->ScreenPosToPrevHistoryGuideBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
				PrevHistoryGuideViewport,
				FScreenTransform::ETextureBasis::ScreenPosition,
				FScreenTransform::ETextureBasis::TextureUV);
			PassParameters->ScreenPosToResurrectionGuideBufferUV = FScreenTransform::ChangeTextureBasisFromTo(
				ResurrectionGuideViewport,
				FScreenTransform::ETextureBasis::ScreenPosition,
				FScreenTransform::ETextureBasis::TextureUV);
			PassParameters->ResurrectionGuideUVViewportBilinearMin = GetScreenPassTextureViewportParameters(ResurrectionGuideViewport).UVViewportBilinearMin;
			PassParameters->ResurrectionGuideUVViewportBilinearMax = GetScreenPassTextureViewportParameters(ResurrectionGuideViewport).UVViewportBilinearMax;
			PassParameters->HistoryGuideQuantizationError = ComputePixelFormatQuantizationError(PrevHistory.GuideArray->Desc.Format);
			PassParameters->PerceptionAdd = FMath::Pow(0.5f, CVarTSRShadingExposureOffset.GetValueOnRenderThread());
		}

		PassParameters->ResurrectionFrameIndex = ResurrectionFrameSliceIndex;
		PassParameters->PrevFrameIndex = PrevFrameSliceIndex;
		PassParameters->ClipToResurrectionClip = ClipToResurrectionClip;

		PassParameters->ReprojectedHistoryGuideOutput = GraphBuilder.CreateUAV(ReprojectedHistoryGuideTexture);
		if (ReprojectedHistoryMoireTexture)
		{
			PassParameters->ReprojectedHistoryMoireOutput = GraphBuilder.CreateUAV(ReprojectedHistoryMoireTexture);
		}
		PassParameters->HoleFilledVelocityOutput = GraphBuilder.CreateUAV(HoleFilledVelocityTexture);
		PassParameters->DecimateMaskOutput = GraphBuilder.CreateUAV(DecimateMaskTexture);
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.DecimateHistory"));

		FTSRDecimateHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRDecimateHistoryCS::FMoireReprojectionDim>(FlickeringFramePeriod > 0.0f);
		PermutationVector.Set<FTSRDecimateHistoryCS::FResurrectionReprojectionDim>(bCanResurrectHistory);
		PermutationVector.Set<FTSRShader::F16BitVALUDim>(bUse16BitVALU);
		PermutationVector.Set<FTSRShader::FAlphaChannelDim>(bSupportsAlpha);

		TShaderMapRef<FTSRDecimateHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR DecimateHistory(#%d %s%s%s%s) %dx%d",
				PermutationVector.ToDimensionValueId(),
				PermutationVector.Get<FTSRDecimateHistoryCS::FMoireReprojectionDim>() ? TEXT("ReprojectMoire") : TEXT(""),
				PermutationVector.Get<FTSRDecimateHistoryCS::FResurrectionReprojectionDim>() ? TEXT(" ReprojectResurrection") : TEXT(""),
				PermutationVector.Get<FTSRShader::F16BitVALUDim>() ? TEXT(" 16bit") : TEXT(""),
				PermutationVector.Get<FTSRShader::FAlphaChannelDim>() ? TEXT(" AlphaChannel") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 2 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));

		DilatedVelocityTexture = HoleFilledVelocityTexture;
	}

	// Merge PostDOF translucency within same scene color.
	FRDGTextureRef InputSceneColorTexture = nullptr;
	if (!bHasSeparateTranslucency)
	{
		InputSceneColorTexture = PassInputs.SceneColor.Texture;
	}

	// Perform a history reject the history.
	FRDGTextureRef HistoryRejectionTexture = nullptr;
	FRDGTextureRef InputSceneColorLdrLumaTexture = nullptr;
	FRDGTextureRef AntiAliasMaskTexture = nullptr;
	FRDGTextureSRVRef MoireHistoryTexture = nullptr;
	{
		const bool bComputeInputSceneColorTexture = InputSceneColorLdrLumaTexture == nullptr;
		if (bComputeInputSceneColorTexture)
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				HistoryColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			InputSceneColorTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.SceneColor"));
		}

		const bool bComputeLdrLuma = RejectionAntiAliasingQuality > 0;
		if (bComputeLdrLuma)
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
				PF_R8G8B8A8,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			HistoryRejectionTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.HistoryRejection"));
		}

		if (bComputeLdrLuma)
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			AntiAliasMaskTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.AntiAliasing.Mask"));
		}

		FScreenPassTextureViewport TranslucencyViewport(
			SeparateTranslucencyTexture->Desc.Extent, SeparateTranslucencyRect);

		FTSRConvolutionNetworkShader::FPermutationDomain ConvolutionNetworkPermutationVector;
		ConvolutionNetworkPermutationVector.Set<FTSRConvolutionNetworkShader::FWaveSizeOps>(SelectWaveSize({ 16, 32, 64 }));
		ConvolutionNetworkPermutationVector.Set<FTSRShader::F16BitVALUDim>(bUse16BitVALU);
		ConvolutionNetworkPermutationVector.Set<FTSRShader::FAlphaChannelDim>(bSupportsAlpha);

		FTSRRejectShadingCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRConvolutionNetworkShader::FPermutationDomain>(ConvolutionNetworkPermutationVector);
		PermutationVector.Set<FTSRRejectShadingCS::FFlickeringDetectionDim>(FlickeringFramePeriod > 0.0f);
		PermutationVector.Set<FTSRRejectShadingCS::FHistoryResurrectionDim>(bCanResurrectHistory);
		PermutationVector = FTSRRejectShadingCS::RemapPermutation(PermutationVector);

		const int32 GroupTileSize = 32;
		const int32 TileOverscan = FMath::Clamp(CVarTSRShadingTileOverscan.GetValueOnRenderThread(), 3, GroupTileSize / 2 - 1);
		const int32 TileSize = GroupTileSize - 2 * TileOverscan;

		FTSRRejectShadingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRRejectShadingCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->InputPixelPosToTranslucencyTextureUV =
			((FScreenTransform::Identity + 0.5f - InputRect.Min) / InputRect.Size()) *
			FScreenTransform::ChangeTextureBasisFromTo(TranslucencyViewport, FScreenTransform::ETextureBasis::ViewportUV, FScreenTransform::ETextureBasis::TextureUV);
		PassParameters->TranslucencyTextureUVMin = GetScreenPassTextureViewportParameters(TranslucencyViewport).UVViewportBilinearMin;
		PassParameters->TranslucencyTextureUVMax = GetScreenPassTextureViewportParameters(TranslucencyViewport).UVViewportBilinearMax;
		PassParameters->ClipToResurrectionClip = ClipToResurrectionClip;
		PassParameters->HistoryGuideQuantizationError = ComputePixelFormatQuantizationError(History.GuideArray->Desc.Format);
		PassParameters->FlickeringFramePeriod = FlickeringFramePeriod;
		PassParameters->TheoricBlendFactor = 1.0f / (1.0f + MaxHistorySampleCount / OutputToInputResolutionFractionSquare);
		PassParameters->TileOverscan = TileOverscan;
		PassParameters->PerceptionAdd = FMath::Pow(0.5f, CVarTSRShadingExposureOffset.GetValueOnRenderThread());
		PassParameters->bEnableResurrection = bCanResurrectHistory;
		PassParameters->bEnableFlickeringHeuristic = FlickeringFramePeriod > 0.0f;

		PassParameters->InputTexture = PassInputs.SceneColor.Texture;
		if (PassInputs.FlickeringInputTexture.IsValid())
		{
			ensure(InputRect == PassInputs.FlickeringInputTexture.ViewRect);
			PassParameters->InputMoireLumaTexture = PassInputs.FlickeringInputTexture.Texture;
		}
		else
		{
			PassParameters->InputMoireLumaTexture = BlackDummy;
		}
		PassParameters->InputSceneTranslucencyTexture = SeparateTranslucencyTexture;
		PassParameters->ReprojectedHistoryGuideTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
			ReprojectedHistoryGuideTexture, /* SliceIndex = */ 0 * HistoryColorGuideSliceCountWithoutResurrection));
		if (bSupportsAlpha)
		{
			PassParameters->ReprojectedHistoryGuideMetadataTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
				ReprojectedHistoryGuideTexture, /* SliceIndex = */ 0 * HistoryColorGuideSliceCountWithoutResurrection + 1));
		}
		PassParameters->ReprojectedHistoryMoireTexture = ReprojectedHistoryMoireTexture ? GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
			ReprojectedHistoryMoireTexture, /* SliceIndex = */ 0)) : GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackDummy));
		if (bCanResurrectHistory)
		{
			PassParameters->ResurrectedHistoryGuideTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
				ReprojectedHistoryGuideTexture, /* SliceIndex = */ 1 * HistoryColorGuideSliceCountWithoutResurrection + 0));

			if (bSupportsAlpha)
			{
				PassParameters->ResurrectedHistoryGuideMetadataTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
					ReprojectedHistoryGuideTexture, /* SliceIndex = */ 1 * HistoryColorGuideSliceCountWithoutResurrection + 1));
			}
		}
		else
		{
			PassParameters->ResurrectedHistoryGuideTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackDummy));
			PassParameters->ResurrectedHistoryGuideMetadataTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackDummy));
		}
		PassParameters->DecimateMaskTexture = DecimateMaskTexture;
		PassParameters->IsMovingMaskTexture = IsMovingMaskTexture ? IsMovingMaskTexture : GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackUintDummy));
		PassParameters->ClosestDepthTexture = ClosestDepthTexture;

		// Outputs
		{
			if (View.bStatePrevViewInfoIsReadOnly)
			{
				PassParameters->HistoryGuideOutput = CreateDummyUAVArray(GraphBuilder, History.GuideArray->Desc.Format);
			}
			else
			{
				FRDGTextureUAVDesc GuideUAVDesc(History.GuideArray);
				GuideUAVDesc.FirstArraySlice = CurrentFrameSliceIndex * HistoryColorGuideSliceCountWithoutResurrection;
				GuideUAVDesc.NumArraySlices = HistoryColorGuideSliceCountWithoutResurrection;

				PassParameters->HistoryGuideOutput = GraphBuilder.CreateUAV(GuideUAVDesc);
			}

			// Output history for the anti-flickering heuristic that know how something flicker overtime.
			if (FlickeringFramePeriod == 0.0f)
			{
				PassParameters->HistoryMoireOutput = CreateDummyUAVArray(GraphBuilder, History.MoireArray->Desc.Format);
			}
			else if (View.bStatePrevViewInfoIsReadOnly)
			{
				FRDGTextureDesc Desc = FRDGTextureDesc::Create2DArray(
					InputExtent,
					History.MoireArray->Desc.Format,
					FClearValueBinding::None,
					/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
					/* InArraySize = */ 1);

				// Create an unused texture for the moire history so that the VisualizeTSR can still display the updated moire history.
				FRDGTextureRef UnusedMoireHistoryTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.History.Moire"));
				GraphBuilder.RemoveUnusedTextureWarning(UnusedMoireHistoryTexture);

				PassParameters->HistoryMoireOutput = GraphBuilder.CreateUAV(UnusedMoireHistoryTexture);
				MoireHistoryTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(UnusedMoireHistoryTexture, 0));
			}
			else
			{
				PassParameters->HistoryMoireOutput = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(History.MoireArray));

				MoireHistoryTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(History.MoireArray, /* SliceIndex = */ 0));
			}

			// Output how the history should rejected in the HistoryUpdate
			PassParameters->HistoryRejectionOutput = GraphBuilder.CreateUAV(HistoryRejectionTexture);

			// Amends how the history should be reprojected
			PassParameters->DilatedVelocityOutput = GraphBuilder.CreateUAV(DilatedVelocityTexture);

			// Output the composed translucency and opaque scene color to speed up HistoryUpdate
			PassParameters->InputSceneColorOutput = bComputeInputSceneColorTexture
				? GraphBuilder.CreateUAV(InputSceneColorTexture)
				: CreateDummyUAV(GraphBuilder, HistoryColorFormat);

			// Output LDR luminance to speed up spatial anti-aliaser
			PassParameters->InputSceneColorLdrLumaOutput = bComputeLdrLuma
				? GraphBuilder.CreateUAV(InputSceneColorLdrLumaTexture)
				: CreateDummyUAV(GraphBuilder, PF_R8);
			PassParameters->AntiAliasMaskOutput = bComputeLdrLuma
				? GraphBuilder.CreateUAV(AntiAliasMaskTexture)
				: CreateDummyUAV(GraphBuilder, PF_R8_UINT);

			PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.RejectShading"));
		}

		TShaderMapRef<FTSRRejectShadingCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR RejectShading(#%d TileSize=%d PaddingCostMultiplier=%1.1f WaveSize=%d VALU=%s%s FlickeringFramePeriod=%f%s) %dx%d",
				PermutationVector.ToDimensionValueId(),
				TileSize,
				FMath::Pow(float(GroupTileSize) / float(TileSize), 2),
				int32(PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>().Get<FTSRRejectShadingCS::FWaveSizeOps>()),
				PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>().Get<FTSRShader::F16BitVALUDim>() ? TEXT("16bit") : TEXT("32bit"),
				PermutationVector.Get<FTSRConvolutionNetworkShader::FPermutationDomain>().Get<FTSRShader::FAlphaChannelDim>() ? TEXT(" AlphaChannel") : TEXT(""),
				PassParameters->FlickeringFramePeriod,
				PassParameters->bEnableResurrection ? TEXT(" Resurrection") : TEXT(""),
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), TileSize));
	}

	// Spatial anti-aliasing when doing history rejection.
	FRDGTextureRef AntiAliasingTexture = nullptr;
	if (RejectionAntiAliasingQuality > 0)
	{
		{
			FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				InputExtent,
				PF_R8G8_UINT,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			AntiAliasingTexture = GraphBuilder.CreateTexture(Desc, TEXT("TSR.AntiAliasing"));
		}

		FTSRSpatialAntiAliasingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRSpatialAntiAliasingCS::FParameters>();
		PassParameters->CommonParameters = CommonParameters;
		PassParameters->InputSceneColorLdrLumaTexture = InputSceneColorLdrLumaTexture;
		PassParameters->AntiAliasMaskTexture = AntiAliasMaskTexture;
		PassParameters->AntiAliasingOutput = GraphBuilder.CreateUAV(AntiAliasingTexture);
		PassParameters->DebugOutput = CreateDebugUAV(InputExtent, TEXT("Debug.TSR.SpatialAntiAliasing"));

		FTSRSpatialAntiAliasingCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRSpatialAntiAliasingCS::FQualityDim>(RejectionAntiAliasingQuality);

		TShaderMapRef<FTSRSpatialAntiAliasingCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR SpatialAntiAliasing(#%d Quality=%d) %dx%d",
				PermutationVector.ToDimensionValueId(),
				RejectionAntiAliasingQuality,
				InputRect.Width(), InputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(InputRect.Size(), 8));
	}

	// Update temporal history.
	FRDGTextureSRVRef UpdateHistoryTextureSRV = nullptr;
	FRDGTextureSRVRef SceneColorOutputHalfResTextureSRV = nullptr;
	FRDGTextureSRVRef SceneColorOutputQuarterResTextureSRV = nullptr;
	FRDGTextureSRVRef SceneColorOutputEighthResTextureSRV = nullptr;
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
		PassParameters->InputSceneColorTexture = InputSceneColorTexture;
		PassParameters->InputSceneTranslucencyTexture = SeparateTranslucencyTexture;
		PassParameters->HistoryRejectionTexture = HistoryRejectionTexture;

		PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
		PassParameters->AntiAliasingTexture = AntiAliasingTexture;

		PassParameters->TranslucencyPixelPosMin = SeparateTranslucencyRect.Min;
		PassParameters->TranslucencyPixelPosMax = SeparateTranslucencyRect.Max - 1;

		FScreenTransform HistoryPixelPosToViewportUV = (FScreenTransform::Identity + 0.5f) * CommonParameters.HistoryInfo.ViewportSizeInverse;
		PassParameters->HistoryPixelPosToScreenPos = HistoryPixelPosToViewportUV * FScreenTransform::ViewportUVToScreenPos;
		PassParameters->HistoryPixelPosToInputPPCo = HistoryPixelPosToViewportUV * CommonParameters.InputInfo.ViewportSize + CommonParameters.InputJitter + CommonParameters.InputPixelPosMin;
		PassParameters->HistoryPixelPosToTranslucencyPPCo = HistoryPixelPosToViewportUV * SeparateTranslucencyRect.Size() + CommonParameters.InputJitter * SeparateTranslucencyRect.Size() / CommonParameters.InputInfo.ViewportSize + SeparateTranslucencyRect.Min;
		PassParameters->HistoryQuantizationError = ComputePixelFormatQuantizationError(HistoryColorFormat);

		// All parameters to control the sample count in history.
		PassParameters->HistorySampleCount = MaxHistorySampleCount / OutputToHistoryResolutionFractionSquare;
		PassParameters->HistoryHisteresis = 1.0f / PassParameters->HistorySampleCount;
		PassParameters->WeightClampingRejection = 1.0f - (CVarTSRHistoryRejectionSampleCount.GetValueOnRenderThread() / OutputToHistoryResolutionFractionSquare) * PassParameters->HistoryHisteresis;
		PassParameters->WeightClampingPixelSpeedAmplitude = FMath::Clamp(1.0f - CVarTSRWeightClampingSampleCount.GetValueOnRenderThread() * PassParameters->HistoryHisteresis, 0.0f, 1.0f);
		PassParameters->InvWeightClampingPixelSpeed = 1.0f / (CVarTSRWeightClampingPixelSpeed.GetValueOnRenderThread() * OutputToHistoryResolutionFraction);
		
		PassParameters->InputToHistoryFactor = float(HistorySize.X) / float(InputRect.Width());
		PassParameters->InputContributionMultiplier = OutputToHistoryResolutionFractionSquare; 
		PassParameters->bGenerateOutputMip1 = false;
		PassParameters->bGenerateOutputMip2 = false;
		PassParameters->bGenerateOutputMip3 = false;
		PassParameters->bHasSeparateTranslucency = bHasSeparateTranslucency;

		PassParameters->HistoryArrayIndices = HistoryArrayIndices;
		PassParameters->PrevHistoryParameters = PrevHistoryParameters;
		if (bCameraCut)
		{
			PassParameters->ResurrectionFrameIndex = 0;
			PassParameters->PrevFrameIndex = 0;

			PassParameters->PrevHistoryColorTexture = GraphBuilder.CreateSRV(BlackArrayDummy);
			PassParameters->PrevHistoryMetadataTexture = GraphBuilder.CreateSRV(BlackArrayDummy);
		}
		else
		{
			FRHIRange16 SliceRange(uint16(PrevFrameSliceIndex), uint16(1));
			if (bCanResurrectHistory)
			{
				SliceRange = PrevHistorySliceSequence.GetSRVSliceRange(CurrentFrameSliceIndex, PrevFrameSliceIndex);
			}
			check(SliceRange.IsInRange(ResurrectionFrameSliceIndex));
			check(SliceRange.IsInRange(PrevFrameSliceIndex));
			check(!SliceRange.IsInRange(CurrentFrameSliceIndex) || History.ColorArray != PrevHistory.ColorArray);

			FRDGTextureSRVDesc PrevColorSRVDesc(PrevHistory.ColorArray);
			PrevColorSRVDesc.NumMipLevels = 1;

			FRDGTextureSRVDesc PrevMetadataSRVDesc(PrevHistory.MetadataArray);
			PrevMetadataSRVDesc.NumMipLevels = 1;

			PrevColorSRVDesc.FirstArraySlice = SliceRange.First;
			PrevColorSRVDesc.NumArraySlices = SliceRange.Num;

			PrevMetadataSRVDesc.FirstArraySlice = SliceRange.First;
			PrevMetadataSRVDesc.NumArraySlices = SliceRange.Num;

			PassParameters->ResurrectionFrameIndex = ResurrectionFrameSliceIndex - PrevColorSRVDesc.FirstArraySlice;
			PassParameters->PrevFrameIndex = PrevFrameSliceIndex - PrevColorSRVDesc.FirstArraySlice;

			PassParameters->PrevHistoryColorTexture = GraphBuilder.CreateSRV(PrevColorSRVDesc);
			PassParameters->PrevHistoryMetadataTexture = GraphBuilder.CreateSRV(PrevMetadataSRVDesc);
		}

		{
			FRDGTextureUAVDesc ColorUAVDesc(History.ColorArray);
			ColorUAVDesc.FirstArraySlice = CurrentFrameSliceIndex;
			ColorUAVDesc.NumArraySlices = 1;

			FRDGTextureUAVDesc MetadataUAVDesc(History.MetadataArray);
			MetadataUAVDesc.FirstArraySlice = CurrentFrameSliceIndex;
			MetadataUAVDesc.NumArraySlices = 1;
            
			PassParameters->HistoryArrayIndices = HistoryArrayIndices;
			PassParameters->HistoryColorOutput = GraphBuilder.CreateUAV(ColorUAVDesc);
			PassParameters->HistoryMetadataOutput = GraphBuilder.CreateUAV(MetadataUAVDesc);

			UpdateHistoryTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(
				History.ColorArray, ColorUAVDesc.FirstArraySlice + HistoryArrayIndices.HighFrequency));
		}

		if (PassInputs.bGenerateOutputMip1 && HistorySize == OutputRect.Size())
		{
			FRDGTextureUAVDesc Mip1Desc(History.ColorArray);
			Mip1Desc.MipLevel = 1;
			Mip1Desc.FirstArraySlice = UpdateHistoryTextureSRV->Desc.FirstArraySlice;
			Mip1Desc.NumArraySlices = 1;

			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(Mip1Desc);
		}
		else if (PassInputs.bGenerateSceneColorHalfRes && HistorySize == OutputRect.Size())
		{
			FRDGTextureDesc HalfResDesc = FRDGTextureDesc::Create2DArray(
				OutputExtent / 2,
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* ArraySize = */ 1);
			FRDGTextureRef SceneColorOutputHalfResTexture = GraphBuilder.CreateTexture(HalfResDesc, TEXT("TSR.HalfResOutput"));

			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputHalfResTexture));

			SceneColorOutputHalfResTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(SceneColorOutputHalfResTexture, /* SliceIndex = */ 0));
		}
		else if (PassInputs.bGenerateSceneColorQuarterRes && HistorySize == OutputRect.Size())
		{
			FRDGTextureDesc QuarterResDesc = FRDGTextureDesc::Create2DArray(
				OutputExtent / 4,
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* ArraySize = */ 1);
			FRDGTextureRef SceneColorOutputQuarterResTexture = GraphBuilder.CreateTexture(QuarterResDesc, TEXT("TSR.QuarterResOutput"));

			PassParameters->bGenerateOutputMip2 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputQuarterResTexture));

			SceneColorOutputQuarterResTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(SceneColorOutputQuarterResTexture, /* SliceIndex = */ 0));
		}
		else if (PassInputs.bGenerateSceneColorEighthRes && HistorySize == OutputRect.Size())
		{
			FRDGTextureDesc QuarterResDesc = FRDGTextureDesc::Create2DArray(
				FIntPoint::DivideAndRoundUp(OutputExtent, 8),
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* ArraySize = */ 1);
			FRDGTextureRef SceneColorOutputEighthResTexture = GraphBuilder.CreateTexture(QuarterResDesc, TEXT("TSR.EighthResOutput"));

			PassParameters->bGenerateOutputMip3 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputEighthResTexture));

			SceneColorOutputEighthResTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(SceneColorOutputEighthResTexture, /* SliceIndex = */ 0));
		}
		else
		{
			PassParameters->SceneColorOutputMip1 = CreateDummyUAVArray(GraphBuilder, PF_FloatR11G11B10);
		}
		PassParameters->DebugOutput = CreateDebugUAV(HistoryExtent, TEXT("Debug.TSR.UpdateHistory"));

		FTSRUpdateHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRUpdateHistoryCS::FQualityDim>(UpdateHistoryQuality);
		PermutationVector.Set<FTSRShader::F16BitVALUDim>(bUse16BitVALU);
		PermutationVector.Set<FTSRShader::FAlphaChannelDim>(bSupportsAlpha);

		TShaderMapRef<FTSRUpdateHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR UpdateHistory(#%d Quality=%s%s%s%s%s) %dx%d",
				PermutationVector.ToDimensionValueId(),
				kUpdateQualityNames[int32(PermutationVector.Get<FTSRUpdateHistoryCS::FQualityDim>())],
				PermutationVector.Get<FTSRShader::F16BitVALUDim>() ? TEXT(" 16bit") : TEXT(""),
				PermutationVector.Get<FTSRShader::FAlphaChannelDim>() ? TEXT(" AlphaChannel") : TEXT(""),
				HistoryColorFormat == PF_FloatR11G11B10 ? TEXT(" R11G11B10") : TEXT(""),
				PassParameters->bGenerateOutputMip3 ? TEXT(" OutputMip3") : (PassParameters->bGenerateOutputMip2 ? TEXT(" OutputMip2") : (PassParameters->bGenerateOutputMip1 ? TEXT(" OutputMip1") : TEXT(""))),
				HistorySize.X, HistorySize.Y),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(HistorySize, 8));
	}

	// If we upscaled the history buffer, downsize back to the secondary screen percentage size.
	FRDGTextureSRVRef SceneColorOutputTextureSRV = UpdateHistoryTextureSRV;
	if (HistorySize != OutputRect.Size())
	{
		check(!SceneColorOutputHalfResTextureSRV);
		check(!SceneColorOutputQuarterResTextureSRV);
		
		bool bNyquistHistory = HistorySize.X == 2 * OutputRect.Width() && HistorySize.Y == 2 * OutputRect.Height();

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

		PassParameters->UpdateHistoryOutputTexture = UpdateHistoryTextureSRV;
		
		FRDGTextureRef SceneColorOutputTexture;
		{
			FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
				OutputExtent,
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV,
				/* NumMips = */ PassInputs.bGenerateOutputMip1 ? 2 : 1);
			SceneColorOutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("TSR.Output"));

			PassParameters->SceneColorOutputMip0 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputTexture, /* InMipLevel = */ 0));
			SceneColorOutputTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(SceneColorOutputTexture));
		}

		if (PassInputs.bGenerateOutputMip1)
		{
			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputTexture, /* InMipLevel = */ 1));
		}
		else if (PassInputs.bGenerateSceneColorHalfRes || PassInputs.bGenerateSceneColorQuarterRes || PassInputs.bGenerateSceneColorEighthRes)
		{
			FRDGTextureDesc HalfResDesc = FRDGTextureDesc::Create2D(
				OutputExtent / 2,
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);
			FRDGTextureRef SceneColorOutputHalfResTexture = GraphBuilder.CreateTexture(HalfResDesc, TEXT("TSR.HalfResOutput"));

			PassParameters->bGenerateOutputMip1 = true;
			PassParameters->SceneColorOutputMip1 = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(SceneColorOutputHalfResTexture));

			SceneColorOutputHalfResTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(SceneColorOutputHalfResTexture));
		}
		else
		{
			PassParameters->SceneColorOutputMip1 = CreateDummyUAV(GraphBuilder, PF_FloatR11G11B10);
		}
		PassParameters->DebugOutput = CreateDebugUAV(OutputExtent, TEXT("Debug.TSR.ResolveHistory"));

		FTSRResolveHistoryCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FTSRResolveHistoryCS::FNyquistDim>(bNyquistHistory ? SelectWaveSize({ 16, 32 }) : 0);
		PermutationVector.Set<FTSRShader::F16BitVALUDim>(bUse16BitVALU);
		PermutationVector.Set<FTSRShader::FAlphaChannelDim>(bSupportsAlpha);
		PermutationVector = FTSRResolveHistoryCS::RemapPermutation(PermutationVector);

		TShaderMapRef<FTSRResolveHistoryCS> ComputeShader(View.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TSR ResolveHistory(#%d WaveSize=%d%s%s%s) %dx%d", //-V510
				PermutationVector.ToDimensionValueId(),
				PermutationVector.Get<FTSRResolveHistoryCS::FNyquistDim>(),
				PermutationVector.Get<FTSRShader::F16BitVALUDim>() ? TEXT(" 16bit") : TEXT(""),
				PermutationVector.Get<FTSRShader::FAlphaChannelDim>() ? TEXT(" AlphaChannel") : TEXT(""),
				PassParameters->bGenerateOutputMip1 ? TEXT(" OutputMip1") : TEXT(""),
				OutputRect.Width(), OutputRect.Height()),
			AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
			ComputeShader,
			PassParameters,
			FComputeShaderUtils::GetGroupCount(OutputRect.Size(), PermutationVector.Get<FTSRResolveHistoryCS::FNyquistDim>() ? 6 : 8));
		
		SceneColorOutputTextureSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(SceneColorOutputTexture));
	}

	// Extract all resources for next frame.
	if (!View.bStatePrevViewInfoIsReadOnly)
	{
		FTSRHistory& OutputHistory = View.ViewState->PrevFrameViewInfo.TSRHistory;
		OutputHistory.InputViewportRect = InputRect;
		OutputHistory.OutputViewportRect = FIntRect(FIntPoint(0, 0), HistorySize);
		OutputHistory.FormatBit = uint32(HistoryFormatBits);
		OutputHistory.FrameStorageCount     = HistorySliceSequence.FrameStorageCount;
		OutputHistory.FrameStoragePeriod    = HistorySliceSequence.FrameStoragePeriod;
		OutputHistory.AccumulatedFrameCount = bCameraCutResurrection ? 1 : FMath::Min(InputHistory.AccumulatedFrameCount + 1, HistorySliceSequence.GetRollingIndexCount());
		OutputHistory.LastFrameRollingIndex = CurrentFrameRollingIndex;
		if (bCameraCutResurrection)
		{
			OutputHistory.ViewMatrices.SetNum(OutputHistory.FrameStorageCount);
			OutputHistory.SceneColorPreExposures.SetNum(OutputHistory.FrameStorageCount);
			OutputHistory.InputViewportRects.SetNum(OutputHistory.FrameStorageCount);
		}
		else
		{
			OutputHistory.ViewMatrices = InputHistory.ViewMatrices;
			OutputHistory.SceneColorPreExposures = InputHistory.SceneColorPreExposures;
			OutputHistory.InputViewportRects = InputHistory.InputViewportRects;
		}
		OutputHistory.ViewMatrices[CurrentFrameSliceIndex] = View.ViewMatrices;
		OutputHistory.SceneColorPreExposures[CurrentFrameSliceIndex] = View.PreExposure;
		OutputHistory.InputViewportRects[CurrentFrameSliceIndex] = InputRect;

		// Extract filterable history
		GraphBuilder.QueueTextureExtraction(History.ColorArray, &OutputHistory.ColorArray);
		GraphBuilder.QueueTextureExtraction(History.MetadataArray, &OutputHistory.MetadataArray);

		// Extract history guide
		GraphBuilder.QueueTextureExtraction(History.GuideArray, &OutputHistory.GuideArray);

		if (FlickeringFramePeriod > 0.0f)
		{
			GraphBuilder.QueueTextureExtraction(History.MoireArray, &OutputHistory.MoireArray);
		}

		// Extract the output for next frame SSR so that separate translucency shows up in SSR.
		{
			// Output in TemporalAAHistory and not CustomSSR so Lumen can pick up ScreenSpaceRayTracingInput in priority to ensure consistent behavior between TAA and TSR.
			GraphBuilder.QueueTextureExtraction(
				SceneColorOutputTextureSRV->Desc.Texture, &View.ViewState->PrevFrameViewInfo.TemporalAAHistory.RT[0]);
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ViewportRect = OutputRect;
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.ReferenceBufferSize = OutputExtent;
			View.ViewState->PrevFrameViewInfo.TemporalAAHistory.OutputSliceIndex = SceneColorOutputTextureSRV->Desc.FirstArraySlice;
		}
	}

#if !UE_BUILD_OPTIMIZED_SHOWFLAGS
	if (IsVisualizeTSREnabled(View))
	{
		RDG_EVENT_SCOPE(GraphBuilder, "VisualizeTSR %dx%d", OutputRect.Width(), OutputRect.Height());

		enum class EVisualizeId : int32
		{
			Overview = -1,
			HistorySampleCount = 0,
			ParallaxDisocclusionMask = 1,
			HistoryRejection = 2,
			HistoryClamp = 3,
			ResurrectionMask = 4,
			ResurrectedColor = 5,
			SpatialAntiAliasingMask = 6,
			AntiFlickering = 7,
			MAX,
		};

		static const TCHAR* kVisualizationName[] = {
			TEXT("HistorySampleCount"),
			TEXT("ParallaxDisocclusionMask"),
			TEXT("HistoryRejection"),
			TEXT("HistoryClamp"),
			TEXT("ResurrectionMask"),
			TEXT("ResurrectedColor"),
			TEXT("SpatialAntiAliasingMask"),
			TEXT("AntiFlickering"),
		};
		static_assert(UE_ARRAY_COUNT(kVisualizationName) == int32(EVisualizeId::MAX), "kVisualizationName doesn't match EVisualizeId");

		const EVisualizeId Visualization = EVisualizeId(FMath::Clamp(CVarTSRVisualize.GetValueOnRenderThread(), int32(EVisualizeId::Overview), int32(EVisualizeId::MAX) - 1));
		FIntRect VisualizeRect = Visualization == EVisualizeId::Overview ? FIntRect(OutputRect.Min + OutputRect.Size() / 4, OutputRect.Min + (OutputRect.Size() * 3) / 4) : OutputRect;

		auto Visualize = [&](EVisualizeId VisualizeId, FString Label)
		{
			check(VisualizeId != EVisualizeId::Overview);

			FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
				OutputExtent,
				ColorFormat,
				FClearValueBinding::None,
				/* InFlags = */ TexCreate_ShaderResource | TexCreate_UAV);

			FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("TSR.Visualize"));

			FTSRVisualizeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTSRVisualizeCS::FParameters>();
			PassParameters->CommonParameters = CommonParameters;
			PassParameters->PrevHistoryParameters = PrevHistoryParameters;
			PassParameters->OutputPixelPosToScreenPos = (FScreenTransform::Identity - OutputRect.Min + 0.5f) / OutputRect.Size() * FScreenTransform::ViewportUVToScreenPos;
			PassParameters->ScreenPosToHistoryUV = FScreenTransform::ChangeTextureBasisFromTo(HistoryExtent, FIntRect(FIntPoint::ZeroValue, HistorySize), FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
			PassParameters->ScreenPosToInputPixelPos = FScreenTransform::ChangeTextureBasisFromTo(InputExtent, InputRect, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TexelPosition);
			PassParameters->ScreenPosToInputUV = FScreenTransform::ChangeTextureBasisFromTo(InputExtent, InputRect, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
			{
				FScreenPassTextureViewport PrevHistoryGuideViewport(History.GuideArray->Desc.Extent, InputHistory.InputViewportRect - InputHistory.InputViewportRect.Min);
				PassParameters->ScreenPosToMoireHistoryUV = FScreenTransform::ChangeTextureBasisFromTo(PrevHistoryGuideViewport, FScreenTransform::ETextureBasis::ScreenPosition, FScreenTransform::ETextureBasis::TextureUV);
				PassParameters->MoireHistoryUVBilinearMin = GetScreenPassTextureViewportParameters(PrevHistoryGuideViewport).UVViewportBilinearMin;
				PassParameters->MoireHistoryUVBilinearMax = GetScreenPassTextureViewportParameters(PrevHistoryGuideViewport).UVViewportBilinearMax;
			}

			PassParameters->ClipToResurrectionClip = ClipToResurrectionClip;
			PassParameters->OutputViewRectMin = VisualizeRect.Min;
			PassParameters->OutputViewRectMax = VisualizeRect.Max;
			PassParameters->VisualizeId = int32(VisualizeId);
			PassParameters->bCanResurrectHistory = bCanResurrectHistory;
			PassParameters->bCanSpatialAntiAlias = RejectionAntiAliasingQuality > 0;
			PassParameters->MaxHistorySampleCount = MaxHistorySampleCount;
			PassParameters->OutputToHistoryResolutionFractionSquare = OutputToHistoryResolutionFractionSquare;
			PassParameters->FlickeringFramePeriod = FlickeringFramePeriod;
			PassParameters->PerceptionAdd = FMath::Pow(0.5f, CVarTSRShadingExposureOffset.GetValueOnRenderThread());

			PassParameters->InputTexture = PassInputs.SceneColor.Texture;
			if (PassInputs.FlickeringInputTexture.IsValid())
			{
				ensure(InputRect == PassInputs.FlickeringInputTexture.ViewRect);
				PassParameters->InputMoireLumaTexture = PassInputs.FlickeringInputTexture.Texture;
			}
			else
			{
				PassParameters->InputMoireLumaTexture = BlackDummy;
			}
			PassParameters->InputSceneTranslucencyTexture = SeparateTranslucencyTexture;
			PassParameters->SceneColorTexture = SceneColorOutputTextureSRV;
			PassParameters->ClosestDepthTexture = ClosestDepthTexture;
			PassParameters->DilatedVelocityTexture = DilatedVelocityTexture;
			PassParameters->IsMovingMaskTexture = IsMovingMaskTexture ? IsMovingMaskTexture : GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackUintDummy));
			PassParameters->HistoryRejectionTexture = HistoryRejectionTexture;
			PassParameters->MoireHistoryTexture = MoireHistoryTexture ? MoireHistoryTexture : GraphBuilder.CreateSRV(FRDGTextureSRVDesc(BlackDummy));
			PassParameters->AntiAliasMaskTexture = AntiAliasMaskTexture ? AntiAliasMaskTexture : BlackUintDummy;
			PassParameters->HistoryMetadataTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(History.MetadataArray, CurrentFrameSliceIndex));
			if (PrevHistory.ColorArray == BlackArrayDummy)
			{
				PassParameters->ResurrectedHistoryColorTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(PrevHistory.ColorArray, 0));
			}
			else
			{
				PassParameters->ResurrectedHistoryColorTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForSlice(PrevHistory.ColorArray, bCanResurrectHistory ? ResurrectionFrameSliceIndex : PrevFrameSliceIndex));
			}

			PassParameters->Output = GraphBuilder.CreateUAV(OutputTexture);
			PassParameters->DebugOutput = CreateDebugUAV(OutputExtent, TEXT("Debug.TSR.Visualize"));

			TShaderMapRef<FTSRVisualizeCS> ComputeShader(View.ShaderMap);
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TSR Visualize(%s) %dx%d", kVisualizationName[int32(VisualizeId)], VisualizeRect.Width(), VisualizeRect.Height()),
				AsyncComputePasses >= 3 ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(VisualizeRect.Size(), 8));

			FVisualizeBufferTile Tile;
			Tile.Input = FScreenPassTexture(OutputTexture, VisualizeRect);
			Tile.Label = FString::Printf(TEXT("%s (r.TSR.Visualize=%d)"), *Label, int32(VisualizeId));
			return Tile;
		};

		FRDGTextureRef OutputTexture;
		if (Visualization == EVisualizeId::Overview)
		{
			TArray<FVisualizeBufferTile> Tiles;
			Tiles.SetNum(16);
			{
				Tiles[4 * 0 + 0] = Visualize(EVisualizeId::HistorySampleCount, TEXT("Accumulated Sample Count"));
				Tiles[4 * 0 + 1] = Visualize(EVisualizeId::ParallaxDisocclusionMask, TEXT("Parallax Disocclusion"));
				Tiles[4 * 0 + 2] = Visualize(EVisualizeId::HistoryRejection, TEXT("History Rejection"));
				Tiles[4 * 0 + 3] = Visualize(EVisualizeId::HistoryClamp, TEXT("History Clamp"));
				Tiles[4 * 1 + 0] = Visualize(EVisualizeId::ResurrectionMask, TEXT("Resurrection Mask"));
				if (bCanResurrectHistory)
				{
					Tiles[4 * 2 + 0] = Visualize(EVisualizeId::ResurrectedColor, TEXT("Resurrected Frame"));
				}
				Tiles[4 * 3 + 0] = Visualize(EVisualizeId::SpatialAntiAliasingMask, TEXT("Spatial Anti-Aliasing"));
				Tiles[4 * 1 + 3] = Visualize(EVisualizeId::AntiFlickering, TEXT("Flickering Temporal Analysis"));
			}

			{
				FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
					OutputExtent,
					ColorFormat,
					FClearValueBinding::Black,
					/* InFlags = */ TexCreate_ShaderResource | TexCreate_RenderTargetable);

				OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("TSR.VisualizeOverview"));

				FVisualizeBufferInputs VisualizeBufferInputs;
				VisualizeBufferInputs.OverrideOutput = FScreenPassRenderTarget(FScreenPassTexture(OutputTexture, OutputRect), ERenderTargetLoadAction::EClear);
				VisualizeBufferInputs.SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, FScreenPassTextureSlice(SceneColorOutputTextureSRV, OutputRect));
				VisualizeBufferInputs.Tiles = Tiles;
				AddVisualizeBufferPass(GraphBuilder, View, VisualizeBufferInputs);
			}
		}
		else
		{
			OutputTexture = Visualize(Visualization, TEXT("")).Input.Texture;
		}

		FDefaultTemporalUpscaler::FOutputs Outputs;
		Outputs.FullRes = FScreenPassTextureSlice(GraphBuilder.CreateSRV(FRDGTextureSRVDesc(OutputTexture)), OutputRect);
		return Outputs;
	}
#endif

	FDefaultTemporalUpscaler::FOutputs Outputs;
	Outputs.FullRes = FScreenPassTextureSlice(SceneColorOutputTextureSRV, OutputRect);
	if (SceneColorOutputHalfResTextureSRV)
	{
		Outputs.HalfRes.TextureSRV = SceneColorOutputHalfResTextureSRV;
		Outputs.HalfRes.ViewRect.Min = OutputRect.Min / 2;
		Outputs.HalfRes.ViewRect.Max = Outputs.HalfRes.ViewRect.Min + FIntPoint::DivideAndRoundUp(OutputRect.Size(), 2);
	}
	if (SceneColorOutputQuarterResTextureSRV)
	{
		Outputs.QuarterRes.TextureSRV = SceneColorOutputQuarterResTextureSRV;
		Outputs.QuarterRes.ViewRect.Min = OutputRect.Min / 4;
		Outputs.QuarterRes.ViewRect.Max = Outputs.HalfRes.ViewRect.Min + FIntPoint::DivideAndRoundUp(OutputRect.Size(), 4);
	}
	if (SceneColorOutputEighthResTextureSRV)
	{
		Outputs.EighthRes.TextureSRV = SceneColorOutputEighthResTextureSRV;
		Outputs.EighthRes.ViewRect.Min = FIntPoint::DivideAndRoundUp(OutputRect.Min, 8);
		Outputs.EighthRes.ViewRect.Max = Outputs.EighthRes.ViewRect.Min + FIntPoint::DivideAndRoundUp(OutputRect.Size(), 8);
	}
	Outputs.VelocityFlattenTextures = VelocityFlattenTextures;
	return Outputs;
} // AddTemporalSuperResolutionPasses()
