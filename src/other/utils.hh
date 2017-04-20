#pragma once

#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <utility> // for tuple::get

namespace utils {
namespace impl {
    template<typename ...T>
    struct voider { using type = void; };
}

template<typename ...T>
using void_t = typename impl:: voider<T...>:: type;

namespace impl {
    template<typename, template<class...> class Template, typename ...Args>
    struct can_apply_impl
        : public std:: false_type {};

    template<template<class...> class Template, typename ...Args>
    struct can_apply_impl<void_t<Template<Args...>>, Template, Args...>
        : public std:: true_type {
            using res = Template<Args...>;
    };
}

template<template<class...> class Template, typename ...Args>
using can_apply = impl:: can_apply_impl<void, Template, Args...>;

template<typename T>
std::ostream & operator<< (std:: ostream &o, const std:: vector<T> &v);
template<typename F, typename G>
std::ostream & operator<< (std:: ostream &o, const std:: pair<F, G> &pr);
template<typename F, typename G>
std::ostream & operator<< (std:: ostream &o, const std:: tuple<F, G> &pr);
template<typename F, typename G, typename H>
std::ostream & operator<< (std:: ostream &o, const std:: tuple<F, G, H> &pr);

template<typename F, typename G>
std::ostream & operator<< (std:: ostream &o, const std:: pair<F, G> &pr) {
    o << '('
        << std::get<0>(pr)
        << ','
        << std::get<1>(pr)
        << ')';
    return o;
}
template<typename F, typename G>
std::ostream & operator<< (std:: ostream &o, const std:: tuple<F, G> &pr) {
    o << '('
        << std::get<0>(pr)
        << ','
        << std::get<1>(pr)
        << ')';
    return o;
}
template<typename F, typename G, typename H>
std::ostream & operator<< (std:: ostream &o, const std:: tuple<F, G, H> &pr) {
    o << '('
        << std::get<0>(pr)
        << ','
        << std::get<1>(pr)
        << ','
        << std::get<2>(pr)
        << ')';
    return o;
}

template<typename T>
std::ostream & operator<< (std:: ostream &o, const std:: vector<T> &v) {
    o << '[';
    for(auto & e : v) {
        if(&e != &v.front())
            o << ',';
        o << e;
    }
    o << ']';


    return o;
}
template<>
inline
std::ostream & operator<< <uint8_t> (std:: ostream &o, const std:: vector<uint8_t> &v) {
    o << '[';
    for(auto & e : v) {
        if(&e != &v.front())
            o << ',';
        o << (int)e;
    }
    o << ']';


    return o;
}
template<typename T, typename ...Ts>
auto mk_vector(T t, Ts ...ts) -> std:: vector<T> {
    return std:: vector<T>{t, ts...};
}
template<typename T>
void print_type(T&&) {
    std:: cout << __PRETTY_FUNCTION__ << std:: endl;
}

template<typename T>
void pop_back_expected(std:: vector<T> &v, T const & expected) {
    assert(!v.empty());
    assert(expected == v.back());
    v.pop_back();
}
inline
void pop_back_expected_or_nan(std:: vector<long double> &v, long double expected) {
    assert(!v.empty());
    if(std::isnan(v.back())) {
        v.pop_back();
        return;
    }
    else
        pop_back_expected(v, expected);
}
template<typename T>
T    pop_back_and_return(std:: vector<T> &v) {
    assert(!v.empty());
    T was_at_the_back = std::move(v.back());
    v.pop_back();
    return was_at_the_back;
}
template<typename T>
std::ptrdiff_t ssize(const T& t) {
    return t.size();
}

template<typename T>
T lexical_cast(std:: string const & s);

template<>
inline
int lexical_cast<int>(std:: string const & s) {
    int d;
    int n;
    int ret = sscanf(s.c_str(), "%d %n", &d, &n);
    if(ret == 1 && n == ssize(s)) {
        return d;
    }
    else
        throw std:: invalid_argument{"can't parse this int"};
}
template<>
inline
double lexical_cast<double>(std:: string const & s) {
    double val;
    int n;
    int ret = sscanf(s.c_str(), "%lg %n", &val, &n);
    if(ret == 1 && n == ssize(s)) {
        return val;
    }
    else
        throw std:: invalid_argument{std::string("Can't parse this double [") + s + ']'};
}
std:: vector<std:: string>   tokenize       ( std:: string      const & line
                                            , char                delimiter
        );

template<int i>
struct priority_tag;
template<int i>
struct priority_tag : public priority_tag<i-1> {};
template<>
struct priority_tag<0> {};

template<typename ...>
struct and_all_impl;

template<typename ...Ts>
constexpr
auto and_all(Ts ...ts) { return and_all_impl<Ts...>::impl(ts...); }

template<typename T0>
struct and_all_impl<T0> {
    static constexpr
    T0  impl(T0 t0) { return t0; }
};
template<typename T0, typename T1, typename ...Ts>
struct and_all_impl<T0, T1, Ts...> {
    static constexpr
    auto  impl(T0 t0, T1 t1, Ts ...ts) { return and_all(t0 && t1, ts...); }
};

// This is my own 'apply', as it's officially only in c++17, not c++14
namespace detail {
template <class F, class Tuple, std::size_t... I>
constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>)
{
    return std::forward<F>(f)(std::get<I>(std::forward<Tuple>(t))...);
}
}  // namespace detail

template <class F, class Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
    return detail::apply_impl(
        std::forward<F>(f), std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>{}>{});
}

struct non_copyable_empty {
    non_copyable_empty             ()                           = default;
    non_copyable_empty             (non_copyable_empty const &) = delete;
    non_copyable_empty             (non_copyable_empty      &&) = default;
    non_copyable_empty & operator= (non_copyable_empty const &) = delete;
    non_copyable_empty & operator= (non_copyable_empty      &&) = default;
};
struct empty {
};


} // namespace utils
