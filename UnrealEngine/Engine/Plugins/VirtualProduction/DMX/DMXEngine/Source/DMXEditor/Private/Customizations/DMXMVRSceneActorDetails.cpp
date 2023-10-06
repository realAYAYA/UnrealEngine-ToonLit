// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/DMXMVRSceneActorDetails.h"

#include "MVR/DMXMVRSceneActor.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "ISceneOutliner.h"
#include "LevelEditor.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"


#define LOCTEXT_NAMESPACE "DMXMVRSceneActorDetails"

TSharedRef<IDetailCustomization> FDMXMVRSceneActorDetails::MakeInstance()
{
	return MakeShared<FDMXMVRSceneActorDetails>();
}

void FDMXMVRSceneActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	for (TWeakObjectPtr<UObject> Object : ObjectsBeingCustomized)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = Cast<ADMXMVRSceneActor>(Object))
		{
			OuterSceneActors.Add(MVRSceneActor);
		}
	}

	CreateRefreshMVRSceneSection(DetailBuilder);
	CreateGDTFToActorClassSection(DetailBuilder);

	// Listen to map and actor changes
	FEditorDelegates::MapChange.AddSP(this, &FDMXMVRSceneActorDetails::OnMapChange);

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().AddSP(this, &FDMXMVRSceneActorDetails::OnActorDeleted);
	}
}

void FDMXMVRSceneActorDetails::CreateRefreshMVRSceneSection(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& MVRCategory = DetailBuilder.EditCategory("MVR");

	MVRCategory.AddProperty(DetailBuilder.GetProperty(ADMXMVRSceneActor::GetDMXLibraryPropertyNameChecked()));
	MVRCategory.AddCustomRow(LOCTEXT("RefreshSceneFilterText", "Refresh from DMX Library"))
		.WholeRowContent()
		[			
			SNew(SBorder)
			.Padding(8.f, 0.f, 0.f, 0.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshActorsFromDMXLibraryButtonCaption", "Update Actors from DMX Library"))
				.OnClicked(this, &FDMXMVRSceneActorDetails::OnRefreshActorsFromDMXLibraryClicked)
			]
		];

	const TSharedRef<IPropertyHandle> RespawnDeletedActorHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, bRespawnDeletedActorsOnRefresh));
	RespawnDeletedActorHandle->MarkHiddenByCustomization();
	MVRCategory.AddCustomRow(LOCTEXT("RespawnDeletedActorsFilterText", "Respawn Deleted Actors"))
		.NameContent()
		[
			SNew(SBorder)
			.Padding(8.f, 0.f, 0.f, 0.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				RespawnDeletedActorHandle->CreatePropertyNameWidget()
			]
		]
		.ValueContent()
		[
			RespawnDeletedActorHandle->CreatePropertyValueWidget()
		];
	
	const TSharedRef<IPropertyHandle> UpdateTransformHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, bUpdateTransformsOnRefresh));
	UpdateTransformHandle->MarkHiddenByCustomization();
	MVRCategory.AddCustomRow(LOCTEXT("ResetTransformsFilterText", "Reset Transforms"))
		.NameContent()
		[
			SNew(SBorder)
			.Padding(8.f, 0.f, 0.f, 0.f)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				UpdateTransformHandle->CreatePropertyNameWidget()
			]
		]
		.ValueContent()
		[
			UpdateTransformHandle->CreatePropertyValueWidget()
		];
}

void FDMXMVRSceneActorDetails::CreateGDTFToActorClassSection(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& ActorTypeForGDTFCategory = DetailBuilder.EditCategory("GDTF to Spawned Actor");
	ActorTypeForGDTFCategory.InitiallyCollapsed(false);

	const TSharedRef<IPropertyHandle> GDTFToDefaultActorClassesHandle = DetailBuilder.GetProperty(ADMXMVRSceneActor::GetGDTFToDefaultActorClassesPropertyNameChecked());
	GDTFToDefaultActorClassesHandle->MarkHiddenByCustomization();

	const TSharedPtr<IPropertyHandleArray> GDTFToDefaultActorClassHandleArray = GDTFToDefaultActorClassesHandle->AsArray();
	FSimpleDelegate GDTFToDefaultActorClassArrayChangedDelegate = FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::RequestRefresh);
	GDTFToDefaultActorClassHandleArray->SetOnNumElementsChanged(GDTFToDefaultActorClassArrayChangedDelegate);

	uint32 NumGDTFToDefaultActorClassElements;
	if (!ensure(GDTFToDefaultActorClassHandleArray->GetNumElements(NumGDTFToDefaultActorClassElements) == FPropertyAccess::Success))
	{
		return;
	}

	for (uint32 GDTFToDefaultActorClassElementIndex = 0; GDTFToDefaultActorClassElementIndex < NumGDTFToDefaultActorClassElements; GDTFToDefaultActorClassElementIndex++)
	{
		const TSharedPtr<IPropertyHandle> GDTFToDefaultActorClassHandle = GDTFToDefaultActorClassHandleArray->GetElement(GDTFToDefaultActorClassElementIndex);
		const TSharedPtr<IPropertyHandle> GDTFHandle = GDTFToDefaultActorClassHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, GDTF));
		DefaultActorClassHandle = GDTFToDefaultActorClassHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass));
		DefaultActorClassHandle->SetOnChildPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::OnPreEditChangeActorClassInGDTFToActorClasses));
		DefaultActorClassHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FDMXMVRSceneActorDetails::OnPostEditChangeActorClassInGDTFToActorClasses));

		UObject* GDTFObject;
		if (!GDTFHandle->GetValue(GDTFObject))
		{
			return;
		}
		UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(GDTFObject);
		if (!GDTF)
		{
			continue;
		}

		if (!IsAnyActorUsingGDTF(GDTF))
		{
			continue;
		}

		ActorTypeForGDTFCategory.AddCustomRow(LOCTEXT("GDTFToDefaultActorClassFilter", "GDTF"))
		.NameContent()
		[
			GDTFHandle->CreatePropertyValueWidget()
		]
		.ValueContent()
		[
			SNew(SWrapBox)

			+ SWrapBox::Slot()
			[
				DefaultActorClassHandle->CreatePropertyValueWidget()
			]

			+ SWrapBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &FDMXMVRSceneActorDetails::OnSelectGDTFToActorClassGroupClicked, GDTFObject)
				.Text(LOCTEXT("SelectGDTFGroupButtonCaption", "Select"))
			]
		];
	}
}

FReply FDMXMVRSceneActorDetails::OnRefreshActorsFromDMXLibraryClicked()
{
	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();

	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = Cast<ADMXMVRSceneActor>(SelectedObject.Get()))
		{
			const FScopedTransaction RefreshActorsFromDMXLibraryTransaction(LOCTEXT("RefreshActorsFromDMXLibraryTransaction", "Update MVR Scene form DMX Library"));
			MVRSceneActor->PreEditChange(ADMXMVRSceneActor::StaticClass()->FindPropertyByName(ADMXMVRSceneActor::GetRelatedAcctorsPropertyNameChecked()));

			MVRSceneActor->RefreshFromDMXLibrary();

			MVRSceneActor->PostEditChange();
		}
	}

	return FReply::Handled();
}

FReply FDMXMVRSceneActorDetails::OnSelectGDTFToActorClassGroupClicked(UObject* GDTFObject)
{
	UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(GDTFObject);
	if (!GDTF)
	{
		return FReply::Unhandled();
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();

	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = Cast<ADMXMVRSceneActor>(SelectedObject.Get()))
		{
			const TArray<AActor*> ActorsForThisGDTF = MVRSceneActor->GetActorsSpawnedForGDTF(GDTF);
			UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

			if (EditorActorSubsystem)
			{
				EditorActorSubsystem->SetSelectedLevelActors(ActorsForThisGDTF);
			}
		}
	}

	// Set focus on the Scene Outliner so the user can execute keyboard commands right away
	const TWeakPtr<class ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance();
	const TSharedPtr<class ISceneOutliner> SceneOutliner = LevelEditor.IsValid() ? LevelEditor.Pin()->GetMostRecentlyUsedSceneOutliner() : nullptr;
	if (SceneOutliner.IsValid())
	{
		SceneOutliner->SetKeyboardFocus();
	}

	return FReply::Handled();
}

void FDMXMVRSceneActorDetails::OnFixturePatchChanged(const UDMXEntityFixturePatch* FixturePatch)
{
	RequestRefresh();
}

void FDMXMVRSceneActorDetails::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	RequestRefresh();
}

void FDMXMVRSceneActorDetails::OnMapChange(uint32 MapChangeFlags)
{
	RequestRefresh();
}

void FDMXMVRSceneActorDetails::OnActorDeleted(AActor* DeletedActor)
{
	RequestRefresh();
}

void FDMXMVRSceneActorDetails::OnPreEditChangeActorClassInGDTFToActorClasses()
{
	for (TWeakObjectPtr<ADMXMVRSceneActor> WeakMVRSceneActor : OuterSceneActors)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = WeakMVRSceneActor.Get())
		{
			MVRSceneActor->PreEditChange(FDMXMVRSceneGDTFToActorClassPair::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FDMXMVRSceneGDTFToActorClassPair, ActorClass)));
		}
	}
}

void FDMXMVRSceneActorDetails::OnPostEditChangeActorClassInGDTFToActorClasses()
{
	for (TWeakObjectPtr<ADMXMVRSceneActor> WeakMVRSceneActor : OuterSceneActors)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = WeakMVRSceneActor.Get())
		{
			MVRSceneActor->PostEditChange();
		}
	}
}

bool FDMXMVRSceneActorDetails::IsAnyActorUsingGDTF(const UDMXImportGDTF* GDTF) const
{
	const TArray<TWeakObjectPtr<UObject>> SelectedObjects = PropertyUtilities->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (ADMXMVRSceneActor* MVRSceneActor = Cast<ADMXMVRSceneActor>(SelectedObject.Get()))
		{
			return !MVRSceneActor->GetActorsSpawnedForGDTF(GDTF).IsEmpty();
		}
	}
	return false;
}

void FDMXMVRSceneActorDetails::RequestRefresh()
{
	PropertyUtilities->RequestRefresh();
}

#undef LOCTEXT_NAMESPACE
