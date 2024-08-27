#include "ToonTextures.h"

void FToonTextures::InitializeViewFamily(FRDGBuilder& GraphBuilder, FViewFamilyInfo& ViewFamily)
{
}

EPixelFormat FToonTextures::GetTBufferFFormatAndCreateFlags(ETextureCreateFlags& OutCreateFlags)
{
	return PF_Unknown;
}

uint32 FToonTextures::GetTBufferRenderTargets(TArrayView<FTextureRenderTargetBinding> RenderTargets) const
{
	return 1;
}

uint32 FToonTextures::GetTBufferRenderTargets(ERenderTargetLoadAction LoadAction, FRenderTargetBindingSlots& RenderTargets) const
{
	return 1;
}
