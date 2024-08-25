// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "InputState.h"
#include "InteractiveToolManager.h"
#include "EdModeInteractiveToolsContext.h"

#include "ScriptableToolsEditorMode.generated.h"

class FUICommandList;
class FLevelObjectsObserver;
class UModelingSceneSnappingManager;
class UModelingSelectionInteraction;
class UGeometrySelectionManager;
class UScriptableToolSet;
class UBlueprint;

UCLASS(Transient)
class SCRIPTABLETOOLSEDITORMODE_API UScriptableToolsEditorMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()
public:
	const static FEditorModeID EM_ScriptableToolsEditorModeId;

	UScriptableToolsEditorMode();
	UScriptableToolsEditorMode(FVTableHelper& Helper);
	~UScriptableToolsEditorMode();
	////////////////
	// UEdMode interface
	////////////////

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;

	virtual void ActorSelectionChangeNotify() override;

	virtual bool ShouldDrawWidget() const override;
	virtual bool ProcessEditDelete() override;
	virtual bool ProcessEditCut() override;

	virtual bool CanAutoSave() const override;

	virtual bool ComputeBoundingBoxForViewportFocus(AActor* Actor, UPrimitiveComponent* PrimitiveComponent, FBox& InOutBox) const override;

	virtual bool GetPivotForOrbit(FVector& OutPivot) const override;

	/*
	 * focus events
	 */

	// called when we "start" this editor mode (ie switch to this tab)
	virtual void Enter() override;

	// called when we "end" this editor mode (ie switch to another tab)
	virtual void Exit() override;

	virtual bool ShouldToolStartBeAllowed(const FString& ToolIdentifier) const override;

	//////////////////
	// End of UEdMode interface
	//////////////////


protected:
	virtual void BindCommands() override;
	virtual void CreateToolkit() override;
	virtual void OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	virtual void OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool) override;
	
	virtual void OnToolPostBuild(UInteractiveToolManager* InToolManager, EToolSide InSide, UInteractiveTool* InBuiltTool, UInteractiveToolBuilder* InToolBuilder, const FToolBuilderState& ToolState);

	void OnToolsContextRender(IToolsContextRenderAPI* RenderAPI);

	void FocusCameraAtCursorHotkey();

	void AcceptActiveToolActionOrTool();
	void CancelActiveToolActionOrTool();

	void ConfigureRealTimeViewportsOverride(bool bEnable);

	void OnBlueprintPreCompile(UBlueprint* Blueprint);
	FDelegateHandle BlueprintPreCompileHandle;

	void OnBlueprintCompiled();
	FDelegateHandle BlueprintCompiledHandle;

protected:
	UPROPERTY()
	TObjectPtr<UScriptableToolSet> ScriptableTools;
public:
	virtual UScriptableToolSet* GetActiveScriptableTools() { return ScriptableTools; }

};
