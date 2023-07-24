// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IncludePython.h"
#include "PyPtr.h"
#include "PyGenUtil.h"
#include "PyConversionMethod.h"
#include "PyConversionResult.h"
#include "Misc/Guid.h"
#include "UObject/GCObject.h"
#include "UObject/Interface.h"
#include "PyWrapperBase.generated.h"

#if WITH_PYTHON

/** Python type for FPyWrapperBase */
extern PyTypeObject PyWrapperBaseType;

/** Initialize the PyWrapperBase types and add them to the given Python module */
void InitializePyWrapperBase(PyGenUtil::FNativePythonModule& ModuleInfo);

/** Base type for all Unreal exposed instances */
struct FPyWrapperBase
{
	/** Common Python Object */
	PyObject_HEAD

	/** New this wrapper instance (called via tp_new for Python, or directly in C++) */
	static FPyWrapperBase* New(PyTypeObject* InType);

	/** Free this wrapper instance (called via tp_dealloc for Python) */
	static void Free(FPyWrapperBase* InSelf);

	/** Initialize this wrapper instance (called via tp_init for Python, or directly in C++) */
	static int Init(FPyWrapperBase* InSelf);

	/** Deinitialize this wrapper instance (called via Init and Free to restore the instance to its New state) */
	static void Deinit(FPyWrapperBase* InSelf);
};

#define PY_OVERRIDE_GETSET_METADATA(TYPE)																						\
	static void SetMetaData(PyTypeObject* PyType, TYPE* MetaData) { FPyWrapperBaseMetaData::SetMetaData(PyType, MetaData); }	\
	static TYPE* GetMetaData(PyTypeObject* PyType) { return (TYPE*)FPyWrapperBaseMetaData::GetMetaData(PyType); }				\
	static TYPE* GetMetaData(FPyWrapperBase* Instance) { return (TYPE*)FPyWrapperBaseMetaData::GetMetaData(Instance); }

#define PY_METADATA_METHODS(TYPE, GUID)																							\
	PY_OVERRIDE_GETSET_METADATA(TYPE)																							\
	static FGuid StaticTypeId() { return (GUID); }																				\
	virtual FGuid GetTypeId() const override { return StaticTypeId(); }

/** Base meta-data for all Unreal exposed types */
struct FPyWrapperBaseMetaData
{
	/** Set the meta-data object on the given type */
	static void SetMetaData(PyTypeObject* PyType, FPyWrapperBaseMetaData* MetaData);

	/** Get the meta-data object from the given type */
	static FPyWrapperBaseMetaData* GetMetaData(PyTypeObject* PyType);

	/** Get the meta-data object from the type of the given instance */
	static FPyWrapperBaseMetaData* GetMetaData(FPyWrapperBase* Instance);

	FPyWrapperBaseMetaData()
	{
	}

	virtual ~FPyWrapperBaseMetaData()
	{
	}

	/** Get the ID associated with this meta-data type */
	virtual FGuid GetTypeId() const = 0;

	/** Get the reflection meta data type object associated with this wrapper type if there is one or nullptr if not. */
	virtual const UField* GetMetaType() const
	{
		return nullptr;
	}

	/** Add object references from the given Python object to the given collector */
	virtual void AddReferencedObjects(FPyWrapperBase* Instance, FReferenceCollector& Collector)
	{
	}
};

typedef TPyPtr<FPyWrapperBase> FPyWrapperBasePtr;

#endif	// WITH_PYTHON

UINTERFACE(MinimalApi)
class UPythonResourceOwner : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IPythonResourceOwner
{
	GENERATED_IINTERFACE_BODY()

public:
	/**
	 * Release all Python resources owned by this object.
	 * Called during Python shutdown to free resources from lingering UObject-based instances.
	 */
	virtual void ReleasePythonResources() = 0;
};
