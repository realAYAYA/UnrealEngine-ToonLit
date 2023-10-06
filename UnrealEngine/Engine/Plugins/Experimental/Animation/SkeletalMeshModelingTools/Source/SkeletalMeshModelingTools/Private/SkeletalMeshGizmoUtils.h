// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Containers/UnrealString.h"
#include "SkeletalMesh/SkeletalMeshEditionInterface.h"

#include "SkeletalMeshGizmoUtils.generated.h"

class UInteractiveToolsContext;
class UInteractiveToolManager;
class UTransformGizmo;
class USkeletonTransformProxy;
struct FGizmoCustomization;

namespace UE
{
	
namespace SkeletalMeshGizmoUtils
{
	/**
	 * The functions below are helper functions that simplify usage of a USkeletalMeshGizmoContextObject
	 * that is registered as a ContextStoreObject in an InteractiveToolsContext
	 */

	/**
	 * If one does not already exist, create a new instance of USkeletalMeshGizmoContextObject and add it to the
	 * ToolsContext's ContextObjectStore
	 * @return true if the ContextObjectStore now has a USkeletalMeshGizmoContextObject (whether it already existed, or was created)
	 */
	SKELETALMESHMODELINGTOOLS_API bool RegisterTransformGizmoContextObject(UInteractiveToolsContext* InToolsContext);

	/**
	 * Remove any existing USkeletalMeshGizmoContextObject from the ToolsContext's ContextObjectStore
	 * @return true if the ContextObjectStore no longer has a USkeletalMeshGizmoContextObject (whether it was removed, or did not exist)
	 */
	SKELETALMESHMODELINGTOOLS_API bool UnregisterTransformGizmoContextObject(UInteractiveToolsContext* InToolsContext);

	/**
	 * Spawn an editor like Transform Gizmo. ToolManager's ToolsContext must have a USkeletalMeshGizmoContextObject registered 
	 */
	SKELETALMESHMODELINGTOOLS_API UTransformGizmo* CreateTransformGizmo(UInteractiveToolManager* ToolManager, void* Owner = nullptr);
}
	
}

/**
 * USkeletalMeshGizmoWrapper is a wrapper class to handle a Transform Gizmo and it's Transform proxy so that it can
 * be used to update skeletal mesh infos using a Gizmo.
 */

UCLASS()
class SKELETALMESHMODELINGTOOLS_API USkeletalMeshGizmoWrapper : public USkeletalMeshGizmoWrapperBase
{
	GENERATED_BODY()
public:
	virtual void Initialize(const FTransform& InTransform = FTransform::Identity, const EToolContextCoordinateSystem& InTransformMode = EToolContextCoordinateSystem::Local) override;
	virtual void HandleBoneTransform(FGetTransform GetTransformFunc, FSetTransform SetTransformFunc) override;
	virtual void Clear() override;

	virtual bool CanInteract() const override;
	virtual bool IsGizmoHit(const FInputDeviceRay& PressPos) const override;

	UPROPERTY()
	TObjectPtr<UTransformGizmo> TransformGizmo;
	UPROPERTY()
	TObjectPtr<USkeletonTransformProxy> TransformProxy;
};

/**
 * USkeletalMeshGizmoContextObject is a utility object that registers a Gizmo Builder for UTransformGizmo.
 * (see UCombinedTransformGizmoContextObject for more details)
 */

UCLASS()
class SKELETALMESHMODELINGTOOLS_API USkeletalMeshGizmoContextObject : public USkeletalMeshGizmoContextObjectBase
{
	GENERATED_BODY()

public:

	// builder identifiers for transform gizmo
	static const FString& TransformBuilderIdentifier();
	
	void RegisterGizmosWithManager(UInteractiveToolManager* InToolManager);
	void UnregisterGizmosWithManager(UInteractiveToolManager* InToolManager);

	virtual USkeletalMeshGizmoWrapperBase* GetNewWrapper(UInteractiveToolManager* InToolManager, UObject* Outer = nullptr, IGizmoStateTarget* InStateTarget = nullptr) override;

private:
	
	static const FGizmoCustomization& GetGizmoCustomization();
	
	bool bGizmosRegistered = false;
};
