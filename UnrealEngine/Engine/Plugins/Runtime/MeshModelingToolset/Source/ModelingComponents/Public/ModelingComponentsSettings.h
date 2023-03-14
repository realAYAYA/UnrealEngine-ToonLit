// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "BodySetupEnums.h"
#include "ModelingComponentsSettings.generated.h"



/**
 * Settings for the Modeling Components plug-in. These settings are primarily used to configure two things:
 *   - Behavior of things like optional Rendering features inside Modeling Tools, eg for edit-preview rendering
 *   - Setup of New Mesh Objects emitted by Modeling Tools (eg their default collision settings, etc)
 */
UCLASS(config=EditorPerProjectUserSettings)
class MODELINGCOMPONENTS_API UModelingComponentsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// UDeveloperSettings overrides
	virtual FName GetContainerName() const { return FName("Project"); }
	virtual FName GetCategoryName() const { return FName("Plugins"); }
	virtual FName GetSectionName() const { return FName("Modeling Mode Tools"); }
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

public:

	//
	// Rendering settings applied to Preview Components used during live mesh editing
	//

	/** Enable Realtime Raytracing support for Mesh Editing Tools. This will impact performance of Tools with Real-Time feedback like 3D Sculpting. */
	UPROPERTY(config, EditAnywhere, Category = "Modeling Tools|Rendering")
	bool bEnableRayTracingWhileEditing = false;


	//
	// New Mesh Object settings, for new meshes/assets emitted by Modeling Tools
	//

	/** Enable Raytracing Support for new Mesh Objects created by Modeling Tools, if support is optional (eg DynamicMeshActors) */
	UPROPERTY(config, EditAnywhere, Category = "Modeling Tools|New Mesh Objects")
	bool bEnableRayTracing = false;

	/** Enable Collision Support for new Mesh Objects created by Modeling Tools */
	UPROPERTY(config, EditAnywhere, Category = "Modeling Tools|New Mesh Objects")
	bool bEnableCollision = true;

	/** Default Collision Mode set on new Mesh Objects created by Modeling Tools */
	UPROPERTY(config, EditAnywhere, Category = "Modeling Tools|New Mesh Objects")
	TEnumAsByte<enum ECollisionTraceFlag> CollisionMode = ECollisionTraceFlag::CTF_UseComplexAsSimple;


};