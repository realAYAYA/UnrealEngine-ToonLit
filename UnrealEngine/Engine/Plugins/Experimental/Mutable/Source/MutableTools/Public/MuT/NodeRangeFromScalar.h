// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuT/Node.h"
#include "MuT/NodeRange.h"

namespace mu
{

	// Forward definitions
    class NodeScalar;


	//! 
	//! \ingroup model
    class MUTABLETOOLS_API NodeRangeFromScalar : public NodeRange
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        NodeRangeFromScalar();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeRangeFromScalar* pNode, OutputArchive& arch );
        static Ptr<NodeRangeFromScalar> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        int GetInputCount() const override;
        Node* GetInputNode( int i ) const override;
		void SetInputNode( int i, NodePtr pNode ) override;


		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

        //!
        Ptr<NodeScalar> GetSize() const;
        void SetSize( const Ptr<NodeScalar>& );

        //!
        const char* GetName() const;
        void SetName( const char* strName );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~NodeRangeFromScalar();

	private:

		Private* m_pD;

	};


}
