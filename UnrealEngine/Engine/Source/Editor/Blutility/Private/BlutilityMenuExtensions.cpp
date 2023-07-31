// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlutilityMenuExtensions.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetDataTagMap.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "BlueprintEditorModule.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailsViewArgs.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorUtilityBlueprint.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "GenericPlatform/GenericApplication.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Input/Reply.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"
#include "Toolkits/IToolkit.h"
#include "Types/SlateEnums.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Script.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

class IToolkitHost;

#define LOCTEXT_NAMESPACE "BlutilityMenuExtensions"

/** Dialog widget used to display function properties */
class SFunctionParamDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SFunctionParamDialog) {}

	/** Text to display on the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonText)

	/** Tooltip text for the "OK" button */
	SLATE_ARGUMENT(FText, OkButtonTooltipText)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<SWindow> InParentWindow, TSharedRef<FStructOnScope> InStructOnScope, FName HiddenPropertyName)
	{
		bOKPressed = false;

		// Initialize details view
		FDetailsViewArgs DetailsViewArgs;
		{
			DetailsViewArgs.bAllowSearch = false;
			DetailsViewArgs.bHideSelectionTip = true;
			DetailsViewArgs.bLockable = false;
			DetailsViewArgs.bSearchInitialKeyFocus = true;
			DetailsViewArgs.bUpdatesFromSelection = false;
			DetailsViewArgs.bShowOptions = false;
			DetailsViewArgs.bShowModifiedPropertiesOption = false;
			DetailsViewArgs.bShowObjectLabel = false;
			DetailsViewArgs.bForceHiddenPropertyVisibility = true;
			DetailsViewArgs.bShowScrollBar = false;
		}
	
		FStructureDetailsViewArgs StructureViewArgs;
		{
			StructureViewArgs.bShowObjects = true;
			StructureViewArgs.bShowAssets = true;
			StructureViewArgs.bShowClasses = true;
			StructureViewArgs.bShowInterfaces = true;
		}

		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedRef<IStructureDetailsView> StructureDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, InStructOnScope);

		// Hide any property that has been marked as such
		StructureDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([HiddenPropertyName](const FPropertyAndParent& InPropertyAndParent)
		{
			if (InPropertyAndParent.Property.GetFName() == HiddenPropertyName)
			{
				return false;
			}

			if (InPropertyAndParent.Property.HasAnyPropertyFlags(CPF_Parm))
			{
				return true;
			}

			for (const FProperty* Parent : InPropertyAndParent.ParentProperties)
			{
				if (Parent->HasAnyPropertyFlags(CPF_Parm))
				{
					return true;
				}
			}

			return false;
		}));

		StructureDetailsView->GetDetailsView()->ForceRefresh();

		ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					StructureDetailsView->GetWidget().ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([this, InParentWindow, InArgs]()
						{
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							bOKPressed = true;
							return FReply::Handled(); 
						})
						.ToolTipText(InArgs._OkButtonTooltipText)
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(InArgs._OkButtonText)
						]
					]
					+SHorizontalBox::Slot()
					.Padding(2.0f)
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(6, 2))
						.OnClicked_Lambda([InParentWindow]()
						{ 
							if(InParentWindow.IsValid())
							{
								InParentWindow.Pin()->RequestDestroyWindow();
							}
							return FReply::Handled(); 
						})
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text(LOCTEXT("Cancel", "Cancel"))
						]
					]
				]
			]
		];
	}

	bool bOKPressed;
};

void FBlutilityMenuExtensions::GetBlutilityClasses(TArray<FAssetData>& OutAssets, FTopLevelAssetPath InClassName)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Get class names
	TArray<FTopLevelAssetPath> BaseNames;
	BaseNames.Add(InClassName);
	TSet<FTopLevelAssetPath> Excluded;
	TSet<FTopLevelAssetPath> DerivedNames;
	AssetRegistry.GetDerivedClassNames(BaseNames, Excluded, DerivedNames);

	// Now get all UEditorUtilityBlueprint assets
	FARFilter Filter;
	Filter.ClassPaths.Add(UEditorUtilityBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	// Check each asset to see if it matches our type
	for (const FAssetData& Asset : AssetList)
	{
		FAssetDataTagMapSharedView::FFindTagResult Result = Asset.TagsAndValues.FindTag(FBlueprintTags::GeneratedClassPath);
		if (Result.IsSet())
		{
			const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(Result.GetValue()));

			if (DerivedNames.Contains(ClassObjectPath))
			{
				OutAssets.Add(Asset);
			}
		}
	}
}
void FBlutilityMenuExtensions::CreateActorBlutilityActionsMenu(FMenuBuilder& MenuBuilder, TMap<class IEditorUtilityExtension*, TSet<int32>> Utils, const TArray<AActor*> SelectedSupportedActors)
{
	CreateBlutilityActionsMenu<AActor*>(MenuBuilder, Utils,
		LOCTEXT("ScriptedActorActions", "Scripted Actor Actions"),
		LOCTEXT("ScriptedActorActionsTooltip", "Scripted actions available for the selected actors"),
		[](const FProperty* Property) -> bool
		{
			if (const FObjectProperty* ObjectProperty = CastField<const FObjectProperty>(Property))
			{
				return ObjectProperty->PropertyClass == AActor::StaticClass();
			}

			return false;
		},
		SelectedSupportedActors,
		"Actors.ScripterActorActions"
	);
}

void FBlutilityMenuExtensions::CreateAssetBlutilityActionsMenu(FMenuBuilder& MenuBuilder, TMap<class IEditorUtilityExtension*, TSet<int32>> Utils, const TArray<FAssetData> SelectedSupportedAssets)
{
	CreateBlutilityActionsMenu<FAssetData>(MenuBuilder, Utils,
		LOCTEXT("ScriptedAssetActions", "Scripted Asset Actions"),
		LOCTEXT("ScriptedAssetActionsTooltip", "Scripted actions available for the selected assets"),
		[](const FProperty* Property) -> bool
		{
			const FFieldClass* ClassOfProperty = Property->GetClass();
			if (ClassOfProperty == FStructProperty::StaticClass())
			{
				const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);
				return StructProperty->Struct->GetName() == TEXT("AssetData");
			}

			return false;
		},
		SelectedSupportedAssets,
		"Actors.ScripterActorActions"
	);	
}

void FBlutilityMenuExtensions::OpenEditorForUtility(const FFunctionAndUtil& FunctionAndUtil)
{
	// Edit the script if we have shift held down
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Cast<UObject>(FunctionAndUtil.Util)->GetClass()->ClassGeneratedBy))
	{
		if (IAssetEditorInstance* AssetEditor = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Blueprint, true))
		{
			check(AssetEditor->GetEditorName() == TEXT("BlueprintEditor"));
			IBlueprintEditor* BlueprintEditor = static_cast<IBlueprintEditor*>(AssetEditor);
			BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
		}
		else
		{
			FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
			TSharedRef<IBlueprintEditor> BlueprintEditor = BlueprintEditorModule.CreateBlueprintEditor(EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), Blueprint, false);
			BlueprintEditor->JumpToHyperlink(FunctionAndUtil.Function, false);
		}
	}
}

void FBlutilityMenuExtensions::ExtractFunctions(TMap<class IEditorUtilityExtension*, TSet<int32>>& Utils, TMap<FString, TArray<FFunctionAndUtil>>& OutCategoryFunctions)
{
	TSet<UClass*> ProcessedClasses;
	const static FName NAME_CallInEditor(TEXT("CallInEditor"));

	// Find the exposed functions available in each class, making sure to not list shared functions from a parent class more than once
	for (TPair< IEditorUtilityExtension*, TSet<int32>> UtilitySelectionPair : Utils)
	{
		IEditorUtilityExtension* Util = UtilitySelectionPair.Key;
		UClass* Class = Cast<UObject>(Util)->GetClass();

		if (ProcessedClasses.Contains(Class))
		{
			continue;
		}

		for (UClass* ParentClass = Class; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
		{
			ProcessedClasses.Add(ParentClass);
		}

		for (TFieldIterator<UFunction> FunctionIt(Class); FunctionIt; ++FunctionIt)
		{
			if (UFunction* Func = *FunctionIt)
			{
				if (Func->HasMetaData(NAME_CallInEditor) && Func->GetReturnProperty() == nullptr)
				{
					const static FName NAME_Category(TEXT("Category"));

					const FString& FunctionCategory = Func->GetMetaData(NAME_Category);
					TArray<FFunctionAndUtil>& Functions = OutCategoryFunctions.FindOrAdd(FunctionCategory);

					Functions.AddUnique(FFunctionAndUtil(Func, Util, UtilitySelectionPair.Value));
				}
			}
		}
	}

	for (TPair<FString, TArray<FFunctionAndUtil>>& CategoryFunctionPair : OutCategoryFunctions)
	{
		// Sort the functions by name
		CategoryFunctionPair.Value.Sort([](const FFunctionAndUtil& A, const FFunctionAndUtil& B) { return A.Function->GetName() < B.Function->GetName(); });
	}
}

template<typename SelectionType>
void FBlutilityMenuExtensions::CreateBlutilityActionsMenu(FMenuBuilder& MenuBuilder, TMap<IEditorUtilityExtension*, TSet<int32>> Utils, const FText& MenuLabel, const FText& MenuToolTip, TFunction<bool(const FProperty * Property)> IsValidPropertyType, const TArray<SelectionType> Selection, const FName& IconName)
{
	TMap<FString, TArray<FFunctionAndUtil>> CategoryFunctions;
	ExtractFunctions(Utils, CategoryFunctions);

	auto AddFunctionEntries = [Selection, IsValidPropertyType](FMenuBuilder& SubMenuBuilder, const TArray<FFunctionAndUtil>& FunctionUtils)
	{
		for (const FFunctionAndUtil& FunctionAndUtil : FunctionUtils)
		{
			const FText TooltipText = FText::Format(LOCTEXT("AssetUtilTooltipFormat", "{0}\n(Shift-click to edit script)"), FunctionAndUtil.Function->GetToolTipText());

			SubMenuBuilder.AddMenuEntry(
				FunctionAndUtil.Function->GetDisplayNameText(),
				TooltipText,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x"),
				FExecuteAction::CreateLambda([FunctionAndUtil, Selection, IsValidPropertyType]
				{
					if (FSlateApplication::Get().GetModifierKeys().IsShiftDown())
					{
						OpenEditorForUtility(FunctionAndUtil);
					}
					else
					{
						// We dont run this on the CDO, as bad things could occur!
						UObject* TempObject = NewObject<UObject>(GetTransientPackage(), Cast<UObject>(FunctionAndUtil.Util)->GetClass());
						TempObject->AddToRoot(); // Some Blutility actions might run GC so the TempObject needs to be rooted to avoid getting destroyed

						if (FunctionAndUtil.Function->NumParms > 0)
						{
							// Create a parameter struct and fill in defaults
							TSharedRef<FStructOnScope> FuncParams = MakeShared<FStructOnScope>(FunctionAndUtil.Function);

							FProperty* FirstParamProperty = nullptr;

							int32 ParameterIndex = 0;
							for (TFieldIterator<FProperty> It(FunctionAndUtil.Function); It&& It->HasAnyPropertyFlags(CPF_Parm); ++It)
							{
								FString Defaults;
								if (UEdGraphSchema_K2::FindFunctionParameterDefaultValue(FunctionAndUtil.Function, *It, Defaults))
								{
									It->ImportText_Direct(*Defaults, It->ContainerPtrToValuePtr<uint8>(FuncParams->GetStructMemory()), nullptr, PPF_None);
								}

								// Check to see if the first parameter matches the selection object type, in that case we can directly forward the selection to it
								if (ParameterIndex == 0 && IsValidPropertyType(*It))
								{
									FirstParamProperty = *It;
								}

								++ParameterIndex;
							}

							bool bApply = true;

							if (!FirstParamProperty || ParameterIndex > 1)
							{
								// pop up a dialog to input params to the function
								TSharedRef<SWindow> Window = SNew(SWindow)
									.Title(FunctionAndUtil.Function->GetDisplayNameText())
									.ClientSize(FVector2D(400, 200))
									.SupportsMinimize(false)
									.SupportsMaximize(false);

								TSharedPtr<SFunctionParamDialog> Dialog;
								Window->SetContent(
									SAssignNew(Dialog, SFunctionParamDialog, Window, FuncParams, FirstParamProperty ? FirstParamProperty->GetFName() : NAME_None)
									.OkButtonText(LOCTEXT("OKButton", "OK"))
									.OkButtonTooltipText(FunctionAndUtil.Function->GetToolTipText()));

								GEditor->EditorAddModalWindow(Window);
								bApply = Dialog->bOKPressed;
							}


							if (bApply)
							{
								FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
								FEditorScriptExecutionGuard ScriptGuard;
								const bool bForwardUserSelection = FirstParamProperty != nullptr;
								if (bForwardUserSelection)
								{
									// For each user-select asset forward the selection object into the function first's parameter (if it matches)
									const FString Path = FirstParamProperty->GetPathName(FunctionAndUtil.Function);

									// Ensure we only process selection objects that are valid for this function/utility
									for (const int32& SelectionIndex : FunctionAndUtil.SelectionIndices)
									{
										const auto SelectedAsset = Selection[SelectionIndex];
										FirstParamProperty->CopySingleValue(FirstParamProperty->ContainerPtrToValuePtr<uint8>(FuncParams->GetStructMemory()), &SelectedAsset);
										TempObject->ProcessEvent(FunctionAndUtil.Function, FuncParams->GetStructMemory());
									}
								}
								else
								{
									// User is expected to manage the asset selection on its own
									TempObject->ProcessEvent(FunctionAndUtil.Function, FuncParams->GetStructMemory());
								}
							}
						}
						else
						{
							FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "BlutilityAction", "Blutility Action"));
							FEditorScriptExecutionGuard ScriptGuard;
							TempObject->ProcessEvent(FunctionAndUtil.Function, nullptr);
						}

						TempObject->RemoveFromRoot();
					}
				})
			);
		}
	};

	// Add a menu item for each function
	if (CategoryFunctions.Num() > 0)
	{
		MenuBuilder.AddSubMenu(
			MenuLabel,
			MenuToolTip,
			FNewMenuDelegate::CreateLambda([CategoryFunctions, AddFunctionEntries](FMenuBuilder& InMenuBuilder)
			{
				TArray<FString> CategoryNames;
				CategoryFunctions.GenerateKeyArray(CategoryNames);
				CategoryNames.Remove(FString());
				CategoryNames.Sort();
				
				// Add functions belong to the same category to a sub-menu
				for (const FString& CategoryName : CategoryNames)
				{
					const TArray<FFunctionAndUtil>& FunctionUtils = CategoryFunctions.FindChecked(CategoryName);
					InMenuBuilder.AddSubMenu(FText::FromString(CategoryName), FText::FromString(CategoryName),
						FNewMenuDelegate::CreateLambda([FunctionUtils, AddFunctionEntries](FMenuBuilder& InSubMenuBuilder) 
						{
							AddFunctionEntries(InSubMenuBuilder, FunctionUtils);
						})
					);
				}

				// Non-categorized functions
				const TArray<FFunctionAndUtil>* DefaultCategoryFunctionsPtr = CategoryFunctions.Find(FString());
				if (DefaultCategoryFunctionsPtr)
				{
					AddFunctionEntries(InMenuBuilder, *DefaultCategoryFunctionsPtr);
				}
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), IconName)
		);
	}
}


UEditorUtilityExtension::UEditorUtilityExtension(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#undef LOCTEXT_NAMESPACE 