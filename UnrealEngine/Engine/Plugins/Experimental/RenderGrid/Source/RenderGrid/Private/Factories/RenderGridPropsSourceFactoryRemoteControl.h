// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/IRenderGridPropsSourceFactory.h"


namespace UE::RenderGrid::Private
{
	/**
	 * The factory class for URenderGridPropsSourceRemoteControl.
	 */
	class RENDERGRID_API FRenderGridPropsSourceFactoryRemoteControl final : public IRenderGridPropsSourceFactory
	{
	public:
		virtual URenderGridPropsSourceBase* CreateInstance(UObject* Outer, UObject* PropsSourceOrigin) override;
	};
}
