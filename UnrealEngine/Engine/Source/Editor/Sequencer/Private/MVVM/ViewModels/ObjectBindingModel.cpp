// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/FolderModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/LayerBarModel.h"
#include "MVVM/ViewModels/BindingLifetimeTrackModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/TrackModelStorageExtension.h"
#include "MVVM/ObjectBindingModelStorageExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModelDragDropOp.h"
#include "MVVM/ViewModels/OutlinerColumns/OutlinerColumnTypes.h"
#include "MVVM/Views/SOutlinerObjectBindingView.h"
#include "MVVM/Views/STrackLane.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/Extensions/IRecyclableExtension.h"
#include "MVVM/Extensions/IBindingLifetimeExtension.h"
#include "ISequencerObjectSchema.h"
#include "Algo/Sort.h"
#include "AnimatedRange.h"
#include "ClassViewerModule.h"
#include "Containers/ArrayBuilder.h"
#include "Engine/LevelStreaming.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailCustomization.h"
#include "IDetailsView.h"
#include "ISequencerModule.h"
#include "ISequencerTrackEditor.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneDynamicBindingCustomization.h"
#include "UniversalObjectLocator.h"
#include "MovieSceneFolder.h"
#include "ObjectBindingTagCache.h"
#include "ObjectEditorUtils.h"
#include "PropertyEditorModule.h"
#include "PropertyPath.h"
#include "SObjectBindingTag.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"
#include "SequencerCommands.h"
#include "SequencerNodeTree.h"
#include "SequencerSettings.h"
#include "MVVM/Views/ViewUtilities.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SSequencerBindingLifetimeOverlay.h"

#define LOCTEXT_NAMESPACE "ObjectBindingModel"

namespace UE::Sequencer
{

namespace
{

struct PropertyMenuData
{
	FString MenuName;
	FPropertyPath PropertyPath;
};

void GetKeyablePropertyPaths(UClass* Class, void* ValuePtr, UStruct* PropertySource, FPropertyPath PropertyPath, FSequencer& Sequencer, TArray<FPropertyPath>& KeyablePropertyPaths)
{
	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bRecurseAllProperties = Sequencer.IsLevelEditorSequencer();

	for (TFieldIterator<FProperty> PropertyIterator(PropertySource); PropertyIterator; ++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;

		if (Property && !Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			PropertyPath.AddProperty(FPropertyInfo(Property));

			bool bIsPropertyKeyable = false;
			FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
			if (ArrayProperty)
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ValuePtr));
				for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
				{
					PropertyPath.AddProperty(FPropertyInfo(ArrayProperty->Inner, Index));

					if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath)))
					{
						KeyablePropertyPaths.Add(PropertyPath);
						bIsPropertyKeyable = true;
					}
					else if (FStructProperty* StructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
					{
						GetKeyablePropertyPaths(Class, ArrayHelper.GetRawPtr(Index), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
					}

					PropertyPath = *PropertyPath.TrimPath(1);
				}
			}

			if (!bIsPropertyKeyable)
			{
				bIsPropertyKeyable = Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath));
				if (bIsPropertyKeyable)
				{
					KeyablePropertyPaths.Add(PropertyPath);
				}
			}

			if (!bIsPropertyKeyable || bRecurseAllProperties)
			{
				if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					GetKeyablePropertyPaths(Class, StructProperty->ContainerPtrToValuePtr<void>(ValuePtr), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
				}
			}

			PropertyPath = *PropertyPath.TrimPath(1);
		}
	}
}

} // anon-namespace

FObjectBindingModel::FObjectBindingModel(FSequenceModel* InOwnerModel, const FMovieSceneBinding& InBinding)
	: ObjectBindingID(InBinding.GetObjectGuid())
	, TrackAreaList(EViewModelListType::TrackArea)
	, TopLevelChildTrackAreaList(GetTopLevelChildTrackAreaGroupType())
	, OwnerModel(InOwnerModel)
{
	RegisterChildList(&TrackAreaList);
	RegisterChildList(&TopLevelChildTrackAreaList);

	SetIdentifier(*ObjectBindingID.ToString());
}

FObjectBindingModel::~FObjectBindingModel()
{
}

EViewModelListType FObjectBindingModel::GetTopLevelChildTrackAreaGroupType()
{
	static EViewModelListType TopLevelChildTrackAreaGroup = RegisterCustomModelListType();
	return TopLevelChildTrackAreaGroup;
}

void FObjectBindingModel::OnConstruct()
{
	if (!LayerBar)
	{
		TSharedPtr<FSequencerEditorViewModel> EditorViewModel = GetEditor();
		TSharedPtr<FSequencer> Sequencer = EditorViewModel->GetSequencerImpl();

		if (Sequencer->GetSequencerSettings()->GetShowLayerBars())
		{
			LayerBar = MakeShared<FLayerBarModel>(AsShared());
			LayerBar->SetLinkedOutlinerItem(SharedThis(this));

			GetChildrenForList(&TopLevelChildTrackAreaList).AddChild(LayerBar);
		}
	}

	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	check(MovieScene);

	FMovieSceneBinding* Binding = MovieScene->FindBinding(ObjectBindingID);
	check(Binding);

	FScopedViewModelListHead RecycledHead(AsShared(), EViewModelListType::Recycled);
	GetChildrenForList(&OutlinerChildList).MoveChildrenTo<IRecyclableExtension>(RecycledHead.GetChildren(), IRecyclableExtension::CallOnRecycle);

	for (UMovieSceneTrack* Track : Binding->GetTracks())
	{
		AddTrack(Track);
	}
}

void FObjectBindingModel::SetParentBindingID(const FGuid& InObjectBindingID)
{
	ParentObjectBindingID = InObjectBindingID;
}

FGuid FObjectBindingModel::GetDesiredParentBinding() const
{
	return ParentObjectBindingID;
}

EObjectBindingType FObjectBindingModel::GetType() const
{
	return EObjectBindingType::Unknown;
}

const UClass* FObjectBindingModel::FindObjectClass() const
{
	return UObject::StaticClass();
}

bool FObjectBindingModel::SupportsRebinding() const
{
	return true;
}

FTrackAreaParameters FObjectBindingModel::GetTrackAreaParameters() const
{
	FTrackAreaParameters Params;
	Params.LaneType = ETrackAreaLaneType::Nested;
	return Params;
}

FViewModelVariantIterator FObjectBindingModel::GetTrackAreaModelList() const
{
	return &TrackAreaList;
}

FViewModelVariantIterator FObjectBindingModel::GetTopLevelChildTrackAreaModels() const
{
	return &TopLevelChildTrackAreaList;
}

void FObjectBindingModel::AddTrack(UMovieSceneTrack* Track)
{
	FTrackModelStorageExtension* TrackStorage = OwnerModel->CastDynamic<FTrackModelStorageExtension>();

	TViewModelPtr<FTrackModel> TrackModel = TrackStorage->CreateModelForTrack(Track, AsShared());

	GetChildrenForList(&OutlinerChildList).AddChild(TrackModel);

	if (TrackModel->IsA<IBindingLifetimeExtension>())
	{
		if (!BindingLifetimeOverlayModel)
		{
			BindingLifetimeOverlayModel = MakeShared<FBindingLifetimeOverlayModel>(AsShared(), GetEditor(), TrackModel.ImplicitCast());
			BindingLifetimeOverlayModel->SetLinkedOutlinerItem(SharedThis(this));
			GetChildrenForList(&TrackAreaList).AddChild(BindingLifetimeOverlayModel);
		}
	}
}

void FObjectBindingModel::RemoveTrack(UMovieSceneTrack* Track)
{
	FTrackModelStorageExtension* TrackStorage = OwnerModel->CastDynamic<FTrackModelStorageExtension>();

	TSharedPtr<FTrackModel> TrackModel = GetChildrenOfType<FTrackModel>().FindBy(Track, &FTrackModel::GetTrack);
	if (TrackModel)
	{
		TrackModel->RemoveFromParent();
		if (TrackModel->IsA<IBindingLifetimeExtension>())
		{
			if (BindingLifetimeOverlayModel)
			{
				BindingLifetimeOverlayModel->RemoveFromParent();
				BindingLifetimeOverlayModel.Reset();
			}
		}
	}
}

FGuid FObjectBindingModel::GetObjectGuid() const
{
	return ObjectBindingID;
}

FOutlinerSizing FObjectBindingModel::GetOutlinerSizing() const
{
	const float CompactHeight = 28.f;
	FViewDensityInfo Density = GetEditor()->GetViewDensity();
	return FOutlinerSizing(Density.UniformHeight.Get(CompactHeight));
}

void FObjectBindingModel::GetIdentifierForGrouping(TStringBuilder<128>& OutString) const
{
	FOutlinerItemModel::GetIdentifier().ToString(OutString);
}

TSharedPtr<SWidget> FObjectBindingModel::CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName)
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = GetEditor();
	TSharedPtr<FSequencer>                Sequencer       = EditorViewModel->GetSequencerImpl();

	if (InColumnName == FCommonOutlinerNames::Label)
	{
		const FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
		const MovieScene::FFixedObjectBindingID FixedObjectBindingID(ObjectBindingID, SequenceID);

		return SNew(SOutlinerItemViewBase, SharedThis(this), EditorViewModel, InParams.TreeViewRow)
			.AdditionalLabelContent()
			[
				SNew(SObjectBindingTags, FixedObjectBindingID, Sequencer->GetObjectBindingTagCache())
			];
	}

	if (InColumnName == FCommonOutlinerNames::Add)
	{
		return UE::Sequencer::MakeAddButton(
			LOCTEXT("TrackText", "Track"),
			FOnGetContent::CreateSP(this, &FObjectBindingModel::GetAddTrackMenuContent),
			SharedThis(this));
	}


	// Ask track editors to populate the column.
	// @todo: this is potentially very slow and will not scale as the number of track editors increases.
	const bool bIsEditColumn = InColumnName == FCommonOutlinerNames::Edit;
	TSharedPtr<SHorizontalBox> Box;

	auto GetEditBox = [&Box]
	{
		if (!Box)
		{
			Box = SNew(SHorizontalBox);

			auto CollapsedIfAllSlotsCollapsed = [Box]() -> EVisibility
			{
				for (int32 Index = 0; Index < Box->NumSlots(); ++Index)
				{
					EVisibility SlotVisibility = Box->GetSlot(Index).GetWidget()->GetVisibility();
					if (SlotVisibility != EVisibility::Collapsed)
					{
						return EVisibility::SelfHitTestInvisible;
					}
				}
				return EVisibility::Collapsed;
			};

			// Make the edit box collapsed if all of its slots are collapsed (or it has none)
			Box->SetVisibility(MakeAttributeLambda(CollapsedIfAllSlotsCollapsed));
		}
		return Box.ToSharedRef();
	};

	for (const TSharedPtr<ISequencerTrackEditor>& TrackEditor : Sequencer->GetTrackEditors())
	{
		TrackEditor->BuildObjectBindingColumnWidgets(GetEditBox, SharedThis(this), InParams, InColumnName);

		if (bIsEditColumn)
		{
			// Backwards compat
			GetEditBox();
			TrackEditor->BuildObjectBindingEditButtons(Box, ObjectBindingID, FindObjectClass());
		}
	}

	return Box && Box->NumSlots() != 0 ? Box : nullptr;
}

bool FObjectBindingModel::GetDefaultExpansionState() const
{
	// Object binding nodes are always expanded by default
	return true;
}

bool FObjectBindingModel::CanRename() const
{
	return true;
}

void FObjectBindingModel::Rename(const FText& NewName)
{
	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieSceneSequence* MovieSceneSequence = OwnerModel->GetSequence();

	if (MovieSceneSequence && Sequencer)
	{
		UMovieScene* MovieScene = MovieSceneSequence->GetMovieScene();

		FScopedTransaction Transaction(LOCTEXT("SetTrackName", "Set Track Name"));

		// Modify the movie scene so that it gets marked dirty and renames are saved consistently.
		MovieScene->Modify();

		FMovieSceneSpawnable*   Spawnable   = MovieScene->FindSpawnable(ObjectBindingID);
		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);

		// If there is only one binding, set the name of the bound actor
		TArrayView<TWeakObjectPtr<>> Objects = Sequencer->FindObjectsInCurrentSequence(ObjectBindingID);
		if (Objects.Num() == 1)
		{
			if (AActor* Actor = Cast<AActor>(Objects[0].Get()))
			{
				Actor->SetActorLabel(NewName.ToString());
			}
		}

		if (Spawnable)
		{
			// Otherwise set our display name
			Spawnable->SetName(NewName.ToString());
		}
		else if (Possessable)
		{
			Possessable->SetName(NewName.ToString());
		}
		else
		{
			MovieScene->SetObjectDisplayName(ObjectBindingID, NewName);
		}
	}
}

FText FObjectBindingModel::GetLabel() const
{
	UMovieSceneSequence* MovieSceneSequence = OwnerModel->GetSequence();
	if (MovieSceneSequence != nullptr)
	{
		return MovieSceneSequence->GetMovieScene()->GetObjectDisplayName(ObjectBindingID);
	}

	return FText();
}

FSlateColor FObjectBindingModel::GetLabelColor() const
{
	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();

	if (!Sequencer)
	{
		return FLinearColor::Red;
	}

	TArrayView<TWeakObjectPtr<> > BoundObjects = Sequencer->FindBoundObjects(ObjectBindingID, OwnerModel->GetSequenceID());

	if (BoundObjects.Num() > 0)
	{
		int32 NumValidObjects = 0;
		for (const TWeakObjectPtr<>& BoundObject : BoundObjects)
		{
			if (BoundObject.IsValid())
			{
				++NumValidObjects;
			}
		}

		if (NumValidObjects == BoundObjects.Num())
		{
			return FOutlinerItemModel::GetLabelColor();
		}

		if (NumValidObjects > 0)
		{
			return FLinearColor::Yellow;
		}
	}

	// Find the last objecting binding ancestor and ask it for the invalid color to use.
	// e.g. Spawnables don't have valid object bindings when their track hasn't spawned them yet,
	// so we override the default behavior of red with a gray so that users don't think there is something wrong.
	TFunction<FSlateColor(const FObjectBindingModel&)> GetObjectBindingAncestorInvalidLabelColor = [&](const FObjectBindingModel& InObjectBindingModel) -> FSlateColor {
		if (!Sequencer->State.GetBindingActivation(InObjectBindingModel.GetObjectGuid(), OwnerModel->GetSequenceID()))
		{
			return FSlateColor::UseSubduedForeground();
		}
		
		if (TSharedPtr<FObjectBindingModel> ParentBindingModel = InObjectBindingModel.FindAncestorOfType<FObjectBindingModel>())
		{
			return GetObjectBindingAncestorInvalidLabelColor(*ParentBindingModel.Get());
		}
		return InObjectBindingModel.GetInvalidBindingLabelColor();
	};

	return GetObjectBindingAncestorInvalidLabelColor(*this);
}

FText FObjectBindingModel::GetTooltipForSingleObjectBinding() const
{
	return FText::Format(LOCTEXT("PossessableBoundObjectToolTip", "(BindingID: {0}"), FText::FromString(LexToString(ObjectBindingID)));
}

FText FObjectBindingModel::GetLabelToolTipText() const
{
	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	if (!Sequencer)
	{
		return FText();
	}

	TArrayView<TWeakObjectPtr<>> BoundObjects = Sequencer->FindBoundObjects(ObjectBindingID, OwnerModel->GetSequenceID());

	if ( BoundObjects.Num() == 0 )
	{
		return FText::Format(LOCTEXT("InvalidBoundObjectToolTip", "The object bound to this track is missing (BindingID: {0})."), FText::FromString(LexToString(ObjectBindingID)));
	}
	else
	{
		TArray<FString> ValidBoundObjectLabels;
		FName BoundObjectClass;
		bool bAddEllipsis = false;
		int32 NumMissing = 0;
		for (const TWeakObjectPtr<>& Ptr : BoundObjects)
		{
			UObject* Obj = Ptr.Get();

			if (Obj == nullptr)
			{
				++NumMissing;
				continue;
			}

			if (Obj->GetClass())
			{
				BoundObjectClass = Obj->GetClass()->GetFName();
			}

			if (AActor* Actor = Cast<AActor>(Obj))
			{
				ValidBoundObjectLabels.Add(Actor->GetActorLabel());
			}
			else
			{
				ValidBoundObjectLabels.Add(Obj->GetName());
			}

			if (ValidBoundObjectLabels.Num() > 3)
			{
				bAddEllipsis = true;
				break;
			}
		}

		// If only 1 bound object, display a simpler tooltip.
		if (ValidBoundObjectLabels.Num() == 1 && NumMissing == 0)
		{
			return GetTooltipForSingleObjectBinding();
		}
		else if (ValidBoundObjectLabels.Num() == 0 && NumMissing == 1)
		{
			return FText::Format(LOCTEXT("InvalidBoundObjectToolTip", "The object bound to this track is missing (BindingID: {0})."), FText::FromString(LexToString(ObjectBindingID)));
		}

		FString MultipleBoundObjectLabel = FString::Join(ValidBoundObjectLabels, TEXT(", "));
		if (bAddEllipsis)
		{
			MultipleBoundObjectLabel += FString::Printf(TEXT("... %d more"), BoundObjects.Num()-3);
		}

		if (NumMissing != 0)
		{
			MultipleBoundObjectLabel += FString::Printf(TEXT(" (%d missing)"), NumMissing);
		}

		return FText::FromString(MultipleBoundObjectLabel + FString::Printf(TEXT(" Class: %s (BindingID: %s)"), *LexToString(BoundObjectClass), *LexToString(ObjectBindingID)));
	}
}

const FSlateBrush* FObjectBindingModel::GetIconBrush() const
{
	const UClass* ClassForObjectBinding = FindObjectClass();
	if (ClassForObjectBinding)
	{
		return FSlateIconFinder::FindIconBrushForClass(ClassForObjectBinding);
	}

	return FAppStyle::GetBrush("Sequencer.InvalidSpawnableIcon");
}

TSharedRef<SWidget> FObjectBindingModel::GetAddTrackMenuContent()
{
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	check(Sequencer);

	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bUseSubMenus = Sequencer->IsLevelEditorSequencer();

	UObject* BoundObject = Sequencer->FindSpawnedObjectOrTemplate(ObjectBindingID);

	const UClass* MainSelectionObjectClass = FindObjectClass();

	TArray<FGuid> ObjectBindings;
	ObjectBindings.Add(ObjectBindingID);

	TArray<UClass*> ObjectClasses;
	ObjectClasses.Add(const_cast<UClass*>(MainSelectionObjectClass));

	// Only include other selected object bindings if this binding is selected. Otherwise, this will lead to 
	// confusion with multiple tracks being added to possibly unrelated objects
	if (OwnerModel->GetEditor()->GetSelection()->Outliner.IsSelected(SharedThis(this)))
	{
		for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
		{
			const FGuid Guid = ObjectBindingNode->GetObjectGuid();
			for (auto RuntimeObject : Sequencer->FindBoundObjects(Guid, OwnerModel->GetSequenceID()))
			{
				if (RuntimeObject.Get() != nullptr)
				{
					ObjectBindings.AddUnique(Guid);
					ObjectClasses.Add(RuntimeObject->GetClass());
					continue;
				}
			}
		}
	}

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>( "Sequencer" );
	TSharedRef<FUICommandList> CommandList = MakeShared<FUICommandList>();

	TSharedRef<FExtender> Extender = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject)).ToSharedRef();

	TArray<TSharedPtr<FExtender>> AllExtenders;
	AllExtenders.Add(Extender);

	TArrayView<UObject* const>                   ContextObjects = BoundObject ? MakeArrayView(&BoundObject, 1) : TArrayView<UObject* const>();
	TMap<const IObjectSchema*, TArray<UObject*>> Map            = IObjectSchema::ComputeRelevancy(ContextObjects);

	for (const TPair<const IObjectSchema*, TArray<UObject*>>& Pair : Map)
	{
		TSharedPtr<FExtender> NewExtension = Pair.Key->ExtendObjectBindingMenu(CommandList, Sequencer, Pair.Value);
		if (NewExtension)
		{
			AllExtenders.Add(NewExtension);
		}
	}
	if (AllExtenders.Num())
	{
		Extender = FExtender::Combine(AllExtenders);
	}

	const UClass* ObjectClass = UClass::FindCommonBase(ObjectClasses);

	for (const TSharedPtr<ISequencerTrackEditor>& CurTrackEditor : Sequencer->GetTrackEditors())
	{
		CurTrackEditor->ExtendObjectBindingTrackMenu(Extender, ObjectBindings, ObjectClass);
	}

	// The menu are generated through reflection and sometime the API exposes some recursivity (think about a Widget returning it parent which is also a Widget). Just by reflection
	// it is not possible to determine when the root object is reached. It needs a kind of simulation which is not implemented. Also, even if the recursivity was correctly handled, the possible
	// permutations tend to grow exponentially. Until a clever solution is found, the simple approach is to disable recursively searching those menus. User can still search the current one though.
	// See UE-131257
	const bool bInRecursivelySearchable = false;

	FMenuBuilder AddTrackMenuBuilder(true, nullptr, Extender, false, &FCoreStyle::Get(), true, NAME_None, bInRecursivelySearchable);

	const int32 NumStartingBlocks = AddTrackMenuBuilder.GetMultiBox()->GetBlocks().Num();

	AddTrackMenuBuilder.BeginSection("Tracks", LOCTEXT("TracksMenuHeader" , "Tracks"));
	Sequencer->BuildObjectBindingTrackMenu(AddTrackMenuBuilder, ObjectBindings, ObjectClass);
	AddTrackMenuBuilder.EndSection();

	TArray<FPropertyPath> KeyablePropertyPaths;

	if (BoundObject != nullptr)
	{
		FPropertyPath PropertyPath;
		GetKeyablePropertyPaths(BoundObject->GetClass(), BoundObject, BoundObject->GetClass(), PropertyPath, *Sequencer, KeyablePropertyPaths);
	}

	// [Aspect Ratio]
	// [PostProcess Settings] [Bloom1Tint] [X]
	// [PostProcess Settings] [Bloom1Tint] [Y]
	// [PostProcess Settings] [ColorGrading]
	// [Ortho View]

	static const FString DefaultPropertyCategory = TEXT("Default");

	// Properties with the category "Default" have no category and should be sorted to the top
	struct FCategorySortPredicate
	{
		bool operator()(const FString& A, const FString& B) const
		{
			if (A == DefaultPropertyCategory)
			{
				return true;
			}
			else if (B == DefaultPropertyCategory)
			{
				return false;
			}
			else
			{
				return A.Compare(B) < 0;
			}
		}
	};

	bool bDefaultCategoryFound = false;

	// Create property menu data based on keyable property paths
	TMap<FString, TArray<PropertyMenuData>> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		FProperty* Property = KeyablePropertyPath.GetRootProperty().Property.Get();
		if (Property)
		{
			PropertyMenuData KeyableMenuData;
			KeyableMenuData.PropertyPath = KeyablePropertyPath;
			if (KeyablePropertyPath.GetRootProperty().ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("PropertyMenuTextFormat", "{0} [{1}]"), Property->GetDisplayNameText(), FText::AsNumber(KeyablePropertyPath.GetRootProperty().ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = Property->GetDisplayNameText().ToString();
			}

			FString CategoryText = FObjectEditorUtils::GetCategory(Property);

			if (CategoryText == DefaultPropertyCategory)
			{
				bDefaultCategoryFound = true;
			}

			KeyablePropertyMenuData.FindOrAdd(CategoryText).Add(KeyableMenuData);
		}
	}

	KeyablePropertyMenuData.KeySort(FCategorySortPredicate());

	// Always add an extension point for Properties section even if none are found (Components rely on this) 
	if (!bDefaultCategoryFound)
	{
		AddTrackMenuBuilder.BeginSection(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader", "Properties"));
		AddTrackMenuBuilder.EndSection();
	}

	// Add menu items
	for (TPair<FString, TArray<PropertyMenuData>>& Pair : KeyablePropertyMenuData)
	{
		// Sort on the property name
		Pair.Value.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
		{
			int32 CompareResult = A.MenuName.Compare(B.MenuName);
			return CompareResult < 0;
		});

		FString CategoryText = Pair.Key;
		
		if (CategoryText == DefaultPropertyCategory)
		{
			AddTrackMenuBuilder.BeginSection(SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader", "Properties"));
		}
		else
		{
			AddTrackMenuBuilder.BeginSection(NAME_None, FText::FromString(CategoryText));
		}
	
		for (int32 MenuDataIndex = 0; MenuDataIndex < Pair.Value.Num(); )
		{
			TArray<FPropertyPath> KeyableSubMenuPropertyPaths;

			KeyableSubMenuPropertyPaths.Add(Pair.Value[MenuDataIndex].PropertyPath);

			// If this menu data only has one property name, add the menu item
			if (Pair.Value[MenuDataIndex].PropertyPath.GetNumProperties() == 1 || !bUseSubMenus)
			{
				AddPropertyMenuItems(AddTrackMenuBuilder, KeyableSubMenuPropertyPaths, 0, -1);
				++MenuDataIndex;
			}
			// Otherwise, look to the next menu data to gather up new data
			else
			{
				for (; MenuDataIndex < Pair.Value.Num()-1; )
				{
					if (Pair.Value[MenuDataIndex].MenuName == Pair.Value[MenuDataIndex+1].MenuName)
					{	
						++MenuDataIndex;
						KeyableSubMenuPropertyPaths.Add(Pair.Value[MenuDataIndex].PropertyPath);
					}
					else
					{
						break;
					}
				}

				AddTrackMenuBuilder.AddSubMenu(
					FText::FromString(Pair.Value[MenuDataIndex].MenuName),
					FText::GetEmpty(), 
					FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::HandleAddTrackSubMenuNew, KeyableSubMenuPropertyPaths, 0));

				++MenuDataIndex;
			}
		}

		AddTrackMenuBuilder.EndSection();
	}

	if (AddTrackMenuBuilder.GetMultiBox()->GetBlocks().Num() == NumStartingBlocks)
	{
		TSharedRef<SWidget> EmptyTip = SNew(SBox)
			.Padding(FMargin(15.f, 7.5f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoKeyablePropertiesFound", "No keyable properties or tracks"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		AddTrackMenuBuilder.AddWidget(EmptyTip, FText(), true, false);
	}

	return AddTrackMenuBuilder.MakeWidget();
}

void FObjectBindingModel::HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPaths, int32 PropertyNameIndexStart)
{
	// [PostProcessSettings] [Bloom1Tint] [X]
	// [PostProcessSettings] [Bloom1Tint] [Y]
	// [PostProcessSettings] [ColorGrading]

	// Create property menu data based on keyable property paths
	TArray<FProperty*> PropertiesTraversed;
	TArray<int32> ArrayIndicesTraversed;
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		PropertyMenuData KeyableMenuData;
		KeyableMenuData.PropertyPath = KeyablePropertyPath;

		// If the path is greater than 1, keep track of the actual properties (not channels) and only add these properties once since we can't do single channel keying of a property yet.
		if (KeyablePropertyPath.GetNumProperties() > 1) //@todo
		{
			const FPropertyInfo& PropertyInfo = KeyablePropertyPath.GetPropertyInfo(1);
			FProperty* Property = PropertyInfo.Property.Get();

			// Search for any array elements
			int32 ArrayIndex = INDEX_NONE;
			for (int32 PropertyInfoIndex = 0; PropertyInfoIndex < KeyablePropertyPath.GetNumProperties(); ++PropertyInfoIndex)
			{
				const FPropertyInfo& ArrayPropertyInfo = KeyablePropertyPath.GetPropertyInfo(PropertyInfoIndex);
				if (ArrayPropertyInfo.ArrayIndex != INDEX_NONE)
				{
					ArrayIndex = ArrayPropertyInfo.ArrayIndex;
					break;
				}
			}

			bool bFound = false;
			for (int32 TraversedIndex = 0; TraversedIndex < PropertiesTraversed.Num(); ++TraversedIndex)
			{
				if (PropertiesTraversed[TraversedIndex] == Property && ArrayIndicesTraversed[TraversedIndex] == ArrayIndex)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				continue;
			}

			if (ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("ArrayElementFormat", "{0} [{1}]"), Property->GetDisplayNameText(), FText::AsNumber(ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = FObjectEditorUtils::GetCategoryFName(Property).ToString();
			}

			PropertiesTraversed.Add(Property);
			ArrayIndicesTraversed.Add(ArrayIndex);
		}
		else
		{
			// No sub menus items, so skip
			continue; 
		}
		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;
		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
		{
			if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
			{
				++MenuDataIndex;
				KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
			}
			else
			{
				break;
			}
		}

		AddTrackMenuBuilder.AddSubMenu(
			FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::AddPropertyMenuItems, KeyableSubMenuPropertyPaths, PropertyNameIndexStart + 1, PropertyNameIndexStart + 2));

		++MenuDataIndex;
	}
}

void FObjectBindingModel::HandlePropertyMenuItemExecute(FPropertyPath PropertyPath)
{
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	UObject* BoundObject = Sequencer->FindSpawnedObjectOrTemplate(ObjectBindingID);

	TArray<UObject*> KeyableBoundObjects;
	if (BoundObject != nullptr)
	{
		if (Sequencer->CanKeyProperty(FCanKeyPropertyParams(BoundObject->GetClass(), PropertyPath)))
		{
			KeyableBoundObjects.Add(BoundObject);
		}
	}

	// Only include other selected object bindings if this binding is selected. Otherwise, this will lead to 
	// confusion with multiple tracks being added to possibly unrelated objects
	if (OwnerModel->GetEditor()->GetSelection()->Outliner.IsSelected(SharedThis(this)))
	{
		for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
		{
			FGuid Guid = ObjectBindingNode->GetObjectGuid();
			for (auto RuntimeObject : Sequencer->FindBoundObjects(Guid, OwnerModel->GetSequenceID()))
			{
				if (Sequencer->CanKeyProperty(FCanKeyPropertyParams(RuntimeObject->GetClass(), PropertyPath)))
				{
					KeyableBoundObjects.AddUnique(RuntimeObject.Get());
				}
			}
		}
	}

	// When auto setting track defaults are disabled, force add a key so that the changed
	// value is saved and is propagated to the property.
	FKeyPropertyParams KeyPropertyParams(KeyableBoundObjects, PropertyPath, Sequencer->GetAutoSetTrackDefaults() == false ? ESequencerKeyMode::ManualKeyForced : ESequencerKeyMode::ManualKey);

	Sequencer->KeyProperty(KeyPropertyParams);
}

void FObjectBindingModel::AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd)
{
	TArray<PropertyMenuData> KeyablePropertyMenuData;

	for (auto KeyableProperty : KeyableProperties)
	{
		TArray<FString> PropertyNames;
		if (PropertyNameIndexEnd == -1)
		{
			PropertyNameIndexEnd = KeyableProperty.GetNumProperties();
		}

		//@todo
		if (PropertyNameIndexStart >= KeyableProperty.GetNumProperties())
		{
			continue;
		}

		for (int32 PropertyNameIndex = PropertyNameIndexStart; PropertyNameIndex < PropertyNameIndexEnd; ++PropertyNameIndex)
		{
			PropertyNames.Add(KeyableProperty.GetPropertyInfo(PropertyNameIndex).Property.Get()->GetDisplayNameText().ToString());
		}

		PropertyMenuData KeyableMenuData;
		{
			KeyableMenuData.PropertyPath = KeyableProperty;
			KeyableMenuData.MenuName = FString::Join( PropertyNames, TEXT( "." ) );
		}

		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); ++MenuDataIndex)
	{
		FUIAction AddTrackMenuAction(FExecuteAction::CreateSP(this, &FObjectBindingModel::HandlePropertyMenuItemExecute, KeyablePropertyMenuData[MenuDataIndex].PropertyPath));
		AddTrackMenuBuilder.AddMenuEntry(FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName), FText(), FSlateIcon(), AddTrackMenuAction);
	}
}


void FObjectBindingModel::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = GetEditor();
	FSequencer* Sequencer = EditorViewModel->GetSequencerImpl().Get();
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");

	UObject* BoundObject = Sequencer->FindSpawnedObjectOrTemplate(ObjectBindingID);
	const UClass* ObjectClass = FindObjectClass();
	
	TSharedPtr<FExtender> Extender = EditorViewModel->GetSequencerMenuExtender(
			SequencerModule.GetObjectBindingContextMenuExtensibilityManager(), TArrayBuilder<UObject*>().Add(BoundObject),
			&FSequencerCustomizationInfo::OnBuildObjectBindingContextMenu, SharedThis(this));
	if (Extender.IsValid())
	{
		MenuBuilder.PushExtender(Extender.ToSharedRef());
	}
	
	// Extenders can go in there.
	MenuBuilder.BeginSection("ObjectBindingActions");
	MenuBuilder.EndSection();

	// External extension.
	Sequencer->BuildCustomContextMenuForGuid(MenuBuilder, ObjectBindingID);

	// Track editor extension.
	TArray<FGuid> ObjectBindings;
	ObjectBindings.Add(ObjectBindingID);
	for (const TSharedPtr<ISequencerTrackEditor>& TrackEditor : Sequencer->GetTrackEditors())
	{
		TrackEditor->BuildObjectBindingContextMenu(MenuBuilder, ObjectBindings, ObjectClass);
	}

	// Up-call.
	FOutlinerItemModel::BuildContextMenu(MenuBuilder);
}

void FObjectBindingModel::BuildOrganizeContextMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddSubMenu(
		LOCTEXT("TagsLabel", "Tags"),
		LOCTEXT("TagsTooltip", "Show this object binding's tags"),
		FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::AddTagMenu)
	);

	FOutlinerItemModel::BuildOrganizeContextMenu(MenuBuilder);
}

void FObjectBindingModel::AddDynamicBindingMenu(FMenuBuilder& MenuBuilder, FMovieSceneDynamicBinding& DynamicBinding)
{
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowScrollBar = false;
	}

	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = false;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = false;
	}

	TSharedRef<IStructureDetailsView> StructureDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
		.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);

	// Register details customizations for this instance
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	StructureDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(
		FMovieSceneDynamicBinding::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMovieSceneDynamicBindingCustomization::MakeInstance, Sequence->GetMovieScene(), ObjectBindingID));

	// We can't just show the FMovieSceneDynamicBinding struct in the details view, because Slate only uses
	// the above details view customization for *properties* (not for the root object). So here we put a copy of
	// our dynamic binding struct inside a container, and when the details view is done setting values on it,
	// we copy these values back to the original dynamic binding.
	TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMovieSceneDynamicBindingContainer::StaticStruct());
	FMovieSceneDynamicBindingContainer* BufferContainer = (FMovieSceneDynamicBindingContainer*)StructOnScope->GetStructMemory();
	BufferContainer->DynamicBinding = DynamicBinding;
	StructureDetailsView->SetStructureData(StructOnScope);

	StructureDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &FObjectBindingModel::OnFinishedChangingDynamicBindingProperties, StructOnScope);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("DynamicBindingHeader", "Dynamic Binding"));
	{
		TSharedRef<SWidget> Widget = StructureDetailsView->GetWidget().ToSharedRef();
		MenuBuilder.AddWidget(Widget, FText());
	}
	MenuBuilder.EndSection();
}

void FObjectBindingModel::OnFinishedChangingDynamicBindingProperties(const FPropertyChangedEvent& ChangeEvent, TSharedPtr<FStructOnScope> ValueStruct)
{
	auto* Container = (FMovieSceneDynamicBindingContainer*)ValueStruct->GetStructMemory();

	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBindingID);
	if (Possessable)
	{
		Possessable->DynamicBinding = Container->DynamicBinding;
		return;
	}
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (Spawnable)
	{
		Spawnable->DynamicBinding = Container->DynamicBinding;
		return;
	}
}

void FObjectBindingModel::AddTagMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(FSequencerCommands::Get().OpenTaggedBindingManager);

	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();

	UMovieSceneSequence* Sequence   = Sequencer->GetRootMovieSceneSequence();
	UMovieScene*         MovieScene = Sequence->GetMovieScene();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ObjectTagsHeader", "Object Tags"));
	{
		TSet<FName> AllTags;

		// Gather all the tags on all currently selected object binding IDs
		FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
		for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
		{
			const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

			UE::MovieScene::FFixedObjectBindingID BindingID(ObjectID, SequenceID);
			for (auto It = Sequencer->GetObjectBindingTagCache()->IterateTags(BindingID); It; ++It)
			{
				AllTags.Add(It.Value());
			}
		}

		bool bIsReadOnly = MovieScene->IsReadOnly();
		for (const FName& TagName : AllTags)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(TagName),
				FText(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &FObjectBindingModel::ToggleTag, TagName),
					FCanExecuteAction::CreateLambda([bIsReadOnly] { return bIsReadOnly == false; }),
					FGetActionCheckState::CreateSP(this, &FObjectBindingModel::GetTagCheckState, TagName)
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddNewHeader", "Add Tag"));
	{
		if (!MovieScene->IsReadOnly())
		{
			TSharedRef<SWidget> Widget =
				SNew(SObjectBindingTag)
				.OnCreateNew(this, &FObjectBindingModel::HandleAddTag);

			MenuBuilder.AddWidget(Widget, FText());
		}
	}
	MenuBuilder.EndSection();
}

ECheckBoxState FObjectBindingModel::GetTagCheckState(FName TagName)
{
	ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;

	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();

	for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
	{
		const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

		UE::MovieScene::FFixedObjectBindingID BindingID(ObjectID, SequenceID);
		ECheckBoxState ThisCheckState = Sequencer->GetObjectBindingTagCache()->HasTag(BindingID, TagName)
			? ECheckBoxState::Checked
			: ECheckBoxState::Unchecked;

		if (CheckBoxState == ECheckBoxState::Undetermined)
		{
			CheckBoxState = ThisCheckState;
		}
		else if (CheckBoxState != ThisCheckState)
		{
			return ECheckBoxState::Undetermined;
		}
	}

	return CheckBoxState;
}

void FObjectBindingModel::ToggleTag(FName TagName)
{
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();

	for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
	{
		const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

		UE::MovieScene::FFixedObjectBindingID BindingID(ObjectID, SequenceID);
		if (!Sequencer->GetObjectBindingTagCache()->HasTag(BindingID, TagName))
		{
			HandleAddTag(TagName);
			return;
		}
	}

	HandleDeleteTag(TagName);
}

void FObjectBindingModel::HandleDeleteTag(FName TagName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("RemoveBindingTag", "Remove tag '{0}' from binding(s)"), FText::FromName(TagName)));

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = Sequencer->GetRootMovieSceneSequence()->GetMovieScene();
	MovieScene->Modify();

	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
	for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
	{
		const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();
		MovieScene->UntagBinding(TagName, UE::MovieScene::FFixedObjectBindingID(ObjectID, SequenceID));
	}
}

void FObjectBindingModel::HandleAddTag(FName TagName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("CreateBindingTag", "Add new tag {0} to binding(s)"), FText::FromName(TagName)));

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = Sequencer->GetRootMovieSceneSequence()->GetMovieScene();
	MovieScene->Modify();

	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
	for (TViewModelPtr<FObjectBindingModel> ObjectBindingNode : OwnerModel->GetEditor()->GetSelection()->Outliner.Filter<FObjectBindingModel>())
	{
		const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();
		MovieScene->TagBinding(TagName, UE::MovieScene::FFixedObjectBindingID(ObjectID, SequenceID));
	}
}

void FObjectBindingModel::SortChildren()
{
	ISortableExtension::SortChildren(SharedThis(this), ESortingMode::PriorityFirst);
}

FSortingKey FObjectBindingModel::GetSortingKey() const
{
	FSortingKey SortingKey;

	if (OwnerModel)
	{
		UMovieScene* MovieScene = OwnerModel->GetMovieScene();
		const FMovieSceneBinding* MovieSceneBinding = MovieScene->FindBinding(ObjectBindingID);

		if (MovieSceneBinding)
		{
			SortingKey.CustomOrder = MovieSceneBinding->GetSortingOrder();
		}

		SortingKey.DisplayName = MovieScene->GetObjectDisplayName(ObjectBindingID);
	}

	// When inside object bindings, we come before tracks. Elsewhere, we come after tracks.
	const bool bHasParentObjectBinding = (CastParent<IObjectBindingExtension>() != nullptr);
	SortingKey.PrioritizeBy(bHasParentObjectBinding ? 2 : 1);

	return SortingKey;
}

void FObjectBindingModel::SetCustomOrder(int32 InCustomOrder)
{
	if (OwnerModel)
	{
		UMovieScene* MovieScene = OwnerModel->GetMovieScene();
		FMovieSceneBinding* MovieSceneBinding = MovieScene->FindBinding(ObjectBindingID);
		if (MovieSceneBinding)
		{
			MovieSceneBinding->SetSortingOrder(InCustomOrder);
		}
	}
}

bool FObjectBindingModel::CanDrag() const
{
	// Can only drag top level object bindings
	TSharedPtr<IObjectBindingExtension> ObjectBindingExtension = FindAncestorOfType<IObjectBindingExtension>();
	return ObjectBindingExtension == nullptr;
}

bool FObjectBindingModel::CanDelete(FText* OutErrorMessage) const
{
	return true;
}

void FObjectBindingModel::Delete()
{
	if (OwnerModel)
	{
		TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
		UMovieScene* MovieScene = Sequencer->GetRootMovieSceneSequence()->GetMovieScene();

		MovieScene->Modify();

		// Untag this binding
		UE::MovieScene::FFixedObjectBindingID BindingID(ObjectBindingID, OwnerModel->GetSequenceID());
		for (auto It = OwnerModel->GetSequencerImpl()->GetObjectBindingTagCache()->IterateTags(BindingID); It; ++It)
		{
			MovieScene->UntagBinding(It.Value(), BindingID);
		}

		// Delete any child object bindings - this will remove their tracks implicitly
		// so no need to delete those manually
		for (const TViewModelPtr<FObjectBindingModel>& ChildObject : GetChildrenOfType<FObjectBindingModel>(EViewModelListType::Outliner).ToArray())
		{
			ChildObject->Delete();
		}

		// Remove from a parent folder if necessary.
		if (TViewModelPtr<FFolderModel> ParentFolder = CastParent<FFolderModel>())
		{
			ParentFolder->GetFolder()->RemoveChildObjectBinding(ObjectBindingID);
		}

		// Delete any loaded object that may be bound to this object binding
		if (FMovieSceneObjectCache* Cache = Sequencer->State.FindObjectCache(OwnerModel->GetSequenceID()))
		{
			Cache->UnloadBinding(ObjectBindingID, Sequencer->GetSharedPlaybackState());
		}

		BindingLifetimeOverlayModel.Reset();
	}
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE

