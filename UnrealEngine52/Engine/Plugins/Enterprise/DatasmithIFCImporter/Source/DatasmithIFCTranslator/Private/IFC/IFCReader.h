// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/TokenizedMessage.h"
#include "Math/Vector.h"
#include "XmlParser.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithIFCReader, Log, All);

class FXmlFile;

namespace IFC
{
	using FLogMessage = TTuple<EMessageSeverity::Type, FString>;

	enum class ESurfaceSide
	{
		Positive = 0,
		Negative,
		Both
	};

	struct FPolygon
	{
		TArray<int32> Points;
		int32 MaterialIndex = 0;
	};

	struct FUnit
	{
		FString Type;
		FString Prefix;
		FString Name;

		float PrefixValue = 0.0f;
	};

	struct FMaterial
	{
		int64 ID = 0;
		FString Name;
		ESurfaceSide SurfaceSide = ESurfaceSide::Both;

		FLinearColor SurfaceColour = FLinearColor::White;
		float Transparency = 0.0f;
		FLinearColor DiffuseColour = FLinearColor::White;

		FLinearColor TransmissionColour = FLinearColor::Black;
		FLinearColor DiffuseTransmissionColour = FLinearColor::Black;
		FLinearColor ReflectionColour = FLinearColor::Black;
		FLinearColor SpecularColour = FLinearColor::Black;
	};

	struct FPropertyValue
	{
		FString Name;
		FString Description;
		FString NominalType;
		FString NominalValue;
		FString Unit;
	};

	struct FProperty
	{
		FString Name;
		FString Description;

		TArray<FPropertyValue> Values;
		TArray<int64> RelatedObjects;
	};

	struct FRootObject
	{
		FString GlobalId;
		FString Type;
		FString Name;
		FString Description;

		FTransform Transform;

		TArray<int64> Children;
	};

	struct FObject : public FRootObject
	{
		// Importer data.
		int64 ShapeId = 0;
		int64 MappedShapeId = 0;
		bool bRootObject = false;
		bool bRootShape = false;

		// IFC specific data.
		int64 IfcInstance = 0;
		bool bVisible = false;
		bool bHasGeometry = false;

		// Object data.
		TArray<int64> Materials;
		TSet<int64> ShapeRepresentationsSet;
		TArray<FProperty*> Properties;

		// Raw geometry data.
		int64 vertexElementSize = 0;
		TArray<float> facesVertices;
		int64 facesVerticesCount = 0;
		TArray<int32> facesIndices;
		int64 facesIndicesCount = 0;

		// Parsed triangles data.
		TArray<FVector> PointsArray;
		TArray<FPolygon> TrianglesArray;

		TArray<int32> DecomposedBy;
		TArray<int32> ContainsElements;
	};

	class FFileReader
	{
	public:
		FFileReader();
		~FFileReader();

		const TArray<FLogMessage>& GetLogMessages() const { return Messages; }

		FString GetIFCVersion() const { return IFCVersion; }
		const TMap<int64, FMaterial>& GetMaterialsMap() const { return IFCMaterialsMap; }
		const TArray<FObject>& GetRootObjects() const { return IFCRootObjects; }
		const TArray<int32>& GetProjects() const { return IFCProjects; }
		const TArray<FObject>& GetObjects() const { return IFCObjects; }

		/**
		 * Gets length unit value for location.
		 */
		float GetLengthUnit() const;

		/**
		 * Loads the contents of a IFC file into interal data.
		 *
		 * @param InFilePath - disk file path
		 * @return true if file was successful read.
		 */
		bool ReadFile(const FString& InFilePath);

		/**
		 * Method to precalculate some data after reading.
		 */
		void PostReadFile(float InSimilarColorTolerance, bool InCreateMissingMaterials);

		/**
		 * All buffered data got from file should be clean here.
		 */
		void CleanUp();

		/**
		 * Finds Object with ID.
		 */
		const FObject* FindObject(int64 ObjectId) const;

		/**
		 * Finds Object with global ID.
		 */
		const FObject* FindObjectFromGlobalId(const FString& InGlobalId) const;

		/**
		 * Finds Object with shape ID that is connected with.
		 */
		const FObject* FindObjectFromShapeId(int64 InShapeId) const;

	protected:
		mutable TArray<FLogMessage> Messages;

		FString IFCVersion;

		/**
		 * Import options.
		 */
		float SimilarColorTolerance = 0.01f;
		bool bCreateMissingMaterials = true;

		/**
		 * Objects holding arrays.
		 */
		TMap<int64, FMaterial> IFCMaterialsMap;
		TArray<int32> IFCProjects;
		TArray<FObject> IFCRootObjects;
		TArray<FObject> IFCObjects;
		TArray<FProperty> IFCFProperties;
		TMap<FString, FUnit> UnitTypesMap;

		/**
		 * Objects helper mapping.
		 */
		TMap<FString, int32> IFCGlobalIdToObjectIdMap;
		TMap<int64, int32> IFCInstanceToObjectIdMap;
		TMap<int64, int64> ShapeIdToMaterialIdMap;
		
		TMap<int64, int32> ShapeIdToObjectIdMap;

	private:

#ifdef WITH_IFC_ENGINE_LIB

		/** Global model identifier used during loading. */
		int64 gIFCModel = 0;

		/** Settings file parsed. */
		FXmlFile XMLFileSettings;

		/**
		 * Loading time buffered entities to compare.
		 */
		int64 IfcMappedItemEntity = 0;
		int64 IfcConversionBasedUnitEntity = 0;
		int64 IfcSIUnitEntity = 0;
		int64 IfcSurfaceStyleEntity = 0;
		int64 IfcSurfaceStyleRenderingEntity = 0;
		int64 IfcSurfaceStyleShadingEntity = 0;
		int64 IfcRelAssociatesMaterialEntity = 0;
		int64 IfcMaterialEntity = 0;
		int64 IfcMaterialLayerSetUsageEntity = 0;
		int64 IfcMaterialLayerSetEntity = 0;
		int64 IfcMaterialLayerEntity = 0;
		int64 IfcStyledItemEntity = 0;
		int64 IfcColourRgbEntity = 0;
		int64 IfcPropertySetEntity = 0;
		int64 IfcElementQuantityEntity = 0;
		int64 IfcPropertySingleValueEntity = 0;
		int64 IfcQuantityLengthEntity = 0;
		int64 IfcQuantityAreaEntity = 0;
		int64 IfcQuantityVolumeEntity = 0;
		int64 IfcQuantityCountEntity = 0;
		int64 IfcQuantityWeigthEntity = 0;
		int64 IfcQuantityTimeEntity = 0;
		int64 IfcBuildingEntity = 0;
		int64 IfcBuildingStoreyEntity = 0;
		int64 IfcDistributionElementEntity = 0;
		int64 IfcElectricalElementEntity = 0;
		int64 IfcElementAssemblyEntity = 0;
		int64 IfcElementComponentEntity = 0;
		int64 IfcEquipmentElementEntity = 0;
		int64 IfcFeatureElementEntity = 0;
		int64 IfcFurnishingElementEntity = 0;
		int64 IfcTransportElementEntity = 0;
		int64 IfcVirtualElementEntity = 0;
		int64 IfcReinforcingElementEntity = 0;
		int64 IfcOpeningElementEntity = 0;
		
		

		
		
		


		/** Gather Units from IFC file. */
		void GatherUnits(int64 InIFCModel, int64 InProjectInstance);

		// scene tree
		void GatherSpatialHierarchy(int64 InIFCModel, int64 InObjectInstance);

		/** Buffer entities to use later. */
		void GetEntitiesTypes();

		/**
		 * Material parsing methods.
		 */
		void GetMaterialInstance(int64 IfcMaterialLayer, TArray<FString>& OutName, TArray<int64>& OutIfcStyledItemInstances);
		void GetMaterialLayers(int64 IfcMaterialLayerSet, TArray<FString>& OutNames, TArray<int64>& OutIfcStyledItemInstances);
		void GetRelatingMaterial(int64 IfcMaterialSelectInstance, TArray<FString>& OutNames, TArray<int64>& OutIfcStyledItemInstances);
		void ParsePresentationStyle(int64 InPresentationStyle, FMaterial& OutMaterial);
		void ParseStyledItem(int64 InStyledItemInstance, FMaterial& OutMaterial);
		void GatherMaterials(const FString& InName);
		void GatherMaterialAssociates(const FString& InName);
		void GetShapeAssociations(int64 InModel, FObject& OutObject);
		void AssignMaterials(FObject* InObject);
		bool CompareMaterial(const FMaterial* InMaterial, uint32 InAmbient, uint32 InDiffuse);
		int32 GetMaterialIndex(FObject* InObject, uint32 InAmbient, uint32 InDiffuse);
		/**
		 * ~Material parsing methods.
		 */

		 /**
		  * Properties parsing methods.
		  */
		void ParseQuantityValues(FProperty& InProperty, int64 InIfcPropertySetInstance);
		void ParsePropertyValues(FProperty& InProperty, int64 InIfcPropertySetInstance);
		void GatherProperties(const FString& InName);
		void AssignProperties();
		/**
		 * ~Properties parsing methods.
		 */

		 /**
		  * Geometry parsing methods.
		  */
		void GetShapeRepresentationId(int64 InModel, FObject& OutObject);
		void ParseGeometry(FObject* InObject, int64 iCircleSegments);
		void GatherObjects(const FString& InName, bool bVisible, int64 iCircleSegments);
		void GatherObjects(int64 InIFCModel, int64 Entity, int64* ObjectInstances, bool bVisible, int64 iCircleSegments);
		// gather objects going down entity hierarchy
		void GatherObjectsEntityHierarchy(int64 InIFCModel, int64 InParentEntity, bool bVisible, int64 CircleSegments);

		/**
		 * ~Geometry parsing methods.
		 */

		 /**
		  * Object parsing methods.
		  */
		void GatherRelContained(const FString& InName);
		/**
		 * ~Object parsing methods.
		 */

#endif
	};

} // namespace IFC
