// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"

#include "AssetDefinition_DataflowAsset.generated.h"


namespace DataflowAssetDefinitionHelpers
{
	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool CreateNewDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset);

	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool OpenDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset);

	// Return true if we should proceed, false if we should re-open the dialog
	DATAFLOWEDITOR_API bool NewOrOpenDialog(const UObject* Asset, UObject*& OutDataflowAsset);

	// Create a new UDataflow if one doesn't already exist for the Cloth Asset
	DATAFLOWEDITOR_API UObject* NewOrOpenDataflowAsset(const UObject* Asset);
}

//
// Copy/Paste structs
//
USTRUCT()
struct DATAFLOWEDITOR_API FDataflowNodeData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Type;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Properties;

	UPROPERTY()
	FVector2D Position = FVector2D::ZeroVector;
};

USTRUCT()
struct DATAFLOWEDITOR_API FDataflowCommentNodeData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FVector2D Size = FVector2D::ZeroVector;

	UPROPERTY()
	FLinearColor Color = FLinearColor::White;

	UPROPERTY()
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY()
	int32 FontSize = 18;
};

USTRUCT()
struct DATAFLOWEDITOR_API FDataflowConnectionData
{
	GENERATED_BODY()

	UPROPERTY()
	FString Out;

	UPROPERTY()
	FString In;

	static FString GetNode(const FString InConnection);
	static FString GetProperty(const FString InConnection);
};

USTRUCT()
struct DATAFLOWEDITOR_API FDataflowCopyPasteContent
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FDataflowNodeData> NodeData;

	UPROPERTY()
	TArray<FDataflowCommentNodeData> CommentNodeData;

	UPROPERTY()
	TArray<FDataflowConnectionData> ConnectionData;
};


UCLASS()
class UAssetDefinition_DataflowAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

private:

	virtual FText GetAssetDisplayName() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual UThumbnailInfo* LoadThumbnailInfo(const FAssetData& InAssetData) const override;

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

