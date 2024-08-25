// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeModifier.h"

#include <string>


namespace mu
{

	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMutableMultipleTagPolicy);

	class NodeModifier::Private : public Node::Private
	{
	public:

		/** Tags that target surface need to have enabled to receive this modifier. */
		TArray<FString> RequiredTags;

		/** In case of multiple tags in RequiredTags: are they all required, or one is enough? */
		EMutableMultipleTagPolicy MultipleTagsPolicy = EMutableMultipleTagPolicy::OnlyOneRequired;

		// Wether the modifier has to be applied after the normal node operations or before
		bool bApplyBeforeNormalOperations = true;

		//!
		void Serialise(OutputArchive& arch) const
		{
            uint32_t ver = 4;
			arch << ver;

			arch << RequiredTags;
			arch << MultipleTagsPolicy;
			arch << bApplyBeforeNormalOperations;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
            uint32_t ver;
			arch >> ver;
			check(ver>=2 && ver<=3);

			if (ver <= 2)
			{
				TArray<std::string> Temp;
				arch >> Temp;
				RequiredTags.SetNum(Temp.Num());
				for( int32 i=0; i<Temp.Num(); ++i)
				{
					RequiredTags[i] = Temp[i].c_str();
				}
			}
			else
			{
				arch >> RequiredTags;
			}

			if (ver >= 4)
			{
				arch >> MultipleTagsPolicy;
			}

			arch >> bApplyBeforeNormalOperations;
		}

	};

}
