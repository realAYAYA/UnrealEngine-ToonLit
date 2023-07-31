// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Misc/Optional.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/MutableMath.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class SExpandableArea;
// Forward declarations
class SMutableCodeViewer;
class STableViewBase;
class SWidget;
namespace mu { struct Curve; }
namespace mu { struct PROGRAM; }
namespace mu { struct PROJECTOR; }
namespace mu { struct SHAPE; }

/**
 * Base Structure to define the different elements used by the lists on this object
 */
struct FMutableConstantElement
{
	/** The index of this element on the host vector. */
	uint32 IndexOnSourceVector;
};

/**
* Cache object used for the generation of the ui elements related to the constant images found on the model
*/
struct FMutableConstantImageElement : public FMutableConstantElement
{
	mu::ImagePtrConst ImagePtr = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant meshes found on the model
*/
struct FMutableConstantMeshElement : public FMutableConstantElement
{
	mu::MeshPtrConst MeshPtr = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant strings found on the model
*/
struct FMutableConstantStringElement : public FMutableConstantElement
{
	const mu::string* MutableString = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant layouts found on the model
*/
struct FMutableConstantLayoutElement : public FMutableConstantElement
{
	mu::LayoutPtrConst Layout = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant skeletons found on the model
*/
struct FMutableConstantSkeletonElement : public FMutableConstantElement
{
	mu::SkeletonPtrConst Skeleton = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant projectors found on the model
*/
struct FMutableConstantProjectorElement : public FMutableConstantElement
{
	const mu::PROJECTOR* Projector = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant matrices found on the model
*/
struct FMutableConstantMatrixElement : public FMutableConstantElement
{ 
	const mu::mat4f* Matrix = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant shapes found on the model
*/
struct FMutableConstantShapeElement : public FMutableConstantElement
{
	const mu::SHAPE* Shape = nullptr;
};

/**
* Cache object used for the generation of the ui elements related to the constant curves found on the model
*/ 
struct FMutableConstantCurveElement : public FMutableConstantElement
{
	const mu::Curve* Curve = nullptr;
};


/**
* Slate panel object designed to hold all the model constants
*/
class SMutableConstantsWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMutableConstantsWidget) {}
	SLATE_END_ARGS()

public:

	/** Builds the widget
	 * @param InArgs - Arguments provided when generating this slate object
	 * @param InMutableProgramPtr - Pointer to the mu::PROGRAM object that holds the constants data.
	 * @param  InMutableCodeViewerPtr - Pointer to the MutableCodeViewer tasked with the previewing of the constant values
	 */
	void Construct(const FArguments& InArgs, const mu::PROGRAM*  InMutableProgramPtr ,  TSharedPtr<SMutableCodeViewer> InMutableCodeViewerPtr);
	
private:

	/** Mutable object containing the constants data */
	const mu::PROGRAM* MutableProgramPtr = nullptr;

	/** Slate capable of accessing the previewer object */
	 TSharedPtr<SMutableCodeViewer> MutableCodeViewerPtr = nullptr;
	
	/**
	 *Sets the back end for the operation of this widget. Each time this is done the ui backend gets updated
	 * @param InProgram - Mutable program object holding all the constants data
	 */
	void SetProgram(const mu::PROGRAM* InProgram);
	
	/*
	* Data backend for the lists of constants
	*/
	
	TArray<TSharedPtr<FMutableConstantImageElement>> ConstantImageElements;
	TArray<TSharedPtr<FMutableConstantMeshElement>> ConstantMeshElements;
	TArray<TSharedPtr<FMutableConstantStringElement>> ConstantStringElements;
	TArray<TSharedPtr<FMutableConstantLayoutElement>> ConstantLayoutElements;
	TArray<TSharedPtr<FMutableConstantProjectorElement>> ConstantProjectorElements;
	TArray<TSharedPtr<FMutableConstantMatrixElement>> ConstantMatrixElements;
	TArray<TSharedPtr<FMutableConstantShapeElement>> ConstantShapeElements;
	TArray<TSharedPtr<FMutableConstantCurveElement>> ConstantCurveElements;
	TArray<TSharedPtr<FMutableConstantSkeletonElement>> ConstantSkeletonElements;

	/**
	 * Load up all the elements with the data found on the mu::PROGRAM object onto TArrays after parsing the data found.
	 */
	void LoadConstantElements();
	
	void LoadConstantStrings();
	void LoadConstantMeshes();
	void LoadConstantImages();
	void LoadConstantLayouts();
	void LoadConstantProjectors();
	void LoadConstantMatrices();
	void LoadConstantShapes();
	void LoadConstantCurves();
	void LoadConstantSkeletons();

	/*
	 * Proxy slates operation objects 
	 */
	
	void OnSelectedStringChanged(TSharedPtr<FMutableConstantStringElement> MutableConstantStringElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedImageChanged(TSharedPtr<FMutableConstantImageElement> MutableConstantImageElement, ESelectInfo::Type SelectionType)const;
	void OnSelectedMeshChanged(TSharedPtr<FMutableConstantMeshElement> MutableConstantMeshElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedLayoutChanged(TSharedPtr<FMutableConstantLayoutElement> MutableConstantLayoutElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedProjectorChanged(TSharedPtr<FMutableConstantProjectorElement> MutableConstantProjectorElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedMatrixChanged(TSharedPtr<FMutableConstantMatrixElement> MutableConstantMatrixElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedShapeChanged(TSharedPtr<FMutableConstantShapeElement> MutableConstantShapeElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedCurveChanged(TSharedPtr<FMutableConstantCurveElement> MutableConstantCurveElement, ESelectInfo::Type SelectionType) const;
	void OnSelectedSkeletonChanged(TSharedPtr<FMutableConstantSkeletonElement> MutableConstantSkeletonElement, ESelectInfo::Type SelectionType) const;
	
	/*
	* UI updating methods
	*/
	
	TSharedRef<ITableRow> OnGenerateStringRow(TSharedPtr<FMutableConstantStringElement> MutableConstantStringElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateImageRow(TSharedPtr<FMutableConstantImageElement> MutableConstantImageElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateMeshRow(TSharedPtr<FMutableConstantMeshElement> MutableConstantMeshElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateLayoutRow(TSharedPtr<FMutableConstantLayoutElement> MutableConstantLayoutElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateProjectorRow(TSharedPtr<FMutableConstantProjectorElement> MutableConstantProjectorElement, const TSharedRef<STableViewBase>& OwnerTable)const;
	TSharedRef<ITableRow> OnGenerateMatrixRow(TSharedPtr<FMutableConstantMatrixElement> MutableConstantMatrixElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateShapeRow(TSharedPtr<FMutableConstantShapeElement> MutableConstantShapeElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateCurveRow(TSharedPtr<FMutableConstantCurveElement> MutableConstantCurveElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> OnGenerateSkeletonRow(TSharedPtr<FMutableConstantSkeletonElement> MutableConstantSkeletonElement, const TSharedRef<STableViewBase>& OwnerTable) const;
	
	/*
	 * Image segment operation 
	 */

	// Part of the original array used do display only a part of the data
	TArray<TSharedPtr<FMutableConstantImageElement>> ConstantImageElementsSegment;

	/** Amount of elements an array segment can hold. The lowest the more stable the UI drawing gets
	*(avoid crash due to oversize slate Y size) */
	const uint32 ElementsPerSegment = 24;

	// Amount of elements a segment can hold. The lowest the more stable the UI drawing gets (avoid crash due to oversize slate Y coordinate)
	uint32 CurrentArraySegment = 0;
	uint32 TotalAmountOfSegments = 0;

	/** Provided the array on ConstantImageElements a new sub array gets generated for the currently selected segment */
	void RegenerateProxyImageArray();
	
	/** Method designed to generate the UI controls to let the user change the image array segment being previewed */
	TSharedRef<SWidget> GenerateImagesSegmentSelectionWidget();

	/** Returns the text containing the page (segment) being drawn at the moment. Designed for the UI to show data to
	 * the user
	 */
	FText OnDrawCurrentImageSegmentText() const;

	/** Determines if the button that loads the previous image array segment should be interactable by the user
	 * It only relates to UI, there are other internal checks to avoid the processing of an existent segment
	 */
	bool ShouldBackButtonBeEnabled() const;
	/** Determines if the button that loads the next image array segment should be interactable by the user
	 * It only relates to UI, there are other internal checks to avoid the processing of an existent segment
	 */
	bool ShouldNextButtonBeEnabled() const;

	/** Action performed when the back button is pressed on the UI. Sets the current segment to the previous one*/
	FReply OnImageBackButtonClicked();
	/** Action performed when the forward button is pressed on the UI. Sets the current segment to the next one*/
	FReply OnImageForwardButtonClicked();

	/** Action performed when the full back button is pressed on the UI. Sets the current segment to the first one*/
	FReply OnImageFullBackButtonClicked();
	/** Action performed when the full forward button is pressed on the UI. Sets the current segment to the last one*/
	FReply OnImageFullForwardButtonClicked();

	
	/*
	 * Image Table sorting methods
	 */

	/** List view showing the constant images data*/
	TSharedPtr<SListView<TSharedPtr<FMutableConstantImageElement>>> ConstantImagesListView;

	/** Id of the last column the user decided to Sort. Usefully in order to interpolate ascending and descending sorting
	 * order 
	 */
	FName LastSortedColumnID = "";
	/** Variable holding what kind of sorting has been used on the last sorting operation*/
	bool bSortAscending = false;

	/** Callback method designed to sort the list of images. It sorts ConstantImageElements */
	void OnImageTableSortRequested(EColumnSortPriority::Type ColumnSortPriority, const FName& ColumnID, EColumnSortMode::Type ColumnSortMode);

	/*
	 * Expandable areas objects and behaviours
	 */

	// Pointers to all expandable areas part of this slate 
	TSharedPtr<SExpandableArea> StringsExpandableArea;
	TSharedPtr<SExpandableArea> ImagesExpandableArea;
	TSharedPtr<SExpandableArea> MeshesExpandableArea;
	TSharedPtr<SExpandableArea> LayoutsExpandableArea;
	TSharedPtr<SExpandableArea> ProjectorsExpandableArea;
	TSharedPtr<SExpandableArea> MatricesExpandableArea;
	TSharedPtr<SExpandableArea> ShapesExpandableArea;
	TSharedPtr<SExpandableArea> CurvesExpandableArea;
	TSharedPtr<SExpandableArea> SkeletonsExpandableArea;

	/** Array with all expandable areas set on this object. Used for dynamic expansion/contraction */
	TArray<TSharedPtr<SExpandableArea>> ExpandableAreas;

	/*
	 * Callback methods called each time one expandable area gets expanded or contracted
	 */
	
	void OnStringsRegionExpansionChanged(bool bExpanded);
	void OnImagesRegionExpansionChanged(bool bExpanded);
	void OnMeshesRegionExpansionChanged(bool bExpanded);
	void OnLayoutsRegionExpansionChanged(bool bExpanded);
	void OnProjectorsRegionExpansionChanged(bool bExpanded);
	void OnMatricesRegionExpansionChanged(bool bExpanded);
	void OnShapesRegionExpansionChanged(bool bExpanded);
	void OnCurvesRegionExpansionChanged(bool bExpanded);
	void OnSkeletonsRegionExpansionChanged(bool bExpanded);
	
	/** Method called each time one expandable area changes expansion state
	 * @param InException - Pointer to the expandable area that will not be contracted while all the others will
	 */
	void ContractExpandableAreas(const TSharedPtr<SExpandableArea>& InException);

	/*
	 * Methods used to get the size of each constant type collection on memory.  
	 */

	// Sizes decomposed on GB, MB, KB and B
	FText ConstantStringsFormattedSize;
	FText ConstantImagesFormattedSize;
	FText ConstantMeshesFormattedSize;
	FText ConstantLayoutsFormattedSize;
	FText ConstantProjectorsFormattedSize;
	FText ConstantMatricesFormattedSize;
	FText ConstantShapesFormattedSize;
	FText ConstantCurvesFormattedSize;
	FText ConstantSkeletonsFormattedSize;
	
	/*
	 * Callback methods used for drawing the titles of each of the constant expandable areas
	 */
	
	FText OnDrawStringsAreaTitle() const;
	FText OnDrawImagesAreaTitle() const;
	FText OnDrawMeshesAreaTitle() const;
	FText OnDrawLayoutsAreaTitle() const;
	FText OnDrawProjectorsAreaTitle() const;
	FText OnDrawMatricesAreaTitle() const;
	FText OnDrawShapesAreaTitle() const;
	FText OnDrawCurvesAreaTitle() const;
	FText OnDrawSkeletonsAreaTitle() const;
	
};



