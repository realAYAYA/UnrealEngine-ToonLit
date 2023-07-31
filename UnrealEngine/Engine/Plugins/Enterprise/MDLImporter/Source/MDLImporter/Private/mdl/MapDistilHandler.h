// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

namespace mi
{
	namespace neuraylib
	{
		class ICompiled_material;
		class IMaterial_definition;
		class ITransaction;
	}
}
namespace Mdl
{
	struct FBakeParam;

	class IMapDistilHandler
	{
	public:
		virtual ~IMapDistilHandler() = default;

		virtual void PreImport(const mi::neuraylib::IMaterial_definition& MDLMaterialDefinition,
		                       const mi::neuraylib::ICompiled_material&   MDLMaterial,
		                       mi::neuraylib::ITransaction&               MDLTransaction) = 0;
		/**
		 * Import the map from the given bake path.
		 * @return True if the map was handled, otherwise will bake the map.
		 */
		virtual bool Import(const FString& MapName, bool bIsTexture, Mdl::FBakeParam& MapBakeParam) = 0;

		virtual void PostImport() = 0;
	};
}
