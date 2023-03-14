// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "EditorModeRegistry.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IMovieRendererInterface.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "SequencerCommands.h"
#include "ISequencerObjectChangeListener.h"
#include "Sequencer.h"
#include "SequencerCustomizationManager.h"
#include "SequencerEdMode.h"
#include "SequencerObjectChangeListener.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailTreeNode.h"
#include "IDetailsView.h"
#include "Tree/CurveEditorTreeFilter.h"
#include "AnimatedPropertyKey.h"
#include "MovieSceneSignedObject.h"

#include "MVVM/CurveEditorExtension.h"
#include "MVVM/CurveEditorIntegrationExtension.h"
#include "MVVM/FolderModelStorageExtension.h"
#include "MVVM/ObjectBindingModelStorageExtension.h"
#include "MVVM/SectionModelStorageExtension.h"
#include "MVVM/TrackModelStorageExtension.h"
#include "MVVM/TrackRowModelStorageExtension.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "SequencerUtilities.h"
#include "FileHelpers.h"
#include "LevelSequence.h"


#include "Misc/CoreDelegates.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/TransBuffer.h"

#if !IS_MONOLITHIC
	UE::MovieScene::FEntityManager*& GEntityManagerForDebugging = UE::MovieScene::GEntityManagerForDebuggingVisualizers;
#endif



#define LOCTEXT_NAMESPACE "SequencerEditor"


namespace UE
{
namespace Sequencer
{

struct FDeferredSignedObjectChangeHandler : UE::MovieScene::IDeferredSignedObjectChangeHandler, FGCObject
{
	FDeferredSignedObjectChangeHandler()
	{
		Init();
	}

	~FDeferredSignedObjectChangeHandler()
	{
		if (UTransBuffer* TransBuffer = WeakBuffer.Get())
		{
			TransBuffer->OnTransactionStateChanged().RemoveAll(this);
		}
	}

	void Init()
	{
		UTransBuffer* TransBuffer = GUnrealEd ? Cast<UTransBuffer>(GUnrealEd->Trans) : nullptr;
		if (TransBuffer)
		{
			WeakBuffer = TransBuffer;
			TransBuffer->OnTransactionStateChanged().AddRaw(this, &FDeferredSignedObjectChangeHandler::OnTransactionStateChanged);
			if (TransBuffer->IsActive())
			{
				DeferTransactionChanges.Emplace();
			}
		}
		else
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([this]{ this->Init(); });
		}
	}

	void OnTransactionStateChanged(const FTransactionContext& TransactionContext, ETransactionStateEventType TransactionState)
	{
		/** A transaction has been started. This will be followed by a TransactionCanceled or TransactionFinalized event. */
		switch (TransactionState)
		{
		case ETransactionStateEventType::TransactionStarted:
		case ETransactionStateEventType::UndoRedoStarted:
			DeferTransactionChanges.Emplace();
			break;

		case ETransactionStateEventType::TransactionCanceled:
		case ETransactionStateEventType::PreTransactionFinalized:
		case ETransactionStateEventType::UndoRedoFinalized:
			DeferTransactionChanges.Reset();
			break;
		}
	}

	void Flush() override
	{
		for (TWeakObjectPtr<UMovieSceneSignedObject> WeakObject : SignedObjects)
		{
			if (UMovieSceneSignedObject* Object = WeakObject.Get())
			{
				Object->BroadcastChanged();
			}
		}
		SignedObjects.Empty();
	}

	void DeferMarkAsChanged(UMovieSceneSignedObject* SignedObject) override
	{
		SignedObjects.Add(SignedObject);
	}

	void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		for (TWeakObjectPtr<UMovieSceneSignedObject> WeakObject : SignedObjects)
		{
			if (UMovieSceneSignedObject* Object = WeakObject.Get())
			{
				Collector.AddReferencedObject(Object);
			}
		}
	}

	bool CreateImplicitScopedModifyDefer() override
	{
		ensure(!DeferImplicitChanges.IsSet());
		DeferImplicitChanges.Emplace();
		return true;
	}

	void ResetImplicitScopedModifyDefer() override
	{
		DeferImplicitChanges.Reset();
	}

	FString GetReferencerName() const override
	{
		return TEXT("FDeferredSignedObjectChangeHandler");
	}

	TSet<TWeakObjectPtr<UMovieSceneSignedObject>> SignedObjects;
	TWeakObjectPtr<UTransBuffer>   WeakBuffer;
	TOptional<UE::MovieScene::FScopedSignedObjectModifyDefer> DeferTransactionChanges;
	TOptional<UE::MovieScene::FScopedSignedObjectModifyDefer> DeferImplicitChanges;
};

} // namespace Sequencer
} // namespace UE


// Destructor defined in CPP to avoid having to #include SequencerChannelInterface.h in the main module definition
ISequencerModule::~ISequencerModule()
{
}

ECurveEditorTreeFilterType ISequencerModule::GetSequencerSelectionFilterType()
{
	static ECurveEditorTreeFilterType FilterType = FCurveEditorTreeFilter::RegisterFilterType();
	return FilterType;
}

static TSharedPtr<IDetailKeyframeHandler> GetKeyframeHandler(TWeakPtr<IDetailTreeNode> OwnerTreeNode)
{
	TSharedPtr<IDetailTreeNode> OwnerTreeNodePtr = OwnerTreeNode.Pin();
	if (!OwnerTreeNodePtr.IsValid())
	{
		return TSharedPtr<IDetailKeyframeHandler>();
	}

	IDetailsView* DetailsView = OwnerTreeNodePtr->GetNodeDetailsView();
	if (DetailsView == nullptr)
	{
		return TSharedPtr<IDetailKeyframeHandler>();
	}

	return DetailsView->GetKeyframeHandler();
}

static bool IsKeyframeButtonVisible(TWeakPtr<IDetailTreeNode> OwnerTreeNode, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = GetKeyframeHandler(OwnerTreeNode);
	if (!KeyframeHandler.IsValid() || !PropertyHandle.IsValid())
	{
		return false;
	}

	const UClass* ObjectClass = PropertyHandle->GetOuterBaseClass();
	if (ObjectClass == nullptr)
	{
		return false;
	}

	return KeyframeHandler->IsPropertyKeyable(ObjectClass, *PropertyHandle);
}

static bool IsKeyframeButtonEnabled(TWeakPtr<IDetailTreeNode> OwnerTreeNode)
{
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = GetKeyframeHandler(OwnerTreeNode);
	if (!KeyframeHandler.IsValid())
	{
		return false;
	}

	return KeyframeHandler->IsPropertyKeyingEnabled();
}

static void OnAddKeyframeClicked(TWeakPtr<IDetailTreeNode> OwnerTreeNode, TSharedPtr<IPropertyHandle> PropertyHandle)
{
	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = GetKeyframeHandler(OwnerTreeNode);
	if (!KeyframeHandler.IsValid() || !PropertyHandle.IsValid())
	{
		return;
	}

	KeyframeHandler->OnKeyPropertyClicked(*PropertyHandle);
}

static void RegisterKeyframeExtensionHandler(const FOnGenerateGlobalRowExtensionArgs& Args, TArray<FPropertyRowExtensionButton>& OutExtensionButtons)
{
	// local copy for capturing in handlers below
	TSharedPtr<IPropertyHandle> PropertyHandle = Args.PropertyHandle;
	if (!PropertyHandle.IsValid())
	{
		return;
	}

	static FSlateIcon CreateKeyIcon(FAppStyle::Get().GetStyleSetName(), "Sequencer.AddKey.Details");

	TWeakPtr<IDetailTreeNode> OwnerTreeNode = Args.OwnerTreeNode;

	FPropertyRowExtensionButton& CreateKey = OutExtensionButtons.AddDefaulted_GetRef();
	CreateKey.Icon = CreateKeyIcon;
	CreateKey.Label = NSLOCTEXT("PropertyEditor", "CreateKey", "Create Key");
	CreateKey.ToolTip = NSLOCTEXT("PropertyEditor", "CreateKeyToolTip", "Add a keyframe for this property.");
	CreateKey.UIAction = FUIAction(
		FExecuteAction::CreateStatic(&OnAddKeyframeClicked, OwnerTreeNode, PropertyHandle),
		FCanExecuteAction::CreateStatic(&IsKeyframeButtonEnabled, OwnerTreeNode),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateStatic(&IsKeyframeButtonVisible, OwnerTreeNode, PropertyHandle)
	);
}

/**
 * SequencerModule implementation (private)
 */
class FSequencerModule
	: public ISequencerModule
{
public:

	// ISequencerModule interface

	virtual TSharedRef<ISequencer> CreateSequencer(const FSequencerInitParams& InitParams) override
	{
		TSharedRef<FSequencer> Sequencer = MakeShared<FSequencer>();
		TSharedRef<ISequencerObjectChangeListener> ObjectChangeListener = MakeShared<FSequencerObjectChangeListener>(Sequencer);

		OnPreSequencerInit.Broadcast(Sequencer, ObjectChangeListener, InitParams);

		Sequencer->InitSequencer(InitParams, ObjectChangeListener, TrackEditorDelegates, EditorObjectBindingDelegates);

		OnSequencerCreated.Broadcast(Sequencer);

		return Sequencer;
	}
	
	virtual FDelegateHandle RegisterTrackEditor( FOnCreateTrackEditor InOnCreateTrackEditor, TArrayView<FAnimatedPropertyKey> AnimatedPropertyTypes ) override
	{
		TrackEditorDelegates.Add( InOnCreateTrackEditor );
		FDelegateHandle Handle = TrackEditorDelegates.Last().GetHandle();
		for (const FAnimatedPropertyKey& Key : AnimatedPropertyTypes)
		{
			PropertyAnimators.Add(Key);
		}

		if (AnimatedPropertyTypes.Num() > 0)
		{
			FAnimatedTypeCache CachedTypes;
			CachedTypes.FactoryHandle = Handle;
			for (const FAnimatedPropertyKey& Key : AnimatedPropertyTypes)
			{
				CachedTypes.AnimatedTypes.Add(Key);
			}
			AnimatedTypeCache.Add(CachedTypes);
		}
		return Handle;
	}

	virtual void UnRegisterTrackEditor( FDelegateHandle InHandle ) override
	{
		TrackEditorDelegates.RemoveAll( [=](const FOnCreateTrackEditor& Delegate){ return Delegate.GetHandle() == InHandle; } );
		int32 CacheIndex = AnimatedTypeCache.IndexOfByPredicate([=](const FAnimatedTypeCache& In) { return In.FactoryHandle == InHandle; });
		if (CacheIndex != INDEX_NONE)
		{
			for (const FAnimatedPropertyKey& Key : AnimatedTypeCache[CacheIndex].AnimatedTypes)
			{
				PropertyAnimators.Remove(Key);
			}
			AnimatedTypeCache.RemoveAtSwap(CacheIndex);
		}
	}

	virtual FDelegateHandle RegisterTrackModel(FOnCreateTrackModel InCreator) override
	{
		TrackModelDelegates.Add(InCreator);
		return TrackModelDelegates.Last().GetHandle();
	}

	virtual void UnregisterTrackModel(FDelegateHandle InHandle) override
	{
		TrackModelDelegates.RemoveAll([=](const FOnCreateTrackModel& Delegate) { return Delegate.GetHandle() == InHandle; });
	}

	virtual FDelegateHandle RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate InOnSequencerCreated) override
	{
		return OnSequencerCreated.Add(InOnSequencerCreated);
	}

	virtual void UnregisterOnSequencerCreated(FDelegateHandle InHandle) override
	{
		OnSequencerCreated.Remove(InHandle);
	}

	virtual FDelegateHandle RegisterOnPreSequencerInit(FOnPreSequencerInit::FDelegate InOnPreSequencerInit) override
	{
		return OnPreSequencerInit.Add(InOnPreSequencerInit);
	}

	virtual void UnregisterOnPreSequencerInit(FDelegateHandle InHandle) override
	{
		OnPreSequencerInit.Remove(InHandle);
	}

	virtual FDelegateHandle RegisterEditorObjectBinding(FOnCreateEditorObjectBinding InOnCreateEditorObjectBinding) override
	{
		EditorObjectBindingDelegates.Add(InOnCreateEditorObjectBinding);
		return EditorObjectBindingDelegates.Last().GetHandle();
	}

	virtual void UnRegisterEditorObjectBinding(FDelegateHandle InHandle) override
	{
		EditorObjectBindingDelegates.RemoveAll([=](const FOnCreateEditorObjectBinding& Delegate) { return Delegate.GetHandle() == InHandle; });
	}

	void RegisterMenus()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.LevelSequence");
		if (!Menu)
		{
			return;
		}

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("SequencerActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Context)
			{
				return;
			}

			if (Context->SelectedAssets.Num() == 1 && Context->SelectedAssets[0].IsInstanceOf(ULevelSequence::StaticClass()))
			{
				if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Context->SelectedAssets[0].GetAsset()))
				{
					// if this LevelSequence has associated maps, offer to load them
					TArray<FString> AssociatedMaps = FSequencerUtilities::GetAssociatedMapPackages(LevelSequence);

					if(AssociatedMaps.Num()>0)
					{
						InSection.AddSubMenu(
							"SequencerOpenMap_Label",
							LOCTEXT("SequencerOpenMap_Label", "Open Map"),
							LOCTEXT("SequencerOpenMap_Tooltip", "Open a map associated with this Level Sequence Asset"),
							FNewMenuDelegate::CreateLambda(
								[AssociatedMaps](FMenuBuilder& SubMenuBuilder)
								{
									for (const FString& AssociatedMap : AssociatedMaps)
									{
										SubMenuBuilder.AddMenuEntry(
											FText::FromString(FPaths::GetBaseFilename(AssociatedMap)),
											FText(),
											FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Levels"),
											FExecuteAction::CreateLambda(
												[AssociatedMap]
												{
													FEditorFileUtils::LoadMap(AssociatedMap);
												}
											)
										);
									}
								}
							),
							false,
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Levels")
						);
					}
				}
			}
		}));
	}

	virtual void StartupModule() override
	{
		using namespace UE::Sequencer;
		using namespace UE::MovieScene;

		if (GIsEditor)
		{
			FEditorModeRegistry::Get().RegisterMode<FSequencerEdMode>(
				FSequencerEdMode::EM_SequencerMode,
				NSLOCTEXT("Sequencer", "SequencerEditMode", "Sequencer Mode"),
				FSlateIcon(),
				false);

			if (UToolMenus::TryGet())
			{
				FSequencerCommands::Register();
				RegisterMenus();
			}
			else
			{
				FCoreDelegates::OnPostEngineInit.AddStatic(&FSequencerCommands::Register);
				FCoreDelegates::OnPostEngineInit.AddRaw(this, &FSequencerModule::RegisterMenus);
			}

			// Set the deferred handler for use by Sequence editors. It is not necessary in commandlets.
			// TODO: Create/Delete the handler when Sequence editors are opened/closed.
			if (!IsRunningCommandlet())
			{
				UMovieSceneSignedObject::SetDeferredHandler(MakeUnique<FDeferredSignedObjectChangeHandler>());
			}

			FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			OnGetGlobalRowExtensionHandle = EditModule.GetGlobalRowExtensionDelegate().AddStatic(&RegisterKeyframeExtensionHandler);
		}

		FSequenceModel::CreateExtensionsEvent.AddLambda(
			[&](TSharedPtr<FEditorViewModel> InEditor, TSharedPtr<FSequenceModel> InModel)
			{
				InModel->AddDynamicExtension(FFolderModelStorageExtension::ID);
				InModel->AddDynamicExtension(FObjectBindingModelStorageExtension::ID);
				InModel->AddDynamicExtension(FTrackModelStorageExtension::ID, TrackModelDelegates);
				InModel->AddDynamicExtension(FTrackRowModelStorageExtension::ID);
				InModel->AddDynamicExtension(FSectionModelStorageExtension::ID);

				// If the editor supports a curve editor, add an integration extension to
				// sync view-model hierarchies between the outliner and curve editor.
				if (InEditor->CastDynamic<FCurveEditorExtension>())
				{
					InModel->AddDynamicExtension(FCurveEditorIntegrationExtension::ID);
				}
			}
		);

		ObjectBindingContextMenuExtensibilityManager = MakeShareable( new FExtensibilityManager );
		AddTrackMenuExtensibilityManager = MakeShareable( new FExtensibilityManager );
		ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
		ActionsMenuExtensibilityManager = MakeShareable(new FExtensibilityManager);

		SequencerCustomizationManager = MakeShareable(new FSequencerCustomizationManager);
	}

	virtual void ShutdownModule() override
	{
		if (GIsEditor)
		{
			UMovieSceneSignedObject::SetDeferredHandler(nullptr);

			FSequencerCommands::Unregister();

			if (FPropertyEditorModule* EditModulePtr = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
			{
				EditModulePtr->GetGlobalRowExtensionDelegate().Remove(OnGetGlobalRowExtensionHandle);
			}

			FEditorModeRegistry::Get().UnregisterMode(FSequencerEdMode::EM_SequencerMode);
		}
	}

	virtual void RegisterPropertyAnimator(FAnimatedPropertyKey Key) override
	{
		PropertyAnimators.Add(Key);
	}

	virtual void UnRegisterPropertyAnimator(FAnimatedPropertyKey Key) override
	{
		PropertyAnimators.Remove(Key);
	}

	virtual bool CanAnimateProperty(FProperty* Property) override
	{
		if (PropertyAnimators.Contains(FAnimatedPropertyKey::FromProperty(Property)))
		{
			return true;
		}

		FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);

		// Check each level of the property hierarchy
		FFieldClass* PropertyType = Property->GetClass();
		while (PropertyType && PropertyType != FProperty::StaticClass())
		{
			FAnimatedPropertyKey Key = FAnimatedPropertyKey::FromPropertyTypeName(PropertyType->GetFName());

			// For object properties, check each parent type of the object (ie, so a track that animates UBaseClass ptrs can be used with a UDerivedClass property)
			UClass* ClassType = (ObjectProperty && ObjectProperty->PropertyClass) ? ObjectProperty->PropertyClass->GetSuperClass() : nullptr;
			while (ClassType)
			{
				Key.ObjectTypeName = ClassType->GetFName();
				if (PropertyAnimators.Contains(Key))
				{
					return true;
				}
				ClassType = ClassType->GetSuperClass();
			}

			Key.ObjectTypeName = NAME_None;
			if (PropertyAnimators.Contains(Key))
			{
				return true;
			}

			// Look at the property's super class
			PropertyType = PropertyType->GetSuperClass();
		}

		return false;
	}

	virtual TSharedPtr<FExtensibilityManager> GetObjectBindingContextMenuExtensibilityManager() const override { return ObjectBindingContextMenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetAddTrackMenuExtensibilityManager() const override { return AddTrackMenuExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const override { return ToolBarExtensibilityManager; }
	virtual TSharedPtr<FExtensibilityManager> GetActionsMenuExtensibilityManager() const override { return ActionsMenuExtensibilityManager; }

	virtual TSharedPtr<FSequencerCustomizationManager> GetSequencerCustomizationManager() const override { return SequencerCustomizationManager; }


	virtual FDelegateHandle RegisterMovieRenderer(TUniquePtr<IMovieRendererInterface>&& InMovieRenderer) override
	{
		FDelegateHandle NewHandle(FDelegateHandle::GenerateNewHandle);
		MovieRenderers.Add(FMovieRendererEntry{ NewHandle, MoveTemp(InMovieRenderer) });
		return NewHandle;
	}

	virtual void UnregisterMovieRenderer(FDelegateHandle InDelegateHandle) override
	{
		MovieRenderers.RemoveAll([InDelegateHandle](const FMovieRendererEntry& In){ return In.Handle == InDelegateHandle; });
	}

	virtual IMovieRendererInterface* GetMovieRenderer(const FString& InMovieRendererName) override
	{
		for (const FMovieRendererEntry& MovieRenderer : MovieRenderers)
		{
			if (MovieRenderer.Renderer->GetDisplayName() == InMovieRendererName)
			{
				return MovieRenderer.Renderer.Get();
			}
		}

		return nullptr;
	}

	virtual TArray<FString> GetMovieRendererNames() override
	{
		TArray<FString> MovieRendererNames;
		for (const FMovieRendererEntry& MovieRenderer : MovieRenderers)
		{
			MovieRendererNames.Add(MovieRenderer.Renderer->GetDisplayName());
		}
		return MovieRendererNames;
	}

private:

	TSet<FAnimatedPropertyKey> PropertyAnimators;

	/** List of auto-key handler delegates sequencers will execute when they are created */
	TArray< FOnCreateTrackEditor > TrackEditorDelegates;

	/** List of object binding handler delegates sequencers will execute when they are created */
	TArray< FOnCreateEditorObjectBinding > EditorObjectBindingDelegates;

	/** List of track model creators */
	TArray<FOnCreateTrackModel> TrackModelDelegates;

	/** Global details row extension delegate; */
	FDelegateHandle OnGetGlobalRowExtensionHandle;

	/** Multicast delegate used to notify others of sequencer initialization params and allow modification. */
	FOnPreSequencerInit OnPreSequencerInit;

	/** Multicast delegate used to notify others of sequencer creations */
	FOnSequencerCreated OnSequencerCreated;

	struct FAnimatedTypeCache
	{
		FDelegateHandle FactoryHandle;
		TArray<FAnimatedPropertyKey, TInlineAllocator<4>> AnimatedTypes;
	};

	/** Map of all track editor factories to property types that they have registered to animated */
	TArray<FAnimatedTypeCache> AnimatedTypeCache;

	TSharedPtr<FExtensibilityManager> ObjectBindingContextMenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> AddTrackMenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ActionsMenuExtensibilityManager;

	TSharedPtr<FSequencerCustomizationManager> SequencerCustomizationManager;

	struct FMovieRendererEntry
	{
		FDelegateHandle Handle;
		TUniquePtr<IMovieRendererInterface> Renderer;
	};

	/** Array of movie renderers */
	TArray<FMovieRendererEntry> MovieRenderers;
};

IMPLEMENT_MODULE(FSequencerModule, Sequencer);

#undef LOCTEXT_NAMESPACE
