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
