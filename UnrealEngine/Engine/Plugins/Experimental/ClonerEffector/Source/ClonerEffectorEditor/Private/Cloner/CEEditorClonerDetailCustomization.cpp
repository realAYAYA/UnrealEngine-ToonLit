// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEEditorClonerDetailCustomization.h"

#include "Cloner/CEClonerActor.h"
#include "Cloner/Layouts/CEClonerLayoutBase.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceCurve.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "CEEditorClonerDetailCustomization"

void FCEEditorClonerDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	static const TArray<FName> CategoriesPriority =
	{
		TEXT("Cloner"),
		TEXT("Layout"),
		TEXT("Renderer"),
		TEXT("Progress"),
		TEXT("Step"),
		TEXT("Range"),
		TEXT("Spawn"),
		TEXT("Lifetime"),
	};

	// Sort categories
	int32 Order = InDetailBuilder.EditCategory(TEXT("Transform")).GetSortOrder();
	for (const FName& Category : CategoriesPriority)
	{
		IDetailCategoryBuilder& CategoryBuilder = InDetailBuilder.EditCategory(Category);
		CategoryBuilder.SetSortOrder(++Order);
	}

	// Gather all cloner actor selected
	TArray<TWeakObjectPtr<ACEClonerActor>> SelectedClonersWeak = InDetailBuilder.GetSelectedObjectsOfType<ACEClonerActor>();

	LayoutFunctionNames.Empty();

	if (SelectedClonersWeak.IsEmpty())
	{
		return;
	}

	// Look for layout UFunctions and group them by name
	for (const TWeakObjectPtr<ACEClonerActor>& ClonerWeak : SelectedClonersWeak)
	{
		const ACEClonerActor* Cloner = ClonerWeak.Get();
		if (!Cloner)
		{
			continue;
		}

		// Look for CallInEditor functions in active layout
		UCEClonerLayoutBase* ActiveLayout = Cloner->GetActiveLayout();
		if (!ActiveLayout)
		{
			continue;
		}

		// Iterate through all UFunctions in the class and its parent classes.
		for (UFunction* Function : TFieldRange<UFunction>(ActiveLayout->GetClass(), EFieldIteratorFlags::ExcludeSuper))
		{
			// Only CallInEditor function with 0 parameters
			if (Function && Function->HasMetaData("CallInEditor") && Function->NumParms == 0)
			{
				FName FunctionName = Function->GetFName();
				TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunctions = LayoutFunctionNames.FindOrAdd(FunctionName);
				ObjectFunctions.Add(ActiveLayout, Function);
			}
		}
	}

	if (!LayoutFunctionNames.IsEmpty())
	{
		// Add buttons for selected cloners
		const TSharedPtr<SHorizontalBox> FunctionsWidget = SNew(SHorizontalBox);
		for (const TPair<FName, TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>>& LayoutFunctionNamesPair : LayoutFunctionNames)
		{
			const FText ButtonLabel = FText::FromString(FName::NameToDisplayString(LayoutFunctionNamesPair.Key.ToString(), false));

			FunctionsWidget->AddSlot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(ButtonLabel)
				.OnClicked(this, &FCEEditorClonerDetailCustomization::OnExecuteFunction, LayoutFunctionNamesPair.Key)
			];
		}

		IDetailCategoryBuilder& ClonerCategoryBuilder = InDetailBuilder.EditCategory("Cloner");

		ClonerCategoryBuilder
			.AddCustomRow(FText::FromString("LayoutFunctions"))
			.WholeRowContent()
			[
				FunctionsWidget.ToSharedRef()
			];
	}

	if (SelectedClonersWeak.Num() != 1)
	{
		return;
	}

	const ACEClonerActor* Cloner = SelectedClonersWeak[0].Get();

	if (!Cloner)
	{
		return;
	}

	UNiagaraDataInterfaceCurve* LifetimeScaleCurve = Cloner->GetLifetimeScaleCurveDI();

	if (!LifetimeScaleCurve)
	{
		return;
	}

	const TArray<UObject*> ShowObjects {LifetimeScaleCurve};
	IDetailCategoryBuilder& LifetimeCategory = InDetailBuilder.EditCategory(TEXT("Lifetime"));
	LifetimeCategory.SetShowAdvanced(true);

	FAddPropertyParams Params;
	Params.HideRootObjectNode(true);
	Params.CreateCategoryNodes(false);

	IDetailPropertyRow* CurveRow = LifetimeCategory.AddExternalObjects(ShowObjects, EPropertyLocation::Advanced, Params);
	CurveRow->Visibility(MakeAttributeLambda([Cloner]()
	{
		return Cloner
			&& Cloner->GetLifetimeEnabled()
			&& Cloner->GetLifetimeScaleEnabled()
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}));
}

FReply FCEEditorClonerDetailCustomization::OnExecuteFunction(FName InFunctionName)
{
	if (TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>> const* ObjectFunctions = LayoutFunctionNames.Find(InFunctionName))
	{
		for (const TPair<TWeakObjectPtr<UObject>, TWeakObjectPtr<UFunction>>& ObjectFunction : *ObjectFunctions)
		{
			UObject* Object = ObjectFunction.Key.Get();
			UFunction* Function = ObjectFunction.Value.Get();

			if (!Object || !Function)
			{
				continue;
			}

			Object->ProcessEvent(Function, nullptr);
		}
	}

	return FReply::Handled();
}

void FCEEditorClonerDetailCustomization::RegisterCustomSections() const
{
	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	static const FName ClassName = ACEClonerActor::StaticClass()->GetFName();

	const TSharedRef<FPropertySection> StreamingSection = PropertyModule.FindOrCreateSection(ClassName, "Streaming", LOCTEXT("Cloner.Streaming", "Streaming"));
	StreamingSection->RemoveCategory("World Partition");
	StreamingSection->RemoveCategory("Data Layers");
	StreamingSection->RemoveCategory("HLOD");

	const TSharedRef<FPropertySection> GeneralSection = PropertyModule.FindOrCreateSection(ClassName, "General", LOCTEXT("Cloner.General", "General"));
	GeneralSection->AddCategory("Cloner");

	const TSharedRef<FPropertySection> LayoutSection = PropertyModule.FindOrCreateSection(ClassName, "Layouts", LOCTEXT("Cloner.Layouts", "Layouts"));
	LayoutSection->AddCategory("Layout");

	const TSharedRef<FPropertySection> RendererSection = PropertyModule.FindOrCreateSection(ClassName, "Renderer", LOCTEXT("Cloner.Renderer", "Renderer"));
	RendererSection->AddCategory("Renderer");

	const TSharedRef<FPropertySection> ProgressSection = PropertyModule.FindOrCreateSection(ClassName, "Progress", LOCTEXT("Cloner.Progress", "Progress"));
	ProgressSection->AddCategory("Progress");

	const TSharedRef<FPropertySection> StepSection = PropertyModule.FindOrCreateSection(ClassName, "Step", LOCTEXT("Cloner.Step", "Step"));
	StepSection->AddCategory("Step");

	const TSharedRef<FPropertySection> RangeSection = PropertyModule.FindOrCreateSection(ClassName, "Range", LOCTEXT("Cloner.Range", "Range"));
	RangeSection->AddCategory("Range");

	const TSharedRef<FPropertySection> SpawnSection = PropertyModule.FindOrCreateSection(ClassName, "Spawn", LOCTEXT("Cloner.Spawn", "Spawn"));
	SpawnSection->AddCategory("Spawn");

	const TSharedRef<FPropertySection> LifetimeSection = PropertyModule.FindOrCreateSection(ClassName, "Lifetime", LOCTEXT("Cloner.Lifetime", "Lifetime"));
	LifetimeSection->AddCategory("Lifetime");
}

#undef LOCTEXT_NAMESPACE
