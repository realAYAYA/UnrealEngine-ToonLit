// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMXPixelMappingOutputComponent.h"

#include "IDMXPixelMappingRenderer.h"
#include "Library/DMXEntityReference.h"

#include "Templates/SubclassOf.h"

#include "DMXPixelMappingRendererComponent.generated.h"

enum class EDMXPixelMappingRendererType : uint8;
class UDMXPixelMappingLayoutScript;

enum class EMapChangeType : uint8;
class UMaterialInterface;
class UTexture;
class UUserWidget;
class UTextureRenderTarget2D;
class UWorld;


/**
 * Component for rendering input texture
 */
UCLASS(BlueprintType, Blueprintable)
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingRendererComponent
	: public UDMXPixelMappingOutputComponent
{
	GENERATED_BODY()

public:
	/** Default Constructor */
	UDMXPixelMappingRendererComponent();

	/** Destructor */
	~UDMXPixelMappingRendererComponent();

	//~ Begin UObject implementation
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedChainEvent) override;
#endif // WITH_EDITOR
	//~ End UObject implementation

	//~ Begin UDMXPixelMappingBaseComponent implementation
	virtual const FName& GetNamePrefix() override;
	virtual void ResetDMX() override;
	virtual void SendDMX() override;
	virtual void Render() final;
	virtual void RenderAndSendDMX() final;
	//~ End UDMXPixelMappingBaseComponent implementation

#if WITH_EDITOR
	/** Render all downsample pixel for editor preview texture */
	void RenderEditorPreviewTexture();
	
	/** Get target for preview, create new one if does not exists. */
	UTextureRenderTarget2D* GetPreviewRenderTarget();
#endif

	/** Get reference to the active input texture */
	UTexture* GetRendererInputTexture() const;

	/** Get renderer interfece */
	const TSharedPtr<IDMXPixelMappingRenderer>& GetRenderer() { return PixelMappingRenderer; }

	/**
	 * Get pixel position in downsample buffer target based on pixel index
	 *
	 * @param InIndex Index of the pixel in buffer texture.
	 * @return FIntPoint X and Y position of the pixel in texture
	 */
	FIntPoint GetPixelPosition(int32 InIndex) const;

	/** Get active world. It could be editor or build world */
	UWorld* GetWorld() const;

#if WITH_EDITOR
	/**
	 * Take of container widget which is holds widget for all child components.
	 */
	UE_DEPRECATED(5.1, "Pixel Mapping Components no longer hold their own widget, in an effort to separate Views from Data.")
	TSharedRef<SWidget> TakeWidget();
#endif // WITH_EDITOR

	/*----------------------------------
		Blueprint interface
	----------------------------------*/

	/** Render input texture for downsampling */
	UFUNCTION(BlueprintCallable, Category = "DMX|PixelMapping")
	void RendererInputTexture();

	/** Create or update size of  buffer target for rendering downsample pixels */
	void CreateOrUpdateDownsampleBufferTarget();

	/** 
	 * Add pixel params for downsampling set
	 *
	 * @param InDownsamplePixelParam pixel rendering params
	 */
	void AddPixelToDownsampleSet(FDMXPixelMappingDownsamplePixelParam&& InDownsamplePixelParam);

	/** Get amount of downsample pixels */
	int32 GetDownsamplePixelNum() const { return DownsamplePixelParams.Num(); }

	/**
	 * Pass the downsample CPU buffer from Render Thread to Game Thread and store 
	 * 
	 * @param InDownsampleBuffer CPU buffer
	 * @param InRect buffer X and Y dimension
	 */
	void SetDownsampleBuffer(TArray<FLinearColor>&& InDownsampleBuffer, FIntRect InRect);

	/** Get Pixel color by given downsample pixel index. Returns false if no color value could be acquired */
	bool GetDownsampleBufferPixel(const int32 InDownsamplePixelIndex, FLinearColor& OutLinearColor);

	/** Get Pixels color by given downsample pixel range. Returns false if no color values could be acquired */
	bool GetDownsampleBufferPixels(const int32 InDownsamplePixelIndexStart, const int32 InDownsamplePixelIndexEnd, TArray<FLinearColor>& OutLinearColors);

	/** Reset the color by given downsample pixel index */
	bool ResetColorDownsampleBufferPixel(const int32 InDownsamplePixelIndex);

	/** Reset the color by given downsample pixel range */
	bool ResetColorDownsampleBufferPixels(const int32 InDownsamplePixelIndexStart, const int32 InDownsamplePixelIndexEnd);

	/** Remove all pixels from DownsampleBuffer */
	void EmptyDownsampleBuffer();

private:
	/** Resize input target based on X and Y input material size  */
	void ResizeMaterialRenderTarget(int32 InSizeX, int32 InSizeY);

	/** Generate new input widget based on UMG */
	void UpdateInputWidget(TSubclassOf<UUserWidget> InInputWidget);

	/** Resize output texture for editor preview */
	void ResizePreviewRenderTarget(uint32 InSizeX, uint32 InSizeY);

#if WITH_EDITOR
	/** Map changer handler */
	void OnMapChanged(UWorld* InWorld, EMapChangeType MapChangeType);
#endif

	/** Initialize all textures and creation or loading asset */
	void Initialize();

	/** Create a render target with unique name */
	UTextureRenderTarget2D* CreateRenderTarget(const FName& InBaseName);

public:
	/**
	 * Returns the Modulators of the component corresponding to the patch specified. 
	 * Note, this node does a lookup on all fixture patches in use, hence may be slow and shouldn't be called on tick. 
	 */
	UFUNCTION(BlueprintCallable, Category = "DMX")
	bool GetPixelMappingComponentModulators(FDMXEntityFixturePatchRef FixturePatchRef, TArray<UDMXModulator*>& DMXModulators);

	/** Type of rendering, Texture, Material, UMG, etc... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render Settings")
	EDMXPixelMappingRendererType RendererType;

	/** Texture to Downsampling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render Settings")
	TObjectPtr<UTexture> InputTexture;

	/** Material to Downsampling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render Settings", meta = (DisplayName = "User Interface Material"))
	TObjectPtr<UMaterialInterface> InputMaterial;

	/** UMG to Downsampling */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render Settings")
	TSubclassOf<UUserWidget> InputWidget;
	
	/** The brightness of the renderer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render Settings", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float Brightness;

	/** Layout script for the children of this component (hidden in customizations and displayed in its own panel). */
	UPROPERTY(EditAnywhere, Instanced, Category = "Layout")
	TObjectPtr<UDMXPixelMappingLayoutScript> LayoutScript;

	/** Check if a Component can be moved under another one (used for copy/move/duplicate) */
	virtual bool CanBeMovedTo(const UDMXPixelMappingBaseComponent* Component) const override;

#if WITH_EDITOR
	/** Returns the component canvas used for this widget */
	FORCEINLINE TSharedPtr<SConstraintCanvas> GetComponentsCanvas() const { return ComponentsCanvas; }
#endif // WITH_EDITOR

private:
#if WITH_EDITORONLY_DATA
	/** Editor preview output target */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget;
#endif // WITH_EDITORONLY_DATA

	/** Material of UMG texture to downsample */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> InputRenderTarget;

	/** Reference to renderer */
	TSharedPtr<IDMXPixelMappingRenderer> PixelMappingRenderer;

	/** UMG widget for downsampling */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> UserWidget;

#if WITH_EDITORONLY_DATA
	/** Canvas for all UI downsamping component widgets */
	TSharedPtr<SConstraintCanvas> ComponentsCanvas;

	/** Change level Delegate */
	FDelegateHandle OnChangeLevelHandle;
#endif // WITH_EDITORONLY_DATA

	/** Retrieve total count of all output targets that support shared rendering and updates a counter. O(n) */
	int32 GetTotalDownsamplePixelCount();

	/** Helper function checks the downsample pixel range */
	bool IsPixelRangeValid(const int32 InDownsamplePixelIndexStart, const int32 InDownsamplePixelIndexEnd) const;

	/** GPU downsample pixel buffer target texture */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> DownsampleBufferTarget;

	/** CPU downsample pixel buffer */
	TArray<FLinearColor> DownsampleBuffer;

	/** Counter for all pixels from child components */
	int32 DownsamplePixelCount;

	/** Critical section for set, update and get color array */
	FCriticalSection DownsampleBufferCS;

	/** Hold the params of the pixels for downsamle rendering */
	TArray<FDMXPixelMappingDownsamplePixelParam> DownsamplePixelParams;

	/** Initial texture color */
	static const FLinearColor ClearTextureColor;

public:
	/** Max downsample target size */
	static const FIntPoint MaxDownsampleBufferTargetSize;
};
