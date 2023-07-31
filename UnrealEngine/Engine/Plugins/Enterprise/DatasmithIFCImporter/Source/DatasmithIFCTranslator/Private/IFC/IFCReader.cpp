// Copyright Epic Games, Inc. All Rights Reserved.

#include "IFCReader.h"

#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/RotationMatrix.h"
#include "Math/Transform.h"

#ifdef WITH_IFC_ENGINE_LIB
#if PLATFORM_64BITS
#define WIN64
#endif
#include "ifc/ifcengine.h"
#include "ifc/engine.h"
#endif


DEFINE_LOG_CATEGORY(LogDatasmithIFCReader);

namespace IFC
{

#ifdef WITH_IFC_ENGINE_LIB
	static const FString IfcPoject_Type = TEXT("IFCPROJECT");
	static const FString IfcMappedItem_Type = TEXT("IFCMAPPEDITEM");
	static const FString IfcSIUnit_Type = TEXT("IFCSIUNIT");
	static const FString IfcConversionBasedUnit_Type = TEXT("IFCCONVERSIONBASEDUNIT");
	static const FString IfcSurfaceStyle_Type = TEXT("IFCSURFACESTYLE");
	static const FString IfcSurfaceStyleRendering_Type = TEXT("IFCSURFACESTYLERENDERING");
	static const FString IfcSurfaceStyleShading_Type = TEXT("IFCSURFACESTYLESHADING");
	static const FString IfcRelAssociatesMaterial_Type = TEXT("IFCRELASSOCIATESMATERIAL");
	static const FString IfcMaterial_Type = TEXT("IFCMATERIAL");
	static const FString IfcMaterialLayerSetUsage_Type = TEXT("IFCMATERIALLAYERSETUSAGE");
	static const FString IfcMaterialLayerSet_Type = TEXT("IFCMATERIALLAYERSET");
	static const FString IfcMaterialLayer_Type = TEXT("IFCMATERIALLAYER");
	static const FString IfcStyledItem_Type = TEXT("IFCSTYLEDITEM");
	static const FString IfcColourRgb_Type = TEXT("IFCCOLOURRGB");
	static const FString IfcPropertySet_Type = TEXT("IFCPROPERTYSET");
	static const FString IfcElementQuantity_Type = TEXT("IFCELEMENTQUANTITY");
	static const FString IfcPropertySingleValue_Type = TEXT("IFCPROPERTYSINGLEVALUE");
	static const FString IfcQuantityLength_Type = TEXT("IFCQUANTITYLENGTH");
	static const FString IfcQuantityArea_Type = TEXT("IFCQUANTITYAREA");
	static const FString IfcQuantityVolume_Type = TEXT("IFCQUANTITYVOLUME");
	static const FString IfcQuantityCount_Type = TEXT("IFCQUANTITYCOUNT");
	static const FString IfcQuantityWeigth_Type = TEXT("IFCQUANTITYWEIGHT");
	static const FString IfcQuantityTime_Type = TEXT("IFCQUANTITYTIME");
	static const FString IfcBuilding_Type = TEXT("IFCBUILDING");
	static const FString IfcBuildingStorey_Type = TEXT("IFCBUILDINGSTOREY");
	static const FString IfcDistributionElement_Type = TEXT("IFCDISTRIBUTIONELEMENT");
	static const FString IfcElectricalElement_Type = TEXT("IFCELECTRICALELEMENT");
	static const FString IfcElementAssembly_Type = TEXT("IFCELEMENTASSEMBLY");
	static const FString IfcElementComponent_Type = TEXT("IFCELEMENTCOMPONENT");
	static const FString IfcEquipmentElement_Type = TEXT("IFCEQUIPMENTELEMENT");
	static const FString IfcFeatureElement_Type = TEXT("IFCFEATUREELEMENT");
	static const FString IfcFurnishingElement_Type = TEXT("IFCFURNISHINGELEMENT");
	static const FString IfcTransportElement_Type = TEXT("IFCTRANSPORTELEMENT");
	static const FString IfcVirtualElement_Type = TEXT("IFCVIRTUALELEMENT");
	static const FString IfcReinforcingElement_Type = TEXT("IFCREINFORCINGELEMENT");
	static const FString IfcOpeningElement_Type = TEXT("IFCOPENINGELEMENT");

	static const FString IfcRepresentation_Name = TEXT("Representation");
	static const FString IfcRepresentations_Name = TEXT("Representations");
	static const FString IfcRepresentationIdentifier_Name = TEXT("RepresentationIdentifier");
	static const FString IfcItem_Name = TEXT("Item");
	static const FString IfcItems_Name = TEXT("Items");
	static const FString IfcMappingSource_Name = TEXT("MappingSource");
	static const FString IfcMappedRepresentation_Name = TEXT("MappedRepresentation");
	static const FString IfcUnitsInContext_Name = TEXT("UnitsInContext");
	static const FString IfcUnits_Name = TEXT("Units");
	static const FString IfcConversionFactor_Name = TEXT("ConversionFactor");
	static const FString IfcUnitComponent_Name = TEXT("UnitComponent");
	static const FString IfcValueComponent_Name = TEXT("ValueComponent");
	static const FString IfcStyles_Name = TEXT("Styles");
	static const FString IfcHasAssociations_Name = TEXT("HasAssociations");
	static const FString IfcHasRepresentation_Name = TEXT("HasRepresentation");
	static const FString IfcRelatingObject_Name = TEXT("RelatingObject");
	static const FString IfcRelatedObjects_Name = TEXT("RelatedObjects");
	static const FString IfcRelatedElements_Name = TEXT("RelatedElements");
	static const FString IfcRelatingStructure_Name = TEXT("RelatingStructure");
	static const FString IfcMaterial_Name = TEXT("Material");
	static const FString IfcRelatingMaterial_Name = TEXT("RelatingMaterial");
	static const FString IfcForLayerSet_Name = TEXT("ForLayerSet");
	static const FString IfcMaterialLayers_Name = TEXT("MaterialLayers");
	static const FString IfcRelatingPropertyDefinition_Name = TEXT("RelatingPropertyDefinition");
	static const FString IfcPropertyDefinitionOf_Name = TEXT("PropertyDefinitionOf");
	static const FString IfcDefinesType_Name = TEXT("DefinesType");
	static const FString IfcHasProperties_Name = TEXT("HasProperties");
	static const FString IfcQuantities_Name = TEXT("Quantities");
	static const FString IfcContainsElements_Name = TEXT("ContainsElements");

	/**
	 * Flags to use in geometry configuration.
	 */
	static const int_t flagbit0 = (1 << 0);
	static const int_t flagbit1 = (1 << 1);
	static const int_t flagbit2 = (1 << 2);
	static const int_t flagbit3 = (1 << 3);
	static const int_t flagbit4 = (1 << 4);
	static const int_t flagbit5 = (1 << 5);
	static const int_t flagbit6 = (1 << 6);
	static const int_t flagbit7 = (1 << 7);
	static const int_t flagbit8 = (1 << 8);
	static const int_t flagbit9 = (1 << 9);
	static const int_t flagbit10 = (1 << 10);
	static const int_t flagbit11 = (1 << 11);
	static const int_t flagbit12 = (1 << 12);
	static const int_t flagbit13 = (1 << 13);
	static const int_t flagbit14 = (1 << 14);
	static const int_t flagbit15 = (1 << 15);
	static const int_t flagbit24 = (1 << 24);
	static const int_t flagbit25 = (1 << 25);
	static const int_t flagbit26 = (1 << 26);
	static const int_t flagbit27 = (1 << 27);

	namespace Helper
	{
		// Note: This might modify InOutIFCModel, as we will close the file and open it again with the right schema, and
		// ifc_engine might give us a new model id or reuse the same one
		bool ApplySchema(int64& InOutIFCModel, const FString& InFilePath, FString &OutSettingsPath, FString &OutIFCVersion)
		{
			if (InOutIFCModel == 0)
			{
				return false;
			}

			FString ResourcesDir = IPluginManager::Get().FindPlugin(TEXT("DatasmithIFCImporter"))->GetBaseDir() / TEXT("Resources/IFC/");

			wchar_t * szSchema = NULL;
			GetSPFFHeaderItem(InOutIFCModel, 9, 0, sdaiUNICODE, (char **)&szSchema);
			OutIFCVersion = FString(szSchema);

			if ((szSchema == NULL) ||
				(wcsstr(szSchema, TEXT("IFC2")) != NULL))
			{
				sdaiCloseModel(InOutIFCModel);

				InOutIFCModel = sdaiOpenModelBNUnicode(0, (char*)*InFilePath, (char*) *(ResourcesDir + TEXT("IFC2X3_TC1.exp")));
				if (InOutIFCModel == 0)
				{
					return false;
				}

				OutSettingsPath = ResourcesDir + TEXT("IFC2X3-Settings.xml");

				return true;
			}

			if ((wcsstr(szSchema,TEXT("IFC4x")) != NULL) ||
				(wcsstr(szSchema,TEXT("IFC4X")) != NULL))
			{
				sdaiCloseModel(InOutIFCModel);

				InOutIFCModel = sdaiOpenModelBNUnicode(0, (char*)*InFilePath, (char*) *(ResourcesDir + TEXT("IFC4x1_FINAL.exp")));
				if (InOutIFCModel == 0)
				{
					return false;
				}

				OutSettingsPath = ResourcesDir + TEXT("IFC4-Settings.xml");

				return true;
			}

			if ((wcsstr(szSchema,TEXT("IFC4")) != NULL) ||
				(wcsstr(szSchema,TEXT("IFC2x4")) != NULL) ||
				(wcsstr(szSchema,TEXT("IFC2X4")) != NULL))
			{
				sdaiCloseModel(InOutIFCModel);

				InOutIFCModel = sdaiOpenModelBNUnicode(0, (char*)*InFilePath, (char*) *(ResourcesDir + TEXT("IFC4_ADD2.exp")));
				if (InOutIFCModel == 0)
				{
					return false;
				}

				OutSettingsPath = ResourcesDir + TEXT("IFC4-Settings.xml");

				return true;
			}

			return false;
		}

		void GetStringAttribute(int_t InInstance, const FString& InName, FString& OutString)
		{
			wchar_t	* value = 0;
			sdaiGetAttrBN(InInstance, (char*)*InName, sdaiUNICODE, &value);
			OutString = FString(value);
		}

		void GetADBAttribute(int_t InInstance, const FString& InName, FString& OutType, FString& OutValue)
		{
			int_t ADB = 0;
			sdaiGetAttrBN(InInstance, (char*)*InName, sdaiADB, &ADB);
			if (ADB != 0)
			{
				OutType = FString((wchar_t *)sdaiGetADBTypePath((void*)ADB, 0));
			}

			GetStringAttribute(InInstance, InName, OutValue);
		}

		void GetFloatAttribute(int_t InInstance, const FString& InName, float& OutValue)
		{
			double Value = 0.0;
			sdaiGetAttrBN(InInstance, (char*)*InName, sdaiREAL, &Value);
			OutValue = (float)Value;
		}

		void GetRGBColorAttribute(int_t InInstance, const FString& InName, FLinearColor& OutColor)
		{
			// IfcColourRgb
			int_t IfcColourRgbInstance = 0;
			sdaiGetAttrBN(InInstance, (char*)*InName, sdaiINSTANCE, &IfcColourRgbInstance);
			if (IfcColourRgbInstance == 0)
			{
				return;
			}

			GetFloatAttribute(IfcColourRgbInstance, TEXT("Red"), OutColor.R);
			GetFloatAttribute(IfcColourRgbInstance, TEXT("Green"), OutColor.G);
			GetFloatAttribute(IfcColourRgbInstance, TEXT("Blue"), OutColor.B);
		}

		void GetRGBColorOrFactorAttribute(int_t InInstance, const FString& InName, const FLinearColor& InColor, FLinearColor& OutColor)
		{
			// IfcColourOrFactor
			int_t IfcColourRgbInstance = 0;
			sdaiGetAttrBN(InInstance, (char*)*InName, sdaiINSTANCE, &IfcColourRgbInstance);
			if (IfcColourRgbInstance == 0)
			{
				int_t ADB = 0;
				sdaiGetAttrBN(InInstance, (char*)*InName, sdaiADB, &ADB);
				if (ADB != 0)
				{
					double Factor = 0;
					sdaiGetADBValue((void *)ADB, sdaiREAL, &Factor);

					OutColor.R = InColor.R * (float)Factor;
					OutColor.G = InColor.G * (float)Factor;
					OutColor.B = InColor.B * (float)Factor;
				}

				return;
			}

			GetRGBColorAttribute(InInstance, InName, OutColor);
		}

		float ParseSIPrefixText(const FString& Text)
		{
			if (Text.Equals(TEXT(".METRE.")))			return 1.0f;
			else if (Text.Equals(TEXT(".INCH.")))		return 0.0254f;
			else if (Text.Equals(TEXT(".CENTI.")))		return 0.01f;
			else if (Text.Equals(TEXT(".MILLI.")))		return 0.001f;
			else if (Text.Equals(TEXT(".MICRO.")))		return 0.000001f;

			return 1.0f;	// Default.
		}

		ESurfaceSide ParseSurfaceSideText(const FString& Text)
		{
			if (Text.Equals(TEXT(".POSITIVE.")))		return ESurfaceSide::Positive;
			if (Text.Equals(TEXT(".NEGATIVE.")))		return ESurfaceSide::Negative;

			return ESurfaceSide::Both;	// Most common default.
		}

		// The cast of a TCHAR pointer to a char one is intentional, going through casts avoid compiler's warnings
		int_t LocalsdaiGetEntity( int_t model, const TCHAR* entityName )
		{
			return sdaiGetEntity( model, reinterpret_cast<char*>(const_cast<TCHAR*>(entityName)) );
		}

		void LocalsdaiGetAttrBN(int_t instance, const TCHAR* attributeName, int_t valueType, void* value)
		{
			sdaiGetAttrBN( instance, reinterpret_cast<char*>(const_cast<TCHAR*>(attributeName)), valueType, value );
		}

	} // namespace Helper

#endif

	FFileReader::FFileReader() {}

	FFileReader::~FFileReader() {}

	float FFileReader::GetLengthUnit() const
	{
		const FUnit* Unit = UnitTypesMap.Find(TEXT(".LENGTHUNIT."));
		if (Unit != nullptr)
		{
			return Unit->PrefixValue;
		}
		return 1.0f;	// Default.
	}

#ifdef WITH_IFC_ENGINE_LIB
	void FFileReader::GetEntitiesTypes()
	{
		IfcMappedItemEntity = sdaiGetEntity(gIFCModel, (char*)*IfcMappedItem_Type);
		IfcConversionBasedUnitEntity = sdaiGetEntity(gIFCModel, (char*)*IfcConversionBasedUnit_Type);
		IfcSIUnitEntity = sdaiGetEntity(gIFCModel, (char*)*IfcSIUnit_Type);
		IfcSurfaceStyleEntity = sdaiGetEntity(gIFCModel, (char*)*IfcSurfaceStyle_Type);
		IfcSurfaceStyleRenderingEntity = sdaiGetEntity(gIFCModel, (char*)*IfcSurfaceStyleRendering_Type);
		IfcSurfaceStyleShadingEntity = sdaiGetEntity(gIFCModel, (char*)*IfcSurfaceStyleShading_Type);
		IfcRelAssociatesMaterialEntity = sdaiGetEntity(gIFCModel, (char*)*IfcRelAssociatesMaterial_Type);
		IfcMaterialEntity = sdaiGetEntity(gIFCModel, (char*)*IfcMaterial_Type);
		IfcMaterialLayerSetUsageEntity = sdaiGetEntity(gIFCModel, (char*)*IfcMaterialLayerSetUsage_Type);
		IfcMaterialLayerSetEntity = sdaiGetEntity(gIFCModel, (char*)*IfcMaterialLayerSet_Type);
		IfcMaterialLayerEntity = sdaiGetEntity(gIFCModel, (char*)*IfcMaterialLayer_Type);
		IfcStyledItemEntity = sdaiGetEntity(gIFCModel, (char*)*IfcStyledItem_Type);
		IfcColourRgbEntity = sdaiGetEntity(gIFCModel, (char*)*IfcColourRgb_Type);
		IfcPropertySetEntity = sdaiGetEntity(gIFCModel, (char*)*IfcPropertySet_Type);
		IfcElementQuantityEntity = sdaiGetEntity(gIFCModel, (char*)*IfcElementQuantity_Type);
		IfcPropertySingleValueEntity = sdaiGetEntity(gIFCModel, (char*)*IfcPropertySingleValue_Type);
		IfcQuantityLengthEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityLength_Type);
		IfcQuantityAreaEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityArea_Type);
		IfcQuantityVolumeEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityVolume_Type);
		IfcQuantityCountEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityCount_Type);
		IfcQuantityWeigthEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityWeigth_Type);
		IfcQuantityTimeEntity = sdaiGetEntity(gIFCModel, (char*)*IfcQuantityTime_Type);
		IfcBuildingEntity = sdaiGetEntity(gIFCModel, (char*)*IfcBuilding_Type);
		IfcBuildingStoreyEntity = sdaiGetEntity(gIFCModel, (char*)*IfcBuildingStorey_Type);
		IfcDistributionElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcDistributionElement_Type);
		IfcElectricalElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcElectricalElement_Type);
		IfcElementAssemblyEntity = sdaiGetEntity(gIFCModel, (char*)*IfcElementAssembly_Type);
		IfcElementComponentEntity = sdaiGetEntity(gIFCModel, (char*)*IfcElementComponent_Type);
		IfcEquipmentElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcEquipmentElement_Type);
		IfcFeatureElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcFeatureElement_Type);
		IfcFurnishingElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcFurnishingElement_Type);
		IfcTransportElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcTransportElement_Type);
		IfcVirtualElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcVirtualElement_Type);
		IfcReinforcingElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcReinforcingElement_Type);
		IfcOpeningElementEntity = sdaiGetEntity(gIFCModel, (char*)*IfcOpeningElement_Type);
	}
#endif

	bool FFileReader::ReadFile(const FString& InFilePath)
	{
		CleanUp();		// In the ideal case this is not needed.

		Messages.Empty();

#ifdef WITH_IFC_ENGINE_LIB
		setStringUnicode(1);

		gIFCModel = sdaiOpenModelBNUnicode(0, (char*)*InFilePath, 0);
		if (gIFCModel == 0)
		{
			Messages.Emplace(EMessageSeverity::Error, TEXT("Can't load IFC file:") + InFilePath);
			return false;
		}

		FString SettingsPath;
		if (!Helper::ApplySchema(gIFCModel, InFilePath, SettingsPath, IFCVersion))
		{
			Messages.Emplace(EMessageSeverity::Error, TEXT("Can't load IFC file shema:") + InFilePath);
			return false;
		}

		if (!XMLFileSettings.LoadFile(SettingsPath, EConstructMethod::ConstructFromFile))
		{
			Messages.Emplace(EMessageSeverity::Error, TEXT("Can't load IFC xml file settings:") + SettingsPath);
			return false;
		}

		UE_LOG(LogDatasmithIFCReader, Log, TEXT("IFC file loaded"));

		return true;
#else
		Messages.Emplace(EMessageSeverity::Error, TEXT("Check plugin configuration!"));
		return false;
#endif
	}

	void FFileReader::PostReadFile(float InSimilarColorTolerance, bool InCreateMissingMaterials)
	{
		SimilarColorTolerance = InSimilarColorTolerance;
		bCreateMissingMaterials = InCreateMissingMaterials;

#ifdef WITH_IFC_ENGINE_LIB

		check(!sdaiErrorQuery());

		setBRepProperties(
			gIFCModel,
			1 + 2 + 4 + 32,	// + 64,
			0.92,
			0.000001,
			600
		);

		GetEntitiesTypes();

		GatherMaterials(TEXT("IfcStyledItem"));
		GatherMaterialAssociates(TEXT("IfcRelAssociatesMaterial"));

		const FXmlNode* RootNode = XMLFileSettings.GetRootNode();
		for (const FXmlNode* Node = RootNode->GetFirstChildNode(); Node; Node = Node->GetNextNode())
		{
			if (Node->GetTag().Equals(TEXT("object")))
			{
				FString NameNode = Node->GetAttribute(TEXT("name"));

				FString HideNode = Node->GetAttribute(TEXT("hide"));
				bool bVisible = !HideNode.Equals(TEXT("true"));

				FString SegmentsNode = Node->GetAttribute(TEXT("segments"));
				int64 iCircleSegments = FCString::Atoi64(*SegmentsNode);

				//GatherObjects(NameNode, bVisible, iCircleSegments > 0 ? iCircleSegments : DefaultCircleSegments);
			}
		}

		// gather starting from root entity for all spacial/geometry objects - IfcProduct
		// http://www.buildingsmart-tech.org/ifc/IFC2x3/TC1/html/ifckernel/lexical/ifcproduct.htm
		GatherObjectsEntityHierarchy(gIFCModel, Helper::LocalsdaiGetEntity(gIFCModel, TEXT("IFCPRODUCT")), true, 72);

		GatherProperties(TEXT("IfcRelDefinesByProperties"));
		AssignProperties();

		int_t* ProjectsEntities = sdaiGetEntityExtentBN(gIFCModel, (char*)*IfcPoject_Type);
		int_t ProjectsEntitiesCount = sdaiGetMemberCount(ProjectsEntities);
		for (int_t I = 0; I < ProjectsEntitiesCount; ++I)
		{
			int_t ProjectInstance = 0;
			engiGetAggrElement(ProjectsEntities, I, sdaiINSTANCE, &ProjectInstance);

			GatherUnits(gIFCModel, ProjectInstance);

			{
				int_t LocalInstance = ProjectInstance;
				FString EntityName = TEXT("ifcProject");
				bool bVisible = true;

				int32 NewObjIndex = IFCObjects.AddDefaulted();
				FObject& NewIFCObject = IFCObjects.Last();

				NewIFCObject.IfcInstance = LocalInstance;
				NewIFCObject.bVisible = bVisible;

				IFCInstanceToObjectIdMap.Add(LocalInstance, NewObjIndex);

				NewIFCObject.Type = *EntityName;

				Helper::GetStringAttribute(LocalInstance, TEXT("GlobalId"), NewIFCObject.GlobalId);
				Helper::GetStringAttribute(LocalInstance, TEXT("Name"), NewIFCObject.Name);
				Helper::GetStringAttribute(LocalInstance, TEXT("Description"), NewIFCObject.Description);

				IFCGlobalIdToObjectIdMap.Add(NewIFCObject.GlobalId, NewObjIndex);

				if (NewIFCObject.Name.IsEmpty())
				{
					NewIFCObject.Name = NewIFCObject.Type;
				}
			}

			IFCProjects.Add(*IFCInstanceToObjectIdMap.Find(ProjectInstance));
			GatherSpatialHierarchy(gIFCModel, ProjectInstance);


			if (ProjectsEntitiesCount > 1)
			{
				Messages.Emplace(EMessageSeverity::Warning, TEXT("Reduced IFC file support with multiple IFCPROJECTs inside!"));
			}
			break;	// Assume there is only one project in one IFC file.
		}

 		//check(UnitTypesMap.Num());

 		GatherRelContained(TEXT("IfcRelContainedInSpatialStructure"));
#endif
	}

#ifdef WITH_IFC_ENGINE_LIB
	void FFileReader::GatherSpatialHierarchy(int64 InIFCModel, int64 InObjectInstance)
	{

		int32* FoundIndex = IFCInstanceToObjectIdMap.Find(InObjectInstance);

		check(FoundIndex && IFCObjects.IsValidIndex(*FoundIndex));

		FObject& Object = IFCObjects[*FoundIndex];

		int_t RelAggregatesEntity = Helper::LocalsdaiGetEntity(InIFCModel, TEXT("IFCRELAGGREGATES"));
		int_t* DecomposedByInstances = nullptr;
		Helper::LocalsdaiGetAttrBN(InObjectInstance, TEXT("IsDecomposedBy"), sdaiAGGR, &DecomposedByInstances);
		if (DecomposedByInstances)
		{
			int_t DecomposedByInstancesCount = sdaiGetMemberCount(DecomposedByInstances);;

			for (int_t DecomposedByInstanceIndex = 0;DecomposedByInstanceIndex < DecomposedByInstancesCount; ++DecomposedByInstanceIndex)
			{
				int_t DecomposedByInstance = 0;
				engiGetAggrElement(DecomposedByInstances, DecomposedByInstanceIndex, sdaiINSTANCE, &DecomposedByInstance);

				if (sdaiGetInstanceType(DecomposedByInstance) == RelAggregatesEntity)
				{
					int_t* ChildInstances = nullptr;
					sdaiGetAttrBN(DecomposedByInstance, (char*)*IfcRelatedObjects_Name, sdaiAGGR, &ChildInstances);
					int_t ChildInstancesCount = sdaiGetMemberCount(ChildInstances);
					if (ChildInstancesCount)
					{
						for (int_t ChildInstanceIndex = 0; ChildInstanceIndex < ChildInstancesCount; ++ChildInstanceIndex)
						{
							int_t ChildInstance = 0;
							engiGetAggrElement(ChildInstances, ChildInstanceIndex, sdaiINSTANCE, &ChildInstance);

							int32* ObjectIndexPtr = IFCInstanceToObjectIdMap.Find(ChildInstance);
							if (ObjectIndexPtr && IFCObjects.IsValidIndex(*ObjectIndexPtr))
							{
								Object.DecomposedBy.Add(*ObjectIndexPtr);

								GatherSpatialHierarchy(InIFCModel, ChildInstance);
							}
							else
							{
								TCHAR* EntityNamePtr = nullptr;
								engiGetEntityName(sdaiGetInstanceType(ChildInstance), sdaiUNICODE, (char**)&EntityNamePtr);
								FString EntityName = EntityNamePtr;
								Messages.Emplace(EMessageSeverity::Warning, TEXT("Undefined Entity Type: ") + EntityName);
							}
						}
					}
				}
			}
		}

		int_t RelContainedInSpatialStructureEntity = Helper::LocalsdaiGetEntity(InIFCModel, TEXT("IFCRELCONTAINEDINSPATIALSTRUCTURE") );
		int_t* ContainsElementsInstances = nullptr;
		Helper::LocalsdaiGetAttrBN(InObjectInstance, TEXT("ContainsElements"), sdaiAGGR, &ContainsElementsInstances);
		if (ContainsElementsInstances)
		{
			int_t ContainsElementsInstancesCount = sdaiGetMemberCount(ContainsElementsInstances);;

			for (int_t ContainsElementsInstanceIndex = 0; ContainsElementsInstanceIndex < ContainsElementsInstancesCount; ++ContainsElementsInstanceIndex)
			{
				int_t ContainsElementsInstance = 0;
				engiGetAggrElement(ContainsElementsInstances, ContainsElementsInstanceIndex, sdaiINSTANCE, &ContainsElementsInstance);

				if (sdaiGetInstanceType(ContainsElementsInstance) == RelContainedInSpatialStructureEntity)
				{
					int_t* ChildInstances = nullptr;
					sdaiGetAttrBN(ContainsElementsInstance, (char*)*IfcRelatedElements_Name, sdaiAGGR, &ChildInstances);
					int_t ChildInstancesCount = sdaiGetMemberCount(ChildInstances);
					if (ChildInstancesCount)
					{
						for (int_t ChildInstanceIndex = 0; ChildInstanceIndex < ChildInstancesCount; ++ChildInstanceIndex)
						{
							int_t ChildInstance = 0;
							engiGetAggrElement(ChildInstances, ChildInstanceIndex, sdaiINSTANCE, &ChildInstance);

							int32* ObjectIndexPtr = IFCInstanceToObjectIdMap.Find(ChildInstance);
							check(ObjectIndexPtr && IFCObjects.IsValidIndex(*ObjectIndexPtr));

							Object.ContainsElements.Add(*ObjectIndexPtr);

							GatherSpatialHierarchy(InIFCModel, ChildInstance);
						}
					}

				}

			}
		}

	}
#endif

	void FFileReader::CleanUp()
	{
		Messages.Empty();

		IFCMaterialsMap.Empty();
		IFCRootObjects.Empty();
		IFCProjects.Empty();
		IFCObjects.Empty();
		IFCFProperties.Empty();
		UnitTypesMap.Empty();

		IFCGlobalIdToObjectIdMap.Empty();
		IFCInstanceToObjectIdMap.Empty();
		ShapeIdToMaterialIdMap.Empty();
		ShapeIdToObjectIdMap.Empty();

#ifdef WITH_IFC_ENGINE_LIB
		XMLFileSettings.Clear();

		if (gIFCModel)
		{
			cleanMemory(gIFCModel, 4);
			sdaiCloseModel(gIFCModel);
			gIFCModel = 0;
		}
#endif
	}

	const FObject* FFileReader::FindObject(int64 ObjectId) const
	{
		const int32* FoundIndex = IFCInstanceToObjectIdMap.Find(ObjectId);
		if (FoundIndex != nullptr && IFCObjects.IsValidIndex(*FoundIndex))
		{
			return &IFCObjects[*FoundIndex];
		}
		return nullptr;
	}

	const FObject* FFileReader::FindObjectFromGlobalId(const FString& InGlobalId) const
	{
		const int32* FoundIndex = IFCGlobalIdToObjectIdMap.Find(InGlobalId);
		if (FoundIndex != nullptr && IFCObjects.IsValidIndex(*FoundIndex))
		{
			return &IFCObjects[*FoundIndex];
		}
		return nullptr;
	}

	const FObject* FFileReader::FindObjectFromShapeId(int64 InShapeId) const
	{
		if (ShapeIdToObjectIdMap.Contains(InShapeId))
		{
			if (IFCObjects.IsValidIndex(ShapeIdToObjectIdMap[InShapeId]))
			{
				return (const FObject*) &IFCObjects[ShapeIdToObjectIdMap[InShapeId]];
			}
		}
		return nullptr;
	}

#ifdef WITH_IFC_ENGINE_LIB

	void FFileReader::GatherUnits(int64 InIFCModel, int64 InProjectInstance)
	{
		int_t UnitsInContextInstance = 0;
		sdaiGetAttrBN(InProjectInstance, (char*) *IfcUnitsInContext_Name, sdaiINSTANCE, &UnitsInContextInstance);

		int_t* UnitsSet = 0;
		sdaiGetAttrBN(UnitsInContextInstance, (char*) *IfcUnits_Name, sdaiAGGR, &UnitsSet);
		int_t UnitsSetCount = sdaiGetMemberCount(UnitsSet);

		for (int_t I = 0; I < UnitsSetCount; ++I)
		{
			int_t UnitInstance = 0;
			engiGetAggrElement(UnitsSet, I, sdaiINSTANCE, &UnitInstance);

			if (sdaiGetInstanceType(UnitInstance) == IfcConversionBasedUnitEntity)
			{
				// http://standards.buildingsmart.org/IFC/RELEASE/IFC2x3/TC1/HTML/ifcmeasureresource/lexical/ifcmeasurewithunit.htm
				int_t MeasureWithUnitInstance = 0;
				sdaiGetAttrBN(UnitInstance, (char*) *IfcConversionFactor_Name, sdaiINSTANCE, &MeasureWithUnitInstance);

				if (ensure(MeasureWithUnitInstance))
				{
					// get SI unit from conversion unit
					int_t SiUnitInstance = 0;
					sdaiGetAttrBN(MeasureWithUnitInstance, (char*) *IfcUnitComponent_Name, sdaiINSTANCE, &SiUnitInstance);

					int_t* ADB = 0;
					sdaiGetAttrBN(MeasureWithUnitInstance, (char*) *IfcValueComponent_Name, sdaiADB, &ADB);

					double Value = 0;
					sdaiGetADBValue((void*)ADB, sdaiREAL, &Value);

					if (sdaiGetInstanceType(SiUnitInstance) == IfcSIUnitEntity)
					{
						FUnit NewUnit;

						Helper::GetStringAttribute(SiUnitInstance, TEXT("UnitType"), NewUnit.Type);
						Helper::GetStringAttribute(SiUnitInstance, TEXT("Prefix"), NewUnit.Prefix);
						Helper::GetStringAttribute(SiUnitInstance, TEXT("Name"), NewUnit.Name);

						if (NewUnit.Type.Equals(TEXT(".LENGTHUNIT.")))
						{
							NewUnit.PrefixValue = Helper::ParseSIPrefixText(NewUnit.Prefix) * Value; // Factor in conversion value

							UE_LOG(LogDatasmithIFCReader, Log, TEXT("LengthUnit = %f"), NewUnit.PrefixValue);
						}

						UnitTypesMap.Add(NewUnit.Type, NewUnit);
					}
				}
			}
			else if (sdaiGetInstanceType(UnitInstance) == IfcSIUnitEntity)
			{
				FUnit NewUnit;

				Helper::GetStringAttribute(UnitInstance, TEXT("UnitType"), NewUnit.Type);
				Helper::GetStringAttribute(UnitInstance, TEXT("Prefix"), NewUnit.Prefix);
				Helper::GetStringAttribute(UnitInstance, TEXT("Name"), NewUnit.Name);

				if (NewUnit.Type.Equals(TEXT(".LENGTHUNIT.")))
				{
					NewUnit.PrefixValue = Helper::ParseSIPrefixText(NewUnit.Prefix);

					UE_LOG(LogDatasmithIFCReader, Log, TEXT("LengthUnit = %f"), NewUnit.PrefixValue);
				}

				UnitTypesMap.Add(NewUnit.Type, NewUnit);
			}
		}
	}

	void FFileReader::GetMaterialInstance(int64 IfcMaterialLayer, TArray<FString>& OutName, TArray<int64>& OutIfcStyledItemInstances)
	{
		FString NewName;
		Helper::GetStringAttribute(IfcMaterialLayer, TEXT("Name"), NewName);
		OutName.Add(NewName);

		int_t* IfcMaterialDefinitionRepresentationAggr = nullptr;
		sdaiGetAttrBN(IfcMaterialLayer, (char*) *IfcHasRepresentation_Name, sdaiAGGR, &IfcMaterialDefinitionRepresentationAggr);
		int_t IfcMaterialDefinitionRepresentationAggrCnt = sdaiGetMemberCount(IfcMaterialDefinitionRepresentationAggr);
		for (int_t I = 0; I < IfcMaterialDefinitionRepresentationAggrCnt; I++)
		{
			int_t IfcMaterialDefinitionRepresentationInstance = 0;
			engiGetAggrElement(IfcMaterialDefinitionRepresentationAggr, I, sdaiINSTANCE, &IfcMaterialDefinitionRepresentationInstance);

			int_t* IfcMaterialRepresentationAggr = nullptr;
			sdaiGetAttrBN(IfcMaterialDefinitionRepresentationInstance, (char*) *IfcRepresentations_Name, sdaiAGGR, &IfcMaterialRepresentationAggr);
			int_t IfcMaterialRepresentationAggrCnt = sdaiGetMemberCount(IfcMaterialRepresentationAggr);
			for (int_t R = 0; R < IfcMaterialRepresentationAggrCnt; R++)
			{
				int_t IfcMaterialRepresentationInstance = 0;
				engiGetAggrElement(IfcMaterialRepresentationAggr, R, sdaiINSTANCE, &IfcMaterialRepresentationInstance);

				int_t* IfcMaterialItemsAggr = nullptr;
				sdaiGetAttrBN(IfcMaterialRepresentationInstance, (char*) *IfcItems_Name, sdaiAGGR, &IfcMaterialItemsAggr);
				int_t IfcMaterialItemsAggrCnt = sdaiGetMemberCount(IfcMaterialItemsAggr);
				for (int_t M = 0; M < IfcMaterialItemsAggrCnt; M++)
				{
					int_t IfcRepresentationInstance = 0;
					engiGetAggrElement(IfcMaterialItemsAggr, M, sdaiINSTANCE, &IfcRepresentationInstance);

					if (sdaiGetInstanceType(IfcRepresentationInstance) == IfcStyledItemEntity)
					{
						OutIfcStyledItemInstances.Add(IfcRepresentationInstance);
					}
				}
			}
		}
	}

	void FFileReader::GetMaterialLayers(int64 IfcMaterialLayerSet, TArray<FString>& OutNames, TArray<int64>& OutIfcStyledItemInstances)
	{
		int_t* IfcMaterialLayerAggr = nullptr;
		sdaiGetAttrBN(IfcMaterialLayerSet, (char*) *IfcMaterialLayers_Name, sdaiAGGR, &IfcMaterialLayerAggr);
		int_t IfcMaterialLayerAggrCnt = sdaiGetMemberCount(IfcMaterialLayerAggr);
		for (int_t I = 0; I < IfcMaterialLayerAggrCnt; I++)
		{
			int_t IfcMaterialLayer = 0;
			engiGetAggrElement(IfcMaterialLayerAggr, I, sdaiINSTANCE, &IfcMaterialLayer);
			if (sdaiGetInstanceType(IfcMaterialLayer) == IfcMaterialLayerEntity)
			{
				int_t IfcMaterialInstance = 0;
				sdaiGetAttrBN(IfcMaterialLayer, (char*) *IfcMaterial_Name, sdaiINSTANCE, &IfcMaterialInstance);
				GetMaterialInstance(IfcMaterialInstance, OutNames, OutIfcStyledItemInstances);
			}
		}
	}

	void FFileReader::GetRelatingMaterial(int64 IfcMaterialSelectInstance, TArray<FString>& OutNames, TArray<int64>& OutIfcStyledItemInstances)
	{
		if (sdaiGetInstanceType(IfcMaterialSelectInstance) == IfcMaterialEntity)
		{
			GetMaterialInstance(IfcMaterialSelectInstance, OutNames, OutIfcStyledItemInstances);
		}
		else if (sdaiGetInstanceType(IfcMaterialSelectInstance) == IfcMaterialLayerSetUsageEntity)
		{
			int_t IfcMaterialLayerSetInstance = 0;
			sdaiGetAttrBN(IfcMaterialSelectInstance, (char*) *IfcForLayerSet_Name, sdaiINSTANCE, &IfcMaterialLayerSetInstance);
			GetMaterialLayers(IfcMaterialLayerSetInstance, OutNames, OutIfcStyledItemInstances);
		}
		else if (sdaiGetInstanceType(IfcMaterialSelectInstance) == IfcMaterialLayerSetEntity)
		{
			GetMaterialLayers(IfcMaterialSelectInstance, OutNames, OutIfcStyledItemInstances);
		}
		else if (sdaiGetInstanceType(IfcMaterialSelectInstance) == IfcMaterialLayerEntity)
		{
			int_t IfcMaterialInstance = 0;
			sdaiGetAttrBN(IfcMaterialSelectInstance, (char*) *IfcMaterial_Name, sdaiINSTANCE, &IfcMaterialInstance);
			GetMaterialInstance(IfcMaterialInstance, OutNames, OutIfcStyledItemInstances);
		}
	}

	void FFileReader::ParsePresentationStyle(int64 InPresentationStyle, FMaterial& OutMaterial)
	{
		if (sdaiGetInstanceType(InPresentationStyle) == IfcSurfaceStyleEntity)
		{
			OutMaterial.ID = InPresentationStyle;
			Helper::GetStringAttribute(InPresentationStyle, TEXT("Name"), OutMaterial.Name);
			if (OutMaterial.Name.IsEmpty())
			{
				OutMaterial.Name = TEXT("IfcSurfaceStyle");
			}

			FString TempText;
			Helper::GetStringAttribute(InPresentationStyle, TEXT("Side"), TempText);
			OutMaterial.SurfaceSide = Helper::ParseSurfaceSideText(TempText);

			int_t* StylesEntities = 0;
			sdaiGetAttrBN(InPresentationStyle, (char *) *IfcStyles_Name, sdaiAGGR, &StylesEntities);
			int_t StylesEntitiesCount = sdaiGetMemberCount(StylesEntities);
			for (int_t S = 0; S < StylesEntitiesCount; ++S)
			{
				// IfcSurfaceStyleElementSelect
				int_t SurfaceStyleInstance = 0;
				engiGetAggrElement(StylesEntities, S, sdaiINSTANCE, &SurfaceStyleInstance);
				if (sdaiGetInstanceType(SurfaceStyleInstance) == IfcSurfaceStyleRenderingEntity)
				{
					if (S == 0 && StylesEntitiesCount == 1)
					{
						OutMaterial.ID = SurfaceStyleInstance;
					}
					else
					{
						Messages.Emplace(EMessageSeverity::Warning, TEXT("Unsupported number of IfcSurfaceStyleElementSelect > 1 found!"));
					}

					Helper::GetRGBColorAttribute(SurfaceStyleInstance, TEXT("SurfaceColour"), OutMaterial.SurfaceColour);
					Helper::GetFloatAttribute(SurfaceStyleInstance, TEXT("Transparency"), OutMaterial.Transparency);
					Helper::GetRGBColorOrFactorAttribute(SurfaceStyleInstance, TEXT("DiffuseColour"), OutMaterial.SurfaceColour, OutMaterial.DiffuseColour);

					Helper::GetRGBColorOrFactorAttribute(SurfaceStyleInstance, TEXT("TransmissionColour"), OutMaterial.SurfaceColour, OutMaterial.TransmissionColour);
					Helper::GetRGBColorOrFactorAttribute(SurfaceStyleInstance, TEXT("DiffuseTransmissionColour"), OutMaterial.SurfaceColour, OutMaterial.DiffuseTransmissionColour);
					Helper::GetRGBColorOrFactorAttribute(SurfaceStyleInstance, TEXT("ReflectionColour"), OutMaterial.SurfaceColour, OutMaterial.ReflectionColour);
					Helper::GetRGBColorOrFactorAttribute(SurfaceStyleInstance, TEXT("SpecularColour"), OutMaterial.SurfaceColour, OutMaterial.SpecularColour);

					FString OutType;
					FString ReflectanceMethod;
					Helper::GetADBAttribute(SurfaceStyleInstance, TEXT("ReflectanceMethod"), OutType, ReflectanceMethod);

					FString specular_exponent;
					Helper::GetADBAttribute(SurfaceStyleInstance, TEXT("specular_exponent"), OutType, specular_exponent);

				}
				else if (sdaiGetInstanceType(SurfaceStyleInstance) == IfcSurfaceStyleShadingEntity)
				{
					if (S == 0 && StylesEntitiesCount == 1)
					{
						OutMaterial.ID = SurfaceStyleInstance;
					}
					else
					{
						Messages.Emplace(EMessageSeverity::Warning, TEXT("Unsupported number of IfcSurfaceStyleElementSelect > 1 found!"));
					}

					Helper::GetRGBColorAttribute(SurfaceStyleInstance, TEXT("SurfaceColour"), OutMaterial.SurfaceColour);
					Helper::GetFloatAttribute(SurfaceStyleInstance, TEXT("Transparency"), OutMaterial.Transparency);
				}
				else
				{
					Messages.Emplace(EMessageSeverity::Warning, TEXT("Unsupported Instance of IfcSurfaceStyleElementSelect"));
				}
			}
		}
		else
		{
			Messages.Emplace(EMessageSeverity::Warning, TEXT("Unsupported Instance IfcPresentationStyleSelect != IfcSurfaceStyle"));
		}
	}

	void FFileReader::ParseStyledItem(int64 InStyledItemInstance, FMaterial& OutMaterial)
	{
		int_t* StylesEntities = 0;
		sdaiGetAttrBN(InStyledItemInstance, (char *) *IfcStyles_Name, sdaiAGGR, &StylesEntities);
		int_t StylesEntitiesCount = sdaiGetMemberCount(StylesEntities);
		for (int_t S = 0; S < StylesEntitiesCount; ++S)
		{
			// IfcPresentationStyleAssignment
			int_t StyleInstance = 0;
			engiGetAggrElement(StylesEntities, S, sdaiINSTANCE, &StyleInstance);

			int_t* PresentationEntities = 0;
			sdaiGetAttrBN(StyleInstance, (char *) *IfcStyles_Name, sdaiAGGR, &PresentationEntities);
			int_t PresentationEntitiesCount = sdaiGetMemberCount(PresentationEntities);
			for (int_t P = 0; P < PresentationEntitiesCount; ++P)
			{
				// IfcPresentationStyleSelect
				int_t SurfaceStyleInstance = 0;
				engiGetAggrElement(PresentationEntities, P, sdaiINSTANCE, &SurfaceStyleInstance);

				ParsePresentationStyle(SurfaceStyleInstance, OutMaterial);

				if (PresentationEntitiesCount > 1)
				{
					Messages.Emplace(EMessageSeverity::Warning, TEXT("Reduced IFC file support with multiple IfcPresentationStyleSelect's inside!"));
				}
				break;
			}
			if (StylesEntitiesCount > 1)
			{
				Messages.Emplace(EMessageSeverity::Warning, TEXT("Reduced IFC file support with multiple IfcPresentationStyleAssignment's inside!"));
			}
			break;
		}
	}

	void FFileReader::GatherMaterials(const FString& InName)
	{
		int_t* ObjectEntities = sdaiGetEntityExtentBN(gIFCModel, (char *) *InName);
		int_t ObjectEntitiesCount = sdaiGetMemberCount(ObjectEntities);
		if (ObjectEntitiesCount == 0)
		{
			return;	// No entities found.
		}

		for (int_t I = 0; I < ObjectEntitiesCount; ++I)
		{
			// IfcStyledItem
			int_t IfcStyledItemInstance = 0;
			engiGetAggrElement(ObjectEntities, I, sdaiINSTANCE, &IfcStyledItemInstance);

			int_t ItemInstance = 0;
			sdaiGetAttrBN(IfcStyledItemInstance, (char *) *IfcItem_Name, sdaiINSTANCE, &ItemInstance);

			if (ItemInstance == 0)
			{
				continue;
			}

			FMaterial NewMaterial;
			ParseStyledItem(IfcStyledItemInstance, NewMaterial);

			if (NewMaterial.ID > 0)
			{
				if (IFCMaterialsMap.Contains(NewMaterial.ID) == false)
				{
					IFCMaterialsMap.Add(NewMaterial.ID, NewMaterial);
				}

				ShapeIdToMaterialIdMap.Add(ItemInstance, NewMaterial.ID);
			}
		}
	}

	void FFileReader::GatherMaterialAssociates(const FString& InName)
	{
		int_t* ObjectEntities = sdaiGetEntityExtentBN(gIFCModel, (char *) *InName);
		int_t ObjectEntitiesCount = sdaiGetMemberCount(ObjectEntities);
		if (ObjectEntitiesCount == 0)
		{
			return;	// No entities found.
		}

		for (int_t I = 0; I < ObjectEntitiesCount; ++I)
		{
			// IfcRelAssociatesMaterial
			int_t LocalInstance = 0;
			engiGetAggrElement(ObjectEntities, I, sdaiINSTANCE, &LocalInstance);
			if (sdaiGetInstanceType(LocalInstance) == IfcRelAssociatesMaterialEntity)
			{
				// RelatedObjects
				TArray<int_t> RelatedObjects;
				RelatedObjects.Empty(5);
				int_t* IfcRelatedObjectsInstanceAggr = nullptr;
				sdaiGetAttrBN(LocalInstance, (char*) *IfcRelatedObjects_Name, sdaiAGGR, &IfcRelatedObjectsInstanceAggr);
				int_t IfcRelatedObjectsInstanceAggrCnt = sdaiGetMemberCount(IfcRelatedObjectsInstanceAggr);
				for (int_t O = 0; O < IfcRelatedObjectsInstanceAggrCnt; O++)
				{
					int_t IfcRelatedObjectsInstanceInstance = 0;
					engiGetAggrElement(IfcRelatedObjectsInstanceAggr, O, sdaiINSTANCE, &IfcRelatedObjectsInstanceInstance);

					// IfcProductRepresentation
					int_t IfcProductRepresentationInstance = 0;
					sdaiGetAttrBN(IfcRelatedObjectsInstanceInstance, (char*) *IfcRepresentation_Name, sdaiINSTANCE, &IfcProductRepresentationInstance);
					if (IfcProductRepresentationInstance != 0)
					{
						RelatedObjects.Add(IfcProductRepresentationInstance);
					}
				}

				// RelatingMaterial
				int_t IfcMaterialSelectInstance = 0;
				sdaiGetAttrBN(LocalInstance, (char*) *IfcRelatingMaterial_Name, sdaiINSTANCE, &IfcMaterialSelectInstance);

				TArray<FString> FoundNames;
				TArray<int_t> IfcStyledItemInstances;
				GetRelatingMaterial(IfcMaterialSelectInstance, FoundNames, IfcStyledItemInstances);

				for (int_t M = 0; M < IfcStyledItemInstances.Num(); ++M)
				{
					FMaterial NewMaterial;
					ParseStyledItem(IfcStyledItemInstances[M], NewMaterial);

					if (NewMaterial.ID > 0)
					{
						if (IFCMaterialsMap.Contains(NewMaterial.ID) == false)
						{
							IFCMaterialsMap.Add(NewMaterial.ID, NewMaterial);
						}

					}
				}
			}
		}
	}

	void FFileReader::GetShapeAssociations(int64 InModel, FObject& OutObject)
	{
		// IfcRelAssociates
		int_t* IfcRelAssociatesInstanceAggr = nullptr;
		sdaiGetAttrBN(OutObject.IfcInstance, (char*) *IfcHasAssociations_Name, sdaiAGGR, &IfcRelAssociatesInstanceAggr);
		int_t IfcRelAssociatesInstanceAggrCnt = sdaiGetMemberCount(IfcRelAssociatesInstanceAggr);
		for (int_t I = 0; I < IfcRelAssociatesInstanceAggrCnt; I++)
		{
			// IfcRelAssociates
			int_t IfcRelAssociatesInstance = 0;
			engiGetAggrElement(IfcRelAssociatesInstanceAggr, I, sdaiINSTANCE, &IfcRelAssociatesInstance);
			if (IfcRelAssociatesInstance == 0)
			{
				continue;
			}

			int_t* IfcRelatedObjectsInstanceAggr = nullptr;
			sdaiGetAttrBN(IfcRelAssociatesInstance, (char*) *IfcRelatedObjects_Name, sdaiAGGR, &IfcRelatedObjectsInstanceAggr);
			int_t IfcRelatedObjectsInstanceAggrCnt = sdaiGetMemberCount(IfcRelatedObjectsInstanceAggr);
			for (int_t A = 0; A < IfcRelatedObjectsInstanceAggrCnt; A++)
			{
				engiGetAggrElement(IfcRelatedObjectsInstanceAggr, A, sdaiINSTANCE, &IfcRelAssociatesInstance);
				if (sdaiGetInstanceType(IfcRelAssociatesInstance) == IfcRelAssociatesMaterialEntity)
				{
					// RelatingMaterial
					int_t IfcMaterialSelectInstance = 0;
					sdaiGetAttrBN(IfcRelAssociatesInstance, (char*) *IfcRelatingMaterial_Name, sdaiINSTANCE, &IfcMaterialSelectInstance);

					TArray<FString> FoundNames;
					TArray<int_t> IfcStyledItemInstances;
					GetRelatingMaterial(IfcMaterialSelectInstance, FoundNames, IfcStyledItemInstances);

					for (int_t M = 0; M < IfcStyledItemInstances.Num(); ++M)
					{
						FMaterial NewMaterial;
						ParseStyledItem(IfcStyledItemInstances[M], NewMaterial);

						if (NewMaterial.ID > 0)
						{
							if (IFCMaterialsMap.Contains(NewMaterial.ID) == false)
							{
								IFCMaterialsMap.Add(NewMaterial.ID, NewMaterial);
							}

						}
					}
				}
			}
		}
	}

	void FFileReader::AssignMaterials(FObject* InObject)
	{
		InObject->Materials.Empty(5);

		for (auto& ShapeRepr : InObject->ShapeRepresentationsSet)
		{
			int64* FoundMaterial = ShapeIdToMaterialIdMap.Find(ShapeRepr);
			if (FoundMaterial != nullptr)
			{
				InObject->Materials.AddUnique(IFCMaterialsMap[*FoundMaterial].ID);
			}
		}
	}

	bool FFileReader::CompareMaterial(const FMaterial* InMaterial, uint32 InAmbient, uint32 InDiffuse)
	{
		if (InMaterial == nullptr)
		{
			return false;
		}

		FLinearColor AmbientColor = FLinearColor(((InAmbient >> 24) & 0xff) / 255.0f, ((InAmbient >> 16) & 0xff) / 255.0f, ((InAmbient >> 8) & 0xff) / 255.0f);
		if (!InMaterial->SurfaceColour.Equals(AmbientColor, SimilarColorTolerance))
		{
			return false;
		}

		float Transparency = 1.0f - (((InAmbient) & 0xff) / 255.0f);
		if (FMath::Abs(InMaterial->Transparency - Transparency) >= SimilarColorTolerance)
		{
			return false;
		}

		FLinearColor DiffuseColor = FLinearColor(((InDiffuse >> 24) & 0xff) / 255.0f, ((InDiffuse >> 16) & 0xff) / 255.0f, ((InDiffuse >> 8) & 0xff) / 255.0f);
		if (!InMaterial->DiffuseColour.Equals(DiffuseColor, SimilarColorTolerance))
		{
			return false;
		}

		return true;		// No difference found.
	}

	int32 FFileReader::GetMaterialIndex(FObject* InObject, uint32 InAmbient, uint32 InDiffuse)
	{
		for (int32 I = 0; I < InObject->Materials.Num(); I++)
		{
			if (CompareMaterial(IFCMaterialsMap.Find(InObject->Materials[I]), InAmbient, InDiffuse))
			{
				return I;		// No difference found.
			}
		}

		// No attached materials found. Check from all materials to fit.
		for (auto& MaterialItem : IFCMaterialsMap)
		{
			if (CompareMaterial(&MaterialItem.Value, InAmbient, InDiffuse))
			{
				return InObject->Materials.Add(MaterialItem.Key);
			}
		}

		// Seems that this material was not found at all. Create one if visible.
		if (((InAmbient) & 0xff) > 0 && bCreateMissingMaterials)
		{
			FMaterial NewMaterial;
			NewMaterial.ID = InObject->IfcInstance;		// TODO: Should be unique?
			NewMaterial.Name = InObject->Name + FString::FromInt(InObject->Materials.Num());
			NewMaterial.SurfaceSide = ESurfaceSide::Both;
			NewMaterial.SurfaceColour = FLinearColor(((InAmbient >> 24) & 0xff) / 255.0f, ((InAmbient >> 16) & 0xff) / 255.0f, ((InAmbient >> 8) & 0xff) / 255.0f);
			NewMaterial.Transparency = 1.0f - (((InAmbient) & 0xff) / 255.0f);
			NewMaterial.DiffuseColour = FLinearColor(((InDiffuse >> 24) & 0xff) / 255.0f, ((InDiffuse >> 16) & 0xff) / 255.0f, ((InDiffuse >> 8) & 0xff) / 255.0f);

			if (IFCMaterialsMap.Contains(NewMaterial.ID) == false)
			{
				FMaterial& AddedMaterial = IFCMaterialsMap.Add(NewMaterial.ID, NewMaterial);

				return InObject->Materials.Add(NewMaterial.ID);
			}
		}

		// Look for default material.
		for (int32 I = 0; I < InObject->Materials.Num(); I++)
		{
			if (InObject->Materials[I] == 0)
			{
				return I;		// Default material found.
			}
		}

		if (!bCreateMissingMaterials)
		{
			return 0;
		}

		// Create a default material if not exists.
		if (IFCMaterialsMap.Contains(0) == false)
		{
			FMaterial NewMaterial;
			NewMaterial.ID = 0;
			NewMaterial.Name = TEXT("DefaultMaterial") + FString::FromInt(InObject->Materials.Num());
			NewMaterial.SurfaceSide = ESurfaceSide::Both;
			NewMaterial.SurfaceColour = FLinearColor::Green;
			NewMaterial.Transparency = 0.0f;
			NewMaterial.DiffuseColour = 0.5f * NewMaterial.SurfaceColour;

			FMaterial& AddedMaterial = IFCMaterialsMap.Add(NewMaterial.ID, NewMaterial);

			return InObject->Materials.Add(NewMaterial.ID);
		}

		// Add default material to this object.
		return InObject->Materials.Add(IFCMaterialsMap[0].ID);
	}

	void FFileReader::ParseQuantityValues(FProperty& InProperty, int64 InIfcPropertySetInstance)
	{
		if (sdaiGetInstanceType(InIfcPropertySetInstance) != IfcElementQuantityEntity)
		{
			Messages.Emplace(EMessageSeverity::Error, TEXT("Unsupported PropertySet entity!"));
			return;
		}

		int_t* QuantitiesInstanceAggr = nullptr;
		sdaiGetAttrBN(InIfcPropertySetInstance, (char*) *IfcQuantities_Name, sdaiAGGR, &QuantitiesInstanceAggr);
		int_t QuantitiesInstanceAggrCnt = sdaiGetMemberCount(QuantitiesInstanceAggr);
		for (int_t P = 0; P < QuantitiesInstanceAggrCnt; P++)
		{
			// IfcPhysicalSimpleQuantity
			int_t SingleValueInstance = 0;
			engiGetAggrElement(QuantitiesInstanceAggr, P, sdaiINSTANCE, &SingleValueInstance);

			InProperty.Values.AddDefaulted();
			FPropertyValue& NewValue = InProperty.Values.Last();

			Helper::GetStringAttribute(SingleValueInstance, TEXT("Name"), NewValue.Name);
			Helper::GetStringAttribute(SingleValueInstance, TEXT("Description"), NewValue.Description);
			Helper::GetStringAttribute(SingleValueInstance, TEXT("Unit"), NewValue.Unit);

			int_t QuantityType = sdaiGetInstanceType(SingleValueInstance);
			if (QuantityType == IfcQuantityLengthEntity)
			{
				NewValue.NominalType = TEXT("Quantity Length");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("LengthValue"), NewValue.NominalValue);
			}
			else if (QuantityType == IfcQuantityAreaEntity)
			{
				NewValue.NominalType = TEXT("Quantity Area");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("AreaValue"), NewValue.NominalValue);
			}
			else if (QuantityType == IfcQuantityVolumeEntity)
			{
				NewValue.NominalType = TEXT("Quantity Volume");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("VolumeValue"), NewValue.NominalValue);
			}
			else if (QuantityType == IfcQuantityCountEntity)
			{
				NewValue.NominalType = TEXT("Quantity Count");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("CountValue"), NewValue.NominalValue);
			}
			else if (QuantityType == IfcQuantityWeigthEntity)
			{
				NewValue.NominalType = TEXT("Quantity Weigth");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("WeigthValue"), NewValue.NominalValue);
			}
			else if (QuantityType == IfcQuantityTimeEntity)
			{
				NewValue.NominalType = TEXT("Quantity Time");
				Helper::GetStringAttribute(SingleValueInstance, TEXT("TimeValue"), NewValue.NominalValue);
			}
			else
			{
				Messages.Emplace(EMessageSeverity::Error, TEXT("Unsupported QuantityType!"));
			}
		}
	}

	void FFileReader::ParsePropertyValues(FProperty& InProperty, int64 InIfcPropertySetInstance)
	{
		if (sdaiGetInstanceType(InIfcPropertySetInstance) != IfcPropertySetEntity)
		{
			ParseQuantityValues(InProperty, InIfcPropertySetInstance);
			return;
		}

		int_t* HasPropertiesInstanceAggr = nullptr;
		sdaiGetAttrBN(InIfcPropertySetInstance, (char*) *IfcHasProperties_Name, sdaiAGGR, &HasPropertiesInstanceAggr);
		int_t HasPropertiesInstanceAggrCnt = sdaiGetMemberCount(HasPropertiesInstanceAggr);
		for (int_t P = 0; P < HasPropertiesInstanceAggrCnt; P++)
		{
			// IfcPropertySingleValue
			int_t SingleValueInstance = 0;
			engiGetAggrElement(HasPropertiesInstanceAggr, P, sdaiINSTANCE, &SingleValueInstance);

			if (sdaiGetInstanceType(SingleValueInstance) != IfcPropertySingleValueEntity)
			{
				Messages.Emplace(EMessageSeverity::Error, TEXT("Unsupported PropertyValue type!"));
			}

			InProperty.Values.AddDefaulted();
			FPropertyValue& NewValue = InProperty.Values.Last();

			Helper::GetStringAttribute(SingleValueInstance, TEXT("Name"), NewValue.Name);
			Helper::GetStringAttribute(SingleValueInstance, TEXT("Description"), NewValue.Description);
			Helper::GetADBAttribute(SingleValueInstance, TEXT("NominalValue"), NewValue.NominalType, NewValue.NominalValue);
			Helper::GetStringAttribute(SingleValueInstance, TEXT("Unit"), NewValue.Unit);
		}
	}

	void FFileReader::GatherProperties(const FString& InName)
	{
		int_t* ObjectEntities = sdaiGetEntityExtentBN(gIFCModel, (char *) *InName);
		int_t ObjectEntitiesCount = sdaiGetMemberCount(ObjectEntities);
		if (ObjectEntitiesCount == 0)
		{
			return;	// No entities found.
		}

		IFCFProperties.Empty(ObjectEntitiesCount);

		for (int_t I = 0; I < ObjectEntitiesCount; ++I)
		{
			// IfcRelDefinesByProperties
			int_t LocalInstance = 0;
			engiGetAggrElement(ObjectEntities, I, sdaiINSTANCE, &LocalInstance);

			int_t RelatingPropertyInstance = 0;
			sdaiGetAttrBN(LocalInstance, (char *) *IfcRelatingPropertyDefinition_Name, sdaiINSTANCE, &RelatingPropertyInstance);
			if (RelatingPropertyInstance == 0)
			{
				continue;
			}

			int32 NewIndex = IFCFProperties.AddDefaulted();
			FProperty& NewProperty = IFCFProperties.Last();

			// IfcPropertySetDefinition
			Helper::GetStringAttribute(RelatingPropertyInstance, TEXT("Name"), NewProperty.Name);
			Helper::GetStringAttribute(RelatingPropertyInstance, TEXT("Description"), NewProperty.Description);

			ParsePropertyValues(NewProperty, RelatingPropertyInstance);

			int_t* IfcRelatedObjectsInstanceAggr = nullptr;
			sdaiGetAttrBN(LocalInstance, (char*) *IfcRelatedObjects_Name, sdaiAGGR, &IfcRelatedObjectsInstanceAggr);
			int_t IfcRelatedObjectsInstanceAggrCnt = sdaiGetMemberCount(IfcRelatedObjectsInstanceAggr);

			NewProperty.RelatedObjects.Empty(IfcRelatedObjectsInstanceAggrCnt);
			for (int_t O = 0; O < IfcRelatedObjectsInstanceAggrCnt; O++)
			{
				// IfcObject
				engiGetAggrElement(IfcRelatedObjectsInstanceAggr, O, sdaiINSTANCE, &LocalInstance);
				if (LocalInstance != 0)
				{
					NewProperty.RelatedObjects.Add(LocalInstance);
				}
			}
		}
	}

	void FFileReader::AssignProperties()
	{
		for (IFC::FProperty& PropertyObject : IFCFProperties)
		{
			for (int64 ObjectId : PropertyObject.RelatedObjects)
			{
				int32* FoundIndex = IFCInstanceToObjectIdMap.Find(ObjectId);
				if (FoundIndex != nullptr && IFCObjects.IsValidIndex(*FoundIndex))
				{
					FObject& FoundObject = IFCObjects[*FoundIndex];
					FoundObject.Properties.Add(&PropertyObject);
				}
			}
		}
	}

	void FFileReader::GetShapeRepresentationId(int64 InModel, FObject& OutObject)
	{
		// parsing Representation of IfcProduct
		//http://www.buildingsmart-tech.org/ifc/IFC2x3/TC1/html/ifckernel/lexical/ifcproduct.htm

		// Basic identification is based on Instance.
		OutObject.ShapeId = OutObject.IfcInstance;

		// IfcProductDefinitionShape
		int_t IfcRepresentationInstance = 0;
		sdaiGetAttrBN(OutObject.IfcInstance, (char*) *IfcRepresentation_Name, sdaiINSTANCE, &IfcRepresentationInstance);
		if (IfcRepresentationInstance == 0)
		{
			return;
		}

		int_t* IfcRepresentationInstanceAggr = nullptr;
		sdaiGetAttrBN(IfcRepresentationInstance, (char*) *IfcRepresentations_Name, sdaiAGGR, &IfcRepresentationInstanceAggr);
		if (IfcRepresentationInstanceAggr == nullptr)
		{
			return;
		}

		int_t IfcRepresentationInstanceAggrCnt = sdaiGetMemberCount(IfcRepresentationInstanceAggr);
		for (int_t I = 0; I < IfcRepresentationInstanceAggrCnt; I++)
		{
			// IfcRepresentation
			engiGetAggrElement(IfcRepresentationInstanceAggr, I, sdaiINSTANCE, &IfcRepresentationInstance);

			wchar_t	* representationIdentifier = nullptr;
			sdaiGetAttrBN(IfcRepresentationInstance, (char*) *IfcRepresentationIdentifier_Name, sdaiUNICODE, &representationIdentifier);
			if (representationIdentifier == nullptr || wcscmp(representationIdentifier, L"Body") != 0)
			{
				continue;
			}

			// Shape ID based on IfcRepresentation.
			OutObject.ShapeId = IfcRepresentationInstance;

			int_t* IfcRepresentationItemInstanceAggr = nullptr;
			sdaiGetAttrBN(IfcRepresentationInstance, (char*) *IfcItems_Name, sdaiAGGR, &IfcRepresentationItemInstanceAggr);
			int_t IfcRepresentationItemInstanceAggrCnt = sdaiGetMemberCount(IfcRepresentationItemInstanceAggr);
			for (int_t J = 0; J < IfcRepresentationItemInstanceAggrCnt; J++)
			{
				// IfcRepresentationItem
				int_t IfcRepresentationItemInstance = 0;
				engiGetAggrElement(IfcRepresentationItemInstanceAggr, J, sdaiINSTANCE, &IfcRepresentationItemInstance);

				if (sdaiGetInstanceType(IfcRepresentationItemInstance) == IfcMappedItemEntity)
				{
					int_t IfcMappingSourceInstance = 0;
					sdaiGetAttrBN(IfcRepresentationItemInstance, (char*) *IfcMappingSource_Name, sdaiINSTANCE, &IfcMappingSourceInstance);
					if (IfcMappingSourceInstance == 0)
					{
						continue;
					}

					// IfcRepresentation
					int_t IfcMappedRepresentationInstance = 0;
					sdaiGetAttrBN(IfcMappingSourceInstance, (char*) *IfcMappedRepresentation_Name, sdaiINSTANCE, &IfcMappedRepresentationInstance);
					if (IfcMappedRepresentationInstance == 0)
					{
						continue;
					}

					sdaiGetAttrBN(IfcMappedRepresentationInstance, (char*) *IfcRepresentationIdentifier_Name, sdaiUNICODE, &representationIdentifier);
					if (wcscmp(representationIdentifier, L"Body") != 0)
					{
						continue;
					}

					// Mapped shape ID based on IfcRepresentation.
					OutObject.MappedShapeId = IfcMappedRepresentationInstance;

					int_t* IfcMappedRepresentationItemInstanceAggr = nullptr;
					sdaiGetAttrBN(IfcMappedRepresentationInstance, (char*) *IfcItems_Name, sdaiAGGR, &IfcMappedRepresentationItemInstanceAggr);
					int_t IfcMappedRepresentationItemInstanceAggrCnt = sdaiGetMemberCount(IfcMappedRepresentationItemInstanceAggr);
					for (int_t M = 0; M < IfcMappedRepresentationItemInstanceAggrCnt; M++)
					{
						// IfcRepresentationItem
						engiGetAggrElement(IfcMappedRepresentationItemInstanceAggr, M, sdaiINSTANCE, &IfcRepresentationItemInstance);
						OutObject.ShapeRepresentationsSet.Add(IfcRepresentationItemInstance);
					}

					break;
				}
				else
				{
					OutObject.ShapeRepresentationsSet.Add(IfcRepresentationItemInstance);
				}
			}
		}
	}

	void FFileReader::ParseGeometry(FObject* InObject, int64 iCircleSegments)
	{
		circleSegments(iCircleSegments, 5);

		//
		//	Get Geometry
		//
		int_t	setting = 0, mask = 0;
		mask += flagbit2;        //    PRECISION (32/64 bit)
		mask += flagbit3;        //	   INDEX ARRAY (32/64 bit)
		mask += flagbit5;        //    NORMALS
		mask += flagbit8;        //    TRIANGLES
		mask += flagbit9;        //    LINES
		mask += flagbit10;       //    POINTS
		mask += flagbit12;       //    WIREFRAME (ALL FACES)
		mask += flagbit13;       //    WIREFRAME (CONCEPTUAL FACES)
		mask += flagbit24;		 //	   AMBIENT
		mask += flagbit25;		 //	   DIFFUSE
		mask += flagbit26;		 //	   EMISSIVE
		mask += flagbit27;		 //	   SPECULAR

		setting += 0;		     //    SINGLE PRECISION (float)
		setting += 0;            //    32 BIT INDEX ARRAY (Int32)
		setting += flagbit5;     //    NORMALS ON
		setting += flagbit8;     //    TRIANGLES ON
		//setting += flagbit9;     //    LINES ON
		//setting += flagbit10;    //    POINTS ON
		setting += flagbit12;    //    WIREFRAME OFF (ALL FACES)
		setting += flagbit13;    //    WIREFRAME ON (CONCEPTUAL FACES)
		setting += flagbit24;	 //	   AMBIENT
		setting += flagbit25;	 //	   DIFFUSE
		setting += flagbit26;	 //	   EMISSIVE
		setting += flagbit27;	 //	   SPECULAR
		InObject->vertexElementSize = SetFormat(gIFCModel, setting, mask);

		//UE_LOG(LogDatasmithIFCReader, Log, TEXT("vertexElementkSize = %lli"), InObject->vertexElementSize);

		int_t iVerticesCount = 0;
		int_t iIndicesCount = 0;
		int_t iTransformCount = 0;
		CalculateInstance(InObject->IfcInstance, &iVerticesCount, &iIndicesCount, &iTransformCount);
		if ((iVerticesCount > 0) && (iIndicesCount > 0))
		{
			InObject->facesVerticesCount = iVerticesCount;
			InObject->facesIndicesCount = iIndicesCount;

			double transformationMatrix[12];
			double minVector[3];
			double maxVector[3];
			GetBoundingBox(InObject->IfcInstance, transformationMatrix, minVector, maxVector);

			FMatrix ConversionFromIFCToLocal;
			{
				FMatrix Matrix = FMatrix::Identity;
				FVector Axis0 = FVector(transformationMatrix[0], transformationMatrix[1], transformationMatrix[2]);
				FVector Axis1 = FVector(transformationMatrix[3], transformationMatrix[4], transformationMatrix[5]);
				FVector Axis2 = FVector(transformationMatrix[6], transformationMatrix[7], transformationMatrix[8]);
				FVector Origin = FVector(transformationMatrix[9], transformationMatrix[10], transformationMatrix[11]);
				Matrix.SetAxes(&Axis0, &Axis1, &Axis2, &Origin);
				ConversionFromIFCToLocal = Matrix.Inverse();
			}

			{
				// Convert the object transform from IFC (right-handed) to Unreal (left-handed)
				// Another way of thinking about this: The final transform should converts to IFC space, apply the object
				// transform, then converts back to Unreal space. The first conversion is a pre-multiply and flips a Y column
				// (or row), and the second conversion is a post-multiply and flips the Y row (or column),
				// hence the pattern of minus signs
				FMatrix Matrix = FMatrix::Identity;
				FVector Axis0  = FVector(  transformationMatrix[0], - transformationMatrix[1],   transformationMatrix[2]);
				FVector Axis1  = FVector(- transformationMatrix[3],   transformationMatrix[4], - transformationMatrix[5]);
				FVector Axis2  = FVector(  transformationMatrix[6], - transformationMatrix[7],   transformationMatrix[8]);
				FVector Origin = FVector(  transformationMatrix[9], - transformationMatrix[10],  transformationMatrix[11]);
				Matrix.SetAxes(&Axis0, &Axis1, &Axis2, &Origin);
				InObject->Transform.SetFromMatrix(Matrix);
			}

			if (InObject->bRootShape)
			{
				{
					transformationMatrix[0] = ConversionFromIFCToLocal.M[0][0];
					transformationMatrix[1] = ConversionFromIFCToLocal.M[0][1];
					transformationMatrix[2] = ConversionFromIFCToLocal.M[0][2];
					transformationMatrix[3] = ConversionFromIFCToLocal.M[1][0];
					transformationMatrix[4] = ConversionFromIFCToLocal.M[1][1];
					transformationMatrix[5] = ConversionFromIFCToLocal.M[1][2];
					transformationMatrix[6] = ConversionFromIFCToLocal.M[2][0];
					transformationMatrix[7] = ConversionFromIFCToLocal.M[2][1];
					transformationMatrix[8] = ConversionFromIFCToLocal.M[2][2];
					transformationMatrix[9] = ConversionFromIFCToLocal.M[3][0];
					transformationMatrix[10] = ConversionFromIFCToLocal.M[3][1];
					transformationMatrix[11] = ConversionFromIFCToLocal.M[3][2];
				}
				SetVertexBufferTransformation(gIFCModel, transformationMatrix);

				InObject->facesVertices.SetNumUninitialized(iVerticesCount * (InObject->vertexElementSize / sizeof(float))); // <X, Y, Z, Nx, Ny, Nz ...>
				InObject->facesIndices.SetNumUninitialized(iIndicesCount);

				UpdateInstanceVertexBuffer(InObject->IfcInstance, InObject->facesVertices.GetData());
				UpdateInstanceIndexBuffer(InObject->IfcInstance, InObject->facesIndices.GetData());

				AssignMaterials(InObject);
				uint32_t LastAmbient = 0;
				uint32_t LastDiffuse = 0;
				int32 MaterialIndex = 0;

				int_t faceCnt = getConceptualFaceCnt(InObject->IfcInstance);
				for (int_t j = 0; j < faceCnt; j++)
				{
					int_t	startIndexTriangles = 0, noIndicesTrangles = 0;

					int_t	noTriangles = 0;
					int64_t faceHandle = GetConceptualFace(InObject->IfcInstance, j, &startIndexTriangles, &noTriangles);
					noIndicesTrangles = noTriangles * 3;

					for (int32 p = 0; p < noIndicesTrangles; p += 3)
					{
						InObject->TrianglesArray.AddDefaulted();
						FPolygon& NewTriangle = InObject->TrianglesArray.Last();
						for (int32 p2 = 0; p2 < 3; p2++)
						{
							if (InObject->facesIndices[startIndexTriangles + p + p2] >= 0)
							{
								NewTriangle.Points.Add(InObject->facesIndices[startIndexTriangles + p + p2]);
							}
						}

						int32_t	ColorIndex = InObject->facesIndices[startIndexTriangles + p];

						uint32_t ambient = *(uint32_t*)&(InObject->facesVertices[ColorIndex * (InObject->vertexElementSize / sizeof(float)) + 6]);
						uint32_t diffuse = *(uint32_t*)&(InObject->facesVertices[ColorIndex * (InObject->vertexElementSize / sizeof(float)) + 7]);
						uint32_t emissive = *(uint32_t*)&(InObject->facesVertices[ColorIndex * (InObject->vertexElementSize / sizeof(float)) + 8]);
						uint32_t specular = *(uint32_t*)&(InObject->facesVertices[ColorIndex * (InObject->vertexElementSize / sizeof(float)) + 9]);

						//UE_LOG(LogDatasmithIFCReader, Log, TEXT("diffuse = %d, %d, %d, %d"), (diffuse)&0xff, (diffuse>>8)&0xff, (diffuse>>16)&0xff, (diffuse>>24)&0xff);

						if (j == 0 || ambient != LastAmbient || diffuse != LastDiffuse)
						{
							MaterialIndex = GetMaterialIndex(InObject, ambient, diffuse);

							LastAmbient = ambient;
							LastDiffuse = diffuse;
						}

						NewTriangle.MaterialIndex = MaterialIndex;
					}
				}
			}

			InObject->bHasGeometry = InObject->facesVertices.Num() && InObject->facesIndices.Num();
		}
		cleanMemory(gIFCModel, 0);
	}

	void FFileReader::GatherObjectsEntityHierarchy(int64 InIFCModel, int64 InParentEntity, bool bVisible, int64 CircleSegments)
	{
		int_t* ObjectInstances = sdaiGetEntityExtent(InIFCModel, InParentEntity);

		if ((InParentEntity == IfcDistributionElementEntity) ||
			(InParentEntity == IfcElectricalElementEntity) ||
			(InParentEntity == IfcElementAssemblyEntity) ||
			(InParentEntity == IfcElementComponentEntity) ||
			(InParentEntity == IfcEquipmentElementEntity) ||
			(InParentEntity == IfcFeatureElementEntity) ||
			(InParentEntity == IfcFurnishingElementEntity) ||
			(InParentEntity == IfcTransportElementEntity) ||
			(InParentEntity == IfcVirtualElementEntity)) {
			CircleSegments = 24;
		}

		if (InParentEntity == IfcReinforcingElementEntity) {
			CircleSegments = 12;
		}

		GatherObjects(InIFCModel, InParentEntity, ObjectInstances, bVisible, CircleSegments);

		int_t EntityCount = engiGetEntityCount(InIFCModel);
		for (int_t EntityIndex = 0; EntityIndex < EntityCount; ++EntityIndex)
		{
			int_t Entity = engiGetEntityElement(InIFCModel, EntityIndex);

			if (engiGetEntityParent(Entity) == InParentEntity)
			{
				GatherObjectsEntityHierarchy(InIFCModel, Entity, bVisible, CircleSegments);
			}
		}
	}

	void FFileReader::GatherObjects(const FString& InName, bool bVisible, int64 iCircleSegments)
	{
		int_t* ObjectEntities = sdaiGetEntityExtentBN(gIFCModel, (char *)*InName);

		GatherObjects(gIFCModel, sdaiGetEntity(gIFCModel, (char *)*InName), ObjectEntities, bVisible, iCircleSegments);
	}

	void FFileReader::GatherObjects(int64 InIFCModel, int64 Entity, int_t* ObjectInstances, bool bVisible, int64 iCircleSegments)
	{
		if (Entity == IfcOpeningElementEntity)
		{
			return; // Ignore openings.
		}

		// Getting all root entities with name.
		int_t ObjectInstancesCount = sdaiGetMemberCount(ObjectInstances);
		if (ObjectInstancesCount == 0)
		{
			return;	// No entities found.
		}

		TCHAR* EntityNamePtr = nullptr;
		engiGetEntityName(Entity, sdaiUNICODE, (char**)&EntityNamePtr);

		FString EntityName = EntityNamePtr;

		for (int_t ObjectInstanceIndex = 0; ObjectInstanceIndex < ObjectInstancesCount; ++ObjectInstanceIndex)
		{
			int_t LocalInstance = 0;
			engiGetAggrElement(ObjectInstances, ObjectInstanceIndex, sdaiINSTANCE, &LocalInstance);

			int32 NewObjIndex = IFCObjects.AddDefaulted();
			FObject& NewIFCObject = IFCObjects.Last();

			NewIFCObject.IfcInstance = LocalInstance;
			NewIFCObject.bVisible = bVisible;

			IFCInstanceToObjectIdMap.Add(LocalInstance, NewObjIndex);

			NewIFCObject.Type = *EntityName;

			Helper::GetStringAttribute(LocalInstance, TEXT("GlobalId"), NewIFCObject.GlobalId);
			Helper::GetStringAttribute(LocalInstance, TEXT("Name"), NewIFCObject.Name);
			Helper::GetStringAttribute(LocalInstance, TEXT("Description"), NewIFCObject.Description);

			IFCGlobalIdToObjectIdMap.Add(NewIFCObject.GlobalId, NewObjIndex);

			if (NewIFCObject.Name.IsEmpty())
			{
				NewIFCObject.Name = NewIFCObject.Type;
			}

			NewIFCObject.bRootObject = true;	// Assume all are root at this stage.

			GetShapeRepresentationId(InIFCModel, NewIFCObject);
			GetShapeAssociations(InIFCModel, NewIFCObject);

			int64 ShapeId = NewIFCObject.MappedShapeId ? NewIFCObject.MappedShapeId : NewIFCObject.ShapeId;
			if (ShapeIdToObjectIdMap.Contains(ShapeId) == false)
			{
				NewIFCObject.bRootShape = true;
				ShapeIdToObjectIdMap.Add(ShapeId, NewObjIndex);
			}

			ParseGeometry(&NewIFCObject, iCircleSegments);
		}
	}

	void FFileReader::GatherRelContained(const FString& InName)
	{
		int_t* ObjectEntities = sdaiGetEntityExtentBN(gIFCModel, (char *) *InName);
		int_t ObjectEntitiesCount = sdaiGetMemberCount(ObjectEntities);
		if (ObjectEntitiesCount == 0)
		{
			return;	// No entities found.
		}

		IFCRootObjects.Empty(ObjectEntitiesCount);

		for (int_t I = 0; I < ObjectEntitiesCount; ++I)
		{
			// IfcRelContainedInSpatialStructure
			int_t LocalInstance = 0;
			engiGetAggrElement(ObjectEntities, I, sdaiINSTANCE, &LocalInstance);

			int_t RelatingStructureInstance = 0;
			sdaiGetAttrBN(LocalInstance, (char *) *IfcRelatingStructure_Name, sdaiINSTANCE, &RelatingStructureInstance);
			if (RelatingStructureInstance == 0)
			{
				continue;
			}
			if (sdaiGetInstanceType(RelatingStructureInstance) != IfcBuildingStoreyEntity
				&& sdaiGetInstanceType(RelatingStructureInstance) != IfcBuildingEntity)
			{
				continue;
			}

			int32 NewObjIndex = IFCRootObjects.AddDefaulted();
			FObject& NewIFCObject = IFCRootObjects.Last();

			NewIFCObject.Type = sdaiGetInstanceType(RelatingStructureInstance) == IfcBuildingEntity ? TEXT("IfcBuilding") : TEXT("IfcBuildingStorey");

			Helper::GetStringAttribute(RelatingStructureInstance, TEXT("GlobalId"), NewIFCObject.GlobalId);
			Helper::GetStringAttribute(RelatingStructureInstance, TEXT("Name"), NewIFCObject.Name);
			Helper::GetStringAttribute(RelatingStructureInstance, TEXT("Description"), NewIFCObject.Description);

			if (NewIFCObject.Name.IsEmpty())
			{
				NewIFCObject.Name = NewIFCObject.Type;
			}

			int_t* IfcRelatedElementsInstanceAggr = nullptr;
			sdaiGetAttrBN(LocalInstance, (char*) *IfcRelatedElements_Name, sdaiAGGR, &IfcRelatedElementsInstanceAggr);
			int_t IfcRelatedElementsInstanceAggrCnt = sdaiGetMemberCount(IfcRelatedElementsInstanceAggr);
			for (int_t E = 0; E < IfcRelatedElementsInstanceAggrCnt; E++)
			{
				// IfcProduct
				engiGetAggrElement(IfcRelatedElementsInstanceAggr, E, sdaiINSTANCE, &LocalInstance);

				if (LocalInstance != 0)
				{
					NewIFCObject.Children.Add(LocalInstance);

					const int32* FoundIndex = IFCInstanceToObjectIdMap.Find(LocalInstance);
					if (FoundIndex != nullptr && IFCObjects.IsValidIndex(*FoundIndex))
					{
						IFCObjects[*FoundIndex].bRootObject = false;	// This is child object now.
					}
				}
			}
		}
	}

#endif

}  // namespace IFC
