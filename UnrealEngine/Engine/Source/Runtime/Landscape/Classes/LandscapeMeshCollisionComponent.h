// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Templates/RefCounting.h"
#include "EngineDefines.h"
#include "Serialization/BulkData.h"
#include "LandscapeHeightfieldCollisionComponent.h"

#include "Chaos/PhysicalMaterials.h"

#include "LandscapeMeshCollisionComponent.generated.h"

class UPhysicalMaterial;
struct FNavigableGeometryExport;

namespace Chaos
{
	class FTriangleMeshImplicitObject;
}

UCLASS()
class ULandscapeMeshCollisionComponent : public ULandscapeHeightfieldCollisionComponent
{
	GENERATED_BODY()

public:

	ULandscapeMeshCollisionComponent();
	virtual ~ULandscapeMeshCollisionComponent();

	// Keep the possibility to share projected height field PhysX object with editor mesh collision objects...

	/** Guid used to share PhysX heightfield objects in the editor */
	UPROPERTY()
	FGuid MeshGuid;

	struct FTriMeshGeometryRef : public FRefCountedObject
	{
		FGuid Guid;

		TArray<Chaos::FMaterialHandle> UsedChaosMaterials;
		Chaos::FTriangleMeshImplicitObjectPtr TrimeshGeometry;

		// When deleted, remove PRAGMA_DISABLE_DEPRECATION_WARNINGS / PRAGMA_ENABLE_DEPRECATION_WARNINGS from ~FTriMeshGeometryRef()
		UE_DEPRECATED(5.4, "Please use TrimeshGeometry instead")
		TUniquePtr<Chaos::FTriangleMeshImplicitObject> Trimesh;
		
#if WITH_EDITORONLY_DATA
		Chaos::FTriangleMeshImplicitObjectPtr EditorTrimeshGeometry;
		
		// When deleted, remove PRAGMA_DISABLE_DEPRECATION_WARNINGS / PRAGMA_ENABLE_DEPRECATION_WARNINGS from ~FTriMeshGeometryRef()
		UE_DEPRECATED(5.4, "Please use EditorTrimeshGeometry instead")
		TUniquePtr<Chaos::FTriangleMeshImplicitObject> EditorTrimesh;
#endif // WITH_EDITORONLY_DATA

		FTriMeshGeometryRef();
		FTriMeshGeometryRef(FGuid& InGuid);
		virtual ~FTriMeshGeometryRef();

		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize);
	};

#if WITH_EDITORONLY_DATA
	/** The collision mesh values. */
	FWordBulkData CollisionXYOffsetData; //  X, Y Offset in raw format...
#endif //WITH_EDITORONLY_DATA

	/** Physics engine version of heightfield data. */
	TRefCountPtr<FTriMeshGeometryRef> MeshRef;

	//~ Begin UActorComponent Interface.
protected:
	virtual void OnCreatePhysicsState() override;
public:
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	//End UPrimitiveComponent interface

	//~ Begin INavRelevantInterface Interface
	virtual bool SupportsGatheringGeometrySlices() const override { return false; }
	//~ End INavRelevantInterface Interface

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
#if WITH_EDITOR
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

	virtual bool CookCollisionData(const FName& Format, bool bUseDefaultMaterialOnly, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const override;
	virtual uint32 ComputeCollisionHash() const override { return 0; }
#endif
	//~ End UObject Interface.

	//~ Begin ULandscapeHeightfieldCollisionComponent Interface
	virtual void CreateCollisionObject() override;
	virtual bool RecreateCollision() override;
	//~ End ULandscapeHeightfieldCollisionComponent Interface
};
