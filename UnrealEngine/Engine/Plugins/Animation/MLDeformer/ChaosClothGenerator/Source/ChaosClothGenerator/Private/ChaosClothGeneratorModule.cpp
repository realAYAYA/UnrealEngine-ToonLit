// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosClothGeneratorToolsMenuExtender.h"
#include "MLDeformerEditorToolkit.h"
#include "PropertyEditorModule.h"
#include "SClothGeneratorWidget.h"

namespace UE::Chaos::ClothGenerator
{
	class FChaosClothGeneratorModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			UE::MLDeformer::FMLDeformerEditorToolkit::AddToolsMenuExtender(CreateToolsMenuExtender());
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout("ClothGeneratorProperties", FOnGetDetailCustomizationInstance::CreateStatic(&FClothGeneratorDetails::MakeInstance));
		}

		virtual void ShutdownModule() override
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("ClothGeneratorProperties");
		}
	};
};

IMPLEMENT_MODULE(UE::Chaos::ClothGenerator::FChaosClothGeneratorModule, ChaosClothGenerator)