// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "MaterialValueType.h"
#include "Math/Vector4.h"
#include "Misc/Guid.h"

enum EMaterialProperty : int;
enum EMaterialShadingModel : int;
enum EShaderFrequency : uint8;

class FMaterialCompiler;
class UMaterial;

struct FMaterialShadingModelField;

/**
 * Custom attribute blend functions
 */
typedef int32(*MaterialAttributeBlendFunction)(FMaterialCompiler* Compiler, int32 A, int32 B, int32 Alpha);

/**
 * Attribute data describing a material property
 */
class FMaterialAttributeDefintion
{
public:
	FMaterialAttributeDefintion(const FGuid& InGUID, const FString& AttributeName, EMaterialProperty InProperty,
		EMaterialValueType InValueType, const FVector4& InDefaultValue, EShaderFrequency InShaderFrequency,
		int32 InTexCoordIndex = INDEX_NONE, bool bInIsHidden = false, MaterialAttributeBlendFunction InBlendFunction = nullptr);

	ENGINE_API int32 CompileDefaultValue(FMaterialCompiler* Compiler) const;

	bool operator==(const FMaterialAttributeDefintion& Other) const
	{
		return (AttributeID == Other.AttributeID);
	}

	FGuid				AttributeID;
	FVector4			DefaultValue;
	FString				AttributeName;
	EMaterialProperty	Property;
	EMaterialValueType	ValueType;
	EShaderFrequency	ShaderFrequency;
	int32				TexCoordIndex;

	// Optional function pointer for custom blend behavior
	MaterialAttributeBlendFunction BlendFunction;

	// Hidden from auto-generated lists but valid for manual material creation
	bool				bIsHidden;
};

/**
 * Attribute data describing a material property used for a custom output
 */
class FMaterialCustomOutputAttributeDefintion : public FMaterialAttributeDefintion
{
public:
	FMaterialCustomOutputAttributeDefintion(const FGuid& InGUID, const FString& InAttributeName, const FString& InFunctionName, EMaterialProperty InProperty,
		EMaterialValueType InValueType, const FVector4& InDefaultValue, EShaderFrequency InShaderFrequency, MaterialAttributeBlendFunction InBlendFunction = nullptr);

	bool operator==(const FMaterialCustomOutputAttributeDefintion& Other) const
	{
		return (AttributeID == Other.AttributeID);
	}

	// Name of function used to access attribute in shader code
	FString							FunctionName;
};

/**
 * Material property to attribute data mappings
 */
class FMaterialAttributeDefinitionMap
{
public:
	ENGINE_API FMaterialAttributeDefinitionMap();

	/** Compiles the default expression for a material attribute */
	static ENGINE_API int32 CompileDefaultExpression(FMaterialCompiler* Compiler, EMaterialProperty Property);

	/** Compiles the default expression for a material attribute */
	static ENGINE_API int32 CompileDefaultExpression(FMaterialCompiler* Compiler, const FGuid& AttributeID);

	/** Returns the display name of a material attribute */
	static ENGINE_API const FString& GetAttributeName(EMaterialProperty Property);

	/** Returns the display name of a material attribute */
	static ENGINE_API const FString& GetAttributeName(const FGuid& AttributeID);

	/** Returns the display name of a material attribute, accounting for overrides based on properties of a given material */
	static ENGINE_API FText GetDisplayNameForMaterial(EMaterialProperty Property, UMaterial* Material);

	/** Returns the display name of a material attribute, accounting for overrides based on properties of a given material */
	static ENGINE_API FText GetDisplayNameForMaterial(const FGuid& AttributeID, UMaterial* Material);

	/** Returns the value type of a material attribute */
	static ENGINE_API EMaterialValueType GetValueType(EMaterialProperty Property);

	/** Returns the value type of a material attribute */
	static ENGINE_API EMaterialValueType GetValueType(const FGuid& AttributeID);

	/** Returns the default value of a material property */
	static ENGINE_API FVector4f GetDefaultValue(EMaterialProperty Property);

	/** Returns the default value of a material attribute */
	static ENGINE_API FVector4f GetDefaultValue(const FGuid& AttributeID);

	/** Returns the shader frequency of a material attribute */
	static ENGINE_API EShaderFrequency GetShaderFrequency(EMaterialProperty Property);

	/** Returns the shader frequency of a material attribute */
	static ENGINE_API EShaderFrequency GetShaderFrequency(const FGuid& AttributeID);

	/** Returns the attribute ID for a matching material property */
	static ENGINE_API FGuid GetID(EMaterialProperty Property);

	/** Returns a the material property matching the specified attribute AttributeID */
	static ENGINE_API EMaterialProperty GetProperty(const FGuid& AttributeID);

	/** Returns the custom blend function of a material attribute */
	static ENGINE_API MaterialAttributeBlendFunction GetBlendFunction(const FGuid& AttributeID);

	/** Returns a default attribute AttributeID */
	static ENGINE_API FGuid GetDefaultID();

	/** Appends a hash of the property map intended for use with the DDC key */
	static ENGINE_API void AppendDDCKeyString(FString& String);

	/** Appends a new attribute definition to the custom output list */
	static ENGINE_API void AddCustomAttribute(const FGuid& AttributeID, const FString& AttributeName, const FString& FunctionName, EMaterialValueType ValueType, const FVector4& DefaultValue, MaterialAttributeBlendFunction BlendFunction = nullptr);

	/** Returns the first custom attribute ID that has the specificed attribute name */
	static ENGINE_API FGuid GetCustomAttributeID(const FString& AttributeName);

	/** Returns the first custom attribute definition that has the specificed attribute name */
	static ENGINE_API const FMaterialCustomOutputAttributeDefintion* GetCustomAttribute(const FString& AttributeName);

	/** Returns the first custom attribute definition that has the specificed attribute AttributeID */
	static ENGINE_API const FMaterialCustomOutputAttributeDefintion* GetCustomAttribute(const FGuid& AttributeID);

	/** Returns a list of registered custom attributes */
	static ENGINE_API void GetCustomAttributeList(TArray<FMaterialCustomOutputAttributeDefintion>& CustomAttributeList);

	static const TArray<FGuid>& GetOrderedVisibleAttributeList()
	{
		return GMaterialPropertyAttributesMap.OrderedVisibleAttributeList;
	}

private:
	// Customization class for displaying data in the material editor
	friend class FMaterialAttributePropertyDetails;

	/** Returns a list of display names and their associated GUIDs for material properties */
	static ENGINE_API void GetAttributeNameToIDList(TArray<TPair<FString, FGuid>>& NameToIDList);

	// Internal map management
	ENGINE_API void InitializeAttributeMap();

	ENGINE_API void Add(const FGuid& AttributeID, const FString& AttributeName, EMaterialProperty Property,
		EMaterialValueType ValueType, const FVector4& DefaultValue, EShaderFrequency ShaderFrequency,
		int32 TexCoordIndex = INDEX_NONE, bool bIsHidden = false, MaterialAttributeBlendFunction BlendFunction = nullptr);

	ENGINE_API FMaterialAttributeDefintion* Find(const FGuid& AttributeID);
	ENGINE_API FMaterialAttributeDefintion* Find(EMaterialProperty Property);

	// Helper functions to determine display name based on shader model, material domain, etc.
	static ENGINE_API FText GetAttributeOverrideForMaterial(const FGuid& AttributeID, UMaterial* Material);
	static ENGINE_API FString GetPinNameFromShadingModelField(FMaterialShadingModelField InShadingModels, const TArray<TKeyValuePair<EMaterialShadingModel, FString>>& InCustomShadingModelPinNames, const FString& InDefaultPinName);

	static ENGINE_API FMaterialAttributeDefinitionMap GMaterialPropertyAttributesMap;

	TMap<EMaterialProperty, FMaterialAttributeDefintion>	AttributeMap; // Fixed map of compile-time definitions
	TArray<FMaterialCustomOutputAttributeDefintion>			CustomAttributes; // Array of custom output definitions
	TArray<FGuid>											OrderedVisibleAttributeList; // List used for consistency with e.g. combobox filling

	FString													AttributeDDCString;
	bool bIsInitialized;
};
