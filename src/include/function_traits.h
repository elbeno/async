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
  using functionType = std::function<R(A...)>;
  using returnType = R;

  static const size_t arity = sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = typename std::tuple_element<n, std::tuple<A...>>::type;
    using bareType = typename std::decay<type>::type;
  };

  // Application calls the function
  using appliedType = R;
  static inline appliedType apply(functionType f, A&&... args)
  {
    return f(std::forward<A...>(args...));
  }
};

// Specialization for 2 argument functions, to enable partial application
template <typename R, typename A1, typename A2>
struct function_traits<R(A1, A2)>
{
  using functionType = std::function<R(A1, A2)>;
  using returnType = R;

  static const size_t arity = 2;

  template <size_t n>
  struct Arg
  {
    using type = typename std::tuple_element<n, std::tuple<A1, A2>>::type;
    using bareType = typename std::decay<type>::type;
  };

  // (Partial) Application binds the first argument
  using appliedType = std::function<R(A2)>;
  static inline appliedType apply(functionType f, A1&& a1)
  {
    // in case a1 is an rvalue ref, we need to capture "by move" and make it
    // mutable, to call the function with a move
    return [f, a = std::move(a1)] (A2&& a2) mutable -> R
    {
      return f(std::move(a), std::forward<A2>(a2));
    };
  }
};

// Specialization for 3+ argument functions, to enable partial application
template <typename R, typename A1, typename A2, typename... A>
struct function_traits<R(A1, A2, A...)>
{
  using functionType = std::function<R(A1, A2, A...)>;
  using returnType = R;

  static const size_t arity = 2 + sizeof...(A);

  template <size_t n>
  struct Arg
  {
    using type = typename std::tuple_element<n, std::tuple<A1, A2, A...>>::type;
    using bareType = typename std::decay<type>::type;
  };

  // (Partial) Application binds the first argument
  using appliedType = std::function<R(A2, A...)>;
  static inline appliedType apply(functionType f, A1&& a1)
  {
    // see capture-by-move note above
    return [f, a = std::move(a1)] (A2&& a2, A&&... args) mutable -> R
    {
      return f(std::move(a), std::forward<A2>(a2), std::forward<A...>(args...));
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
