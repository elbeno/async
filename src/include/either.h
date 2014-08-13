#pragma once

#include "function_traits.h"
#include <ostream>

//------------------------------------------------------------------------------
// The identity monad

//------------------------------------------------------------------------------
// simple identity type
// The either monad

template <typename Left, typename Right>
struct Either
{
  typedef Left L;
  typedef Right R;

  explicit Either(const R& r) : m_isR(true), m_left(nullptr) { m_right = new R(r); }
  Either(const L& l, bool) : m_isR(false), m_right(nullptr) { m_left = new L(l); }

  L* m_left;
  R* m_right;
  bool m_isR;

};

//------------------------------------------------------------------------------
// normal C++ things: output, equality

template <typename L, typename R>
std::ostream& operator<<(std::ostream& s, const Either<L, R>& e)
{
  if (e.m_isR)
    s << "Right:" << *e.m_right;
  else
    s << "Left:" << *e.m_left;
  return s;
}

template<typename L, typename R>
bool operator==(const Either<L, R>& a, const Either<L, R>& b)
{
  return a.m_isR == b.m_isR
    && (a.m_isR
        ? *a.m_right == *b.m_right
        : *a.m_left == *b.m_left);
}

template<typename L, typename R>
bool operator!=(const Either<L, R>& a, const Either<L, R>& b)
{
  return !(a == b);
}

//------------------------------------------------------------------------------
// functor and monad functions

namespace either
{

  template <typename A, typename F>
  Either<A, typename function_traits<F>::returnType> fmap(
      const F& f,
      const Either<A, typename function_traits<F>::template Arg<0>::bareType>& e)
  {
    typedef typename function_traits<F>::returnType C;

    if (!e.m_isR)
      return Either<A, C>(*e.m_left, true);
    return Either<A, C>(f(*e.m_right));
  }

  template <typename A, typename B>
  Either<A, B> mjoin(const Either<A, Either<A, B>>& e)
  {
    if (!e.m_isR)
      return Either<A, B>(*e.m_left, true);
    return *e.m_right;
  }

  template <typename A, typename F>
  typename function_traits<F>::returnType mbind(
      const F& f,
      const Either<A, typename function_traits<F>::template Arg<0>::bareType>& e)
  {
    return mjoin(fmap(f, e));
  }

  template <typename A, typename B>
  Either<A, B> mreturn(B b)
  {
    return Either<A, B>(b);
  }
}

//------------------------------------------------------------------------------
// sugar operators

template <typename A, typename F>
typename function_traits<F>::returnType operator>>=(
    const Either<A, typename function_traits<F>::template Arg<0>::bareType>& e,
    const F& f)
{
  return either::mbind(f, e);
}

template <typename A, typename B, typename F>
typename function_traits<F>::returnType operator>>(
    const Either<A,B>&, const F& f)
{
  return f();
}
