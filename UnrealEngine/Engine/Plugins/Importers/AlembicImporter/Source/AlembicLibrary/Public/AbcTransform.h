// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbcObject.h"

class ALEMBICLIBRARY_API FAbcTransform : public IAbcObject
{
public:
	FAbcTransform(const Alembic::AbcGeom::IXform& InTransform, const FAbcFile* InFile, IAbcObject* InParent = nullptr);
	virtual ~FAbcTransform() {}

	/** Begin IAbcObject overrides */
	virtual bool ReadFirstFrame(const float InTime, const int32 FrameIndex) final;
	virtual void SetFrameAndTime(const float InTime, const int32 FrameIndex, const EFrameReadFlags InFlags, const int32 TargetIndex = INDEX_NONE) final;
	virtual FMatrix GetMatrix(const int32 FrameIndex) const final;
	virtual bool HasConstantTransform() const final;
	virtual void PurgeFrameData(const int32 FrameIndex) final;
	/** End IAbcObject overrides */	
public:
	/** Flag whether or not this transformation object is identity constant */
	bool bConstantIdentity;
protected:
	/** Alembic representation of this object */
	const Alembic::AbcGeom::IXform Transform;
	/** Schema extracted from transform object  */
	const Alembic::AbcGeom::IXformSchema Schema;

	/** Initial value for this object in first frame with available data */
	FMatrix InitialValue;
	/** Resident set of matrix values for this object, used for parallel reading of samples/frames */
	FMatrix ResidentMatrices[MaxNumberOfResidentSamples];
};