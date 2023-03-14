// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Field.h"

#include "PropertyValue.generated.h"

#define PATH_DELIMITER TEXT(" / ")
#define ATTACH_CHILDREN_NAME TEXT("Children")

DECLARE_MULTICAST_DELEGATE(FOnPropertyRecorded);
DECLARE_MULTICAST_DELEGATE(FOnPropertyApplied);

class UVariantObjectBinding;
class USCS_Node;
class FProperty;

UENUM()
enum class EPropertyValueCategory : uint8
{
	Undefined = 0,
	Generic = 1,
	RelativeLocation = 2,
	RelativeRotation = 4,
	RelativeScale3D = 8,
	Visibility = 16,
	Material = 32,
	Color = 64,
	Option = 128
};
ENUM_CLASS_FLAGS(EPropertyValueCategory)

// Describes one link in a full property path
// For array properties, a link might be the outer (e.g. AttachChildren, -1, None)
// while also it may be an inner (e.g. AttachChildren, 2, Cube)
// Doing this allows us to resolve components regardless of their order, which
// is important for handling component reordering and transient components (e.g.
// runtime billboard components, etc)
USTRUCT()
struct VARIANTMANAGERCONTENT_API FCapturedPropSegment
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString PropertyName;

	UPROPERTY()
	int32 PropertyIndex = INDEX_NONE;

	UPROPERTY()
	FString ComponentName;
};

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValue : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	void Init(const TArray<FCapturedPropSegment>& InCapturedPropSegments, FFieldClass* InLeafPropertyClass, const FString& InFullDisplayString, const FName& InPropertySetterName, EPropertyValueCategory InCategory = EPropertyValueCategory::Generic);

	class UVariantObjectBinding* GetParent() const;

	// Combined hash of this property and its indices
	// We don't use GetTypeHash for this because almost always we want to hash UPropertyValues by
	// the pointer instead, for complete uniqueness even with the same propertypath
	// This is mostly just used for grouping UPropertyValues together for editing multiple at once
	uint32 GetPropertyPathHash();

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	// Tries to resolve the property value on the passed object, or the parent binding's bound object if the argument is nullptr
	virtual bool Resolve(UObject* OnObject = nullptr);
	bool HasValidResolve() const;
	void ClearLastResolve();

	void* GetPropertyParentContainerAddress() const;
	virtual UStruct* GetPropertyParentContainerClass() const;

	// Fetches the value bytes for this property from the resolved object
	virtual TArray<uint8> GetDataFromResolvedObject() const;

	// Uses GetDataFromResolvedObject to update our recorded data
	virtual void RecordDataFromResolvedObject();

	// Applies our recorded data to the PropertyValuePtr for the resolved object
	virtual void ApplyDataToResolvedObject();

	// Returns the type of FProperty (FObjectProperty, FFloatProperty, etc)
	// If checking for Enums, prefer checking if GetEnumPropertyEnum() != nullptr, as it may be that
	// we received a FNumericProperty that actually represents an enum, which wouldn't be reflected here
	virtual FFieldClass* GetPropertyClass() const;
	EPropertyValueCategory GetPropCategory() const;
	virtual UScriptStruct* GetStructPropertyStruct() const;
	virtual UClass* GetObjectPropertyObjectClass() const;
	UEnum* GetEnumPropertyEnum() const;
	virtual bool ContainsProperty(const FProperty* Prop) const;

	// Returns an array of link segments that together describe the full property path
	const TArray<FCapturedPropSegment>& GetCapturedPropSegments() const;

	// Utility functions for UEnumProperties
	TArray<FName> GetValidEnumsFromPropertyOverride();
	FString GetEnumDocumentationLink();
	// Used RecordedData as an enum value and gets the corresponding index for our Enum
	int32 GetRecordedDataAsEnumIndex();
	// Sets our RecordedData with the value that matches Index, for our Enum
	void SetRecordedDataFromEnumIndex(int32 Index);
	// Makes sure RecordedData data is a valid Enum index for our Enum (_MAX is not allowed)
	void SanitizeRecordedEnumData();
	bool IsNumericPropertySigned();
	bool IsNumericPropertyUnsigned();
	bool IsNumericPropertyFloatingPoint();

	// Utility functions for string properties
	const FName& GetNamePropertyName() const;
	const FString& GetStrPropertyString() const;
	const FText& GetTextPropertyText() const;

	FName GetPropertyName() const;
	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	FText GetPropertyTooltip() const;
	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	const FString& GetFullDisplayString() const;
	FString GetLeafDisplayString() const;
	virtual int32 GetValueSizeInBytes() const;
	int32 GetPropertyOffsetInBytes() const;

	UFUNCTION(BlueprintCallable, Category="PropertyValue")
	bool HasRecordedData() const;
	const TArray<uint8>& GetRecordedData();
	virtual void SetRecordedData(const uint8* NewDataBytes, int32 NumBytes, int32 Offset = 0);
	virtual const TArray<uint8>& GetDefaultValue();
	void ClearDefaultValue();
	// Returns true if our recorded data would remain the same if we called
	// RecordDataFromResolvedObject right now
	virtual bool IsRecordedDataCurrent();

	FOnPropertyApplied& GetOnPropertyApplied();
	FOnPropertyRecorded& GetOnPropertyRecorded();

#if WITH_EDITORONLY_DATA
	/**
	 * Get the order with which the VariantManager should display this in a property list. Lower values will be shown higher up
	 */
	uint32 GetDisplayOrder() const;

	/**
	 * Set the order with which the VariantManager should display this in a property list. Lower values will be shown higher up
	 */
	void SetDisplayOrder(uint32 InDisplayOrder);
#endif //WITH_EDITORONLY_DATA

protected:

	void SetRecordedDataInternal(const uint8* NewDataBytes, int32 NumBytes, int32 Offset = 0);

	FProperty* GetProperty() const;

	// Applies the recorded data to the TargetObject via the PropertySetter function
	// (e.g. SetIntensity instead of setting the Intensity UPROPERTY directly)
	virtual void ApplyViaFunctionSetter(UObject* TargetObject);

	// Recursively navigate the component/USCS_Node hierarchy trying to resolve our property path
	bool ResolveUSCSNodeRecursive(const USCS_Node* Node, int32 SegmentIndex);
	bool ResolvePropertiesRecursive(UStruct* ContainerClass, void* ContainerAddress, int32 PropertyIndex);

	FOnPropertyApplied OnPropertyApplied;
	FOnPropertyRecorded OnPropertyRecorded;
	
#if WITH_EDITOR
	void OnPIEEnded(const bool bIsSimulatingInEditor);
#endif

	// Temp data cached from last resolve
	FProperty* LeafProperty;
	UStruct* ParentContainerClass;
	void* ParentContainerAddress;
	UObject* ParentContainerObject; // Leafmost UObject* in the property path. Required as ParentContainerAddress
	uint8* PropertyValuePtr;        // may be pointing at a C++ struct
	UFunction* PropertySetter;

	// Properties were previously stored like this. Use CapturedPropSegments from now on, which stores
	// properties by name instead. It is much safer, as we can't guarantee these pointers will be valid
	// if they point at other packages (will depend on package load order, etc).
	UPROPERTY()
	TArray<TFieldPath<FProperty>> Properties_DEPRECATED;
	UPROPERTY()
	TArray<int32> PropertyIndices_DEPRECATED;

	UPROPERTY()
	TArray<FCapturedPropSegment> CapturedPropSegments;

	UPROPERTY()
	FString FullDisplayString;

	UPROPERTY()
	FName PropertySetterName;

	UPROPERTY()
	TMap<FString, FString> PropertySetterParameterDefaults;

	UPROPERTY()
	bool bHasRecordedData;

	// We use these mainly to know how to serialize/deserialize the values of properties that need special care
	// (e.g. UObjectProperties, name properties, text properties, etc)
	UPROPERTY()
	TObjectPtr<UClass> LeafPropertyClass_DEPRECATED;
	FFieldClass* LeafPropertyClass;

	UPROPERTY()
	TArray<uint8> ValueBytes;

	UPROPERTY()
	EPropertyValueCategory PropCategory;

	TArray<uint8> DefaultValue;

	TSoftObjectPtr<UObject> TempObjPtr;
	FName TempName;
	FString TempStr;
	FText TempText;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 DisplayOrder;
#endif //WITH_EDITORONLY_DATA
};

// Deprecated: Only here for backwards compatibility with 4.21
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueTransform : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};

// Deprecated: Only here for backwards compatibility
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueVisibility : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

	// UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface
};