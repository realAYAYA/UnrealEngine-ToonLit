// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/RenderGridPropsSourceWidgetFactoryRemoteControl.h"
#include "UI/SRenderGridPropsRemoteControl.h"
#include "RenderGrid/RenderGridPropsSource.h"


TSharedPtr<UE::RenderGrid::Private::SRenderGridPropsBase> UE::RenderGrid::Private::FRenderGridPropsSourceWidgetFactoryRemoteControl::CreateInstance(URenderGridPropsSourceBase* PropsSource, TSharedPtr<IRenderGridEditor> BlueprintEditor)
{
	if (URenderGridPropsSourceRemoteControl* InPropsSource = Cast<URenderGridPropsSourceRemoteControl>(PropsSource))
	{
		return SNew(SRenderGridPropsRemoteControl, BlueprintEditor, InPropsSource);
	}
	return nullptr;
}
