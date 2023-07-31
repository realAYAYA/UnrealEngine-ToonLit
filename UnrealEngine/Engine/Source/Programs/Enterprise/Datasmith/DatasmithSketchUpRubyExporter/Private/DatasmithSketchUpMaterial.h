// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/material.h"
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Misc/SecureHash.h"
#include "Templates/SharedPointer.h"


class IDatasmithBaseMaterialElement;

namespace DatasmithSketchUp
{
	class FExportContext;

	class FNodeOccurence;
	class FEntitiesGeometry;
	class FMaterial;
	class FTexture;

	// Holds datasmith material and records connections to its 'users' - meshes and nodes
	class FMaterialOccurrence : FNoncopyable
	{
	public:

		TSharedPtr<IDatasmithBaseMaterialElement> DatasmithElement;

		const TCHAR* GetName();

		void RemoveDatasmithElement(FExportContext& Context);

		void Invalidate(FExportContext& Context)
		{
			RemoveDatasmithElement(Context);
		}

		bool IsInvalidated()
		{
			return !DatasmithElement;
		}

		void AddUser()
		{
			UserCount++;
		}

		// Returns true if material has no users
		bool RemoveUser(FExportContext& Context);

		bool HasUsers()
		{
			return UserCount != 0;
		}

		void RegisterGeometry(FEntitiesGeometry* Geom); // Tie material to a geometry it's used on
		void UnregisterGeometry(FExportContext& Context, FEntitiesGeometry* Geom);

		void RegisterInstance(FNodeOccurence* NodeOccurrence); // Tie material to an instance it's used on
		void UnregisterInstance(FExportContext& Context, FNodeOccurence* NodeOccurrence);

		void ApplyRegularMaterial(FMaterialIDType MaterialId); // Apply material to meshes/instances
		void ApplyLayerMaterial(FLayerIDType LayerId); // Apply material to meshes/instances

		TSet<FEntitiesGeometry*> MeshesMaterialDirectlyAppliedTo;
		TSet<FNodeOccurence*> NodesMaterialInheritedBy;

	private:
		int32 UserCount = 0;
	};

	// Associates SketchUp material with its Datasmith occurrences
	// A SketchUp material can have two instances in Datasmith scene - when it's directly applied to a face and when inherited from Component
	// Why two instances - directly applied materials have their texture scaling baked into Face UVs by SketchUp, inherited material needs to scale UVs 
	class FMaterial : FNoncopyable
	{

	public:
		static FMaterialIDType const DEFAULT_MATERIAL_ID;
		static FMaterialIDType const INHERITED_MATERIAL_ID;

		FMaterial(SUMaterialRef InMaterialRef);

		static TSharedPtr<FMaterial> Create(FExportContext& Context, SUMaterialRef InMaterialRef);

		static TSharedRef<IDatasmithBaseMaterialElement> CreateDefaultMaterialElement(FExportContext& Context);

		// Convert the SketchUp sRGB color to a Datasmith linear color.
		static FLinearColor ConvertColor(const SUColor& C, bool bAlphaUsed = false);

		// Indicate that this material is used as directly applied on a mesh
		FMaterialOccurrence& RegisterGeometry(FEntitiesGeometry*);
		void UnregisterGeometry(FExportContext&, FEntitiesGeometry*);

		// Indicate that this material is used as directly applied on an instance occurrence
		// Note - this is not per 'instance' as every instance can be in separate place in scene multiple times possibly resulting in different inherited materials
		FMaterialOccurrence& RegisterInstance(FNodeOccurence*);
		void UnregisterInstance(FExportContext&, FNodeOccurence* NodeOccurrence);


		void Invalidate();
		bool IsInvalidated()
		{
			return bInvalidated;
		}
		void UpdateTexturesUsage(FExportContext& Context);
		void Update(FExportContext& Context); // create datasmith elements for material occurrences
		void Remove(FExportContext& Context);

		bool IsUsed()
		{
			return MaterialDirectlyAppliedToMeshes.HasUsers() || MaterialInheritedByNodes.HasUsers();
		}

		FTexture* GetTexture()
		{
			return Texture;
		}

		FMD5Hash ComputeHash(FExportContext& Context);

		// Material can be directly applied to a face in SketchUp
		FMaterialOccurrence MaterialDirectlyAppliedToMeshes;
		// In case face has Default material assigned it inherits material set to it's parent(in general - first non-Default material in ancestors chain)
		FMaterialOccurrence MaterialInheritedByNodes;

		// Whether UVs of geometry have texture scaling baked, when applying this material
		// Scaling is baked for regular materials, applied directly to faces so regular materials assigned to static meshes need to take this into account.
		bool bGeometryHasScalingBakedIntoUvs = true;
	private:

		SUMaterialRef MaterialRef;

		int32 EntityId;

		FTexture* Texture = nullptr;

		uint8 bInvalidated : 1;

		friend class FMaterialOccurrence;
	};

}
