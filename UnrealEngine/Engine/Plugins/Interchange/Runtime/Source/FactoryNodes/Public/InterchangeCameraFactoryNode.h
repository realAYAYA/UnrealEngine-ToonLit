// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeActorFactoryNode.h"

#if WITH_ENGINE
	#include "CineCameraComponent.h"
#endif

#include "InterchangeCameraFactoryNode.generated.h"

UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangePhysicalCameraFactoryNode : public UInterchangeActorFactoryNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomFocalLength(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FocalLength, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomFocalLength(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangePhysicalCameraFactoryNode, FocalLength, float, UCineCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomSensorWidth(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(SensorWidth, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomSensorWidth(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangePhysicalCameraFactoryNode, SensorWidth, float, UCineCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomSensorHeight(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(SensorHeight, float);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomSensorHeight(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangePhysicalCameraFactoryNode, SensorHeight, float, UCineCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomFocusMethod(ECameraFocusMethod& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FocusMethod, ECameraFocusMethod);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomFocusMethod(const ECameraFocusMethod& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangePhysicalCameraFactoryNode, FocusMethod, ECameraFocusMethod, UCineCameraComponent);
	}	

	virtual void CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object) override
	{
		Super::CopyWithObject(SourceNode, Object);

		if (const UInterchangePhysicalCameraFactoryNode* ActorFactoryNode = Cast<UInterchangePhysicalCameraFactoryNode>(SourceNode))
		{
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangePhysicalCameraFactoryNode, FocalLength, float, UCineCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangePhysicalCameraFactoryNode, SensorWidth, float, UCineCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangePhysicalCameraFactoryNode, SensorHeight, float, UCineCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangePhysicalCameraFactoryNode, FocusMethod, ECameraFocusMethod, UCineCameraComponent::StaticClass())
		}
	}

private:
	const UE::Interchange::FAttributeKey Macro_CustomFocalLengthKey = UE::Interchange::FAttributeKey(TEXT("FocalLength"));
	const UE::Interchange::FAttributeKey Macro_CustomSensorWidthKey = UE::Interchange::FAttributeKey(TEXT("SensorWidth"));
	const UE::Interchange::FAttributeKey Macro_CustomSensorHeightKey = UE::Interchange::FAttributeKey(TEXT("SensorHeight"));
	const UE::Interchange::FAttributeKey Macro_CustomFocusMethodKey = UE::Interchange::FAttributeKey(TEXT("FocusMethod"));

private:
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(FocalLength, float, UCineCameraComponent, TEXT("CurrentFocalLength"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(SensorWidth, float, UCineCameraComponent, TEXT("Filmback.SensorWidth"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(SensorHeight, float, UCineCameraComponent, TEXT("Filmback.SensorHeight"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(FocusMethod, ECameraFocusMethod, UCineCameraComponent, TEXT("FocusSettings.FocusMethod"));
};


/*
* Used for common non-physical camera with orthographic or perspective projection.
*/
UCLASS(BlueprintType)
class INTERCHANGEFACTORYNODES_API UInterchangeStandardCameraFactoryNode : public UInterchangeActorFactoryNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
		bool GetCustomProjectionMode(TEnumAsByte<ECameraProjectionMode::Type>& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(ProjectionMode, TEnumAsByte<ECameraProjectionMode::Type>);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
		bool SetCustomProjectionMode(const TEnumAsByte<ECameraProjectionMode::Type>& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, ProjectionMode, TEnumAsByte<ECameraProjectionMode::Type>, UCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomWidth(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(Width, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomWidth(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, Width, float, UCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomNearClipPlane(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(NearClipPlane, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomNearClipPlane(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, NearClipPlane, float, UCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomFarClipPlane(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FarClipPlane, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomFarClipPlane(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, FarClipPlane, float, UCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool GetCustomAspectRatio(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(AspectRatio, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
	bool SetCustomAspectRatio(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, AspectRatio, float, UCameraComponent);
	}

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
		bool GetCustomFieldOfView(float& AttributeValue) const
	{
		IMPLEMENT_NODE_ATTRIBUTE_GETTER(FieldOfView, float);
	}
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | CameraFactory")
		bool SetCustomFieldOfView(const float& AttributeValue, bool bAddApplyDelegate = true)
	{
		IMPLEMENT_NODE_ATTRIBUTE_SETTER_WITH_CUSTOM_DELEGATE_WITH_CUSTOM_CLASS(UInterchangeStandardCameraFactoryNode, FieldOfView, float, UCameraComponent);
	}

	virtual void CopyWithObject(const UInterchangeFactoryBaseNode* SourceNode, UObject* Object) override
	{
		Super::CopyWithObject(SourceNode, Object);

		if (const UInterchangeStandardCameraFactoryNode* ActorFactoryNode = Cast<UInterchangeStandardCameraFactoryNode>(SourceNode))
		{
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, ProjectionMode, TEnumAsByte<ECameraProjectionMode::Type>, UCameraComponent::StaticClass())

			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, Width, float, UCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, NearClipPlane, float, UCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, FarClipPlane, float, UCameraComponent::StaticClass())
			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, AspectRatio, float, UCameraComponent::StaticClass())

			COPY_NODE_DELEGATES_WITH_CUSTOM_DELEGATE(ActorFactoryNode, UInterchangeStandardCameraFactoryNode, FieldOfView, float, UCameraComponent::StaticClass())
		}
	}

private:
	const UE::Interchange::FAttributeKey Macro_CustomProjectionModeKey = UE::Interchange::FAttributeKey(TEXT("ProjectionMode"));

	const UE::Interchange::FAttributeKey Macro_CustomWidthKey = UE::Interchange::FAttributeKey(TEXT("Width"));
	const UE::Interchange::FAttributeKey Macro_CustomNearClipPlaneKey = UE::Interchange::FAttributeKey(TEXT("NearClip"));
	const UE::Interchange::FAttributeKey Macro_CustomFarClipPlaneKey = UE::Interchange::FAttributeKey(TEXT("FarClipPlane"));
	const UE::Interchange::FAttributeKey Macro_CustomAspectRatioKey = UE::Interchange::FAttributeKey(TEXT("AspectRatio"));
	
	const UE::Interchange::FAttributeKey Macro_CustomFieldOfViewKey = UE::Interchange::FAttributeKey(TEXT("FieldOfView"));

private:
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(ProjectionMode, TEnumAsByte<ECameraProjectionMode::Type>, UCameraComponent, TEXT("ProjectionMode"));

	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(Width, float, UCameraComponent, TEXT("OrthoWidth"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(NearClipPlane, float, UCameraComponent, TEXT("OrthoNearClipPlane"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(FarClipPlane, float, UCameraComponent, TEXT("OrthoFarClipPlane"));
	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(AspectRatio, float, UCameraComponent, TEXT("AspectRatio"));

	IMPLEMENT_NODE_ATTRIBUTE_DELEGATE_BY_PROPERTYNAME(FieldOfView, float, UCameraComponent, TEXT("FieldOfView"));
};