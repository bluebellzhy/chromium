// Copyright 2008 Google Inc. All Rights Reserved.
// <<license>>

#ifndef V8_SMART_POINTER_H_
#define V8_SMART_POINTER_H_

namespace v8 { namespace internal {


// A 'scoped pointer' that calls delete[] on its pointer when the
// destructor is called.
template<typename T>
class SmartPointer {
 public:
  // Construct a scoped pointer from a plain one.
  inline SmartPointer(T* pointer) : p(pointer) {}


  // When the destructor of the scoped pointer is executed the plain pointer
  // is deleted using delete[].  This implies that you must allocate with
  // new[...], not new(...).
  inline ~SmartPointer() { if (p) delete [] p; }


  // Copy constructor removes the pointer from the original to avoid double
  // freeing.
  inline SmartPointer(const SmartPointer<T>& rhs) : p(rhs.p) {
    const_cast<SmartPointer<T>&>(rhs).p = NULL;
  }


  // You can get the underlying pointer out with the * operator.
  inline T* operator*() { return p; }


  // You can use -> as if it was a plain pointer.
  inline T* operator->() { return p; }


  // We don't have implicit conversion to a T* since that hinders migration:
  // You would not be able to change a method from returning a T* to
  // returning an SmartPointer<T> and then get errors wherever it is used.


  // If you want to take out the plain pointer and don't want it automatically
  // deleted then call Detach().  Afterwards, the smart pointer is empty
  // (NULL).
  inline T* Detach() {
    T* temp = p;
    p = NULL;
    return temp;
  }


  // Assignment requires an empty (NULL) SmartPointer as the receiver.  Like
  // the copy constructor it removes the pointer in the original to avoid
  // double freeing.
  inline SmartPointer& operator=(const SmartPointer<T>& rhs) {
    ASSERT(p == NULL);
    p = rhs.p;
    const_cast<SmartPointer<T>&>(rhs).p = NULL;
    return *this;
  }


 private:
  T* p;
};

} }  // namespace v8::internal

#endif  // V8_SMART_POINTER_H_
