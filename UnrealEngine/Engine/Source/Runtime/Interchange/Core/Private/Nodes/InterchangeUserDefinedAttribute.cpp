// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/InterchangeUserDefinedAttribute.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Nodes/InterchangeBaseNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeUserDefinedAttribute)

const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributeBaseKey = TEXT("UserDefined_");
const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributeDelegateKey = TEXT("AddDelegate_");
const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributeValuePostKey = TEXT("_Value");
const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributePayLoadPostKey = TEXT("_Payload");

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Boolean(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const bool& Value, const FString& PayloadKey, bool RequiresDelegate /*= false*/)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload, RequiresDelegate);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Float(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const float& Value, const FString& PayloadKey, bool RequiresDelegate /*= false*/)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload, RequiresDelegate);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Double(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const double& Value, const FString& PayloadKey, bool RequiresDelegate /*= false*/)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload, RequiresDelegate);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Int32(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const int32& Value, const FString& PayloadKey, bool RequiresDelegate /*= false*/)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload, RequiresDelegate);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_FString(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const FString& Value, const FString& PayloadKey, bool RequiresDelegate /*= false*/)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload, RequiresDelegate);
}

bool UInterchangeUserDefinedAttributesAPI::RemoveUserDefinedAttribute(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName)
{
	bool RequiresDelegates;
	const bool bGeneratePayloadKey = false;
	if (HasAttribute(InterchangeNode, UserDefinedAttributeName, bGeneratePayloadKey, RequiresDelegates))
	{
		UE::Interchange::FAttributeKey UserDefinedValueKey = MakeUserDefinedPropertyValueKey(UserDefinedAttributeName, RequiresDelegates);
		if (!InterchangeNode->RemoveAttribute(UserDefinedValueKey.Key))
		{
			return false;
		}

		UE::Interchange::FAttributeKey UserDefinedPayloadKey = MakeUserDefinedPropertyPayloadKey(UserDefinedAttributeName, RequiresDelegates);
		if (InterchangeNode->HasAttribute(UserDefinedPayloadKey))
		{
			if (!InterchangeNode->RemoveAttribute(UserDefinedPayloadKey.Key))
			{
				return false;
			}
		}
	}

	//Attribute was successfully removed/ never existed
	return true;
}

bool UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute_Boolean(const UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, bool& OutValue, FString& OutPayloadKey)
{
	TOptional<FString> OptionalPayload;
	bool bResult = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, OutValue, OptionalPayload);
	if (OptionalPayload.IsSet())
	{
		OutPayloadKey = OptionalPayload.GetValue();
	}
	return bResult;
}

bool UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute_Float(const UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, float& OutValue, FString& OutPayloadKey)
{
	TOptional<FString> OptionalPayload;
	bool bResult = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, OutValue, OptionalPayload);
	if (OptionalPayload.IsSet())
	{
		OutPayloadKey = OptionalPayload.GetValue();
	}
	return bResult;
}

bool UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute_Double(const UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, double& OutValue, FString& OutPayloadKey)
{
	TOptional<FString> OptionalPayload;
	bool bResult = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, OutValue, OptionalPayload);
	if (OptionalPayload.IsSet())
	{
		OutPayloadKey = OptionalPayload.GetValue();
	}
	return bResult;
}

bool UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute_Int32(const UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, int32& OutValue, FString& OutPayloadKey)
{
	TOptional<FString> OptionalPayload;
	bool bResult = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, OutValue, OptionalPayload);
	if (OptionalPayload.IsSet())
	{
		OutPayloadKey = OptionalPayload.GetValue();
	}
	return bResult;
}

bool UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute_FString(const UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, FString& OutValue, FString& OutPayloadKey)
{
	TOptional<FString> OptionalPayload;
	bool bResult = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, OutValue, OptionalPayload);
	if (OptionalPayload.IsSet())
	{
		OutPayloadKey = OptionalPayload.GetValue();
	}
	return bResult;
}

TArray<FInterchangeUserDefinedAttributeInfo> UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(const UInterchangeBaseNode* InterchangeNode)
{
	check(InterchangeNode);
	TArray<UE::Interchange::FAttributeKey> AttributeKeys;
	InterchangeNode->GetAttributeKeys(AttributeKeys);
	TArray<FInterchangeUserDefinedAttributeInfo> UserDefinedAttributeInfos;
	int32 RightChopIndex = UserDefinedAttributeBaseKey.Len();
	int32 LeftChopIndex = UserDefinedAttributeValuePostKey.Len();
	int32 AddDelegateRightChopIndex = UserDefinedAttributeDelegateKey.Len();

	for (UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		const FString AttributeKeyString = AttributeKey.ToString();
		if (AttributeKeyString.StartsWith(UserDefinedAttributeBaseKey) && AttributeKeyString.EndsWith(UserDefinedAttributeValuePostKey))
		{
			bool RequiresDelegate = false;
			FString UserDefinedAttributeName = AttributeKeyString.RightChop(RightChopIndex).LeftChop(LeftChopIndex);
			if (UserDefinedAttributeName.StartsWith(UserDefinedAttributeDelegateKey))
			{
				UserDefinedAttributeName = UserDefinedAttributeName.RightChop(AddDelegateRightChopIndex);
				RequiresDelegate = true;
			}
			FInterchangeUserDefinedAttributeInfo& UserDefinedAttributeInfo = UserDefinedAttributeInfos.AddDefaulted_GetRef();
			UserDefinedAttributeInfo.Type = InterchangeNode->GetAttributeType(AttributeKey);
			UserDefinedAttributeInfo.Name = UserDefinedAttributeName;
			UserDefinedAttributeInfo.RequiresDelegate = RequiresDelegate;

			//Get the optional payload key
			const FString StorageBaseKey = UserDefinedAttributeBaseKey + UserDefinedAttributeName;
			const UE::Interchange::FAttributeKey UserDefinedPayloadKey = MakeUserDefinedPropertyPayloadKey(UserDefinedAttributeName, RequiresDelegate);
			if (InterchangeNode->HasAttribute(UserDefinedPayloadKey))
			{
				FString PayloadKey;
				InterchangeNode->GetStringAttribute(UserDefinedPayloadKey.Key, PayloadKey);
				UserDefinedAttributeInfo.PayloadKey = PayloadKey;
			}
		}
	}
	return UserDefinedAttributeInfos;
}

void UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(const UInterchangeBaseNode* InterchangeNode, TArray<FInterchangeUserDefinedAttributeInfo>& UserDefinedAttributeInfos)
{
	check(InterchangeNode);
	UserDefinedAttributeInfos = GetUserDefinedAttributeInfos(InterchangeNode);
}

void UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(const UInterchangeBaseNode* InterchangeSourceNode, UInterchangeBaseNode* InterchangeDestinationNode, bool bAddSourceNodeName)
{
	check(InterchangeSourceNode);
	check(InterchangeDestinationNode);
	TArray<UE::Interchange::FAttributeKey> AttributeKeys;
	InterchangeSourceNode->GetAttributeKeys(AttributeKeys);
	int32 RightChopIndex = UserDefinedAttributeBaseKey.Len();
	int32 LeftChopIndex = UserDefinedAttributeValuePostKey.Len();
	int32 AddDelegateRightChopIndex = UserDefinedAttributeDelegateKey.Len();

	for (UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		const FString AttributeKeyString = AttributeKey.ToString();
		if (AttributeKeyString.StartsWith(UserDefinedAttributeBaseKey) && AttributeKeyString.EndsWith(UserDefinedAttributeValuePostKey))
		{
			bool RequiresDelegate = false;
			FString UserDefinedAttributeName = AttributeKeyString.RightChop(RightChopIndex).LeftChop(LeftChopIndex);
			if (UserDefinedAttributeName.StartsWith(UserDefinedAttributeDelegateKey))
			{
				UserDefinedAttributeName = UserDefinedAttributeName.RightChop(AddDelegateRightChopIndex);
				RequiresDelegate = true;
			}
			
			UE::Interchange::EAttributeTypes Type = InterchangeSourceNode->GetAttributeType(AttributeKey);
			FString DuplicateName = UserDefinedAttributeName;
			if (bAddSourceNodeName)
			{
				DuplicateName = InterchangeSourceNode->GetDisplayLabel() + TEXT(".") + UserDefinedAttributeName;
			}

			//Get the optional payload key
			const UE::Interchange::FAttributeKey UserDefinedPayloadKey = MakeUserDefinedPropertyPayloadKey(UserDefinedAttributeName, RequiresDelegate);
			TOptional<FString> PayloadKey;
			if (InterchangeSourceNode->HasAttribute(UserDefinedPayloadKey))
			{
				FString PayloadKeyValue;
				InterchangeSourceNode->GetStringAttribute(UserDefinedPayloadKey.Key, PayloadKeyValue);
				PayloadKey = PayloadKeyValue;
			}
			switch (Type)
			{
				case UE::Interchange::EAttributeTypes::Bool:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<bool>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Int8:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int8>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Int16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int16>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Int32:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int32>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Int64:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int64>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::UInt8:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint8>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::UInt16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint16>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::UInt32:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint32>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::UInt64:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint64>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Float:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<float>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Float16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FFloat16>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector2f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector2f>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector3f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector3f>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector4f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector4f>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Double:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<double>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector2d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector2D>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector3d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector3d>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::Vector4d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector4d>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
				case UE::Interchange::EAttributeTypes::String:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FString>(AttributeKey.Key), PayloadKey, RequiresDelegate);
					break;
			}
		}
	}
}

void UInterchangeUserDefinedAttributesAPI::AddApplyAndFillDelegatesToFactory(UInterchangeFactoryBaseNode* InterchangeFactoryNode, UClass* ParentClass)
{
	TArray<UE::Interchange::FAttributeKey> AttributeKeys;
	InterchangeFactoryNode->GetAttributeKeys(AttributeKeys);
	int32 RightChopIndex = UserDefinedAttributeBaseKey.Len();
	int32 LeftChopIndex = UserDefinedAttributeValuePostKey.Len();
	int32 AddDelegateRightChopIndex = UserDefinedAttributeDelegateKey.Len();

	for (UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		const FString AttributeKeyString = AttributeKey.ToString();
		if (AttributeKeyString.StartsWith(UserDefinedAttributeBaseKey) && AttributeKeyString.EndsWith(UserDefinedAttributeValuePostKey))
		{
			bool RequiresDelegate = false;
			FString UserDefinedAttributeName = AttributeKeyString.RightChop(RightChopIndex).LeftChop(LeftChopIndex);
			if (!UserDefinedAttributeName.StartsWith(UserDefinedAttributeDelegateKey))
			{
				continue;				
			}

			UserDefinedAttributeName = UserDefinedAttributeName.RightChop(AddDelegateRightChopIndex);
			RequiresDelegate = true;

			UE::Interchange::EAttributeTypes Type = InterchangeFactoryNode->GetAttributeType(AttributeKey);
			switch (Type)
			{
			case UE::Interchange::EAttributeTypes::Bool:
				InterchangeFactoryNode->AddApplyAndFillDelegates<bool>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Int8:
				InterchangeFactoryNode->AddApplyAndFillDelegates<int8>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Int16:
				InterchangeFactoryNode->AddApplyAndFillDelegates<int16>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Int32:
				InterchangeFactoryNode->AddApplyAndFillDelegates<int32>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Int64:
				InterchangeFactoryNode->AddApplyAndFillDelegates<int64>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::UInt8:
				InterchangeFactoryNode->AddApplyAndFillDelegates<uint8>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::UInt16:
				InterchangeFactoryNode->AddApplyAndFillDelegates<uint16>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::UInt32:
				InterchangeFactoryNode->AddApplyAndFillDelegates<uint32>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::UInt64:
				InterchangeFactoryNode->AddApplyAndFillDelegates<uint64>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Float:
				InterchangeFactoryNode->AddApplyAndFillDelegates<float>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Float16:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FFloat16>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector2f:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector2f>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector3f:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector3f>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector4f:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector4f>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Double:
				InterchangeFactoryNode->AddApplyAndFillDelegates<double>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector2d:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector2D>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector3d:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector3d>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::Vector4d:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FVector4d>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			case UE::Interchange::EAttributeTypes::String:
				InterchangeFactoryNode->AddApplyAndFillDelegates<FString>(AttributeKey.Key, ParentClass, *UserDefinedAttributeName);
				break;
			}
		}
	}
}

UE::Interchange::FAttributeKey UInterchangeUserDefinedAttributesAPI::MakeUserDefinedPropertyValueKey(const FString& UserDefinedAttributeName, bool RequiresDelegate)
{
	return MakeUserDefinedPropertyKey(UserDefinedAttributeName, RequiresDelegate);
}

UE::Interchange::FAttributeKey UInterchangeUserDefinedAttributesAPI::MakeUserDefinedPropertyPayloadKey(const FString& UserDefinedAttributeName, bool RequiresDelegate)
{
	const bool bGeneratePayloadKey = true;
	return MakeUserDefinedPropertyKey(UserDefinedAttributeName, RequiresDelegate, bGeneratePayloadKey);
}

bool UInterchangeUserDefinedAttributesAPI::HasAttribute(const UInterchangeBaseNode* InterchangeSourceNode, const FString& InUserDefinedAttributeName, bool GeneratePayloadKey, bool& OutRequiresDelegate)
{
	UE::Interchange::FAttributeKey KeyWithDelegate = MakeUserDefinedPropertyKey(InUserDefinedAttributeName,/*RequiresDelegate =*/ true, GeneratePayloadKey);
	UE::Interchange::FAttributeKey KeyWithoutDelegate = MakeUserDefinedPropertyKey(InUserDefinedAttributeName,/*RequiresDelegate =*/ false, GeneratePayloadKey);
	OutRequiresDelegate = true;
	if (InterchangeSourceNode->HasAttribute(KeyWithDelegate))
	{
		return true;
	}
	else if (InterchangeSourceNode->HasAttribute(KeyWithoutDelegate))
	{
		OutRequiresDelegate = false;
		return true;
	}

	return false;
}

UE::Interchange::FAttributeKey UInterchangeUserDefinedAttributesAPI::MakeUserDefinedPropertyKey(const FString& UserDefinedAttributeName, bool RequiresDelegate, bool GeneratePayloadKey /*= false*/)
{ 
	//Create a unique Key for this user defined attribute
	FString Prefix = UserDefinedAttributeBaseKey;
	if (RequiresDelegate)
	{
		Prefix = Prefix + UserDefinedAttributeDelegateKey;
	}

	const FString Suffix = GeneratePayloadKey ? UserDefinedAttributePayLoadPostKey : UserDefinedAttributeValuePostKey;

	return UE::Interchange::FAttributeKey(Prefix + UserDefinedAttributeName + Suffix);
}