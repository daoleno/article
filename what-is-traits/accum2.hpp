// traits/accum2.hpp
#ifndef ACCUM_HPP
#define ACCUM_HPP
#include "accumtraits2.hpp"
template <typename T>
inline typename AccumulationTraits<T>::AccT accum(T const* beg, T const* end) {
  // return type is traits of the element type
  typedef typename AccumulationTraits<T>::AccT AccT;
  AccT total = AccT();  // assume T() actually creates a zero value
  while (beg != end) {
    total += *beg;
    ++beg;
  }
  return total;
}
#endif  // ACCUM_HPP