// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundTriggerCompareNode.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"

namespace Metasound
{
	namespace MetasoundTriggerCompareNodePrivate
	{
		FNodeClassMetadata CreateNodeClassMetadata(const FName& InDataTypeName, const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "TriggerCompare", InOperatorName, InDataTypeName },
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Trigger },
				{ METASOUND_LOCTEXT("EqualsKeyword", "="), METASOUND_LOCTEXT("NotEqualsKeyword", "!="), METASOUND_LOCTEXT("LessThanKeyword", "<"), METASOUND_LOCTEXT("GreaterThanKeyword", ">"), METASOUND_LOCTEXT("LessEqualsKeyword", "<="), METASOUND_LOCTEXT("GreaterEqualsKeyword", ">=")},
				FNodeDisplayStyle()
			};

			return Metadata;
		}
	}

	using FTriggerCompareNodeInt32 = TTriggerCompareNode<int32>;
	METASOUND_REGISTER_NODE(FTriggerCompareNodeInt32)

	using FTriggerCompareNodeFloat = TTriggerCompareNode<float>;
	METASOUND_REGISTER_NODE(FTriggerCompareNodeFloat)

	using FTriggerCompareNodeBool = TTriggerCompareNode<bool>;
	METASOUND_REGISTER_NODE(FTriggerCompareNodeBool)
}

#undef LOCTEXT_NAMESPACE // MetasoundTriggerDelayNode
