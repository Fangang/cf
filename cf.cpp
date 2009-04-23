// Copyright (C) 2009 Chris Double. All Rights Reserved.
// See the license at the end of this file
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <functional>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include "cf.h"

// If defined, compiles as a test applicatation that tests
// that things are working.
#if defined(TEST)
#include <boost/test/minimal.hpp>
#endif

using namespace std;
using namespace boost;
using namespace boost::lambda;

// Assert a condition is true and throw an XYError if it is not
#define xy_assert(condition, code) xy_assert_impl((condition), (code), xy, __FILE__, __LINE__)

void xy_assert_impl(bool condition, XYError::code c, shared_ptr<XY> const& xy, char const* file, int line) {
  if (!condition)
    throw XYError(xy, c, file, line);
}

// Given an input string, unescape any special characters
string unescape(string s) {
  string r1 = replace_all_copy(s, "\\\"", "\"");
  string r2 = replace_all_copy(r1, "\\n", "\n");
  return r2;
}
 
// Given an input string, escape any special characters
string escape(string s) {
  string r1 = replace_all_copy(s, "\"", "\\\"");
  string r2 = replace_all_copy(r1, "\n", "\\n");
  return r2;
}
 
// This lovely piece of code is to allow XY objects to be called within
// Boost lambda expressions, even though they are actually shared_ptr
// objects instead of XYObjects.
namespace boost {
  namespace lambda {
    template <class R> struct function_adaptor< R (XYObject::*)() const >
    {
      template <class T> struct sig { typedef R type; };
      template <class RET>
       static string apply(R (XYObject::*func)() const, shared_ptr<XYObject>const & o) {
          return (o.get()->*func)();
        }
    };
    template <class R, class D> struct function_adaptor< R (XYObject::*)(D) const >
    {
      template <class T> struct sig { typedef R type; };
      template <class RET>
       static string apply(R (XYObject::*func)(D) const, shared_ptr<XYObject>const & o,D d) {
          return (o.get()->*func)(d);
        }
    };
  }
}

// Macro to implement double dispatch operations in class
#define DD_IMPL(class, name)						\
  shared_ptr<XYObject> class::name(XYObject* rhs) { return rhs->name(this); } \
  shared_ptr<XYObject> class::name(XYFloat* lhs) { return dd_##name(lhs, this); } \
  shared_ptr<XYObject> class::name(XYInteger* lhs) { return dd_##name(lhs, this); } \
  shared_ptr<XYObject> class::name(XYSequence* lhs) { return dd_##name(lhs, this); }

#define DD_IMPL2(name, op) \
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYInteger* rhs) { \
  return msp(new XYFloat(lhs->mValue op rhs->as_float()->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYFloat* lhs, XYSequence* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i).get()));	\
  return list; \
}\
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYFloat* rhs) { \
  return msp(new XYFloat(lhs->as_float()->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYInteger* rhs) { \
  return msp(new XYInteger(lhs->mValue op rhs->mValue)); \
} \
\
static shared_ptr<XYObject> dd_##name(XYInteger* lhs, XYSequence* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  size_t len = rhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->name(rhs->at(i).get()));	\
  return list; \
} \
\
static shared_ptr<XYObject> dd_##name(XYSequence* lhs, XYObject* rhs) { \
  shared_ptr<XYList> list(new XYList()); \
  size_t len = lhs->size();\
  for(int i=0; i < len; ++i)\
    list->mList.push_back(lhs->at(i)->name(rhs));	\
  return list; \
} \
\
static shared_ptr<XYObject> dd_##name(XYSequence* lhs, XYSequence* rhs) { \
  assert(lhs->size() == rhs->size()); \
  shared_ptr<XYList> list(new XYList()); \
  size_t lhs_len = lhs->size();\
  size_t rhs_len = rhs->size();\
  for(int li = 0, ri = 0; li < lhs_len && ri < rhs_len; ++li, ++ri)\
    list->mList.push_back(lhs->at(li)->name(rhs->at(ri).get()));    \
  return list; \
}

DD_IMPL2(add, +)
DD_IMPL2(subtract, -)
DD_IMPL2(multiply, *)

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->mValue));
}

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->mValue / rhs->as_float()->mValue));
}

static shared_ptr<XYObject> dd_divide(XYFloat* lhs, XYSequence* rhs) { 
  shared_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i).get()));
  return list;
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->mValue));
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYInteger* rhs) {
  return msp(new XYFloat(lhs->as_float()->mValue / rhs->as_float()->mValue));
}

static shared_ptr<XYObject> dd_divide(XYInteger* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->divide(rhs->at(i).get()));
  return list;
}

static shared_ptr<XYObject> dd_divide(XYSequence* lhs, XYObject* rhs) {
  shared_ptr<XYList> list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->divide(rhs));
  return list;
}


static shared_ptr<XYObject> dd_divide(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  shared_ptr<XYList> list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for(int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->divide(rhs->at(ri).get()));
  return list; 
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYInteger* rhs) {
  shared_ptr<XYFloat> result(new XYFloat(lhs->mValue));
  mpf_pow_ui(result->mValue.get_mpf_t(), lhs->mValue.get_mpf_t(), rhs->as_uint());
  return result;
}

static shared_ptr<XYObject> dd_power(XYFloat* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i).get()));
  return list;
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYFloat* rhs) {
  return msp(new XYFloat(pow(static_cast<double>(lhs->mValue.get_d()), 
			     static_cast<double>(rhs->mValue.get_d()))));
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYInteger* rhs) {
  shared_ptr<XYInteger> result(new XYInteger(lhs->mValue));
  mpz_pow_ui(result->mValue.get_mpz_t(), lhs->mValue.get_mpz_t(), rhs->as_uint());
  return result;
}

static shared_ptr<XYObject> dd_power(XYInteger* lhs, XYSequence* rhs) {
  shared_ptr<XYList> list(new XYList());
  size_t len = rhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->power(rhs->at(i).get()));
  return list;
}

static shared_ptr<XYObject> dd_power(XYSequence* lhs, XYObject* rhs) {
  shared_ptr<XYList> list(new XYList());
  size_t len = lhs->size();
  for (int i=0; i < len; ++i)
    list->mList.push_back(lhs->at(i)->power(rhs));
  return list;
}


static shared_ptr<XYObject> dd_power(XYSequence* lhs, XYSequence* rhs) {
  assert(lhs->size() == rhs->size());
  shared_ptr<XYList> list(new XYList());
  size_t lhs_len = lhs->size();
  size_t rhs_len = rhs->size();
  for (int li=0, ri=0; li < lhs_len && ri < rhs_len; ++li, ++ri)
    list->mList.push_back(lhs->at(li)->power(rhs->at(ri).get()));
  return list;
}

// XYObject
shared_ptr<XYObject> XYObject::add(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::add(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::subtract(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::multiply(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::divide(XYSequence* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYObject* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYFloat* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYInteger* rhs) {
  assert(1==0);
}

shared_ptr<XYObject> XYObject::power(XYSequence* rhs) {
  assert(1==0);
}

// XYNumber
XYNumber::XYNumber(Type type) : mType(type) { }

// XYFloat
DD_IMPL(XYFloat, add)
DD_IMPL(XYFloat, subtract)
DD_IMPL(XYFloat, multiply)
DD_IMPL(XYFloat, divide)
DD_IMPL(XYFloat, power)

XYFloat::XYFloat(long v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(double v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(string v) : XYNumber(FLOAT), mValue(v) { }
XYFloat::XYFloat(mpf_class const& v) : XYNumber(FLOAT), mValue(v) { }

string XYFloat::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYFloat::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

int XYFloat::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYFloat> o = dynamic_pointer_cast<XYFloat>(rhs);
  if (!o) {
    shared_ptr<XYInteger> i = dynamic_pointer_cast<XYInteger>(rhs);
    if (i)
      return cmp(mValue, i->mValue);
    else
      return toString(true).compare(rhs->toString(true));
  }

  return cmp(mValue, o->mValue);
}

bool XYFloat::is_zero() const {
  return mValue == 0;
}

unsigned int XYFloat::as_uint() const {
  return mValue.get_ui();
}

shared_ptr<XYInteger> XYFloat::as_integer() {
  return msp(new XYInteger(mValue));
}

shared_ptr<XYFloat> XYFloat::as_float() {
  return dynamic_pointer_cast<XYFloat>(shared_from_this());
}

shared_ptr<XYNumber> XYFloat::floor() {
  shared_ptr<XYFloat> result(new XYFloat(::floor(mValue)));
  return result;
}

// XYInteger
DD_IMPL(XYInteger, add)
DD_IMPL(XYInteger, subtract)
DD_IMPL(XYInteger, multiply)
DD_IMPL(XYInteger, divide)
DD_IMPL(XYInteger, power)
XYInteger::XYInteger(long v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(string v) : XYNumber(INTEGER), mValue(v) { }
XYInteger::XYInteger(mpz_class const& v) : XYNumber(INTEGER), mValue(v) { }

string XYInteger::toString(bool) const {
  return lexical_cast<string>(mValue);
}

void XYInteger::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

int XYInteger::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYInteger> o = dynamic_pointer_cast<XYInteger>(rhs);
  if (!o) {
    shared_ptr<XYFloat> f = dynamic_pointer_cast<XYFloat>(rhs);
    if (f)
      return cmp(mValue, f->mValue);
    else
      return toString(true).compare(rhs->toString(true));
  }

  return cmp(mValue, o->mValue);
}

bool XYInteger::is_zero() const {
  return mValue == 0;
}

unsigned int XYInteger::as_uint() const {
  return mValue.get_ui();
}

shared_ptr<XYInteger> XYInteger::as_integer() {
  return dynamic_pointer_cast<XYInteger>(shared_from_this());
}

shared_ptr<XYFloat> XYInteger::as_float() {
  return msp(new XYFloat(mValue));
}

shared_ptr<XYNumber> XYInteger::floor() {
  return dynamic_pointer_cast<XYNumber>(shared_from_this());
}

// XYSymbol
XYSymbol::XYSymbol(string v) : mValue(v) { }

string XYSymbol::toString(bool) const {
  return mValue;
}

void XYSymbol::eval1(shared_ptr<XY> const& xy) {
  XYEnv::iterator it = xy->mP.find(mValue);
  if (it != xy->mP.end())
    // Primitive symbol, execute immediately
    (*it).second->eval1(xy);
  else
    xy->mX.push_back(shared_from_this());
}

int XYSymbol::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYSymbol> o = dynamic_pointer_cast<XYSymbol>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

// XYString
XYString::XYString(string v) : mValue(v) { }

string XYString::toString(bool parse) const {
  if (parse) {
    ostringstream s;
    s << '\"' << escape(mValue) << '\"';
    return s.str();
  }
  
  return mValue;
}

void XYString::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

int XYString::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYString> o = dynamic_pointer_cast<XYString>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mValue.compare(o->mValue);
}

size_t XYString::size()
{
  return mValue.size();
}

void XYString::pushBackInto(List& list) {
  for(string::iterator it = mValue.begin(); it != mValue.end(); ++it)
    list.push_back(msp(new XYInteger(*it)));
}

shared_ptr<XYObject> XYString::at(size_t n)
{
  return msp(new XYInteger(mValue[n]));
}

shared_ptr<XYObject> XYString::head()
{
  assert(mValue.size() > 0);
  return msp(new XYInteger(mValue[0]));
}

shared_ptr<XYSequence> XYString::tail()
{
  if (mValue.size() <= 1) 
    return msp(new XYString(""));

  return msp(new XYString(mValue.substr(1)));
}

boost::shared_ptr<XYSequence> XYString::join(boost::shared_ptr<XYSequence> const& rhs)
{
  XYString const* rhs_string = dynamic_cast<XYString const*>(rhs.get());
  if (rhs_string) {
    if (this->shared_from_this().use_count() == 2) {
      mValue += rhs_string->mValue;
      return dynamic_pointer_cast<XYSequence>(this->shared_from_this());
    }

    return msp(new XYString(mValue + rhs_string->mValue));
  }

  shared_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYShuffle
XYShuffle::XYShuffle(string v) { 
  vector<string> result;
  split(result, v, is_any_of("-"));
  assert(result.size() == 2);
  mBefore = result[0];
  mAfter  = result[1];
}

string XYShuffle::toString(bool) const {
  ostringstream out;
  out << mBefore << "-" << mAfter;
  return out.str();
}

void XYShuffle::eval1(shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= mBefore.size(), XYError::STACK_UNDERFLOW);
  map<char, shared_ptr<XYObject> > env;
  for(string::reverse_iterator it = mBefore.rbegin(); it != mBefore.rend(); ++it) {
    env[*it] = xy->mX.back();
    xy->mX.pop_back();
  }

  for(string::iterator it = mAfter.begin(); it != mAfter.end(); ++it) {
    assert(env.find(*it) != env.end());
    xy->mX.push_back(env[*it]);
  }
}

int XYShuffle::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYShuffle> o = dynamic_pointer_cast<XYShuffle>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return (mBefore + mAfter).compare(o->mBefore + o->mAfter);
}

// XYSequence
DD_IMPL(XYSequence, add)
DD_IMPL(XYSequence, subtract)
DD_IMPL(XYSequence, multiply)
DD_IMPL(XYSequence, divide)
DD_IMPL(XYSequence, power)
int XYSequence::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  size_t lhs_len = size();
  size_t rhs_len = o->size();
  int li = 0;
  int ri = 0;

  for(li=0, ri=0;li < lhs_len && ri < rhs_len; ++li, ++ri) {
    int c = at(li)->compare(o->at(ri));
    if (c != 0)
      return c;
  }

  if(li != lhs_len)
    return -1;

  if(ri != rhs_len)
    return 1;

  return 0;
}

// XYList
XYList::XYList() { }

template <class InputIterator>
XYList::XYList(InputIterator first, InputIterator last) {
  mList.assign(first, last);
}

string XYList::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for_each(mList.begin(), mList.end(), s << bind(&XYObject::toString, _1, parse) << " ");
  s << "]";
  return s.str();
}

void XYList::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

size_t XYList::size()
{
  return mList.size();
}

void XYList::pushBackInto(List& list) {
  for(List::iterator it = mList.begin(); it != mList.end(); ++it)
    list.push_back(*it);
}

shared_ptr<XYObject> XYList::at(size_t n)
{
  return mList[n];
}

shared_ptr<XYObject> XYList::head()
{
  assert(mList.size() > 0);
  return mList[0];
}

shared_ptr<XYSequence> XYList::tail()
{
  if (mList.size() <= 1) 
    return msp(new XYList());

  return msp(new XYSlice(dynamic_pointer_cast<XYSequence>(shared_from_this()), 1, mList.size()));
}

boost::shared_ptr<XYSequence> XYList::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYSlice
XYSlice::XYSlice(shared_ptr<XYSequence> original,
                 int begin,
		 int end)  :
  mOriginal(original),
  mBegin(begin),
  mEnd(end)
{   
  shared_ptr<XYSlice> slice;
  while ((slice = dynamic_pointer_cast<XYSlice>(mOriginal))) {
    // Find the original, non-slice sequence. Without this we can
    // corrupt the C stack due to too much recursion when destroying
    // the tree of slices.
    mOriginal = slice->mOriginal;
    mBegin += slice->mBegin;
    mEnd += slice->mEnd;
  }
}

string XYSlice::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for (int i=mBegin; i < mEnd; ++i) 
    s << mOriginal->at(i)->toString(parse) << " ";
  s << "]";
  return s.str();
}

void XYSlice::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

size_t XYSlice::size()
{
  return mEnd - mBegin;
}

void XYSlice::pushBackInto(List& list) {
  for (int i = mBegin; i != mEnd; ++i)
    list.push_back(mOriginal->at(i));
}

shared_ptr<XYObject> XYSlice::at(size_t n)
{
  assert(mBegin + n < mEnd);
  return mOriginal->at(mBegin + n);
}

shared_ptr<XYObject> XYSlice::head()
{
  assert(mBegin != mEnd);
  return mOriginal->at(mBegin);
}

shared_ptr<XYSequence> XYSlice::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  return msp(new XYSlice(mOriginal, mBegin+1, mEnd));
}

boost::shared_ptr<XYSequence> XYSlice::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYSequence> self(dynamic_pointer_cast<XYSequence>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.push_front(self);
      return result;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.push_back(self);
      result->mSequences.insert(result->mSequences.end(), 
		  	        join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  return msp(new XYJoin(self, rhs));
}

// XYJoin
XYJoin::XYJoin(shared_ptr<XYSequence> first,
               shared_ptr<XYSequence> second)
{ 
  mSequences.push_back(first);
  mSequences.push_back(second);
}

string XYJoin::toString(bool parse) const {
  ostringstream s;
  s << "[ ";
  for(const_iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    for (int i=0; i < (*it)->size(); ++i)
      s << (*it)->at(i)->toString(parse) << " ";
  }
  s << "]";
  return s.str();
}

void XYJoin::eval1(shared_ptr<XY> const& xy) {
  xy->mX.push_back(shared_from_this());
}

size_t XYJoin::size()
{
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) 
    s += (*it)->size();

  return s;
}

shared_ptr<XYObject> XYJoin::at(size_t n)
{
  assert(n < size());
  size_t s = 0;
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    size_t b = s;
    s += (*it)->size();
    if (n < s)
      return (*it)->at(n-b);
  }

  assert(1 == 0);
}

void XYJoin::pushBackInto(List& list)
{  
  for(iterator it = mSequences.begin(); it != mSequences.end(); ++it) {
    (*it)->pushBackInto(list);
  }
}

shared_ptr<XYObject> XYJoin::head()
{
  return at(0);
}

shared_ptr<XYSequence> XYJoin::tail()
{
  if (size() <= 1)
    return msp(new XYList());

  return msp(new XYSlice(dynamic_pointer_cast<XYSequence>(shared_from_this()), 1, size()));
}

boost::shared_ptr<XYSequence> XYJoin::join(boost::shared_ptr<XYSequence> const& rhs)
{
  shared_ptr<XYJoin> self(dynamic_pointer_cast<XYJoin>(shared_from_this()));

  if (dynamic_cast<XYJoin const*>(rhs.get())) {
    // If the reference to rhs is unique then we can modify the object itself.
    if (rhs.unique()) {    
      shared_ptr<XYJoin> result(dynamic_pointer_cast<XYJoin>(rhs));
      result->mSequences.insert(result->mSequences.begin(), 
				mSequences.begin(), mSequences.end());
      return result;
    }
    else if (self.use_count() == 2) { // to account for the 1 reference we are holding
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      mSequences.insert(mSequences.end(), 
			join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return self;
    }
    else {
      // Pointer is shared, we have to copy the data
      shared_ptr<XYJoin> join_rhs(dynamic_pointer_cast<XYJoin>(rhs));
      shared_ptr<XYJoin> result(new XYJoin());
      result->mSequences.insert(result->mSequences.end(), 
				mSequences.begin(), mSequences.end());
      result->mSequences.insert(result->mSequences.end(), 
				join_rhs->mSequences.begin(), join_rhs->mSequences.end());
      return result;
    }
  }

  // rhs is not a join
  if (self.use_count() == 2) { // to account for the 1 reference we are holding
    mSequences.push_back(rhs);
    return self;
  }
  
  // rhs is not a join and we can't modify ourselves
  shared_ptr<XYJoin> result(new XYJoin());
  result->mSequences.insert(result->mSequences.end(), 
			    mSequences.begin(), mSequences.end());
  result->mSequences.push_back(rhs);
  return result;
}

// XYPrimitive
XYPrimitive::XYPrimitive(string n, void (*func)(shared_ptr<XY> const&)) : mName(n), mFunc(func) { }

string XYPrimitive::toString(bool) const {
  return mName;
}

void XYPrimitive::eval1(shared_ptr<XY> const& xy) {
  mFunc(xy);
}

int XYPrimitive::compare(shared_ptr<XYObject> rhs) {
  shared_ptr<XYPrimitive> o = dynamic_pointer_cast<XYPrimitive>(rhs);
  if (!o)
    return toString(true).compare(rhs->toString(true));

  return mName.compare(o->mName);
}
 
// Primitive Implementations

// + [X^lhs^rhs] Y] -> [X^lhs+rhs Y]
static void primitive_addition(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->add(rhs.get()));
}

// - [X^lhs^rhs] Y] -> [X^lhs-rhs Y]
static void primitive_subtraction(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->subtract(rhs.get()));
}

// * [X^lhs^rhs] Y] -> [X^lhs*rhs Y]
static void primitive_multiplication(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->multiply(rhs.get()));
}

// % [X^lhs^rhs] Y] -> [X^lhs/rhs Y]
static void primitive_division(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->divide(rhs.get()));
}

// ^ [X^lhs^rhs] Y] -> [X^lhs**rhs Y]
static void primitive_power(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs(xy->mX.back());
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs(xy->mX.back());
  assert(lhs);
  xy->mX.pop_back();

  xy->mX.push_back(lhs->power(rhs.get()));
}

// _ floor [X^n] Y] -> [X^n Y]
static void primitive_floor(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  xy->mX.push_back(n->floor());
}

// set [X^value^name Y] -> [X Y] 
static void primitive_set(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYObject> value = xy->mX.back();
  xy->mX.pop_back();

  xy->mEnv[name->mValue] = value;
}

// get [X^name Y] [X^value Y]
static void primitive_get(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSymbol> name = dynamic_pointer_cast<XYSymbol>(xy->mX.back());
  xy_assert(name, XYError::TYPE);
  xy->mX.pop_back();

  XYEnv::iterator it = xy->mEnv.find(name->mValue);
  xy_assert(it != xy->mEnv.end(), XYError::SYMBOL_NOT_FOUND);

  shared_ptr<XYObject> value = (*it).second;
  xy->mX.push_back(value);
}

// . [X^{O1..On} Y] [X O1^..^On^Y]
static void primitive_unquote(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> o = xy->mX.back();
  xy->mX.pop_back();

  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(o);

  if (list) {
    XYStack temp;
    list->pushBackInto(temp);

    xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
  }
  else {
    shared_ptr<XYSymbol> symbol = dynamic_pointer_cast<XYSymbol>(o);
    if (symbol) {
      // If it's a symbol, get the value of the symbol and apply
      // unquote to that.
      XYEnv::iterator it = xy->mEnv.find(symbol->mValue);
      if (it != xy->mEnv.end()) {
	xy->mX.push_back((*it).second);
	primitive_unquote(xy);
      }
      else {
	// If the symbol doesn't exist in the environment, just
	// return the symbol itself.
	xy->mX.push_back(symbol);
      }
    }
    else 
      xy->mY.push_front(o);
  }
}

// ) [X^{pattern} Y] [X^result Y]
static void primitive_pattern_ss(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  shared_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(pattern, XYError::TYPE);
  xy->mX.pop_back();
  assert(pattern->size() != 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(pattern->at(0), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->size() > 1) {
    int start = 0;
    int end   = pattern->size();
    shared_ptr<XYList> list(new XYList());    
    xy->replacePattern(env, msp(new XYSlice(pattern, ++start, end)), back_inserter(list->mList));
    assert(list->size() > 0);

    // Append to stack
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mX.insert(xy->mX.end(), list->mList.begin(), list->mList.end());
  }
}

// ( [X^{pattern} Y] [X result^Y]
static void primitive_pattern_sq(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  // Get the pattern from the stack
  shared_ptr<XYSequence> pattern = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(pattern, XYError::TYPE);
  xy->mX.pop_back();
  assert(pattern->size() != 0);

  // Populate env with a mapping between the pattern variables to the
  // values on the stack.
  XYEnv env;
  xy->getPatternValues(pattern->at(0), inserter(env, env.begin()));
  // Process pattern body using these mappings.
  if (pattern->size() > 1) {
    int start = 0;
    int end   = pattern->size();
    shared_ptr<XYList> list(new XYList());
    xy->replacePattern(env, msp(new XYSlice(pattern, ++start, end)), back_inserter(list->mList));
    assert(list->size() > 0);

    // Prepend to queue
    list = dynamic_pointer_cast<XYList>(list->mList[0]);
    xy_assert(list, XYError::TYPE);
    xy->mY.insert(xy->mY.begin(), list->mList.begin(), list->mList.end());
  }
}

// ` dip [X^b^{a0..an} Y] [X a0..an^b^Y]
static void primitive_dip(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  xy->mY.push_front(o);
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end());
}

// | reverse [X^{a0..an} Y] [X^{an..a0} Y]
static void primitive_reverse(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYList> reversed(new XYList());
  list->pushBackInto(reversed->mList);
  reverse(reversed->mList.begin(), reversed->mList.end());
  xy->mX.push_back(reversed);
}

// \ quote [X^o Y] [X^{o} Y]
static void primitive_quote(boost::shared_ptr<XY> const& xy) {
  assert(xy->mY.size() >= 1);
  shared_ptr<XYObject> o = xy->mY.front();
  assert(o);
  xy->mY.pop_front();

  shared_ptr<XYList> list = msp(new XYList());
  list->mList.push_back(o);
  xy->mX.push_back(list);
}

// , join [X^a^b Y] [X^{...} Y]
static void primitive_join(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);
  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  shared_ptr<XYSequence> list_lhs = dynamic_pointer_cast<XYSequence>(lhs);
  shared_ptr<XYSequence> list_rhs = dynamic_pointer_cast<XYSequence>(rhs);

  if (list_lhs && list_rhs) {
    // Reset original pointers to enable copy on write optimisations
    rhs.reset(static_cast<XYSequence*>(0));
    lhs.reset(static_cast<XYSequence*>(0));

    // Two lists are concatenated
    xy->mX.push_back(list_lhs->join(list_rhs));
  }
  else if(list_lhs) {
    // Reset original pointers to enable copy on write optimisations
    lhs.reset(static_cast<XYSequence*>(0));

    // If rhs is not a list, it is added to the end of the list.
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(rhs);
    xy->mX.push_back(list_lhs->join(list));
  }
  else if(list_rhs) {
    // Reset original pointers to enable copy on write optimisations
    rhs.reset(static_cast<XYSequence*>(0));

    // If lhs is not a list, it is added to the front of the list
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    xy->mX.push_back(list->join(list_rhs));
  }
  else {
    // If neither are lists, a list is made containing the two items
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(lhs);
    list->mList.push_back(rhs);
    xy->mX.push_back(list);
  }
}

// $ stack - Expects a program on the X stack. That program
// has stack effect ( stack queue -- stack queue ). $ will
// call this program with X and Y on the stack, and replace
// X and Y with the results.
static void primitive_stack(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSequence> list  = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYList> stack(new XYList(xy->mX.begin(), xy->mX.end()));
  shared_ptr<XYList> queue(new XYList(xy->mY.begin(), xy->mY.end()));

  xy->mX.push_back(stack);
  xy->mX.push_back(queue);
  xy->mY.push_front(msp(new XYSymbol("$$")));
  XYStack temp;
  list->pushBackInto(temp);
  xy->mY.insert(xy->mY.begin(), temp.begin(), temp.end()); 
}

// $$ stackqueue - Helper word for '$'. Given a stack and queue on the
// stack, replaces the existing stack and queue with them.
static void primitive_stackqueue(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYSequence> queue = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(queue, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYSequence> stack = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(stack, XYError::TYPE);
  xy->mX.pop_back();

  XYStack stemp;
  stack->pushBackInto(stemp);

  XYQueue qtemp;
  for( int i=0; i < queue->size(); ++i)
    qtemp.push_back(queue->at(i));

  xy->mX.assign(stemp.begin(), stemp.end());
  xy->mY.assign(qtemp.begin(), qtemp.end());
}

// = equals [X^a^b Y] [X^? Y] 
static void primitive_equals(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) == 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <  [X^a^b Y] [X^? Y] 
static void primitive_lessThan(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) < 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >  [X^a^b Y] [X^? Y] 
static void primitive_greaterThan(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) > 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// <=  [X^a^b Y] [X^? Y] 
static void primitive_lessThanEqual(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) <= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}

// >=  [X^a^b Y] [X^? Y] 
static void primitive_greaterThanEqual(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> rhs = xy->mX.back();
  assert(rhs);
  xy->mX.pop_back();

  shared_ptr<XYObject> lhs = xy->mX.back();
  assert(lhs);
  xy->mX.pop_back();

  if (lhs->compare(rhs) >= 0)
    xy->mX.push_back(msp(new XYInteger(1)));
  else
    xy->mX.push_back(msp(new XYInteger(0)));
}


// not not [X^a Y] [X^? Y] 
static void primitive_not(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> o = xy->mX.back();
  assert(o);
  xy->mX.pop_back();

  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(o);
  if (n && n->is_zero()) {
    xy->mX.push_back(msp(new XYInteger(1)));
  }
  else {
    shared_ptr<XYSequence> l = dynamic_pointer_cast<XYSequence>(o);
    if(l && l->size() == 0)
      xy->mX.push_back(msp(new XYInteger(1)));
    else
      xy->mX.push_back(msp(new XYInteger(0)));
  }
}

// @ nth [X^n^{...} Y] [X^o Y] 
static void primitive_nth(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 2, XYError::STACK_UNDERFLOW);

  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(list, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYObject> index(xy->mX.back());
  assert(index);
  xy->mX.pop_back();

  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(index);
  shared_ptr<XYSequence> s = dynamic_pointer_cast<XYSequence>(index);
  xy_assert(n || s, XYError::TYPE);

  if (n) {
    // Index is a number, do a direct index into the list
    if (n->as_uint() >= list->size()) 
      xy->mX.push_back(msp(new XYInteger(list->size())));
    else
      xy->mX.push_back(list->at(n->as_uint()));    
  }
  else if (s) {
    // Index is a list. Use this as a path into the list.
    if (s->size() == 0) {
      // If the path is empty, return the entire list
      xy->mX.push_back(list);
    }    
    else {
      shared_ptr<XYObject> head = s->head();
      shared_ptr<XYSequence> tail = s->tail();

      // If head is a list, then it's a request to get multiple values
      shared_ptr<XYSequence> headlist = dynamic_pointer_cast<XYSequence>(head);
      if (headlist && headlist->size() == 0) {
	if (tail->size() == 0) {
	  xy->mX.push_back(list);
	}
	else {
	  XYSequence::List code;
	  XYSequence::List code2;
	  for (int i=0; i < list->size(); ++i) {
	    code.push_back(tail);
	    code.push_back(list->at(i));
	    code.push_back(msp(new XYSymbol("@")));
	    code.push_back(msp(new XYSymbol("'")));
	    code.push_back(msp(new XYSymbol("'")));
	    code.push_back(msp(new XYSymbol("`")));	  
	  }	
	  for (int i=0; i < list->size() - 1; ++i) {
	    code2.push_back(msp(new XYSymbol(",")));
	  }
	  xy->mY.insert(xy->mY.begin(), code2.begin(), code2.end());
	  xy->mY.insert(xy->mY.begin(), code.begin(), code.end());	  
	}
      }
      else if (headlist) {
	XYSequence::List code;
	XYSequence::List code2;
	for (int i=0; i < headlist->size(); ++ i) {
	  code.push_back(headlist->at(i));
	  code.push_back(list);
	  code.push_back(msp(new XYSymbol("@")));			
	  code.push_back(msp(new XYSymbol("'")));
	  code.push_back(msp(new XYSymbol("'")));
	  code.push_back(msp(new XYSymbol("`")));
	}
	
	for (int i=0; i < headlist->size()-1; ++i) {
	  code2.push_back(msp(new XYSymbol(",")));
	}

	xy->mY.insert(xy->mY.begin(), code2.begin(), code2.end());
	xy->mY.insert(xy->mY.begin(), code.begin(), code.end());
      }
      else if (tail->size() == 0) {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(msp(new XYSymbol("@")));
      }
      else {
	xy->mX.push_back(head);
	xy->mX.push_back(list);
	xy->mY.push_front(msp(new XYSymbol("@")));
	xy->mY.push_front(msp(new XYShuffle("ab-ba")));
	xy->mY.push_front(tail);
	xy->mY.push_front(msp(new XYSymbol("@")));
      }
    }
  }
}

// print [X^n Y] [X Y] 
static void primitive_print(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(true);
}

// println [X^n Y] [X Y] 
static void primitive_println(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(true) << endl;
}


// write [X^n Y] [X Y] 
static void primitive_write(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  cout << o->toString(false);
}

// count [X^{...} Y] [X^n Y] 
// Returns the length of any list. If the item at the top of the
// stack is an atom, returns 1.
static void primitive_count(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYObject> o(xy->mX.back());
  assert(o);
  xy->mX.pop_back();

  shared_ptr<XYSequence> list(dynamic_pointer_cast<XYSequence>(o));
  if (list)
    xy->mX.push_back(msp(new XYInteger(list->size())));
  else {
    shared_ptr<XYString> s(dynamic_pointer_cast<XYString>(o));
    if (s)
      xy->mX.push_back(msp(new XYInteger(s->mValue.size())));
    else
      xy->mX.push_back(msp(new XYInteger(1)));
  }
}

// Forward declare tokenize function for tokenize primitive
template <class InputIterator, class OutputIterator>
void tokenize(InputIterator first, InputIterator last, OutputIterator out);

// tokenize [X^s Y] [X^{tokens} Y] 
// Given a string, returns a list of cf tokens
static void primitive_tokenize(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYString> s(dynamic_pointer_cast<XYString>(xy->mX.back()));
  xy_assert(s, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> tokens;
  tokenize(s->mValue.begin(), s->mValue.end(), back_inserter(tokens));

  shared_ptr<XYList> result(new XYList());
  for(vector<string>::iterator it=tokens.begin(); it != tokens.end(); ++it)
    result->mList.push_back(msp(new XYString(*it)));

  xy->mX.push_back(result);
}

// parse [X^{tokens} Y] [X^{...} Y] 
// Given a list of tokens, parses it and returns the program
static void primitive_parse(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);

  shared_ptr<XYSequence> tokens(dynamic_pointer_cast<XYSequence>(xy->mX.back()));
  xy_assert(tokens, XYError::TYPE);
  xy->mX.pop_back();

  vector<string> strings;
  for (int i=0; i < tokens->size(); ++i) {
    shared_ptr<XYString> s = dynamic_pointer_cast<XYString>(tokens->at(i));
    xy_assert(s, XYError::TYPE);
    strings.push_back(s->mValue);
  }

  shared_ptr<XYList> result(new XYList());
  parse(strings.begin(), strings.end(), back_inserter(result->mList));
  xy->mX.push_back(result);
}

// getline [X Y] [X^".." Y] 
// Get a line of input from the user
static void primitive_getline(boost::shared_ptr<XY> const& xy) {
  assert(cin.good());

  string line;
  getline(cin, line);
  assert(cin.good());

  xy->mX.push_back(msp(new XYString(line)));
}

// millis [X Y] [X^m Y]
// Runs the number of milliseconds on the stack since
// 1 Janary 1970.
static void primitive_millis(boost::shared_ptr<XY> const& xy) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  ptime e(microsec_clock::universal_time());
  ptime s(date(1970,1,1));

  time_duration d(e - s);

  xy->mX.push_back(msp(new XYInteger(d.total_milliseconds())));
}

// enum [X^n Y] -> [X^{0..n} Y]
static void primitive_enum(boost::shared_ptr<XY> const& xy) {
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYNumber> n = dynamic_pointer_cast<XYNumber>(xy->mX.back());
  xy_assert(n, XYError::TYPE);
  xy->mX.pop_back();

  int value = n->as_uint();
  shared_ptr<XYList> list = msp(new XYList());
  for(int i=0; i < value; ++i)
    list->mList.push_back(msp(new XYInteger(i)));
  xy->mX.push_back(list);
}

// clone [X^o Y] -> [X^o Y]
static void primitive_clone(boost::shared_ptr<XY> const& xy) {
  // TODO: Only works for sequences for now
  // Implemented to work around an XYJoin limitation
  xy_assert(xy->mX.size() >= 1, XYError::STACK_UNDERFLOW);
  shared_ptr<XYSequence> o = dynamic_pointer_cast<XYSequence>(xy->mX.back());
  xy_assert(o, XYError::TYPE);
  xy->mX.pop_back();

  shared_ptr<XYList> r(new XYList());
  o->pushBackInto(r->mList);
  xy->mX.push_back(r);
}

// XYTimeLimit
XYTimeLimit::XYTimeLimit(unsigned int milliseconds) :
  mMilliseconds(milliseconds) {
}

void XYTimeLimit::start(XY* xy) {
  using namespace boost::posix_time;
  
  mStart = (microsec_clock::universal_time() - ptime(min_date_time)).total_milliseconds();
}

bool XYTimeLimit::check(XY* xy) {
  using namespace boost::posix_time;
  unsigned int now = (microsec_clock::universal_time() - ptime(min_date_time)).total_milliseconds();
  return (now - mStart >= mMilliseconds);
}

// XYError
XYError::XYError(shared_ptr<XY> const& xy, code c) :
  mXY(xy),
  mCode(c),
  mLine(-1),
  mFile(0)
{
}

XYError::XYError(shared_ptr<XY> const& xy, code c, char const* file, int line) :
  mXY(xy),
  mCode(c),
  mLine(line),
  mFile(file)
{
}

string XYError::message() {
  ostringstream str;
  switch(mCode) {
  case LIMIT_REACHED:
    str << "Time limit to run code has been exceeded";
    break;

  case STACK_UNDERFLOW:
    str << "Stack underflow";
    break;

  case SYMBOL_NOT_FOUND:
    str << "Symbol not found";
    break;

  case TYPE:
    str << "Type error";
    break;

  default:
    return "Unknown error";
  }

  if (mLine >= 0)
    str << " at line " << mLine;

  if (mFile)
    str << " in file " << mFile;

  str << ".";
  return str.str();
}

// XY
XY::XY() {
  mP["+"]   = msp(new XYPrimitive("+", primitive_addition));
  mP["-"]   = msp(new XYPrimitive("-", primitive_subtraction));
  mP["*"]   = msp(new XYPrimitive("*", primitive_multiplication));
  mP["%"]   = msp(new XYPrimitive("%", primitive_division));
  mP["^"]   = msp(new XYPrimitive("^", primitive_power));
  mP["_"]   = msp(new XYPrimitive("_", primitive_floor));
  mP["set"] = msp(new XYPrimitive("set", primitive_set));
  mP[";"]   = msp(new XYPrimitive(";", primitive_get));
  mP["."]   = msp(new XYPrimitive(".", primitive_unquote));
  mP[")"]   = msp(new XYPrimitive(")", primitive_pattern_ss));
  mP["("]   = msp(new XYPrimitive("(", primitive_pattern_sq));
  mP["`"]   = msp(new XYPrimitive("`", primitive_dip));
  mP["|"]   = msp(new XYPrimitive("|", primitive_reverse));
  mP["'"]   = msp(new XYPrimitive("'", primitive_quote));
  mP[","]   = msp(new XYPrimitive(",", primitive_join));
  mP["$"]   = msp(new XYPrimitive("$", primitive_stack));
  mP["$$"]  = msp(new XYPrimitive("$$", primitive_stackqueue));
  mP["="]   = msp(new XYPrimitive("=", primitive_equals));
  mP["<"]   = msp(new XYPrimitive("<", primitive_lessThan));
  mP["<="]  = msp(new XYPrimitive("<=", primitive_lessThanEqual));
  mP[">"]   = msp(new XYPrimitive(">", primitive_greaterThan));
  mP[">="]  = msp(new XYPrimitive(">=", primitive_greaterThanEqual));
  mP["not"] = msp(new XYPrimitive("not", primitive_not));
  mP["@"]   = msp(new XYPrimitive("@", primitive_nth));
  mP["println"] = msp(new XYPrimitive("print", primitive_println));
  mP["print"] = msp(new XYPrimitive("print", primitive_print));
  mP["write"] = msp(new XYPrimitive("write", primitive_write));
  mP["count"] = msp(new XYPrimitive("count", primitive_count));
  mP["tokenize"] = msp(new XYPrimitive("tokenize", primitive_tokenize));
  mP["parse"] = msp(new XYPrimitive("parse", primitive_parse));
  mP["getline"] = msp(new XYPrimitive("getline", primitive_getline));
  mP["millis"] = msp(new XYPrimitive("millis", primitive_millis));
  mP["enum"]   = msp(new XYPrimitive("+", primitive_enum));
  mP["clone"]   = msp(new XYPrimitive("clone", primitive_clone));
}

void XY::checkLimits() {
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    if ((*it)->check(this)) {
      // This limit was reached, stop executing and throw the error
      throw XYError(shared_from_this(), XYError::LIMIT_REACHED);
    }
  }
}

void XY::print() {
  for_each(mX.begin(), mX.end(), cout << bind(&XYObject::toString, _1, true) << " ");
  cout << " -> ";
  for_each(mY.begin(), mY.end(), cout << bind(&XYObject::toString, _1, true) << " ");
  cout << endl;
}

void XY::eval1() {
  assert(mY.size() > 0);

  shared_ptr<XYObject> o = mY.front();
  assert(o);

  mY.pop_front();
  o->eval1(shared_from_this());
}

void XY::eval() {
  for(XYLimits::iterator it = mLimits.begin(); it != mLimits.end(); ++it) {
    (*it)->start(this);
  }

  while (mY.size() > 0) {
    eval1();
    checkLimits();
  }
}

template <class OutputIterator>
void XY::match(OutputIterator out, 
               shared_ptr<XYObject> object,
               shared_ptr<XYObject> pattern,
               shared_ptr<XYSequence> sequence,
               int i) {
  shared_ptr<XYSequence> object_list = dynamic_pointer_cast<XYSequence>(object);
  shared_ptr<XYSequence> pattern_list = dynamic_pointer_cast<XYSequence>(pattern);
  shared_ptr<XYSymbol> pattern_symbol = dynamic_pointer_cast<XYSymbol>(pattern);
  if (object_list && pattern_list) {
    int pi = 0;
    int oi = 0;
    for(pi=0, oi=0; pi < pattern_list->size() && oi < object_list->size(); ++pi, ++oi) {
      match(out, object_list->at(oi), pattern_list->at(pi), object_list, oi);
    }
    // If there are more pattern items than there are list items,
    // set the pattern value to null.
    while(pi < pattern_list->size()) {
      shared_ptr<XYSymbol> s = dynamic_pointer_cast<XYSymbol>(pattern_list->at(pi));
      if (s) {
        *out++ = make_pair(s->mValue, msp(new XYList()));
      }
      ++pi;
    }
  }
  else if(pattern_list && !object_list) {
    // If the pattern is a list, but the object is not, 
    // pretend the object is a one element list. This enables:
    // 42 [ [[a A]] a A ] -> 42 []
    shared_ptr<XYList> list(new XYList());
    list->mList.push_back(object);
    match(out, list, pattern, sequence, i);
  }
  else if(pattern_symbol) {
    string uppercase = pattern_symbol->mValue;
    to_upper(uppercase);
    if (uppercase == pattern_symbol->mValue) {
      *out++ = make_pair(pattern_symbol->mValue, new XYSlice(sequence, i, sequence->size()));
    }
    else
      *out++ = make_pair(pattern_symbol->mValue, object);
  }
}

template <class OutputIterator>
void XY::getPatternValues(shared_ptr<XYObject> pattern, OutputIterator out) {
  shared_ptr<XYSequence> list = dynamic_pointer_cast<XYSequence>(pattern);
  if (list) {
    assert(mX.size() >= list->size());
    shared_ptr<XYList> stack(new XYList(mX.end() - list->size(), mX.end()));
    match(out, stack, pattern, stack, 0);
    mX.resize(mX.size() - list->size());
  }
  else {
    shared_ptr<XYObject> o = mX.back();
    mX.pop_back();
    shared_ptr<XYList> list(new XYList());
    match(out, o, pattern, list, list->size());
  }
}
 
template <class OutputIterator>
void XY::replacePattern(XYEnv const& env, shared_ptr<XYObject> object, OutputIterator out) {
  shared_ptr<XYSequence> list   = dynamic_pointer_cast<XYSequence>(object);
  shared_ptr<XYSymbol>   symbol = dynamic_pointer_cast<XYSymbol>(object); 
  if (list) {
    // Recurse through the list replacing variables as needed
    shared_ptr<XYList> new_list(new XYList());
    for(int i=0; i < list->size(); ++i)
      replacePattern(env, list->at(i), back_inserter(new_list->mList));
    *out++ = new_list;
  }
  else if (symbol) {
    XYEnv::const_iterator it = env.find(symbol->mValue);
    if (it != env.end())
      *out++ = (*it).second;
    else
      *out++ = object;
  }
  else
    *out++ = object;
}

enum XYState {
  XYSTATE_INIT,
  XYSTATE_STRING_START,
  XYSTATE_STRING_MIDDLE,
  XYSTATE_STRING_END,
  XYSTATE_NUMBER_START,
  XYSTATE_NUMBER_REST,
  XYSTATE_SYMBOL_START,
  XYSTATE_SYMBOL_REST,
  XYSTATE_LIST_START
};

// Returns true if the string is a shuffle pattern
bool is_shuffle_pattern(string s) {
  // A string is a shuffle pattern if it is of the form:
  //   abcd-dbca
  // No letters may be duplicated on the lhs.
  // The rhs must not contain letters that are not in the lhs.
  // The lhs may be empty but the rhs may not be.
  vector<string> result;
  split(result, s, is_any_of("-"));
  if (result.size() != 2)
    return false;

  string before = trim_copy(result[0]);
  string after = trim_copy(result[1]);

  if(before.size() == 0 && after.size() == 0)
    return false;
 
  sort(before.begin(), before.end());
  string::iterator bsend = before.end();
  string::iterator bnend = unique(before.begin(), before.end());
  if(bsend != bnend)
    return false; // Duplicates on the lhs of the pattern are invalid
  sort(after.begin(), after.end());
  string::iterator anend = unique(after.begin(), after.end());
  vector<char> diff;
  set_difference(after.begin(), anend, before.begin(), bnend, back_inserter(diff));
  return diff.size() == 0;
}

// Return regex for tokenizing integers
boost::xpressive::sregex re_integer() {
  using namespace boost::xpressive;
  return optional('-') >> +_d;
}

// Return regex for tokenizing floats
boost::xpressive::sregex re_float() {
  using namespace boost::xpressive;
  return optional('-') >> +_d >> '.' >> *_d;
}

// Return regex for tokenizing numbers
boost::xpressive::sregex re_number() {
  using namespace boost::xpressive;
  return re_float() | re_integer();
}

// Return regex for tokenizing specials
boost::xpressive::sregex re_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return (set= '\\' , '[' , ']' , '{' , '}' , '(' , ')' , ';' , '!' , '.' , ',' , '`' , '\'' , '|', '@');
}

// Return regex for non-specials
boost::xpressive::sregex re_non_special() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set[(set= '\\','[',']','{','}','(',')',';','!','.',',','`','\'','|','@') | _s]);
}

// Return regex for symbols
boost::xpressive::sregex re_symbol() {
  using namespace boost::xpressive;
  return !range('0', '9') >> +(re_non_special());
}

// Return regex for a character in a string
boost::xpressive::sregex re_stringchar() {
  using namespace boost::xpressive;
  using boost::xpressive::set;
  return ~(set= '\"','\\') | ('\\' >> _);
}

// Return regex for strings
boost::xpressive::sregex re_string() {
  using namespace boost::xpressive;
  return as_xpr('\"') >> *(re_stringchar()) >> '\"';
}

// Return regex for comments
boost::xpressive::sregex re_comment() {
  using namespace boost::xpressive;
  return as_xpr('*') >> '*' >> -*_ >> '*' >> '*';
}

// Copyright (C) 2009 Chris Double. All Rights Reserved.
// The original author of this code can be contacted at: chris.double@double.co.nz
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// DEVELOPERS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
