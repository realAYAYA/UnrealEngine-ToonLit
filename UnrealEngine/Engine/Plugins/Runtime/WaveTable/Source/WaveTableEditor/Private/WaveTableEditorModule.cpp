// Copyright Epic Games, Inc. All Rights Reserved.
#include "WaveTableEditorModule.h"

#include "AssetTypeActions_WaveTableBank.h"
#include "ICurveEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "WaveTableCurveEditorViewStacked.h"
#include "WaveTableTransformLayout.h"

DEFINE_LOG_CATEGORY(LogWaveTableEditor);


namespace WaveTable
{
	namespace Editor
	{
		namespace ModulePrivate
		{
			static const FName AssetToolName { "AssetTools" };
			static const FName CurveEditorName { "CurveEditor" };
			static const FName PropertyEditorName { "PropertyEditor" };

			template <typename T>
			void AddAssetAction(IAssetTools& AssetTools, TArray<TSharedPtr<FAssetTypeActions_Base>>& AssetArray)
			{
				TSharedPtr<T> AssetAction = MakeShared<T>();
				TSharedPtr<FAssetTypeActions_Base> AssetActionBase = StaticCastSharedPtr<FAssetTypeActions_Base>(AssetAction);
				AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());
				AssetArray.Add(AssetActionBase);
			}
		} // namespace ModulePrivate

		void FModule::StartupModule()
		{
			using namespace ModulePrivate;

			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(PropertyEditorName);
			PropertyModule.RegisterCustomPropertyTypeLayout
			(
				"WaveTableTransform",
				FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTransformLayoutCustomization::MakeInstance)
			);

			ICurveEditorModule& CurveEditorModule = FModuleManager::LoadModuleChecked<ICurveEditorModule>(CurveEditorName);
			FWaveTableCurveModel::WaveTableViewId = CurveEditorModule.RegisterView(FOnCreateCurveEditorView::CreateStatic(
				[](TWeakPtr<FCurveEditor> WeakCurveEditor) -> TSharedRef<SCurveEditorView>
				{
					return SNew(SViewStacked, WeakCurveEditor);
				}
			));

			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolName).Get();
			AddAssetAction<FAssetTypeActions_WaveTableBank>(AssetTools, AssetActions);
		}

		void FModule::ShutdownModule()
		{
			using namespace ModulePrivate;

			if (FModuleManager::Get().IsModuleLoaded(PropertyEditorName))
			{
				FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorName);
				PropertyModule.UnregisterCustomPropertyTypeLayout("WaveTableTransform");
			}

// 			if (FModuleManager::Get().IsModuleLoaded(AssetToolName))
// 			{
// 				IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolName).Get();
// 				for (const TSharedPtr<FAssetTypeActions_Base>& ActionPtr : AssetActions)
// 				{
// 					AssetTools.UnregisterAssetTypeActions(ActionPtr);
// 				}
// 			}

			if (FModuleManager::Get().IsModuleLoaded(CurveEditorName))
			{
				ICurveEditorModule& CurveEditorModule = FModuleManager::LoadModuleChecked<ICurveEditorModule>(CurveEditorName);
				CurveEditorModule.UnregisterView(FWaveTableCurveModel::WaveTableViewId);
				FWaveTableCurveModel::WaveTableViewId = ECurveEditorViewID::Invalid;
			}
		}
	} // namespace Editor
} // namespace WaveTable

IMPLEMENT_MODULE(WaveTable::Editor::FModule, WaveTableEditor);
