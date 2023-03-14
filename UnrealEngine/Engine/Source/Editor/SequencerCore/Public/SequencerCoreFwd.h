// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE
{
namespace Sequencer
{

/**
 * View model type forward declarations
 */
class ICastable;
class FViewModel;
class FSharedViewModelData;
class FViewModelHierarchyOperation;
class FScopedViewModelHierarchyOperation;

struct FViewModelListHead;
struct FViewModelChildren;

/**
 * Iterator forward declarations - include MVVM/ViewModels/ViewModelIterators.h for complete declarations
 */
struct FParentModelIterator;
struct FParentFirstChildIterator;
struct FViewModelListIterator;
struct FViewModelSubListIterator;
struct FViewModelIterationState;

template<typename, typename> struct TTypedIterator;

template<typename T>
using TParentModelIterator = TTypedIterator<T, FParentModelIterator>;
template<typename T>
using TParentFirstChildIterator = TTypedIterator<T, FParentFirstChildIterator>;
template<typename T>
using TViewModelListIterator = TTypedIterator<T, FViewModelListIterator>;
template<typename T>
using TViewModelSubListIterator = TTypedIterator<T, FViewModelSubListIterator>;


/**
 * Shared/weak extension pointer forward declarations - include MVVM/ViewModelPtr.h for complete declarations
 */
struct FImplicitViewModelCast;
struct FImplicitWeakViewModelPin;
struct FImplicitWeakViewModelCast;

struct FViewModelPtr;
struct FWeakViewModelPtr;

template<typename ExtensionType>
struct TViewModelPtr;

template<typename ExtensionType>
struct TWeakViewModelPtr;


} // namespace Sequencer
} // namespace UE
