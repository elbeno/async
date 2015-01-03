#pragma once

#include <functional>
#include <tuple>
#include <type_traits>

// For function objects (and lambdas), their function_traits are the
// function_traits of their operator()
template <typename T>
struct function_traits
  : public function_traits<decltype(&T::operator())>
{};

// For a regular function, we destructure its type appropriately
template <typename R, typename... A>
struct function_traits<R(A...)>
{
  using returnType = R;

  static const size_t arity = sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = std::tuple_element_t<n, std::tuple<A...>>;
    using bareType = std::decay_t<type>;
  };

  // Application calls the function
  using appliedType = R;

  template <typename F>
  static inline auto apply(F&& f, A&&... args)
  {
    return f(std::forward<A...>(args...));
  }
};

// Specialization for 2 argument functions, to enable partial application
template <typename R, typename A1, typename A2>
struct function_traits<R(A1, A2)>
{
  // decay the first arg so we can use a universal ref and not incur a copy
  using A1B = std::decay_t<A1>;
  using returnType = R;

  static const size_t arity = 2;

  template <size_t n>
  struct Arg
  {
    using type = std::tuple_element_t<n, std::tuple<A1, A2>>;
    using bareType = std::decay_t<type>;
  };

  // (Partial) Application binds the first argument
  using appliedType = std::function<R(A2)>;

  template <typename F>
  static inline auto apply(F&& f, A1B&& a1)
  {
    // in case a1 is an rvalue ref, we need to capture "by forward" and make it
    // mutable, to call the function with a move
    return [f1 = std::forward<F>(f), a = std::forward<A1B>(a1)]
      (A2&& a2) mutable -> R
    {
      return f1(std::move(a), std::forward<A2>(a2));
    };
  }
};

// Specialization for 3+ argument functions, to enable partial application
template <typename R, typename A1, typename A2, typename... A>
struct function_traits<R(A1, A2, A...)>
{
  // see decay note above
  using A1B = std::decay_t<A1>;
  using returnType = R;

  static const size_t arity = 2 + sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = std::tuple_element_t<n, std::tuple<A1, A2, A...>>;
    using bareType = std::decay_t<type>;
  };

  // (Partial) Application binds the first argument
  using appliedType = std::function<R(A2, A...)>;

  template <typename F>
  static inline auto apply(F&& f, A1B&& a1)
  {
    // see capture-by-forward note above
    return [f1 = std::forward<F>(f), a = std::forward<A1B>(a1)]
      (A2&& a2, A&&... args) mutable -> R
    {
      return f1(std::move(a), std::forward<A2>(a2), std::forward<A...>(args...));
    };
  }
};

// For class member functions, extract the type
template <typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...)>
  : public function_traits<R(A...)>
{};

template <typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...) const>
  : public function_traits<R(A...)>
{};

// Remove const, volatile, pointers and references
template <typename T>
struct function_traits<const T>
  : public function_traits<T>
{};

template <typename T>
struct function_traits<volatile T>
  : public function_traits<T>
{};

template <typename T>
struct function_traits<T*>
  : public function_traits<T>
{};

template <typename T>
struct function_traits<T&>
  : public function_traits<T>
{};

// Useful for SFINAE on template arguments
template <typename ...>
using void_t = void;

// Result of applying a function to some args, if it can be applied
template <typename Signature, typename = void>
struct result_of {};

template <typename F, typename... A>
struct result_of<F(A...), void_t<decltype(std::declval<F>()(std::declval<A>()...))>>
{
  using type = decltype(std::declval<F>()(std::declval<A>()...));
};

template <typename Signature>
using result_of_t = typename result_of<Signature>::type;

// Generate a list of integer arguments
template <int...>
struct indices {};

template <int N, int...S>
struct indices_for : public indices_for<N-1, N-1, S...> {};

template <int...S>
struct indices_for<0, S...>
{
  using type = indices<S...>;
};

template <int N>
using indices_for_t = typename indices_for<N>::type;

// Curried functions
template <typename F, typename... A>
struct curried
{
private:
  using Args = std::tuple<A...>;

public:
  curried(F&& f, Args&& args)
    : mF(std::forward<F>(f))
    , mArgs(std::forward<Args>(args))
  {}

  // partial function application
  template<typename Arg, typename... Rest>
  curried<F, A..., Arg> operator()(Arg&& arg, Rest const&..., ...) const&
  {
    static_assert(sizeof...(Rest) == 0,
                  "curried function requires 1 argument");
    return { mF,
        std::tuple_cat(mArgs,
                       std::forward_as_tuple(std::forward<Arg>(arg))) };
  }

  template<typename Arg, typename... Rest>
  curried<F, A..., Arg> operator()(Arg&& arg, Rest const&..., ...) &&
  {
    static_assert(sizeof...(Rest) == 0,
                  "curried function requires 1 argument");
    return { std::move(mF),
        std::tuple_cat(std::move(mArgs),
                       std::forward_as_tuple(std::forward<Arg>(arg))) };
  }

  // regular function application with all args supplied
  template <typename Arg>
  result_of_t<F&(A..., Arg)> operator()(Arg&& arg) const&
  {
    return invoke(mF, std::tuple_cat(mArgs,
                                     std::forward_as_tuple(std::forward<Arg>(arg))));
  }

  template <typename Arg>
  result_of_t<F&(A..., Arg)> operator()(Arg&& arg) &&
  {
    return invoke(std::move(mF),
                  std::tuple_cat(std::move(mArgs),
                                 std::forward_as_tuple(std::forward<Arg>(arg))));
  }

  // support currying nullary functions
  template <typename Func = F>
  result_of_t<Func&(A...)> operator()() const&
  {
    return invoke(mF, mArgs);
  }

  template <typename Func = F>
  result_of_t<Func&(A...)> operator()() &&
  {
    return invoke(std::move(mF), std::move(mArgs));
  }

private:
  template <typename Func, typename Tuple, int... Indices>
  static result_of_t<Func(std::tuple_element_t<Indices, Tuple>...)>
  invoke(Func&& f, Tuple&& t, indices<Indices...>)
  {
    return std::forward<Func>(f)(std::get<Indices>(std::forward<Tuple>(t))...);
  }

  template <typename Func, typename Tuple>
  static auto invoke(Func&& f, Tuple&& t)
  {
    return invoke(std::forward<Func>(f), std::forward<Tuple>(t),
                  indices_for_t<std::tuple_size<std::decay_t<Tuple>>::value>());
  }

  F mF;
  Args mArgs;
};

template <typename F>
curried<F> curry(F&& f)
{
  return { std::forward<F>(f), {} };
}
