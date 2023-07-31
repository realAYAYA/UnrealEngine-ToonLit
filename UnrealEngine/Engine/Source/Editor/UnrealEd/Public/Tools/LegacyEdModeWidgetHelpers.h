// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"
#include "Math/Vector.h"
#include "Tools/LegacyEdModeInterfaces.h"
#include "Tools/UEdMode.h"

#include "LegacyEdModeWidgetHelpers.generated.h"

class FCanvas;
class FEditorModeTools;
class FEditorViewportClient;
class FModeTool;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
struct FConvexVolume;

enum EModeTools : int8;
class HHitProxy;
struct FViewportClick;
class UObject;

class UNREALED_API FLegacyEdModeWidgetHelper
{
	friend class FEditorModeRegistry;
	friend class UBaseLegacyWidgetEdMode;

public:
	FLegacyEdModeWidgetHelper();
	virtual ~FLegacyEdModeWidgetHelper() = default;

	virtual bool AllowWidgetMove();
	virtual bool CanCycleWidgetMode() const;
	virtual bool ShowModeWidgets() const;
	virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const;
	virtual FVector GetWidgetLocation() const;
	virtual bool ShouldDrawWidget() const;
	virtual bool UsesTransformWidget() const;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const;
	virtual FVector GetWidgetNormalFromCurrentAxis(void* InData);
	virtual void SetCurrentWidgetAxis(EAxisList::Type InAxis) final;
	virtual EAxisList::Type GetCurrentWidgetAxis() const final;
	virtual bool UsesPropertyWidgets() const;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData);
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData);
	
	virtual void ActorSelectionChangeNotify();
	virtual bool AllowsViewportDragTool() const;

	// Property Widgets
	/** Structure that holds info about our optional property widget */
	struct FPropertyWidgetInfo
	{
		FString PropertyName;
		int32 PropertyIndex;
		FName PropertyValidationName;
		FString DisplayName;
		bool bIsTransform;

		FPropertyWidgetInfo()
			: PropertyIndex(INDEX_NONE)
			, bIsTransform(false)
		{
		}

		void GetTransformAndColor(UObject* BestSelectedItem, bool bIsSelected, FTransform& OutLocalTransform, FString& OutValidationMessage, FColor& OutDrawColor) const;
	};

	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale);
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas);

	// @TODO: Find a better home for these?
	/** Value of UPROPERTY can be edited with a widget in the editor (translation, rotation) */
	static const FName MD_MakeEditWidget;
	/** Specifies a function used for validation of the current value of a property.  The function returns a string that is empty if the value is valid, or contains an error description if the value is invalid */
	static const FName MD_ValidateWidgetUsing;
	/** Returns true if this structure can support creating a widget in the editor */
	static bool CanCreateWidgetForStructure(const UStruct* InPropStruct);
	/** Returns true if this property can support creating a widget in the editor */
	static bool CanCreateWidgetForProperty(FProperty* InProp);
	/** See if we should create a widget for the supplied property when selecting an actor instance */
	static bool ShouldCreateWidgetForProperty(FProperty* InProp);

protected:
	/**
	 * Returns the first selected Actor, or NULL if there is no selection.
	 */
	AActor* GetFirstSelectedActorInstance() const;

	/**
	 * Gets an array of property widget info structures for the given struct/class type for the given container.
	 *
	 * @param InStruct The type of structure/class to access widget info structures for.
	 * @param InContainer The container of the given type.
	 * @param OutInfos An array of widget info structures (output).
	 */
	void GetPropertyWidgetInfos(const UStruct* InStruct, const void* InContainer, TArray<FPropertyWidgetInfo>& OutInfos) const;

	/** Finds the best item to display widgets for (preferring selected components over selected actors) */
	virtual UObject* GetItemToTryDisplayingWidgetsFor(FTransform& OutWidgetToWorld) const;

	/** Name of the property currently being edited */
	FString EditedPropertyName;

	/** If the property being edited is an array property, this is the index of the element we're currently dealing with */
	int32 EditedPropertyIndex;

	/** Indicates  */
	bool bEditedPropertyIsTransform;

	/** The current axis that is being dragged on the widget. */
	EAxisList::Type CurrentWidgetAxis;

	/** Pointer back to the mode tools that we are registered with */
	FEditorModeTools* Owner;

	TScriptInterface<ILegacyEdModeWidgetInterface> ParentModeInterface;
};

// This class is to aid transitioning from native FEdModes to UEdModes, in the case that the FEdMode used property widgets and/or transform widgets
// To use this class:
//   1. Subclass FLegacyEdModeWidgetHelper, and override the methods needed for your native FEdMode implementation
//   2. Transition your native FEdMode to a UObject, by inheriting from this class
//   3. Override CreateWidgetHelper function to return a SharedRef to the class you created in step 1.
UCLASS(Abstract)
class UNREALED_API UBaseLegacyWidgetEdMode : public UEdMode, public ILegacyEdModeWidgetInterface, public ILegacyEdModeViewportInterface
{
	GENERATED_BODY()

public:
	// UEdMode overrides
	// If you need to do any initialization in your mode, be sure to still call through to this function.
	// It creates the WidgetHelper and hooks up the mode manager pointer for you.
	virtual void Initialize() override;
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override;
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

	// ILegacyEdModeWidgetInterface overrides are just a pass through to the FLegacyEdModeWidgetHelper
	virtual bool AllowWidgetMove() override;
	virtual bool CanCycleWidgetMode() const override;
	virtual bool ShowModeWidgets() const override;
	virtual EAxisList::Type GetWidgetAxisToDraw(UE::Widget::EWidgetMode InWidgetMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual bool ShouldDrawWidget() const override;
	virtual bool UsesTransformWidget() const override;
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;
	virtual FVector GetWidgetNormalFromCurrentAxis(void* InData) override;
	virtual void SetCurrentWidgetAxis(EAxisList::Type InAxis) override;
	virtual EAxisList::Type GetCurrentWidgetAxis() const override;
	virtual bool UsesPropertyWidgets() const override;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual bool GetCustomInputCoordinateSystem(FMatrix& InMatrix, void* InData) override;
	virtual void ActorSelectionChangeNotify() override;
	virtual bool AllowsViewportDragTool() const override;

protected:
	virtual TSharedRef<FLegacyEdModeWidgetHelper> CreateWidgetHelper();

	TSharedPtr<FLegacyEdModeWidgetHelper> WidgetHelper;
};
