// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "material/MaterialFactory.h"

#include "Containers/Array.h"
#include "Materials/Material.h"
#include "Templates/SharedPointer.h"

namespace Mdl
{
	struct FMaterial;
}

namespace Generator
{
	class FFunctionLoader;
}
class FMDLMaterialSelector
{
public:
	enum class EMaterialType
	{
		Opaque = 0,
		Masked,
		Translucent,
		Clearcoat,
		Emissive,
		Carpaint,
		Subsurface,
		Count
	};

	FMDLMaterialSelector();
	~FMDLMaterialSelector();

	EMaterialType GetMaterialType(const Mdl::FMaterial& Material) const;

	const Mat::IMaterialFactory& GetMaterialFactory(EMaterialType MaterialType) const;
	const Mat::IMaterialFactory& GetMaterialFactory(const Mdl::FMaterial& Material) const;

	static FString ToString(EMaterialType Type);

private:
	TArray<TSharedPtr<Mat::IMaterialFactory>, TFixedAllocator<(int)EMaterialType::Count> > MaterialFactories;

	TUniquePtr<Generator::FFunctionLoader> FunctionLoader;
};
