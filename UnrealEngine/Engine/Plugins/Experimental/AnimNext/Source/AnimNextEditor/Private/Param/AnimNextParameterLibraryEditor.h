// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"
#include "GraphEditor.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

enum class ERigVMGraphNotifType : uint8;
class UAnimNextParameterLibrary;
class FDocumentTracker;
class FDocumentTabFactory;
class FTabInfo;
class FTabManager;
class IToolkitHost;
class URigVMGraph;
class URigVMController;
class UDetailsViewWrapperObject;

namespace UE::AnimNext::Editor
{

class FParametersTabSummoner;
class FParametersEditorMode;

namespace ParameterLibraryModes
{
	extern const FName ParameterLibraryEditor;
}

namespace ParameterLibraryTabs
{
	extern const FName Details;
	extern const FName Parameters;
}

class FParameterLibraryEditor : public FWorkflowCentricApplication
{
public:
	FParameterLibraryEditor();
	virtual ~FParameterLibraryEditor() override;

	/** Edits the specified asset */
	void InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextParameterLibrary* InAnimNextParameterLibrary);

private:
	friend class FParameterLibraryEditorMode;
	friend class FParameterLibraryTabSummoner;

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual void InitToolMenuContext(FToolMenuContext& InMenuContext) override;

	// FAssetEditorToolkit interface
	virtual void GetSaveableObjects(TArray<UObject*>& OutObjects) const override;

	void BindCommands();

	void ExtendMenu();

	void ExtendToolbar();

	void SetSelectedObjects(TArray<UObject*> InObjects);

	void HandleDetailsViewCreated(TSharedRef<IDetailsView> InDetailsView)
	{
		DetailsView = InDetailsView;
	}

	// The asset we are editing
	UAnimNextParameterLibrary* ParameterLibrary = nullptr;

	// Our details panel
	TSharedPtr<IDetailsView> DetailsView;
};

}