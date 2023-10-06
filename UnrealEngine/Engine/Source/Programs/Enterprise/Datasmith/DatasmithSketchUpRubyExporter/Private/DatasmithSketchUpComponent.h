// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

// Datasmith SDK.
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

namespace DatasmithSketchUp
{
	class FExportContext;

	class FNodeOccurence;
	class FDatasmithInstantiatedMesh;

	class FCamera;
	class FComponentDefinition;
	class FComponentInstance;
	class FDefinition;
	class FImage;
	class FImageMaterial;
	class FEntities;
	class FEntitiesGeometry;
	class FEntity;
	class FMaterial;
	class FMaterialOccurrence;
	class FTexture;
	class FMetadata;

	// Identifies each occurrence of an Entity(ComponentInstance or Group) within the model graph
	// As each ComponentInstance or Group can appear multiple times in the SketchUp model hierarchy
	// this object represents each specific occurrence of it
	class FNodeOccurence : FNoncopyable
	{
	public:
		FNodeOccurence(FEntity& InEntity)
			: ParentNode(nullptr)
			, Entity(InEntity)
			, Depth(0)
			,  bVisibilityInvalidated(true)
			,  bPropertiesInvalidated(true)
			,  bMeshActorsInvalidated(true)
			,  bHierarchyInvalidated(true)
		{}

		FNodeOccurence(FNodeOccurence* InParentNode, FEntity& InEntity)
			: ParentNode(InParentNode)
			, Entity(InEntity)
			, Depth(InParentNode->Depth + 1)
			, bVisibilityInvalidated(true)
			, bPropertiesInvalidated(true)
			, bMeshActorsInvalidated(true)
			, bHierarchyInvalidated(true)
		{}

		// Update node hierarchy, bForceUpdate - to make update node and descendants when transform changes
		void UpdateVisibility(FExportContext& Context);
		void Update(FExportContext& Context);

		void ResetNodeActors(FExportContext& Context); // Reset actors before update
		void RemoveDatasmithActorHierarchy(FExportContext& Context);  // Clean whole datasmith hierarchy from Datasmith Scene (e.g. when made invisible)

		void InvalidateProperties(); // Invalidate name and transform. Invalidate propagates down the hierarchy - child transforms depend on the parent
		void InvalidateMeshActors();
		void SetVisibility(bool);


		FString GetActorName();

		FString GetActorLabel();

		void RemoveOccurrence(FExportContext& Context);

		void ResetMetadataElement(FExportContext& Context); // Reset properties of actor's metadata to fill it anew

		FNodeOccurence* ParentNode;

		FEntity& Entity; // SketchUp entity this Node is an occurrence of

		TSet<FNodeOccurence*> Children;

		// Data that is computed from the hierarchy where Entity occurrence resides
		int32 Depth;
		SUTransformation WorldTransform;
		FMaterialIDType InheritedMaterialID;
		SULayerRef EffectiveLayerRef = SU_INVALID;
		bool bVisible = true; // Computed visibility for this occurrence(affecting descendants)

		// Datasmith elements this Node spawns
		FString DatasmithActorName;
		FString DatasmithActorLabel;
		TSharedPtr<IDatasmithActorElement> DatasmithActorElement;
		TSharedPtr<IDatasmithMetaDataElement> DatasmithMetadataElement;
		TArray<TSharedPtr<IDatasmithMeshActorElement>> MeshActors; // Mesh actors for Loose geometry

		FMaterial* MaterialOverride = nullptr; // Material used by this node

		// Flags indicating which Datasmith elements need to be updated from SketchUp
		// todo: do we really need so many flags for node? Just one flag to rebuild whole hierarchy of Datasmith actors?
		// Note - this doesn't mean that all needs to be recreated, literally, reuse if possible. 
		uint8 bVisibilityInvalidated:1;
		uint8 bPropertiesInvalidated:1; // Whether this occurrence properties(transform, name) need to be updated
		uint8 bMeshActorsInvalidated:1; // Whether this occurrence MeshActors need updating. Happens initially when node was added and when node geometry is invalidated
		uint8 bHierarchyInvalidated:1; // Children need to be rebuilt
	};

	// For SketchUp's Definition that provides access to Entities and converts to Datasmith
	class FDefinition : FNoncopyable
	{
	public:

		FDefinition();

		virtual ~FDefinition();

		virtual void Parse(FExportContext& Context) = 0;
		virtual void UpdateGeometry(FExportContext& Context) = 0; // Convert definition's Entities geometry to Datasmith Mesh
		virtual void UpdateMetadata(FExportContext& Context) = 0; 

		void EntityVisible(FEntity* Entity, bool bVisible);

		// Modification methods
		virtual void AddInstance(FExportContext& Context, TSharedPtr<FComponentInstance> Instance) = 0; // Register child CompoenntInstance Entity of Definition's Entities
		virtual void AddImage(FExportContext& Context, TSharedPtr<FImage> Image) = 0;

		virtual void InvalidateInstancesGeometry(FExportContext& Context) = 0; // Mark that all instances(and their occurrences) needed to be updated
		virtual void InvalidateInstancesMetadata(FExportContext& Context) = 0; // Mark that all instances(and their occurrences) needed to be updated
		virtual void FillOccurrenceActorMetadata(FNodeOccurence& Node) = 0;
		
		virtual FString GetSketchupSourceGUID() = 0; // Guid of the definition, SketchUp hashes definition contents into guid
		virtual FString GetSketchupSourceName() = 0; // Name used for label
		virtual FString GetSketchupSourceId() = 0; // Unique name identifier

		virtual SUTransformation GetMeshBakedTransform() = 0; // Transform that may be used to bake geometry

		FEntities& GetEntities()
		{
			return *Entities;
		}

		void InvalidateDefinitionGeometry()
		{
			bGeometryInvalidated = true;
		}

		void UpdateDefinition(FExportContext& Context);
		void ParseNode(FExportContext& Context, FNodeOccurence& Node);  // Parse hierarchy of a child node('child' meaning 'from this definition's entities')
		void ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material);

	protected:

		TSharedPtr<DatasmithSketchUp::FEntities> Entities;

		TSet<FEntity*> VisibleEntities;
		uint8 bMeshesAdded:1;
		uint8 bGeometryInvalidated:1;
		uint8 bPropertiesInvalidated:1;
	};


	// Associated with SketchUp ComponentDefinition
	class FComponentDefinition : public FDefinition
	{
	public:
		FComponentDefinition(
			SUComponentDefinitionRef InComponentDefinitionRef // source SketchUp component definition
		);

		virtual ~FComponentDefinition() override;

		// Begin FDefinition
		virtual void Parse(FExportContext& Context) override;
		virtual void UpdateGeometry(FExportContext& Context) override;
		virtual void UpdateMetadata(FExportContext& Context) override;

		virtual void AddInstance(FExportContext& Context, TSharedPtr<FComponentInstance> Instance) override;
		virtual void AddImage(FExportContext& Context, TSharedPtr<FImage> Image) override;

		virtual void InvalidateInstancesGeometry(FExportContext& Context) override;
		virtual void InvalidateInstancesMetadata(FExportContext& Context) override;
		virtual void FillOccurrenceActorMetadata(FNodeOccurence& Node) override;

		virtual FString GetSketchupSourceGUID() override;
		virtual FString GetSketchupSourceName() override;
		virtual FString GetSketchupSourceId() override;

		virtual SUTransformation GetMeshBakedTransform() override;
		// End FDefinition

		// Register/unregister instanced of this definition
		void LinkComponentInstance(FComponentInstance* ComponentInstance);
		void UnlinkComponentInstance(FComponentInstance* ComponentInstance);
		void RemoveComponentDefinition(FExportContext& Context);

		bool ShouldBakeTransformIntoMesh();
		void GetLocalTransform(FComponentInstance& ComponentInstance, SUTransformation& SComponentInstanceLocalTransform);
		void ComponentInstancePropertiesInvalidated();

		// Source SketchUp component ID.
		FComponentDefinitionIDType SketchupSourceID;
		TSet<FComponentInstance*> Instances; // Tracked instances of this ComponentDefinition

		// Whether or not the source SketchUp component behaves like a billboard, always presenting a 2D surface perpendicular to the direction of camera.
		bool bSketchupSourceFaceCamera = false;

	private:
		SUComponentDefinitionRef ComponentDefinitionRef;

		TUniquePtr<FMetadata> ParsedMetadata; // Shared metadata parsed from source SU component to be added to each occurrence actor's datasmith metatada element

		bool bBakeTransformIntoMesh = false;
	};


	class FModelDefinition : public FDefinition
	{
	public:
		FModelDefinition(SUModelRef Model);

		// Beging FDefinition
		virtual void Parse(FExportContext& Context) override;
		
		virtual void UpdateGeometry(FExportContext& Context) override;
		virtual void UpdateMetadata(FExportContext& Context) override;

		virtual void AddInstance(FExportContext& Context, TSharedPtr<FComponentInstance> Instance) override;
		virtual void AddImage(FExportContext& Context, TSharedPtr<FImage> Image) override;

		virtual void InvalidateInstancesGeometry(FExportContext& Context) override;
		virtual void InvalidateInstancesMetadata(FExportContext& Context) override;
		virtual void FillOccurrenceActorMetadata(FNodeOccurence& Node) override;

		virtual FString GetSketchupSourceGUID() override;
		virtual FString GetSketchupSourceName()  override;
		virtual FString GetSketchupSourceId() override;

		virtual SUTransformation GetMeshBakedTransform() override;
		// End FDefinition

		bool UpdateModel(FExportContext& Context);

	private:
		SUModelRef Model;
	};

	// In SketchUp Entities that reside in a ComponentDefinition/Model can be ComponentInstances, Groups, Faces (and others)
	// ComponentInstances and Groups create model hierarchy, Faces constitute the geometry("meat"!) of Entities
	class FEntities : FNoncopyable
	{
	public:

		FDefinition& Definition;

		FEntities(FDefinition& InDefinition) : Definition(InDefinition) {}

		void UpdateGeometry(FExportContext& Context);
		void AddMeshesToDatasmithScene(FExportContext& Context);
		void RemoveMeshesFromDatasmithScene(FExportContext& Context);

		TArray<SUGroupRef> GetGroups();
		TArray<SUComponentInstanceRef> GetComponentInstances();
		TArray<SUImageRef> GetImages();

		// Source SketchUp component entities.
		SUEntitiesRef EntitiesRef = SU_INVALID;

		TSharedPtr<FEntitiesGeometry> EntitiesGeometry;
	};

	// Represents an SketchUp's Entities'(not Entity's!) loose geometry
	class FEntitiesGeometry : FNoncopyable
	{
	public:

		int32 GetMeshCount()
		{
			return Meshes.Num();
		}

		const TCHAR* GetMeshElementName(int32 MeshIndex);
		bool IsMeshUsingInheritedMaterial(int32 MeshIndex);
		int32 GetInheritedMaterialOverrideSlotId();

		TArray<TSharedPtr<FDatasmithInstantiatedMesh>> Meshes;
		TSet<int32> FaceIds; // EntityId of all the VISIBLE faces composing the mesh
		TSet<DatasmithSketchUp::FEntityIDType> Layers; // EntityId of all layers assigned to geometry faces(needed to identify if geometry needs to be rebuilt when layer visibility changes)

		TSet<FMaterial*> MaterialsUsed;
		bool bDefaultMaterialUsed = false;

		bool IsDefaultLayerMaterialUsed()
		{
			return bDefaultMaterialUsed;
		}

		void SetDefaultLayerMaterialUsed()
		{
			bDefaultMaterialUsed = true;
		}

		// todo: update reusing datasmith elements? 
		// todo: update occurrences that used this entities - MeshActors need to be refreshed in accordance to OR this could be done on a level higher?
	};

	// Interface to implement SketchUp Entity Node - i.e. an instance of a ComponentDefinition(ComponentInstance or Group), Model, Image
	class FEntity : FNoncopyable
	{
	public:

		FEntity(SUEntityRef InEntityRef)
			: EntityRef(InEntityRef)
			, bGeometryInvalidated(true)
			, bPropertiesInvalidated(true)
		{}

		virtual ~FEntity() {}

		virtual int64 GetPersistentId() = 0;
		virtual FString GetName() = 0;

		virtual void ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material) = 0;

		virtual void UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node) = 0; // Update occurrence of this entity
		virtual void UpdateOccurrenceVisibility(FExportContext& Context, FNodeOccurence&) = 0;  // Re-evaluate visibility of entity's occurrence
		virtual void UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node) = 0; // Rebuild datasmith actors of entity's occurrence
		virtual void ResetOccurrenceActors(FExportContext& Context, FNodeOccurence& Node) = 0; // Remove datasmith actors of entity's occurrence from datasmith scene

		virtual void InvalidateOccurrencesGeometry(FExportContext& Context) = 0;
		virtual void InvalidateOccurrencesProperties(FExportContext& Context) = 0;

		virtual void EntityOccurrenceVisible(FNodeOccurence* Node, bool bUses);

		virtual void UpdateEntityProperties(FExportContext& Context);
		virtual void UpdateMetadata(FExportContext& Context) = 0;

		FNodeOccurence& CreateNodeOccurrence(FExportContext& Context, FNodeOccurence& ParentNode);
		void DeleteOccurrence(FExportContext& Context, FNodeOccurence* Node);
		void RemoveOccurrences(FExportContext& Context);
		/////

		// Invalidates transform, name
		void InvalidateEntityProperties()
		{
			bPropertiesInvalidated = true;
		}

		void InvalidateEntityGeometry()
		{
			bGeometryInvalidated = true;
		}

		void UpdateEntityGeometry(FExportContext& Context);

		void SetParentDefinition(FExportContext& Context, FDefinition* InParent);
		bool IsParentDefinition(FDefinition* InParent)
		{
			return Parent == InParent;
		}

		FDefinition* Parent = nullptr;

		SUEntityRef EntityRef = SU_INVALID;

		TArray<FNodeOccurence*> Occurrences; // All occurrencies of this Entity in Model hierarchy
		TSet<FNodeOccurence*> VisibleNodes; // Occurrences currently fully visible

		uint8 bGeometryInvalidated:1;
		uint8 bPropertiesInvalidated:1;
	};

	// Entity that has SUEntities children (Component or model)
	class FEntityWithEntities: public FEntity
	{
		using Super = FEntity;
	public:

		FEntityWithEntities(SUEntityRef InEntityRef)
			: Super(InEntityRef)
		{}

		virtual void UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node); // Update occurrence of this entity
		
		virtual void UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node);

		virtual void EntityOccurrenceVisible(FNodeOccurence* Node, bool bUses) override;

		virtual FDefinition* GetDefinition() = 0;
		virtual bool GetAssignedMaterial(FMaterialIDType& MaterialId) = 0; // Get material of this entity
	};

	class FComponentInstance : public FEntityWithEntities
	{
		using Super = FEntityWithEntities;
	public:
		FComponentDefinition& Definition;

		FComponentInstance(SUEntityRef InEntityRef, FComponentDefinition& InDefinition)
			: Super(InEntityRef)
			, Definition(InDefinition)
		{}

		virtual ~FComponentInstance() override;

		// >>> FEntity
		virtual FDefinition* GetDefinition() override;
		virtual bool GetAssignedMaterial(FMaterialIDType& MaterialId) override;
		virtual void InvalidateOccurrencesGeometry(FExportContext& Context) override;
		virtual void UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node) override;

		virtual void InvalidateOccurrencesProperties(FExportContext& Context) override;
		virtual int64 GetPersistentId() override;
		virtual FString GetName() override;
		virtual void UpdateOccurrenceVisibility(FExportContext& Context, FNodeOccurence&) override;
		virtual void UpdateMetadata(FExportContext& Context) override;
		virtual void UpdateEntityProperties(FExportContext& Context) override;
		virtual void UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node) override;
		virtual void ResetOccurrenceActors(FExportContext& Context, FNodeOccurence& Node) override;

		void ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material);
		// <<< FEntity

		void BuildNodeNames(FNodeOccurence& Node);
		void SetupActor(FExportContext& Context, FNodeOccurence& Node);

		void ParseNode(FExportContext& Context, FNodeOccurence&);
		void RemoveComponentInstance(FExportContext& Context);

		FComponentInstanceIDType GetComponentInstanceId();
		SUComponentInstanceRef GetComponentInstanceRef(); 

		void FillOccurrenceActorMetadata(FNodeOccurence& Node);

		bool bHidden = false;
		SULayerRef LayerRef = SU_INVALID;
		bool bLayerVisible = true;

		TUniquePtr<FMetadata> ParsedMetadata;
	};

	class FImage : public FEntity
	{
		using Super = FEntity;
	public:

		FImage(SUImageRef InEntityRef);

		virtual ~FImage() override
		{
		}

		virtual int64 GetPersistentId() override;
		virtual FString GetName() override;
		virtual void ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material) override;
		virtual void UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node) override;
		void RemoveImageFromDatasmithScene(FExportContext& Context);
		virtual void UpdateOccurrenceVisibility(FExportContext& Context, FNodeOccurence&) override;
		virtual void UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node) override;
		virtual void ResetOccurrenceActors(FExportContext& Context, FNodeOccurence& Node) override;
		virtual void InvalidateOccurrencesGeometry(FExportContext& Context) override;
		virtual void InvalidateOccurrencesProperties(FExportContext& Context) override;
		virtual void UpdateMetadata(FExportContext& Context) override;
		virtual void EntityOccurrenceVisible(FNodeOccurence* Node, bool bUses) override;

		void UpdateEntityProperties(FExportContext& Context);

		const TCHAR* GetMeshElementName();
		void Update(FExportContext& Context);
		void UpdateGeometry(FExportContext& Context);
		const TCHAR* GetDatasmithTextureElementName();

		FString GetFileName();
		void InvalidateImage();

		FString MeshElementName;

		void BuildNodeNames(FNodeOccurence& Node);
		void SetupActor(FExportContext& Context, FNodeOccurence& Node);

		void RemoveImage(FExportContext& Context);

		bool bHidden = false;
		SULayerRef LayerRef = SU_INVALID;
		bool bLayerVisible = true;

		TUniquePtr<FMetadata> ParsedMetadata;

		TSharedPtr<IDatasmithMeshElement> DatasmithMeshElement;

		FImageMaterial* ImageMaterial = nullptr;

	};

	class FModel : public FEntityWithEntities
	{
		using Super = FEntityWithEntities;
	public:
		FModel(class FModelDefinition& InDefinition);

		// >>> FEntity
		virtual FDefinition* GetDefinition() override;
		virtual bool GetAssignedMaterial(FMaterialIDType& MaterialId) override;
		virtual void InvalidateOccurrencesGeometry(FExportContext& Context) override;
		// void UpdateOccurrence(FExportContext& Context, FNodeOccurence& Node) override;
		virtual void InvalidateOccurrencesProperties(FExportContext& Context) override;
		virtual int64 GetPersistentId() override;
		virtual FString GetName() override;
		virtual void UpdateOccurrenceVisibility(FExportContext& Context, FNodeOccurence&) override;

		virtual void UpdateMetadata(FExportContext& Context) override;
		virtual void ApplyOverrideMaterialToNode(FNodeOccurence& Node, FMaterialOccurrence& Material) override;
		virtual void UpdateOccurrenceMeshActors(FExportContext& Context, FNodeOccurence& Node) override;
		virtual void ResetOccurrenceActors(FExportContext& Context, FNodeOccurence& Node) override;
		// <<< FEntity

	private:
		FModelDefinition& Definition;
	};
}
