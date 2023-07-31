// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	class NodeColourConstant;
	typedef Ptr<NodeColourConstant> NodeColourConstantPtr;
	typedef Ptr<const NodeColourConstant> NodeColourConstantPtrConst;


	//! This node outputs a predefined colour value.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourConstant : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourConstant();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourConstant* pNode, OutputArchive& arch );
		static NodeColourConstantPtr StaticUnserialise( InputArchive& arch );

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        virtual int GetInputCount() const override;
        virtual Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the value that this node returns
		void GetValue( float* pR, float* pG, float* pB ) const;

		//! Set the value to be returned by this node
		void SetValue( float r, float g, float b );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourConstant();

	private:

		Private* m_pD;

	};


}
