// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "material/MaterialFactory.h"

#include "Templates/Tuple.h"

namespace Generator
{
	class FFunctionLoader;
}
namespace Mat
{
	class FBakedMaterialFactory : public IMaterialFactory
	{
	public:
		FBakedMaterialFactory(Generator::FFunctionLoader& FunctionLoader);

		virtual void Create(const Mdl::FMaterial& MdlMaterial, const Mat::FParameterMap& Parameters, UMaterial& Material) const override;

		static UMaterialExpression* GetTilingParameter(Generator::FFunctionLoader& FunctionLoader, UMaterial& Material);

	private:
		Generator::FFunctionLoader& FunctionLoader;
	};
}
