// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectDetails.h"

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EdGraphSchema_K2.h"
#include "Editor/EditorEngine.h"
#include "Engine/Blueprint.h"
#include "Framework/SlateDelegates.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Layout/Margin.h"
#include "Math/NumericLimits.h"
#include "Misc/Attribute.h"
#include "Misc/CString.h"
#include "ObjectEditorUtils.h"
#include "Reflection/FunctionUtils.h"
#include "Settings/BlueprintEditorProjectSettings.h"
#include "SWarningOrErrorBox.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealNames.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "ObjectDetails"

TSharedRef<IDetailCustomization> FObjectDetails::MakeInstance()
{
	return MakeShareable(new FObjectDetails);
}

void FObjectDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AddExperimentalWarningCategory(DetailBuilder);
	AddCallInEditorMethods(DetailBuilder);
}

void FObjectDetails::AddExperimentalWarningCategory(IDetailLayoutBuilder& DetailBuilder)
{
	bool bBaseClassIsExperimental = false;
	bool bBaseClassIsEarlyAccess = false;
	FString MostDerivedDevelopmentClassName;
	FObjectEditorUtils::GetClassDevelopmentStatus(DetailBuilder.GetBaseClass(), bBaseClassIsExperimental, bBaseClassIsEarlyAccess, MostDerivedDevelopmentClassName);

	if (bBaseClassIsExperimental || bBaseClassIsEarlyAccess)
	{
		const FName CategoryName(TEXT("Warning"));
		const FText CategoryDisplayName = LOCTEXT("WarningCategoryDisplayName", "Warning");
		const FText WarningText = bBaseClassIsExperimental ? FText::Format( LOCTEXT("ExperimentalClassWarning", "Uses experimental class: {0}") , FText::FromString(MostDerivedDevelopmentClassName) )
			: FText::Format( LOCTEXT("EarlyAccessClassWarning", "Uses beta class {0}"), FText::FromString(MostDerivedDevelopmentClassName) );
		const FText SearchString = WarningText;

		IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(CategoryName, CategoryDisplayName, ECategoryPriority::Transform);

		FDetailWidgetRow& WarningRow = WarningCategory.AddCustomRow(SearchString)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.f, 4.f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(WarningText)
				]
			];
	}
}

static bool CanCallFunctionBasedOnParams(const UFunction* TestFunction)
{
	bool bCanCall = TestFunction->GetBoolMetaData(FBlueprintMetadata::MD_CallInEditor) && (TestFunction->ParmsSize == 0); // no params required, we can call it!

	// else - if the function only takes a world context object we can use the editor's
	// world context - but only if the blueprint is editor only:
	if (UClass* TestFunctionOwnerClass = TestFunction->GetOwnerClass())
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(TestFunctionOwnerClass->ClassGeneratedBy))
		{
			if (FBlueprintEditorUtils::IsEditorUtilityBlueprint(Blueprint) && Blueprint->BlueprintType == BPTYPE_FunctionLibrary)
			{
				using namespace UE::Reflection;
				return TestFunction->HasMetaData(FBlueprintMetadata::MD_WorldContext) &&
					DoesStaticFunctionSignatureMatch<void(TObjectPtr<UObject>)>(TestFunction);
			}
		}
	}

	return bCanCall;
}

void FObjectDetails::AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder)
{
	// metadata tag for defining sort order of function buttons within a Category
	static const FName NAME_DisplayPriority("DisplayPriority");

	const bool bDisallowEditorUtilityBlueprintFunctions = GetDefault<UBlueprintEditorProjectSettings>()->bDisallowEditorUtilityBlueprintFunctionsInDetailsView;

	// Get all of the functions we need to display (done ahead of time so we can sort them)
	TArray<UFunction*, TInlineAllocator<8>> CallInEditorFunctions;
	for (TFieldIterator<UFunction> FunctionIter(DetailBuilder.GetBaseClass(), EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
	{
		UFunction* TestFunction = *FunctionIter;

		if (CanCallFunctionBasedOnParams(TestFunction))
		{
			bool bAllowFunction = true;
			if (UClass* TestFunctionOwnerClass = TestFunction->GetOwnerClass())
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(TestFunctionOwnerClass->ClassGeneratedBy))
				{
					if (FBlueprintEditorUtils::IsEditorUtilityBlueprint(Blueprint))
					{
						// Skip Blutilities if disabled via project settings
						bAllowFunction = !bDisallowEditorUtilityBlueprintFunctions;
					}
				}
			}

			if (bAllowFunction)
			{
				const FName FunctionName = TestFunction->GetFName();
				if (!CallInEditorFunctions.FindByPredicate([&FunctionName](const UFunction* Func) { return Func->GetFName() == FunctionName; }))
				{
					CallInEditorFunctions.Add(*FunctionIter);
				}
			}
		}
	}

	if (CallInEditorFunctions.Num() > 0)
	{
		// Copy off the objects being customized so we can invoke a function on them later, removing any that are a CDO
		DetailBuilder.GetObjectsBeingCustomized(/*out*/ SelectedObjectsList);
		SelectedObjectsList.RemoveAllSwap([](TWeakObjectPtr<UObject> ObjPtr) { UObject* Obj = ObjPtr.Get(); return (Obj == nullptr) || Obj->HasAnyFlags(RF_ArchetypeObject); });

		if (SelectedObjectsList.Num() == 0)
		{
			// remove all non-static functions - no objects to call them on
			CallInEditorFunctions.RemoveAllSwap([](const UFunction* Function) { return !Function->HasAnyFunctionFlags(FUNC_Static);});

			if (CallInEditorFunctions.Num() == 0)
			{
				return;
			}
		}

		// Sort the functions by category and then by DisplayPriority meta tag, and then by name
		CallInEditorFunctions.Sort([](UFunction& A, UFunction& B)
		{
			const int32 CategorySort = A.GetMetaData(FBlueprintMetadata::MD_FunctionCategory).Compare(B.GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
			if (CategorySort != 0)
			{
				return (CategorySort <= 0);
			}
			else 
			{
				FString DisplayPriorityAStr = A.GetMetaData(NAME_DisplayPriority);
				int32 DisplayPriorityA = (DisplayPriorityAStr.IsEmpty() ? MAX_int32 : FCString::Atoi(*DisplayPriorityAStr));
				if (DisplayPriorityA == 0 && !FCString::IsNumeric(*DisplayPriorityAStr))
				{
					DisplayPriorityA = MAX_int32;
				}

				FString DisplayPriorityBStr = B.GetMetaData(NAME_DisplayPriority);
				int32 DisplayPriorityB = (DisplayPriorityBStr.IsEmpty() ? MAX_int32 : FCString::Atoi(*DisplayPriorityBStr));
				if (DisplayPriorityB == 0 && !FCString::IsNumeric(*DisplayPriorityBStr))
				{
					DisplayPriorityB = MAX_int32;
				}

				return (DisplayPriorityA == DisplayPriorityB) ? (A.GetName() <= B.GetName()) : (DisplayPriorityA <= DisplayPriorityB);
			}
		});

		struct FCategoryEntry
		{
			FName CategoryName;
			FName RowTag;
			TSharedPtr<SWrapBox> WrapBox;
			FTextBuilder FunctionSearchText;

			FCategoryEntry(FName InCategoryName)
				: CategoryName(InCategoryName)
			{
				WrapBox = SNew(SWrapBox)
					// Setting the preferred size here (despite using UseAllottedSize) is a workaround for an issue
					// when contained in a scroll box: prior to the first tick, the wrap box will use preferred size
					// instead of allotted, and if preferred size is set small, it will cause the box to wrap a lot and
					// request too much space from the scroll box. On next tick, SWrapBox is updated but the scroll box
					// does not realize that it needs to show more elements, until it is scrolled.
					// Setting a large value here means that the SWrapBox will request too little space prior to tick,
					// which will cause the scroll box to virtualize more elements at the start, but this is less broken.
					.PreferredSize(2000)
					.UseAllottedSize(true);
			}
		};

		// Build up a set of functions for each category, accumulating search text and buttons in a wrap box
		FName ActiveCategory;
		TArray<FCategoryEntry, TInlineAllocator<8>> CategoryList;
		for (UFunction* Function : CallInEditorFunctions)
		{
			FName FunctionCategoryName(NAME_Default);
			if (Function->HasMetaData(FBlueprintMetadata::FBlueprintMetadata::MD_FunctionCategory))
			{
				FunctionCategoryName = FName(*Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
			}

			if (FunctionCategoryName != ActiveCategory)
			{
				ActiveCategory = FunctionCategoryName;
				CategoryList.Emplace(FunctionCategoryName);
			}
			FCategoryEntry& CategoryEntry = CategoryList.Last();

			const FText ButtonCaption = UK2Node_CallFunction::GetUserFacingFunctionName(Function);
			FText FunctionTooltip = Function->GetToolTipText();
			if (FunctionTooltip.IsEmpty())
			{
				FunctionTooltip = ButtonCaption;
			}

			TWeakObjectPtr<UFunction> WeakFunctionPtr(Function);
			CategoryEntry.WrapBox->AddSlot()
			.Padding(0.0f, 0.0f, 5.0f, 3.0f)
			[
				SNew(SButton)
				.Text(ButtonCaption)
				.OnClicked(FOnClicked::CreateSP(this, &FObjectDetails::OnExecuteCallInEditorFunction, WeakFunctionPtr))
				.ToolTipText(FunctionTooltip.IsEmptyOrWhitespace() ? LOCTEXT("CallInEditorTooltip", "Call an event on the selected object(s)") : FunctionTooltip)
			];

			CategoryEntry.RowTag = Function->GetFName();
			CategoryEntry.FunctionSearchText.AppendLine(ButtonCaption);
			CategoryEntry.FunctionSearchText.AppendLine(FunctionTooltip);

			if (ButtonCaption.ToString() != Function->GetName())
			{
				CategoryEntry.FunctionSearchText.AppendLine(FText::FromString(Function->GetName()));
			}
		}
		
		// Now edit the categories, adding the button strips to the details panel
		for (FCategoryEntry& CategoryEntry : CategoryList)
		{
			IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(CategoryEntry.CategoryName);
			CategoryBuilder.AddCustomRow(CategoryEntry.FunctionSearchText.ToText())
			.RowTag(CategoryEntry.RowTag)
			[
				CategoryEntry.WrapBox.ToSharedRef()
			];
		}
	}
}

FReply FObjectDetails::OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr)
{
	using namespace UE::Reflection;
	if (UFunction* Function = WeakFunctionPtr.Get())
	{
		//@TODO: Consider naming the transaction scope after the fully qualified function name for better UX
		FScopedTransaction Transaction(LOCTEXT("ExecuteCallInEditorMethod", "Call In Editor Action"));
		TStrongObjectPtr<UFunction> CallingFunction(Function);

		if (Function->HasMetaData(FBlueprintMetadata::MD_WorldContext) &&
			DoesStaticFunctionSignatureMatch<void(TObjectPtr<UObject>)>(Function))
		{
			FEditorScriptExecutionGuard ScriptGuard;
			extern ENGINE_API class UEngine* GEngine;
			UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
			UObject* WorldContextObject = EditorEngine->GetEditorWorldContext().World();
			TStrongObjectPtr<UObject> CDO(Function->GetOwnerClass()->ClassDefaultObject);
			CDO->ProcessEvent(Function, &WorldContextObject);
		}
		else
		{
			FEditorScriptExecutionGuard ScriptGuard;
			for (TWeakObjectPtr<UObject> SelectedObjectPtr : SelectedObjectsList)
			{
				if (UObject* Object = SelectedObjectPtr.Get())
				{
					ensure(Function->ParmsSize == 0);
					TStrongObjectPtr<UObject> ObjectStrong(Object);
					Object->ProcessEvent(Function, nullptr);
				}
			}
		}

	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
