// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/Node.h"
#include "MuT/Visitor.h"
#include "MuT/AST.h"
#include "MuR/Operations.h"
#include "MuR/MemoryPrivate.h"

#include <memory>

namespace mu
{


    //!
    class Node::Private :
        public Base,
        public BaseVisitable<Ptr<class ASTOp>,DefaultCatchAll,true>
    {
    public:

        //! This is an opaque context used to attach to reported error messages.
		const void* m_errorContext = nullptr;

		//! Generic pointer to the node owning this private.
		const Node* m_pNode = nullptr;
    };



#define MUTABLE_IMPLEMENT_NODE( N, T, PN, PT )				\
    N::N()													\
    {														\
		Type = T;											\
        m_pD = new Private();								\
		m_pD->m_pNode = this;								\
		PN::Type = PT;										\
	}														\
                                                            \
    N::~N()													\
    {														\
        check( m_pD );										\
        delete m_pD;										\
        m_pD = nullptr;										\
    }														\
                                                            \
    void N::Serialise( const N* p, OutputArchive& arch )	\
    {														\
        arch << *p->m_pD;									\
    }														\
                                                            \
	void N::SerialiseWrapper( OutputArchive& arch ) const	\
	{														\
		N::Serialise(this, arch);							\
	}														\
                                                            \
    Ptr<N> N::StaticUnserialise( InputArchive& arch )		\
    {														\
        Ptr<N> pResult = new N();							\
        arch >> *pResult->m_pD;								\
        return pResult;										\
    }														\
                                                            \
    N::Private* N::GetPrivate() const						\
    {														\
        return m_pD;										\
    }														\
                                                            \
    Node::Private* N::GetBasePrivate() const				\
    {														\
        return m_pD;										\
    }														\
                                                            \
    const NODE_TYPE* N::GetType() const						\
    {														\
        return GetStaticType();								\
    }														\
                                                            \
    const NODE_TYPE* N::GetStaticType()						\
    {														\
        return &Private::s_type;							\
    }

}
