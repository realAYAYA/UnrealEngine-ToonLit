// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject Data Helpers
//////////////////////////////////////////////////////////////////////////////////////////////
const FTextureShareCoreSceneViewData* FTextureShareObject::GetSceneViewData(const FTextureShareCoreViewDesc& InViewDesc) const
{
	for (const FTextureShareCoreObjectProxyData& CoreObjectProxyDataIt : ReceivedCoreObjectProxyData)
	{
		return CoreObjectProxyDataIt.ProxyData.SceneData.FindByEqualsFunc(InViewDesc);
	}

	return nullptr;
}

 bool FTextureShareObject::GetReceivedProxyDataFrameMarker(FTextureShareCoreObjectFrameMarker& OutObjectFrameMarker) const
{
	for (const FTextureShareCoreObjectProxyData& CoreObjectProxyDataIt : ReceivedCoreObjectProxyData)
	{
		if (const FTextureShareCoreObjectFrameMarker* ExistObjectFrameMarker = CoreObjectProxyDataIt.ProxyData.RemoteFrameMarkers.FindByEqualsFunc(CoreObjectDesc))
		{
			OutObjectFrameMarker.ObjectDesc = CoreObjectProxyDataIt.Desc;
			OutObjectFrameMarker.FrameMarker = ExistObjectFrameMarker->FrameMarker;

			return true;
		}
	}

	return false;
}
