// Copyright Epic Games, Inc. All Rights Reserved.
//
#include "SteamVRStereoLayers.h"
#include "CoreMinimal.h"
#include "SteamVRPrivate.h"
#include "StereoLayerManager.h"
#include "SteamVRHMD.h"
#include "Misc/ScopeLock.h"
#include "DefaultXRCamera.h"

const FName FSteamVRHQLayer::ShapeName = FName("SteamVRHQLayer");

#if STEAMVR_SUPPORTED_PLATFORMS

PRAGMA_DISABLE_DEPRECATION_WARNINGS

// enable to ensure all openvr calls succeed
#if 0
static vr::EVROverlayError sOvrError;
#define OVR_VERIFY(x) sOvrError = x; ensure(sOvrError == vr::VROverlayError_None)
#else
#define OVR_VERIFY(x) x
#endif

#if !PLATFORM_MAC
#include "IVulkanDynamicRHI.h"
#endif

/*=============================================================================
*
* Helper functions
*
*/

//=============================================================================
static void TransformToSteamSpace(const FTransform& In, vr::HmdMatrix34_t& Out, float WorldToMeterScale)
{
	const FQuat InRot = In.GetRotation();
	FQuat OutRot(InRot.Y, InRot.Z, -InRot.X, -InRot.W);

	const FVector InPos = In.GetTranslation();
	FVector OutPos(InPos.Y, InPos.Z, -InPos.X);
	OutPos /= WorldToMeterScale;

	const FVector InScale = In.GetScale3D();
	FVector OutScale(InScale.Y, InScale.Z, InScale.X);

	Out = FSteamVRHMD::ToHmdMatrix34(FTransform(OutRot, OutPos, OutScale).ToMatrixWithScale());
}


/*=============================================================================
*
* FSteamVRHMD's IStereoLayer implementation via TStereLayerManager<>
*
*/

//=============================================================================
void SetLayerDescMember(FSteamVRLayer& Layer, const IStereoLayers::FLayerDesc& InLayerDesc)
{
	if (InLayerDesc.Texture != Layer.LayerDesc.Texture)
	{
		Layer.bUpdateTexture = true;
	}
	Layer.LayerDesc = InLayerDesc;
}

//=============================================================================
bool GetLayerDescMember(const FSteamVRLayer& Layer, IStereoLayers::FLayerDesc& OutLayerDesc)
{
	OutLayerDesc = Layer.LayerDesc;
	return true;
}

//=============================================================================
void MarkLayerTextureForUpdate(FSteamVRLayer& Layer)
{
	Layer.bUpdateTexture = true;
}

//=============================================================================
void FSteamVRHMD::UpdateLayer(struct FSteamVRLayer& Layer, uint32 LayerId, bool bIsValid)
{
	if (bIsValid && Layer.OverlayHandle == vr::k_ulOverlayHandleInvalid)
	{
		FString OverlayName = FString::Printf(TEXT("StereoLayer:%u"), LayerId);
		const auto OverlayNameAnsiStr = StringCast<ANSICHAR>(*OverlayName);
		OVR_VERIFY(VROverlay->CreateOverlay(OverlayNameAnsiStr.Get(), OverlayNameAnsiStr.Get(), &Layer.OverlayHandle));
		OVR_VERIFY(VROverlay->HideOverlay(Layer.OverlayHandle));
		Layer.bUpdateTexture = true;
	}
	else if (!bIsValid && Layer.OverlayHandle != vr::k_ulOverlayHandleInvalid)
	{
		OVR_VERIFY(VROverlay->DestroyOverlay(Layer.OverlayHandle));
		Layer.OverlayHandle = vr::k_ulOverlayHandleInvalid;
	}

	if (Layer.OverlayHandle == vr::k_ulOverlayHandleInvalid)
	{
		return;
	}

	if (Layer.LayerDesc.Flags & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL)
	{
		UE_LOG(LogHMD, Warning, TEXT("Unsupported StereoLayer flag. SteamVR StereoLayers do not support disabling alpha renderding. Make the texture opaque instead."));
	}

	const float WorldToMeterScale = GetWorldToMetersScale();
	check(WorldToMeterScale > 0.f);

	if (Layer.LayerDesc.HasShape<FSteamVRHQLayer>())
	{
		vr::VROverlayHandle_t ExistingHQLayer = VROverlay->GetHighQualityOverlay();
		if (ExistingHQLayer != vr::k_ulOverlayHandleInvalid && ExistingHQLayer != Layer.OverlayHandle)
		{
			UE_LOG(LogHMD, Warning, TEXT("There can only be one high-quality stereo layer active at a time. Please disable or change one or more layers to non-high-quality."));
		}
		else
		{
			VROverlay->SetHighQualityOverlay(Layer.OverlayHandle);

			const auto& HQSettings = Layer.LayerDesc.GetShape<FSteamVRHQLayer>();
			VROverlay->SetOverlayFlag(Layer.OverlayHandle, vr::VROverlayFlags_Curved, HQSettings.bCurved);
			VROverlay->SetOverlayFlag(Layer.OverlayHandle, vr::VROverlayFlags_RGSS4X, HQSettings.bAntiAlias);
			if (HQSettings.bCurved)
			{
				VROverlay->SetOverlayAutoCurveDistanceRangeInMeters(Layer.OverlayHandle, HQSettings.AutoCurveMinDistance / WorldToMeterScale, HQSettings.AutoCurveMaxDistance / WorldToMeterScale);
			}
		}
	}
	else if (!Layer.LayerDesc.HasShape<FQuadLayer>())
	{
		UE_LOG(LogHMD, Warning, TEXT("Unsupported StereoLayer shape. SteamVR StereoLayers can only be Quads or High Quality Quads."));
	}

	// UVs
	vr::VRTextureBounds_t TextureBounds;
    TextureBounds.uMin = Layer.LayerDesc.UVRect.Min.X;
    TextureBounds.uMax = Layer.LayerDesc.UVRect.Max.X;

	if ( IsOpenGLPlatform( GMaxRHIShaderPlatform ) )
	{
		TextureBounds.vMin = Layer.LayerDesc.UVRect.Max.Y;
		TextureBounds.vMax = Layer.LayerDesc.UVRect.Min.Y;
	}
	else
	{
		TextureBounds.vMin = Layer.LayerDesc.UVRect.Min.Y;
		TextureBounds.vMax = Layer.LayerDesc.UVRect.Max.Y;
	}

	OVR_VERIFY(VROverlay->SetOverlayTextureBounds(Layer.OverlayHandle, &TextureBounds));
	OVR_VERIFY(VROverlay->SetOverlayWidthInMeters(Layer.OverlayHandle, Layer.LayerDesc.QuadSize.X / WorldToMeterScale));
	
	float TexelAspect = 1.0f;
	// OpenVR overlays already take texture size into account, so we have to explicitly undo that in case the preserve texture ratio flag is not set. 
	if (!(Layer.LayerDesc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) && Layer.LayerDesc.Texture.IsValid())
	{
		FRHITexture2D* Texture2D = Layer.LayerDesc.Texture->GetTexture2D();
		if (Texture2D && Texture2D->GetSizeX() != 0)
		{
			// Initially set texel aspect so the image will be rendered in 1:1 ratio regardless of image size.
			TexelAspect = (float)Texture2D->GetSizeY() / (float)Texture2D->GetSizeX();
		}

		// Now apply the ratio determined by Quad size:
		if (Layer.LayerDesc.QuadSize.Y > 0.0f)
		{
			TexelAspect *= (Layer.LayerDesc.QuadSize.X / Layer.LayerDesc.QuadSize.Y);
		}
	}

	OVR_VERIFY(VROverlay->SetOverlayTexelAspect(Layer.OverlayHandle, TexelAspect));

	// Shift layer priority values up by -INT32_MIN, as SteamVR uses unsigned integers for the layer order where UE uses signed integers.
	// This will preserve the correct order between layers with negative and positive priorities
	OVR_VERIFY(VROverlay->SetOverlaySortOrder(Layer.OverlayHandle, Layer.LayerDesc.Priority-INT32_MIN));

	// Transform
	switch (Layer.LayerDesc.PositionType)
	{
	case ELayerType::WorldLocked:
	{
		// World locked layer positions are updated every frame.
		break;
	}
	case ELayerType::TrackerLocked:
	{
		vr::HmdMatrix34_t HmdTransform;
		TransformToSteamSpace(Layer.LayerDesc.Transform, HmdTransform, WorldToMeterScale);
		OVR_VERIFY(VROverlay->SetOverlayTransformAbsolute(Layer.OverlayHandle, VRCompositor->GetTrackingSpace(), &HmdTransform));
		break;
	}
	case ELayerType::FaceLocked:
	default:
	{
		vr::HmdMatrix34_t HmdTransform;
		TransformToSteamSpace(Layer.LayerDesc.Transform, HmdTransform, WorldToMeterScale);
		OVR_VERIFY(VROverlay->SetOverlayTransformTrackedDeviceRelative(Layer.OverlayHandle, vr::TrackingUniverseSeated, &HmdTransform));
	}
	};
}

//=============================================================================
void FSteamVRHMD::UpdateStereoLayers_RenderThread()
{
	// If we don't have a valid tracking position, the calls to ShowOverlay/SetOverlayTexture below will not have any effect.
	if (!HasValidTrackingPosition())
	{
		return;
	}

	// Metal is not supported yet
	check(!IsMetalPlatform(GMaxRHIShaderPlatform));

	static const auto CVarMixLayerPriorities = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.StereoLayers.bMixLayerPriorities"));
	const bool bUpdateLayerPriorities = (CVarMixLayerPriorities->GetValueOnRenderThread() == 0) && GetStereoLayersDirty();
	
	typedef TTuple<vr::VROverlayHandle_t, int32, bool /* bIsFaceLocked */> LayerPriorityInfo;
	TArray<LayerPriorityInfo> LayerPriorities;

	const float WorldToMeterScale = GetWorldToMetersScale();
	FTransform InvWorldTransform {ENoInit::NoInit};
	{
		// Calculate a transform to translate from world to tracker relative coordinates.
		FQuat AdjustedPlayerOrientation = BaseOrientation.Inverse() * PlayerOrientation;
		AdjustedPlayerOrientation.Normalize();

		FVector AdjustedPlayerLocation = PlayerLocation - AdjustedPlayerOrientation.RotateVector(BaseOffset);
		if (XRCamera.IsValid() && XRCamera->GetUseImplicitHMDPosition())
		{
			AdjustedPlayerLocation -= PlayerOrientation.RotateVector(RenderTrackingFrame.DevicePosition[IXRTrackingSystem::HMDDeviceId]);
		}

		InvWorldTransform = FTransform(AdjustedPlayerOrientation, AdjustedPlayerLocation).Inverse();
	}

	// We have loop through all layers every frame, in case we have world locked layers or continuously updated textures.
	ForEachLayer([&](uint32 /* unused */, FSteamVRLayer& Layer)
	{
		if (Layer.OverlayHandle != vr::k_ulOverlayHandleInvalid)
		{
			// Update world locked layer positions.
			if (Layer.LayerDesc.PositionType == ELayerType::WorldLocked)
			{
				vr::HmdMatrix34_t HmdTransform;
				TransformToSteamSpace(Layer.LayerDesc.Transform * InvWorldTransform, HmdTransform, WorldToMeterScale);
				OVR_VERIFY(VROverlay->SetOverlayTransformAbsolute(Layer.OverlayHandle, VRCompositor->GetTrackingSpace(), &HmdTransform));
			}

			// Update layer textures
			if (Layer.bUpdateTexture || (Layer.LayerDesc.Flags & LAYER_FLAG_TEX_CONTINUOUS_UPDATE))
			{
				vr::Texture_t Texture;
				vr::VRVulkanTextureData_t VulkanTexture{};
				if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
				{
#if PLATFORM_MAC
					check( 0 );
#else
					IVulkanDynamicRHI* VulkanRHI = GetIVulkanDynamicRHI();

					FRHITexture2D* TextureRHI2D = Layer.LayerDesc.Texture->GetTexture2D();
					check(TextureRHI2D);

					VulkanTexture.m_pInstance = VulkanRHI->RHIGetVkInstance();
					VulkanTexture.m_pDevice = VulkanRHI->RHIGetVkDevice();
					VulkanTexture.m_pPhysicalDevice = VulkanRHI->RHIGetVkPhysicalDevice();
					VulkanTexture.m_pQueue = VulkanRHI->RHIGetGraphicsVkQueue();
					VulkanTexture.m_nQueueFamilyIndex = VulkanRHI->RHIGetGraphicsQueueFamilyIndex();
					VulkanTexture.m_nImage = (uint64_t)VulkanRHI->RHIGetVkImage(TextureRHI2D);
					VulkanTexture.m_nWidth = TextureRHI2D->GetSizeX();
					VulkanTexture.m_nHeight = TextureRHI2D->GetSizeY();
					VulkanTexture.m_nFormat = (uint32_t)VulkanRHI->RHIGetViewVkFormat(TextureRHI2D);
					VulkanTexture.m_nSampleCount = 1;

					Texture.handle = &VulkanTexture;
					Texture.eType = vr::TextureType_Vulkan;
#endif
				}
				else if (RHIGetInterfaceType() == ERHIInterfaceType::OpenGL)
				{
					// We need to dereference the pointer to the real handle
					uintptr_t TextureID = *reinterpret_cast<uint32*>(Layer.LayerDesc.Texture->GetNativeResource());
					Texture.handle = reinterpret_cast<void*>(TextureID);
					Texture.eType = vr::TextureType_OpenGL;
				}
				else
				{
#if PLATFORM_WINDOWS
					Texture.handle = Layer.LayerDesc.Texture->GetNativeResource();
					Texture.eType = vr::TextureType_DirectX;
#else
					check( 0 );
#endif
				}
				Texture.eColorSpace = vr::ColorSpace_Auto;
				OVR_VERIFY(VROverlay->SetOverlayTexture(Layer.OverlayHandle, &Texture));
				OVR_VERIFY(VROverlay->ShowOverlay(Layer.OverlayHandle));

				Layer.bUpdateTexture = false;
			}

			if (bUpdateLayerPriorities)
			{
				LayerPriorities.Add(LayerPriorityInfo(Layer.OverlayHandle, Layer.LayerDesc.Priority, Layer.LayerDesc.PositionType == FaceLocked));
			}
		}
	});

	if (bUpdateLayerPriorities && LayerPriorities.Num() > 0)
	{
		auto SortLayersPredicate = [&](const LayerPriorityInfo& A, const LayerPriorityInfo& B)
		{
			const bool bAisFaceLocked = A.Get<2>();
			const bool bBisFaceLocked = B.Get<2>();

			if (bAisFaceLocked != bBisFaceLocked)
			{
				return bBisFaceLocked;
			}
			else
			{
				return A.Get<1>() < B.Get<1>();
			}
		};

		LayerPriorities.Sort(SortLayersPredicate);

		uint32 SortOrder = 0;
		int32 PrevPriority = LayerPriorities[0].Get<1>();
		bool PrevWasFaceLocked = LayerPriorities[0].Get<2>();

		for (LayerPriorityInfo Info : LayerPriorities)
		{
			// If multiple layers have the same priority, assign same sort order to them as well.
			if (PrevPriority != Info.Get<1>() || PrevWasFaceLocked != Info.Get<2>())
			{
				PrevPriority = Info.Get<1>();
				PrevWasFaceLocked = Info.Get<2>();
				SortOrder++;
			}
			OVR_VERIFY(VROverlay->SetOverlaySortOrder(Info.Get<0>(), SortOrder));
		}

	}
}

//=============================================================================
IStereoLayers* FSteamVRHMD::GetStereoLayers()
{
	// Metal is not supported yet. Fall back to the default portable implementation
	if (IsMetalPlatform(GMaxRHIShaderPlatform))
	{
		return FHeadMountedDisplayBase::GetStereoLayers();
	}

	if (VROverlay)
	{
		return this;
	}

	return nullptr;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#endif //STEAMVR_SUPPORTED_PLATFORMS
