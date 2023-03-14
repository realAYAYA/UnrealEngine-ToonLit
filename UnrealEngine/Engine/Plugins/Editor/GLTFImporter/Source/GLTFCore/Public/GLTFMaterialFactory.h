// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GLTFLogger.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

namespace GLTF
{
	struct FAsset;
	struct FTexture;
	class FMaterialElement;
	class ITextureElement;
	class FMaterialFactoryImpl;

	enum class ETextureMode
	{
		Color,
		Grayscale,
		Normal
	};

	class GLTFCORE_API ITextureFactory
	{
	public:
		virtual ~ITextureFactory() = default;

		virtual ITextureElement* CreateTexture(const GLTF::FTexture& Texture, UObject* ParentPackage, EObjectFlags Flags,
		                                       GLTF::ETextureMode TextureMode) = 0;
		virtual void             CleanUp()                                     = 0;
	};

	class GLTFCORE_API IMaterialElementFactory
	{
	public:
		virtual ~IMaterialElementFactory() = default;

		virtual FMaterialElement* CreateMaterial(const TCHAR* Name, UObject* ParentPackage, EObjectFlags Flags) = 0;
	};

	class GLTFCORE_API FMaterialFactory
	{
	public:
		FMaterialFactory(IMaterialElementFactory* MaterialElementFactory, ITextureFactory* TextureFactory);
		~FMaterialFactory();

		const TArray<FMaterialElement*>& CreateMaterials(const GLTF::FAsset& Asset, UObject* ParentPackage, EObjectFlags Flags);

		const TArray<FLogMessage>&       GetLogMessages() const;
		const TArray<FMaterialElement*>& GetMaterials() const;

		IMaterialElementFactory& GetMaterialElementFactory();
		ITextureFactory&         GetTextureFactory();

		void CleanUp();

	private:
		TUniquePtr<FMaterialFactoryImpl> Impl;
	};

}  // namespace GLTF
