// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IStereoLayers.h: Abstract interface for adding in stereoscopically projected
	layers on top of the world
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "StereoLayerShapes.h"
#include "RHI.h"

class IStereoLayers
{

	// The pointer to the shape of a stereo layer is wrapped in this simple class that clones and destroys the shape.
	// This is to avoid having to implement non-default copy and assignment operations for the entire FLayerDesc struct.
	struct FShapeWrapper
	{
		TUniquePtr<IStereoLayerShape> Wrapped;
		FShapeWrapper(IStereoLayerShape* InWrapped)
			: Wrapped(InWrapped)
		{}

		FShapeWrapper(const FShapeWrapper& In) :
			Wrapped(In.IsValid()?In.Wrapped->Clone():nullptr)
		{}

		FShapeWrapper(FShapeWrapper&& In)
		{
			Wrapped = MoveTemp(In.Wrapped);
		}

		FShapeWrapper& operator= (IStereoLayerShape* InWrapped)
		{
			Wrapped = TUniquePtr<IStereoLayerShape>(InWrapped);
			return *this;
		}

		FShapeWrapper& operator= (const FShapeWrapper& In)
		{
			return operator=(In.IsValid() ? In.Wrapped->Clone() : nullptr);
		}

		FShapeWrapper& operator= (FShapeWrapper&& In)
		{
			Wrapped = MoveTemp(In.Wrapped);
			return *this;
		}

		bool IsValid() const { return Wrapped.IsValid(); }
	};

public:

	enum ELayerType
	{
		WorldLocked,
		TrackerLocked,
		FaceLocked
	};

	enum ELayerFlags
	{
		// Internally copies the texture on every frame for video, etc.
		LAYER_FLAG_TEX_CONTINUOUS_UPDATE	= 0x00000001,
		// Ignore the textures alpha channel, this makes the stereo layer opaque. Flag is ignored on Steam VR.
		LAYER_FLAG_TEX_NO_ALPHA_CHANNEL		= 0x00000002,
		// Quad Y component will be calculated based on the texture dimensions
		LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO	= 0x00000004,
		// The layer will intersect with the scene's depth. Currently only supported on Oculus platforms.
		LAYER_FLAG_SUPPORT_DEPTH = 0x00000008,
		// Required on some platforms to enable rendering of external textures.
		LAYER_FLAG_TEX_EXTERNAL = 0x00000010,
		// When set, this layer will not be rendered.
		LAYER_FLAG_HIDDEN = 0x00000020,
	};


	/**
	 * Structure describing the visual appearance of a single stereo layer
	 */
	struct FLayerDesc
	{

		FLayerDesc()
			: Shape(new FQuadLayer())
		{
		}

		FLayerDesc(const IStereoLayerShape& InShape)
			: Shape(InShape.Clone())
		{
		}

		void SetLayerId(uint32 InId) { Id = InId; }
		uint32 GetLayerId() const { return Id; }
		bool IsVisible() const { return Texture != nullptr && !(Flags & LAYER_FLAG_HIDDEN); }

		// Layer IDs must be larger than 0
		const static uint32	INVALID_LAYER_ID = 0; 
		// The layer's ID
		uint32				Id			= INVALID_LAYER_ID;
		// View space transform
		FTransform			Transform	 = FTransform::Identity;
		// Size of rendered quad
		FVector2D			QuadSize	 = FVector2D(1.0f, 1.0f);
		// UVs of rendered quad in UE units
		FBox2D				UVRect		 = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));

		// Size of texture that the compositor should allocate. Unnecessary if Texture is provided. The compositor will allocate a cubemap whose faces are of LayerSize if ShapeType is CubemapLayer.
		FIntPoint			LayerSize = FIntPoint(0, 0);
		// Render order priority, higher priority render on top of lower priority. Face-Locked layers are rendered on top of other layer types regardless of priority. 
		int32				Priority	 = 0;
		// Which space the layer is locked within
		ELayerType			PositionType = ELayerType::FaceLocked;
		// which shape of layer it is. FQuadLayer is the only shape supported by all VR platforms.
		// queries the shape of the layer at run time
		template <typename T> bool HasShape() const { check(Shape.IsValid()); return Shape.Wrapped->GetShapeName() == T::ShapeName; }
		// returns Shape cast to the supplied type. It's up to the caller to have ensured the cast is valid before calling this method.
		template <typename T> T& GetShape() { check(HasShape<T>()); return *static_cast<T*>(Shape.Wrapped.Get()); }
		template <typename T> const T& GetShape() const { check(HasShape<T>()); return *static_cast<const T*>(Shape.Wrapped.Get()); }
		template <typename T, typename... InArgTypes> void SetShape(InArgTypes&&... Args) { Shape = FShapeWrapper(new T(Forward<InArgTypes>(Args)...)); }


		// Texture mapped for right eye (if one texture provided, mono assumed)
		FTextureRHIRef		Texture		 = nullptr;	
		// Texture mapped for left eye (if one texture provided, mono assumed)
		FTextureRHIRef		LeftTexture  = nullptr;
		// Uses LAYER_FLAG_... -- See: ELayerFlags
		uint32				Flags		 = 0;
	private: 
		FShapeWrapper		Shape;
	};

	virtual ~IStereoLayers() { }

	/**
	 * Creates a new layer from a given texture resource, which is projected on top of the world as a quad.
	 *
	 * @param	InLayerDesc		A reference to the texture resource to be used on the quad
	 * @return	A unique identifier for the layer created
	 */
	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) = 0;
	
	/**
	 * Destroys the specified layer, stopping it from rendering over the world.
	 *
	 * @param	LayerId		The ID of layer to be destroyed
	 */
	virtual void DestroyLayer(uint32 LayerId) = 0;

	/**
	 * Saves the current stereo layer state on a stack to later restore them.
	 *
	 * Useful for creating temporary overlays that should be torn down later.
	 *
	 * When bPreserve is false, existing layers will be temporarily disabled and restored again when calling PopLayerState()
	 * The disabled layer's properties are still accessible by calling Get and SetLayerDesc, but nothing will change until after
	 * the state has been restored. Calling DestroyLayer on an inactive layer, will prevent it from being restored when PopLayerState() is called.
	 *
	 * When bPreserve is true, existing layers will remain active, but when calling PopLayerState(), any changed properties
	 * will be restored back to their previous values. Calling DestroyLayer on an active layer id will make the layer inactive. The layer
	 * will be reactivated when the state is restored. (You can call DestroyLayer multiple times on the same layer id to remove successively older
	 * versions of a layer.)
	 *
	 * In either case, layers created after PushLayerState() will be destroyed upon calling PopLayerState(). 
	 * 
	 * @param	bPreserve	Whether the existing layers should be preserved after saving the state. If false all existing layers will be disabled.
	 */
	virtual void PushLayerState(bool bPreserve = false) {};

	/**
	 * Restores the stereo layer state from the last save state. 
	 * 
	 * Currently active layers will be destroyed and replaced with the previous state.
	 */
	virtual void PopLayerState() {};

	/**
	 * Returns true if the StereoLayers implementation supports saving and restoring state using Push/PopLayerState()
	 */
	virtual bool SupportsLayerState() { return false; }

	/** 
	 * Optional method to hide the 3D scene and only render the stereo overlays. 
	 * No-op if not supported by the platform.
	 *
	 * If pushing and popping layer state is supported, the visibility of the background layer should be part of
	 * the saved state.
	 */
	virtual void HideBackgroundLayer() {}

	/**
	 * Optional method to undo the effect of hiding the 3D scene.
	 * No-op if not supported by the platform.
	 */
	virtual void ShowBackgroundLayer() {}

	/** 
	 * Tell if the background layer is visible. Platforms that do not implement Hide/ShowBackgroundLayer() 
	 * always return true.
	 */
	virtual bool IsBackgroundLayerVisible() const { return true; }

	/**
	 * Set the a new layer description
	 *
	 * @param	LayerId		The ID of layer to be set the description
	 * @param	InLayerDesc	The new description to be set
	 */
	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) = 0;

	/**
	 * Get the currently set layer description
	 *
	 * @param	LayerId			The ID of layer to be set the description
	 * @param	OutLayerDesc	The returned layer description
	 * @return	Whether the returned layer description is valid
	 */
	virtual bool GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc) = 0;

	/**
	 * Marks this layers texture for update
	 *
	 * @param	LayerId			The ID of layer to be set the description
	 */
	virtual void MarkTextureForUpdate(uint32 LayerId) = 0;

	/**
	 * Update splash screens from current state
	 */
	virtual void UpdateSplashScreen() {};

	/**
	* If true the debug layers are copied to the spectator screen, because they do not naturally end up on the spectator screen as part of the 3d view.
	*/
	virtual bool ShouldCopyDebugLayersToSpectatorScreen() const = 0;

public:
	/**
	* Set the splash screen attributes
	*
	* @param Texture			(in) A texture to be used for the splash. B8R8G8A8 format.
	* @param Scale				(in) Scale of the texture.
	* @param Offset				(in) Position from which to start rendering the texture.
	* @param ShowLoadingMovie	(in) Whether the splash screen presents loading movies.
	*/
	UE_DEPRECATED(4.24, "Use the IXRLoadingScreen interface instead of IStereoLayers::*SplashScreen")
	void SetSplashScreen(FTextureRHIRef Texture, FVector2D Scale, FVector Offset, bool bShowLoadingMovie)
	{
		bSplashShowMovie = bShowLoadingMovie;
		SplashTexture = nullptr;
		if (Texture)
		{
			SplashTexture = Texture->GetTexture2D();
			SplashOffset = Offset;
			SplashScale = Scale;
		}
	}

	/**
	* Show the splash screen and override the normal VR display
	*/
	UE_DEPRECATED(4.24, "Use the IXRLoadingScreen interface instead of IStereoLayers::*SplashScreen")
	void ShowSplashScreen()
	{
		bSplashIsShown = true;
		UpdateSplashScreen();
	}

	/**
	* Hide the splash screen and return to normal display.
	*/
	UE_DEPRECATED(4.24, "Use the IXRLoadingScreen interface instead of IStereoLayers::*SplashScreen")
	void HideSplashScreen()
	{
		bSplashIsShown = false;
		UpdateSplashScreen();
	}

	/**
	* Set the splash screen's movie texture.
	*
	* @param InMovieTexture		(in) A movie texture to be used for the splash. B8R8G8A8 format.
	*/
	UE_DEPRECATED(4.24, "Use the IXRLoadingScreen interface instead of IStereoLayers::*SplashScreen")
	void SetSplashScreenMovie(FTextureRHIRef Texture)
	{
		SplashMovie = nullptr;
		if (Texture)
		{
			SplashMovie = Texture->GetTexture2D();
			bSplashShowMovie = true;
		}
		UpdateSplashScreen();
	}

	virtual FLayerDesc GetDebugCanvasLayerDesc(FTextureRHIRef Texture)
	{
		// Default debug layer desc
		IStereoLayers::FLayerDesc StereoLayerDesc;
		StereoLayerDesc.Transform = FTransform(FVector(100.f, 0, 0));
		StereoLayerDesc.QuadSize = FVector2D(120.f, 120.f);
		StereoLayerDesc.PositionType = IStereoLayers::ELayerType::FaceLocked;
		StereoLayerDesc.Texture = Texture;
		StereoLayerDesc.Flags = IStereoLayers::ELayerFlags::LAYER_FLAG_TEX_CONTINUOUS_UPDATE;
		StereoLayerDesc.Flags |= IStereoLayers::ELayerFlags::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO;
		return StereoLayerDesc;
	}

	/**
	* Get texture reference to HMD swapchain to avoid the copy path, useful for continuous update layers
	*/
	virtual void GetAllocatedTexture(uint32 LayerId, FTextureRHIRef &Texture, FTextureRHIRef &LeftTexture)
	{
		Texture = nullptr;
		LeftTexture = nullptr;
	}

protected:
	bool				bSplashIsShown = false;
	bool				bSplashShowMovie = false;
	FTexture2DRHIRef	SplashTexture;
	FTexture2DRHIRef	SplashMovie;
	FVector			    SplashOffset = FVector::ZeroVector;
	FVector2D			SplashScale = FVector2D(1.0f, 1.0f);
	uint32				SplashLayerHandle = 0;
};
