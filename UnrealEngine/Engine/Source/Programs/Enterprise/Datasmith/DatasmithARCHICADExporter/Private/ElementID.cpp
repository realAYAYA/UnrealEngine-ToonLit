// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElementID.h"

#include "GSProcessControl.hpp"
#include "Transformation.hpp"

#include <stdexcept>

BEGIN_NAMESPACE_UE_AC

template <>
FAssValueName::SAssValueName TAssEnumName< ModelerAPI::Element::Type >::AssEnumName[] = {
	EnumName(ModelerAPI::Element, UndefinedElement),
	EnumName(ModelerAPI::Element, WallElement),
	EnumName(ModelerAPI::Element, SlabElement),
	EnumName(ModelerAPI::Element, RoofElement),
	EnumName(ModelerAPI::Element, CurtainWallElement),
	EnumName(ModelerAPI::Element, CWFrameElement),
	EnumName(ModelerAPI::Element, CWPanelElement),
	EnumName(ModelerAPI::Element, CWJunctionElement),
	EnumName(ModelerAPI::Element, CWAccessoryElement),
	EnumName(ModelerAPI::Element, CWSegmentElement),
	EnumName(ModelerAPI::Element, ShellElement),
	EnumName(ModelerAPI::Element, SkylightElement),
	EnumName(ModelerAPI::Element, FreeshapeElement),
	EnumName(ModelerAPI::Element, DoorElement),
	EnumName(ModelerAPI::Element, WindowElement),
	EnumName(ModelerAPI::Element, ObjectElement),
	EnumName(ModelerAPI::Element, LightElement),
	EnumName(ModelerAPI::Element, ColumnElement),
	EnumName(ModelerAPI::Element, MeshElement),
	EnumName(ModelerAPI::Element, BeamElement),
	EnumName(ModelerAPI::Element, RoomElement),
#if AC_VERSION >= 21
	EnumName(ModelerAPI::Element, StairElement),
	EnumName(ModelerAPI::Element, RiserElement),
	EnumName(ModelerAPI::Element, TreadElement),
	EnumName(ModelerAPI::Element, StairStructureElement),
	EnumName(ModelerAPI::Element, RailingElement),
	EnumName(ModelerAPI::Element, ToprailElement),
	EnumName(ModelerAPI::Element, HandrailElement),
	EnumName(ModelerAPI::Element, RailElement),
	EnumName(ModelerAPI::Element, RailingPostElement),
	EnumName(ModelerAPI::Element, InnerPostElement),
	EnumName(ModelerAPI::Element, BalusterElement),
	EnumName(ModelerAPI::Element, RailingPanelElement),
	EnumName(ModelerAPI::Element, RailingSegmentElement),
	EnumName(ModelerAPI::Element, RailingNodeElement),
	EnumName(ModelerAPI::Element, RailPatternElement),
	EnumName(ModelerAPI::Element, InnerTopRailEndElement),
	EnumName(ModelerAPI::Element, InnerHandRailEndElement),
	EnumName(ModelerAPI::Element, RailFinishingObjectElement),
	EnumName(ModelerAPI::Element, TopRailConnectionElement),
	EnumName(ModelerAPI::Element, HandRailConnectionElement),
	EnumName(ModelerAPI::Element, RailConnectionElement),
	EnumName(ModelerAPI::Element, RailEndElement),
	EnumName(ModelerAPI::Element, BalusterSetElement),
#endif
#if AC_VERSION >= 23
	EnumName(ModelerAPI::Element, AnalyticalSupportElement),
	EnumName(ModelerAPI::Element, AnalyticalLinkElement),
	EnumName(ModelerAPI::Element, Opening),
	EnumName(ModelerAPI::Element, Openingframeinfill),
	EnumName(ModelerAPI::Element, Openingpatchinfill),
	EnumName(ModelerAPI::Element, ColumnSegmentElement),
	EnumName(ModelerAPI::Element, BeamSegmentElement),
#endif
	EnumName(ModelerAPI::Element, OtherElement),
	EnumEnd(-1)};

// Contructor
FElementID::FElementID(const FSyncContext& InSyncContext)
	: SyncContext(InSyncContext)
	, Index3D(0)
	, SyncData(nullptr)
	, MeshClass(nullptr)
	, LibPartInfo(nullptr)
	, bLibPartInfoFetched(false)
{
}

// Initialize with 3D element
void FElementID::InitElement(GS::Int32 InIndex3d)
{
	Index3D = InIndex3d;
	SyncContext.GetModel().GetElement(Index3D, &Element3D);
	APIElementHeader.guid = APINULLGuid;
	MeshClass = nullptr;
	LibPartInfo = nullptr;
	bLibPartInfoFetched = false;
	ElementName.clear();
}

// Initialize with sync data
void FElementID::InitElement(FSyncData* IOSyncdata)
{
	UE_AC_TestPtr(IOSyncdata);
	SyncData = IOSyncdata;
	Index3D = IOSyncdata->GetIndex3D();
	if (Index3D > 0)
	{
		GS::Int32 ElementCount = SyncContext.GetModel().GetElementCount();
		if (ensureMsgf(ElementCount >= Index3D, TEXT("Element index became outside of elements range of the model")))
		{
			SyncContext.GetModel().GetElement(Index3D, &Element3D);
		}
	}
	MeshClass = nullptr;
	LibPartInfo = nullptr;
	bLibPartInfoFetched = false;
}

// Initialize element header from 3D element
bool FElementID::InitHeader()
{
	if (IsInvalid())
	{
		throw std::runtime_error(
			Utf8StringFormat("FElementID::InitHeader - Invalid element for index=%d\n", Index3D).c_str());
	}
	Zap(&APIElementHeader);
	APIElementHeader.guid = GSGuid2APIGuid(Element3D.GetElemGuid());
	GSErrCode GSErr = ACAPI_Element_GetHeader(&APIElementHeader, 0);
	if (GSErr != NoError)
	{
		utf8_string ErrorName(GetErrorName(GSErr));
		utf8_string TypeName(GetTypeName());
		UE_AC_DebugF("Error \"%s\" with element %d {%s} Type=%s\n", ErrorName.c_str(), Index3D,
					 Element3D.GetElemGuid().ToUniString().ToUtf8(), TypeName.c_str());
		if (GSErr != APIERR_BADID)
		{
			UE_AC::ThrowGSError(GSErr, __FILE__, __LINE__);
		}
		return false;
	}
	return true;
}

FMeshClass* FElementID::GetMeshClass()
{
	if (MeshClass == nullptr && Index3D != 0)
	{
		ModelerAPI::BaseElemId			   BaseElemId;
		GS::NonInterruptibleProcessControl processControl;
		Element3D.GetBaseElemId(&BaseElemId, processControl, ModelerAPI::Element::EdgeColorInBaseElemId::NotIncluded,
								ModelerAPI::Element::PolygonAndFaceTextureMappingInBaseElemId::NotIncluded,
								ModelerAPI::Element::BodyTextureMappingInBaseElemId::NotIncluded,
								ModelerAPI::Element::EliminationInfoInBaseElemId::NotIncluded);
#if AC_VERSION < 24
		GS::HashValue OldHashValue = BaseElemId;
		GS::ULong	  HashValue = OldHashValue.hashValue;
#else
		GS::ULong HashValue = BaseElemId.GenerateHashValue();
#endif
		MeshClass = SyncContext.GetSyncDatabase().GetMeshClass(HashValue);
		bool bTransformed = (Element3D.GetElemLocalToWorldTransformation().status & TR_IDENT) != 0;
		if (MeshClass == nullptr)
		{
			TUniquePtr< FMeshClass > NewInstance = MakeUnique< FMeshClass >();
			MeshClass = NewInstance.Get();
			MeshClass->Hash = HashValue;
			MeshClass->ElementType = Element3D.GetType();
			MeshClass->TransformCount = bTransformed ? 0 : 1;
			SyncContext.GetSyncDatabase().AddInstance(HashValue, std::move(NewInstance));
			
			UE_AC_VerboseF("FElementID::GetMeshClass - First instance %u {%s}\n", MeshClass->Hash,
						   APIGuidToString(APIElementHeader.guid).ToUtf8());
		}
		else
		{
			if (MeshClass->ElementType != Element3D.GetType())
			{
				if (MeshClass->ElementType == ModelerAPI::Element::Type::UndefinedElement)
				{
					MeshClass->ElementType = Element3D.GetType();
				}
				else
				{
					UE_AC_DebugF("FElementID::GetMeshClass - MeshClass Hash %u collision Type %s != %s\n",
								 MeshClass->Hash, GetTypeName(MeshClass->ElementType), GetTypeName());
					MeshClass->ElementType = Element3D.GetType();
					MeshClass->MeshElement.Reset();
					MeshClass->bMeshElementInitialized = false;
				}
			}
			++MeshClass->InstancesCount;
			MeshClass->TransformCount += bTransformed ? 0 : 1;
			UE_AC_VerboseF("FElementID::GetMeshClass - Reuse MeshClass %u {%s}\n", MeshClass->Hash,
						   APIGuidToString(APIElementHeader.guid).ToUtf8());
		}
	}
	return MeshClass;
}

// If this element is related to a lib part ?
const FLibPartInfo* FElementID::GetLibPartInfo()
{
	if (bLibPartInfoFetched == false)
	{
		// Get the lib part from it's UnId
		FGSUnID::Buffer lpfUnID = {0};
		GSErrCode		GSErr = ACAPI_Goodies(APIAny_GetElemLibPartUnIdID, &APIElementHeader, lpfUnID);
		if (GSErr == NoError)
		{
			LibPartInfo = SyncContext.GetSyncDatabase().GetLibPartInfo(lpfUnID);
		}
		else if (GSErr != APIERR_BADID)
		{
			UE_AC_DebugF("FElementID::InitLibPartInfo - APIAny_GetElemLibPartUnIdID return error %s\n",
						 GetErrorName(GSErr));
		}
		bLibPartInfoFetched = true;
	}

	return LibPartInfo;
}

// Get the name of the element (For debugging trace)
const utf8_t* FElementID::GetElementName()
{
	if (ElementName.size() == 0)
	{
		GS::UniString InfoStringID;
		GSErrCode GSErr = ACAPI_Database(APIDb_GetElementInfoStringID, (void*)&APIElementHeader.guid, &InfoStringID);
		if (GSErr == NoError)
		{
			ElementName =
				Utf8StringFormat("{%s}:\"%s\"", APIGuidToString(APIElementHeader.guid).ToUtf8(), InfoStringID.ToUtf8());
		}
		else
		{
			ElementName =
				Utf8StringFormat("{%s}:Error=%s", APIGuidToString(APIElementHeader.guid).ToUtf8(), GetErrorName(GSErr));
		}
	}
	return ElementName.c_str();
}

// Connect childs of this parent
void FElementID::CollectDependantElementsType(API_ElemTypeID TypeID) const
{
	GS::Array< API_Guid > ConnectedElements;
	UE_AC_TestGSError(ACAPI_Element_GetConnectedElements(APIElementHeader.guid, TypeID, &ConnectedElements));
	for (USize i = 0; i < ConnectedElements.GetSize(); ++i)
	{
		FSyncData*& ChildSyncData = SyncContext.GetSyncDatabase().GetSyncData(APIGuid2GSGuid(ConnectedElements[i]));
		if (ChildSyncData == nullptr)
		{
			ChildSyncData = new FSyncData::FElement(APIGuid2GSGuid(ConnectedElements[i]), SyncContext);
		}
		ChildSyncData->SetParent(SyncData);
		UE_AC_VerboseF("FElementID::ConnectedElements %u %s -> %s\n", i, APIGuidToString(ConnectedElements[i]).ToUtf8(),
					   SyncData->GetId().ToUniString().ToUtf8());
	}
}

// Connect to parent or childs
void FElementID::HandleDepedencies() const
{
	if (GET_HEADER_TYPEID(APIElementHeader) == API_WallID)
	{
		CollectDependantElementsType(API_WindowID);
		CollectDependantElementsType(API_DoorID);
	}
	else if (GET_HEADER_TYPEID(APIElementHeader) == API_RoofID || GET_HEADER_TYPEID(APIElementHeader) == API_ShellID)
	{
		CollectDependantElementsType(API_SkylightID);
	}
	else if (GET_HEADER_TYPEID(APIElementHeader) == API_WindowID || GET_HEADER_TYPEID(APIElementHeader) == API_DoorID ||
		GET_HEADER_TYPEID(APIElementHeader) == API_SkylightID)
	{
		// Do nothing
	}
	else
	{
		GS::Guid				  OwnerElemGuid = APIGuid2GSGuid(APIElementHeader.guid);
		API_Guid				  OwnerElemApiGuid = APIElementHeader.guid;
		API_HierarchicalElemType  HierarchicalElemType = API_SingleElem;
		API_HierarchicalOwnerType HierarchicalOwnerType = API_RootHierarchicalOwner;
		GSErrCode GSErr = ACAPI_Goodies(APIAny_GetHierarchicalElementOwnerID, &OwnerElemGuid, &HierarchicalOwnerType,
										&HierarchicalElemType, &OwnerElemApiGuid);
		if (GSErr != NoError || OwnerElemApiGuid == APINULLGuid)
		{
			return;
		}

		if (HierarchicalElemType == API_ChildElemInMultipleElem)
		{
			FSyncData*& Parent = SyncContext.GetSyncDatabase().GetSyncData(APIGuid2GSGuid(OwnerElemApiGuid));
			if (Parent == nullptr)
			{
				Parent = new FSyncData::FElement(APIGuid2GSGuid(OwnerElemApiGuid), SyncContext);
			}
			SyncData->SetParent(Parent);
			SyncData->SetIsAComponent();
			Parent->SetDefaultParent(*this);
			UE_AC_VerboseF("FElementID::MakeConnections Child %s -> Parent %s\n",
						   SyncData->GetId().ToUniString().ToUtf8(), APIGuidToString(OwnerElemApiGuid).ToUtf8());
		}
		else
		{
		}
	}
}

END_NAMESPACE_UE_AC
