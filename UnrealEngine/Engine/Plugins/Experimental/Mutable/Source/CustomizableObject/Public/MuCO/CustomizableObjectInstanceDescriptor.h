// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/MultilayerProjector.h"
#include "UObject/ObjectPtr.h"

#include "CustomizableObjectInstanceDescriptor.generated.h"

class FArchive;
class UCustomizableInstancePrivateData;
class UCustomizableObject;
class UCustomizableObjectInstance;

namespace mu
{
	class Parameters;
	
	template<typename Type>
	class Ptr;
}


/** Set of parameters + state that defines a CustomizableObjectInstance.
 *
 * This object has the same parameters + state interfice as UCustomizableObjectInstance.
 * UCustomizableObjectInstance must share the same interface. Any public methods added here should also end up in the Instance. */
USTRUCT()
struct CUSTOMIZABLEOBJECT_API FCustomizableObjectInstanceDescriptor
{
	GENERATED_BODY()

	FCustomizableObjectInstanceDescriptor() = default;
	
	explicit FCustomizableObjectInstanceDescriptor(UCustomizableObject& Object);

	FCustomizableObjectInstanceDescriptor(const FCustomizableObjectInstanceDescriptor& Other) = default;

	/** Serialize this object. */
	void SaveDescriptor(FArchive &Ar);

	/** Deserialize this object. */
	void LoadDescriptor(FArchive &Ar);

	UCustomizableObject* GetCustomizableObject() const;

	bool GetBuildParameterDecorations() const;
	
	void SetBuildParameterDecorations(bool Value);
	
	void SetCustomizableObject(UCustomizableObject* InCustomizableObject);

	mu::Ptr<mu::Parameters> ReloadParametersFromObject(); // TODO Split function in 2: 1. Load parameters from model, 2. Get mu::ParametersPtr
    
	// ------------------------------------------------------------
	// Parameters
	// ------------------------------------------------------------

	TArray<FCustomizableObjectBoolParameterValue>& GetBoolParameters();

	const TArray<FCustomizableObjectBoolParameterValue>& GetBoolParameters() const;
	
	TArray<FCustomizableObjectIntParameterValue>& GetIntParameters();

	const TArray<FCustomizableObjectIntParameterValue>& GetIntParameters() const;

	TArray<FCustomizableObjectFloatParameterValue>& GetFloatParameters();

	const TArray<FCustomizableObjectFloatParameterValue>& GetFloatParameters() const;
	
	TArray<FCustomizableObjectTextureParameterValue>& GetTextureParameters();
	
	const TArray<FCustomizableObjectTextureParameterValue>& GetTextureParameters() const;

	TArray<FCustomizableObjectVectorParameterValue>& GetVectorParameters();
	
	const TArray<FCustomizableObjectVectorParameterValue>& GetVectorParameters() const;

	TArray<FCustomizableObjectProjectorParameterValue>& GetProjectorParameters();
	
	const TArray<FCustomizableObjectProjectorParameterValue>& GetProjectorParameters() const;
	
	/** Return true if there are any parameters. */
	bool HasAnyParameters() const;

	/** Gets the value of the int parameter with name "ParamName". */
	const FString& GetIntParameterSelectedOption(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Sets the selected option of an int parameter by the option's name. */
	void SetIntParameterSelectedOption(int32 IntParamIndex, const FString& SelectedOption, int32 RangeIndex = -1);

	/** Sets the selected option of an int parameter, by the option's name */
	void SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, int32 RangeIndex = -1);

	/** Gets the value of a float parameter with name "FloatParamName". */
	float GetFloatParameterSelectedOption(const FString& FloatParamName, int32 RangeIndex = -1) const;

	/** Sets the float value "FloatValue" of a float parameter with index "FloatParamIndex". */
	void SetFloatParameterSelectedOption(const FString& FloatParamName, float FloatValue, int32 RangeIndex = -1);

	/** Gets the value of a color parameter with name "ColorParamName". */
	FLinearColor GetColorParameterSelectedOption(const FString& ColorParamName) const;

	/** Sets the color value "ColorValue" of a color parameter with index "ColorParamIndex". */
	void SetColorParameterSelectedOption(const FString& ColorParamName, const FLinearColor& ColorValue);

	/** Gets the value of the bool parameter with name "BoolParamName". */
	bool GetBoolParameterSelectedOption(const FString& BoolParamName) const;

	/** Sets the bool value "BoolValue" of a bool parameter with name "BoolParamName". */
	void SetBoolParameterSelectedOption(const FString& BoolParamName, bool BoolValue);

	/** Sets the vector value "VectorValue" of a bool parameter with index "VectorParamIndex". */
	void SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue);

	/** Sets the projector values of a projector parameter with index "ProjectorParamIndex". */
	void SetProjectorValue(const FString& ProjectorParamName,
		const FVector& Pos, const FVector& Direction, const FVector& Up, const FVector& Scale,
		float Angle,
		int32 RangeIndex = -1);

	/** Set only the projector position. */
	void SetProjectorPosition(const FString& ProjectorParamName, const FVector3f& Pos, int32 RangeIndex = -1);

	/** Get the projector values of a projector parameter with index "ProjectorParamIndex". */
	void GetProjectorValue(const FString& ProjectorParamName,
		FVector& OutPos, FVector& OutDirection, FVector& OutUp, FVector& OutScale,
		float& OutAngle, ECustomizableObjectProjectorType& OutType,
		int32 RangeIndex = -1) const;

	/** Float version. See GetProjectorValue. */
	void GetProjectorValueF(const FString& ProjectorParamName,
		FVector3f& OutPos, FVector3f& OutDirection, FVector3f& OutUp, FVector3f& OutScale,
		float& OutAngle, ECustomizableObjectProjectorType& OutType,
		int32 RangeIndex = -1) const;

	/** Get the current projector position for the parameter with the given name. */
	FVector GetProjectorPosition(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector direction vector for the parameter with the given name. */
	FVector GetProjectorDirection(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector up vector for the parameter with the given name. */
	FVector GetProjectorUp(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector scale for the parameter with the given name. */
	FVector GetProjectorScale(const FString & ParamName, int32 RangeIndex = -1) const;

	/** Get the current cylindrical projector angle for the parameter with the given name. */
	float GetProjectorAngle(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector type for the parameter with the given name. */
	ECustomizableObjectProjectorType GetProjectorParameterType(const FString& ParamName, int32 RangeIndex = -1) const;

	/** Get the current projector for the parameter with the given name. */
	FCustomizableObjectProjector GetProjector(const FString& ParamName, int32 RangeIndex) const;
	
	/** Finds in IntParameters a parameter with name ParamName, returns the index if found, -1 otherwise. */
	int32 FindIntParameterNameIndex(const FString& ParamName) const;

	/** Finds in FloatParameters a parameter with name ParamName, returns the index if found, -1 otherwise. */
	int32 FindFloatParameterNameIndex(const FString& ParamName) const;

	/** Finds in BoolParameters a parameter with name ParamName, returns the index if found, -1 otherwise. */
	int32 FindBoolParameterNameIndex(const FString& ParamName) const;

	/** Finds in VectorParameters a parameter with name ParamName, returns the index if found, -1 otherwise. */
	int32 FindVectorParameterNameIndex(const FString& ParamName) const;

	/** Finds in ProjectorParameters a parameter with name ParamName, returns the index if found, -1 otherwise. */
	int32 FindProjectorParameterNameIndex(const FString& ParamName) const;

	// Parameter Ranges

	/** Returns true if the parameter is multidimensional (has multiple ranges). */
	bool IsParamMultidimensional(const FString& ParamName) const;

	/** Returns true if the parameter is multidimensional (has multiple ranges). */
	bool IsParamMultidimensional(int32 ParamIndex) const;

	int32 CurrentParamRange(const FString& ParamName) const;

	/** Increases the range of values of the integer with ParamName, returns the index of the new integer value, -1 otherwise.
	 * The added value is initialized with the first integer option and is the last one of the range. */
	int32 AddValueToIntRange(const FString& ParamName);

	/** Increases the range of values of the float with ParamName, returns the index of the new float value, -1 otherwise.
	 * The added value is initialized with 0.5f and is the last one of the range. */
	int32 AddValueToFloatRange(const FString& ParamName);

	/** Increases the range of values of the projector with ParamName, returns the index of the new projector value, -1 otherwise.
	 * The added value is initialized with the default projector as set up in the editor and is the last one of the range. */
	int32 AddValueToProjectorRange(const FString& ParamName);

	/** Remove the last of the integer range of values from the parameter ParamName, returns the index of the last valid integer, -1 if no values left. */
	int32 RemoveValueFromIntRange(const FString& ParamName);

	/** Remove the RangeIndex element of the integer range of values from the parameter ParamName, returns the index of the last valid integer, -1 if no values left. */
	int32 RemoveValueFromIntRange(const FString& ParamName, int32 RangeIndex);

	/** Remove the last of the float range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left. */
	int32 RemoveValueFromFloatRange(const FString& ParamName);

	/** Remove the RangeIndex element of the float range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left. */
	int32 RemoveValueFromFloatRange(const FString& ParamName, int32 RangeIndex);

	/** Remove the last of the projector range of values from the parameter ParamName, returns the index of the last valid projector, -1 if no values left. */
	int32 RemoveValueFromProjectorRange(const FString& ParamName);

	/** Remove the RangeIndex element of the projector range of values from the parameter ParamName, returns the index of the last valid projector, -1 if no values left. */
	int32 RemoveValueFromProjectorRange(const FString& ParamName, int32 RangeIndex);

	// Default values

	FCustomizableObjectProjector GetProjectorDefaultValue(int32 ParamIndex) const;
	
	// ------------------------------------------------------------
   	// States
   	// ------------------------------------------------------------

	/** Get the current optimization state. */
	int32 GetState() const;

	/** Get the current optimization state. */	
	FString GetCurrentState() const;

	/** Set the current optimization state. */
	void SetState(int32 InState);

	/** Set the current optimization state. */
	void SetCurrentState(const FString& StateName);

	// ------------------------------------------------------------
	
	void SetRandomValues();

	// ------------------------------------------------------------
	// Multilayer Projectors
	// ------------------------------------------------------------
	
	/** Given Multilayer Projector name, create a new Multilayer Projector Helper (if non-existent). See FMultilayerProjector.
	 *
	 * @return ture if successfully created (or was already created).
	 */
	bool CreateMultiLayerProjector(const FName& ProjectorParamName);
	
	/** Given Multilayer Projector name, remove a Multilayer Projector Helper. See FMultilayerProjector. */
	void RemoveMultilayerProjector(const FName& ProjectorParamName);
	
	// Layers

	/** See FMultilayerProjector::NumLayers. */
	int32 MultilayerProjectorNumLayers(const FName& ProjectorParamName) const;

	/** See FMultilayerProjector::CreateLayer. */
	void MultilayerProjectorCreateLayer(const FName& ProjectorParamName, int32 Index);

	/** See FMultilayerProjector::RemoveLayerAt. */
	void MultilayerProjectorRemoveLayerAt(const FName& ProjectorParamName, int32 Index);

	/** See FMultilayerProjector::GetLayer. */
	FMultilayerProjectorLayer MultilayerProjectorGetLayer(const FName& ProjectorParamName, int32 Index) const;

	/** See FMultilayerProjector::UpdateLayer. */
	void MultilayerProjectorUpdateLayer(const FName& ProjectorParamName, int32 Index, const FMultilayerProjectorLayer& Layer);

	// Virtual layers

	/** See FMultilayerProjector::GetVirtualLayers. */
	TArray<FName> MultilayerProjectorGetVirtualLayers(const FName& ProjectorParamName) const;
	
	/** See FMultilayerProjector::VirtualLayer. */
	void MultilayerProjectorCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::FindOrCreateVirtualLayer. */
	FMultilayerProjectorVirtualLayer MultilayerProjectorFindOrCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::RemoveVirtualLayer. */
	void MultilayerProjectorRemoveVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::GetVirtualLayer. */
	FMultilayerProjectorVirtualLayer MultilayerProjectorGetVirtualLayer(const FName& ProjectorParamName, const FName& Id) const;

	/** See FMultilayerProjector::UpdateVirtualLayer. */
	void MultilayerProjectorUpdateVirtualLayer(const FName& ProjectorParamName, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer);

private:
	UPROPERTY()
	TObjectPtr<UCustomizableObject> CustomizableObject = nullptr;

	UPROPERTY()
	TArray<FCustomizableObjectBoolParameterValue> BoolParameters;

	UPROPERTY()
	TArray<FCustomizableObjectIntParameterValue> IntParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectFloatParameterValue> FloatParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectTextureParameterValue> TextureParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectVectorParameterValue> VectorParameters;
	
	UPROPERTY()
	TArray<FCustomizableObjectProjectorParameterValue> ProjectorParameters;

	/** Mutable parameters optimization state. */
	int32 State = 0;
	
	/** Flag to control the build of the parameter description images required for customization UI.
     * These descriptions get generated with every instance update, so it should be disabled when not needed. */
	bool bBuildParameterDecorations = false;

	/** These are the LODs we want to have, they MUST NOT be used in an update (Mutable thread). */
	int32 MinLODToLoad = 0;
	int32 MaxLODToLoad = INT32_MAX;

	/** Lookup of UCustomizableObjectDescriptor::IntParameters. */
	TMap<FString, int32> IntParametersLookupTable;
	
	/** Multilayer Projector helpers. See FMultilayerProjector.*/
	UPROPERTY()
	TMap<FName, FMultilayerProjector> MultilayerProjectors;
	
	void Init();

	void CreateParametersLookupTable();
	
	// Friends
	friend CUSTOMIZABLEOBJECT_API uint32 GetTypeHash(const FCustomizableObjectInstanceDescriptor& Key);
	friend UCustomizableObjectInstance;
	friend UCustomizableInstancePrivateData;
	friend FMultilayerProjector;
};


CUSTOMIZABLEOBJECT_API uint32 GetTypeHash(const FCustomizableObjectInstanceDescriptor& Key);