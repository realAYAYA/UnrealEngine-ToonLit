// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/FolderModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/LayerBarModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/TrackModelStorageExtension.h"
#include "MVVM/ObjectBindingModelStorageExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModelDragDropOp.h"
#include "MVVM/Views/SOutlinerObjectBindingView.h"
#include "MVVM/Extensions/IRecyclableExtension.h"
#include "Modules/ModuleManager.h"
#include "ISequencerModule.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "MovieSceneBinding.h"
#include "PropertyPath.h"
#include "ScopedTransaction.h"
#include "Sequencer.h"
#include "SequencerNodeTree.h"
#include "SequencerCommands.h"
#include "SequencerSettings.h"
#include "SequencerUtilities.h"
#include "ISequencerTrackEditor.h"
#include "SObjectBindingTag.h"
#include "Containers/ArrayBuilder.h"
#include "ObjectBindingTagCache.h"
#include "ObjectEditorUtils.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "ClassViewerModule.h"
#include "Algo/Sort.h"
#include "Engine/LevelStreaming.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "ObjectBindingModel"

namespace UE
{
namespace Sequencer
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

			bool bIsPropertyKeyable = Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath));
			if (bIsPropertyKeyable)
			{
				KeyablePropertyPaths.Add(PropertyPath);
			}

			FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
			if (!bIsPropertyKeyable && ArrayProperty)
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

struct FMovieSceneSpawnableFlagCheckState
{
	FSequencer* Sequencer;
	UMovieScene* MovieScene;
	bool FMovieSceneSpawnable::*PtrToFlag;

	ECheckBoxState operator()() const
	{
		using namespace UE::Sequencer;

		ECheckBoxState CheckState = ECheckBoxState::Undetermined;
		for (TWeakPtr<FViewModel> WeakItem : Sequencer->GetSelection().GetSelectedOutlinerItems())
		{
			if (IObjectBindingExtension* ObjectBindingID = ICastable::CastWeakPtr<IObjectBindingExtension>(WeakItem))
			{
				FMovieSceneSpawnable* SelectedSpawnable = MovieScene->FindSpawnable(ObjectBindingID->GetObjectGuid());
				if (SelectedSpawnable)
				{
					if (CheckState != ECheckBoxState::Undetermined && SelectedSpawnable->*PtrToFlag != ( CheckState == ECheckBoxState::Checked ))
					{
						return ECheckBoxState::Undetermined;
					}
					CheckState = SelectedSpawnable->*PtrToFlag ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				}
			}
		}
		return CheckState;
	}
};

struct FMovieSceneSpawnableFlagToggler
{
	FSequencer* Sequencer;
	UMovieScene* MovieScene;
	bool FMovieSceneSpawnable::*PtrToFlag;
	FText TransactionText;

	void operator()() const
	{
		using namespace UE::Sequencer;

		FScopedTransaction Transaction(TransactionText);

		const ECheckBoxState CheckState = FMovieSceneSpawnableFlagCheckState{Sequencer, MovieScene, PtrToFlag}();

		MovieScene->Modify();
		for (TWeakPtr<FViewModel> WeakItem : Sequencer->GetSelection().GetSelectedOutlinerItems())
		{
			if (IObjectBindingExtension* ObjectBinding = ICastable::CastWeakPtr<IObjectBindingExtension>(WeakItem))
			{
				FMovieSceneSpawnable* SelectedSpawnable = MovieScene->FindSpawnable(ObjectBinding->GetObjectGuid());
				if (SelectedSpawnable)
				{
					SelectedSpawnable->*PtrToFlag = (CheckState == ECheckBoxState::Unchecked);
				}
			}
		}
	}
};

} // anon-namespace

FObjectBindingModel::FObjectBindingModel(FSequenceModel* InOwnerModel, const FMovieSceneBinding& InBinding)
	: ObjectBindingID(InBinding.GetObjectGuid())
	, TrackAreaList(EViewModelListType::TrackArea)
	, OwnerModel(InOwnerModel)
{
	RegisterChildList(&TrackAreaList);

	SetIdentifier(*ObjectBindingID.ToString());
}

FObjectBindingModel::~FObjectBindingModel()
{
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
			LayerBar->SetLinkedOutlinerItem(AsShared());

			GetChildrenForList(&TrackAreaList).AddChild(LayerBar);
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
	Params.LaneType = ETrackAreaLaneType::Inline;
	return Params;
}

FViewModelVariantIterator FObjectBindingModel::GetTrackAreaModelList() const
{
	return &TrackAreaList;
}

void FObjectBindingModel::AddTrack(UMovieSceneTrack* Track)
{
	FTrackModelStorageExtension* TrackStorage = OwnerModel->CastDynamic<FTrackModelStorageExtension>();

	TSharedPtr<FTrackModel> TrackModel = TrackStorage->CreateModelForTrack(Track, AsShared());

	GetChildrenForList(&OutlinerChildList).AddChild(TrackModel);
}

void FObjectBindingModel::RemoveTrack(UMovieSceneTrack* Track)
{
	FTrackModelStorageExtension* TrackStorage = OwnerModel->CastDynamic<FTrackModelStorageExtension>();

	TSharedPtr<FTrackModel> TrackModel = GetChildrenOfType<FTrackModel>().FindBy(Track, &FTrackModel::GetTrack);
	if (TrackModel)
	{
		TrackModel->RemoveFromParent();
	}
}

FGuid FObjectBindingModel::GetObjectGuid() const
{
	return ObjectBindingID;
}

FOutlinerSizing FObjectBindingModel::GetOutlinerSizing() const
{
	return FOutlinerSizing(20.f, 4.f);
}

void FObjectBindingModel::GetIdentifierForGrouping(TStringBuilder<128>& OutString) const
{
	FOutlinerItemModel::GetIdentifier().ToString(OutString);
}

TSharedRef<SWidget> FObjectBindingModel::CreateOutlinerView(const FCreateOutlinerViewParams& InParams)
{
	TSharedPtr<FSequencerEditorViewModel> EditorViewModel = GetEditor();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel->GetSequencerImpl();

	const FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
	const MovieScene::FFixedObjectBindingID FixedObjectBindingID(ObjectBindingID, SequenceID);

	return SNew(SOutlinerObjectBindingView, SharedThis(this), EditorViewModel, InParams.TreeViewRow)
		.AdditionalLabelContent()
		[
			SNew(SObjectBindingTags, FixedObjectBindingID, Sequencer->GetObjectBindingTagCache())
		];
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

		SetIdentifier(FName(*NewName.ToString()));
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

	// Spawnables don't have valid object bindings when their track hasn't spawned them yet,
	// so we override the default behavior of red with a gray so that users don't think there is something wrong.
	constexpr bool bIncludeThis = true;
	for (TSharedPtr<FObjectBindingModel> Parent : GetAncestorsOfType<FObjectBindingModel>(bIncludeThis))
	{
		if (Parent->GetType() == EObjectBindingType::Spawnable)
		{
			return FSlateColor::UseSubduedForeground();
		}
	}

	return FLinearColor::Red;
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
	if (Sequencer->GetSelection().IsSelected(AsShared()))
	{
		for (const TWeakPtr<FViewModel>& Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
		{
			const FObjectBindingModel* ObjectBindingNode = Node.Pin()->CastThisChecked<FObjectBindingModel>();
			if (!ObjectBindingNode)
			{
				continue;
			}

			const FGuid Guid = ObjectBindingNode->GetObjectGuid();
			for (auto RuntimeObject : Sequencer->FindBoundObjects(Guid, OwnerModel->GetSequenceID()))
			{
				if (RuntimeObject != nullptr)
				{
					ObjectBindings.AddUnique(Guid);
					ObjectClasses.Add(RuntimeObject->GetClass());
					continue;
				}
			}
		}
	}

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>( "Sequencer" );
	TSharedRef<FUICommandList> CommandList(new FUICommandList);

	TSharedRef<FExtender> Extender = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject)).ToSharedRef();

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

	// Create property menu data based on keyable property paths
	TArray<PropertyMenuData> KeyablePropertyMenuData;
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
			KeyablePropertyMenuData.Add(KeyableMenuData);
		}
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});
	

	// Add menu items
	AddTrackMenuBuilder.BeginSection( SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader" , "Properties"));
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;

		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		// If this menu data only has one property name, add the menu item
		if (KeyablePropertyMenuData[MenuDataIndex].PropertyPath.GetNumProperties() == 1 || !bUseSubMenus)
		{
			AddPropertyMenuItems(AddTrackMenuBuilder, KeyableSubMenuPropertyPaths, 0, -1);
			++MenuDataIndex;
		}
		// Otherwise, look to the next menu data to gather up new data
		else
		{
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
				FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::HandleAddTrackSubMenuNew, KeyableSubMenuPropertyPaths, 0));

			++MenuDataIndex;
		}
	}
	AddTrackMenuBuilder.EndSection();

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

	for (const TWeakPtr<FViewModel>& Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
	{
		FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node);
		if (ObjectBindingNode)
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
	FSequencer* Sequencer = GetEditor()->GetSequencerImpl().Get();
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");

	UObject* BoundObject = Sequencer->FindSpawnedObjectOrTemplate(ObjectBindingID);
	const UClass* ObjectClass = FindObjectClass();

	TSharedRef<FUICommandList> CommandList(new FUICommandList);
	TSharedPtr<FExtender> Extender = SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject));
	if (Extender.IsValid())
	{
		MenuBuilder.PushExtender(Extender.ToSharedRef());
	}

	if (Sequencer->IsLevelEditorSequencer())
	{
		UMovieScene* MovieScene = OwnerModel->GetMovieScene();
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);

		if (Spawnable)
		{
			MenuBuilder.BeginSection("Spawnable", LOCTEXT("SpawnableMenuSectionName", "Spawnable"));
	
			MenuBuilder.AddSubMenu(
				LOCTEXT("OwnerLabel", "Spawned Object Owner"),
				LOCTEXT("OwnerTooltip", "Specifies how the spawned object is to be owned"),
				FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::AddSpawnOwnershipMenu)
			);

			MenuBuilder.AddSubMenu(
				LOCTEXT("SubLevelLabel", "Spawnable Level"),
				LOCTEXT("SubLevelTooltip", "Specifies which level the spawnable should be spawned into"),
				FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::AddSpawnLevelMenu)
			);

			MenuBuilder.AddSubMenu(
				LOCTEXT("ChangeClassLabel", "Change Class"),
				LOCTEXT("ChangeClassTooltip", "Change the class (object template) that this spawns from"),
				FNewMenuDelegate::CreateSP(this, &FObjectBindingModel::AddChangeClassMenu));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ContinuouslyRespawn", "Continuously Respawn"),
				LOCTEXT("ContinuouslyRespawnTooltip", "When enabled, this spawnable will always be respawned if it gets destroyed externally. When disabled, this object will only ever be spawned once for each spawn key even if it is destroyed externally"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda(FMovieSceneSpawnableFlagToggler{Sequencer, MovieScene, &FMovieSceneSpawnable::bContinuouslyRespawn, LOCTEXT("ContinuouslyRespawnTransaction", "Set Continuously Respawn")}),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda(FMovieSceneSpawnableFlagCheckState{Sequencer, MovieScene, &FMovieSceneSpawnable::bContinuouslyRespawn})
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("EvaluateTracksWhenNotSpawned", "Evaluate Tracks When Not Spawned"),
				LOCTEXT("EvaluateTracksWhenNotSpawnedTooltip", "When enabled, any tracks on this object binding or its children will still be evaluated even when the object is not spawned."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda(FMovieSceneSpawnableFlagToggler{Sequencer, MovieScene, &FMovieSceneSpawnable::bEvaluateTracksWhenNotSpawned, LOCTEXT("EvaluateTracksWhenNotSpawned_Transaction", "Evaluate Tracks When Not Spawned")}),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda(FMovieSceneSpawnableFlagCheckState{Sequencer, MovieScene, &FMovieSceneSpawnable::bEvaluateTracksWhenNotSpawned})
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("NetAddressable", "Net Addressable"),
				LOCTEXT("NetAddressableTooltip", "When enabled, this spawnable will be spawned using a unique name that allows it to be addressed by the server and client (useful for relative movement calculations on spawned props)"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda(FMovieSceneSpawnableFlagToggler{Sequencer, MovieScene, &FMovieSceneSpawnable::bNetAddressableName, LOCTEXT("NetAddressableTransaction", "Set Net Addressable")}),
					FCanExecuteAction(),
					FGetActionCheckState::CreateLambda(FMovieSceneSpawnableFlagCheckState{Sequencer, MovieScene, &FMovieSceneSpawnable::bNetAddressableName})
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SaveCurrentSpawnableState );
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToPossessable );

			MenuBuilder.EndSection();
		}
		else
		{
			MenuBuilder.BeginSection("Possessable");

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToSpawnable );

			MenuBuilder.EndSection();
		}

		MenuBuilder.BeginSection("Import/Export", LOCTEXT("ImportExportMenuSectionName", "Import/Export"));
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ImportFBX", "Import..."),
			LOCTEXT("ImportFBXTooltip", "Import FBX animation to this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ Sequencer->ImportFBXOntoSelectedNodes(); })
			));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExportFBX", "Export..."),
			LOCTEXT("ExportFBXTooltip", "Export FBX animation from this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ Sequencer->ExportFBX(); })
			));
			
		MenuBuilder.EndSection();
	}

	Sequencer->BuildCustomContextMenuForGuid(MenuBuilder, ObjectBindingID);
	TArray<FGuid> ObjectBindings;
	ObjectBindings.Add(ObjectBindingID);
	for (const TSharedPtr<ISequencerTrackEditor>& TrackEditor : Sequencer->GetTrackEditors())
	{
		TrackEditor->BuildObjectBindingContextMenu(MenuBuilder, ObjectBindings, ObjectClass);
	}

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

void FObjectBindingModel::AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}
	auto Callback = [=](ESpawnOwnership NewOwnership){

		FScopedTransaction Transaction(LOCTEXT("SetSpawnOwnership", "Set Spawnable Ownership"));

		Spawnable->SetSpawnOwnership(NewOwnership);

		// Overwrite the completion state for all spawn sections to ensure the expected behaviour.
		EMovieSceneCompletionMode NewCompletionMode = NewOwnership == ESpawnOwnership::InnerSequence ? EMovieSceneCompletionMode::RestoreState : EMovieSceneCompletionMode::KeepState;

		// Make all spawn sections retain state
		UMovieSceneSpawnTrack* SpawnTrack = MovieScene->FindTrack<UMovieSceneSpawnTrack>(ObjectBindingID);
		if (SpawnTrack)
		{
			for (UMovieSceneSection* Section : SpawnTrack->GetAllSections())
			{
				Section->Modify();
				Section->EvalOptions.CompletionMode = NewCompletionMode;
			}
		}
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThisSequence_Label", "This Sequence"),
		LOCTEXT("ThisSequence_Tooltip", "Indicates that this sequence will own the spawned object. The object will be destroyed at the end of the sequence."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::InnerSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MasterSequence_Label", "Master Sequence"),
		LOCTEXT("MasterSequence_Tooltip", "Indicates that the outermost sequence will own the spawned object. The object will be destroyed when the outermost sequence stops playing."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::MasterSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::MasterSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("External_Label", "External"),
		LOCTEXT("External_Tooltip", "Indicates this object's lifetime is managed externally once spawned. It will not be destroyed by sequencer."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::External),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::External; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

void FObjectBindingModel::AddSpawnLevelMenu(FMenuBuilder& MenuBuilder)
{
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { Sequencer->SetSelectedNodesSpawnableLevel(NAME_None); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == NAME_None; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	UWorld* World = Cast<UWorld>(Sequencer->GetPlaybackContext());
	if (!World)
	{
		return;
	}

	for (ULevelStreaming* LevelStreaming : World->GetStreamingLevels())
	{
		if (LevelStreaming)
		{
			FName LevelName = FPackageName::GetShortFName( LevelStreaming->GetWorldAssetPackageFName() );

			MenuBuilder.AddMenuEntry(
				FText::FromName(LevelName),
				FText::FromName(LevelName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] { Sequencer->SetSelectedNodesSpawnableLevel(LevelName); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == LevelName; })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

void FObjectBindingModel::AddChangeClassMenu(FMenuBuilder& MenuBuilder)
{
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bIsActorsOnly = true;
	Options.bIsPlaceableOnly = true;

	const UClass* ClassForObjectBinding = FindObjectClass();
	if (ClassForObjectBinding)
	{
		Options.ViewerTitleString = FText::FromString(TEXT("Change from: ") + ClassForObjectBinding->GetFName().ToString());
	}
	else
	{
		Options.ViewerTitleString = FText::FromString(TEXT("Change from: (empty)"));
	}

	MenuBuilder.AddWidget(
		SNew(SBox)
		.MinDesiredWidth(300.0f)
		.MaxDesiredHeight(400.0f)
		[
			ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateRaw(this, &FObjectBindingModel::HandleTemplateActorClassPicked))
		],
		FText(), true, false
	);
}

void FObjectBindingModel::HandleTemplateActorClassPicked(UClass* ChosenClass)
{
	FSlateApplication::Get().DismissAllMenus();

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBindingID);
	if (!Spawnable)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ChangeClass", "Change Class"));

	MovieScene->Modify();

	TValueOrError<FNewSpawnable, FText> Result = Sequencer->GetSpawnRegister().CreateNewSpawnableType(*ChosenClass, *MovieScene, nullptr);
	if (Result.IsValid())
	{
		Spawnable->SetObjectTemplate(Result.GetValue().ObjectTemplate);

		Sequencer->GetSpawnRegister().DestroySpawnedObject(Spawnable->GetGuid(), OwnerModel->GetSequenceID(), *Sequencer.Get());
		Sequencer->ForceEvaluate();
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
		for (const TWeakPtr<FViewModel>& Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
		{
			if (FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node))
			{
				const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

				UE::MovieScene::FFixedObjectBindingID BindingID(ObjectID, SequenceID);
				for (auto It = Sequencer->GetObjectBindingTagCache()->IterateTags(BindingID); It; ++It)
				{
					AllTags.Add(It.Value());
				}
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

	for (const TWeakPtr<FViewModel>& Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
	{
		if (FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node))
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
	}

	return CheckBoxState;
}

void FObjectBindingModel::ToggleTag(FName TagName)
{
	TSharedPtr<FSequencer> Sequencer = OwnerModel->GetSequencerImpl();
	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();

	for (TWeakPtr<FViewModel> Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
	{
		if (FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node))
		{
			const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

			UE::MovieScene::FFixedObjectBindingID BindingID(ObjectID, SequenceID);
			if (!Sequencer->GetObjectBindingTagCache()->HasTag(BindingID, TagName))
			{
				HandleAddTag(TagName);
				return;
			}
		}
	}

	HandleDeleteTag(TagName);
}

void FObjectBindingModel::HandleDeleteTag(FName TagName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("RemoveBindingTag", "Remove tag '{0}' from binding(s)"), FText::FromName(TagName)));

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();
	MovieScene->Modify();

	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
	for (TWeakPtr<FViewModel> Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
	{
		if (FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node))
		{
			const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

			MovieScene->UntagBinding(TagName, UE::MovieScene::FFixedObjectBindingID(ObjectID, SequenceID));
		}
	}
}

void FObjectBindingModel::HandleAddTag(FName TagName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("CreateBindingTag", "Add new tag {0} to binding(s)"), FText::FromName(TagName)));

	TSharedPtr<ISequencer> Sequencer = OwnerModel->GetSequencer();
	UMovieScene* MovieScene = Sequencer->GetRootMovieSceneSequence()->GetMovieScene();
	MovieScene->Modify();

	FMovieSceneSequenceID SequenceID = OwnerModel->GetSequenceID();
	for (TWeakPtr<FViewModel> Node : Sequencer->GetSelection().GetSelectedOutlinerItems())
	{
		if (FObjectBindingModel* ObjectBindingNode = ICastable::CastWeakPtr<FObjectBindingModel>(Node))
		{
			const FGuid& ObjectID = ObjectBindingNode->GetObjectGuid();

			MovieScene->TagBinding(TagName, UE::MovieScene::FFixedObjectBindingID(ObjectID, SequenceID));
		}
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
		const FMovieSceneBinding* MovieSceneBinding = MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding)
		{
			return Binding.GetObjectGuid() == ObjectBindingID;
		});

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
	UMovieScene* MovieScene = OwnerModel->GetMovieScene();

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
}

} // namespace Sequencer
} // namespace UE

#undef LOCTEXT_NAMESPACE

