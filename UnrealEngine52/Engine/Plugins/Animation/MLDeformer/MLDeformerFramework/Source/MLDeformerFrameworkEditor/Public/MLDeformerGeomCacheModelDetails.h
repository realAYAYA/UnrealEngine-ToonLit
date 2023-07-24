// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerModelDetails.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UMLDeformerGeomCacheModel;
class UGeometryCache;
class USkeletalMesh;

namespace UE::MLDeformer
{
	class FMLDeformerGeomCacheEditorModel;
	class FMLDeformerEditorModel;

	/**
	 * The detail customization of a geometry cache based model.
	 * The model it customizes needs to be derived from UMLDeformerGeomCacheModel.
	 * It will automatically insert some error when the geom cache doesn't match the animation sequence etc.
	 * If your model inherits from a UMLDeformerGeomCacheModel then you can also inherit your model detail customization from this FMLDeformerGeomCacheModelDetails class.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerGeomCacheModelDetails
		: public FMLDeformerModelDetails
	{
	public:
		// FMLDeformerModelDetails overrides.
		virtual bool UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects) override;
		virtual void AddTargetMesh() override;
		virtual void AddAnimSequenceErrors() override;
		// ~END FMLDeformerModelDetails overrides.

	protected:
		/** Add warnings to the UI about mesh mapping errors when mapping between geometry caches and a skeletal mesh. */
		void AddGeomCacheMeshMappingWarnings(IDetailCategoryBuilder* InTargetMeshCategoryBuilder, USkeletalMesh* SkeletalMesh, UGeometryCache* GeometryCache);

	protected:
		/** The geometry cache based runtime model. This value gets updated once UpdateMemberPointers gets called. */
		TObjectPtr<UMLDeformerGeomCacheModel> GeomCacheModel = nullptr;

		/** The geometry cache based editor model. This value gets updated once UpdateMemberPointers gets called. */
		FMLDeformerGeomCacheEditorModel* GeomCacheEditorModel = nullptr;
	};
}	// namespace UE::MLDeformer
