// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "ComponentElementWorldInterface.generated.h"

UCLASS()
class ENGINE_API UComponentElementWorldInterface : public UObject, public ITypedElementWorldInterface
{
	GENERATED_BODY()

public:
	virtual bool CanEditElement(const FTypedElementHandle& InElementHandle) override;
	virtual bool IsTemplateElement(const FTypedElementHandle& InElementHandle) override;
	virtual ULevel* GetOwnerLevel(const FTypedElementHandle& InElementHandle) override;
	virtual UWorld* GetOwnerWorld(const FTypedElementHandle& InElementHandle) override;
	virtual bool GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds) override;
	virtual bool CanMoveElement(const FTypedElementHandle& InElementHandle, const ETypedElementWorldType InWorldType) override;
	virtual bool GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	virtual bool SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	virtual bool GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	virtual bool SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	virtual bool FindSuitableTransformAlongPath(const FTypedElementHandle& InElementHandle, const FVector& InPathStart, const FVector& InPathEnd, const FCollisionShape& InTestShape, TArrayView<const FTypedElementHandle> InElementsToIgnore, FTransform& OutSuitableTransform) override;
};
