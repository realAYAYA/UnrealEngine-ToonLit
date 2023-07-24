// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendDocument.h"

#include "Algo/AnyOf.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "IAudioParameterInterfaceRegistry.h"
#include "Logging/LogMacros.h"
#include "MetasoundFrontendDocumentVersioning.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundLog.h"
#include "MetasoundVertex.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundFrontendDocument)

namespace Metasound
{
	const FGuid FrontendInvalidID = FGuid();

	namespace Frontend
	{
		namespace DisplayStyle
		{
			namespace EdgeAnimation
			{
				const FLinearColor DefaultColor = FLinearColor::Transparent;
			}

			namespace NodeLayout
			{
				const FVector2D DefaultOffsetX { 300.0f, 0.0f };
				const FVector2D DefaultOffsetY { 0.0f, 80.0f };
			} // namespace NodeLayout
		} // namespace DisplayStyle
	} // namespace Frontend

	namespace DocumentPrivate
	{
		/*
		 * Sets an array to a given array and updates the change ID if the array changed.
		 * @returns true if value changed, false if not.
		 */
		template <typename TElementType>
		bool SetWithChangeID(const TElementType& InNewValue, TElementType& OutValue, FGuid& OutChangeID)
		{
			if (OutValue != InNewValue)
			{
				OutValue = InNewValue;
				OutChangeID = FGuid::NewGuid();
				return true;
			}

			return false;
		}

		/* Array Text specialization as FText does not implement == nor does it support IsBytewiseComparable */
		template <>
		bool SetWithChangeID<TArray<FText>>(const TArray<FText>& InNewArray, TArray<FText>& OutArray, FGuid& OutChangeID)
		{
			bool bIsEqual = OutArray.Num() == InNewArray.Num();
			if (bIsEqual)
			{
				for (int32 i = 0; i < InNewArray.Num(); ++i)
				{
					bIsEqual &= InNewArray[i].IdenticalTo(OutArray[i]);
				}
			}

			if (!bIsEqual)
			{
				OutArray = InNewArray;
				OutChangeID = FGuid::NewGuid();
			}

			return !bIsEqual;
		}

		/* Text specialization as FText does not implement == nor does it support IsBytewiseComparable */
		template <>
		bool SetWithChangeID<FText>(const FText& InNewText, FText& OutText, FGuid& OutChangeID)
		{
			if (!InNewText.IdenticalTo(OutText))
			{
				OutText = InNewText;
				OutChangeID = FGuid::NewGuid();
				return true;
			}

			return false;
		}

		EMetasoundFrontendVertexAccessType CoreVertexAccessTypeToFrontendVertexAccessType(Metasound::EVertexAccessType InAccessType)
		{
			switch (InAccessType)
			{
				case EVertexAccessType::Value:
					return EMetasoundFrontendVertexAccessType::Value;

				case EVertexAccessType::Reference:
				default:
					return EMetasoundFrontendVertexAccessType::Reference;
			}
		}
	} // namespace DocumentPrivate
} // namespace Metasound

#if WITH_EDITORONLY_DATA
void FMetasoundFrontendDocumentModifyContext::ClearDocumentModified()
{
	bDocumentModified = false;
}

bool FMetasoundFrontendDocumentModifyContext::GetDocumentModified() const
{
	return bDocumentModified;
}

bool FMetasoundFrontendDocumentModifyContext::GetForceRefreshViews() const
{
	return bForceRefreshViews;
}

const TSet<FName>& FMetasoundFrontendDocumentModifyContext::GetInterfacesModified() const
{
	return InterfacesModified;
}

const TSet<FGuid>& FMetasoundFrontendDocumentModifyContext::GetMemberIDsModified() const
{
	return MemberIDsModified;
}

const TSet<FGuid>& FMetasoundFrontendDocumentModifyContext::GetNodeIDsModified() const
{
	return NodeIDsModified;
}

void FMetasoundFrontendDocumentModifyContext::Reset()
{
	bDocumentModified = false;
	bForceRefreshViews = false;
	InterfacesModified.Empty();
	MemberIDsModified.Empty();
	NodeIDsModified.Empty();
}

void FMetasoundFrontendDocumentModifyContext::SetDocumentModified()
{
	bDocumentModified = true;
}

void FMetasoundFrontendDocumentModifyContext::SetForceRefreshViews()
{
	bDocumentModified = true;
	bForceRefreshViews = true;
}

void FMetasoundFrontendDocumentModifyContext::AddInterfaceModified(FName InInterfaceModified)
{
	bDocumentModified = true;
	InterfacesModified.Add(InInterfaceModified);
}

void FMetasoundFrontendDocumentModifyContext::AddInterfacesModified(const TSet<FName>& InInterfacesModified)
{
	bDocumentModified = true;
	InterfacesModified.Append(InInterfacesModified);
}

void FMetasoundFrontendDocumentModifyContext::AddMemberIDModified(const FGuid& InMemberIDModified)
{
	bDocumentModified = true;
	MemberIDsModified.Add(InMemberIDModified);
}

void FMetasoundFrontendDocumentModifyContext::AddMemberIDsModified(const TSet<FGuid>& InMemberIDsModified)
{
	bDocumentModified = true;
	MemberIDsModified.Append(InMemberIDsModified);
}

void FMetasoundFrontendDocumentModifyContext::AddNodeIDModified(const FGuid& InNodeIDModified)
{
	bDocumentModified = true;
	NodeIDsModified.Add(InNodeIDModified);
}

void FMetasoundFrontendDocumentModifyContext::AddNodeIDsModified(const TSet<FGuid>& InNodesModified)
{
	bDocumentModified = true;
	NodeIDsModified.Append(InNodesModified);
}
#endif // WITH_EDITORONLY_DATA


FMetasoundFrontendNodeInterface::FMetasoundFrontendNodeInterface(const FMetasoundFrontendClassInterface& InClassInterface)
{
	for (const FMetasoundFrontendClassInput& Input : InClassInterface.Inputs)
	{
		Inputs.Add(Input);
	}

	for (const FMetasoundFrontendClassOutput& Output : InClassInterface.Outputs)
	{
		Outputs.Add(Output);
	}

	for (const FMetasoundFrontendClassEnvironmentVariable& EnvVar : InClassInterface.Environment)
	{
		FMetasoundFrontendVertex EnvVertex;
		EnvVertex.Name = EnvVar.Name;
		EnvVertex.TypeName = EnvVar.TypeName;

		Environment.Add(MoveTemp(EnvVertex));
	}
}

FMetasoundFrontendNode::FMetasoundFrontendNode(const FMetasoundFrontendClass& InClass)
: ClassID(InClass.ID)
, Name(InClass.Metadata.GetClassName().Name.ToString())
, Interface(InClass.Interface)
{

}

FString FMetasoundFrontendVersion::ToString() const
{
	return FString::Format(TEXT("{0} {1}"), { Name.ToString(), Number.ToString() });
}

bool FMetasoundFrontendVersion::IsValid() const
{
	return Number != GetInvalid().Number && Name != GetInvalid().Name;
}

const FMetasoundFrontendVersion& FMetasoundFrontendVersion::GetInvalid()
{
	static const FMetasoundFrontendVersion InvalidVersion { FName(), FMetasoundFrontendVersionNumber::GetInvalid() };
	return InvalidVersion;
}

bool FMetasoundFrontendVertex::IsFunctionalEquivalent(const FMetasoundFrontendVertex& InLHS, const FMetasoundFrontendVertex& InRHS)
{
	return (InLHS.Name == InRHS.Name) && (InLHS.TypeName == InRHS.TypeName);
}

void FMetasoundFrontendClassVertex::SplitName(FName& OutNamespace, FName& OutParameterName) const
{
	Audio::FParameterPath::SplitName(Name, OutNamespace, OutParameterName);
}

bool FMetasoundFrontendClassVertex::IsFunctionalEquivalent(const FMetasoundFrontendClassVertex& InLHS, const FMetasoundFrontendClassVertex& InRHS)
{
	return FMetasoundFrontendVertex::IsFunctionalEquivalent(InLHS, InRHS) && (InLHS.AccessType == InRHS.AccessType);
}

bool FMetasoundFrontendClassVertex::CanConnectVertexAccessTypes(EMetasoundFrontendVertexAccessType InFromType, EMetasoundFrontendVertexAccessType InToType)
{
	// Reroute nodes can have undefined access type, so if either is unset, then connection is valid.
	if (EMetasoundFrontendVertexAccessType::Unset != InFromType && EMetasoundFrontendVertexAccessType::Unset != InToType)
	{
		if (EMetasoundFrontendVertexAccessType::Value == InToType)
		{
			// If the input vertex accesses by "Value" then the output vertex 
			// must also access by "Value" to enforce unexpected consequences 
			// of connecting data which varies over time to an input which only
			// evaluates the data during operator initialization.
			return (EMetasoundFrontendVertexAccessType::Value == InFromType);
		}
	}

	return true;
}

FMetasoundFrontendClassName::FMetasoundFrontendClassName(const FName& InNamespace, const FName& InName, const FName& InVariant)
: Namespace(InNamespace)
, Name(InName)
, Variant(InVariant)
{
}

FMetasoundFrontendClassName::FMetasoundFrontendClassName(const Metasound::FNodeClassName& InName)
: FMetasoundFrontendClassName(InName.GetNamespace(), InName.GetName(), InName.GetVariant())
{
}

FName FMetasoundFrontendClassName::GetScopedName() const
{
	return Metasound::FNodeClassName::FormatScopedName(Namespace, Name);
}

FName FMetasoundFrontendClassName::GetFullName() const
{
	return Metasound::FNodeClassName::FormatFullName(Namespace, Name, Variant);
}

FString FMetasoundFrontendClassName::ToString() const
{
	return GetFullName().ToString();
}

bool operator==(const FMetasoundFrontendClassName& InLHS, const FMetasoundFrontendClassName& InRHS)
{
	return (InLHS.Namespace == InRHS.Namespace) && (InLHS.Name == InRHS.Name) && (InLHS.Variant == InRHS.Variant);
}

bool operator!=(const FMetasoundFrontendClassName& InLHS, const FMetasoundFrontendClassName& InRHS)
{
	return !(InLHS == InRHS);
}

FMetasoundFrontendClassInterface FMetasoundFrontendClassInterface::GenerateClassInterface(const Metasound::FVertexInterface& InVertexInterface)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetasoundFrontendClassInterface ClassInterface;

	// Copy over inputs
	{
		const FInputVertexInterface& InputInterface = InVertexInterface.GetInputInterface();

#if WITH_EDITOR
		FMetasoundFrontendInterfaceStyle InputStyle;
#endif // WITH_EDITOR

		// Reserve memory to minimize memory use in ClassInterface.Inputs array.
		ClassInterface.Inputs.Reserve(InputInterface.Num());


		for (const FInputDataVertex& InputVertex : InputInterface)
		{
			FMetasoundFrontendClassInput ClassInput;

			ClassInput.Name = InputVertex.VertexName;
			ClassInput.TypeName = InputVertex.DataTypeName;
			ClassInput.VertexID = FGuid::NewGuid();
			ClassInput.AccessType = Metasound::DocumentPrivate::CoreVertexAccessTypeToFrontendVertexAccessType(InputVertex.AccessType);


#if WITH_EDITOR
			const FDataVertexMetadata& VertexMetadata = InputVertex.Metadata;

			ClassInput.Metadata.SetSerializeText(false);
			ClassInput.Metadata.SetDisplayName(VertexMetadata.DisplayName);
			ClassInput.Metadata.SetDescription(VertexMetadata.Description);
			ClassInput.Metadata.bIsAdvancedDisplay = VertexMetadata.bIsAdvancedDisplay;

			// Advanced display items are pushed to bottom of sort order
			ClassInput.Metadata.SortOrderIndex = InputInterface.GetSortOrderIndex(InputVertex.VertexName);
			if (ClassInput.Metadata.bIsAdvancedDisplay)
			{
				ClassInput.Metadata.SortOrderIndex += InputInterface.Num();
			}
			InputStyle.DefaultSortOrder.Add(ClassInput.Metadata.SortOrderIndex);
#endif // WITH_EDITOR

			FLiteral DefaultLiteral = InputVertex.GetDefaultLiteral();
			if (DefaultLiteral.GetType() != ELiteralType::Invalid)
			{
				ClassInput.DefaultLiteral.SetFromLiteral(DefaultLiteral);
			}


			ClassInterface.Inputs.Add(MoveTemp(ClassInput));
		}

#if WITH_EDITOR
		// Must set via direct accessor to avoid updating the change GUID
		// (All instances of this generation call should be done for code
		// defined classes only, which do not currently create a persistent
		// change hash between builds and leave the guid 0'ed).
		ClassInterface.InputStyle = InputStyle;
#endif // WITH_EDITOR
	}

	// Copy over outputs
	{
		const FOutputVertexInterface& OutputInterface = InVertexInterface.GetOutputInterface();

#if WITH_EDITOR
		FMetasoundFrontendInterfaceStyle OutputStyle;
#endif // WITH_EDITOR

		// Reserve memory to minimize memory use in ClassInterface.Outputs array.
		ClassInterface.Outputs.Reserve(OutputInterface.Num());

		for (const FOutputDataVertex& OutputVertex: OutputInterface)
		{
			FMetasoundFrontendClassOutput ClassOutput;


			ClassOutput.Name = OutputVertex.VertexName;
			ClassOutput.TypeName = OutputVertex.DataTypeName;
			ClassOutput.VertexID = FGuid::NewGuid();
			ClassOutput.AccessType = Metasound::DocumentPrivate::CoreVertexAccessTypeToFrontendVertexAccessType(OutputVertex.AccessType);

#if WITH_EDITOR
			const FDataVertexMetadata& VertexMetadata = OutputVertex.Metadata;

			ClassOutput.Metadata.SetSerializeText(false);
			ClassOutput.Metadata.SetDisplayName(VertexMetadata.DisplayName);
			ClassOutput.Metadata.SetDescription(VertexMetadata.Description);
			ClassOutput.Metadata.bIsAdvancedDisplay = VertexMetadata.bIsAdvancedDisplay;

			// Advanced display items are pushed to bottom below non-advanced
			ClassOutput.Metadata.SortOrderIndex = OutputInterface.GetSortOrderIndex(OutputVertex.VertexName);
			if (ClassOutput.Metadata.bIsAdvancedDisplay)
			{
				ClassOutput.Metadata.SortOrderIndex += OutputInterface.Num();
			}
			OutputStyle.DefaultSortOrder.Add(ClassOutput.Metadata.SortOrderIndex);
#endif // WITH_EDITOR

			ClassInterface.Outputs.Add(MoveTemp(ClassOutput));
		}

#if WITH_EDITOR
		// Must set via direct accessor to avoid updating the change GUID
		// (All instances of this generation call should be done for code
		// defined classes only, which do not currently create a persistent
		// change hash between builds and leave the guid 0'ed).
		ClassInterface.OutputStyle = MoveTemp(OutputStyle);
#endif // WITH_EDITOR
	}

	// Reserve size to minimize memory use in ClassInterface.Environment array
	ClassInterface.Environment.Reserve(InVertexInterface.GetEnvironmentInterface().Num());

	for (const FEnvironmentVertex& EnvVertex : InVertexInterface.GetEnvironmentInterface())
	{
		FMetasoundFrontendClassEnvironmentVariable EnvVar;

		EnvVar.Name = EnvVertex.VertexName;
		EnvVar.bIsRequired = true;

		ClassInterface.Environment.Add(EnvVar);
	}

	return ClassInterface;
}

#if WITH_EDITOR
void FMetasoundFrontendClassMetadata::SetAuthor(const FString& InAuthor)
{
	using namespace Metasound::DocumentPrivate;

	SetWithChangeID(InAuthor, Author, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetCategoryHierarchy(const TArray<FText>& InCategoryHierarchy)
{
	using namespace Metasound::DocumentPrivate;

	TArray<FText>& TextToSet = bSerializeText ? CategoryHierarchy : CategoryHierarchyTransient;
	SetWithChangeID(InCategoryHierarchy, TextToSet, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetKeywords(const TArray<FText>& InKeywords)
{
	using namespace Metasound::DocumentPrivate;
	TArray<FText>& TextToSet = bSerializeText ? Keywords : KeywordsTransient;
	SetWithChangeID(InKeywords, TextToSet, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetDescription(const FText& InDescription)
{
	using namespace Metasound::DocumentPrivate;

	FText& TextToSet = bSerializeText ? Description : DescriptionTransient;
	SetWithChangeID(InDescription, TextToSet, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetDisplayName(const FText& InDisplayName)
{
	using namespace Metasound::DocumentPrivate;

	FText& TextToSet = bSerializeText ? DisplayName : DisplayNameTransient;
	SetWithChangeID(InDisplayName, TextToSet, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetIsDeprecated(bool bInIsDeprecated)
{
	using namespace Metasound::DocumentPrivate;
	SetWithChangeID(bInIsDeprecated, bIsDeprecated, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetPromptIfMissing(const FText& InPromptIfMissing)
{
	using namespace Metasound::DocumentPrivate;
	SetWithChangeID(InPromptIfMissing, PromptIfMissingTransient, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetSerializeText(bool bInSerializeText)
{
	if (bSerializeText)
	{
		if (!bInSerializeText)
		{
			DescriptionTransient = Description;
			DisplayNameTransient = DisplayName;

			Description = { };
			DisplayName = { };

			KeywordsTransient = MoveTemp(Keywords);
			CategoryHierarchyTransient = MoveTemp(CategoryHierarchy);
		}
	}
	else
	{
		if (bInSerializeText)
		{
			Description = DescriptionTransient;
			DisplayName = DisplayNameTransient;

			DescriptionTransient = { };
			DisplayNameTransient = { };

			Keywords = MoveTemp(KeywordsTransient);
			CategoryHierarchy = MoveTemp(CategoryHierarchyTransient);
		}
	}

	bSerializeText = bInSerializeText;
}
#endif // WITH_EDITOR

void FMetasoundFrontendClassMetadata::SetVersion(const FMetasoundFrontendVersionNumber& InVersion)
{
	using namespace Metasound::DocumentPrivate;
	SetWithChangeID(InVersion, Version, ChangeID);
}

void FMetasoundFrontendClassMetadata::SetClassName(const FMetasoundFrontendClassName& InClassName)
{
	using namespace Metasound::DocumentPrivate;
	SetWithChangeID(InClassName, ClassName, ChangeID);
}

#if WITH_EDITOR
bool FMetasoundFrontendClass::CacheGraphDependencyMetadataFromRegistry(FMetasoundFrontendClass& InOutDependency)
{
	using namespace Metasound::Frontend;

	const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(InOutDependency.Metadata);
	FMetasoundFrontendClass RegistryClass;

	FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get();
	if (ensure(Registry))
	{
		if (Registry->FindFrontendClassFromRegistered(Key, RegistryClass))
		{
			InOutDependency.Metadata = RegistryClass.Metadata;
			InOutDependency.Style = RegistryClass.Style;

			using FNameTypeKey = TPair<FName, FName>;
			using FVertexMetadataMap = TMap<FNameTypeKey, const FMetasoundFrontendVertexMetadata*>;
			auto MakePairFromVertex = [](const FMetasoundFrontendClassVertex& InVertex)
			{
				const FNameTypeKey Key(InVertex.Name, InVertex.TypeName);
				return TPair<FNameTypeKey, const FMetasoundFrontendVertexMetadata*> { Key, &InVertex.Metadata };
			};

			auto AddRegistryVertexMetadata = [](const FVertexMetadataMap& InInterfaceMembers, FMetasoundFrontendClassVertex& OutVertex, FMetasoundFrontendInterfaceStyle& OutNewStyle)
			{
				const FNameTypeKey Key(OutVertex.Name, OutVertex.TypeName);
				if (const FMetasoundFrontendVertexMetadata* RegVertex = InInterfaceMembers.FindRef(Key))
				{
					OutVertex.Metadata = *RegVertex;
					OutVertex.Metadata.SetSerializeText(false);
				}
				OutNewStyle.DefaultSortOrder.Add(OutVertex.Metadata.SortOrderIndex);
			};

			FMetasoundFrontendInterfaceStyle InputStyle;
			FVertexMetadataMap InputMembers;
			Algo::Transform(RegistryClass.Interface.Inputs, InputMembers, [&](const FMetasoundFrontendClassInput& Input) { return MakePairFromVertex(Input); });
			Algo::ForEach(InOutDependency.Interface.Inputs, [&](FMetasoundFrontendClassInput& Input)
			{
				AddRegistryVertexMetadata(InputMembers, Input, InputStyle);
			});
			InOutDependency.Interface.SetInputStyle(InputStyle);

			FMetasoundFrontendInterfaceStyle OutputStyle;
			FVertexMetadataMap OutputMembers;
			Algo::Transform(RegistryClass.Interface.Outputs, OutputMembers, [&](const FMetasoundFrontendClassOutput& Output) { return MakePairFromVertex(Output); });
			Algo::ForEach(InOutDependency.Interface.Outputs, [&](FMetasoundFrontendClassOutput& Output)
			{
				AddRegistryVertexMetadata(OutputMembers, Output, OutputStyle);
			});
			InOutDependency.Interface.SetOutputStyle(OutputStyle);

			return true;
		}
	}

	return false;
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
FMetasoundFrontendClassStyle FMetasoundFrontendClassStyle::GenerateClassStyle(const Metasound::FNodeDisplayStyle& InNodeDisplayStyle)
{
	FMetasoundFrontendClassStyle Style;

	Style.Display.bShowName = InNodeDisplayStyle.bShowName;
	Style.Display.bShowInputNames = InNodeDisplayStyle.bShowInputNames;
	Style.Display.bShowOutputNames = InNodeDisplayStyle.bShowOutputNames;
	Style.Display.ImageName = InNodeDisplayStyle.ImageName;

	return Style;
}
#endif // WITH_EDITORONLY_DATA

FMetasoundFrontendClassMetadata FMetasoundFrontendClassMetadata::GenerateClassMetadata(const Metasound::FNodeClassMetadata& InNodeClassMetadata, EMetasoundFrontendClassType InType)
{
	FMetasoundFrontendClassMetadata NewMetadata;

	NewMetadata.Type = InType;

	// TODO: This flag is only used by the graph class' metadata.
	// Should probably be moved elsewhere (AssetBase?) as to not
	// get confused with behavior encapsulated on registry class
	// descriptions/individual node class dependencies.
	NewMetadata.bAutoUpdateManagesInterface = false;

	NewMetadata.ClassName = InNodeClassMetadata.ClassName;
	NewMetadata.Version = { InNodeClassMetadata.MajorVersion, InNodeClassMetadata.MinorVersion };

#if WITH_EDITOR
	NewMetadata.SetSerializeText(false);
	NewMetadata.SetDisplayName(InNodeClassMetadata.DisplayName);
	NewMetadata.SetDescription(InNodeClassMetadata.Description);
	NewMetadata.SetPromptIfMissing(InNodeClassMetadata.PromptIfMissing);
	NewMetadata.SetAuthor(InNodeClassMetadata.Author);
	NewMetadata.SetKeywords(InNodeClassMetadata.Keywords);
	NewMetadata.SetCategoryHierarchy(InNodeClassMetadata.CategoryHierarchy);

	NewMetadata.bIsDeprecated = InNodeClassMetadata.bDeprecated;
#endif // WITH_EDITOR

	return NewMetadata;
}

FMetasoundFrontendClassInput::FMetasoundFrontendClassInput(const FMetasoundFrontendClassVertex& InOther)
:	FMetasoundFrontendClassVertex(InOther)
{
	using namespace Metasound::Frontend;

	EMetasoundFrontendLiteralType DefaultType = GetMetasoundFrontendLiteralType(IDataTypeRegistry::Get().GetDesiredLiteralType(InOther.TypeName));

	DefaultLiteral.SetType(DefaultType);
}

FMetasoundFrontendClassVariable::FMetasoundFrontendClassVariable(const FMetasoundFrontendClassVertex& InOther)
	: FMetasoundFrontendClassVertex(InOther)
{
	using namespace Metasound::Frontend;

	EMetasoundFrontendLiteralType DefaultType = GetMetasoundFrontendLiteralType(IDataTypeRegistry::Get().GetDesiredLiteralType(InOther.TypeName));

	DefaultLiteral.SetType(DefaultType);
}

FMetasoundFrontendGraphClass::FMetasoundFrontendGraphClass()
{
	Metadata.SetType(EMetasoundFrontendClassType::Graph);
}

FMetasoundFrontendVersionNumber FMetasoundFrontendDocument::GetMaxVersion()
{
	return Metasound::Frontend::FVersionDocument::GetMaxVersion();
}

FMetasoundFrontendDocument::FMetasoundFrontendDocument()
{
	RootGraph.ID = FGuid::NewGuid();
	RootGraph.Metadata.SetType(EMetasoundFrontendClassType::Graph);
	ArchetypeVersion = FMetasoundFrontendVersion::GetInvalid();
}

const TCHAR* LexToString(EMetasoundFrontendVertexAccessType InVertexAccess)
{
	switch (InVertexAccess)
	{
		case EMetasoundFrontendVertexAccessType::Value:
			return TEXT("Value");
			
		case EMetasoundFrontendVertexAccessType::Reference:
			return TEXT("Reference");

		case EMetasoundFrontendVertexAccessType::Unset:
		default:
			return TEXT("Unset");
	}
}

