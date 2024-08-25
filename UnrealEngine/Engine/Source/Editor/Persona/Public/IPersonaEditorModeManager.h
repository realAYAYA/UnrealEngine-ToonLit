// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetEditorModeManager.h"

#include "IPersonaEditorModeManager.generated.h"

UINTERFACE(MinimalAPI)
class UPersonaManagerContext : public UInterface
{
	GENERATED_BODY()
};

/** Persona-specific extensions to the asset editor mode manager */
class IPersonaManagerContext
{
	GENERATED_BODY()
public:
	/** 
	 * Get a camera target for when the user focuses the viewport
	 * @param OutTarget		The target object bounds
	 * @return true if the location is valid
	 */
	virtual bool GetCameraTarget(FSphere& OutTarget) const = 0;

	/** 
	 * Get debug info for any editor modes that are active
	 * @param	OutDebugText	The text to draw
	 */
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const = 0;
};

class IPersonaEditorModeManager;

UCLASS()
class PERSONA_API UPersonaEditorModeManagerContext : public UObject, public IPersonaManagerContext
{
	GENERATED_BODY()
public:
	// Only for use by FAnimationViewportClient::GetPersonaModeManager for compatibility
	IPersonaEditorModeManager* GetPersonaEditorModeManager() const;
	virtual bool GetCameraTarget(FSphere& OutTarget) const override;
	virtual void GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const override;
private:
	IPersonaEditorModeManager* ModeManager = nullptr;
	static UPersonaEditorModeManagerContext* CreateFor(IPersonaEditorModeManager* InModeManager)
	{
		UPersonaEditorModeManagerContext* NewPersonaContext = NewObject<UPersonaEditorModeManagerContext>();
		NewPersonaContext->ModeManager = InModeManager;
		return NewPersonaContext;
	}
	friend IPersonaEditorModeManager;
};

class IPersonaEditorModeManager : public FAssetEditorModeManager, public IPersonaManagerContext
{
public:
	IPersonaEditorModeManager();
	virtual ~IPersonaEditorModeManager() override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
private:
	TObjectPtr<UPersonaEditorModeManagerContext> PersonaModeManagerContext{UPersonaEditorModeManagerContext::CreateFor(this)};
};
