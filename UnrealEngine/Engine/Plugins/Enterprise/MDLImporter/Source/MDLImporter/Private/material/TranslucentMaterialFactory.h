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
	class FTranslucentMaterialFactory : public IMaterialFactory
	{
	public:
		FTranslucentMaterialFactory(Generator::FFunctionLoader& FunctionLoader);

		virtual void Create(const Mdl::FMaterial& MdlMaterial, const Mat::FParameterMap& Parameters, UMaterial& Material) const override;

	private:
		Generator::FFunctionLoader& FunctionLoader;
	};
}
