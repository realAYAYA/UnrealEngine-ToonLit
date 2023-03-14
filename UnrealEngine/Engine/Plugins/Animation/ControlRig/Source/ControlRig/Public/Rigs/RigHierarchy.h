// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVM.h"
#include "RigHierarchyElements.h"
#include "RigHierarchyPose.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "EdGraph/EdGraphPin.h"
#include "RigHierarchyDefines.h"
#if WITH_EDITOR
#include "RigVMPythonUtils.h"
#endif
#include "Containers/Queue.h"
#include "RigHierarchy.generated.h"

class URigHierarchy;
class URigHierarchyController;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FRigHierarchyModifiedEvent, ERigHierarchyNotification /* type */, URigHierarchy* /* hierarchy */, const FRigBaseElement* /* element */);
DECLARE_EVENT_FiveParams(URigHierarchy, FRigHierarchyUndoRedoTransformEvent, URigHierarchy*, const FRigElementKey&, ERigTransformType::Type, const FTransform&, bool /* bUndo */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FRigHierarchyMetadataChangedDelegate, const FRigElementKey& /* Key */, const FName& /* Name */);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FRigHierarchyMetadataTagChangedDelegate, const FRigElementKey& /* Key */, const FName& /* Tag */, bool /* AddedOrRemoved */);

UENUM()
enum ERigTransformStackEntryType
{
	TransformPose,
	ControlOffset,
	ControlShape,
	CurveValue
};

USTRUCT()
struct FRigTransformStackEntry
{
	GENERATED_BODY()

	FORCEINLINE FRigTransformStackEntry()
		: Key()
		, EntryType(ERigTransformStackEntryType::TransformPose)
		, TransformType(ERigTransformType::CurrentLocal)
		, OldTransform(FTransform::Identity)
		, NewTransform(FTransform::Identity)
		, bAffectChildren(true)
		, Callstack() 
	{}

	FORCEINLINE FRigTransformStackEntry(
		const FRigElementKey& InKey,
		ERigTransformStackEntryType InEntryType,
		ERigTransformType::Type InTransformType,
		const FTransform& InOldTransform,
		const FTransform& InNewTransform,
		bool bInAffectChildren,
		const TArray<FString>& InCallstack =  TArray<FString>())
		: Key(InKey)
		, EntryType(InEntryType)
		, TransformType(InTransformType)
		, OldTransform(InOldTransform)
		, NewTransform(InNewTransform)
		, bAffectChildren(bInAffectChildren)
		, Callstack(InCallstack)
	{}

	UPROPERTY()
	FRigElementKey Key;

	UPROPERTY()
	TEnumAsByte<ERigTransformStackEntryType> EntryType;

	UPROPERTY()
	TEnumAsByte<ERigTransformType::Type> TransformType;
	
	UPROPERTY()
	FTransform OldTransform;

	UPROPERTY()
	FTransform NewTransform;

	UPROPERTY()
	bool bAffectChildren;

	UPROPERTY()
	TArray<FString> Callstack;
};

UCLASS(BlueprintType)
class CONTROLRIG_API URigHierarchy : public UObject
{
	GENERATED_BODY()

public:

	typedef TMap<int32, TArray<int32>> TElementDependencyMap;
	typedef TPair<int32, TArray<int32>> TElementDependencyMapPair;
	typedef TTuple<int32, int32, int32, ERigTransformType::Type> TInstructionSliceElement;
	inline static const FName TagMetadataName = TEXT("Tags");

	URigHierarchy();

	// UObject interface
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	void Save(FArchive& Ar);
	void Load(FArchive& Ar);
	virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	/**
	 * Clears the whole hierarchy and removes all elements.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void Reset();

	/**
	 * Resets the hierarchy to the state of its default. This refers to the
	 * hierarchy on the default object.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void ResetToDefault();

	/**
	 * Copies the contents of a hierarchy onto this one
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void CopyHierarchy(URigHierarchy* InHierarchy);

	/**
	 * Returns a hash for the hierarchy representing all names
	 * as well as the topology version.
	 */
	uint32 GetNameHash() const;

	/**
	 * Returns a hash representing the topological state of the hierarchy
	 */
	uint32 GetTopologyHash(bool bIncludeTopologyVersion = true, bool bIncludeTransientControls = false) const;

#if WITH_EDITOR
	/**
	* Add dependent hierarchies that listens to changes made to this hierarchy
	* Note: By default, only changes to the initial states of this hierarchy is mirrored to the listening hierarchies
	*/	
	void RegisterListeningHierarchy(URigHierarchy* InHierarchy);
	
	/**
	* Remove dependent hierarchies that listens to changes made to this hierarchy
	*/	
	void UnregisterListeningHierarchy(URigHierarchy* InHierarchy);
	
	void ClearListeningHierarchy();
#endif

	/**
	 * Returns the default hierarchy for this hierarchy (or nullptr)
	 */
	URigHierarchy* GetDefaultHierarchy() { return DefaultHierarchyPtr.Get(); }

public:
	/**
	 * Copies the contents of a hierarchy onto this one
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void CopyPose(URigHierarchy* InHierarchy, bool bCurrent, bool bInitial, bool bWeights, bool bMatchPoseInGlobalIfNeeded = false);

	/**
	 * Update all elements that depend on external references
	 */
	void UpdateReferences(const FRigUnitContext* InContext);

	/**
	 * Resets the current pose of a filtered lost if elements to the initial / ref pose.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void ResetPoseToInitial(ERigElementType InTypeFilter);

	/**
	 * Resets the current pose of all elements to the initial / ref pose.
	 */
	void ResetPoseToInitial()
	{
		ResetPoseToInitial(ERigElementType::All);
	}

	/**
	 * Resets all curves to 0.0
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    void ResetCurveValues();

	/**
	 * Returns the number of elements in the Hierarchy.
	 * @return The number of elements in the Hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE int32 Num() const
	{
		return Elements.Num();
	}

	/**
	 * Returns the number of elements in the Hierarchy.
	 * @param InElementType The type filter to apply
	 * @return The number of elements in the Hierarchy
	 */
    int32 Num(ERigElementType InElementType) const;

	// iterators
	FORCEINLINE TArray<FRigBaseElement*>::RangedForIteratorType      begin() { return Elements.begin(); }
	FORCEINLINE TArray<FRigBaseElement*>::RangedForIteratorType      end() { return Elements.end(); }

	/**
	 * Iterator function to invoke a lambda / TFunction for each element
	 * @param PerElementFunction The function to invoke for each element
	 */
	FORCEINLINE_DEBUGGABLE void ForEach(TFunction<bool(FRigBaseElement*)> PerElementFunction) const
	{
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			if(!PerElementFunction(Elements[ElementIndex]))
			{
				return;
			}
		}
	}

	/**
	 * Filtered template Iterator function to invoke a lambda / TFunction for each element of a given type.
	 * @param PerElementFunction The function to invoke for each element of a given type
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void ForEach(TFunction<bool(T*)> PerElementFunction) const
	{
		for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
		{
			if(T* CastElement = Cast<T>(Elements[ElementIndex]))
			{
				if(!PerElementFunction(CastElement))
				{
					return;
				}
			}
		}
	}

	/**
	 * Returns true if the provided element index is valid
	 * @param InElementIndex The index to validate
	 * @return Returns true if the provided element index is valid
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool IsValidIndex(int32 InElementIndex) const
	{
		return Elements.IsValidIndex(InElementIndex);
	}

	/**
	 * Returns true if the provided element key is valid
	 * @param InKey The key to validate
	 * @return Returns true if the provided element key is valid
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Contains", ScriptName = "Contains"))
	FORCEINLINE bool Contains_ForBlueprint(FRigElementKey InKey) const
	{
		return Contains(InKey);
	}

	/**
	 * Returns true if the provided element key is valid
	 * @param InKey The key to validate
	 * @return Returns true if the provided element key is valid
	 */
	FORCEINLINE bool Contains(const FRigElementKey& InKey) const
	{
		return GetIndex(InKey) != INDEX_NONE;
	}

	/**
	 * Returns true if the provided element key is valid as a certain typename
	 * @param InKey The key to validate
	 * @return Returns true if the provided element key is valid
	 */
	template<typename T>
	FORCEINLINE bool Contains(const FRigElementKey& InKey) const
	{
		return Find<T>(InKey) != nullptr;
	}

	/**
	 * Returns true if the provided element is procedural.
	 * @param InKey The key to validate
	 * @return Returns true if the element is procedural
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool IsProcedural(const FRigElementKey& InKey) const;

	/**
	 * Returns true if the provided element is procedural.
	 * @param InElement The element to check
	 * @return Returns true if the element is procedural
	 */
	bool IsProcedural(const FRigBaseElement* InElement) const;

	/**
	 * Returns the index of an element given its key
	 * @param InKey The key of the element to retrieve the index for
	 * @return The index of the element or INDEX_NONE
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get Index", ScriptName = "GetIndex"))
	FORCEINLINE int32 GetIndex_ForBlueprint(FRigElementKey InKey) const
	{
		return GetIndex(InKey);
	}

	/**
	 * Returns the index of an element given its key
	 * @param InKey The key of the element to retrieve the index for
	 * @return The index of the element or INDEX_NONE
	 */
	FORCEINLINE int32 GetIndex(const FRigElementKey& InKey) const
	{
		if(const int32* Index = IndexLookup.Find(InKey))
		{
			return *Index;
		}
		return INDEX_NONE;
	}

	/**
	 * Returns the indices of an array of keys
	 * @param InKeys The keys of the elements to retrieve the indices for
	 * @return The indices of the elements or INDEX_NONE
	 */
	FORCEINLINE TArray<int32> GetIndices(const TArray<FRigElementKey>& InKeys) const
	{
		TArray<int32> Indices;
		for(const FRigElementKey& Key : InKeys)
		{
			Indices.Add(GetIndex(Key));
		}
		return Indices;
	}

	/**
	 * Returns the key of an element given its index
	 * @param InElementIndex The index of the element to retrieve the key for
	 * @return The key of an element given its index
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE FRigElementKey GetKey(int32 InElementIndex) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			return Elements[InElementIndex]->Key;
		}
		return FRigElementKey();
	}

	/**
	 * Returns the keys of an array of indices
	 * @param InElementIndices The indices to retrieve the keys for
	 * @return The keys of the elements given the indices
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE TArray<FRigElementKey> GetKeys(const TArray<int32> InElementIndices) const
	{
		TArray<FRigElementKey> Keys;
		for(int32 Index : InElementIndices)
		{
			Keys.Add(GetKey(Index));
		}
		return Keys;
	}

	/**
	 * Returns an element at a given index or nullptr.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FORCEINLINE const FRigBaseElement* Get(int32 InIndex) const
	{
		if(Elements.IsValidIndex(InIndex))
		{
			return Elements[InIndex];
		}
		return nullptr;
	}

	/**
	 * Returns an element at a given index or nullptr.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FRigBaseElement* Get(int32 InIndex)
	{
		if(Elements.IsValidIndex(InIndex))
		{
			return Elements[InIndex];
		}
		return nullptr;
	}

	/**
	 * Returns an element at a given index or nullptr.
	 * This templated method also casts to the chosen
	 * element type but does not guarantee a valid result.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE const T* Get(int32 InIndex) const
	{
		return Cast<T>(Get(InIndex));
	}

	/**
	 * Returns an element at a given index or nullptr.
	 * This templated method also casts to the chosen
	 * element type but does not guarantee a valid result.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE T* Get(int32 InIndex)
	{
		return Cast<T>(Get(InIndex));
	}

	/**
	 * Returns an element at a given index.
	 * This templated method also casts to the chosen
	 * element type and checks for a the valid result.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE const T* GetChecked(int32 InIndex) const
	{
		return CastChecked<T>(Get(InIndex));
	}

	/**
	 * Returns an element at a given index.
	 * This templated method also casts to the chosen
	 * element type and checks for a the valid result.
	 * @param InIndex The index of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
    FORCEINLINE T* GetChecked(int32 InIndex)
	{
		return CastChecked<T>(Get(InIndex));
	}

	/**
     * Returns a handle to an existing element
     * @param InKey The key of the handle to retrieve.
     * @return The retrieved handle (may be invalid)
     */
	FORCEINLINE FRigElementHandle GetHandle(const FRigElementKey& InKey) const
	{
		if(Contains(InKey))
		{
			return FRigElementHandle((URigHierarchy*)this, InKey);
		}
		return FRigElementHandle();
	}

	/**
	 * Returns an element for a given key or nullptr.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FORCEINLINE const FRigBaseElement* Find(const FRigElementKey& InKey) const
	{
		return Get(GetIndex(InKey));
	}

	/**
	 * Returns an element for a given key or nullptr.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FORCEINLINE FRigBaseElement* Find(const FRigElementKey& InKey)
	{
		return Get(GetIndex(InKey));
	}

	/**
	 * Returns an element for a given key and raises for invalid results.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FORCEINLINE const FRigBaseElement* FindChecked(const FRigElementKey& InKey) const
	{
		const FRigBaseElement* Element = Get(GetIndex(InKey));
		check(Element);
		return Element;
	}

	/**
	 * Returns an element for a given key and raises for invalid results.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	FORCEINLINE FRigBaseElement* FindChecked(const FRigElementKey& InKey)
	{
		FRigBaseElement* Element = Get(GetIndex(InKey));
		check(Element);
		return Element;
	}

	/**
	 * Returns an element for a given key or nullptr.
	 * This templated method also casts to the chosen
	 * element type but does not guarantee a valid result.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE const T* Find(const FRigElementKey& InKey) const
	{
		return Get<T>(GetIndex(InKey));
	}

	/**
	 * Returns an element for a given key or nullptr.
	 * This templated method also casts to the chosen
	 * element type but does not guarantee a valid result.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE T* Find(const FRigElementKey& InKey)
	{
		return Get<T>(GetIndex(InKey));
	}
	
private:
	/**
	* Returns bone element for a given key, for scripting purpose only, for cpp usage, use Find<FRigBoneElement>()
	* @param InKey The key of the bone element to retrieve. 
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Find Bone", ScriptName = "FindBone"))
    FRigBoneElement FindBone_ForBlueprintOnly(const FRigElementKey& InKey) const
	{
		if (const FRigBoneElement* Bone = Find<FRigBoneElement>(InKey))
		{
			return *Bone;
		}
		return FRigBoneElement();
	}	
	
	/**
	* Returns control element for a given key, for scripting purpose only, for cpp usage, use Find<FRigControlElement>()
	* @param InKey The key of the control element to retrieve. 
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Find Control", ScriptName = "FindControl"))
    FRigControlElement FindControl_ForBlueprintOnly(const FRigElementKey& InKey) const
	{
		if (const FRigControlElement* Control = Find<FRigControlElement>(InKey))
		{
			return *Control;
		}
		return FRigControlElement();
	}	

	/**
	* Returns null element for a given key, for scripting purpose only, for cpp usage, use Find<FRigControlElement>()
	* @param InKey The key of the null element to retrieve. 
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Find Null", ScriptName = "FindNull"))
    FRigNullElement FindNull_ForBlueprintOnly(const FRigElementKey& InKey) const
	{
		if (const FRigNullElement* Null = Find<FRigNullElement>(InKey))
		{
			return *Null;
		}
		return FRigNullElement();
	}
	
public:	
	/**
	 * Returns an element for a given key.
	 * This templated method also casts to the chosen
	 * element type and checks for a the valid result.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
	FORCEINLINE const T* FindChecked(const FRigElementKey& InKey) const
	{
		return GetChecked<T>(GetIndex(InKey));
	}

	/**
	 * Returns an element for a given key.
	 * This templated method also casts to the chosen
	 * element type and checks for a the valid result.
	 * @param InKey The key of the element to retrieve.
	 * @return The retrieved element or nullptr.
	 */
	template<typename T>
    FORCEINLINE T* FindChecked(const FRigElementKey& InKey)
	{
		return GetChecked<T>(GetIndex(InKey));
	}

	/**
	 * Filtered accessor to retrieve all elements of a given type
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	template<typename T>
	FORCEINLINE TArray<T*> GetElementsOfType(bool bTraverse = false) const
	{
		TArray<T*> Results;

		if(bTraverse)
		{
			TArray<bool> ElementVisited;
			ElementVisited.AddZeroed(Elements.Num());

			Traverse([&ElementVisited, &Results](FRigBaseElement* InElement, bool& bContinue)
			{
			    bContinue = !ElementVisited[InElement->GetIndex()];

			    if(bContinue)
			    {
			        if(T* CastElement = Cast<T>(InElement))
			        {
			            Results.Add(CastElement);
			        }
			        ElementVisited[InElement->GetIndex()] = true;
			    }
			});
		}
		else
		{
			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
			{
				if(T* CastElement = Cast<T>(Elements[ElementIndex]))
				{
					Results.Add(CastElement);
				}
			}
		}
		return Results;
	}

	/**
	 * Filtered accessor to retrieve all element keys of a given type
	 * @param bTraverse Returns the element keys in order of a depth first traversal
	 */
	template<typename T>
    FORCEINLINE TArray<FRigElementKey> GetKeysOfType(bool bTraverse = false) const
	{
		TArray<FRigElementKey> Keys;
		TArray<T*> Results = GetElementsOfType<T>(bTraverse);
		for(T* Element : Results)
		{
			Keys.Add(Element->GetKey());
		}
		return Keys;
	}

	/**
	 * Filtered accessor to retrieve all elements of a given type
	 * @param InKeepElementFunction A function to return true if an element is to be keep
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	template<typename T>
    FORCEINLINE TArray<T*> GetFilteredElements(TFunction<bool(T*)> InKeepElementFunction, bool bTraverse = false) const
	{
		TArray<T*> Results;

		if(bTraverse)
		{
			TArray<bool> ElementVisited;
			ElementVisited.AddZeroed(Elements.Num());
		
			Traverse([&ElementVisited, &Results, InKeepElementFunction](FRigBaseElement* InElement, bool& bContinue)
            {
                bContinue = !ElementVisited[InElement->GetIndex()];

                if(bContinue)
                {
                    if(T* CastElement = Cast<T>(InElement))
                    {
						if(InKeepElementFunction(CastElement))
						{
							Results.Add(CastElement);
						}
                    }
                    ElementVisited[InElement->GetIndex()] = true;
                }
            });
		}
		else
		{
			for (int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++)
			{
				if(T* CastElement = Cast<T>(Elements[ElementIndex]))
				{
					if(InKeepElementFunction(CastElement))
					{
						Results.Add(CastElement);
					}
				}
			}
		}
		return Results;
	}

	/**
	 * Returns all Bone elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	FORCEINLINE TArray<FRigBoneElement*> GetBones(bool bTraverse = false) const
	{
		return GetElementsOfType<FRigBoneElement>(bTraverse);
	}

	/**
	 * Returns all Bone elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get Bones", ScriptName = "GetBones"))
	FORCEINLINE TArray<FRigElementKey> GetBoneKeys(bool bTraverse = true) const
	{
		return GetKeysOfType<FRigBoneElement>(bTraverse);
	}

	/**
	 * Returns all Null elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	FORCEINLINE TArray<FRigNullElement*> GetNulls(bool bTraverse = false) const
	{
		return GetElementsOfType<FRigNullElement>(bTraverse);
	}

	/**
	* Returns all Null elements
	* @param bTraverse Returns the elements in order of a depth first traversal
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get Nulls", ScriptName = "GetNulls"))
	FORCEINLINE TArray<FRigElementKey> GetNullKeys(bool bTraverse = true) const
	{
		return GetKeysOfType<FRigNullElement>(bTraverse);
	}

	/**
	 * Returns all Control elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	FORCEINLINE TArray<FRigControlElement*> GetControls(bool bTraverse = false) const
	{
		return GetElementsOfType<FRigControlElement>(bTraverse);
	}

	/**
	 * Returns all Control elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get Controls", ScriptName = "GetControls"))
	FORCEINLINE TArray<FRigElementKey> GetControlKeys(bool bTraverse = true) const
	{
		return GetKeysOfType<FRigControlElement>(bTraverse);
	}

	/**
	 * Returns all transient Control elements
	 */
	FORCEINLINE TArray<FRigControlElement*> GetTransientControls() const
	{
		return GetFilteredElements<FRigControlElement>([](FRigControlElement* ControlElement) -> bool
		{
			return ControlElement->Settings.bIsTransientControl;
		});
	}

	/**
	 * Returns all Curve elements
	 */
	FORCEINLINE TArray<FRigCurveElement*> GetCurves() const
	{
		return GetElementsOfType<FRigCurveElement>();
	}

	/**
	 * Returns all Curve elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get Curves", ScriptName = "GetCurves"))
	FORCEINLINE TArray<FRigElementKey> GetCurveKeys() const
	{
		return GetKeysOfType<FRigCurveElement>(false);
	}

	/**
	 * Returns all RigidBody elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	FORCEINLINE TArray<FRigRigidBodyElement*> GetRigidBodies(bool bTraverse = false) const
	{
		return GetElementsOfType<FRigRigidBodyElement>(bTraverse);
	}

	/**
	 * Returns all RigidBody elements
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get RigidBodies", ScriptName = "GetRigidBodies"))
    FORCEINLINE TArray<FRigElementKey> GetRigidBodyKeys(bool bTraverse = true) const
	{
		return GetKeysOfType<FRigRigidBodyElement>(bTraverse);
	}

	/**
	 * Returns all references
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	FORCEINLINE TArray<FRigReferenceElement*> GetReferences(bool bTraverse = false) const
	{
		return GetElementsOfType<FRigReferenceElement>(bTraverse);
	}

	/**
	 * Returns all references
	 * @param bTraverse Returns the elements in order of a depth first traversal
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get References", ScriptName = "GetReferences"))
    FORCEINLINE TArray<FRigElementKey> GetReferenceKeys(bool bTraverse = true) const
	{
		return GetKeysOfType<FRigReferenceElement>(bTraverse);
	}

	/**
	 * Returns the name of metadata for a given element
	 * @param InItem The element key to return the metadata keys for
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	TArray<FName> GetMetadataNames(FRigElementKey InItem);

	/**
	 * Returns the type of metadata given its name the item it is stored under
	 * @param InItem The element key to return the metadata type for
	 * @param InMetadataName The name of the metadata to return the type for
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	ERigMetadataType GetMetadataType(FRigElementKey InItem, FName InMetadataName);

	/**
	 * Removes the metadata under a given element 
	 * @param InItem The element key to search under
	 * @param InMetadataName The name of the metadata to remove
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool RemoveMetadata(FRigElementKey InItem, FName InMetadataName);

	/**
     * Removes all of the metadata under a given item 
	 * @param InItem The element key to search under
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool RemoveAllMetadata(FRigElementKey InItem);

	/**
	 * Queries and returns the value of bool metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE bool GetBoolMetadata(FRigElementKey InItem, FName InMetadataName, bool DefaultValue) const
	{
		return GetMetadata<bool>(InItem, ERigMetadataType::Bool, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of bool array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<bool> GetBoolArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<bool>(InItem, ERigMetadataType::BoolArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a bool value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetBoolMetadata(FRigElementKey InItem, FName InMetadataName, bool InValue)
	{
		return SetMetadata<bool>(InItem, ERigMetadataType::Bool, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a bool array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetBoolArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<bool> InValue)
	{
		return SetArrayMetadata<bool>(InItem, ERigMetadataType::BoolArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of float metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE float GetFloatMetadata(FRigElementKey InItem, FName InMetadataName, float DefaultValue) const
	{
		return GetMetadata<float>(InItem, ERigMetadataType::Float, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of float array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<float> GetFloatArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<float>(InItem, ERigMetadataType::FloatArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a float value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetFloatMetadata(FRigElementKey InItem, FName InMetadataName, float InValue)
	{
		return SetMetadata<float>(InItem, ERigMetadataType::Float, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a float array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetFloatArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<float> InValue)
	{
		return SetArrayMetadata<float>(InItem, ERigMetadataType::FloatArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of int32 metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE int32 GetInt32Metadata(FRigElementKey InItem, FName InMetadataName, int32 DefaultValue) const
	{
		return GetMetadata<int32>(InItem, ERigMetadataType::Int32, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of int32 array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<int32> GetInt32ArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<int32>(InItem, ERigMetadataType::Int32Array, InMetadataName);
	}

	/**
	 * Sets the metadata to a int32 value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetInt32Metadata(FRigElementKey InItem, FName InMetadataName, int32 InValue)
	{
		return SetMetadata<int32>(InItem, ERigMetadataType::Int32, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a int32 array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetInt32ArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<int32> InValue)
	{
		return SetArrayMetadata<int32>(InItem, ERigMetadataType::Int32Array, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FName metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FName GetNameMetadata(FRigElementKey InItem, FName InMetadataName, FName DefaultValue) const
	{
		return GetMetadata<FName>(InItem, ERigMetadataType::Name, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FName array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FName> GetNameArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FName>(InItem, ERigMetadataType::NameArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FName value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetNameMetadata(FRigElementKey InItem, FName InMetadataName, FName InValue)
	{
		return SetMetadata<FName>(InItem, ERigMetadataType::Name, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FName array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetNameArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FName> InValue)
	{
		return SetArrayMetadata<FName>(InItem, ERigMetadataType::FloatArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FVector metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FVector GetVectorMetadata(FRigElementKey InItem, FName InMetadataName, FVector DefaultValue) const
	{
		return GetMetadata<FVector>(InItem, ERigMetadataType::Vector, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FVector array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FVector> GetVectorArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FVector>(InItem, ERigMetadataType::VectorArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FVector value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetVectorMetadata(FRigElementKey InItem, FName InMetadataName, FVector InValue)
	{
		return SetMetadata<FVector>(InItem, ERigMetadataType::Vector, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FVector array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetVectorArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FVector> InValue)
	{
		return SetArrayMetadata<FVector>(InItem, ERigMetadataType::VectorArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FRotator metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FRotator GetRotatorMetadata(FRigElementKey InItem, FName InMetadataName, FRotator DefaultValue) const
	{
		return GetMetadata<FRotator>(InItem, ERigMetadataType::Rotator, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FRotator array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FRotator> GetRotatorArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FRotator>(InItem, ERigMetadataType::RotatorArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FRotator value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetRotatorMetadata(FRigElementKey InItem, FName InMetadataName, FRotator InValue)
	{
		return SetMetadata<FRotator>(InItem, ERigMetadataType::Rotator, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FRotator array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetRotatorArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FRotator> InValue)
	{
		return SetArrayMetadata<FRotator>(InItem, ERigMetadataType::RotatorArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FQuat metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FQuat GetQuatMetadata(FRigElementKey InItem, FName InMetadataName, FQuat DefaultValue) const
	{
		return GetMetadata<FQuat>(InItem, ERigMetadataType::Quat, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FQuat array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FQuat> GetQuatArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FQuat>(InItem, ERigMetadataType::QuatArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FQuat value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetQuatMetadata(FRigElementKey InItem, FName InMetadataName, FQuat InValue)
	{
		return SetMetadata<FQuat>(InItem, ERigMetadataType::Quat, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FQuat array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetQuatArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FQuat> InValue)
	{
		return SetArrayMetadata<FQuat>(InItem, ERigMetadataType::QuatArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FTransform metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FTransform GetTransformMetadata(FRigElementKey InItem, FName InMetadataName, FTransform DefaultValue) const
	{
		return GetMetadata<FTransform>(InItem, ERigMetadataType::Transform, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FTransform array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FTransform> GetTransformArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FTransform>(InItem, ERigMetadataType::TransformArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FTransform value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetTransformMetadata(FRigElementKey InItem, FName InMetadataName, FTransform InValue)
	{
		return SetMetadata<FTransform>(InItem, ERigMetadataType::Transform, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FTransform array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetTransformArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FTransform> InValue)
	{
		return SetArrayMetadata<FTransform>(InItem, ERigMetadataType::TransformArray, InMetadataName, InValue);
	}

		/**
	 * Queries and returns the value of FLinearColor metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FLinearColor GetLinearColorMetadata(FRigElementKey InItem, FName InMetadataName, FLinearColor DefaultValue) const
	{
		return GetMetadata<FLinearColor>(InItem, ERigMetadataType::LinearColor, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FLinearColor array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FLinearColor> GetLinearColorArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FLinearColor>(InItem, ERigMetadataType::LinearColorArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FLinearColor value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetLinearColorMetadata(FRigElementKey InItem, FName InMetadataName, FLinearColor InValue)
	{
		return SetMetadata<FLinearColor>(InItem, ERigMetadataType::LinearColor, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FLinearColor array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetLinearColorArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FLinearColor> InValue)
	{
		return SetArrayMetadata<FLinearColor>(InItem, ERigMetadataType::LinearColorArray, InMetadataName, InValue);
	}

	/**
	 * Queries and returns the value of FRigElementKey metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 * @param DefaultValue The default value to fall back on
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE FRigElementKey GetRigElementKeyMetadata(FRigElementKey InItem, FName InMetadataName, FRigElementKey DefaultValue) const
	{
		return GetMetadata<FRigElementKey>(InItem, ERigMetadataType::RigElementKey, InMetadataName, DefaultValue);
	}

	/**
	 * Queries and returns the value of FRigElementKey array metadata
	 * @param InItem The element key to return the metadata for
	 * @param InMetadataName The name of the metadata to query
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FRigElementKey> GetRigElementKeyArrayMetadata(FRigElementKey InItem, FName InMetadataName) const
	{
		return GetArrayMetadata<FRigElementKey>(InItem, ERigMetadataType::RigElementKeyArray, InMetadataName);
	}

	/**
	 * Sets the metadata to a FRigElementKey value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetRigElementKeyMetadata(FRigElementKey InItem, FName InMetadataName, FRigElementKey InValue)
	{
		return SetMetadata<FRigElementKey>(InItem, ERigMetadataType::RigElementKey, InMetadataName, InValue);
	}

	/**
	 * Sets the metadata to a FRigElementKey array value
	 * @param InItem The element key to set the metadata for
	 * @param InMetadataName The name of the metadata to set
	 * @param InValue The value to set
	 * @return Returns true if setting the metadata was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetRigElementKeyArrayMetadata(FRigElementKey InItem, FName InMetadataName, TArray<FRigElementKey> InValue)
	{
		return SetArrayMetadata<FRigElementKey>(InItem, ERigMetadataType::RigElementKeyArray, InMetadataName, InValue);
	}

	/*
	 * Returns the tags for a given item
	 * @param InItem The item to return the tags for
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE TArray<FName> GetTags(FRigElementKey InItem) const
	{
		return GetNameArrayMetadata(InItem, TagMetadataName);
	}

	/*
	 * Returns true if a given item has a certain tag
	 * @param InItem The item to return the tags for
	 * @param InTag The tag to check
	 */
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	FORCEINLINE bool HasTag(FRigElementKey InItem, FName InTag) const
	{
		return GetTags(InItem).Contains(InTag);
	}

	/*
     * Sets a tag on an element in the hierarchy
     * @param InItem The item to set the tag for
     * @param InTag The tag to set
     */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool SetTag(FRigElementKey InItem, FName InTag)
	{
		TArray<FName> Tags = GetTags(InItem);
		Tags.AddUnique(InTag);
		return SetNameArrayMetadata(InItem, TagMetadataName, Tags);
	}

	/**
	 * Returns the selected elements
	 * @InTypeFilter The types to retrieve the selection for
	 * @return An array of the currently selected elements
	 */
	TArray<const FRigBaseElement*> GetSelectedElements(ERigElementType InTypeFilter = ERigElementType::All) const;

	/**
	 * Returns the keys of selected elements
	 * @InTypeFilter The types to retrieve the selection for
	 * @return An array of the currently selected elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	TArray<FRigElementKey> GetSelectedKeys(ERigElementType InTypeFilter = ERigElementType::All) const;

	/**
	 * Returns true if a given element is selected
	 * @param InKey The key to check
	 * @return true if a given element is selected
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool IsSelected(FRigElementKey InKey) const
	{
		return IsSelected(Find(InKey));
	}

	/**
	 * Returns true if a given element is selected
	 * @param InIndex The index to check
	 * @return true if a given element is selected
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE bool IsSelectedByIndex(int32 InIndex) const
	{
		return IsSelected(Get(InIndex));
	}

	FORCEINLINE bool IsSelected(int32 InIndex) const
	{
		return IsSelectedByIndex(InIndex);
	}

	/**
	 * Sorts the input key list by traversing the hierarchy
	 * @param InKeys The keys to sort
	 * @return The sorted keys
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE TArray<FRigElementKey> SortKeys(const TArray<FRigElementKey>& InKeys) const
	{
		TArray<FRigElementKey> Result;
		Traverse([InKeys, &Result](FRigBaseElement* Element, bool& bContinue)
        {
            const FRigElementKey& Key = Element->GetKey();
            if(InKeys.Contains(Key))
            {
                Result.AddUnique(Key);
            }
        });
		return Result;
	}

	/**
	 * Returns the max allowed length for a name within the hierarchy.
	 * @return Returns the max allowed length for a name within the hierarchy.
	 */
	static int32 GetMaxNameLength() { return 100; }

	/**
	 * Sanitizes a name by removing invalid characters.
	 * @param InOutName The name to sanitize in place.
	 */
	static void SanitizeName(FString& InOutName);

	/**
	 * Sanitizes a name by removing invalid characters.
	 * @param InName The name to sanitize.
	 * @return The sanitized name.
 	 */
	static FName GetSanitizedName(const FString& InName);

	/**
	 * Returns true if a given name is available.
	 * @param InPotentialNewName The name to test for availability
	 * @param InType The type of the to-be-added element
	 * @param OutErrorMessage An optional pointer to return a potential error message 
	 * @return Returns true if the name is available.
	 */
	bool IsNameAvailable(const FString& InPotentialNewName, ERigElementType InType, FString* OutErrorMessage = nullptr) const;

	/**
	 * Returns true if a given display name is available.
	 * @param InParentElement The element to check the display name under
	 * @param InPotentialNewDisplayName The name to test for availability
	 * @param OutErrorMessage An optional pointer to return a potential error message 
	 * @return Returns true if the name is available.
	 */
	bool IsDisplayNameAvailable(const FRigElementKey& InParentElement, const FString& InPotentialNewDisplayName, FString* OutErrorMessage = nullptr) const;

	/**
	 * Returns a valid new name for a to-be-added element.
	 * @param InPotentialNewName The name to be sanitized and adjusted for availability
	 * @param InType The type of the to-be-added element
	 * @return Returns the name to use for the to-be-added element.
	 */
	FName GetSafeNewName(const FString& InPotentialNewName, ERigElementType InType) const;

	/**
	 * Returns a valid new display name for a control
	 * @param InParentElement The element to check the display name under
	 * @param InPotentialNewDisplayName The name to be sanitized and adjusted for availability
	 * @return Returns the name to use for the to-be-added element.
	 */
	FName GetSafeNewDisplayName(const FRigElementKey& InParentElement, const FString& InPotentialNewDisplayName) const;

	/**
	 * Returns the modified event, which can be used to 
	 * subscribe to topological changes happening within the hierarchy.
	 * @return The event used for subscription.
	 */
	FRigHierarchyModifiedEvent& OnModified() { return ModifiedEvent; }

	/**
	 * Returns the MetadataChanged event, which can be used to track metadata changes
	 * Note: This notification has a very high volume - so the consequences of subscribing
	 * to it may cause performance slowdowns.
	 */
	FRigHierarchyMetadataChangedDelegate& OnMetadataChanged() { return MetadataChangedDelegate; }

	/**
	 * Returns the MetadataTagChanged event, which can be used to track metadata tag changes
	 * Note: This notification has a very high volume - so the consequences of subscribing
	 * to it may cause performance slowdowns.
	 */
	FRigHierarchyMetadataTagChangedDelegate& OnMetadataTagChanged() { return MetadataTagChangedDelegate; }

	/**
	 * Returns the local current or initial value for a given key.
	 * If the key is invalid FTransform::Identity will be returned.
	 * @param InKey The key to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The local current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetLocalTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetLocalTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the local current or initial value for a element index.
	 * If the index is invalid FTransform::Identity will be returned.
	 * @param InElementIndex The index to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The local current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE FTransform GetLocalTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[InElementIndex]))
			{
				return GetTransform(TransformElement, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal);
			}
		}
		return FTransform::Identity;
	}

	FORCEINLINE_DEBUGGABLE FTransform GetLocalTransform(int32 InElementIndex) const
	{
		return GetLocalTransformByIndex(InElementIndex, false);
	}
	FORCEINLINE_DEBUGGABLE FTransform GetInitialLocalTransform(int32 InElementIndex) const
	{
		return GetLocalTransformByIndex(InElementIndex, true);
	}

	FORCEINLINE_DEBUGGABLE FTransform GetInitialLocalTransform(const FRigElementKey &InKey) const
	{
		return GetLocalTransform(InKey, true);
	}

	/**
	 * Sets the local current or initial transform for a given key.
	 * @param InKey The key to set the transform for
	 * @param InTransform The new transform value to set
	 * @param bInitial If true the initial transform will be used
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetLocalTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetLocalTransformByIndex(GetIndex(InKey), InTransform, bInitial, bAffectChildren, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Sets the local current or initial transform for a given element index.
	 * @param InElementIndex The index of the element to set the transform for
	 * @param InTransform The new transform value to set
	 * @param bInitial If true the initial transform will be used
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetLocalTransformByIndex(int32 InElementIndex, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[InElementIndex]))
			{
				SetTransform(TransformElement, InTransform, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal, bAffectChildren, bSetupUndo, false, bPrintPythonCommands);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void SetLocalTransform(int32 InElementIndex, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetLocalTransformByIndex(InElementIndex, InTransform, false, bAffectChildren, bSetupUndo, bPrintPythonCommands);
	}

	FORCEINLINE_DEBUGGABLE void SetInitialLocalTransform(int32 InElementIndex, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetLocalTransformByIndex(InElementIndex, InTransform, true, bAffectChildren, bSetupUndo, bPrintPythonCommands);
    }

	FORCEINLINE_DEBUGGABLE void SetInitialLocalTransform(const FRigElementKey& InKey, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetLocalTransform(InKey, InTransform, true, bAffectChildren, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Returns the global current or initial value for a given key.
	 * If the key is invalid FTransform::Identity will be returned.
	 * @param InKey The key to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetGlobalTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the global current or initial value for a element index.
	 * If the index is invalid FTransform::Identity will be returned.
	 * @param InElementIndex The index to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[InElementIndex]))
			{
				return GetTransform(TransformElement, bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal);
			}
		}
		return FTransform::Identity;
	}

	FORCEINLINE_DEBUGGABLE FTransform GetGlobalTransform(int32 InElementIndex) const
	{
		return GetGlobalTransformByIndex(InElementIndex, false);
	}
	FORCEINLINE_DEBUGGABLE FTransform GetInitialGlobalTransform(int32 InElementIndex) const
	{
		return GetGlobalTransformByIndex(InElementIndex, true);
	}

	FORCEINLINE_DEBUGGABLE FTransform GetInitialGlobalTransform(const FRigElementKey &InKey) const
	{
		return GetGlobalTransform(InKey, true);
	}

	/**
	 * Sets the global current or initial transform for a given key.
	 * @param InKey The key to set the transform for
	 * @param InTransform The new transform value to set
	 * @param bInitial If true the initial transform will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetGlobalTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommand = false)
	{
		SetGlobalTransformByIndex(GetIndex(InKey), InTransform, bInitial, bAffectChildren, bSetupUndo, bPrintPythonCommand);
	}

	/**
	 * Sets the global current or initial transform for a given element index.
	 * @param InElementIndex The index of the element to set the transform for
	 * @param InTransform The new transform value to set
	 * @param bInitial If true the initial transform will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetGlobalTransformByIndex(int32 InElementIndex, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommand = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Elements[InElementIndex]))
			{
				SetTransform(TransformElement, InTransform, bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal, bAffectChildren, bSetupUndo, false, bPrintPythonCommand);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void SetGlobalTransform(int32 InElementIndex, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false)
	{
		SetGlobalTransformByIndex(InElementIndex, InTransform, false, bAffectChildren, bSetupUndo);
	}

	FORCEINLINE_DEBUGGABLE void SetInitialGlobalTransform(int32 InElementIndex, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false)
	{
		SetGlobalTransformByIndex(InElementIndex, InTransform, true, bAffectChildren, bSetupUndo);
	}

	FORCEINLINE_DEBUGGABLE void SetInitialGlobalTransform(const FRigElementKey& InKey, const FTransform& InTransform, bool bAffectChildren = true, bool bSetupUndo = false)
	{
		SetGlobalTransform(InKey, InTransform, true, bAffectChildren, bSetupUndo);
	}

	/**
	 * Returns the global offset transform for a given control element.
	 * @param InKey The key of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global offset transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalControlOffsetTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetGlobalControlOffsetTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the global offset transform for a given control element.
	 * @param InElementIndex The index of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global offset transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalControlOffsetTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				return GetControlOffsetTransform(ControlElement, bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal);
			}
		}
		return FTransform::Identity;
	}

	/**
	 * Returns the local shape transform for a given control element.
	 * @param InKey The key of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The local shape transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE FTransform GetLocalControlShapeTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetLocalControlShapeTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the local shape transform for a given control element.
	 * @param InElementIndex The index of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The local shape transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE FTransform GetLocalControlShapeTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				return GetControlShapeTransform(ControlElement, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal);
			}
		}
		return FTransform::Identity;
	}

	/**
	 * Returns the global shape transform for a given control element.
	 * @param InKey The key of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global shape transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalControlShapeTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetGlobalControlShapeTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the global shape transform for a given control element.
	 * @param InElementIndex The index of the control to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The global shape transform
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetGlobalControlShapeTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				return GetControlShapeTransform(ControlElement, bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal);
			}
		}
		return FTransform::Identity;
	}

	/**
	 * Returns a control's current value given its key
	 * @param InKey The key of the element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FRigControlValue GetControlValue(FRigElementKey InKey, ERigControlValueType InValueType = ERigControlValueType::Current) const
	{
		return GetControlValueByIndex(GetIndex(InKey), InValueType);
	}

	/**
	 * Returns a control's current value given its key
	 * @param InKey The key of the element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetControlValue(FRigElementKey InKey, ERigControlValueType InValueType = ERigControlValueType::Current) const
	{
		return GetControlValueByIndex(GetIndex(InKey), InValueType).Get<T>();
	}

	/**
	 * Returns a control's current value given its index
	 * @param InElementIndex The index of the element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FRigControlValue GetControlValueByIndex(int32 InElementIndex, ERigControlValueType InValueType = ERigControlValueType::Current) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				return GetControlValue(ControlElement, InValueType);
			}
		}
		return FRigControlValue();
	}

	/**
	 * Returns a control's current value given its index
	 * @param InElementIndex The index of the element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	FORCEINLINE_DEBUGGABLE FRigControlValue GetControlValue(int32 InElementIndex, ERigControlValueType InValueType = ERigControlValueType::Current) const
	{
		return GetControlValueByIndex(InElementIndex, InValueType);
	}

	/**
	 * Returns a control's current value given its index
	 * @param InElementIndex The index of the element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetControlValue(int32 InElementIndex, ERigControlValueType InValueType = ERigControlValueType::Current) const
	{
		return GetControlValueByIndex(InElementIndex, InValueType).Get<T>();
	}

	/**
	 * Returns a control's initial value given its index
	 * @param InElementIndex The index of the element to retrieve the initial value for
	 * @return Returns the current value of the control
	 */
	FORCEINLINE_DEBUGGABLE FRigControlValue GetInitialControlValue(int32 InElementIndex) const
	{
		return GetControlValueByIndex(InElementIndex, ERigControlValueType::Initial);
	}

	/**
	 * Returns a control's initial value given its index
	 * @param InElementIndex The index of the element to retrieve the current value for
	 * @return Returns the current value of the control
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetInitialControlValue(int32 InElementIndex) const
	{
		return GetInitialControlValue(InElementIndex).Get<T>();
	}

	/**
	 * Returns a control's preferred rotator (local transform rotation)
	 * @param InKey The key of the element to retrieve the current value for
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @return Returns the current preferred rotator
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE FRotator GetControlPreferredRotator(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetControlPreferredRotatorByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns a control's preferred rotator (local transform rotation)
	 * @param InElementIndex The element index to look up
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @return Returns the current preferred rotator
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE FRotator GetControlPreferredRotatorByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				return GetControlPreferredRotator(ControlElement, bInitial);
			}
		}
		return FRotator::ZeroRotator;
	}

	/**
	 * Returns a control's preferred rotator (local transform rotation)
	 * @param InControlElement The element to look up
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @return Returns the current preferred rotator
	 */
	FORCEINLINE_DEBUGGABLE FRotator GetControlPreferredRotator(FRigControlElement* InControlElement, bool bInitial = false) const
	{
		if(InControlElement)
		{
			return InControlElement->PreferredEulerAngles.GetRotator(bInitial);
		}
		return FRotator::ZeroRotator;
	}

	/**
	 * Sets a control's preferred rotator (local transform rotation)
	 * @param InKey The key of the element to retrieve the current value for
	 * @param InValue The new preferred rotator to set
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @param bFixEulerFlips If true the new rotator value will use the shortest path
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetControlPreferredRotator(FRigElementKey InKey, const FRotator& InValue, bool bInitial = false, bool bFixEulerFlips = false)
	{
		SetControlPreferredRotatorByIndex(GetIndex(InKey), InValue, bInitial, bFixEulerFlips);
	}
	

	/**
	 * Sets a control's preferred rotator (local transform rotation)
	 * @param InElementIndex The element index to look up
	 * @param InValue The new preferred rotator to set
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @param bFixEulerFlips If true the new rotator value will use the shortest path
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetControlPreferredRotatorByIndex(int32 InElementIndex, const FRotator& InValue, bool bInitial = false, bool bFixEulerFlips = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlPreferredRotator(ControlElement, InValue, bInitial, bFixEulerFlips);
			}
		}
	}

	/**
	 * Sets a control's preferred rotator (local transform rotation)
	 * @param InControlElement The element to look up
	 * @param InValue The new preferred rotator to set
	 * @param bInitial If true we'll return the preferred rotator for the initial - otherwise current transform
	 * @param bFixEulerFlips If true the new rotator value will use the shortest path
	 */
	FORCEINLINE_DEBUGGABLE void SetControlPreferredRotator(FRigControlElement* InControlElement, const FRotator& InValue, bool bInitial = false, bool bFixEulerFlips = false)
	{
		if(InControlElement)
		{
			InControlElement->PreferredEulerAngles.SetRotator(InValue, bInitial, bFixEulerFlips);
		}
	}
	
	/**
	 * Returns the pin type to use for a control
	 * @param InControlElement The control to return the pin type for
	 * @return The pin type
	 */
	FEdGraphPinType GetControlPinType(FRigControlElement* InControlElement) const;

	/**
	 * Returns the default value to use for a pin for a control
	 * @param InControlElement The control to return the pin default value for
	 * @param bForEdGraph If this is true to the math types' ::ToString will be used rather than text export
	 * @param InValueType The type of value to return
	 * @return The pin default value
	 */
	FString GetControlPinDefaultValue(FRigControlElement* InControlElement, bool bForEdGraph, ERigControlValueType InValueType = ERigControlValueType::Initial) const;

	/**
	 * Sets a control's current value given its key
	 * @param InKey The key of the element to set the current value for
	 * @param InValue The value to set on the control
	 * @param InValueType The type of value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlValue(FRigElementKey InKey, FRigControlValue InValue, ERigControlValueType InValueType = ERigControlValueType::Current, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetControlValueByIndex(GetIndex(InKey), InValue, InValueType, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Sets a control's current value given its key
	 * @param InKey The key of the element to set the current value for
	 * @param InValue The value to set on the control
	 * @param InValueType The type of value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void SetControlValue(FRigElementKey InKey, const T& InValue, ERigControlValueType InValueType = ERigControlValueType::Current, bool bSetupUndo = false, bool bPrintPythonCommands = false) const
	{
		return SetControlValue(InKey, FRigControlValue::Make<T>(InValue), InValueType, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Sets a control's current value given its index
	 * @param InElementIndex The index of the element to set the current value for
	 * @param InValue The value to set on the control
	 * @param InValueType The type of value to set
  	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetControlValueByIndex(int32 InElementIndex, FRigControlValue InValue, ERigControlValueType InValueType = ERigControlValueType::Current, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlValue(ControlElement, InValue, InValueType, bSetupUndo, false, bPrintPythonCommands);
			}
		}
	}

	/**
	 * Sets a control's current value given its index
	 * @param InElementIndex The index of the element to set the current value for
	 * @param InValue The value to set on the control
	 * @param InValueType The type of value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	FORCEINLINE_DEBUGGABLE void SetControlValue(int32 InElementIndex, const FRigControlValue& InValue, ERigControlValueType InValueType = ERigControlValueType::Current, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		SetControlValueByIndex(InElementIndex, InValue, InValueType, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Sets a control's current value given its index
	 * @param InElementIndex The index of the element to set the current value for
	 * @param InValue The value to set on the control
	 * @param InValueType The type of value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void SetControlValue(int32 InElementIndex, const T& InValue, ERigControlValueType InValueType = ERigControlValueType::Current, bool bSetupUndo = false) const
	{
		return SetControlValue(InElementIndex, FRigControlValue::Make<T>(InValue), InValueType, bSetupUndo);
	}

	/**
	 * Sets a control's initial value given its index
	 * @param InElementIndex The index of the element to set the initial value for
	 * @param InValue The value to set on the control
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	FORCEINLINE_DEBUGGABLE void SetInitialControlValue(int32 InElementIndex, const FRigControlValue& InValue, bool bSetupUndo = false)
	{
		SetControlValueByIndex(InElementIndex, InValue, ERigControlValueType::Initial, bSetupUndo);
	}

	/**
	 * Sets a control's initial value given its index
	 * @param InElementIndex The index of the element to set the initial value for
	 * @param InValue The value to set on the control
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	template<typename T>
	FORCEINLINE_DEBUGGABLE void SetInitialControlValue(int32 InElementIndex, const T& InValue, bool bSetupUndo = false) const
	{
		return SetInitialControlValue(InElementIndex, FRigControlValue::Make<T>(InValue), bSetupUndo);
	}

	/**
	 * Sets a control's current visibility based on a key
	 * @param InKey The key of the element to set the visibility for
	 * @param bVisibility The visibility to set on the control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlVisibility(FRigElementKey InKey, bool bVisibility)
	{
		SetControlVisibilityByIndex(GetIndex(InKey), bVisibility);
	}

	/**
	 * Sets a control's current visibility based on a key
	 * @param InElementIndex The index of the element to set the visibility for
	 * @param bVisibility The visibility to set on the control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlVisibilityByIndex(int32 InElementIndex, bool bVisibility)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlVisibility(ControlElement, bVisibility);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void SetControlVisibility(int32 InElementIndex, bool bVisibility)
	{
		SetControlVisibilityByIndex(InElementIndex, bVisibility);
	}

	/**
	 * Returns a curve's value given its key
	 * @param InKey The key of the element to retrieve the value for
	 * @return Returns the value of the curve
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE float GetCurveValue(FRigElementKey InKey) const
	{
		return GetCurveValueByIndex(GetIndex(InKey));
	}

	/**
	 * Returns a curve's value given its index
	 * @param InElementIndex The index of the element to retrieve the value for
	 * @return Returns the value of the curve
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE float GetCurveValueByIndex(int32 InElementIndex) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[InElementIndex]))
			{
				return GetCurveValue(CurveElement);
			}
		}
		return 0.f;
	}

	// TODO: Deprecate?
	FORCEINLINE_DEBUGGABLE float GetCurveValue(int32 InElementIndex) const
	{
		return GetCurveValueByIndex(InElementIndex);
	}

	/**
	 * Returns whether a curve's value is set, given its key
	 * @param InKey The key of the element to retrieve the value for
	 * @return Returns true if the value is set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE bool IsCurveValueSet(FRigElementKey InKey) const
	{
		return IsCurveValueSetByIndex(GetIndex(InKey));
	}

	/**
	 * Returns a curve's value given its index
	 * @param InElementIndex The index of the element to retrieve the value for
	 * @return Returns true if the value is set, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE bool IsCurveValueSetByIndex(int32 InElementIndex) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[InElementIndex]))
			{
				return IsCurveValueSet(CurveElement);
			}
		}
		return false;
	}
	
	/**
	 * Sets a curve's value given its key
	 * @param InKey The key of the element to set the value for
	 * @param InValue The value to set on the curve
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetCurveValue(FRigElementKey InKey, float InValue, bool bSetupUndo = false)
	{
		SetCurveValueByIndex(GetIndex(InKey), InValue, bSetupUndo);
	}

	/**
	 * Sets a curve's value given its index
	 * @param InElementIndex The index of the element to set the value for
	 * @param InValue The value to set on the curve
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetCurveValueByIndex(int32 InElementIndex, float InValue, bool bSetupUndo = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[InElementIndex]))
			{
				SetCurveValue(CurveElement, InValue, bSetupUndo);
			}
		}
	}

	// TODO: Deprecate?
	FORCEINLINE_DEBUGGABLE void SetCurveValue(int32 InElementIndex, float InValue, bool bSetupUndo = false)
	{
		SetCurveValueByIndex(InElementIndex, InValue, bSetupUndo);
	}

	/**
	 * Sets a curve's value given its key
	 * @param InKey The key of the element to set the value for
	 * @param InValue The value to set on the curve
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void UnsetCurveValue(FRigElementKey InKey, bool bSetupUndo = false)
	{
		UnsetCurveValueByIndex(GetIndex(InKey), bSetupUndo);
	}

	/**
	 * Sets a curve's value given its index
	 * @param InElementIndex The index of the element to set the value for
	 * @param InValue The value to set on the curve
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void UnsetCurveValueByIndex(int32 InElementIndex, bool bSetupUndo = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Elements[InElementIndex]))
			{
				UnsetCurveValue(CurveElement, bSetupUndo);
			}
		}
	}

	/**
	 * Sets the offset transform for a given control element by key
	 * @param InKey The key of the control element to set the offset transform for
	 * @param InTransform The new offset transform value to set
	 * @param bInitial If true the initial value will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlOffsetTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		return SetControlOffsetTransformByIndex(GetIndex(InKey), InTransform, bInitial, bAffectChildren, bSetupUndo, bPrintPythonCommands);
	}

	/**
	 * Sets the local offset transform for a given control element by index
	 * @param InElementIndex The index of the control element to set the offset transform for
 	 * @param InTransform The new local offset transform value to set
	 * @param bInitial If true the initial value will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlOffsetTransformByIndex(int32 InElementIndex, FTransform InTransform, bool bInitial = false, bool bAffectChildren = true, bool bSetupUndo = false, bool bPrintPythonCommands = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlOffsetTransform(ControlElement, InTransform, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal, bAffectChildren, bSetupUndo, bPrintPythonCommands);
			}
		}
	}

	/**
	 * Sets the shape transform for a given control element by key
	 * @param InKey The key of the control element to set the shape transform for
	 * @param InTransform The new shape transform value to set
	 * @param bInitial If true the initial value will be used
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlShapeTransform(FRigElementKey InKey, FTransform InTransform, bool bInitial = false, bool bSetupUndo = false)
	{
		return SetControlShapeTransformByIndex(GetIndex(InKey), InTransform, bInitial, bSetupUndo);
	}

	/**
	 * Sets the local shape transform for a given control element by index
	 * @param InElementIndex The index of the control element to set the shape transform for
	 * @param InTransform The new local shape transform value to set
	 * @param bInitial If true the initial value will be used
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE void SetControlShapeTransformByIndex(int32 InElementIndex, FTransform InTransform, bool bInitial = false, bool bSetupUndo = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlShapeTransform(ControlElement, InTransform, bInitial ? ERigTransformType::InitialLocal : ERigTransformType::CurrentLocal, bSetupUndo);
			}
		}
	}

	/**
	 * Sets the control settings for a given control element by key
	 * @param InKey The key of the control element to set the settings for
	 * @param InSettings The new control settings value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetControlSettings(FRigElementKey InKey, FRigControlSettings InSettings, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false)
	{
		return SetControlSettingsByIndex(GetIndex(InKey), InSettings, bSetupUndo, bForce, bPrintPythonCommands);
	}

	/**
	 * Sets the control settings for a given control element by index
	 * @param InElementIndex The index of the control element to set the settings for
	 * @param InSettings The new control settings value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE_DEBUGGABLE void SetControlSettingsByIndex(int32 InElementIndex, FRigControlSettings InSettings, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false)
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Elements[InElementIndex]))
			{
				SetControlSettings(ControlElement, InSettings, bSetupUndo, bForce, bPrintPythonCommands);
			}
		}
	}

	/**
	 * Returns the global current or initial value for a given key.
	 * If the element does not have a parent FTransform::Identity will be returned.
	 * @param InKey The key of the element to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The element's parent's global current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetParentTransform(FRigElementKey InKey, bool bInitial = false) const
	{
		return GetParentTransformByIndex(GetIndex(InKey), bInitial);
	}

	/**
	 * Returns the global current or initial value for a given element index.
	 * If the element does not have a parent FTransform::Identity will be returned.
	 * @param InElementIndex The index of the element to retrieve the transform for
	 * @param bInitial If true the initial transform will be used
	 * @return The element's parent's global current or initial transform's value.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FORCEINLINE_DEBUGGABLE FTransform GetParentTransformByIndex(int32 InElementIndex, bool bInitial = false) const
	{
		if(Elements.IsValidIndex(InElementIndex))
		{
			return GetParentTransform(Elements[InElementIndex], bInitial ? ERigTransformType::InitialGlobal : ERigTransformType::CurrentGlobal);
		}
		return FTransform::Identity;
	}

	/**
	 * Returns the child elements of a given element key
	 * @param InKey The key of the element to retrieve the children for
	 * @param bRecursive If set to true grand-children will also be returned etc
	 * @return Returns the child elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	TArray<FRigElementKey> GetChildren(FRigElementKey InKey, bool bRecursive = false) const;

	/**
	 * Returns the child elements of a given element index
	 * @param InIndex The index of the element to retrieve the children for
	 * @param bRecursive If set to true grand-children will also be returned etc
	 * @return Returns the child elements' indices
	 */
    TArray<int32> GetChildren(int32 InIndex, bool bRecursive = false) const;

	/**
	 * Returns the child elements of a given element
	 * @param InElement The element to retrieve the children for
	 * @return Returns the child elements
	 */
	const FRigBaseElementChildrenArray& GetChildren(const FRigBaseElement* InElement) const;

	/**
	 * Returns the child elements of a given element
	 * @param InElement The element to retrieve the children for
	 * @param bRecursive If set to true grand-children will also be returned etc
	 * @return Returns the child elements
	 */
	FRigBaseElementChildrenArray GetChildren(const FRigBaseElement* InElement, bool bRecursive) const;

	/**
	 * Returns the parent elements of a given element key
	 * @param InKey The key of the element to retrieve the parents for
	 * @param bRecursive If set to true parents of parents will also be returned
	 * @return Returns the parent elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    TArray<FRigElementKey> GetParents(FRigElementKey InKey, bool bRecursive = false) const;

	/**
	 * Returns the parent elements of a given element index
	 * @param InIndex The index of the element to retrieve the parents for
	 * @param bRecursive If set to true parents of parents will also be returned
	 * @return Returns the parent elements' indices
	 */
    TArray<int32> GetParents(int32 InIndex, bool bRecursive = false) const;

	/**
	 * Returns the parent elements of a given element
	 * @param InElement The element to retrieve the parents for
	 * @param bRecursive If set to true parents of parents will also be returned
	 * @return Returns the parent elements
	 */
	FRigBaseElementParentArray GetParents(const FRigBaseElement* InElement, bool bRecursive = false) const;

	/**
	 * Returns the default parent element's key of a given child key
	 * @param InKey The key of the element to retrieve the parent for
	 * @return Returns the default parent element key
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FRigElementKey GetDefaultParent(FRigElementKey InKey) const;

	/**
	 * Returns the first parent element of a given element key
	 * @param InKey The key of the element to retrieve the parents for
	 * @return Returns the first parent element
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FRigElementKey GetFirstParent(FRigElementKey InKey) const;

	/**
	 * Returns the first parent element of a given element index
	 * @param InIndex The index of the element to retrieve the parent for
	 * @return Returns the first parent index (or INDEX_NONE)
	 */
    int32 GetFirstParent(int32 InIndex) const;

	/**
	 * Returns the first parent element of a given element
	 * @param InElement The element to retrieve the parents for
	 * @return Returns the first parent element
	 */
	FRigBaseElement* GetFirstParent(const FRigBaseElement* InElement) const;

	/**
	 * Returns the number of parents of an element
	 * @param InKey The key of the element to retrieve the number of parents for
	 * @return Returns the number of parents of an element
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    int32 GetNumberOfParents(FRigElementKey InKey) const;

	/**
	 * Returns the number of parents of an element
	 * @param InIndex The index of the element to retrieve the number of parents for
	 * @return Returns the number of parents of an element
	 */
    int32 GetNumberOfParents(int32 InIndex) const;

	/**
	 * Returns the number of parents of an element
	 * @param InElement The element to retrieve the number of parents for
	 * @return Returns the number of parents of an element
	 */
	int32 GetNumberOfParents(const FRigBaseElement* InElement) const;

	/**
	 * Returns the weight of a parent below a multi parent element
	 * @param InChild The key of the multi parented element
	 * @param InParent The key of the parent to look up the weight for
	 * @param bInitial If true the initial weights will be used
	 * @return Returns the weight of a parent below a multi parent element, or FLT_MAX if the parent is invalid
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    FRigElementWeight GetParentWeight(FRigElementKey InChild, FRigElementKey InParent, bool bInitial = false) const;

	/**
	 * Returns the weight of a parent below a multi parent element
	 * @param InChild The multi parented element
	 * @param InParent The parent to look up the weight for
	 * @param bInitial If true the initial weights will be used
	 * @return Returns the weight of a parent below a multi parent element, or FLT_MAX if the parent is invalid
	 */
    FRigElementWeight GetParentWeight(const FRigBaseElement* InChild, const FRigBaseElement* InParent, bool bInitial = false) const;

	/**
	 * Returns the weight of a parent below a multi parent element
	 * @param InChild The multi parented element
	 * @param InParentIndex The index of the parent inside of the multi parent element
	 * @param bInitial If true the initial weights will be used
	 * @return Returns the weight of a parent below a multi parent element, or FLT_MAX if the parent is invalid
	 */
	FRigElementWeight GetParentWeight(const FRigBaseElement* InChild, int32 InParentIndex, bool bInitial = false) const;

	/**
	 * Returns the weights of all parents below a multi parent element
	 * @param InChild The key of the multi parented element
	 * @param bInitial If true the initial weights will be used
	 * @return Returns the weight of a parent below a multi parent element, or FLT_MAX if the parent is invalid
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	TArray<FRigElementWeight> GetParentWeightArray(FRigElementKey InChild, bool bInitial = false) const;

	/**
	 * Returns the weights of all parents below a multi parent element
	 * @param InChild The multi parented element
	 * @param bInitial If true the initial weights will be used
	 * @return Returns the weight of a parent below a multi parent element, or FLT_MAX if the parent is invalid
	 */
	TArray<FRigElementWeight> GetParentWeightArray(const FRigBaseElement* InChild, bool bInitial = false) const;

	/**
	 * Get the current active for the passed in key. This is only valid when only one parent has a weight value and the other parents have zero weights
	 * @param InKey The multi parented element
	 * @return Returns the first parent with a non-zero weight
	 */
	FRigElementKey GetActiveParent(const FRigElementKey& InKey) const;

	/**
	 * Sets the weight of a parent below a multi parent element
	 * @param InChild The key of the multi parented element
	 * @param InParent The key of the parent to look up the weight for
	 * @param InWeight The new weight to set for the parent
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    bool SetParentWeight(FRigElementKey InChild, FRigElementKey InParent, FRigElementWeight InWeight, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Sets the weight of a parent below a multi parent element
	 * @param InChild The multi parented element
	 * @param InParent The parent to look up the weight for
	 * @param InWeight The new weight to set for the parent
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	bool SetParentWeight(FRigBaseElement* InChild, const FRigBaseElement* InParent, FRigElementWeight InWeight, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Sets the weight of a parent below a multi parent element
	 * @param InChild The multi parented element
	 * @param InParentIndex The index of the parent inside of the multi parent element
	 * @param InWeight The new weight to set for the parent
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	bool SetParentWeight(FRigBaseElement* InChild, int32 InParentIndex, FRigElementWeight InWeight, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Sets the all of the weights of the parents of a multi parent element
	 * @param InChild The key of the multi parented element
	 * @param InWeights The new weights to set for the parents
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool SetParentWeightArray(FRigElementKey InChild, TArray<FRigElementWeight> InWeights, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Sets the all of the weights of the parents of a multi parent element
	 * @param InChild The multi parented element
	 * @param InWeights The new weights to set for the parents
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	bool SetParentWeightArray(FRigBaseElement* InChild,  const TArray<FRigElementWeight>& InWeights, bool bInitial = false, bool bAffectChildren = true);

	/**
	* Sets the all of the weights of the parents of a multi parent element
	* @param InChild The multi parented element
	* @param InWeights The new weights to set for the parents
	* @param bInitial If true the initial weights will be used
	* @param bAffectChildren If set to false children will not move (maintain global).
	* @return Returns true if changing the weight was successful
	*/
	bool SetParentWeightArray(FRigBaseElement* InChild,  const TArrayView<const FRigElementWeight>& InWeights, bool bInitial = false, bool bAffectChildren = true);

	/**
	* Determines if the element can be switched to a provided parent
	* @param InChild The key of the multi parented element
	* @param InParent The key of the parent to look up the weight for
    * @param InDependencyMap An additional map of dependencies to respect
	* @param OutFailureReason An optional pointer to retrieve the reason for failure
	* @return Returns true if changing the weight was successful
	*/
	bool CanSwitchToParent(FRigElementKey InChild, FRigElementKey InParent, const TElementDependencyMap& InDependencyMap = TElementDependencyMap(), FString* OutFailureReason = nullptr);

	/**
	 * Switches a multi parent element to a single parent.
	 * This sets the new parent's weight to 1.0 and disables
	 * weights for all other potential parents.
	 * @param InChild The key of the multi parented element
	 * @param InParent The key of the parent to look up the weight for
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool SwitchToParent(FRigElementKey InChild, FRigElementKey InParent, bool bInitial = false, bool bAffectChildren = true)
	{
		return SwitchToParent(InChild, InParent, bInitial, bAffectChildren, TElementDependencyMap(), nullptr);
	}
	bool SwitchToParent(FRigElementKey InChild, FRigElementKey InParent, bool bInitial, bool bAffectChildren, const TElementDependencyMap& InDependencyMap, FString* OutFailureReason);

	/**
	 * Switches a multi parent element to a single parent.
	 * This sets the new parent's weight to 1.0 and disables
	 * weights for all other potential parents.
	 * @param InChild The multi parented element
	 * @param InParent The parent to look up the weight for
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
     * @param InDependencyMap An additional map of dependencies to respect
	 * @param OutFailureReason An optional pointer to retrieve the reason for failure
	 * @return Returns true if changing the weight was successful
	 */
	bool SwitchToParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bInitial = false, bool bAffectChildren = true, const TElementDependencyMap& InDependencyMap = TElementDependencyMap(), FString* OutFailureReason = nullptr);

	/**
	 * Switches a multi parent element to a single parent.
	 * This sets the new parent's weight to 1.0 and disables
	 * weights for all other potential parents.
	 * @param InChild The multi parented element
	 * @param InParentIndex The index of the parent inside of the multi parent element
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	bool SwitchToParent(FRigBaseElement* InChild, int32 InParentIndex, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Switches a multi parent element to its first parent
	 * @param InChild The key of the multi parented element
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool SwitchToDefaultParent(FRigElementKey InChild, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Switches a multi parent element to its first parent
	 * This sets the new parent's weight to 1.0 and disables
	 * weights for all other potential parents.
	 * @param InChild The multi parented element
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	bool SwitchToDefaultParent(FRigBaseElement* InChild, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Switches a multi parent element to world space.
	 * This injects a world space reference.
	 * @param InChild The key of the multi parented element
	 * @param bInitial If true the initial weights will be used
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @return Returns true if changing the weight was successful
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	bool SwitchToWorldSpace(FRigElementKey InChild, bool bInitial = false, bool bAffectChildren = true);

	/**
	* Switches a multi parent element to world space.
	* This injects a world space reference.
	* @param InChild The multi parented element
	* @param bInitial If true the initial weights will be used
	* @param bAffectChildren If set to false children will not move (maintain global).
	* @return Returns true if changing the weight was successful
	*/
	bool SwitchToWorldSpace(FRigBaseElement* InChild, bool bInitial = false, bool bAffectChildren = true);

	/**
	 * Adds the world space reference or returns it
	 */
	FRigElementKey GetOrAddWorldSpaceReference();
	
	static FRigElementKey GetDefaultParentKey();
	static FRigElementKey GetWorldSpaceReferenceKey();

	/**
	 * Returns true if an element is parented to another element
	 * @param InChild The key of the child element to check for a parent
	 * @param InParent The key of the parent element to check for
	 * @return True if the child is parented to the parent
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE bool IsParentedTo(FRigElementKey InChild, FRigElementKey InParent) const
	{
		return IsParentedTo(GetIndex(InChild), GetIndex(InParent));
	}

	/**
	 * Returns true if an element is parented to another element
	 * @param InChildIndex The index of the child element to check for a parent
	 * @param InParentIndex The index of the parent element to check for
	 * @param InDependencyMap An additional map of dependencies to respect
	 * @return True if the child is parented to the parent
	 */
    FORCEINLINE bool IsParentedTo(int32 InChildIndex, int32 InParentIndex, const TElementDependencyMap& InDependencyMap = TElementDependencyMap()) const
	{
		if(Elements.IsValidIndex(InChildIndex) && Elements.IsValidIndex(InParentIndex))
		{
			return IsParentedTo(Elements[InChildIndex], Elements[InParentIndex], InDependencyMap);
		}
		return false;
	}

	/**
	 * Returns all element keys of this hierarchy
	 * @param bTraverse If set to true the keys will be returned by depth first traversal
	 * @param InElementType The type filter to apply
	 * @return The keys of all elements
	 */
	TArray<FRigElementKey> GetAllKeys(bool bTraverse = false, ERigElementType InElementType = ERigElementType::All) const;

	/**
	 * Returns element keys of this hierarchy, filtered by a predicate.
	 * @param InPredicateFunc The predicate function to apply. Should return \c true if the element
	 *    should be added to the result array.
	 * @param bInTraverse If set to true the keys will be returned by depth first traversal
	 * @return The keys of all elements
	 */
	TArray<FRigElementKey> GetKeysByPredicate(TFunctionRef<bool(const FRigBaseElement&)> InPredicateFunc, bool bInTraverse = false) const;

	/**
	 * Returns all element keys of this hierarchy
	 * @param bTraverse If set to true the keys will be returned by depth first traversal
	 * @return The keys of all elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Get All Keys", ScriptName = "GetAllKeys"))
	FORCEINLINE TArray<FRigElementKey> GetAllKeys_ForBlueprint(bool bTraverse = true) const
	{
		return GetAllKeys(bTraverse, ERigElementType::All);
	}

	/**
	 * Helper function to traverse the hierarchy
	 * @param InElement The element to start the traversal at
	 * @param bTowardsChildren If set to true the traverser walks downwards (towards the children), otherwise upwards (towards the parents)
	 * @param PerElementFunction The function to call for each visited element
	 */
	void Traverse(FRigBaseElement* InElement, bool bTowardsChildren, TFunction<void(FRigBaseElement*, bool& /* continue */)> PerElementFunction) const;

	/**
	 * Helper function to traverse the hierarchy from the root
	 * @param PerElementFunction The function to call for each visited element
	 * @param bTowardsChildren If set to true the traverser walks downwards (towards the children), otherwise upwards (towards the parents)
	 */
	void Traverse(TFunction<void(FRigBaseElement*, bool& /* continue */)> PerElementFunction, bool bTowardsChildren = true) const;

	/**
	 * Performs undo for one transform change
	 */
	bool Undo();

	/**
	 * Performs redo for one transform change
	 */
	bool Redo();

	/**
	 * Returns the event fired during undo / redo
	 */
	FRigHierarchyUndoRedoTransformEvent& OnUndoRedo() { return UndoRedoEvent; }

	/**
	 * Starts an interaction on the rig.
	 * This will cause all transform actions happening to be merged
	 */
	void StartInteraction() { bIsInteracting = true; }

	/**
	 * Starts an interaction on the rig.
	 * This will cause all transform actions happening to be merged
	 */
	FORCEINLINE void EndInteraction()
	{
		bIsInteracting = false;
		LastInteractedKey.Reset();
	}

	/**
	 * Returns the transform stack index
	 */
	int32 GetTransformStackIndex() const { return TransformStackIndex; }

	/**
	 * Sends an event from the hierarchy to the world
	 * @param InEvent The event to send
	 * @param bAsynchronous If set to true the event will go on a thread safe queue
	 */
	void SendEvent(const FRigEventContext& InEvent, bool bAsynchronous = true);

	/**
	* Sends an autokey event from the hierarchy to the world
	* @param InElement The element to send the autokey for
	* @param InOffsetInSeconds The time offset in seconds
	* @param bAsynchronous If set to true the event will go on a thread safe queue
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	void SendAutoKeyEvent(FRigElementKey InElement, float InOffsetInSeconds = 0.f, bool bAsynchronous = true);

	/**
	 * Returns the delegate to listen to for events coming from this hierarchy
	 * @return The delegate to listen to for events coming from this hierarchy
	 */
	FORCEINLINE FRigEventDelegate& OnEventReceived() { return EventDelegate; }
	
	/**
	 * Returns true if the hierarchy controller is currently available
	 * The controller may not be available during certain events.
	 * If the controller is not available then GetController() will return nullptr.
	 */ 
	UFUNCTION(BlueprintPure, Category = URigHierarchy)
	bool IsControllerAvailable() const;

	/**
	 * Returns a controller for this hierarchy.
	 * Note: If the controller is not available this will return nullptr 
	 * even if the bCreateIfNeeded flag is set to true. You can check the 
	 * controller's availability with IsControllerAvailable().
	 * @param bCreateIfNeeded Creates a controller if needed
	 * @return The Controller for this hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	URigHierarchyController* GetController(bool bCreateIfNeeded = true);

	/**
	 * Returns the topology version of this hierarchy
	 */
	uint16 GetTopologyVersion() const { return TopologyVersion; }

	/**
	 * Increments the topology version
	 */
	void IncrementTopologyVersion();

	/**
	 * Returns the metadata version of this hierarchy
	 */
	uint16 GetMetadataVersion() const { return MetadataVersion; }

	/**
	 * Increments the metadata version
	 */
	FORCEINLINE void IncrementMetadataVersion(const FRigElementKey& InKey, const FName& InName)
	{
		MetadataVersion += 1 + (int32)HashCombine(GetTypeHash(InKey), GetTypeHash(InName));
	}

	/**
     * Returns the metadata tag version of this hierarchy
	 */
	uint16 GetMetadataTagVersion() const { return MetadataTagVersion; }

	/**
	 * Increments the metadataTag version
	 */
	FORCEINLINE void IncrementMetadataTagVersion(const FRigElementKey& InKey, const FName& InTag, bool bAdded)
	{
		MetadataTagVersion += 1 + (int32)HashCombine(GetTypeHash(InKey), GetTypeHash(InTag));
	}

	/**
	 * Returns the current / initial pose of the hierarchy
	 * @param bInitial If set to true the initial pose will be returned
	 * @return The pose of the hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FORCEINLINE FRigPose GetPose(
		bool bInitial = false
	) const
	{
		return GetPose(bInitial, ERigElementType::All, FRigElementKeyCollection());
	}

	/**
	 * Returns the current / initial pose of the hierarchy
	 * @param bInitial If set to true the initial pose will be returned
	 * @param InElementType The types of elements to get
	 * @param InItems An optional list of items to get
	 * @return The pose of the hierarchy
	 */
	FRigPose GetPose(
		bool bInitial,
		ERigElementType InElementType,
		const FRigElementKeyCollection& InItems 
	) const;

	/**
	 * Returns the current / initial pose of the hierarchy
	 * @param bInitial If set to true the initial pose will be returned
	 * @param InElementType The types of elements to get
	 * @param InItems An optional list of items to get
	 * @return The pose of the hierarchy
	 */
	FRigPose GetPose(
		bool bInitial,
		ERigElementType InElementType,
		const TArrayView<const FRigElementKey>& InItems 
	) const;

	/**
	 * Sets the current / initial pose of the hierarchy
	 * @param InPose The pose to set on the hierarchy
	 * @param InTransformType The transform type to set
	 */
	FORCEINLINE void SetPose(
		const FRigPose& InPose,
		ERigTransformType::Type InTransformType = ERigTransformType::CurrentLocal
	)
	{
		SetPose(InPose, InTransformType, ERigElementType::All, FRigElementKeyCollection(), 1.f);
	}

	/**
	 * Sets the current / initial pose of the hierarchy
	 * @param InPose The pose to set on the hierarchy
	 * @param InTransformType The transform type to set
	 * @param InElementType The types of elements to set
	 * @param InItems An optional list of items to set
	 * @param InWeight A weight to define how much the pose needs to be mixed in
	 */
	void SetPose(
		const FRigPose& InPose,
		ERigTransformType::Type InTransformType,
		ERigElementType InElementType,
		const FRigElementKeyCollection& InItems,
		float InWeight
	);

	/**
	 * Sets the current / initial pose of the hierarchy
	 * @param InPose The pose to set on the hierarchy
	 * @param InTransformType The transform type to set
	 * @param InElementType The types of elements to set
	 * @param InItems An optional list of items to set
	 * @param InWeight A weight to define how much the pose needs to be mixed in
	 */
	void SetPose(
		const FRigPose& InPose,
		ERigTransformType::Type InTransformType,
		ERigElementType InElementType,
		const TArrayView<const FRigElementKey>& InItems, 
		float InWeight
	);

	/**
	 * Sets the current / initial pose of the hierarchy
	 * @param InPose The pose to set on the hierarchy
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy, meta = (DisplayName = "Set Pose", ScriptName = "SetPose"))
	FORCEINLINE void SetPose_ForBlueprint(FRigPose InPose)
	{
		return SetPose(InPose);
	}

	/**
	 * Creates a rig control value from a bool value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FRigControlValue MakeControlValueFromBool(bool InValue)
	{
		return FRigControlValue::Make<bool>(InValue);
	}

	/**
	 * Creates a rig control value from a float value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromFloat(float InValue)
	{
		return FRigControlValue::Make<float>(InValue);
	}

	/**
	 * Returns the contained float value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted float value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE float GetFloatFromControlValue(FRigControlValue InValue)
	{
		return InValue.Get<float>();
	}

	/**
	 * Creates a rig control value from a int32 value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromInt(int32 InValue)
	{
		return FRigControlValue::Make<int32>(InValue);
	}

	/**
	 * Returns the contained int32 value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted int32 value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE int32 GetIntFromControlValue(FRigControlValue InValue)
	{
		return InValue.Get<int32>();
	}

	/**
	 * Creates a rig control value from a FVector2D value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromVector2D(FVector2D InValue)
	{
		return FRigControlValue::Make<FVector3f>(FVector3f(InValue.X, InValue.Y, 0.f));
	}

	/**
	 * Returns the contained FVector2D value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FVector2D value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FVector2D GetVector2DFromControlValue(FRigControlValue InValue)
	{
		const FVector3f Vector = InValue.Get<FVector3f>();
		return FVector2D(Vector.X, Vector.Y);
	}

	/**
	 * Creates a rig control value from a FVector value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromVector(FVector InValue)
	{
		return FRigControlValue::Make<FVector>(InValue);
	}

	/**
	 * Returns the contained FVector value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FVector value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FVector GetVectorFromControlValue(FRigControlValue InValue)
	{
		return (FVector)InValue.Get<FVector3f>();
	}

	/**
	 * Creates a rig control value from a FRotator value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromRotator(FRotator InValue)
	{
		return FRigControlValue::Make<FVector>(InValue.Euler());
	}

	/**
	 * Returns the contained FRotator value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FRotator value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FRotator GetRotatorFromControlValue(FRigControlValue InValue)
	{
		return FRotator::MakeFromEuler((FVector)InValue.Get<FVector3f>());
	}

	/**
	 * Creates a rig control value from a FTransform value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromTransform(FTransform InValue)
	{
		return FRigControlValue::Make<FRigControlValue::FTransform_Float>(InValue);
	}

	/**
	 * Returns the contained FTransform value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FTransform value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FTransform GetTransformFromControlValue(FRigControlValue InValue)
	{
		return InValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
	}

	/**
	 * Creates a rig control value from a FEulerTransform value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromEulerTransform(FEulerTransform InValue)
	{
		return FRigControlValue::Make<FRigControlValue::FEulerTransform_Float>(InValue);
	}

	/**
	 * Returns the contained FEulerTransform value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FEulerTransform value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FEulerTransform GetEulerTransformFromControlValue(FRigControlValue InValue)
	{
		return InValue.Get<FRigControlValue::FEulerTransform_Float>().ToTransform();
	}

	/**
	 * Creates a rig control value from a FTransformNoScale value
	 * @param InValue The value to create the rig control value from
	 * @return The converted control rig val ue
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
    static FORCEINLINE FRigControlValue MakeControlValueFromTransformNoScale(FTransformNoScale InValue)
	{
		return FRigControlValue::Make<FRigControlValue::FTransformNoScale_Float>(InValue);
	}

	/**
	 * Returns the contained FTransformNoScale value from a a Rig Control Value
	 * @param InValue The Rig Control value to convert from
	 * @return The converted FTransformNoScale value
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	static FORCEINLINE FTransformNoScale GetTransformNoScaleFromControlValue(FRigControlValue InValue)
	{
		return InValue.Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
	}

private:

	FRigHierarchyModifiedEvent ModifiedEvent;
	FRigHierarchyMetadataChangedDelegate MetadataChangedDelegate;
	FRigHierarchyMetadataTagChangedDelegate MetadataTagChangedDelegate;
	FRigEventDelegate EventDelegate;

public:

	void Notify(ERigHierarchyNotification InNotifType, const FRigBaseElement* InElement);

	/**
	 * Returns a transform based on a given transform type
	 * @param InTransformElement The element to retrieve the transform for
	 * @param InTransformType The type of transform to retrieve
	 * @return The local current or initial transform's value.
	 */
	FTransform GetTransform(FRigTransformElement* InTransformElement, const ERigTransformType::Type InTransformType) const;

	/**
	 * Returns a transform for a given element's parent based on the transform type
	 * If the element does not have a parent FTransform::Identity will be returned.
	 * @param InElement The element to retrieve the transform for
	 * @param InTransformType The type of transform to retrieve
	 * @return The element's parent's transform
	 */
	FTransform GetParentTransform(FRigBaseElement* InElement, const ERigTransformType::Type InTransformType) const;

	/**
	 * Sets a transform for a given element based on the transform type
	 * @param InTransformElement The element to set the transform for
	 * @param InTransform The type of transform to set
	 * @param InTransformType The type of transform to set
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Set the transform even if it is the same as the previously set one
	 */
	void SetTransform(FRigTransformElement* InTransformElement, const FTransform& InTransform, const ERigTransformType::Type InTransformType, bool bAffectChildren, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false);

	/**
	 * Returns the global offset transform for a given control element.
	 * @param InControlElement The control element to retrieve the offset transform for
	 * @param InTransformType The type of transform to set
	 * @return The global offset transform
	 */
	FTransform GetControlOffsetTransform(FRigControlElement* InControlElement, const ERigTransformType::Type InTransformType) const;

	/**
	 * Sets the offset transform for a given control element
	 * @param InControlElement The element to set the transform for
	 * @param InTransform The offset transform to set
	 * @param InTransformType The type of transform to set. Note: for offset transform, setting the initial value also updates the current value
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Set the transform even if it is the same as the previously set one
	 */
	void SetControlOffsetTransform(FRigControlElement* InControlElement, const FTransform& InTransform, const ERigTransformType::Type InTransformType, bool bAffectChildren, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false);

	/**
	 * Returns the global shape transform for a given control element.
	 * @param InControlElement The control element to retrieve the shape transform for
	 * @param InTransformType The type of transform to set
	 * @return The global shape transform
	 */
	FTransform GetControlShapeTransform(FRigControlElement* InControlElement, const ERigTransformType::Type InTransformType) const;

	/**
	 * Sets the shape transform for a given control element
	 * @param InControlElement The element to set the transform for
	 * @param InTransform The shape transform to set
	 * @param InTransformType The type of transform to set. Note: for shape transform, setting the initial value also updates the current value
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Set the transform even if it is the same as the previously set one
	 */
	void SetControlShapeTransform(FRigControlElement* InControlElement, const FTransform& InTransform, const ERigTransformType::Type InTransformType, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false);

	/**
	 * Sets the control settings for a given control element
	 * @param InControlElement The element to set the settings for
	 * @param InSettings The new control settings value to set
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 */
	void SetControlSettings(FRigControlElement* InControlElement, FRigControlSettings InSettings, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false);

	/**
	 * Returns a control's current value
	 * @param InControlElement The element to retrieve the current value for
	 * @param InValueType The type of value to return
	 * @return Returns the current value of the control
	 */
	FRigControlValue GetControlValue(FRigControlElement* InControlElement, ERigControlValueType InValueType) const;

	template<typename T>
	FORCEINLINE_DEBUGGABLE T GetControlValue(FRigControlElement* InControlElement, ERigControlValueType InValueType) const
	{
		return GetControlValue(InControlElement, InValueType).Get<T>();
	}

	/**
	 * Sets a control's current value
	 * @param InControlElement The element to set the current value for
	 * @param InValueType The type of value to set
	 * @param InValue The value to set on the control
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Set the transform even if it is the same as the previously set one
	 */
	void SetControlValue(FRigControlElement* InControlElement, const FRigControlValue& InValue, ERigControlValueType InValueType, bool bSetupUndo = false, bool bForce = false, bool bPrintPythonCommands = false, bool bFixEulerFlips = false);

	template<typename T>
	FORCEINLINE_DEBUGGABLE void SetControlValue(FRigControlElement* InControlElement, const T& InValue, ERigControlValueType InValueType, bool bSetupUndo = false, bool bForce = false) const
	{
		SetControlValue(InControlElement, FRigControlValue::Make<T>(InValue), InValueType, bSetupUndo, bForce);
	}

	/**
	 * Sets a control's current visibility
	 * @param InControlElement The element to set the visibility for
	 * @param bVisibility The new visibility for the control
	 */
	void SetControlVisibility(FRigControlElement* InControlElement, bool bVisibility);

	/**
	 * Returns a curve's value. If the curve value is not set, returns 
	 * @param InCurveElement The element to retrieve the value for
	 * @return Returns the value of the curve
	 */
	float GetCurveValue(FRigCurveElement* InCurveElement) const;

	/**
	 * Returns whether a curve's value is set. If the curve value is not set, returns false. 
	 * @param InCurveElement The element to retrieve the value for
	 * @return Returns true if the value is set, false otherwise.
	 */
	bool IsCurveValueSet(FRigCurveElement* InCurveElement) const;

	/**
	 * Sets a curve's value
	 * @param InCurveElement The element to set the value for
	 * @param InValue The value to set on the control
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Set the transform even if it is the same as the previously set one
	 */
	void SetCurveValue(FRigCurveElement* InCurveElement, float InValue, bool bSetupUndo = false, bool bForce = false);

	/**
	 * Unsets a curve's value. Basically the curve's value becomes meaningless.
	 * @param InCurveElement The element to set the value for
	 * @param bSetupUndo If true the transform stack will be setup for undo / redo
	 * @param bForce Unset the curve even if it was already unset.
	 */
	void UnsetCurveValue(FRigCurveElement* InCurveElement, bool bSetupUndo = false, bool bForce = false);

	/**
	 * Returns the previous name of an element prior to a rename operation
	 * @param InKey The key of the element to request the old name for
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FName GetPreviousName(const FRigElementKey& InKey) const;

	/**
	 * Returns the previous parent of an element prior to a reparent operation
	 * @param InKey The key of the element to request the old parent  for
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchy)
	FRigElementKey GetPreviousParent(const FRigElementKey& InKey) const;

	/**
	 * Returns true if an element is parented to another element
	 * @param InChild The child element to check for a parent
	 * @param InParent The parent element to check for
	 * @param InDependencyMap An additional map of dependencies to respect
	 * @return True if the child is parented to the parent
	 */
	bool IsParentedTo(FRigBaseElement* InChild, FRigBaseElement* InParent, const TElementDependencyMap& InDependencyMap = TElementDependencyMap()) const;

	/**
	 * Returns true if an element is affected to another element
	 * @param InDependent The dependent element to check for a dependency
	 * @param InDependency The dependency element to check for
	 * @param InElementsVisited An array to keep track of whether an element is visited to avoid infinite recursion
	 * @param InDependencyMap An additional map of dependencies to respect
	 * @return True if the child is parented to the parent
	 */
	bool IsDependentOn(FRigBaseElement* InDependent, FRigBaseElement* InDependency, TArray<bool>& InElementsVisited, const TElementDependencyMap& InDependencyMap = TElementDependencyMap()) const;

	/**
	 * Returns a reference to the suspend notifications flag
	 */
	FORCEINLINE bool& GetSuspendNotificationsFlag() { return bSuspendNotifications; }

	/*
	 * Returns true if a hierarchy will record any change.
	 * This is used for debugging purposes.
	 */
	bool IsTracingChanges() const;

	/**
	 * Returns true if the control is animatable
	 */
	bool IsAnimatable(const FRigElementKey& InKey) const;

	/**
	 * Returns true if the control is animatable
	 */
	bool IsAnimatable(const FRigControlElement* InControlElement) const;

	/**
	 * Returns true if the control should be grouped in editor
	 */
	bool ShouldBeGrouped(const FRigElementKey& InKey) const;

	/**
	 * Returns true if the control should be grouped in editor
	 */
	bool ShouldBeGrouped(const FRigControlElement* InControlElement) const;

#if WITH_EDITOR

	/**
	 * Clears the undo / redo stack of this hierarchy
	 */
	void ResetTransformStack();

	/**
	 * Stores the current pose for tracing
	 */
	void StorePoseForTrace(const FString& InPrefix);

	/**
	 * Updates the format for trace floating point numbers
	 */
	static void CheckTraceFormatIfRequired();
	
	/**
	 * Dumps the content of the transform stack to a string
	 */
	void DumpTransformStackToFile(FString* OutFilePath = nullptr);

	/**
	 * Tells this hierarchy to trace a series of frames
	 */
	void TraceFrames(int32 InNumFramesToTrace);
	
#endif

private:

	/**
	 * Returns true if a given element is selected
	 * @param InElement The element to check
	 * @return true if a given element is selected
	 */
    bool IsSelected(const FRigBaseElement* InElement) const;

	/**
	 * Removes the transient cached children table for all elements.
	 */
	void ResetCachedChildren();

	/**
	 * Updates the transient cached children table for a given element if needed (or if bForce == true).
	 * @param InElement The element to update the children table for
	 * @param bForce If set to true the table will always be updated
	 */
	void UpdateCachedChildren(const FRigBaseElement* InElement, bool bForce = false) const;

	/**
	* Updates the transient cached children table for all elements if needed (or if bForce == true).
	* @param bForce If set to true the table will always be updated
	*/
	void UpdateAllCachedChildren() const;

	/**
	 * Corrects a parent element key for space switching
	 */
	FRigElementKey PreprocessParentElementKeyForSpaceSwitching(const FRigElementKey& InChildKey, const FRigElementKey& InParentKey);

	/*
	 * Helper function to create an element for a given type
	 */
	FRigBaseElement* MakeElement(ERigElementType InElementType, int32 InCount = 1, int32* OutStructureSize = nullptr); 

	/*
	* Helper function to create an element for a given type
	*/
	void DestroyElement(FRigBaseElement*& InElement);

	/*
	 * Templated helper function to create an element
	 */
	template<typename ElementType = FRigBaseElement>
	FORCEINLINE ElementType* NewElement(int32 Num = 1)
	{
		ElementType* NewElements = (ElementType*)FMemory::Malloc(sizeof(ElementType) * Num);
		for(int32 Index=0;Index<Num;Index++)
		{
			new(&NewElements[Index]) ElementType();
			NewElements[Index].MetadataChangedDelegate.BindStatic(&URigHierarchy::OnMetadataChanged_Static, this);
			NewElements[Index].MetadataTagChangedDelegate.BindStatic(&URigHierarchy::OnMetadataTagChanged_Static, this);
		}
		NewElements[0].OwnedInstances = Num;
		return NewElements;
	}

	/**
	 * Marks all affected elements of a given element as dirty
	 * @param InTransformElement The element that has changed
	 * @param bInitial If true the initial transform will be dirtied
	 * @param bAffectChildren If set to false children will not move (maintain global).
	 */
	void PropagateDirtyFlags(FRigTransformElement* InTransformElement, bool bInitial, bool bAffectChildren, bool bComputeOpposed = true, bool bMarkDirty = true) const;

public:

	/**
	* Performs validation of the cache within the hierarchy on any mutation.
	*/
	FORCEINLINE void EnsureCacheValidity() const
	{
#if WITH_EDITOR
		if(bEnableCacheValidityCheck)
		{
			URigHierarchy* MutableThis = (URigHierarchy*)this; 
			MutableThis->EnsureCacheValidityImpl();
		}
#endif
	}

	/*
	 * Cleans up caches after load
	 */
	void CleanupInvalidCaches();

private:
	
	/**
	 * The topology version of the hierarchy changes when elements are
	 * added, removed, re-parented or renamed.
	 */
	UPROPERTY(transient)
	uint16 TopologyVersion;

	/**
	 * The metadata version of the hierarchy changes when metadata is being
	 * created or removed (not when the metadata values changes)
	 */
	UPROPERTY(transient)
	uint16 MetadataVersion;

	/**
	 * The metadata version of the hierarchy changes when metadata is being
	 * created or removed (not when the metadata values changes)
	 */
	UPROPERTY(transient)
	uint16 MetadataTagVersion;

	/**
	 * If set to false the dirty flag propagation will be disabled
	 */
	UPROPERTY(transient)
	bool bEnableDirtyPropagation;

	// Storage for the elements
	mutable TArray<FRigBaseElement*> Elements;

	// Storage for the elements
	mutable TArray<TArray<FRigBaseElement*>> ElementsPerType;

	// Managed lookup from Key to Index
	TMap<FRigElementKey, int32> IndexLookup;

	// Static empty element array used for ref returns
	static const FRigBaseElementChildrenArray EmptyElementArray;

	///////////////////////////////////////////////
	/// Undo redo related
	///////////////////////////////////////////////

	/**
	 * The index identifying where we stand with the stack
	 */
	UPROPERTY()
	int32 TransformStackIndex;

	/**
	 * A flag to indicate if the next serialize should contain only transform changes
	 */
	bool bTransactingForTransformChange;
	
	/**
	 * The stack of actions to undo.
	 * Note: This is also used when performing traces on the hierarchy.
	 */
	TArray<FRigTransformStackEntry> TransformUndoStack;

	/**
	 * The stack of actions to undo
	 */
	TArray<FRigTransformStackEntry> TransformRedoStack;

	/**
	 * Sets the transform stack index - which in turns performs a series of undos / redos
	 * @param InTransformStackIndex The new index for the transform stack
	 */
	bool SetTransformStackIndex(int32 InTransformStackIndex);

	/**
	 * Stores a transform on the stack
	 */
	void PushTransformToStack(
			const FRigElementKey& InKey,
            ERigTransformStackEntryType InEntryType,
            ERigTransformType::Type InTransformType,
            const FTransform& InOldTransform,
            const FTransform& InNewTransform,
            bool bAffectChildren,
            bool bModify);

	/**
	 * Stores a curve value on the stack
	 */
	void PushCurveToStack(
            const FRigElementKey& InKey,
            float InOldCurveValue,
            float InNewCurveValue,
            bool bInOldIsCurveValueSet,
            bool bInNewIsCurveValueSet,
            bool bModify);

	/**
	 * Restores a transform on the stack
	 */
	bool ApplyTransformFromStack(const FRigTransformStackEntry& InEntry, bool bUndo);

	/**
	 * Computes all parts of the pose
	 */
	void ComputeAllTransforms();

	/**
	 * Manages merging transform actions into one during an interaction
	 */
	bool bIsInteracting;

	/**
	* Stores the last key being interacted on
	*/
	FRigElementKey LastInteractedKey;

	/** 
	 * If set to true all notifs coming from this hierarchy will be suspended
	 */
	bool bSuspendNotifications;

	/**
	 * The event fired during undo / redo
	 */
	FRigHierarchyUndoRedoTransformEvent UndoRedoEvent;

	TWeakObjectPtr<URigHierarchy> HierarchyForSelectionPtr;
	TWeakObjectPtr<URigHierarchy> DefaultHierarchyPtr;
	TArray<FRigElementKey> OrderedSelection;

	UPROPERTY(Transient)
	TObjectPtr<URigHierarchyController> HierarchyController;
	bool bIsControllerAvailable;

	TMap<FRigElementKey, FRigElementKey> PreviousParentMap;

	/*We save this so Sequencer can remap this after load*/
	UPROPERTY()
	TMap<FRigElementKey, FRigElementKey> PreviousNameMap;

	int32 ResetPoseHash;
	TArray<bool> ResetPoseIsFilteredOut;
	TArray<int32> ElementsToRetainLocalTransform;
	
#if WITH_EDITOR

	// this is mainly used for propagating changes between hierarchies in the direction of blueprint -> CDO -> other instances
	struct FRigHierarchyListener
	{
		FRigHierarchyListener()
			: Hierarchy(nullptr)
			, bShouldReactToInitialChanges(true)
			, bShouldReactToCurrentChanges(true) 
		{}

		bool ShouldReactToChange(ERigTransformType::Type InTransformType) const
		{
			if(Hierarchy.IsValid())
			{
				if(ERigTransformType::IsInitial(InTransformType))
				{
					return bShouldReactToInitialChanges;
				}

				if(ERigTransformType::IsCurrent(InTransformType))
				{
					return bShouldReactToCurrentChanges;
				}
			}
			return false;
		}
		
		TWeakObjectPtr<URigHierarchy> Hierarchy;
		bool bShouldReactToInitialChanges;
		bool bShouldReactToCurrentChanges;
	};
	
	TArray<FRigHierarchyListener> ListeningHierarchies;
	friend class FRigHierarchyListenerGuard;

	// a bool to guard against circular dependencies among listening hierarchies
	bool bPropagatingChange;

	// a bool to disable any propagation checks and force propagation
	bool bForcePropagation;
	
#endif

#if WITH_EDITOR

protected:
	
	int32 TraceFramesLeft;
	int32 TraceFramesCaptured;
	TMap<FName, FRigPose> TracePoses;

#endif

	FORCEINLINE static int32 RigElementTypeToFlatIndex(ERigElementType InElementType)
	{
		switch(InElementType)
		{
			case ERigElementType::Bone:
			{
				return 0;
			}
			case ERigElementType::Null:
			{
				return 1;
			}
			case ERigElementType::Control:
			{
				return 2;
			}
			case ERigElementType::Curve:
			{
				return 3;
			}
			case ERigElementType::RigidBody:
			{
				return 4;
			}
			case ERigElementType::Reference:
			{
				return 5;
			}
			case ERigElementType::Last:
			{
				return 6;
			}
			case ERigElementType::All:
			default:
			{
				checkNoEntry();
				break;
			}
		}

		return INDEX_NONE;
	}

	FORCEINLINE static ERigElementType FlatIndexToRigElementType(int32 InIndex)
	{
		switch(InIndex)
		{
			case 0:
			{
				return ERigElementType::Bone;
			}
			case 1:
			{
				return ERigElementType::Null;
			}
			case 2:
			{
				return ERigElementType::Control;
			}
			case 3:
			{
				return ERigElementType::Curve;
			}
			case 4:
			{
				return ERigElementType::RigidBody;
			}
			case 5:
			{
				return ERigElementType::Reference;
			}
			case 6:
			{
				return ERigElementType::Last;
			}
			default:
			{
				checkNoEntry();
				break;
			}
		}

		return ERigElementType::None;
	}

public:

	const FRigElementKeyCollection* FindCachedCollection(uint32 InHash) const { return KeyCollectionCache.Find(InHash); }
	FRigElementKeyCollection& FindOrAddCachedCollection(uint32 InHash) const { return KeyCollectionCache.FindOrAdd(InHash); };
	void AddCachedCollection(uint32 InHash, const FRigElementKeyCollection& InCollection) const { KeyCollectionCache.Add(InHash, InCollection); }
	
private:
	
	mutable TMap<uint32, FRigElementKeyCollection> KeyCollectionCache;

	FTransform GetWorldTransformForReference(const FRigUnitContext* InContext, const FRigElementKey& InKey, bool bInitial);
	
	FORCEINLINE static float GetWeightForLerp(const float WeightA, const float WeightB)
	{
		float Weight = 0.f;
		const float ClampedWeightA = FMath::Max(WeightA, 0.f);
		const float ClampedWeightB = FMath::Max(WeightB, 0.f);
		const float OverallWeight = ClampedWeightA + ClampedWeightB;
		if(OverallWeight > SMALL_NUMBER)
		{
			Weight = ClampedWeightB / OverallWeight;
		}
		return Weight;
	}

	struct FConstraintIndex
	{
		int32 Location;
		int32 Rotation;
		int32 Scale;

		FConstraintIndex()
			: Location(INDEX_NONE)
			, Rotation(INDEX_NONE)
			, Scale(INDEX_NONE)
		{}

		FConstraintIndex(int32 InIndex)
			: Location(InIndex)
			, Rotation(InIndex)
			, Scale(InIndex)
		{}
	};

	FTransform ComputeLocalControlValue(
		FRigControlElement* ControlElement,
		const FTransform& InGlobalTransform,
		ERigTransformType::Type InTransformType) const;
	
	FTransform SolveParentConstraints(
		const FRigElementParentConstraintArray& InConstraints,
		const ERigTransformType::Type InTransformType,
		const FTransform& InLocalOffsetTransform,
		bool bApplyLocalOffsetTransform,
		const FTransform& InLocalPoseTransform,
		bool bApplyLocalPoseTransform) const;

	FTransform InverseSolveParentConstraints(
		const FTransform& InGlobalTransform,
		const FRigElementParentConstraintArray& InConstraints,
		const ERigTransformType::Type InTransformType,
		const FTransform& InLocalOffsetTransform) const;

	FTransform LazilyComputeParentConstraint(
		const FRigElementParentConstraintArray& Constraints,
		int32 InIndex,
		const ERigTransformType::Type InTransformType,
		const FTransform& InLocalOffsetTransform,
		bool bApplyLocalOffsetTransform,
		const FTransform& InLocalPoseTransform,
		bool bApplyLocalPoseTransform) const;

	static void ComputeParentConstraintIndices(
		const FRigElementParentConstraintArray& InConstraints,
		ERigTransformType::Type InTransformType,
		FConstraintIndex& OutFirstConstraint,
		FConstraintIndex& OutSecondConstraint,
		FConstraintIndex& OutNumConstraintsAffecting,
		FRigElementWeight& OutTotalWeight
	);

	static void IntegrateParentConstraintVector(
		FVector& OutVector,
		const FTransform& InTransform,
		float InWeight,
		bool bIsLocation);

	static void IntegrateParentConstraintQuat(
		int32& OutNumMixedRotations,
		FQuat& OutFirstRotation,
		FQuat& OutMixedRotation,
		const FTransform& InTransform,
		float InWeight);

#if WITH_EDITOR
	static TArray<FString> ControlSettingsToPythonCommands(const FRigControlSettings& Settings, const FString& NameSettings);
#endif

	template<typename T>
	FORCEINLINE const T& GetMetadata(const FRigElementKey& InItem, ERigMetadataType InType, const FName& InMetadataName, const T& DefaultValue) const
	{
		return GetMetadata<T>(Find(InItem), InType, InMetadataName, DefaultValue);
	}

	template<typename T>
	FORCEINLINE const T& GetMetadata(const FRigBaseElement* InElement, ERigMetadataType InType, const FName& InMetadataName, const T& DefaultValue) const
	{
		if(InElement)
		{
			if(FRigBaseMetadata* Metadata = InElement->GetMetadata(InMetadataName, InType))
			{
				return *(const T*)Metadata->GetValueData();
			}
		}
		return DefaultValue;
	}

	template<typename T>
	FORCEINLINE const TArray<T>& GetArrayMetadata(const FRigElementKey& InItem, ERigMetadataType InType, const FName& InMetadataName) const
	{
		return GetArrayMetadata<T>(Find(InItem), InType, InMetadataName);
	}

	template<typename T>
	FORCEINLINE const TArray<T>& GetArrayMetadata(const FRigBaseElement* InElement, ERigMetadataType InType, const FName& InMetadataName) const
	{
		static const TArray<T> EmptyArray;
		return GetMetadata<TArray<T>>(InElement, InType, InMetadataName, EmptyArray);
	}

	template<typename T>
	FORCEINLINE bool SetMetadata(const FRigElementKey& InItem, ERigMetadataType InType, const FName& InMetadataName, const T& InValue)
	{
		return SetMetadata<T>(Find(InItem), InType, InMetadataName, InValue);
	}

	template<typename T>
	FORCEINLINE bool SetMetadata(FRigBaseElement* InElement, ERigMetadataType InType, const FName& InMetadataName, const T& InValue)
	{
		if(InElement)
		{
			return InElement->SetMetaData(InMetadataName, InType, &InValue, sizeof(T));
		}
		return false;
	}

	template<typename T>
	FORCEINLINE bool SetArrayMetadata(const FRigElementKey& InItem, ERigMetadataType InType, const FName& InMetadataName, const TArray<T>& InValue)
	{
		return SetMetadata<TArray<T>>(Find(InItem), InType, InMetadataName, InValue);
	}

	template<typename T>
	FORCEINLINE bool SetArrayMetadata(FRigBaseElement* InElement, ERigMetadataType InType, const FName& InMetadataName, const TArray<T>& InValue)
	{
		return SetMetadata<TArray<T>>(InElement, InType, InMetadataName, InValue);
	}

	void OnMetadataChanged(const FRigElementKey& InKey, const FName& InName);
	void OnMetadataTagChanged(const FRigElementKey& InKey, const FName& InTag, bool bAdded);

protected:

	FORCEINLINE static void OnMetadataChanged_Static(const FRigElementKey& InKey, const FName& InName, URigHierarchy* InHierarchy)
	{
		check(InHierarchy);
		check(IsValid(InHierarchy));
		InHierarchy->OnMetadataChanged(InKey, InName);
	}
	FORCEINLINE static void OnMetadataTagChanged_Static(const FRigElementKey& InKey, const FName& InTag, bool bAdded, URigHierarchy* InHierarchy)
	{
		check(InHierarchy);
		check(IsValid(InHierarchy));
		InHierarchy->OnMetadataTagChanged(InKey, InTag, bAdded);
	}
	
	bool bEnableCacheValidityCheck;

	static bool bEnableValidityCheckbyDefault;

	UPROPERTY(transient)
	TObjectPtr<URigHierarchy> HierarchyForCacheValidation;

	mutable TMap<FRigElementKey, FRigElementKey> DefaultParentPerElement;

	bool bUpdatePreferedEulerAngleWhenSettingTransform;
	
private:
	
	void EnsureCacheValidityImpl();

	const FRigVMExtendedExecuteContext* ExecuteContext;

#if WITH_EDITOR
	mutable bool bRecordTransformsAtRuntime;
	mutable TArray<TInstructionSliceElement> ReadTransformsAtRuntime;
	mutable TArray<TInstructionSliceElement> WrittenTransformsAtRuntime;
public:

	TElementDependencyMap GetDependenciesForVM(const URigVM* InVM, FName InEventName = NAME_None) const;

private:
	
#endif

	void UpdateVisibilityOnProxyControls();

	static const TArray<FString>& GetTransformTypeStrings();

	struct FQueuedNotification
	{
		ERigHierarchyNotification Type;
		FRigElementKey Key;
		
		FORCEINLINE bool operator == (const FQueuedNotification& InOther) const
		{
			return Type == InOther.Type && Key == InOther.Key;
		}
	};
	TQueue<FQueuedNotification> QueuedNotifications;

	void QueueNotification(ERigHierarchyNotification InNotification, const FRigBaseElement* InElement);
	void SendQueuedNotifications();
	
	friend class URigHierarchyController;
	friend class UControlRig;
	friend class FControlRigEditor;
	friend struct FRigHierarchyValidityBracket;
	friend struct FRigHierarchyGlobalValidityBracket;
	friend struct FControlRigVisualGraphUtils;
	friend struct FRigHierarchyEnableControllerBracket;
	friend struct FRigHierarchyExecuteContextBracket;
};

struct CONTROLRIG_API FRigHierarchyInteractionBracket
{
public:
	
	FRigHierarchyInteractionBracket(URigHierarchy* InHierarchy)
		: Hierarchy(InHierarchy)
	{
		check(Hierarchy);
		Hierarchy->Notify(ERigHierarchyNotification::InteractionBracketOpened, nullptr);
	}

	~FRigHierarchyInteractionBracket()
	{
		Hierarchy->Notify(ERigHierarchyNotification::InteractionBracketClosed, nullptr);
	}

private:

	URigHierarchy* Hierarchy;
};

struct CONTROLRIG_API FRigHierarchyEnableControllerBracket
{
private:
	FRigHierarchyEnableControllerBracket(URigHierarchy* InHierarchy, bool bEnable)
		: GuardIsControllerAvailable(InHierarchy->bIsControllerAvailable, bEnable)
	{
	}

	friend class URigHierarchy;
	friend class UControlRig;

	// certain units are allowed to use this
	friend struct FRigUnit_AddParent;
	friend struct FRigUnit_SetDefaultParent;

private:
	TGuardValue<bool> GuardIsControllerAvailable;
};

struct CONTROLRIG_API FRigHierarchyExecuteContextBracket
{
private:

	FRigHierarchyExecuteContextBracket(URigHierarchy* InHierarchy, const FRigVMExtendedExecuteContext* InContext)
		: Hierarchy(InHierarchy)
		, PreviousContext(InHierarchy->ExecuteContext)
	{
		Hierarchy->ExecuteContext = InContext;
	}

	~FRigHierarchyExecuteContextBracket()
	{
		Hierarchy->ExecuteContext = PreviousContext;
		Hierarchy->SendQueuedNotifications();
	}

	URigHierarchy* Hierarchy;
	const FRigVMExtendedExecuteContext* PreviousContext;

	friend class UControlRig;
};

struct CONTROLRIG_API FRigHierarchyValidityBracket
{
	public:
	FRigHierarchyValidityBracket(URigHierarchy* InHierarchy)
		: bPreviousValue(false)
		, HierarchyPtr() 
	{
		if(InHierarchy)
		{
			bPreviousValue = InHierarchy->bEnableCacheValidityCheck;
			InHierarchy->bEnableCacheValidityCheck = false;
			HierarchyPtr = InHierarchy;
		}
	}

	~FRigHierarchyValidityBracket()
	{
		if(HierarchyPtr.IsValid())
		{
			URigHierarchy* Hierarchy = HierarchyPtr.Get();
			Hierarchy->bEnableCacheValidityCheck = bPreviousValue;
			Hierarchy->EnsureCacheValidity();
		}
	}

	private:

	bool bPreviousValue;
	TWeakObjectPtr<URigHierarchy> HierarchyPtr;
};

struct CONTROLRIG_API FRigHierarchyGlobalValidityBracket
{
public:
	FRigHierarchyGlobalValidityBracket(bool bEnable = true)
		: bPreviousValue(URigHierarchy::bEnableValidityCheckbyDefault)
	{
		URigHierarchy::bEnableValidityCheckbyDefault = true;
	}

	~FRigHierarchyGlobalValidityBracket()
	{
		URigHierarchy::bEnableValidityCheckbyDefault = bPreviousValue;
	}

private:

	bool bPreviousValue;
};

template<>
FORCEINLINE_DEBUGGABLE FVector2D URigHierarchy::GetControlValue(FRigControlElement* InControlElement, ERigControlValueType InValueType) const
{
	const FVector3f Value = GetControlValue(InControlElement, InValueType).Get<FVector3f>();
	return FVector2D(Value.X, Value.Y);
}

template<>
FORCEINLINE_DEBUGGABLE void URigHierarchy::SetControlValue(int32 InElementIndex, const FVector2D& InValue, ERigControlValueType InValueType, bool bSetupUndo) const
{
	return SetControlValue(InElementIndex, FRigControlValue::Make<FVector3f>(FVector3f(InValue.X, InValue.Y, 0.f)), InValueType, bSetupUndo);
}

#if WITH_EDITOR

class FRigHierarchyListenerGuard
{
public:
	FORCEINLINE_DEBUGGABLE FRigHierarchyListenerGuard(
		URigHierarchy* InHierarchy, 
		bool bInEnableInitialChanges, 
		bool bInEnableCurrentChanges,
		URigHierarchy* InListeningHierarchy = nullptr)
			: Hierarchy(InHierarchy)
			, bEnableInitialChanges(bInEnableInitialChanges)
			, bEnableCurrentChanges(bInEnableCurrentChanges)
			, ListeningHierarchy(InListeningHierarchy)
	{
		check(Hierarchy);

		if(ListeningHierarchy == nullptr)
		{
			InitialFlags.AddUninitialized(Hierarchy->ListeningHierarchies.Num());
			CurrentFlags.AddUninitialized(Hierarchy->ListeningHierarchies.Num());

			for(int32 Index = 0; Index < Hierarchy->ListeningHierarchies.Num(); Index++)
			{
				URigHierarchy::FRigHierarchyListener& Listener = Hierarchy->ListeningHierarchies[Index];
				InitialFlags[Index] = Listener.bShouldReactToInitialChanges;
				CurrentFlags[Index] = Listener.bShouldReactToCurrentChanges;

				Listener.bShouldReactToInitialChanges = bInEnableInitialChanges; 
				Listener.bShouldReactToCurrentChanges = bInEnableCurrentChanges; 
			}
		}
		else
		{
			for(int32 Index = 0; Index < Hierarchy->ListeningHierarchies.Num(); Index++)
			{
				URigHierarchy::FRigHierarchyListener& Listener = Hierarchy->ListeningHierarchies[Index];

				if(Listener.Hierarchy.Get() == ListeningHierarchy)
				{
					InitialFlags.Add(Listener.bShouldReactToInitialChanges);
					CurrentFlags.Add(Listener.bShouldReactToCurrentChanges);

					Listener.bShouldReactToInitialChanges = bInEnableInitialChanges; 
					Listener.bShouldReactToCurrentChanges = bInEnableCurrentChanges;
					break;
				}
			}
		}
	}

	FORCEINLINE_DEBUGGABLE ~FRigHierarchyListenerGuard()
	{
		if(ListeningHierarchy == nullptr)
		{
			check(Hierarchy->ListeningHierarchies.Num() == InitialFlags.Num());
			check(Hierarchy->ListeningHierarchies.Num() == CurrentFlags.Num());
			
			for(int32 Index = 0; Index < Hierarchy->ListeningHierarchies.Num(); Index++)
			{
				URigHierarchy::FRigHierarchyListener& Listener = Hierarchy->ListeningHierarchies[Index];
				Listener.bShouldReactToInitialChanges = InitialFlags[Index];
				Listener.bShouldReactToCurrentChanges = CurrentFlags[Index];
			}
		}
		else
		{
			for(int32 Index = 0; Index < Hierarchy->ListeningHierarchies.Num(); Index++)
			{
				URigHierarchy::FRigHierarchyListener& Listener = Hierarchy->ListeningHierarchies[Index];

				if(Listener.Hierarchy.Get() == ListeningHierarchy)
				{
					check(InitialFlags.Num() == 1);
					check(CurrentFlags.Num() == 1);

					Listener.bShouldReactToInitialChanges = InitialFlags[0];
					Listener.bShouldReactToCurrentChanges = CurrentFlags[0];
					break;
				}
			}
		}
	}

private:

	URigHierarchy* Hierarchy; 
	bool bEnableInitialChanges; 
	bool bEnableCurrentChanges;
	URigHierarchy* ListeningHierarchy;
	TArray<bool> InitialFlags;
	TArray<bool> CurrentFlags;
};

#endif