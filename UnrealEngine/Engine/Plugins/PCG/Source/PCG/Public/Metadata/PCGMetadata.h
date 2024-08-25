// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGMetadataCommon.h" // IWYU pragma: keep
#include "PCGMetadataAttributeTraits.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "PCGMetadata.generated.h"

struct FPCGPoint;
struct FPCGContext;

UCLASS(BlueprintType)
class PCG_API UPCGMetadata : public UObject
{
	GENERATED_BODY()

public:

	//~ Begin UObject interface
	virtual void Serialize(FArchive& InArchive) override;
	virtual void BeginDestroy() override;
	//~ End UObject interface

	/** Initializes the metadata from a parent metadata, if any (can be null). Copies attributes and values. */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void Initialize(const UPCGMetadata* InParent) { InitializeWithAttributeFilter(InParent, TSet<FName>()); }

	/** Initializes the metadata from a parent metadata, if any (can be null) with the option to not add attributes from the parent. */
	void Initialize(const UPCGMetadata* InParent, bool bAddAttributesFromParent);

	/**
	 * Initializes the metadata from a parent metadata. Copies attributes and values.
	 * @param InParent The parent metadata to use as a template, if any (can be null).
	 * @param InFilteredAttributes Optional list of attributes to exclude or include when adding the attributes from the parent.
	 * @param InFilterMode Defines attribute filter operation.
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void InitializeWithAttributeFilter(const UPCGMetadata* InParent, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode = EPCGMetadataFilterMode::ExcludeAttributes);

	/** Initializes the metadata from a parent metadata by copying all attributes to it.
	* @param InMetadataToCopy Metadata to copy from
	* @param InOptionalEntriesToCopy Optional array that contains the keys to copy over. This array order will be respected, so it can also be used to re-order entries. Can be null.
	*/
	void InitializeAsCopy(const UPCGMetadata* InMetadataToCopy, const TArray<PCGMetadataEntryKey>* InOptionalEntriesToCopy = nullptr);

	/** Initializes the metadata from a parent metadata by copy filtered attributes only to it
	* @param InMetadataToCopy Metadata to copy from
	* @param InFilteredAttributes Attributes to keep/exclude, can be empty.
	* @param InFilterMode Filter to know if we should keep or exclude InFilteredAttributes.
	* @param InOptionalEntriesToCopy Optional array that contains the keys to copy over. This array order will be respected, so it can also be used to re-order entries. Can be null.
	*/
	void InitializeAsCopyWithAttributeFilter(const UPCGMetadata* InMetadataToCopy, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode = EPCGMetadataFilterMode::ExcludeAttributes, const TArray<PCGMetadataEntryKey>* InOptionalEntriesToCopy = nullptr);

	/** Creates missing attributes from another metadata if they are not currently present - note that this does not copy values */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AddAttributes(const UPCGMetadata* InOther) { AddAttributesFiltered(InOther, TSet<FName>()); }

	/**
	 * Creates missing attributes from another metadata if they are not currently present - note that this does not copy values.
	 * @param InOther The other metadata to obtain a list of attributes from.
	 * @param InFilteredAttributes Optional list of attributes to exclude or include when adding the attributes.
	 * @param InFilterMode Defines attribute filter operation.
	 */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AddAttributesFiltered(const UPCGMetadata* InOther, const TSet<FName>& InFilteredAttributes, EPCGMetadataFilterMode InFilterMode = EPCGMetadataFilterMode::ExcludeAttributes);

	/** Creates missing attribute from another metadata if it is not currently present - note that this does not copy values */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AddAttribute(const UPCGMetadata* InOther, FName AttributeName);

	/** Copies attributes from another metadata, including entries & values. Warning: this is intended when dealing with the same data set */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata|Advanced")
	void CopyAttributes(const UPCGMetadata* InOther);

	/** Copies an attribute from another metadata, including entries & values. Warning: this is intended when dealing with the same data set */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata|Advanced")
	void CopyAttribute(const UPCGMetadata* InOther, FName AttributeToCopy, FName NewAttributeName);

	/** Copies another attribute, with options to keep its parent and copy entries/values */
	FPCGMetadataAttributeBase* CopyAttribute(const FPCGMetadataAttributeBase* OriginalAttribute, FName NewAttributeName, bool bKeepParent, bool bCopyEntries, bool bCopyValues);

	/** Returns this metadata's parent */
	TWeakObjectPtr<const UPCGMetadata> GetParentPtr() const { return Parent; }
	const UPCGMetadata* GetParent() const { return Parent.Get(); }
	const UPCGMetadata* GetRoot() const;
	bool HasParent(const UPCGMetadata* InTentativeParent) const;

	/** Unparents current metadata by flattening the attributes (values, entries, etc.) and potentially compress the data to remove unused values. */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata|Advanced")
	void Flatten();

	/** Unparents current metadata by flattening the attributes (values, entries, etc.) */
	void FlattenImpl();

	/** Unparents current metadata, flatten attribute and only keep the entries specified. Return true if something has changed and keys needs be updated. */
	bool FlattenAndCompress(const TArray<PCGMetadataEntryKey>& InEntryKeysToKeep);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateInteger32Attribute(FName AttributeName, int32 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateInteger64Attribute(FName AttributeName, int64 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateFloatAttribute(FName AttributeName, float DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateDoubleAttribute(FName AttributeName, double DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateVectorAttribute(FName AttributeName, FVector DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata", meta=(DisplayName = "Create Vector4 Attribute"))
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateVector4Attribute(FName AttributeName, FVector4 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateVector2Attribute(FName AttributeName, FVector2D DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateRotatorAttribute(FName AttributeName, FRotator DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateQuatAttribute(FName AttributeName, FQuat DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateTransformAttribute(FName AttributeName, FTransform DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateStringAttribute(FName AttributeName, FString DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateNameAttribute(FName AttributeName, FName DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateBoolAttribute(FName AttributeName, bool DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateSoftObjectPathAttribute(FName AttributeName, const FSoftObjectPath& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	UPARAM(DisplayName = "Metadata") UPCGMetadata* CreateSoftClassPathAttribute(FName AttributeName, const FSoftClassPath& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	/** Creates an attribute given a property.
	* @param AttributeName: Target attribute to create
	* @param Object: Object to get the property value from
	* @param Property: The property to set from
	* @returns true if the attribute creation succeeded
	*/
	bool CreateAttributeFromProperty(FName AttributeName, const UObject* Object, const FProperty* Property);

	/** Creates an attribute given a property.
	* @param AttributeName: Target attribute to create
	* @param Data: Data pointer to get the property value from
	* @param Property: The property to set from
	* @returns true if the attribute creation succeeded
	*/
	bool CreateAttributeFromDataProperty(FName AttributeName, const void* Data, const FProperty* Property);

	/** Set an attribute given a property and its value.
	* @param AttributeName: Target attribute to set the property's value to
	* @param EntryKey: Metadata entry key to set the value to
	* @param Object: Object to get the property value from
	* @param Property: The property to set from
	* @param bCreate: If true and the attribute doesn't exists, it will create an attribute based on the property type
	* @returns true if the attribute creation (if required) and the value set succeeded
	*/
	bool SetAttributeFromProperty(FName AttributeName, PCGMetadataEntryKey& EntryKey, const UObject* Object, const FProperty* Property, bool bCreate);

	/** Set an attribute given a property and its value.
	* @param AttributeName: Target attribute to set the property's value to
	* @param EntryKey: Metadata entry key to set the value to
	* @param Data: Data pointer to get the property value from
	* @param Property: The property to set from
	* @param bCreate: If true and the attribute doesn't exists, it will create an attribute based on the property type
	* @returns true if the attribute creation (if required) and the value set succeeded
	*/
	bool SetAttributeFromDataProperty(FName AttributeName, PCGMetadataEntryKey& EntryKey, const void* Data, const FProperty* Property, bool bCreate);

	/** Get attributes */
	FPCGMetadataAttributeBase* GetMutableAttribute(FName AttributeName);
	const FPCGMetadataAttributeBase* GetConstAttribute(FName AttributeName) const;
	const FPCGMetadataAttributeBase* GetConstAttributeById(int32 AttributeId) const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	bool HasAttribute(FName AttributeName) const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	bool HasCommonAttributes(const UPCGMetadata* InMetadata) const;

	/** Return the number of attributes in this metadata. */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	int32 GetAttributeCount() const;

	template <typename T>
	FPCGMetadataAttribute<T>* GetMutableTypedAttribute(FName AttributeName);

	template <typename T>
	const FPCGMetadataAttribute<T>* GetConstTypedAttribute(FName AttributeName) const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void GetAttributes(TArray<FName>& AttributeNames, TArray<EPCGMetadataTypes>& AttributeTypes) const;

	/** Returns name of the most recently created attribute, or none if no attributes are present. */
	FName GetLatestAttributeNameOrNone() const;

	/** Delete/Hide attribute */
	// Due to stream inheriting, we might want to consider "hiding" parent stream and deleting local streams only
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void DeleteAttribute(FName AttributeName);

	/** Copy attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	bool CopyExistingAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent = true);

	/** Rename attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	bool RenameAttribute(FName AttributeToRename, FName NewAttributeName);

	/** Clear/Reinit attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void ClearAttribute(FName AttributeToClear);

	/** Change type of an attribute */
	bool ChangeAttributeType(FName AttributeName, int16 AttributeNewType);

	/** Adds a unique entry key to the metadata */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	int64 AddEntry(int64 ParentEntryKey = -1);

	/** Advanced method.
	*   In a MT context, we might not want to add the entry directly (because of write lock). Call this to generate an unique index in the MT context
	*   And call AddDelayedEntries at the end when you want to add all the entries.
	*   Make sure to not call AddEntry in a different thread or even in the same thread if you use this function, or it will change all the indexes.
	*/
	int64 AddEntryPlaceholder();

	/** Advanced method.
	*   If you used AddEntryPlaceholder, call this function at the end of your MT processing to add all the entries in one shot.
	*   Make sure to call this one at the end!!
	*   @param AllEntries Array of pairs. First item is the EntryIndex generated by AddEntryPlaceholder, the second the ParentEntryKey (cf AddEntry)
	*/
	void AddDelayedEntries(const TArray<TTuple<int64, int64>>& AllEntries);

	/** Initializes the metadata entry key. Returns true if key set from either parent */
	bool InitializeOnSet(PCGMetadataEntryKey& InOutKey, PCGMetadataEntryKey InParentKeyA = PCGInvalidEntryKey, const UPCGMetadata* InParentMetadataA = nullptr, PCGMetadataEntryKey InParentKeyB = PCGInvalidEntryKey, const UPCGMetadata* InParentMetadataB = nullptr);

	/** Metadata chaining mechanism */
	PCGMetadataEntryKey GetParentKey(PCGMetadataEntryKey LocalItemKey) const;

	/** Attributes operations */
	void MergeAttributes(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op);
	void MergeAttributesSubset(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubetA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetb, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op);

	void ResetWeightedAttributes(PCGMetadataEntryKey& OutKey);
	void AccumulateWeightedAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, float Weight, bool bSetNonInterpolableAttributes, PCGMetadataEntryKey& OutKey);

	void SetAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, PCGMetadataEntryKey& OutKey);
	void SetAttributes(const TArrayView<const PCGMetadataEntryKey>& InKeys, const UPCGMetadata* InMetadata, const TArrayView<PCGMetadataEntryKey>& OutKeys, FPCGContext* OptionalContext = nullptr);

	/** Attributes operations - shorthand for points */
	void MergePointAttributes(const FPCGPoint& InPointA, const FPCGPoint& InPointB, FPCGPoint& OutPoint, EPCGMetadataOp Op);
	void MergePointAttributesSubset(const FPCGPoint& InPointA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubetA, const FPCGPoint& InPointB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetb, FPCGPoint& OutPoint, EPCGMetadataOp Op);
	void SetPointAttributes(const TArrayView<const FPCGPoint>& InPoints, const UPCGMetadata* InMetadata, const TArrayView<FPCGPoint>& OutPoints, FPCGContext* OptionalContext = nullptr);

	/** Blueprint-friend versions */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void MergeAttributesByKey(int64 KeyA, const UPCGMetadata* MetadataA, int64 KeyB, const UPCGMetadata* MetadataB, int64 TargetKey, EPCGMetadataOp Op, int64& OutKey);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void ResetWeightedAttributesByKey(int64 TargetKey, int64& OutKey);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AccumulateWeightedAttributesByKey(int64 Key, const UPCGMetadata* Metadata, float Weight, bool bSetNonInterpolableAttributes, int64 TargetKey, int64& OutKey);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void SetAttributesByKey(int64 Key, const UPCGMetadata* InMetadata, int64 TargetKey, int64& OutKey);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void MergePointAttributes(const FPCGPoint& PointA, const UPCGMetadata* MetadataA, const FPCGPoint& PointB, const UPCGMetadata* MetadataB, UPARAM(ref) FPCGPoint& TargetPoint, EPCGMetadataOp Op);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void SetPointAttributes(const FPCGPoint& Point, const UPCGMetadata* Metadata, UPARAM(ref) FPCGPoint& OutPoint);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void ResetPointWeightedAttributes(FPCGPoint& OutPoint);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AccumulatePointWeightedAttributes(const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, float Weight, bool bSetNonInterpolableAttributes, UPARAM(ref) FPCGPoint& OutPoint);

	void ComputePointWeightedAttribute(FPCGPoint& OutPoint, const TArrayView<TPair<const FPCGPoint*, float>>& InWeightedPoints, const UPCGMetadata* InMetadata);
	void ComputeWeightedAttribute(PCGMetadataEntryKey& OutKey, const TArrayView<TPair<PCGMetadataEntryKey, float>>& InWeightedKeys, const UPCGMetadata* InMetadata);

	int64 GetItemKeyCountForParent() const;
	int64 GetLocalItemCount() const;

	/** Return the number of entries in metadata including the parent entries. */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata", meta = (DisplayName = "Get Number of Entries"))
	int64 GetItemCountForChild() const;

	/**
	* Create a new attribute. If the attribute already exists, it will raise a warning (use FindOrCreateAttribute if this usecase can arise)
	* If the attribute already exists but is of the wrong type, it will fail and return nullptr. Same if the name is invalid.
	* Return a typed attribute pointer, of the requested type T.
	*/
	template<typename T>
	FPCGMetadataAttribute<T>* CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent);

	/**
	* Find or create an attribute. Follows CreateAttribute signature.
	* Extra boolean bOverwriteIfTypeMismatch allows to overwrite an existing attribute if the type mismatch.
	* Same as CreateAttribute, it will return nullptr if the attribute name is invalid.
	* Return a typed attribute pointer, of the requested type T.
	*/
	template<typename T>
	FPCGMetadataAttribute<T>* FindOrCreateAttribute(FName AttributeName, const T& DefaultValue = T{}, bool bAllowsInterpolation = true, bool bOverrideParent = true, bool bOverwriteIfTypeMismatch = true);

	// Need this gymnastic for those 2 functions, because blueprints doesn't support default arguments for Arrays, and we don't want to force c++ user to provide OptionalNewEntriesOrder.

	/** Initializes the metadata from a parent metadata by copying all attributes to it.
	* @param InMetadataToCopy Metadata to copy from
	* @param InOptionalEntriesToCopy Optional array that contains the keys to copy over. This array order will be respected, so it can also be used to re-order entries. If empty, copy them all.
	*/
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata", meta = (DisplayName = "Initialize As Copy", AutoCreateRefTerm = "InOptionalEntriesToCopy"))
	void K2_InitializeAsCopy(const UPCGMetadata* InMetadataToCopy, const TArray<int64>& InOptionalEntriesToCopy) { return InitializeAsCopy(InMetadataToCopy, !InOptionalEntriesToCopy.IsEmpty() ? &InOptionalEntriesToCopy : nullptr); }

	/** Initializes the metadata from a parent metadata by copy filtered attributes only to it
	* @param InMetadataToCopy Metadata to copy from
	* @param InFilteredAttributes Attributes to keep/exclude, can be empty.
	* @param InOptionalEntriesToCopy Optional array that contains the keys to copy over. This array order will be respected, so it can also be used to re-order entries. If empty, copy them all.
	* @param InFilterMode Filter to know if we should keep or exclude InFilteredAttributes.
	*/
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata", meta = (DisplayName = "Initialize As Copy With Attribute Filter", AutoCreateRefTerm = "InFilteredAttributes,InOptionalEntriesToCopy"))
	void K2_InitializeAsCopyWithAttributeFilter(const UPCGMetadata* InMetadataToCopy, const TSet<FName>& InFilteredAttributes, const TArray<int64>& InOptionalEntriesToCopy, EPCGMetadataFilterMode InFilterMode = EPCGMetadataFilterMode::ExcludeAttributes)
	{
		return InitializeAsCopyWithAttributeFilter(InMetadataToCopy, InFilteredAttributes, InFilterMode, !InOptionalEntriesToCopy.IsEmpty() ? &InOptionalEntriesToCopy : nullptr);
	}

protected:
	FPCGMetadataAttributeBase* CopyAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent, bool bCopyEntries, bool bCopyValues);

	bool ParentHasAttribute(FName AttributeName) const;

	void AddAttributeInternal(FName AttributeName, FPCGMetadataAttributeBase* Attribute);
	void RemoveAttributeInternal(FName AttributeName);

	void SetLastCachedSelectorOnOwner(FName AttributeName);

	UPROPERTY()
	TObjectPtr<const UPCGMetadata> Parent;

	// Set of parents kept for streams relationship and GC collection
	// But otherwise not used directly
	UPROPERTY()
	TSet<TWeakObjectPtr<const UPCGMetadata>> OtherParents;

	TMap<FName, FPCGMetadataAttributeBase*> Attributes;
	PCGMetadataAttributeKey NextAttributeId = 0;

	TArray<PCGMetadataEntryKey> ParentKeys;
	int64 ItemKeyOffset = 0;

	mutable FRWLock AttributeLock;
	mutable FRWLock ItemLock;

	TAtomic<int64> DelayedEntriesIndex = 0;
};

template<typename T>
FPCGMetadataAttribute<T>* UPCGMetadata::CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	if (!FPCGMetadataAttributeBase::IsValidName(AttributeName))
	{
		UE_LOG(LogPCG, Error, TEXT("Attribute name '%s' is invalid"), *AttributeName.ToString());
		return nullptr;
	}

	const FPCGMetadataAttributeBase* ParentAttribute = nullptr;

	if (bOverrideParent && Parent)
	{
		ParentAttribute = Parent->GetConstAttribute(AttributeName);
	}

	if (ParentAttribute && (ParentAttribute->GetTypeId() != PCG::Private::MetadataTypes<T>::Id))
	{
		// Can't parent if the types doesn't match
		ParentAttribute = nullptr;
	}

	FPCGMetadataAttribute<T>* NewAttribute = new FPCGMetadataAttribute<T>(this, AttributeName, ParentAttribute, DefaultValue, bAllowsInterpolation);

	{
		FWriteScopeLock WriteLock(AttributeLock);

		if (FPCGMetadataAttributeBase** ExistingAttribute = Attributes.Find(AttributeName))
		{
			delete NewAttribute;
			if ((*ExistingAttribute)->GetTypeId() != PCG::Private::MetadataTypes<T>::Id)
			{
				UE_LOG(LogPCG, Error, TEXT("Attribute %s already exists but is not the right type. Abort."), *AttributeName.ToString());
				return nullptr;
			}
			else
			{
				UE_LOG(LogPCG, Warning, TEXT("Attribute %s already exists"), *AttributeName.ToString());
				NewAttribute = static_cast<FPCGMetadataAttribute<T>*>(*ExistingAttribute);
			}
		}
		else
		{
			NewAttribute->AttributeId = NextAttributeId++;
			AddAttributeInternal(AttributeName, NewAttribute);

			// Also when creating an attribute, notify the PCG Data owner that the latest attribute manipulated is this one.
			SetLastCachedSelectorOnOwner(AttributeName);
		}
	}

	return NewAttribute;
}

template<typename T>
FPCGMetadataAttribute<T>* UPCGMetadata::FindOrCreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent, bool bOverwriteIfTypeMismatch)
{
	FPCGMetadataAttribute<T>* Attribute = GetMutableTypedAttribute<T>(AttributeName);

	// If Attribute is null, but we have an attribute with this name, we have a type mismatch.
	// Will be overwrite if flag bOverwriteIfTypeMismatch is at true.
	if (!Attribute && HasAttribute(AttributeName) && bOverwriteIfTypeMismatch)
	{
		DeleteAttribute(AttributeName);
	}

	if (!Attribute)
	{
		Attribute = CreateAttribute<T>(AttributeName, DefaultValue, bAllowsInterpolation, bOverrideParent);
	}

	return Attribute;
}

template <typename T>
FPCGMetadataAttribute<T>* UPCGMetadata::GetMutableTypedAttribute(FName AttributeName)
{
	FPCGMetadataAttributeBase* BaseAttribute = GetMutableAttribute(AttributeName);
	return (BaseAttribute && (BaseAttribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id))
		? static_cast<FPCGMetadataAttribute<T>*>(BaseAttribute)
		: nullptr;

}

template <typename T>
const FPCGMetadataAttribute<T>* UPCGMetadata::GetConstTypedAttribute(FName AttributeName) const
{
	const FPCGMetadataAttributeBase* BaseAttribute = GetConstAttribute(AttributeName);
	return (BaseAttribute && (BaseAttribute->GetTypeId() == PCG::Private::MetadataTypes<T>::Id))
		? static_cast<const FPCGMetadataAttribute<T>*>(BaseAttribute)
		: nullptr;
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGMetadataAccessor.h"
#endif
