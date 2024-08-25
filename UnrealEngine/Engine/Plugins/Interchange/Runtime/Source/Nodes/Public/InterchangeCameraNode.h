// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeCameraNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangePhysicalCameraNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("PhysicalCamera");
	}

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("PhysicalCameraNode");
		return TypeName;
	}

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomFocalLength(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FocalLength, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomFocalLength(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(FocalLength, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomSensorWidth(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(SensorWidth, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomSensorWidth(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SensorWidth, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
		bool GetCustomSensorHeight(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(SensorHeight, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
		bool SetCustomSensorHeight(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(SensorHeight, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomEnableDepthOfField(bool& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(EnableDepthOfField, bool);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomEnableDepthOfField(const bool& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(EnableDepthOfField, bool);
	}
private:
	const UE::Interchange::FAttributeKey Macro_CustomFocalLengthKey = UE::Interchange::FAttributeKey(TEXT("FocalLength"));
	const UE::Interchange::FAttributeKey Macro_CustomSensorWidthKey = UE::Interchange::FAttributeKey(TEXT("SensorWidth"));
	const UE::Interchange::FAttributeKey Macro_CustomSensorHeightKey = UE::Interchange::FAttributeKey(TEXT("SensorHeight"));
	const UE::Interchange::FAttributeKey Macro_CustomEnableDepthOfFieldKey = UE::Interchange::FAttributeKey(TEXT("EnableDepthOfField"));
};

UENUM(BlueprintType)
enum class EInterchangeCameraProjectionType : uint8
{
	Perspective,
	Orthographic
};

// Primarily used for Ortho Camera
UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeStandardCameraNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("StandardCamera");
	}

	/**
	 * Return the node type name of the class. This is used when reporting errors.
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("StandardCameraNode");
		return TypeName;
	}

public:

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomProjectionMode(EInterchangeCameraProjectionType& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(ProjectionMode, EInterchangeCameraProjectionType);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomProjectionMode(const EInterchangeCameraProjectionType& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(ProjectionMode, EInterchangeCameraProjectionType);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomWidth(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(Width, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomWidth(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(Width, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomNearClipPlane(float& AttributeValue) const
	{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(NearClipPlane, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomNearClipPlane(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(NearClipPlane, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomFarClipPlane(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FarClipPlane, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomFarClipPlane(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(FarClipPlane, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomAspectRatio(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(AspectRatio, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomAspectRatio(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(AspectRatio, float);
	}

	//Field of View in Degrees.
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool GetCustomFieldOfView(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FieldOfView, float);
	}
	//Field of View in Degrees.
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | Camera")
	bool SetCustomFieldOfView(const float& AttributeValue)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(FieldOfView, float);
	}

private:
	const UE::Interchange::FAttributeKey Macro_CustomProjectionModeKey = UE::Interchange::FAttributeKey(TEXT("ProjectionMode"));
	
	const UE::Interchange::FAttributeKey Macro_CustomWidthKey = UE::Interchange::FAttributeKey(TEXT("Width"));
	const UE::Interchange::FAttributeKey Macro_CustomNearClipPlaneKey = UE::Interchange::FAttributeKey(TEXT("NearClipPlane"));
	const UE::Interchange::FAttributeKey Macro_CustomFarClipPlaneKey = UE::Interchange::FAttributeKey(TEXT("FarClipPlane"));
	const UE::Interchange::FAttributeKey Macro_CustomAspectRatioKey = UE::Interchange::FAttributeKey(TEXT("AspectRatio"));

	const UE::Interchange::FAttributeKey Macro_CustomFieldOfViewKey = UE::Interchange::FAttributeKey(TEXT("FieldOfView"));
};