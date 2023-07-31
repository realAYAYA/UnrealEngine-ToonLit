// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UsdWrappers/ForwardDeclarations.h"

#include "Templates/UniquePtr.h"

namespace UE
{
	namespace Internal
	{
		class FUsdEditContextImpl;
	}

	/**
	 * Minimal pxr::UsdEditContext wrapper for Unreal that can be used from no-rtti modules.
	 */
	class UNREALUSDWRAPPER_API FUsdEditContext final
	{
		FUsdEditContext( const FUsdEditContext& Other ) = delete;
		FUsdEditContext& operator=( const FUsdEditContext& Other ) = delete;

	public:
		explicit FUsdEditContext( const FUsdStageWeak& Stage );
		FUsdEditContext( const FUsdStageWeak& Stage, const FSdfLayer& EditTarget );
		~FUsdEditContext();

	private:
		TUniquePtr< Internal::FUsdEditContextImpl > Impl;
	};
}
