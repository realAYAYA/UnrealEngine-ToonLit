// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigModuleDetails.h"
#include "Widgets/SWidget.h"
#include "IDetailChildrenBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "ModularRigController.h"
#include "ControlRigBlueprint.h"
#include "ControlRigEditorStyle.h"
#include "ControlRigElementDetails.h"
#include "Graph/ControlRigGraph.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "SEnumCombo.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Styling/AppStyle.h"
#include "Editor/SModularRigTreeView.h"
#include "StructViewerFilter.h"
#include "StructViewerModule.h"
#include "Features/IModularFeatures.h"
#include "IPropertyAccessEditor.h"
#include "ModularRigRuleManager.h"
#include "ScopedTransaction.h"
#include "Editor/SRigHierarchyTreeView.h"

#define LOCTEXT_NAMESPACE "ControlRigModuleDetails"

static const FText ControlRigModuleDetailsMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

static void RigModuleDetails_GetCustomizedInfo(TSharedRef<IPropertyHandle> InStructPropertyHandle, UControlRigBlueprint*& OutBlueprint)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigBlueprint>())
		{
			OutBlueprint = CastChecked<UControlRigBlueprint>(Object);
			break;
		}

		OutBlueprint = Object->GetTypedOuter<UControlRigBlueprint>();
		if(OutBlueprint)
		{
			break;
		}

		if(const UControlRig* ControlRig = Object->GetTypedOuter<UControlRig>())
		{
			OutBlueprint = Cast<UControlRigBlueprint>(ControlRig->GetClass()->ClassGeneratedBy);
			if(OutBlueprint)
			{
				break;
			}
		}
	}

	if (OutBlueprint == nullptr)
	{
		TArray<UPackage*> Packages;
		InStructPropertyHandle->GetOuterPackages(Packages);
		for (UPackage* Package : Packages)
		{
			if (Package == nullptr)
			{
				continue;
			}

			TArray<UObject*> SubObjects;
			Package->GetDefaultSubobjects(SubObjects);
			for (UObject* SubObject : SubObjects)
			{
				if (UControlRig* Rig = Cast<UControlRig>(SubObject))
				{
					UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Rig->GetClass()->ClassGeneratedBy);
					if (Blueprint)
					{
						if(Blueprint->GetOutermost() == Package)
						{
							OutBlueprint = Blueprint;
							break;
						}
					}
				}
			}

			if (OutBlueprint)
			{
				break;
			}
		}
	}
}

static UControlRigBlueprint* RigModuleDetails_GetBlueprintFromRig(UModularRig* InRig)
{
	if(InRig == nullptr)
	{
		return nullptr;
	}

	UControlRigBlueprint* Blueprint = InRig->GetTypedOuter<UControlRigBlueprint>();
	if(Blueprint == nullptr)
	{
		Blueprint = Cast<UControlRigBlueprint>(InRig->GetClass()->ClassGeneratedBy);
	}
	return Blueprint;
}

void FRigModuleInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	PerModuleInfos.Reset();

	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailBuilder.GetObjectsBeingCustomized(DetailObjects);
	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		if(UControlRig* ModuleInstance = Cast<UControlRig>(DetailObject))
		{
			if(const UModularRig* ModularRig = Cast<UModularRig>(ModuleInstance->GetOuter()))
			{
				if(const FRigModuleInstance* Module = ModularRig->FindModule(ModuleInstance))
				{
					const FString Path = Module->GetPath();

					FPerModuleInfo Info;
					Info.Path = Path;
					Info.Module = ModularRig->GetHandle(Path);
					if(!Info.Module.IsValid())
					{
						return;
					}
					
					if(const UControlRigBlueprint* Blueprint = Info.GetBlueprint())
					{
						if(const UModularRig* DefaultModularRig = Cast<UModularRig>(Blueprint->GeneratedClass->GetDefaultObject()))
						{
							Info.DefaultModule = DefaultModularRig->GetHandle(Path);
						}
					}

					PerModuleInfos.Add(Info);
				}
			}
		}
	}

	// don't customize if the 
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	TArray<FName> OriginalCategoryNames;
	DetailBuilder.GetCategoryNames(OriginalCategoryNames);

	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory(TEXT("General"), LOCTEXT("General", "General"));
	{
		static const FText NameTooltip = LOCTEXT("NameTooltip", "The name is used to determine the long name (the full path) and to provide a unique address within the rig.");
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(NameTooltip)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetName)
			.OnTextCommitted(this, &FRigModuleInstanceDetails::SetName, DetailBuilder.GetPropertyUtilities())
			.ToolTipText(NameTooltip)
			.OnVerifyTextChanged(this, &FRigModuleInstanceDetails::OnVerifyNameChanged)
		];

		static const FText ShortNameTooltip = LOCTEXT("ShortNameTooltip", "The short name is used for the user interface, for example the sequencer channels.\nThis value can be edited and adjusted as needed.");
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Short Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Short Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(ShortNameTooltip)
			.IsEnabled(PerModuleInfos.Num() == 1)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetShortName)
			.OnTextCommitted(this, &FRigModuleInstanceDetails::SetShortName, DetailBuilder.GetPropertyUtilities())
			.ToolTipText(ShortNameTooltip)
			.IsEnabled(PerModuleInfos.Num() == 1)
			.OnVerifyTextChanged(this, &FRigModuleInstanceDetails::OnVerifyShortNameChanged)
		];

		static const FText LongNameTooltip = LOCTEXT("LongNameTooltip", "The long name represents a unique address within the rig but isn't used for the user interface.");
		GeneralCategory.AddCustomRow(FText::FromString(TEXT("Long Name")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Long Name")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("LongNameTooltip", "The long name represents a unique address within the rig but isn't used for the user interface."))
			.ToolTipText(LongNameTooltip)
			.IsEnabled(false)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetLongName)
			.ToolTipText(LongNameTooltip)
			.IsEnabled(false)
		];

		GeneralCategory.AddCustomRow(FText::FromString(TEXT("RigClass")))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("RigClass")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsEnabled(true)
		]
		.ValueContent()
		[
			SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigModuleInstanceDetails::GetRigClassPath)
			.IsEnabled(true)
		];
	}

	IDetailCategoryBuilder& ConnectionsCategory = DetailBuilder.EditCategory(TEXT("Connections"), LOCTEXT("Connections", "Connections"));
	{
		bool bDisplayConnectors = PerModuleInfos.Num() >= 1;
		if (PerModuleInfos.Num() > 1)
		{
			UModularRig* ModularRig = PerModuleInfos[0].GetModularRig();
			for (FPerModuleInfo& Info : PerModuleInfos)
			{
				if (Info.GetModularRig() != ModularRig)
				{
					bDisplayConnectors = false;
					break;
				}
			}
		}
		if (bDisplayConnectors)
		{
			TArray<FRigModuleConnector> Connectors = GetConnectors();
			for(const FRigModuleConnector& Connector : Connectors)
			{
				const FText Label = FText::FromString(Connector.Name);
				TSharedPtr<SVerticalBox> ButtonBox;

				TArray<FRigElementResolveResult> Matches;
				for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
				{
					const FPerModuleInfo& Info = PerModuleInfos[ModuleIndex];
					if (const FRigModuleInstance* Module = Info.GetModule())
					{
						if (UModularRig* ModularRig = Info.GetModularRig())
						{
							if (URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
							{
								FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *Info.Path, *Connector.Name);
								FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
								if (FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Hierarchy->Find(ConnectorKey)))
								{
									const UModularRigRuleManager* RuleManager = ModularRig->GetHierarchy()->GetRuleManager();
									if (ModuleIndex == 0)
									{
										Matches = RuleManager->FindMatches(ConnectorElement, Module, ModularRig->GetElementKeyRedirector()).GetMatches();
									}
									else
									{
										const FModularRigResolveResult& ConnectorMatches = RuleManager->FindMatches(ConnectorElement, Module, ModularRig->GetElementKeyRedirector());
										Matches.FilterByPredicate([ConnectorMatches](const FRigElementResolveResult& Match)
										{
											return ConnectorMatches.ContainsMatch(Match.GetKey());
										});
									}
								}
							}
						}
					}
				}

				FRigTreeDelegates TreeDelegates;
				TreeDelegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateLambda([this]()
				{
					return PerModuleInfos[0].GetModularRig()->GetHierarchy();
				});
				TreeDelegates.OnRigTreeIsItemVisible = FOnRigTreeIsItemVisible::CreateLambda([Matches](const FRigElementKey& InTarget)
				{
					return Matches.ContainsByPredicate([InTarget](const FRigElementResolveResult& Match)
					{
						return Match.GetKey() == InTarget;
					});
				});
				TreeDelegates.OnGetSelection.BindLambda([this, Connector]() -> TArray<FRigElementKey>
				{
					FRigElementKey Target;
					FRigElementKeyRedirector Redirector = PerModuleInfos[0].GetModularRig()->GetElementKeyRedirector();
					for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
					{
						FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *PerModuleInfos[ModuleIndex].Path, *Connector.Name);
						FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
						if (const FRigElementKey* Key = Redirector.FindExternalKey(ConnectorKey))
						{
							if (ModuleIndex == 0)
							{
								Target = *Key;
							}
							else if (Target != *Key)
							{
								Target.Name = *ControlRigModuleDetailsMultipleValues.ToString();
								return {Target};
							}
						}
						else
						{
							Target.Name = *ControlRigModuleDetailsMultipleValues.ToString();
							return {Target};
						}
					}
					return {};
				});
				TreeDelegates.OnSelectionChanged.BindSP(this, &FRigModuleInstanceDetails::OnConnectorTargetChanged, Connector);
			

				ConnectionsCategory.AddCustomRow(Label)
					.NameContent()
					[
						SNew(STextBlock)
						.Text(Label)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.IsEnabled(true)
					]
					.ValueContent()
					[
						SNew(SHorizontalBox)

						// Combo button
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.f, 0.f, 0.f, 0.f)
						[
							SNew( SComboButton )
							.ContentPadding(3)
							.MenuPlacement(MenuPlacement_BelowAnchor)
							.OnComboBoxOpened(this, &FRigModuleInstanceDetails::PopulateConnectorTargetList, Connector.Name)
							.ButtonContent()
							[
								// Wrap in configurable box to restrain height/width of menu
								SNew(SBox)
								.MinDesiredWidth(150.0f)
								[
									SAssignNew(ButtonBox, SVerticalBox)
								]
							]
							.MenuContent()
							[
								SNew(SBorder)
								.Visibility(EVisibility::Visible)
								.BorderImage(FAppStyle::GetBrush("Menu.Background"))
								[
									SAssignNew(ConnectionListBox.FindOrAdd(Connector.Name), SSearchableRigHierarchyTreeView)
										.RigTreeDelegates(TreeDelegates)
								]
							]
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.f, 0.f, 0.f, 0.f)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.AutoHeight()
							[
								// Reset button
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.f, 0.f, 0.f, 0.f)
								[
									SAssignNew(ResetConnectorButton.FindOrAdd(Connector.Name), SButton)
									.ButtonStyle( FAppStyle::Get(), "NoBorder" )
									.ButtonColorAndOpacity_Lambda([this, Connector]()
									{
										const TSharedPtr<SButton>& Button = ResetConnectorButton.FindRef(Connector.Name);
										return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
									})
									.OnClicked_Lambda([this, Connector]()
									{
										for (FPerModuleInfo& Info : PerModuleInfos)
										{
											FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *Info.Path, *Connector.Name);
											FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
											Info.GetBlueprint()->GetModularRigController()->DisconnectConnector(ConnectorKey);
										}
										return FReply::Handled();
									})
									.ContentPadding(1.f)
									.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Reset_Connector", "Reset Connector"))
									[
										SNew(SImage)
										.ColorAndOpacity_Lambda( [this, Connector]()
										{
											const TSharedPtr<SButton>& Button = ResetConnectorButton.FindRef(Connector.Name);
											return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
										})
										.Image(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault").GetIcon())
									]
								]

								// Use button
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.f, 0.f, 0.f, 0.f)
								[
									SAssignNew(UseSelectedButton.FindOrAdd(Connector.Name), SButton)
									.ButtonStyle( FAppStyle::Get(), "NoBorder" )
									.ButtonColorAndOpacity_Lambda([this, Connector]()
									{
										const TSharedPtr<SButton>& Button = UseSelectedButton.FindRef(Connector.Name);
										return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
									})
									.OnClicked_Lambda([this, Connector]()
									{
										if (UModularRig* ModularRig = PerModuleInfos[0].GetModularRig())
										{
											const TArray<FRigElementKey>& Selected = ModularRig->GetHierarchy()->GetSelectedKeys();
											if (Selected.Num() > 0)
											{
												for (FPerModuleInfo& Info : PerModuleInfos)
												{
													FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *Info.Path, *Connector.Name);
													FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
													Info.GetBlueprint()->GetModularRigController()->ConnectConnectorToElement(ConnectorKey, Selected[0], true, ModularRig->GetModularRigSettings().bAutoResolve);
												}
											}
										}
										return FReply::Handled();
									})
									.ContentPadding(1.f)
									.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Use_Selected", "Use Selected"))
									[
										SNew(SImage)
										.ColorAndOpacity_Lambda( [this, Connector]()
										{
											const TSharedPtr<SButton>& Button = UseSelectedButton.FindRef(Connector.Name);
											return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
										})
										.Image(FAppStyle::GetBrush("Icons.CircleArrowLeft"))
									]
								]

								// Select in hierarchy button
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.f, 0.f, 0.f, 0.f)
								[
									SAssignNew(SelectElementButton.FindOrAdd(Connector.Name), SButton)
									.ButtonStyle( FAppStyle::Get(), "NoBorder" )
									.ButtonColorAndOpacity_Lambda([this, Connector]()
									{
										const TSharedPtr<SButton>& Button = SelectElementButton.FindRef(Connector.Name);
										return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
									})
									.OnClicked_Lambda([this, Connector]()
									{
										if (UModularRig* ModularRig = PerModuleInfos[0].GetModularRig())
										{
											FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *PerModuleInfos[0].Path, *Connector.Name);
											FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
											if (const FRigElementKey* TargetKey = ModularRig->GetElementKeyRedirector().FindExternalKey(ConnectorKey))
											{
												ModularRig->GetHierarchy()->GetController()->SelectElement(*TargetKey, true, true);
											}
										}
										return FReply::Handled();
									})
									.ContentPadding(1.f)
									.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Select_Element", "Select Element"))
									[
										SNew(SImage)
										.ColorAndOpacity_Lambda( [this, Connector]()
										{
											const TSharedPtr<SButton>& Button = SelectElementButton.FindRef(Connector.Name);
											return Button.IsValid() && Button->IsHovered()
											? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
											: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
										})
										.Image(FAppStyle::GetBrush("Icons.Search"))
									]
								]
							]
						]
					];

				FRigElementKey CurrentTargetKey;
				if (PerModuleInfos.Num() >= 1)
				{
					for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
					{
						FRigElementKey TargetKey;
						FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *PerModuleInfos[ModuleIndex].Path, *Connector.Name);
						FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
						if (const FRigElementKey* Key = PerModuleInfos[ModuleIndex].GetModularRig()->GetElementKeyRedirector().FindExternalKey(ConnectorKey))
						{
							TargetKey = *Key;
						}
						if (ModuleIndex == 0)
						{
							CurrentTargetKey = TargetKey;
						}
						else
						{
							if (CurrentTargetKey != TargetKey)
							{
								CurrentTargetKey.Name = *ControlRigModuleDetailsMultipleValues.ToString();
							}
						}
					}
				}
				TPair<const FSlateBrush*, FSlateColor> IconAndColor = SRigHierarchyItem::GetBrushForElementType(PerModuleInfos[0].GetModularRig()->GetHierarchy(), CurrentTargetKey);
				PopulateConnectorCurrentTarget(ButtonBox, IconAndColor.Key, IconAndColor.Value, FText::FromName(CurrentTargetKey.Name));
			}
		}
	}

	for(const FName& OriginalCategoryName : OriginalCategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(OriginalCategoryName);
		{
			IPropertyAccessEditor& PropertyAccessEditor = IModularFeatures::Get().GetModularFeature<IPropertyAccessEditor>("PropertyAccessEditor");

			TArray<TSharedRef<IPropertyHandle>> DefaultProperties;
			Category.GetDefaultProperties(DefaultProperties, true, true);

			for(const TSharedRef<IPropertyHandle>& DefaultProperty : DefaultProperties)
			{
				const FProperty* Property = DefaultProperty->GetProperty();
				if(Property == nullptr)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				// skip advanced properties for now
				const bool bAdvancedDisplay = Property->HasAnyPropertyFlags(CPF_AdvancedDisplay);
				if(bAdvancedDisplay)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				// skip non-public properties for now
				const bool bIsPublic = Property->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst);
				const bool bIsInstanceEditable = !Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance);
				if(!bIsPublic || !bIsInstanceEditable)
				{
					DetailBuilder.HideProperty(DefaultProperty);
					continue;
				}

				const FSimpleDelegate OnValueChangedDelegate = FSimpleDelegate::CreateSP(this, &FRigModuleInstanceDetails::OnConfigValueChanged, Property->GetFName());
				DefaultProperty->SetOnPropertyValueChanged(OnValueChangedDelegate);
				DefaultProperty->SetOnChildPropertyValueChanged(OnValueChangedDelegate);

				FPropertyBindingWidgetArgs BindingArgs;
				BindingArgs.Property = (FProperty*)Property;
				BindingArgs.CurrentBindingText = TAttribute<FText>::CreateLambda([this, Property]()
				{
					return GetBindingText(Property);
				});
				BindingArgs.CurrentBindingImage = TAttribute<const FSlateBrush*>::CreateLambda([this, Property]()
				{
					return GetBindingImage(Property);
				});
				BindingArgs.CurrentBindingColor = TAttribute<FLinearColor>::CreateLambda([this, Property]()
				{
					return GetBindingColor(Property);
				});

				BindingArgs.OnCanBindProperty.BindLambda([](const FProperty* InProperty) -> bool { return true; });
				BindingArgs.OnCanBindToClass.BindLambda([](UClass* InClass) -> bool { return false; });
				BindingArgs.OnCanRemoveBinding.BindRaw(this, &FRigModuleInstanceDetails::CanRemoveBinding);
				BindingArgs.OnRemoveBinding.BindSP(this, &FRigModuleInstanceDetails::HandleRemoveBinding);

				BindingArgs.bGeneratePureBindings = true;
				BindingArgs.bAllowNewBindings = true;
				BindingArgs.bAllowArrayElementBindings = false;
				BindingArgs.bAllowStructMemberBindings = false;
				BindingArgs.bAllowUObjectFunctions = false;

				BindingArgs.MenuExtender = MakeShareable(new FExtender);
				BindingArgs.MenuExtender->AddMenuExtension(
					"Properties",
					EExtensionHook::After,
					nullptr,
					FMenuExtensionDelegate::CreateSPLambda(this, [this, Property](FMenuBuilder& MenuBuilder)
					{
						FillBindingMenu(MenuBuilder, Property);
					})
				);

				TSharedPtr<SWidget> ValueWidget = DefaultProperty->CreatePropertyValueWidgetWithCustomization(DetailBuilder.GetDetailsView());

				const bool bShowChildren = true;
				Category.AddProperty(DefaultProperty).CustomWidget(bShowChildren)
				.NameContent()
				[
					DefaultProperty->CreatePropertyNameWidget()
				]

				.ValueContent()
				[
					ValueWidget ? ValueWidget.ToSharedRef() : SNullWidget::NullWidget
					// todo: if the property is bound / or partially bound
					// mark the property value widget as disabled / read only.
				]

				.ExtensionContent()
				[
					PropertyAccessEditor.MakePropertyBindingWidget(nullptr, BindingArgs)
				];
			}
		}
	}
}

FText FRigModuleInstanceDetails::GetName() const
{
	const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule();
	if(FirstModule == nullptr)
	{
		return FText();
	}
	
	const FName FirstValue = FirstModule->Name;
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
			{
				if (!Module->Name.IsEqual(FirstValue, ENameCase::CaseSensitive))
				{
					bSame = false;
					break;
				}
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}
	return FText::FromName(FirstValue);
}

void FRigModuleInstanceDetails::SetName(const FText& InValue, ETextCommit::Type InCommitType, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	if(InValue.IsEmpty())
	{
		return;
	}
	
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				const FString OldPath = ModuleInstance->GetPath();
				(void)Controller->RenameModule(OldPath, *InValue.ToString(), true);
			}
		}
	}
}

bool FRigModuleInstanceDetails::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	if(InText.IsEmpty())
	{
		static const FText EmptyNameIsNotAllowed = LOCTEXT("EmptyNameIsNotAllowed", "Empty name is not allowed.");
		OutErrorMessage = EmptyNameIsNotAllowed;
		return false;
	}

	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				if (!Controller->CanRenameModule(ModuleInstance->GetPath(), *InText.ToString(), OutErrorMessage))
				{
					return false;
				}
			}
		}
	}

	return true;
}

FText FRigModuleInstanceDetails::GetShortName() const
{
	const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule();
	if(FirstModule == nullptr)
	{
		return FText();
	}

	const FString FirstValue = FirstModule->GetShortName();
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
			{
				if (!Module->GetShortName().Equals(FirstValue, ESearchCase::CaseSensitive))
				{
					bSame = false;
					break;
				}
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}
	return FText::FromString(FirstValue);
}

void FRigModuleInstanceDetails::SetShortName(const FText& InValue, ETextCommit::Type InCommitType, const TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	if(InValue.IsEmpty())
	{
		return;
	}
	
	check(PerModuleInfos.Num() == 1);
	const FPerModuleInfo& Info = PerModuleInfos[0];
	if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			UModularRigController* Controller = Blueprint->GetModularRigController();
			Controller->SetModuleShortName(ModuleInstance->GetPath(), *InValue.ToString(), true);
		}
	}
}

bool FRigModuleInstanceDetails::OnVerifyShortNameChanged(const FText& InText, FText& OutErrorMessage)
{
	check(PerModuleInfos.Num() == 1);

	if(InText.IsEmpty())
	{
		return true;
	}

	const FPerModuleInfo& Info = PerModuleInfos[0];
	if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			UModularRigController* Controller = Blueprint->GetModularRigController();
			return Controller->CanSetModuleShortName(ModuleInstance->GetPath(), *InText.ToString(), OutErrorMessage);
		}
	}

	return false;
}

FText FRigModuleInstanceDetails::GetLongName() const
{
	const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule();
	if(FirstModule == nullptr)
	{
		return FText();
	}

	const FString FirstValue = FirstModule->GetLongName();
	if(PerModuleInfos.Num() > 1)
	{
		bool bSame = true;
		for (int32 i=1; i<PerModuleInfos.Num(); ++i)
		{
			if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
			{
				if (!Module->GetLongName().Equals(FirstValue, ESearchCase::CaseSensitive))
				{
					bSame = false;
					break;
				}
			}
		}
		if (!bSame)
		{
			return ControlRigModuleDetailsMultipleValues;
		}
	}
	return FText::FromString(FirstValue);
}
FText FRigModuleInstanceDetails::GetRigClassPath() const
{
	if(PerModuleInfos.Num() > 1)
	{
		if(const FRigModuleInstance* FirstModule = PerModuleInfos[0].GetModule())
		{
			bool bSame = true;
			for (int32 i=1; i<PerModuleInfos.Num(); ++i)
			{
				if(const FRigModuleInstance* Module = PerModuleInfos[i].GetModule())
				{
					if (Module->GetRig()->GetClass() != FirstModule->GetRig()->GetClass())
					{
						bSame = false;
						break;
					}
				}
			}
			if (!bSame)
			{
				return ControlRigModuleDetailsMultipleValues;
			}
		}
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (const UControlRig* ModuleRig = Module->GetRig())
		{
			return FText::FromString(ModuleRig->GetClass()->GetClassPathName().ToString());
		}
	}

	return FText();
}

TArray<FRigModuleConnector> FRigModuleInstanceDetails::GetConnectors() const
{
	if(PerModuleInfos.Num() > 1)
	{
		TArray<FRigModuleConnector> CommonConnectors;
		if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
		{
			if (const UControlRig* ModuleRig = Module->GetRig())
			{
				CommonConnectors = ModuleRig->GetRigModuleSettings().ExposedConnectors;
			}
		}
		for (int32 ModuleIndex=1; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
		{
			if (const FRigModuleInstance* Module = PerModuleInfos[ModuleIndex].GetModule())
			{
				if (const UControlRig* ModuleRig = Module->GetRig())
				{
					const TArray<FRigModuleConnector>& ModuleConnectors = ModuleRig->GetRigModuleSettings().ExposedConnectors;
					CommonConnectors = CommonConnectors.FilterByPredicate([ModuleConnectors](const FRigModuleConnector& Connector)
					{
						return ModuleConnectors.Contains(Connector);
					});
				}
			}
		}
		return CommonConnectors;
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (const UControlRig* ModuleRig = Module->GetRig())
		{
			return ModuleRig->GetRigModuleSettings().ExposedConnectors;
		}
	}

	return TArray<FRigModuleConnector>();
}

FRigElementKeyRedirector FRigModuleInstanceDetails::GetConnections() const
{
	if(PerModuleInfos.Num() > 1)
	{
		return FRigElementKeyRedirector();
	}

	if (const FRigModuleInstance* Module = PerModuleInfos[0].GetModule())
	{
		if (UControlRig* ModuleRig = Module->GetRig())
		{
			return ModuleRig->GetElementKeyRedirector();
		}
	}

	return FRigElementKeyRedirector();
}

void FRigModuleInstanceDetails::PopulateConnectorTargetList(const FString InConnectorName)
{
	ConnectionListBox.FindRef(InConnectorName)->GetTreeView()->RefreshTreeView(true);
}

void FRigModuleInstanceDetails::PopulateConnectorCurrentTarget(TSharedPtr<SVerticalBox> InListBox, const FSlateBrush* InBrush, const FSlateColor& InColor, const FText& InTitle)
{
	static const FSlateBrush* RoundedBoxBrush = FControlRigEditorStyle::Get().GetBrush(TEXT("ControlRig.SpacePicker.RoundedRect"));
	
	TSharedPtr<SHorizontalBox> RowBox, ButtonBox;
	InListBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Fill)
	.Padding(4.0, 0.0, 4.0, 0.0)
	[
		SNew( SButton )
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(0.0))
		[
			SAssignNew(RowBox, SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(0)
			[
				SNew(SBorder)
				.Padding(FMargin(5.0, 2.0, 5.0, 2.0))
				.BorderImage(RoundedBoxBrush)
				.BorderBackgroundColor(FSlateColor(FLinearColor::Transparent))
				.Content()
				[
					SAssignNew(ButtonBox, SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
					[
						SNew(SImage)
						.Image(InBrush)
						.ColorAndOpacity(InColor)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					.Padding(0)
					[
						SNew( STextBlock )
						.Text( InTitle )
						.Font( IDetailLayoutBuilder::GetDetailFont() )
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SSpacer)
					]
				]
			]
		]
	];
}

void FRigModuleInstanceDetails::OnConfigValueChanged(const FName InVariableName)
{
	TMap<FString, FString> ModuleValues;
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if (const UControlRig* ModuleRig = ModuleInstance->GetRig())
			{
				FString ValueStr = ModuleRig->GetVariableAsString(InVariableName);
				ModuleValues.Add(ModuleInstance->GetPath(), ValueStr);
			}
		}
	}
	
	for (const TPair<FString, FString>& Value : ModuleValues)
	{
		if (UControlRigBlueprint* Blueprint = PerModuleInfos[0].GetBlueprint())
		{
			UModularRigController* Controller = Blueprint->GetModularRigController();
			Controller->SetConfigValueInModule(Value.Key, InVariableName, Value.Value);
		}
	}
}

void FRigModuleInstanceDetails::OnConnectorTargetChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo, const FRigModuleConnector InConnector)
{
	if (SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}
	
	FScopedTransaction Transaction(LOCTEXT("ModuleHierarchyResolveConnector", "Resolve Connector"));
	for (FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UModularRigController* Controller = Info.GetBlueprint()->GetModularRigController())
		{
			FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *Info.Path, *InConnector.Name);
			FRigElementKey ConnectorKey(*ConnectorPath, ERigElementType::Connector);
			if (Selection.IsValid())
			{
				const FModularRigSettings& Settings = Info.GetModularRig()->GetModularRigSettings();
				Controller->ConnectConnectorToElement(ConnectorKey, Selection->Key, true, Settings.bAutoResolve);
			}
			else
			{
				Controller->DisconnectConnector(ConnectorKey);
			}
		}
	}
}

const FRigModuleInstanceDetails::FPerModuleInfo& FRigModuleInstanceDetails::FindModule(const FString& InPath) const
{
	const FPerModuleInfo* Info = FindModuleByPredicate([InPath](const FPerModuleInfo& Info)
	{
		if(const FRigModuleInstance* Module = Info.GetModule())
		{
			return Module->GetPath() == InPath;
		}
		return false;
	});

	if(Info)
	{
		return *Info;
	}

	static const FPerModuleInfo EmptyInfo;
	return EmptyInfo;
}

const FRigModuleInstanceDetails::FPerModuleInfo* FRigModuleInstanceDetails::FindModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.FindByPredicate(InPredicate);
}

bool FRigModuleInstanceDetails::ContainsModuleByPredicate(const TFunction<bool(const FPerModuleInfo&)>& InPredicate) const
{
	return PerModuleInfos.ContainsByPredicate(InPredicate);
}

void FRigModuleInstanceDetails::RegisterSectionMappings(FPropertyEditorModule& PropertyEditorModule, UClass* InClass)
{
	TSharedRef<FPropertySection> MetadataSection = PropertyEditorModule.FindOrCreateSection(InClass->GetFName(), "Metadata", LOCTEXT("Metadata", "Metadata"));
	MetadataSection->AddCategory("Metadata");
}

FText FRigModuleInstanceDetails::GetBindingText(const FProperty* InProperty) const
{
	const FName VariableName = InProperty->GetFName();
	FText FirstValue;
	for (int32 ModuleIndex=0; ModuleIndex<PerModuleInfos.Num(); ++ModuleIndex)
	{
		if (const FRigModuleReference* ModuleReference = PerModuleInfos[ModuleIndex].GetReference())
		{
			if(ModuleReference->Bindings.Contains(VariableName))
			{
				const FText BindingText = FText::FromString(ModuleReference->Bindings.FindChecked(VariableName));
				if(ModuleIndex == 0)
				{
					FirstValue = BindingText;
				}
				else if(!FirstValue.EqualTo(BindingText))
				{
					return ControlRigModuleDetailsMultipleValues;
				}
			}
		}
	}
	return FirstValue;
}

const FSlateBrush* FRigModuleInstanceDetails::GetBindingImage(const FProperty* InProperty) const
{
	static FName TypeIcon(TEXT("Kismet.VariableList.TypeIcon"));
	static FName ArrayTypeIcon(TEXT("Kismet.VariableList.ArrayTypeIcon"));

	if(CastField<FArrayProperty>(InProperty))
	{
		return FAppStyle::GetBrush(ArrayTypeIcon);
	}
	return FAppStyle::GetBrush(TypeIcon);
}

FLinearColor FRigModuleInstanceDetails::GetBindingColor(const FProperty* InProperty) const
{
	if(InProperty)
	{
		FEdGraphPinType PinType;
		const UEdGraphSchema_K2* Schema_K2 = GetDefault<UEdGraphSchema_K2>();
		if (Schema_K2->ConvertPropertyToPinType(InProperty, PinType))
		{
			const URigVMEdGraphSchema* Schema = GetDefault<URigVMEdGraphSchema>();
			return Schema->GetPinTypeColor(PinType);
		}
	}
	return FLinearColor::White;
}

void FRigModuleInstanceDetails::FillBindingMenu(FMenuBuilder& MenuBuilder, const FProperty* InProperty) const
{
	if(PerModuleInfos.IsEmpty())
	{
		return;
	}

	UControlRigBlueprint* Blueprint = PerModuleInfos[0].GetBlueprint();
	UModularRigController* Controller = Blueprint->GetModularRigController();

	TArray<FString> CombinedBindings;
	for(int32 Index = 0; Index < PerModuleInfos.Num(); Index++)
	{
		const FPerModuleInfo& Info  = PerModuleInfos[Index];
		const TArray<FString> Bindings = Controller->GetPossibleBindings(Info.GetPath(), InProperty->GetFName());
		if(Index == 0)
		{
			CombinedBindings = Bindings;
		}
		else
		{
			// reduce the set of bindings to the overall possible bindings
			CombinedBindings.RemoveAll([Bindings](const FString& Binding)
			{
				return !Bindings.Contains(Binding);
			});
		}
	}

	if(CombinedBindings.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction()),
			SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoBindingAvailable", "No bindings available for this property."))
					.ColorAndOpacity(FLinearColor::White)
				]
			);
		return;
	}

	// sort lexically
	CombinedBindings.Sort();

	// create a map of all of the variables per menu prefix (the module path the variables belong to)
	struct FPerMenuData
	{
		FString Name;
		FString ParentMenuPath;
		TArray<FString> SubMenuPaths;
		TArray<FString> Variables;

		static void SetupMenu(
			TSharedRef<FRigModuleInstanceDetails const> ThisDetails,
			const FProperty* InProperty,
			FMenuBuilder& InMenuBuilder,
			const FString& InMenuPath,
			TSharedRef<TMap<FString, FPerMenuData>> PerMenuData)
		{
			FPerMenuData& Data = PerMenuData->FindChecked((InMenuPath));

			Data.SubMenuPaths.Sort();
			Data.Variables.Sort();

			for(const FString& VariablePath : Data.Variables)
			{
				FString VariableName = VariablePath;
				(void)URigHierarchy::SplitNameSpace(VariablePath, nullptr, &VariableName);
				
				InMenuBuilder.AddMenuEntry(
					FUIAction(FExecuteAction::CreateLambda([ThisDetails, InProperty, VariablePath]()
					{
						ThisDetails->HandleChangeBinding(InProperty, VariablePath);
					})),
					SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(1.0f, 0.0f)
						[
							SNew(SImage)
							.Image(ThisDetails->GetBindingImage(InProperty))
							.ColorAndOpacity(ThisDetails->GetBindingColor(InProperty))
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(VariableName))
							.ColorAndOpacity(FLinearColor::White)
						]
					);
			}

			for(const FString& SubMenuPath : Data.SubMenuPaths)
			{
				const FPerMenuData& SubMenuData = PerMenuData->FindChecked(SubMenuPath);

				const FText Label = FText::FromString(SubMenuData.Name);
				static const FText TooltipFormat = LOCTEXT("BindingMenuTooltipFormat", "Access to all variables of the {0} module");
				const FText Tooltip = FText::Format(TooltipFormat, Label);  
				InMenuBuilder.AddSubMenu(Label, Tooltip, FNewMenuDelegate::CreateLambda([ThisDetails, InProperty, SubMenuPath, PerMenuData](FMenuBuilder& SubMenuBuilder)
				{
					SetupMenu(ThisDetails, InProperty, SubMenuBuilder, SubMenuPath, PerMenuData);
				}));
			}
		}
	};
	
	// define the root menu
	const TSharedRef<TMap<FString, FPerMenuData>> PerMenuData = MakeShared<TMap<FString, FPerMenuData>>();
	PerMenuData->FindOrAdd(FString());

	// make sure all levels of the menu are known and we have the variables available
	for(const FString& BindingPath : CombinedBindings)
	{
		FString MenuPath;
		(void)URigHierarchy::SplitNameSpace(BindingPath, &MenuPath, nullptr);

		FString PreviousMenuPath = MenuPath;
		FString ParentMenuPath = MenuPath, RemainingPath;
		while(URigHierarchy::SplitNameSpace(ParentMenuPath, &ParentMenuPath, &RemainingPath))
		{
			// scope since the map may change at the end of this block
			{
				FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
				if(Data.Name.IsEmpty())
				{
					Data.Name = RemainingPath;
				}
			}
			
			PerMenuData->FindOrAdd(ParentMenuPath).SubMenuPaths.AddUnique(PreviousMenuPath);
			PerMenuData->FindOrAdd(PreviousMenuPath).ParentMenuPath = ParentMenuPath;
			PerMenuData->FindOrAdd(PreviousMenuPath).Name = RemainingPath;
			if(!ParentMenuPath.Contains(UModularRig::NamespaceSeparator))
			{
				PerMenuData->FindOrAdd(FString()).SubMenuPaths.AddUnique(ParentMenuPath);
				PerMenuData->FindOrAdd(ParentMenuPath).Name = ParentMenuPath;
			}
			PreviousMenuPath = ParentMenuPath;
		}

		FPerMenuData& Data = PerMenuData->FindOrAdd(MenuPath);
		if(Data.Name.IsEmpty())
		{
			Data.Name = MenuPath;
		}

		Data.Variables.Add(BindingPath);
		if(!MenuPath.IsEmpty())
		{
			PerMenuData->FindChecked(Data.ParentMenuPath).SubMenuPaths.AddUnique(MenuPath);
		}
	}

	// build the menu
	FPerMenuData::SetupMenu(SharedThis(this), InProperty, MenuBuilder, FString(), PerMenuData);
}

bool FRigModuleInstanceDetails::CanRemoveBinding(FName InPropertyName) const
{
	// offer the "removing binding" button if any of the selected module instances
	// has a binding for the given variable
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
		{
			if(ModuleInstance->VariableBindings.Contains(InPropertyName))
			{
				return true;
			}
		}
	}
	return false; 
}

void FRigModuleInstanceDetails::HandleRemoveBinding(FName InPropertyName) const
{
	FScopedTransaction Transaction(LOCTEXT("RemoveModuleVariableTransaction", "Remove Binding"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->UnBindModuleVariable(ModuleInstance->GetPath(), InPropertyName);
			}
		}
	}
}

void FRigModuleInstanceDetails::HandleChangeBinding(const FProperty* InProperty, const FString& InNewVariablePath) const
{
	FScopedTransaction Transaction(LOCTEXT("BindModuleVariableTransaction", "Bind Module Variable"));
	for(const FPerModuleInfo& Info : PerModuleInfos)
	{
		if (UControlRigBlueprint* Blueprint = Info.GetBlueprint())
		{
			if (const FRigModuleInstance* ModuleInstance = Info.GetModule())
			{
				UModularRigController* Controller = Blueprint->GetModularRigController();
				Controller->BindModuleVariable(ModuleInstance->GetPath(), InProperty->GetFName(), InNewVariablePath);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
