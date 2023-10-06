// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeImage.h"


namespace mu
{

	// Forward definitions
	class InputArchive;
	class OutputArchive;


	/** Node that outputs an "image reference".
	*/
	class MUTABLETOOLS_API NodeImageReference : public NodeImage
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		NodeImageReference();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise(const NodeImageReference* pNode, OutputArchive& arch);
		static Ptr<NodeImageReference> StaticUnserialise(InputArchive& arch);

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------

		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		virtual int GetInputCount() const override;
		virtual Node* GetInputNode(int i) const override;
		void SetInputNode(int i, Ptr<Node> pNode) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------

		/** Set this node value to be an "image reference" (to point to an unreal engine image). */
		void SetImageReference(uint32 ID);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeImageReference();

	private:

		Private* m_pD;

	};

}
