// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/NotifyHook.h"
#include "MuCOE/ICustomizableObjectDebugger.h"
#include "Templates/SharedPointer.h"
#include "Toolkits/IToolkit.h"
#include "UObject/GCObject.h"
#include "UObject/NameTypes.h"

class FReferenceCollector;
class UCustomizableObject;
struct FSlateBrush;


DECLARE_DELEGATE(FCreatePreviewInstanceFlagDelegate);


/**
 * CustomizableObject Editor class
 */
class FCustomizableObjectDebugger : 
	public ICustomizableObjectDebugger, 
	public FGCObject,
	public FNotifyHook
{
public:

	/**
	 * Edits the specified object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The object to edit
	 */
	void InitCustomizableObjectDebugger( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCustomizableObject* ObjectToEdit );

	// FGCObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override; 
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectDebugger");
	}

	// IToolkit interface
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// AssetEditorToolkit interface
	virtual bool CanSaveAsset() const override { return false; }
	virtual const FSlateBrush* GetDefaultTabIcon() const override;

	// ICustomizableObjectDebugger interface
	virtual UCustomizableObject* GetCustomizableObject() override { return CustomizableObject; }


private:

	/** The currently viewed object. */
	UCustomizableObject* CustomizableObject = nullptr;

	/**	The tab ids for all the tabs used */
	static const FName MutableNewTabId;


};
