// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FractureTool.h"

#include "FractureToolGenerators.generated.h"

class AGeometryCollectionActor;
class UGeometryCollection;

UCLASS()
class UFractureToolGenerateAsset : public UFractureActionTool
{
public:
	GENERATED_BODY()

	UFractureToolGenerateAsset(const FObjectInitializer& ObjInit) 
		: Super(ObjInit)
		, AssetPath(FString(TEXT("/Game")))
	{}

	// UFractureActionTool Interface
	virtual FText GetDisplayText() const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetToolIcon() const override;
	virtual void RegisterUICommand(FFractureEditorCommands* BindingContext) override;
	virtual void Execute(TWeakPtr<FFractureEditorModeToolkit> InToolkit) override;
	virtual bool CanExecute() const override;

private:
	void OpenGenerateAssetDialog(TArray<AActor*>& Actors);
	void OnGenerateAssetPathChosen(const FString& InAssetPath, bool bAddInternalMaterials, bool bSplitComponents, TArray<AActor*> Actors);
	AGeometryCollectionActor* ConvertActorsToGeometryCollection(const FString& InAssetPath, bool bAddInternalMaterials, bool bSplitComponents, TArray<AActor*>& Actors);
	class AGeometryCollectionActor* CreateNewGeometryActor(const FString& Name, const FTransform& Transform, bool AddMaterials /*= false*/);
	static ULevel* GetSelectedLevel();
	static AActor* AddActor(ULevel* InLevel, UClass* Class);

private:
	TWeakPtr<FFractureEditorModeToolkit> Toolkit;

	UPROPERTY()
	FString AssetPath;
};


UCLASS()
class UFractureToolResetAsset : public UFractureToolGenerateAsset
{
public:
	GENERATED_BODY()

	UFractureToolResetAsset(const FObjectInitializer& ObjInit) : Super(ObjInit) {}

	// UFractureActionTool Interface
	virtual FText GetDisplayText() const override;
	virtual FText GetTooltipText() const override;
	virtual FSlateIcon GetToolIcon() const override;
	virtual void RegisterUICommand(FFractureEditorCommands* BindingContext) override;
	virtual void Execute(TWeakPtr<FFractureEditorModeToolkit> InToolkit) override;
	virtual bool CanExecute() const override;
};


