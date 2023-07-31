// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UnrealWidgetFwd.h"
#include "AssetEditorGizmoFactory.h"

#include "GizmoEdMode.generated.h"

class UCombinedTransformGizmo;
class UInteractiveGizmo;
class UInteractiveGizmoManager;

UCLASS()
class GIZMOEDMODE_API UGizmoEdModeSettings : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class GIZMOEDMODE_API UGizmoEdMode : public UEdMode
{
	GENERATED_BODY()
public:
	UGizmoEdMode();

	void AddFactory(TScriptInterface<IAssetEditorGizmoFactory> GizmoFactory);
	virtual bool UsesToolkits() const override
	{
		return false;
	}

private:
	void ActorSelectionChangeNotify() override;
	void Enter() override;
	void Exit() override;
	void ModeTick(float DeltaTime) override;

	bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }

	void RecreateGizmo();
	void DestroyGizmo();

	UPROPERTY()
	TArray<TScriptInterface<IAssetEditorGizmoFactory>> GizmoFactories;
	IAssetEditorGizmoFactory* LastFactory = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<UInteractiveGizmo>> InteractiveGizmos;

	FDelegateHandle WidgetModeChangedHandle;

	bool bNeedInitialGizmos{false};
};
