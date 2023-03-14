// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Components/PrimitiveComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"

class FMaterialRenderProxy;

class ENGINE_API IStaticLightingSystem
{
public:
	virtual const class FMeshMapBuildData* GetPrimitiveMeshMapBuildData(const UPrimitiveComponent* Component, int32 LODIndex) { return nullptr; }
	virtual const class FLightComponentMapBuildData* GetLightComponentMapBuildData(const ULightComponent* Component) { return nullptr; }
	virtual const class FPrecomputedVolumetricLightmap* GetPrecomputedVolumetricLightmap() { return nullptr; }
	virtual ~IStaticLightingSystem() {}
};

class ENGINE_API IStaticLightingSystemImpl
{
public:	
	virtual IStaticLightingSystem* GetStaticLightingSystemForWorld(UWorld* InWorld) { return nullptr; }

	virtual void EditorTick() {}
	virtual bool IsStaticLightingSystemRunning() { return false; }
};

class ENGINE_API FStaticLightingSystemInterface
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FPrimitiveComponentBasedSignature, UPrimitiveComponent* /*InComponent*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FLightComponentBasedSignature, ULightComponentBase* /*InComponent*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FStationaryLightChannelReassignmentSignature, ULightComponentBase* /*InComponent*/, int32 /*NewShadowMapChannel*/);
	DECLARE_MULTICAST_DELEGATE(FLightmassImportanceVolumeModifiedSignature);
	DECLARE_MULTICAST_DELEGATE_OneParam(FMaterialInvalidationSignature, FMaterialRenderProxy* /*Material*/);

	static FPrimitiveComponentBasedSignature OnPrimitiveComponentRegistered;
	static FPrimitiveComponentBasedSignature OnPrimitiveComponentUnregistered;
	static FLightComponentBasedSignature OnLightComponentRegistered;
	static FLightComponentBasedSignature OnLightComponentUnregistered;
	static FStationaryLightChannelReassignmentSignature OnStationaryLightChannelReassigned;
	static FLightmassImportanceVolumeModifiedSignature OnLightmassImportanceVolumeModified;
	static FMaterialInvalidationSignature OnMaterialInvalidated;
	static FSimpleMulticastDelegate OnSkyAtmosphereModified;
	
	static const class FMeshMapBuildData* GetPrimitiveMeshMapBuildData(const UPrimitiveComponent* Component, int32 LODIndex = 0);
	static const class FLightComponentMapBuildData* GetLightComponentMapBuildData(const ULightComponent* Component);
	static const class FPrecomputedVolumetricLightmap* GetPrecomputedVolumetricLightmap(UWorld* World);
	
	static void EditorTick();
	static void GameTick(float DeltaSeconds);
	static bool IsStaticLightingSystemRunning();

	static FStaticLightingSystemInterface* Get();

	void RegisterImplementation(FName Name, IStaticLightingSystemImpl* Impl);
	void UnregisterImplementation(FName Name);
	IStaticLightingSystemImpl* GetPreferredImplementation();

private:
	TMap<FName, IStaticLightingSystemImpl*> Implementations;

	static FStaticLightingSystemInterface* Interface;
};
