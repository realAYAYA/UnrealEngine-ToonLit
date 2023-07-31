// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeColour.h"


namespace mu
{

	// Forward definitions
	class NodeColourArithmeticOperation;
	typedef Ptr<NodeColourArithmeticOperation> NodeColourArithmeticOperationPtr;
	typedef Ptr<const NodeColourArithmeticOperation> NodeColourArithmeticOperationPtrConst;


    //! Perform a per-component arithmetic operation between two colours.
	//! \ingroup model
	class MUTABLETOOLS_API NodeColourArithmeticOperation : public NodeColour
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeColourArithmeticOperation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeColourArithmeticOperation* pNode, OutputArchive& arch );
		static NodeColourArithmeticOperationPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		virtual const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		virtual int GetInputCount() const override;
		virtual Node* GetInputNode( int i ) const override;
		virtual void SetInputNode( int i, NodePtr pNode ) override;


		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Possible arithmetic operations
		typedef enum
		{
			AO_ADD,
			AO_SUBTRACT,
			AO_MULTIPLY,
            AO_DIVIDE,
            _AO_COUNT
		} OPERATION;

        static const char* s_opTypeName[ _AO_COUNT ];

		//! Get child selection type
		OPERATION GetOperation() const;

		//! Set the child selection type
		void SetOperation(OPERATION);


		//! Get the first operand
		NodeColourPtr GetA() const;
		void SetA(NodeColourPtr);

		// Get the second operand
		NodeColourPtr GetB() const;
		void SetB(NodeColourPtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeColourArithmeticOperation();

	private:

		Private* m_pD;

	};


}
