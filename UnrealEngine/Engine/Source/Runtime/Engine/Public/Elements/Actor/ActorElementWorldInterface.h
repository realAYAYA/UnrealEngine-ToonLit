// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "ActorElementWorldInterface.generated.h"

struct FCollisionQueryParams;

UCLASS(MinimalAPI)
class UActorElementWorldInterface : public UObject, public ITypedElementWorldInterface
{
	GENERATED_BODY()

public:
	ENGINE_API virtual bool IsTemplateElement(const FTypedElementHandle& InElementHandle) override;
	ENGINE_API virtual ULevel* GetOwnerLevel(const FTypedElementHandle& InElementHandle) override;
	ENGINE_API virtual UWorld* GetOwnerWorld(const FTypedElementHandle& InElementHandle) override;
	ENGINE_API virtual bool GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds) override;
	ENGINE_API virtual bool CanMoveElement(const FTypedElementHandle& InElementHandle, const ETypedElementWorldType InWorldType) override;
	ENGINE_API virtual bool CanScaleElement(const FTypedElementHandle& InElementHandle) override;
	ENGINE_API virtual bool GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	ENGINE_API virtual bool SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	ENGINE_API virtual bool GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform) override;
	ENGINE_API virtual bool SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform) override;
	ENGINE_API virtual bool FindSuitableTransformAtPoint(const FTypedElementHandle& InElementHandle, const FTransform& InPotentialTransform, FTransform& OutSuitableTransform) override;
	ENGINE_API virtual bool FindSuitableTransformAlongPath(const FTypedElementHandle& InElementHandle, const FVector& InPathStart, const FVector& InPathEnd, const FCollisionShape& InTestShape, TArrayView<const FTypedElementHandle> InElementsToIgnore, FTransform& OutSuitableTransform) override;
	ENGINE_API virtual TArray<FTypedElementHandle> GetSelectionElementsFromSelectionFunction(const FTypedElementHandle& InElementHandle, const FWorldSelectionElementArgs& SelectionArgs, const TFunction<bool(const FTypedElementHandle&, const FWorldSelectionElementArgs&)>& SelectionFunction) override;
	static ENGINE_API bool FindSuitableTransformAlongPath_WorldSweep(const UWorld* InWorld, const FVector& InPathStart, const FVector& InPathEnd, const FCollisionShape& InTestShape, TArrayView<const FTypedElementHandle> InElementsToIgnore, FCollisionQueryParams& InOutParams, FTransform& OutSuitableTransform);
	static ENGINE_API void AddIgnoredCollisionQueryElement(const FTypedElementHandle& InElementHandle, FCollisionQueryParams& InOutParams);
};
