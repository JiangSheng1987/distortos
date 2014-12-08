/**
 * \file
 * \brief FifoQueueBase class header
 *
 * \author Copyright (C) 2014 Kamil Szczygiel http://www.distortec.com http://www.freddiechopin.info
 *
 * \par License
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * \date 2014-12-10
 */

#ifndef INCLUDE_DISTORTOS_SCHEDULER_FIFOQUEUEBASE_HPP_
#define INCLUDE_DISTORTOS_SCHEDULER_FIFOQUEUEBASE_HPP_

#include "distortos/Semaphore.hpp"

#include "distortos/estd/TypeErasedFunctor.hpp"

namespace distortos
{

namespace scheduler
{

/// FifoQueueBase class is a base for FifoQueue template class
class FifoQueueBase
{
private:

	/// required by templated constructor to deduce type
	template<typename T>
	class TypeTag
	{

	};

	/// type of uninitialized storage for data
	template<typename T>
	using Storage = typename std::aligned_storage<sizeof(T), alignof(T)>::type;


	/**
	 * \brief Functor is a type-erased interface for functors which execute some action on queue's storage (like
	 * copy-constructing, swapping, destroying, emplacing, ...).
	 *
	 * The functor will be called by FifoQueueBase internals with one argument - \a storage - which is a reference to
	 * pointer to queue's storage - after executing functor's action, the pointer should be incremented to next position
	 * (using the actual size of element)
	 */

	using Functor = estd::TypeErasedFunctor<void(void*&)>;

	/**
	 * \brief BoundedFunctor is a type-erased Functor which calls its bounded functor to execute actions on queue's
	 * storage and deals with the pointer increments
	 *
	 * \param T is the type of element
	 * \param F is the type of bounded functor, it will be called with <em>Storage<T>*</em> as only argument
	 */

	template<typename T, typename F>
	class BoundedFunctor : public Functor
	{
	public:

		/**
		 * \brief BoundedFunctor's constructor
		 *
		 * \param [in] boundedFunctor is a rvalue reference to bounded functor which will be used to move-construct
		 * internal bounded functor
		 */

		constexpr explicit BoundedFunctor(F&& boundedFunctor) :
				boundedFunctor_{std::move(boundedFunctor)}
		{

		}

		/**
		 * \brief Calls the bounded functor which will execute some action on queue's storage (like copy-constructing,
		 * swapping, destroying, emplacing, ...) and increments the storage pointer to next position (using the actual
		 * size of element)
		 *
		 * \param [in,out] storage is a reference to pointer to queue's storage - after executing bounded functor, the
		 * pointer will be incremented to next position (using the actual size of element)
		 */

		virtual void operator()(void*& storage) const override
		{
			auto typedStorage = static_cast<Storage<T>*>(storage);
			boundedFunctor_(typedStorage);
			storage = typedStorage + 1;
		}

	private:

		/// bounded functor
		F boundedFunctor_;
	};

	/**
	 * \brief Helper factory function to make BoundedFunctor object with partially deduced template arguments
	 *
	 * \param T is the type of element
	 * \param F is the type of bounded functor, it will be called with <em>Storage<T>*</em> as only argument
	 *
	 * \param [in] boundedFunctor is a rvalue reference to bounded functor which will be used to move-construct internal
	 * bounded functor
	 *
	 * \return BoundedFunctor object with partially deduced template arguments
	 */

	template<typename T, typename F>
	constexpr static BoundedFunctor<T, F> makeBoundedFunctor(F&& boundedFunctor)
	{
		return BoundedFunctor<T, F>{std::move(boundedFunctor)};
	}

protected:

	/**
	 * \brief FifoQueueBase's constructor
	 *
	 * \param T is the type of data in queue
	 *
	 * \param [in] storage is an array of Storage<T> elements
	 * \param [in] maxElements is the number of elements in storage array
	 * \param [in] typeTag is used to deduce T, as deduction from storage is not possible (nested-name-specifier, so a
	 * non-deduced context)
	 */

	template<typename T>
	FifoQueueBase(Storage<T>* const storage, const size_t maxElements, TypeTag<T>) :
			popSemaphore_{0, maxElements},
			pushSemaphore_{maxElements, maxElements},
			storageBegin_{storage},
			storageEnd_{storage + maxElements},
			readPosition_{storage},
			writePosition_{storage}
	{

	}

	/**
	 * \brief FifoQueueBase's destructor
	 *
	 * \note Polymorphic objects of FifoQueueBase type must not be deleted via pointer/reference
	 */

	~FifoQueueBase()
	{

	}

	/**
	 * \brief Pops the oldest (first) element from the queue.
	 *
	 * \param T is the type of data popped from queue
	 *
	 * \param [out] value is a reference to object that will be used to return popped value, its contents are swapped
	 * with the value in the queue's storage and destructed when no longer needed
	 *
	 * \return zero if element was popped successfully, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	template<typename T>
	int pop(T& value)
	{
		const auto swapFunctor = makeBoundedFunctor<T>(
				[&value](Storage<T>* const storage)
				{
					auto& swappedValue = *reinterpret_cast<T*>(storage);
					std::swap(value, swappedValue);
					swappedValue.~T();
				});
		return popImplementation(swapFunctor);
	}

	/**
	 * \brief Pushes the element to the queue.
	 *
	 * \param T is the type of data pushed to queue
	 *
	 * \param [in] value is a reference to object that will be pushed, value in queue's storage is copy-constructed
	 *
	 * \return zero if element was pushed successfully, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	template<typename T>
	int push(const T& value)
	{
		const auto copyFunctor = makeBoundedFunctor<T>(
				[&value](Storage<T>* const storage)
				{
					new (storage) T{value};
				});
		return pushImplementation(copyFunctor);
	}

	/**
	 * \brief Pushes the element to the queue.
	 *
	 * \param T is the type of data pushed to queue
	 *
	 * \param [in] value is a rvalue reference to object that will be pushed, value in queue's storage is
	 * move-constructed
	 *
	 * \return zero if element was pushed successfully, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	template<typename T>
	int push(T&& value)
	{
		const auto moveFunctor = makeBoundedFunctor<T>(
				[&value](Storage<T>* const storage)
				{
					new (storage) T{std::move(value)};
				});
		return pushImplementation(moveFunctor);
	}

private:

	/**
	 * \brief Implementation of pop() using type-erased functor
	 *
	 * \param [in] functor is a reference to Functor which will execute actions related to popping - it will get a
	 * reference to readPosition_ as argument
	 *
	 * \return zero if element was popped successfully, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	int popImplementation(const Functor& functor)
	{
		return popPushImplementation(functor, popSemaphore_, pushSemaphore_, readPosition_);
	}

	/**
	 * \brief Implementation of pop() and push() using type-erased functor
	 *
	 * \param [in] functor is a reference to Functor which will execute actions related to popping/pushing - it will get
	 * a reference to \a storage as argument
	 * \param [in] waitSemaphore is a reference to semaphore that will be waited for, \a popSemaphore_ for pop(), \a
	 * pushSemaphore_ for push()
	 * \param [in] postSemaphore is a reference to semaphore that will be posted after the operation, \a pushSemaphore_
	 * for pop(), \a popSemaphore_ for push()
	 * \param [in] storage is a reference to appropriate pointer to storage, which will be passed to \a functor, \a
	 * readPosition_ for pop(), \a writePosition_ for push()
	 *
	 * \return zero if operation was successful, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	int popPushImplementation(const Functor& functor, Semaphore& waitSemaphore, Semaphore& postSemaphore,
			void*& storage);

	/**
	 * \brief Implementation of push() using type-erased functor
	 *
	 * \param [in] functor is a reference to Functor which will execute actions related to pushing - it will get a
	 * reference to writePosition_ as argument
	 *
	 * \return zero if element was pushed successfully, error code otherwise:
	 * - error codes returned by Semaphore::wait();
	 * - error codes returned by Semaphore::post();
	 */

	int pushImplementation(const Functor& functor)
	{
		return popPushImplementation(functor, pushSemaphore_, popSemaphore_, writePosition_);
	}

	/// semaphore guarding access to "pop" functions - its value is equal to the number of available elements
	Semaphore popSemaphore_;

	/// semaphore guarding access to "push" functions - its value is equal to the number of free slots
	Semaphore pushSemaphore_;

	/// beginning of array with Storage elements
	void* const storageBegin_;

	/// pointer to past-the-last element of array with Storage elements
	void* const storageEnd_;

	/// pointer to first element available for reading
	void* readPosition_;

	/// pointer to first free slot available for writing
	void* writePosition_;
};

}	// namespace scheduler

}	// namespace distortos

#endif	// INCLUDE_DISTORTOS_SCHEDULER_FIFOQUEUEBASE_HPP_
