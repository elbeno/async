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
  using functionType = R(A...);
  using returnType = R;

  static const size_t arity = sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = typename std::tuple_element<n, std::tuple<A...>>::type;
    using bareType = typename std::decay<type>::type;
  };

  // To simplify partial application
  using boundType = R;
};

// Specialization for 2+ argument functions, to enable partial application
template <typename R, typename A1, typename A2, typename... A>
struct function_traits<R(A1, A2, A...)>
{
  using functionType = R(A1, A2, A...);
  using returnType = R;

  static const size_t arity = 2 + sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = typename std::tuple_element<n, std::tuple<A1, A2, A...>>::type;
    using bareType = typename std::decay<type>::type;
  };

  // Partial application: bind the first argument
  using boundType = std::function<R(A2, A...)>;
  static boundType bind1st(functionType f, A1 a1)
  {
    return [=] (A2 a2, A... args) -> R { return f(a1, a2, args...); };
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

// Remove const pointers and references
template <typename T>
struct function_traits<const T>
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
