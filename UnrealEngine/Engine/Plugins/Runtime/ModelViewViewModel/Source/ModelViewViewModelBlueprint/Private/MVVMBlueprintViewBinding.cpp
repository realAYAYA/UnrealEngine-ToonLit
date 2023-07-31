// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVMBlueprintViewBinding.h"

#include "Bindings/MVVMConversionFunctionHelper.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "MVVMBlueprintView.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintExtension.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MVVMBlueprintViewBinding)

FName FMVVMBlueprintViewBinding::GetFName() const
{
	return *BindingId.ToString(EGuidFormats::DigitsWithHyphensLower);
}

namespace UE::MVVM::Private
{
	void AppendViewModelPathString(const UMVVMBlueprintView* BlueprintView, const FMVVMBlueprintPropertyPath& ViewModelPath, FStringBuilderBase& PathBuilder)
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
		else
		{
			PathBuilder << TEXT("<none>");
		}

		PathBuilder << TEXT(".");

		const FString PropertyPath = ViewModelPath.GetBasePropertyPath();
		if (!PropertyPath.IsEmpty())
		{
			PathBuilder << PropertyPath;
		}
		else
		{
			PathBuilder << TEXT("<none>");
		}
	}
	
	FString GetBindingViewModelName(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding)
	{
		UMVVMWidgetBlueprintExtension_View* ExtensionView = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
		UMVVMBlueprintView* BlueprintView = ExtensionView->GetBlueprintView();

		TStringBuilder<256> NameBuilder;

		if (UEdGraph* Graph = ConversionFunctionHelper::GetGraph(WidgetBlueprint, Binding, true))
		{
			if (const UK2Node_CallFunction* CallFunctionNode = ConversionFunctionHelper::GetFunctionNode(Graph))
			{
				NameBuilder << CallFunctionNode->GetFunctionName();
				NameBuilder << TEXT("(");

				bool bFirst = true;

				for (const UEdGraphPin* Pin : CallFunctionNode->Pins)
				{
					if (Pin->PinName == UEdGraphSchema_K2::PN_Self || 
						Pin->Direction != EGPD_Input)
					{
						continue;
					}

					if (!bFirst)
					{
						NameBuilder << TEXT(", ");
					}

					FMVVMBlueprintPropertyPath ArgumentPath = ConversionFunctionHelper::GetPropertyPathForArgument(WidgetBlueprint, CallFunctionNode, Pin->GetFName(), true);
					if (!ArgumentPath.IsEmpty())
					{
						AppendViewModelPathString(BlueprintView, ArgumentPath, NameBuilder);
					}
					else
					{
						NameBuilder << Pin->GetDefaultAsString();
					}

					bFirst = false;
				}

				NameBuilder << TEXT(")");
			}
			else
			{
				NameBuilder << TEXT("<error>");
			}
		}
		else if (!Binding.Conversion.SourceToDestinationFunction.GetMemberName().IsNone())
		{
			if (const UFunction* SourceToDestFunction = Binding.Conversion.SourceToDestinationFunction.ResolveMember<UFunction>(WidgetBlueprint->SkeletonGeneratedClass))
			{
				NameBuilder << SourceToDestFunction->GetName();
				NameBuilder << TEXT("(");
			
				AppendViewModelPathString(BlueprintView, Binding.ViewModelPath, NameBuilder);

				NameBuilder << TEXT(")");
			}
			else
			{
				NameBuilder << Binding.Conversion.SourceToDestinationFunction.GetMemberName();
				NameBuilder << TEXT("()");
			}
		}
		else if (!Binding.ViewModelPath.IsEmpty())
		{
			AppendViewModelPathString(BlueprintView, Binding.ViewModelPath, NameBuilder);
		}

		FString Name = NameBuilder.ToString();
		if (Name.IsEmpty())
		{
			Name = TEXT("<none>");
		}

		return Name;
	}

	void AppendWidgetPathString(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& WidgetPath, FStringBuilderBase& PathBuilder)
	{
		if (WidgetBlueprint == nullptr || WidgetPath.GetWidgetName().IsNone())
		{
			PathBuilder << TEXT("<none>");
		}
		else if (WidgetBlueprint->GetFName() == WidgetPath.GetWidgetName())
		{
			PathBuilder << TEXT("self");
		}
		else
		{
			PathBuilder << WidgetPath.GetWidgetName();
		}

		PathBuilder << TEXT(".");

		const FString PropertyPath = WidgetPath.GetBasePropertyPath();
		if (!PropertyPath.IsEmpty())
		{
			PathBuilder << PropertyPath;
		}
		else
		{
			PathBuilder << TEXT("<none>");
		}
	}

	FString GetBindingWidgetName(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding)
	{
		TStringBuilder<256> NameBuilder;

		if (UEdGraph* Graph = ConversionFunctionHelper::GetGraph(WidgetBlueprint, Binding, false))
		{
			if (const UK2Node_CallFunction* CallFunctionNode = ConversionFunctionHelper::GetFunctionNode(Graph))
			{
				NameBuilder << CallFunctionNode->GetFunctionName();
				NameBuilder << TEXT("(");

				bool bFirst = true;

				for (const UEdGraphPin* Pin : CallFunctionNode->Pins)
				{
					if (Pin->Direction == EGPD_Input)
					{
						if (!bFirst)
						{
							NameBuilder << TEXT(", ");
						}

						FMVVMBlueprintPropertyPath ArgumentPath = ConversionFunctionHelper::GetPropertyPathForArgument(WidgetBlueprint, CallFunctionNode, Pin->GetFName(), true);
						if (!ArgumentPath.IsEmpty())
						{
							AppendWidgetPathString(WidgetBlueprint, ArgumentPath, NameBuilder);
						}
						else
						{
							NameBuilder << Pin->GetDefaultAsString();
						}
					}

					bFirst = false;
				}

				NameBuilder << TEXT(")");
			}
			else
			{
				NameBuilder << TEXT("<error>");
			}
		}
		else if (!Binding.Conversion.DestinationToSourceFunction.GetMemberName().IsNone())
		{
			if (const UFunction* DestToSourceFunction = Binding.Conversion.DestinationToSourceFunction.ResolveMember<UFunction>(WidgetBlueprint->SkeletonGeneratedClass))
			{
				NameBuilder << DestToSourceFunction->GetName();
				NameBuilder << TEXT("(");
			
				AppendWidgetPathString(WidgetBlueprint, Binding.WidgetPath, NameBuilder);

				NameBuilder << TEXT(")");
			}
			else
			{
				NameBuilder << Binding.Conversion.SourceToDestinationFunction.GetMemberName();
				NameBuilder << TEXT("()");
			}
		}
		else if (!Binding.WidgetPath.IsEmpty())
		{
			AppendWidgetPathString(WidgetBlueprint, Binding.WidgetPath, NameBuilder);
		}

		FString Name = NameBuilder.ToString();
		if (Name.IsEmpty())
		{
			Name = TEXT("<none>");
		}

		return Name;
	}
}

FString FMVVMBlueprintViewBinding::GetDisplayNameString(const UWidgetBlueprint* WidgetBlueprint) const
{
	check(WidgetBlueprint);

	TStringBuilder<256> NameBuilder;

	NameBuilder << UE::MVVM::Private::GetBindingWidgetName(WidgetBlueprint, *this);

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

	NameBuilder << UE::MVVM::Private::GetBindingViewModelName(WidgetBlueprint, *this);

	return NameBuilder.ToString();
}
