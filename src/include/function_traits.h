#pragma once

#include <tuple>
#include <type_traits>

template <typename T>
struct function_traits
  : public function_traits<decltype(&T::operator())>
{};

template <typename R, typename... A>
struct function_traits<R(A...)>
{
  typedef R returnType;
  typedef R functionType(A...);

  enum { arity = sizeof...(A) };

  template <size_t n>
  struct Arg
  {
    typedef typename std::tuple_element<n, std::tuple<A...>>::type type;

    typedef typename std::remove_cv<
      typename std::remove_reference<
      typename std::tuple_element<n, std::tuple<A...>>
        ::type>::type>::type bareType;
  };
};

template <typename R, typename... A>
struct function_traits<R(*)(A...)>
  : public function_traits<R(A...)>
{};

template <typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...)>
  : public function_traits<R(A...)>
{};

template <typename C, typename R, typename... A>
struct function_traits<R(C::*)(A...) const>
  : public function_traits<R(A...)>
{};
