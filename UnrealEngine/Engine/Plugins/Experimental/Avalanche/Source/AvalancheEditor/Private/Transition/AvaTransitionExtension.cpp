// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionExtension.h"
#include "AvaSequenceDefaultTags.h"
#include "AvaTagCollection.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeEditorData.h"
#include "AvaWorldSubsystemUtils.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Conditions/AvaTransitionSceneMatchCondition.h"
#include "Conditions/AvaTransitionTypeMatchCondition.h"
#include "Editor.h"
#include "IAvaTransitionEditorModule.h"
#include "IRemoteControlUIModule.h"
#include "PropertyHandle.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Tasks/AvaTransitionWaitForLayerTask.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Transition/AvaTransitionPlaySequenceTask.h"

#define LOCTEXT_NAMESPACE "AvaTransitionExtension"

namespace UE::AvaEditor::Private
{
	void AddSceneTypeCondition(UStateTreeState& InState, EAvaTransitionComparisonResult InComparisonType)
	{
		TStateTreeEditorNode<FAvaTransitionSceneMatchCondition>& ConditionNode = InState.AddEnterCondition<FAvaTransitionSceneMatchCondition>();

		FAvaTransitionSceneMatchCondition& Condition = ConditionNode.GetNode();

		Condition.SceneComparisonType  = InComparisonType;
		ConditionNode.ConditionOperand = EStateTreeConditionOperand::Or;
	}

	void AddTransitionCondition(UStateTreeState& InState, EAvaTransitionType InTransitionType)
	{
		FAvaTransitionTypeMatchCondition& Condition = InState.AddEnterCondition<FAvaTransitionTypeMatchCondition>().GetNode();
		Condition.TransitionType = InTransitionType;
	}

	void SetStateCompletedResult(UStateTreeState& InState, EStateTreeTransitionType InStateTransitionType)
	{
		InState.Transitions.Empty(1);
		InState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, InStateTransitionType);
	}

	FAvaSequencePlayParams& AddPlayTask(UStateTreeState& InState, const FAvaTagHandle& InSequenceTag)
	{
		FAvaTransitionPlaySequenceTask& PlayTask = InState.AddTask<FAvaTransitionPlaySequenceTask>().GetNode();

		PlayTask.QueryType   = EAvaTransitionSequenceQueryType::Tag;
		PlayTask.SequenceTag = InSequenceTag;
		PlayTask.WaitType    = EAvaTransitionSequenceWaitType::WaitUntilStop;

		return PlayTask.PlaySettings;
	}
}

FDelegateHandle FAvaTransitionExtension::PropertyFilterHandle;

FAvaTransitionRundownExtension FAvaTransitionExtension::RundownExtension;

void FAvaTransitionExtension::StaticStartup()
{
	RundownExtension.Startup();

	IRemoteControlUIModule& RemoteControlUIModule = IRemoteControlUIModule::Get();

	PropertyFilterHandle = RemoteControlUIModule.AddPropertyFilter(FOnDisplayExposeIcon::CreateLambda(
		[](const FRCExposesPropertyArgs& InPropertyArgs)->bool
		{
			if (InPropertyArgs.PropertyHandle.IsValid())
			{
				if (const UClass* BaseClass = InPropertyArgs.PropertyHandle->GetOuterBaseClass())
				{
					// filter out any property coming from State Tree
					return !BaseClass->IsChildOf<UStateTreeEditorData>()
						&& !BaseClass->IsChildOf<UStateTreeState>();
				}
			}
			return true;
		}));
}

void FAvaTransitionExtension::StaticShutdown()
{
	RundownExtension.Shutdown();

	if (IRemoteControlUIModule* RemoteControlUIModule = FModuleManager::Get().GetModulePtr<IRemoteControlUIModule>(TEXT("RemoteControlUI")))
	{
		RemoteControlUIModule->RemovePropertyFilter(PropertyFilterHandle);
		PropertyFilterHandle.Reset();
	}
}

void FAvaTransitionExtension::Construct(const TSharedRef<IAvaEditor>& InEditor)
{
	FAvaEditorExtension::Construct(InEditor);

	IAvaTransitionEditorModule::Get().GetOnBuildDefaultTransitionTree()
		.BindSP(this, &FAvaTransitionExtension::BuildDefaultTransitionTree);
}

void FAvaTransitionExtension::Activate()
{
	if (UWorld* const World = GetWorld())
	{
		if (UAvaTransitionSubsystem* TransitionSubsystem = World->GetSubsystem<UAvaTransitionSubsystem>())
		{
			// Ensure that the Transition Behavior is created for the Persistent Level
			TransitionSubsystem->GetOrCreateTransitionBehavior();
		}
	}
}

void FAvaTransitionExtension::Deactivate()
{
	CloseTransitionEditor();
}

void FAvaTransitionExtension::ExtendToolbarMenu(UToolMenu& InMenu)
{
	FToolMenuSection& Section = InMenu.FindOrAddSection(DefaultSectionName);

	FSlateIcon TransitionTreeIcon = FSlateIconFinder::FindCustomIconForClass(UAvaTransitionTree::StaticClass(), TEXT("ClassThumbnail"));

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(TEXT("TransitionLogicButton")
		, FExecuteAction::CreateSP(this, &FAvaTransitionExtension::OpenTransitionEditor)
		, LOCTEXT("TransitionLogicLabel", "Transition Logic")
		, LOCTEXT("TransitionLogicTooltip", "Opens the Transition Logic Editor for the given Scene")
		, TransitionTreeIcon));

	Entry.StyleNameOverride = TEXT("CalloutToolbar");

	Section.AddEntry(FToolMenuEntry::InitComboButton(TEXT("TransitionLogicComboButton")
		, FUIAction()
		, FNewToolMenuDelegate::CreateSP(this, &FAvaTransitionExtension::GenerateTransitionLogicOptions)
		, LOCTEXT("TransitionLogicOptionsLabel", "Transition Logic Options")
		, LOCTEXT("TransitionLogicOptionsTooltip", "Transition Logic Options")
		, TransitionTreeIcon
		, /*bSimpleComboBox*/true));
}

IAvaTransitionBehavior* FAvaTransitionExtension::GetTransitionBehavior() const
{
	if (UAvaTransitionSubsystem* TransitionSubsystem = FAvaWorldSubsystemUtils::GetWorldSubsystem<UAvaTransitionSubsystem>(this))
	{
		return TransitionSubsystem->GetTransitionBehavior();
	}
	return nullptr;
}

void FAvaTransitionExtension::BuildDefaultTransitionTree(UAvaTransitionTreeEditorData& InTreeEditorData)
{
	using namespace UE::AvaEditor::Private;

	UStateTreeState& RootState = InTreeEditorData.AddRootState();
	SetStateCompletedResult(RootState, EStateTreeTransitionType::Succeeded);

	const FAvaSequenceDefaultTags& DefaultTags = FAvaSequenceDefaultTags::Get();

	// Same scene transition
	{
		UStateTreeState& State = RootState.AddChildState(TEXT("Same scene transition"));
		AddSceneTypeCondition(State, EAvaTransitionComparisonResult::Same);

		const FAvaTagHandle ChangeTag   = DefaultTags.Change.MakeTagHandle();
		const FAvaSequenceTime MarkTime = FAvaSequenceTime(TEXT("A"));

		// Change out (0 to A)
		{
			UStateTreeState& ChangeOut = State.AddChildState(TEXT("Change Out"));
			AddTransitionCondition(ChangeOut , EAvaTransitionType::Out);
			SetStateCompletedResult(ChangeOut, EStateTreeTransitionType::Succeeded);

			FAvaSequencePlayParams& PlaySettings = AddPlayTask(ChangeOut, ChangeTag);
			PlaySettings.Start.bHasTimeConstraint = false;
            PlaySettings.End = MarkTime;
		}

		// Change in (A to End)
		{
			UStateTreeState& ChangeIn = State.AddChildState(TEXT("Change In"));
			AddTransitionCondition(ChangeIn , EAvaTransitionType::In);

			UStateTreeState& WaitState = ChangeIn.AddChildState(TEXT("Wait for change out"));
			SetStateCompletedResult(WaitState, EStateTreeTransitionType::NextSelectableState);

			FAvaTransitionWaitForLayerTask& WaitTask = WaitState.AddTask<FAvaTransitionWaitForLayerTask>().GetNode();
			WaitTask.LayerType = EAvaTransitionLayerCompareType::Same;

			UStateTreeState& PlayChangeInState = ChangeIn.AddChildState(TEXT("Play change in"));
			SetStateCompletedResult(PlayChangeInState, EStateTreeTransitionType::Succeeded);

			FAvaSequencePlayParams& PlaySettings = AddPlayTask(PlayChangeInState, ChangeTag);
			PlaySettings.Start = MarkTime;
			PlaySettings.End.bHasTimeConstraint = false;
		}
	}

	// Different or no scene transition
	{
		UStateTreeState& State = RootState.AddChildState(TEXT("Different or no scene transition"));
		AddSceneTypeCondition(State, EAvaTransitionComparisonResult::Different);
		AddSceneTypeCondition(State, EAvaTransitionComparisonResult::None);

		// Out
		UStateTreeState& Out = State.AddChildState(TEXT("Out"));
		AddTransitionCondition(Out , EAvaTransitionType::Out);
		SetStateCompletedResult(Out, EStateTreeTransitionType::Succeeded);
		AddPlayTask(Out, DefaultTags.Out.MakeTagHandle());

		// In
		UStateTreeState& In = State.AddChildState(TEXT("In"));
		AddTransitionCondition(In , EAvaTransitionType::In);
		SetStateCompletedResult(In, EStateTreeTransitionType::Succeeded);
		AddPlayTask(In, DefaultTags.In.MakeTagHandle());
	}
}

void FAvaTransitionExtension::OpenTransitionEditor()
{
	if (const IAvaTransitionBehavior* TransitionBehavior = GetTransitionBehavior())
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		check(AssetEditorSubsystem);
		AssetEditorSubsystem->OpenEditorForAsset(TransitionBehavior->GetTransitionTree());
	}
}

void FAvaTransitionExtension::CloseTransitionEditor()
{
	if (const IAvaTransitionBehavior* TransitionBehavior = GetTransitionBehavior())
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(TransitionBehavior->GetTransitionTree());
		}
	}
}

void FAvaTransitionExtension::GenerateTransitionLogicOptions(UToolMenu* InMenu)
{
	if (IAvaTransitionEditorModule::IsLoaded())
	{
		IAvaTransitionEditorModule::Get().GenerateTransitionTreeOptionsMenu(InMenu, GetTransitionBehavior());
	}
}

#undef LOCTEXT_NAMESPACE
