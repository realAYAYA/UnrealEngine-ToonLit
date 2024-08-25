// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureScalability.h"

#include "HAL/IConsoleManager.h"
#include "VT/VirtualTexturePoolConfig.h"
#include "VT/VirtualTextureRecreate.h"

namespace VirtualTextureScalability
{
#if WITH_EDITOR
	static TAutoConsoleVariable<int32> CVarVTMaxUploadsPerFrameInEditor(
		TEXT("r.VT.MaxUploadsPerFrameInEditor"),
		32,
		TEXT("Max number of page uploads per frame when in editor"),
		ECVF_RenderThreadSafe
	);
#endif 

	static TAutoConsoleVariable<int32> CVarVTMaxUploadsPerFrame(
		TEXT("r.VT.MaxUploadsPerFrame"),
		8,
		TEXT("Max number of page uploads per frame in game"),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);

	static TAutoConsoleVariable<int32> CVarVTMaxUploadsPerFrameStreaming(
		TEXT("r.VT.MaxUploadsPerFrame.Streaming"),
		0,
		TEXT("If positive, max number of page uploads per frame in game for streaming VT. Negative means no limit.\n")
		TEXT("If zero, SVTs won't be budgeted separately. They will be limited by r.VT.MaxUploadsPerFrame along with other types of VTs. This is the old behavior.\n")
		TEXT("This limit should be high if streaming pages is slow so that I/O requests are not throttled which can cause long delays to acquire page data."),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);

	static TAutoConsoleVariable<int32> CVarMaxPagesProducedPerFrame(
		TEXT("r.VT.MaxTilesProducedPerFrame"),
		30,
		TEXT("Max number of pages that can be produced per frame"),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);

#if WITH_EDITOR
	static TAutoConsoleVariable<int32> CVarVTMaxContinuousUpdatesPerFrameInEditor(
		TEXT("r.VT.MaxContinuousUpdatesPerFrameInEditor"),
		8,
		TEXT("Max number of page uploads for pages that are already mapped when in editor."),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);
#endif

	static TAutoConsoleVariable<int32> CVarVTMaxContinuousUpdatesPerFrame(
		TEXT("r.VT.MaxContinuousUpdatesPerFrame"),
		1,
		TEXT("Max number of page uploads for pages that are already mapped."),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);

	static TAutoConsoleVariable<int32> CVarVTMaxReleasedPerFrame(
		TEXT("r.VT.MaxReleasedPerFrame"),
		0,
		TEXT("Max number of allocated virtual textures to release per frame"),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);

	static TAutoConsoleVariable<int32> CVarVTPageFreeThreshold(
		TEXT("r.VT.PageFreeThreshold"),
		60,
		TEXT("Number of frames since the last time a VT page was used, before it's considered free.\n")
		TEXT("VT pages are not necesarily marked as used on the CPU every time they're accessed by the GPU.\n")
		TEXT("Increasing this threshold reduces the chances that an in-use frame is considered free."),
		ECVF_RenderThreadSafe);

	static const int NumScalabilityGroups = 3;

	static float GTileCountBiases[NumScalabilityGroups] = { 0 };
	static FAutoConsoleVariableRef CVarVTTileCountBias_ForBackwardsCompat(
		TEXT("r.VT.RVT.TileCountBias"),
		GTileCountBiases[0],
		TEXT("Bias to apply to Runtime Virtual Texture size.\n")
		TEXT(" Group 0"),
		ECVF_Scalability
	);
	static FAutoConsoleVariableRef CVarVTTileCountBias0(
		TEXT("r.VT.RVT.TileCountBias.Group0"),
		GTileCountBiases[0],
		TEXT("Bias to apply to Runtime Virtual Texture size.\n")
		TEXT(" Group 0"),
		ECVF_Scalability
	);
	static FAutoConsoleVariableRef CVarVTTileCountBias1(
		TEXT("r.VT.RVT.TileCountBias.Group1"),
		GTileCountBiases[1],
		TEXT("Bias to apply to Runtime Virtual Texture size.\n")
		TEXT(" Group 1"),
		ECVF_Scalability
	);
	static FAutoConsoleVariableRef CVarVTTileCountBias2(
		TEXT("r.VT.RVT.TileCountBias.Group2"),
		GTileCountBiases[2],
		TEXT("Bias to apply to Runtime Virtual Texture size.\n")
		TEXT(" Group 2"),
		ECVF_Scalability
	);

	static TAutoConsoleVariable<int32> CVarVTMobileEnableManualTrilinearFiltering(
		TEXT("r.VT.Mobile.ManualTrilinearFiltering"),
		1,
		TEXT("Whether to use a manual trilinear filtering for VTs on mobile platforms.\n")
		TEXT("This more expensive filtering is used on mobile platforms that do not support Temporal Anti-Aliasing.\n"),
		ECVF_RenderThreadSafe | ECVF_ReadOnly);

	static TAutoConsoleVariable<int32> CVarVTEnableAnisotropy(
		TEXT("r.VT.AnisotropicFiltering"),
		0,
		TEXT("Is anisotropic filtering for VTs enabled?"),
		ECVF_RenderThreadSafe | ECVF_ReadOnly);

	static TAutoConsoleVariable<int32> CVarVTMaxAnisotropy(
		TEXT("r.VT.MaxAnisotropy"),
		8,
		TEXT("MaxAnisotropy setting for Virtual Texture sampling."),
		ECVF_RenderThreadSafe | ECVF_Scalability
	);


	/** Track changes and apply to relevant systems. This allows us to dynamically change the scalability settings. */
	static void OnUpdate()
	{
		const float MaxAnisotropy = CVarVTMaxAnisotropy.GetValueOnGameThread();

		static float LastMaxAnisotropy = MaxAnisotropy;
		static float LastTileCountBiases[3] = { GTileCountBiases[0], GTileCountBiases[1], GTileCountBiases[2] };

		bool bUpdate = false;
		if (LastMaxAnisotropy != MaxAnisotropy)
		{
			LastMaxAnisotropy = MaxAnisotropy;
			bUpdate = true;
		}
		if (LastTileCountBiases[0] != GTileCountBiases[0] || LastTileCountBiases[1] != GTileCountBiases[1] || LastTileCountBiases[2] != GTileCountBiases[2])
		{
			LastTileCountBiases[0] = GTileCountBiases[0];
			LastTileCountBiases[1] = GTileCountBiases[1];
			LastTileCountBiases[2] = GTileCountBiases[2];
			bUpdate = true;
		}

		if (bUpdate)
		{
			VirtualTexture::Recreate();
		}
	}

	FAutoConsoleVariableSink GConsoleVariableSink(FConsoleCommandDelegate::CreateStatic(&OnUpdate));


	int32 GetMaxUploadsPerFrame()
	{
#if WITH_EDITOR
		// Don't want this scalability setting to affect editor because we rely on reactive updates while editing.
		return GIsEditor ? CVarVTMaxUploadsPerFrameInEditor.GetValueOnAnyThread() : CVarVTMaxUploadsPerFrame.GetValueOnAnyThread();
#else
		return CVarVTMaxUploadsPerFrame.GetValueOnAnyThread();
#endif
	}

	int32 GetMaxUploadsPerFrameForStreamingVT()
	{
		int32 Budget = CVarVTMaxUploadsPerFrameStreaming.GetValueOnAnyThread();

		if (Budget < 0)
		{
			Budget = MAX_int32;
		}
#if WITH_EDITOR
		// Don't want this scalability setting to affect editor because we rely on reactive updates while editing.
		return GIsEditor ? CVarVTMaxUploadsPerFrameInEditor.GetValueOnAnyThread() : Budget;
#else
		return Budget;
#endif
	}

	int32 GetMaxPagesProducedPerFrame()
	{
		return CVarMaxPagesProducedPerFrame.GetValueOnAnyThread();
	}

	int32 GetMaxContinuousUpdatesPerFrame()
	{
#if WITH_EDITOR
		// Don't want this scalability setting to affect editor because we rely on reactive updates while editing, like GPULightmass.
		return GIsEditor ? CVarVTMaxContinuousUpdatesPerFrameInEditor.GetValueOnAnyThread() : CVarVTMaxContinuousUpdatesPerFrame.GetValueOnAnyThread();
#else
		return CVarVTMaxContinuousUpdatesPerFrame.GetValueOnAnyThread();
#endif
	}

	int32 GetMaxAllocatedVTReleasedPerFrame()
	{
#if WITH_EDITOR
		return 0;
#else
		return CVarVTMaxReleasedPerFrame.GetValueOnAnyThread();
#endif
	}

	uint32 GetPageFreeThreshold()
	{
		return FMath::Max(CVarVTPageFreeThreshold.GetValueOnRenderThread(), 0);
	}

	int32 GetRuntimeVirtualTextureSizeBias(uint32 GroupIndex)
	{
		return GroupIndex < NumScalabilityGroups ? GTileCountBiases[GroupIndex] : 0;
	}

	bool IsAnisotropicFilteringEnabled()
	{
		return CVarVTEnableAnisotropy.GetValueOnAnyThread() != 0;
	}

	int32 GetMaxAnisotropy()
	{
		return CVarVTMaxAnisotropy.GetValueOnAnyThread();
	}

	// Begin deprecated functions.
	// Can remove include of VirtualTexturePoolConfig.h when these are removed.
	float GetPoolSizeScale() 
	{
		return VirtualTexturePool::GetPoolSizeScale(); 
	}
	float GetPoolSizeScale(uint32 GroupIndex) 
	{
		return VirtualTexturePool::GetPoolSizeScale(); 
	}
	int32 GetSplitPhysicalPoolSize() 
	{
		return VirtualTexturePool::GetSplitPhysicalPoolSize(); 
	}
	uint32 GetPhysicalPoolSettingsHash() 
	{
		return VirtualTexturePool::GetConfigHash(); 
	}
	// End deprecated functions.
}
