// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdPrim.h"
#include "Widgets/IUSDTreeViewItem.h"

#include "UsdWrappers/ForwardDeclarations.h"
#include "UsdWrappers/UsdStage.h"
#include "UsdWrappers/UsdPrim.h"

#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"

using FUsdPrimViewModelRef = TSharedRef< class FUsdPrimViewModel >;
using FUsdPrimViewModelPtr = TSharedPtr< class FUsdPrimViewModel >;

class USDSTAGEEDITORVIEWMODELS_API FUsdPrimModel : public TSharedFromThis< FUsdPrimModel >
{
public:
	FText GetName() const { return Name; }
	FText GetType() const { return Type; }
	bool HasPayload() const { return bHasPayload; }
	bool IsLoaded() const { return bIsLoaded; }
	bool HasCompositionArcs() const { return bHasCompositionArcs; }
	bool IsVisible() const { return bIsVisible; }

	FText Name;
	FText Type;
	bool bHasPayload = false;
	bool bIsLoaded = false;
	bool bHasCompositionArcs = false;
	bool bIsVisible = true;
};

class USDSTAGEEDITORVIEWMODELS_API FUsdPrimViewModel : public IUsdTreeViewItem
{
public:
	FUsdPrimViewModel( FUsdPrimViewModel* InParentItem, const UE::FUsdStageWeak& InUsdStage, const UE::FUsdPrim& InPrim = {} );

	TArray< FUsdPrimViewModelRef >& UpdateChildren();

	void FillChildren();

	void RefreshData( bool bRefreshChildren );

	bool HasVisibilityAttribute() const;
	void ToggleVisibility();
	void TogglePayload();

	void ApplySchema( FName SchemaName );
	bool CanApplySchema( FName SchemaName ) const;
	void RemoveSchema( FName SchemaName );
	bool CanRemoveSchema( FName SchemaName ) const;

	// Returns true if this prim has at least one spec on the stage's local layer stack
	bool HasSpecsOnLocalLayer() const;

	void DefinePrim( const TCHAR* PrimName );

	/** Delegate for hooking up an inline editable text block to be notified that a rename is requested. */
	DECLARE_DELEGATE( FOnRenameRequest );

	/** Broadcasts whenever a rename is requested */
	FOnRenameRequest RenameRequestEvent;

public:
	void ClearReferences();
	void ClearPayloads();

public:
	UE::FUsdStageWeak UsdStage;
	UE::FUsdPrim UsdPrim;

	FUsdPrimViewModel* ParentItem;
	TArray< FUsdPrimViewModelRef > Children;

	TSharedRef< FUsdPrimModel > RowData; // Data model

	bool bIsRenamingExistingPrim = false;
};
