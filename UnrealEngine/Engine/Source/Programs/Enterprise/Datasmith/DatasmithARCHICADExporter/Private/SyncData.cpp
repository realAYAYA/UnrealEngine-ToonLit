// Copyright Epic Games, Inc. All Rights Reserved.

#include "SyncData.h"

#include "ElementID.h"
#include "Synchronizer.h"
#include "Commander.h"
#include "Utils/ElementTools.h"
#include "Element2StaticMesh.h"
#include "MetaData.h"
#include "GeometryUtil.h"
#include "Utils/AutoChangeDatabase.h"
#include "Utils/TimeStat.h"
#include "TexturesCache.h"
#include "Utils/TaskMgr.h"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

#define TRACE_ATTACH_OBSERVERS 0

// Control access on this object (for queue operations)
GS::Lock ChildAccessControl;

// Condition variable
GS::Condition ChildAccessControlCV(ChildAccessControl);

// Control access on this object (for queue operations)
GS::Lock ElementAccessControl;

// Condition variable
GS::Condition ElementAccessControlCV(ElementAccessControl);

// Constructor
FSyncData::FSyncData(const GS::Guid& InGuid)
	: ElementId(InGuid)
{
}

// Destructor
FSyncData::~FSyncData()
{
	if (Parent != nullptr)
	{
		UE_AC_DebugF("FSyncData::~FSyncData - Deleting child while attached to it's parent {%s}\n",
					 ElementId.ToUniString().ToUtf8());
		Parent->RemoveChild(this);
		Parent = nullptr;
	}
	for (FChildsArray::SizeType i = Childs.Num(); i != 0; --i)
	{
		Childs[0]->SetParent(nullptr);
		UE_AC_Assert(i == Childs.Num());
	}
}

// Update data from a 3d element
void FSyncData::Update(const FElementID& InElementID)
{
	const API_Elem_Head& Header = InElementID.GetHeader();
	UE_AC_Assert(ElementId == APIGuid2GSGuid(Header.guid));
	UE_AC_Assert(Index3D == 0 && InElementID.GetIndex3D() != 0);

	Index3D = InElementID.GetIndex3D();
	if (GenId != InElementID.GetElement3D().GetGenId())
	{
		GenId = InElementID.GetElement3D().GetGenId();
		bIsModified = true;
	}

	// If AC element has been modified, recheck connections
	if (ModificationStamp != Header.modiStamp)
	{
		if (ModificationStamp > Header.modiStamp)
		{
			UE_AC_DebugF("FSyncData::Update {%s} New stamp younger: %lld, %lld\n",
						 APIGuidToString(Header.guid).ToUtf8(), ModificationStamp, Header.modiStamp);
		}
		ModificationStamp = Header.modiStamp;
		InElementID.HandleDepedencies();
		bIsModified = true;
	}
	if (ModificationStamp == 0)
	{
		UE_AC_DebugF("FSyncData::Update {%s} ModificationStamp == 0\n", APIGuidToString(Header.guid).ToUtf8());
	}

	SetDefaultParent(InElementID);
}

// Recursively clean. Delete element that hasn't 3d geometry related to it
void FSyncData::CleanAfterScan(FSyncDatabase* IOSyncDatabase)
{
	for (FChildsArray::SizeType IdxChild = Childs.Num(); IdxChild != 0;)
	{
		Childs[--IdxChild]->CleanAfterScan(IOSyncDatabase);
	}
	if (Childs.Num() == 0 && Index3D == 0)
	{
		DeleteMe(IOSyncDatabase);
	}
}

// Delete this sync data
void FSyncData::DeleteMe(FSyncDatabase* IOSyncDatabase)
{
	SetParent(nullptr);
	IOSyncDatabase->DeleteSyncData(ElementId);
	delete this;
}

void FSyncData::SetParent(FSyncData* InParent)
{
	if (Parent != InParent)
	{
		if (InParent)
		{
			InParent->AddChild(this);
		}
		if (Parent)
		{
			Parent->RemoveChild(this);
		}
		Parent = InParent;
	}
}

// Connect this actor to a default parent if it doesn't already have one
void FSyncData::SetDefaultParent(const FElementID& InElementID)
{
	if (!HasParent())
	{
		const API_Elem_Head& Header = InElementID.GetHeader();

		if (Header.hotlinkGuid == APINULLGuid)
		{
			// Parent is a layer
			SetParent(&InElementID.SyncContext.GetSyncDatabase().GetLayerSyncData(Header.layer));
		}
		else
		{
			// Parent is a hot link instance
			FSyncData*& ParentFound =
				InElementID.SyncContext.GetSyncDatabase().GetSyncData(APIGuid2GSGuid(Header.hotlinkGuid));
			if (ParentFound == nullptr)
			{
				ParentFound = new FSyncData::FHotLinkInstance(APIGuid2GSGuid(Header.hotlinkGuid),
															  &InElementID.SyncContext.GetSyncDatabase());
			}
			SetParent(ParentFound);
		}
	}
}

// Add a child to this sync data
void FSyncData::AddChild(FSyncData* InChild)
{
	UE_AC_TestPtr(InChild);

	for (auto& i : Childs)
	{
		if (i == InChild)
		{
			UE_AC_VerboseF("FSyncData::AddChild - Child already present\n");
			return;
		}
	}
	Childs.Add(InChild);
}

// Remove a child from this sync data
void FSyncData::RemoveChild(FSyncData* InChild)
{
	if (Childs.RemoveSingle(InChild) == 0)
	{
		UE_AC_VerboseF("FSyncData::RemoveChild - Child not present\n");
	}
}

// Return true if this element and all it's childs have been cut out
bool FSyncData::CheckAllCutOut()
{
	return true;
}

// Guid given to the scene element.
const GS::Guid FSyncData::FScene::SceneGUID("CBDEFBEF-0D4E-4162-8C4C-64AC34CEB4E6");

FSyncData::FScene::FScene()
	: FSyncData(SceneGUID)
{
}

// Delete this sync data
void FSyncData::FScene::DeleteMe(FSyncDatabase* IOSyncDatabase)
{
	if (SceneInfoMetaData.IsValid())
	{
		IOSyncDatabase->GetScene()->RemoveMetaData(SceneInfoMetaData);
		SceneInfoMetaData.Reset();
	}
	if (SceneInfoActorElement.IsValid())
	{
		IOSyncDatabase->GetScene()->RemoveActor(SceneInfoActorElement, EDatasmithActorRemovalRule::RemoveChildren);
		SceneInfoActorElement.Reset();
	}
	FSyncData::DeleteMe(IOSyncDatabase);
}

void FSyncData::FScene::AddChildActor(const TSharedPtr< IDatasmithActorElement >& InActor)
{
	GS::Guard< GS::Lock > lck(ChildAccessControl);
	if (!SceneElement.IsValid())
	{
		ThrowAssertionFail(__FILE__, __LINE__);
	}
	SceneElement->AddActor(InActor);
}

// Set (or replace) datasmith actor element related to this sync data
void FSyncData::FScene::SetActorElement(const TSharedPtr< IDatasmithActorElement >& /* InActor */)
{
	UE_AC_Assert(false); // Scene is not an actor
}

// Set the element to the scene element
void FSyncData::FScene::Process(FProcessInfo* IOProcessInfo)
{
	if (SceneElement.IsValid())
	{
		UE_AC_Assert(SceneElement == IOProcessInfo->SyncContext.GetSyncDatabase().GetScene());
	}
	SceneElement = IOProcessInfo->SyncContext.GetSyncDatabase().GetScene();
	UpdateInfo(IOProcessInfo);
}

void FSyncData::FScene::UpdateInfo(FProcessInfo* IOProcessInfo)
{
	if (!SceneInfoActorElement.IsValid())
	{
		SceneInfoActorElement = FDatasmithSceneFactory::CreateActor(GSStringToUE(SceneGUID.ToUniString()));
		IOProcessInfo->SyncContext.GetScene().AddActor(SceneInfoActorElement);
	}

	FMetaData InfoMetaData(SceneInfoActorElement);

	GSErrCode	  err = NoError;
	GS::UniString projectName = GS::UniString("Untitled");

	// Project info
	{
		API_ProjectInfo projectInfo;
		err = ACAPI_Environment(APIEnv_ProjectID, &projectInfo);
		if (err == NoError)
		{
			if (!projectInfo.untitled || projectInfo.projectName == nullptr)
				projectName = *projectInfo.projectName;
			InfoMetaData.AddStringProperty(TEXT("ProjectName"), projectName);

			if (projectInfo.projectPath != nullptr)
			{
				InfoMetaData.AddStringProperty(TEXT("ProjectPath"), *projectInfo.projectPath);
			}

			if (projectInfo.location != nullptr)
			{
				InfoMetaData.AddStringProperty(TEXT("ProjectLocation"), projectInfo.location->ToDisplayText());
			}

			if (projectInfo.location_team != nullptr)
			{
				InfoMetaData.AddStringProperty(TEXT("SharedProjectLocation"),
											   projectInfo.location_team->ToDisplayText());
			}
		}
	}

	// Project note info
	{
		API_ProjectNoteInfo projectNoteInfo;
		BNZeroMemory(&projectNoteInfo, sizeof(API_ProjectNoteInfo));
		err = ACAPI_Environment(APIEnv_GetProjectNotesID, &projectNoteInfo);

		if (err == NoError)
		{
			InfoMetaData.AddStringProperty(TEXT("Client"), projectNoteInfo.client);
			InfoMetaData.AddStringProperty(TEXT("Company"), projectNoteInfo.company);
			InfoMetaData.AddStringProperty(TEXT("Country"), projectNoteInfo.country);
			InfoMetaData.AddStringProperty(TEXT("PostalCode"), projectNoteInfo.code);
			InfoMetaData.AddStringProperty(TEXT("City"), projectNoteInfo.city);
			InfoMetaData.AddStringProperty(TEXT("Street"), projectNoteInfo.street);
			InfoMetaData.AddStringProperty(TEXT("MainArchitect"), projectNoteInfo.architect);
			InfoMetaData.AddStringProperty(TEXT("Draftsperson"), projectNoteInfo.draftsmen);
			InfoMetaData.AddStringProperty(TEXT("ProjectStatus"), projectNoteInfo.projectStatus);
			InfoMetaData.AddStringProperty(TEXT("DateOfIssue"), projectNoteInfo.dateOfIssue);
			InfoMetaData.AddStringProperty(TEXT("Keywords"), projectNoteInfo.keywords);
			InfoMetaData.AddStringProperty(TEXT("Notes"), projectNoteInfo.notes);
		}
	}

	// Place info
	{
		API_PlaceInfo placeInfo;
		err = ACAPI_Environment(APIEnv_GetPlaceSetsID, &placeInfo);
		if (err == NoError)
		{
			InfoMetaData.AddStringProperty(TEXT("Longitude"), GS::ValueToUniString(placeInfo.longitude));
			InfoMetaData.AddStringProperty(TEXT("Latitude"), GS::ValueToUniString(placeInfo.latitude));
			InfoMetaData.AddStringProperty(TEXT("Altitude"), GS::ValueToUniString(placeInfo.altitude));
			InfoMetaData.AddStringProperty(TEXT("North"), GS::ValueToUniString(placeInfo.north));
			InfoMetaData.AddStringProperty(TEXT("SunAngleXY"), GS::ValueToUniString(placeInfo.sunAngXY));
			InfoMetaData.AddStringProperty(TEXT("SunAngleZ"), GS::ValueToUniString(placeInfo.sunAngZ));
			InfoMetaData.AddStringProperty(TEXT("TimeZoneInMinutes"),
										   GS::ValueToUniString(placeInfo.timeZoneInMinutes));
			InfoMetaData.AddStringProperty(TEXT("TimeZoneOffset"), GS::ValueToUniString(placeInfo.timeZoneOffset));

			GSTime		 gstime;
			GSTimeRecord timeRecord(placeInfo.year, placeInfo.month, 0, placeInfo.day, placeInfo.hour, placeInfo.minute,
									placeInfo.second, 0);
			TIGetGSTime(&timeRecord, &gstime, TI_LOCAL_TIME);
			InfoMetaData.AddStringProperty(TEXT("LocalDateTime"),
										   TIGetTimeString(gstime, TI_LONG_DATE_FORMAT | TI_SHORT_TIME_FORMAT));
		}
	}

    FAutoChangeDatabase AutoRestoreDB(APIWind_3DModelID);
    API_Coord DbOffset = {0.0, 0.0};
    GSErrCode  GSErr = ACAPI_Database(APIDb_GetOffsetID, &DbOffset);
    if (GSErr == NoError)
    {
        InfoMetaData.AddStringProperty(TEXT("VirtualToWorldOffset.X"), GS::ValueToUniString(DbOffset.x));
        InfoMetaData.AddStringProperty(TEXT("VirtualToWorldOffset.Y"), GS::ValueToUniString(DbOffset.y));
    }
    else
    {
        UE_AC_DebugF("FSyncData::FScene::UpdateInfo - APIDb_GetOffsetID return error %s", GetErrorName(GSErr));
    }

    API_Coord3D LocOrigo = {0.0, 0.0, 0.0};
    GSErr = ACAPI_Database(APIDb_GetLocOrigoID, &LocOrigo);
    if (GSErr == NoError)
    {
        InfoMetaData.AddStringProperty(TEXT("UserOrigin.X"), GS::ValueToUniString(LocOrigo.x));
        InfoMetaData.AddStringProperty(TEXT("UserOrigin.Y"), GS::ValueToUniString(LocOrigo.y));
        InfoMetaData.AddStringProperty(TEXT("UserOrigin.Z"), GS::ValueToUniString(LocOrigo.z));
    }
    else
    {
        UE_AC_DebugF("FSyncData::FScene::UpdateInfo - APIDb_GetLocOrigoID return error %s", GetErrorName(GSErr));
    }

	SceneInfoActorElement->SetLabel(GSStringToUE(GS::UniString(projectName + " Project Informations")));

	InfoMetaData.SetOrUpdate(&SceneInfoMetaData, &IOProcessInfo->SyncContext.GetScene());
}

// Return Element as an actor
const TSharedPtr< IDatasmithActorElement >& FSyncData::FScene::GetActorElement() const
{
	static TSharedPtr< IDatasmithActorElement > NotAnActor;
	return NotAnActor;
}

void FSyncData::FScene::RemoveChildActor(const TSharedPtr< IDatasmithActorElement >& InActor)
{
	UE_AC_Assert(SceneElement.IsValid());
	SceneElement->RemoveActor(InActor, EDatasmithActorRemovalRule::RemoveChildren);
}

FSyncData::FActor::FActor(const GS::Guid& InGuid)
	: FSyncData(InGuid)
{
}

// Delete this sync data
void FSyncData::FActor::DeleteMe(FSyncDatabase* IOSyncDatabase)
{
	IOSyncDatabase->GetScene()->RemoveMetaData(MetaData);
	SetActorElement(TSharedPtr< IDatasmithActorElement >());
	FSyncData::DeleteMe(IOSyncDatabase);
}

void FSyncData::FActor::AddChildActor(const TSharedPtr< IDatasmithActorElement >& InActor)
{
	GS::Guard< GS::Lock > lck(ChildAccessControl);
	if (!ActorElement.IsValid())
	{
		ThrowAssertionFail(__FILE__, __LINE__);
	}
	ActorElement->AddChild(InActor);
}

void FSyncData::FActor::RemoveChildActor(const TSharedPtr< IDatasmithActorElement >& InActor)
{
	GS::Guard< GS::Lock > lck(ChildAccessControl);
	UE_AC_Assert(ActorElement.IsValid());
	ActorElement->RemoveChild(InActor);
}

// Set (or replace) datasmith actor element related to this sync data
void FSyncData::FActor::SetActorElement(const TSharedPtr< IDatasmithActorElement >& InElement)
{
	GS::Guard< GS::Lock > lck(ElementAccessControl);
	if (ActorElement != InElement)
	{
		UE_AC_TestPtr(Parent);
		if (ActorElement.IsValid())
		{
			Parent->RemoveChildActor(ActorElement);
			ActorElement.Reset();
		}
		if (InElement.IsValid())
		{
			Parent->AddChildActor(InElement);
			ActorElement = InElement;
		}
	}
}

// Add tags data
bool FSyncData::FActor::UpdateTags(const FTagsArray& InTags)
{
	int32 Count = (int32)InTags.GetSize();
	int32 Index = 0;
	if (ActorElement->GetTagsCount() == Count)
	{
		while (Index < Count && FCString::Strcmp(GSStringToUE(InTags[Index]), ActorElement->GetTag(Index)) == 0)
		{
			++Index;
		}
		if (Index == Count)
		{
			return false; // All Tags unchanged
		}
	}

	ActorElement->ResetTags();
	for (Index = 0; Index < Count; ++Index)
	{
		ActorElement->AddTag(GSStringToUE(InTags[Index]));
	}
	return true; // Tags changed
}

// Replace the current meta data by this new one
void FSyncData::FActor::ReplaceMetaData(IDatasmithScene&							   IOScene,
										const TSharedPtr< IDatasmithMetaDataElement >& InNewMetaData)
{
	// Disconnect previous meta data
	if (MetaData.IsValid())
	{
		IOScene.RemoveMetaData(MetaData);
		MetaData.Reset();
	}

	MetaData = InNewMetaData;

	// Connect previous meta data
	if (MetaData.IsValid())
	{
		MetaData->SetAssociatedElement(ActorElement);
	}
	IOScene.AddMetaData(MetaData);
}

// Guid used to synthetize layer guid
const GS::Guid FSyncData::FLayer::LayerGUID("97D32F90-A33E-0000-8305-D1A7D3FCED66");

// Return the synthetized layer guid.
GS::Guid FSyncData::FLayer::GetLayerGUID(short Layer)
{
	GS::Guid TmpGUID = LayerGUID;
	reinterpret_cast< short* >(&TmpGUID)[3] = Layer;
	return TmpGUID;
}

// Return true if this guid is for a layer
short FSyncData::FLayer::IsLayerGUID(GS::Guid LayerID)
{
	reinterpret_cast< short* >(&LayerID)[3] = 0;
	return LayerID == LayerGUID;
}

// Return the layer index
short FSyncData::FLayer::GetLayerIndex(const GS::Guid& InLayerID)
{
	return reinterpret_cast< const short* >(&InLayerID)[3];
}

FSyncData::FLayer::FLayer(const GS::Guid& InGuid)
	: FSyncData::FActor(InGuid)
{
}

void FSyncData::FLayer::Process(FProcessInfo* /* IOProcessInfo */)
{
	if (!ActorElement.IsValid())
	{
		short LayerIndex = GetLayerIndex(ElementId);

		// Get the layer's name
		GS::UniString LayerName;

		API_Attribute attribute;
		Zap(&attribute);
		attribute.header.typeID = API_LayerID;
#if AC_VERSION > 26
		attribute.header.index = ACAPI_CreateAttributeIndex(LayerIndex);
#else
		attribute.header.index = short(LayerIndex);
#endif
		attribute.header.uniStringNamePtr = &LayerName;
		GSErrCode error = ACAPI_Attribute_Get(&attribute);
		if (error != NoError)
		{
			// This case happened for the special ArchiCAD layer
			UE_AC_DebugF("CElementsHierarchy::CreateLayerNode - Error %s for layer index=%d\n", GetErrorName(error),
						 LayerIndex);
			if (error == APIERR_DELETED)
			{
				static const GS::UniString LayerDeleted("Layer deleted");
				LayerName = LayerDeleted;
			}
			else
			{
				utf8_string LayerError(Utf8StringFormat("Layer error=%s", GetErrorName(error)));
				LayerName = GS::UniString(LayerError.c_str(), CC_UTF8);
			}
		}
		else if (LayerName == "\x14") // Special ARCHICAD layer
			LayerName = "ARCHICAD";
		UE_AC_Assert(LayerName.GetLength() > 0);
		GS::Guid							 LayerGuid = APIGuid2GSGuid(attribute.layer.head.guid);
		TSharedRef< IDatasmithActorElement > NewActor =
			FDatasmithSceneFactory::CreateActor(GSStringToUE(LayerGuid.ToUniString()));
		NewActor->SetLabel(GSStringToUE(LayerName));
		SetActorElement(NewActor);
	}
}

inline Geometry::Transformation3D Convert(const ModelerAPI::Transformation& InMatrix)
{
	Geometry::Matrix33 M33;

	M33.Set(0, 0, InMatrix.matrix[0][0]);
	M33.Set(0, 1, InMatrix.matrix[0][1]);
	M33.Set(0, 2, InMatrix.matrix[0][2]);
	M33.Set(1, 0, InMatrix.matrix[1][0]);
	M33.Set(1, 1, InMatrix.matrix[1][1]);
	M33.Set(1, 2, InMatrix.matrix[1][2]);
	M33.Set(2, 0, InMatrix.matrix[2][0]);
	M33.Set(2, 1, InMatrix.matrix[2][1]);
	M33.Set(2, 2, InMatrix.matrix[2][2]);

	Geometry::Transformation3D Converted;

	Converted.SetMatrix(M33);
	Converted.SetOffset(Vector3D(InMatrix.matrix[0][3], InMatrix.matrix[1][3], InMatrix.matrix[2][3]));

	return Converted;
}

class FConvertGeometry2MeshElement : public FTaskMgr::FTask
{
  public:
	FConvertGeometry2MeshElement(const FSyncContext& InSyncContext, FSyncData::FElement* InElementSyncData,
								 FMeshClass* InMeshClass);

	void AddElementGeometry(FElementID* IOElementID, const Geometry::Vector3D& InGeometryShift);

	bool HasGeometry() const { return Element2StaticMesh.HasGeometry(); }

	void Run()
	{
		/*
		 #if PLATFORM_WINDOWS
		 SetThreadName(GS::Thread::GetCurrent().GetName().ToUtf8());
		 #else
		 pthread_setname_np(GS::Thread::GetCurrent().GetName().ToUtf8());
		 #endif
		 */
		try
		{
			TSharedPtr< IDatasmithMeshElement > Mesh;
			if (HasGeometry())
			{
				Mesh = Element2StaticMesh.CreateMesh();
			}
			MeshClass->SetMeshElement(Mesh);
			MeshClass->SetWaitingInstanceMesh(&SyncContext.GetSyncDatabase());
		}
		catch (std::exception& e)
		{
			UE_AC_DebugF("FConvertGeometry2MeshElement::Run - Catch std exception %s\n", e.what());
		}
		catch (GS::GSException& gs)
		{
			UE_AC_DebugF("FConvertGeometry2MeshElement::Run - Catch gs exception %s\n", gs.GetMessage().ToUtf8());
		}
		catch (...)
		{
			UE_AC_DebugF("FConvertGeometry2MeshElement::Run - Catch unknown exception\n");
		}
	}

  private:
	const FSyncContext&	 SyncContext;
	FElement2StaticMesh	 Element2StaticMesh;
	FSyncData::FElement& ElementSyncData;
	FMeshClass*			 MeshClass = nullptr;
};

FConvertGeometry2MeshElement::FConvertGeometry2MeshElement(const FSyncContext&	InSyncContext,
														   FSyncData::FElement* InElementSyncData,
														   FMeshClass*			InMeshClass)
	: SyncContext(InSyncContext)
	, Element2StaticMesh(InSyncContext)
	, ElementSyncData(*InElementSyncData)
	, MeshClass(InMeshClass)
{
	UE_AC_TestPtr(InElementSyncData);
}

void FConvertGeometry2MeshElement::AddElementGeometry(FElementID*						IOElementID,
													  const Geometry::Vector3D&          InGeometryShift
	)
{
	UE_AC_TestPtr(IOElementID);

	Element2StaticMesh.AddElementGeometry(IOElementID->GetElement3D(), InGeometryShift);
}

FSyncData::FElement::FElement(const GS::Guid& InGuid, const FSyncContext& /* InSyncContext */)
	: FSyncData::FActor(InGuid)
{
}

static void CopyActor(IDatasmithActorElement* OutDestActor, const IDatasmithActorElement& InSourceActor)
{
	UE_AC_TestPtr(OutDestActor);
	if (&InSourceActor == OutDestActor)
	{
		return;
	}

	OutDestActor->SetLabel(InSourceActor.GetLabel());
	OutDestActor->SetLayer(InSourceActor.GetLayer());
	OutDestActor->SetIsAComponent(InSourceActor.IsAComponent());
	OutDestActor->SetTranslation(InSourceActor.GetTranslation(), false);
	OutDestActor->SetRotation(InSourceActor.GetRotation(), false);

	// Copy childs from old actor to new one
	int32 ChildrenCount = InSourceActor.GetChildrenCount();
	for (int32 ChildIndex = 0; ChildIndex < ChildrenCount; ++ChildIndex)
	{
		OutDestActor->AddChild(InSourceActor.GetChild(ChildIndex));
	}

	int32 Count = InSourceActor.GetTagsCount();
	OutDestActor->ResetTags();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutDestActor->AddTag(InSourceActor.GetTag(Index));
	}
}

void FSyncData::FElement::MeshElementChanged()
{
	UE_AC_Assert(ActorElement.IsValid());
	if (MeshElement.IsValid())
	{
		// UE_AC_STAT(IOProcessInfo->SyncContext.Stats.TotalInstancesCreated++);
		if (!ActorElement->IsA(EDatasmithElementType::StaticMeshActor))
		{
			TSharedPtr< IDatasmithActorElement >	 OldActor = ActorElement;
			TSharedPtr< IDatasmithMeshActorElement > NewMeshActor =
				FDatasmithSceneFactory::CreateMeshActor(OldActor->GetName());
			SetActorElement(NewMeshActor);
			CopyActor(NewMeshActor.Get(), *OldActor.Get());
		}
		IDatasmithMeshActorElement& MeshActor = *StaticCastSharedPtr< IDatasmithMeshActorElement >(ActorElement).Get();
		MeshActor.SetStaticMeshPathName(MeshElement->GetName());
		// MeshElement->SetLabel(ActorElement->GetLabel());
	}
	else
	{
		// UE_AC_STAT(IOProcessInfo->SyncContext.Stats.TotalEmptyInstancesCreated++);
		if (ActorElement->IsA(EDatasmithElementType::StaticMeshActor))
		{
			TSharedPtr< IDatasmithActorElement > OldMeshActor = ActorElement;
			TSharedPtr< IDatasmithActorElement > NewActor =
				FDatasmithSceneFactory::CreateActor(OldMeshActor->GetName());
			SetActorElement(NewActor);
			CopyActor(NewActor.Get(), *OldMeshActor.Get());
		}
	}
}

// Return true if this element and all it's childs have been cut out
bool FSyncData::FElement::CheckAllCutOut()
{
	if (Index3D != 0)
	{
		return false;
	}
	for (FChildsArray::SizeType IterChild = 0; IterChild < Childs.Num(); ++IterChild)
	{
		if (!Childs[IterChild]->CheckAllCutOut())
		{
			return false;
		}
	}
	return true;
}

void FSyncData::FElement::Process(FProcessInfo* IOProcessInfo)
{
	if (Index3D == 0) // No 3D imply an hierarchical parent or recently cut out element
	{
		if (ActorElement.IsValid())
		{
			if (ActorElement->IsA(EDatasmithElementType::StaticMeshActor)) // Previously was a mesh, now presume cut out
			{
				// Element is a child cut out and it's parent hasn't been completely cut-out.
				SetActorElement(TSharedPtr< IDatasmithActorElement >());
				if (!CheckAllCutOut())
				{
					UE_AC_DebugF("FSyncData::FElement::Process - Element cut out with uncut child %s\n",
								 ElementId.ToUniString().ToUtf8());
				}
			}
		}
		else // Hierarchical parent
		{
			IOProcessInfo->ElementID.InitElement(this);
			IOProcessInfo->ElementID.InitHeader(GSGuid2APIGuid(ElementId));
			CheckModificationStamp(IOProcessInfo->ElementID.GetHeader().modiStamp);

			TypeID = IOProcessInfo->ElementID.GetTypeID();

			UE_AC_STAT(IOProcessInfo->SyncContext.Stats.TotalOwnerCreated++);
			TSharedRef< IDatasmithActorElement > NewActor =
				FDatasmithSceneFactory::CreateActor(GSStringToUE(ElementId.ToUniString()));

			GS::UniString ElemenInfo;
			if (FElementTools::GetInfoString(IOProcessInfo->ElementID.GetHeader().guid, &ElemenInfo))
			{
				NewActor->SetLabel(GSStringToUE(ElemenInfo));
			}
			else
			{
				NewActor->SetLabel(TEXT("Unnamed"));
			}
			NewActor->SetIsAComponent(bIsAComponent);

			SetActorElement(NewActor);
			bMetadataProcessed = false;
			if (IOProcessInfo->bProcessMetaData)
			{
				ProcessMetaData(&IOProcessInfo->SyncContext.GetSyncDatabase());
			}
		}
	}
	else
	{
		if (IsModified())
		{
			// Advance progression bar to the current value
			IOProcessInfo->SyncContext.NewCurrentValue(++IOProcessInfo->ProgessValue);

			IOProcessInfo->ElementID.InitElement(this);
			IOProcessInfo->ElementID.InitHeader();

			TypeID = IOProcessInfo->ElementID.GetTypeID();

			ModelerAPI::Transformation LocalToWorld =
				IOProcessInfo->ElementID.GetElement3D().GetElemLocalToWorldTransformation();
			// Shift geometry to pivot it at the bounds center
			Geometry::Vector3D GeometryShift;
			{
				Box3D LocalBounds = IOProcessInfo->ElementID.GetElement3D().GetBounds(
					ModelerAPI::CoordinateSystem::ElemLocal);
				Geometry::Point3D LocalBoundsCenter{
					(LocalBounds.xMin + LocalBounds.xMax) * 0.5,
					(LocalBounds.yMin + LocalBounds.yMax) * 0.5,
					LocalBounds.zMin};

				// Transform center to world
				TRANMAT LocalToWorldTranmatOrig;
				LocalToWorld.ToTRANMAT(&LocalToWorldTranmatOrig);
				Geometry::Point3D BoundsCenterWorld = Geometry::TransformPoint(LocalToWorldTranmatOrig, LocalBoundsCenter);

				// "Re-pivot" object to the center of bounding box by
				// ...shifting geometry to have its local zero coordinates at the geometry bounds center
				GeometryShift = -LocalBoundsCenter;
				// ...and changing transform to translate geometry zero point to bounds center in world space we computed
				LocalToWorld.matrix[0][3] = BoundsCenterWorld[0];
				LocalToWorld.matrix[1][3] = BoundsCenterWorld[1];
				LocalToWorld.matrix[2][3] = BoundsCenterWorld[2];
				LocalToWorld.status = BoundsCenterWorld.IsNullVector(EPS) ? TR_IDENT : TR_TRANSL_ONLY;
			}

			if (!ActorElement.IsValid())
			{
				SetActorElement(FDatasmithSceneFactory::CreateMeshActor(GSStringToUE(ElementId.ToUniString())));
			}
			ActorElement->SetIsAComponent(bIsAComponent);

			// Set actor label
			GS::UniString ElemenInfo;
			if (FElementTools::GetInfoString(IOProcessInfo->ElementID.GetHeader().guid, &ElemenInfo))
			{
				ActorElement->SetLabel(GSStringToUE(ElemenInfo));
			}
			else
			{
				ActorElement->SetLabel(TEXT("Unnamed"));
			}

			ActorElement->SetTranslation(FGeometryUtil::GetTranslationVector(LocalToWorld.matrix));
			ActorElement->SetRotation(FGeometryUtil::GetRotationQuat(LocalToWorld.matrix));

			// Set actor layer
#if AC_VERSION > 26
			const short Index = short(IOProcessInfo->ElementID.GetHeader().layer.ToInt32_Deprecated());
			ActorElement->SetLayer(*IOProcessInfo->SyncContext.GetSyncDatabase().GetLayerName(Index));
#else
			ActorElement->SetLayer(*IOProcessInfo->SyncContext.GetSyncDatabase().GetLayerName(IOProcessInfo->ElementID.GetHeader().layer));
#endif

			bMetadataProcessed = false;
			if (IOProcessInfo->bProcessMetaData)
			{
				ProcessMetaData(&IOProcessInfo->SyncContext.GetSyncDatabase());
			}

			FMeshClass* MeshClass = IOProcessInfo->ElementID.GetMeshClass();
			UE_AC_Assert(MeshClass != nullptr);
			constexpr short IsRelative = short(TR_DET_1 | TR_TRANSL_ONLY);
			if (MeshClass->AddInstance(this, &IOProcessInfo->SyncContext.GetSyncDatabase()) == FMeshClass::kBuild)
			{
				MeshClass->Translation = ActorElement->GetTranslation();
				MeshClass->Rotation = ActorElement->GetRotation();
				FConvertGeometry2MeshElement* ConvertGeometry2MeshElement =
					new FConvertGeometry2MeshElement(IOProcessInfo->SyncContext, this, MeshClass);
				ConvertGeometry2MeshElement->AddElementGeometry(&IOProcessInfo->ElementID, GeometryShift);
				if (ConvertGeometry2MeshElement->HasGeometry())
				{
					UE_AC_STAT(IOProcessInfo->SyncContext.Stats.TotalMeshClassesCreated++);
				}
				else
				{
					UE_AC_STAT(IOProcessInfo->SyncContext.Stats.TotalEmptyMeshClassesCreated++);
				}

				// ConvertGeometry2MeshElement->Run();
				// delete ConvertGeometry2MeshElement;
				FTaskMgr::GetMgr()->AddTask(ConvertGeometry2MeshElement, FTaskMgr::kSchedule);
			}
		}
	}
}

// Add tags data
bool FSyncData::FElement::AddTags(FSyncDatabase* IOSyncDatabase)
{
	UE_AC_Assert(ActorElement.IsValid());

	FTagsArray Tags(10);

	static const GS::UniString PrefixTagUniqueID("Archicad.Element.UniqueID.");
	Tags.Push(PrefixTagUniqueID + ElementId.ToUniString());

	static const GS::UniString PrefixTagType("Archicad.Element.Type.");
	GS::UniString			   ElementTypeName(FElementTools::TypeName(TypeID), CC_UTF8);
	Tags.Push(PrefixTagType + ElementTypeName);

	if (TypeID == API_ObjectID || TypeID == API_LampID || TypeID == API_WindowID || TypeID == API_DoorID)
	{
		API_Element APIElement;
		Zap(&APIElement);
		APIElement.header.guid = GSGuid2APIGuid(ElementId);
		GSErrCode GSErr = ACAPI_Element_Get(&APIElement, 0);
		UE_AC_Assert(GET_HEADER_TYPEID(APIElement.header) == TypeID);

		if (GSErr == NoError)
		{
			GS::Int32 LibPartIndex = 0;
			switch (TypeID)
			{
				case API_ObjectID:
					LibPartIndex = APIElement.object.libInd;
					break;
				case API_LampID:
					LibPartIndex = APIElement.lamp.libInd;
					break;
				case API_WindowID:
					LibPartIndex = APIElement.window.openingBase.libInd;
					break;
				case API_DoorID:
					LibPartIndex = APIElement.door.openingBase.libInd;
					break;
				default:
					UE_AC_Assert(false);
					break;
			}
			FLibPartInfo* LibPartInfo = IOSyncDatabase->GetLibPartInfo(LibPartIndex);
			if (LibPartInfo != nullptr)
			{
				static const GS::UniString PrefixLibPartMain("Archicad.Element.LibPart.Main.");
				Tags.Push(PrefixLibPartMain + LibPartInfo->Guid.Main.ToUniString());

				static const GS::UniString PrefixLibPartRev("Archicad.Element.LibPart.Rev.");
				Tags.Push(PrefixLibPartRev + LibPartInfo->Guid.Rev.ToUniString());

				static const GS::UniString PrefixLibPartName("Archicad.Element.LibPart.Name.");
				Tags.Push(PrefixLibPartName + LibPartInfo->Name);
			}

			if (TypeID == API_ObjectID || TypeID == API_LampID)
			{
				UE_AC_Assert(offsetof(API_Element, object.reflected) == offsetof(API_Element, lamp.reflected));
				if (APIElement.object.useObjMaterials)
				{
					static const GS::UniString TagUseObjectMaterial("Archicad.Element.UseObjectMaterial");
					Tags.Push(TagUseObjectMaterial);
				}
				if (APIElement.object.reflected)
				{
					static const GS::UniString TagObjectReflected("Archicad.Element.Reflected");
					Tags.Push(TagObjectReflected);
				}
			}
			else if (TypeID == API_WindowID || TypeID == API_DoorID)
			{
				UE_AC_Assert(offsetof(API_Element, window.openingBase) == offsetof(API_Element, door.openingBase));
				if (APIElement.window.openingBase.reflected)
				{
					static const GS::UniString TagObjectReflected("Archicad.Element.Mirror.Y");
					Tags.Push(TagObjectReflected);
				}
				if (APIElement.window.openingBase.oSide)
				{
					static const GS::UniString TagObjectReflected("Archicad.Element.Mirror.X");
					Tags.Push(TagObjectReflected);
				}
				if (APIElement.window.openingBase.refSide)
				{
					static const GS::UniString TagObjectReflected("Archicad.Element.Mirror.X.Same");
					Tags.Push(TagObjectReflected);
				}
			}
		}
	}

	return UpdateTags(Tags);
}

// Return true if this element need to update tags and metadata
bool FSyncData::FElement::NeedTagsAndMetaDataUpdate()
{
	return !bMetadataProcessed;
}

// Called from process meta data idle task
bool FSyncData::FElement::ProcessMetaData(FSyncDatabase* IOSyncDatabase)
{
	if (bMetadataProcessed)
	{
		return false;
	}

	bMetadataProcessed = true;
	return AddTags(IOSyncDatabase) | UpdateMetaData(&IOSyncDatabase->GetScene().Get());
}

void FSyncData::FElement::SetMesh(FSyncDatabase* IOSyncDatabase, const TSharedPtr< IDatasmithMeshElement >& InMesh)
{
	IOSyncDatabase->SetMesh(&MeshElement, InMesh);
	MeshElementChanged();
}

// Attach observer for Auto Sync
bool FSyncData::FElement::AttachObserver(FAttachObservers* IOAttachObservers)
{
	bool bChanged = false;

	// We attach observer only when we will need it
	if (bIsObserved == false)
	{
#if ATTACH_ONSERVER_STAT
		FTimeStat SlotStart;
#endif
		bIsObserved = true;
		GSErrCode GSErr = ACAPI_Element_AttachObserver(GSGuid2APIGuid(ElementId), APINotifyElement_EndEvents);
		if (GSErr != NoError && GSErr != APIERR_LINKEXIST)
		{
			UE_AC_DebugF("FSyncData::FElement::AttachObserver - ACAPI_Element_AttachObserver error=%s\n",
						 GetErrorName(GSErr));
		}
#if ATTACH_ONSERVER_STAT
		double AfterAttachObserver = FTimeStat::CpuTimeClock();
#endif

		API_Elem_Head ElementHead;
		Zap(&ElementHead);
		ElementHead.guid = GSGuid2APIGuid(ElementId);
		GSErr = ACAPI_Element_GetHeader(&ElementHead);
		if (GSErr == NoError)
		{
			bChanged = ElementHead.modiStamp != ModificationStamp;
			if (bChanged)
			{
				UE_AC_TraceF("FSyncData::FElement::AttachObserver - Object {%s} - ModificationStamp %lld -> %lld\n",
							 ElementId.ToUniString().ToUtf8(), ModificationStamp, ElementHead.modiStamp);
			}
		}
		else
		{
			UE_AC_DebugF("FSyncData::FElement::AttachObserver - ACAPI_Element_GetHeader error=%s\n",
						 GetErrorName(GSErr));
		}
#if ATTACH_ONSERVER_STAT
		IOAttachObservers->CumulateStats(SlotStart, AfterAttachObserver);
#endif
	}

	return bChanged;
}

// Delete this sync data
void FSyncData::FElement::DeleteMe(FSyncDatabase* IOSyncDatabase)
{
	IOSyncDatabase->SetMesh(&MeshElement, TSharedPtr< IDatasmithMeshElement >());
	FSyncData::FActor::DeleteMe(IOSyncDatabase);
}

// Rebuild the meta data of this element
bool FSyncData::FElement::UpdateMetaData(IDatasmithScene* IOScene)
{
	FMetaData MetaDataExporter(ActorElement);
	MetaDataExporter.ExportMetaData(ElementId);

	// Add some object and lamp meta data
	if (TypeID == API_ObjectID || TypeID == API_LampID)
	{
		FAutoMemo AutoMemo(GSGuid2APIGuid(ElementId), APIMemoMask_AddPars);
		if (AutoMemo.GSErr == NoError)
		{
			if (AutoMemo.Memo.params) // Can be null
			{
				GS::UniString EscapeAssetId;
				if (GetParameter(AutoMemo.Memo.params, "enscapeAssetId", &EscapeAssetId))
				{
					MetaDataExporter.AddProperty(TEXT("EnscapeAssetId"), EDatasmithKeyValuePropertyType::String,
												 EscapeAssetId);
				}
			}
		}
	}
	return MetaDataExporter.SetOrUpdate(&MetaData, IOScene);
}

void FSyncData::FCameraSet::Process(FProcessInfo* /* IOProcessInfo */)
{
	if (!ActorElement.IsValid())
	{
		TSharedRef< IDatasmithActorElement > NewActor =
			FDatasmithSceneFactory::CreateActor(GSStringToUE(ElementId.ToUniString()));

		NewActor->SetLabel(GSStringToUE(Name));

		SetActorElement(NewActor);

		if (bOpenedPath)
		{
			NewActor->AddTag(TEXT("Path.opened"));
		}
		else
		{
			NewActor->AddTag(TEXT("Path.closed"));
		}
	}
}

// Guid given to the current view.
const GS::Guid FSyncData::FCamera::CurrentViewGUID("B2BD9C50-60EB-4E64-902B-D1574FADEC45");

void FSyncData::FCamera::Process(FProcessInfo* /* IOProcessInfo */)
{
	if (!ActorElement.IsValid())
	{
		SetActorElement(FDatasmithSceneFactory::CreateCameraActor(GSStringToUE(ElementId.ToUniString())));
		MarkAsModified();
	}

	if (ElementId == CurrentViewGUID)
	{
		InitWithCurrentView();
	}
	else
	{
		if (IsModified())
		{
			InitWithCameraElement();
		}
	}
}

void FSyncData::FCamera::InitWithCurrentView()
{
	FAutoChangeDatabase changeDB(APIWind_3DModelID);

	IDatasmithCameraActorElement& CameraElement = static_cast< IDatasmithCameraActorElement& >(*ActorElement.Get());
	CameraElement.SetLabel(TEXT("Current view"));

	// Set the camera data from AC 3D projection info
	API_3DProjectionInfo projSets;
	GSErrCode			 GSErr = ACAPI_Environment(APIEnv_Get3DProjectionSetsID, &projSets, NULL);
	if (GSErr == GS::NoError)
	{
		if (projSets.isPersp)
		{
			CameraElement.SetTranslation(FGeometryUtil::GetTranslationVector(
				{projSets.u.persp.pos.x, projSets.u.persp.pos.y, projSets.u.persp.cameraZ}));

			CameraElement.SetRotation(FGeometryUtil::GetRotationQuat(
				FGeometryUtil::GetPitchAngle(projSets.u.persp.cameraZ, projSets.u.persp.targetZ,
											 projSets.u.persp.distance),
				projSets.u.persp.azimuth, projSets.u.persp.rollAngle));
			CameraElement.SetFocusDistance(FGeometryUtil::GetDistance3D(
				abs(projSets.u.persp.cameraZ - projSets.u.persp.targetZ), projSets.u.persp.distance));
			CameraElement.SetFocalLength(
				FGeometryUtil::GetCameraFocalLength(CameraElement.GetSensorWidth(), projSets.u.persp.viewCone));
		}
		else
		{
			// Get the matrix.
			CameraElement.SetTranslation(FGeometryUtil::GetTranslationVector(
				reinterpret_cast< const double(*)[4] >(projSets.u.axono.invtranmat.tmx)));
			CameraElement.SetRotation(FGeometryUtil::GetRotationQuat(
				reinterpret_cast< const double(*)[4] >(projSets.u.axono.invtranmat.tmx)));
			CameraElement.SetFocusDistance(10000);
			CameraElement.SetFocalLength(FGeometryUtil::GetCameraFocalLength(CameraElement.GetSensorWidth(), 45));
		}
	}
	else
	{
		UE_AC_DebugF("FSyncData::FCamera::InitWithCurrentView - APIEnv_Get3DProjectionSetsID returned error %d\n",
					 GSErr);
	}
}

void FSyncData::FCamera::InitWithCameraElement()
{
	API_Element camera;
	Zap(&camera);
	camera.header.guid = GSGuid2APIGuid(ElementId);
	UE_AC_TestGSError(ACAPI_Element_Get(&camera));

	IDatasmithCameraActorElement& CameraElement = static_cast< IDatasmithCameraActorElement& >(*ActorElement.Get());
	const TCHAR*				  cameraSetLabel = TEXT("Unamed camera");
	UE_AC_TestPtr(Parent);
	if (Parent->GetElement().IsValid())
	{
		cameraSetLabel = Parent->GetElement()->GetLabel();
	}
	CameraElement.SetLabel(*FString::Printf(TEXT("%s Camera %d"), cameraSetLabel, Index));

	const API_PerspPars& camPars = camera.camera.perspCam.persp;

	CameraElement.SetTranslation(FGeometryUtil::GetTranslationVector({camPars.pos.x, camPars.pos.y, camPars.cameraZ}));

	CameraElement.SetRotation(
		FGeometryUtil::GetRotationQuat(FGeometryUtil::GetPitchAngle(camPars.cameraZ, camPars.targetZ, camPars.distance),
									   camPars.azimuth * RADDEG, camPars.rollAngle * RADDEG));

	CameraElement.SetFocusDistance(
		FGeometryUtil::GetDistance3D(abs(camPars.cameraZ - camPars.targetZ), camPars.distance));
	CameraElement.SetFocalLength(
		FGeometryUtil::GetCameraFocalLength(CameraElement.GetSensorWidth(), camPars.viewCone * RADDEG));
}

void FSyncData::FLight::Process(FProcessInfo* IOProcessInfo)
{
	if (!ActorElement.IsValid())
	{
		if (Parameters.bIsAreaLight)
		{
			SetActorElement(FDatasmithSceneFactory::CreateAreaLight(GSStringToUE(ElementId.ToUniString())));
		}
		else if (Parameters.bIsParallelLight)
		{
			SetActorElement(FDatasmithSceneFactory::CreateDirectionalLight(GSStringToUE(ElementId.ToUniString())));
		}
		else
		{
			switch (LightData.LightType)
			{
				case ModelerAPI::Light::Type::DirectionLight:
					SetActorElement(
						FDatasmithSceneFactory::CreateDirectionalLight(GSStringToUE(ElementId.ToUniString())));
					break;
				case ModelerAPI::Light::Type::SpotLight:
					SetActorElement(FDatasmithSceneFactory::CreateSpotLight(GSStringToUE(ElementId.ToUniString())));
					break;
				case ModelerAPI::Light::Type::PointLight:
					SetActorElement(FDatasmithSceneFactory::CreatePointLight(GSStringToUE(ElementId.ToUniString())));
					break;
				default:
					throw std::runtime_error(
						Utf8StringFormat("FSyncData::FLight::Process - Invalid light type %d\n", LightData.LightType)
							.c_str());
			}
		}
	}
	if (IsModified())
	{
		IDatasmithLightActorElement& LightElement = static_cast< IDatasmithLightActorElement& >(*ActorElement.Get());

		const TCHAR* ParentLabel = TEXT("Unamed object");
		UE_AC_TestPtr(Parent);
		if (Parent->GetElement().IsValid())
		{
			ParentLabel = Parent->GetElement()->GetLabel();
		}
		LightElement.SetLabel(*FString::Printf(TEXT("%s - Light %d"), ParentLabel, Index));
		if (Parent->GetActorElement().IsValid())
		{
			LightElement.SetLayer(Parent->GetActorElement()->GetLayer());
		}

		LightElement.SetTranslation(LightData.Position);
		LightElement.SetRotation(LightData.Rotation);
		LightElement.SetIntensity(Parameters.Intensity);
		if (LightData.Color != FLinearColor::Black || Parameters.ColorComponentCount != 3)
		{
			LightElement.SetColor(LightData.Color);
		}
		else
		{
			LightElement.SetColor(ACRGBColorToUELinearColor(Parameters.GS_Color));
		}
		if (LightElement.IsA(EDatasmithElementType::PointLight))
		{
			IDatasmithPointLightElement& PointLightElement = static_cast< IDatasmithPointLightElement& >(LightElement);
			if (Parameters.bUsePhotometric)
			{
				PointLightElement.SetIntensityUnits(Parameters.Units);
			}
			else
			{
				PointLightElement.SetAttenuationRadius(
					float(Parameters.DetRadius * IOProcessInfo->SyncContext.ScaleLength));
			}
		}
		if (LightElement.IsA(EDatasmithElementType::SpotLight))
		{
			IDatasmithSpotLightElement& PointLightElement = static_cast< IDatasmithSpotLightElement& >(LightElement);
			float InnerConeAngleClamped = FGeometryUtil::Clamp(LightData.InnerConeAngle, 1.0f, 89.0f - 0.001f);
			PointLightElement.SetInnerConeAngle(InnerConeAngleClamped);
			float OuterConeAngleClamped =
				FGeometryUtil::Clamp(LightData.OuterConeAngle, InnerConeAngleClamped + 0.001f, 89.0f);
			PointLightElement.SetOuterConeAngle(OuterConeAngleClamped);
		}
		if (LightElement.IsA(EDatasmithElementType::AreaLight))
		{
			IDatasmithAreaLightElement& AreaLightElement = static_cast< IDatasmithAreaLightElement& >(LightElement);

			EDatasmithLightShape LightShape = EDatasmithLightShape::None;
			switch (Parameters.AreaShape)
			{
				case FLightGDLParameters::EC4dDetAreaShape::kRectangle:
				case FLightGDLParameters::EC4dDetAreaShape::kCube:
				case FLightGDLParameters::EC4dDetAreaShape::kLine:
					LightShape = EDatasmithLightShape::Rectangle;
					break;
				case FLightGDLParameters::EC4dDetAreaShape::kDisc:
					LightShape = EDatasmithLightShape::Disc;
					break;
				case FLightGDLParameters::EC4dDetAreaShape::kSphere:
				case FLightGDLParameters::EC4dDetAreaShape::kHemisphere:
					LightShape = EDatasmithLightShape::Sphere;
					break;
				case FLightGDLParameters::EC4dDetAreaShape::kCylinder:
				case FLightGDLParameters::EC4dDetAreaShape::kPerpendicularCylinder:
					LightShape = EDatasmithLightShape::Cylinder;
					break;
				default:
					LightShape = EDatasmithLightShape::Rectangle;
					break;
			}
			AreaLightElement.SetLightShape(LightShape);

			EDatasmithAreaLightType AreaLightType = EDatasmithAreaLightType::Point;
			switch (LightData.LightType)
			{
				case ModelerAPI::Light::Type::DirectionLight:
					AreaLightType = EDatasmithAreaLightType::Rect;
					break;
				case ModelerAPI::Light::Type::SpotLight:
					AreaLightType = EDatasmithAreaLightType::Spot;
					break;
				case ModelerAPI::Light::Type::PointLight:
					AreaLightType = EDatasmithAreaLightType::Point;
					break;
				default:
					AreaLightType = EDatasmithAreaLightType::Point;
					break;
			}
			AreaLightElement.SetLightType(AreaLightType);
			AreaLightElement.SetWidth(float(Parameters.AreaSize.y * IOProcessInfo->SyncContext.ScaleLength));
			AreaLightElement.SetLength(float(Parameters.AreaSize.x * IOProcessInfo->SyncContext.ScaleLength));
		}
		if (!Parameters.IESFileName.IsEmpty())
		{
			LightElement.SetUseIes(true);
			const FTexturesCache::FIESTexturesCacheElem& Texture =
				IOProcessInfo->SyncContext.GetTexturesCache().GetIESTexture(IOProcessInfo->SyncContext,
																			GSStringToUE(Parameters.IESFileName));
			LightElement.SetIesTexturePathName(*Texture.TexturePath);
			LightElement.SetUseIesBrightness(Parameters.bUsePhotometric);
			// LightElement.SetIesBrightnessScale(1.0);
			// LightElement.SetIesRotation(const FQuat& IesRotation);
		}
		else
		{
			LightElement.SetUseIes(false);
			LightElement.SetUseIesBrightness(false);
			LightElement.SetIesTexturePathName(TEXT(""));
		}
	}
}

FSyncData::FLight::FLightGDLParameters::FLightGDLParameters() {}

FSyncData::FLight::FLightGDLParameters::FLightGDLParameters(const API_Guid&		InLightGuid,
															const FLibPartInfo* InLibPartInfo)
{
	FAutoMemo AutoMemo(InLightGuid, APIMemoMask_AddPars);
	if (AutoMemo.GSErr == NoError)
	{
		if (AutoMemo.Memo.params) // Can be null
		{
			double value;
			if (GetParameter(AutoMemo.Memo.params, "gs_light_intensity", &value))
			{
				Intensity = value / 100.0 * 5000;
			}
			if (GetParameter(AutoMemo.Memo.params, "gs_color_red", &GS_Color.red))
			{
				++ColorComponentCount;
			}
			if (GetParameter(AutoMemo.Memo.params, "gs_color_green", &GS_Color.green))
			{
				++ColorComponentCount;
			}
			if (GetParameter(AutoMemo.Memo.params, "gs_color_blue", &GS_Color.blue))
			{
				++ColorComponentCount;
			}
			GetParameter(AutoMemo.Memo.params, "c4dPhoPhotometric", &bUsePhotometric);
			if (bUsePhotometric)
			{
				GS::UniString PhoUnit; //
				GetParameter(AutoMemo.Memo.params, "c4dPhoUnit", &PhoUnit);
				if (PhoUnit == "candela")
				{
					Units = EDatasmithLightUnits::Candelas;
					GetParameter(AutoMemo.Memo.params, "photoIntensityCandela", &Intensity);
				}
				if (PhoUnit == "lumen")
				{
					Units = EDatasmithLightUnits::Lumens;
					GetParameter(AutoMemo.Memo.params, "photoIntensityLumen", &Intensity);
				}
			}
			else
			{
				GetParameter(AutoMemo.Memo.params, "c4dDetRadius", &DetRadius);
			}
			GetParameter(AutoMemo.Memo.params, "c4dPhoIESFile", &IESFileName);

			bIsAreaLight = GetParameter(AutoMemo.Memo.params, "c4dDetAreaX", &AreaSize.x);
			bIsAreaLight |= GetParameter(AutoMemo.Memo.params, "c4dDetAreaY", &AreaSize.y);
			bIsAreaLight |= GetParameter(AutoMemo.Memo.params, "c4dDetAreaZ", &AreaSize.z);
			double TmpAreaShape;
			if (GetParameter(AutoMemo.Memo.params, "iC4dDetAreaShape", &TmpAreaShape))
			{
				if (TmpAreaShape >= kDisc && TmpAreaShape <= kPerpendicularCylinder)
				{
					AreaShape = (EC4dDetAreaShape) int(TmpAreaShape + 0.5);
				}
			}
			GetParameter(AutoMemo.Memo.params, "rotAngleX", &WindowLightAngle);
			GetParameter(AutoMemo.Memo.params, "angleSunAzimuth", &SunAzimuthAngle);
			GetParameter(AutoMemo.Memo.params, "angleSunAltitude", &SunAltitudeAngle);

			if (InLibPartInfo != nullptr)
			{
				static GS::Guid ParallelLightMainGuid("FF603AFE-10AE-466E-A360-87924FA7E24A");
				static GS::Guid SunLightMainGuid("0A68517B-9FCA-483A-93A7-2E85FE6BDBF9");
				bIsParallelLight =
					InLibPartInfo->Guid.Main == ParallelLightMainGuid || InLibPartInfo->Guid.Main == SunLightMainGuid;
			}

			GetParameter(AutoMemo.Memo.params, "bGenShadow", &bGenShadow); // Currently not available in Datasmith.
		}
	}
	else
	{
		UE_AC_DebugF(
			"FSyncData::FLight::FLightGDLParameters::FLightGDLParameters - Error=%d when getting element memo\n",
			AutoMemo.GSErr);
	}
	if (ColorComponentCount != 0 && ColorComponentCount != 3)
	{
		UE_AC_DebugF("FSyncData::FLight::FLightGDLParameters::FLightGDLParameters - ColorComponentCount is %u\n",
					 ColorComponentCount);
	}
};

bool FSyncData::FLight::FLightGDLParameters::operator!=(const FLightGDLParameters& InOther) const
{
	return GS_Color != InOther.GS_Color || ColorComponentCount != InOther.ColorComponentCount ||
		   Intensity != InOther.Intensity || bUsePhotometric != InOther.bUsePhotometric || Units != InOther.Units ||
		   DetRadius != InOther.DetRadius || IESFileName != InOther.IESFileName || AreaShape != InOther.AreaShape ||
		   AreaSize != InOther.AreaSize || WindowLightAngle != InOther.WindowLightAngle ||
		   SunAzimuthAngle != InOther.SunAzimuthAngle || SunAltitudeAngle != InOther.SunAltitudeAngle ||
		   bIsParallelLight != InOther.bIsParallelLight || bGenShadow != InOther.bGenShadow;
}

FSyncData::FLight::FLightData::FLightData() {}

FSyncData::FLight::FLightData::FLightData(const ModelerAPI::Light& InLight)
{
	LightType = InLight.GetType();

	InnerConeAngle = float(InLight.GetFalloffAngle1() * 180.0f / PI);
	OuterConeAngle = float(InLight.GetFalloffAngle2() * 180.0f / PI);
	Color = ACRGBColorToUELinearColor(InLight.GetColor());

	Position = FGeometryUtil::GetTranslationVector(InLight.GetPosition());
	Rotation = FGeometryUtil::GetRotationQuat(InLight.GetDirection());
}

bool FSyncData::FLight::FLightData::operator!=(const FLightData& InOther) const
{
	return LightType != InOther.LightType || InnerConeAngle != InOther.InnerConeAngle ||
		   OuterConeAngle != InOther.OuterConeAngle || Color != InOther.Color || Position != InOther.Position ||
		   Rotation != InOther.Rotation;
}

const GS::Guid FSyncData::FHotLinksRoot::HotLinksRootGUID("C4BFD876-FDE9-4CCF-8899-12023968DC0D");

void FSyncData::FHotLinksRoot::Process(FProcessInfo* /* IOProcessInfo */)
{
	if (!ActorElement.IsValid())
	{
		SetActorElement(FDatasmithSceneFactory::CreateActor(GSStringToUE(ElementId.ToUniString())));
		ActorElement->SetLabel(TEXT("Hot Links"));
	}
}

void FSyncData::FHotLinkNode::Process(FProcessInfo* IOProcessInfo)
{
	if (!ActorElement.IsValid())
	{
		SetActorElement(FDatasmithSceneFactory::CreateActor(GSStringToUE(ElementId.ToUniString())));

#if AC_VERSION < 26
		API_HotlinkNode hotlinkNode;
		Zap(&hotlinkNode);
#else
		API_HotlinkNode hotlinkNode = {0};
#endif
		hotlinkNode.guid = GSGuid2APIGuid(ElementId);
		GSErrCode err = ACAPI_Database(APIDb_GetHotlinkNodeID, &hotlinkNode);
		if (err == NoError)
		{
			GS::UniString Label = hotlinkNode.name;
			if (hotlinkNode.refFloorName[0] != '\0')
			{
				Label += " Floor ";
				Label += hotlinkNode.refFloorName;
			}
			ActorElement->SetLabel(GSStringToUE(Label));

			FMetaData MyMetaData(ActorElement);

			const TCHAR* HotLinkType = TEXT("Unknown");
			if (hotlinkNode.type == APIHotlink_Module)
			{
				HotLinkType = TEXT("Module");
			}
			else if (hotlinkNode.type == APIHotlink_XRef)
			{
				HotLinkType = TEXT("XRef");
			}
			MyMetaData.AddStringProperty(TEXT("HotLinkType"), HotLinkType);

			if (hotlinkNode.sourceLocation != nullptr)
			{
				MyMetaData.AddStringProperty(TEXT("HotLinkLocation"), hotlinkNode.sourceLocation->ToDisplayText());
			}
			if (hotlinkNode.serverSourceLocation != nullptr)
			{
				MyMetaData.AddStringProperty(TEXT("HotLinkSharedLocation"),
											 hotlinkNode.serverSourceLocation->ToDisplayText());
			}
			MyMetaData.AddStringProperty(TEXT("StoryRangeType"), hotlinkNode.storyRangeType == APIHotlink_SingleStory
																	 ? TEXT("Single")
																	 : TEXT("All"));

			const TCHAR* SourceLinkType = TEXT("Unknown");
			if (hotlinkNode.sourceType == APIHotlink_LocalFile)
			{
				SourceLinkType = TEXT("LocalFile");
			}
			else if (hotlinkNode.sourceType == APIHotlink_TWFS)
			{
				SourceLinkType = TEXT("TWFS");
			}
			else if (hotlinkNode.sourceType == APIHotlink_TWProject)
			{
				SourceLinkType = TEXT("TWProject");
			}
			MyMetaData.AddStringProperty(TEXT("StorySourceType"), SourceLinkType);

			ReplaceMetaData(IOProcessInfo->SyncContext.GetScene(), MyMetaData.GetMetaData());

#if AC_VERSION < 25
			// No need to free API_HotlinkNode allocated members starting with ArchiCAD 25
			delete hotlinkNode.sourceLocation;
			delete hotlinkNode.serverSourceLocation;
			BMKillPtr(&hotlinkNode.userData.data);
#endif
		}
		else
		{
			UE_AC_DebugF("FSyncData::FHotLinkInstance::Process - ACAPI_Element_Get - Error=%d\n", err);
		}
	}
}

FSyncData::FHotLinkInstance::FHotLinkInstance(const GS::Guid& InGuid, FSyncDatabase* IOSyncDatabase)
	: FSyncData::FActor(InGuid)
{
	Transformation = {};
	Transformation.tmx[0] = 1.0;
	Transformation.tmx[5] = 1.0;
	Transformation.tmx[10] = 1.0;

	API_Element hotlinkElem;
	Zap(&hotlinkElem);
	GET_HEADER_TYPEID(hotlinkElem.header) = API_HotlinkID;
	hotlinkElem.header.guid = GSGuid2APIGuid(ElementId);
	GSErrCode err = ACAPI_Element_Get(&hotlinkElem);
	if (err == NoError)
	{
		// Parent is a hot link node
		FSyncData*& RefHotLinkNode = IOSyncDatabase->GetSyncData(APIGuid2GSGuid(hotlinkElem.hotlink.hotlinkNodeGuid));
		FSyncData*	HotLinkNode = RefHotLinkNode;
		if (HotLinkNode == nullptr)
		{
			HotLinkNode = new FSyncData::FHotLinkNode(APIGuid2GSGuid(hotlinkElem.hotlink.hotlinkNodeGuid));
			RefHotLinkNode = HotLinkNode;
			if (!HotLinkNode->HasParent())
			{
				FSyncData*& HotLinksRoot = IOSyncDatabase->GetSyncData(FSyncData::FHotLinksRoot::HotLinksRootGUID);
				if (HotLinksRoot == nullptr)
				{
					HotLinksRoot = new FSyncData::FHotLinksRoot();
					HotLinksRoot->SetParent(&IOSyncDatabase->GetSceneSyncData());
				}
				HotLinkNode->SetParent(HotLinksRoot);
			}
		}
		SetParent(HotLinkNode);
	}
	else
	{
		UE_AC_DebugF("FSyncData::FHotLinkInstance::FHotLinkInstance - ACAPI_Element_Get - Error=%d\n", err);
	}
}

void FSyncData::FHotLinkInstance::Process(FProcessInfo* IOProcessInfo)
{
	if (!ActorElement.IsValid())
	{
		SetActorElement(FDatasmithSceneFactory::CreateActor(GSStringToUE(ElementId.ToUniString())));

		API_Element hotlinkElem;
		Zap(&hotlinkElem);
		GET_HEADER_TYPEID(hotlinkElem.header) = API_HotlinkID;
		hotlinkElem.header.guid = GSGuid2APIGuid(ElementId);
		GSErrCode err = ACAPI_Element_Get(&hotlinkElem);
		if (err == NoError)
		{
			const TCHAR* HotLinkType = TEXT("Unknown");
			if (hotlinkElem.hotlink.type == APIHotlink_Module)
			{
				HotLinkType = TEXT("Module");
			}
			else if (hotlinkElem.hotlink.type == APIHotlink_XRef)
			{
				HotLinkType = TEXT("XRef");
			}
			const TCHAR* ParentLabel = TEXT("Unamed object");
			UE_AC_Assert(Parent != nullptr && Parent->ElementId == hotlinkElem.hotlink.hotlinkNodeGuid);
			if (Parent->GetElement().IsValid())
			{
				ParentLabel = Parent->GetElement()->GetLabel();
			}
			ActorElement->SetLabel(*FString::Printf(TEXT("%s - %s Instance %llu"), ParentLabel, HotLinkType,
													IOProcessInfo->GetCurrentIndex()));

			Transformation = hotlinkElem.hotlink.transformation;

			FMetaData MyMetaData(ActorElement);

			MyMetaData.AddStringProperty(TEXT("HotLinkType"), HotLinkType);

			ReplaceMetaData(IOProcessInfo->SyncContext.GetScene(), MyMetaData.GetMetaData());

			// What do we do with hotlinkElem.hotlink.hotlinkGroupGuid ?
		}
		else
		{
			UE_AC_DebugF("FSyncData::FHotLinkInstance::Process - ACAPI_Element_Get - Error=%d\n", err);
		}
	}
}

// Start the process with this root observer
void FSyncData::FInterator::Start(FSyncData* Root)
{
	Stop();
	Stack.Add({Root, 0});
	ProcessedCount = 0;
	ProcessTime = 0.0;
}

// Stop processing
void FSyncData::FInterator::Stop()
{
	// Stop process
	Stack.Empty();
}

FSyncData::FInterator::EProcessControl FSyncData::FInterator::ProcessUntil(double TimeSliceEnd)
{
	double			startTime = FTimeStat::RealTimeClock();
	EProcessControl ProcessControl = kContinue;
	while (FTimeStat::RealTimeClock() < TimeSliceEnd && ProcessControl == kContinue)
	{
		FSyncData* Current = Next();
		ProcessControl = Process(Current);
	}
	if (ProcessControl == kInterrupted)
	{
		Stop();
	}
	ProcessTime += FTimeStat::RealTimeClock() - startTime;
	return ProcessControl;
}

// Return the next FSyncData
FSyncData* FSyncData::FInterator::Next()
{
	FSyncData* Current = nullptr;
	while (Stack.Num() != 0 && Current == nullptr)
	{
		FSyncData* Parent = Stack.Top().Parent;

		// Start with root
		if (ProcessedCount == 0)
		{
			ProcessedCount = 1;
			return Parent;
		}

		// Traverse all tree
		FChildsArray::SizeType ChildIndex = Stack.Top().ChildIndex;
		if (ChildIndex < Parent->Childs.Num())
		{
			Current = Parent->Childs[ChildIndex++];
			++ProcessedCount;
			Stack.Top().ChildIndex = ChildIndex;
			Stack.Add({Current, 0});
		}
		else
		{
			Stack.Pop(EAllowShrinking::No);
		}
	}
	return Current;
}

// Return the index of the current.
FSyncData::FChildsArray::SizeType FSyncData::FInterator::GetCurrentIndex()
{
	return Stack.Num() > 1 ? Stack[Stack.Num() - 2].ChildIndex : 0;
}

// Start the process with this root observer
void FSyncData::FProcessMetadata::Start(FSyncData* Root)
{
	FSyncData::FInterator::Start(Root);
	MetadataProcessedCount = 0;
	bMetadataUpdated = false;
}

// Call ProcessMetaData for the sync data
FSyncData::FInterator::EProcessControl FSyncData::FProcessMetadata::Process(FSyncData* InCurrent)
{
	if (InCurrent == nullptr)
	{
		return FInterator::kDone;
	}

	if (InCurrent->NeedTagsAndMetaDataUpdate())
	{
		++MetadataProcessedCount;
		bMetadataUpdated |= InCurrent->ProcessMetaData(Synchronizer->GetSyncDatabase());
	}

	return FInterator::kContinue;
}

// Constructor
FSyncData::FAttachObservers::FAttachObservers() {}

// Start the process with this root observer
void FSyncData::FAttachObservers::Start(FSyncData* Root)
{
	FSyncData::FInterator::Start(Root);

#if ATTACH_ONSERVER_STAT
	AttachObserverProcessTimeStart.ReStart();
	AttachObserverProcessTimeEnd = AttachObserverProcessTimeStart;
	AttachObserverStartTime = FTimeStat::RealTimeClock();
	AttachObserverTime = 0.0;
	GetHeaderTime = 0.0;
	AttachCount = 0;
#endif
}

FSyncData::FInterator::EProcessControl FSyncData::FAttachObservers::Process(FSyncData* InCurrent)
{
	if (InCurrent == nullptr)
	{
		return FInterator::kDone;
	}

	if (InCurrent->AttachObserver(this))
	{
		return FInterator::kInterrupted;
	}

	return FInterator::kContinue;
}

// Process attachment until done or until time slice finish
bool FSyncData::FAttachObservers::ProcessAttachUntil(double TimeSliceEnd)
{
#if TRACE_ATTACH_OBSERVERS
	int	   NbProcessedStart = GetProcessedCount();
	double startTime = FTimeStat::RealTimeClock();
#endif

	FInterator::EProcessControl ProcessControl = ProcessUntil(TimeSliceEnd);

#if TRACE_ATTACH_OBSERVERS
	if (GetProcessedCount() - NbProcessedStart != 0)
	{
		UE_AC_TraceF("FSyncData::FAttachObservers::ProcessAttachUntil - Nb Processed = %d (Start=%lf, End=%lf)\n",
					 GetProcessedCount() - NbProcessedStart, startTime, FTimeStat::RealTimeClock());
	}
#endif

#if ATTACH_ONSERVER_STAT
	if (ProcessControl == FInterator::kInterrupted || ProcessControl == FInterator::kDone)
	{
		PrintStat();
	}
#endif

	return ProcessControl == FInterator::kInterrupted; // If interrupted then we request an update
}

#if ATTACH_ONSERVER_STAT
void FSyncData::FAttachObservers::CumulateStats(const FTimeStat& SlotStart, double AfterAttachObserver)
{
	++AttachCount;
	AttachObserverTime += AfterAttachObserver - SlotStart.GetCpuTime();
	GetHeaderTime += FTimeStat::CpuTimeClock() - AfterAttachObserver;
	AttachObserverProcessTimeEnd.AddDiff(SlotStart);
}

// Log attach observer statistics
void FSyncData::FAttachObservers::PrintStat()
{
	if (AttachCount != 0)
	{
		double AttachObserversTime = FTimeStat::RealTimeClock() - AttachObserverStartTime;
		if (AttachObserversTime < 0.0)
		{
			AttachObserversTime += AttachObserversTime + 24 * 60 * 60;
		}
		UE_AC_ReportF("TraceObserverStat - Count = %d TotalTime=%.1lfs (AttachObserver=%.1lfns, GetHeader=%.1lfns)\n",
					  AttachCount, AttachObserversTime, AttachObserverTime / AttachCount * 1000000.0,
					  GetHeaderTime / AttachCount * 1000000.0);
		AttachObserverProcessTimeEnd.PrintDiff("Attach Observers", AttachObserverProcessTimeStart);
		AttachCount = 0;
	}
}
#endif

END_NAMESPACE_UE_AC
