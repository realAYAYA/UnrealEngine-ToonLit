// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageParameter.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeRange.h"


namespace mu
{

    class NodeImageParameter::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_name;
		string m_uid;

		TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 2;
			arch << ver;

			arch << m_name;
			arch << m_uid;
			arch << m_ranges;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver<=2);

			arch >> m_name;
            arch >> m_uid;

			if (ver >= 2)
			{
				arch >> m_ranges;
			}
		}
	};

}
