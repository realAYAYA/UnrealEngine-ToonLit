// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFrontendDocumentVersioning.h"

#include "Algo/Transform.h"
#include "Interfaces/MetasoundFrontendInterface.h"
#include "MetasoundAccessPtr.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendDocumentController.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"
#include "Misc/App.h"


namespace Metasound
{
	namespace Frontend
	{
		class FVersionDocumentTransform : public IDocumentTransform
		{
			protected:
				virtual FMetasoundFrontendVersionNumber GetTargetVersion() const = 0;
				virtual void TransformInternal(FDocumentHandle InDocument) const = 0;

			public:
				bool Transform(FDocumentHandle InDocument) const override
				{
					if (FMetasoundFrontendDocumentMetadata* Metadata = InDocument->GetMetadata())
					{
						const FMetasoundFrontendVersionNumber TargetVersion = GetTargetVersion();
						if (Metadata->Version.Number < TargetVersion)
						{
							TransformInternal(InDocument);
							Metadata->Version.Number = TargetVersion;
							return true;
						}
					}

					return false;
				}
		};

		/** Versions document from 1.0 to 1.1. */
		class FVersionDocument_1_1 : public FVersionDocumentTransform
		{
			FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_1(FName InName, const FString& InPath)
			: Name(InName)
			, Path(InPath)
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 1 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				FGraphHandle GraphHandle = InDocument->GetRootGraph();
				TArray<FNodeHandle> FrontendNodes = GraphHandle->GetNodes();

				// Before literals could be stored on node inputs directly, they were stored
				// by creating hidden input nodes. Update the doc by finding all hidden input
				// nodes, placing the literal value of the input node directly on the
				// downstream node's input. Then delete the hidden input node.
				for (FNodeHandle& NodeHandle : FrontendNodes)
				{
					const bool bIsHiddenNode = NodeHandle->GetNodeStyle().Display.Visibility == EMetasoundFrontendNodeStyleDisplayVisibility::Hidden;
					const bool bIsInputNode = EMetasoundFrontendClassType::Input == NodeHandle->GetClassMetadata().GetType();
					const bool bIsHiddenInputNode = bIsHiddenNode && bIsInputNode;

					if (bIsHiddenInputNode)
					{
						// Get literal value from input node.
						const FGuid VertexID = GraphHandle->GetVertexIDForInputVertex(NodeHandle->GetNodeName());
						const FMetasoundFrontendLiteral DefaultLiteral = GraphHandle->GetDefaultInput(VertexID);

						// Apply literal value to downstream node's inputs.
						TArray<FOutputHandle> OutputHandles = NodeHandle->GetOutputs();
						if (ensure(OutputHandles.Num() == 1))
						{
							FOutputHandle OutputHandle = OutputHandles[0];
							TArray<FInputHandle> Inputs = OutputHandle->GetConnectedInputs();
							OutputHandle->Disconnect();

							for (FInputHandle& Input : Inputs)
							{
								if (const FMetasoundFrontendLiteral* Literal = Input->GetClassDefaultLiteral())
								{
									if (!Literal->IsEqual(DefaultLiteral))
									{
										Input->SetLiteral(DefaultLiteral);
									}
								}
								else
								{
									Input->SetLiteral(DefaultLiteral);
								}
							}
						}
						GraphHandle->RemoveNode(*NodeHandle);
					}
				}
#else
				UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.1 to 1.2. */
		class FVersionDocument_1_2 : public FVersionDocumentTransform
		{
		private:
			const FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_2(const FName InName, const FString& InPath)
				: Name(InName)
				, Path(InPath)
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 2 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				const FMetasoundFrontendGraphClass& GraphClass = InDocument->GetRootGraphClass();
				FMetasoundFrontendClassMetadata Metadata = GraphClass.Metadata;

				Metadata.SetClassName({ "GraphAsset", Name, *Path });
				Metadata.SetDisplayName(FText::FromString(Name.ToString()));
				InDocument->GetRootGraph()->SetGraphMetadata(Metadata);
#else
				UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.2 to 1.3. */
		class FVersionDocument_1_3 : public FVersionDocumentTransform
		{
		public:
			FVersionDocument_1_3()
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return {1, 3};
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
				const FMetasoundFrontendGraphClass& GraphClass = InDocument->GetRootGraphClass();
				FMetasoundFrontendClassMetadata Metadata = GraphClass.Metadata;

				Metadata.SetClassName(FMetasoundFrontendClassName { FName(), *FGuid::NewGuid().ToString(), FName() });
				InDocument->GetRootGraph()->SetGraphMetadata(Metadata);
			}
		};

		/** Versions document from 1.3 to 1.4. */
		class FVersionDocument_1_4 : public FVersionDocumentTransform
		{
		public:
			FVersionDocument_1_4()
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return {1, 4};
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
				FMetasoundFrontendDocumentMetadata* Metadata = InDocument->GetMetadata();
				check(Metadata);
				check(Metadata->Version.Number.Major == 1);
				check(Metadata->Version.Number.Minor == 3);

				const TSet<FMetasoundFrontendVersion>& Interfaces = InDocument->GetInterfaceVersions();

				// Version 1.3 did not have an "InterfaceVersion" property on the
				// document, so any document that is being updated should start off
				// with an "Invalid" interface version.
				if (ensure(Interfaces.IsEmpty()))
				{
					// At the time when version 1.4 of the document was introduced, 
					// these were the only available interfaces. 
					static const FMetasoundFrontendVersion PreexistingInterfaceVersions[] = {
						FMetasoundFrontendVersion{"MetaSound", {1, 0}},
						FMetasoundFrontendVersion{"MonoSource", {1, 0}},
						FMetasoundFrontendVersion{"StereoSource", {1, 0}},
						FMetasoundFrontendVersion{"MonoSource", {1, 1}},
						FMetasoundFrontendVersion{"StereoSource", {1, 1}}
					};
					static const int32 NumPreexistingInterfaceVersions = sizeof(PreexistingInterfaceVersions) / sizeof(PreexistingInterfaceVersions[0]);

					TArray<FMetasoundFrontendInterface> CandidateInterfaces;
					IInterfaceRegistry& InterfaceRegistry = IInterfaceRegistry::Get();
					for (int32 i = 0; i < NumPreexistingInterfaceVersions; i++)
					{
						FMetasoundFrontendInterface Interface;
						if (InterfaceRegistry.FindInterface(GetInterfaceRegistryKey(PreexistingInterfaceVersions[i]), Interface))
						{
							CandidateInterfaces.Add(Interface);
						}
					}

					const FMetasoundFrontendGraphClass& RootGraph = InDocument->GetRootGraphClass();
					const TArray<FMetasoundFrontendClass>& Dependencies = InDocument->GetDependencies();
					const TArray<FMetasoundFrontendGraphClass>& Subgraphs = InDocument->GetSubgraphs();

					if (const FMetasoundFrontendInterface* Interface = FindMostSimilarInterfaceSupportingEnvironment(RootGraph, Dependencies, Subgraphs, CandidateInterfaces))
					{
						UE_LOG(LogMetaSound, Display, TEXT("Assigned interface [InterfaceVersion:%s] to document [RootGraphClassName:%s]"),
							*Interface->Version.ToString(), *RootGraph.Metadata.GetClassName().ToString());

						InDocument->AddInterfaceVersion(Interface->Version);
					}
					else
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Failed to find interface for document [RootGraphClassName:%s]"),
							*RootGraph.Metadata.GetClassName().ToString());
					}
				}
			}
		};

		/** Versions document from 1.4 to 1.5. */
		class FVersionDocument_1_5 : public FVersionDocumentTransform
		{
			FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_5(FName InName, const FString& InPath)
				: Name(InName)
				, Path(InPath)
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 5 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				const FMetasoundFrontendClassMetadata& Metadata = InDocument->GetRootGraphClass().Metadata;
				const FText NewAssetName = FText::FromString(Name.ToString());
				if (Metadata.GetDisplayName().CompareTo(NewAssetName) != 0)
				{
					FMetasoundFrontendClassMetadata NewMetadata = Metadata;
					NewMetadata.SetDisplayName(NewAssetName);
					InDocument->GetRootGraph()->SetGraphMetadata(NewMetadata);
				}
#else
				UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.5 to 1.6. */
		class FVersionDocument_1_6 : public FVersionDocumentTransform
		{
		public:
			FVersionDocument_1_6() = default;

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 6 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
				const FGuid NewAssetClassID = FGuid::NewGuid();
				FRenameRootGraphClass::Generate(InDocument, NewAssetClassID);
			}
		};

		/** Versions document from 1.6 to 1.7. */
		class FVersionDocument_1_7 : public FVersionDocumentTransform
		{
			FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_7(FName InName, const FString& InPath)
				: Name(InName)
				, Path(InPath)
			{
			}


			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 7 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				auto RenameTransform = [](FNodeHandle NodeHandle)
				{
					// Required nodes are all (at the point of this transform) providing
					// unique names and customized display names (ex. 'Audio' for both mono &
					// L/R output, On Play, & 'On Finished'), so do not replace them by nulling
					// out the guid as a name and using the converted FName of the FText DisplayName.
					if (!NodeHandle->IsInterfaceMember())
					{
						const FName NewNodeName = *NodeHandle->GetDisplayName().ToString();
						NodeHandle->IterateInputs([&](FInputHandle InputHandle)
						{
							InputHandle->SetName(NewNodeName);
						});

						NodeHandle->IterateOutputs([&](FOutputHandle OutputHandle)
						{
							OutputHandle->SetName(NewNodeName);
						});

						NodeHandle->SetDisplayName(FText());
						NodeHandle->SetNodeName(NewNodeName);
					}
				};

				InDocument->GetRootGraph()->IterateNodes(RenameTransform, EMetasoundFrontendClassType::Input);
				InDocument->GetRootGraph()->IterateNodes(RenameTransform, EMetasoundFrontendClassType::Output);
#else
				UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.7 to 1.8. */
		class FVersionDocument_1_8 : public FVersionDocumentTransform
		{
			FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_8(FName InName, const FString& InPath)
				: Name(InName)
				, Path(InPath)
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 8 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				// Do not serialize MetaData text for dependencies as
				// CacheRegistryData dynamically provides this.
				InDocument->IterateDependencies([](FMetasoundFrontendClass& Dependency)
				{
					constexpr bool bSerializeText = false;
					Dependency.Metadata.SetSerializeText(bSerializeText);

					for (FMetasoundFrontendClassInput& Input : Dependency.Interface.Inputs)
					{
						Input.Metadata.SetSerializeText(false);
					}

					for (FMetasoundFrontendClassOutput& Output : Dependency.Interface.Outputs)
					{
						Output.Metadata.SetSerializeText(false);
					}
				});

				const TSet<FMetasoundFrontendVersion>& InterfaceVersions = InDocument->GetInterfaceVersions();

				using FNameDataTypePair = TPair<FName, FName>;
				TSet<FNameDataTypePair> InterfaceInputs;
				TSet<FNameDataTypePair> InterfaceOutputs;

				for (const FMetasoundFrontendVersion& Version : InterfaceVersions)
				{
					FInterfaceRegistryKey RegistryKey = GetInterfaceRegistryKey(Version);
					const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(RegistryKey);
					if (ensure(Entry))
					{
						const FMetasoundFrontendInterface& Interface = Entry->GetInterface();
						Algo::Transform(Interface.Inputs, InterfaceInputs, [](const FMetasoundFrontendClassInput& Input)
						{
							return FNameDataTypePair(Input.Name, Input.TypeName);
						});

						Algo::Transform(Interface.Outputs, InterfaceOutputs, [](const FMetasoundFrontendClassOutput& Output)
						{
							return FNameDataTypePair(Output.Name, Output.TypeName);
						});
					}
				}

				// Only serialize MetaData text for inputs owned by the graph (not by interfaces)
				FMetasoundFrontendGraphClass RootGraphClass = InDocument->GetRootGraphClass();
				for (FMetasoundFrontendClassInput& Input : RootGraphClass.Interface.Inputs)
				{
					const bool bSerializeText = !InterfaceInputs.Contains(FNameDataTypePair(Input.Name, Input.TypeName));
					Input.Metadata.SetSerializeText(bSerializeText);
				}

				// Only serialize MetaData text for outputs owned by the graph (not by interfaces)
				for (FMetasoundFrontendClassOutput& Output : RootGraphClass.Interface.Outputs)
				{
					const bool bSerializeText = !InterfaceOutputs.Contains(FNameDataTypePair(Output.Name, Output.TypeName));
					Output.Metadata.SetSerializeText(bSerializeText);
				}

				InDocument->SetRootGraphClass(MoveTemp(RootGraphClass));
#else
			UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.8 to 1.9. */
		class FVersionDocument_1_9 : public FVersionDocumentTransform
		{
			FName Name;
			const FString& Path;

		public:
			FVersionDocument_1_9(FName InName, const FString& InPath)
				: Name(InName)
				, Path(InPath)
			{
			}

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 9 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
#if WITH_EDITOR
				// Display name text is no longer copied at this versioning point for assets
				// from the asset's FName to avoid FText warnings regarding generation from
				// an FString.  It also avoids desync if asset gets moved.
				FMetasoundFrontendGraphClass RootGraphClass = InDocument->GetRootGraphClass();
				RootGraphClass.Metadata.SetDisplayName(FText());
				InDocument->SetRootGraphClass(MoveTemp(RootGraphClass));
#else
				UE_LOG(LogMetaSound, Error, TEXT("Asset '%s' at '%s' must be saved with editor enabled in order to version document to target version '%s'."), *Name.ToString(), *Path, *GetTargetVersion().ToString());
#endif // !WITH_EDITOR
			}
		};

		/** Versions document from 1.9 to 1.10. */
		class FVersionDocument_1_10 : public FVersionDocumentTransform
		{
		public:
			FVersionDocument_1_10() = default;

			FMetasoundFrontendVersionNumber GetTargetVersion() const override
			{
				return { 1, 10 };
			}

			void TransformInternal(FDocumentHandle InDocument) const override
			{
				FMetasoundFrontendGraphClass Class = InDocument->GetRootGraphClass();
				FMetasoundFrontendGraphClassPresetOptions PresetOptions = Class.PresetOptions;
				Class.PresetOptions.bIsPreset = Class.Metadata.GetAndClearAutoUpdateManagesInterface_Deprecated();
				InDocument->SetRootGraphClass(MoveTemp(Class));
			}
		};

		FVersionDocument::FVersionDocument(FName InName, const FString& InPath)
			: Name(InName)
			, Path(InPath)
		{
		}

		bool FVersionDocument::Transform(FDocumentHandle InDocument) const
		{
			if (!ensure(InDocument->IsValid()))
			{
				return false;
			}

			bool bWasUpdated = false;

			FMetasoundFrontendDocumentMetadata* Metadata = InDocument->GetMetadata();
			check(Metadata);
			const FMetasoundFrontendVersionNumber InitVersionNumber = Metadata->Version.Number;

			// Add additional transforms here after defining them above, example below.
			bWasUpdated |= FVersionDocument_1_1(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_2(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_3().Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_4().Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_5(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_6().Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_7(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_8(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_9(Name, Path).Transform(InDocument);
			bWasUpdated |= FVersionDocument_1_10().Transform(InDocument);

			if (bWasUpdated)
			{
				const FMetasoundFrontendVersionNumber& NewVersionNumber = Metadata->Version.Number;
				UE_LOG(LogMetaSound, Display, TEXT("MetaSound at '%s' Document Versioned: '%s' --> '%s'"), *Path, *InitVersionNumber.ToString(), *NewVersionNumber.ToString());
			}

			return bWasUpdated;
		}
	} // namespace Frontend
} // namespace Metasound
