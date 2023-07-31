// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UniquePtr.h"

namespace UE
{
	namespace Internal
	{
		class FSdfChangeBlockImpl;
	}

	/**
	 * Minimal pxr::SdfChangeBlock wrapper for Unreal that can be used from no-rtti modules.
	 */
	class UNREALUSDWRAPPER_API FSdfChangeBlock final
	{
	public:
		FSdfChangeBlock();
		~FSdfChangeBlock();

	private:
		TUniquePtr< Internal::FSdfChangeBlockImpl > Impl;
	};
}
