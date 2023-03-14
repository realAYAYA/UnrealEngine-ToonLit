// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Misc/Optional.h"
#include "MuR/Image.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Mesh.h"
#include "MuR/Model.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/Operations.h"
#include "MuR/System.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeHash.h"
#include "Types/SlateConstants.h"
#include "Types/SlateEnums.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FMutableCodeTreeElement;
class FReferenceCollector;
class ITableRow;
class SBorder;
class SMutableBoolViewer;
class SMutableColorViewer;
class SMutableConstantsWidget;
class SMutableCurveViewer;
class SMutableImageViewer;
class SMutableIntViewer;
class SMutableLayoutViewer;
class SMutableMeshViewer;
class SMutableParametersWidget;
class SMutableProjectorViewer;
class SMutableScalarViewer;
class SMutableSkeletonViewer;
class SMutableStringViewer;
class STextComboBox;
class SWidget;
namespace mu { struct Curve; }
namespace mu { struct PROJECTOR; }
namespace mu { struct SHAPE; }
struct FGeometry;

/** This widget shows the internal Mutable Code for debugging purposes. 
 * This is not the Unreal source graph in the UCustomizableObject, but the actual Mutable virtual machine graph.
 */
class SMutableCodeViewer :
	public SCompoundWidget,
	public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SMutableCodeViewer) {}

	/** User-visible tag to identify the source of the data shown. */
	SLATE_ARGUMENT(FString, DataTag)
		
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const mu::ModelPtr& InMutableModel /*, const TSharedPtr<SDockTab>& ConstructUnderMajorTab*/);

	// SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:

	/** The Mutable Model that we are showing. */
	mu::ModelPtr MutableModel;
	
	/** Selected model operation for preview. */
	mu::OP::ADDRESS SelectedOperationAddress = 0;

	/** Mutable parameters used in the preview. */
	mu::ParametersPtr PreviewParameters;

	/** Widget showing the parameters that affect the current preview. */
	TSharedPtr<SMutableParametersWidget> ParametersWidget;

	/** Widget showing the constants found on the mutable Model program */
	TSharedPtr<SMutableConstantsWidget> ConstantsWidget;

	/** If true, the parameters have changed and we need to update the preview. */
	bool bIsPreviewPendingUpdate = false;

	/** Widget container where different previews will be created. */
	TSharedPtr<SBorder> PreviewBorder;

	/*
	* Preview windows for the data types exposed by Mutable
	*/

	/** Widget used to show the preview of layout operation results. Once created it is reused to preserve the settings. */
	TSharedPtr<SMutableLayoutViewer> PreviewLayoutViewer;

	/** Widget used to show the preview of image operation results. Once created it is reused to preserve the settings. */
	TSharedPtr<SMutableImageViewer> PreviewImageViewer;

	/** Widget used to show a preview of the mesh and the metadata it holds */
	TSharedPtr<SMutableMeshViewer> PreviewMeshViewer;

	/** Widget used to show a preview of a mutable bool value */
	TSharedPtr<SMutableBoolViewer> PreviewBoolViewer;

	/** Widget used to show a preview of a mutable int value */
	TSharedPtr<SMutableIntViewer> PreviewIntViewer;

	/** Widget used to show a preview of a mutable float value */
	TSharedPtr<SMutableScalarViewer> PreviewScalarViewer;

	/** Widget used to show a preview of a mutable string value */
	TSharedPtr<SMutableStringViewer> PreviewStringViewer;
	
	/** Widget used to show a preview of a mutable color value */
	TSharedPtr<SMutableColorViewer> PreviewColorViewer;

	/** Widget used to display the data held on the mutable projector objects */
	TSharedPtr<SMutableProjectorViewer> PreviewProjectorViewer;

	/** Widget used to display the data held on the mutable skeleton objects */
	TSharedPtr<SMutableSkeletonViewer> PreviewSkeletonViewer;

	/** Widget used to display the data held on the mutable curve objects */
	TSharedPtr<SMutableCurveViewer> PreviewCurveViewer;
	
	/** Tree widget showing the code hierarchically. */
	TSharedPtr<STreeView<TSharedPtr<FMutableCodeTreeElement>>> TreeView;

	/** Root nodes of the tree widget. */
	TArray<TSharedPtr<FMutableCodeTreeElement>> RootNodes;

	/** Cache of tree elements matching the operations that have been generated so far. 
	* We store both the parent and the operation in the key, because a single operation may appear multiple times if it has different parents.
	*/
	struct FItemCacheKey
	{
		mu::OP::ADDRESS Parent=0;
		mu::OP::ADDRESS Child=0;
		uint32 ChildIndexInParent=0;

		friend FORCEINLINE bool operator == (const FItemCacheKey& A, const FItemCacheKey& B)
		{
			return A.Parent == B.Parent && A.Child == B.Child && A.ChildIndexInParent == B.ChildIndexInParent;
		}

		friend FORCEINLINE uint32 GetTypeHash(const FItemCacheKey& Key)
		{
			return HashCombine(Key.Parent, HashCombine(Key.Child, Key.ChildIndexInParent));
		}
	};

	/*
	* Tree widget objects
	*/

	TMap< FItemCacheKey, TSharedPtr<FMutableCodeTreeElement>> ItemCache;

	/** Main tree item for each op. An op can be represented with multiple tree nodes if it is reachable from different paths. */
	TMap< mu::OP::ADDRESS, TSharedPtr<FMutableCodeTreeElement>> MainItemPerOp;

	/** List with all the elements related to nodes displayed on the tree. */
	TArray<TSharedPtr<FMutableCodeTreeElement>> TreeElements;

	/** Array with all the elements that have been manually expanded by the user */
	TMap< mu::OP::ADDRESS, TSharedPtr<FMutableCodeTreeElement>> ExpandedElements;

	/** Control boolean to know if there are any highlighted elements on the tree */
	bool bIsElementHighlighted = false;

	/** Operation that is currently being highlighted */
	mu::OP::ADDRESS HighlightedOperation{};

	/*
	 * Navigation : Operation type navigation Type selection object
	 */
	
	/** Slate object that provides the user a way of selecting what kind of operation it wants to navigate.*/
	TSharedPtr<STextComboBox> TargetedTypeSelector;
	
	/** Operation type we are using to search for tree nodes. Driven primarily by the UI */
	mu::OP_TYPE OperationTypeToSearch = mu::OP_TYPE::NONE;

	/** Array with all the names for each of the operations available on mutable. Used by the UI  */
	TArray<TSharedPtr<FString>> OperationTypesStrings;
	
	/** Stores a list of strings based on the possible types of operations mutable define to be used by the UI */
	void GenerateNavigationOpTypeStrings();

	/** Method called when the user changes the targeted type using the UI. It is only half part of the operation since
	 * "OnOptionTypeSelectionChangedAfterRefresh" will be the one that at the end will perform the operations over the tree.
	 */
	void OnOptionTypeSelectionChanged(TSharedPtr<FString, ESPMode::ThreadSafe> NesSelectedOperationString, ESelectInfo::Type SelectionType);
	
	/** Update the array with all the elements using the current operation. Required when changing the targeted operation type */
	void LocateNavigationElements();
	
	/*
	 * Navigation : UI callback methods
	 */

	FText OnPrintNavigableObjectAddressesCount() const;
	
	/** Method that tells the UI if the bottom to go to the previous element can be interacted
	 * @return It returns true if the UI element should be interactable, false if not.
	 */
	bool CanInteractWithPreviousOperationButton() const;

	/** Method that tells the UI if the bottom to go to the next element can be interacted
	 * @return It returns true if the UI element should be interactable, false if not.
	 */
	bool CanInteractWithNextOperationButton() const;
	

	/** Callback method used to allow the user to directly set the type of operation to be scanning directly by selecting a
	 * operation on the tree and using that operation type as the type to search for
	 */
	void OnSelectedOperationTypeFromTree();

	/*
	 * Navigation : control flags
	 */
	
	/** Flag that tells the system the user does want to select the previous operation of the targeted type. */
	bool bIsSearchingForPreviousElement = false;
	
	/** Flag that tells the system the user does want to select the next operation of the targeted type. */
	bool bIsSearchingForNextElement = false;
	
	/** Flag used by the navigation system to know when it is required the full expansion of the tree.
	 * It allows the system to safely jump from one element to another without the risk of leaving elements in the
	 * middle without being accessed.
	 */
	bool bTreeWasExpanded = false;
	
	/** Flag set to true when the navigation system is automatically scrolling over the graph */
	bool bIsScrolling = false;

	/** Flag that tells the system when the navigation addresses have been updated and therefore action should be
	 * taken after the tree refresh. Currently we look for the first element present on the array of addresses */
	bool bUpdatedNavigationAddresses = false;
	
	/*
	 * Navigation : OP Address based navigation
	 */
	
	/** Collection of Mutable addresses. Cache used during tree navigation.
	 * They serve as the navigation system backend and determine if an element will or not be stopped at during
	 * the navigation process.
	 * @note Not set as a TSet since I want to, in future revisions, be able to trust the order of the elements of the collection.
	 */
	TArray<mu::OP::ADDRESS> NavigationOPAddresses;

	/** Provided an Operation type store as navigation addresses all operations of the targeted Op type
	 * @note The operations found get saved on the collection  NavigationOPAddresses.
	 * @param TargetedOperationType The type of operation we want to cache to be later able to navigate over it.
	 */
	void CacheAddressesOfOperationsOfType(mu::OP_TYPE TargetedOperationType);

	/** Fills an array with all the addresses of operations that do have in common the same targeted operation type
	 * @param TargetOperationType The operation type used to discriminate what operation addresses we want to retrieve.
	 * @param InParentAddress The address of the parent object. Used to get the children and then process them too.
	 * @param OutAddressesOfType Output with all the addresses that are of the same type as TargetOperationType.
	 * @param  AlreadyProcessedAddresses Used during recursive calls. Cache the already processed operations to avoid
	 * processing them more than once.
	 */
	void GetOperationsOfType ( const mu::OP_TYPE& TargetOperationType, const mu::OP::ADDRESS& InParentAddress, const mu::PROGRAM& InProgram,
		TSet<mu::OP::ADDRESS>& OutAddressesOfType,
		TSet<mu::OP::ADDRESS>& AlreadyProcessedAddresses);


	/*
	 * Navigation : Caching of operation types being used on the model
	 */

	/** Operation types present on the currently set mutable model. */
	TArray<mu::OP_TYPE> ModelOperationTypes;

	
	/** Fills ModelOperationTypes with all the types present on the current model.
	 * It also makes sure we have a NONE operation and that the operations are sorted alphabetically.
	 * @note Method designed to be called once per model.*/
	void CacheOperationTypesPresentOnModel();

	
	/** Method to scan over all the operations performed on the current model to produce a set of operation types present on it.
	 * @param InParentAddress Address of the parent object. Required to be able to perform recursive calls to this method (to get the data from the children)
	 * @param  InProgram Mutable program.
	 * @param OutLocatedOperations Output of the method : Set with all the operation types present on the model.
	 * @param AlreadyProcessedAddresses Addresses already processed. Used to avoid processing the same operation twice.
	 */
	void GetOperationTypesPresentOnModel( const mu::OP::ADDRESS& InParentAddress, const mu::PROGRAM& InProgram,
		TSet<mu::OP_TYPE>& OutLocatedOperations,
		TSet<mu::OP::ADDRESS>& AlreadyProcessedAddresses);

	/*
	 * Navigation : Cache operation instances based on constant resource
	 */

public:
	
	/**
	 * Provided a constant resource data type and index on its constant array this method sets all the operations that
	 * make usage of said constant to be navigable by the navigation system.
	 * @note The output of this operation will be cached onto NavigationOPAddresses so we can navigate over them.
	 * @param ConstantDataType The constant data type to locate operations referencing it: It is required for the
	 * system to know what operations should be checked for the usage of the provided constant resource. For example, a
	 * type of mu::DATATYPE::DT_Curve will try to locate operations that we know operate with constant curves.
	 * @param IndexOnConstantsArray The index of the constant on its constants array. If it is, for example, a constant mesh
	 * this value represents the index of the constant inside constantMeshes array on mu::program.
	 */
	void CacheAddressesRelatedWithConstantResource(const mu::DATATYPE ConstantDataType, const int32 IndexOnConstantsArray);

private:

	/** The addresses of the root operations. Cached when this object gets loaded on the Construct method of this slate */
	TArray<mu::OP::ADDRESS> RootNodeAddresses;
	
	/** Utility method that provides you with the addresses of the root nodes of the program */
	void CacheRootNodeAddresses();
	
	/**
	 * Provided a data type and the index of the constant object using said data type locate all operations that do
	 * directly make use of said constant resource.
	 * @param ConstantDataType The type of the resource we are providing an index of. 
	 * @param IndexOnConstantsArray The index of the constant resource we want to know what operations are using it.
	 * @param InProgram The mutable program containing all the operations of the graph.
	 * @param InParentAddress The address of the root object whose child operations we are searching the provided constant on.
	 * @param OutAddressesWithPresence An array with all the addresses to mutable operations making use of the provided constant resource.
	 * @param AlreadyProcessedAddresses Cache with the addresses already processed. It is only intended to be used on recursive calls.
	 */
	void GetOperationsReferencingConstantResource(const mu::DATATYPE ConstantDataType,
	                                              const int32 IndexOnConstantsArray,
	                                              const mu::PROGRAM& InProgram,
	                                              const mu::OP::ADDRESS& InParentAddress,
	                                              TSet<mu::OP::ADDRESS>& OutAddressesWithPresence,
	                                              TSet<mu::OP::ADDRESS>& AlreadyProcessedAddresses);

	/**
	 * Provided an operation address this method tells us if that address is making use of our constant resource or not.
	 * It will take into account the datatype of the provided resource to discard or acknowledge the operation type of the
	 * provided operation address.
	 * @param IndexOnConstantsArray The index onto its respective constants array of the constant resource provided. We do
	 * not give a pointer to the constant resource but the index of it onto its hosting array.
	 * @param ConstantDataType The type of data we know the constant uses. It is used to check for ones or other operations since
	 * not all operations do access the same constant resources.
	 * @param OperationAddress The address of the operation currently being scanned for constant resources.
	 * @param InProgram The mutable program to use to check the type of operation that corresponds to that operation address.
	 * @return True if the operation at OperationAddress does make use of the provided constant resource. False otherwise
	 */
	bool IsConstantResourceUsedByOperation(const int32 IndexOnConstantsArray,
	                                       const mu::DATATYPE ConstantDataType, const mu::OP::ADDRESS OperationAddress,
	                                       const mu::PROGRAM& InProgram) const;
	
	
	/*
	 * Navigation : operation methods and variables
	 */
	
	/** The element on FoundElementsOfType that we are inspecting and using as origin for our search.
	 * @note Do not update it's value manually */
	TSharedPtr< FMutableCodeTreeElement> CurrentNavigationElement;
	
	/** Array with all the elements on the tree of the same type as the one defined on "TargetedOperationType" */
	TArray<TSharedPtr<FMutableCodeTreeElement>> NavigationFoundElements;
	
	/** Copy of the array FoundElementsOfType done before moving on to the next element. Required to check if the
	 * expansion of the tree view did add or remove elements of the type we are looking for.
	 */
	TArray<TSharedPtr<FMutableCodeTreeElement>> NavigationPreviouslyFoundElements;
	
	
	/** Amount of elements the scroll will jump up and down while searching for new elements
	 * It is required to be able to locate elements whose rows have not been yet generated
	 * Do not change.
	 */
	const int32 MinTreeScrollStep = 1;

	/** Variable holding the scroll step to be used. It will vary depending on the amount of elements the tree can display
	 * witch may change from resolution to resolution. This is then the value to be used during the automatic scrolling
	 * operation of the tree navigation system. It will start at the min possible value and only increase when posible.
	 */
	int32 ComputedMaxViewScrollStep = MinTreeScrollStep;

	/** Tree view manipulation method designed to set the element to be selected on the UI side of the tree. It is also
	 *required to take note of what the current found element is to be able to navigate the tree of elements
	 *@param NewlyFoundElement - The element we want to now be using as the NavigationFoundElement */
	void SetCurrentNavigationElement(TSharedPtr<FMutableCodeTreeElement> NewlyFoundElement);

	
	/** Consistency focused method designed to sort the list of navigation elements based on the position of them
	* on the tree view. It is required since new elements may be added or removed due to user activity.
	*/
	void SortNavigationElementsArray();
	
	/** Used by the UI and internally does what is necessary to get to the previous element of the targeted search type.
	 * It is designed to work alongside GoToPreviousOperationAfterRefresh(...)
	 * @return A reply to tell if the UI action has been handled or not.
	 */
	FReply OnGoToPreviousOperationButtonPressed();

	/** Used by the UI and internally does what is necessary to get to the next element of the targeted search type.
	 * It is designed to work alongside GoToNextOperationAfterRefresh(...)
	 * @return A reply to tell if the UI action has been handled or not.
	 */
	FReply OnGoToNextOperationButtonPressed();
	
	/** Makes the currently selected operation be the previous one able to be located on the tree of operations.
	 * @note It must only be called after the tree has been fully refreshed. If not inconsistent behaviour may arise.
	 */
	void GoToPreviousOperationAfterRefresh();
	
	/** Makes the currently selected operation of the targeted type to be the next one. Called after the tree has been
	 * refreshed with new contents (fully refreshed).
	 * @note It must only be called after the tree has been fully refreshed. If not inconsistent behaviour may arise.
	 */
	void GoToNextOperationAfterRefresh();

	/** Method that compares the contents of "PreviousTargetedElementsOfType" against the ones on "CurrentTargetedElementsOfType" to
	 *check if there is any new element found on the current elements array not found on the Previous one
	 * @return True if there is any new element defined on the "CurrentTargetedElementsOfType" not present on "PreviousTargetedElementsOfType",
	 * false if not.
	 */
	bool HaveNewElementsBeenFound() const;
	
	/*
	* Main callbacks from the tree widget standard operation. 
	*/

	/** Callback that generates a new Row object based on a Tree Node*/
 	TSharedRef<ITableRow> GenerateRowForNodeTree(TSharedPtr<FMutableCodeTreeElement> InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	
	/**
	* Provided an element it is returned a list with all the immediate children it has 
	* @param InInfo - The MutableCodeElement whose children objects we are searching
	* @param OutChildren - The children of the provided MutableCodeTreeElement
	*/
	void GetChildrenForInfo(TSharedPtr<FMutableCodeTreeElement> InInfo, TArray< TSharedPtr<FMutableCodeTreeElement> >& OutChildren);

	/** 
	* Callback invoked each time an element of the tree gets expanded or contracted 
	* @param InInfo - The MutableCodeElement to be contracted or expanded
	* @param bInExpanded - Determines if the action is of contraction (false) or expansion (true).
	*/
	void OnExpansionChanged(TSharedPtr<FMutableCodeTreeElement> InInfo, bool bInExpanded);

	/** 
	* Callback invoked each time the selected element changes used to generate the previews depending of the 
	* operation selected
	* @param InInfo - The MutableCodeElement that has been selected. Can be null if unselected
	* @param SelectInfo - Way in witch the selection changed
	*/
	void OnSelectionChanged(TSharedPtr<FMutableCodeTreeElement> InInfo, ESelectInfo::Type SelectInfo);

	/** 
	* Callback invoked when a tree row is getting removed 
	* @param InTreeRow - The tree row that is being removed from the tree view
	*/
	void OnRowReleased(const TSharedRef<ITableRow>& InTreeRow);

	/** Callback invoked when whe press the right mouse button. Useful for adding context menu objects */
	TSharedPtr<SWidget> OnTreeContextMenuOpening();

	void TreeExpandRecursive(TSharedPtr<FMutableCodeTreeElement> Info, bool bExpand );
	
	/** Temporal object designed to be used during the recursive operation of TreeExpandElements() and group
	 * strongly related data
	 */
	struct FProcessedOperationsBuffer
	{
		/** Array with the operation addresses of all original (not duplicates) expanded operations. */
		TArray<mu::OP::ADDRESS> ExpandedOriginalOperations;

		/** Array with the operation addresses of all duplicated expanded operations. */
		TArray<mu::OP::ADDRESS> ExpandedDuplicatedOperations;
	};
	
	/** 
	* Expands the provided elements and all the children they have. 
	* @note It performs the operation by going to the deepest children and expanding from then upwards to the origin before
	* proceeding to the next parent. This way we make sure that the expansion of the elements is following the order
	* of left to right and top to bottom in a "Z" like pattern like the way the tree gets read by humans.
	* @param InElementsToExpand - The root info objects where to start the expansion
	* @param bForceExpandDuplicates - (Optional) Determines if the duplicated objects must be expanded. 
	*	Used for certain situations where we want to expand them. By default they do not get expanded
	* @param FilteringDataType - (Optional) determines what kind of operations should be expanded. By default no
	* filtering is applied
	* @param InExpandedOperationsBuffer (Optional) Object containing all the duplicated and original elements already processed
	* during the subsequent recursive calls to this method.
	*/
	void TreeExpandElements(TArray<TSharedPtr<FMutableCodeTreeElement>>& InElementsToExpand,
		bool bForceExpandDuplicates = false,
		mu::DATATYPE FilteringDataType = mu::DATATYPE::DT_NONE,
		TSharedPtr<FProcessedOperationsBuffer> InExpandedOperationsBuffer = nullptr);
	

	/** Expand only the unique elements (mot duplicates) */
	void TreeExpandUnique();

	/** Expand only the elements using as operation type DT_INSTANCE */
	void TreeExpandInstance();
	
	/** Grabs the selected element and expands all the elements inside the selected branch. Duplicates are ignored */
	void TreeExpandSelected();

	/** 
	* Highlights all tree elements that share the same operation as the element provided
	* @param InTargetElement - The info object to be used as blueprint to search for similar objects and highlight them
	*/
	void HighlightDuplicatesOfEntry(const TSharedPtr<FMutableCodeTreeElement>& InTargetElement);

	/** Clear all the highlighted elements and set them to their default visual state */
	void ClearHighlightedItems();

	/** Called when any parameter value has changed, with the parameter index as argument.  */
	void OnPreviewParameterValueChanged( int32 ParamIndex );

	

	/*
	 * Control of "Skip Mips" for image operations.
	 */

	/** Operation type we are using to search for tree nodes. Driven by the UI */
	int32 MipsToSkip = 0;

	/** */
	bool bSelectedOperationIsImage = false;

	/** Stores a list of strings based on the possible types of operations mutable define to be used by the UI */
	EVisibility IsMipSkipVisible() const;

	/** Stores a list of strings based on the possible types of operations mutable define to be used by the UI */
	TOptional<int32> GetCurrentMipSkip() const;

	/** */
	void OnCurrentMipSkipChanged(int32 NewValue);

	/*
	 * Remote previewer invocation methods
	 */

public:
	void PreviewMutableImage (mu::ImagePtrConst InImagePtr);
	void PreviewMutableMesh (mu::MeshPtrConst InMeshPtr);
	void PreviewMutableLayout(mu::LayoutPtrConst Layout);
	void PreviewMutableSkeleton(mu::SkeletonPtrConst Skeleton);
	void PreviewMutableString(const mu::string* InStringPtr);
	void PreviewMutableProjector(const mu::PROJECTOR* Projector);
	void PreviewMutableMatrix(const mu::mat4f* Mat);
	void PreviewMutableShape(const mu::SHAPE* Shape);
	void PreviewMutableCurve(const mu::Curve* Curve);
	
private:
	void PrepareStringViewer();
	void PrepareImageViewer();
	void PrepareMeshViewer();
	void PrepareLayoutViewer();
	void PrepareProjectorViewer();
};



/** An row of the code tree in the SMutableCodeViewer. */
class FMutableCodeTreeElement : public TSharedFromThis<FMutableCodeTreeElement>
{
public:
	FMutableCodeTreeElement(const mu::ModelPtr& InModel, mu::OP::ADDRESS InOperation, const FString& InCaption, const TSharedPtr<FMutableCodeTreeElement>* InDuplicatedOf = nullptr)
	{
		MutableModel = InModel;
		MutableOperation = InOperation;
		Caption = InCaption;
		if (InDuplicatedOf)
		{
			DuplicatedOf = *InDuplicatedOf;
		}

		// Check what type of operation is (state constant or dynamic resource)
		{
			// If duplicated then grab the already processed data on the original operation
			if (InDuplicatedOf)
			{
				bIsDynamicResource = DuplicatedOf->bIsDynamicResource;
				bIsStateConstant = DuplicatedOf->bIsStateConstant;

				// All required data has been processed so an early exit is required
				return;
			}
			
			// Iterate over all states and try to locate the operation
			const mu::PROGRAM& MutableProgram = InModel->GetPrivate()->m_program;
			for (const mu::PROGRAM::STATE& CurrentState : MutableProgram.m_states)
			{
				// Check if it is a dynamic resource
				for (auto& DynamicResource : CurrentState.m_dynamicResources)
				{
					// If the operation gets located then mark it as dynamic resource
					if (DynamicResource.Key == MutableOperation)
					{
						bIsDynamicResource = true;
						break;
					}
				}
				
				// Early exit: A dynamic resource can not be at the same time a state constant
				if (bIsDynamicResource)
				{
					return;
				}
				
				// Check if it is a state constant
				bIsStateConstant = CurrentState.m_updateCache.Contains(MutableOperation);
			}
		}
		
	}

public:

	/** */
	mu::ModelPtr MutableModel;

	/** Mutable Graph Node represented in this tree row. */
	mu::OP::ADDRESS MutableOperation;

	/** If true means that it will not update when a runtime parameter on the state gets updated */
	bool bIsStateConstant = false;

	/** If true then the mesh or image of this operation may change during the state update */
	bool bIsDynamicResource = false;

	/** Label representing this operation. */
	FString Caption;

	/** If this tree element is a duplicated of another op, this is the op. */
	TSharedPtr<FMutableCodeTreeElement> DuplicatedOf;
};
