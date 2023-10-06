// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "AssetTagItemTypes.h"
#include "CollectionManagerTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "Input/DragAndDrop.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

class SWidget;
struct FAssetData;

class FCollectionDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FCollectionDragDropOp, FDecoratedDragDropOp)

	/** Data for the collections this item represents */
	TArray<FCollectionNameType> Collections;

	static TSharedRef<FCollectionDragDropOp> New(TArray<FCollectionNameType> InCollections, const EAssetTagItemViewMode InAssetTagViewMode = EAssetTagItemViewMode::Standard)
	{
		TSharedRef<FCollectionDragDropOp> Operation = MakeShareable(new FCollectionDragDropOp);
		
		Operation->AssetTagViewMode = InAssetTagViewMode;
		Operation->MouseCursor = EMouseCursor::GrabHandClosed;
		Operation->Collections = MoveTemp(InCollections);
		Operation->Construct();

		return Operation;
	}
	
public:
	/** @return The assets from this drag operation */
	UNREALED_API TArray<FAssetData> GetAssets() const;

	UNREALED_API virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;

private:
	UNREALED_API FText GetDecoratorText() const;

	EAssetTagItemViewMode AssetTagViewMode = EAssetTagItemViewMode::Standard;
};
