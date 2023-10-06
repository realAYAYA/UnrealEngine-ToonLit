// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditorTypes.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WaveTableSettings.h"

class FSpawnTabArgs;
enum class EWaveTableCurveSource : uint8;
namespace WaveTable::Editor { class FWaveTableCurveModel; }
struct FRichCurve;


// Forward Declarations
class FCurveEditor;
struct FWaveTableTransform;
class IToolkitHost;
class SCurveEditorPanel;
class UCurveFloat;

namespace WaveTable
{
	namespace Editor
	{
		class WAVETABLEEDITOR_API FBankEditorBase : public FAssetEditorToolkit, public FNotifyHook, public FEditorUndoClient
		{
		public:
			FBankEditorBase();
			virtual ~FBankEditorBase() = default;

			void Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* InParentObject);

			/** FAssetEditorToolkit interface */
			virtual FName GetToolkitFName() const override;
			virtual FText GetBaseToolkitName() const override;
			virtual FString GetWorldCentricTabPrefix() const override;
			virtual FLinearColor GetWorldCentricTabColorScale() const override;
			virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
			virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;

			/** FNotifyHook interface */
			virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

			// Regenerates curves set to "file" without refreshing whole stack view
			void RegenerateFileCurves();

			/** Updates & redraws curves. */
			void RefreshCurves();

		protected:
			struct FCurveData
			{
				FCurveModelID ModelID;

				/* Curve used purely for display.  May be down-sampled from
				 * asset's curve for performance while editing */
				TSharedPtr<FRichCurve> ExpressionCurve;

				FCurveData()
					: ModelID(FCurveModelID::Unique())
				{
				}
			};

			virtual void PostUndo(bool bSuccess) override;
			virtual void PostRedo(bool bSuccess) override;

			virtual bool GetIsPropertyEditorDisabled() const { return false; }

			// Returns resolution of bank being edited. By default, bank does not support WaveTable generation
			// by being set to no resolution.
			virtual EWaveTableResolution GetBankResolution() const { return EWaveTableResolution::None; }

			// Returns sampling mode of bank being edited.
			virtual EWaveTableSamplingMode GetBankSamplingMode() const { return EWaveTableSamplingMode::FixedResolution; }

			// Returns sample rate to use if BankSamplingMode is set to 'SampleRate'.
			virtual int32 GetBankSampleRate() const { return 0; }

			// Returns whether or not bank is bipolar.  By default, returns false (bank is unipolar), functionally
			// operating as a unipolar envelope editor.
			virtual bool GetBankIsBipolar() const { return false; }

			// Construct a new curve model for the given FRichCurve.  Allows for editor implementation to construct custom curve model types.
			virtual TUniquePtr<FWaveTableCurveModel> ConstructCurveModel(FRichCurve& InRichCurve, UObject* InParentObject, EWaveTableCurveSource InSource) = 0;

			// Returns the transform associated with the given index
			virtual FWaveTableTransform* GetTransform (int32 InIndex) const = 0;

			// Returns the number of transforms associated with the given bank.
			virtual int32 GetNumTransforms() const = 0;

			void SetCurve(int32 InTransformIndex, FRichCurve& InRichCurve, EWaveTableCurveSource InSource);

		private:
			// Regenerates expression curve at the given index. If curve is a 'File' and source PCM data in Transform
			// is too long, inline edits are not included in recalculation for better interact performance.
			void GenerateExpressionCurve(FCurveData& OutCurveData, int32 InTransformIndex, bool bInIsUnset = false);

			void InitCurves();

			void ResetCurves();

			/**	Spawns the tab allowing for editing/viewing the output curve(s) */
			TSharedRef<SDockTab> SpawnTab_OutputCurve(const FSpawnTabArgs& Args);

			/**	Spawns the tab allowing for editing/viewing the output curve(s) */
			TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);

			/** Get the orientation for the snap value controls. */
			EOrientation GetSnapLabelOrientation() const;

			/** Trims keys out-of-bounds in provided curve */
			static void TrimKeys(FRichCurve& OutCurve);

			/** Clears the expression curve at the given input index */
			void ClearExpressionCurve(int32 InTransformIndex);

			bool RequiresNewCurve(int32 InTransformIndex, const FRichCurve& InRichCurve) const;

			TSharedPtr<FUICommandList> ToolbarCurveTargetCommands;

			TSharedPtr<FCurveEditor> CurveEditor;
			TSharedPtr<SCurveEditorPanel> CurvePanel;

			TArray<FCurveData> CurveData;

			/** Properties tab */
			TSharedPtr<IDetailsView> PropertiesView;

			/** Settings Editor App Identifier */
			static const FName AppIdentifier;
			static const FName CurveTabId;
			static const FName PropertiesTabId;
		};

		class WAVETABLEEDITOR_API FBankEditor : public FBankEditorBase
		{
		public:
			FBankEditor() = default;
			virtual ~FBankEditor() = default;

		protected:
			virtual EWaveTableResolution GetBankResolution() const override;
			virtual EWaveTableSamplingMode GetBankSamplingMode() const override;
			virtual int32 GetBankSampleRate() const override;
			virtual bool GetBankIsBipolar() const override;
			virtual int32 GetNumTransforms() const override;
			virtual FWaveTableTransform* GetTransform(int32 InIndex) const override;

			virtual TUniquePtr<FWaveTableCurveModel> ConstructCurveModel(FRichCurve& InRichCurve, UObject* InParentObject, EWaveTableCurveSource InSource) override;
		};
	} // namespace Editor
} // namespace WaveTable

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Curves/CurveOwnerInterface.h"
#include "Toolkits/IToolkitHost.h"
#include "WaveTableCurveEditorViewStacked.h"
#endif
