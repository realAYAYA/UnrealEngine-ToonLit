// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "CurveDrawInfo.h"
#include "CurveEditorTypes.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/Platform.h"
#include "ICurveEditorDragOperation.h"
#include "ICurveEditorToolExtension.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Rendering/RenderingCommon.h"
#include "SCurveEditorView.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "CurveEditorSettings.h"

class FCurveEditor;
class FCurveModel;
class FMenuBuilder;
class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class IMenu;
struct FCurveEditorDelayedDrag;
struct FCurveEditorScreenSpace;
struct FCurveEditorToolID;
struct FCurveModelID;
struct FCurvePointHandle;
struct FGeometry;
struct FOptionalSize;
struct FPointerEvent;
struct FKeyAttributes;

namespace CurveViewConstants
{
	/** The default offset from the top-right corner of curve views for curve labels to be drawn. */
	constexpr float CurveLabelOffsetX = 15.f;
	constexpr float CurveLabelOffsetY = 10.f;

	constexpr FLinearColor BufferedCurveColor = FLinearColor(.4f, .4f, .4f);

	/**
	 * Pre-defined layer offsets for specific curve view elements. Fixed values are used to decouple draw order and layering
	 * Some elements deliberately leave some spare layers as a buffer for slight tweaks to layering within that element
	 */
	namespace ELayerOffset
	{
		enum
		{
			Background     = 0,
			GridLines      = 1,
			GridOverlays   = 2,
			GridLabels     = 3,
			Curves         = 10,
			HoveredCurves  = 15,
			Keys           = 20,
			SelectedKeys   = 30,
			Tools          = 35,
			DragOperations = 40,
			Labels         = 45,
			WidgetContent  = 50,
			Last = Labels
		};
	}
}

/**
 */
class CURVEEDITOR_API SInteractiveCurveEditorView : public SCurveEditorView
{
public:

	SLATE_BEGIN_ARGS(SInteractiveCurveEditorView)
		: _BackgroundTint(FLinearColor::White)
		, _MaximumCapacity(0)
		, _AutoSize(true)
	{}

		SLATE_ARGUMENT(FLinearColor, BackgroundTint)

		SLATE_ARGUMENT(int32, MaximumCapacity)

		SLATE_ATTRIBUTE(float, FixedHeight)

		SLATE_ARGUMENT(bool, AutoSize)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor);

	virtual void GetGridLinesX(TSharedRef<const FCurveEditor> CurveEditor, TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels = nullptr) const override;
	virtual void GetGridLinesY(TSharedRef<const FCurveEditor> CurveEditor, TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels = nullptr) const override;

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder, TOptional<FCurvePointHandle> ClickedPoint, TOptional<FCurveModelID> HoveredCurveID);

protected:

	// ~SCurveEditorView Interface
	virtual bool GetPointsWithinWidgetRange(const FSlateRect& WidgetRectangle, TArray<FCurvePointHandle>* OutPoints) const override;
	virtual bool GetCurveWithinWidgetRange(const FSlateRect& WidgetRectangle, TArray<FCurvePointHandle>* OutPoints) const override;
	virtual TOptional<FCurveModelID> GetHoveredCurve() const override;

	virtual FText FormatToolTipCurveName(const FCurveModel& CurveModel) const;
	virtual FText FormatToolTipTime(const FCurveModel& CurveModel, double EvaluatedTime) const;
	virtual FText FormatToolTipValue(const FCurveModel& CurveModel, double EvaluatedValue) const;

	virtual void PaintView(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

protected:

	// SWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	// ~SWidget Interface

protected:

	void DrawBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const;
	void DrawGridLines(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const;
	void DrawCurves(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const;
	void DrawBufferedCurves(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const;

	FSlateColor GetCurveCaptionColor() const;
	FText GetCurveCaption() const;

private:
	void HandleDirectKeySelectionByMouse(TSharedPtr<FCurveEditor> CurveEditor, const FPointerEvent& MouseEvent, TOptional<FCurvePointHandle> MouseDownPoint);

	void CreateContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Updates our distance to all of the curves we represent. */
	void UpdateCurveProximities(FVector2D MousePixel);

	TOptional<FCurvePointHandle> HitPoint(FVector2D MousePixel) const;

	bool IsToolTipEnabled() const;
	FText GetToolTipCurveName() const;
	FText GetToolTipTimeText() const;
	FText GetToolTipValueText() const;

	/**
	 * Returns the proper tangent value so we can keep the curve remain the original shape
	 *
	 * @param InTime		The time we are trying to add a key to
	 * @param InValue		The value we are trying to add a key to
	 * @param CurveToAddTo  The curve we are trying to add a key to
	 * @param DeltaTime		Negative to get the left tangent, positive for right. Remember to use FMath::Abs() when needed
	 * @return				The tangent value relatives to the DeltaTime upon mouse click's position
	 */
	double GetTangentValue(const double InTime, const double InValue, FCurveModel* CurveToAddTo, double DeltaTime) const;

	/*~ Command binding callbacks */
	void AddKeyAtScrubTime(TSet<FCurveModelID> ForCurves);
	void AddKeyAtMousePosition(TSet<FCurveModelID> ForCurves);
	void AddKeyAtTime(const TSet<FCurveModelID>& ToCurves, double InTime);
	void PasteKeys(TSet<FCurveModelID> ToCurves);

	void OnCurveEditorToolChanged(FCurveEditorToolID InToolId);

	/**
	 * Rebind contextual command mappings that rely on the mouse position
	 */
	void RebindContextualActions(FVector2D InMousePosition);

	/** Copy the curves from this view and set them as the Curve Editor's buffered curve support. */
	void BufferCurves();
	/** Attempt to apply the previously buffered curves to the currently selected curves. */
	void ApplyBufferCurves(const bool bSwapBufferCurves);
	/** Check if it's legal to buffer any of our selected curves. */
	bool CanBufferedCurves() const;
	/** Check if it's legal to apply any of the buffered curves to our currently selected curves. */
	bool CanApplyBufferedCurves() const;
	/** Returns interpolation mode and tangent mode based on neighbours or default curve editor if no neighbours . */
	FKeyAttributes GetDefaultKeyAttributesForCurveTime(const FCurveEditor& CurveEditor, const FCurveModel& CurveModel, double EvalTime) const;

protected:

	/** Background tint for this widget */
	FLinearColor BackgroundTint;

private:

	/** (Optional) the current drag operation */
	TOptional<FCurveEditorDelayedDrag> DragOperation;

	struct FCachedToolTipData
	{
		FCachedToolTipData() {}

		FText Text;
		FText EvaluatedValue;
		FText EvaluatedTime;
	};

	TOptional<FCachedToolTipData> CachedToolTipData;

	/** Array of curve proximities in slate units that's updated on mouse move */
	TArray<TTuple<FCurveModelID, float>> CurveProximities;

	/** Track if we have a context menu active. Used to supress hover updates as it causes flickers in the CanExecute bindings. */
	TWeakPtr<IMenu> ActiveContextMenu;

	/** Cached location of the mouse relative to this widget each tick. This is so that command bindings related to the mouse cursor can create them at the right time. */
	FVector2D CachedMousePosition;

	/** Cached curve caption, used to determine when to refresh the retainer */
	mutable FText CachedCurveCaption;

	/** Cached curve caption color, used to determine when to refresh the retainer */
	mutable FSlateColor CachedCurveCaptionColor;

	mutable bool bNeedsRefresh = false;
};