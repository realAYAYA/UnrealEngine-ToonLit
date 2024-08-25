// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableGraphViewer.h"

#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "IDesktopPlatform.h"
#include "Misc/Paths.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Views/STreeView.h"
#include "ScopedTransaction.h"
#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourFromScalars.h"
#include "MuT/NodeColourParameter.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeComponentEdit.h"
#include "MuT/NodeImageFormat.h"
#include "MuT/NodeImageInterpolate.h"
#include "MuT/NodeImageInvert.h"
#include "MuT/NodeImageLayer.h"
#include "MuT/NodeImageLayerColour.h"
#include "MuT/NodeImageMipmap.h"
#include "MuT/NodeImageMultiLayer.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/NodeImageProject.h"
#include "MuT/NodeImageResize.h"
#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImageSwizzle.h"
#include "MuT/NodeImageTable.h"
#include "MuT/NodeObjectGroup.h"
#include "MuT/NodeObjectNew.h"
#include "MuT/NodeSurfaceEdit.h"
#include "MuT/NodeSurfaceSwitch.h"
#include "MuT/NodeSurfaceVariation.h"
#include "MuT/Streams.h"
#include "MuT/NodeLOD.h"
#include "MuT/NodeMeshConstant.h"
#include "MuT/NodeMeshFormat.h"
#include "MuT/NodeMeshFragment.h"
#include "MuT/NodeMeshMakeMorph.h"
#include "MuT/NodeMeshMorph.h"
#include "MuT/NodeMeshTable.h"
#include "MuT/NodeModifierMeshClipDeform.h"
#include "MuT/NodeModifierMeshClipMorphPlane.h"
#include "MuT/NodeModifierMeshClipWithUVMask.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarTable.h"


// This is necessary because of problems with rtti information in other platforms. In any case, this part of the debugger is only useful in the standard editor.
#if PLATFORM_WINDOWS
#include "MuT/NodeObjectNewPrivate.h"
#include "MuT/NodeObjectGroupPrivate.h"
#include "MuT/NodeSurfaceNewPrivate.h"
#include "MuT/NodeSurfaceEditPrivate.h"
#include "MuT/NodeSurfaceSwitchPrivate.h"
#include "MuT/NodeSurfaceVariationPrivate.h"
#include "MuT/NodeLODPrivate.h"
#include "MuT/NodeComponentPrivate.h"
#include "MuT/NodeModifierPrivate.h"
#include "MuT/NodeComponentNewPrivate.h"
#include "MuT/NodeComponentEditPrivate.h"
#include "MuT/NodeImageFormatPrivate.h"
#include "MuT/NodeMeshFormatPrivate.h"
#include "MuT/NodePatchImagePrivate.h"
#include "MuT/NodeMeshConstantPrivate.h"
#include "MuT/NodeModifierMeshClipMorphPlanePrivate.h"
#include "MuT/NodePatchMeshPrivate.h"
#include "MuT/NodeImageSwitchPrivate.h"
#include "MuT/NodeImageLayerColourPrivate.h"
#include "MuT/NodeImageLayerPrivate.h"
#include "MuT/NodeImageMipmapPrivate.h"
#include "MuT/NodeImageResizePrivate.h"
#include "MuT/NodeModifierMeshClipDeformPrivate.h"
#include "MuT/NodeModifierMeshClipWithMeshPrivate.h"
#include "MuT/NodeModifierMeshClipWithUVMaskPrivate.h"
#include "MuT/NodeColourParameterPrivate.h"
#include "MuT/NodeColourSampleImagePrivate.h"
#include "MuT/NodeImageInterpolatePrivate.h"
#include "MuT/NodeImagePlainColourPrivate.h"
#include "MuT/NodeImageProjectPrivate.h"
#include "MuT/NodeMeshFragmentPrivate.h"
#include "MuT/NodeMeshMorphPrivate.h"
#include "MuT/NodeScalarEnumParameterPrivate.h"
#include "MuT/NodeScalarParameterPrivate.h"
#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeMeshMakeMorphPrivate.h"
#include "MuT/NodeScalarCurvePrivate.h"
#include "MuT/NodeProjectorPrivate.h"
#include "MuT/NodeColourSwitchPrivate.h"
#include "MuT/NodeImageInvertPrivate.h"
#include "MuT/NodeImageSwizzlePrivate.h"
#include "MuT/NodeImageMultiLayerPrivate.h"
#include "MuT/NodeMeshTablePrivate.h"
#include "MuT/NodeColourFromScalarsPrivate.h"
#include "MuT/NodeScalarSwitchPrivate.h"
#endif

class FExtender;
class FReferenceCollector;
class FUICommandList;
class ITableRow;
class SWidget;
struct FGeometry;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "SMutableDebugger"

// \todo: multi-column tree
namespace MutableGraphTreeViewColumns
{
	static const FName Name("Name");
};


class SMutableGraphTreeRow : public STableRow<TSharedPtr<FMutableGraphTreeElement>>
{
public:

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<FMutableGraphTreeElement>& InRowItem)
	{
		RowItem = InRowItem;

		FText MainLabel = FText::GetEmpty();
		if (RowItem->MutableNode)
		{
			const char* TypeName = RowItem->MutableNode->GetType()->m_strName;
			
			const FString LabelString = RowItem->Prefix.IsEmpty() 
				? StringCast<TCHAR>(TypeName).Get() 
				: FString::Printf( TEXT("%s : %s"), *RowItem->Prefix, StringCast<TCHAR>(TypeName).Get() );

			MainLabel = FText::FromString(LabelString);
			if (RowItem->DuplicatedOf)
			{
				MainLabel = FText::FromString( FString::Printf(TEXT("%s (Duplicated)"), StringCast<TCHAR>(TypeName).Get()));
			}
		}
		else
		{
			MainLabel = FText::FromString( *RowItem->Prefix);
		}


		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.ShouldDrawWires(true)
			]

			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(MainLabel)
			]
		];

		STableRow< TSharedPtr<FMutableGraphTreeElement> >::ConstructInternal(
			STableRow::FArguments()
			//.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(true)
			, InOwnerTableView
		);

	}


private:

	TSharedPtr<FMutableGraphTreeElement> RowItem;
};


void SMutableGraphViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Add UObjects here if we own any at some point
	//Collector.AddReferencedObject(CustomizableObject);
}


FString SMutableGraphViewer::GetReferencerName() const
{
	return TEXT("SMutableGraphViewer");
}


void SMutableGraphViewer::Construct(const FArguments& InArgs, const mu::NodePtr& InRootNode)
{
	DataTag = InArgs._DataTag;
	ReferencedRuntimeTextures = InArgs._ReferencedRuntimeTextures;
	ReferencedCompileTextures = InArgs._ReferencedCompileTextures;
	RootNode = InRootNode;

	FToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolBar");

	// Export
	ToolbarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([InRootNode]()
				{
					TArray<FString> SaveFilenames;
					IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
					bool bSave = false;
					if (!DesktopPlatform) return;

					FString LastExportPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_EXPORT);
					FString FileTypes = TEXT("Mutable source data files|*.mutable_source|All files|*.*");
					bSave = DesktopPlatform->SaveFileDialog(
						FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
						TEXT("Export Mutable object"),
						*LastExportPath,
						TEXT("exported.mutable_source"),
						*FileTypes,
						EFileDialogFlags::None,
						SaveFilenames
					);

					if (!bSave) return;

					// Dump source model to a file.
					FString SaveFileName = FString(SaveFilenames[0]);
					mu::OutputFileStream stream(SaveFileName);
					stream.Write(MUTABLE_SOURCE_MODEL_FILETAG, 4);
					mu::OutputArchive arch(&stream);
					mu::Node::Serialise(InRootNode.get(), arch);
					stream.Flush();

					FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_EXPORT, SaveFileName);
				})
			),
			NAME_None,
			LOCTEXT("ExportMutableGraph", "Export"),
			LOCTEXT("ExportMutableGraphTooltip", "Export a debug mutable graph file."),
			FSlateIcon(),
			EUserInterfaceActionType::Button
		);
		
	ToolbarBuilder.EndSection();

	ToolbarBuilder.AddWidget(SNew(STextBlock).Text(MakeAttributeLambda([this]() { return FText::FromString(DataTag); })));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			ToolbarBuilder.MakeWidget()
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		[
			SNew(SSplitter)
			.Orientation(EOrientation::Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				[
					SAssignNew(TreeView, STreeView<TSharedPtr<FMutableGraphTreeElement>>)
					.TreeItemsSource(&RootNodes)
					.OnGenerateRow(this,&SMutableGraphViewer::GenerateRowForNodeTree)
					.OnGetChildren(this, &SMutableGraphViewer::GetChildrenForInfo)
					.OnSetExpansionRecursive(this, &SMutableGraphViewer::TreeExpandRecursive)
					.OnContextMenuOpening(this, &SMutableGraphViewer::OnTreeContextMenuOpening)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						+ SHeaderRow::Column(MutableGraphTreeViewColumns::Name)
						.FillWidth(25.f)
						.DefaultLabel(LOCTEXT("Node Name", "Node Name"))
					)
				]
			]
			+ SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.BorderImage(UE_MUTABLE_GET_BRUSH("ToolPanel.GroupBorder"))
				.Padding(FMargin(4.0f, 4.0f))
				//[
				//	SubjectsTreeView->AsShared()
				//]
			]
		]
	];
	
	RebuildTree();
}


void SMutableGraphViewer::RebuildTree()
{
	RootNodes.Reset();
	ItemCache.Reset();
	MainItemPerNode.Reset();

	RootNodes.Add(MakeShareable(new FMutableGraphTreeElement(RootNode)));
	TreeView->RequestTreeRefresh();
	TreeExpandUnique();
}


TSharedRef<ITableRow> SMutableGraphViewer::GenerateRowForNodeTree(TSharedPtr<FMutableGraphTreeElement> InTreeNode, const TSharedRef<STableViewBase>& InOwnerTable)
{
	TSharedRef<SMutableGraphTreeRow> Row = SNew(SMutableGraphTreeRow, InOwnerTable, InTreeNode);
	return Row;
}

void SMutableGraphViewer::GetChildrenForInfo(TSharedPtr<FMutableGraphTreeElement> InInfo, TArray<TSharedPtr<FMutableGraphTreeElement>>& OutChildren)
{
// This is necessary because of problems with rtti information in other platforms. In any case, this part of the debugger is only useful in the standard editor.
#if PLATFORM_WINDOWS
	if (!InInfo->MutableNode)
	{
		return;
	}

	// If this is a duplicated of another row, don't provide its children.
	if (InInfo->DuplicatedOf)
	{
		return;
	}

	mu::Node* ParentNode = InInfo->MutableNode.get();
	uint32 InputIndex = 0;

	auto AddChildFunc = [this, ParentNode, &InputIndex, &OutChildren](mu::Node* ChildNode, const FString& Prefix)
	{
		if (ChildNode)
		{
			FItemCacheKey Key = { ParentNode, ChildNode, InputIndex };
			TSharedPtr<FMutableGraphTreeElement>* CachedItem = ItemCache.Find(Key);

			if (CachedItem)
			{
				OutChildren.Add(*CachedItem);
			}
			else
			{
				TSharedPtr<FMutableGraphTreeElement>* MainItemPtr = MainItemPerNode.Find(ChildNode);
				TSharedPtr<FMutableGraphTreeElement> Item = MakeShareable(new FMutableGraphTreeElement(ChildNode, MainItemPtr, Prefix));
				OutChildren.Add(Item);
				ItemCache.Add(Key, Item);

				if (!MainItemPtr)
				{
					MainItemPerNode.Add(ChildNode, Item);
				}
			}
		}
		else
		{
			// No mutable node has been provided so create a dummy tree element
			TSharedPtr<FMutableGraphTreeElement> Item = MakeShareable(new FMutableGraphTreeElement(nullptr, nullptr , Prefix));
			OutChildren.Add(Item);
		}
		++InputIndex;
	};

	if (ParentNode->GetType() == mu::NodeObjectNew::GetStaticType())
	{
		mu::NodeObjectNew* ObjectNew = StaticCast<mu::NodeObjectNew*>(ParentNode);
		mu::NodeObjectNew::Private* Private = ObjectNew->GetPrivate();
		for (int32 l = 0; l < Private->m_lods.Num(); ++l)
		{
			AddChildFunc(Private->m_lods[l].get(), TEXT("LOD") );
		}

		for (int32 l = 0; l < Private->m_children.Num(); ++l)
		{
			AddChildFunc(Private->m_children[l].get(), TEXT("CHILD"));
		}
	}

	else if (ParentNode->GetType() == mu::NodeObjectGroup::GetStaticType())
	{
		mu::NodeObjectGroup* ObjectGroup = StaticCast<mu::NodeObjectGroup*>(ParentNode);
		mu::NodeObjectGroup::Private* Private = ObjectGroup->GetPrivate();
		for (int32 l = 0; l < Private->m_children.Num(); ++l)
		{
			AddChildFunc(Private->m_children[l].get(), TEXT("CHILD"));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceNew::GetStaticType())
	{
		mu::NodeSurfaceNew* SurfaceNew = StaticCast<mu::NodeSurfaceNew*>(ParentNode);
		mu::NodeSurfaceNew::Private* Private = SurfaceNew->GetPrivate();
		for (int32 l = 0; l < Private->m_meshes.Num(); ++l)
		{
			AddChildFunc(Private->m_meshes[l].m_pMesh.get(), TEXT("MESH"));
		}

		for (int32 l = 0; l < Private->m_images.Num(); ++l)
		{
			AddChildFunc(Private->m_images[l].m_pImage.get(), FString::Printf(TEXT("IMAGE [%s]"), *Private->m_images[l].m_name));
		}

		for (int32 l = 0; l < Private->m_vectors.Num(); ++l)
		{
			AddChildFunc(Private->m_vectors[l].m_pVector.get(), FString::Printf(TEXT("VECTOR [%s]"), *Private->m_vectors[l].m_name));
		}

		for (int32 l = 0; l < Private->m_scalars.Num(); ++l)
		{
			AddChildFunc(Private->m_scalars[l].m_pScalar.get(), FString::Printf(TEXT("SCALAR [%s]"), *Private->m_scalars[l].m_name));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceEdit::GetStaticType())
	{
		mu::NodeSurfaceEdit* SurfaceEdit = StaticCast<mu::NodeSurfaceEdit*>(ParentNode);
		mu::NodeSurfaceEdit::Private* Private = SurfaceEdit->GetPrivate();
		AddChildFunc(Private->m_pMesh.get(), TEXT("MESH"));
		AddChildFunc(Private->m_pMorph.get(), TEXT("MORPH"));
		AddChildFunc(Private->m_pFactor.get(), TEXT("MORPH_FACTOR"));

		for (int32 l = 0; l < Private->m_textures.Num(); ++l)
		{
			AddChildFunc(Private->m_textures[l].m_pExtend.get(), FString::Printf(TEXT("EXTEND [%d]"), l));
			AddChildFunc(Private->m_textures[l].m_pPatch.get(), FString::Printf(TEXT("PATCH [%d]"), l));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceSwitch::GetStaticType())
	{
		mu::NodeSurfaceSwitch* SurfaceSwitch = StaticCast<mu::NodeSurfaceSwitch*>(ParentNode);
		mu::NodeSurfaceSwitch::Private* Private = SurfaceSwitch->GetPrivate();
		AddChildFunc(Private->Parameter.get(), TEXT("PARAM"));
		for (int32 l = 0; l < Private->Options.Num(); ++l)
		{
			AddChildFunc(Private->Options[l].get(), FString::Printf(TEXT("OPTION [%d]"), l));
		}
	}

	else if (ParentNode->GetType() == mu::NodeSurfaceVariation::GetStaticType())
	{
		mu::NodeSurfaceVariation* SurfaceVar = StaticCast<mu::NodeSurfaceVariation*>(ParentNode);
		mu::NodeSurfaceVariation::Private* Private = SurfaceVar->GetPrivate();
		for (int32 l = 0; l < Private->m_defaultSurfaces.Num(); ++l)
		{
			AddChildFunc(Private->m_defaultSurfaces[l].get(), FString::Printf(TEXT("DEF SURF [%d]"), l));
		}
		for (int32 l = 0; l < Private->m_defaultModifiers.Num(); ++l)
		{
			AddChildFunc(Private->m_defaultModifiers[l].get(), FString::Printf(TEXT("DEF MOD [%d]"), l));
		}

		for (int32 v = 0; v < Private->m_variations.Num(); ++v)
		{
			const mu::NodeSurfaceVariation::Private::FVariation Var = Private->m_variations[v];
			for (int32 l = 0; l < Var.m_surfaces.Num(); ++l)
			{
				AddChildFunc(Var.m_surfaces[l].get(), FString::Printf(TEXT("VAR [%s] SURF [%d]"), *Var.m_tag, l));
			}
			for (int32 l = 0; l < Var.m_modifiers.Num(); ++l)
			{
				AddChildFunc(Var.m_modifiers[l].get(), FString::Printf(TEXT("VAR [%s] MOD [%d]"), *Var.m_tag, l));
			}
		}
	}
	
	else if (ParentNode->GetType() == mu::NodeLOD::GetStaticType())
	{
		mu::NodeLOD* LodVar = StaticCast<mu::NodeLOD*>(ParentNode);
		mu::NodeLOD::Private* Private = LodVar->GetPrivate();

		for (int32 Component = 0; Component < Private->m_components.Num(); Component++)
		{
			AddChildFunc(Private->m_components[Component].get(), FString::Printf(TEXT("COMP [%d]"),  Component));
		}
		for (int32 Modifier = 0; Modifier < Private->m_modifiers.Num(); Modifier++)
		{
			AddChildFunc(Private->m_modifiers[Modifier].get(), FString::Printf(TEXT("MOD [%d]"), Modifier));
		}
	}
	
	else if (ParentNode->GetType() == mu::NodeComponentNew::GetStaticType())
	{
		mu::NodeComponentNew* ComponentVar = StaticCast<mu::NodeComponentNew*>(ParentNode);
		mu::NodeComponentNew::Private* Private = ComponentVar->GetPrivate();
		for (int32 Surface = 0; Surface < Private->m_surfaces.Num(); Surface++)
		{
			AddChildFunc(Private->m_surfaces[Surface].get(), FString::Printf(TEXT("SURF [%d]"), Surface));
		}
	}

	else if (ParentNode->GetType() == mu::NodeComponentEdit::GetStaticType())
	{
		mu::NodeComponentEdit* ComponentEditVar = StaticCast<mu::NodeComponentEdit*>(ParentNode);
		mu::NodeComponentEdit::Private* Private = ComponentEditVar->GetPrivate();
		for (int32 Surface = 0; Surface < Private->m_surfaces.Num(); Surface++)
		{
			AddChildFunc(Private->m_surfaces[Surface].get(), FString::Printf(TEXT("SURF [%d]"), Surface));
		}
	}
	

	else if (ParentNode->GetType() == mu::NodeMeshConstant::GetStaticType())
	{
		mu::NodeMeshConstant* MeshConstantVar = StaticCast<mu::NodeMeshConstant*>(ParentNode);
		mu::NodeMeshConstant::Private* Private = MeshConstantVar->GetPrivate();
		for (int32 LayoutIndex = 0; LayoutIndex < Private->m_layouts.Num(); LayoutIndex++)
		{
			AddChildFunc(Private->m_layouts[LayoutIndex].get(), FString::Printf(TEXT("LAYOUT [%d]"), LayoutIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeImageFormat::GetStaticType())
	{
		mu::NodeImageFormat* ImageFormatVar = StaticCast<mu::NodeImageFormat*>(ParentNode);
		mu::NodeImageFormat::Private* Private = ImageFormatVar->GetPrivate();
		AddChildFunc(Private->m_source.get(), FString::Printf(TEXT("SOURCE IMAGE")));
	}

	else if (ParentNode->GetType() == mu::NodeMeshFormat::GetStaticType())
	{
		mu::NodeMeshFormat* MeshFormatVar = StaticCast<mu::NodeMeshFormat*>(ParentNode);
		mu::NodeMeshFormat::Private* Private = MeshFormatVar->GetPrivate();
		AddChildFunc(Private->m_pSource.get(), FString::Printf(TEXT("SOURCE MESH")));
	}

	else if (ParentNode->GetType() == mu::NodePatchImage::GetStaticType())
	{
		mu::NodePatchImage* PatchImageVar = StaticCast<mu::NodePatchImage*>(ParentNode);
		mu::NodePatchImage::Private* Private = PatchImageVar->GetPrivate();
		AddChildFunc(Private->m_pImage.get(), FString::Printf(TEXT("IMAGE")));
		AddChildFunc(Private->m_pMask.get(), FString::Printf(TEXT("MASK")));
	}

	else if (ParentNode->GetType() == mu::NodeModifierMeshClipMorphPlane::GetStaticType())
	{
		// Nothing to show
	}

	else if (ParentNode->GetType() == mu::NodeModifierMeshClipWithMesh::GetStaticType())
	{
		mu::NodeModifierMeshClipWithMesh* ModifierMeshClipWithMeshVar = StaticCast<mu::NodeModifierMeshClipWithMesh*>(ParentNode);
		mu::NodeModifierMeshClipWithMesh::Private* Private = ModifierMeshClipWithMeshVar->GetPrivate();
		AddChildFunc(Private->ClipMesh.get(), FString::Printf(TEXT("CLIP MESH")));
	}

	else if (ParentNode->GetType() == mu::NodeModifierMeshClipDeform::GetStaticType())
	{
		mu::NodeModifierMeshClipDeform* ModifierMeshClipDeformVar = StaticCast<mu::NodeModifierMeshClipDeform*>(ParentNode);
		mu::NodeModifierMeshClipDeform::Private* Private = ModifierMeshClipDeformVar->GetPrivate();
		AddChildFunc(Private->ClipMesh.get(), FString::Printf(TEXT("CLIP MESH")));
	}

	else if (ParentNode->GetType() == mu::NodeModifierMeshClipWithUVMask::GetStaticType())
	{
		mu::NodeModifierMeshClipWithUVMask* ModifierMeshClipWithUVMaskVar = StaticCast<mu::NodeModifierMeshClipWithUVMask*>(ParentNode);
		mu::NodeModifierMeshClipWithUVMask::Private* Private = ModifierMeshClipWithUVMaskVar->GetPrivate();
		AddChildFunc(Private->ClipMask.get(), FString::Printf(TEXT("CLIP MASK")));
	}
	
	else if (ParentNode->GetType() == mu::NodePatchMesh::GetStaticType())
	{
		mu::NodePatchMesh* PatchMeshVar = StaticCast<mu::NodePatchMesh*>(ParentNode);
		mu::NodePatchMesh::Private* Private = PatchMeshVar->GetPrivate();
		AddChildFunc(Private->m_pAdd.get(), FString::Printf(TEXT("ADD")));
		AddChildFunc(Private->m_pRemove.get(), FString::Printf(TEXT("REMOVE")));
	}

	else if (ParentNode->GetType() == mu::NodeImageSwitch::GetStaticType())
	{
		mu::NodeImageSwitch* ImageSwitchVar = StaticCast<mu::NodeImageSwitch*>(ParentNode);
		mu::NodeImageSwitch::Private* Private = ImageSwitchVar->GetPrivate();
		AddChildFunc(Private->m_pParameter.get(), FString::Printf(TEXT("PARAM")));
		for (int32 OptionIndex = 0; OptionIndex < Private->m_options.Num(); OptionIndex++)
		{
			AddChildFunc(Private->m_options[OptionIndex].get(), FString::Printf(TEXT("OPTION [%d]"), OptionIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeImageMipmap::GetStaticType())
	{
		mu::NodeImageMipmap* ImageMipMapVar = StaticCast<mu::NodeImageMipmap*>(ParentNode);
		mu::NodeImageMipmap::Private* Private = ImageMipMapVar->GetPrivate();
		AddChildFunc(Private->m_pSource.get(), FString::Printf(TEXT("SOURCE")));
		AddChildFunc(Private->m_pFactor.get(), FString::Printf(TEXT("FACTOR")));
	}

	else if (ParentNode->GetType() == mu::NodeImageLayer::GetStaticType())
	{
		mu::NodeImageLayer* ImageLayerVar = StaticCast<mu::NodeImageLayer*>(ParentNode);
		mu::NodeImageLayer::Private* Private = ImageLayerVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), FString::Printf(TEXT("BASE")));
		AddChildFunc(Private->m_pMask.get(), FString::Printf(TEXT("MASK")));
		AddChildFunc(Private->m_pBlended.get(), FString::Printf(TEXT("BLEND")));
	}
	
	else if (ParentNode->GetType() == mu::NodeImageLayerColour::GetStaticType())
	{
		mu::NodeImageLayerColour* ImageLayerColourVar = StaticCast<mu::NodeImageLayerColour*>(ParentNode);
		mu::NodeImageLayerColour::Private* Private = ImageLayerColourVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), FString::Printf(TEXT("BASE")));
		AddChildFunc(Private->m_pMask.get(), FString::Printf(TEXT("MASK")));
		AddChildFunc(Private->m_pColour.get(), FString::Printf(TEXT("COLOR")));
	}

	else if (ParentNode->GetType() == mu::NodeImageResize::GetStaticType())
	{
		mu::NodeImageResize* ImageResizeVar = StaticCast<mu::NodeImageResize*>(ParentNode);
		mu::NodeImageResize::Private* Private = ImageResizeVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), FString::Printf(TEXT("BASE")));
	}

	else if (ParentNode->GetType() == mu::NodeMeshMorph::GetStaticType())
	{
		mu::NodeMeshMorph* MeshMorphVar = StaticCast<mu::NodeMeshMorph*>(ParentNode);
		mu::NodeMeshMorph::Private* Private = MeshMorphVar->GetPrivate();
		AddChildFunc(Private->Base.get(), FString::Printf(TEXT("BASE")));
		AddChildFunc(Private->Morph.get(), FString::Printf(TEXT("MORPH")));
		AddChildFunc(Private->Factor.get(), FString::Printf(TEXT("FACTOR")));
	}

	else if (ParentNode->GetType() == mu::NodeImageProject::GetStaticType())
	{
		mu::NodeImageProject* ImageProjectVar = StaticCast<mu::NodeImageProject*>(ParentNode);
		mu::NodeImageProject::Private* Private = ImageProjectVar->GetPrivate();
		AddChildFunc(Private->m_pProjector.get(), FString::Printf(TEXT("PROJECTOR")));
		AddChildFunc(Private->m_pMesh.get(), FString::Printf(TEXT("MESH")));
		AddChildFunc(Private->m_pImage.get(), FString::Printf(TEXT("IMAGE")));
		AddChildFunc(Private->m_pMask.get(), FString::Printf(TEXT("MASK")));
		AddChildFunc(Private->m_pAngleFadeStart.get(), FString::Printf(TEXT("FADE START ANGLE")));
		AddChildFunc(Private->m_pAngleFadeEnd.get(), FString::Printf(TEXT("FADE END ANGLE")));
	}

	else if (ParentNode->GetType() == mu::NodeImagePlainColour::GetStaticType())
	{
		mu::NodeImagePlainColour* ImagePlainColourVar = StaticCast<mu::NodeImagePlainColour*>(ParentNode);
        mu::NodeImagePlainColour::Private* Private = ImagePlainColourVar->GetPrivate();
		AddChildFunc(Private->m_pColour.get(), FString::Printf(TEXT("COLOR")));
	}

	else if (ParentNode->GetType() == mu::NodeLayoutBlocks::GetStaticType())
	{
		// Nothing to show
	}

	else if (ParentNode->GetType() == mu::NodeScalarEnumParameter::GetStaticType())
	{
		mu::NodeScalarEnumParameter* ScalarEnumParameterVar = StaticCast<mu::NodeScalarEnumParameter*>(ParentNode);
		mu::NodeScalarEnumParameter::Private* Private = ScalarEnumParameterVar->GetPrivate();
		for (int32 RangeIndex = 0; RangeIndex < Private->m_ranges.Num(); RangeIndex++)
		{
			AddChildFunc(Private->m_ranges[RangeIndex].get(), FString::Printf(TEXT("RANGE [%d]"), RangeIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeMeshFragment::GetStaticType())
	{
		mu::NodeMeshFragment* MeshFragmentVar = StaticCast<mu::NodeMeshFragment*>(ParentNode);
		mu::NodeMeshFragment::Private* Private = MeshFragmentVar->GetPrivate();
		AddChildFunc(Private->m_pMesh.get(), FString::Printf(TEXT("MESH")));
	}

	else if (ParentNode->GetType() == mu::NodeColourSampleImage::GetStaticType())
	{
		mu::NodeColourSampleImage* ColorSampleImageVar = StaticCast<mu::NodeColourSampleImage*>(ParentNode);
		mu::NodeColourSampleImage::Private* Private = ColorSampleImageVar->GetPrivate();
		AddChildFunc(Private->m_pImage.get(), FString::Printf(TEXT("IMAGE")));
		AddChildFunc(Private->m_pX.get(), FString::Printf(TEXT("X")));
		AddChildFunc(Private->m_pY.get(), FString::Printf(TEXT("Y")));
	}

	else if (ParentNode->GetType() == mu::NodeImageInterpolate::GetStaticType())
	{
		mu::NodeImageInterpolate* ImageInterpolateVar = StaticCast<mu::NodeImageInterpolate*>(ParentNode);
		mu::NodeImageInterpolate::Private* Private = ImageInterpolateVar->GetPrivate();
		AddChildFunc(Private->m_pFactor.get(), FString::Printf(TEXT("FACTOR")));
		for (int32 TargetIndex = 0; TargetIndex < Private->m_targets.Num(); TargetIndex++)
		{
			AddChildFunc(Private->m_targets[TargetIndex].get(), FString::Printf(TEXT("TARGET [%d]"), TargetIndex));
		}
	}
	
	else if (ParentNode->GetType() == mu::NodeScalarConstant::GetStaticType())
	{
		// Nothing to show
	}

	else if (ParentNode->GetType() == mu::NodeScalarParameter::GetStaticType())
	{
		mu::NodeScalarParameter* ScalarParameterVar = StaticCast<mu::NodeScalarParameter*>(ParentNode);
		mu::NodeScalarParameter::Private* Private = ScalarParameterVar->GetPrivate();
		for (int32 RangeIndex = 0; RangeIndex < Private->m_ranges.Num(); RangeIndex++)
		{
			AddChildFunc(Private->m_ranges[RangeIndex].get(), FString::Printf(TEXT("RANGE [%d]"), RangeIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeColourParameter::GetStaticType())
	{
		mu::NodeColourParameter* ColorParameterVar = StaticCast<mu::NodeColourParameter*>(ParentNode);
		mu::NodeColourParameter::Private* Private = ColorParameterVar->GetPrivate();
		for (int32 RangeIndex = 0; RangeIndex < Private->m_ranges.Num(); RangeIndex++)
		{
			AddChildFunc(Private->m_ranges[RangeIndex].get(), FString::Printf(TEXT("RANGE [%d]"), RangeIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeColourConstant::GetStaticType())
	{
		// Nothing to show
	}


	else if (ParentNode->GetType() == mu::NodeImageConstant::GetStaticType())
	{
		// Nothing to show
	}

	
	else if (ParentNode->GetType() == mu::NodeScalarCurve::GetStaticType())
	{
		mu::NodeScalarCurve* ScalarCurveVar = StaticCast<mu::NodeScalarCurve*>(ParentNode);
		mu::NodeScalarCurve::Private* Private = ScalarCurveVar->GetPrivate();
		AddChildFunc(Private->m_input_scalar.get(), FString::Printf(TEXT("INPUT")));
	}

	else if (ParentNode->GetType() == mu::NodeMeshMakeMorph::GetStaticType())
	{
		mu::NodeMeshMakeMorph* MeshMakeMorphVar = StaticCast<mu::NodeMeshMakeMorph*>(ParentNode);
		mu::NodeMeshMakeMorph::Private* Private = MeshMakeMorphVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), FString::Printf(TEXT("BASE")));
		AddChildFunc(Private->m_pTarget.get(), FString::Printf(TEXT("TARGET")));
	}
	
	else if (ParentNode->GetType() == mu::NodeProjectorParameter::GetStaticType())
	{
		mu::NodeProjectorParameter* ProjectorParameterVar = StaticCast<mu::NodeProjectorParameter*>(ParentNode);
		mu::NodeProjectorParameter::Private* Private = ProjectorParameterVar->GetPrivate();
		for (int32 RangeIndex = 0; RangeIndex < Private->m_ranges.Num(); RangeIndex++)
		{
			AddChildFunc(Private->m_ranges[RangeIndex].get(), FString::Printf(TEXT("RANGE [%d]"), RangeIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeProjectorConstant::GetStaticType())
	{
		// Nothing to show
	}

	else if (ParentNode->GetType() == mu::NodeColourSwitch::GetStaticType())
	{
		mu::NodeColourSwitch* ColorSwitchVar = StaticCast<mu::NodeColourSwitch*>(ParentNode);
		mu::NodeColourSwitch::Private* Private = ColorSwitchVar->GetPrivate();
		AddChildFunc(Private->m_pParameter.get(), TEXT("PARAM"));
		for (int32 OptionIndex = 0; OptionIndex < Private->m_options.Num(); ++OptionIndex)
		{
			AddChildFunc(Private->m_options[OptionIndex].get(), FString::Printf(TEXT("OPTION [%d]"), OptionIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeImageSwizzle::GetStaticType())
	{
		mu::NodeImageSwizzle* ImageSwizzleVar = StaticCast<mu::NodeImageSwizzle*>(ParentNode);
		mu::NodeImageSwizzle::Private* Private = ImageSwizzleVar->GetPrivate();
		for (int32 SourceIndex = 0; SourceIndex < Private->m_sources.Num(); ++SourceIndex)
		{
			AddChildFunc(Private->m_sources[SourceIndex].get(), FString::Printf(TEXT("SOURCE [%d]"), SourceIndex));
		}
	}
	
	else if (ParentNode->GetType() == mu::NodeImageInvert::GetStaticType())
	{
		mu::NodeImageInvert* ImageInvertVar = StaticCast<mu::NodeImageInvert*>(ParentNode);
		mu::NodeImageInvert::Private* Private = ImageInvertVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), TEXT("BASE"));
	}

	else if (ParentNode->GetType() == mu::NodeImageMultiLayer::GetStaticType())
	{
		mu::NodeImageMultiLayer* ImageMultilayerVar = StaticCast<mu::NodeImageMultiLayer*>(ParentNode);
		mu::NodeImageMultiLayer::Private* Private = ImageMultilayerVar->GetPrivate();
		AddChildFunc(Private->m_pBase.get(), FString::Printf(TEXT("BASE")));
		AddChildFunc(Private->m_pMask.get(), FString::Printf(TEXT("MASK")));
		AddChildFunc(Private->m_pBlended.get(), FString::Printf(TEXT("BLEND")));
		AddChildFunc(Private->m_pRange.get(), FString::Printf(TEXT("RANGE")));
	}
	
	else if (ParentNode->GetType() == mu::NodeImageTable::GetStaticType())
	{
		// No nodes to show
	}

	else if (ParentNode->GetType() == mu::NodeMeshTable::GetStaticType())
	{
		mu::NodeMeshTable* MeshTableVar = StaticCast<mu::NodeMeshTable*>(ParentNode);
		mu::NodeMeshTable::Private* Private = MeshTableVar->GetPrivate();
		for (int32 LayoutIndex = 0; LayoutIndex < Private->Layouts.Num(); ++LayoutIndex)
		{
			AddChildFunc(Private->Layouts[LayoutIndex].get(), FString::Printf(TEXT("LAYOUT [%d]"), LayoutIndex));
		}
	}

	else if (ParentNode->GetType() == mu::NodeScalarTable::GetStaticType())
	{
		// Nothing to show
	}

	else if (ParentNode->GetType() == mu::NodeScalarSwitch::GetStaticType())
	{
		mu::NodeScalarSwitch* ScalarSwitchVar = StaticCast<mu::NodeScalarSwitch*>(ParentNode);
		mu::NodeScalarSwitch::Private* Private = ScalarSwitchVar->GetPrivate();
		AddChildFunc(Private->m_pParameter.get(), TEXT("PARAM"));
		for (int32 OptionIndex = 0; OptionIndex < Private->m_options.Num(); ++OptionIndex)
		{
			AddChildFunc(Private->m_options[OptionIndex].get(), FString::Printf(TEXT("OPTION [%d]"), OptionIndex));
		}
	}
	
	else if (ParentNode->GetType() == mu::NodeColourFromScalars::GetStaticType())
	{
		mu::NodeColourFromScalars* ScalarTableVar = StaticCast<mu::NodeColourFromScalars*>(ParentNode);
		mu::NodeColourFromScalars::Private* Private = ScalarTableVar->GetPrivate();
		AddChildFunc(Private->m_pX.get(), TEXT("X"));
		AddChildFunc(Private->m_pY.get(), TEXT("Y"));
		AddChildFunc(Private->m_pZ.get(), TEXT("Z"));
		AddChildFunc(Private->m_pW.get(), TEXT("W"));
	}
	
	else
	{
		const FString ParentNodeTypeString = ANSI_TO_TCHAR(ParentNode->GetType()->m_strName);
		UE_LOG(LogMutable,Error,TEXT("The node of type %s has not been implemented, so its children won't be added to the tree."), *ParentNodeTypeString);

		// Add a placeholder to the tree
		const FString Prefix =  FString::Printf(TEXT("[%s] NODE TYPE NOT IMPLEMENTED"), *ParentNodeTypeString);
		AddChildFunc(nullptr, Prefix);
	}
#endif
}


TSharedPtr<SWidget> SMutableGraphViewer::OnTreeContextMenuOpening()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Graph_Expand_Instance", "Expand Instance-Level Operations"),
		LOCTEXT("Graph_Expand_Instance_Tooltip", "Expands all the operations in the tree that are instance operations (not images, meshes, booleans, etc.)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SMutableGraphViewer::TreeExpandUnique)
			//, FCanExecuteAction::CreateSP(this, &SMutableCodeViewer::HasAnyItemInPalette)
		)
	);

	return MenuBuilder.MakeWidget();
}


void SMutableGraphViewer::TreeExpandRecursive(TSharedPtr<FMutableGraphTreeElement> InInfo, bool bExpand)
{
	if (bExpand)
	{
		TreeExpandUnique();
	}
}


void SMutableGraphViewer::TreeExpandUnique()
{
	TArray<TSharedPtr<FMutableGraphTreeElement>> Pending = RootNodes;

	TSet<TSharedPtr<FMutableGraphTreeElement>> Processed;

	TArray<TSharedPtr<FMutableGraphTreeElement>> Children;

	while (!Pending.IsEmpty())
	{
		TSharedPtr<FMutableGraphTreeElement> Item = Pending.Pop();
		TreeView->SetItemExpansion(Item, true);

		Children.SetNum(0);
		GetChildrenForInfo(Item, Children);
		Pending.Append(Children);
	}
}


FReply SMutableGraphViewer::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(Files[0]);

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}
	}

	return FReply::Unhandled();
}


FReply SMutableGraphViewer::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);

	if (TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>())
	{
		if (DragDropOp->HasFiles())
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if (Files.Num() == 1)
			{
				const FString DraggedFileExtension = FPaths::GetExtension(Files[0], true);
				if (DraggedFileExtension == TEXT(".mutable_source"))
				{
					// Dump source model to a file.
					mu::InputFileStream stream(Files[0]);

					char MutableSourceTag[4] = {};
					stream.Read(MutableSourceTag, 4);

					if (!FMemory::Memcmp(MutableSourceTag, MUTABLE_SOURCE_MODEL_FILETAG, 4))
					{
						mu::InputArchive arch(&stream);
						RootNode = mu::Node::StaticUnserialise( arch );
						DataTag = FString("dropped-file ") + FPaths::GetCleanFilename(Files[0]);
						RebuildTree();

						return FReply::Handled();
					}

					return FReply::Unhandled();
				}
			}
		}

		return FReply::Unhandled();
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE 


