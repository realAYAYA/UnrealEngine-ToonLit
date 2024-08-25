// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimGraph/AnimGraphNodeBinding_AnimNextParameters.h"
#include "Graph/AnimGraph/AnimNodeExposedValueHandler_AnimNextParameters.h"
#include "Graph/AnimGraph/AnimBlueprintExtension_AnimNextParameters.h"
#include "Widgets/SNullWidget.h"
#include "UncookedOnlyUtils.h"
#include "AnimBlueprintExtension_Base.h"

#if WITH_EDITOR
#include "AnimNextEditorModule.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Param/ParamUtils.h"
#include "Param/ParamCompatibility.h"
#include "FindInBlueprintManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#endif

#define LOCTEXT_NAMESPACE "AnimGraphNodeBinding_AnimNextParameters"

UScriptStruct* UAnimGraphNodeBinding_AnimNextParameters::GetAnimNodeHandlerStruct() const
{
	return FAnimNodeExposedValueHandler_AnimNextParameters::StaticStruct();
}

void UAnimGraphNodeBinding_AnimNextParameters::OnExpandNode(IAnimBlueprintCompilationContext& InCompilationContext, UAnimGraphNode_Base* InNode, UEdGraph* InSourceGraph)
{
	if (UAnimGraphNode_Base* Node = GetTypedOuter<UAnimGraphNode_Base>())
	{
		if (UAnimBlueprint* AnimBlueprint = Node->GetTypedOuter<UAnimBlueprint>())
		{
			UAnimBlueprintExtension_AnimNextParameters* AnimNextExtension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_AnimNextParameters>(AnimBlueprint);
			AnimNextExtension->RedirectPropertyPaths(InCompilationContext, InNode);
		}
	}
}

bool UAnimGraphNodeBinding_AnimNextParameters::HasBinding(FName InBindingName, bool bCheckArrayIndexName) const
{
	if (bCheckArrayIndexName)
	{
		// This checks for the 'pin name' of an array element
		return PropertyBindings.Find(InBindingName) != nullptr;
	}
	else
	{
		// Comparison without name index to deal with arrays
		// This checks for any pin of an array element and the array itself
		const FName ComparisonName = FName(InBindingName, 0);

		for (const TPair<FName, FAnimNextAnimGraphNodeParameterBinding>& BindingPair : PropertyBindings)
		{
			if (ComparisonName == FName(BindingPair.Key, 0))
			{
				return true;
			}
		}

		return false;
	}
}

void UAnimGraphNodeBinding_AnimNextParameters::RemoveBindings(FName InBindingName)
{
	const FName ComparisonName = FName(InBindingName, 0);

	for (auto Iter = PropertyBindings.CreateIterator(); Iter; ++Iter)
	{
		if (ComparisonName == FName(Iter.Key(), 0))
		{
			Iter.RemoveCurrent();
		}
	}
}

void UAnimGraphNodeBinding_AnimNextParameters::AddPinSearchMetaDataInfo(const UEdGraphPin* InPin, FName InBindingName, TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	if (const FAnimNextAnimGraphNodeParameterBinding* BindingInfo = PropertyBindings.Find(InBindingName))
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(FText::FromString(TEXT("Parameter Binding")), BindingInfo->CachedParameterNameText));
	}
}

void UAnimGraphNodeBinding_AnimNextParameters::UpdateBindingNames(TFunctionRef<FString(const FString& InOldName)> InModifierFunction)
{
	if (UAnimGraphNode_Base* Node = GetTypedOuter<UAnimGraphNode_Base>())
	{
		TMap<FName, FAnimNextAnimGraphNodeParameterBinding> NewBindings;

		for (const TPair<FName, FAnimNextAnimGraphNodeParameterBinding>& BindingPair : PropertyBindings)
		{
			FString BindingNameString = BindingPair.Key.ToString();

			FString NewNameString = InModifierFunction(BindingNameString);

			if (NewNameString != BindingNameString)
			{
				FName NewName = *NewNameString;
				FAnimNextAnimGraphNodeParameterBinding NewBinding = BindingPair.Value;
				NewBinding.BindingName = NewName;
				NewBindings.Add(NewName, NewBinding);
			}
		}

		if (NewBindings.Num() > 0)
		{
			PropertyBindings = NewBindings;
		}
	}
}

void UAnimGraphNodeBinding_AnimNextParameters::GetRequiredExtensions(TArray<TSubclassOf<UAnimBlueprintExtension>>& OutExtensions) const
{
	OutExtensions.Add(UAnimBlueprintExtension_AnimNextParameters::StaticClass());
}

void UAnimGraphNodeBinding_AnimNextParameters::ProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	if (UAnimGraphNode_Base* Node = GetTypedOuter<UAnimGraphNode_Base>())
	{
		if (UAnimBlueprint* AnimBlueprint = Node->GetTypedOuter<UAnimBlueprint>())
		{
			UAnimBlueprintExtension_AnimNextParameters* AnimNextExtension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_AnimNextParameters>(AnimBlueprint);
			// Record pose pins for later patchup and gather pins that have an associated evaluation handler
			AnimNextExtension->ProcessNodePins(Node, InCompilationContext, OutCompiledData);

			// Call base handler to allow BP logic to be wired up as well as AnimNext parameters
			UAnimBlueprintExtension_Base* BaseExtension = UAnimBlueprintExtension::GetExtension<UAnimBlueprintExtension_Base>(AnimBlueprint);
			BaseExtension->ProcessNonPosePins(Node, InCompilationContext, OutCompiledData, UAnimBlueprintExtension_Base::EPinProcessingFlags::Constants | UAnimBlueprintExtension_Base::EPinProcessingFlags::BlueprintHandlers);
		}
	}
}

#if WITH_EDITOR
TSharedRef<SWidget> UAnimGraphNodeBinding_AnimNextParameters::MakePropertyBindingWidget(const UAnimGraphNode_Base::FAnimPropertyBindingWidgetArgs& InArgs)
{
	using namespace UE::AnimNext;
	using namespace UE::AnimNext::Editor;
	using namespace UE::AnimNext::UncookedOnly;

	enum class ECurrentValueType : int32
	{
		None,
		Pin,
		Binding,
		Dynamic,
		MultipleValues,
	};

	UAnimGraphNode_Base* FirstAnimGraphNode = InArgs.Nodes[0];
	const bool bMultiSelect = InArgs.Nodes.Num() > 1;

	if (FirstAnimGraphNode->HasValidBlueprint())
	{
		UBlueprint* Blueprint = FirstAnimGraphNode->GetAnimBlueprint();

		int32 PinArrayIndex = InArgs.PinName.GetNumber() - 1;
		const bool bIsArrayOrArrayElement = InArgs.PinProperty->IsA<FArrayProperty>();
		const bool bIsArrayElement = bIsArrayOrArrayElement && PinArrayIndex != INDEX_NONE && InArgs.bPropertyIsOnFNode;
		const bool bIsArray = bIsArrayOrArrayElement && PinArrayIndex == INDEX_NONE && InArgs.bPropertyIsOnFNode;

		FProperty* PropertyToBindTo = bIsArrayElement ? CastField<FArrayProperty>(InArgs.PinProperty)->Inner : InArgs.PinProperty;

		// Properties could potentially be removed underneath this widget, so keep a TFieldPath reference to them
		TFieldPath<FProperty> BindingPropertyPath(PropertyToBindTo);
		TFieldPath<FProperty> PinPropertyPath(InArgs.PinProperty);

		return SNew(SComboButton)
			.ToolTipText_Lambda([InArgs]()
			{
				ECurrentValueType CurrentValueType = ECurrentValueType::None;

				const FText MultipleValues = LOCTEXT("MultipleValuesToolTip", "Bindings Have Multiple Values");
				const FText ExposedAsPin = LOCTEXT("ExposedAsPinToolTip", "Exposed As a Pin on the Node");
				const FText BindValue = LOCTEXT("BindValueToolTip", "Bind This Value");
				const FText DynamicValue = LOCTEXT("DynamicValueToolTip", "Dynamic value that can be set externally");
				FText CurrentValue;

				auto SetAssignValue = [&CurrentValueType, &CurrentValue, &MultipleValues](const FText& InValue, ECurrentValueType InType)
				{
					if (CurrentValueType != ECurrentValueType::MultipleValues)
					{
						if (CurrentValueType == ECurrentValueType::None)
						{
							CurrentValueType = InType;
							CurrentValue = InValue;
						}
						else if (CurrentValueType == InType)
						{
							if (!CurrentValue.EqualTo(InValue))
							{
								CurrentValueType = ECurrentValueType::MultipleValues;
								CurrentValue = MultipleValues;
							}
						}
						else
						{
							CurrentValueType = ECurrentValueType::MultipleValues;
							CurrentValue = MultipleValues;
						}
					}
				};

				const FName ComparisonName = FName(InArgs.PinName, 0);

				for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
				{
					if(const UAnimGraphNodeBinding_AnimNextParameters* Binding = Cast<UAnimGraphNodeBinding_AnimNextParameters>(AnimGraphNode->GetBinding()))
					{
						if (const FAnimNextAnimGraphNodeParameterBinding* BindingPtr = Binding->PropertyBindings.Find(InArgs.BindingName))
						{
							if (BindingPtr->CachedParameterNameText.IsEmpty())
							{
								SetAssignValue(BindValue, ECurrentValueType::Binding);
							}
							else
							{
								SetAssignValue(FText::Format(LOCTEXT("BindingToolTipFormat", "Pin is bound to parameter '{0}'"), BindingPtr->CachedParameterNameText), ECurrentValueType::Binding);
							}
						}
						else if (AnimGraphNode->AlwaysDynamicProperties.Find(ComparisonName))
						{
							SetAssignValue(DynamicValue, ECurrentValueType::Dynamic);
						}
						else
						{
							TArrayView<FOptionalPinFromProperty> OptionalPins;
							InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
							if (OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
							{
								SetAssignValue(InArgs.bOnGraphNode ? BindValue : ExposedAsPin, ECurrentValueType::Pin);
							}
							else
							{
								SetAssignValue(BindValue, ECurrentValueType::None);
							}
						}
					}
				}

				return CurrentValue;
			})
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
				.Text_Lambda([InArgs]()
				{
					ECurrentValueType CurrentValueType = ECurrentValueType::None;

					const FText MultipleValues = LOCTEXT("MultipleValuesLabel", "Multiple Values");
					const FText Bind = LOCTEXT("BindLabel", "Param");
					const FText ExposedAsPin = LOCTEXT("ExposedAsPinLabel", "Pin");
					const FText Dynamic = LOCTEXT("DynamicLabel", "Dynamic");
					FText CurrentValue = Bind;

					auto SetAssignValue = [&CurrentValueType, &CurrentValue, &MultipleValues](const FText& InValue, ECurrentValueType InType)
					{
						if (CurrentValueType != ECurrentValueType::MultipleValues)
						{
							if (CurrentValueType == ECurrentValueType::None)
							{
								CurrentValueType = InType;
								CurrentValue = InValue;
							}
							else if (CurrentValueType == InType)
							{
								if (!CurrentValue.EqualTo(InValue))
								{
									CurrentValueType = ECurrentValueType::MultipleValues;
									CurrentValue = MultipleValues;
								}
							}
							else
							{
								CurrentValueType = ECurrentValueType::MultipleValues;
								CurrentValue = MultipleValues;
							}
						}
					};

					const FName ComparisonName = FName(InArgs.PinName, 0);

					for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
					{
						if(const UAnimGraphNodeBinding_AnimNextParameters* Binding = Cast<UAnimGraphNodeBinding_AnimNextParameters>(AnimGraphNode->GetBinding()))
						{
							if (const FAnimNextAnimGraphNodeParameterBinding* BindingPtr = Binding->PropertyBindings.Find(InArgs.BindingName))
							{
								SetAssignValue(BindingPtr->CachedParameterNameText, ECurrentValueType::Binding);
							}
							else if (AnimGraphNode->AlwaysDynamicProperties.Find(ComparisonName))
							{
								SetAssignValue(Dynamic, ECurrentValueType::Dynamic);
							}
							else
							{
								TArrayView<FOptionalPinFromProperty> OptionalPins;
								InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
								if (OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin)
								{
									SetAssignValue(InArgs.bOnGraphNode ? Bind : ExposedAsPin, ECurrentValueType::Pin);
								}
								else
								{
									SetAssignValue(Bind, ECurrentValueType::None);
								}
							}
						}
					}

					return CurrentValue;
				})
			]
			.OnGetMenuContent(FOnGetContent::CreateLambda([InArgs, PropertyToBindTo, BindingPropertyPath, PinArrayIndex, bIsArray, bIsArrayElement, Blueprint]()
			{
				FMenuBuilder MenuBuilder(true, nullptr);
				MenuBuilder.BeginSection("Pins", LOCTEXT("Pin", "Pin"));
				{
					auto ExposeAsPin = [InArgs, Blueprint]()
					{
						bool bHasBinding = false;

						for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
						{
							if (AnimGraphNode->HasBinding(InArgs.BindingName))
							{
								bHasBinding = true;
								break;
							}
						}

						{
							FScopedTransaction Transaction(LOCTEXT("PinExposure", "Pin Exposure"));

							// Switching from non-pin to pin, remove any bindings
							for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
							{
								if(UAnimGraphNodeBinding_AnimNextParameters* Binding = CastChecked<UAnimGraphNodeBinding_AnimNextParameters>(AnimGraphNode->GetMutableBinding()))
								{
									Binding->Modify();
									AnimGraphNode->Modify();

									TArrayView<FOptionalPinFromProperty> OptionalPins;
									InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
									const bool bVisible = OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin;
									InArgs.OnSetPinVisibility.ExecuteIfBound(AnimGraphNode, !bVisible || bHasBinding, InArgs.OptionalPinIndex);

									// Remove all bindings that match the property, array or array elements
									const FName ComparisonName = FName(InArgs.BindingName, 0);
									for (auto It = Binding->PropertyBindings.CreateIterator(); It; ++It)
									{
										if (ComparisonName == FName(It.Key(), 0))
										{
											It.RemoveCurrent();
										}
									}
								}
							}

							FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
						}
					};

					auto GetExposedAsPinCheckState = [InArgs]()
					{
						bool bPinShown = false;
						bool bHasBinding = false;

						for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
						{
							if (AnimGraphNode->HasBinding(InArgs.BindingName))
							{
								bHasBinding = true;
								break;
							}

							TArrayView<FOptionalPinFromProperty> OptionalPins;
							InArgs.OnGetOptionalPins.ExecuteIfBound(AnimGraphNode, OptionalPins);
							bPinShown |= OptionalPins.IsValidIndex(InArgs.OptionalPinIndex) && OptionalPins[InArgs.OptionalPinIndex].bShowPin;
						}

						// Pins are exposed if we have a binding or not, so treat as unchecked only if we have
						// no binding
						return bPinShown && !bHasBinding ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					};

					MenuBuilder.AddMenuEntry(
						LOCTEXT("ExposeAsPin", "Expose As Pin"),
						LOCTEXT("ExposeAsPinTooltip", "Show/hide this property as a pin on the node"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.PinIcon"),
						FUIAction(
							FExecuteAction::CreateLambda(ExposeAsPin),
							FCanExecuteAction(),
							FGetActionCheckState::CreateLambda(GetExposedAsPinCheckState)
						),
						NAME_None,
						EUserInterfaceActionType::Check
					);

					if (InArgs.bPropertyIsOnFNode)
					{
						auto MakeDynamic = [InArgs, Blueprint]()
						{
							// Comparison without name index to deal with arrays
							const FName ComparisonName = FName(InArgs.PinName, 0);

							bool bIsAlwaysDynamic = false;
							for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
							{
								if (AnimGraphNode->AlwaysDynamicProperties.Contains(ComparisonName))
								{
									bIsAlwaysDynamic = true;
									break;
								}
							}

							{
								FScopedTransaction Transaction(LOCTEXT("AlwaysDynamic", "Always Dynamic"));

								for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
								{
									AnimGraphNode->Modify();

									if (bIsAlwaysDynamic)
									{
										AnimGraphNode->AlwaysDynamicProperties.Remove(ComparisonName);
									}
									else
									{
										AnimGraphNode->AlwaysDynamicProperties.Add(ComparisonName);
									}
								}

								FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
							}
						};

						auto GetDynamicCheckState = [InArgs]()
						{
							// Comparison without name index to deal with arrays
							const FName ComparisonName = FName(InArgs.PinName, 0);

							bool bIsAlwaysDynamic = false;
							for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
							{
								for (const FName& AlwaysDynamicPropertyName : AnimGraphNode->AlwaysDynamicProperties)
								{
									if (ComparisonName == FName(AlwaysDynamicPropertyName, 0))
									{
										bIsAlwaysDynamic = true;
										break;
									}
								}
							}

							return bIsAlwaysDynamic ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						};

						MenuBuilder.AddMenuEntry(
							LOCTEXT("DynamicValue", "Dynamic Value"),
							LOCTEXT("DynamicValueTooltip", "Flag this value as dynamic. This way it can be set from functions even when not exposed as a pin."),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda(MakeDynamic),
								FCanExecuteAction(),
								FGetActionCheckState::CreateLambda(GetDynamicCheckState)
							),
							NAME_None,
							EUserInterfaceActionType::Check
						);
					}
				}
				MenuBuilder.EndSection();

				IModule& EditorModule = FModuleManager::Get().LoadModuleChecked<IModule>("AnimNextEditor");
				FParameterPickerArgs Args;
				Args.OnParameterPicked = FOnParameterPicked::CreateLambda([InArgs, BindingPropertyPath, PinArrayIndex, bIsArray, bIsArrayElement, Blueprint](const FParameterBindingReference& InBinding)
				{
					FSlateApplication::Get().DismissAllMenus();

					for (UAnimGraphNode_Base* AnimGraphNode : InArgs.Nodes)
					{
						if (UAnimGraphNodeBinding_AnimNextParameters* Binding = Cast<UAnimGraphNodeBinding_AnimNextParameters>(AnimGraphNode->GetMutableBinding()))
						{
							Binding->Modify();

							// Reset to default so that references are not preserved
							FProperty* BindingProperty = BindingPropertyPath.Get();
							if (BindingProperty && BindingProperty->GetOwner<UStruct>() && AnimGraphNode->GetFNodeType()->IsChildOf(BindingProperty->GetOwner<UStruct>()) && BindingProperty->IsA<FObjectPropertyBase>())
							{
								void* PropertyAddress = BindingProperty->ContainerPtrToValuePtr<void>(AnimGraphNode->GetFNode());
								BindingProperty->InitializeValue(PropertyAddress);
							}

							// Pins are exposed if we have a binding or not - and after running this we do.
							InArgs.OnSetPinVisibility.ExecuteIfBound(AnimGraphNode, true, InArgs.OptionalPinIndex);

							// Need to break all pin links now we have a binding
							if (UEdGraphPin* Pin = AnimGraphNode->FindPin(InArgs.PinName))
							{
								Pin->BreakAllPinLinks();
							}

							if (bIsArray)
							{
								// Remove bindings for array elements if this is an array
								FName ComparisonName(InArgs.BindingName, 0);
								for (auto Iter = Binding->PropertyBindings.CreateIterator(); Iter; ++Iter)
								{
									if (ComparisonName == FName(Iter.Key(), 0))
									{
										Iter.RemoveCurrent();
									}
								}
							}
							else if (bIsArrayElement)
							{
								// If we are an array element, remove only whole-array bindings
								FName ComparisonName(InArgs.BindingName, 0);
								for (auto Iter = Binding->PropertyBindings.CreateIterator(); Iter; ++Iter)
								{
									if (ComparisonName == Iter.Key())
									{
										Iter.RemoveCurrent();
									}
								}
							}

							FAnimNextAnimGraphNodeParameterBinding NewBinding;
							NewBinding.BindingName = InArgs.BindingName;
							NewBinding.ParameterName = InBinding.Parameter;
							NewBinding.CachedParameterNameText = UncookedOnly::FUtils::GetParameterDisplayNameText(NewBinding.ParameterName);
							NewBinding.ArrayIndex = PinArrayIndex;
							Binding->PropertyBindings.Add(InArgs.BindingName, NewBinding);
						}
					}

					FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				});
				Args.OnFilterParameterType = FOnFilterParameterType::CreateLambda([ParamTypeHandle = FParamTypeHandle::FromProperty(PropertyToBindTo)](const FAnimNextParamType& InParamType)
				{
					return (FParamUtils::GetCompatibility(ParamTypeHandle, InParamType.GetHandle()).IsCompatibleWithDataLoss()) ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
				});
				Args.NewParameterType = FParamTypeHandle::FromProperty(PropertyToBindTo).GetType();
				Args.bMultiSelect = false;
				Args.bShowBlocks = false;
			
				MenuBuilder.AddWidget(
					SNew(SBox)
					.WidthOverride(300.0f)
					.HeightOverride(400.0f)
					[
						EditorModule.CreateParameterPicker(Args)
					],
					FText::GetEmpty(), true, false);

				return MenuBuilder.MakeWidget();
			}));
	}
	else
	{
		return SNew(SSpacer);
	}
}
#endif

#undef LOCTEXT_NAMESPACE