/*<std-header orig-src='shore' incl-file-exclusion='W_VECTOR_H'>

 $Id: w_vector.h,v 1.10 1999/06/07 19:02:59 kupsch Exp $

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#ifndef W_VECTOR_H
#define W_VECTOR_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

#include "w_base.h"

template <class T>
class w_vector_t {

  public :
  w_vector_t(const unsigned setsize)
    {
      w_assert3(setsize > 0);
      size = setsize;
      data = new T[size];
    }
  w_vector_t(const w_vector_t & other)
    {
      size = other.size;
      data = new T[size];
      *this = other;
    }
  ~w_vector_t()
    {
      delete []data;
    }

  const unsigned vectorSize() const
    {
      return size;
    }

  const w_vector_t & operator = (const w_vector_t & other);
  const w_vector_t & operator = (const T & el);

  bool operator == (const T & el) const;
  bool operator > (const T & el) const;
  bool operator >= (const T & el) const;
  bool operator < (const T & el) const;
  bool operator <= (const T & el) const;

  bool operator == (const w_vector_t & other) const;
  bool operator != (const w_vector_t & other) const;
  bool operator <= (const w_vector_t & other) const;
  bool operator >= (const w_vector_t & other) const;
  bool operator < (const w_vector_t & other) const;
  bool operator > (const w_vector_t & other) const;

  const w_vector_t & operator += (const w_vector_t & other);
  const w_vector_t & operator -= (const w_vector_t & other);
  const w_vector_t & operator *= (const w_vector_t & other);
  const w_vector_t & operator /= (const w_vector_t & other);

  T & operator [] (const unsigned i) const
    {
      return data[i];
    }

  const T sum() const;

  const w_vector_t & max(const w_vector_t & other);
  const w_vector_t & min(const w_vector_t & other);

  private :
  unsigned size;
  T * data;
};


template <class T> 
const w_vector_t<T> & w_vector_t<T>::operator = (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);
  memcpy(data, other.data, sizeof(T) * size);
  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator = (const T & el)
{
  for (unsigned int i = 0; i < size; i++)
    data[i] = el;
  return *this;
}

template<class T> 
bool w_vector_t<T>::operator == (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] != other.data[i])
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator < (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] >= other.data[i])
      return false;

  return true;
}
  
template<class T> 
bool w_vector_t<T>::operator > (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] <= other.data[i])
      return false;

  return true;
}
  
template<class T> 
bool w_vector_t<T>::operator != (const w_vector_t<T> & other) const
{
  return !(*this == other);
}

template<class T> 
bool w_vector_t<T>::operator <= (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] > other.data[i])
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator >= (const w_vector_t<T> & other) const
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    if (data[i] < other.data[i])
      return false;

  return true;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator += (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] += other.data[i];

  return *this;
}
  
template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator -= (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] -= other.data[i];

  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator *= (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] *= other.data[i];

  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::operator /= (const w_vector_t<T> & other)
{
  w_assert3(size == other.size);

  for (unsigned int i = 0; i < size; i++)
    data[i] /= other.data[i];

  return *this;
}

template<class T> 
const T w_vector_t<T>::sum() const
{
  T acc = data[0];

  for (unsigned int i = 1; i < size; i++)
    acc += data[i];

  return acc;
}
  
template<class T> 
const w_vector_t<T> & w_vector_t<T>::max(const w_vector_t<T> & other)
{
  for (unsigned int i = 0; i < size; i++)
    if (other.data[i] > data[i])
      data[i] = other.data[i];

  return *this;
}

template<class T> 
const w_vector_t<T> & w_vector_t<T>::min(const w_vector_t<T> & other)
{
  for (unsigned int i = 0; i < size; i++)
    if (other.data[i] < data[i])
      data[i] = other.data[i];

  return *this;
}

template<class T> 
bool w_vector_t<T>::operator == (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] != el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator > (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] <= el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator < (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] >= el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator >= (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] < el)
      return false;

  return true;
}

template<class T> 
bool w_vector_t<T>::operator <= (const T & el) const
{
  for (unsigned int i = 0; i < size; i++)
    if (data[i] > el)
      return false;

  return true;
}

/*<std-footer incl-file-exclusion='W_VECTOR_H'>  -- do not edit anything below this line -- */

#endif          /*</std-footer>*/
