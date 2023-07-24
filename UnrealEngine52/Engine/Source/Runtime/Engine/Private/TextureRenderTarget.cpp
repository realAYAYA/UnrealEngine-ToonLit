// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureRenderTarget.cpp: UTextureRenderTarget implementation
=============================================================================*/

#include "Engine/TextureRenderTarget.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "TextureResource.h"
#include "SceneRenderTargetParameters.h"
#include "EngineModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureRenderTarget)

/*-----------------------------------------------------------------------------
UTextureRenderTarget
-----------------------------------------------------------------------------*/

UTextureRenderTarget::UTextureRenderTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NeverStream = true;
	SRGB = true;
	LODGroup = TEXTUREGROUP_RenderTarget;	
	bNeedsTwoCopies = false;
	bCanCreateUAV = false;
#if WITH_EDITORONLY_DATA
	CompressionNone = true;
#endif // #if WITH_EDITORONLY_DATA
}


FTextureRenderTargetResource* UTextureRenderTarget::GetRenderTargetResource()
{
	check(IsInRenderingThread() || 
		(IsInParallelRenderingThread() && (!GetResource() || GetResource()->IsInitialized()))); // we allow this in parallel, but only if the resource is initialized...otherwise it might be a race on intialization
	FTextureRenderTargetResource* Result = NULL;
	if (GetResource() &&
		GetResource()->IsInitialized() )
	{
		Result = static_cast<FTextureRenderTargetResource*>(GetResource());
	}
	return Result;
}


FTextureRenderTargetResource* UTextureRenderTarget::GameThread_GetRenderTargetResource()
{
	check( IsInGameThread() );
	return static_cast< FTextureRenderTargetResource*>(GetResource());
}


FTextureResource* UTextureRenderTarget::CreateResource()
{
	return NULL;
}


EMaterialValueType UTextureRenderTarget::GetMaterialType() const
{
	return MCT_Texture;
}

/*-----------------------------------------------------------------------------
	FTextureRenderTargetResource
-----------------------------------------------------------------------------*/

/** 
 * Return true if a render target of the given format is allowed
 * for creation
 */
bool FTextureRenderTargetResource::IsSupportedFormat( EPixelFormat Format )
{
	switch( Format )
	{
	case PF_B8G8R8A8:
	case PF_R8G8B8A8:
	case PF_A16B16G16R16:
	case PF_FloatRGB:
	case PF_FloatRGBA: // for exporting materials to .obj/.mtl
	case PF_FloatR11G11B10: //Pixel inspector for Reading HDR Color
	case PF_A2B10G10R10: //Pixel inspector for normal buffer
	case PF_DepthStencil: //Pixel inspector for depth and stencil buffer
	case PF_G16:// for heightmaps
		return true;
	default:
		return false;
	}
}

/** 
* Render target resource should be sampled in linear color space
*
* @return display gamma expected for rendering to this render target 
*/
float FTextureRenderTargetResource::GetDisplayGamma() const
{
	return 2.2f;  
}

/*-----------------------------------------------------------------------------
FDeferredUpdateResource
-----------------------------------------------------------------------------*/

/** 
* if true then FDeferredUpdateResource::UpdateResources needs to be called 
* (should only be set on the rendering thread)
*/
bool FDeferredUpdateResource::bNeedsUpdate = true;

/** 
* Resources can be added to this list if they need a deferred update during scene rendering.
* @return global list of resource that need to be updated. 
*/
TLinkedList<FDeferredUpdateResource*>*& FDeferredUpdateResource::GetUpdateList()
{		
	static TLinkedList<FDeferredUpdateResource*>* FirstUpdateLink = NULL;
	return FirstUpdateLink;
}

/**
* Iterate over the global list of resources that need to
* be updated and call UpdateResource on each one.
*/
void FDeferredUpdateResource::UpdateResources(FRHICommandListImmediate& RHICmdList)
{
	if( bNeedsUpdate )
	{
		TLinkedList<FDeferredUpdateResource*>*& UpdateList = FDeferredUpdateResource::GetUpdateList();
		for( TLinkedList<FDeferredUpdateResource*>::TIterator ResourceIt(UpdateList);ResourceIt; )
		{
			FDeferredUpdateResource* RTResource = *ResourceIt;
			// iterate to next resource before removing an entry
			ResourceIt.Next();

			if( RTResource )
			{
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(FlushDeferredResourceUpdate);
				RTResource->FlushDeferredResourceUpdate(RHICmdList);
			}
		}
		// since the updates should only occur once globally
		// then we need to reset this before rendering any viewports
		bNeedsUpdate = false;
	}
}

/**
 * Performs a deferred resource update on this resource if it exists in the UpdateList.
 */
void FDeferredUpdateResource::FlushDeferredResourceUpdate( FRHICommandListImmediate& RHICmdList )
{
	if( UpdateListLink.IsLinked() )
	{
		checkf(bNeedsUpdate, TEXT("The update list does not need to be updated at this point"));

		UpdateDeferredResource(RHICmdList);
		if( bOnlyUpdateOnce )
		{
			// Remove from list if only a single update was requested
			RemoveFromDeferredUpdateList();
		}
	}
}

/**
* Add this resource to deferred update list
* @param OnlyUpdateOnce - flag this resource for a single update if true
*/
void FDeferredUpdateResource::AddToDeferredUpdateList( bool OnlyUpdateOnce )
{
	bool bExists=false;
	TLinkedList<FDeferredUpdateResource*>*& UpdateList = FDeferredUpdateResource::GetUpdateList();
	for( TLinkedList<FDeferredUpdateResource*>::TIterator ResourceIt(UpdateList);ResourceIt;ResourceIt.Next() )
	{
		if( (*ResourceIt) == this )
		{
			bExists=true;
			break;
		}
	}
	if( !bExists )
	{
		UpdateListLink = TLinkedList<FDeferredUpdateResource*>(this);
		UpdateListLink.LinkHead(UpdateList);
		bNeedsUpdate = true;
	}
	bOnlyUpdateOnce=OnlyUpdateOnce;
}

/**
* Remove this resource from deferred update list
*/
void FDeferredUpdateResource::RemoveFromDeferredUpdateList()
{
	UpdateListLink.Unlink();
}

void FDeferredUpdateResource::ResetSceneTextureExtentsHistory()
{
	GetRendererModule().ResetSceneTextureExtentHistory();
}
