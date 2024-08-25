// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"

#include "MuT/NodeBool.h"
#include "MuT/NodeImage.h"
#include "MuT/NodeRange.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeBool::Private : public Node::Private
	{
	};


	class NodeBoolConstant::Private : public NodeBool::Private
	{
	public:

		static FNodeType s_type;

		bool m_value;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 0;
			arch << ver;

			arch << m_value;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver==0);

			arch >> m_value;
		}
	};


	class NodeBoolParameter::Private : public NodeBool::Private
	{
	public:

		static FNodeType s_type;

		bool m_defaultValue;
		FString m_name;
		FString m_uid;

        TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 3;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
			arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=2 && ver<=3);

			arch >> m_defaultValue;
			if (ver <= 2)
			{
				std::string Temp;
				arch >> Temp;
				m_name = Temp.c_str();
				arch >> Temp;
				m_uid = Temp.c_str();
			}
			else
			{
				arch >> m_name;
				arch >> m_uid;
			}
			
			arch >> m_ranges;
        }
	};


	class NodeBoolNot::Private : public NodeBool::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeBool> m_pSource;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 0;
			arch << ver;

			arch << m_pSource;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver<=0);

			arch >> m_pSource;
		}
	};


	class NodeBoolAnd::Private : public NodeBool::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeBool> m_pA;
		Ptr<NodeBool> m_pB;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 0;
			arch << ver;

			arch << m_pA;
			arch << m_pB;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver<=0);

			arch >> m_pA;
			arch >> m_pB;
		}
	};

}
