#pragma once

#include "either.h"
#include "function_traits.h"

#include <functional>
#include <memory>
#include <mutex>
#include <utility>

//------------------------------------------------------------------------------
// The async monad

//------------------------------------------------------------------------------
// Simple type representing an asynchronous value which can be retrieved by
// passing a continuation to receive it.

template <typename T>
using Continuation = std::function<void (T)>;

template <typename T>
using Async = std::function<void (Continuation<T>)>;

namespace async
{

  // Lift a value into an async context: just call the continuation with the
  // captured value.
  template <typename A>
  inline Async<A> pure(A a)
  {
    return [a] (Continuation<A> cont) { cont(a); };
  }

  // Fmap a function into an async context: the new async will pass the existing
  // async a continuation that calls the new continuation with the result of
  // calling the function.
  template <typename F,
            typename A = typename function_traits<F>::template Arg<0>::bareType,
            typename B = typename function_traits<F>::returnType>
  inline Async<B> fmap(F f, Async<A> aa)
  {
    return [=] (Continuation<B> cont)
    {
      aa([=] (A a) { cont(f(a)); });
    };
  }

  // Apply an async function to an async argument: this is more involved. We
  // need to call each async, passing a continuation that stores its argument if
  // the other one isn't present, otherwise applies the function and calls the
  // new continuation with the result.
  template <typename F,
            typename A = typename function_traits<F>::template Arg<0>::bareType,
            typename B = typename function_traits<F>::returnType>
  inline Async<B> apply(Async<F> af, Async<A> aa)
  {
    return [=] (Continuation<B> cont)
    {
      struct Data
      {
        std::unique_ptr<F> pf;
        std::unique_ptr<A> pa;
        std::mutex m;
      };
      std::shared_ptr<Data> pData = std::make_shared<Data>();

      af([=] (F f) {
          bool have_a = false;
          {
            // if we don't have a already, store f
            std::lock_guard<std::mutex> g(pData->m);
            have_a = static_cast<bool>(pData->pa);
            if (!have_a)
              pData->pf = std::make_unique<F>(std::move(f));
          }
          // if we have both sides, call the continuation and we're done
          if (have_a)
            cont(f(*pData->pa));
        });

      aa([=] (A a) {
          bool have_f = false;
          {
            // if we don't have f already, store a
            std::lock_guard<std::mutex> g(pData->m);
            have_f = static_cast<bool>(pData->pf);
            if (!have_f)
              pData->pa = std::make_unique<A>(std::move(a));
          }
          // if we have both sides, call the continuation and we're done
          if (have_f)
            cont((*pData->pf)(a));
        });
    };
  }

  template <typename F,
            typename A = typename function_traits<F>::template Arg<0>::bareType,
            typename AB = typename function_traits<F>::returnType>
  inline AB bind(Async<A> aa, F f)
  {
    using C = typename function_traits<AB>::template Arg<0>::bareType;
    return [=] (C cont)
    {
      aa([=] (A a) { f(a)(cont); });
    };
  }

  template <typename F,
            typename A = typename function_traits<F>::template Arg<0>::bareType,
            typename AB = typename function_traits<F>::returnType>
  inline AB sequence(Async<A> aa, F f)
  {
    using C = typename function_traits<AB>::template Arg<0>::bareType;
    return [=] (C cont)
    {
      aa([=] (A) { f()(cont); });
    };
  }

  // The technique for ANDing together two Asyncs is similar to apply.
  template <typename A, typename B>
  inline Async<std::pair<A,B>> logical_and(Async<A> aa, Async<B> ab)
  {
    return [=] (Continuation<std::pair<A,B>> cont)
    {
      struct Data
      {
        std::unique_ptr<A> pa;
        std::unique_ptr<B> pb;
        std::mutex m;
      };
      std::shared_ptr<Data> pData = std::make_shared<Data>();

      aa([=] (A a) {
          bool have_b = false;
          {
            // if we don't have b already, store a
            std::lock_guard<std::mutex> g(pData->m);
            have_b = static_cast<bool>(pData->pb);
            if (!have_b)
              pData->pa = std::make_unique<A>(std::move(a));
          }
          // if we have both sides, call the continuation and we're done
          if (have_b)
            cont(std::make_pair(a, *pData->pb));
        });

      ab([=] (B b) {
          bool have_a = false;
          {
            // if we don't have a already, store b
            std::lock_guard<std::mutex> g(pData->m);
            have_a = static_cast<bool>(pData->pa);
            if (!have_a)
              pData->pb = std::make_unique<B>(std::move(b));
          }
          // if we have both sides, call the continuation and we're done
          if (have_a)
            cont(std::make_pair(*pData->pa, b));
        });
    };
  }

  // The zero element of the Async monoid. It never calls its continuation.
  template <typename T = int>
  Async<T> zero()
  {
    return [] (Continuation<T>) {};
  }

  // ORing together two Asyncs: call the continuation with the result of the
  // first one that completes. Problem: how to cancel the other one, how to
  // clean up in case of ORing with zero...
  template <typename A, typename B>
  inline Async<Either<A,B>> logical_or(Async<A> aa, Async<B> ab)
  {
    return [=] (Continuation<Either<A,B>> cont)
    {
      struct Data
      {
        Data() : done(false) {}
        bool done;
        std::mutex m;
      };
      std::shared_ptr<Data> pData = std::make_shared<Data>();

      aa([=] (A a) {
          bool done = false;
          {
            std::lock_guard<std::mutex> g(pData->m);
            done = pData->done;
            pData->done = true;
          }
          if (!done)
            cont(Either<A,B>(a, true));
        });

      ab([=] (B b) {
          bool done = false;
          {
            std::lock_guard<std::mutex> g(pData->m);
            done = pData->done;
            pData->done = true;
          }
          if (!done)
            cont(Either<A,B>(b));
        });
    };
  }

}

// Syntactic sugar: >= is Haskell's >>=, and > is Haskell's >>.

template <typename F,
          typename A = typename function_traits<F>::template Arg<0>::bareType,
          typename AB = typename function_traits<F>::returnType>
inline AB operator>=(Async<A>&& a, F f)
{
  return async::bind(std::forward<Async<A>>(a), f);
}

template <typename F,
          typename A,
          typename AB = typename function_traits<F>::returnType>
inline AB operator>(Async<A>&& a, F f)
{
  return async::sequence(std::forward<Async<A>>(a), f);
}

template <typename A, typename B>
inline Async<std::pair<A,B>> operator&&(Async<A>&& a, Async<B>&& b)
{
  return async::logical_and<A,B>(std::forward<Async<A>>(a),
                                 std::forward<Async<B>>(b));
}
template <typename A, typename B>
inline Async<Either<A,B>> operator||(Async<A>&& a, Async<B>&& b)
{
  return async::logical_or<A,B>(std::forward<Async<A>>(a),
                                std::forward<Async<B>>(b));
}
