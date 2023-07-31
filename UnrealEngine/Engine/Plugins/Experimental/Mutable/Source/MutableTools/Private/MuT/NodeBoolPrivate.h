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

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		bool m_value;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_value;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_value;
		}
	};


	class NodeBoolParameter::Private : public NodeBool::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		bool m_defaultValue;
		string m_name;
		string m_uid;

        TArray<Ptr<NodeRange>> m_ranges;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 2;
			arch << ver;

			arch << m_defaultValue;
			arch << m_name;
			arch << m_uid;
            arch << m_ranges;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver==2);

			arch >> m_defaultValue;
			arch >> m_name;
            arch >> m_uid;
            arch >> m_ranges;
        }
	};


	class NodeBoolIsNull::Private : public NodeBool::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		NodePtr m_pSource;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pSource;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver<=0);

			arch >> m_pSource;
		}
	};


	class NodeBoolNot::Private : public NodeBool::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		NodeBoolPtr m_pSource;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pSource;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver<=0);

			arch >> m_pSource;
		}
	};


	class NodeBoolAnd::Private : public NodeBool::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		NodeBoolPtr m_pA;
		NodeBoolPtr m_pB;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pA;
			arch << m_pB;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
            check(ver<=0);

			arch >> m_pA;
			arch >> m_pB;
		}
	};

}
