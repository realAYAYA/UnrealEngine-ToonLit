// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/System.h"
#include "MuT/Node.h"
#include "MuT/NodeObject.h"
#include "MuT/NodeExtensionData.h"


namespace mu
{
	class NodeLOD;

	//! Node that creates a new object by setting its levels-of-detail and its children.
	//! \ingroup model
	class MUTABLETOOLS_API NodeObjectNew : public NodeObject
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeObjectNew();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeObjectNew* pNode, OutputArchive& arch );
		static Ptr<NodeObjectNew> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------
        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// NodeObject Interface
		//-----------------------------------------------------------------------------------------
        virtual const FString& GetName() const override;
		virtual void SetName( const FString& ) override;
		virtual const FString& GetUid() const override;
		virtual void SetUid( const FString& ) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Get the number of levels of detail in the object.
		int GetLODCount() const;
		void SetLODCount( int );

		//! Get a level of detail node from the object.
		Ptr<NodeLOD> GetLOD( int index ) const;
		void SetLOD( int index, Ptr<NodeLOD>);

		//! Get the number of child objects
		int GetChildCount() const;
		void SetChildCount( int );

		//! Get a child object node
		NodeObjectPtr GetChild( int index ) const;
		void SetChild( int index, NodeObjectPtr );

		//! Set the number of states that the model can be in.
		int32 GetStateCount() const;
		void SetStateCount( int32 c );

		//! Set the name of a state
		void SetStateName( int32 s, const FString& n );

		//! See if a state has a parameter as runtime.
		bool HasStateParam( int32 s, const FString& param ) const;

		//! Add a runtime parameter to the state.
		void AddStateParam( int32 s, const FString& param );

		//! Remove a runtime parameter from a state.
		void RemoveStateParam( int32 s, const FString& param );

        //! Set the optimisation properties of a state
		void SetStateProperties(int32 StateIndex,
			ETextureCompressionStrategy TextureCompressionStrategy,
			bool bOnlyFirstLOD,
			uint8 FirstLOD,
			uint8 NumExtraLODsToBuildAfterFirstLOD);

		//! Connect a node that produces ExtensionData to be added to the final Instance, and provide a name to associate with the data
		void AddExtensionDataNode(NodeExtensionDataPtr Node, const FString& Name);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeObjectNew();

	private:

		Private* m_pD;

	};



}
