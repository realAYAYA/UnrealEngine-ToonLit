// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"


namespace mu
{

	// Forward definitions
	class NodeObject;
	typedef Ptr<NodeObject> NodeObjectPtr;
	typedef Ptr<const NodeObject> NodeObjectPtrConst;


    //! %Base class for any node that outputs an object.
	//! \ingroup model
	class MUTABLETOOLS_API NodeObject : public Node
	{
	public:

		// Possible subclasses
		enum class EType : uint8
		{
			New = 0,
			Group = 1,
			State = 2,

			None
		};

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		static void Serialise( const NodeObject* pNode, OutputArchive& arch );
		static NodeObjectPtr StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Get the name of the object.
		virtual const char* GetName() const = 0;

		//! Set the name of the object.
		virtual void SetName( const char* strName ) = 0;

		//! Get the uid of the object.
		virtual const char* GetUid() const = 0;

		//! Set the uid of the object.
		virtual void SetUid( const char* strUid ) = 0;

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		inline ~NodeObject() {}

		//!
		EType Type = EType::None;

	};


}
