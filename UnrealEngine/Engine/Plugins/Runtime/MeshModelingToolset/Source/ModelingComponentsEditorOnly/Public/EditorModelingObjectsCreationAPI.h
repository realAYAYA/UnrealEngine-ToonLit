// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelingObjectsCreationAPI.h"

#include "EditorModelingObjectsCreationAPI.generated.h"

class UInteractiveToolsContext;

/**
 * Implementation of UModelingObjectsCreationAPI suitable for use in UE Editor.
 * - CreateMeshObject() currently creates a StaticMesh Asset/Actor
 * - CreateTextureObject() currently creates a UTexture2D Asset
 * 
 * This is intended to be registered in the ToolsContext ContextObjectStore.
 * Static utility functions ::Register() / ::Find() / ::Deregister() can be used to do this in a consistent way.
 * 
 * Several client-provided callbacks can be used to customize functionality (eg in Modeling Mode) 
 *  - GetNewAssetPathNameCallback is called to determine an asset path. This can be used to do
 *    things like pop up an interactive path-selection dialog, use project-defined paths, etc
 *  - OnModelingMeshCreated is broadcast for each new created mesh object
 *  - OnModelingTextureCreated is broadcast for each new created texture object
 */
UCLASS()
class MODELINGCOMPONENTSEDITORONLY_API UEditorModelingObjectsCreationAPI : public UModelingObjectsCreationAPI
{
	GENERATED_BODY()
public:

	// UFUNCTION(BlueprintCallable, Category = "Modeling Objects")
	virtual FCreateMeshObjectResult CreateMeshObject(const FCreateMeshObjectParams& CreateMeshParams) override;

	// UFUNCTION(BlueprintCallable, Category = "Modeling Objects")
	virtual FCreateTextureObjectResult CreateTextureObject(const FCreateTextureObjectParams& CreateTexParams) override;


	//
	// Non-UFunction variants that support std::move operators
	//

	virtual bool HasMoveVariants() const { return true; }

	virtual FCreateMeshObjectResult CreateMeshObject(FCreateMeshObjectParams&& CreateMeshParams) override;

	virtual FCreateTextureObjectResult CreateTextureObject(FCreateTextureObjectParams&& CreateTexParams) override;


	//
	// Callbacks that editor can hook into to handle asset creation
	//

	DECLARE_DELEGATE_RetVal_ThreeParams(FString, FGetAssetPathNameCallbackSignature, const FString& BaseName, const UWorld* TargetWorld, FString SuggestedFolder);
	FGetAssetPathNameCallbackSignature GetNewAssetPathNameCallback;

	DECLARE_MULTICAST_DELEGATE_OneParam(FModelingMeshCreatedSignature, const FCreateMeshObjectResult& CreatedInfo);
	FModelingMeshCreatedSignature OnModelingMeshCreated;

	DECLARE_MULTICAST_DELEGATE_OneParam(FModelingTextureCreatedSignature, const FCreateTextureObjectResult& CreatedInfo);
	FModelingTextureCreatedSignature OnModelingTextureCreated;



	//
	// Utility functions to handle registration/unregistration
	//

	static UEditorModelingObjectsCreationAPI* Register(UInteractiveToolsContext* ToolsContext);
	static UEditorModelingObjectsCreationAPI* Find(UInteractiveToolsContext* ToolsContext);
	static bool Deregister(UInteractiveToolsContext* ToolsContext);



	//
	// internal implementations called by public functions
	//
	FCreateMeshObjectResult CreateStaticMeshAsset(FCreateMeshObjectParams&& CreateMeshParams);
	FCreateMeshObjectResult CreateVolume(FCreateMeshObjectParams&& CreateMeshParams);
	FCreateMeshObjectResult CreateDynamicMeshActor(FCreateMeshObjectParams&& CreateMeshParams);

	TArray<UMaterialInterface*> FilterMaterials(const TArray<UMaterialInterface*>& MaterialsIn);
};
