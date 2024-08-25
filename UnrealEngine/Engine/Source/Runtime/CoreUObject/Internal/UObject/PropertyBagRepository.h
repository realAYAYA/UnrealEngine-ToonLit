// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "UObject/GCObject.h"

class UObjectBase;
class UObject;

namespace UE
{

class FPropertyBag;
class FPropertyPathName;

// Singleton class tracking property bag association with objects
class FPropertyBagRepository : public FGCObject
{
	struct FPropertyBagAssociationData
	{
		void Destroy();
		
		FPropertyBag* Bag = nullptr;		// The existence of an association implies the existence of the bag. TODO: Ref bags via handle? Store as value?
		TObjectPtr<UObject> InstanceDataObject = nullptr;
	};
	// TODO: Make private throughout and extend access permissions here or in wrapper classes? Don't want engine code modifying bags outside of serializers and details panels.
	//friend UObjectBase;
	//friend UStruct;
	friend struct FScopedInstanceDataObjectLoad;

private:
	friend class FPropertyBagRepositoryLock;
	mutable FCriticalSection CriticalSection;
	
	// Lifetimes/ownership:
	// Managed within UObjectBase and synced with object lifetime. The repo tracks pointers to bags, not the bags themselves.
	// FPropertyBag destruction handles destruction of sub-bags automatically, via the FPropertyBagProperty tracking them.

	/** Map of objects/subobjects to their top level property bag. */
	// TODO: Currently will only exist in editor world, but could tracking per world make some sense for teardown in future? We're relying on object destruction to occur properly to free these up. 
	TMap<const UObjectBase*, FPropertyBagAssociationData> AssociatedData;
	// /** Map of subobject/container/struct/etc. paths (needs FPropertyBagPath) to their property bag. Might not want to track subobjects here (they'll be in ObjectToPropertyBagMap already). */
	// TMap<FSoftObjectPath, FPropertyBag*> ObjectPathToPropertySubBagMap;
	
	//TMap<const UObjectBase*, UObject*> ObjectToInstanceDataObjectMap;

	// used to make sure IDOs don't have name overlap
	TMap<const UObjectBase*, TObjectPtr<UObject>> Namespaces;

	/** Internal registry that tracks the current set of types for property bag container objects instanced as placeholders for package exports that have invalid or missing class imports on load. */
	TUniquePtr<class FPropertyBagPlaceholderTypeRegistry> PropertyBagPlaceholderTypeRegistry;

	FPropertyBagRepository();

public:
	FPropertyBagRepository(const FPropertyBagRepository &) = delete;
	FPropertyBagRepository& operator=(const FPropertyBagRepository&) = delete;
	
	// Singleton accessor
	static COREUOBJECT_API FPropertyBagRepository& Get();

	// Reclaim space - TODO: Hook up to GC.
	void ShrinkMaps();
	
	// TODO: Restrict bag creation to actor creation and UStruct::SerializeVersionedTaggedProperties?
	// Object owner is tracked internally
	FPropertyBag* CreateOuterBag(const UObjectBase* Owner);

	// Future version for reworked InstanceDataObjects - track InstanceDataObject rather than bag (directly):
	/**
	 * Instantiate an InstanceDataObject object representing all fields within the bag, tracked against the owner object.
	 * @param Owner			- Associated in world object.
	 * @return				- Custom InstanceDataObject object, UClass derived from associated bag.
	 */
	COREUOBJECT_API UObject* CreateInstanceDataObject(const UObjectBase* Owner);

	// TODO: Restrict property bag  destruction to within UObject::BeginDestroy() & FPropertyBagProperty destructor.
	// Removes bag, InstanceDataObject, and all associated data for this object.
	void DestroyOuterBag(const UObjectBase* Owner);

	/**
	 * ReassociateObjects
	 * @param ReplacedObjects - old/new owner object pairs. Reassigns InstanceDataObjects/bags to the new owner.
	 */
	COREUOBJECT_API void ReassociateObjects(const TMap<UObject*, UObject*>& ReplacedObjects);

	/**
	 * RequiresFixup - test if InstanceDataObject properties perfectly match object instance properties. This is necessary for the object to be published in UEFN.    
	 * @param Object	- Object to test.
	 * @return			- Does the object's InstanceDataObject contain any loose properties requiring user fixup before the object may be published?
	 */
	COREUOBJECT_API bool RequiresFixup(const UObjectBase* Object) const;
	
	// Accessors
	COREUOBJECT_API bool HasBag(const UObjectBase* Owner) const;
	COREUOBJECT_API FPropertyBag* FindBag(const UObjectBase* Owner);
	COREUOBJECT_API const FPropertyBag* FindBag(const UObjectBase* Owner) const;

	COREUOBJECT_API bool HasInstanceDataObject(const UObjectBase* Owner) const;
	COREUOBJECT_API UObject* FindInstanceDataObject(const UObjectBase* Owner);
	COREUOBJECT_API const UObject* FindInstanceDataObject(const UObjectBase* Owner) const;
	
	// query whether a property in an object was set when the object was deserialized
	COREUOBJECT_API static bool WasPropertySetBySerialization(UObject* Object, const FPropertyPathName& Path);
	// query whether a property in Struct was set when the struct was deserialized
	COREUOBJECT_API static bool WasPropertySetBySerialization(const UStruct* Struct, const void* StructData, const FProperty* Property, int32 ArrayIndex = 0);

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	// End FGCObject interface

	// query for whether or not the given struct/class is a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderType(UStruct* Type);
	// query for whether or not the given object was created as a placeholder type
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObject(UObject* Object);
	// query for whether or not creating property bag placeholder objects should be allowed
	static COREUOBJECT_API bool IsPropertyBagPlaceholderObjectSupportEnabled();

	// create a new placeholder type object to swap in for a missing class/struct; this will be associated with a property bag when objects are serialized so we don't lose data
	static COREUOBJECT_API UStruct* CreatePropertyBagPlaceholderType(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags, UStruct* SuperStruct = nullptr);
	template<typename T = UObject>
	static UClass* CreatePropertyBagPlaceholderClass(UObject* Outer, UClass* Class, FName Name = NAME_None, EObjectFlags Flags = RF_NoFlags)
	{
		return Cast<UClass>(CreatePropertyBagPlaceholderType(Outer, Class, Name, Flags, T::StaticClass()));
	}

private:
	void Lock() const { CriticalSection.Lock(); }
	void Unlock() const { CriticalSection.Unlock(); }

	// Internal functions requiring the repository to be locked before being called

	// Delete owner reference and disassociate all data. Returns success.
	bool RemoveAssociationUnsafe(const UObjectBase* Owner);
	
	// Instantiate InstanceDataObject within BagData. Returns InstanceDataObject object. 
	void CreateInstanceDataObjectUnsafe(const UObjectBase* Owner, FPropertyBagAssociationData& BagData);
};

} // UE
