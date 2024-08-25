// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpCommon.h"
#include "DatasmithSketchUpUtils.h"

#include "DatasmithSketchUpCamera.h"
#include "DatasmithSketchUpComponent.h"
#include "DatasmithSketchUpMaterial.h"
#include "DatasmithSketchUpMesh.h"
#include "DatasmithSketchUpMetadata.h"
#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"


#include "DatasmithSketchUpExportContext.h"

#include "DatasmithDirectLink.h"

#include "DirectLinkEndpoint.h"

#include "IDatasmithExporterUIModule.h"
#include "IDirectLinkUI.h"

#include "DatasmithExportOptions.h"

#include "DatasmithSketchUpSDKBegins.h"
#include <SketchUpAPI/sketchup.h>
// SketchUp prior to 2020.2 doesn't have api to convert SU entities between Ruby and C 
#ifndef SKP_SDK_2019
#include <SketchUpAPI/application/ruby_api.h>
#endif
#include "DatasmithSketchUpSDKCeases.h"

#pragma warning(push)
// disable(SU2020): "__GNUC__' is not defined as a preprocessor macro, replacing"
#pragma warning(disable: 4668)
// disable(SU2020): macro name '_INTEGRAL_MAX_BITS' is reserved, '#define' ignored
#pragma warning(disable: 4117)
// disable(SU2020): 'DEPRECATED' : macro redefinition; 'ASSUME': macro redefinition
#pragma warning(disable: 4005)
// disable(SU2021): 'reinterpret_cast': unsafe conversion from 'ruby::backward::cxxanyargs::void_type (__cdecl *)' to 'rb_gvar_setter_t (__cdecl *)'	
#pragma warning(disable: 4191)
// disable(SU2019 & SU2020): 'register' is no longer a supported storage class	
#pragma warning(disable: 5033)
// disable(SU2024): '_Header_cstdbool': warning STL4004: <ccomplex>, <cstdalign>, <cstdbool>, and <ctgmath> are deprecated in C++17. You can define _SILENCE_CXX17_C_HEADER_DEPRECATION_WARNING or _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS to suppress this warning.
#pragma warning(disable: 4996)
// disable(SU2024): Implicit conversion from 'VALUE' to bool. Possible information loss
#pragma warning(disable: 4800)
// disable(SU2024): Dereferencing NULL pointer
#pragma warning(disable: 6011)


#undef DEPRECATED
#include <ruby.h>
#pragma warning(pop)

// Datasmith SDK.
#include "Containers/Array.h"
#include "Containers/StringConv.h"
#include "DatasmithExporterManager.h"
#include "DatasmithSceneExporter.h"
#include "DatasmithSceneFactory.h"
#include "Misc/Paths.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "DatasmithSceneXmlWriter.h"

#include "DatasmithSceneFactory.h"

class FDatasmithSketchUpScene
{
public:
	TSharedRef<IDatasmithScene> DatasmithSceneRef;
	TSharedRef<FDatasmithSceneExporter> SceneExporterRef;

	FDatasmithSketchUpScene() 
		: DatasmithSceneRef(FDatasmithSceneFactory::CreateScene(TEXT("")))
		, SceneExporterRef(MakeShared<FDatasmithSceneExporter>())
	{
#define SKETCHUP_HOST_NAME       TEXT("SketchUp")
#define SKETCHUP_VENDOR_NAME     TEXT("Trimble Inc.")

		DatasmithSceneRef->SetHost(SKETCHUP_HOST_NAME);

		// Set the vendor name of the application used to build the scene.
		DatasmithSceneRef->SetVendor(SKETCHUP_VENDOR_NAME);

		SUEdition Edition;
		SUGetEdition(&Edition);
		FString ProductName = TEXT("SketchUp Pro");
		switch (Edition)
		{
		case SUEdition_Make: 
			ProductName = TEXT("SketchUp Make"); 
			break;
		case SUEdition_Pro: 
			ProductName = TEXT("SketchUp Pro"); 
			break;
		default:
			ProductName = TEXT("SketchUp Unknown"); 
			break;
		};

		// Set the product name of the application used to build the scene.
		DatasmithSceneRef->SetProductName(*ProductName);

// todo: Mac binary doesn't export SUGetVersionStringUtf8(although docs doesn't mention this)
// Might fall back to SUGetAPIVersion
#if PLATFORM_WINDOWS
		TArray<char> VersionArr;
		VersionArr.SetNum(32);
		while (SUGetVersionStringUtf8(VersionArr.Num(), VersionArr.GetData()) == SU_ERROR_INSUFFICIENT_SIZE)
		{
			VersionArr.SetNum(VersionArr.Num());
		}
		FUTF8ToTCHAR Converter(VersionArr.GetData(), VersionArr.Num());
		FString VersionStr(Converter.Length(), Converter.Get());

		// Set the product version of the application used to build the scene.
		DatasmithSceneRef->SetProductVersion(*VersionStr);
#endif		

		// XXX: PreExport needs to be called before DirectLink instance is constructed - 
		// Reason - it calls initialization of FTaskGraphInterface. Callstack:
		// PreExport:
		//  - FDatasmithExporterManager::Initialize 
		//	-- DatasmithGameThread::InitializeInCurrentThread
		//  --- GEngineLoop.PreInit
		//  ---- PreInitPreStartupScreen
		//  ----- FTaskGraphInterface::Startup
		PreExport();
	}

	TSharedRef<IDatasmithScene>& GetDatasmithSceneRef()
	{
		return DatasmithSceneRef;
	}

	TSharedRef<FDatasmithSceneExporter>& GetSceneExporterRef()
	{
		return SceneExporterRef;
	}

	void SetName(const TCHAR* InName)
	{
		SceneExporterRef->SetName(InName);
		DatasmithSceneRef->SetName(InName);
		DatasmithSceneRef->SetLabel(InName);
	}

	void SetOutputPath(const TCHAR* InOutputPath)
	{
		// Set the output folder where this scene will be exported.
		SceneExporterRef->SetOutputPath(InOutputPath);
		DatasmithSceneRef->SetResourcePath(SceneExporterRef->GetOutputPath());
	}

	void PreExport()
	{
		// Start measuring the time taken to export the scene.
		SceneExporterRef->PreExport();
	}
};

class FDatasmithSketchUpDirectLinkManager
{
public:

	class FEndpointObserver: public DirectLink::IEndpointObserver
	{
	public:

		TArray<FString> ConnectionStatusList;

		FCriticalSection ConnectionStatusListCriticalSection;

		TArray<FString> GetConnectionStatus()
		{
			FScopeLock Lock(&ConnectionStatusListCriticalSection);
			return ConnectionStatusList;
		}
		
		FORCENOINLINE void OnStateChanged(const DirectLink::FRawInfo& RawInfo) override
		{
			using namespace DirectLink;

			FScopeLock Lock(&ConnectionStatusListCriticalSection); // OnStateChanged is called from a separate thread
			ConnectionStatusList.Reset();

			for (const FRawInfo::FStreamInfo& StreamInfo : RawInfo.StreamsInfo)
			{
				if (StreamInfo.ConnectionState != EStreamConnectionState::Active)
				{
					continue;
				}

				const FRawInfo::FDataPointInfo* SourceDataPointInfo =  RawInfo.DataPointsInfo.Find(StreamInfo.Source);
				if(!SourceDataPointInfo || !SourceDataPointInfo->bIsOnThisEndpoint)
				{
					continue;
				}

				const FRawInfo::FDataPointInfo* DestinationDataPointInfo = RawInfo.DataPointsInfo.Find(StreamInfo.Destination);

				if(!DestinationDataPointInfo)
				{
					continue;
				}
				
				
				const FRawInfo::FEndpointInfo* SourceEndpointInfo = RawInfo.EndpointsInfo.Find(SourceDataPointInfo->EndpointAddress);
				const FRawInfo::FEndpointInfo* DestinationEndpointInfo = RawInfo.EndpointsInfo.Find(DestinationDataPointInfo->EndpointAddress);

				ConnectionStatusList.Add(SourceDataPointInfo->Name);
				ConnectionStatusList.Add(SourceEndpointInfo->Name);
				ConnectionStatusList.Add(SourceEndpointInfo->UserName);
				ConnectionStatusList.Add(SourceEndpointInfo->ExecutableName);
				ConnectionStatusList.Add(SourceEndpointInfo->ComputerName);

				ConnectionStatusList.Add(DestinationEndpointInfo->Name);
				ConnectionStatusList.Add(DestinationDataPointInfo->Name);
				ConnectionStatusList.Add(DestinationEndpointInfo->Name);
				ConnectionStatusList.Add(DestinationEndpointInfo->UserName);
				ConnectionStatusList.Add(DestinationEndpointInfo->ExecutableName);
				ConnectionStatusList.Add(DestinationEndpointInfo->ComputerName);
			}
		}
	};

	static bool Init(bool bEnableUI, const FString& InEnginePath)
	{
		FDatasmithExporterManager::FInitOptions Options;
		Options.bEnableMessaging = true; // DirectLink requires the Messaging service.
		Options.bSuppressLogs = false;   // Log are useful, don't suppress them
		Options.bUseDatasmithExporterUI = bEnableUI;
		Options.RemoteEngineDirPath = *InEnginePath;

		if (!FDatasmithExporterManager::Initialize(Options))
		{
			return false;
		}

		if (int32 ErrorCode = FDatasmithDirectLink::ValidateCommunicationSetup())
		{
			return false;
		}


		return true;
	}

	void InitializeForScene(FDatasmithSketchUpScene& Scene)
	{
		DirectLinkImpl.InitializeForScene(Scene.GetDatasmithSceneRef());
		DirectLinkImpl.GetEnpoint()->AddEndpointObserver(&StateObserver);
	}

	void Close()
	{
		DirectLinkImpl.GetEnpoint()->RemoveEndpointObserver(&StateObserver);
	}

	TArray<FString> GetConnectionStatus()
	{
		return StateObserver.GetConnectionStatus();
	}

	void UpdateScene(FDatasmithSketchUpScene& Scene)
	{
		// FDatasmithSceneUtils::CleanUpScene(Scene.GetDatasmithSceneRef(), true);
		DirectLinkImpl.UpdateScene(Scene.GetDatasmithSceneRef());
	}

private:
	FDatasmithDirectLink DirectLinkImpl;
	FEndpointObserver StateObserver;
};

// Maintains Datasmith scene and promotes Sketchup scene change events to it, updating DirectLink
class FDatasmithSketchUpDirectLinkExporter
{
public:
	FDatasmithSketchUpScene ExportedScene;

	bool bEnableDirectLink;
	FDatasmithSketchUpDirectLinkManager DirectLinkManager;

	DatasmithSketchUp::FExportContext Context;

	FDatasmithSketchUpDirectLinkExporter(const TCHAR* InName, const TCHAR* InOutputPath, bool bInEnableDirectLink) : bEnableDirectLink(bInEnableDirectLink)
	{
		// Set scene name before initializing DirectLink for the scene so that the name is passed along
		ExportedScene.SetName(InName);
		ExportedScene.SetOutputPath(InOutputPath);

		// NOTE: InitializeForScene needs to be called in order to have DirectLink UI working(is was crashing otherwise)
		if (bEnableDirectLink)
		{
			DirectLinkManager.InitializeForScene(ExportedScene);
		}
	}

	bool Start()
	{
		Context.DatasmithScene = ExportedScene.GetDatasmithSceneRef();
		Context.SceneExporter = ExportedScene.GetSceneExporterRef();
		Context.Populate();

		SetSceneModified();
		return true;
	}

	bool Stop()
	{
		if (bEnableDirectLink)
		{
			DirectLinkManager.Close();
		}
		return true;
	}

	bool Update(bool bModifiedHint)
	{
		return Context.Update(bModifiedHint);
	}

	void SendUpdate()
	{
		if (bEnableDirectLink)
		{
			DirectLinkManager.UpdateScene(ExportedScene);
		}
	}

	void ExportCurrentDatasmithScene()
	{
		ExportedScene.GetSceneExporterRef()->Export(ExportedScene.GetDatasmithSceneRef());
	}

	void SetSceneModified()
	{
		// todo: set flag that update needs to be sent
	}

	FORCENOINLINE bool OnComponentInstanceChanged(SUEntityRef Entity)
	{
		DatasmithSketchUp::FEntityIDType EntityId = DatasmithSketchUpUtils::GetEntityID(Entity);

		Context.ComponentInstances.InvalidateComponentInstanceProperties(EntityId);
		SetSceneModified();
		return true;
	}

	FORCENOINLINE bool InvalidateGeometryForFace(DatasmithSketchUp::FEntityIDType FaceId)
	{
		// When Face is modified find Entities it belongs too, reexport Entities meshes and update Occurrences using this Entities
		if (DatasmithSketchUp::FEntities* Entities = Context.EntitiesObjects.FindFace(FaceId.EntityID))
		{
			Entities->Definition.InvalidateDefinitionGeometry();
			return true;
		}
		// todo: why there's could have been no Entities registered for a 'modified' Face?
		return false;
	}

	FORCENOINLINE bool OnEntityRemoved(DatasmithSketchUp::FEntityIDType ParentEntityId, DatasmithSketchUp::FEntityIDType EntityId)
	{
		// todo: map each existing entity id to its type so don't need to check every collection?

		//Try ComponentInstance/Group
		if (Context.ComponentInstances.RemoveComponentInstance(ParentEntityId, EntityId))
		{
			return true;
		}

		// Try Face
		if (InvalidateGeometryForFace(EntityId))
		{
			return true;
		}

		if (Context.Images.RemoveImage(ParentEntityId, EntityId))
		{
			return true;
		}

		// Try Material
		// Doesn't seem like material removal event ever comes through properly
		// MaterialsObserver::onMaterialRemove simply has deleted material entity(meaning there's not way to retrieve correct reference/id for it) 
		ensure(!Context.Materials.RegularMaterials.RemoveMaterial(EntityId));

		return false;
	}

	bool OnEntityModified(DatasmithSketchUp::FEntityIDType EntityId)
	{
		if (Context.ComponentInstances.InvalidateComponentInstanceProperties(EntityId))
		{
			return true;
		}

		{
			// Modified Entity could be a ComponentDefinition
			// This event is called when ComponentDefinition properties(like name) is modified
			// Also this is fired on special occasions - Stamp operation while modifying
			// Faces of a component/group it's performed on doesn't sent Face modification event but sends this 
			// So we take this opportunity  to invalidate Definition geometry too
			DatasmithSketchUp::FDefinition* Definition = Context.GetDefinition(EntityId);
			if (Definition)
			{
				Definition->InvalidateDefinitionGeometry();
				return true;
			}
		}

		if (Context.Materials.RegularMaterials.InvalidateMaterial(EntityId))
		{
			return true;
		}

		if (Context.Images.InvalidateImage(EntityId))
		{
			return true;
		}

		return false;
	}

	bool OnGeometryModified(DatasmithSketchUp::FEntityIDType EntityId)
	{
		DatasmithSketchUp::FDefinition* DefinitionPtr = Context.GetDefinition(EntityId);

		if (!DefinitionPtr)
		{
			// Not a component entity
			return false;
		}

		DefinitionPtr->InvalidateDefinitionGeometry();
		return true;
	}


	bool OnEntityAdded(DatasmithSketchUp::FEntityIDType ParentEntityId, DatasmithSketchUp::FEntityIDType EntityId)
	{
		DatasmithSketchUp::FDefinition* DefinitionPtr = Context.GetDefinition(ParentEntityId);
		
		if (!DefinitionPtr)
		{
			// Not a component entity
			return false;
		}

		DatasmithSketchUp::FEntities& Entities = DefinitionPtr->GetEntities();

		TArray<SUGroupRef> Groups = Entities.GetGroups();
		if (SUGroupRef* GroupRefPtr = Groups.FindByPredicate([EntityId](const SUGroupRef& GroupRef) { return DatasmithSketchUpUtils::GetGroupID(GroupRef) == EntityId; }))
		{
			SUGroupRef GroupRef = *GroupRefPtr;

			SUEntityRef Entity = SUGroupToEntity(GroupRef);

			SUComponentInstanceRef Ref = SUComponentInstanceFromEntity(Entity);

			// todo: remove dup
			DefinitionPtr->AddInstance(Context, Context.ComponentInstances.AddComponentInstance(*DefinitionPtr, Ref));
			return true;
		}

		// todo: embed Find by id into Entities itself
		TArray<SUComponentInstanceRef> ComponentInstances = Entities.GetComponentInstances();
		if (SUComponentInstanceRef* ComponentInstanceRefPtr = ComponentInstances.FindByPredicate([EntityId](const SUComponentInstanceRef& EntityRef) { return DatasmithSketchUpUtils::GetComponentInstanceID(EntityRef) == EntityId; }))
		{
			SUComponentInstanceRef ComponentInstanceRef = *ComponentInstanceRefPtr;

			SUEntityRef Entity = SUComponentInstanceToEntity(ComponentInstanceRef);
			// todo: remove dup

			SUComponentInstanceRef Ref = SUComponentInstanceFromEntity(Entity);
			DefinitionPtr->AddInstance(Context, Context.ComponentInstances.AddComponentInstance(*DefinitionPtr, Ref));
			return true;
		}

		return false;
	}

	bool OnEntityAdded(SUEntityRef EntityParent, SUEntityRef Entity)
	{
		SURefType EntityType = SUEntityGetType(Entity);
		switch (EntityType)
		{
		case SURefType_Group:
		case SURefType_ComponentInstance:
		{
			DatasmithSketchUp::FDefinition* DefinitionPtr = Context.GetDefinition(EntityParent);
			if (ensure(DefinitionPtr)) // Parent definition expected to already exist when new entity being added
			{
				DefinitionPtr->AddInstance(Context, Context.ComponentInstances.AddComponentInstance(*DefinitionPtr, SUComponentInstanceFromEntity(Entity)));
			}

			break;
		}
		case SURefType_Image:
		{
			DatasmithSketchUp::FDefinition* DefinitionPtr = Context.GetDefinition(EntityParent);
			if (ensure(DefinitionPtr)) // Parent definition expected to already exist when new entity being added
			{
				DefinitionPtr->AddImage(Context, Context.Images.AddImage(*DefinitionPtr, SUImageFromEntity(Entity)));
			}

			break;
		}
		case SURefType_Face:
		{
			Context.GetDefinition(EntityParent)->InvalidateDefinitionGeometry();
			break;
		}
		case SURefType_Material:
		{
			break;
		}
		default:
		{
			return false;
		}
		}
		return true;
	}
	
	bool OnMaterialAdded(DatasmithSketchUp::FEntityIDType EntityId)
	{
		// Not handling material additon here - materials are created when needed by geometry/components
		return true;
	}

	bool OnLayerModified(SUEntityRef Entity)
	{
		DatasmithSketchUp::FEntityIDType LayerId = DatasmithSketchUpUtils::GetEntityID(Entity);
		Context.ComponentInstances.LayerModified(LayerId);
		Context.EntitiesObjects.LayerModified(LayerId);
		Context.Layers.UpdateLayer(SULayerFromEntity(Entity));
		Context.Materials.LayerMaterials.UpdateLayer(SULayerFromEntity(Entity));
		Context.Images.LayerModified(LayerId);

		return true;
	}

	bool OnStyleModified()
	{
		return Context.Materials.RegularMaterials.InvalidateDefaultMaterial();
	}

	bool OnColorByLayerModified()
	{
		return Context.InvalidateColorByLayer();
	}

	bool SetActiveScene(const DatasmithSketchUp::FEntityIDType& EntityID)
	{
		return Context.Scenes.SetActiveScene(EntityID);
	}
};

///////////////////////////////////////////////////////////////////////////
// Ruby wrappers:

// Ruby wrapper utility functions
FString RubyStringToUnreal(VALUE path)
{
	const char* PathPtr = RSTRING_PTR(path);
	long PathLen = RSTRING_LEN(path);

	FUTF8ToTCHAR Converter(PathPtr, PathLen);
	// todo: check that Ruby has utf-8 internally
	// todo: test that non-null term doesn't fail
	return FString(Converter.Length(), Converter.Get());
}

VALUE UnrealStringToRuby(const FString& InStr)
{
	FTCHARToUTF8 Converter(*InStr, InStr.Len());

	return rb_str_new(Converter.Get(), Converter.Length());
}

typedef VALUE(*RubyFunctionType)(ANYARGS);

#pragma warning(push)
// disable: 'reinterpret_cast': unsafe conversion from 'F' to 'RubyFunctionType'
// This IS unsafe but so is Ruby C API
#pragma warning(disable: 4191)
template<typename F>
RubyFunctionType ToRuby(F f)
{
	return reinterpret_cast<RubyFunctionType>(f);
}
#pragma warning(pop)

// DatasmithSketchUpDirectLinkExporter wrapper
VALUE DatasmithSketchUpDirectLinkExporterCRubyClass;

void DatasmithSketchUpDirectLinkExporter_free(void* ptr)
{
	delete reinterpret_cast<FDatasmithSketchUpDirectLinkExporter*>(ptr);
}

// Created object and wrap it to return to Ruby
VALUE DatasmithSketchUpDirectLinkExporter_new(VALUE cls, VALUE name, VALUE path, VALUE enable_directlink)
{
	// Converting args
	bool bEnableDirectLink = RTEST(enable_directlink);

	Check_Type(name, T_STRING);
	Check_Type(path, T_STRING);

	FString NameUnreal = RubyStringToUnreal(name);
	FString PathUnreal = RubyStringToUnreal(path);
	// Done converting args

	FDatasmithSketchUpDirectLinkExporter* ptr = new FDatasmithSketchUpDirectLinkExporter(*NameUnreal, *PathUnreal, bEnableDirectLink);
	VALUE wrapped = Data_Wrap_Struct(cls, 0, DatasmithSketchUpDirectLinkExporter_free, ptr);
	rb_obj_call_init(wrapped, 0, NULL);
	return wrapped;
}

VALUE DatasmithSketchUpDirectLinkExporter_start(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	// Done converting args

	return Ptr->Start() ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_stop(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	// Done converting args

	return Ptr->Stop() ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_send_update(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, ptr);
	// Done converting args

	ptr->SendUpdate();
	return Qtrue;
}

VALUE DatasmithSketchUpDirectLinkExporter_set_active_scene(VALUE self, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args

	return Ptr->SetActiveScene(DatasmithSketchUp::FEntityIDType(EntityId)) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_update(VALUE self, VALUE modified_hint)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, ptr);

	bool bModifiedHint = RTEST(modified_hint);
	// Done converting args

	return ptr->Update(bModifiedHint) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_export_current_datasmith_scene(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, ptr);
	// Done converting args

	ptr->ExportCurrentDatasmithScene();

	return Qtrue;
}

VALUE DatasmithSketchUpDirectLinkExporter_get_connection_status(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, ptr);
	// Done converting args

	TArray<FString> Items = ptr->DirectLinkManager.GetConnectionStatus();

	VALUE Result = rb_ary_new();

	for(const FString& Str: Items)
	{
		rb_ary_push(Result, UnrealStringToRuby(Str));
	}

	return Result;
}



#ifndef SKP_SDK_2019
VALUE DatasmithSketchUpDirectLinkExporter_on_component_instance_changed(VALUE self, VALUE ruby_entity)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	SUEntityRef Entity = SU_INVALID;
	if (SUEntityFromRuby(ruby_entity, &Entity) != SU_ERROR_NONE) {
		rb_raise(rb_eTypeError, "Expected SketchUp Entity");
	}
	// Done converting args

	Ptr->OnComponentInstanceChanged(Entity);

	return Qtrue;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_entity_added(VALUE self, VALUE ruby_parent_entity, VALUE ruby_entity)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	SUEntityRef ParentEntity = SU_INVALID;
	if (!NIL_P(ruby_parent_entity) && (SUEntityFromRuby(ruby_parent_entity, &ParentEntity) != SU_ERROR_NONE)) {
		
		rb_raise(rb_eTypeError, "Expected SketchUp Entity but found '%s'", StringValuePtr(ruby_parent_entity));
	}

	SUEntityRef Entity = SU_INVALID;
	if (SUEntityFromRuby(ruby_entity, &Entity) != SU_ERROR_NONE) {
		rb_raise(rb_eTypeError, "Expected SketchUp Entity or nil");
	}
	// Done converting args

	return Ptr->OnEntityAdded(ParentEntity, Entity) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_layer_modified(VALUE self, VALUE ruby_entity)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	int RubyEntityRubyObjectType = TYPE(ruby_entity);

	SUEntityRef Entity = SU_INVALID;

	if (SUEntityFromRuby(ruby_entity, &Entity) != SU_ERROR_NONE) {
		rb_raise(rb_eTypeError, "Expected SketchUp Entity or nil");
	}
	// Done converting args

	;

	return Ptr->OnLayerModified(Entity) ? Qtrue : Qfalse;
}

#endif

VALUE DatasmithSketchUpDirectLinkExporter_on_entity_modified_by_id(VALUE self, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args

	return Ptr->OnEntityModified(DatasmithSketchUp::FEntityIDType(EntityId)) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_geometry_modified_by_id(VALUE self, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args

	return Ptr->OnGeometryModified(DatasmithSketchUp::FEntityIDType(EntityId)) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_entity_added_by_id(VALUE self, VALUE ruby_parent_entity_id, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_parent_entity_id, T_FIXNUM);
	int32 ParentEntityId = FIX2LONG(ruby_parent_entity_id);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args

	;

	return Ptr->OnEntityAdded(DatasmithSketchUp::FEntityIDType(ParentEntityId), DatasmithSketchUp::FEntityIDType(EntityId)) ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_material_added_by_id(VALUE self, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args

	return Ptr->OnMaterialAdded(DatasmithSketchUp::FEntityIDType(EntityId)) ? Qtrue : Qfalse;
}


VALUE DatasmithSketchUpDirectLinkExporter_on_entity_removed(VALUE self, VALUE ruby_parent_entity_id, VALUE ruby_entity_id)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);

	Check_Type(ruby_parent_entity_id, T_FIXNUM);
	int32 ParentEntityId = FIX2LONG(ruby_parent_entity_id);

	Check_Type(ruby_entity_id, T_FIXNUM);
	int32 EntityId = FIX2LONG(ruby_entity_id);
	// Done converting args
	
	bool bRemovalWasHandled = Ptr->OnEntityRemoved(DatasmithSketchUp::FEntityIDType(ParentEntityId), DatasmithSketchUp::FEntityIDType(EntityId));
	return  bRemovalWasHandled ? Qtrue : Qfalse;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_style_changed(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);
	// Done converting args

	Ptr->OnStyleModified();

	return Qtrue;
}

VALUE DatasmithSketchUpDirectLinkExporter_on_color_by_layer_changed(VALUE self)
{
	// Converting args
	FDatasmithSketchUpDirectLinkExporter* Ptr;
	Data_Get_Struct(self, FDatasmithSketchUpDirectLinkExporter, Ptr);
	// Done converting args

	Ptr->OnColorByLayerModified();

	return Qtrue;
}

VALUE on_load(VALUE self, VALUE enable_ui, VALUE engine_path) {
	// Converting args
	Check_Type(engine_path, T_STRING);

	// We always write textures during export and write them directly where they need to be
	// This options prevents extra copying of texture files (and copying a file was causing empty file on Mac)
	FDatasmithExportOptions::PathTexturesMode = EDSResizedTexturesPath::OriginalFolder;

	bool bEnableUI = RTEST(enable_ui);
	FString EnginePathUnreal = RubyStringToUnreal(engine_path);
	// Done converting args

	// This needs to be called before creating instance of DirectLink
	return FDatasmithSketchUpDirectLinkManager::Init(bEnableUI, EnginePathUnreal) ? Qtrue : Qfalse;
}

VALUE on_unload() {
	FDatasmithDirectLink::Shutdown();
	FDatasmithExporterManager::Shutdown();
	return Qtrue;
}

VALUE open_directlink_ui() {
// todo: implement DL UI on Mac
#if PLATFORM_WINDOWS

	if (IDatasmithExporterUIModule * Module = IDatasmithExporterUIModule::Get())
	{
		if (IDirectLinkUI* UI = Module->GetDirectLinkExporterUI())
		{
			UI->OpenDirectLinkStreamWindow();
			return Qtrue;
		}
	}
#endif	
	return Qfalse;
}

VALUE get_directlink_cache_directory() {
// todo: implement DL UI on Mac
#if PLATFORM_WINDOWS
	if (IDatasmithExporterUIModule* Module = IDatasmithExporterUIModule::Get())
	{
		if (IDirectLinkUI* UI = Module->GetDirectLinkExporterUI())
		{
			return UnrealStringToRuby(UI->GetDirectLinkCacheDirectory());
		}
	}
#endif	
	return Qnil;
}


// todo: hardcoded init module function name
extern "C" DLLEXPORT void Init_DatasmithSketchUp()
{

	VALUE EpicGames = rb_define_module("EpicGames");
	VALUE Datasmith = rb_define_module_under(EpicGames, "DatasmithBackend");

	rb_define_module_function(Datasmith, "on_load", ToRuby(on_load), 2);
	rb_define_module_function(Datasmith, "on_unload", ToRuby(on_unload), 0);

	rb_define_module_function(Datasmith, "open_directlink_ui", ToRuby(open_directlink_ui), 0);
	rb_define_module_function(Datasmith, "get_directlink_cache_directory", ToRuby(get_directlink_cache_directory), 0);

	DatasmithSketchUpDirectLinkExporterCRubyClass = rb_define_class_under(Datasmith, "DatasmithSketchUpDirectLinkExporter", rb_cObject);

	rb_define_singleton_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "new", ToRuby(DatasmithSketchUpDirectLinkExporter_new), 3);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "start", ToRuby(DatasmithSketchUpDirectLinkExporter_start), 0);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "stop", ToRuby(DatasmithSketchUpDirectLinkExporter_stop), 0);

#ifndef SKP_SDK_2019
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_component_instance_changed", ToRuby(DatasmithSketchUpDirectLinkExporter_on_component_instance_changed), 1);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_entity_added", ToRuby(DatasmithSketchUpDirectLinkExporter_on_entity_added), 2);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_layer_modified", ToRuby(DatasmithSketchUpDirectLinkExporter_on_layer_modified), 1);
#endif

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_entity_modified_by_id", ToRuby(DatasmithSketchUpDirectLinkExporter_on_entity_modified_by_id), 1);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_geometry_modified_by_id", ToRuby(DatasmithSketchUpDirectLinkExporter_on_geometry_modified_by_id), 1);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_entity_added_by_id", ToRuby(DatasmithSketchUpDirectLinkExporter_on_entity_added_by_id), 2);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_material_added_by_id", ToRuby(DatasmithSketchUpDirectLinkExporter_on_material_added_by_id), 1);

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_entity_removed", ToRuby(DatasmithSketchUpDirectLinkExporter_on_entity_removed), 2);

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_style_changed", ToRuby(DatasmithSketchUpDirectLinkExporter_on_style_changed), 0);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "on_color_by_layer_changed", ToRuby(DatasmithSketchUpDirectLinkExporter_on_color_by_layer_changed), 0);

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "update", ToRuby(DatasmithSketchUpDirectLinkExporter_update), 1);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "send_update", ToRuby(DatasmithSketchUpDirectLinkExporter_send_update), 0);
	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "export_current_datasmith_scene", ToRuby(DatasmithSketchUpDirectLinkExporter_export_current_datasmith_scene), 0);

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "get_connection_status", ToRuby(DatasmithSketchUpDirectLinkExporter_get_connection_status), 0);

	rb_define_method(DatasmithSketchUpDirectLinkExporterCRubyClass, "set_active_scene", ToRuby(DatasmithSketchUpDirectLinkExporter_set_active_scene), 1);

}

/* todo:
- error reporting(Ruby Console, Log etc)
*/
