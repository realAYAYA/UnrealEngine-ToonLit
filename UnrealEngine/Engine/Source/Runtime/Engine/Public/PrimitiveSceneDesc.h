// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrimitiveComponentId.h"
#include "PrimitiveSceneInfoData.h"
#include "Engine/EngineTypes.h"

class FPrimitiveSceneProxy;
struct FPrimitiveSceneProxyDesc;
class HHitProxy;
class IPrimitiveComponent;
class UPrimtiiveComponent;

/**
* FPrimitiveSceneDesc is a structure that can be used to Add/Remove/Update primitives in an FScene. 
* 
* It encapsulates all the necessary information to create/update the primitive. Usage of an PrimitiveComponentInterface
* is optional, but if one is not provided the ProxyDesc must already be created and passed in the ProxyDesc member.
* 
*/

struct FPrimitiveSceneDesc 
{			
	FPrimitiveSceneProxyDesc* ProxyDesc = nullptr;			
	IPrimitiveComponent* PrimitiveComponentInterface = nullptr;
	FPrimitiveSceneInfoData* PrimitiveSceneData = nullptr;
	
	FPrimitiveComponentId LightingAttachmentComponentId;
	FPrimitiveComponentId LodParentComponentId;
	
	bool bShouldAddtoScene = true; // for UpdatePrimitiveAttachment
	bool bRecreateProxyOnUpdateTransform = false; 
	bool bIsUnreachable = false;	
	bool bBulkReregister = false;
		
	EComponentMobility::Type Mobility = EComponentMobility::Movable;	
	FBoxSphereBounds Bounds;
	FBoxSphereBounds LocalBounds;

	FMatrix RenderMatrix;
	FVector AttachmentRootPosition;
	
	UWorld*  World = nullptr;
	UObject* PrimitiveUObject = nullptr;
	
	// @todo: Possibly add uninitialized FStrings in this object to allow overriding the name without having a corresponding UObject
	FString GetFullName() { return PrimitiveUObject->GetFullName(); }
	FString GetName() { return PrimitiveUObject->GetName(); }	

	bool IsUnreachable() { return bIsUnreachable; }
	bool ShouldRecreateProxyOnUpdateTransform() { return bRecreateProxyOnUpdateTransform; }

	FThreadSafeCounter& GetAttachmentCounter() { return PrimitiveSceneData->AttachmentCounter; }
	FPrimitiveComponentId GetPrimitiveSceneId() const { return PrimitiveSceneData->PrimitiveSceneId; }
	int32 GetRegistrationSerialNumber() const { return PrimitiveSceneData->RegistrationSerialNumber; }
	FPrimitiveComponentId GetLODParentId() const { return LodParentComponentId; }
	FPrimitiveComponentId GetLightingAttachmentId() const { return LightingAttachmentComponentId; }

	void SetLODParentId(FPrimitiveComponentId  Id) { LodParentComponentId = Id; }
	void SetLightingAttachmentId(FPrimitiveComponentId  Id) { LightingAttachmentComponentId = Id; }
	
	double GetLastSubmitTime() { return PrimitiveSceneData->LastSubmitTime; }
	void SetLastSubmitTime(double InSubmitTime) { PrimitiveSceneData->LastSubmitTime = InSubmitTime; }
	
	EComponentMobility::Type GetMobility() { return Mobility; }
	FMatrix GetRenderMatrix() { return RenderMatrix; }
	FVector GetActorPositionForRenderer() { return AttachmentRootPosition; }
	UWorld* GetWorld() { return World; }

	FBoxSphereBounds GetBounds() { return Bounds; }
	FBoxSphereBounds GetLocalBounds() { return LocalBounds; }
	
	FPrimitiveSceneProxy* GetSceneProxy() { return PrimitiveSceneData->SceneProxy; }	
	FPrimitiveSceneProxyDesc* GetSceneProxyDesc() {  return ProxyDesc; }
	FPrimitiveSceneInfoData& GetSceneData() { return *PrimitiveSceneData; }

	void ReleaseSceneProxy() { PrimitiveSceneData->SceneProxy = nullptr; }	

	IPrimitiveComponent* GetPrimitiveComponentInterface() { return PrimitiveComponentInterface; }
	const IPrimitiveComponent* GetPrimitiveComponentInterface() const { return PrimitiveComponentInterface; }

	UPackage* GetOutermost() const { return PrimitiveUObject->GetOutermost(); }
};

struct FInstancedStaticMeshSceneDesc
{
	FInstancedStaticMeshSceneDesc(FPrimitiveSceneDesc& InPrimitiveSceneDesc):
		PrimitiveSceneDesc(InPrimitiveSceneDesc)
	{
	}

	operator FPrimitiveSceneDesc*() { return &PrimitiveSceneDesc; }
	FPrimitiveSceneProxy* GetSceneProxy() { return PrimitiveSceneDesc.GetSceneProxy(); }	
	FBoxSphereBounds GetBounds() { return PrimitiveSceneDesc.GetBounds(); }
	FBoxSphereBounds GetLocalBounds() { return PrimitiveSceneDesc.GetLocalBounds(); }
	

	// Using composition to refer to the PrimitiveSceneDesc instead of inheritance for easier 
	// usage of a class member instead of an heap allocated struct in implementers
	FPrimitiveSceneDesc& PrimitiveSceneDesc;
	UStaticMesh* StaticMesh = nullptr;

	UStaticMesh* GetStaticMesh() { check(StaticMesh); return StaticMesh; }
};