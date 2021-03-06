/*
 * Copyright 2020 The TensorFlow Runtime Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//===- ref_count.h ----------------------------------------------*- C++ -*-===//
//
// Helpers and utilities for working with reference counted types.
//
//===----------------------------------------------------------------------===//

#ifndef TFRT_SUPPORT_REF_COUNT_H_
#define TFRT_SUPPORT_REF_COUNT_H_

#include <atomic>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace tfrt {

extern std::atomic<size_t> total_reference_counted_objects;

// Return the total number of reference-counted objects that are currently
// live in the process.  This is intended for debugging/assertions only, and
// shouldn't be used for mainline logic in the runtime.
inline size_t GetNumReferenceCountedObjects() {
  return total_reference_counted_objects.load(std::memory_order_relaxed);
}

// This class is a common base class for things that need an atomic reference
// count for ownership management.
//
// Subclasses of this are allowed to implement a Destroy() instance method,
// which allows custom allocation/deallocation logic.
//
// This class intentionally doesn't have a virtual destructor or anything else
// that would require a vtable, but subclasses can have one if they choose.
template <typename SubClass>
class ReferenceCounted {
 public:
  ReferenceCounted() : ref_count_(1) {
    total_reference_counted_objects.fetch_add(1, std::memory_order_relaxed);
  }
  ~ReferenceCounted() {
    assert(ref_count_.load() == 0 &&
           "Shouldn't destroy a reference counted object with references!");
    total_reference_counted_objects.fetch_sub(1, std::memory_order_relaxed);
  }

  // Not copyable or movable.
  ReferenceCounted(const ReferenceCounted&) = delete;
  ReferenceCounted& operator=(const ReferenceCounted&) = delete;

  // Add a new reference to this object.
  void AddRef() { ref_count_.fetch_add(1); }

  // Drop a reference to this object, potentially deallocating it.
  void DropRef() {
    if (ref_count_.fetch_sub(1) == 1) static_cast<SubClass*>(this)->Destroy();
  }

  // Return true if reference count is 1.
  bool IsUnique() const { return ref_count_.load() == 1; }

 protected:
  // Subclasses are allowed to customize this, but the default implementation
  // of Destroy() just deletes the pointer.
  void Destroy() { delete static_cast<SubClass*>(this); }

 private:
  std::atomic<unsigned> ref_count_;
};

// This is a smart pointer that keeps the specified reference counted value
// around.  It is move-only to avoid accidental copies, but it can be copied
// explicitly.
template <typename T>
class RCReference {
 public:
  RCReference() : pointer_(nullptr) {}

  RCReference(RCReference&& other) : pointer_(other.pointer_) {
    other.pointer_ = nullptr;
  }

  // Support implicit conversion from RCReference<Derived> to RCReference<Base>.
  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  RCReference(RCReference<U>&& u) : pointer_(u.pointer_) {  // NOLINT
    u.pointer_ = nullptr;
  }

  RCReference& operator=(RCReference&& other) {
    if (pointer_) pointer_->DropRef();
    pointer_ = other.pointer_;
    other.pointer_ = nullptr;
    return *this;
  }

  ~RCReference() {
    if (pointer_ != nullptr) pointer_->DropRef();
  }

  void reset(T* pointer = nullptr) {
    if (pointer_ != nullptr) pointer_->DropRef();
    pointer_ = pointer;
  }

  T* release() {
    T* tmp = pointer_;
    pointer_ = nullptr;
    return tmp;
  }

  // Not implicity copyable, use the CopyRef() method for an explicit copy of
  // this reference.
  RCReference(const RCReference&) = delete;
  RCReference& operator=(const RCReference&) = delete;

  T& operator*() const {
    assert(pointer_ && "null RCReference");
    return *pointer_;
  }

  T* operator->() const {
    assert(pointer_ && "null RCReference");
    return pointer_;
  }

  // Return a raw pointer.
  T* get() const { return pointer_; }

  // Make an explicit copy of this RCReference, increasing the refcount by one.
  RCReference CopyRef() const;

  explicit operator bool() const { return pointer_ != nullptr; }

  void swap(RCReference& other) {
    using std::swap;
    swap(pointer_, other.pointer_);
  }

 private:
  template <typename R>
  friend class RCReference;
  template <typename R>
  friend RCReference<R> FormRef(R*);
  template <typename R>
  friend RCReference<R> TakeRef(R*);

  T* pointer_;
};

// Add a new reference to the specified pointer.
template <typename T>
RCReference<T> FormRef(T* pointer) {
  RCReference<T> ref;
  ref.pointer_ = pointer;
  pointer->AddRef();
  return ref;
}

// Return an RCReference for the specified object and *takes ownership* of a
// +1 reference.  When destroyed, this will drop the reference.
template <typename T>
RCReference<T> TakeRef(T* pointer) {
  RCReference<T> ref;
  ref.pointer_ = pointer;
  return ref;
}

template <typename T>
RCReference<T> RCReference<T>::CopyRef() const {
  if (!pointer_) return RCReference();
  return FormRef(get());
}

// For ADL style swap.
template <typename T>
void swap(RCReference<T>& a, RCReference<T>& b) {
  a.swap(b);
}

}  // namespace tfrt

#endif  // TFRT_SUPPORT_REF_COUNT_H_
