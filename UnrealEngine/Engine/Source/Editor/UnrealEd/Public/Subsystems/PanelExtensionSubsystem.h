// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Templates/SubclassOf.h"
#include "Containers/Queue.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "PanelExtensionSubsystem.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
struct UNREALED_API FPanelExtensionFactory
{
public:
	/** An identifier to allow removal later on. */
	FName Identifier;

	/** Weight used when sorting against other factories for this panel (higher weights sort first) */
	int32 SortWeight = 0;

	/**
	 * Delegate that generates the SExtensionPanel content widget. 
	 * The FWeakObjectPtr param is an opaque context specific to the panel that is being extended
	 * which can be used to customize or populate the extension widget.
	 */
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FCreateExtensionWidget, FWeakObjectPtr);
	FCreateExtensionWidget CreateExtensionWidget;

	UE_DEPRECATED(4.26, "Use FCreateExtensionWidget instead")
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FGenericCreateWidget, const TArray<UObject*>&);

	UE_DEPRECATED(4.26, "Use CreateExtensionWidget instead")
	FGenericCreateWidget CreateWidget;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

class UNREALED_API SExtensionPanel : public SCompoundWidget
{
public:
	~SExtensionPanel();

	SLATE_BEGIN_ARGS(SExtensionPanel)
		: _ExtensionPanelID()
		, _DefaultWidget()
		, _ExtensionContext()
	{}

		/** The ID to identify this Extension point */
		SLATE_ATTRIBUTE(FName, ExtensionPanelID)
		/** The widget to use as content if FPanelExtensionFactory::CreateExtensionWidget returns nullptr */
		SLATE_ATTRIBUTE(TSharedPtr<SWidget>, DefaultWidget)
		/** Context used to customize or populate the extension widget (specific to each panel extension) */
		SLATE_ATTRIBUTE(FWeakObjectPtr, ExtensionContext)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	UObject* GetExtensionContext() const { return ExtensionContext.Get(); }

private:
	void RebuildWidget();

	FName ExtensionPanelID;
	TSharedPtr<SWidget> DefaultWidget;
	FWeakObjectPtr ExtensionContext;
};

/**
 * UPanelExtensionSubsystem
 * Subsystem for creating extensible panels in the Editor
 */
UCLASS()
class UNREALED_API UPanelExtensionSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UPanelExtensionSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterPanelFactory(FName ExtensionPanelID, const FPanelExtensionFactory& InPanelExtensionFactory);
	void UnregisterPanelFactory(FName Identifier, FName ExtensionPanelID = NAME_None);
	bool IsPanelFactoryRegistered(FName Identifier, FName ExtensionPanelID = NAME_None) const;

protected:
	friend class SExtensionPanel;
	TSharedRef<SWidget> CreateWidget(FName ExtensionPanelID, FWeakObjectPtr ExtensionContext);

	DECLARE_MULTICAST_DELEGATE(FPanelFactoryRegistryChanged);
	FPanelFactoryRegistryChanged& OnPanelFactoryRegistryChanged(FName ExtensionPanelID);

private:
	TMap<FName, TArray<FPanelExtensionFactory>> ExtensionPointMap;
	TMap<FName, FPanelFactoryRegistryChanged> PanelFactoryRegistryChangedCallbackMap;

};
