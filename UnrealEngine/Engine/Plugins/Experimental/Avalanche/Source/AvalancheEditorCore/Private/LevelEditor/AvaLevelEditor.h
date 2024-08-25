// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaEditor.h"

class FAvaLevelEditorToolbar;
class ILevelEditor;
enum class EMapChangeType : uint8;

class FAvaLevelEditor : public FAvaEditor
{
public:
	UE_AVA_INHERITS(FAvaLevelEditor, FAvaEditor);

	explicit FAvaLevelEditor(FAvaEditorBuilder& Initializer);

	virtual ~FAvaLevelEditor() override;

	void CreateScene();

	bool CanCreateScene() const;

protected:
	//~ Begin FAvaEditor
	virtual void Construct() override;
	virtual void ExtendLayout(FLayoutExtender& InExtender) override;
	//~ End FAvaEditor

	//~ Begin IAvaEditor
	virtual void BindCommands(const TSharedRef<FUICommandList>& InCommandList) override;
	//~ End IAvaEditor

private:
	void TryOpenScene(EAvaEditorObjectQueryType InQueryType = EAvaEditorObjectQueryType::SearchOnly);

	void OnMapChanged(UWorld* InWorld, EMapChangeType InChangeType);

	void OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor);

	TSharedRef<FAvaLevelEditorToolbar> Toolbar;

	FDelegateHandle OnRegisterLayoutHandle;

	FDelegateHandle OnMapChangedHandle;

	FDelegateHandle OnLevelEditorCreatedHandle;
};
