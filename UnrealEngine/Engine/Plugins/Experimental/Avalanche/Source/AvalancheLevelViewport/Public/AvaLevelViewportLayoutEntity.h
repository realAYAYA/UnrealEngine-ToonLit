// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelViewportLayout.h"
#include "Templates/SharedPointer.h"

class IAvaViewportDataProxy;
class ILevelEditor;
class SAvaLevelViewportFrame;
struct FAssetEditorViewportConstructionArgs;

class AVALANCHELEVELVIEWPORT_API FAvaLevelViewportLayoutEntity : public ILevelViewportLayoutEntity
{
public:
	FAvaLevelViewportLayoutEntity(const FAssetEditorViewportConstructionArgs& InArgs, TSharedPtr<ILevelEditor> InLevelEditor,
		TSharedPtr<IAvaViewportDataProxy> InDataProxy);

	static FName GetStaticType();

	//~ Begin ILevelViewportLayoutEntity
	virtual TSharedPtr<SLevelViewport> AsLevelViewport() const override;
	virtual TSharedRef<SWidget> AsWidget() const override;
	virtual void SetKeyboardFocus() override;
	virtual void OnLayoutDestroyed() override;
	virtual void SaveConfig(const FString& InConfigSection) override;
	virtual FName GetType() const override;
	virtual void TakeHighResScreenShot() const override;
	virtual FLevelEditorViewportClient& GetLevelViewportClient() const override;
	virtual bool IsPlayInEditorViewportActive() const override;
	virtual void RegisterGameViewportIfPIE() override;
	//~ End ILevelViewportLayoutEntity

private:
	TSharedPtr<SLevelViewport> GetViewportWidget() const;

	TSharedPtr<SAvaLevelViewportFrame> Viewport;
};
