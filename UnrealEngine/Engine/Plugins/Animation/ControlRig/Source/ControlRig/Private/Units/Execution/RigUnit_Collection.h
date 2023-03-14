// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "ControlRigDefines.h" 
#include "RigUnit_Collection.generated.h"

USTRUCT(meta = (Abstract, NodeColor = "0.4627450108528137 1.0 0.3294120132923126", Category = "Items"))
struct CONTROLRIG_API FRigUnit_CollectionBase : public FRigUnit
{
	GENERATED_BODY()
};

USTRUCT(meta = (Abstract, NodeColor = "0.4627450108528137 1.0 0.3294120132923126", Category = "Items"))
struct CONTROLRIG_API FRigUnit_CollectionBaseMutable : public FRigUnitMutable
{
	GENERATED_BODY()
};

/**
 * Creates a collection based on a first and last item within a chain.
 * Chains can refer to bone chains or chains within a control hierarchy.
 */
USTRUCT(meta=(DisplayName="Item Chain", Keywords="Bone,Joint,Collection", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionChain : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionChain()
	{
		FirstItem = LastItem = FRigElementKey(NAME_None, ERigElementType::Bone);
		Reverse = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey FirstItem;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey LastItem;

	UPROPERTY(meta = (Input))
	bool Reverse;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Creates an item array based on a first and last item within a chain.
* Chains can refer to bone chains or chains within a control hierarchy.
*/
USTRUCT(meta=(DisplayName="Item Chain", Keywords="Bone,Joint,Collection", Varying))
struct CONTROLRIG_API FRigUnit_CollectionChainArray : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionChainArray()
	{
		FirstItem = LastItem = FRigElementKey(NAME_None, ERigElementType::Bone);
		Reverse = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey FirstItem;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey LastItem;

	UPROPERTY(meta = (Input))
	bool Reverse;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
 * Creates a collection based on a name search.
 * The name search is case sensitive.
 */
USTRUCT(meta = (DisplayName = "Item Name Search", Keywords = "Bone,Joint,Collection,Filter", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionNameSearch : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionNameSearch()
	{
		PartialName = NAME_None;
		TypeToSearch = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FName PartialName;

	UPROPERTY(meta = (Input))
	ERigElementType TypeToSearch;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Creates an item array based on a name search.
* The name search is case sensitive.
*/
USTRUCT(meta = (DisplayName = "Item Name Search", Keywords = "Bone,Joint,Collection,Filter", Varying))
struct CONTROLRIG_API FRigUnit_CollectionNameSearchArray : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionNameSearchArray()
	{
		PartialName = NAME_None;
		TypeToSearch = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FName PartialName;

	UPROPERTY(meta = (Input))
	ERigElementType TypeToSearch;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
 * Creates a collection based on the direct or recursive children
 * of a provided parent item. Returns an empty collection for an invalid parent item.
 */
USTRUCT(meta = (DisplayName = "Get Children", Keywords = "Bone,Joint,Collection,Filter,Parent", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionChildren : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionChildren()
	{
		Parent = FRigElementKey(NAME_None, ERigElementType::Bone);
		bIncludeParent = false;
		bRecursive = false;
		TypeToSearch = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;

	UPROPERTY(meta = (Input))
	bool bIncludeParent;

	UPROPERTY(meta = (Input))
	bool bRecursive;

	UPROPERTY(meta = (Input))
	ERigElementType TypeToSearch;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Creates an item array based on the direct or recursive children
* of a provided parent item. Returns an empty array for an invalid parent item.
*/
USTRUCT(meta = (DisplayName = "Get Children", Category = "Hierarchy", Keywords = "Bone,Joint,Collection,Filter,Parent", Varying))
struct CONTROLRIG_API FRigUnit_CollectionChildrenArray : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionChildrenArray()
	{
		Parent = FRigElementKey(NAME_None, ERigElementType::Bone);
		bIncludeParent = false;
		bRecursive = false;
		TypeToSearch = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Parent;

	UPROPERTY(meta = (Input))
	bool bIncludeParent;

	UPROPERTY(meta = (Input))
	bool bRecursive;

	UPROPERTY(meta = (Input))
	ERigElementType TypeToSearch;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
* Creates an item array for all elements given the filter.
*/
USTRUCT(meta = (DisplayName = "Get All", Category = "Hierarchy", Keywords = "Bone,Joint,Collection,Filter,Parent", Varying))
struct CONTROLRIG_API FRigUnit_CollectionGetAll : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionGetAll()
	{
		TypeToSearch = ERigElementType::All;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	ERigElementType TypeToSearch;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
 * Replaces all names within the collection
 */
USTRUCT(meta = (DisplayName = "Replace Items", Keywords = "Replace,Find,Collection", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionReplaceItems : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionReplaceItems()
	{
		Old = New = NAME_None;
		RemoveInvalidItems = false;
		bAllowDuplicates = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Items;

	UPROPERTY(meta = (Input))
	FName Old;

	UPROPERTY(meta = (Input))
	FName New;

	UPROPERTY(meta = (Input))
	bool RemoveInvalidItems;

	UPROPERTY(meta = (Input))
	bool bAllowDuplicates;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Replaces all names within the item array
*/
USTRUCT(meta = (DisplayName = "Replace Items", Keywords = "Replace,Find,Collection", Varying))
struct CONTROLRIG_API FRigUnit_CollectionReplaceItemsArray : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionReplaceItemsArray()
	{
		Old = New = NAME_None;
		RemoveInvalidItems = false;
		bAllowDuplicates = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	UPROPERTY(meta = (Input))
	FName Old;

	UPROPERTY(meta = (Input))
	FName New;

	UPROPERTY(meta = (Input))
	bool RemoveInvalidItems;

	UPROPERTY(meta = (Input))
	bool bAllowDuplicates;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Result;
};

/**
 * Returns a collection provided a specific array of items.
 */
USTRUCT(meta = (DisplayName = "Collection from Items", Category = "Items|Collections", Keywords = "Collection,Array", Varying))
struct CONTROLRIG_API FRigUnit_CollectionItems : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionItems()
	{
		Items.Add(FRigElementKey(NAME_None, ERigElementType::Bone));
		bAllowDuplicates = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, ExpandByDefault))
	TArray<FRigElementKey> Items;

	UPROPERTY(meta = (Input))
	bool bAllowDuplicates;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;
};

/**
* Returns an array of items provided a collection
*/
USTRUCT(meta = (DisplayName = "Get Items from Collection", Category = "Items|Collections", Keywords = "Collection,Array", Varying))
struct CONTROLRIG_API FRigUnit_CollectionGetItems : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionGetItems()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Output))
	TArray<FRigElementKey> Items;
};

/**
 * Returns an array of relative parent indices for each item. Several options here
 * a) If an item has multiple parents the major parent (based on the weights) will be returned.
 * b) If an item has a parent that's not part of the collection INDEX_NONE will be returned.
 * c) If an item has a parent that's not part of the collection, but a grand parent is we'll use that index instead.
 */
USTRUCT(meta = (DisplayName = "Get Parent Indices", Keywords = "Collection,Array", Varying, Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionGetParentIndices : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionGetParentIndices()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Output))
	TArray<int32> ParentIndices;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns an array of relative parent indices for each item. Several options here
* a) If an item has multiple parents the major parent (based on the weights) will be returned.
* b) If an item has a parent that's not part of the collection INDEX_NONE will be returned.
* c) If an item has a parent that's not part of the collection, but a grand parent is we'll use that index instead.
*/
USTRUCT(meta = (DisplayName = "Get Parent Indices", Keywords = "Collection,Array", Varying))
struct CONTROLRIG_API FRigUnit_CollectionGetParentIndicesItemArray : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionGetParentIndicesItemArray()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	UPROPERTY(meta = (Output))
	TArray<int32> ParentIndices;
};

/**
 * Returns the union of two provided collections
 * (the combination of all items from both A and B).
 */
USTRUCT(meta = (DisplayName = "Union", Keywords = "Combine,Add,Merge,Collection,Hierarchy", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionUnion : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionUnion()
	{
		bAllowDuplicates = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection A;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection B;

	UPROPERTY(meta = (Input))
	bool bAllowDuplicates;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the intersection of two provided collections
 * (the items present in both A and B).
 */
USTRUCT(meta = (DisplayName = "Intersection", Keywords = "Combine,Merge,Collection,Hierarchy", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionIntersection : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionIntersection()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection A;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection B;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the difference between two collections
 * (the items present in A but not in B).
 */
USTRUCT(meta = (DisplayName = "Difference", Keywords = "Collection,Exclude,Subtract", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionDifference : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionDifference()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection A;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection B;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Collection;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the collection in reverse order
 */
USTRUCT(meta = (DisplayName = "Reverse", Keywords = "Direction,Order,Reverse", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionReverse : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionReverse()
	{
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Reversed;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the number of elements in a collection
 */
USTRUCT(meta = (DisplayName = "Count", Keywords = "Collection,Array,Count,Num,Length,Size", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionCount : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionCount()
	{
		Collection = FRigElementKeyCollection();
		Count = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Output))
	int32 Count;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns a single item within a collection by index
 */
USTRUCT(meta = (DisplayName = "Item At Index", Keywords = "Item,GetIndex,AtIndex,At,ForIndex,[]", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionItemAtIndex : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionItemAtIndex()
	{
		Collection = FRigElementKeyCollection();
		Index = 0;
		Item = FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Input))
	int32 Index;

	UPROPERTY(meta = (Output))
	FRigElementKey Item;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Given a collection of items, execute iteratively across all items in a given collection
 */
USTRUCT(meta=(DisplayName="For Each Item", Keywords="Collection,Loop,Iterate", Icon="EditorStyle|GraphEditor.Macro.ForEach_16x", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionLoop : public FRigUnit_CollectionBaseMutable
{
	GENERATED_BODY()

	FRigUnit_CollectionLoop()
	{
		Count = 0;
		Index = 0;
		Ratio = 0.f;
		Continue = false;
	}

	// FRigVMStruct overrides
	FORCEINLINE virtual bool IsForLoop() const override { return true; }
	FORCEINLINE virtual int32 GetNumSlices() const override { return Count; }

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Output))
	FRigElementKey Item;

	UPROPERTY(meta = (Singleton, Output))
	int32 Index;

	UPROPERTY(meta = (Singleton, Output))
	int32 Count;

	/**
	 * Ranging from 0.0 (first item) and 1.0 (last item)
	 * This is useful to drive a consecutive node with a 
	 * curve or an ease to distribute a value.
	 */
	UPROPERTY(meta = (Singleton, Output))
	float Ratio;

	UPROPERTY(meta = (Singleton))
	bool Continue;

	UPROPERTY(meta = (Output))
	FControlRigExecuteContext Completed;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Adds an element to an existing collection
*/
USTRUCT(meta = (DisplayName = "Add Item", Keywords = "Item,Add,Push,Insert", Deprecated = "5.0"))
struct CONTROLRIG_API FRigUnit_CollectionAddItem : public FRigUnit_CollectionBase
{
	GENERATED_BODY()

	FRigUnit_CollectionAddItem()
	{
		Collection = Result = FRigElementKeyCollection();
		Item = FRigElementKey();
	}

	RIGVM_METHOD()
    virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FRigElementKeyCollection Collection;

	UPROPERTY(meta = (Input))
	FRigElementKey Item;

	UPROPERTY(meta = (Output))
	FRigElementKeyCollection Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};