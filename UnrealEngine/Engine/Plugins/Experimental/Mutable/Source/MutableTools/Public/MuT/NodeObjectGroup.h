// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeObject.h"


namespace mu
{

	// Forward definitions
	class NodeObjectGroup;
	typedef Ptr<NodeObjectGroup> NodeObjectGroupPtr;
	typedef Ptr<const NodeObjectGroup>NodeObjectGroupPtrConst;


	//! Node that creates a group of objects and describes how they are selected.
	//! \ingroup model
	class MUTABLETOOLS_API NodeObjectGroup : public NodeObject
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeObjectGroup();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise( const NodeObjectGroup* pNode, OutputArchive& arch );
		static NodeObjectGroupPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------
        const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// NodeObject Interface
		//-----------------------------------------------------------------------------------------
        virtual const FString& GetName() const override;
		virtual void SetName( const FString&) override;
		virtual const FString& GetUid() const override;
		virtual void SetUid( const FString&) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		//! Typed of child selection
		typedef enum
		{
			//! All objects in the group will always be enabled, and no parameter will be generated.
			CS_ALWAYS_ALL,

			//! Only one children may be selected, but it is allowed to have none.
			//! An enumeration parameter will be generated but it may have a null value
			CS_ONE_OR_NONE,

			//! One and only one children has to be selected at all times
			//! An enumeraation parameter will be generated and it cannot be null
			CS_ALWAYS_ONE,

			//! Each child in the group can be enabled or disabled individually.
			//! A boolean parameter will be generated for every child.
			CS_TOGGLE_EACH

		} CHILD_SELECTION;

		//! Get child selection type
		CHILD_SELECTION GetSelectionType() const;

		//! Set the child selection type
		void SetSelectionType( CHILD_SELECTION );

		//! Get the number of child objects
		int GetChildCount() const;

		//! Set the number of child objects
		void SetChildCount( int );

		//! Get a child object node
		NodeObjectPtr GetChild( int index ) const;

		//! Set a child object node
		void SetChild( int index, NodeObjectPtr );

		//! Set default value for CS_ONE_OR_NONE or CS_ALWAYS_ONE groups
		//! the value is the index of the child option in the group
		//! -1 is the value for the None option.
		//! 0 is the first child whether or not the NONE option is present.
		void SetDefaultValue(int32 Value);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
        Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeObjectGroup();

	private:

		Private* m_pD;

	};



}
