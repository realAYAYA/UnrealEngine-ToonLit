// Copyright Epic Games, Inc. All Rights Reserved.

#define BINKRHIFUNCTIONS
#define BINKTEXTURESCLEANUP
#include "egttypes.h"
#include "binktiny.h"
#include "binktextures.h"
#include "binkplugin.h"

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "RHI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Runtime/RHI/Public/RHIStaticStates.h"
#include "Runtime/RenderCore/Public/ShaderParameterUtils.h"
#include "Runtime/RenderCore/Public/RenderResource.h"
#include "Runtime/Renderer/Public/MaterialShader.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"
#include "Runtime/RenderCore/Public/RenderGraphResources.h"

static BINKSHADERS * shaders;

static int attached = 0;
static unsigned rfb;

FRHITexture2D *BinkRHIRenderTarget;
ERenderTargetLoadAction BinkRenderTargetLoadAction;

RADDEFFUNC int setup_rhi( void * device, BINKPLUGININITINFO * info, S32 gpu_assisted, void ** context )
{
  if ( shaders == 0 )
  {
    shaders = Create_Bink_shaders( device );
    if ( shaders == 0 )
      return 0;
  }

  return 1;
}

RADDEFFUNC void shutdown_rhi( void )
{
  if ( shaders )
  {
    Free_Bink_shaders( shaders );
    shaders = 0;
  }
}

RADDEFFUNC void * createtextures_rhi( void * bink )
{
  void * ret = 0;
  if ( shaders )
  {
    ret = Create_Bink_textures( shaders, (HBINK)bink, 0 );
  }
  return ret;
}

RADDEFFUNC void selectrendertarget_rhi(void* texture_target, S32 width, S32 height, S32 do_clear, U32 sdr_or_hdr, S32 current_resource_state)
{
	if (texture_target)
	{
		attached = 1;
		BinkRHIRenderTarget = (FRHITexture2D*)texture_target;
		BinkRenderTargetLoadAction = do_clear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;
	}
}

RADDEFFUNC void selectscreenrendertarget_rhi( void * texture_target, S32 width, S32 height, U32 format_idx, S32 current_resource_state )
{
	if (texture_target)
	{
		attached = 2;
		BinkRHIRenderTarget = (FRHITexture2D*)texture_target;
		BinkRenderTargetLoadAction = ERenderTargetLoadAction::ELoad;
	}
}

RADDEFFUNC void clearrendertarget_rhi(void)
{
	if (attached)
	{
		BinkRHIRenderTarget = NULL;
		attached = 0;
	}
}
  
RADDEFFUNC void begindraw_rhi( void )
{
}

RADDEFFUNC void enddraw_rhi( void )
{
  clearrendertarget_rhi();
}
