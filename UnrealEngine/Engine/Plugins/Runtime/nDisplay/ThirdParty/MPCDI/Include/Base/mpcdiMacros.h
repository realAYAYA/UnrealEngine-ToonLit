/* =========================================================================

  Program:   MPCDI Library
  Language:  C++
  Date:      $Date: 2012-02-08 11:39:41 -0500 (Wed, 08 Feb 2012) $
  Version:   $Revision: 18341 $

  Copyright (c) 2013 Scalable Display Technologies, Inc.
  All Rights Reserved.
  The MPCDI Library is distributed under the BSD license.
  Please see License.txt distributed with this package.

===================================================================auto== */

#pragma once

#ifndef __mpcdiMacros_H_
#define __mpcdiMacros_H_

#include "mpcdiErrorHelper.h"
#include <assert.h>

namespace mpcdi 
{
  #define MPCDI_FAIL_RET(func_or_error) \
    {\
      mpcdi::MPCDI_Error error = func_or_error; \
      if ((error) != mpcdi::MPCDI_SUCCESS) \
        return error; \
    }

  // This macros is used to make the Standard Set Macro
  #define mpcdiSetMacro(_name_, _Type_) \
    inline MPCDI_Error Set##_name_ (const _Type_ & _arg) { this->m_##_name_ = _arg; return MPCDI_SUCCESS; }
  #define mpcdiSetMacroRange(_Name_, _Type_, _Min_, _Max_) \
    static MPCDI_Error CheckRange##_Name_(const _Type_ & _Arg)\
    { \
      if((_Arg<_Min_) || (_Arg > _Max_)) \
      {\
        CREATE_ERROR_MSG(msg, "Value for Set" << #_Name_ << " needs to be in [" << _Min_ << "," << _Max_ << "]" << " input: " << _Arg << " is out of range" );\
        ReturnCustomErrorMacro(MPCDI_VALUE_OUT_OF_RANGE, msg);\
      }\
      return MPCDI_SUCCESS; \
    } \
    inline MPCDI_Error Set##_Name_ (const _Type_ & _Arg) \
    { \
      MPCDI_FAIL_RET(CheckRange##_Name_(_Arg)); \
      this->m_##_Name_ = _Arg; \
      return MPCDI_SUCCESS; \
    }

  #define mpcdiGetMacro(_name_,_type_) \
    inline _type_ Get##_name_ () { return this->m_##_name_; }

  // This macros is used to make the Standard Get Macro
  #define mpcdiGetConstMacro(_name_,_type_) \
    inline const _type_ Get##_name_ () const { return this->m_##_name_; }

  #define mpcdiSet3Macro(_funcname_,_xname_,_yname_,_zname_,Type_) \
    inline MPCDI_Error Set##_funcname_(const Type_ & _argx, const Type_ & _argy, const Type_ & _argz ) \
      { MPCDI_FAIL_RET(this->Set##_xname_(_argx)); MPCDI_FAIL_RET(this->Set##_yname_(_argy)); MPCDI_FAIL_RET(this->Set##_zname_(_argz)); return MPCDI_SUCCESS;  }

  #define mpcdiSet2Macro(_funcname_,_xname_,_yname_,Type_) \
    inline MPCDI_Error Set##_funcname_(const Type_ & _argx, const Type_ & _argy) \
      { MPCDI_FAIL_RET(this->Set##_xname_(_argx)); MPCDI_FAIL_RET(this->Set##_yname_(_argy)); return MPCDI_SUCCESS;}

  // This macros is used to make the Standard Set Macro
  #define  mpcdiSetRefMacro(_name_, _type_) \
    inline _type_ &Set##_name_ () {  return this->m_##_name_; }

  // This macros is used to make the Standard Get Macro
  #define  mpcdiGetRefMacro(_name_, _type_) \
    inline _type_ &GetRef##_name_ () {  return (this->m_##_name_); } \
    inline const _type_ &GetRef##_name_ () const {  return (this->m_##_name_); }

  // Warnings and errors do nothing for now.
  #define mpcdiWarningMacro(x) 
  #define mpcdiErrorMacro(x) 

  // Set of macros to create enums
  #define MPCDI_ENUM_VALUE(name,prefix,assign) prefix##name assign,

  #define MPCDI_ENUM_CASE(name,prefix,assign) case prefix##name: return #name;

  #define MPCDI_ENUM_STRCMP(name,prefix,assign) if (value==std::string(#name)) return prefix##name;

  #define MPCDI_DECLARE_ENUM(EnumType, ENUM_DEF) \
    enum EnumType { \
      ENUM_DEF(MPCDI_ENUM_VALUE, EnumType) \
    }; 

  #define MPCDI_DECLARE_ENUM_TYPEDEF(EnumType, ENUM_DEF) \
    typedef enum  { \
      ENUM_DEF(MPCDI_ENUM_VALUE, EnumType) \
    } EnumType ; 

  #define MPCDI_DECLARE_ENUM_CONV_FUNC(_EnumType_) \
    const std::string Get##_EnumType_(_EnumType_ dummy); \
    _EnumType_ Get##_EnumType_(const std::string string); 

  #define MPCDI_DEFINE_ENUM_CONV_FUNC(_EnumType_,_NAMESPACE_,ENUM_DEF) \
    const std::string _NAMESPACE_::Get##_EnumType_(_EnumType_ value) \
    { \
      switch(value) \
      { \
        ENUM_DEF(MPCDI_ENUM_CASE, _EnumType_) \
        default: return ""; /* handle input error */ \
      } \
    } \
    _EnumType_ _NAMESPACE_::Get##_EnumType_(const std::string value) \
    { \
      ENUM_DEF(MPCDI_ENUM_STRCMP, _EnumType_) \
      return (_EnumType_)0; /* handle input error */ \
    } \

// release an object if not null
#define mpcdiSafeDeleteMacro(object) {                                \
  assert(object-0 == object);   /* Prevents Smart Pointers here */    \
  CompilerAssert(sizeof(object) == sizeof(void*));                    \
  if(object != NULL) delete object; }

// Compiler Asserts
#define CompilerAssert(predicate) _impcdi_CASSERT_LINE(predicate,__LINE__,)

#define _impcdi_PASTE(a,b) a##b
#define _impcdi_CASSERT_LINE(predicate, line, file) \
typedef char _impcdi_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];


} // end namespace mpcdi


#endif // __mpcdiMacros_H_
