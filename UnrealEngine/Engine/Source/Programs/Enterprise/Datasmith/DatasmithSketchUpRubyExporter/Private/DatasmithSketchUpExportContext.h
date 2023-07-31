// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpCamera.h"

#include "Misc/SecureHash.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

#include "Async/Future.h"

class FDatasmithSceneExporter;

class IDatasmithActorElement;
class IDatasmithCameraActorElement;
class IDatasmithMaterialInstanceElement;
class IDatasmithMeshElement;
class IDatasmithMetaDataElement;
class IDatasmithScene;
class IDatasmithTextureElement;


inline uint32 GetTypeHash(const FMD5Hash& Hash)
{
	uint32* HashAsInt32 = (uint32*)Hash.GetBytes();
	return HashAsInt32[0] ^ HashAsInt32[1] ^ HashAsInt32[2] ^ HashAsInt32[3];
}

namespace DatasmithSketchUp
{
	class FExportContext;

	class FNodeOccurence;

	class FCamera;
	class FComponentDefinition;
	class FComponentInstance;
	class FDefinition;
	class FEntities;
	class FEntitiesGeometry;
	class FEntity;
	class FMaterial;
	class FMaterialOccurrence;
	class FModel;
	class FModelDefinition;
	class FTexture;
	class FTextureImageFile;
	class FImage;
	class FImageFile;
	class FImageMaterial;

	class FComponentInstanceCollection
	{
	public:
		FComponentInstanceCollection(FExportContext& InContext) : Context(InContext) {}

		TSharedPtr<FComponentInstance> AddComponentInstance(FDefinition& ParentDefinition, SUComponentInstanceRef InComponentInstanceRef); // Register ComponentInstance as a child of ParentDefinition
		bool RemoveComponentInstance(FComponentInstanceIDType ParentEntityId, FComponentInstanceIDType ComponentInstanceId); // Take note that ComponentInstance removed from ParentDefinition children
		void RemoveComponentInstance(TSharedPtr<FComponentInstance> ComponentInstance);

		bool InvalidateComponentInstanceProperties(FComponentInstanceIDType ComponentInstanceID);
		void InvalidateComponentInstanceGeometry(FComponentInstanceIDType ComponentInstanceID);
		void InvalidateComponentInstanceMetadata(FComponentInstanceIDType ComponentInstanceID);
		void UpdateProperties();
		void UpdateGeometry();

		void LayerModified(DatasmithSketchUp::FEntityIDType LayerId);

		TSharedPtr<FComponentInstance>* FindComponentInstance(FComponentInstanceIDType ComponentInstanceID)
		{
			return ComponentInstanceMap.Find(ComponentInstanceID);
		}

		TMap<FComponentInstanceIDType, TSharedPtr<FComponentInstance>> ComponentInstanceMap;
	private:
		FExportContext& Context;

	};

	class FComponentDefinitionCollection
	{
	public:
		FComponentDefinitionCollection(FExportContext& InContext) : Context(InContext) {}

		void PopulateFromModel(SUModelRef InSModelRef);

		TSharedPtr<FComponentDefinition> AddComponentDefinition(SUComponentDefinitionRef InComponentDefinitionRef);

		TSharedPtr<FComponentDefinition> GetComponentDefinition(SUComponentInstanceRef InSComponentInstanceRef);
		TSharedPtr<FComponentDefinition> GetComponentDefinition(SUComponentDefinitionRef ComponentDefinitionRef);
		TSharedPtr<FComponentDefinition>* FindComponentDefinition(FComponentDefinitionIDType ComponentDefinitionID);

		void Update();

		TMap<FEntityIDType, TSharedPtr<FComponentDefinition>> ComponentDefinitionMap;
	private:
		FExportContext& Context;
	};

	class FImageCollection
	{
	public:
		FImageCollection(FExportContext& InContext) : Context(InContext) {}

		TSharedPtr<FImage> AddImage(FDefinition& ParentDefinition, const SUImageRef& ImageRef);
		void Update();
		bool InvalidateImage(const FEntityIDType& EntityID);
		bool RemoveImage(FComponentInstanceIDType ParentEntityId, FEntityIDType ImageId);
		void UpdateProperties();
		void ReleaseMaterial(FImage& Image);
		const TCHAR* AcquireMaterial(FImage& Image);
		void LayerModified(FEntityIDType LayerId);

	private:
		FExportContext& Context;

		TMap<FEntityIDType, TSharedPtr<FImage>> Images;

		TMap<FMD5Hash, TSharedPtr<FImageMaterial>> ImageMaterials;  // One material for same images(even for different entities - their appearance onlly depends in the image content)
	};

	class FTextureCollection
	{
	public:
		FTextureCollection(FExportContext& InContext) : Context(InContext) {}

		TSharedPtr<FTexture> FindOrAdd(SUTextureRef);

		FTexture* AddTexture(SUTextureRef TextureRef, FString MaterialName, bool bColorized=false);

		// This texture is used in a colorized material so image will be saved in material-specific filename
		FTexture* AddColorizedTexture(SUTextureRef TextureRef, FString MaterialName); 

		void Update();

		// Track texture usage
		void RegisterMaterial(FMaterial*);
		void UnregisterMaterial(FMaterial*);

		// Create image for SketchUp texture or reuse image with same content hash
		// Multiple SU materials might have identical texture used 
		void AcquireImage(FTexture& Texture); 
		void ReleaseImage(FTexture& Texture);

	private:
		FExportContext& Context;

		TMap<FTextureIDType, TSharedPtr<FTexture>> TexturesMap;
		TMap<FString, TSharedPtr<FTextureImageFile>> TextureNameToImageFile; // texture handlers representing same texture

		TMap<FMD5Hash,  TSharedPtr<FTextureImageFile>> Images; // set of images using the same name in SketchUp materials
	};

	// Holds all image files that are exported
	class FImageFileCollection
	{
	public:
		FImageFileCollection(FExportContext& Context): Context(Context) {}

		TSharedPtr<FImageFile> AddImage(SUImageRepRef ImageRep, FString FileName);

		const TCHAR* GetImageFilePath(FImageFile&);
		FMD5Hash GetImageFileHash(FImageFile&);
		bool GetImageHasAlpha(FImageFile& ImageFile);

	private:
		FExportContext& Context;
		TMap<FMD5Hash, TSharedPtr<FImageFile>> ImageFiles;

	};

	class FEntitiesObjectCollection
	{
	public:
		FEntitiesObjectCollection(FExportContext& InContext) : Context(InContext) {}

		TSharedPtr<DatasmithSketchUp::FEntities> AddEntities(FDefinition& InDefinition, SUEntitiesRef EntitiesRef);

		void RegisterEntities(DatasmithSketchUp::FEntities&);
		void UnregisterEntities(DatasmithSketchUp::FEntities&);
		DatasmithSketchUp::FEntities* FindFace(int32 FaceId);

		void LayerModified(FEntityIDType LayerId);

	private:
		FExportContext& Context;
		TMap<int32, DatasmithSketchUp::FEntities*> FaceIdForEntitiesMap; // Identify Entities for each Face
		TMap<DatasmithSketchUp::FEntityIDType, TSet<DatasmithSketchUp::FEntities*>> LayerIdForEntitiesMap; // Identify Entities for each Face
	};

	// Tracks information related to SketchUp "Scenes"(or "Pages" in older UI)
	class FSceneCollection
	{
	public:
		FSceneCollection(FExportContext& InContext) : Context(InContext) {}

		// Initialize the dictionary of camera definitions.
		void PopulateFromModel(
			SUModelRef InSModelRef // model containing SketchUp camera definitions
		);
		bool SetActiveScene(const FEntityIDType& EntityID);

		bool Update();

		TMap<FSceneIDType, TSharedPtr<FCamera>> SceneIdToCameraMap;
		FSceneIDType ActiveSceneId = FSceneIDType();
	private:
		FExportContext& Context;

		FMD5Hash ScenesHash;
	};

	class FRegularMaterials
	{
	public:
		FRegularMaterials(FExportContext& InContext): Context(InContext)
		{
		}

		TSharedPtr<FMaterial> FindOrCreateMaterial(FMaterialIDType MaterialID);

		bool InvalidateMaterial(FMaterialIDType MateriadId);
		bool RemoveMaterial(FMaterialIDType EntityId);

		// Tell that this materials is assigned directly to a face on the geometry
		FMaterialOccurrence* RegisterGeometry(FMaterialIDType MaterialID, DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry);
		// Tell that this materials is assigned on the node
		FMaterialOccurrence* RegisterInstance(FMaterialIDType MaterialID, FNodeOccurence* NodeOccurrence);


		void UnregisterGeometry(DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry);

		bool InvalidateDefaultMaterial();

		void RemoveUnused();
		void UpdateDefaultMaterial();
		void Apply(FMaterial* Material);

	private:
		FExportContext& Context;

		TMap<FMaterialIDType, TSharedPtr<DatasmithSketchUp::FMaterial>> MaterialForMaterialId;
		TMap<DatasmithSketchUp::FMaterial*, FMaterialIDType> MaterialIdForMaterial;
		// TMap<FMaterialIDType, TSharedPtr<DatasmithSketchUp::FMaterial>> MaterialDefinitionMap;

		// todo: include default material into whole update cycle(textures, unused)
		FMaterialOccurrence DefaultMaterial;
	};


	class FLayerMaterials
	{
	public:
		FLayerMaterials(FExportContext& InContext): Context(InContext)
		{
		}

		TSharedPtr<FMaterial> FindOrCreateMaterialForLayer(FLayerIDType LayerID);

		FMaterialOccurrence* RegisterGeometryForLayer(FLayerIDType LayerID, FEntitiesGeometry* EntitiesGeometry);
		void RemoveUnused();
		void Apply(FMaterial* Material);
		void UpdateLayer(SULayerRef LayerRef);
		FMaterialOccurrence* RegisterInstance(FLayerIDType LayerID, FNodeOccurence* Node);
		bool CheckForModifications();

	private:
		FExportContext& Context;

		TMap<FLayerIDType, TSharedPtr<DatasmithSketchUp::FMaterial>> MaterialForLayerId;
		TMap<FLayerIDType, FMD5Hash> MaterialHashForLayerId;
		TMap<DatasmithSketchUp::FMaterial*, FLayerIDType> LayerIdForMaterial;
	};

	class FMaterialCollection
	{
	public:
		FMaterialCollection(FExportContext & InContext) : Context(InContext), LayerMaterials(InContext), RegularMaterials(InContext)
		{
		}

		void Update();

		TSharedPtr<FMaterial> CreateMaterial(SUMaterialRef SMaterialDefinitionRef);
		void InvalidateMaterial(SUMaterialRef SMaterialDefinitionRef);
		bool RemoveMaterial(FMaterial* Material);

		void UnregisterGeometry(DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry);

		void SetMeshActorOverrideMaterial(FNodeOccurence& Node, DatasmithSketchUp::FEntitiesGeometry& EntitiesGeometry, const TSharedPtr<IDatasmithMeshActorElement>& MeshActor);

	private:
		FExportContext& Context;
	public:
		FLayerMaterials LayerMaterials;
		FRegularMaterials RegularMaterials;
	private:
		TSet<DatasmithSketchUp::FMaterial*> Materials;

	};

	// Tracks information related to SketchUp "Tags"/"Layers"
	class FLayerCollection
	{
	public:
		FLayerCollection(FExportContext& InContext) : Context(InContext) {}

		void PopulateFromModel(SUModelRef InSModelRef);
		void UpdateLayer(SULayerRef LayerRef);
		bool IsLayerVisible(SULayerRef LayerRef);
		SULayerRef GetLayer(FLayerIDType LayerID);

		FLayerIDType GetLayerId(SULayerRef LayerRef);

		TMap<FLayerIDType, bool> LayerVisibility;
	private:
		FExportContext& Context;
	};

	struct FOptions
	{
		// Enabling this reverts to legacy behavior when each set of disconnected faces results in a separate static mesh.
		// Disconnected means faces not connected through common edges. E.g. face A and B share an edge, face C shares edge with A then ABC are 'connected' one mesh(A connects with B through C).
		bool bSeparateDisconnectedMeshes = false;
	};


	// Holds all the data needed during export and incremental updates
	class FExportContext
	{
	public:

		FExportContext();

		const TCHAR* GetAssetsOutputPath() const;

		void Populate(); // Create Datasmith scene from the Model
		bool Update(bool bModifiedHint); // Update Datasmith scene to reflect iterative changes done to the Model 

		FDefinition* GetDefinition(SUEntityRef Entity);
		FDefinition* GetDefinition(FEntityIDType DefinitionEntityId);

		bool InvalidateColorByLayer();

		bool PreUpdateColorByLayer();

		FOptions Options;

		SUModelRef ModelRef = SU_INVALID;

		TSharedPtr<IDatasmithScene> DatasmithScene;
		TSharedPtr<FDatasmithSceneExporter> SceneExporter;

		TSharedPtr<FNodeOccurence> RootNode;
		TSharedPtr<FModelDefinition> ModelDefinition;
		TSharedPtr<FModel> Model;

		FComponentDefinitionCollection ComponentDefinitions;
		FComponentInstanceCollection ComponentInstances;
		FEntitiesObjectCollection EntitiesObjects;
		FMaterialCollection Materials;
		FSceneCollection Scenes;
		FTextureCollection Textures;
		FLayerCollection Layers;
		FImageCollection Images;
		FImageFileCollection ImageFiles;

		bool bColorByLayer = false;
		bool bColorByLayerInvaliated = true;

		TArray<TFuture<bool>> MeshExportTasks;
	};
}
