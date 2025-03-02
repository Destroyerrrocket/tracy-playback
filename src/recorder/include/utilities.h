#pragma once

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};
template <typename... Func> overloads(Func...) -> overloads<Func...>;