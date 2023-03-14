// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeObject.h"


namespace mu
{

	//! Forward definitions
	class NodeObjectState;
	typedef Ptr<NodeObjectState> NodeObjectStatePtr;
	typedef Ptr<const NodeObjectState> NodeObjectStatePtrConst;



	//!
	//! \ingroup model
	class MUTABLETOOLS_API NodeObjectState : public NodeObject
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeObjectState();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeObjectState* pNode, OutputArchive& arch );
		static NodeObjectStatePtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node interface
		//-----------------------------------------------------------------------------------------

        

        const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

        int GetInputCount() const override;
        Node* GetInputNode( int i ) const override;
        void SetInputNode( int i, NodePtr pNode ) override;


		//-----------------------------------------------------------------------------------------
		// NodeObject interface
		//-----------------------------------------------------------------------------------------
        const char* GetName() const override;
        void SetName( const char* strName ) override;
        const char* GetUid() const override;
        void SetUid( const char* strUid ) override;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Source object
		NodeObjectPtr GetSource() const;
		void SetSource( NodeObjectPtr );

		//! State diagram to add
		NodeObjectPtr GetStateRoot() const;
		void SetStateRoot( NodeObjectPtr );

		//! Set the name of a state
		const char* GetStateName() const;
		void SetStateName( const char* n );

		//! State optimization options
		void SetStateGPUOptimisation( bool internal, bool external, bool returnGPU = false );
		bool GetStateGPUOptimisationInternal() const;
		bool GetStateGPUOptimisationExternal() const;
		bool GetStateGPUOptimisationReturnImages() const;

		//! See if a state has a parameter as runtime.
		bool HasStateParam( const char* param ) const;

		//! Add a runtime parameter to the state.
		void AddStateParam( const char* param );

		//! Remove a runtime parameter from a state.
		void RemoveStateParam( const char* param );

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeObjectState();

	private:

		Private* m_pD;

	};



}
