// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableConstantsWidget.h"

#include "Algo/StableSort.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuR/Image.h"
#include "MuR/Layout.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/Skeleton.h"
#include "MuT/TypeInfo.h"
#include "SlotBase.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STileView.h"

class ITableRow;
class SWidget;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

// Private namespace with utility functionality used by this slate object
namespace
{
	/** Provided a byte count this function proceeds to output that byte value as bytes, kilobytes, megabytes and gigabytes.
	 * @param SizeInBytes The amount of bytes to convert to a formatted text that represents it.
	 * @return A text representing the value provided as Bytes, Kilobytes, Megabytes and Gigabytes. It will not return
	 * size types with value 0.
	 */
	FText GetSizeInBytesAsBiggerUnits(const uint64 SizeInBytes)
	{
		FString OutputString = "";
		
		uint64 Bytes = SizeInBytes;
		uint64 KiloBytes = 0;
		uint64 MegaBytes = 0;
		uint64 GigaBytes = 0;
		
		// B to KB
		if (Bytes >= 1024)
		{
			KiloBytes = Bytes / 1024;
			Bytes = Bytes % 1024;
			
			// KB to MB
			if (KiloBytes >= 1024)
			{
				MegaBytes = KiloBytes / 1024;
				KiloBytes = KiloBytes % 1024;
				
				// MB to GB
				if (MegaBytes >= 1024)
				{
					GigaBytes = MegaBytes / 1024;
					MegaBytes = MegaBytes % 1024;
				}
			}
		}

		// Compose the string that will be the output of this function 
		if (GigaBytes)
		{
			OutputString.Append(FString::FromInt(GigaBytes) + " GB, ");
		}

		if (MegaBytes)
		{
			OutputString.Append(FString::FromInt(MegaBytes) + " MB, ");
		}

		if (KiloBytes)
		{
			OutputString.Append(FString::FromInt(KiloBytes) + " KB, ");
		}

		if (Bytes)
		{
			OutputString.Append( FString::FromInt(Bytes) + " B ");
		}

		return FText::FromString(OutputString);
	}
}


#pragma region SUPPORT_CLASSES

class SMutableConstantMeshRow final : public STableRow<TSharedPtr<FMutableConstantMeshElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantMeshElement>& InRowElement)
	{
		check(InRowElement);
		
		const FText MeshProxyText =
			FText::Format( LOCTEXT("MeshConstantProxyLabel", "{0}_MESH "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(MeshProxyText)
		];
		
		STableRow< TSharedPtr<FMutableConstantMeshElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);

	}
};

class SMutableConstantStringRow final : public STableRow<TSharedPtr<FMutableConstantStringElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantStringElement>& InRowElement)
	{
		check(InRowElement);
		
		// Generate the text to be displayed taking in mind the string value held by the constant to be able to "preview"
		// it for easier navigation
		const FString MainString = FString::FromInt(InRowElement->IndexOnSourceVector) + FString(TEXT("_STR "));
		const FString ConstantStringText = FString(InRowElement->MutableString->c_str());
		const FString GlimpseConstantText = ConstantStringText.Left(GlimpseCharacterCount);
		
		// Compose the FStrings to produce the UI text to be displayed
		FString UiString = MainString + "\"" + GlimpseConstantText;
		if (ConstantStringText.Len() > GlimpseConstantText.Len())
		{
			// Shortening occured, adding "..." to show it to the user
			UiString.Append("...");
		}
		UiString.Append("\"");
		
		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(FText::FromString(UiString))
		];
		
		STableRow< TSharedPtr<FMutableConstantStringElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}


private:

	/** Determines the amount of characters to be displayed on the string constant UI text as a preview of the value
	 * of the actual string constant.
	 */
	const uint32 GlimpseCharacterCount = 8;
};

namespace ImageConstantTitles
{
	static const FName ImageID("Id");
	static const FName ImageSize("Resolution");
	static const FName ImageMipMaps("MipMapCount");
	static const FName ImageFormat("Format");
	static const FName ImageTotalMemory("MemorySize");
}

class SMutableConstantImageRow final : public SMultiColumnTableRow<TSharedPtr<FMutableConstantImageElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantImageElement>& InRowElement)
	{
		check(InRowElement);
		RowElement = InRowElement;
		
		SMultiColumnTableRow< TSharedPtr<FMutableConstantImageElement> >::Construct(
	STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView
		);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		check(RowElement);

		const mu::Image* Image = RowElement->ImagePtr.get();
		
		// Index
		if (ColumnName == ImageConstantTitles::ImageID)
		{
			const FText IndexAsText = FText::AsNumber(RowElement->IndexOnSourceVector);
			return SNew(STextBlock).Text(IndexAsText);
		}

		// ImageSize (Resolution size)
		if (ColumnName == ImageConstantTitles::ImageSize)
		{
			const FString XSizeString = FString::FromInt( Image->GetSizeX());
			const FString YSizeString = FString::FromInt( Image->GetSizeY());
			const FString XSign = FString("x");
			const FText ImageResolution = FText::FromString( XSizeString + XSign + YSizeString);
			
			return SNew(STextBlock).Text(ImageResolution);
		}

		// Image Mip maps (LODs)
		if (ColumnName == ImageConstantTitles::ImageMipMaps)
		{
			const FText LodCount = FText::AsNumber(Image->GetLODCount());
			return SNew(STextBlock).Text(LodCount);
		}

		// Image format
		if (ColumnName == ImageConstantTitles::ImageFormat)
		{
			const mu::EImageFormat ImageFormat = Image->GetFormat();
			const uint8 ImageFormatValue = static_cast<uint8>(ImageFormat);
			const FText FormatAsText = FText::FromString(
				FString(mu::TypeInfo::s_imageFormatName[ImageFormatValue]));
			
			return SNew(STextBlock).Text(FormatAsText);
		}

		// Memory
		if (ColumnName == ImageConstantTitles::ImageTotalMemory)
		{
			// Todo: Not sure if I do prefer this way of showing the sizes or the one used for the expandable area titles.
			
			uint32 SizeInBytes = Image->GetDataSize();
			FString SizeUnit = " B";
			
			// If convertible to KB then display it as KB (Kilo Bytes)
			if (SizeInBytes >= 1024 && SizeInBytes % 1024 == 0)
			{
				SizeUnit = " KB";
				SizeInBytes /= 1024;
			
				// If convertible to MB then display it as MB (Mega Bytes)
				if (SizeInBytes >= 1024 && SizeInBytes % 1024 == 0)
				{
					SizeUnit = " MB";
					SizeInBytes /= 1024;
				
					// If convertible to GB then display it as GB (Giga Bytes)
					if (SizeInBytes >= 1024 && SizeInBytes % 1024 == 0)
					{
						SizeUnit = " GB";
						SizeInBytes /= 1024;
					}
				}
			}
			
			FText SizeAsText = FText::GetEmpty();
			if (SizeInBytes > 0)
			{
				// Return the text formatted as number
				SizeAsText = FText::FromString( FString::FormatAsNumber(SizeInBytes) + SizeUnit);
			}

			// Return the text with the size
			return SNew(STextBlock).Text(SizeAsText);
		}

		checkNoEntry();
		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FMutableConstantImageElement> RowElement;
};


class SMutableConstantLayoutRow final : public STableRow<TSharedPtr<FMutableConstantLayoutElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantLayoutElement>& InRowElement)
	{
		check(InRowElement);
		const FText LayoutProxyText = FText::Format( LOCTEXT("LayoutConstantProxyLabel", "{0}_LAYOUT "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(LayoutProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantLayoutElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
	
};

class SMutableConstantProjectorRow final : public STableRow<TSharedPtr<FMutableConstantProjectorElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantProjectorElement>& InRowElement)
	{
		check(InRowElement);
		const FText ProjectorProxyText = FText::Format( LOCTEXT("ProjectorConstantProxyLabel", "{0}_PROJECTOR "),InRowElement->IndexOnSourceVector);
		
		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(ProjectorProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantProjectorElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
	
};

class SMutableConstantMatrixRow final : public STableRow<TSharedPtr<FMutableConstantMatrixElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantMatrixElement>& InRowElement)
	{
		check(InRowElement);
		const FText MatrixProxyText = FText::Format( LOCTEXT("MatrixConstantProxyLabel", "{0}_MATRIX "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(MatrixProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantMatrixElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
};

class SMutableConstantShapeRow final : public STableRow<TSharedPtr<FMutableConstantShapeElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantShapeElement>& InRowElement)
	{
		check(InRowElement);
		const FText ShapeProxyText = FText::Format( LOCTEXT("ShapeConstantProxyLabel", "{0}_SHAPE "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(ShapeProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantShapeElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
};

class SMutableConstantCurveRow final : public STableRow<TSharedPtr<FMutableConstantCurveElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantCurveElement>& InRowElement)
	{
		check(InRowElement);
		const FText CurveProxyText = FText::Format( LOCTEXT("CurveConstantProxyLabel", "{0}_CURVE "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(CurveProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantCurveElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
};


class SMutableConstantSkeletonRow final : public STableRow<TSharedPtr<FMutableConstantSkeletonElement>>
{
public:
	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableConstantSkeletonElement>& InRowElement)
	{
		check(InRowElement);
		const FText SkeletonProxyText = FText::Format( LOCTEXT("SkeletonConstantProxyLabel", "{0}_SKELETON "),InRowElement->IndexOnSourceVector);

		this->ChildSlot
		[
			SNew(STextBlock)
			.Text(SkeletonProxyText)
		];

		STableRow< TSharedPtr<FMutableConstantSkeletonElement> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true)
			, InOwnerTableView);
	}
};

#pragma endregion SUPPORT_CLASSES



void SMutableConstantsWidget::Construct(const FArguments& InArgs,const mu::PROGRAM* InMutableProgramPtr,  TSharedPtr<SMutableCodeViewer> InMutableCodeViewerPtr)
{
	// A pointer to MutableCodeViewerPtr is required in order to be able to invoke the preview of our constants
	check (InMutableCodeViewerPtr);
	MutableCodeViewerPtr = InMutableCodeViewerPtr;
	
	// A pointer to the mutable program object is required to get the constants data
	check(InMutableProgramPtr);
	SetProgram(InMutableProgramPtr);
	
	// Formatting constants
	constexpr uint32 InBetweenListsVerticalPadding = 4;
	
	// Vertical size for each entry 
	constexpr float ProxyEntryHeight = 20;
	
	// Panel title
	const FText ConstantsPanelTitle = LOCTEXT("ConstantsPannelName", "Constants : ");
	
	// Child structure
	this->ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(EOrientation::Orient_Vertical)
		
		// String constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(StringsExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnStringsRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawStringsAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantStringElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedStringChanged)
				.ListItemsSource(&ConstantStringElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this, &SMutableConstantsWidget::OnGenerateStringRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]

		// Image constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(ImagesExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnImagesRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawImagesAreaTitle)
			.BodyContent()
			[
				SNew(SVerticalBox)

				// Buttons being used to change the segment to be displayed
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					GenerateImagesSegmentSelectionWidget()
				]

				// List view showing the selected segment of images
				+ SVerticalBox::Slot()
				[
					SAssignNew(ConstantImagesListView,SListView<TSharedPtr<FMutableConstantImageElement>>)
					.OnGenerateRow(this,&SMutableConstantsWidget::OnGenerateImageRow)
					.ListItemsSource(&ConstantImageElementsSegment)
					.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedImageChanged)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(ImageConstantTitles::ImageID)
						.DefaultLabel(FText(LOCTEXT("ImageId", "ID")))
						.OnSort(this, &SMutableConstantsWidget::OnImageTableSortRequested)
						.FillWidth(0.28f)
					
						+ SHeaderRow::Column(ImageConstantTitles::ImageSize)
						.DefaultLabel(FText(LOCTEXT("ImageResolution","Resolution")))
						.OnSort(this, &SMutableConstantsWidget::OnImageTableSortRequested)
						
						+ SHeaderRow::Column(ImageConstantTitles::ImageMipMaps)
						.DefaultLabel(FText(LOCTEXT("ImageMipMaps","Mip Maps")))
						.OnSort(this, &SMutableConstantsWidget::OnImageTableSortRequested)
						
						+ SHeaderRow::Column(ImageConstantTitles::ImageFormat)
						.DefaultLabel(FText(LOCTEXT("ImageFormat","Format")))
						.OnSort(this, &SMutableConstantsWidget::OnImageTableSortRequested)
						
						+ SHeaderRow::Column(ImageConstantTitles::ImageTotalMemory)
						.DefaultLabel(FText(LOCTEXT("ImageMemorySize","Size")))
						.OnSort(this, &SMutableConstantsWidget::OnImageTableSortRequested)
					)
				]
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Mesh Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(MeshesExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnMeshesRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawMeshesAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantMeshElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedMeshChanged)
				.ListItemsSource(&ConstantMeshElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateMeshRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Layout Constants		
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(LayoutsExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnLayoutsRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawLayoutsAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantLayoutElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedLayoutChanged)
				.ListItemsSource(&ConstantLayoutElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateLayoutRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Projector Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(ProjectorsExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnProjectorsRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawProjectorsAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantProjectorElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedProjectorChanged)
				.ListItemsSource(&ConstantProjectorElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateProjectorRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Matrix Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(MatricesExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnMatricesRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawMatricesAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantMatrixElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedMatrixChanged)
				.ListItemsSource(&ConstantMatrixElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateMatrixRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Shape Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(ShapesExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnShapesRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawShapesAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantShapeElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedShapeChanged)
				.ListItemsSource(&ConstantShapeElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateShapeRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Curve Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(CurvesExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnCurvesRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawCurvesAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantCurveElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedCurveChanged)
				.ListItemsSource(&ConstantCurveElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateCurveRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SScrollBox::Slot()
		[
			SNew(SSeparator)
		]
		
		// Skeleton Constants
		+ SScrollBox::Slot()
		.Padding(0,InBetweenListsVerticalPadding)
		[
			SAssignNew(SkeletonsExpandableArea,SExpandableArea)
			.OnAreaExpansionChanged(this,&SMutableConstantsWidget::OnSkeletonsRegionExpansionChanged)
			.InitiallyCollapsed(true)
			.AreaTitle(this,&SMutableConstantsWidget::OnDrawSkeletonsAreaTitle)
			.BodyContent()
			[
				SNew(STileView<TSharedPtr<FMutableConstantSkeletonElement>>)
				.OnSelectionChanged(this,&SMutableConstantsWidget::OnSelectedSkeletonChanged)
				.ListItemsSource(&ConstantSkeletonElements)
				.ItemHeight(ProxyEntryHeight)
				.OnGenerateTile(this,&SMutableConstantsWidget::OnGenerateSkeletonRow)
				.SelectionMode(ESelectionMode::Single)
			]
		]
	];

	// Store all the expandable areas so they are later reachable using loops
	ExpandableAreas.Add(StringsExpandableArea);
	ExpandableAreas.Add(ImagesExpandableArea);
	ExpandableAreas.Add(MeshesExpandableArea);
	ExpandableAreas.Add(LayoutsExpandableArea);
	ExpandableAreas.Add(ProjectorsExpandableArea);
	ExpandableAreas.Add(ShapesExpandableArea);
	ExpandableAreas.Add(CurvesExpandableArea);
	ExpandableAreas.Add(MatricesExpandableArea);
	ExpandableAreas.Add(SkeletonsExpandableArea);
}



void SMutableConstantsWidget::SetProgram(const mu::PROGRAM* InProgram)
{
	// Make sure we do not process the setting of the same program object as the one already set
	if (this->MutableProgramPtr == InProgram)
	{
		return;
	}
	
	// Set only once the program that is being used. No further updates should be required
	this->MutableProgramPtr = InProgram;
	
	// Generate the backend for the lists used in this object
	if (MutableProgramPtr != nullptr)
	{
		LoadConstantElements();
	}
}

#pragma region Row Generation

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateStringRow(
	TSharedPtr<FMutableConstantStringElement> MutableConstantStringElement, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantStringRow> Row = SNew(SMutableConstantStringRow, OwnerTable, MutableConstantStringElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateImageRow(
	TSharedPtr<FMutableConstantImageElement> MutableConstantImageElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantImageRow> Row = SNew(SMutableConstantImageRow,OwnerTable,MutableConstantImageElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateMeshRow(
	TSharedPtr<FMutableConstantMeshElement> MutableConstantMeshElement, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantMeshRow> Row = SNew(SMutableConstantMeshRow, OwnerTable, MutableConstantMeshElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateLayoutRow(
	TSharedPtr<FMutableConstantLayoutElement> MutableConstantLayoutElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantLayoutRow> Row = SNew(SMutableConstantLayoutRow,OwnerTable,MutableConstantLayoutElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateProjectorRow(
	TSharedPtr<FMutableConstantProjectorElement> MutableConstantProjectorElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantProjectorRow> Row = SNew(SMutableConstantProjectorRow,OwnerTable,MutableConstantProjectorElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateMatrixRow(
	TSharedPtr<FMutableConstantMatrixElement> MutableConstantMatrixElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantMatrixRow> Row = SNew(SMutableConstantMatrixRow,OwnerTable,MutableConstantMatrixElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateShapeRow(
	TSharedPtr<FMutableConstantShapeElement> MutableConstantShapeElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantShapeRow> Row = SNew(SMutableConstantShapeRow,OwnerTable,MutableConstantShapeElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateCurveRow(
	TSharedPtr<FMutableConstantCurveElement> MutableConstantCurveElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantCurveRow> Row = SNew(SMutableConstantCurveRow,OwnerTable,MutableConstantCurveElement);
	return Row;
}

TSharedRef<ITableRow> SMutableConstantsWidget::OnGenerateSkeletonRow(
	TSharedPtr<FMutableConstantSkeletonElement> MutableConstantSkeletonElement,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedRef<SMutableConstantSkeletonRow> Row = SNew(SMutableConstantSkeletonRow,OwnerTable,MutableConstantSkeletonElement);
	return Row;
}

bool SMutableConstantsWidget::ShouldBackButtonBeEnabled() const
{
	return  CurrentArraySegment > 0;
}

bool SMutableConstantsWidget::ShouldNextButtonBeEnabled() const
{
	return TotalAmountOfSegments >= 1 && CurrentArraySegment < TotalAmountOfSegments - 1;
}

void SMutableConstantsWidget::RegenerateProxyImageArray()
{
	// Prepare the segment of elements to be filled up
	ConstantImageElementsSegment.SetNum(0);
	ConstantImageElementsSegment.Reserve(ElementsPerSegment);
	
	const uint32 StartingIndex = CurrentArraySegment * ElementsPerSegment;
	const uint32 FinishIndex =
		StartingIndex + FMath::Min(ElementsPerSegment, (static_cast<uint32>(ConstantImageElements.Num()) - StartingIndex));
	// Iterate over the main array from the point where the segment starts to the point where it ends. Stop before if needed
	for	(uint32 MainArrayIndex = StartingIndex ; MainArrayIndex < FinishIndex; ++ MainArrayIndex)
	{
		ConstantImageElementsSegment.Add(ConstantImageElements[MainArrayIndex]);
	}
	
	if (ConstantImagesListView.IsValid())
	{
		ConstantImagesListView->RequestListRefresh();
	}
}

TSharedRef<SWidget> SMutableConstantsWidget::GenerateImagesSegmentSelectionWidget()
{
	// If there are segments to navigate then generate the navigation area
	if (TotalAmountOfSegments > 1)
	{
		TSharedRef<SHorizontalBox> Container =	SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this,&SMutableConstantsWidget::OnImageFullBackButtonClicked)
			.Text(FText(INVTEXT("|<")))
			.IsEnabled(this,&SMutableConstantsWidget::ShouldBackButtonBeEnabled)
		]
		
		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this,&SMutableConstantsWidget::OnImageBackButtonClicked)
			.Text(FText(INVTEXT("<")))
			.IsEnabled(this,&SMutableConstantsWidget::ShouldBackButtonBeEnabled)
		]

		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Center)
		[
			SNew(STextBlock)
			.Justification(ETextJustify::Center)
			.Text(this,&SMutableConstantsWidget::OnDrawCurrentImageSegmentText)
		]
			
		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this,&SMutableConstantsWidget::OnImageForwardButtonClicked)
			.Text(FText(INVTEXT(">")))
			.IsEnabled(this,&SMutableConstantsWidget::ShouldNextButtonBeEnabled)
		]

		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		.AutoWidth()
		[
			SNew(SButton)
			.OnClicked(this,&SMutableConstantsWidget::OnImageFullForwardButtonClicked)
			.Text(FText(INVTEXT(">|")))
			.IsEnabled(this,&SMutableConstantsWidget::ShouldNextButtonBeEnabled)
		];

		return Container;
	}

	// If no more than 1 section is required
	return SNullWidget::NullWidget;
}

FText SMutableConstantsWidget::OnDrawCurrentImageSegmentText() const
{
	FString BuiltText = FString::FromInt(  CurrentArraySegment + 1);
	BuiltText += FString(" / ");
	BuiltText += FString::FromInt(TotalAmountOfSegments);
	
	return FText::Format(LOCTEXT("PageNumber","Page : {0}"), FText::FromString(BuiltText));
}

void SMutableConstantsWidget::OnImageTableSortRequested(EColumnSortPriority::Type ColumnSortPriority, const FName& ColumnID,
                                                        EColumnSortMode::Type ColumnSortMode)
{
	// If the colum has been sorted on one way now do the inverse way of sorting
	if (LastSortedColumnID == ColumnID)
	{
		bSortAscending = !bSortAscending;
	}

	Algo::StableSort(ConstantImageElements, [&](const TSharedPtr<FMutableConstantImageElement>& A, const TSharedPtr<FMutableConstantImageElement>& B)
	{
		// Sort by image id
		if (ColumnID == ImageConstantTitles::ImageID)
		{
			if (bSortAscending)
			{
				return A->IndexOnSourceVector < B->IndexOnSourceVector;
			}
			return A->IndexOnSourceVector > B->IndexOnSourceVector;
		}
		
		// Sort Image mip maps
		if (ColumnID == ImageConstantTitles::ImageMipMaps)
		{
			if (bSortAscending)
			{
				return A->ImagePtr->GetLODCount() < B->ImagePtr->GetLODCount();
			}
			return A->ImagePtr->GetLODCount() > B->ImagePtr->GetLODCount();
		}

		// Sort by image format
		if (ColumnID == ImageConstantTitles::ImageFormat)
		{
			const uint8 AImageFormatValue = static_cast<uint8>(A->ImagePtr->GetFormat());
			const FString AImageFormatString = FString(mu::TypeInfo::s_imageFormatName[AImageFormatValue]);
			
			const uint8 BImageFormatValue = static_cast<uint8>(B->ImagePtr->GetFormat());
			const FString BImageFormatString = FString(mu::TypeInfo::s_imageFormatName[BImageFormatValue]);
			
			if (bSortAscending)
			{
				return AImageFormatString.Compare(BImageFormatString) < 0;
			}
			return AImageFormatString.Compare(BImageFormatString) > 0;
		}
		
		
		// Sort by image size
		if (ColumnID == ImageConstantTitles::ImageSize)
		{
			if (bSortAscending)
			{
				return A->ImagePtr->GetSizeX() *  A->ImagePtr->GetSizeY() <
					B->ImagePtr->GetSizeX() *  B->ImagePtr->GetSizeY();
			}
			return A->ImagePtr->GetSizeX() *  A->ImagePtr->GetSizeY() >
					B->ImagePtr->GetSizeX() *  B->ImagePtr->GetSizeY();
		}
		
		
		// Sort by image used memory
		if (ColumnID == ImageConstantTitles::ImageTotalMemory)
		{
			if (bSortAscending)
			{
				return A->ImagePtr->GetDataSize() < B->ImagePtr->GetDataSize();
			}
			return A->ImagePtr->GetDataSize() > B->ImagePtr->GetDataSize();
		}
		
		return false;
	});

	// Store the last column to be able to do the inverse sorting order the next time it gets pressed
	LastSortedColumnID = ColumnID;

	RegenerateProxyImageArray();
	ConstantImagesListView->RequestListRefresh();
}


FReply SMutableConstantsWidget::OnImageBackButtonClicked()
{
	CurrentArraySegment--;
	
	// Regenerate the proxy list with the elements on the currently selected list segment
	RegenerateProxyImageArray();

	return FReply::Handled();
}

FReply SMutableConstantsWidget::OnImageForwardButtonClicked()
{
	CurrentArraySegment++;
	
	// Regenerate the proxy list with the elements on the currently selected list segment
	RegenerateProxyImageArray();

	return FReply::Handled();
}

FReply SMutableConstantsWidget::OnImageFullBackButtonClicked()
{ 
	CurrentArraySegment = 0;
	
	// Regenerate the proxy list with the elements on the currently selected list segment
	RegenerateProxyImageArray();

	return FReply::Handled();
}

FReply SMutableConstantsWidget::OnImageFullForwardButtonClicked()
{
	CurrentArraySegment = TotalAmountOfSegments - 1;
	
	// Regenerate the proxy list with the elements on the currently selected list segment
	RegenerateProxyImageArray();

	return FReply::Handled();
}

#pragma endregion 

#pragma  region  Expansions Handling

void SMutableConstantsWidget::OnStringsRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(StringsExpandableArea);
	}
}

void SMutableConstantsWidget::OnImagesRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(ImagesExpandableArea);
	}
}

void SMutableConstantsWidget::OnMeshesRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(MeshesExpandableArea);
	}
}

void SMutableConstantsWidget::OnLayoutsRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(LayoutsExpandableArea);
	}
}

void SMutableConstantsWidget::OnProjectorsRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(ProjectorsExpandableArea);
	}
}

void SMutableConstantsWidget::OnMatricesRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(MatricesExpandableArea);
	}
}

void SMutableConstantsWidget::OnShapesRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(ShapesExpandableArea);
	}
}

void SMutableConstantsWidget::OnCurvesRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(CurvesExpandableArea);
	}
}

void SMutableConstantsWidget::OnSkeletonsRegionExpansionChanged(bool bExpanded)
{
	if (bExpanded)
	{
		ContractExpandableAreas(SkeletonsExpandableArea);
	}
}

void SMutableConstantsWidget::ContractExpandableAreas(const TSharedPtr<SExpandableArea>& InException)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if(!InException)
	{
		UE_LOG(LogTemp,Warning,TEXT("No valid expandable area has been provided as exception : All expandable areas will therefore get contracted"));
	}
#endif
	
	for (TSharedPtr<SExpandableArea>& CurrentExpandableArea : ExpandableAreas)
	{
		if (CurrentExpandableArea == InException)
		{
			continue;
		}

		CurrentExpandableArea->SetExpanded(false);
	}
}

#pragma endregion 

#pragma  region Element caches loading

void SMutableConstantsWidget::LoadConstantElements()
{
	LoadConstantStrings();
	LoadConstantImages();
	LoadConstantMeshes();
	LoadConstantLayouts();
	LoadConstantProjectors();
	LoadConstantMatrices();
	LoadConstantShapes();
	LoadConstantCurves();
	LoadConstantSkeletons();
}

void SMutableConstantsWidget::LoadConstantStrings()
{
	check (MutableProgramPtr);

	const int32 ConstantsCount = MutableProgramPtr->m_constantStrings.Num();
	ConstantStringElements.Empty(ConstantsCount);
	uint64 ConstantStringsAccumulatedSize = 0;
	
	for (int32 StringAddressIndex = 0; StringAddressIndex < ConstantsCount; StringAddressIndex++)
	{
		TSharedPtr<FMutableConstantStringElement> ConstantStringElement = MakeShared<FMutableConstantStringElement>();
		ConstantStringElement->MutableString = &(MutableProgramPtr->m_constantStrings[StringAddressIndex]);
		ConstantStringElement->IndexOnSourceVector = StringAddressIndex;
		
		// Cache resource size
		// in case we change the type of the contents of the mu::string we check its size as if it was a vector<>
		ConstantStringsAccumulatedSize += ConstantStringElement->MutableString->size() * sizeof (mu::string::value_type);
		
		ConstantStringElements.Add(ConstantStringElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantStringsFormattedSize = GetSizeInBytesAsBiggerUnits(ConstantStringsAccumulatedSize);
}

void SMutableConstantsWidget::LoadConstantImages()
{
	check (MutableProgramPtr);

	const int32 ConstantsCount = MutableProgramPtr->m_constantImages.Num();
	ConstantImageElements.Empty(ConstantsCount);
	
	uint64 ConstantImagesAccumulatedSize = 0;

	for (int32 ImageIndex = 0; ImageIndex < ConstantsCount; ImageIndex++)
	{
		TSharedPtr<FMutableConstantImageElement> ConstantImageElement =MakeShared<FMutableConstantImageElement>();
		MutableProgramPtr->GetConstant(ImageIndex, ConstantImageElement->ImagePtr, 0);
		ConstantImageElement->IndexOnSourceVector = ImageIndex;
		
		ConstantImagesAccumulatedSize += ConstantImageElement->ImagePtr->GetDataSize();
		
		ConstantImageElements.Add(ConstantImageElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantImagesFormattedSize = GetSizeInBytesAsBiggerUnits(ConstantImagesAccumulatedSize);
	
	// Regenerate the array with only a part of the the full images array
	RegenerateProxyImageArray();

	// Compute the total amount of segments
	{
		// Compute the max amount of segments in case it needs to be updated
		TotalAmountOfSegments = ConstantImageElements.Num() / ElementsPerSegment;
		
		// Add one more segment if there are elements not filling a segment
		TotalAmountOfSegments = ConstantImageElements.Num() % ElementsPerSegment > 0 ? TotalAmountOfSegments + 1 : TotalAmountOfSegments;
	}

}

void SMutableConstantsWidget::LoadConstantMeshes()
{
	check (MutableProgramPtr);
	
	const int32 ConstantsCount = MutableProgramPtr->m_constantMeshes.Num();
	ConstantMeshElements.Empty(ConstantsCount);
	
	uint64 ConstantMeshesAccumulatedSize = 0;
	
	for (int32 MeshIndex = 0; MeshIndex < ConstantsCount; MeshIndex++)
	{
		TSharedPtr< FMutableConstantMeshElement> ConstantMeshElement = MakeShared<FMutableConstantMeshElement>();
		ConstantMeshElement->MeshPtr = MutableProgramPtr->m_constantMeshes[MeshIndex].Value;
		ConstantMeshElement->IndexOnSourceVector = MeshIndex;
		
		ConstantMeshesAccumulatedSize += ConstantMeshElement->MeshPtr->GetDataSize();
		
		ConstantMeshElements.Add(ConstantMeshElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantMeshesFormattedSize = GetSizeInBytesAsBiggerUnits(ConstantMeshesAccumulatedSize);
}

void SMutableConstantsWidget::LoadConstantLayouts()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantLayouts.Num();
	ConstantLayoutElements.Empty(ConstantsCount);
	
	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};
	
	for (int32 LayoutIndex = 0; LayoutIndex < ConstantsCount; LayoutIndex++)
	{		
		TSharedPtr<FMutableConstantLayoutElement> ConstantLayoutElement = MakeShared<FMutableConstantLayoutElement>();
		ConstantLayoutElement->Layout = MutableProgramPtr->m_constantLayouts[LayoutIndex];
		ConstantLayoutElement->IndexOnSourceVector = LayoutIndex;
		
		// Cache resource for later GetBufferSize() call
		ConstantLayoutElement->Layout->Serialise(Archive);

		ConstantLayoutElements.Add(ConstantLayoutElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantLayoutsFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());

}

void SMutableConstantsWidget::LoadConstantSkeletons()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantSkeletons.Num();
	ConstantSkeletonElements.Empty(ConstantsCount);

	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};
	
	for (int32 SkeletonIndex = 0; SkeletonIndex < ConstantsCount; SkeletonIndex++)
	{
		TSharedPtr<FMutableConstantSkeletonElement> ConstantSkeletonElement = MakeShared<FMutableConstantSkeletonElement>();
		ConstantSkeletonElement->Skeleton = MutableProgramPtr->m_constantSkeletons[SkeletonIndex];
		ConstantSkeletonElement->IndexOnSourceVector = SkeletonIndex;
		
		ConstantSkeletonElement->Skeleton->Serialise(Archive);

		ConstantSkeletonElements.Add(ConstantSkeletonElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantSkeletonsFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());
}

void SMutableConstantsWidget::LoadConstantProjectors()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantProjectors.Num();
	ConstantProjectorElements.Empty(ConstantsCount);

	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};
	
	for (int32 ProjectorIndex = 0; ProjectorIndex < ConstantsCount; ProjectorIndex++)
	{
		TSharedPtr<FMutableConstantProjectorElement> ConstantProjectorElement = MakeShared<FMutableConstantProjectorElement>();
		ConstantProjectorElement->Projector = &(MutableProgramPtr->m_constantProjectors[ProjectorIndex]);
		ConstantProjectorElement->IndexOnSourceVector = ProjectorIndex;
		
		ConstantProjectorElement->Projector->Serialise(Archive);
		
		ConstantProjectorElements.Add(ConstantProjectorElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantProjectorsFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());
}

void SMutableConstantsWidget::LoadConstantMatrices()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantMatrices.Num();
	ConstantMatrixElements.Empty(ConstantsCount);

	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};
	
	for (int32 MatrixIndex = 0; MatrixIndex < ConstantsCount; MatrixIndex++)
	{
		TSharedPtr<FMutableConstantMatrixElement> ConstantMatrixElement = MakeShared<FMutableConstantMatrixElement>();
		ConstantMatrixElement->Matrix = &(MutableProgramPtr->m_constantMatrices[MatrixIndex]);
		ConstantMatrixElement->IndexOnSourceVector = MatrixIndex;
		
		ConstantMatrixElement->Matrix->Serialise(Archive);
		
		ConstantMatrixElements.Add(ConstantMatrixElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantMatricesFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());
}

void SMutableConstantsWidget::LoadConstantShapes()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantShapes.Num();
	ConstantShapeElements.Empty(ConstantsCount);

	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};	
	
	for (int32 ShapeIndex = 0; ShapeIndex < ConstantsCount; ShapeIndex++)
	{
		TSharedPtr<FMutableConstantShapeElement> ConstantShapeElement = MakeShared<FMutableConstantShapeElement>();
		ConstantShapeElement->Shape = &(MutableProgramPtr->m_constantShapes[ShapeIndex]);
		ConstantShapeElement->IndexOnSourceVector = ShapeIndex;
		
		ConstantShapeElement->Shape->Serialise(Archive);

		ConstantShapeElements.Add(ConstantShapeElement);
	}
	
	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantShapesFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());
}

void SMutableConstantsWidget::LoadConstantCurves()
{
	check (MutableProgramPtr);
	const int32 ConstantsCount = MutableProgramPtr->m_constantCurves.Num();
	ConstantCurveElements.Empty(ConstantsCount);
	
	mu::OutputMemoryStream Stream;
	mu::OutputArchive Archive{&Stream};
	
	for (int32 CurveIndex = 0; CurveIndex < ConstantsCount; CurveIndex++)
	{
		TSharedPtr<FMutableConstantCurveElement> ConstantCurveElement = MakeShared<FMutableConstantCurveElement>();
		ConstantCurveElement->Curve = &(MutableProgramPtr->m_constantCurves[CurveIndex]);
		ConstantCurveElement->IndexOnSourceVector = CurveIndex;

		ConstantCurveElement->Curve->Serialise(Archive);
		
		ConstantCurveElements.Add(ConstantCurveElement);
	}

	// Cache the size in memory of the constants as a formatted text so it is able to be be later used by the UI
	ConstantCurvesFormattedSize = GetSizeInBytesAsBiggerUnits(Stream.GetBufferSize());
}

#pragma  endregion


#pragma region Previewer invocation methods


void SMutableConstantsWidget::OnSelectedStringChanged(
	TSharedPtr<FMutableConstantStringElement> MutableConstantStringElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantStringElement)
	{
		// Ask the Code viewer to present the element held on your element on the previewer window
		MutableCodeViewerPtr->PreviewMutableString(MutableConstantStringElement->MutableString);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_STRING,MutableConstantStringElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedImageChanged(
	TSharedPtr<FMutableConstantImageElement> MutableConstantImageElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantImageElement)
	{
		MutableCodeViewerPtr->PreviewMutableImage(MutableConstantImageElement->ImagePtr);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_IMAGE,MutableConstantImageElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedMeshChanged(
	TSharedPtr<FMutableConstantMeshElement> MutableConstantMeshElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantMeshElement)
	{
		MutableCodeViewerPtr->PreviewMutableMesh(MutableConstantMeshElement->MeshPtr);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_MESH,MutableConstantMeshElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedLayoutChanged(
	TSharedPtr<FMutableConstantLayoutElement> MutableConstantLayoutElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantLayoutElement)
	{
		MutableCodeViewerPtr->PreviewMutableLayout(MutableConstantLayoutElement->Layout);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_LAYOUT,MutableConstantLayoutElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedProjectorChanged(
	TSharedPtr<FMutableConstantProjectorElement> MutableConstantProjectorElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantProjectorElement)
	{
		MutableCodeViewerPtr->PreviewMutableProjector(MutableConstantProjectorElement->Projector);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_PROJECTOR,MutableConstantProjectorElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedMatrixChanged(
	TSharedPtr<FMutableConstantMatrixElement> MutableConstantMatrixElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantMatrixElement)
	{
		MutableCodeViewerPtr->PreviewMutableMatrix(MutableConstantMatrixElement->Matrix);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_MATRIX,MutableConstantMatrixElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedShapeChanged(
	TSharedPtr<FMutableConstantShapeElement> MutableConstantShapeElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantShapeElement)
	{
		MutableCodeViewerPtr->PreviewMutableShape(MutableConstantShapeElement->Shape);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_SHAPE,MutableConstantShapeElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedCurveChanged(
	TSharedPtr<FMutableConstantCurveElement> MutableConstantCurveElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantCurveElement)
	{
		MutableCodeViewerPtr->PreviewMutableCurve(MutableConstantCurveElement->Curve);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_CURVE,MutableConstantCurveElement->IndexOnSourceVector);
	}
}

void SMutableConstantsWidget::OnSelectedSkeletonChanged(
	TSharedPtr<FMutableConstantSkeletonElement> MutableConstantSkeletonElement,
	ESelectInfo::Type SelectionType) const
{
	if (MutableConstantSkeletonElement)
	{
		MutableCodeViewerPtr->PreviewMutableSkeleton(MutableConstantSkeletonElement->Skeleton);

		MutableCodeViewerPtr->CacheAddressesRelatedWithConstantResource(mu::DATATYPE::DT_SKELETON,MutableConstantSkeletonElement->IndexOnSourceVector);
	}
}

#pragma endregion 

#pragma region Region title drawing callback methods


FText SMutableConstantsWidget::OnDrawStringsAreaTitle() const
{
	return FText::Format( LOCTEXT("StringConstantsTitle", "String Constants ({0}) : {1} "),ConstantStringElements.Num(), ConstantStringsFormattedSize);
}

FText SMutableConstantsWidget::OnDrawImagesAreaTitle() const
{
	return FText::Format( LOCTEXT("ImageConstantsTitle", "Image Constants ({0}) : {1} "), ConstantImageElements.Num(), ConstantImagesFormattedSize);
}

FText SMutableConstantsWidget::OnDrawMeshesAreaTitle() const
{
	return FText::Format(LOCTEXT("MeshConstantsTitle", "Mesh Constants ({0}) : {1} "), ConstantMeshElements.Num(), ConstantMeshesFormattedSize) ;
}

FText SMutableConstantsWidget::OnDrawLayoutsAreaTitle() const
{
	return FText::Format(LOCTEXT("LayoutConstantsTitle", "Layout Constants ({0}) : {1} "), ConstantLayoutElements.Num(), ConstantLayoutsFormattedSize) ;
}

FText SMutableConstantsWidget::OnDrawProjectorsAreaTitle() const
{
	return  FText::Format( LOCTEXT("ProjectorConstantsTitle", "Projector Constants ({0}) : {1} "), ConstantProjectorElements.Num(),ConstantProjectorsFormattedSize);
}

FText SMutableConstantsWidget::OnDrawMatricesAreaTitle() const
{
	return FText::Format(LOCTEXT("MatrixConstantsTitle", "Matrix Constants ({0}) : {1} "), ConstantMatrixElements.Num(),ConstantMatricesFormattedSize) ;
}

FText SMutableConstantsWidget::OnDrawShapesAreaTitle() const
{
	return FText::Format(LOCTEXT("ShapeConstantsTitle", "Shape Constants ({0}) : {1} "),ConstantShapeElements.Num(), ConstantShapesFormattedSize) ;
}

FText SMutableConstantsWidget::OnDrawCurvesAreaTitle() const
{
	return FText::Format(LOCTEXT("CurveConstantsTitle", "Curve Constants ({0}) : {1} "), ConstantCurveElements.Num(), ConstantCurvesFormattedSize) ;
}

FText SMutableConstantsWidget::OnDrawSkeletonsAreaTitle() const
{
	return FText::Format( LOCTEXT("SkeletonConstantsTitle", "Skeleton Constants ({0}) : {1} "), ConstantSkeletonElements.Num(), ConstantSkeletonsFormattedSize);
}

#pragma endregion


#undef LOCTEXT_NAMESPACE
