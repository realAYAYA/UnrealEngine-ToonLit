// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPropertyBinding.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"

#if WITH_EDITOR
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#endif // WITH_EDITOR

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"

#include "DetailLayoutBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Components/WidgetComponent.h"
#include "Binding/PropertyBinding.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Algo/Transform.h"
#include "Widgets/Layout/SSpacer.h"
#include "UObject/UObjectIterator.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "PropertyBinding"

/////////////////////////////////////////////////////
// SPropertyBinding

void SPropertyBinding::Construct(const FArguments& InArgs, UBlueprint* InBlueprint, const TArray<FBindingContextStruct>& InBindingContextStructs)
{
	Blueprint = InBlueprint;
	BindingContextStructs = InBindingContextStructs;

	Args = InArgs._Args;
	PropertyName = Args.Property != nullptr ? Args.Property->GetFName() : NAME_None;

	ChildSlot
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SComboButton)
			.ToolTipText(this, &SPropertyBinding::GetCurrentBindingToolTipText)
			.OnGetMenuContent(this, &SPropertyBinding::OnGenerateDelegateMenu)
			.ContentPadding(1)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				.Clipping(EWidgetClipping::ClipToBounds)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(16.0f)
					[
						SNew(SImage)
						.Image(this, &SPropertyBinding::GetCurrentBindingImage)
						.ColorAndOpacity(this, &SPropertyBinding::GetCurrentBindingColor)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(STextBlock)
					.Text(this, &SPropertyBinding::GetCurrentBindingText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.Visibility(this, &SPropertyBinding::GetGotoBindingVisibility)
			.OnClicked(this, &SPropertyBinding::HandleGotoBindingClicked)
			.VAlign(VAlign_Center)
			.ToolTipText(LOCTEXT("GotoFunction", "Goto Function"))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Search"))
			]
		]
	];
}

bool SPropertyBinding::IsClassDenied(UClass* InClass) const
{
	if(Args.OnCanBindToClass.IsBound() && Args.OnCanBindToClass.Execute(InClass))
	{
		return false;
	}

	return true;
}

bool SPropertyBinding::IsFieldFromDeniedClass(FFieldVariant Field) const
{
	return IsClassDenied(Field.GetOwnerClass());
}

template <typename Predicate>
void SPropertyBinding::ForEachBindableFunction(UClass* FromClass, Predicate Pred) const
{
	if(Args.OnCanBindFunction.IsBound())
	{
		auto CheckBindableClass = [this, &Pred](UClass* InBindableClass)
		{
			// Walk up class hierarchy for native functions and properties
			for ( TFieldIterator<UFunction> FuncIt(InBindableClass, EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt )
			{
				UFunction* Function = *FuncIt;

				// Stop processing functions after reaching a base class that it doesn't make sense to go beyond.
				if ( IsFieldFromDeniedClass(Function) )
				{
					break;
				}

				// Only allow binding pure functions if we're limited to pure function bindings.
				if ( Args.bGeneratePureBindings && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
				{
					continue;
				}

				// Only bind to functions that are callable from blueprints
				if ( !UEdGraphSchema_K2::CanUserKismetCallFunction(Function) )
				{
					continue;
				}

				bool bValidObjectFunction = false;
				if(Args.bAllowUObjectFunctions)
				{
					FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(Function->GetReturnProperty());
					if(ObjectPropertyBase != nullptr && Function->NumParms == 1)
					{
						bValidObjectFunction = true;
					}
				}

				bool bValidStructFunction = false;
				if(Args.bAllowStructFunctions)
				{
					FStructProperty* StructProperty = CastField<FStructProperty>(Function->GetReturnProperty());
					if(StructProperty != nullptr && Function->NumParms == 1)
					{
						bValidStructFunction = true;
					}
				}

				if(bValidObjectFunction || bValidStructFunction || Args.OnCanBindFunction.Execute(Function))
				{
					Pred(FFunctionInfo(Function));
				}
			}
		};

		CheckBindableClass(FromClass);

		if(Args.bAllowFunctionLibraryBindings)
		{
			// Iterate function libraries
			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* TestClass = *ClassIt;
				if (TestClass->IsChildOf(UBlueprintFunctionLibrary::StaticClass()) && (!TestClass->HasAnyClassFlags(CLASS_Abstract)))
				{
					CheckBindableClass(TestClass);
				}
			}
		}
	}
}

template <typename Predicate>
void SPropertyBinding::ForEachBindableProperty(UStruct* InStruct, Predicate Pred) const
{
	if(Args.OnCanBindProperty.IsBound())
	{
		UBlueprintGeneratedClass* SkeletonClass = Blueprint ? Cast<UBlueprintGeneratedClass>(Blueprint->SkeletonGeneratedClass) : nullptr;

		for ( TFieldIterator<FProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt )
		{
			FProperty* Property = *PropIt;

			// Stop processing properties after reaching the stopped base class
			if ( IsFieldFromDeniedClass(Property) )
			{
				break;
			}

			if (Args.OnCanAcceptPropertyOrChildren.IsBound() && Args.OnCanAcceptPropertyOrChildren.Execute(Property) == false)
			{
				continue;
			}

			if (SkeletonClass)
			{
				if (!UEdGraphSchema_K2::CanUserKismetAccessVariable(Property, SkeletonClass, UEdGraphSchema_K2::CannotBeDelegate))
				{
					continue;
				}
			}

			// Also ignore advanced properties
			if (Property->HasAnyPropertyFlags(CPF_AdvancedDisplay | CPF_EditorOnly))
			{
				continue;
			}

			Pred(Property);
		}
	}
}

bool SPropertyBinding::HasBindableProperties(UStruct* InStruct) const
{
	TSet<UStruct*> VisitedStructs;
	return HasBindablePropertiesRecursive(InStruct, VisitedStructs, 0);
}

bool SPropertyBinding::HasBindablePropertiesRecursive(UStruct* InStruct, TSet<UStruct*>& VisitedStructs, const int32 RecursionDepth) const
{
	if (VisitedStructs.Contains(InStruct))
	{
		// If we have visited this struct already and it had bindable properties, it has been accounted for already.
		// Returning false to allow early exit.
		return false;
	}
	VisitedStructs.Add(InStruct);
	
	// Arbitrary cut off to avoid infinite loops.
	if (RecursionDepth > 10)
	{
		return false;
	}
	
	int32 BindableCount = 0;
	ForEachBindableProperty(InStruct, [this, &BindableCount, &VisitedStructs, RecursionDepth] (FProperty* Property)
	{
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);

		if(Args.OnCanBindProperty.Execute(Property))
		{
			BindableCount++;
			return;
		}

		if(Args.bAllowArrayElementBindings && ArrayProperty != nullptr && Args.OnCanBindProperty.Execute(ArrayProperty->Inner))
		{
			BindableCount++;
			return;
		}

		FProperty* InnerProperty = Property;
		if(Args.bAllowArrayElementBindings && ArrayProperty != nullptr)
		{
			InnerProperty = ArrayProperty->Inner;
		}

		FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InnerProperty);
		FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty);

		UStruct* Struct = nullptr;
		UClass* Class = nullptr;

		if ( ObjectProperty )
		{
			Struct = Class = ObjectProperty->PropertyClass;
		}
		else if ( StructProperty )
		{
			Struct = StructProperty->Struct;
		}

		// Recurse into a struct, except if it is the same type as the one we're binding.
		if (Struct && HasBindablePropertiesRecursive(Struct, VisitedStructs, RecursionDepth + 1))
		{
			if (Class)
			{
				// Ignore any subobject properties that are not bindable.
				// Also ignore any class that is explicitly on the deny list.
				if ( IsClassDenied(Class) || (Args.OnCanBindToSubObjectClass.IsBound() && Args.OnCanBindToSubObjectClass.Execute(Class)))
				{
					return;
				}
			}

			if (Args.bAllowArrayElementBindings && ArrayProperty != nullptr)
			{
				BindableCount++;
			}
			else if (Args.bAllowStructMemberBindings)
			{
				BindableCount++;
			}
		}
	});

	if(UClass* Class = Cast<UClass>(InStruct))
	{
		ForEachBindableFunction(Class, [this, &BindableCount, &VisitedStructs, RecursionDepth](const FFunctionInfo& Info)
		{
			FProperty* ReturnProperty = Info.Function->GetReturnProperty();
			
			// We can get here if we accept non-leaf UObject functions, so if so we need to check the return value for compatibility
			if(!Args.bAllowUObjectFunctions || Args.OnCanBindProperty.Execute(ReturnProperty))
			{
				BindableCount++;
			}

			// Only show bindable subobjects, structs and variables if we're generating pure bindings.
			if(Args.bGeneratePureBindings)
			{
				if(FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(ReturnProperty))
				{
					HasBindablePropertiesRecursive(ObjectPropertyBase->PropertyClass, VisitedStructs, RecursionDepth + 1);
				}
				else if(FStructProperty* StructProperty = CastField<FStructProperty>(ReturnProperty))
				{
					HasBindablePropertiesRecursive(StructProperty->Struct, VisitedStructs, RecursionDepth + 1);
				}
			}
		});
	}
	
	return BindableCount > 0;
}

TSharedRef<SWidget> SPropertyBinding::OnGenerateDelegateMenu()
{
	// The menu itself is be searchable.
	const bool bSearchableMenu = true;

	// The menu are generated through reflection and sometime the API exposes some recursivity (think about a Widget returning it parent which is also a Widget). Just by reflection
	// it is not possible to determine when the root object is reached. It needs a kind of simulation which is not implemented. Also, even if the recursivity was correctly handled, the possible
	// permutations tend to grow exponentially. Until a clever solution is found, the simple approach is to disable recursively searching those menus. User can still search the current one though.
	const bool bRecursivelySearchableMenu = false;

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr, Args.MenuExtender, /*bCloseSelfOnly*/false, &FCoreStyle::Get(), bSearchableMenu, NAME_None, bRecursivelySearchableMenu);

	MenuBuilder.BeginSection("BindingActions", LOCTEXT("Bindings", "Bindings"));

	if ( CanRemoveBinding() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("RemoveBinding", "Remove Binding"),
			LOCTEXT("RemoveBindingToolTip", "Removes the current binding"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Cross"),
			FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleRemoveBinding))
			);
	}

	if(Args.bAllowNewBindings && Args.BindableSignature != nullptr)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CreateBinding", "Create Binding"),
			LOCTEXT("CreateBindingToolTip", "Creates a new function on the widget blueprint that will return the binding data for this property."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Plus"),
			FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleCreateAndAddBinding))
			);
	}

	MenuBuilder.EndSection(); //CreateBinding

	// Properties
	{
		// Get the current skeleton class, think header for the blueprint.
		UBlueprintGeneratedClass* SkeletonClass = Blueprint ? Cast<UBlueprintGeneratedClass>(Blueprint->SkeletonGeneratedClass) : nullptr;

		if (SkeletonClass)
		{
			TArray<TSharedPtr<FBindingChainElement>> BindingChain;
			FillPropertyMenu(MenuBuilder, SkeletonClass, BindingChain);
		}
	}

	if (BindingContextStructs.Num() > 0)
	{
		auto MakeContextStructWidget = [this](const FBindingContextStruct& ContextStruct)
		{
			const FText DisplayText = ContextStruct.DisplayText.IsEmpty() ? ContextStruct.Struct->GetDisplayNameText() : ContextStruct.DisplayText;
			const FText ToolTipText = ContextStruct.TooltipText.IsEmpty() ? DisplayText : ContextStruct.TooltipText;

			const FSlateBrush* Icon = ContextStruct.Icon;
			FLinearColor IconColor = FLinearColor::White;
			if (Icon == nullptr)
			{
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

				FEdGraphPinType PinType;

				if (UClass* Class = Cast<UClass>(ContextStruct.Struct))
				{
					PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
					PinType.PinSubCategory = NAME_None;
					PinType.PinSubCategoryObject = Class;
				}
				else if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(ContextStruct.Struct))
				{
					PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
					PinType.PinSubCategory = NAME_None;
					PinType.PinSubCategoryObject = ScriptStruct;
				}
				Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true); 
				IconColor = Schema->GetPinTypeColor(PinType);
			}
			
			return SNew(SHorizontalBox)
				.ToolTipText(ToolTipText)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(18.0f, 0.0f))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(1.0f, 0.0f)
				[
					SNew(SImage)
					.Image(Icon)
					.ColorAndOpacity(IconColor)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(DisplayText)
				];
		};

		bool bSectionOpened = false;
		FText CurrentSection = FText::GetEmpty();
		
		for (int32 i = 0; i < BindingContextStructs.Num(); i++)
		{
			FBindingContextStruct& ContextStruct = BindingContextStructs[i];

			if (!ContextStruct.Section.IdenticalTo(CurrentSection))
			{
				if (bSectionOpened)
				{
					MenuBuilder.EndSection();
				}

				CurrentSection = ContextStruct.Section;
				MenuBuilder.BeginSection(FName(), CurrentSection);
				bSectionOpened = true;
			}
			
			// Make first chain element representing the index in the context array.
			TArray<TSharedPtr<FBindingChainElement>> BindingChain;
			BindingChain.Emplace(MakeShared<FBindingChainElement>(nullptr, i));

			if (Args.OnCanBindToContextStruct.IsBound() && Args.OnCanBindToContextStruct.Execute(ContextStruct.Struct))
			{
				// If the struct can be be bound to directly, create action for that. 
				MenuBuilder.AddMenuEntry(
					FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddBinding, BindingChain)),
					MakeContextStructWidget(ContextStruct));
			}
			else if (HasBindableProperties(ContextStruct.Struct))
			{
				// Show struct properties.
				MenuBuilder.AddSubMenu(
					MakeContextStructWidget(ContextStruct),
					FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, ContextStruct.Struct, BindingChain));
			}
		}

		if (bSectionOpened)
		{
			MenuBuilder.EndSection();
		}

	}

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetCachedDisplayMetrics(DisplayMetrics);

	return
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.MaxHeight(DisplayMetrics.PrimaryDisplayHeight * 0.5)
		[
			MenuBuilder.MakeWidget()
		];
}

void SPropertyBinding::FillPropertyMenu(FMenuBuilder& MenuBuilder, UStruct* InOwnerStruct, TArray<TSharedPtr<FBindingChainElement>> InBindingChain)
{
	auto MakeArrayElementPropertyWidget = [this](FProperty* InProperty, const TArray<TSharedPtr<FBindingChainElement>>& InNewBindingChain)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(InProperty, PinType);
		
		TSharedPtr<FBindingChainElement> LeafElement = InNewBindingChain.Last();

		return SNew(SHorizontalBox)
			.ToolTipText(InProperty->GetToolTipText())
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(18.0f, 0.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FBlueprintEditorUtils::GetIconFromPin(PinType, true))
				.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InProperty->GetFName()))
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.Padding(2.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(24.0f)
				[
					SNew(SNumericEntryBox<int32>)
					.ToolTipText(LOCTEXT("ArrayIndex", "Array Index"))
					.Value_Lambda([LeafElement](){ return LeafElement->ArrayIndex == INDEX_NONE ? TOptional<int32>() : TOptional<int32>(LeafElement->ArrayIndex); })
					.OnValueCommitted(this, &SPropertyBinding::HandleSetBindingArrayIndex, InProperty, InNewBindingChain)
				]
			];
	};

	auto MakeArrayElementEntry = [this, &MenuBuilder, &MakeArrayElementPropertyWidget](FProperty* InProperty, const TArray<TSharedPtr<FBindingChainElement>>& InNewBindingChain)
	{
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddBinding, InNewBindingChain)),
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				MakeArrayElementPropertyWidget(InProperty, InNewBindingChain)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(15.0f, 0.0f))
			]);
	};

	auto MakePropertyWidget = [this](FProperty* InProperty)
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(InProperty, PinType);

		FText PropertyDisplayName;
		if(InProperty->IsNative())
		{
			if(const FString* ScriptNamePtr = InProperty->FindMetaData("ScriptName"))
			{
				PropertyDisplayName = FText::FromString(*ScriptNamePtr);
			}
			else
			{
				PropertyDisplayName = FText::FromString(InProperty->GetName());
			}
		}
		else
		{
			PropertyDisplayName = FText::FromString(InProperty->GetDisplayNameText().ToString());
		}
		
		return SNew(SHorizontalBox)
			.ToolTipText(InProperty->GetToolTipText())
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(18.0f, 0.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FBlueprintEditorUtils::GetIconFromPin(PinType, true))
				.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(PropertyDisplayName)
			];
	};

	auto MakePropertyEntry = [this, &MenuBuilder, &MakePropertyWidget](FProperty* InProperty, const TArray<TSharedPtr<FBindingChainElement>>& InNewBindingChain)
	{
		MenuBuilder.AddMenuEntry(
			FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddBinding, InNewBindingChain)),
			MakePropertyWidget(InProperty));
	};

	auto MakeFunctionWidget = [this](const FFunctionInfo& Info)
	{
		static FName FunctionIcon(TEXT("GraphEditor.Function_16x"));

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		FEdGraphPinType PinType;
		if(FProperty* ReturnProperty = Info.Function->GetReturnProperty())
		{
			Schema->ConvertPropertyToPinType(ReturnProperty, PinType);
		}

		return SNew(SHorizontalBox)
			.ToolTipText(FText::FromString(Info.Tooltip))
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(18.0f, 0.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(1.0f, 0.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(FunctionIcon))
				.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Info.DisplayName)
			];
	};

	//---------------------------------------
	// Function Bindings

	if (Args.bAllowFunctionBindings)
	{
		if ( UClass* OwnerClass = Cast<UClass>(InOwnerStruct) )
		{
			static FName FunctionIcon(TEXT("GraphEditor.Function_16x"));

			MenuBuilder.BeginSection("Functions", LOCTEXT("Functions", "Functions"));
			{
				ForEachBindableFunction(OwnerClass, [this, &InBindingChain, &MenuBuilder, &MakeFunctionWidget] (const FFunctionInfo& Info) 
				{
					TArray<TSharedPtr<FBindingChainElement>> NewBindingChain(InBindingChain);
					NewBindingChain.Emplace(MakeShared<FBindingChainElement>(Info.Function));

					FProperty* ReturnProperty = Info.Function->GetReturnProperty();
					// We can get here if we accept non-leaf UObject functions, so if so we need to check the return value for compatibility
					if(!Args.bAllowUObjectFunctions || Args.OnCanBindProperty.Execute(ReturnProperty))
					{
						MenuBuilder.AddMenuEntry(
							FUIAction(FExecuteAction::CreateSP(this, &SPropertyBinding::HandleAddBinding, NewBindingChain)),
							MakeFunctionWidget(Info));
					}

					// Only show bindable subobjects, structs and variables if we're generating pure bindings.
					if(Args.bGeneratePureBindings)
					{
						if(FObjectPropertyBase* ObjectPropertyBase = CastField<FObjectPropertyBase>(ReturnProperty))
						{
							if (HasBindableProperties(ObjectPropertyBase->PropertyClass))
							{
								MenuBuilder.AddSubMenu(
									MakeFunctionWidget(Info),
									FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, static_cast<UStruct*>(ObjectPropertyBase->PropertyClass), NewBindingChain));
							}
						}
						else if(FStructProperty* StructProperty = CastField<FStructProperty>(ReturnProperty))
						{
							if (HasBindableProperties(StructProperty->Struct))
							{
								MenuBuilder.AddSubMenu(
									MakeFunctionWidget(Info),
									FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, static_cast<UStruct*>(StructProperty->Struct), NewBindingChain));
							}
						}
					}
				});
			}
			MenuBuilder.EndSection(); //Functions
		}
	}

	//---------------------------------------
	// Property Bindings

	// Only show bindable subobjects and variables if we're generating pure bindings.
	if ( Args.bGeneratePureBindings && Args.bAllowPropertyBindings )
	{
		FProperty* BindingProperty = nullptr;
		if(Args.BindableSignature != nullptr)
		{
			BindingProperty = Args.BindableSignature->GetReturnProperty();
		}
		else
		{
			BindingProperty = Args.Property;
		}

		// Find the binder that can handle the delegate return type, don't bother allowing people 
		// to look for bindings that we don't support
		if ( Args.OnCanBindProperty.IsBound() && Args.OnCanBindProperty.Execute(BindingProperty) )
		{
			UStruct* BindingStruct = nullptr;
			FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(BindingProperty);
			FStructProperty* StructProperty = CastField<FStructProperty>(BindingProperty);
			if (ObjectProperty)
			{
				BindingStruct = ObjectProperty->PropertyClass;
			}
			else if (StructProperty)
			{
				BindingStruct = StructProperty->Struct;
			}

			MenuBuilder.BeginSection("Properties", LOCTEXT("Properties", "Properties"));
			{
				ForEachBindableProperty(InOwnerStruct, [this, &InBindingChain, BindingStruct, &MenuBuilder, &MakeArrayElementPropertyWidget, &MakePropertyWidget, &MakePropertyEntry, &MakeArrayElementEntry] (FProperty* Property)
				{
					TArray<TSharedPtr<FBindingChainElement>> NewBindingChain(InBindingChain);
					NewBindingChain.Emplace(MakeShared<FBindingChainElement>(Property));

					if(Args.OnCanBindProperty.Execute(Property))
					{
						MakePropertyEntry(Property, NewBindingChain);
					}

					FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);

					if(Args.bAllowArrayElementBindings && ArrayProperty != nullptr && Args.OnCanBindProperty.Execute(ArrayProperty->Inner))
					{
						TArray<TSharedPtr<FBindingChainElement>> NewArrayElementBindingChain(InBindingChain);
						NewArrayElementBindingChain.Emplace(MakeShared<FBindingChainElement>(Property));
						NewArrayElementBindingChain.Last()->ArrayIndex = 0;

						MakeArrayElementEntry(ArrayProperty->Inner, NewArrayElementBindingChain);
					}

					FProperty* InnerProperty = Property;
					if(Args.bAllowArrayElementBindings && ArrayProperty != nullptr)
					{
						InnerProperty = ArrayProperty->Inner;
					}

					FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InnerProperty);
					FStructProperty* StructProperty = CastField<FStructProperty>(InnerProperty);

					UStruct* Struct = nullptr;
					UClass* Class = nullptr;

					if ( ObjectProperty )
					{
						Struct = Class = ObjectProperty->PropertyClass;
					}
					else if ( StructProperty )
					{
						Struct = StructProperty->Struct;
					}

					// Recurse into a struct if it has some properties we can bind to.
					if (Struct)
					{
						if (Class)
						{
							// Ignore any subobject properties that are not bindable.
							// Also ignore any class that is explicitly on the deny list.
							if ( IsClassDenied(Class) || (Args.OnCanBindToSubObjectClass.IsBound() && Args.OnCanBindToSubObjectClass.Execute(Class)))
							{
								return;
							}
						}

						if (HasBindableProperties(Struct))
						{
							if (Args.bAllowArrayElementBindings && ArrayProperty != nullptr)
							{
								TArray<TSharedPtr<FBindingChainElement>> NewArrayElementBindingChain(InBindingChain);
								NewArrayElementBindingChain.Emplace(MakeShared<FBindingChainElement>(Property));
								NewArrayElementBindingChain.Last()->ArrayIndex = 0;

								MenuBuilder.AddSubMenu(
									MakeArrayElementPropertyWidget(ArrayProperty->Inner, NewArrayElementBindingChain),
									FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, Struct, NewArrayElementBindingChain)
									);
							}
							else if (Args.bAllowStructMemberBindings)
							{
								MenuBuilder.AddSubMenu(
									MakePropertyWidget(Property),
									FNewMenuDelegate::CreateSP(this, &SPropertyBinding::FillPropertyMenu, Struct, NewBindingChain));
							}
						}
					}
				});
			}
			MenuBuilder.EndSection(); //Properties
		}
	}

	// Add 'none' entry only if we just have the search block in the builder
	if ( MenuBuilder.GetMultiBox()->GetBlocks().Num() == 1 )
	{
		MenuBuilder.BeginSection("None", InOwnerStruct->GetDisplayNameText());
		MenuBuilder.AddWidget(SNew(STextBlock).Text(LOCTEXT("None", "None")), FText::GetEmpty());
		MenuBuilder.EndSection(); //None
	}
}

const FSlateBrush* SPropertyBinding::GetCurrentBindingImage() const
{
	if(Args.CurrentBindingImage.IsSet())
	{
		return Args.CurrentBindingImage.Get();
	}

	return nullptr;
}

FText SPropertyBinding::GetCurrentBindingText() const
{
	if(Args.CurrentBindingText.IsSet())
	{
		return Args.CurrentBindingText.Get();
	}

	return LOCTEXT("Bind", "Bind");
}

FText SPropertyBinding::GetCurrentBindingToolTipText() const
{
	if(Args.CurrentBindingToolTipText.IsSet())
	{
		return Args.CurrentBindingToolTipText.Get();
	}

	return GetCurrentBindingText();
}

FSlateColor SPropertyBinding::GetCurrentBindingColor() const
{
	if(Args.CurrentBindingColor.IsSet())
	{
		return Args.CurrentBindingColor.Get();
	}

	return FLinearColor(0.25f, 0.25f, 0.25f);
}

bool SPropertyBinding::CanRemoveBinding()
{
	if(Args.OnCanRemoveBinding.IsBound())
	{
		return Args.OnCanRemoveBinding.Execute(PropertyName);
	}

	return false;
}

void SPropertyBinding::HandleRemoveBinding()
{
	if(Args.OnRemoveBinding.IsBound())
	{
		const FScopedTransaction Transaction(LOCTEXT("UnbindDelegate", "Remove Binding"));

		Args.OnRemoveBinding.Execute(PropertyName);
	}
}

void SPropertyBinding::HandleAddBinding(TArray<TSharedPtr<FBindingChainElement>> InBindingChain)
{
	if(Args.OnAddBinding.IsBound())
	{
		const FScopedTransaction Transaction(LOCTEXT("BindDelegate", "Set Binding"));

		TArray<FBindingChainElement> BindingChain;
		Algo::Transform(InBindingChain, BindingChain, [](TSharedPtr<FBindingChainElement> InElement)
		{
			return *InElement.Get();
		});
		Args.OnAddBinding.Execute(PropertyName, BindingChain);
	}
}

void SPropertyBinding::HandleSetBindingArrayIndex(int32 InArrayIndex, ETextCommit::Type InCommitType, FProperty* InProperty, TArray<TSharedPtr<FBindingChainElement>> InBindingChain)
{
	InBindingChain.Last()->ArrayIndex = InArrayIndex;

	// If the user hit enter on a compatible property, assume they want to accept
	if(Args.OnCanBindProperty.Execute(InProperty) && InCommitType == ETextCommit::OnEnter)
	{
		HandleAddBinding(InBindingChain);
	}
}

void SPropertyBinding::HandleCreateAndAddBinding()
{
	if (!Blueprint)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateDelegate", "Create Binding"));

	Blueprint->Modify();

	FString Pre = Args.bGeneratePureBindings ? FString(TEXT("Get")) : FString(TEXT("On"));

	FString WidgetName;
	if(Args.OnGenerateBindingName.IsBound())
	{
		WidgetName = Args.OnGenerateBindingName.Execute();
	}

	FString Post = (PropertyName != NAME_None) ? PropertyName.ToString() : FString();
	Post.RemoveFromStart(TEXT("On"));
	Post.RemoveFromEnd(TEXT("Event"));

	// Create the function graph.
	FString FunctionName = Pre + WidgetName + Post;
	UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, 
		FBlueprintEditorUtils::FindUniqueKismetName(Blueprint, FunctionName),
		UEdGraph::StaticClass(),
		UEdGraphSchema_K2::StaticClass());
	
	const bool bUserCreated = true;
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, bUserCreated, Args.BindableSignature);

	UFunction* Function = Blueprint->SkeletonGeneratedClass->FindFunctionByName(FunctionGraph->GetFName());
	check(Function);

	Args.OnNewFunctionBindingCreated.ExecuteIfBound(FunctionGraph, Function);
	
	// Add the binding to the blueprint
	TArray<TSharedPtr<FBindingChainElement>> BindingPath;
	BindingPath.Emplace(MakeShared<FBindingChainElement>(Function));

	HandleAddBinding(BindingPath);

	// Only mark bindings as pure that need to be.
	if ( Args.bGeneratePureBindings )
	{
		const UEdGraphSchema_K2* Schema_K2 = Cast<UEdGraphSchema_K2>(FunctionGraph->GetSchema());
		Schema_K2->AddExtraFunctionFlags(FunctionGraph, FUNC_BlueprintPure);
	}

	HandleGotoBindingClicked();
}

EVisibility SPropertyBinding::GetGotoBindingVisibility() const
{
	if(Args.OnCanGotoBinding.IsBound())
	{
		if(Args.OnCanGotoBinding.Execute(PropertyName))
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply SPropertyBinding::HandleGotoBindingClicked()
{
	if(Args.OnGotoBinding.IsBound())
	{
		if(Args.OnGotoBinding.Execute(PropertyName))
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
