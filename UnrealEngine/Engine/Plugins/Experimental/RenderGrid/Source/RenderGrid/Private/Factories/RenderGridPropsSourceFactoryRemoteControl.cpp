// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/RenderGridPropsSourceFactoryRemoteControl.h"
#include "RenderGrid/RenderGridPropsSource.h"


URenderGridPropsSourceBase* UE::RenderGrid::Private::FRenderGridPropsSourceFactoryRemoteControl::CreateInstance(UObject* Outer, UObject* PropsSourceOrigin)
{
	URenderGridPropsSourceRemoteControl* PropsSource = NewObject<URenderGridPropsSourceRemoteControl>(Outer);
	PropsSource->SetSourceOrigin(PropsSourceOrigin);
	return PropsSource;
}
