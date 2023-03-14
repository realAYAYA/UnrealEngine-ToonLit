// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObjectInstanceDescriptor.h"
#include "MuCO/CustomizableObjectParameterTypeDefinitions.h"
#include "MuCO/MultilayerProjector.h"
#include "Serialization/Archive.h"
#include "Templates/SubclassOf.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "CustomizableObjectInstance.generated.h"

class AActor;
class FProperty;
class UAnimInstance;
class UCustomizableInstancePrivateData; // This is used to hide Mutable SDK members in the public headers.
class UCustomizableObject;
class UCustomizableSkeletalComponent;
class UTexture2D;
struct FFrame;
struct FGameplayTagContainer;
struct FPropertyChangedEvent;

//! Order of the unreal vertex buffers when in mutable data
#define MUTABLE_VERTEXBUFFER_POSITION	0
#define MUTABLE_VERTEXBUFFER_TANGENT	1
#define MUTABLE_VERTEXBUFFER_TEXCOORDS	2


USTRUCT()
struct FCustomizedMaterialTexture2D
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(Category=CustomizedMaterialTexture2D, VisibleAnywhere)
	FName Name;
	
	UPROPERTY(Category=CustomizedMaterialTexture2D, EditAnywhere) // Replaced EditInline with EditAnywhere for 4.10
	TObjectPtr<UTexture2D> Texture;


	FCustomizedMaterialTexture2D()
		: Texture(nullptr)
	{
	}

	inline friend FArchive& operator<<( FArchive& Ar, FCustomizedMaterialTexture2D& Data )
	{
		Ar << Data.Name;
		Ar << Data.Texture;
		return Ar;
	}
	
};


// Current state of a projector associated with a parameter.
namespace EProjectorState
{
	typedef uint8 Type;
	
	const Type Hidden = 0;
	const Type Translate = 1;
	const Type Rotate = 2;
	const Type Scale = 3;	
	const Type Selected = 4;
	const Type TypeChanged = 5;
};


/** FString with the possible errors from skeletal mesh update */
namespace ESkeletalMeshState
{
	const FString Correct = "Correct";
	const FString UpdateError = "Update error";
	const FString PostUpdateError = "Post Update Error";
	const FString AsyncUpdatePending = "Async update pending";
};


/** Instance Update Result. */
enum class EUpdateResult : uint8
{
	Success,
	Error
};


/* When creating new delegates use the following conventions:
 *
 * - All delegates must be multicast.
 * - If the delegate is exposed to the API create both, dynamic and native versions (non-dynamic).
 * - Dynamic delegates should not be transient. Use the native version if you do not want it to be saved.
 * - Native delegates names should end with "NativeDelegate".
 * - Dynamic delegates broadcast before native delegates. */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeginUpdateDelegate, UCustomizableObjectInstance*, Instance);
DECLARE_MULTICAST_DELEGATE_OneParam(FBeginUpdateNativeDelegate, UCustomizableObjectInstance*);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FObjectInstanceUpdatedDelegate, UCustomizableObjectInstance*, Instance);
DECLARE_MULTICAST_DELEGATE_OneParam(FObjectInstanceUpdatedNativeDelegate, UCustomizableObjectInstance*);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeginDestroyDelegate, UCustomizableObjectInstance*, Instance);
DECLARE_MULTICAST_DELEGATE_OneParam(FBeginDestroyNativeDelegate, UCustomizableObjectInstance*);

DECLARE_DELEGATE_OneParam(FProjectorStateChangedDelegate, FString);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FEachComponentAnimInstanceClassDelegate, int32, SlotIndex, TSubclassOf<UAnimInstance>, AnimInstClass);

DECLARE_DELEGATE_TwoParams(FEachComponentAnimInstanceClassNativeDelegate, int32 /*SlotIndex*/, TSubclassOf<UAnimInstance> /*AnimInstClass*/);


UCLASS( Blueprintable, BlueprintType, HideCategories=(CustomizableObjectInstance) )
class CUSTOMIZABLEOBJECT_API UCustomizableObjectInstance : public UObject
{
	GENERATED_BODY()

	friend UCustomizableInstancePrivateData;

public:
	UCustomizableObjectInstance();

	FCustomizableObjectInstanceDescriptor& GetDescriptor();

	const FCustomizableObjectInstanceDescriptor& GetDescriptor() const;
	
	void SetDescriptor(const FCustomizableObjectInstanceDescriptor& InDescriptor);

	/** Broadcast at the beginning of an Instance update. */
	UPROPERTY(BlueprintAssignable, Category = CustomizableObjectInstance)
	FBeginUpdateDelegate BeginUpdateDelegate;

	/** Broadcast at the beginning of an Instance update. */
	FBeginUpdateNativeDelegate BeginUpdateNativeDelegate;

	/** Broadcast when the Customizable Object Instance is updated. */
	UPROPERTY(Transient, BlueprintAssignable, Category = CustomizableObjectInstance)
	FObjectInstanceUpdatedDelegate UpdatedDelegate;

	/** Broadcast when the Customizable Object Instance is updated. */
	FObjectInstanceUpdatedNativeDelegate UpdatedNativeDelegate;

	/** Broadcast when UObject::BeginDestroy is being called. */	
	UPROPERTY(BlueprintAssignable, Category = CustomizableObjectInstance)
	FBeginDestroyDelegate BeginDestroyDelegate;

	/** Broadcast when UObject::BeginDestroy is being called. */
	FBeginDestroyNativeDelegate BeginDestroyNativeDelegate;

	TMap<FString, bool> ParamNameToExpandedMap; // Used to check whether a mutable param is expanded in the editor to show its child params

	/** The generated skeletal meshes for this Instance, one for each component */
	UPROPERTY(Transient, VisibleAnywhere, Category = CustomizableSkeletalMesh)
	TArray< TObjectPtr<USkeletalMesh> > SkeletalMeshes;

	// Will store status description of current skeletal mesh generation (for instance, "EmptyLOD0" or "EmptyMesh"
	UPROPERTY()
	FString SkeletalMeshStatus;

#if WITH_EDITOR
	// Will store the previous status description to avoid losing notifications with partial updates.
	FString PreUpdateSkeletalMeshStatus;

	/** During editor, always remember the duration of the last update in the mutable runtime, for profiling. */
	int32 LastUpdateMutableRuntimeCycles = 0;
#endif

public:

	// UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange( const FProperty* InProperty ) const override;
	bool InstanceUpdated; // Flag for the editor, to know when the instance's skeletal mesh has been updated
#endif //WITH_EDITOR

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual FString GetDesc() override;
	virtual bool IsEditorOnly() const override;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetObject(UCustomizableObject* InObject);

	//Get the current CustomizableObject 
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	UCustomizableObject* GetCustomizableObject() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool GetBuildParameterDecorations() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetBuildParameterDecorations(bool Value);

	int32 GetState() const;
	void SetState(int32 InState);

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FString GetCurrentState() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetCurrentState(const FString& StateName);

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	USkeletalMesh* GetSkeletalMesh(int32 ComponentIndex = 0) const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool HasAnySkeletalMesh() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectBoolParameterValue>& GetBoolParameters();

	const TArray<FCustomizableObjectBoolParameterValue>& GetBoolParameters() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectIntParameterValue>& GetIntParameters();

	const TArray<FCustomizableObjectIntParameterValue>& GetIntParameters() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectFloatParameterValue>& GetFloatParameters();

	const TArray<FCustomizableObjectFloatParameterValue>& GetFloatParameters() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectTextureParameterValue>& GetTextureParameters();

	const TArray<FCustomizableObjectTextureParameterValue>& GetTextureParameters() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectVectorParameterValue>& GetVectorParameters();

	const TArray<FCustomizableObjectVectorParameterValue>& GetVectorParameters() const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectProjectorParameterValue>& GetProjectorParameters();
	
	const TArray<FCustomizableObjectProjectorParameterValue>& GetProjectorParameters() const;

	/** See FCustomizableObjectInstanceDescriptor::HasAnyParameters. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool HasAnyParameters() const;
	
	//! Set random values to the parameters. Useful for testing only.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetRandomValues();

	// Utilities to manage saving and loading parameters from profiles.
	bool LoadParametersFromProfile(int32 ProfileIndex);
	bool SaveParametersToProfile(int32 ProfileIndex);
	bool MigrateProfileParametersToCurrentInstance(int32 ProfileIndex);
	void SetSelectedParameterProfileDirty();
	bool IsSelectedParameterProfileDirty() const;

	//
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void UpdateSkeletalMeshAsync(bool bIgnoreCloseDist = false, bool bForceHighPriority = false);

	// Clones the instance creating a new identical transient instance.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	UCustomizableObjectInstance* Clone();

	// Clones the instance creating a new identical static instance.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	UCustomizableObjectInstance* CloneStatic(UObject* Outer);

	// Copy parameters from input instance
	void CopyParametersFromInstance(UCustomizableObjectInstance* Instance);

	// Releases all the mutable resources this instance holds, should only be called when it is not going to be used any more.
	void ReleaseMutableResources(bool bCalledFromBeginDestroy);

	// Get a description image for a parameter.
	// The param index is in the range of o to CustomizableObject->GetParameterCount.
	// This will only be valid if bBuildParameterDecorations was set to true before the last update.
	UTexture2D* GetParameterDescription(int32 ParamIndex, int32 DescIndex);

	// Returns de description texture (ex: color bar) for this parameter and DescIndex
	// This will only be valid if bBuildParameterDecorations was set to true before the last update.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	UTexture2D* GetParameterDescription(const FString& ParamName, int32 DescIndex);

	//! This is only valid if bBuildParameterDecorations has been set before the last update.
	bool IsParameterRelevant( int32 ParameterIndex ) const;

	//! This is only valid if bBuildParameterDecorations has been set before the last update.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool IsParameterRelevant(const FString& ParamName) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool IsParamMultidimensional(const FString& ParamName) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 CurrentParamRange(const FString& ParamName) const;
	
	bool IsParamMultidimensional(int32 ParamIndex) const;
	FCustomizableObjectProjector GetProjectorDefaultValue(int32 ParamIndex) const;

	bool bShowOnlyRuntimeParameters = true;
	bool bShowOnlyRelevantParameters = true;

	/** Control the display of support widgets to edit projectors of this instance. */
	void SetProjectorState( const FString& ParamName, int32 RangeIndex, EProjectorState::Type state );
	void ResetProjectorStates();
	EProjectorState::Type GetProjectorState(const FString& ParamName, int32 RangeIndex) const;
	FProjectorStateChangedDelegate ProjectorStateChangedDelegate;

	// DEPRECATED: Use the method in the CustomizableObject instead which takes an index among all parameters
	// Returns how many possible options an int parameter has
	//int32 GetIntParameterNumOptions(int32 IntParamIndex);

	// DEPRECATED: Use the method in the CustomizableObject instead which takes an index among all parameters
	// Gets the Name of the option at position K in the list of available options for the int parameter. Useful to enumerate the int parameter's possible options (Ex: "Hat1", "Hat2", "Cap", "Nothing")
	//const FString& GetIntParameterAvailableOption(int32 IntParamIndex, int32 K);

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	const FString& GetIntParameterSelectedOption(const FString& ParamName, int32 RangeIndex = -1) const;

	// Sets the selected option of an int parameter by the option's name
	void SetIntParameterSelectedOption(int32 IntParamIndex, const FString& SelectedOption, int32 RangeIndex = -1);

	// Sets the selected option of an int parameter, by the option's name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetIntParameterSelectedOption(const FString& ParamName, const FString& SelectedOptionName, int32 RangeIndex = -1);

	// Gets the value of a float parameter with name "FloatParamName"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	float GetFloatParameterSelectedOption(const FString& FloatParamName, int32 RangeIndex = -1) const;

	// Sets the float value "FloatValue" of a float parameter with index "FloatParamIndex"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetFloatParameterSelectedOption(const FString& FloatParamName, float FloatValue, int32 RangeIndex = -1);

	// Gets the value of a color parameter with name "ColorParamName"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FLinearColor GetColorParameterSelectedOption(const FString& ColorParamName) const;

	// Sets the color value "ColorValue" of a color parameter with index "ColorParamIndex"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetColorParameterSelectedOption(const FString& ColorParamName, const FLinearColor& ColorValue);

	// Sets the bool value "BoolValue" of a bool parameter with name "BoolParamName"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool GetBoolParameterSelectedOption(const FString& BoolParamName) const;

	// Sets the bool value "BoolValue" of a bool parameter with name "BoolParamName"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetBoolParameterSelectedOption(const FString& BoolParamName, bool BoolValue);

	// Sets the vector value "VectorValue" of a bool parameter with index "VectorParamIndex"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetVectorParameterSelectedOption(const FString& VectorParamName, const FLinearColor& VectorValue);

	// Sets the projector values of a projector parameter with index "ProjectorParamIndex"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetProjectorValue(const FString& ProjectorParamName,
		const FVector& OutPos, const FVector& OutDirection, const FVector& OutUp, const FVector& OutScale,
		float OutAngle,
		int32 RangeIndex = -1);

	/** Set only the projector position. */
	void SetProjectorPosition(const FString& ProjectorParamName, const FVector3f& Pos, int32 RangeIndex = -1);

	// Get the projector values of a projector parameter with index "ProjectorParamIndex"
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void GetProjectorValue(const FString& ProjectorParamName,
		FVector& OutPos, FVector& OutDirection, FVector& OutUp, FVector& OutScale,
		float& OutAngle, ECustomizableObjectProjectorType& OutType,
		int32 RangeIndex = -1) const;

	void GetProjectorValueF(const FString& ProjectorParamName,
		FVector3f& Pos, FVector3f& Direction, FVector3f& Up, FVector3f& Scale,
		float& Angle, ECustomizableObjectProjectorType& Type,
		int32 RangeIndex = -1) const;

	// Get the current projector position for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FVector GetProjectorPosition(const FString & ParamName, int32 RangeIndex = -1) const;

	// Get the current projector direction vector for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FVector GetProjectorDirection(const FString & ParamName, int32 RangeIndex = -1) const;

	// Get the current projector up vector for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FVector GetProjectorUp(const FString & ParamName, int32 RangeIndex = -1) const;

	// Get the current projector scale for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FVector GetProjectorScale(const FString & ParamName, int32 RangeIndex = -1) const;

	// Get the current cylindrical projector angle for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	float GetProjectorAngle(const FString& ParamName, int32 RangeIndex = -1) const;

	// Get the current projector type for the parameter with the given name
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	ECustomizableObjectProjectorType GetProjectorParameterType(const FString& ParamName, int32 RangeIndex = -1) const;

	/** See FCustomizableObjectInstanceDescriptor::GetProjector. */
	FCustomizableObjectProjector GetProjector(const FString& ParamName, int32 RangeIndex) const;
	
	// Finds in IntParameters a parameter with name ParamName, returns the index if found, -1 otherwise
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 FindIntParameterNameIndex(const FString& ParamName) const;

	// Finds in FloatParameters a parameter with name ParamName, returns the index if found, -1 otherwise
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 FindFloatParameterNameIndex(const FString& ParamName) const;

	// Finds in BoolParameters a parameter with name ParamName, returns the index if found, -1 otherwise
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 FindBoolParameterNameIndex(const FString& ParamName) const;

	// Finds in VectorParameters a parameter with name ParamName, returns the index if found, -1 otherwise
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 FindVectorParameterNameIndex(const FString& ParamName) const;

	// Finds in ProjectorParameters a parameter with name ParamName, returns the index if found, -1 otherwise
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 FindProjectorParameterNameIndex(const FString& ParamName) const;

	// Increases the range of values of the integer with ParamName, returns the index of the new integer value, -1 otherwise.
	// The added value is initialized with the first integer option and is the last one of the range.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 AddValueToIntRange(const FString& ParamName);

	// Increases the range of values of the float with ParamName, returns the index of the new float value, -1 otherwise.
	// The added value is initialized with 0.5f and is the last one of the range.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 AddValueToFloatRange(const FString& ParamName);

	// Increases the range of values of the projector with ParamName, returns the index of the new projector value, -1 otherwise.
	// The added value is initialized with the default projector as set up in the editor and is the last one of the range.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 AddValueToProjectorRange(const FString& ParamName);

	// Remove the last of the integer range of values from the parameter ParamName, returns the index of the last valid integer, -1 if no values left.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 RemoveValueFromIntRange(const FString& ParamName);

	// Remove the RangeIndex element of the integer range of values from the parameter ParamName, returns the index of the last valid integer, -1 if no values left.
	int32 RemoveValueFromIntRange(const FString& ParamName, int32 RangeIndex);

	// Remove the last of the float range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 RemoveValueFromFloatRange(const FString& ParamName);

	// Remove the RangeIndex element of the float range of values from the parameter ParamName, returns the index of the last valid float, -1 if no values left.
	int32 RemoveValueFromFloatRange(const FString& ParamName, int32 RangeIndex);

	// Remove the last of the projector range of values from the parameter ParamName, returns the index of the last valid projector, -1 if no values left.
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 RemoveValueFromProjectorRange(const FString& ParamName);

	// Remove the RangeIndex element of the projector range of values from the parameter ParamName, returns the index of the last valid projector, -1 if no values left.
	int32 RemoveValueFromProjectorRange(const FString& ParamName, int32 RangeIndex);

	// ------------------------------------------------------------
	// Multilayer Projectors
	// ------------------------------------------------------------
	
	/** Given Multilayer Projector name, create a new Multilayer Projector Helper (if non-existent). See FMultilayerProjector.
	 *
	 * @return ture if successfully created (or was already created).
	 */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	bool CreateMultiLayerProjector(const FName& ProjectorParamName);
	
	/** Given Multilayer Projector name, remove a Multilayer Projector Helper. See FMultilayerProjector. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void RemoveMultilayerProjector(const FName& ProjectorParamName);
	
	// Layers

	/** See FMultilayerProjector::NumLayers. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	int32 MultilayerProjectorNumLayers(const FName& ProjectorParamName) const;

	/** See FMultilayerProjector::CreateLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorCreateLayer(const FName& ProjectorParamName, int32 Index);

	/** See FMultilayerProjector::RemoveLayerAt. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorRemoveLayerAt(const FName& ProjectorParamName, int32 Index);

	/** See FMultilayerProjector::GetLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FMultilayerProjectorLayer MultilayerProjectorGetLayer(const FName& ProjectorParamName, int32 Index) const;

	/** See FMultilayerProjector::UpdateLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorUpdateLayer(const FName& ProjectorParamName, int32 Index, const FMultilayerProjectorLayer& Layer);

	// Virtual layers

	/** See FMultilayerProjector::GetVirtualLayers. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TArray<FName> MultilayerProjectorGetVirtualLayers(const FName& ProjectorParamName) const;
	
	/** See FMultilayerProjector::VirtualLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::FindOrCreateVirtualLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FMultilayerProjectorVirtualLayer MultilayerProjectorFindOrCreateVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::RemoveVirtualLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorRemoveVirtualLayer(const FName& ProjectorParamName, const FName& Id);

	/** See FMultilayerProjector::GetVirtualLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	FMultilayerProjectorVirtualLayer MultilayerProjectorGetVirtualLayer(const FName& ProjectorParamName, const FName& Id) const;

	/** See FMultilayerProjector::UpdateVirtualLayer. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void MultilayerProjectorUpdateVirtualLayer(const FName& ProjectorParamName, const FName& Id, const FMultilayerProjectorVirtualLayer& Layer);
	
	// ------------------------------------------------------------
	
	// Returns the animation BP for the parameter component and slot, gathered from all the meshes that compose this instance
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	TSubclassOf<UAnimInstance> GetAnimBP(int32 ComponentIndex, int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	const FGameplayTagContainer& GetAnimationGameplayTags() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void ForEachAnimInstance(int32 ComponentIndex, FEachComponentAnimInstanceClassDelegate Delegate) const;

	void ForEachAnimInstance(int32 ComponentIndex, FEachComponentAnimInstanceClassNativeDelegate Delegate) const;

	/** Serializes/Deserializes all the customization parameters to/from a descriptor, ready to be sent/read, works like a typical UE4 Two-Way Save System Function  */
	void SaveDescriptor(FArchive &CustomizableObjectDescriptor);
	void LoadDescriptor(FArchive &CustomizableObjectDescriptor);

	// Enable physics asset replacement so that generated skeletal meshes have the merged physics assets of their skeletal mesh parts and reference mesh
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstance)
	void SetReplacePhysicsAssets(bool bReplaceEnabled);

	// Enables the reuse of all possible textures when the instance is updated without any changes in geometry or state (the first update after creation doesn't reuse any)
	// It will only work if the textures aren't compressed, so set the instance to a Mutable state with texture compression disabled
	// WARNING! If texture reuse is enabled, do NOT keep external references to the textures of the instance. The instance owns the textures.
	void SetReuseInstanceTextures(bool bTextureReuseEnabled);

	// Adds/removes a texture channel coverage query to the instance update process. Every material updated by mutable with a channel named TextureName will be checked
	// It only works properly when the Instance is in a state with texture compression disabled.
	void AddQueryTextureCoverage(const FString& TextureName, const FString* MaskOutChannelName = nullptr);
	void RemoveQueryTextureCoverage(const FString& TextureName);

	// Returns the result of a texture coverage query previously registered with AddQueryTextureCoverage. The query is run during an update of the instance.
	// It only works properly when the Instance is in a state with texture compression disabled.
	float GetQueryResultTextureCoverage(const FString& TextureName);
	float GetQueryResultTextureCoverageMasked(const FString& TextureName); // Same as the previous query but this time intersected with the mask texture

	void AdditionalAssetsAsyncLoaded( FGraphEventRef CompletionEvent );

	// The following methods should only be used in an LOD management class
	void SetIsBeingUsedByComponentInPlay(bool bIsUsedByComponent );
	bool GetIsBeingUsedByComponentInPlay() const;
	void SetIsDiscardedBecauseOfTooManyInstances(bool bIsDiscarded);
	bool GetIsDiscardedBecauseOfTooManyInstances() const;
	void SetIsPlayerOrNearIt(bool NewValue);
	float GetMinSquareDistToPlayer() const;
	void SetMinSquareDistToPlayer(float NewValue);
	void SetMinMaxLODToLoad(int32 NewMinLOD, int32 NewMaxLOD, bool LimitLODUpgrades = true);
	int32 GetMinLODToLoad() const;
	int32 GetMaxLODToLoad() const;
	int32 GetNumLODsAvailable() const;

	/* If enabled, CurrentMinLOD will be the first LOD of the generated SkeletalMesh. Otherwise the number of LODs will remain constant and LODs [0 .. CurrentMinLOD] will share the same RenderData. 
	 * Enabled by default */
	void SetUseCurrentMinLODAsBaseLOD(bool bIsBaseLOD);
	bool GetUseCurrentMinLODAsBaseLOD() const;

	/** Instance updated. */
	void Updated(EUpdateResult Result);
	
	/** Hash representing the actual state of the instance (meshes, textures...). This does not include the parameters. */
	uint32 GetDescriptorHash() const;

	/** Hash representing the state of the instance when the update was requested. */
	uint32 GetUpdateDescriptorHash() const;

	// --------------------------------------------------------------------

	/** Flag to know if a property of this instance changed in the editor */
	bool bEditorPropertyChanged = false;

	UCustomizableInstancePrivateData* GetPrivate() const { 
		check(nullptr != PrivateData); // Currently this is initialized in the constructor so we expect it always to exist.
		return PrivateData; 
	}

	bool ProjectorUpdatedInViewport = false;

	// TEMP VARIABLE to ease updating the gizmo in the editor after pasting a new projector's transform information
	bool TempUpdateGizmoInViewport = false;

	// TEMP VARIABLE to verify the projector parameter with transform modified by Paste Transform is set as selected
	FString TempProjectorParameterName;

	// TEMP VARIABLE to verify the projector parameter with transform modified by Paste Transform is set as selected
	int32 TempProjectorParameterRangeIndex;

	// TEMP VARIABLE to avoid the projector selection being reset after pasting transform
	bool AvoidResetProjectorVisibilityForNonNode = false;

	// TEMP VARIABLE to check the Min desired LODs for this instance
	TWeakObjectPtr<UCustomizableSkeletalComponent> NearestToActor;
	TWeakObjectPtr<const AActor> NearestToViewCenter;

#if WITH_EDITOR
	/** If there's a projector parameter pending to be hidden, name of that projector parameter */
	FString LastSelectedProjectorParameter;

	/** For the case of projector parameter in several layers (the name of the projector is the same and only the index will change */
	FString LastSelectedProjectorParameterWithIndex;

	/** Flag to ease detect a projector layer change event in FCustomizableObjectEditor::OnObjectModified */
	bool ProjectorLayerChange = false;

	/** Flag to avoid resetting projector state when editing the alpha value of a layer */
	bool ProjectorAlphaChange = false;

	/** Flag to unselect the projector */
	bool UnselectProjector = false;

	/** Profile index the instance parameters are in and if the profile needs to be refreshed */
	int32 SelectedProfileIndex = INDEX_NONE;
	bool bSelectedProfileDirty = false;

	/** Tag required to avoid updating the wrong projector parameter range index once removed.
	On a Group Projector Parameter, if a projector gizmo is removed while is selected, the projector is unselected and removed. The unselection will causes a late update
	to that projector index, which is performed after the projector being removed. During the update, since the projector index has been removed, the update index is no
	longer valid. This tag allows to check if the last removed index matches with the currently update index. In the case they match, the update does not occur.*/
	FString RemovedProjectorParameterNameWithIndex;
#endif 

private:
	UPROPERTY()
	FCustomizableObjectInstanceDescriptor Descriptor;

	UPROPERTY( Transient )
	TObjectPtr<UCustomizableInstancePrivateData> PrivateData;

	/** Hash of the UCustomizableObjectInstance::Descriptor when UpdateSkeletalMeshAsync has been called. */
	uint32 UpdateDescriptorHash = 0;
	
	/** Hash of the UCustomizableObjectInstance::Descriptor on the last successful update. */
	uint32 DescriptorHash = 0;

	// Deprecated properties
	
	UPROPERTY()
	TObjectPtr<UCustomizableObject> CustomizableObject_DEPRECATED;
	
	UPROPERTY()
	TArray<FCustomizableObjectBoolParameterValue> BoolParameters_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectIntParameterValue> IntParameters_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectFloatParameterValue> FloatParameters_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectTextureParameterValue> TextureParameters_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectVectorParameterValue> VectorParameters_DEPRECATED;

	UPROPERTY()
	TArray<FCustomizableObjectProjectorParameterValue> ProjectorParameters_DEPRECATED;
	
   	UPROPERTY()
   	TMap<FName, FMultilayerProjector> MultilayerProjectors_DEPRECATED;

	UPROPERTY()
	bool bBuildParameterDecorations_DEPRECATED;
};

#if WITH_EDITOR
CUSTOMIZABLEOBJECT_API void CopyTextureProperties(UTexture2D* Texture, const UTexture2D* SourceTexture);
#endif	