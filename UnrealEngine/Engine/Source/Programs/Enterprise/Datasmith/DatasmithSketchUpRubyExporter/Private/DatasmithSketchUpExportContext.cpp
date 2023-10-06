// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpExportContext.h"
#include "DatasmithSketchUpUtils.h"
#include "DatasmithSketchUpComponent.h"
#include "DatasmithSketchUpCamera.h"
#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpMesh.h"
#include "DatasmithSketchUpMetadata.h"
#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"

#include "DatasmithExportOptions.h"
#include "DatasmithMesh.h"
#include "DatasmithMeshExporter.h"
#include "DatasmithSceneExporter.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"

#include "Misc/SecureHash.h"
#include "Misc/Paths.h"


// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"

#include "SketchUpAPI/model/camera.h"
#include "SketchUpAPI/model/component_definition.h"
#include "SketchUpAPI/model/component_instance.h"
#include <SketchUpAPI/model/image.h>

#include "SketchUpAPI/model/layer.h"
#include "SketchUpAPI/model/model.h"
#include <SketchUpAPI/model/rendering_options.h>
#include "SketchUpAPI/model/scene.h"
#include <SketchUpAPI/model/typed_value.h>


#include "SketchUpAPI/application/application.h"

#include "DatasmithSketchUpSDKCeases.h"

using namespace DatasmithSketchUp;


FExportContext::FExportContext()
	: ComponentDefinitions(*this)
	, ComponentInstances(*this)
	, EntitiesObjects(*this)
	, Materials(*this)
	, Scenes(*this)
	, Textures(*this)
	, Layers(*this)
	, Images(*this)
	, ImageFiles(*this)
{
}

const TCHAR* FExportContext::GetAssetsOutputPath() const
{
	return SceneExporter->GetAssetsOutputPath();
}

void FExportContext::Populate()
{
	// Get Active Model
	SUResult su_api_result = SUApplicationGetActiveModel(&ModelRef);
	if (SUIsInvalid(ModelRef)) {
		return;
	}

	SUTransformation WorldTransform = { 1.0, 0.0, 0.0, 0.0,
										0.0, 1.0, 0.0, 0.0,
										0.0, 0.0, 1.0, 0.0,
										0.0, 0.0, 0.0, 1.0 };

	// Set up root 'Node'
	ModelDefinition = MakeShared<DatasmithSketchUp::FModelDefinition>(ModelRef);
	ModelDefinition->Parse(*this);

	// Retrieve the default layer in the SketchUp model.
	SULayerRef DefaultLayerRef = SU_INVALID;
	SUModelGetDefaultLayer(ModelRef, &DefaultLayerRef);

	// Setup root Node, based on Model
	Model = MakeShared<FModel>(*ModelDefinition);
	RootNode = MakeShared<FNodeOccurence>(*Model);
	RootNode->WorldTransform = WorldTransform;
	RootNode->EffectiveLayerRef = DefaultLayerRef;
	// Name and label for root loose mesh actors
	RootNode->DatasmithActorName = TEXT("SU");
	RootNode->DatasmithActorLabel = TEXT("Model");

	// Parse/convert Model
	Layers.PopulateFromModel(ModelRef);
	Scenes.Update();
	ComponentDefinitions.PopulateFromModel(ModelRef);

	ModelDefinition->ParseNode(*this, *RootNode); // Create node hierarchy
}

// ColorByLayer(or ColorByTag) changes material display on every entity by useinf either regular materials or colors(materials) set up for layers
bool FExportContext::PreUpdateColorByLayer()
{
	if (bColorByLayerInvaliated)
	{
		bColorByLayerInvaliated = false;

		bool bColorByLayerNew = bColorByLayer;
		SURenderingOptionsRef RenderingOptionsRef = SU_INVALID;
		if (SUModelGetRenderingOptions(ModelRef, &RenderingOptionsRef) == SU_ERROR_NONE)
		{
			SUTypedValueRef DisplayColorByLayerTypedValue = SU_INVALID;
			SUTypedValueCreate(&DisplayColorByLayerTypedValue);
			if (SURenderingOptionsGetValue(RenderingOptionsRef, "DisplayColorByLayer", &DisplayColorByLayerTypedValue) == SU_ERROR_NONE)
			{
				bool DisplayColorByLayer;
				if (SUTypedValueGetBool(DisplayColorByLayerTypedValue, &DisplayColorByLayer) == SU_ERROR_NONE)
				{
					bColorByLayerNew = DisplayColorByLayer;
				}
			}
		}

		// Check that flag was actually toggled to different state since last update
		if (bColorByLayer != bColorByLayerNew)
		{
			// Invalidate essentially everything when mode changed
			//    - layer materials vs regular materials need to be build/removed (material rebuild is caused by nodes/geom rebuild)
			//	  - meshes need to be rebuild(to split by layer, not by regular material)
			//	  - mesh actors need rebuild tochange override materials
			for (TPair<FComponentInstanceIDType, TSharedPtr<FComponentInstance>> Instance : ComponentInstances.ComponentInstanceMap)
			{
				Instance.Value->InvalidateEntityProperties();
			}
			
			for (const auto& IdValue : ComponentDefinitions.ComponentDefinitionMap)
			{
				TSharedPtr<FComponentDefinition> Definition = IdValue.Value;
				Definition->InvalidateDefinitionGeometry();
			}
			ModelDefinition->InvalidateDefinitionGeometry();

			bColorByLayer = bColorByLayerNew;
			return true;
		}
	}
	else
	{
		if (bColorByLayer)
		{
			return Materials.LayerMaterials.CheckForModifications();
		}
	}
	return false;
}


bool FExportContext::Update(bool bModifiedHint)
{
	bool bModified = bModifiedHint;

	bModified |= PreUpdateColorByLayer();

	bModified |= Scenes.Update();

	bModified |= ModelDefinition->UpdateModel(*this);

	if (!bModified)
	{
		// code below is not supposed to change Datasmith scene if not modification hint(i.e. no invalidated data) was present
		return false;
	}

	// Invalidate occurrences for changed instances first
	Model->UpdateEntityProperties(*this);
	ComponentInstances.UpdateProperties();
	Images.UpdateProperties();

	// Update occurrences visibility(before updating meshes to make sure to skip updating unused meshes)
	RootNode->UpdateVisibility(*this);

	// Update Datasmith Meshes after their usage was refreshed(in visibility update) and before node hierarchy update(where Mesh Actors are updated for meshes)
	ModelDefinition->UpdateDefinition(*this);
	ComponentDefinitions.Update();
	Images.Update();

	// ComponentInstances will invalidate occurrences 
	Model->UpdateEntityGeometry(*this);
	ComponentInstances.UpdateGeometry();

	// Update transforms/names for Datasmith Actors and MeshActors, create these actors if needed
	RootNode->Update(*this);

	Materials.Update();

	// Wait for mesh export to complete
	for(TFuture<bool>& Task: MeshExportTasks)
	{
		Task.Get();
	}
	MeshExportTasks.Reset();

	return bModified;
}

FDefinition* FExportContext::GetDefinition(SUEntityRef Entity)
{
	// No Entity means Model
	if (SUIsInvalid(Entity))
	{
		return ModelDefinition.Get();
	}
	else
	{
		return ComponentDefinitions.GetComponentDefinition(SUComponentDefinitionFromEntity(Entity)).Get();
	}
}

FDefinition* FExportContext::GetDefinition(FEntityIDType DefinitionEntityId)
{
	FDefinition* DefinitionPtr = nullptr;

	if (DefinitionEntityId.EntityID == 0)
	{
		return ModelDefinition.Get();
	}
	else
	{
		if (TSharedPtr<FComponentDefinition>* Ptr = ComponentDefinitions.FindComponentDefinition(DefinitionEntityId))
		{
			return Ptr->Get();
		}
	}
	return nullptr;
}

bool FExportContext::InvalidateColorByLayer()
{
	bColorByLayerInvaliated = true;
	return false; // Don't return 'modified'  - in case user toggles the flag back and forth Update will check if it's actually changed
}

void FComponentDefinitionCollection::Update()
{
	for (const auto& IdValue : ComponentDefinitionMap)
	{
		TSharedPtr<FComponentDefinition> Definition = IdValue.Value;
		Definition->UpdateDefinition(Context);
	}
}

void FImageCollection::Update()
{
	for (const auto& IdValue : Images)
	{
		IdValue.Value->Update(Context);
	}
}

void FImageCollection::UpdateProperties()
{
	for (const auto& IdValue : Images)
	{
		IdValue.Value->UpdateEntityProperties(Context);
	}
}

bool FImageCollection::InvalidateImage(const FEntityIDType& EntityID)
{
	if (TSharedPtr<FImage>* Found = Images.Find(EntityID))
	{
		(*Found)->InvalidateImage();
		return true;
	}
	return false;
}

bool FImageCollection::RemoveImage(FComponentInstanceIDType ParentEntityId, FEntityIDType ImageId)
{
	TSharedPtr<FImage>* Found = Images.Find(ImageId);
	if (!Found)
	{
		return false;
	}
	const TSharedPtr<FImage>& Image = *Found;

	FDefinition* ParentDefinition =  Context.GetDefinition(ParentEntityId);

	// Remove ComponentInstance for good only if incoming ParentDefinition is current Instance's parent. 
	//
	// Details:
	// ComponentInstance which removal is notified could have been relocated to another Definition 
	// This happens when Make Group is done - first new Group is added, containing existing ComponentInstance
	// And only after that event about removal from previous owning Definition is received
	if (Image->IsParentDefinition(ParentDefinition))
	{
		Image->RemoveImage(Context);
		Images.Remove(ImageId);
	}

	return true;

}

bool FSceneCollection::SetActiveScene(const FEntityIDType& EntityID)
{
	if (ActiveSceneId == EntityID)
	{
		return false;
	}
	ActiveSceneId = EntityID;
	return true;
}

bool FSceneCollection::Update()
{
	// Extract set of scenes(cameras ) and compute data hash to determine if Datasmith update is needed

	FMD5 MD5;

	size_t SceneCount = 0;
	SUModelGetNumScenes(Context.ModelRef, &SceneCount);
	MD5.Update(reinterpret_cast<uint8*>(&SceneCount), sizeof(SceneCount));

	TArray<FEntityIDType> SceneIds;

	if (SceneCount > 0)
	{
		// Retrieve the scenes in the SketchUp model.
		TArray<SUSceneRef> Scenes;
		Scenes.Init(SU_INVALID, SceneCount);
		SUResult SResult = SUModelGetScenes(Context.ModelRef, SceneCount, Scenes.GetData(), &SceneCount);
		Scenes.SetNum(SceneCount);
		// Make sure the SketchUp model has scenes to retrieve (no SU_ERROR_NO_DATA).
		if (SResult == SU_ERROR_NONE)
		{
			for (SUSceneRef SceneRef : Scenes)
			{
				// Make sure the SketchUp scene uses a camera.
				bool bSceneUseCamera = false;
				SUSceneGetUseCamera(SceneRef, &bSceneUseCamera); // we can ignore the returned SU_RESULT
				MD5.Update(reinterpret_cast<uint8*>(&bSceneUseCamera), sizeof(bSceneUseCamera));

				if (bSceneUseCamera)
				{
					FEntityIDType SceneId = DatasmithSketchUpUtils::GetSceneID(SceneRef);

					TSharedPtr<DatasmithSketchUp::FCamera> Camera;
					if (TSharedPtr<DatasmithSketchUp::FCamera>* Found =  SceneIdToCameraMap.Find(SceneId))
					{
						Camera = *Found;
					}
					else
					{
						Camera = FCamera::Create(Context, SceneRef);
						SceneIdToCameraMap.Add(SceneId, Camera);
					}

					MD5.Update(reinterpret_cast<uint8*>(&SceneId), sizeof(SceneId));

					Camera->bIsActive = SceneId == ActiveSceneId;
					
					FMD5Hash CameraHash = Camera->GetHash();
					
					MD5.Update(CameraHash.GetBytes(), CameraHash.GetSize());

					SceneIds.Add(SceneId);
				}
			}
		}
	}

	MD5.Update(reinterpret_cast<uint8*>(&ActiveSceneId), sizeof(ActiveSceneId));

	FMD5Hash Hash;
	Hash.Set(MD5);

	bool bModified = ScenesHash != Hash;

	ScenesHash = Hash;

	if (bModified)
	{

		for (TPair<FEntityIDType, TSharedPtr<FCamera>> IdCamera : SceneIdToCameraMap.Array())
		{
			FEntityIDType SceneId = IdCamera.Key;
			TSharedPtr<FCamera> Camera = IdCamera.Value;
			if (SceneIds.Contains(SceneId))
			{
				Camera->Update(Context);
			}
			else
			{
				// Cleanup removed scenes
				Context.DatasmithScene->RemoveActor(Camera->DatasmithCamera, EDatasmithActorRemovalRule::RemoveChildren);
				SceneIdToCameraMap.Remove(SceneId);
			}
		}
	}

	return bModified;
}

void FLayerCollection::PopulateFromModel(SUModelRef InModelRef)
{
	size_t LayerCount = 0;
	SUModelGetNumLayers(InModelRef, &LayerCount);

	TArray<SULayerRef> Layers;
	Layers.Init(SU_INVALID, LayerCount);
	SUResult SResult = SUModelGetLayers(InModelRef, LayerCount, Layers.GetData(), &LayerCount);
	Layers.SetNum(LayerCount);

	for (SULayerRef LayerRef : Layers)
	{
		UpdateLayer(LayerRef);
	}
}

void FLayerCollection::UpdateLayer(SULayerRef LayerRef)
{
	LayerVisibility.FindOrAdd(DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef))) = DatasmithSketchUpUtils::IsLayerVisible(LayerRef);
}

bool FLayerCollection::IsLayerVisible(SULayerRef LayerRef)
{
	bool* Found = LayerVisibility.Find(DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef)));
	return Found ? *Found : true;
}



SULayerRef FLayerCollection::GetLayer(FLayerIDType LayerId)
{
	size_t LayerCount = 0;
	SUModelGetNumLayers(Context.ModelRef, &LayerCount);

	TArray<SULayerRef> Layers;
	Layers.Init(SU_INVALID, LayerCount);
	SUResult SResult = SUModelGetLayers(Context.ModelRef, LayerCount, Layers.GetData(), &LayerCount);
	Layers.SetNum(LayerCount);

	for (SULayerRef LayerRef : Layers)
	{
		if (GetLayerId(LayerRef) == LayerId)
		{
			return LayerRef;
		}
	}
	return SU_INVALID;
}

FLayerIDType FLayerCollection::GetLayerId(SULayerRef LayerRef)
{
	return DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(LayerRef));
}

void FComponentDefinitionCollection::PopulateFromModel(SUModelRef InModelRef)
{
	// Get the number of normal component definitions in the SketchUp model.
	size_t SComponentDefinitionCount = 0;
	SUModelGetNumComponentDefinitions(InModelRef, &SComponentDefinitionCount); // we can ignore the returned SU_RESULT

	if (SComponentDefinitionCount > 0)
	{
		// Retrieve the normal component definitions in the SketchUp model.
		TArray<SUComponentDefinitionRef> SComponentDefinitions;
		SComponentDefinitions.Init(SU_INVALID, SComponentDefinitionCount);
		SUModelGetComponentDefinitions(InModelRef, SComponentDefinitionCount, SComponentDefinitions.GetData(), &SComponentDefinitionCount); // we can ignore the returned SU_RESULT
		SComponentDefinitions.SetNum(SComponentDefinitionCount);

		// Add the normal component definitions to our dictionary.
		for (SUComponentDefinitionRef SComponentDefinitionRef : SComponentDefinitions)
		{
			AddComponentDefinition(SComponentDefinitionRef);
		}
	}

	// Get the number of group component definitions in the SketchUp model.
	size_t SGroupDefinitionCount = 0;
	SUModelGetNumGroupDefinitions(InModelRef, &SGroupDefinitionCount); // we can ignore the returned SU_RESULT

	if (SGroupDefinitionCount > 0)
	{
		// Retrieve the group component definitions in the SketchUp model.
		TArray<SUComponentDefinitionRef> SGroupDefinitions;
		SGroupDefinitions.Init(SU_INVALID, SGroupDefinitionCount);
		SUModelGetGroupDefinitions(InModelRef, SGroupDefinitionCount, SGroupDefinitions.GetData(), &SGroupDefinitionCount); // we can ignore the returned SU_RESULT
		SGroupDefinitions.SetNum(SGroupDefinitionCount);

		// Add the group component definitions to our dictionary.
		for (SUComponentDefinitionRef SGroupDefinitionRef : SGroupDefinitions)
		{
			AddComponentDefinition(SGroupDefinitionRef);
		}
	}
}

TSharedPtr<FComponentDefinition> FComponentDefinitionCollection::AddComponentDefinition(SUComponentDefinitionRef InComponentDefinitionRef)
{
	TSharedPtr<FComponentDefinition> Definition = MakeShared<FComponentDefinition>(InComponentDefinitionRef);
	Definition->Parse(Context);
	ComponentDefinitionMap.Add(Definition->SketchupSourceID, Definition);
	return Definition;
}

TSharedPtr<FComponentDefinition> FComponentDefinitionCollection::GetComponentDefinition(
	SUComponentInstanceRef InComponentInstanceRef
)
{
	// Retrieve the component definition of the SketchUp component instance.
	SUComponentDefinitionRef SComponentDefinitionRef = SU_INVALID;
	SUComponentInstanceGetDefinition(InComponentInstanceRef, &SComponentDefinitionRef); // we can ignore the returned SU_RESULT
	return GetComponentDefinition(SComponentDefinitionRef);
}

TSharedPtr<FComponentDefinition> FComponentDefinitionCollection::GetComponentDefinition(SUComponentDefinitionRef ComponentDefinitionRef)
{
	if (TSharedPtr<FComponentDefinition>* Ptr = FindComponentDefinition(DatasmithSketchUpUtils::GetComponentID(ComponentDefinitionRef)))
	{
		return *Ptr;
	}

	return AddComponentDefinition(ComponentDefinitionRef);
}

TSharedPtr<FComponentDefinition>* FComponentDefinitionCollection::FindComponentDefinition(FComponentDefinitionIDType ComponentDefinitionID)
{
	return ComponentDefinitionMap.Find(ComponentDefinitionID);
}

TSharedPtr<FImage> FImageCollection::AddImage(FDefinition& ParentDefinition, const SUImageRef& ImageRef)
{
	FEntityIDType ImageId = DatasmithSketchUpUtils::GetEntityID(SUImageToEntity(ImageRef));

	TSharedPtr<FImage> Image;
	if (TSharedPtr<FImage>* Found = Images.Find(ImageId))
	{
		Image = *Found; 
	}
	else
	{
		Image = MakeShared<FImage>(ImageRef);
		Images.Add(ImageId, Image);
	}

	Image->SetParentDefinition(Context, &ParentDefinition);

	return Image;
}

void FEntitiesObjectCollection::RegisterEntities(DatasmithSketchUp::FEntities& Entities)
{
	for (int32 FaceId : Entities.EntitiesGeometry->FaceIds)
	{
		FaceIdForEntitiesMap.Add(FaceId, &Entities);
	}

	for (DatasmithSketchUp::FEntityIDType LayerId : Entities.EntitiesGeometry->Layers)
	{
		LayerIdForEntitiesMap.FindOrAdd(LayerId).Add(&Entities);
	}
}

void FEntitiesObjectCollection::UnregisterEntities(DatasmithSketchUp::FEntities& Entities)
{
	if (!Entities.EntitiesGeometry)
	{
		return;
	}

	for (int32 FaceId : Entities.EntitiesGeometry->FaceIds)
	{
		FaceIdForEntitiesMap.Remove(FaceId);
	}

	for (DatasmithSketchUp::FEntityIDType LayerId : Entities.EntitiesGeometry->Layers)
	{
		if(TSet<DatasmithSketchUp::FEntities*>* Ptr = LayerIdForEntitiesMap.Find(LayerId))
		{
			Ptr->Remove(&Entities);
		}
	}
}


TSharedPtr<FEntities> FEntitiesObjectCollection::AddEntities(FDefinition& InDefinition, SUEntitiesRef EntitiesRef)
{
	TSharedPtr<FEntities> Entities = MakeShared<FEntities>(InDefinition);

	Entities->EntitiesRef = EntitiesRef;

	return Entities;
}

DatasmithSketchUp::FEntities* FEntitiesObjectCollection::FindFace(int32 FaceId)
{
	if (DatasmithSketchUp::FEntities** PtrPtr = FaceIdForEntitiesMap.Find(FaceId))
	{
		return *PtrPtr;
	}
	return nullptr;
}

void FEntitiesObjectCollection::LayerModified(FEntityIDType LayerId)
{
	if (TSet<DatasmithSketchUp::FEntities*>* Ptr = LayerIdForEntitiesMap.Find(LayerId))
	{
		for (DatasmithSketchUp::FEntities* Entities : *Ptr)
		{
			Entities->Definition.InvalidateDefinitionGeometry();
		}
	}
}

TSharedPtr<FComponentInstance> FComponentInstanceCollection::AddComponentInstance(FDefinition& ParentDefinition, SUComponentInstanceRef InComponentInstanceRef)
{
	FComponentInstanceIDType ComponentInstanceId = DatasmithSketchUpUtils::GetComponentInstanceID(InComponentInstanceRef);

	TSharedPtr<FComponentInstance> ComponentInstance;
	if (TSharedPtr<FComponentInstance>* Ptr = ComponentInstanceMap.Find(ComponentInstanceId))
	{
		ComponentInstance = *Ptr; 
	}
	else
	{
		TSharedPtr<FComponentDefinition> Definition = Context.ComponentDefinitions.GetComponentDefinition(InComponentInstanceRef);
		ComponentInstance = MakeShared<FComponentInstance>(SUComponentInstanceToEntity(InComponentInstanceRef), *Definition);
		Definition->LinkComponentInstance(ComponentInstance.Get());
		ComponentInstanceMap.Add(ComponentInstanceId, ComponentInstance);
	}

	ComponentInstance->SetParentDefinition(Context, &ParentDefinition);

	return ComponentInstance;
}

bool FComponentInstanceCollection::RemoveComponentInstance(FComponentInstanceIDType ParentEntityId, FComponentInstanceIDType ComponentInstanceId)
{
	const TSharedPtr<FComponentInstance>* ComponentInstancePtr = ComponentInstanceMap.Find(ComponentInstanceId);
	if (!ComponentInstancePtr)
	{
		return false;
	}
	const TSharedPtr<FComponentInstance>& ComponentInstance = *ComponentInstancePtr;

	FDefinition* ParentDefinition =  Context.GetDefinition(ParentEntityId);

	// Remove ComponentInstance for good only if incoming ParentDefinition is current Instance's parent. 
	//
	// Details:
	// ComponentInstance which removal is notified could have been relocated to another Definition 
	// This happens when Make Group is done - first new Group is added, containing existing ComponentInstance
	// And only after that event about removal from previous owning Definition is received
	if (ComponentInstance->IsParentDefinition(ParentDefinition))
	{
		RemoveComponentInstance(ComponentInstance);
	}

	return true;
}

void FComponentInstanceCollection::RemoveComponentInstance(TSharedPtr<FComponentInstance> ComponentInstance)
{
	ComponentInstance->RemoveComponentInstance(Context);
	ComponentInstanceMap.Remove(ComponentInstance->GetComponentInstanceId());
}


void FComponentInstanceCollection::InvalidateComponentInstanceGeometry(FComponentInstanceIDType ComponentInstanceID)
{
	if (TSharedPtr<FComponentInstance>* Ptr = FindComponentInstance(ComponentInstanceID))
	{
		(*Ptr)->InvalidateEntityGeometry();
	}
}

void FComponentInstanceCollection::InvalidateComponentInstanceMetadata(FComponentInstanceIDType ComponentInstanceID)
{
	if (TSharedPtr<FComponentInstance>* Ptr = FindComponentInstance(ComponentInstanceID))
	{
		(*Ptr)->InvalidateEntityProperties(); // Metadata is updated with properties
	}
}

bool FComponentInstanceCollection::InvalidateComponentInstanceProperties(FComponentInstanceIDType ComponentInstanceId)
{
	if (TSharedPtr<FComponentInstance>* Ptr = FindComponentInstance(ComponentInstanceId))
	{
		TSharedPtr<FComponentInstance> ComponentInstance = *Ptr;
		
		// Replacing definition on a component instance fires the same event as changing properties
		SUComponentInstanceRef ComponentInstanceRef = ComponentInstance->GetComponentInstanceRef();
		TSharedPtr<FComponentDefinition> Definition = Context.ComponentDefinitions.GetComponentDefinition(ComponentInstanceRef);
		if (ComponentInstance->GetDefinition() != Definition.Get())
		{
			// Recreate and re-add instance
			FDefinition* ParentDefinition = ComponentInstance->Parent;
			RemoveComponentInstance(ComponentInstance);
			ParentDefinition->AddInstance(Context, AddComponentInstance(*ParentDefinition, ComponentInstanceRef));
		}

		ComponentInstance->InvalidateEntityProperties();
		return true;
	}
	return false;
}

void FComponentInstanceCollection::UpdateProperties()
{
	for (const auto& KeyValue : ComponentInstanceMap)
	{
		TSharedPtr<FComponentInstance> ComponentInstance = KeyValue.Value;
		ComponentInstance->UpdateEntityProperties(Context);
	}
}

void FComponentInstanceCollection::UpdateGeometry()
{
	for (const auto& KeyValue : ComponentInstanceMap)
	{
		TSharedPtr<FComponentInstance> ComponentInstance = KeyValue.Value;
		ComponentInstance->UpdateEntityGeometry(Context);
	}
}

void FComponentInstanceCollection::LayerModified(DatasmithSketchUp::FEntityIDType LayerId)
{
	for (const auto& KeyValue : ComponentInstanceMap)
	{
		TSharedPtr<DatasmithSketchUp::FComponentInstance> ComponentInstance = KeyValue.Value;
		if (SUIsValid(ComponentInstance->LayerRef) && (LayerId == DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(ComponentInstance->LayerRef))))
		{
			ComponentInstance->InvalidateEntityProperties();
		}
	}
}

FMaterialOccurrence* FRegularMaterials::RegisterInstance(FMaterialIDType MaterialID, FNodeOccurence* NodeOccurrence)
{
	if (NodeOccurrence->MaterialOverride)
	{
		NodeOccurrence->MaterialOverride->UnregisterInstance(Context, NodeOccurrence);
	}

	if (const TSharedPtr<DatasmithSketchUp::FMaterial> Material = FindOrCreateMaterial(MaterialID))
	{
		return &Material->RegisterInstance(NodeOccurrence);
	}

	return {}; // Don't use a material if material id is unknown(of default)
}

void FRegularMaterials::UnregisterGeometry(DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry)
{
	if (EntitiesGeometry->bDefaultMaterialUsed)
	{
		DefaultMaterial.UnregisterGeometry(Context, EntitiesGeometry);
		EntitiesGeometry->bDefaultMaterialUsed = false;
	}
}

FMaterialOccurrence* FRegularMaterials::RegisterGeometry(FMaterialIDType MaterialID, DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry)
{
	if (const TSharedPtr<DatasmithSketchUp::FMaterial> Material = FindOrCreateMaterial(MaterialID))
	{
		EntitiesGeometry->MaterialsUsed.Add(Material.Get());
		return &Material->RegisterGeometry(EntitiesGeometry);
	}

	// Use default material on static mesh in case material id not found in added materials(most likely id is 0 for Default itself)
	if (!EntitiesGeometry->bDefaultMaterialUsed)
	{
		DefaultMaterial.RegisterGeometry(EntitiesGeometry);
		EntitiesGeometry->bDefaultMaterialUsed = true;
	}
	return &DefaultMaterial; 
}

FMaterialOccurrence* FLayerMaterials::RegisterGeometryForLayer(FLayerIDType LayerID, FEntitiesGeometry* EntitiesGeometry)
{
	if (const TSharedPtr<DatasmithSketchUp::FMaterial> Material = FindOrCreateMaterialForLayer(LayerID))
	{
		EntitiesGeometry->MaterialsUsed.Add(Material.Get());
		return &Material->RegisterGeometry(EntitiesGeometry);
	}

	return {};
}

void FMaterialCollection::UnregisterGeometry(DatasmithSketchUp::FEntitiesGeometry* EntitiesGeometry)
{
	if (!EntitiesGeometry)
	{
		return;
	}

	TSet<FMaterial*> MaterialsUsed = EntitiesGeometry->MaterialsUsed;

	for (FMaterial* Ptr : MaterialsUsed)
	{
		DatasmithSketchUp::FMaterial& Material = *Ptr;
		Material.UnregisterGeometry(Context, EntitiesGeometry);
	}

	RegularMaterials.UnregisterGeometry(EntitiesGeometry);

	EntitiesGeometry->MaterialsUsed.Reset();
}

TSharedPtr<FMaterial> FMaterialCollection::CreateMaterial(SUMaterialRef MaterialDefinitionRef)
{
	TSharedPtr<FMaterial> Material = FMaterial::Create(Context, MaterialDefinitionRef);
	Materials.Add(Material.Get());
	return Material;
}

void FMaterialCollection::SetMeshActorOverrideMaterial(FNodeOccurence& Node, DatasmithSketchUp::FEntitiesGeometry& EntitiesGeometry, const TSharedPtr<IDatasmithMeshActorElement>& MeshActor)
{
	// SketchUp has 'material override' only for single material - 'Default' material or material from default('Layer0') layer in ColorByTag mode. 
	// So we reset overrides on the actor to remove this single override(if it was set) and re-add new override(if there's one)
	MeshActor->ResetMaterialOverrides();

	if (Context.bColorByLayer)
	{
		// Retrieve the default layer in the SketchUp model.
		SULayerRef DefaultLayerRef = SU_INVALID;
		SUModelGetDefaultLayer(Context.ModelRef, &DefaultLayerRef);

		if (!SUAreEqual(DefaultLayerRef, Node.EffectiveLayerRef)) // Don't apply default layer(Layer0) material as override
		{
			FEntityIDType LayerId = DatasmithSketchUpUtils::GetEntityID(SULayerToEntity(Node.EffectiveLayerRef));
			if (FMaterialOccurrence* Material = LayerMaterials.RegisterInstance(LayerId, &Node))
			{
				MeshActor->AddMaterialOverride(Material->GetName(), EntitiesGeometry.GetInheritedMaterialOverrideSlotId());
			}
		}
	}
	else
	{
		if (FMaterialOccurrence* Material = RegularMaterials.RegisterInstance(Node.InheritedMaterialID, &Node))
		{
			MeshActor->AddMaterialOverride(Material->GetName(), EntitiesGeometry.GetInheritedMaterialOverrideSlotId());
		}
	}
}

bool FRegularMaterials::InvalidateMaterial(FMaterialIDType MateriadId)
{
	if (TSharedPtr<FMaterial>* Ptr = MaterialForMaterialId.Find(MateriadId))
	{
		FMaterial& Material = **Ptr;

		Material.Invalidate();
		return true;
	}
	return false;
}

bool FRegularMaterials::RemoveMaterial(FMaterialIDType EntityId)
{
	TSharedPtr<FMaterial> Material;
	if (MaterialForMaterialId.RemoveAndCopyValue(EntityId, Material))
	{
		MaterialIdForMaterial.Remove(Material.Get());
		Context.Materials.RemoveMaterial(Material.Get());
		return true;
	}
	return false;
}

bool FMaterialCollection::RemoveMaterial(FMaterial* Material)
{
	Materials.Remove(Material);
	Material->Remove(Context);
	return true;
}

bool FRegularMaterials::InvalidateDefaultMaterial()
{
	DefaultMaterial.Invalidate(Context);
	return true;
}

TSharedPtr<FMaterial> FRegularMaterials::FindOrCreateMaterial(FMaterialIDType MaterialId)
{
	if (TSharedPtr<FMaterial>* Ptr = MaterialForMaterialId.Find(MaterialId))
	{
		return *Ptr;
	}

	// Get the number of material definitions in the SketchUp model.
	size_t SMaterialDefinitionCount = 0;
	SUModelGetNumMaterials(Context.ModelRef, &SMaterialDefinitionCount); // we can ignore the returned SU_RESULT

	// Retrieve the material definitions in the SketchUp model.
	TArray<SUMaterialRef> SMaterialDefinitions;
	SMaterialDefinitions.Init(SU_INVALID, SMaterialDefinitionCount);
	SUModelGetMaterials(Context.ModelRef, SMaterialDefinitionCount, SMaterialDefinitions.GetData(), &SMaterialDefinitionCount); // we can ignore the returned SU_RESULT
	SMaterialDefinitions.SetNum(SMaterialDefinitionCount);

	// Add the material definitions to our dictionary.
	for (SUMaterialRef SMaterialDefinitionRef : SMaterialDefinitions)
	{
		if (MaterialId == DatasmithSketchUpUtils::GetMaterialID(SMaterialDefinitionRef))
		{
			TSharedPtr<FMaterial> Material = Context.Materials.CreateMaterial(SMaterialDefinitionRef);
			MaterialForMaterialId.Emplace(MaterialId, Material);
			MaterialIdForMaterial.Add(Material.Get(), MaterialId);
			return Material;
		}
	}
	return {};
}

TSharedPtr<FMaterial> FLayerMaterials::FindOrCreateMaterialForLayer(FLayerIDType LayerID)
{
	if (TSharedPtr<FMaterial>* Ptr = MaterialForLayerId.Find(LayerID))
	{
		return *Ptr;
	}

	SULayerRef LayerRef = Context.Layers.GetLayer(LayerID);

	if (SUIsValid(LayerRef))
	{
		SUMaterialRef MaterialRef;
		if (SULayerGetMaterial(LayerRef, &MaterialRef) == SU_ERROR_NONE)
		{
			TSharedPtr<FMaterial> Material = Context.Materials.CreateMaterial(MaterialRef);
			Material->bGeometryHasScalingBakedIntoUvs = false;
			MaterialForLayerId.Add(LayerID, Material);
			LayerIdForMaterial.Add(Material.Get(), LayerID);
			return Material;
		}
	}
	return {};
}

