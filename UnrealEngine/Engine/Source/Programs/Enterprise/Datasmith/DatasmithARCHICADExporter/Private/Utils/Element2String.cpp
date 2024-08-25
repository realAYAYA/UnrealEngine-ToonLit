// Copyright Epic Games, Inc. All Rights Reserved.

#include "Element2String.h"
#include "ElementTools.h"
#include "TAssValueName.h"
#include "LibPartInfo.h"

BEGIN_NAMESPACE_UE_AC

utf8_string GetHotLinkInfo(const API_Element& InElement)
{
	utf8_string HotLinkInfo;
	API_Guid	ElemGuid = InElement.header.guid;
	API_Guid	HotlinkElemGuid = APINULLGuid;
	GSErrCode	GSErr = ACAPI_Goodies(APIAny_GetContainingHotlinkGuidID, &ElemGuid, &HotlinkElemGuid);
	if (GSErr == NoError)
	{
		if (HotlinkElemGuid != APINULLGuid)
		{
			API_Element HotlinkElem;
			Zap(&HotlinkElem);
			GET_HEADER_TYPEID(HotlinkElem.header) = API_HotlinkID;
			HotlinkElem.header.guid = HotlinkElemGuid;
			GSErr = ACAPI_Element_Get(&HotlinkElem);
			if (GSErr == NoError)
			{
				HotLinkInfo = Utf8StringFormat(
					"\t\tHotlink info type=%s, Guid={%s}\n\t\t\thotlinkNodeGuid={%s}, HotlinkNodeGuid={%s}\n%s\n",
					HotlinkElem.hotlink.type == APIHotlink_Module ? "Module" : "XRef",
					APIGuidToString(HotlinkElem.header.guid).ToUtf8(),
					APIGuidToString(HotlinkElem.hotlink.hotlinkNodeGuid).ToUtf8(),
					APIGuidToString(HotlinkElem.hotlink.hotlinkGroupGuid).ToUtf8(),
					""); // MatrixAsString("HotLink Matrix", 16, HotlinkElem.hotlink.transformation).c_str());
			}
			else
			{
				UE_AC_DebugF("GetHotLinkInfo - APIAny_GetContainingHotlinkGuidID - Error=%d\n", GSErr);
			}
		}
	}
	else
	{
		UE_AC_DebugF("GetHotLinkInfo - ACAPI_Element_Get - Error=%d\n", GSErr);
	}
	return HotLinkInfo;
}

static bool bDumpLibSections = false;

// Tool:Return some element information as a string
utf8_string FElement2String::GetElementAsShortString(const API_Element& InElement)
{
	utf8_string	  ElementString;
	GS::UniString ElementInfo;
	FElementTools::GetInfoString(InElement.header.guid, &ElementInfo);

	ElementString += Utf8StringFormat("Element \"%s\", type=%s, Guid={%s}, Layer=\"%s\", Floor=%d\n",
									  ElementInfo.ToUtf8(), FElementTools::TypeName(GET_HEADER_TYPEID(InElement.header)),
									  APIGuidToString(InElement.header.guid).ToUtf8(),
									  GetLayerName(InElement.header.layer).ToUtf8(), InElement.header.floorInd);

	return ElementString;
}

// Tool:Return some element information as a string
utf8_string FElement2String::GetElementAsShortString(const API_Guid& InId)
{
	API_Element Element;
	Zap(&Element);
	Element.header.guid = InId;
	GSErrCode GSErr = ACAPI_Element_Get(&Element);
	if (GSErr == NoError)
	{
		return GetElementAsShortString(Element);
	}
	else
	{
		UE_AC_DebugF("Can't get element {%s} Error=%d\n", APIGuidToString(InId).ToUtf8(), GSErr);
		return Utf8StringFormat("{%s} is an invalid element id\n", APIGuidToString(InId).ToUtf8());
	}
}

// Tool:Return some element information as a string
utf8_string FElement2String::GetElementAsString(const API_Element& InElement)
{
	utf8_string	  ElementString;
	GS::GSErrCode GSErr;

	// Compute MD5 of memo
	API_Guid MemoMD5 = APINULLGuid;
	{
		FAutoMemo AutoMemo(InElement.header.guid, APIMemoMask_All);
		if (AutoMemo.GSErr == NoError)
		{
			MD5::Generator MD5Generator;
			if (AutoMemo.Memo.params)
			{
				MD5Generator.Update(*AutoMemo.Memo.params, BMGetHandleSize((GSConstHandle)AutoMemo.Memo.params));
			}
			if (AutoMemo.Memo.sideMaterials)
			{
				MD5Generator.Update(AutoMemo.Memo.sideMaterials, sizeof(*AutoMemo.Memo.sideMaterials));
			}
			MD5::FingerPrint fp;
			MD5Generator.Finish(fp);
			MemoMD5 = Fingerprint2API_Guid(fp);
		}
		else
		{
			UE_AC_DebugF("FElement2String::GetElementAsString - Error=%d when getting element memo\n", AutoMemo.GSErr);
		}
	}

	GS::UniString ElementInfo;
	FElementTools::GetInfoString(InElement.header.guid, &ElementInfo);

	// Get the lib part from it's UnId
	char LpfUnID[sizeof(API_LibPart::ownUnID)];
	GSErr = ACAPI_Goodies(APIAny_GetElemLibPartUnIdID, (void*)&InElement, LpfUnID);
	if (GSErr != NoError && GSErr != APIERR_BADID)
	{
		UE_AC_DebugF("FElement2String::GetElementAsString - Error=%d when getting element lib part\n", GSErr);
	}

	ElementString += Utf8StringFormat(
		"\tElement \"%s\", type=%s, Guid={%s}\n\t\t\tMemoMD5={%s} Layer=\"%s\", Floor=%d\n", ElementInfo.ToUtf8(),
		FElementTools::TypeName(GET_HEADER_TYPEID(InElement.header)), APIGuidToString(InElement.header.guid).ToUtf8(),
		APIGuidToString(MemoMD5).ToUtf8(), GetLayerName(InElement.header.layer).ToUtf8(), InElement.header.floorInd);
	utf8_string HotLinkInfo = GetHotLinkInfo(InElement);
	if (HotLinkInfo.size())
	{
		ElementString += HotLinkInfo;
	}
	if (InElement.header.groupGuid != APINULLGuid || InElement.header.hotlinkGuid != APINULLGuid)
	{
		ElementString += "\t\t\t";
		if (InElement.header.groupGuid != APINULLGuid)
		{
			ElementString += Utf8StringFormat("Group={%s}", APIGuidToString(InElement.header.groupGuid).ToUtf8());
			API_Guid ParentGroup = APINULLGuid;
			GSErr = ACAPI_ElementGroup_GetGroup(InElement.header.groupGuid, &ParentGroup);
			UE_AC_TestGSError(GSErr);
			while (ParentGroup != APINULLGuid)
			{
				ElementString += Utf8StringFormat(", Parent={%s}", APIGuidToString(ParentGroup).ToUtf8());
				API_Guid CurrentGroup = ParentGroup;
				ParentGroup = APINULLGuid;
				GSErr = ACAPI_ElementGroup_GetGroup(CurrentGroup, &ParentGroup);
				UE_AC_TestGSError(GSErr);
			}
		}
		if (InElement.header.hotlinkGuid != APINULLGuid)
		{
			ElementString += Utf8StringFormat("%sHotlink={%s}", InElement.header.groupGuid == APINULLGuid ? "" : ", ",
											  APIGuidToString(InElement.header.hotlinkGuid).ToUtf8());
		}
		ElementString += "\n";
	}

	if (LpfUnID[0])
	{
		ElementString += Utf8StringFormat("\t\tLibPart=%s\n", LpfUnID);
		if (bDumpLibSections)
		{
			FAuto_API_LibPart LibPart;
			strcpy(LibPart.ownUnID, LpfUnID);
			GSErr = ACAPI_LibPart_Search(&LibPart, false);
			if (GSErr == NoError)
			{
				GS::Int32			 NbSections;
				API_LibPartSection** Sections = nullptr;
				GSErr = ACAPI_LibPart_GetSectionList(LibPart.index, &NbSections, &Sections);
				if (GSErr == NoError)
				{
					for (GS::Int32 IndexSection = 0; IndexSection < NbSections; IndexSection++)
					{
						API_LibPartSection* Section = *Sections + IndexSection;
						GSHandle			SectionHdl = nullptr;
						GS::UniString		SectionStr;
						GSErr = ACAPI_LibPart_GetSection(LibPart.index, Section, &SectionHdl, &SectionStr);
						if (GSErr == NoError)
						{
							BMKillHandle(&SectionHdl);
							char sectTypeStr[5];
							*(GSType*)sectTypeStr = Section->sectType;
							sectTypeStr[4] = 0;
							ElementString +=
								Utf8StringFormat("\t\t\tSection[%d], type='%s'/%d Content =\"%s\"\n", IndexSection,
												 sectTypeStr, Section->subIdent, SectionStr.ToUtf8());
						}
					}
					BMKillHandle((GSHandle*)&Sections);
				}
			}
		}
	}

	Int32					   LibInd = 0;
	const API_ObjectType*	   ObjectType = nullptr;
	const API_OpeningBaseType* OpeningBase = nullptr;
	utf8_string				   ObjectData;
	switch (GET_HEADER_TYPEID(InElement.header))
	{
		case API_WallID:
			{
				API_Coord Begin = InElement.wall.begC;
				API_Coord End = InElement.wall.endC;
				ObjectData = Utf8StringFormat("Wall Begin={%lf, %lf}, End={%lf, %lf} Angle=%lf", Begin.x, Begin.y,
											  End.x, End.y, InElement.wall.angle);
				if (InElement.wall.hasWindow)
				{
					ObjectData += Utf8StringFormat(", hasWindow");
				}
				if (InElement.wall.hasDoor)
				{
					ObjectData += Utf8StringFormat(", hasDoor");
				}
				ObjectData +=
					Utf8StringFormat("\n\t\t\theight=%lf, bottomOffset=%lf, topOffset=%lf, thickness=%lf, "
									 "thickness1=%lf, offset=%lf, offsetFromOutside=%lf, logHeight=%lf",
									 InElement.wall.height, InElement.wall.bottomOffset, InElement.wall.topOffset,
									 InElement.wall.thickness, InElement.wall.thickness1, InElement.wall.offset,
									 InElement.wall.offsetFromOutside, InElement.wall.logHeight);
			}
			break;
		case API_ObjectID:
			LibInd = InElement.object.libInd;
			ObjectType = &InElement.object;
			ObjectData = "Object";
			break;
		case API_LampID:
			LibInd = InElement.lamp.libInd;
			ObjectType = &InElement.lamp;
			ObjectData = "Lamp";
			break;
		case API_DoorID:
			LibInd = InElement.lamp.libInd;
			OpeningBase = &InElement.door.openingBase;
			ObjectData = "Door";
			break;
		case API_WindowID:
			LibInd = InElement.lamp.libInd;
			OpeningBase = &InElement.window.openingBase;
			ObjectData = "Window";
			break;
		case API_SkylightID:
			LibInd = InElement.lamp.libInd;
			OpeningBase = &InElement.skylight.openingBase;
			ObjectData = "Skylight";
			break;

		default:
			break;
	}
	if (LibInd != 0)
	{
		ObjectData += Utf8StringFormat(" LibIndex=%d", LibInd);
		FAuto_API_LibPart LibPart;
		LibPart.index = LibInd;
		GSErrCode GSErr = ACAPI_LibPart_Get(&LibPart);
		if (GSErr == GS::NoError)
		{
			ObjectData +=
				Utf8StringFormat(", Id=%s, Name=\"%s\"", LibPart.ownUnID, GS::UniString(LibPart.docu_UName).ToUtf8());
		}
	}
	if (ObjectType != nullptr)
	{
		if (ObjectType->useObjMaterials)
		{
#if AC_VERSION < 27
			ObjectData += Utf8StringFormat(", mat=%d", ObjectType->mat);
#else
			ObjectData += Utf8StringFormat(", mat=%d", ObjectType->mat.ToInt32_Deprecated());
#endif
		}
		if (ObjectType->reflected)
		{
			ObjectData += Utf8StringFormat(", reflected");
		}
		if (ObjectType->angle != 0)
		{
			ObjectData += Utf8StringFormat(", angle=%lf", ObjectType->angle);
		}
		if (ObjectType->offset.x != 0 || ObjectType->offset.y != 0)
		{
			ObjectData += Utf8StringFormat(", offset={%lf, %lf}", ObjectType->offset.x, ObjectType->offset.y);
		}
		if (ObjectType->pos.x != 0 || ObjectType->pos.y != 0)
		{
			ObjectData += Utf8StringFormat(", pos={%lf, %lf}", ObjectType->pos.x, ObjectType->pos.y);
		}
#if AC_VERSION < 26
		if (ObjectType->ownerID || ObjectType->owner != APINULLGuid)
		{
			ObjectData += Utf8StringFormat(", ownerID=%d, owner={%s}", ObjectType->ownerID,
				APIGuidToString(ObjectType->owner).ToUtf8());
	}
#else
		if (ObjectType->ownerType.typeID || ObjectType->owner != APINULLGuid)
		{
			ObjectData += Utf8StringFormat(", ownerID=%d, owner={%s}", ObjectType->ownerType.typeID,
				APIGuidToString(ObjectType->owner).ToUtf8());
		}
#endif
	}
	if (OpeningBase != nullptr)
	{
		if (OpeningBase->reflected)
		{
			ObjectData += Utf8StringFormat(", reflected");
		}
		if (OpeningBase->oSide)
		{
			ObjectData += Utf8StringFormat(", oSide");
		}
		if (OpeningBase->refSide)
		{
			ObjectData += Utf8StringFormat(", refSide");
		}
	}
	if (ObjectData.size() != 0)
	{
		ElementString += Utf8StringFormat("\t\t%s\n", ObjectData.c_str());
	}

	return ElementString;
}

// Tool:return all element's informations as a string
utf8_string FElement2String::GetAllElementAsString(const API_Element& InElement)
{
	utf8_string AllElementString(GetElementAsString(InElement));
	AllElementString += GetParametersAsString(InElement.header.guid);
	AllElementString += GetIFCAttributesAsString(InElement.header.guid);
	AllElementString += GetIFCPropertiesAsString(InElement.header.guid);
	AllElementString += GetComponentsAsString(InElement.header);
	AllElementString += GetDescriptorsAsString(InElement.header);
	AllElementString += GetPropertiesAsString(InElement.header.guid);
	AllElementString += GetPropertyObjectsAsString(InElement.header);

	return AllElementString;
}

// Tool:return all element's informations as a string
utf8_string FElement2String::GetAllElementAsString(const API_Guid& InElementGuid)
{
	API_Element Element;
	Zap(&Element);
	Element.header.guid = InElementGuid;
	GSErrCode GSErr = ACAPI_Element_Get(&Element);
	if (GSErr == NoError)
	{
		return GetAllElementAsString(Element);
	}
	else
	{
		UE_AC_DebugF("Can't get element {%s} Error=%d\n", APIGuidToString(InElementGuid).ToUtf8(), GSErr);
		return Utf8StringFormat("{%s} is an invalid element id\n", APIGuidToString(InElementGuid).ToUtf8());
	}
}

// Tool: Trace element informations
void FElement2String::DumpInfo(const API_Guid& InElementGuid)
{
	utf8_string DumpString(GetAllElementAsString(InElementGuid));

	if (DumpString.size())
	{
		UE_AC_TraceF("%s", DumpString.c_str());
	}
}

#if 0
	#pragma mark -
#endif

// Tool:Return parameters as a string
utf8_string FElement2String::GetParametersAsString(const API_Guid& InElementGuid)
{
	FAutoMemo AutoMemo(InElementGuid, APIMemoMask_AddPars);
	if (AutoMemo.GSErr == NoError)
	{
		if (AutoMemo.Memo.params) // Can be null
		{
			return GetParametersAsString(AutoMemo.Memo.params);
		}
	}
	else
	{
		UE_AC_DebugF("FElement2String::GetParametersAsString - Error=%d when getting element memo\n", AutoMemo.GSErr);
	}
	return utf8_string();
}

// Tool:Return IFC attributes as a string
utf8_string FElement2String::GetIFCAttributesAsString(const API_Guid& InElementGuid)
{
	utf8_string IFCAttributesString;

	GS::Array< API_IFCAttribute > Attributes;
	bool						  bStoredOnly = false;
	GSErrCode					  GSErr = ACAPI_Element_GetIFCAttributes(InElementGuid, bStoredOnly, &Attributes);
	if (GSErr == NoError)
	{
		GS::USize NbAttr = Attributes.GetSize();
		if (NbAttr)
		{
			IFCAttributesString += Utf8StringFormat("\t\tIFCAtributes Nb=%d\n", NbAttr);
		}
		for (GS::UIndex IndexAttribute = 0; IndexAttribute < NbAttr; IndexAttribute++)
		{
			const API_IFCAttribute& Attribute = Attributes[IndexAttribute];
			IFCAttributesString += Utf8StringFormat(
				"\t\t\tName=%s, Type=%s, %s=%s\n", Attribute.attributeName.ToUtf8(), Attribute.attributeType.ToUtf8(),
				Attribute.hasValue ? "Value" : "Default", Attribute.attributeValue.ToUtf8());
		}
	}
	else
	{
		if (GSErr != APIERR_BADPARS)
		{
			UE_AC_DebugF("FElement2String::GetIFCAttributesAsString - Error=%d getting IFC attributes\n", GSErr);
		}
	}

	return IFCAttributesString;
}

// Tool:Return IFC properties as a string
utf8_string FElement2String::GetIFCPropertiesAsString(const API_Guid& InElementGuid)
{
	utf8_string IFCPropertiesString;

	GS::Array< API_IFCProperty > Properties;
	bool						 bStoredOnly = false;
	GSErrCode					 GSErr = ACAPI_Element_GetIFCProperties(InElementGuid, bStoredOnly, &Properties);
	if (GSErr == NoError)
	{
		GS::USize NbProp = Properties.GetSize();
		if (NbProp)
		{
			IFCPropertiesString += Utf8StringFormat("\t\tIFCProperties Nb=%d\n", NbProp);
		}
		for (GS::UIndex IndexProperty = 0; IndexProperty < NbProp; IndexProperty++)
		{
			const API_IFCProperty& Property = Properties[IndexProperty];

			IFCPropertiesString += Utf8StringFormat(
				"\t\t\tSet=%s, Name=%s, Desc=%s, Type=%d, Value=", Property.head.propertySetName.ToUtf8(),
				Property.head.propertyName.ToUtf8(), Property.head.propertyDescription.ToUtf8(),
				Property.head.propertyType);
			GS::GSSize	NbValues;
			GS::GSIndex IndexValue;
			switch (Property.head.propertyType)
			{
				case API_IFCPropertySingleValueType:
					IFCPropertiesString += GetIFCPropertyValue(Property.singleValue.nominalValue) + "\n";
					break;
				case API_IFCPropertyListValueType:
					IFCPropertiesString += "{";
					NbValues = Property.listValue.listValues.GetSize();
					for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
					{
						if (IndexValue > 0)
						{
							IFCPropertiesString += ", ";
						}
						IFCPropertiesString += GetIFCPropertyValue(Property.listValue.listValues[IndexValue]);
					}
					IFCPropertiesString += "}\n";
					break;
				case API_IFCPropertyBoundedValueType:
					IFCPropertiesString += "[";
					IFCPropertiesString += GetIFCPropertyValue(Property.boundedValue.lowerBoundValue);
					IFCPropertiesString += "..";
					IFCPropertiesString += GetIFCPropertyValue(Property.boundedValue.upperBoundValue);
					IFCPropertiesString += "]\n";
					break;
				case API_IFCPropertyEnumeratedValueType:
					IFCPropertiesString += "(";
					NbValues = Property.enumeratedValue.enumerationReference.GetSize();
					for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
					{
						if (IndexValue > 0)
						{
							IFCPropertiesString += "| ";
						}
						IFCPropertiesString +=
							GetIFCPropertyValue(Property.enumeratedValue.enumerationReference[IndexValue]);
					}
					IFCPropertiesString += ")\n";

					IFCPropertiesString += " {";
					NbValues = Property.enumeratedValue.enumerationValues.GetSize();
					for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
					{
						if (IndexValue > 0)
						{
							IFCPropertiesString += ", ";
						}
						IFCPropertiesString +=
							GetIFCPropertyValue(Property.enumeratedValue.enumerationValues[IndexValue]);
					}
					IFCPropertiesString += "}\n";
					break;
				case API_IFCPropertyTableValueType:
					IFCPropertiesString += "«";
					NbValues = Property.tableValue.definingValues.GetSize();
					for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
					{
						if (IndexValue > 0)
						{
							IFCPropertiesString += ", ";
						}
						IFCPropertiesString += GetIFCPropertyValue(Property.tableValue.definingValues[IndexValue]);
					}
					IFCPropertiesString += "»\n";

					IFCPropertiesString += " {";
					NbValues = Property.tableValue.definedValues.GetSize();
					for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
					{
						if (IndexValue > 0)
						{
							IFCPropertiesString += ", ";
						}
						IFCPropertiesString += GetIFCPropertyValue(Property.tableValue.definedValues[IndexValue]);
					}
					IFCPropertiesString += "}\n";
					break;
				default:
					UE_AC_DebugF("FElement2String::GetIFCPropertiesAsString propertyType=%d\n",
								 Property.head.propertyType);
					break;
			}
		}
	}
	else
	{
		if (GSErr != APIERR_BADPARS)
		{
			UE_AC_DebugF("FElement2String::GetIFCPropertiesAsString - Error=%d getting IFC attributes\n", GSErr);
		}
	}

	return IFCPropertiesString;
}

// Tool:Return Components as a string
utf8_string FElement2String::GetComponentsAsString(const API_Elem_Head& InElementHeader)
{
	utf8_string ComponentsString;
	GSErrCode	GSErr = NoError;

#if AC_VERSION < 25
	API_ComponentRefType** CompRefs = nullptr;
	Int32				   NbComp;
	GSErr = ACAPI_Element_GetComponents(&InElementHeader, &CompRefs, &NbComp);
	if (GSErr == NoError)
	{
		if (NbComp)
		{
			ComponentsString += Utf8StringFormat("\t\tComponents Nb=%d\n", NbComp);
		}
		for (Int32 IndexComponent = 0; IndexComponent < NbComp; IndexComponent++)
		{
			if ((*CompRefs)[IndexComponent].status != APIDBRef_Deleted)
			{
				API_ListData ListData;
				Zap(&ListData);
				ListData.header.typeID = API_ComponentID;
				ListData.header.index = (*CompRefs)[IndexComponent].index;
				ListData.header.setIndex = (*CompRefs)[IndexComponent].setIndex;
				switch ((*CompRefs)[IndexComponent].status)
				{
					case APIDBRef_Normal:
						GSErr = ACAPI_ListData_Get(&ListData);
						break;
					case APIDBRef_Local:
						GSErr =
							ACAPI_ListData_GetLocal((*CompRefs)[IndexComponent].libIndex, &InElementHeader, &ListData);
						break;
				}
				if (GSErr != NoError)
				{
					break;
				}

				ComponentsString +=
					Utf8StringFormat("\t\t\tKeycode/code: \"%s\"/\"%s\" quantity: %.2f\n", ListData.component.keycode,
									 ListData.component.code, (*CompRefs)[IndexComponent].quantity);
			}
		}
		BMKillHandle((GSHandle*)&CompRefs);
	}
#else
	GS::Array< API_ElemComponentID > Components;
	GSErr = ACAPI_Element_GetComponents(InElementHeader.guid, Components);
	USize NbComp = Components.GetSize();
	if (GSErr == NoError)
	{
		if (NbComp != 0)
		{
			ComponentsString += Utf8StringFormat("\t\tComponents Nb=%d\n", NbComp);
		}
		for (USize IndexComponent = 0; IndexComponent < NbComp; IndexComponent++)
		{
			API_ElemComponentID& Component = Components[IndexComponent];

			GS::Array< API_PropertyDefinition > propertyDefinitions;
			GSErr = ACAPI_ElemComponent_GetPropertyDefinitions(Component, API_PropertyDefinitionFilter_All,
															   propertyDefinitions);
			if (GSErr != NoError)
			{
				UE_AC_DebugF("FElement2String::GetComponentsAsString - Error=%d getting property definitions\n", GSErr);
				break;
			}

			GS::Array< API_Property > properties;
			GSErr = ACAPI_ElemComponent_GetPropertyValues(Component, propertyDefinitions, properties);
			if (GSErr != NoError)
			{
				UE_AC_DebugF("FElement2String::GetComponentsAsString - Error=%d getting property values\n", GSErr);
				break;
			}

			for (const API_Property& Property : properties)
			{
				API_PropertyGroup Group = {};
				Group.guid = Property.definition.groupGuid;
				GSErr = ACAPI_Property_GetPropertyGroup(Group);
				if (GSErr != NoError)
				{
					UE_AC_DebugF("FElement2String::GetComponentsAsString - Error=%d getting property group\n", GSErr);
					continue;
				}

				if (Property.status == API_Property_NotAvailable || Property.status == API_Property_NotEvaluated)
				{
					UE_AC_DebugF("FElement2String::GetComponentsAsString - Property not available\n");
					continue;
				}

				GS::UniString PropertyValue;
				GSErr = ACAPI_Property_GetPropertyValueString(Property, &PropertyValue);
				if (GSErr == NoError)
				{
					ComponentsString += Utf8StringFormat("\t\t\tProperty Group:\"%s\" Definition:\"%s\" Value=\"%s\"\n",
														 Group.name.ToUtf8(), Property.definition.name.ToUtf8(),
														 PropertyValue.ToUtf8());
				}
				else
				{
					UE_AC_DebugF("FElement2String::GetComponentsAsString - Error=%d getting property value\n", GSErr);
				}
			}
		}
	}
#endif
	else
	{
		if (GSErr != APIERR_GENERAL)
		{
			UE_AC_DebugF("FElement2String::GetComponentsAsString - Error=%d getting components\n", GSErr);
		}
	}

	return ComponentsString;
}

// Tool:Return descriptors as a string
utf8_string FElement2String::GetDescriptorsAsString(const API_Elem_Head& InElementHeader)
{
	utf8_string DescriptorsString;

	API_DescriptorRefType** DescRefs = nullptr;
	Int32					NbDesc;
	GSErrCode				GSErr = ACAPI_Element_GetDescriptors(&InElementHeader, &DescRefs, &NbDesc);
	if (GSErr == NoError)
	{
		if (NbDesc)
		{
			DescriptorsString += Utf8StringFormat("\t\tDescriptors Nb=%d\n", NbDesc);
		}
		for (Int32 IndexDescriptor = 0; IndexDescriptor < NbDesc; IndexDescriptor++)
		{
			API_ListData ListData;
			Zap(&ListData);
			if ((*DescRefs)[IndexDescriptor].status != APIDBRef_Deleted)
			{
				ListData.header.typeID = API_DescriptorID;
				ListData.header.setIndex = (*DescRefs)[IndexDescriptor].setIndex;
				ListData.header.index = (*DescRefs)[IndexDescriptor].index;
				switch ((*DescRefs)[IndexDescriptor].status)
				{
					case APIDBRef_Normal:
						GSErr = ACAPI_ListData_Get(&ListData);
						break;
					case APIDBRef_Local:
						GSErr =
							ACAPI_ListData_GetLocal((*DescRefs)[IndexDescriptor].libIndex, &InElementHeader, &ListData);
						break;
				}
				/* I didn't complete the code because I haven’t found a file with descriptors yet */
				if (GSErr == NoError)
				{
					DescriptorsString += Utf8StringFormat("\t\t\tKeycode/code: \"%s\"/\"%s\"\n",
														  ListData.descriptor.keycode, ListData.descriptor.code);
				}
				else
				{
					UE_AC_DebugF("FElement2String::GetDescriptorsAsString - Error=%d getting list data\n", GSErr);
				}
			}
			BMKillHandle(&ListData.descriptor.name);
		}
		BMKillHandle((GSHandle*)&DescRefs);
	}
	else
	{
		if (GSErr != APIERR_GENERAL)
		{
			UE_AC_DebugF("FElement2String::GetDescriptorsAsString - Error=%d getting descriptors\n", GSErr);
		}
	}

	return DescriptorsString;
}

// Tool:Return properties as a string
utf8_string FElement2String::GetPropertiesAsString(const API_Guid& InElementGuid)
{
	utf8_string PropertiesString;

	// Get all properties definitions applicable to the element
	GS::Array< API_PropertyDefinition > Definitions;
	GSErrCode							GSErr =
		ACAPI_Element_GetPropertyDefinitions(InElementGuid, API_PropertyDefinitionFilter_UserDefined, Definitions);
	if (GSErr == NoError)
	{
		// Build a list to query element's properties
		GS::USize				  NbProps = Definitions.GetSize();
		GS::Array< API_Property > Properties;
		Properties.SetCapacity(NbProps);
		GS::UIndex IndexProps;
		for (IndexProps = 0; IndexProps < NbProps; IndexProps++)
		{
			if (IsPropertyGroupExportable(Definitions[IndexProps].groupGuid) &&
				ACAPI_Element_IsPropertyDefinitionVisible(InElementGuid, Definitions[IndexProps].guid))
			{
				API_Property Property;
				GSErr = ACAPI_Element_GetPropertyValue(InElementGuid, Definitions[IndexProps].guid, Property);
				if (GSErr == NoError)
				{
					Properties.Push(Property);
				}
				else
				{
					UE_AC_DebugF("FElement2String::GetPropertiesAsString - Error=%d getting properties values\n",
								 GSErr);
				}
			}
		}
		NbProps = Properties.GetSize();

		if (NbProps > 0)
		{
			if (NbProps)
			{
				PropertiesString += Utf8StringFormat("\t\tProperties definitions Nb=%d\n", NbProps);
			}
			for (IndexProps = 0; IndexProps < NbProps; IndexProps++)
			{
				// Add properties definitions and values to string
				const API_Property& Property = Properties[IndexProps];
				PropertiesString += PropertyDefinition2String(Property.definition, Property.value, Property.isDefault);
			}
		}
	}
	else
	{
		UE_AC_DebugF("FElement2String::GetPropertiesAsString - Error=%d getting properties definitions\n", GSErr);
	}

	return PropertiesString;
}

// Tool:Return property objects as a string
utf8_string FElement2String::GetPropertyObjectsAsString(const API_Elem_Head& InElementHeader)
{
	utf8_string PropertyObjectsString;

	API_PropertyObjectRefType** PropRefs;
	Int32						NbProps;
	GSErrCode					GSErr = ACAPI_Element_GetPropertyObjects(&InElementHeader, &PropRefs, &NbProps);
	if (GSErr == NoError)
	{
		if (NbProps)
		{
			PropertyObjectsString += Utf8StringFormat("\t\tPropertyObjects Nb=%d, Handle size%d\n", NbProps,
													  BMGetHandleSize((GSConstHandle)PropRefs));
		}
		for (Int32 IndexPropertyObject = 0; IndexPropertyObject < NbProps; IndexPropertyObject++)
		{
			FAuto_API_LibPart LibPart;
			LibPart.index = (*PropRefs)[IndexPropertyObject].libIndex;
			GSErr = ACAPI_LibPart_Get(&LibPart);
			if (GSErr == NoError)
			{
				const utf8_t* s =
					(*PropRefs)[IndexPropertyObject].assign ? "Associated property:" : "Property by criteria:";
				PropertyObjectsString +=
					Utf8StringFormat("\t\t\t%s \"%s\"\n", s, GS::UniString(LibPart.docu_UName).ToUtf8());
			}
			else
			{
				UE_AC_DebugF("FElement2String::GetPropertyObjectsAsString - Error=%d getting lib parts\n", GSErr);
			}
		}
		BMKillHandle((GSHandle*)&PropRefs);
	}
	else
	{
		if (GSErr != APIERR_BADID)
		{
			UE_AC_DebugF("FElement2String::GetPropertyObjectsAsString - Error=%d getting property objects\n", GSErr);
		}
	}

	return PropertyObjectsString;
}

#if PLATFORM_WINDOWS // Define for non compatible OS
	#define strlcpy(d, s, sz) strncpy(d, s, sz - 1)
	#define strlcat(d, s, sz) strncat(d, s, sz - strlen(d) - 1)
#endif

// clang-format off
template <>
FAssValueName::SAssValueName TAssEnumName< API_AddParID >::AssEnumName[] = {
	{ API_ZombieParT, 				"Zombie" },
	{ APIParT_Integer, 				"Integer" },
	{ APIParT_Length, 				"Length" },
	{ APIParT_Angle, 				"Angle" },
	{ APIParT_RealNum, 				"RealNum" },
	{ APIParT_LightSw, 				"LightSw" },
	{ APIParT_ColRGB, 				"ColRGB" },
	{ APIParT_Intens, 				"Intens" },
	{ APIParT_LineTyp, 				"LineTyp" },
	{ APIParT_Mater, 				"Mater" },
	{ APIParT_FillPat, 				"FillPat" },
	{ APIParT_PenCol, 				"PenCol" },
	{ APIParT_CString, 				"CString" },
	{ APIParT_Boolean, 				"Boolean" },
	{ APIParT_Separator, 			"Separator" },
	{ APIParT_Title, 				"Title" },
	{ APIParT_BuildingMaterial, 	"BuildingMaterial" },
	{ APIParT_Profile, 				"Profile" },
	{ APIParT_Dictionary, 			"Dictionary" },

	{ -1, nullptr }
};
// clang-format on

// Tool:Return parameters as a string
utf8_string FElement2String::GetParametersAsString(const API_AddParType* const* InParamsHandle)
{
	UE_AC_Assert(InParamsHandle != nullptr && *InParamsHandle != nullptr);

	utf8_string Params;

	// For all parameters
	GS::UInt32 NbParams = BMGetHandleSize((GSConstHandle)InParamsHandle) / sizeof(**InParamsHandle);
	if (NbParams)
	{
		Params += Utf8StringFormat("\t\tParameters Nb=%d\n", NbParams);
	}
	for (GS::UInt32 IndexParam = 0; IndexParam < NbParams; IndexParam++)
	{
		const API_AddParType& Param = (*InParamsHandle)[IndexParam];

		if (Param.flags & (0 /* | API_ParFlg_Hidden | API_ParFlg_Disabled */))
		{
			continue;
		}

		utf8_string ParamName(Param.name);
		utf8_string DescName(GS::UniString(Param.uDescname).ToUtf8());

		char FlagsString[256];
		strlcpy(FlagsString, " [", sizeof(FlagsString));
		if (Param.flags & API_ParFlg_Child)
		{
			strlcat(FlagsString, "Child+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_BoldName)
		{
			strlcat(FlagsString, "Bold+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_Fixed)
		{
			strlcat(FlagsString, "Fixed+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_Unique)
		{
			strlcat(FlagsString, "Unique+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_SHidden)
		{
			strlcat(FlagsString, "SHidden+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_Open)
		{
			strlcat(FlagsString, "Open+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_Disabled)
		{
			strlcat(FlagsString, "Disabled+", sizeof(FlagsString));
		}
		if (Param.flags & API_ParFlg_Hidden)
		{
			strlcat(FlagsString, "Hidded+", sizeof(FlagsString));
		}
		size_t l = strlen(FlagsString);
		if (l == 2)
		{
			FlagsString[0] = 0; // No flags... no []
		}
		else
		{
			FlagsString[l - 1] = ']'; // Replace last + by closing ]
		}

		utf8_string ValueDescription(GS::UniString(Param.valueDescription).ToUtf8());
		if (DescName.size())
		{
			ParamName += utf8_string(" #") + DescName + "#";
		}
		if (FlagsString[0])
		{
			ParamName += FlagsString;
		}
		if (ValueDescription.size())
		{
			ParamName += utf8_string(" ='") + ValueDescription + "' <- ";
		}
		else
		{
			ParamName += utf8_string(" = ");
		}

		if (Param.arrayDescriptions != nullptr)
		{
			ParamName += " ['";
#if 0
			const GS::uchar_t* PosArrayDescriptions = *(const GS::uchar_t**)Param.arrayDescriptions;
			const GS::uchar_t* EndArrayDescriptions = PosArrayDescriptions + BMGetHandleSize(Param.arrayDescriptions);
			while (PosArrayDescriptions < EndArrayDescriptions)
			{
				size_t Length = GS::ucslen(PosArrayDescriptions) + 1;
				UE_AC_Assert(PosArrayDescriptions + Length <= EndArrayDescriptions);
				ParamName += GS::UniString(PosArrayDescriptions).ToUtf8();
				PosArrayDescriptions += Length;
				if (PosArrayDescriptions < EndArrayDescriptions)
				{
					ParamName += "', '";
				}
			}
			UE_AC_Assert(PosArrayDescriptions == EndArrayDescriptions);
#else
			ParamName += "?";
#endif
			ParamName += "'] ";
		}

		if (Param.typeMod == API_ParSimple)
		{
			// Single value parameters
			switch (Param.typeID)
			{
					// Add the parameter depending of it's value type
				case APIParT_Integer:
				case APIParT_LightSw:
				case APIParT_Intens:
				case APIParT_LineTyp:
				case APIParT_Mater:
				case APIParT_FillPat:
				case APIParT_PenCol:
					Params +=
						Utf8StringFormat("\t\t\t[%3d] %s %s:%d\n", IndexParam, ParamName.c_str(),
										 TAssEnumName< API_AddParID >::GetName(Param.typeID), (Int32)Param.value.real);
					break;
				case APIParT_Boolean:
					Params += Utf8StringFormat("\t\t\t[%3d] %s Boolean:%s\n", IndexParam, ParamName.c_str(),
											   Param.value.real != 0 ? "true" : "false");
					break;

				case APIParT_Length:
				case APIParT_Angle:
				case APIParT_RealNum:
				case APIParT_ColRGB:
					Params += Utf8StringFormat("\t\t\t[%3d] %s %s:%lg\n", IndexParam, ParamName.c_str(),
											   TAssEnumName< API_AddParID >::GetName(Param.typeID), Param.value.real);
					break;

				case APIParT_CString:
				case APIParT_Title:
					Params += Utf8StringFormat("\t\t\t[%3d] %s %s:\"%s\"\n", IndexParam, ParamName.c_str(),
											   TAssEnumName< API_AddParID >::GetName(Param.typeID),
											   GS::UniString(Param.value.uStr).ToUtf8());
					break;

				case APIParT_Separator:
					Params += "\n";
					break;

				default:
					UE_AC_DebugF("FElement2String::GetParametersAsString [%3d] typeMod=%d\n", IndexParam,
								 Param.typeMod);
					break;
			}
		}
		else
		{
			// Matrix parameter
			GS::Int32 PosString = 0;
			Params += Utf8StringFormat("\t\t\t[%3d] %s={\n", IndexParam, ParamName.c_str());
			for (GS::Int32 Index1 = 0; Index1 < Param.dim1; Index1++)
			{
				Params += Utf8StringFormat("\t\t\t\t{");
				for (GS::Int32 Index2 = 0; Index2 < Param.dim2; Index2++)
				{
					if (Index2 != 0)
					{
						Params += ", ";
					}
					if (Param.typeID == APIParT_CString)
					{
						GS::uchar_t* valueStr = (GS::uchar_t*)*Param.value.array + PosString;
						PosString += GS::ucslen32(valueStr) + 1;
						Params += Utf8StringFormat("\"%s\"", GS::UniString(valueStr).ToUtf8());
					}
					else
					{
						Params += Utf8StringFormat("%lg", ((double*)*Param.value.array)[PosString++]);
					}
				}
				Params += "}\n";
			}
			Params += "\t\t\t}\n";
		}
	}

	return Params;
}

bool FElement2String::IsPropertyGroupExportable(const API_Guid& InGroupGuid)
{
	API_PropertyGroup Group;
	Group.guid = InGroupGuid;
	if (ACAPI_Property_GetPropertyGroup(Group) != NoError)
	{
		return false;
	}

	return Group.groupType == API_PropertyCustomGroupType;
}

GS::UniString FElement2String::GetPropertyGroupName(const API_Guid& InGroupGuid)
{
	API_PropertyGroup Group;
	Group.guid = InGroupGuid;
	if (ACAPI_Property_GetPropertyGroup(Group) != NoError)
	{
		static const GS::UniString InvalidGroupId("Invalid group id");
		return InvalidGroupId;
	}

	static const utf8_t* typeNames[] = {"Static  ", "Dynamic ", "Custom  "};
	return GS::UniString(typeNames[Group.groupType], CC_UTF8) + Group.name;
}

utf8_string FElement2String::GetVariantValue(const API_Variant& InVariant)
{
	switch (InVariant.type)
	{
		case API_PropertyUndefinedValueType:
			UE_AC_DebugF("GetVariantValue - Undefined value type\n");
			return "--- Undefined value type ---";
		case API_PropertyIntegerValueType:
			return Utf8StringFormat("%d", InVariant.intValue);
		case API_PropertyRealValueType:
			return Utf8StringFormat("%lg", InVariant.doubleValue);
		case API_PropertyStringValueType:
			return Utf8StringFormat("\"%s\"", InVariant.uniStringValue.ToUtf8());
		case API_PropertyBooleanValueType:
			return InVariant.boolValue ? "true" : "false";
		case API_PropertyGuidValueType:
			return utf8_string("{") + APIGuidToString(InVariant.guidValue).ToUtf8() + "}";
	}
	UE_AC_DebugF("GetVariantValue - Invalid variant %d\n", InVariant.type);
	return Utf8StringFormat("Invalid Variant(%d)", InVariant.type);
}

utf8_string FElement2String::PropertyDefinition2String(const API_PropertyDefinition& InDefinition,
													   const API_PropertyValue& InValue, bool bInDefault)
{
	utf8_string				 Value;
	GS::USize				 NbValues;
	GS::UIndex				 IndexValue;
	const API_PropertyValue& PropValue = bInDefault ? InDefinition.defaultValue.basicValue : InValue;
	switch (InDefinition.collectionType)
	{
		case API_PropertyUndefinedCollectionType:
			UE_AC_DebugF("FElement2String::PropertyDefinition2String - Undefined collection type\n");
			Value = "--- Undefined collection type ---";
			break;
		case API_PropertySingleCollectionType:
			Value = GetVariantValue(PropValue.singleVariant.variant);
			break;
		case API_PropertyListCollectionType:
			Value = "{";
			NbValues = PropValue.listVariant.variants.GetSize();
			for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
			{
				if (IndexValue > 0)
				{
					Value += ", ";
				}
				Value += GetVariantValue(PropValue.listVariant.variants[IndexValue]);
			}
			Value += "}";
			break;
#if AC_VERSION < 25
		case API_PropertySingleChoiceEnumerationCollectionType:
			Value = "#";
			Value += GetVariantValue(PropValue.singleEnumVariant.keyVariant) + ":";
			Value += GetVariantValue(PropValue.singleEnumVariant.displayVariant);
			break;
		case API_PropertyMultipleChoiceEnumerationCollectionType:
			Value = "{";
			NbValues = PropValue.multipleEnumVariant.variants.GetSize();
			for (IndexValue = 0; IndexValue < NbValues; IndexValue++)
			{
				if (IndexValue > 0)
				{
					Value += ", #";
				}
				else
				{
					Value += "#";
				}
				Value += GetVariantValue(PropValue.multipleEnumVariant.variants[IndexValue].keyVariant) + ":";
				Value += GetVariantValue(PropValue.multipleEnumVariant.variants[IndexValue].displayVariant);
			}
			Value += "}";
			break;
#endif
		default:
			UE_AC_DebugF("FElement2String::PropertyDefinition2String - Invalid collection type %d\n",
						 InDefinition.collectionType);
			Value = "--- Invalid collection type ---";
			break;
	}
	utf8_string DescriptionString(InDefinition.description.ToUtf8());
	RemoveLeadingAndTrailing(&DescriptionString);
	if (DescriptionString.size())
	{
		DescriptionString.insert(0, ", Description=");
	}
	const utf8_t* DefaultString = bInDefault ? "Default" : "Value";
	utf8_string	  result(
		  Utf8StringFormat("\t\t\tGroup=\"%s\", Name=\"%s\"%s" /*, CollectionType=%s, VariantType=%s*/ ", %s=%s\n",
						   GetPropertyGroupName(InDefinition.groupGuid).ToUtf8(), InDefinition.name.ToUtf8(),
						   DescriptionString.c_str(), DefaultString, Value.c_str()));
#if 1
	result += Utf8StringFormat("\t\t\t\tGrpID={%s}, PropId={%s}\n", APIGuidToString(InDefinition.groupGuid).ToUtf8(),
							   APIGuidToString(InDefinition.guid).ToUtf8());
#endif
	return result;
}

utf8_string FElement2String::GetIFCPropertyValue(const API_IFCPropertyValue& InValue)
{
	return Utf8StringFormat("(%s) %s", InValue.valueType.ToUtf8(), GetIFCPropertyAnyValue(InValue.value).c_str());
}

utf8_string FElement2String::GetIFCPropertyAnyValue(const API_IFCPropertyAnyValue& InValue)
{
	switch (InValue.primitiveType)
	{
		case API_IFCPropertyAnyValueIntegerType:
			return Utf8StringFormat("%lld", InValue.intValue);
		case API_IFCPropertyAnyValueRealType:
			return Utf8StringFormat("%lg", InValue.doubleValue);
		case API_IFCPropertyAnyValueBooleanType:
			return Utf8StringFormat("%s", InValue.boolValue ? "true" : "false");
		case API_IFCPropertyAnyValueLogicalType:
			return Utf8StringFormat("%lld", InValue.intValue);
		case API_IFCPropertyAnyValueStringType:
			return Utf8StringFormat("\"%s\"", InValue.stringValue.ToUtf8());
	}
	UE_AC_DebugF("GetIFCPropertyAnyValue primitiveType=%d\n", InValue.primitiveType);
	return Utf8StringFormat("Invalid primitive type(%d)", InValue.primitiveType);
}

inline static bool IsSpace(char C)
{
	return C == ' ' || C == '\t' || C == '\n';
}

// Remove leading and trailing space
void FElement2String::RemoveLeadingAndTrailing(utf8_string* IOString)
{
	// Remove trailing spaces
	size_t IndexChar = IOString->size();
	while (IndexChar != 0)
	{
		if (!IsSpace(IOString->at(--IndexChar)))
		{
			++IndexChar;
			break;
		}
	}
	IOString->erase(IndexChar);

	// Remove leading spaces
	IndexChar = utf8_string::npos;
	while (++IndexChar < IOString->size() && IsSpace(IOString->at(IndexChar)))
	{
	}
	IOString->erase(0, IndexChar);
}

utf8_string FDump2String::ListLibraries()
{
	utf8_string DumpString;

	GS::Array< API_LibraryInfo > LibInfoArray;

	if (ACAPI_Environment(APIEnv_GetLibrariesID, &LibInfoArray) == NoError)
	{
		DumpString = Utf8StringFormat("ListLibraries - The number of loaded libraries is %u\n", LibInfoArray.GetSize());
		for (UInt32 IndexLibrary = 0; IndexLibrary < LibInfoArray.GetSize(); IndexLibrary++)
		{
			const API_LibraryInfo& LibInfo = LibInfoArray[IndexLibrary];
			DumpString += Utf8StringFormat("\tLibrary #%-2d name=\"%s\" type=%d\n", IndexLibrary, LibInfo.name.ToUtf8(),
										   LibInfo.libraryType);
			GS::UniString LibPath;
			LibInfo.location.ToPath(&LibPath);
			DumpString += Utf8StringFormat("\t\tPath=\"%s\"\n", LibPath.ToUtf8());
		}
	}

	return DumpString;
}

END_NAMESPACE_UE_AC
