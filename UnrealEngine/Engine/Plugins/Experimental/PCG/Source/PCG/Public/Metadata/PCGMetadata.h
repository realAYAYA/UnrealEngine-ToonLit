// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGModule.h"
#include "PCGMetadataAccessor.h"
#include "PCGMetadataAttributeTraits.h"
#include "PCGMetadataCommon.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "PCGMetadata.generated.h"


UCLASS(BlueprintType)
class PCG_API UPCGMetadata : public UObject
{
	GENERATED_BODY()

public:

	//~ Begin UObject interface
	virtual void Serialize(FArchive& InArchive) override;
	virtual void BeginDestroy() override;
	//~ End UObject interface

	/** Initializes the metadata from a parent metadata, if any (can be null) */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void Initialize(const UPCGMetadata* InParent);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void InitializeAsCopy(const UPCGMetadata* InMetadataToCopy);

	/** Creates missing attributes from another metadata if they are not currently present - note that this does not copy values */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void AddAttributes(const UPCGMetadata* InOther);

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
	const UPCGMetadata* GetParent() const { return Parent.Get(); }
	const UPCGMetadata* GetRoot() const;
	bool HasParent(const UPCGMetadata* InTentativeParent) const;

	/** Create new streams */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateInteger64Attribute(FName AttributeName, int64 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateFloatAttribute(FName AttributeName, float DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateDoubleAttribute(FName AttributeName, double DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateVectorAttribute(FName AttributeName, FVector DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata", meta=(DisplayName = "Create Vector4 Attribute"))
	void CreateVector4Attribute(FName AttributeName, FVector4 DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateVector2Attribute(FName AttributeName, FVector2D DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateRotatorAttribute(FName AttributeName, FRotator DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateQuatAttribute(FName AttributeName, FQuat DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateTransformAttribute(FName AttributeName, FTransform DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CreateStringAttribute(FName AttributeName, FString DefaultValue, bool bAllowsInterpolation, bool bOverrideParent = true);

	/** Set an attribute given a property and its value.
	* @param AttributeName: Target attribute to set the property's value to
	* @param EntryKey: Metadata entry key to set the value to
	* @param Object: Object to get the property value from
	* @param Property: The property to set from
	* @param bCreate: If true and the attribute doesn't exists, it will create an attribute based on the property type
	* @returns true if the attribute creation (if required) and the value set succeeded
	*/
	bool SetAttributeFromProperty(FName AttributeName, PCGMetadataEntryKey& EntryKey, const UObject* Object, const FProperty* Property, bool bCreate);

	/** Get attributes */
	FPCGMetadataAttributeBase* GetMutableAttribute(FName AttributeName);
	const FPCGMetadataAttributeBase* GetConstAttribute(FName AttributeName) const;
	const FPCGMetadataAttributeBase* GetConstAttributeById(int32 AttributeId) const;
	bool HasAttribute(FName AttributeName) const;
	int32 GetAttributeCount() const;

	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void GetAttributes(TArray<FName>& AttributeNames, TArray<EPCGMetadataTypes>& AttributeTypes) const;

	FName GetLatestAttributeNameOrNone() const;

	/** Delete/Hide attribute */
	// Due to stream inheriting, we might want to consider "hiding" parent stream and deleting local streams only
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void DeleteAttribute(FName AttributeName);

	/** Copy attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void CopyExistingAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent = true);

	/** Rename attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void RenameAttribute(FName AttributeToRename, FName NewAttributeName);

	/** Clear/Reinit attribute */
	UFUNCTION(BlueprintCallable, Category = "PCG|Metadata")
	void ClearAttribute(FName AttributeToClear);

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
	bool InitializeOnSet(PCGMetadataEntryKey& InKey, PCGMetadataEntryKey InParentKeyA = PCGInvalidEntryKey, const UPCGMetadata* InParentMetadataA = nullptr, PCGMetadataEntryKey InParentKeyB = PCGInvalidEntryKey, const UPCGMetadata* InParentMetadataB = nullptr);

	/** Metadata chaining mechanism */
	PCGMetadataEntryKey GetParentKey(PCGMetadataEntryKey LocalItemKey) const;

	/** Attributes operations */
	void MergeAttributes(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op);
	void MergeAttributesSubset(PCGMetadataEntryKey InKeyA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubetA, PCGMetadataEntryKey InKeyB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetb, PCGMetadataEntryKey& OutKey, EPCGMetadataOp Op);

	void ResetWeightedAttributes(PCGMetadataEntryKey& OutKey);
	void AccumulateWeightedAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, float Weight, bool bSetNonInterpolableAttributes, PCGMetadataEntryKey& OutKey);

	void SetAttributes(PCGMetadataEntryKey InKey, const UPCGMetadata* InMetadata, PCGMetadataEntryKey& OutKey);
	void SetAttributes(const TArrayView<PCGMetadataEntryKey>& InKeys, const UPCGMetadata* InMetadata, const TArrayView<PCGMetadataEntryKey>& OutKeys);

	/** Attributes operations - shorthand for points */
	void MergePointAttributes(const FPCGPoint& InPointA, const FPCGPoint& InPointB, FPCGPoint& OutPoint, EPCGMetadataOp Op);
	void MergePointAttributesSubset(const FPCGPoint& InPointA, const UPCGMetadata* InMetadataA, const UPCGMetadata* InMetadataSubetA, const FPCGPoint& InPointB, const UPCGMetadata* InMetadataB, const UPCGMetadata* InMetadataSubsetb, FPCGPoint& OutPoint, EPCGMetadataOp Op);
	void SetPointAttributes(const TArrayView<const FPCGPoint>& InPoints, const UPCGMetadata* InMetadata, const TArrayView<FPCGPoint>& OutPoints);

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
	int64 GetItemCountForChild() const;

	template<typename T>
	FPCGMetadataAttributeBase* CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent);

protected:
	FPCGMetadataAttributeBase* CopyAttribute(FName AttributeToCopy, FName NewAttributeName, bool bKeepParent, bool bCopyEntries, bool bCopyValues);

	bool ParentHasAttribute(FName AttributeName) const;

	void AddAttributeInternal(FName AttributeName, FPCGMetadataAttributeBase* Attribute);
	void RemoveAttributeInternal(FName AttributeName);

	UPROPERTY()
	TWeakObjectPtr<const UPCGMetadata> Parent;

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
FPCGMetadataAttributeBase* UPCGMetadata::CreateAttribute(FName AttributeName, const T& DefaultValue, bool bAllowsInterpolation, bool bOverrideParent)
{
	const FPCGMetadataAttributeBase* ParentAttribute = nullptr;

	if (bOverrideParent && Parent.IsValid())
	{
		ParentAttribute = Parent->GetConstAttribute(AttributeName);
	}

	FPCGMetadataAttributeBase* NewAttribute = new FPCGMetadataAttribute<T>(this, AttributeName, ParentAttribute, DefaultValue, bAllowsInterpolation);

	AttributeLock.WriteLock();
	if (FPCGMetadataAttributeBase** ExistingAttribute = Attributes.Find(AttributeName))
	{
		UE_LOG(LogPCG, Warning, TEXT("Attribute %s already exists"), *AttributeName.ToString());
		delete NewAttribute;
		NewAttribute = *ExistingAttribute;
	}
	else
	{
		NewAttribute->AttributeId = NextAttributeId++;
		AddAttributeInternal(AttributeName, NewAttribute);
	}
	AttributeLock.WriteUnlock();

	return NewAttribute;
}