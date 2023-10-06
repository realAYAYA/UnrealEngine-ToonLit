// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewBinding.h"

#include "Blueprint/WidgetTree.h"
#include "Bindings/MVVMConversionFunctionHelper.h"
#include "K2Node_CallFunction.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMWidgetBlueprintExtension_View.h"

#include "MVVMFunctionGraphHelper.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintViewBinding)

#define LOCTEXT_NAMESPACE "MVVMBlueprintViewBinding"

void FMVVMBlueprintViewConversionPath::GenerateWrapper(UBlueprint* Blueprint)
{
	if (DestinationToSourceConversion)
	{
		DestinationToSourceConversion->GetOrCreateWrapperGraph(Blueprint);
	}
	if (SourceToDestinationConversion)
	{
		SourceToDestinationConversion->GetOrCreateWrapperGraph(Blueprint);
	}
}

void FMVVMBlueprintViewConversionPath::SavePinValues(UBlueprint* Blueprint)
{
	if (DestinationToSourceConversion)
	{
		DestinationToSourceConversion->SavePinValues(Blueprint);
	}
	if (SourceToDestinationConversion)
	{
		SourceToDestinationConversion->SavePinValues(Blueprint);
	}
}

void FMVVMBlueprintViewConversionPath::DeprecateViewConversionFunction(UBlueprint* Blueprint)
{
	auto Deprecate = [Blueprint](FName& Wrapper, FMemberReference& Reference, TObjectPtr<UMVVMBlueprintViewConversionFunction>& ConversionFunction)
	{
		if (!Wrapper.IsNone())
		{
			TObjectPtr<UEdGraph>* GraphPtr = Blueprint->FunctionGraphs.FindByPredicate([Wrapper](const UEdGraph* Other) { return Other->GetFName() == Wrapper; });
			if (GraphPtr)
			{
				ConversionFunction = NewObject<UMVVMBlueprintViewConversionFunction>(Blueprint);
				ConversionFunction->InitializeFromWrapperGraph(Blueprint, *GraphPtr);
			}
		}
		else if (!Reference.GetMemberName().IsNone())
		{
			ConversionFunction = NewObject<UMVVMBlueprintViewConversionFunction>(Blueprint);
			ConversionFunction->InitializeFromMemberReference(Blueprint, Reference);
		}
		Wrapper = FName();
		Reference = FMemberReference();
	};

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Deprecate(DestinationToSourceWrapper_DEPRECATED, DestinationToSourceFunction_DEPRECATED, DestinationToSourceConversion);
	Deprecate(SourceToDestinationWrapper_DEPRECATED, SourceToDestinationFunction_DEPRECATED, SourceToDestinationConversion);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FName FMVVMBlueprintViewBinding::GetFName() const
{
	return *BindingId.ToString(EGuidFormats::DigitsWithHyphensLower);
}

namespace UE::MVVM::Private
{
	FText GetDisplayNameForField(const FMVVMConstFieldVariant& Field)
	{
		if (!Field.IsEmpty())
		{
			if (Field.IsProperty())
			{
				return Field.GetProperty()->GetDisplayNameText();
			}
			if (Field.IsFunction())
			{
				return Field.GetFunction()->GetDisplayNameText();
			}
		}
		return LOCTEXT("None", "<None>");
	}

	FString GetDisplayNameForWidget(const UWidgetBlueprint* WidgetBlueprint, const FName& WidgetName)
	{
		if (UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree)
		{
			if (UWidget* FoundWidget = WidgetTree->FindWidget(WidgetName))
			{
				return FoundWidget->GetDisplayLabel().IsEmpty() ? WidgetName.ToString() : FoundWidget->GetDisplayLabel();
			}
		}

		return WidgetName.IsNone() ? TEXT("<none>") : WidgetName.ToString();
	}

	void AppendViewModelPathString(const UWidgetBlueprint* WidgetBlueprint, const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& ViewModelPath, FStringBuilderBase& PathBuilder, FStringBuilderBase& FunctionKeywordsBuilder, bool bUseDisplayName = false, bool bAppendFunctionKeywords = false)
	{
		if (ViewModelPath.IsEmpty())
		{
			PathBuilder << TEXT("<none>");
			return;
		}

		const FMVVMBlueprintViewModelContext* ViewModel = BlueprintView != nullptr ? BlueprintView->FindViewModel(ViewModelPath.GetViewModelId()) : nullptr;
		if (ViewModel)
		{
			PathBuilder << ViewModel->GetDisplayName().ToString();
		}
		else if (ViewModelPath.IsFromWidget())
		{
			if (bUseDisplayName)
			{
				PathBuilder << GetDisplayNameForWidget(WidgetBlueprint, ViewModelPath.GetWidgetName());
			}
			else
			{
				PathBuilder << ViewModelPath.GetWidgetName();
			}
		}
		else
		{
			PathBuilder << TEXT("<none>");
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> Fields = ViewModelPath.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
		for (const UE::MVVM::FMVVMConstFieldVariant& Field : Fields)
		{
			PathBuilder << TEXT(".");
			if (bUseDisplayName)
			{
				PathBuilder << GetDisplayNameForField(Field).ToString();
			}
			else
			{
				PathBuilder << Field.GetName().ToString();
			}

			if (Field.IsFunction() && bAppendFunctionKeywords)
			{
				FString FunctionKeywords = Field.GetFunction()->GetMetaData(FBlueprintMetadata::MD_FunctionKeywords);
				if (!FunctionKeywords.IsEmpty())
				{
					FunctionKeywordsBuilder << TEXT(".");
					FunctionKeywordsBuilder << FunctionKeywords;
				}
			}
		}
	}
	
	void AppendWidgetPathString(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& WidgetPath, FStringBuilderBase& PathBuilder, FStringBuilderBase& FunctionKeywordsBuilder, bool bUseDisplayName = false, bool bAppendFunctionKeywords = false)
	{
		if (WidgetBlueprint == nullptr)
		{
			PathBuilder << TEXT("<none>");
			return;
		}

		if (WidgetPath.GetWidgetName().IsNone() && !bUseDisplayName)
		{
			PathBuilder << TEXT("<none>");
		}
		else if (WidgetBlueprint->GetFName() == WidgetPath.GetWidgetName())
		{
			if (bUseDisplayName)
			{
				PathBuilder << TEXT("self");
			}
			else
			{
				PathBuilder << WidgetBlueprint->GetName();
			}
		}
		else
		{
			if (bUseDisplayName)
			{
				PathBuilder << GetDisplayNameForWidget(WidgetBlueprint, WidgetPath.GetWidgetName());
			}
			else
			{
				PathBuilder << WidgetPath.GetWidgetName();
			}
		}

		TArray<UE::MVVM::FMVVMConstFieldVariant> Fields = WidgetPath.GetFields(WidgetBlueprint->SkeletonGeneratedClass);
		for (const UE::MVVM::FMVVMConstFieldVariant& Field : Fields)
		{
			PathBuilder << TEXT(".");
			if (bUseDisplayName)
			{
				PathBuilder << GetDisplayNameForField(Field).ToString();
			}
			else
			{
				PathBuilder << Field.GetName().ToString();
			}

			if (Field.IsFunction() && bAppendFunctionKeywords)
			{
				FString FunctionKeywords = Field.GetFunction()->GetMetaData(FBlueprintMetadata::MD_FunctionKeywords);
				if (!FunctionKeywords.IsEmpty())
				{
					FunctionKeywordsBuilder << TEXT(".");
					FunctionKeywordsBuilder << FunctionKeywords;
				}
			}
		}
	}

	FString GetBindingPathName(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bIsSource, FStringBuilderBase& FunctionKeywordsBuilder, bool bUseDisplayName = false, bool bAppendFunctionKeywords = false)
	{
		UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
		UMVVMBlueprintView* BlueprintView = ExtensionView->GetBlueprintView();

		TStringBuilder<256> NameBuilder;

		auto AddPath = [&](const FMVVMBlueprintPropertyPath& ArgumentPath)
		{
			if (ArgumentPath.IsFromViewModel())
			{
				AppendViewModelPathString(WidgetBlueprint, BlueprintView, ArgumentPath, NameBuilder, FunctionKeywordsBuilder, bUseDisplayName, bAppendFunctionKeywords);
			}
			else
			{
				AppendWidgetPathString(WidgetBlueprint, ArgumentPath, NameBuilder, FunctionKeywordsBuilder, bUseDisplayName, bAppendFunctionKeywords);
			}
		};

		if (UMVVMBlueprintViewConversionFunction* ViewConversionFunction = Binding.Conversion.GetConversionFunction(bIsSource))
		{
			TVariant<const UFunction*, TSubclassOf<UK2Node>> ConversionFunction = ViewConversionFunction->GetConversionFunction(WidgetBlueprint->SkeletonGeneratedClass);
			if (ConversionFunction.IsType<const UFunction*>())
			{
				if (const UFunction* ConversionFunctionPtr = ConversionFunction.Get<const UFunction*>())
				{
					if (bUseDisplayName)
					{
						NameBuilder << ConversionFunctionPtr->GetDisplayNameText().ToString();
					}
					else 
					{
						NameBuilder << ConversionFunctionPtr->GetFName();
					}

					if (bAppendFunctionKeywords)
					{
						FString FunctionKeywords = ConversionFunctionPtr->GetMetaData(FBlueprintMetadata::MD_FunctionKeywords);
						if (!FunctionKeywords.IsEmpty())
						{
							FunctionKeywordsBuilder << TEXT(".");
							FunctionKeywordsBuilder << FunctionKeywords;
						}
					}
				}
				else
				{
					NameBuilder << TEXT("<ERROR>");
				}
			}
			else
			{
				TSubclassOf<UK2Node> ConversionFunctionNode = ConversionFunction.Get<TSubclassOf<UK2Node>>();
				if (ConversionFunctionNode.Get())
				{
					if (bUseDisplayName)
					{
						NameBuilder << ConversionFunctionNode->GetDisplayNameText().ToString();
					}
					else
					{
						NameBuilder << ConversionFunctionNode->GetFName();
					}
				}
				else
				{
					NameBuilder << TEXT("<ERROR>");
				}
			}

			// AddPins
			{
				NameBuilder << TEXT("(");

				if (ViewConversionFunction->NeedsWrapperGraph())
				{
					bool bFirst = true;
					for (const FMVVMBlueprintPin& Pin : ViewConversionFunction->GetPins())
					{
						if (!bFirst)
						{
							NameBuilder << TEXT(", ");
						}

						if (Pin.UsedPathAsValue())
						{
							AddPath(Pin.GetPath());
						}
						else
						{
							NameBuilder << Pin.GetValueAsString(WidgetBlueprint->SkeletonGeneratedClass);
						}

						bFirst = false;
					}
				}
				else
				{
					const FMVVMBlueprintPropertyPath& Path = bIsSource ? Binding.SourcePath : Binding.DestinationPath;
					AddPath(Path);
				}

				NameBuilder << TEXT(")");
			}
		}
		else
		{
			const FMVVMBlueprintPropertyPath& PropertyPath = bIsSource ? Binding.SourcePath : Binding.DestinationPath;
			AddPath(PropertyPath);
		}

		FString Name = NameBuilder.ToString();
		if (Name.IsEmpty())
		{
			Name = TEXT("<none>");
		}

		return Name;
	}
}

FString FMVVMBlueprintViewBinding::GetDisplayNameString(const UWidgetBlueprint* WidgetBlueprint, bool bUseDisplayName) const
{
	check(WidgetBlueprint);

	TStringBuilder<256> NameBuilder;
	TStringBuilder<2> FunctionKeywordsBuilder; // This is only passed to GetBindingPathName but never used in this function.

	NameBuilder << UE::MVVM::Private::GetBindingPathName(WidgetBlueprint, *this, false, FunctionKeywordsBuilder, bUseDisplayName);

	if (BindingType == EMVVMBindingMode::TwoWay)
	{
		NameBuilder << TEXT(" <-> ");
	}
	else if (UE::MVVM::IsForwardBinding(BindingType))
	{
		NameBuilder << TEXT(" <- ");
	}
	else if (UE::MVVM::IsBackwardBinding(BindingType))
	{
		NameBuilder << TEXT(" -> ");
	}
	else
	{
		NameBuilder << TEXT(" ??? "); // shouldn't happen
	}

	NameBuilder << UE::MVVM::Private::GetBindingPathName(WidgetBlueprint, *this, true, FunctionKeywordsBuilder, bUseDisplayName);

	return NameBuilder.ToString();
}

FString FMVVMBlueprintViewBinding::GetSearchableString(const UWidgetBlueprint* WidgetBlueprint) const
{
	check(WidgetBlueprint);

	FString SearchString;

	// Get the binding string with variable names.
	SearchString.Append(GetDisplayNameString(WidgetBlueprint, false));

	// Remove the extra formatting that we don't need for search.
	// We will include the formatted string as well in the second call to GetBindingPathName.
	SearchString.ReplaceInline(TEXT(" "), TEXT(""));
	SearchString.ReplaceInline(TEXT(")"), TEXT(""));
	SearchString.ReplaceInline(TEXT("("), TEXT("."));
	SearchString.ReplaceInline(TEXT(","), TEXT("."));

	SearchString.Append(TEXT("."));

	// Get the binding string with display names.
	SearchString.Append(GetDisplayNameString(WidgetBlueprint, true));

	// Create the function keywords string.
	TStringBuilder<128> FunctionKeywordsBuilder;
	UE::MVVM::Private::GetBindingPathName(WidgetBlueprint, *this, false, FunctionKeywordsBuilder, false, true);
	UE::MVVM::Private::GetBindingPathName(WidgetBlueprint, *this, true, FunctionKeywordsBuilder, false, true);

	SearchString.Append(FunctionKeywordsBuilder);

	return SearchString;
}

#undef LOCTEXT_NAMESPACE