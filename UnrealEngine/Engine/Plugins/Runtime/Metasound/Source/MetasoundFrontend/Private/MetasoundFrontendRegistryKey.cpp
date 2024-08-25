// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistryKey.h"


namespace Metasound::Frontend
{
	namespace NodeClassInfoPrivate
	{
		auto GetVertexTypeName = [](const FMetasoundFrontendVertex& Vertex) { return Vertex.TypeName; };
	}

	FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendClassMetadata& InMetadata)
		: ClassName(InMetadata.GetClassName())
		, Type(InMetadata.GetType())
		, Version(InMetadata.GetVersion())
	{
	}

	FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FSoftObjectPath& InAssetPath)
		: ClassName(InClass.Metadata.GetClassName())
		, Type(EMetasoundFrontendClassType::External) // Overridden as it is considered the same as an external class in registries
		, AssetClassID(FGuid(ClassName.Name.ToString()))
		, Version(InClass.Metadata.GetVersion())
	{
		using namespace NodeClassInfoPrivate;

		ensure(AssetPath.TrySetPath(InAssetPath.ToString()));
		ensure(!AssetPath.IsNull());

#if WITH_EDITORONLY_DATA
		Algo::Transform(InClass.Interface.Inputs, InputTypes, GetVertexTypeName);
		Algo::Transform(InClass.Interface.Outputs, OutputTypes, GetVertexTypeName);
		bIsPreset = InClass.PresetOptions.bIsPreset;
#endif // WITH_EDITORONLY_DATA
	}

	FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FTopLevelAssetPath& InAssetPath)
		: ClassName(InClass.Metadata.GetClassName())
		, Type(EMetasoundFrontendClassType::External) // Overridden as it is considered the same as an external class in registries
		, AssetClassID(FGuid(ClassName.Name.ToString()))
		, AssetPath(InAssetPath)
		, Version(InClass.Metadata.GetVersion())
	{
		using namespace NodeClassInfoPrivate;

		ensure(!AssetPath.IsNull());

#if WITH_EDITORONLY_DATA
		Algo::Transform(InClass.Interface.Inputs, InputTypes, GetVertexTypeName);
		Algo::Transform(InClass.Interface.Outputs, OutputTypes, GetVertexTypeName);
		bIsPreset = InClass.PresetOptions.bIsPreset;
#endif // WITH_EDITORONLY_DATA
	}

	UObject* FNodeClassInfo::LoadAsset() const
	{
		return nullptr;
	}

	FNodeRegistryKey::FNodeRegistryKey(EMetasoundFrontendClassType InType, const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion, int32 InMinorVersion)
		: Type(InType)
		, ClassName(InClassName)
		, Version({ InMajorVersion, InMinorVersion })
	{
	}

	FNodeRegistryKey::FNodeRegistryKey(EMetasoundFrontendClassType InType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InVersion)
		: Type(InType)
		, ClassName(InClassName)
		, Version(InVersion)
	{
	}

	FNodeRegistryKey::FNodeRegistryKey(const FNodeClassMetadata& InNodeMetadata)
		: Type(EMetasoundFrontendClassType::External) // Overridden as it is considered the same as an external class in registries
		, ClassName(InNodeMetadata.ClassName)
		, Version({ InNodeMetadata.MajorVersion, InNodeMetadata.MinorVersion })
	{
	}

	FNodeRegistryKey::FNodeRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
		: Type(InNodeMetadata.GetType())
		, ClassName(InNodeMetadata.GetClassName())
		, Version(InNodeMetadata.GetVersion())
	{
		checkf(InNodeMetadata.GetType() != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use FNodeRegistryKey ctor that is provided FMetasoundFrontendGraphClass"));
	}

	FNodeRegistryKey::FNodeRegistryKey(const FMetasoundFrontendGraphClass& InGraphClass)
		: Type(EMetasoundFrontendClassType::External) // Type overridden as all graphs are considered the same as an external class in the registry
		, ClassName(InGraphClass.Metadata.GetClassName())
		, Version(InGraphClass.Metadata.GetVersion())
	{
	}

	FNodeRegistryKey::FNodeRegistryKey(const FNodeClassInfo& InClassInfo)
		: Type(InClassInfo.Type)
		, ClassName(InClassInfo.ClassName)
		, Version(InClassInfo.Version)
	{
		checkf(InClassInfo.Type != EMetasoundFrontendClassType::Graph, TEXT("Cannot create key from 'graph' type. Likely meant to use FNodeRegistryKey ctor that is provided FMetasoundFrontendGraphClass"));
	}

	FNodeRegistryKey::FNodeRegistryKey(const FString& InKeyString)
	{
		Parse(InKeyString, *this);
	}

	const FNodeRegistryKey& FNodeRegistryKey::GetInvalid()
	{
		static const FNodeRegistryKey InvalidKey;
		return InvalidKey;
	}

	bool FNodeRegistryKey::IsValid() const
	{
		return Type != EMetasoundFrontendClassType::Invalid && ClassName.IsValid() && Version.IsValid();
	}

	void FNodeRegistryKey::Reset()
	{
		Type = EMetasoundFrontendClassType::Invalid;
		ClassName = { };
		Version = { };
	}

	FString FNodeRegistryKey::ToString() const
	{
		TStringBuilder<128> KeyStringBuilder;
		KeyStringBuilder.Append(LexToString(Type));
		KeyStringBuilder.AppendChar('_');
		KeyStringBuilder.Append(ClassName.GetFullName().ToString());
		KeyStringBuilder.AppendChar('_');
		KeyStringBuilder.Append(FString::FromInt(Version.Major));
		KeyStringBuilder.AppendChar('.');
		KeyStringBuilder.Append(FString::FromInt(Version.Minor));
		return KeyStringBuilder.ToString();
	}

	FString FNodeRegistryKey::ToString(const FString& InScopeHeader) const
	{
		checkf(InScopeHeader.Len() < 128, TEXT("Scope text is limited to 128 characters"));

		TStringBuilder<256> Builder; // 128 for key and 128 for scope text

		Builder.Append(InScopeHeader);
		Builder.Append(TEXT(" ["));
		Builder.Append(ToString());
		Builder.Append(TEXT(" ]"));
		return Builder.ToString();
	}

	bool FNodeRegistryKey::Parse(const FString& InKeyString, FNodeRegistryKey& OutKey)
	{
		TArray<FString> Tokens;
		InKeyString.ParseIntoArray(Tokens, TEXT("_"));
		if (Tokens.Num() == 3)
		{
			EMetasoundFrontendClassType Type;
			if (Metasound::Frontend::StringToClassType(Tokens[0], Type))
			{
				FMetasoundFrontendClassName ClassName;
				if (FMetasoundFrontendClassName::Parse(Tokens[1], ClassName))
				{
					FMetasoundFrontendVersionNumber Version;
					FString MajorVersionString;
					FString MinorVersionString;
					if (Tokens[2].Split(TEXT("."), &MajorVersionString, &MinorVersionString))
					{
						Version.Major = FCString::Atoi(*MajorVersionString);
						Version.Minor = FCString::Atoi(*MinorVersionString);

						OutKey = FNodeRegistryKey(Type, ClassName, Version.Major, Version.Minor);
						return true;
					}
				}
			}
		}

		return false;
	}

	FString FGraphRegistryKey::ToString() const
	{
		TStringBuilder<256> Builder;
		Builder.Append(NodeKey.ToString());
		Builder.Append(TEXT(", "));
		Builder.Append(AssetPath.GetPackageName().ToString());
		Builder.Append(TEXT("/"));
		Builder.Append(AssetPath.GetAssetName().ToString());
		return Builder.ToString();
	}

	FString FGraphRegistryKey::ToString(const FString& InScopeHeader) const
	{
		TStringBuilder<512> Builder;
		Builder.Append(InScopeHeader);
		Builder.Append(TEXT(" ["));
		Builder.Append(ToString());
		Builder.Append(TEXT(" ]"));
		return Builder.ToString();
	}

	bool FGraphRegistryKey::IsValid() const
	{
		return NodeKey.IsValid() && AssetPath.IsValid();
	}
} // namespace Metasound::Frontend
