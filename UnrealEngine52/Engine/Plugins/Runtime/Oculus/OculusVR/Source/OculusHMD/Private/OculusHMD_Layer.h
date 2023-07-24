// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"
#include "ProceduralMeshComponent.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_CustomPresent.h"
#include "XRSwapChain.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOvrpLayer
//-------------------------------------------------------------------------------------------------

class FOvrpLayer : public TSharedFromThis<FOvrpLayer, ESPMode::ThreadSafe>
{
public:
	FOvrpLayer(uint32 InOvrpLayerId);
	~FOvrpLayer();

protected:
	uint32 OvrpLayerId;
};

typedef TSharedPtr<FOvrpLayer, ESPMode::ThreadSafe> FOvrpLayerPtr;


//-------------------------------------------------------------------------------------------------
// FLayer
//-------------------------------------------------------------------------------------------------

class FLayer : public TSharedFromThis<FLayer, ESPMode::ThreadSafe>
{
public:
	FLayer(uint32 InId, const IStereoLayers::FLayerDesc& InDesc);
	FLayer(const FLayer& InLayer);
	~FLayer();

	uint32 GetId() const { return Id; }
	void SetDesc(const IStereoLayers::FLayerDesc& InDesc);
	const IStereoLayers::FLayerDesc& GetDesc() const { return Desc; }
	void SetEyeLayerDesc(const ovrpLayerDesc_EyeFov& InEyeLayerDesc, const ovrpRecti InViewportRect[ovrpEye_Count]);
	const FXRSwapChainPtr& GetSwapChain() const { return SwapChain; }
	const FXRSwapChainPtr& GetRightSwapChain() const { return RightSwapChain; }
	const FXRSwapChainPtr& GetDepthSwapChain() const { return DepthSwapChain; }
	const FXRSwapChainPtr& GetFoveationSwapChain() const { return FoveationSwapChain; }
	void MarkTextureForUpdate() { bUpdateTexture = true; }
#if PLATFORM_ANDROID
	bool NeedsPokeAHole() { return (Desc.Flags & IStereoLayers::LAYER_FLAG_SUPPORT_DEPTH) != 0; }
	void HandlePokeAHoleComponent();
	void BuildPokeAHoleMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector2D>& UV0);
#else
	bool NeedsPokeAHole() { return false; }
#endif

	FTextureRHIRef GetTexture() { return Desc.Texture; }

	TSharedPtr<FLayer, ESPMode::ThreadSafe> Clone() const;

	bool CanReuseResources(const FLayer* InLayer) const;
	void Initialize_RenderThread(const FSettings* Settings, FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList, const FLayer* InLayer = nullptr);
	void UpdateTexture_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList);

	const ovrpLayerSubmit* UpdateLayer_RHIThread(const FSettings* Settings, const FGameFrame* Frame, const int LayerIndex);
	void IncrementSwapChainIndex_RHIThread(FCustomPresent* CustomPresent);
	void ReleaseResources_RHIThread();
	bool IsVisible() { return (Desc.Flags & IStereoLayers::LAYER_FLAG_HIDDEN) == 0; }

	bool bNeedsTexSrgbCreate;

protected:
	uint32 Id;
	IStereoLayers::FLayerDesc Desc;
	int OvrpLayerId;
	ovrpLayerDescUnion OvrpLayerDesc;
	ovrpLayerSubmitUnion OvrpLayerSubmit;
	FOvrpLayerPtr OvrpLayer;
	FXRSwapChainPtr SwapChain;
	FXRSwapChainPtr DepthSwapChain;
	FXRSwapChainPtr FoveationSwapChain;
	FXRSwapChainPtr RightSwapChain;
	FXRSwapChainPtr RightDepthSwapChain;
	bool bUpdateTexture;
	bool bInvertY;
	bool bHasDepth;

	UProceduralMeshComponent* PokeAHoleComponentPtr;
	AActor* PokeAHoleActor;
};

typedef TSharedPtr<FLayer, ESPMode::ThreadSafe> FLayerPtr;


//-------------------------------------------------------------------------------------------------
// FLayerPtr_CompareId
//-------------------------------------------------------------------------------------------------

struct FLayerPtr_CompareId
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		return A->GetId() < B->GetId();
	}
};


//-------------------------------------------------------------------------------------------------
// FLayerPtr_ComparePriority
//-------------------------------------------------------------------------------------------------

struct FLayerPtr_ComparePriority
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		if (A->GetDesc().Priority < B->GetDesc().Priority)
			return true;
		if (A->GetDesc().Priority > B->GetDesc().Priority)
			return false;

		return A->GetId() < B->GetId();
	}
};

struct FLayerPtr_CompareTotal
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		// Draw PoleAHole layers (Android only), EyeFov layer, followed by other layers
		int32 PassA = (A->GetId() == 0) ? 0 : A->NeedsPokeAHole() ? -1 : 1;
		int32 PassB = (B->GetId() == 0) ? 0 : B->NeedsPokeAHole() ? -1 : 1;

		if (PassA != PassB)
			return PassA < PassB;

		// Draw non-FaceLocked layers first
		const IStereoLayers::FLayerDesc& DescA = A->GetDesc();
		const IStereoLayers::FLayerDesc& DescB = B->GetDesc();

		bool bFaceLockedA = (DescA.PositionType == IStereoLayers::ELayerType::FaceLocked);
		bool bFaceLockedB = (DescB.PositionType == IStereoLayers::ELayerType::FaceLocked);

		if (bFaceLockedA != bFaceLockedB)
			return !bFaceLockedA;

		// Draw layers by ascending priority
		if (DescA.Priority != DescB.Priority)
			return DescA.Priority < DescB.Priority;

		// Draw layers by ascending id
		return A->GetId() < B->GetId();
	}
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
