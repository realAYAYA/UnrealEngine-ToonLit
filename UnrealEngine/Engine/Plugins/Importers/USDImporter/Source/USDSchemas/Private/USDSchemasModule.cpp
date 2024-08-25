// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDSchemasModule.h"

#include "USDSchemaTranslator.h"

#if WITH_EDITOR
#include "MDLImporterModule.h"
#endif	  // #if WITH_EDITOR

#include "Modules/ModuleManager.h"

#if USE_USD_SDK
#include "Custom/MaterialXUSDShadeMaterialTranslator.h"
#include "Custom/MDLUSDShadeMaterialTranslator.h"
#include "USDGeomCameraTranslator.h"
#include "USDGeometryCacheTranslator.h"
#include "USDGeomMeshTranslator.h"
#include "USDGeomPointInstancerTranslator.h"
#include "USDGeomPrimitiveTranslator.h"
#include "USDGeomXformableTranslator.h"
#include "USDGroomTranslator.h"
#include "USDLuxLightTranslator.h"
#include "USDMemory.h"
#include "USDShadeMaterialTranslator.h"
#include "USDSkelSkeletonTranslator.h"
#include "USDVolVolumeTranslator.h"
#endif	  // #if USE_USD_SDK

class FUsdSchemasModule : public IUsdSchemasModule
{
public:
	virtual void StartupModule() override
	{
#if USE_USD_SDK
		LLM_SCOPE_BYTAG(Usd);

		FUsdSchemaTranslatorRegistry& Registry = GetTranslatorRegistry();
		FUsdRenderContextRegistry& ShaderRegistry = GetRenderContextRegistry();

		// Register the default translators
		TranslatorHandles = {
			Registry.Register<FUsdGeomCameraTranslator>(TEXT("UsdGeomCamera")),
			Registry.Register<FUsdGeomMeshTranslator>(TEXT("UsdGeomMesh")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomCapsule")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomCone")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomCube")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomCylinder")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomPlane")),
			Registry.Register<FUsdGeomPrimitiveTranslator>(TEXT("UsdGeomSphere")),
			Registry.Register<FUsdGeomPointInstancerTranslator>(TEXT("UsdGeomPointInstancer")),
			Registry.Register<FUsdGeomXformableTranslator>(TEXT("UsdGeomXformable")),
			Registry.Register<FUsdShadeMaterialTranslator>(TEXT("UsdShadeMaterial")),
			Registry.Register<FUsdLuxLightTranslator>(TEXT("UsdLuxBoundableLightBase")),
			Registry.Register<FUsdLuxLightTranslator>(TEXT("UsdLuxNonboundableLightBase")),
			Registry.Register<FUsdVolVolumeTranslator>(TEXT("UsdVolVolume"))};

#if WITH_EDITOR
		ShaderRegistry.Register(FMaterialXUsdShadeMaterialTranslator::MaterialXRenderContext);
		TranslatorHandles.Add(Registry.Register<FMaterialXUsdShadeMaterialTranslator>(TEXT("UsdShadeMaterial")));

		// Creating skeletal meshes technically works in Standalone mode, but by checking for this we artificially block it
		// to not confuse users as to why it doesn't work at runtime. Not registering the actual translators lets the inner meshes get parsed as
		// static meshes, at least.
		if (GIsEditor)
		{
			TranslatorHandles.Append({
				Registry.Register<FUsdSkelSkeletonTranslator>(TEXT("UsdSkelSkeleton")),
				Registry.Register<FUsdGroomTranslator>(TEXT("UsdGeomXformable")),
				// The GeometryCacheTranslator also works on UsdGeomXformable through the GroomTranslator
				Registry.Register<FUsdGeometryCacheTranslator>(TEXT("UsdGeomMesh")),
			});

			if (IMDLImporterModule* MDLImporterModule = FModuleManager::Get().LoadModulePtr<IMDLImporterModule>(TEXT("MDLImporter")))
			{
				ShaderRegistry.Register(FMdlUsdShadeMaterialTranslator::MdlRenderContext);
				TranslatorHandles.Add(Registry.Register<FMdlUsdShadeMaterialTranslator>(TEXT("UsdShadeMaterial")));
			}
		}
#endif	  // WITH_EDITOR

#endif	  // #if USE_USD_SDK
	}

	virtual void ShutdownModule() override
	{
		FUsdSchemaTranslatorRegistry& Registry = GetTranslatorRegistry();
		FUsdRenderContextRegistry& ShaderRegistry = GetRenderContextRegistry();

		for (const FRegisteredSchemaTranslatorHandle& TranslatorHandle : TranslatorHandles)
		{
			Registry.Unregister(TranslatorHandle);
		}

#if USE_USD_SDK && WITH_EDITOR
		ShaderRegistry.Unregister(FMdlUsdShadeMaterialTranslator::MdlRenderContext);
		ShaderRegistry.Unregister(FMaterialXUsdShadeMaterialTranslator::MaterialXRenderContext);
#endif	  // WITH_EDITOR
	}

	virtual FUsdSchemaTranslatorRegistry& GetTranslatorRegistry() override
	{
		return UsdSchemaTranslatorRegistry;
	}

	virtual FUsdRenderContextRegistry& GetRenderContextRegistry() override
	{
		return UsdRenderContextRegistry;
	}

protected:
	FUsdSchemaTranslatorRegistry UsdSchemaTranslatorRegistry;
	FUsdRenderContextRegistry UsdRenderContextRegistry;

	TArray<FRegisteredSchemaTranslatorHandle> TranslatorHandles;
};

IMPLEMENT_MODULE_USD(FUsdSchemasModule, USDSchemas);
