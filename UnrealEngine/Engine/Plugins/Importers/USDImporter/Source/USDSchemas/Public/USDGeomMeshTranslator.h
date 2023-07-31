// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDGeomXformableTranslator.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "MeshDescription.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

class UStaticMesh;
class FStaticMeshComponentRecreateRenderStateContext;

PXR_NAMESPACE_OPEN_SCOPE
	class UsdGeomMesh;
PXR_NAMESPACE_CLOSE_SCOPE

namespace UsdUtils
{
	struct FUsdPrimMaterialAssignmentInfo;
}

class FBuildStaticMeshTaskChain : public FUsdSchemaTranslatorTaskChain
{
public:
	explicit FBuildStaticMeshTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath );

protected:
	// Inputs
	// When multiple meshes are collapsed together, this Schema might not be the same as the Context schema, which is the root schema
	UE::FSdfPath PrimPath;
	TSharedRef< FUsdSchemaTranslationContext > Context;
	TArray<FMeshDescription> LODIndexToMeshDescription;
	TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfo;

	// Outputs
	UStaticMesh* StaticMesh = nullptr;

	// Required to prevent StaticMesh from being used for drawing while it is being rebuilt
	TSharedPtr<FStaticMeshComponentRecreateRenderStateContext> RecreateRenderStateContextPtr;

protected:
	UE::FUsdPrim GetPrim() const { return Context->Stage.GetPrimAtPath( PrimPath ); }

	virtual void SetupTasks();
};

class FGeomMeshCreateAssetsTaskChain : public FBuildStaticMeshTaskChain
{
public:
	explicit FGeomMeshCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& PrimPath, const FTransform& AdditionalTransform = FTransform::Identity );

protected:
	// Inputs
	FTransform AdditionalTransform;

protected:
	virtual void SetupTasks() override;
};

class USDSCHEMAS_API FUsdGeomMeshTranslator : public FUsdGeomXformableTranslator
{
public:
	using Super = FUsdGeomXformableTranslator;

	using FUsdGeomXformableTranslator::FUsdGeomXformableTranslator;

	FUsdGeomMeshTranslator( const FUsdGeomMeshTranslator& Other ) = delete;
	FUsdGeomMeshTranslator& operator=( const FUsdGeomMeshTranslator& Other ) = delete;

	virtual void CreateAssets() override;
	virtual USceneComponent* CreateComponents() override;
	virtual void UpdateComponents( USceneComponent* SceneComponent ) override;

	virtual bool CollapsesChildren( ECollapsingType CollapsingType ) const override;
	virtual bool CanBeCollapsed( ECollapsingType CollapsingType ) const override;

};

#endif // #if USE_USD_SDK
