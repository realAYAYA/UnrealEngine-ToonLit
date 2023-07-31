// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformCrt.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Guid.h"
#include "Misc/TVariant.h"
#include "Nodes/InterchangeBaseNodeUtilities.h"
#include "Templates/SharedPointer.h"
#include "Types/AttributeStorage.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

#include "InterchangeBaseNode.generated.h"

class FArchive;
class UInterchangeBaseNode;
struct FFrame;

/**
 * Internal Helper to get set custom property for class that derive from UInterchangeBaseNode. This is use by the macro IMPLEMENT_UOD_ATTRIBUTE.
 */
namespace InterchangePrivateNodeBase
{
	/**
	 * Retrieve a custom attribute if the attribute exist
	 *
	 * @param Attributes - The attribute storage you want to query the custom attribute
	 * @param AttributeKey - The storage key for the attribute
	 * @param OperationName - The name of the operation in case there is an error
	 * @param OutAttributeValue - This is where we store the value we retrieve from the storage
	 *
	 * @return - return true if the attribute exist in the storage and was query without error.
	 *           return false if the attribute do not exist or there is an error retriving it from the Storage
	 */
	template<typename ValueType>
	bool GetCustomAttribute(const UE::Interchange::FAttributeStorage& Attributes, const UE::Interchange::FAttributeKey& AttributeKey, const FString& OperationName, ValueType& OutAttributeValue)
	{
		if (!Attributes.ContainAttribute(AttributeKey))
		{
			return false;
		}
		UE::Interchange::FAttributeStorage::TAttributeHandle<ValueType> AttributeHandle = Attributes.GetAttributeHandle<ValueType>(AttributeKey);
		if (!AttributeHandle.IsValid())
		{
			return false;
		}
		UE::Interchange::EAttributeStorageResult Result = AttributeHandle.Get(OutAttributeValue);
		if (!UE::Interchange::IsAttributeStorageResultSuccess(Result))
		{
			UE::Interchange::LogAttributeStorageErrors(Result, OperationName, AttributeKey);
			return false;
		}
		return true;
	}

	/**
	 * Add or update a custom attribute value in the specified storage
	 *
	 * @param Attributes - The attribute storage you want to add or update the custom attribute
	 * @param AttributeKey - The storage key for the attribute
	 * @param OperationName - The name of the operation in case there is an error
	 * @param AttributeValue - The value we want to add or update in the storage
	 */
	template<typename ValueType>
	bool SetCustomAttribute(UE::Interchange::FAttributeStorage& Attributes, const UE::Interchange::FAttributeKey& AttributeKey, const FString& OperationName, const ValueType& AttributeValue)
	{
		UE::Interchange::EAttributeStorageResult Result = Attributes.RegisterAttribute(AttributeKey, AttributeValue);
		if (!UE::Interchange::IsAttributeStorageResultSuccess(Result))
		{
			UE::Interchange::LogAttributeStorageErrors(Result, OperationName, AttributeKey);
			return false;
		}
		return true;
	}

	/**
	 * Finds a property by name in Outer and supports looking into FStructProperties (embedded structs) with a '.' separating the property names.
	 * 
	 * @param Container - The container for the property values. If the final property is inside a UScriptStruct, the container will be set to the UScriptStruct instance address.
	 * @param Outer - The UStruct containing the top level property.
	 * @param PropertyPath - A dot separated chain of properties. Doesn't support going through external objects.
	 * @return The Property matching the last name in the PropertyPath.
	 */
	INTERCHANGECORE_API FProperty* FindPropertyByPathChecked(TVariant<UObject*, uint8*>& Container, UStruct* Outer, FStringView PropertyPath);
}

/**
 * Use those macro to create Get/Set/ApplyToAsset for an attribute you want to support in your custom UOD node that derive from UInterchangeBaseNode.
 * The attribute key will be declare under private access modifier and the functions are declare under public access modifier.
 * So the class access modifier is public after calling this macro.
 *
 * @note - The Get will return false if the attribute was never set.
 * @note - The Set will add the attribute to the storage or update the value if the storage already has this attribute.
 * @note - The Apply will set a member variable of the AssetType instance. It will apply the value only if the storage contain the attribute key
 *
 * @param AttributeName - The name of the Get/Set functions. Example if you pass Foo you will end up with GetFoo and SetFoo function
 * @param AttributeType - This is to specify the type of the attribute. bool, float, FString... anything supported by the FAttributeStorage
 * @param AssetType - This is the asset you want to apply the storage value"
 * @param EnumType - Optional, specify it only if the AssetType member is an enum so we can type cast it in the apply function (we use uint8 to store the enum value)"
 */
#define IMPLEMENT_NODE_ATTRIBUTE_KEY(AttributeName)																				\
const UE::Interchange::FAttributeKey Macro_Custom##AttributeName##Key = UE::Interchange::FAttributeKey(TEXT(#AttributeName));


#define IMPLEMENT_NODE_ATTRIBUTE_GETTER(AttributeName, AttributeType)																					\
	FString OperationName = GetTypeName() + TEXT(".Get" #AttributeName);																				\
	return InterchangePrivateNodeBase::GetCustomAttribute<AttributeType>(*Attributes, Macro_Custom##AttributeName##Key, OperationName, AttributeValue);

#define IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(AttributeName, AttributeType)																		\
	FString OperationName = GetTypeName() + TEXT(".Set" #AttributeName);																				\
	return InterchangePrivateNodeBase::SetCustomAttribute<AttributeType>(*Attributes, Macro_Custom##AttributeName##Key, OperationName, AttributeValue);


#define INTERCHANGE_BASE_NODE_ADD_ATTRIBUTE(ValueType)																												\
	UE::Interchange::FAttributeStorage::TAttributeHandle<ValueType> Handle = RegisterAttribute<ValueType>(UE::Interchange::FAttributeKey(NodeAttributeKey), Value);	\
	return Handle.IsValid();

#define INTERCHANGE_BASE_NODE_GET_ATTRIBUTE(ValueType)																							\
	if(HasAttribute(UE::Interchange::FAttributeKey(NodeAttributeKey)))																			\
	{																																			\
		UE::Interchange::FAttributeStorage::TAttributeHandle<ValueType> Handle = GetAttributeHandle<ValueType>(UE::Interchange::FAttributeKey(NodeAttributeKey));	\
		if (Handle.IsValid())																													\
		{																																		\
			return (Handle.Get(OutValue) == UE::Interchange::EAttributeStorageResult::Operation_Success);										\
		}																																		\
	}																																			\
	return false;

//Interchange namespace
namespace UE::Interchange
{
	/**
		* Helper struct use to declare static const data we use in the UInterchangeBaseNode
		* Node that derive from UInterchangeBaseNode can also add a struct that derive from this one to add there static data
		* @note: The static data are mainly for holding Attribute keys. All attributes that are always available for a node should be in this class or a derived class.
		*/
	struct INTERCHANGECORE_API FBaseNodeStaticData
	{
		static const FAttributeKey& UniqueIDKey();
		static const FAttributeKey& DisplayLabelKey();
		static const FAttributeKey& ParentIDKey();
		static const FAttributeKey& IsEnabledKey();
		static const FAttributeKey& TargetAssetIDsKey();
		static const FAttributeKey& ClassTypeAttributeKey();
		static const FAttributeKey& AssetNameKey();
		static const FAttributeKey& NodeContainerTypeKey();
	};

} //ns UE::Interchange

UENUM(BlueprintType)
enum class EInterchangeNodeContainerType : uint8
{
	None,
	TranslatedScene,
	TranslatedAsset,
	FactoryData
};

/**
 * This struct is used to store and retrieve key value attributes. The attributes are store in a generic FAttributeStorage which serialize the value in a TArray64<uint8>
 * See UE::Interchange::EAttributeTypes to know the supported template types
 * This is an abstract class. This is the base class of the interchange node graph format, all class in this format should derive from this class
 */
UCLASS(BlueprintType)
class INTERCHANGECORE_API UInterchangeBaseNode : public UObject
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* HierarchySeparator = TEXT("\\");

	UInterchangeBaseNode();

	/**
	 * Initialize the base data of the node
	 * @param UniqueID - The uniqueId for this node
	 * @param DisplayLabel - The name of the node
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	void InitializeNode(const FString& UniqueID, const FString& DisplayLabel, const EInterchangeNodeContainerType NodeContainerType);

	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const;

	/**
	 * Icon name, use to retrieve the brush when we display the node in any UI
	 */
	virtual FName GetIconName() const;

	/**
	 * UI that inspect node attribute call this to give a readable name to attribute key
	 */
	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const;

	/**
	 * UI that inspect node attribute call this to display or hide an attribute
	 */
	virtual bool ShouldHideAttribute(const UE::Interchange::FAttributeKey& NodeAttributeKey) const;

	/**
	 * UI that inspect node attribute call this to display the attribute under the returned category
	 */
	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const;

	/**
	 * Add an attribute to the node
	 * @param NodeAttributeKey - The key of the attribute
	 * @param Value - The attribute value.
	 *
	 */
	template<typename T>
	UE::Interchange::FAttributeStorage::TAttributeHandle<T> RegisterAttribute(const UE::Interchange::FAttributeKey& NodeAttributeKey, const T& Value)
	{
		const UE::Interchange::EAttributeStorageResult Result = Attributes->RegisterAttribute(NodeAttributeKey, Value);
		
		if (IsAttributeStorageResultSuccess(Result))
		{
			return Attributes->GetAttributeHandle<T>(NodeAttributeKey);
		}
		LogAttributeStorageErrors(Result, TEXT("RegisterAttribute"), NodeAttributeKey);
		return UE::Interchange::FAttributeStorage::TAttributeHandle<T>();
	}

	/**
	 * Return true if the node contain an attribute with the specified Key
	 * @param nodeAttributeKey - The key of the searched attribute
	 *
	 */
	virtual bool HasAttribute(const UE::Interchange::FAttributeKey& NodeAttributeKey) const;

	/**
	 * This function return an attribute type for the specified Key. Return type None if the key is invalid
	 *
	 * @param NodeAttributeKey - The key of the attribute
	 */
	virtual UE::Interchange::EAttributeTypes GetAttributeType(const UE::Interchange::FAttributeKey& NodeAttributeKey) const;

	/**
	 * This function return an  attribute handle for the specified Key.
	 * If there is an issue with the KEY or storage the method will trip a check, always make sure you have a valid key before calling this
	 * 
	 * @param NodeAttributeKey - The key of the attribute
	 */
	template<typename T>
	UE::Interchange::FAttributeStorage::TAttributeHandle<T> GetAttributeHandle(const UE::Interchange::FAttributeKey& NodeAttributeKey) const
	{
		return Attributes->GetAttributeHandle<T>(NodeAttributeKey);
	}

	/**
	 * Use this function to query all the node attribute keys
	 */
	void GetAttributeKeys(TArray<UE::Interchange::FAttributeKey>& AttributeKeys) const;

	/**
	 * Remove any attribute from this node. Return false if we cannot remove it. If the attribute do not exist it will return true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool RemoveAttribute(const FName& NodeAttributeKey);

	/**
	 * Add a boolean attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddBooleanAttribute(const FName& NodeAttributeKey, const bool& Value);
	
	/**
	 * Get a boolean attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetBooleanAttribute(const FName& NodeAttributeKey, bool& OutValue) const;

	/**
	 * Add a int32 attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddInt32Attribute(const FName& NodeAttributeKey, const int32& Value);
	
	/**
	 * Get a int32 attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetInt32Attribute(const FName& NodeAttributeKey, int32& OutValue) const;

	/**
	 * Add a float attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddFloatAttribute(const FName& NodeAttributeKey, const float& Value);
	
	/**
	 * Get a float attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetFloatAttribute(const FName& NodeAttributeKey, float& OutValue) const;

	/**
	 * Add a double attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddDoubleAttribute(const FName& NodeAttributeKey, const double& Value);
	
	/**
	 * Get a double attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetDoubleAttribute(const FName& NodeAttributeKey, double& OutValue) const;

	/**
	 * Add a string attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddStringAttribute(const FName& NodeAttributeKey, const FString& Value);
	
	/**
	 * Get a string attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetStringAttribute(const FName& NodeAttributeKey, FString& OutValue) const;

	/**
	 * Add a guid attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddGuidAttribute(const FName& NodeAttributeKey, const FGuid& Value);

	/**
	 * Get a guid attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetGuidAttribute(const FName& NodeAttributeKey, FGuid& OutValue) const;

	/**
	 * Add a FLinearColor attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddLinearColorAttribute(const FName& NodeAttributeKey, const FLinearColor& Value);
	
	/**
	 * Get a FLinearColor attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetLinearColorAttribute(const FName& NodeAttributeKey, FLinearColor& OutValue) const;

	/**
	 * Add a Vector2 attribute to this node. Return false if the attribute do not exist or if we cannot add it
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddVector2Attribute(const FName& NodeAttributeKey, const FVector2f& Value);

	/**
	 * Get a Vector2 attribute from this node. Return false if the attribute do not exist
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetVector2Attribute(const FName& NodeAttributeKey, FVector2f& OutValue) const;

	template<typename AttributeType>
	AttributeType GetAttributeChecked(const FName& NodeAttributeKey) const
	{
		AttributeType Value = AttributeType();
		check(HasAttribute(UE::Interchange::FAttributeKey(NodeAttributeKey)));
		UE::Interchange::FAttributeStorage::TAttributeHandle<AttributeType> Handle = GetAttributeHandle<AttributeType>(UE::Interchange::FAttributeKey(NodeAttributeKey));
		check(Handle.IsValid());
		check(Handle.Get(Value) == UE::Interchange::EAttributeStorageResult::Operation_Success);
		return Value;
	}

	template<typename AttributeType>
	bool GetAttribute(const FName& NodeAttributeKey, AttributeType& OutValue) const
	{
		INTERCHANGE_BASE_NODE_GET_ATTRIBUTE(AttributeType);
	}
	template<typename AttributeType>
	bool SetAttribute(const FName& NodeAttributeKey, const AttributeType& Value)
	{
		INTERCHANGE_BASE_NODE_ADD_ATTRIBUTE(AttributeType);
	}

	/**
	 * Return the unique id pass in the constructor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	FString GetUniqueID() const;

	/**
	 * Return the display label.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	FString GetDisplayLabel() const;

	/**
	 * Change the display label.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetDisplayLabel(const FString& DisplayName);

	/**
	 * Return the parent unique id. In case the attribute does not exist it will return InvalidNodeUid()
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	FString GetParentUid() const;

	/**
	 * Set the parent unique id.
	 */
	UE_DEPRECATED(5.1, "This function should not be use - Use UInterchangeBaseNodeContainer::SetNodeParentUid function to create a parent hierarchy. This will ensure the container cache is always in sync")
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetParentUid(const FString& ParentUid);

	/**
	 * Get number of target assets relating to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	int32 GetTargetNodeCount() const;

	/**
	 * Get target assets relating to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	void GetTargetNodeUids(TArray<FString>& OutTargetAssets) const;

	/**
	 * Add asset node UID relating to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddTargetNodeUid(const FString& AssetUid) const;

	/**
	 * Remove asset node UID relating to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool RemoveTargetNodeUid(const FString& AssetUid) const;

	/**
	 * IsEnable true mean that the node will be import/export, if false it will be discarded.
	 * Return false if this node was disabled. Return true if the attribute is not there or if it was enabled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool IsEnabled() const;

	/**
	 * Set the IsEnable attribute to determine if this node should be part of the import/export process
	 * @param bIsEnabled - The enabled state we want to set this node. True will import/export the node, fals will not.
	 * @return true if it was able to set the attribute, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetEnabled(const bool bIsEnabled);

	/**
	 * Return the node container type which define the purpose of the node (Factory node, translated scene node or translated asset node).
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	EInterchangeNodeContainerType GetNodeContainerType() const;

	/**
	 * Return a FGuid build from the FSHA1 of all the attribute data contain in the node.
	 *
	 * @note the attribute are sorted by key when building the FSHA1 data. The hash will be deterministic for the same data whatever
	 * the order we add the attributes.
	 * This function interface is pure virtual
	 */
	virtual FGuid GetHash() const;

	/**
	 * Optional, Any node that can import/export an asset should set the proper name we will give to the asset.
	 * If the attribute was never set, it will return GetDisplayLabel.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	virtual FString GetAssetName() const;

	/**
	 * Set the name we want for the imported asset this node represent. The asset factory will call GetAssetName()
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	virtual bool SetAssetName(const FString& AssetName);

	/** Return the invalid unique ID */
	static FString InvalidNodeUid();

	/**
	 * Serialize the node, by default only the attribute storage is serialize for a node
	 */
	virtual void Serialize(FArchive& Ar) override;

	static void CompareNodeStorage(UInterchangeBaseNode* NodeA, const UInterchangeBaseNode* NodeB, TArray<UE::Interchange::FAttributeKey>& RemovedAttributes, TArray<UE::Interchange::FAttributeKey>& AddedAttributes, TArray<UE::Interchange::FAttributeKey>& ModifiedAttributes);
	
	static void CopyStorageAttributes(const UInterchangeBaseNode* SourceNode, UInterchangeBaseNode* DestinationNode, TArray<UE::Interchange::FAttributeKey>& AttributeKeys);

	static void CopyStorage(const UInterchangeBaseNode* SourceNode, UInterchangeBaseNode* DestinationNode);
	
	virtual void AppendAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
	{
		return;
	}

protected:
	/** The storage use to store the Key value attribute for this node. */
	TSharedPtr<UE::Interchange::FAttributeStorage, ESPMode::ThreadSafe> Attributes;

	bool bIsInitialized = false;

	/**
	 * This tracks the IDs of asset nodes which are the target of factories
	 */
	mutable UE::Interchange::TArrayAttributeHelper<FString> TargetNodes;
};
