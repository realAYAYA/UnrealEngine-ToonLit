// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "CoreMinimal.h"
#include "Engine/VolumeTexture.h"
#include "OpenColorIOColorSpace.h"
#include "OpenColorIOShared.h"
#include "RenderCommandFence.h"
#include "RHIDefinitions.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "OpenColorIOColorTransform.generated.h"


class UOpenColorIOConfiguration;


/**
 * Object used to generate shader and LUTs from OCIO configuration file and contain required resource to make a color space transform.
 */
UCLASS()
class OPENCOLORIO_API UOpenColorIOColorTransform : public UObject
{
	GENERATED_BODY()

public:

	UOpenColorIOColorTransform(const FObjectInitializer& ObjectInitializer);
	virtual ~UOpenColorIOColorTransform() {};

public:
	bool Initialize(UOpenColorIOConfiguration* InOwner, const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	bool Initialize(UOpenColorIOConfiguration* InOwner, const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);

	/**
	 * Cache resource shaders for cooking on the given shader platform.
	 * If a matching shader map is not found in memory or the DDC, a new one will be compiled.
	 * This does not apply completed results to the renderer scenes.
	 * Caller is responsible for deleting OutCachedResource.
	 */
	void CacheResourceShadersForCooking(EShaderPlatform InShaderPlatform, const ITargetPlatform* TargetPlatform, const FString& InShaderHash, const FString& InShaderCode, const FString& InRawConfigHash, TArray<FOpenColorIOTransformResource*>& OutCachedResources);

	/**
	 * Serialize LUT data. This will effectively serialize the LUT only when cooking
	 */
	void SerializeLuts(FArchive& Ar);

	/**
	 * Cache resource LUT for rendering/cooking on the given shader platform.
	 * If a matching texture is not found in DDC, a new one will be generated from raw data.
	 */
	void CacheResourceTextures();

	/**
	 * Cache resource shaders for rendering.
	 * If a matching shader map is not found in memory or the DDC, a new one will be compiled.
	 */
	void CacheResourceShadersForRendering(bool bRegenerateId);
	void CacheShadersForResources(EShaderPlatform InShaderPlatform, FOpenColorIOTransformResource* InResourcesToCache, bool bApplyCompletedShaderMapForRendering, bool bIsCooking, const ITargetPlatform* TargetPlatform = nullptr);

	FOpenColorIOTransformResource* AllocateResource();

	/**
	 * Returns the desired resources required to apply this transform during rendering.
	 */
	bool GetRenderResources(ERHIFeatureLevel::Type InFeatureLevel, FOpenColorIOTransformResource*& OutShaderResource, TSortedMap<int32, FTextureResource*>& OutTextureResources);

	UE_DEPRECATED(5.1, "GetShaderAndLUTResouces is deprecated, please use GetRenderResources instead.")
	bool GetShaderAndLUTResouces(ERHIFeatureLevel::Type InFeatureLevel, FOpenColorIOTransformResource*& OutShaderResource, FTextureResource*& OutLUT3dResource);

	bool IsTransform(const FString& InSourceColorSpace, const FString& InDestinationColorSpace) const;
	bool IsTransform(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection) const;

	/**
	 * Get the display view direction type, when applicable.
	 *
	 * @param OutDirection The returned direction.
	 * @return True if the transformation is of display-view type.
	 */
	bool GetDisplayViewDirection(EOpenColorIOViewTransformDirection& OutDirection) const;

	// For all ColorTransforms, UOpenColorIOColorTransform::CacheResourceShadersForRendering
	static void AllColorTransformsCacheResourceShadersForRendering();

	

protected:

	/**
	 * Helper function to serialize shader maps for the given color transform resources.
	 */
	static void SerializeOpenColorIOShaderMaps(const TMap<const ITargetPlatform*, TArray<FOpenColorIOTransformResource*>>* PlatformColorTransformResourcesToSavePtr, FArchive& Ar, TArray<FOpenColorIOTransformResource>&  OutLoadedResources);
	
	/**
	 * Helper function to register serialized shader maps for the given color transform resources
	 */
	static void ProcessSerializedShaderMaps(UOpenColorIOColorTransform* Owner, TArray<FOpenColorIOTransformResource>& LoadedResources, FOpenColorIOTransformResource* (&OutColorTransformResourcesLoaded)[ERHIFeatureLevel::Num]);
	
	/**
	 * Returns a Guid for the LUT based on its unique identifier, name and the OCIO DDC key.
	 */
	static void GetOpenColorIOLUTKeyGuid(const FString& InProcessorIdentifier, const FName& InName, FGuid& OutLutGuid );

	UE_DEPRECATED(5.1, "This version of GetOpenColorIOLUTKeyGuid is deprecated. Please use the version that takes InName instead.")
	static void GetOpenColorIOLUTKeyGuid(const FString& InLutIdentifier, FGuid& OutLutGuid);

	/**
	 * Generate the LUT and shader associated to the desired color space transform.
	 * @note: Only in editor
	 * @return true if parameters are valid.
	 */
	bool GenerateColorTransformData(const FString& InSourceColorSpace, const FString& InDestinationColorSpace);
	bool GenerateColorTransformData(const FString& InSourceColorSpace, const FString& InDisplay, const FString& InView, EOpenColorIOViewTransformDirection InDirection);

	/**
	 * Helper function returning the color space transform name based on source and destination color spaces.
	 */
	FString GetTransformFriendlyName();

	/**
	 * Fetch shader code and hash from the OCIO library
	 * @note: Uses library only in editor. 
	 * @return: true if not in editor (shader has been serialized during cooking), true if shader could be generated from the library otherwise
	 */
	bool UpdateShaderInfo(FString& OutShaderCodeHash, FString& OutShaderCode, FString& OutRawConfigHash);

	/**
	 * Helper function taking raw 3D LUT data coming from the library and initializing a UVolumeTexture with it.
	 */
	TObjectPtr<UTexture> CreateTexture3DLUT(const FString& InProcessorIdentifier, const FName& InName, uint32 InLutLength, TextureFilter InFilter, const float* InSourceData);

	UE_DEPRECATED(5.1, "Update3dLutTexture is deprecated, please use CreateTexture3DLUT instead.")
	void Update3dLutTexture(const FString& InLutIdentifier, const float* InSourceData);

	/**
	 * Helper function taking raw 1D LUT data coming from the library and initializing a UTexture with it.
	 */
	TObjectPtr<UTexture> CreateTexture1DLUT(const FString& InProcessorIdentifier, const FName& InName, uint32 InTextureWidth, uint32 InTextureHeight, TextureFilter InFilter, bool bRedChannelOnly, const float* InSourceData );

private:
	void FlushResourceShaderMaps();

public:

	//~ Begin UObject interface
	void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif

	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void FinishDestroy() override;

#if WITH_EDITOR
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearCachedCookedPlatformData(const ITargetPlatform *TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;

#endif //WITH_EDITOR
	//~ End UObject interface

	/**
	 * Releases rendering resources used by this color transform.
	 * This should only be called directly if the ColorTransform will not be deleted through the GC system afterward.
	 * FlushRenderingCommands() must have been called before this.
	 */
	void ReleaseResources();

public:

	UPROPERTY()
	TObjectPtr<UOpenColorIOConfiguration> ConfigurationOwner;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	bool bIsDisplayViewType = false;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	FString SourceColorSpace;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	FString DestinationColorSpace;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	FString Display;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	FString View;

	UPROPERTY(VisibleAnywhere, Category = "ColorSpace")
	EOpenColorIOViewTransformDirection DisplayViewDirection = EOpenColorIOViewTransformDirection::Forward;
private:

	/** If the color space requires textures, this will contains the data to do the transform */
	/** Note: This will be serialized when cooking. Otherwhise, it relies on raw data of the library and what's on DDC */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UTexture>> Textures;
	
	/** Inline ColorTransform resources serialized from disk. To be processed on game thread in PostLoad. */
	TArray<FOpenColorIOTransformResource> LoadedTransformResources;
	
	FOpenColorIOTransformResource* ColorTransformResources[ERHIFeatureLevel::Num];

	FRenderCommandFence ReleaseFence;

#if WITH_EDITORONLY_DATA
	/* ColorTransform resources being cached for cooking. */
	TMap<const class ITargetPlatform*, TArray<FOpenColorIOTransformResource*>> CachedColorTransformResourcesForCooking;

	/** Handle so we can unregister the delegate */
	FDelegateHandle FeatureLevelChangedDelegateHandle;
#endif //WITH_EDITORONLY_DATA
};
