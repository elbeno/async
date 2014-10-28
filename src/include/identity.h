#pragma once

#include "function_traits.h"
#include <ostream>

//------------------------------------------------------------------------------
// The identity monad

//------------------------------------------------------------------------------
// simple identity type

template <typename T>
struct Identity
{
  typedef T I;
  explicit Identity(const T& t) : m_t(t) {}
  T m_t;
};

//------------------------------------------------------------------------------
// normal C++ things: output, equality

template <typename T>
std::ostream& operator<<(std::ostream& s, const Identity<T>& i)
{
  return s << i.m_t;
}

template<typename T>
bool operator==(const Identity<T>& a, const Identity<T>& b)
{
  return a.m_t == b.m_t;
}

template<typename T>
bool operator!=(const Identity<T>& a, const Identity<T>& b)
{
  return !(a == b);
}

//------------------------------------------------------------------------------
// functor and monad functions

namespace identity
{

  template <typename F>
  Identity<typename function_traits<F>::returnType> fmap(
      const F& f,
      const Identity<typename function_traits<F>::template Arg<0>::bareType>& i)
  {
    return Identity<typename function_traits<F>::returnType>(f(i.m_t));
  }

  template <typename T>
  Identity<T> mjoin(const Identity<Identity<T>>& i)
  {
    return i.m_t;
  }

  template <typename F>
  typename function_traits<F>::returnType mbind(
      const F& f,
      const Identity<typename function_traits<F>::template Arg<0>::bareType>& i)
  {
    return mjoin(fmap(f, i));
  }

  template <typename T>
  Identity<T> mreturn(T t)
  {
    return Identity<T>(t);
  }

}

//------------------------------------------------------------------------------
// sugar operators

// unfortunately in c++, >>= is right associative, so in order to chain binds
// without parens, we need an alternative operator

template <typename F>
typename function_traits<F>::returnType operator>=(
    const Identity<typename function_traits<F>::template Arg<0>::bareType>& i,
    const F& f)
{
  return identity::mbind(f, i);
}

template <typename F, typename T>
typename function_traits<F>::returnType operator>(
    const Identity<T>&, const F& f)
{
  return f();
}
