// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElementTools.h"
#include "TAssValueName.h"

BEGIN_NAMESPACE_UE_AC

// clang-format off

template <>
FAssValueName::SAssValueName TAssEnumName< API_ElemTypeID >::AssEnumName[] = {
	{ API_ZombieElemID, 				"Zombie" },

	{ API_WallID, 						"Wall" },
	{ API_ColumnID, 					"Column" },
	{ API_BeamID, 						"Beam" },
	{ API_WindowID, 					"Window" },
	{ API_DoorID, 						"Door" },
	{ API_ObjectID, 					"Object" },
	{ API_LampID, 						"Lamp" },
	{ API_SlabID, 						"Slab" },
	{ API_RoofID, 						"Roof" },
	{ API_MeshID, 						"Mesh" },

	{ API_DimensionID, 					"Dimension" },
	{ API_RadialDimensionID, 			"RadialDimension" },
	{ API_LevelDimensionID, 			"LevelDimension" },
	{ API_AngleDimensionID, 			"AngleDimension" },

	{ API_TextID, 						"Text" },
	{ API_LabelID, 						"Label" },
	{ API_ZoneID, 						"Zone" },

	{ API_HatchID, 						"Hatch" },
	{ API_LineID, 						"Line" },
	{ API_PolyLineID, 					"PolyLine" },
	{ API_ArcID, 						"Arc" },
	{ API_CircleID, 					"Circle" },
	{ API_SplineID, 					"Spline" },
	{ API_HotspotID, 					"Hotspot" },

	{ API_CutPlaneID, 					"CutPlane" },
	{ API_CameraID, 					"Camera" },
	{ API_CamSetID, 					"CamSet" },

	{ API_GroupID, 						"Group" },
	{ API_SectElemID, 					"SectElem" },

	{ API_DrawingID, 					"Drawing" },
	{ API_PictureID, 					"Picture" },
	{ API_DetailID, 					"Detail" },
	{ API_ElevationID, 					"Elevation" },
	{ API_InteriorElevationID, 			"InteriorElevation" },
	{ API_WorksheetID, 					"Worksheet" },

	{ API_HotlinkID, 					"Hotlink" },

	{ API_CurtainWallID, 				"CurtainWall" },
	{ API_CurtainWallSegmentID, 		"CurtainWallSegment" },
	{ API_CurtainWallFrameID, 			"CurtainWallFrame" },
	{ API_CurtainWallPanelID, 			"CurtainWallPanel" },
	{ API_CurtainWallJunctionID, 		"CurtainWallJunction" },
	{ API_CurtainWallAccessoryID, 		"CurtainWallAccessory" },
	{ API_ShellID, 						"Shell" },
	{ API_SkylightID, 					"Skylight" },
	{ API_MorphID, 						"Morph" },
	{ API_ChangeMarkerID, 				"ChangeMarker" },
	{ API_StairID, 						"Stair" },
	{ API_RiserID, 						"Riser" },
	{ API_TreadID, 						"Tread" },
	{ API_StairStructureID, 			"StairStructure" },
	{ API_RailingID, 					"Railing" },
	{ API_RailingToprailID, 			"RailingToprail" },
	{ API_RailingHandrailID, 			"RailingHandrail" },
	{ API_RailingRailID, 				"RailingRail" },
	{ API_RailingPostID, 				"RailingPost" },
	{ API_RailingInnerPostID, 			"RailingInnerPost" },
	{ API_RailingBalusterID, 			"RailingBaluster" },
	{ API_RailingPanelID, 				"RailingPanel" },
	{ API_RailingSegmentID, 			"RailingSegment" },
	{ API_RailingNodeID, 				"RailingNode" },
	{ API_RailingBalusterSetID, 		"RailingBalusterSet" },
	{ API_RailingPatternID, 			"RailingPattern" },
	{ API_RailingToprailEndID, 			"RailingToprailEnd" },
	{ API_RailingHandrailEndID, 		"RailingHandrailEnd" },
	{ API_RailingRailEndID, 			"RailingRailEnd" },
	{ API_RailingToprailConnectionID, 	"RailingToprailConnection" },
	{ API_RailingHandrailConnectionID, 	"RailingHandrailConnection" },
	{ API_RailingRailConnectionID, 		"RailingRailConnection" },
	{ API_RailingEndFinishID, 			"RailingEndFinish" },

#if AC_VERSION < 26
	{ API_AnalyticalSupportID, 			"AnalyticalSupport" },
	{ API_AnalyticalLinkID, 			"AnalyticalLink" },
#endif

	{ API_BeamSegmentID, 				"BeamSegment" },
	{ API_ColumnSegmentID, 				"ColumnSegment" },
	{ API_OpeningID, 					"Opening" },
#if AC_VERSION > 24 && AC_VERSION < 26
	{ API_AnalyticalPointLoadID, 		"AnalyticalPointLoad" },
	{ API_AnalyticalEdgeLoadID, 		"AnalyticalEdgeLoad" },
	{ API_AnalyticalSurfaceLoadID, 		"AnalyticalSurfaceLoad" },
#endif

	{ -1, nullptr }
};

template <>
FAssValueName::SAssValueName TAssEnumName< API_ElemVariationID >::AssEnumName[] = {
	ValueName(APIVarId_Generic),

#if AC_VERSION < 25
	ValueName(APIVarId_LabelVirtSy),
	ValueName(APIVarId_LabelCeil),
	ValueName(APIVarId_LabelRoof),
	ValueName(APIVarId_LabelShell),
	ValueName(APIVarId_LabelMesh),
	ValueName(APIVarId_LabelHatch),
	ValueName(APIVarId_LabelCurtainWall),
	ValueName(APIVarId_LabelCWPanel),
	ValueName(APIVarId_LabelCWFrame),
	ValueName(APIVarId_LabelWall2),
	ValueName(APIVarId_LabelColumn),
	ValueName(APIVarId_LabelBeam),
	ValueName(APIVarId_LabelWind),
	ValueName(APIVarId_LabelDoor),
	ValueName(APIVarId_LabelSkylight),
	ValueName(APIVarId_LabelSymb),
	ValueName(APIVarId_LabelLight),
	ValueName(APIVarId_LabelMorph),
	ValueName(APIVarId_LabelCWAccessory),
	ValueName(APIVarId_LabelCWJunction),
#endif

	{ APIVarId_SymbStair, 		"SymbStair" },
	{ APIVarId_WallEnd, 		"WallEnd" },
	{ APIVarId_SymbStair, 		"SymbStair" },
	{ APIVarId_Door, 			"Door" },
	{ APIVarId_Object, 			"Object" },
	{ APIVarId_GridElement, 	"GridElement" },
	{ APIVarId_Light, 			"Light" },
	{ APIVarId_CornerWindow, 	"CornerWindow" },

	EnumEnd(-1)
};

// clang-format on

// Tool: return the info string (≈ name)
bool FElementTools::GetInfoString(const API_Guid& InGUID, GS::UniString* OutString)
{
	GSErrCode GSErr = ACAPI_Database(APIDb_GetElementInfoStringID, (void*)&InGUID, OutString);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("CElement::GetInfoString - Get info string error=%s\n", GetErrorName(GSErr));
		return false;
	}
	return true;
}

// Tool: Return the localize name for element type id
const utf8_t* FElementTools::TypeName(API_ElemTypeID InElementType)
{
#if AC_VERSION >= 26 
	if (API_ExternalElemID == InElementType)
	{
		// todo: might be possible to extract more information on the exact external type used
		return "External";	
	}
#endif

	UE_AC_Assert(API_FirstElemType <= InElementType && InElementType <= API_LastElemType);

	static const utf8_t* TypeNames[API_LastElemType + 1] = {};

	if (TypeNames[InElementType] == nullptr)
	{
		TypeNames[InElementType] = TAssEnumName< API_ElemTypeID >::GetName(InElementType);
	}

	return TypeNames[InElementType];
}

// Tool: Return the localize name for element's type
const utf8_t* FElementTools::TypeName(const API_Guid& InElementGuid)
{
	API_Elem_Head ElementHead;
	Zap(&ElementHead);
	ElementHead.guid = InElementGuid;
	GSErrCode GSErr = ACAPI_Element_GetHeader(&ElementHead);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FElementTools::TypeName - Can't get element header {%s} Error=%d\n",
					 APIGuidToString(InElementGuid).ToUtf8(), GSErr);
		GET_HEADER_TYPEID(ElementHead) = API_ZombieElemID;
	}
	return TypeName(GET_HEADER_TYPEID(ElementHead));
}

// Tool: Return the variation as string
utf8_string FElementTools::GetVariationAsString(API_ElemVariationID InVariation)
{
	utf8_string VariationString;
	if (InVariation != APIVarId_Generic)
	{
		const utf8_t* VarName = TAssEnumName< API_ElemVariationID >::GetName(InVariation, FAssValueName::kDontThrow);
		if (VarName[0] != 'U')
		{
			VariationString = VarName;
		}
		else
		{
			VariationString += '\'';
			const unsigned char* sv = (const unsigned char*)&InVariation;
			size_t				 IndexVariation = 0;
			for (; IndexVariation < 4 && sv[IndexVariation] >= 32 && sv[IndexVariation] <= 126; IndexVariation++)
			{
				VariationString += (char)sv[IndexVariation];
			}
			if (IndexVariation != 4)
			{
				VariationString = Utf8StringFormat("0x%08X", InVariation);
			}
			else
			{
				VariationString += '\'';
			}
		}
	}
	return VariationString;
}

// Tool: return libpart index (or 0 if no libpart)
GS::Int32 FElementTools::GetLibPartIndex(const API_Element& InElement)
{
	switch (GET_HEADER_TYPEID(InElement.header))
	{
		case API_WindowID:
		case API_DoorID:
			return InElement.door.openingBase.libInd;
		case API_ObjectID:
		case API_LampID:
			return InElement.lamp.libInd;
		case API_ZoneID:
			return InElement.zone.libInd;
		default:
			return 0;
	}
}

class GSUnID
{
  public:
	typedef char Buffer[128];
	GS::Guid	 Main;
	GS::Guid	 Rev;

	static const char* UnIDNullStr;

	GS::GSErrCode InitWithString(const Buffer inStr);
};
const char* GSUnID::UnIDNullStr = "{00000000-0000-0000-0000-000000000000}-{00000000-0000-0000-0000-000000000000}";

GS::GSErrCode GSUnID::InitWithString(const Buffer inStr)
{
	GS::Guid	main, rev;
	char		guidStr[60];
	const Int32 strLen = sizeof(guidStr) / sizeof(guidStr[0]);
	Int32		i;

	const char* uiStr = inStr;
	while (*uiStr == ' ' || *uiStr == '\t')
	{
		uiStr++;
	}

	if (*uiStr++ != '{')
	{
		return Error;
	}

	for (i = 0; i < strLen - 1 && *uiStr != 0 && *uiStr != '}'; i++, uiStr++)
	{
		guidStr[i] = *uiStr;
	}
	if (*uiStr != '}')
	{
		return Error;
	}
	guidStr[i] = 0;

	if (main.ConvertFromString(guidStr) != NoError)
	{
		return Error;
	}

	if (*(++uiStr)++ != '-' || *uiStr++ != '{')
	{
		return Error;
	}

	for (i = 0; i < strLen - 1 && *uiStr != 0 && *uiStr != '}'; i++, uiStr++)
	{
		guidStr[i] = *uiStr;
	}
	if (*uiStr != '}')
	{
		return Error;
	}
	guidStr[i] = 0;

	if (rev.ConvertFromString(guidStr) != NoError)
	{
		return Error;
	}

	Main = main;
	Rev = rev;

	return NoError;
}

GS::Guid FElementTools::GetLibPartId(const API_Elem_Head& InElement)
{
	GSUnID::Buffer lpfUnID = {0};

	GS::GSErrCode GSErr = ACAPI_Goodies(APIAny_GetElemLibPartUnIdID, const_cast< API_Elem_Head* >(&InElement), lpfUnID);
	if (GSErr == NoError)
	{
		GSUnID LibPartUnIdID;
		GSErr = LibPartUnIdID.InitWithString(lpfUnID);
		if (GSErr == NoError)
		{
			return LibPartUnIdID.Main;
		}
		UE_AC_DebugF("CElementID::InitLibPartInfo - InitWithString (error=%s)\n", GetErrorName(GSErr));
	}
	else
	{
		UE_AC_DebugF("CElementID::InitLibPartInfo - Can't get element lib part id (error=%s)\n", GetErrorName(GSErr));
	}
	return GS::NULLGuid;
}

// Tool: return element's owner guid
API_Guid FElementTools::GetOwner(const API_Element& InElement)
{
	size_t Offset = GetOwnerOffset(GET_HEADER_TYPEID(InElement.header));
	if (Offset)
	{
		return *reinterpret_cast< const API_Guid* >(reinterpret_cast< const char* >(&InElement) + Offset);
	}
	return APINULLGuid;
}

// Tool: return element's owner guid
API_Guid FElementTools::GetOwner(const API_Guid& InElementGuid)
{
	API_Element ApiElement;
	Zap(&ApiElement);
	ApiElement.header.guid = InElementGuid;
	auto GSErr = ACAPI_Element_Get(&ApiElement);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("CSyncContext::IsSelected - ACAPI_Element_Get error=%s ObjectId=%s\n", GetErrorName(GSErr),
					 APIGuidToString(InElementGuid).ToUtf8());
		return APINULLGuid;
	}
	return GetOwner(ApiElement);
}

// Tool: return owner offset for specified element type
size_t FElementTools::GetOwnerOffset(API_ElemTypeID InTypeID)
{
	static size_t TableString[API_LastElemType + 1] = {0};
	static bool	  bInitialized = false;

	if (bInitialized == false)
	{
		bInitialized = true;

		TableString[API_WallID] = offsetof(API_Element, wall.head);
		TableString[API_ColumnID] = offsetof(API_Element, column.head);
		TableString[API_BeamID] = offsetof(API_Element, beam.head);
		TableString[API_WindowID] = offsetof(API_Element, window.owner);
		TableString[API_DoorID] = offsetof(API_Element, door.owner);
		TableString[API_ObjectID] = offsetof(API_Element, object.owner);
		TableString[API_LampID] = offsetof(API_Element, lamp.owner);
		TableString[API_SlabID] = offsetof(API_Element, slab.head);
		TableString[API_RoofID] = offsetof(API_Element, roof.head);
		TableString[API_MeshID] = offsetof(API_Element, mesh.head);

		TableString[API_DimensionID] = offsetof(API_Element, dimension.head);
		TableString[API_RadialDimensionID] = offsetof(API_Element, radialDimension.head);
		TableString[API_LevelDimensionID] = offsetof(API_Element, levelDimension.head);
		TableString[API_AngleDimensionID] = offsetof(API_Element, angleDimension.head);

		TableString[API_TextID] = offsetof(API_Element, text.owner);
		TableString[API_LabelID] = offsetof(API_Element, label.parent);
		TableString[API_ZoneID] = offsetof(API_Element, zone.head);

		TableString[API_HatchID] = offsetof(API_Element, hatch.head);
		TableString[API_LineID] = offsetof(API_Element, line.head);
		TableString[API_PolyLineID] = offsetof(API_Element, polyLine.head);
		TableString[API_ArcID] = offsetof(API_Element, arc.head);
		TableString[API_CircleID] = offsetof(API_Element, circle.head);
		TableString[API_SplineID] = offsetof(API_Element, spline.head);
		TableString[API_HotspotID] = offsetof(API_Element, hotspot.head);

		TableString[API_CutPlaneID] = offsetof(API_Element, cutPlane.head);
		TableString[API_CameraID] = offsetof(API_Element, camera.head);
		TableString[API_CamSetID] = offsetof(API_Element, camset.head);

		TableString[API_GroupID] = 0;
		TableString[API_SectElemID] = offsetof(API_Element, sectElem.head);

		TableString[API_DrawingID] = offsetof(API_Element, drawing.head);
		TableString[API_PictureID] = offsetof(API_Element, picture.head);
		TableString[API_DetailID] = offsetof(API_Element, detail.head);
		TableString[API_ElevationID] = offsetof(API_Element, elevation.head);
		TableString[API_InteriorElevationID] = offsetof(API_Element, interiorElevation.head);
		TableString[API_WorksheetID] = offsetof(API_Element, worksheet.head);

		TableString[API_HotlinkID] = offsetof(API_Element, hotlink.head);

		TableString[API_CurtainWallID] = offsetof(API_Element, curtainWall.head);
		TableString[API_CurtainWallSegmentID] = offsetof(API_Element, cwSegment.owner);
		TableString[API_CurtainWallFrameID] = offsetof(API_Element, cwFrame.owner);
		TableString[API_CurtainWallPanelID] = offsetof(API_Element, cwPanel.owner);
		TableString[API_CurtainWallJunctionID] = offsetof(API_Element, cwJunction.owner);
		TableString[API_CurtainWallAccessoryID] = offsetof(API_Element, cwAccessory.owner);
		TableString[API_ShellID] = offsetof(API_Element, shell.head);
		TableString[API_SkylightID] = offsetof(API_Element, skylight.owner);
		TableString[API_MorphID] = offsetof(API_Element, morph.head);
		TableString[API_ChangeMarkerID] = offsetof(API_Element, changeMarker.head);

		TableString[API_StairID] = offsetof(API_Element, stair.head);
		TableString[API_RiserID] = offsetof(API_Element, stairRiser.owner);
		TableString[API_TreadID] = offsetof(API_Element, stairTread.owner);
		TableString[API_StairStructureID] = offsetof(API_Element, stairStructure.owner);

		TableString[API_RailingID] = offsetof(API_Element, railing.head);
		TableString[API_RailingToprailID] = offsetof(API_Element, railingToprail.owner);
		TableString[API_RailingHandrailID] = offsetof(API_Element, railingHandrail.owner);
		TableString[API_RailingRailID] = offsetof(API_Element, railingRail.owner);
		TableString[API_RailingPostID] = offsetof(API_Element, railingPost.owner);
		TableString[API_RailingInnerPostID] = offsetof(API_Element, railingInnerPost.owner);
		TableString[API_RailingBalusterID] = offsetof(API_Element, railingBaluster.owner);
		TableString[API_RailingPanelID] = offsetof(API_Element, railingPanel.owner);
		TableString[API_RailingSegmentID] = offsetof(API_Element, railingSegment.owner);
		TableString[API_RailingNodeID] = offsetof(API_Element, railingNode.owner);
		TableString[API_RailingBalusterSetID] = offsetof(API_Element, railingBalusterSet.owner);
		TableString[API_RailingPatternID] = offsetof(API_Element, railingPattern.owner);
		TableString[API_RailingToprailEndID] = offsetof(API_Element, railingToprailEnd.owner);
		TableString[API_RailingHandrailEndID] = offsetof(API_Element, railingToprailEnd.owner);
		TableString[API_RailingRailEndID] = offsetof(API_Element, railingRailEnd.owner);
		TableString[API_RailingToprailConnectionID] = offsetof(API_Element, railingToprailConnection.owner);
		TableString[API_RailingHandrailConnectionID] = offsetof(API_Element, railingHandrailConnection.owner);
		TableString[API_RailingRailConnectionID] = offsetof(API_Element, railingRailConnection.owner);
		TableString[API_RailingEndFinishID] = offsetof(API_Element, railingEndFinish.owner);

#if AC_VERSION < 26
		TableString[API_AnalyticalSupportID] = offsetof(API_Element, analyticalSupport.head);
		TableString[API_AnalyticalLinkID] = offsetof(API_Element, analyticalLink.head);
#endif
		TableString[API_ColumnSegmentID] = offsetof(API_Element, columnSegment.owner);
		TableString[API_BeamSegmentID] = offsetof(API_Element, beamSegment.owner);
		TableString[API_OpeningID] = offsetof(API_Element, opening.owner);
	}

	if (InTypeID < 0 && InTypeID > API_LastElemType)
	{
		UE_AC_DebugF("FElementTools::GetOwnerOffset - Invalid API_ElemTypeID=%d\n", InTypeID);
		InTypeID = API_ZombieElemID;
	}
	return TableString[InTypeID];
}

// Tool: return classifications of the element
GSErrCode FElementTools::GetElementClassifications(
	GS::Array< GS::Pair< API_ClassificationSystem, API_ClassificationItem > >& OutClassifications,
	const API_Guid&															   InElementGuid)
{
	GS::Array< GS::Pair< API_Guid, API_Guid > > SystemItemPairs;
	UE_AC_ReturnOnGSError(ACAPI_Element_GetClassificationItems(InElementGuid, SystemItemPairs));

	for (GS::Array< GS::Pair< API_Guid, API_Guid > >::FastIterator IterSystemItemPair = SystemItemPairs.BeginFast();
		 IterSystemItemPair != SystemItemPairs.EndFast(); ++IterSystemItemPair)
	{
		API_ClassificationSystem System;
		System.guid = IterSystemItemPair->first;
		UE_AC_ReturnOnGSError(ACAPI_Classification_GetClassificationSystem(System));

		API_ClassificationItem Classification;
		UE_AC_ReturnOnGSError(
			ACAPI_Element_GetClassificationInSystem(InElementGuid, IterSystemItemPair->first, Classification));

		GS::Pair< API_ClassificationSystem, API_ClassificationItem > ApiClassification(System, Classification);
		OutClassifications.Push(ApiClassification);
	}

	return NoError;
}

GSErrCode FElementTools::GetElementProperties(GS::Array< API_Property >& OutProperties, const API_Guid& InElementGuid)
{
	GS::Array< API_PropertyDefinition > PropertyDefinitions;
	UE_AC_ReturnOnGSError(ACAPI_Element_GetPropertyDefinitions(InElementGuid, API_PropertyDefinitionFilter_UserDefined,
															   PropertyDefinitions));

	GS::Array< API_PropertyDefinition > PropertyDefinitionsFiltered;
	for (const API_PropertyDefinition& PropertyDefinition : PropertyDefinitions)
	{
		if (PropertyDefinition.measureType == API_PropertyDefaultMeasureType ||
			PropertyDefinition.measureType == API_PropertyUndefinedMeasureType)
		{
			API_Property Property;
			Property.definition = PropertyDefinition;
			OutProperties.Push(Property);
			PropertyDefinitionsFiltered.Push(PropertyDefinition);
		}
	}

	UE_AC_ReturnOnGSError(ACAPI_Element_GetPropertyValues(InElementGuid, PropertyDefinitionsFiltered, OutProperties));

	return NoError;
}

END_NAMESPACE_UE_AC
