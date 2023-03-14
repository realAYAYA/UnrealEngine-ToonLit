// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorModule.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetTypeActions_Base.h"
#include "Brushes/SlateImageBrush.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphUtilities.h"
#include "Styling/AppStyle.h"
#include "HAL/IConsoleManager.h"
#include "IDetailCustomization.h"
#include "ISettingsModule.h"
#include "Metasound.h"
#include "MetasoundAssetSubsystem.h"
#include "MetasoundAssetTypeActions.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundDetailCustomization.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphConnectionDrawingPolicy.h"
#include "MetasoundEditorGraphMemberDefaults.h"
#include "MetasoundEditorGraphNodeFactory.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundNodeDetailCustomization.h"
#include "MetasoundSettings.h"
#include "MetasoundSource.h"
#include "MetasoundTime.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundUObjectRegistry.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/UObjectGlobals.h"


DEFINE_LOG_CATEGORY(LogMetasoundEditor);


static int32 MetaSoundEditorAsyncRegistrationEnabledCVar = 1;
FAutoConsoleVariableRef CVarMetaSoundEditorAsyncRegistrationEnabled(
	TEXT("au.MetaSound.Editor.AsyncRegistrationEnabled"),
	MetaSoundEditorAsyncRegistrationEnabledCVar,
	TEXT("Enable registering all MetaSound asset classes asyncronously on editor load.\n")
	TEXT("0: Disabled, !0: Enabled (default)"),
	ECVF_Default);

// Forward declarations 
class UMetasoundInterfacesView;

namespace Metasound
{
	namespace Editor
	{
		using FMetasoundGraphPanelPinFactory = FGraphPanelPinFactory;

		static const FName AssetToolName { "AssetTools" };

		template <typename T>
		void AddAssetAction(IAssetTools& AssetTools, TArray<TSharedPtr<FAssetTypeActions_Base>>& AssetArray)
		{
			TSharedPtr<T> AssetAction = MakeShared<T>();
			TSharedPtr<FAssetTypeActions_Base> AssetActionBase = StaticCastSharedPtr<FAssetTypeActions_Base>(AssetAction);
			AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());
			AssetArray.Add(AssetActionBase);
		}

		class FSlateStyle final : public FSlateStyleSet
		{
		public:
			FSlateStyle()
				: FSlateStyleSet("MetaSoundStyle")
			{
				SetParentStyleName(FAppStyle::GetAppStyleSetName());

				SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Runtime/Metasound/Content/Editor/Slate"));
				SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

				static const FVector2D Icon20x20(20.0f, 20.0f);
				static const FVector2D Icon40x40(40.0f, 40.0f);

				static const FVector2D Icon16 = FVector2D(16.0f, 16.0f);
				static const FVector2D Icon64 = FVector2D(64.0f, 64.0f);

				const FVector2D Icon15x11(15.0f, 11.0f);

				// Metasound Editor
				{
					Set("MetaSoundPatch.Color", FColor(31, 133, 31));
					Set("MetaSoundSource.Color", FColor(103, 214, 66));

					// Actions
					Set("MetasoundEditor.Play", new IMAGE_BRUSH_SVG(TEXT("Icons/play"), Icon40x40));
					Set("MetasoundEditor.Play.Small", new IMAGE_BRUSH_SVG(TEXT("Icons/play"), Icon20x20));
					Set("MetasoundEditor.Play.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/play_thumbnail"), Icon64));
					Set("MetasoundEditor.Play.Thumbnail.Hovered", new IMAGE_BRUSH_SVG(TEXT("Icons/play_thumbnail_hover"), Icon64));

					Set("MetasoundEditor.Play.Active.Valid", new IMAGE_BRUSH_SVG(TEXT("Icons/play_active_valid"), Icon40x40));
					Set("MetasoundEditor.Play.Active.Warning", new IMAGE_BRUSH_SVG(TEXT("Icons/play_active_warning"), Icon40x40));
					Set("MetasoundEditor.Play.Inactive.Valid", new IMAGE_BRUSH_SVG(TEXT("Icons/play_inactive_valid"), Icon40x40));
					Set("MetasoundEditor.Play.Inactive.Warning", new IMAGE_BRUSH_SVG(TEXT("Icons/play_inactive_warning"), Icon40x40));
					Set("MetasoundEditor.Play.Error", new IMAGE_BRUSH_SVG(TEXT("Icons/play_error"), Icon40x40));

					Set("MetasoundEditor.Stop", new IMAGE_BRUSH_SVG(TEXT("Icons/stop"), Icon40x40));

					Set("MetasoundEditor.Stop.Disabled", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_disabled"), Icon40x40));
					Set("MetasoundEditor.Stop.Active", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_active"), Icon40x40));
					Set("MetasoundEditor.Stop.Inactive", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_inactive"), Icon40x40));
					Set("MetasoundEditor.Stop.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_thumbnail"), Icon64));
					Set("MetasoundEditor.Stop.Thumbnail.Hovered", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_thumbnail_hover"), Icon64));

					Set("MetasoundEditor.Import", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_40x.png")), Icon40x40));
					Set("MetasoundEditor.Import.Small", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_40x.png")), Icon20x20));
					Set("MetasoundEditor.Export", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_40x.png")), Icon40x40));
					Set("MetasoundEditor.Export.Small", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_40x.png")), Icon20x20));
					Set("MetasoundEditor.ExportError", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_error_40x.png")), Icon40x40));
					Set("MetasoundEditor.ExportError.Small", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/build_error_40x.png")), Icon20x20));
					Set("MetasoundEditor.Settings", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/settings_40x.png")), Icon20x20));

					// Graph Editor
					Set("MetasoundEditor.Graph.Node.Body.Input", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_input_body_64x.png")), FVector2D(114.0f, 64.0f)));
					Set("MetasoundEditor.Graph.Node.Body.Default", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_default_body_64x.png")), FVector2D(64.0f, 64.0f)));

					Set("MetasoundEditor.Graph.TriggerPin.Connected", new IMAGE_BRUSH(TEXT("Graph/pin_trigger_connected"), Icon15x11));
					Set("MetasoundEditor.Graph.TriggerPin.Disconnected", new IMAGE_BRUSH(TEXT("Graph/pin_trigger_disconnected"), Icon15x11));

					Set("MetasoundEditor.Graph.Node.Class.Native", new IMAGE_BRUSH_SVG(TEXT("Icons/native_node"), FVector2D(8.0f, 16.0f)));
					Set("MetasoundEditor.Graph.Node.Class.Graph", new IMAGE_BRUSH_SVG(TEXT("Icons/graph_node"), Icon16));
					Set("MetasoundEditor.Graph.Node.Class.Input", new IMAGE_BRUSH_SVG(TEXT("Icons/input_node"), FVector2D(16.0f, 13.0f)));
					Set("MetasoundEditor.Graph.Node.Class.Output", new IMAGE_BRUSH_SVG(TEXT("Icons/output_node"), FVector2D(16.0f, 13.0f)));
					Set("MetasoundEditor.Graph.Node.Class.Reroute", new IMAGE_BRUSH_SVG(TEXT("Icons/reroute_node"), Icon16));
					Set("MetasoundEditor.Graph.Node.Class.Variable", new IMAGE_BRUSH_SVG(TEXT("Icons/variable_node"), FVector2D(16.0f, 13.0f)));

					Set("MetasoundEditor.Graph.Node.Math.Add", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_add_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Divide", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_divide_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Modulo", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_modulo_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Multiply", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_multiply_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Subtract", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_subtract_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Power", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_power_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Math.Logarithm", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_math_logarithm_40x.png")), Icon40x40));
					Set("MetasoundEditor.Graph.Node.Conversion", new FSlateImageBrush(RootToContentDir(TEXT("/Graph/node_conversion_40x.png")), Icon40x40));

					Set("MetasoundEditor.Graph.InvalidReroute", new IMAGE_BRUSH_SVG(TEXT("Icons/invalid_reroute"), Icon16));
					Set("MetasoundEditor.Graph.ConstructorPinArray", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/array_pin_rotated.png")), Icon16));
					Set("MetasoundEditor.Graph.ConstructorPinArrayDisconnected", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/array_pin_rotated_disconnected.png")), Icon16));
					Set("MetasoundEditor.Graph.ArrayPin", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/array_pin.png")), Icon16));
					Set("MetasoundEditor.Graph.ConstructorPin", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/square_pin_rotated.png")), Icon16));
					Set("MetasoundEditor.Graph.ConstructorPinDisconnected", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/square_pin_rotated_disconnected.png")), Icon16));

					// Analyzers
					Set("MetasoundEditor.Analyzers.BackgroundColor", FLinearColor(0.0075f, 0.0075f, 0.0075, 1.0f));

					// Misc
					Set("MetasoundEditor.Speaker", new FSlateImageBrush(RootToContentDir(TEXT("/Icons/speaker_144x.png")), FVector2D(144.0f, 144.0f)));
					Set("MetasoundEditor.Metasound.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/metasound_icon"), Icon16));

					// Class Icons
					auto SetClassIcon = [this, InIcon16 = Icon16, InIcon64 = Icon64](const FString& ClassName)
					{
						const FString IconFileName = FString::Printf(TEXT("Icons/%s"), *ClassName.ToLower());
						const FSlateColor DefaultForeground(FStyleColors::Foreground);

						Set(*FString::Printf(TEXT("ClassIcon.%s"), *ClassName), new IMAGE_BRUSH_SVG(IconFileName, InIcon16));
						Set(*FString::Printf(TEXT("ClassThumbnail.%s"), *ClassName), new IMAGE_BRUSH_SVG(IconFileName, InIcon64));
					};

					SetClassIcon(TEXT("MetasoundPatch"));
					SetClassIcon(TEXT("MetasoundSource"));

					Set("MetasoundEditor.MetasoundPatch.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundpatch_icon"), Icon20x20));
					Set("MetasoundEditor.MetasoundPatch.Preset.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundpatchpreset_icon"), Icon20x20));
					Set("MetasoundEditor.MetasoundSource.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundsource_icon"), Icon20x20));
					Set("MetasoundEditor.MetasoundSource.Preset.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundsourcepreset_icon"), Icon20x20));
					Set("MetasoundEditor.MetasoundPatch.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundpatch_thumbnail"), Icon20x20));
					Set("MetasoundEditor.MetasoundPatch.Preset.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundpatchpreset_thumbnail"), Icon20x20));
					Set("MetasoundEditor.MetasoundSource.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundsource_thumbnail"), Icon20x20));
					Set("MetasoundEditor.MetasoundSource.Preset.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/metasoundsourcepreset_thumbnail"), Icon20x20));
				}

				FSlateStyleRegistry::RegisterSlateStyle(*this);
			}
		};

		namespace Style
		{
			FSlateIcon CreateSlateIcon(FName InName)
			{
				return { "MetaSoundStyle", InName};
			}

			const FSlateBrush& GetSlateBrushSafe(FName InName)
			{
				const ISlateStyle* MetaSoundStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle");
				if (ensureMsgf(MetaSoundStyle, TEXT("Missing slate style 'MetaSoundStyle'")))
				{
					const FSlateBrush* Brush = MetaSoundStyle->GetBrush(InName);
					if (ensureMsgf(Brush, TEXT("Missing brush '%s'"), *InName.ToString()))
					{
						return *Brush;
					}
				}

				if (const FSlateBrush* NoBrush = FAppStyle::GetBrush("NoBrush"))
				{
					return *NoBrush;
				}

				static const FSlateBrush NullBrush;
				return NullBrush;
			}
		}

		class FModule : public IMetasoundEditorModule
		{
			void LoadAndRegisterAsset(const FAssetData& InAssetData)
			{
				Frontend::FMetaSoundAssetRegistrationOptions RegOptions;
				RegOptions.bForceReregister = false;
				if (const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>())
				{
					RegOptions.bAutoUpdateLogWarningOnDroppedConnection = Settings->bAutoUpdateLogWarningOnDroppedConnection;
				}

				if (InAssetData.IsAssetLoaded())
				{
					if (UObject* AssetObject = InAssetData.GetAsset())
					{
						FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(AssetObject);
						check(MetaSoundAsset);
						MetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
					}
				}
				else
				{
					if (!MetaSoundEditorAsyncRegistrationEnabledCVar)
					{
						return;
					}

					if (AssetPrimeStatus == EAssetPrimeStatus::NotRequested)
					{
						return;
					}

					ActiveAsyncAssetLoadRequests++;

					FSoftObjectPath AssetPath = InAssetData.ToSoftObjectPath();
					auto LoadAndRegister = [this, ObjectPath = AssetPath, RegOptions](const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)
					{
						if (Result == EAsyncLoadingResult::Succeeded)
						{
							FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(ObjectPath.ResolveObject());
							check(MetaSoundAsset);
							if (!MetaSoundAsset->IsRegistered())
							{
								MetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
							}
						}

						ActiveAsyncAssetLoadRequests--;
						if (AssetPrimeStatus == EAssetPrimeStatus::InProgress && ActiveAsyncAssetLoadRequests == 0)
						{
							AssetPrimeStatus = EAssetPrimeStatus::Complete;
						}
					};
					LoadPackageAsync(AssetPath.GetLongPackageName(), FLoadPackageAsyncDelegate::CreateLambda(LoadAndRegister));
				}
			}

			void AddClassRegistryAsset(const FAssetData& InAssetData)
			{
				using namespace Frontend;

				if (!IsMetaSoundAssetClass(InAssetData.AssetClassPath))
				{
					return;
				}

				check(GEngine);
				UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
				check(AssetSubsystem);

				const FNodeRegistryKey RegistryKey = AssetSubsystem->AddOrUpdateAsset(InAssetData);

				// Can be invalid if being called for the first time on an asset before FRenameRootGraphClass is called
				if (NodeRegistryKey::IsValid(RegistryKey))
				{
					const bool bPrimeRequested = AssetPrimeStatus > EAssetPrimeStatus::NotRequested;
					const bool bIsRegistered = FMetasoundFrontendRegistryContainer::Get()->IsNodeRegistered(RegistryKey);
					if (bPrimeRequested && !bIsRegistered)
					{
						LoadAndRegisterAsset(InAssetData);
					}
				}
			}

			void UpdateClassRegistryAsset(const FAssetData& InAssetData)
			{
				using namespace Frontend;

				if (!IsMetaSoundAssetClass(InAssetData.AssetClassPath))
				{
					return;
				}

				check(GEngine);
				UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
				check(AssetSubsystem);

				const FNodeRegistryKey RegistryKey = AssetSubsystem->AddOrUpdateAsset(InAssetData);
				const bool bPrimeRequested = AssetPrimeStatus > EAssetPrimeStatus::NotRequested;
				const bool bIsRegistered = FMetasoundFrontendRegistryContainer::Get()->IsNodeRegistered(RegistryKey);

				// Have to re-register even if prime was not requested to avoid registry desync.
				if (bPrimeRequested || bIsRegistered)
				{
					LoadAndRegisterAsset(InAssetData);
				}
			}

			void OnPackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
			{
				using namespace Metasound;
				using namespace Metasound::Editor;
				using namespace Metasound::Frontend;

				if (!InPackageReloadedEvent)
				{
					return;
				}

				if (InPackageReloadPhase != EPackageReloadPhase::OnPackageFixup)
				{
					return;
				}

				for (const TPair<UObject*, UObject*>& Pair : InPackageReloadedEvent->GetRepointedObjects())
				{
					if (UObject* Obj = Pair.Key)
					{
						if (IsMetaSoundAssetClass(Obj->GetClass()->GetClassPathName()))
						{
							check(GEngine);
							UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
							check(AssetSubsystem);

							// Use the editor version of UnregisterWithFrontend so it refreshes any open MetaSound editors
							AssetSubsystem->RemoveAsset(*Pair.Key);
							FGraphBuilder::UnregisterGraphWithFrontend(*Pair.Key);
						}
					}

					if (UObject* Obj = Pair.Value)
					{
						if (IsMetaSoundAssetClass(Obj->GetClass()->GetClassPathName()))
						{
							check(GEngine);
							UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
							check(AssetSubsystem);
							// Use the editor version of RegisterWithFrontend so it refreshes any open MetaSound editors
							AssetSubsystem->AddOrUpdateAsset(*Pair.Value);
							FGraphBuilder::RegisterGraphWithFrontend(*Pair.Value);
						}
					}
				}
			}

			void OnAssetScanFinished()
			{
				AssetScanStatus = EAssetScanStatus::Complete;

				if (AssetPrimeStatus == EAssetPrimeStatus::Requested)
				{
					PrimeAssetRegistryAsync();
				}

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FModule::AddClassRegistryAsset);
				AssetRegistryModule.Get().OnAssetUpdated().AddRaw(this, &FModule::UpdateClassRegistryAsset);
				AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FModule::RemoveAssetFromClassRegistry);
				AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FModule::RenameAssetInClassRegistry);

				AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);

				FCoreUObjectDelegates::OnPackageReloaded.AddRaw(this, &FModule::OnPackageReloaded);
			}

			void RemoveAssetFromClassRegistry(const FAssetData& InAssetData)
			{
				if (IsMetaSoundAssetClass(InAssetData.AssetClassPath))
				{
					check(GEngine);
					UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
					check(AssetSubsystem);

					// Use the editor version of UnregisterWithFrontend so it refreshes any open MetaSound editors
					AssetSubsystem->RemoveAsset(InAssetData);
					if (UObject* AssetObject = InAssetData.GetAsset())
					{
						FGraphBuilder::UnregisterGraphWithFrontend(*AssetObject);
					}
				}
			}

			void RenameAssetInClassRegistry(const FAssetData& InAssetData, const FString& InOldObjectPath)
			{
				if (IsMetaSoundAssetClass(InAssetData.AssetClassPath))
				{
					check(GEngine);
					UMetaSoundAssetSubsystem* AssetSubsystem = GEngine->GetEngineSubsystem<UMetaSoundAssetSubsystem>();
					check(AssetSubsystem);

					// Use the FGraphBuilder Register call instead of registering via the
					// MetaSoundAssetSubsystem so as to properly refresh respective open editors.
					constexpr bool bReregisterWithFrontend = false;
					AssetSubsystem->RenameAsset(InAssetData, bReregisterWithFrontend);

					constexpr bool bForceViewSynchronization = true;
					UObject* AssetObject = InAssetData.GetAsset();
					FGraphBuilder::RegisterGraphWithFrontend(*AssetObject, bForceViewSynchronization);
				}
			}

			void RegisterInputDefaultClasses()
			{
				TSubclassOf<UMetasoundEditorGraphMemberDefaultLiteral> NodeClass;
				for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
				{
					UClass* Class = *ClassIt;
					if (!Class->IsNative())
					{
						continue;
					}

					if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
					{
						continue;
					}

					if (!ClassIt->IsChildOf(UMetasoundEditorGraphMemberDefaultLiteral::StaticClass()))
					{
						continue;
					}

					if (const UMetasoundEditorGraphMemberDefaultLiteral* DefaultLiteralCDO = Class->GetDefaultObject<UMetasoundEditorGraphMemberDefaultLiteral>())
					{
						InputDefaultLiteralClassRegistry.Add(DefaultLiteralCDO->GetLiteralType(), DefaultLiteralCDO->GetClass());
					}
				}
			}

			void RegisterCorePinTypes()
			{
				using namespace Metasound::Frontend;

				const IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();

				TArray<FName> DataTypeNames;
				DataTypeRegistry.GetRegisteredDataTypeNames(DataTypeNames);

				for (FName DataTypeName : DataTypeNames)
				{
					FDataTypeRegistryInfo RegistryInfo;
					if (ensure(DataTypeRegistry.GetDataTypeInfo(DataTypeName, RegistryInfo)))
					{
						FName PinCategory = DataTypeName;
						FName PinSubCategory;

						// Types like triggers & AudioBuffer are specialized, so ignore their preferred
						// literal types to classify the category.
						if (!FGraphBuilder::IsPinCategoryMetaSoundCustomDataType(PinCategory))
						{
							// Primitives
							switch (RegistryInfo.PreferredLiteralType)
							{
								case ELiteralType::Boolean:
								case ELiteralType::BooleanArray:
								{
									PinCategory = FGraphBuilder::PinCategoryBoolean;
								}
								break;

								case ELiteralType::Float:
								{
									PinCategory = FGraphBuilder::PinCategoryFloat;
								}
								break;

								case ELiteralType::FloatArray:
								{
									if (RegistryInfo.bIsArrayType)
									{
										PinCategory = FGraphBuilder::PinCategoryFloat;
									}
								}
								break;

								case ELiteralType::Integer:
								{
									PinCategory = FGraphBuilder::PinCategoryInt32;
								}
								break;

								case ELiteralType::IntegerArray:
								{
									if (RegistryInfo.bIsArrayType)
									{
										PinCategory = FGraphBuilder::PinCategoryInt32;
									}
								}
								break;

								case ELiteralType::String:
								{
									PinCategory = FGraphBuilder::PinCategoryString;
								}
								break;

								case ELiteralType::StringArray:
								{
									if (RegistryInfo.bIsArrayType)
									{
										PinCategory = FGraphBuilder::PinCategoryString;
									}
								}
								break;

								case ELiteralType::UObjectProxy:
								case ELiteralType::UObjectProxyArray:
								{
									PinCategory = FGraphBuilder::PinCategoryObject;
								}
								break;

								case ELiteralType::None:
								case ELiteralType::NoneArray:
								case ELiteralType::Invalid:
								default:
								{
									static_assert(static_cast<int32>(ELiteralType::Invalid) == 12, "Possible missing binding of pin category to primitive type");
								}
								break;
							}
						}

						RegisterPinType(DataTypeName, PinCategory, PinSubCategory);
					}
				}
			}

			void RegisterPinType(FName InDataTypeName, FName InPinCategory, FName InPinSubCategory)
			{
				using namespace Frontend;

				FDataTypeRegistryInfo DataTypeInfo;
				IDataTypeRegistry::Get().GetDataTypeInfo(InDataTypeName, DataTypeInfo);

				// Default to object as most calls to this outside of the MetaSound Editor will be for custom UObject types
				const FName PinCategory = InPinCategory.IsNone() ? FGraphBuilder::PinCategoryObject : InPinCategory;

				const EPinContainerType ContainerType = DataTypeInfo.bIsArrayType ? EPinContainerType::Array : EPinContainerType::None;
				FEdGraphPinType PinType(PinCategory, InPinSubCategory, nullptr, ContainerType, false, FEdGraphTerminalType());
				UClass* ClassToUse = IDataTypeRegistry::Get().GetUClassForDataType(InDataTypeName);
				PinType.PinSubCategoryObject = Cast<UObject>(ClassToUse);

				PinTypes.Emplace(InDataTypeName, MoveTemp(PinType));
			}

			void ShutdownAssetClassRegistry()
			{
				if (FAssetRegistryModule* AssetRegistryModule = static_cast<FAssetRegistryModule*>(FModuleManager::Get().GetModule("AssetRegistry")))
				{
					AssetRegistryModule->Get().OnAssetAdded().RemoveAll(this);
					AssetRegistryModule->Get().OnAssetUpdated().RemoveAll(this);
					AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
					AssetRegistryModule->Get().OnAssetRenamed().RemoveAll(this);
					AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);

					FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);
				}
			}

			virtual void PrimeAssetRegistryAsync() override
			{
				// Ignore step if still loading assets from initial scan but set prime status as requested.
				if (AssetScanStatus <= EAssetScanStatus::InProgress)
				{
					AssetPrimeStatus = EAssetPrimeStatus::Requested;
					return;
				}

				if (AssetPrimeStatus != EAssetPrimeStatus::InProgress)
				{
					AssetPrimeStatus = EAssetPrimeStatus::InProgress;

					FARFilter Filter;
					Filter.ClassPaths = MetaSoundClassNames;

					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
					AssetRegistryModule.Get().EnumerateAssets(Filter, [this](const FAssetData& AssetData)
					{
						AddClassRegistryAsset(AssetData);
						return true;
					});
				}
			}

			virtual EAssetPrimeStatus GetAssetRegistryPrimeStatus() const override
			{
				return AssetPrimeStatus;
			}

			virtual void RegisterExplicitProxyClass(const UClass& InClass) override
			{
				using namespace Metasound::Frontend;

				const IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();
				FDataTypeRegistryInfo RegistryInfo;
				ensureAlways(DataTypeRegistry.IsUObjectProxyFactory(InClass.GetDefaultObject()));

				ExplicitProxyClasses.Add(&InClass);
			}

			virtual bool IsExplicitProxyClass(const UClass& InClass) const override
			{
				return ExplicitProxyClasses.Contains(&InClass);
			}

			virtual TUniquePtr<FMetasoundDefaultLiteralCustomizationBase> CreateMemberDefaultLiteralCustomization(UClass& InClass, IDetailCategoryBuilder& InDefaultCategoryBuilder) const override
			{
				const TUniquePtr<IMemberDefaultLiteralCustomizationFactory>* CustomizationFactory = LiteralCustomizationFactories.Find(&InClass);
				if (CustomizationFactory && CustomizationFactory->IsValid())
				{
					return (*CustomizationFactory)->CreateLiteralCustomization(InDefaultCategoryBuilder);
				}

				return nullptr;
			}

			virtual const TSubclassOf<UMetasoundEditorGraphMemberDefaultLiteral> FindDefaultLiteralClass(EMetasoundFrontendLiteralType InLiteralType) const override
			{
				return InputDefaultLiteralClassRegistry.FindRef(InLiteralType);
			}

			virtual const FSlateBrush* GetIconBrush(FName InDataType, const bool bIsConstructorType) const override
			{
				Frontend::FDataTypeRegistryInfo Info;
				Frontend::IDataTypeRegistry::Get().GetDataTypeInfo(InDataType, Info);

				if (Info.bIsArrayType)
				{
					return bIsConstructorType ? &Style::GetSlateBrushSafe("MetasoundEditor.Graph.ConstructorPinArray") : &Style::GetSlateBrushSafe("MetasoundEditor.Graph.ArrayPin");
				}
				else
				{
					return bIsConstructorType ? &Style::GetSlateBrushSafe("MetasoundEditor.Graph.ConstructorPin") : FAppStyle::GetBrush("Icons.BulletPoint");
				}
			}

			virtual const FEdGraphPinType* FindPinType(FName InDataTypeName) const
			{
				return PinTypes.Find(InDataTypeName);
			}

			virtual bool IsMetaSoundAssetClass(const FTopLevelAssetPath& InClassName) const override
			{
				// TODO: Move to IMetasoundUObjectRegistry (overload IsRegisteredClass to take in class name?)
				return MetaSoundClassNames.Contains(InClassName);
			}

			virtual void StartupModule() override
			{
				METASOUND_LLM_SCOPE;
				// Register Metasound asset type actions
				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolName).Get();

				AddAssetAction<FAssetTypeActions_MetaSoundPatch>(AssetTools, AssetActions);
				AddAssetAction<FAssetTypeActions_MetaSoundSource>(AssetTools, AssetActions);

				FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
				
				PropertyModule.RegisterCustomClassLayout(
					UMetaSoundPatch::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundDetailCustomization>(UMetaSoundPatch::GetDocumentPropertyName()); }));

				PropertyModule.RegisterCustomClassLayout(
					UMetaSoundSource::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundDetailCustomization>(UMetaSoundSource::GetDocumentPropertyName()); }));

				PropertyModule.RegisterCustomClassLayout(
					UMetasoundInterfacesView::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundInterfacesDetailCustomization>(); }));

				PropertyModule.RegisterCustomClassLayout(
					UMetasoundEditorGraphInput::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundInputDetailCustomization>(); }));

				PropertyModule.RegisterCustomClassLayout(
					UMetasoundEditorGraphOutput::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundOutputDetailCustomization>(); }));

				PropertyModule.RegisterCustomClassLayout(
					UMetasoundEditorGraphVariable::StaticClass()->GetFName(),
					FOnGetDetailCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundVariableDetailCustomization>(); }));

				PropertyModule.RegisterCustomPropertyTypeLayout(
					"MetaSoundEditorGraphMemberDefaultBoolRef",
					FOnGetPropertyTypeCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundMemberDefaultBoolDetailCustomization>(); }));

				PropertyModule.RegisterCustomPropertyTypeLayout(
					"MetaSoundEditorGraphMemberDefaultIntRef",
					FOnGetPropertyTypeCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundMemberDefaultIntDetailCustomization>(); }));

				PropertyModule.RegisterCustomPropertyTypeLayout(
					"MetaSoundEditorGraphMemberDefaultObjectRef",
					FOnGetPropertyTypeCustomizationInstance::CreateLambda([]() { return MakeShared<FMetasoundMemberDefaultObjectDetailCustomization>(); }));

				LiteralCustomizationFactories.Add(UMetasoundEditorGraphMemberDefaultLiteral::StaticClass(), MakeUnique<FMetasoundDefaultLiteralCustomizationFactory>());
				LiteralCustomizationFactories.Add(UMetasoundEditorGraphMemberDefaultFloat::StaticClass(), MakeUnique<FMetasoundFloatLiteralCustomizationFactory>());
				LiteralCustomizationFactories.Add(UMetasoundEditorGraphMemberDefaultObjectArray::StaticClass(), MakeUnique<FMetasoundObjectArrayLiteralCustomizationFactory>());

				StyleSet = MakeShared<FSlateStyle>();

				RegisterCorePinTypes();
				RegisterInputDefaultClasses();

				GraphConnectionFactory = MakeShared<FGraphConnectionDrawingPolicyFactory>();
				FEdGraphUtilities::RegisterVisualPinConnectionFactory(GraphConnectionFactory);

				GraphNodeFactory = MakeShared<FMetasoundGraphNodeFactory>();
				FEdGraphUtilities::RegisterVisualNodeFactory(GraphNodeFactory);

				GraphPanelPinFactory = MakeShared<FMetasoundGraphPanelPinFactory>();
				FEdGraphUtilities::RegisterVisualPinFactory(GraphPanelPinFactory);

				ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");

				SettingsModule.RegisterSettings("Editor", "ContentEditors", "MetaSound Editor",
					NSLOCTEXT("MetaSoundsEditor", "MetaSoundEditorSettingsName", "MetaSound Editor"),
					NSLOCTEXT("MetaSoundsEditor", "MetaSoundEditorSettingsDescription", "Customize MetaSound Editor."),
					GetMutableDefault<UMetasoundEditorSettings>()
				);

				MetaSoundClassNames.Add(UMetaSoundPatch::StaticClass()->GetClassPathName());
				MetaSoundClassNames.Add(UMetaSoundSource::StaticClass()->GetClassPathName());

				// Required to query MetaSound assets (that have been redirected to MetaSoundPatch assets) created before UE Release 5.1
				MetaSoundClassNames.Add(FTopLevelAssetPath(TEXT("/Script/MetasoundEngine.MetaSound")));

				FAssetTypeActions_MetaSoundPatch::RegisterMenuActions();
				FAssetTypeActions_MetaSoundSource::RegisterMenuActions();

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				if (AssetRegistryModule.Get().IsLoadingAssets())
				{
					AssetScanStatus = EAssetScanStatus::InProgress;
					AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FModule::OnAssetScanFinished);
				}
				else
				{
					AssetScanStatus = EAssetScanStatus::Complete;
				}

				// Metasound Engine registers USoundWave as a proxy class in the
				// Metasound Frontend. The frontend registration must occur before
				// the Metasound Editor registration of a USoundWave.
				FModuleManager::LoadModuleChecked<IModuleInterface>("MetasoundEngine");

				RegisterExplicitProxyClass(*USoundWave::StaticClass());

				// Required to ensure logic to order nodes for presets exclusive to
				// editor is propagated to transform instances while editing in editor.
				Frontend::DocumentTransform::RegisterNodeDisplayNameProjection([](const Frontend::FNodeHandle& NodeHandle)
				{
					constexpr bool bIncludeNamespace = false;
					return FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
				});
			}

			virtual void ShutdownModule() override
			{
				METASOUND_LLM_SCOPE;

				if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
				{
					SettingsModule->UnregisterSettings("Editor", "Audio", "MetaSound Editor");
				}

				if (FModuleManager::Get().IsModuleLoaded(AssetToolName))
				{
					IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolName).Get();
					for (TSharedPtr<FAssetTypeActions_Base>& AssetAction : AssetActions)
					{
						AssetTools.UnregisterAssetTypeActions(AssetAction.ToSharedRef());
					}
				}

				if (GraphConnectionFactory.IsValid())
				{
					FEdGraphUtilities::UnregisterVisualPinConnectionFactory(GraphConnectionFactory);
				}

				if (GraphNodeFactory.IsValid())
				{
					FEdGraphUtilities::UnregisterVisualNodeFactory(GraphNodeFactory);
					GraphNodeFactory.Reset();
				}

				if (GraphPanelPinFactory.IsValid())
				{
					FEdGraphUtilities::UnregisterVisualPinFactory(GraphPanelPinFactory);
					GraphPanelPinFactory.Reset();
				}

				ShutdownAssetClassRegistry();

				AssetActions.Reset();
				PinTypes.Reset();
				MetaSoundClassNames.Reset();
			}

			TArray<FTopLevelAssetPath> MetaSoundClassNames;

			TArray<TSharedPtr<FAssetTypeActions_Base>> AssetActions;
			TMap<EMetasoundFrontendLiteralType, const TSubclassOf<UMetasoundEditorGraphMemberDefaultLiteral>> InputDefaultLiteralClassRegistry;
			TMap<FName, FEdGraphPinType> PinTypes;

			TMap<UClass*, TUniquePtr<IMemberDefaultLiteralCustomizationFactory>> LiteralCustomizationFactories;

			TSharedPtr<FMetasoundGraphNodeFactory> GraphNodeFactory;
			TSharedPtr<FGraphPanelPinConnectionFactory> GraphConnectionFactory;
			TSharedPtr<FMetasoundGraphPanelPinFactory> GraphPanelPinFactory;
			TSharedPtr<FSlateStyleSet> StyleSet;

			TSet<const UClass*> ExplicitProxyClasses;

			EAssetPrimeStatus AssetPrimeStatus = EAssetPrimeStatus::NotRequested;
			EAssetScanStatus AssetScanStatus = EAssetScanStatus::NotRequested;
			int32 ActiveAsyncAssetLoadRequests = 0;
		};
	} // namespace Editor
} // namespace Metasound

IMPLEMENT_MODULE(Metasound::Editor::FModule, MetasoundEditor);
