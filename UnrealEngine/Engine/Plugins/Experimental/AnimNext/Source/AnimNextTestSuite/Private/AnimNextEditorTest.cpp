// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UncookedOnlyUtils.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Misc/AutomationTest.h"
#include "Param/ParameterBlockFactory.h"
#include "Param/ParameterLibraryFactory.h"
#include "Param/AnimNextParameterBlock.h"
#include "Param/AnimNextParameterBlockBinding.h"
#include "Param/AnimNextParameterLibrary.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "ScopedTransaction.h"
#include "Editor.h"
#include "IPythonScriptPlugin.h"
#endif

// AnimNext Editor Tests

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Library, "Animation.AnimNext.Parameters.Editor.Library", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Library::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	UFactory* LibraryFactory = NewObject<UAnimNextParameterLibraryFactory>();
	UAnimNextParameterLibrary* Library = Cast<UAnimNextParameterLibrary>(LibraryFactory->FactoryCreateNew(UAnimNextParameterLibrary::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterLibrary"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Library == nullptr)
	{
		AddError(TEXT("Could not create parameter library."));
		return false;
	}

	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam0"), FAnimNextParamType::GetType<bool>()) != nullptr, TEXT("Could not create new bool parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam1"), FAnimNextParamType::GetType<uint8>()) != nullptr, TEXT("Could not create new uint8 parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam2"), FAnimNextParamType::GetType<int32>()) != nullptr, TEXT("Could not create new int32 parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam3"), FAnimNextParamType::GetType<int64>()) != nullptr, TEXT("Could not create new int64 parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam4"), FAnimNextParamType::GetType<float>()) != nullptr, TEXT("Could not create new float parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam5"), FAnimNextParamType::GetType<double>()) != nullptr, TEXT("Could not create new double parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam6"), FAnimNextParamType::GetType<FName>()) != nullptr, TEXT("Could not create new FName parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam7"), FAnimNextParamType::GetType<FString>()) != nullptr, TEXT("Could not create new FString parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam8"), FAnimNextParamType::GetType<FText>()) != nullptr, TEXT("Could not create new FText parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam9"), FAnimNextParamType::GetType<EPropertyBagPropertyType>()) != nullptr, TEXT("Could not create new enum parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam10"), FAnimNextParamType::GetType<FVector>()) != nullptr, TEXT("Could not create new FVector parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam11"), FAnimNextParamType::GetType<FQuat>()) != nullptr, TEXT("Could not create new FQuat parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam12"), FAnimNextParamType::GetType<FTransform>()) != nullptr, TEXT("Could not create new FTransform parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam13"), FAnimNextParamType::GetType<TObjectPtr<UObject>>()) != nullptr, TEXT("Could not create new TObjectPtr<UObject> parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam14"), FAnimNextParamType::GetType<TObjectPtr<UAnimSequence>>()) != nullptr, TEXT("Could not create new TObjectPtr<UAnimSequence> parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam15"), FAnimNextParamType::GetType<TArray<float>>()) != nullptr, TEXT("Could not create new TArray<float> parameter in library."));
	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam16"), FAnimNextParamType::GetType<TArray<TObjectPtr<UAnimSequence>>>()) != nullptr, TEXT("Could not create new TArray<TObjectPtr<UAnimSequence>> parameter in library."));

	AddErrorIfFalse(Library->RemoveParameter(TEXT("TestParam0")), TEXT("Could not remove parameter from library."));
	AddErrorIfFalse(Library->RemoveParameters({ TEXT("TestParam1"), TEXT("TestParam2") }), TEXT("Could not remove parameters from library."));
	AddErrorIfFalse(Library->FindParameter(TEXT("TestParam3")) != nullptr, TEXT("Could not find parameter in library."));
	
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Block, "Animation.AnimNext.Parameters.Editor.Block", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Block::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	UFactory* LibraryFactory = NewObject<UAnimNextParameterLibraryFactory>();
	UAnimNextParameterLibrary* Library = Cast<UAnimNextParameterLibrary>(LibraryFactory->FactoryCreateNew(UAnimNextParameterLibrary::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterLibrary"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Library == nullptr)
	{
		AddError(TEXT("Could not create parameter library."));
		return false;
	}

	AddErrorIfFalse(Library->AddParameter(TEXT("TestParam"), FAnimNextParamType::GetType<bool>()) != nullptr, TEXT("Could not create new parameter in library."));

	UFactory* BlockFactory = NewObject<UAnimNextParameterBlockFactory>();
	UAnimNextParameterBlock* Block = Cast<UAnimNextParameterBlock>(BlockFactory->FactoryCreateNew(UAnimNextParameterBlock::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterBlock"), RF_Transient, nullptr, nullptr, NAME_None));
	if(Block == nullptr)
	{
		AddError(TEXT("Could not create parameter block."));
		return false;
	}

	UAnimNextParameterBlock_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData(Block);
	if(EditorData == nullptr)
	{
		AddError(TEXT("Parameter block has no editor data."));
		return false;
	}

	// AddBinding
	UAnimNextParameterBlockBinding* Binding = nullptr;
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		Binding = EditorData->AddBinding(TEXT("TestParam"), Library);
		AddErrorIfFalse(Binding != nullptr, TEXT("Could not create new binding in block."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// RemoveAllBindings
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveAllBindings(TEXT("TestParam")), TEXT("Failed to remove binding."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	// RemoveEntry
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		AddErrorIfFalse(EditorData->RemoveEntry(Binding), TEXT("Failed to remove entry."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), EditorData->Entries.Num()));

	GEditor->UndoTransaction();
	AddErrorIfFalse(EditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), EditorData->Entries.Num()));
	
	UAnimNextParameterBlock* OtherBlock = Cast<UAnimNextParameterBlock>(BlockFactory->FactoryCreateNew(UAnimNextParameterBlock::StaticClass(), GetTransientPackage(), TEXT("TestAnimNextParameterBlock2"), RF_Transient, nullptr, nullptr, NAME_None));
	if(OtherBlock == nullptr)
	{
		AddError(TEXT("Could not create additional parameter block."));
		return false;
	}
	
	UAnimNextParameterBlock_EditorData* OtherEditorData = UncookedOnly::FUtils::GetEditorData(OtherBlock);
	if(OtherEditorData == nullptr)
	{
		AddError(TEXT("Additional parameter block has no editor data."));
		return false;
	}

	// AddBindingReference
	{
		FScopedTransaction Transaction(FText::GetEmpty());
		UAnimNextParameterBlockBindingReference* Ref = OtherEditorData->AddBindingReference(TEXT("TestParam"), Library, Block);
		AddErrorIfFalse(Ref != nullptr, TEXT("Could not create new binding reference in block."));
	}

	GEditor->UndoTransaction();
	AddErrorIfFalse(OtherEditorData->Entries.Num() == 0, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 0)."), OtherEditorData->Entries.Num()));

	GEditor->RedoTransaction();
	AddErrorIfFalse(OtherEditorData->Entries.Num() == 1, FString::Printf(TEXT("Unexpected entry count found in parameter block (Have %d, expected 1)."), OtherEditorData->Entries.Num()));

	// FindBinding
	AddErrorIfFalse(EditorData->FindBinding(TEXT("TestParam")) != nullptr, TEXT("Could not find binding in block."));
	AddErrorIfFalse(OtherEditorData->FindBinding(TEXT("TestParam")) != nullptr, TEXT("Could not find binding refernce in block."));

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnimationAnimNextParametersEditorTest_Python, "Animation.AnimNext.Parameters.Editor.Python", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnimationAnimNextParametersEditorTest_Python::RunTest(const FString& InParameters)
{
	using namespace UE::AnimNext;

	const TCHAR* Script = TEXT(
		"asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n"
		"library = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestLibrary\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterLibrary, factory = unreal.AnimNextParameterLibraryFactory())\n"
		"block = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestBlock\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterBlock, factory = unreal.AnimNextParameterBlockFactory())\n"
		"other_block = unreal.AssetTools.create_asset(asset_tools, asset_name = \"TestBlock1\", package_path = \"/Game/\", asset_class = unreal.AnimNextParameterBlock, factory = unreal.AnimNextParameterBlockFactory())\n"
		"library.add_parameter(name = \"TestParam\", value_type = unreal.PropertyBagPropertyType.BOOL, container_type = unreal.PropertyBagContainerType.NONE)\n"
		"block.add_binding(name = \"TestParam\", library = library)\n"
		"other_block.add_binding_reference(name = \"TestParam\", library = library, referenced_block = block)\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(library)\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(block)\n"
		"unreal.EditorAssetLibrary.delete_loaded_asset(other_block)\n"
	);

	IPythonScriptPlugin::Get()->ExecPythonCommand(Script);

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	return true;
}

#endif	// WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR