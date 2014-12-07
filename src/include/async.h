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
  using type = std::function<void (typename std::decay<T>::type)>;
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
  struct FromAsync<T&> : public FromAsync<T>
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
  inline Async<A> pure(A&& a)
  {
    return [a1 = std::forward<A>(a)]
      (typename Continuation<A>::type cont) mutable
    {
      cont(std::move(a1));
    };
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
    using A = typename function_traits<F>::template Arg<0>::bareType;
    using B = typename function_traits<F>::appliedType;

    return [f1 = std::forward<F>(f), aa1 = std::forward<AA>(aa)]
      (const typename Continuation<B>::type& cont)
    {
      aa1([=] (A&& a) {
          cont(function_traits<F>::apply(std::move(f1), std::forward<A>(a)));
        });
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
  inline Async<B> apply(AF&& af, AA&& aa)
  {
    using A = typename FromAsync<AA>::type;
    using F = typename FromAsync<AF>::type;

    struct Data
    {
      std::unique_ptr<F> pf;
      std::unique_ptr<A> pa;
      std::mutex m;
    };
    std::shared_ptr<Data> pData = std::make_shared<Data>();

    return [pData, af1 = std::forward<AF>(af), aa1 = std::forward<AA>(aa)]
      (const typename Continuation<B>::type& cont)
    {
      af1([=] (F&& f) {
          bool have_a = false;
          {
            // if we don't have a already, store f
            std::lock_guard<std::mutex> g(pData->m);
            have_a = static_cast<bool>(pData->pa);
            if (!have_a)
              pData->pf = std::make_unique<F>(std::forward<F>(f));
          }
          // if we have both sides, call the continuation and we're done
          if (have_a)
            cont(function_traits<F>::apply(std::forward<F>(f), std::move(*pData->pa)));
        });

      aa1([=] (A&& a) {
          bool have_f = false;
          {
            // if we don't have f already, store a
            std::lock_guard<std::mutex> g(pData->m);
            have_f = static_cast<bool>(pData->pf);
            if (!have_f)
              pData->pa = std::make_unique<A>(std::forward<A>(a));
          }
          // if we have both sides, call the continuation and we're done
          if (have_f)
            cont(function_traits<F>::apply(std::move(*pData->pf), std::forward<A>(a)));
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
    using A = typename function_traits<F>::template Arg<0>::bareType;
    using C = typename function_traits<AB>::template Arg<0>::bareType;
    return [=] (const C& cont)
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
      return [=] (const C& cont)
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
      return [=] (const C& cont)
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

  // Convert an Async<void> to an Async<Void>. Useful for using Async<void> with
  // && and || operators.
  template <typename T>
  struct IgnoreVoid
  {
    using type = T;
  };

  template <>
  struct IgnoreVoid<void>
  {
    using type = Void;
  };

  inline Async<Void> ignore(Async<void> av)
  {
    return [=] (const typename Continuation<Void>::type& cont)
    {
      av([=] () { cont(Void()); });
    };
  }

  template <typename F,
            typename AA = typename Async<
              typename function_traits<F>::template Arg<0>::bareType>
            ::type,
            typename AB = typename Async<
              typename function_traits<F>::template Arg<1>::bareType>
            ::type,
            typename C = typename function_traits<F>::returnType>
  inline Async<C> concurrently(AA&& aa, AB&& ab, F&& f)
  {
    return apply(fmap(std::forward<F>(f), std::forward<AA>(aa)), std::forward<AB>(ab));
  }

  template <typename F, typename A, typename B>
  struct runConcurrently
  {
    using C = typename function_traits<F>::returnType;
    inline Async<C> operator()(Async<A>&& aa, Async<B>&& ab, F&& f)
    {
      return concurrently<F>(std::forward<Async<A>>(aa), std::forward<Async<B>>(ab),
                             std::forward<F>(f));
    }
  };

  template <typename F, typename A>
  struct runConcurrently<F, A, void>
  {
    using C = typename function_traits<F>::returnType;
    inline Async<C> operator()(Async<A>&& aa, Async<void>&& ab, F&& f)
    {
      return concurrently<F>(std::forward<Async<A>>(aa), ignore(ab),
                             std::forward<F>(f));
    }
  };

  template <typename F, typename B>
  struct runConcurrently<F, void, B>
  {
    using C = typename function_traits<F>::returnType;
    inline Async<C> operator()(Async<void>&& aa, Async<B>&& ab, F&& f)
    {
      return concurrently<F>(ignore(aa), std::forward<Async<B>>(ab),
                             std::forward<F>(f));
    }
  };

  template <typename F>
  struct runConcurrently<F, void, void>
  {
    using C = typename function_traits<F>::returnType;
    inline Async<C> operator()(Async<void>&& aa, Async<void>&& ab, F&& f)
    {
      return concurrently<F>(ignore(aa), ignore(ab), std::forward<F>(f));
    }
  };

  // The zero element of the Async monoid. It never calls its continuation.
  template <typename T = Void>
  inline Async<T> zero()
  {
    return [] (typename Continuation<T>::type) {};
  }

  // Race two Asyncs: call the continuation with the result of the first one
  // that completes. Problem: how to cancel the other one, how to clean up in
  // case of ORing with zero.
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

    return [=] (const typename Continuation<Either<A,B>>::type& cont)
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

  template <typename A, typename B>
  struct runRace
  {
    inline Async<Either<A,B>> operator()(Async<A>&& aa, Async<B>&& ab)
    {
      return race<Async<A>, Async<B>>(
          std::forward<Async<A>>(aa), std::forward<Async<B>>(ab));
    }
  };

  template <typename A>
  struct runRace<A, void>
  {
    inline Async<Either<A,Void>> operator()(Async<A>&& aa, Async<void>&& ab)
    {
      return race<Async<A>, Async<Void>>(
          std::forward<Async<A>>(aa), ignore(ab));
    }
  };

  template <typename B>
  struct runRace<void, B>
  {
    inline Async<Either<Void, B>> operator()(Async<void>&& aa, Async<B>&& ab)
    {
      return race<Async<Void>, Async<B>>(
          ignore(aa), std::forward<Async<B>>(ab));
    }
  };

  template <>
  struct runRace<void, void>
  {
    inline Async<Either<Void, Void>> operator()(Async<void>&& aa, Async<void>&& ab)
    {
      return race<Async<Void>, Async<Void>>(
          ignore(aa), ignore(ab));
    }
  };
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

// The behaviour of && is to return the pair of results. If one of the operands
// is Async<void>, it is converted to Async<Void> for the purposes of making a
// pair.

template <typename AA, typename AB,
          typename A = typename async::IgnoreVoid<typename async::FromAsync<AA>::type>::type,
          typename B = typename async::IgnoreVoid<typename async::FromAsync<AB>::type>::type>
inline Async<std::pair<A,B>> operator&&(AA&& a, AB&& b)
{
  using F = decltype(std::make_pair<A,B>);
  using RA = typename async::FromAsync<AA>::type;
  using RB = typename async::FromAsync<AB>::type;
  return async::runConcurrently<F,RA,RB>()(
      std::forward<AA>(a), std::forward<AB>(b), std::make_pair<A,B>);
}

// The behaviour of || is to return the result of whichever Async completed
// first.

template <typename AA, typename AB,
          typename A = typename async::IgnoreVoid<typename async::FromAsync<AA>::type>::type,
          typename B = typename async::IgnoreVoid<typename async::FromAsync<AB>::type>::type>
inline Async<Either<A,B>> operator||(AA&& a, AB&& b)
{
  using RA = typename async::FromAsync<AA>::type;
  using RB = typename async::FromAsync<AB>::type;
  return async::runRace<RA,RB>()(std::forward<AA>(a),
                                 std::forward<AB>(b));
}
