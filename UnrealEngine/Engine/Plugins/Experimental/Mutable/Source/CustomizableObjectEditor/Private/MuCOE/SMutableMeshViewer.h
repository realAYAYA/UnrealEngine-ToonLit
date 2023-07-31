// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Vector2D.h"
#include "Misc/Optional.h"
#include "MuR/Mesh.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class FReferenceCollector;
class ITableRow;
class SBorder;
class SMutableMeshViewport;
class SMutableSkeletonViewer;
class SScrollBox;
class SSplitter;
class STableViewBase;
class SWidget;
class USkeletalMesh;
namespace mu { class FMeshBufferSet; }
struct FAssetData;


/** Container designed to hold the buffer channel data of a mutable mesh buffer to be later used by the UI */
struct FBufferChannelElement
{
	FText SemanticIndex = FText(INVTEXT(""));
	FText BufferSemantic = FText(INVTEXT(""));
	FText BufferFormat = FText(INVTEXT(""));
	FText BufferComponentCount = FText(INVTEXT(""));
};


/** Element representing a mutable buffer. It contains an array with all the elements representing the channels the mutable buffer is made of.*/
struct FBufferElement
{
	/** The index of the buffer on the origin mutable buffer set */
	FText BufferIndex = FText(INVTEXT(""));

	/** An array of BufferChannels that represent the relative mutable channels on the mutable buffer*/
	TSharedPtr<TArray<TSharedPtr<FBufferChannelElement>>> BufferChannels;
};

/** Widget designed to show the statistical data from a Mutable Mesh*/
class SMutableMeshViewer : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SMutableMeshViewer) {}
		SLATE_ARGUMENT_DEFAULT(mu::MeshPtrConst, Mesh){nullptr};
	SLATE_END_ARGS()

	/** Builds the widget */
	void Construct(const FArguments& InArgs);

	// SWidget interface
	// void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FString GetReferencerName() const override
	{
		return TEXT("SMutableMeshViewer");
	}

	/** Set the Mutable Mesh to be used for this widget */
	void SetMesh(const mu::MeshPtrConst& InMesh);

	
	/** Generate a new SListView for all the ChannelElements provided.
	 * @param InBufferChannelElements - List with all the elements that will be ued to fill InHostListView
	 * @return A SListView containing all the channel elements as UI elements. 
	 */
	TSharedRef<SWidget> GenerateBufferChannelsListView(
		const TSharedPtr<TArray<TSharedPtr<FBufferChannelElement>>>& InBufferChannelElements);

private:
	
	/** Slate whose task is to display the skeleton found on this mesh as a slate tree view */
	TSharedPtr<SMutableSkeletonViewer> MutableSkeletonViewer;

	/** Thumbnail objects used to render a view of the selected skeletal mesh for the mesh viewport*/
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;
	TSharedPtr<FAssetThumbnail> AssetThumbnail;

	/** Data backend for the widget. It represents the mesh that is being "displayed" */
	mu::MeshPtrConst MutableMesh = nullptr;

	/** Is true, mesh has changed and we need to update. */
	bool bIsPendingUpdate = false;

	/** Splitter used to separate the two sides of the slate (tables and viewport) */
	TSharedPtr<SSplitter> SpaceSplitter;

	/** Slate object containing all the buffer tables alongside with the bone tree */
	TSharedPtr<SScrollBox> DataSpaceSlate;

	/** Viewport object to preview the current mesh inside an actual Unreal scene */
	TSharedPtr<SMutableMeshViewport> MeshViewport;
	
	/*
	 * Slate views for the main types of mesh buffers (vertex , index and face)
	 * Each buffer element also contains the channels it uses
	 */
	
	TSharedPtr<SListView<TSharedPtr<FBufferElement>>> VertexBuffersSlateView;
	TSharedPtr<SListView<TSharedPtr<FBufferElement>>> IndexBuffersSlateView;
	TSharedPtr<SListView<TSharedPtr<FBufferElement>>> FaceBuffersSlateView;

private:
	
	/** The border where the object responsible for providing the user of a way to select objects is */
	TSharedPtr<SBorder> ReferenceMeshSelectionSpace;

	/** Generates all slate objects related with the Mesh Viewport Slate */
	TSharedRef<SWidget> GenerateViewportSlates();

	/** Generates the tables showing the buffer data on the mesh alongside with the bone tree found on the mutable mesh */
	TSharedRef<SWidget> GenerateDataTableSlates();
	
	/** Width and height for the skeletal mesh thumbnail */
	FVector2f ThumbnailSize = FVector2f();
	FVector2f WidgetSize = FVector2f();

	// Single property that only draws the combo box widget of the skeletal mesh
	TSharedPtr<class ISinglePropertyView> SkeletalMeshSelector;
	
	/** Method called each time the mesh selected changes so the UI gets updated reliably */
	void OnMeshChanged();

private:
	
	/* Generic UI callbacks used by the Widget */
	FText GetVertexCount() const;
	FText GetFaceCount() const;
	FText GetBoneCount() const;

	/*
	 * Elements used to feed the buffers list (index and buffer channels as an internal list)
	 */
	
	TArray<TSharedPtr<FBufferElement>> VertexBuffers;
	TArray<TSharedPtr<FBufferElement>> IndexBuffers;
	TArray<TSharedPtr<FBufferElement>> FaceBuffers;
	
	/** 
	* Fills the provided TArray with the buffer definitions generated from the mutable buffers found on the provided mutable buffer set
	* @param InMuMeshBufferSetPtr - Mutable buffer set pointer pointing to the buffer set witch buffer data is wanted to be saved on the provided TArray
	* @param OutBuffersDataArray - TArray witch will contain the parsed data from the provided mutable buffer set converted onto FBufferElements
	* @param InHostListView - The slate list using the previously referenced array as data backend. 
	*/
	void FillTargetBufferSetDataArray(const mu::FMeshBufferSet& InMuMeshBufferSetPtr,
	                                  TArray<TSharedPtr<FBufferElement>>& OutBuffersDataArray,
	                                  TSharedPtr<SListView<TSharedPtr<FBufferElement>>>& InHostListView);

	
	/**
	 * Provided an array of buffer elements and the type of buffer it generates a new SListView for said buffer elements
	 * @param InBufferElements - The FBufferElement objects to be used as backend for the SListView
	 * @param InBufferSetTypeName - The name of the buffer set you are generating. Some examples are "Vertex", "index" or "Face"
	 * @param OutHostListView - THe SListView generated.
	 * @return - A shared reference to the SListView generated. Required by this slate operation.
	 */
	TSharedRef<SWidget> GenerateBuffersListView(TSharedPtr<SListView<TSharedPtr<FBufferElement>>>& OutHostListView,
	                                          const TArray<TSharedPtr<FBufferElement>>& InBufferElements,
	                                          const FText& InBufferSetTypeName);
	
	/** Callback method invoked each time a new row of the SListView containing the buffer elements needs to be built
	 * @param InBuffer - The buffer element being used to build the new row.
	 * @param OwnerTable - The table that is going to get the new row added into it.
	 * @return The new row object as the base interface used for all the table rows
	 */
	TSharedRef<ITableRow> OnGenerateBufferRow(TSharedPtr<FBufferElement, ESPMode::ThreadSafe> InBuffer,
											  const TSharedRef<STableViewBase, ESPMode::ThreadSafe>& OwnerTable);
	
	/** 
	* Callback Method responsible of generating each row of the buffer channel lists based on the channel definition provided.
	* @param InBufferChannel - The mesh Buffer channel definition that is being used as blueprint for the actual UI row
	* @param OwnerTable - The parent UI element that is going to be expanded with the new row object
	* @return The new row object as the base interface used for all the table rows
	*/
	TSharedRef<ITableRow> OnGenerateBufferChannelRow(TSharedPtr<FBufferChannelElement> InBufferChannel,
	                                                 const TSharedRef<STableViewBase>& OwnerTable);
	
	
	/*
	 * Reference skeleton mesh selection
	 */

	/** Currently selected reference mesh */
	const USkeletalMesh* SelectedReferenceMesh = nullptr;

	/** Callback invoked when the selected asset on the reference skeletal mesh selection space has changed
	 * @param AssetData - The data of the selected asset on the selection space
	 */
	void OnSelectedReferenceSkeletalMeshChanged(const FAssetData& AssetData);

	/** Filters the assets found in order to know if the asset should be exposed on the UI for usage
	 * @param AssetData - The data of the asset being checked for validation
	 * @return - True means that the asset WILL NOT appear on the list of selectable resources
	 */
	bool OnShouldFilterAsset(const FAssetData& AssetData) const;

	/**
	 * Provides the SObjectPropertyEntryBox a way to know what to display in the selected object space
	 */
	FString OnObjectPath() const;
};
