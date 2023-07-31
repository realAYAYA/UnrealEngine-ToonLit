// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDSchemasModule.h"

#include "USDSchemaTranslator.h"

#if WITH_EDITOR
#include "MDLImporterModule.h"
#endif // #if WITH_EDITOR

#include "Modules/ModuleManager.h"

#if USE_USD_SDK
#include "USDGeomCameraTranslator.h"
#include "USDGeomMeshTranslator.h"
#include "USDGeomPointInstancerTranslator.h"
#include "USDGeomXformableTranslator.h"
#include "USDGroomTranslator.h"
#include "USDLuxLightTranslator.h"
#include "USDMemory.h"
#include "USDShadeMaterialTranslator.h"
#include "USDSkelRootTranslator.h"
#include "Custom/MDLUSDShadeMaterialTranslator.h"
#endif // #if USE_USD_SDK

class FUsdSchemasModule : public IUsdSchemasModule
{
public:
	virtual void StartupModule() override
	{
#if USE_USD_SDK
		LLM_SCOPE_BYTAG(Usd);

		// Register the default translators
		UsdGeomCameraTranslatorHandle = GetTranslatorRegistry().Register< FUsdGeomCameraTranslator >( TEXT("UsdGeomCamera") );
		UsdGeomMeshTranslatorHandle = GetTranslatorRegistry().Register< FUsdGeomMeshTranslator >( TEXT("UsdGeomMesh") );
		UsdGeomPointInstancerTranslatorHandle = GetTranslatorRegistry().Register< FUsdGeomPointInstancerTranslator >( TEXT("UsdGeomPointInstancer") );
		UsdGeomXformableTranslatorHandle = GetTranslatorRegistry().Register< FUsdGeomXformableTranslator >( TEXT("UsdGeomXformable") );
		UsdShadeMaterialTranslatorHandle = GetTranslatorRegistry().Register< FUsdShadeMaterialTranslator >( TEXT("UsdShadeMaterial") );
		UsdLuxBoundableLightBaseTranslatorHandle = GetTranslatorRegistry().Register< FUsdLuxLightTranslator >( TEXT("UsdLuxBoundableLightBase") );
		UsdLuxNonboundableLightBaseTranslatorHandle = GetTranslatorRegistry().Register< FUsdLuxLightTranslator >( TEXT("UsdLuxNonboundableLightBase") );

#if WITH_EDITOR
		// Creating skeletal meshes technically works in Standalone mode, but by checking for this we artificially block it
		// to not confuse users as to why it doesn't work at runtime. Not registering the actual translators lets the inner meshes get parsed as static meshes, at least.
		if ( GIsEditor )
		{
			UsdSkelRootTranslatorHandle = GetTranslatorRegistry().Register< FUsdSkelRootTranslator >( TEXT("UsdSkelRoot") );
			UsdGroomTranslatorHandle = GetTranslatorRegistry().Register< FUsdGroomTranslator >( TEXT("UsdGeomXformable") );

			if ( IMDLImporterModule* MDLImporterModule = FModuleManager::Get().LoadModulePtr< IMDLImporterModule >( TEXT("MDLImporter") ) )
			{
				MdlUsdShadeMaterialTranslatorHandle = GetTranslatorRegistry().Register< FMdlUsdShadeMaterialTranslator >( TEXT("UsdShadeMaterial") );
				GetRenderContextRegistry().Register( FMdlUsdShadeMaterialTranslator::MdlRenderContext );
			}
		}
#endif // WITH_EDITOR

#endif // #if USE_USD_SDK
	}

	virtual void ShutdownModule() override
	{
#if USE_USD_SDK
		GetTranslatorRegistry().Unregister( UsdGeomCameraTranslatorHandle );
		GetTranslatorRegistry().Unregister( UsdGeomMeshTranslatorHandle );
		GetTranslatorRegistry().Unregister( UsdGeomPointInstancerTranslatorHandle );
#if WITH_EDITOR
		if ( GIsEditor )
		{
			GetTranslatorRegistry().Unregister( UsdSkelRootTranslatorHandle );
			GetTranslatorRegistry().Unregister( UsdGroomTranslatorHandle );

			GetTranslatorRegistry().Unregister( MdlUsdShadeMaterialTranslatorHandle );
			GetRenderContextRegistry().Unregister( FMdlUsdShadeMaterialTranslator::MdlRenderContext );
		}
#endif // WITH_EDITOR
		GetTranslatorRegistry().Unregister( UsdGeomXformableTranslatorHandle );
		GetTranslatorRegistry().Unregister( UsdShadeMaterialTranslatorHandle );
		GetTranslatorRegistry().Unregister( UsdLuxBoundableLightBaseTranslatorHandle );
		GetTranslatorRegistry().Unregister( UsdLuxNonboundableLightBaseTranslatorHandle );
#endif // #if USE_USD_SDK
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

	FRegisteredSchemaTranslatorHandle UsdGeomCameraTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdGeomMeshTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdGeomPointInstancerTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdSkelRootTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdGeomXformableTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdShadeMaterialTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdGroomTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdLuxBoundableLightBaseTranslatorHandle;
	FRegisteredSchemaTranslatorHandle UsdLuxNonboundableLightBaseTranslatorHandle;

	// Custom schemas
	FRegisteredSchemaTranslatorHandle MdlUsdShadeMaterialTranslatorHandle;
};

IMPLEMENT_MODULE_USD( FUsdSchemasModule, USDSchemas );
