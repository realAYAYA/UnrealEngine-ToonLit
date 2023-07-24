// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/InterchangeUserDefinedAttribute.h"

#include "Nodes/InterchangeBaseNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeUserDefinedAttribute)

const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributeBaseKey = TEXT("UserDefined_");
const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributeValuePostKey = TEXT("_Value");
const FString UInterchangeUserDefinedAttributesAPI::UserDefinedAttributePayLoadPostKey = TEXT("_Payload");

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Boolean(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const bool& Value, const FString& PayloadKey)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Float(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const float& Value, const FString& PayloadKey)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Double(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const double& Value, const FString& PayloadKey)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_Int32(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const int32& Value, const FString& PayloadKey)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload);
}

bool UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute_FString(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName, const FString& Value, const FString& PayloadKey)
{
	TOptional<FString> OptionalPayload;
	if (!PayloadKey.IsEmpty())
	{
		OptionalPayload = PayloadKey;
	}
	return UInterchangeUserDefinedAttributesAPI::CreateUserDefinedAttribute(InterchangeNode, UserDefinedAttributeName, Value, OptionalPayload);
}

bool UInterchangeUserDefinedAttributesAPI::RemoveUserDefinedAttribute(UInterchangeBaseNode* InterchangeNode, const FString& UserDefinedAttributeName)
{

	FString StorageBaseKey = UserDefinedAttributeBaseKey + UserDefinedAttributeName;
	UE::Interchange::FAttributeKey UserDefinedValueKey = UE::Interchange::FAttributeKey(StorageBaseKey + UserDefinedAttributeValuePostKey);
	if (InterchangeNode->HasAttribute(UserDefinedValueKey))
	{
		if (!InterchangeNode->RemoveAttribute(UserDefinedValueKey.Key))
		{
			return false;
		}
	}

	UE::Interchange::FAttributeKey UserDefinedPayloadKey = UE::Interchange::FAttributeKey(StorageBaseKey + UserDefinedAttributePayLoadPostKey);
	if (InterchangeNode->HasAttribute(UserDefinedPayloadKey))
	{
		if (!InterchangeNode->RemoveAttribute(UserDefinedPayloadKey.Key))
		{
			return false;
		}
	}

	//Attribute was successfuly removed
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
	for (UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		const FString AttributeKeyString = AttributeKey.ToString();
		if (AttributeKeyString.StartsWith(UserDefinedAttributeBaseKey) && AttributeKeyString.EndsWith(UserDefinedAttributeValuePostKey))
		{
			FString UserDefinedAttributeName = AttributeKeyString.RightChop(RightChopIndex).LeftChop(LeftChopIndex);
			FInterchangeUserDefinedAttributeInfo& UserDefinedAttributeInfo = UserDefinedAttributeInfos.AddDefaulted_GetRef();
			UserDefinedAttributeInfo.Type = InterchangeNode->GetAttributeType(AttributeKey);
			UserDefinedAttributeInfo.Name = UserDefinedAttributeName;

			//Get the optional payload key
			const FString StorageBaseKey = UserDefinedAttributeBaseKey + UserDefinedAttributeName;
			const UE::Interchange::FAttributeKey UserDefinedPayloadKey = UE::Interchange::FAttributeKey(StorageBaseKey + UserDefinedAttributePayLoadPostKey);
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
	for (UE::Interchange::FAttributeKey& AttributeKey : AttributeKeys)
	{
		const FString AttributeKeyString = AttributeKey.ToString();
		if (AttributeKeyString.StartsWith(UserDefinedAttributeBaseKey) && AttributeKeyString.EndsWith(UserDefinedAttributeValuePostKey))
		{
			FString UserDefinedAttributeName = AttributeKeyString.RightChop(RightChopIndex).LeftChop(LeftChopIndex);
			//Get the optional payload key
			const FString StorageBaseKey = UserDefinedAttributeBaseKey + UserDefinedAttributeName;

			UE::Interchange::EAttributeTypes Type = InterchangeSourceNode->GetAttributeType(AttributeKey);
			FString DuplicateName = UserDefinedAttributeName;
			if (bAddSourceNodeName)
			{
				DuplicateName = InterchangeSourceNode->GetDisplayLabel() + TEXT(".") + UserDefinedAttributeName;
			}

			const UE::Interchange::FAttributeKey UserDefinedPayloadKey = UE::Interchange::FAttributeKey(StorageBaseKey + UserDefinedAttributePayLoadPostKey);
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
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<bool>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Int8:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int8>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Int16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int16>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Int32:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int32>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Int64:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<int64>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::UInt8:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint8>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::UInt16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint16>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::UInt32:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint32>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::UInt64:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<uint64>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Float:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<float>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Float16:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FFloat16>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector2f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector2f>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector3f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector3f>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector4f:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector4f>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Double:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<double>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector2d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector2D>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector3d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector3d>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::Vector4d:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FVector4d>(AttributeKey.Key), PayloadKey);
					break;
				case UE::Interchange::EAttributeTypes::String:
					CreateUserDefinedAttribute(InterchangeDestinationNode, DuplicateName, InterchangeSourceNode->GetAttributeChecked<FString>(AttributeKey.Key), PayloadKey);
					break;
			}
		}
	}
}

