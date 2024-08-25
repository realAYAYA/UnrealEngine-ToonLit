// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"
#include "NNEDenoiserSettings.h"

namespace UE::Renderer::Private
{
	class IPathTracingDenoiser;
	class IPathTracingSpatialTemporalDenoiser;
}

namespace UE::NNEDenoiser::Private
{

class FViewExtension final : public FSceneViewExtensionBase
{
	public:
		FViewExtension(const FAutoRegister& AutoRegister);
		virtual ~FViewExtension();

		void SetupViewFamily(FSceneViewFamily& InViewFamily) override { }
		void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override { }
			
		void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

		void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	private:
		void ApplySettings(const UNNEDenoiserSettings* Settings);

#if WITH_EDITOR
		void OnDenoiserSettingsChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);
#endif

		TUniquePtr<UE::Renderer::Private::IPathTracingDenoiser> DenoiserToSwap;
		TUniquePtr<UE::Renderer::Private::IPathTracingSpatialTemporalDenoiser> SpatialTemporalDenoiserToSwap;

		// Cached settings and CVar values
		bool bDenoiserEnabled = true;
		EDenoiserRuntimeType RuntimeType;
		FString RuntimeName;
		FString ModelDataName;
	};

} // namespace UE::NNEDenoiser::Private