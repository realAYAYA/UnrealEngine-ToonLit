// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


class URenderGridPropsSourceBase;


namespace UE::RenderGrid
{
	/**
	 * The base class for the factory classes that will create URenderGridPropsSourceBase instances.
	 */
	class RENDERGRID_API IRenderGridPropsSourceFactory
	{
	public:
		virtual ~IRenderGridPropsSourceFactory() = default;
		virtual URenderGridPropsSourceBase* CreateInstance(UObject* Outer, UObject* PropsSourceOrigin) { return nullptr; }
	};
}
