// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeUtilities.h"
#include "Types/AttributeStorage.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#include "InterchangeFactoryBaseNode.generated.h"

class UObject;
struct FFrame;

#define IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(AttributeName, AttributeType, ObjectType, PropertyName)		\
	bool ApplyCustom##AttributeName##ToAsset(UObject* Asset) const														\
	{																													\
		return ApplyAttributeToObject<AttributeType>(Macro_Custom##AttributeName##Key.Key, Asset, PropertyName);		\
	}																													\
																														\
	bool FillCustom##AttributeName##FromAsset(UObject* Asset)															\
	{																													\
		return FillAttributeFromObject<AttributeType>(Macro_Custom##AttributeName##Key.Key, Asset, PropertyName);		\
	}

#define IMPLEMENT_NODE_ATTRIBUTE_SETTER(NodeClassName, AttributeName, AttributeType, AssetType)														\
	FString OperationName = GetTypeName() + TEXT(".Set" #AttributeName);																			\
	if(InterchangePrivateNodeBase::SetCustomAttribute<AttributeType>(*Attributes, Macro_Custom##AttributeName##Key, OperationName, AttributeValue))	\
	{																																				\
		if(bAddApplyDelegate)																														\
		{																																			\
			FName AttributeNameImpl = TEXT(""#AttributeName);																						\
			AddApplyAndFillDelegates<AttributeType>(Macro_Custom##AttributeName##Key.Key, AssetType::StaticClass(), AttributeNameImpl);				\
		}																																			\
		return true;																																\
	}																																				\
	return false;

/*
 * Use this macro to implement a custom node attribute setter that will add delegate to apply the node attribute value to any UObject derive from
 * the specified type class. Use this macro if the type class is different from the Node GetObjectCLass()
 *
 * The node should derive from UInterchangeFactoryBaseNode to apply a delegate
 * The node need to implement 2 functions for the attribute
 *  - ApplyCustom##AttributeName##ToAsset(UObject*) , see UE::Interchange::FApplyAttributeToAsset delegate signature
 *  - FillCustom##AttributeName##FromAsset(UObject*) , see UE::Interchange::FFillAttributeToAsset delegate signature
 */
#define IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(NodeClassName, AttributeName, AttributeType, AssetType)				\
	FString OperationName = GetTypeName() + TEXT(".Set" #AttributeName);																			\
	if(InterchangePrivateNodeBase::SetCustomAttribute<AttributeType>(*Attributes, Macro_Custom##AttributeName##Key, OperationName, AttributeValue))	\
	{																																				\
		if(bAddApplyDelegate)																														\
		{																																			\
			check(NodeClassName::StaticClass()->IsChildOf<UInterchangeFactoryBaseNode>());															\
			UClass* AssetClass = AssetType::StaticClass();																							\
			TArray<UE::Interchange::FApplyAttributeToAsset>& Delegates = ApplyCustomAttributeDelegates.FindOrAdd(AssetClass);						\
			Delegates.Add(UE::Interchange::FApplyAttributeToAsset::CreateUObject(this, &NodeClassName::ApplyCustom##AttributeName##ToAsset));		\
			TArray<UE::Interchange::FFillAttributeToAsset>& FillDelegates = FillCustomAttributeDelegates.FindOrAdd(AssetClass);						\
			FillDelegates.Add(UE::Interchange::FFillAttributeToAsset::CreateUObject(this, &NodeClassName::FillCustom##AttributeName##FromAsset));	\
		}																																			\
		return true;																																\
	}																																				\
	return false;

/*
 * This macro instead of receiving a TypeClass use the factory node GetObjectClass() to register the delegate
 * See documentation of IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS
 */
#define IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE(NodeClassName, AttributeName, AttributeType)											\
	FString OperationName = GetTypeName() + TEXT(".Set" #AttributeName);																			\
	if(InterchangePrivateNodeBase::SetCustomAttribute<AttributeType>(*Attributes, Macro_Custom##AttributeName##Key, OperationName, AttributeValue))	\
	{																																				\
		if(bAddApplyDelegate)																														\
		{																																			\
			check(NodeClassName::StaticClass()->IsChildOf<UInterchangeFactoryBaseNode>());															\
			TArray<UE::Interchange::FApplyAttributeToAsset>& Delegates = ApplyCustomAttributeDelegates.FindOrAdd(GetObjectClass());					\
			Delegates.Add(UE::Interchange::FApplyAttributeToAsset::CreateUObject(this, &NodeClassName::ApplyCustom##AttributeName##ToAsset));		\
			TArray<UE::Interchange::FFillAttributeToAsset>& FillDelegates = FillCustomAttributeDelegates.FindOrAdd(GetObjectClass());				\
			FillDelegates.Add(UE::Interchange::FFillAttributeToAsset::CreateUObject(this, &NodeClassName::FillCustom##AttributeName##FromAsset));	\
		}																																			\
		return true;																																\
	}																																				\
	return false;


namespace UE::Interchange
{

	DECLARE_DELEGATE_RetVal_OneParam(bool, FApplyAttributeToAsset, UObject*);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFillAttributeToAsset, UObject*);

	struct INTERCHANGECORE_API FFactoryBaseNodeStaticData : public FBaseNodeStaticData
	{
		static const FString& FactoryDependenciesBaseKey();
		static const FAttributeKey& ReimportStrategyFlagsKey();
	};

} // namespace UE::Interchange

UENUM()
enum class EReimportStrategyFlags : uint8
{
	//Do not apply any property when re-importing, simply change the source data
	ApplyNoProperties,
	//Always apply all pipeline specified properties
	ApplyPipelineProperties,
	//Always apply all pipeline properties, but leave the properties modified in editor since the last import
	ApplyEditorChangedProperties
};

/**
 * This struct is used to store and retrieve key value attributes. The attributes are store in a generic FAttributeStorage which serialize the value in a TArray64<uint8>
 * See UE::Interchange::EAttributeTypes to know the supported template types
 * This is an abstract class. This is the base class of the interchange node graph format, all class in this format should derive from this class
 */
UCLASS(BlueprintType)
class INTERCHANGECORE_API UInterchangeFactoryBaseNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeFactoryBaseNode();

	virtual FString GetKeyDisplayName(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;
	virtual FString GetAttributeCategory(const UE::Interchange::FAttributeKey& NodeAttributeKey) const override;

	/**
	 * Return the reimport strategy flags.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	EReimportStrategyFlags GetReimportStrategyFlags() const;

	/**
	 * Change the reimport strategy flags.
	 *
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetReimportStrategyFlags(const EReimportStrategyFlags& ReimportStrategyFlags);

	/**
	 * Return the UClass of the object we represent so we can find factory/writer
	 */
	virtual class UClass* GetObjectClass() const;

	/**
	 * Return the custom sub-path under PackageBasePath, where the assets will be created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetCustomSubPath(FString& AttributeValue) const;

	/**
	 * Set the custom sub-path under PackageBasePath where the assets will be created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetCustomSubPath(const FString& AttributeValue);

	/**
	 * This function allow to retrieve the number of factory dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	int32 GetFactoryDependenciesCount() const;

	/**
	 * This function allow to retrieve the dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	void GetFactoryDependencies(TArray<FString>& OutDependencies ) const;

	/**
	 * This function allow to retrieve one dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	void GetFactoryDependency(const int32 Index, FString& OutDependency) const;

	/**
	 * Add one dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool AddFactoryDependencyUid(const FString& DependencyUid);

	/**
	 * Remove one dependency from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool RemoveFactoryDependencyUid(const FString& DependencyUid);

	/**
	 * Return the custom ReferenceObject. The UObject this factory node has created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool GetCustomReferenceObject(FSoftObjectPath& AttributeValue) const;

	/**
	 * Set the custom ReferenceObject. The UObject this factory node has created.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node")
	bool SetCustomReferenceObject(const FSoftObjectPath& AttributeValue);

	static FString BuildFactoryNodeUid(const FString& TranslatedNodeUid);

	/**
	 * Adds the delegates that will read and write the attribute value to a UObject.
	 * @param NodeAttributeKey	The key of the attribute for which we are adding the delegates.
	 * @param ObjectClass		The class of the object that we will apply and fill on.
	 * @param PropertyName		The name of the property of the ObjectClass that we will read and write to.
	 */
	template<typename AttributeType>
	inline void AddApplyAndFillDelegates(const FName& NodeAttributeKey, UClass* ObjectClass, const FName PropertyName);

	/**
	 * Writes an attribute value to a UObject.
	 * @param NodeAttributeKey	The key for the attribute to write.
	 * @param Object			The object to write to.
	 * @param PropertyName		The name of the property to write to on the Object.
	 * @result					True if we succeeded.
	 */
	template<typename AttributeType>
	inline bool ApplyAttributeToObject(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName) const;

	/**
	 * Reads an attribute value from a UObject.
	 * @param NodeAttributeKey	The key for the attribute to update.
	 * @param Object			The object to read from.
	 * @param PropertyName		The name of the property to read from on the Object.
	 * @result					True if we succeeded.
	 */
	template<typename AttributeType>
	inline bool FillAttributeFromObject(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName);

	/**
	 * Each Attribute that was set and have a delegate set for the specified UObject->UClass will
	 * get the delegate execute so it apply the attribute to the UObject property.
	 * See the macros IMPLEMENT_NODE_ATTRIBUTE_SETTER at the top of the file to know how delegates are setup for property.
	 */
	void ApplyAllCustomAttributeToObject(UObject* Object) const;

	void FillAllCustomAttributeFromObject(UObject* Object) const;

protected:
	/**
	 * Those dependencies are use by the interchange parsing task to make sure the asset are created in the correct order.
	 * Example: Mesh factory node will have dependencies on material factory node
	 *          Material factory node will have dependencies on texture factory node
	 */
	UE::Interchange::TArrayAttributeHelper<FString> FactoryDependencies;

	/* This array hold the delegate to apply the attribute that has to be set on an UObject */
	TMap<UClass*, TArray<UE::Interchange::FApplyAttributeToAsset>> ApplyCustomAttributeDelegates;

	TMap<UClass*, TArray<UE::Interchange::FFillAttributeToAsset>> FillCustomAttributeDelegates;

private:

	const UE::Interchange::FAttributeKey Macro_CustomSubPathKey = UE::Interchange::FAttributeKey(TEXT("SubPath"));
	const UE::Interchange::FAttributeKey Macro_CustomReferenceObjectKey = UE::Interchange::FAttributeKey(TEXT("ReferenceObject"));
};

template<typename AttributeType>
inline void UInterchangeFactoryBaseNode::AddApplyAndFillDelegates(const FName& NodeAttributeKey, UClass* ObjectClass, const FName PropertyName)
{
	TArray<UE::Interchange::FApplyAttributeToAsset>& ApplyDelegates = ApplyCustomAttributeDelegates.FindOrAdd(ObjectClass);
	ApplyDelegates.Add(
		UE::Interchange::FApplyAttributeToAsset::CreateLambda(
			[NodeAttributeKey, PropertyName, this](UObject* Asset)
			{
				return ApplyAttributeToObject<AttributeType>(NodeAttributeKey, Asset, PropertyName);
			})
	);

	TArray<UE::Interchange::FFillAttributeToAsset>& FillDelegates = FillCustomAttributeDelegates.FindOrAdd(ObjectClass);
	FillDelegates.Add(
		UE::Interchange::FApplyAttributeToAsset::CreateLambda(
			[NodeAttributeKey, PropertyName, this](UObject* Asset)
			{
				return FillAttributeFromObject<AttributeType>(NodeAttributeKey, Asset, PropertyName);
			})
	);
}

template<typename AttributeType>
inline bool UInterchangeFactoryBaseNode::ApplyAttributeToObject(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName) const
{
	if (!Object)
	{
		return false;
	}

	AttributeType ValueData;
	if (GetAttribute<AttributeType>(NodeAttributeKey, ValueData))
	{
		TVariant<UObject*, uint8*> Container;
		Container.Set<UObject*>(Object);
		if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
		{
			AttributeType* PropertyValueAddress;
			if (Container.IsType<UObject*>())
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<AttributeType>(Container.Get<UObject*>());
			}
			else
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<AttributeType>(Container.Get<uint8*>());
			}

			*PropertyValueAddress = ValueData;
		}
		return true;
	}
	return false;
}

/**
 * Specialized version of ApplyAttributeToObject for strings.
 * If the target property is a FObjectPropertyBase, treat the string as an object path.
 */
template<>
inline bool UInterchangeFactoryBaseNode::ApplyAttributeToObject<FString>(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName) const
{
	if (!Object)
	{
		return false;
	}

	FString ValueData;
	if (GetAttribute<FString>(NodeAttributeKey, ValueData))
	{
		TVariant<UObject*, uint8*> Container;
		Container.Set<UObject*>(Object);
		if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
		{
			FString* PropertyValueAddress;
			if (Container.IsType<UObject*>())
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<FString>(Container.Get<UObject*>());
			}
			else
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<FString>(Container.Get<uint8*>());
			}

			if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
			{
				ObjectProperty->SetObjectPropertyValue(PropertyValueAddress, FSoftObjectPath(ValueData).TryLoad());
			}
			else
			{
				*PropertyValueAddress = ValueData;
			}
		}
		return true;
	}
	return false;
}

/**
 * Specialized version of ApplyAttributeToObject for bools.
 * If the target property is a FBoolProperty, treat the propertyg as a bitfield.
 */
template<>
inline bool UInterchangeFactoryBaseNode::ApplyAttributeToObject<bool>(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName) const
{
	if (!Object)
	{
		return false;
	}

	bool bValueData;
	if (GetAttribute<bool>(NodeAttributeKey, bValueData))
	{
		TVariant<UObject*, uint8*> Container;
		Container.Set<UObject*>(Object);
		if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
		{
			bool* PropertyValueAddress;
			if (Container.IsType<UObject*>())
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<bool>(Container.Get<UObject*>());
			}
			else
			{
				PropertyValueAddress = Property->ContainerPtrToValuePtr<bool>(Container.Get<uint8*>());
			}

			// Support for bitfields
			if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
			{
				BoolProperty->SetPropertyValue(PropertyValueAddress, bValueData);
			}
			else
			{
				*PropertyValueAddress = bValueData;
			}
		}
		return true;
	}
	return false;
}

template<typename AttributeType>
inline bool UInterchangeFactoryBaseNode::FillAttributeFromObject(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName)
{
	TVariant<UObject*, uint8*> Container;
	Container.Set<UObject*>(Object);
	if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
	{
		AttributeType* PropertyValueAddress;
		if (Container.IsType<UObject*>())
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<AttributeType>(Container.Get<UObject*>());
		}
		else
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<AttributeType>(Container.Get<uint8*>());
		}

		// Support for bitfields
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			const bool bPropertyValue = BoolProperty->GetPropertyValue(PropertyValueAddress);
			return SetAttribute(NodeAttributeKey, bPropertyValue);
		}
		else
		{
			return SetAttribute(NodeAttributeKey, *PropertyValueAddress);
		}
	}

	return false;
}

/**
 * Specialized version of FillAttributeFromObject for strings.
 * If the target property is a FObjectPropertyBase, treat the string as an object path.
 */
template<>
inline bool UInterchangeFactoryBaseNode::FillAttributeFromObject<FString>(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName)
{
	TVariant<UObject*, uint8*> Container;
	Container.Set<UObject*>(Object);
	if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
	{
		FString* PropertyValueAddress;
		if (Container.IsType<UObject*>())
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<FString>(Container.Get<UObject*>());
		}
		else
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<FString>(Container.Get<uint8*>());
		}

		if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
		{
			UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue(PropertyValueAddress);
			return SetAttribute(NodeAttributeKey, ObjectValue->GetPathName());
		}
		else
		{
			return SetAttribute(NodeAttributeKey, *PropertyValueAddress);
		}
	}

	return false;
}

/**
 * Specialized version of FillAttributeFromObject for bools.
 * If the target property is a FBoolProperty, treat the property as a bitfield.
 */
template<>
inline bool UInterchangeFactoryBaseNode::FillAttributeFromObject<bool>(const FName& NodeAttributeKey, UObject* Object, const FName PropertyName)
{
	TVariant<UObject*, uint8*> Container;
	Container.Set<UObject*>(Object);
	if (FProperty* Property = InterchangePrivateNodeBase::FindPropertyByPathChecked(Container, Object->GetClass(), PropertyName.ToString()))
	{
		bool* PropertyValueAddress;
		if (Container.IsType<UObject*>())
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<bool>(Container.Get<UObject*>());
		}
		else
		{
			PropertyValueAddress = Property->ContainerPtrToValuePtr<bool>(Container.Get<uint8*>());
		}

		// Support for bitfields
		if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
		{
			const bool bPropertyValue = BoolProperty->GetPropertyValue(PropertyValueAddress);
			return SetAttribute(NodeAttributeKey, bPropertyValue);
		}
		else
		{
			return SetAttribute(NodeAttributeKey, *PropertyValueAddress);
		}
	}

	return false;
}
