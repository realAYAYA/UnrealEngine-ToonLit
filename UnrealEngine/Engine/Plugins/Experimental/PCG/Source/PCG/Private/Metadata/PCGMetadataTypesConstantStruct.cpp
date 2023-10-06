// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/PCGMetadataTypesConstantStruct.h"

FString FPCGMetadataTypesConstantStruct::ToString() const
{
	switch (Type)
	{
	case EPCGMetadataTypes::Integer32:
		return FString::Printf(TEXT("%d"), Int32Value);
	case EPCGMetadataTypes::Float:
		return FString::Printf(TEXT("%.2f"), FloatValue);
	case EPCGMetadataTypes::Integer64:
		return FString::Printf(TEXT("%lld"), IntValue);
	case EPCGMetadataTypes::Double:
		return FString::Printf(TEXT("%.2f"), DoubleValue);
	case EPCGMetadataTypes::String:
	{
		switch (StringMode)
		{
		case EPCGMetadataTypesConstantStructStringMode::String:
			return FString::Printf(TEXT("\"%s\""), *StringValue);
		case EPCGMetadataTypesConstantStructStringMode::SoftObjectPath:
			return FString::Printf(TEXT("\"%s\""), *(SoftObjectPathValue.GetAssetName()));
		case EPCGMetadataTypesConstantStructStringMode::SoftClassPath:
			return FString::Printf(TEXT("\"%s\""), *(SoftClassPathValue.GetAssetName()));
		default:
			break;
		}
	}
	case EPCGMetadataTypes::Name:
		return FString::Printf(TEXT("N(\"%s\")"), *NameValue.ToString());
	case EPCGMetadataTypes::Vector2:
		return FString::Printf(TEXT("V(%.2f, %.2f)"), Vector2Value.X, Vector2Value.Y);
	case EPCGMetadataTypes::Vector:
		return FString::Printf(TEXT("V(%.2f, %.2f, %.2f)"), VectorValue.X, VectorValue.Y, VectorValue.Z);
	case EPCGMetadataTypes::Vector4:
		return FString::Printf(TEXT("V(%.2f, %.2f, %.2f, %.2f)"), Vector4Value.X, Vector4Value.Y, Vector4Value.Z, Vector4Value.W);
	case EPCGMetadataTypes::Rotator:
		return FString::Printf(TEXT("R(%.2f, %.2f, %.2f)"), RotatorValue.Roll, RotatorValue.Pitch, RotatorValue.Yaw);
	case EPCGMetadataTypes::Quaternion:
		return FString::Printf(TEXT("Q(%.2f, %.2f, %.2f, %.2f)"), QuatValue.X, QuatValue.Y, QuatValue.Z, QuatValue.W);
	case EPCGMetadataTypes::Transform:
		return FString::Printf(TEXT("Transform"));
	case EPCGMetadataTypes::Boolean:
		return FString::Printf(TEXT("%s"), (BoolValue ? TEXT("True") : TEXT("False")));
	default:
		break;
	}

	return FString();
}