#pragma once

#include "either.h"
#include "function_traits.h"

#include <functional>
#include <memory>
#include <mutex>
#include <ostream>
#include <utility>

//------------------------------------------------------------------------------
// The async monad

//------------------------------------------------------------------------------
// Simple type representing an asynchronous value which can be retrieved by
// passing a continuation to receive it.

template <typename T>
struct Continuation
{
  using type = std::function<void (T)>;
};

template <>
struct Continuation<void>
{
  using type = std::function<void ()>;
};

template <typename T>
using Async = std::function<void (typename Continuation<T>::type)>;

namespace async
{

  template <typename T>
  struct FromAsync
  {
  };

  template <typename T>
  struct FromAsync<std::function<void (std::function<void (T)>)>>
  {
    using type = T;
  };

  template <>
  struct FromAsync<std::function<void (std::function<void ()>)>>
  {
    using type = void;
  };

  // Lift a value into an async context: just call the continuation with the
  // captured value.
  // a -> m a
  template <typename A>
  inline Async<A> pure(A a)
  {
    return [a] (typename Continuation<A>::type cont) { cont(a); };
  }

  // Fmap a function into an async context: the new async will pass the existing
  // async a continuation that calls the new continuation with the result of
  // calling the function.
  // (a -> b) -> m a -> m b
  template <typename F,
            typename AA = typename Async<
              typename function_traits<F>::template Arg<0>::bareType>
            ::type>
  inline Async<typename function_traits<F>::appliedType> fmap(
      F&& f, AA&& aa)
  {
    using A = typename FromAsync<typename std::remove_reference<AA>::type>::type;
    using B = typename function_traits<F>::appliedType;

    return [=] (typename Continuation<B>::type cont)
    {
      aa([=] (A a) { cont(function_traits<F>::apply(f, a)); });
    };
  }

  // Apply an async function to an async argument: this is more involved. We
  // need to call each async, passing a continuation that stores its argument if
  // the other one isn't present, otherwise applies the function and calls the
  // new continuation with the result.
  // m (a -> b) -> m a -> m b
  template <typename AF,
            typename AA = typename Async<
              typename function_traits<
                typename FromAsync<AF>::type>
              ::template Arg<0>::bareType>
            ::type,
            typename B = typename function_traits<
              typename FromAsync<AF>::type>
            ::appliedType>
  inline Async<B> apply(AF af, AA&& aa)
  {
    using A = typename FromAsync<typename std::remove_reference<AA>::type>::type;
    using F = typename FromAsync<AF>::type;

    struct Data
    {
      std::unique_ptr<F> pf;
      std::unique_ptr<A> pa;
      std::mutex m;
    };
    std::shared_ptr<Data> pData = std::make_shared<Data>();

    return [=] (typename Continuation<B>::type cont)
    {
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
            cont(function_traits<F>::apply(f, *pData->pa));
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
            cont(function_traits<F>::apply(*pData->pf, a));
        });
    };
  }

  // Bind an async value to a function returning async. We need to call the
  // async, passing a continuation that calls the function on the argument, then
  // passes the new continuation to that async value. A can't be void here; use
  // sequence instead for that case.
  // m a -> (a -> m b) - > m b
  template <typename F,
            typename AA = typename Async<
              typename function_traits<F>::template Arg<0>::bareType>
            ::type,
            typename AB = typename function_traits<F>::returnType>
  inline AB bind(AA&& aa, F&& f)
  {
    using A = typename FromAsync<typename std::remove_reference<AA>::type>::type;
    using C = typename function_traits<AB>::template Arg<0>::bareType;
    return [=] (C cont)
    {
      aa([=] (A a) { f(std::move(a))(cont); });
    };
  }

  // Sequence is like bind, but it drops the result of the first async. We need
  // to deal separately with Async<void> and Async<T>.
  // m a -> m b -> m b
  template <typename F, typename AA, typename A>
  struct sequence
  {
    using AB = typename function_traits<F>::returnType;
    inline AB operator()(AA&& aa, F&& f)
    {
      using C = typename function_traits<AB>::template Arg<0>::bareType;
      return [=] (C cont)
      {
        aa([=] (A) { f()(cont); });
      };
    }
  };

  template <typename F, typename AA>
  struct sequence<F, AA, void>
  {
    using AB = typename function_traits<F>::returnType;
    inline AB operator()(AA&& aa, F&& f)
    {
      using C = typename function_traits<AB>::template Arg<0>::bareType;
      return [=] (C cont)
      {
        aa([=] () { f()(cont); });
      };
    }
  };

  // For use in zero, pair and Either.
  struct Void {};
  std::ostream& operator<<(std::ostream& s, const Void&)
  {
    return s << "(void)";
  }

  // Run two asyncs concurrently: the technique is similar to apply. TODO:
  // support either or both AA/AB being Async<void> (a similar approach to
  // sequence()).
  template <typename F,
            typename AA = typename Async<
              typename function_traits<F>::template Arg<0>::bareType>
            ::type,
            typename AB = typename Async<
              typename function_traits<F>::template Arg<1>::bareType>
            ::type,
            typename C = typename function_traits<F>::returnType>
  inline Async<C> concurrently(AA&& aa, AB&& ab, F f)
  {
    using A = typename function_traits<F>::template Arg<0>::bareType;
    using B = typename function_traits<F>::template Arg<1>::bareType;

    struct Data
    {
      std::unique_ptr<A> pa;
      std::unique_ptr<B> pb;
      std::mutex m;
    };
    std::shared_ptr<Data> pData = std::make_shared<Data>();

    return [=] (typename Continuation<C>::type cont)
    {
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
            cont(f(std::move(a), std::move(*pData->pb)));
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
            cont(f(std::move(*pData->pa), std::move(b)));
        });
    };
  }

  // The zero element of the Async monoid. It never calls its continuation.
  template <typename T = Void>
  Async<T> zero()
  {
    return [] (typename Continuation<T>::type) {};
  }

  // Race two Asyncs: call the continuation with the result of the first one
  // that completes. Problem: how to cancel the other one, how to clean up in
  // case of ORing with zero. TODO: support either or both AA/AB being
  // Async<void> (a similar approach to sequence()).
  template <typename AA, typename AB,
            typename A = typename FromAsync<AA>::type,
            typename B = typename FromAsync<AB>::type>
  inline Async<Either<A,B>> race(AA&& aa, AB&& ab)
  {
    struct Data
    {
      Data() : done(false) {}
      bool done;
      std::mutex m;
    };
    std::shared_ptr<Data> pData = std::make_shared<Data>();

    return [=] (typename Continuation<Either<A,B>>::type cont)
    {
      aa([=] (A a) {
          bool done = false;
          {
            std::lock_guard<std::mutex> g(pData->m);
            done = pData->done;
            pData->done = true;
          }
          if (!done)
            cont(Either<A,B>(std::move(a), true));
        });

      ab([=] (B b) {
          bool done = false;
          {
            std::lock_guard<std::mutex> g(pData->m);
            done = pData->done;
            pData->done = true;
          }
          if (!done)
            cont(Either<A,B>(std::move(b)));
        });
    };
  }

}

// Syntactic sugar: >= is Haskell's >>=, and > is Haskell's >>.

template <typename F,
          typename A = typename function_traits<F>::template Arg<0>::bareType,
          typename AB = typename function_traits<F>::returnType>
inline AB operator>=(Async<A>&& a, F&& f)
{
  return async::bind(std::forward<Async<A>>(a),
                     std::forward<F>(f));
}

template <typename F, typename AA,
          typename A = typename async::FromAsync<AA>::type,
          typename AB = typename function_traits<F>::returnType>
inline AB operator>(AA&& a, F&& f)
{
  return async::sequence<F,AA,A>()(std::forward<AA>(a),
                                   std::forward<F>(f));
}

template <typename AA, typename AB,
          typename A = typename async::FromAsync<AA>::type,
          typename B = typename async::FromAsync<AB>::type>
inline Async<std::pair<A,B>> operator&&(AA&& a, AB&& b)
{
  return async::concurrently(std::forward<AA>(a),
                             std::forward<AB>(b),
                             std::make_pair<A,B>);
}

template <typename AA, typename AB,
          typename A = typename async::FromAsync<AA>::type,
          typename B = typename async::FromAsync<AB>::type>
inline Async<Either<A,B>> operator||(AA&& a, AB&& b)
{
  return async::race(std::forward<AA>(a),
                     std::forward<AB>(b));
}
