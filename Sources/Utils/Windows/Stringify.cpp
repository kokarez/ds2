//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#define __DebugServer2_Utils_Stringify_h_Private
#include "DebugServer2/Utils/Stringify.h"
#include "DebugServer2/Host/Windows/ExtraWrappers.h"

namespace ds2 {
namespace Utils {

char const *Stringify::DebugEvent(DWORD event) {
  switch (event) {
    DO_STRINGIFY(EXCEPTION_DEBUG_EVENT)
    DO_STRINGIFY(CREATE_THREAD_DEBUG_EVENT)
    DO_STRINGIFY(CREATE_PROCESS_DEBUG_EVENT)
    DO_STRINGIFY(EXIT_THREAD_DEBUG_EVENT)
    DO_STRINGIFY(EXIT_PROCESS_DEBUG_EVENT)
    DO_STRINGIFY(LOAD_DLL_DEBUG_EVENT)
    DO_STRINGIFY(UNLOAD_DLL_DEBUG_EVENT)
    DO_STRINGIFY(OUTPUT_DEBUG_STRING_EVENT)
    DO_STRINGIFY(RIP_EVENT)
    DO_DEFAULT("unknown debug event", event)
  }
}

char const *Stringify::ExceptionCode(DWORD code) {
  switch (code) {
    DO_STRINGIFY(EXCEPTION_ACCESS_VIOLATION)
    DO_STRINGIFY(EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
    DO_STRINGIFY(EXCEPTION_BREAKPOINT)
    DO_STRINGIFY(EXCEPTION_DATATYPE_MISALIGNMENT)
    DO_STRINGIFY(EXCEPTION_FLT_DENORMAL_OPERAND)
    DO_STRINGIFY(EXCEPTION_FLT_DIVIDE_BY_ZERO)
    DO_STRINGIFY(EXCEPTION_FLT_INEXACT_RESULT)
    DO_STRINGIFY(EXCEPTION_FLT_INVALID_OPERATION)
    DO_STRINGIFY(EXCEPTION_FLT_OVERFLOW)
    DO_STRINGIFY(EXCEPTION_FLT_STACK_CHECK)
    DO_STRINGIFY(EXCEPTION_FLT_UNDERFLOW)
    DO_STRINGIFY(EXCEPTION_ILLEGAL_INSTRUCTION)
    DO_STRINGIFY(EXCEPTION_IN_PAGE_ERROR)
    DO_STRINGIFY(EXCEPTION_INT_DIVIDE_BY_ZERO)
    DO_STRINGIFY(EXCEPTION_INT_OVERFLOW)
    DO_STRINGIFY(EXCEPTION_INVALID_DISPOSITION)
    DO_STRINGIFY(EXCEPTION_NONCONTINUABLE_EXCEPTION)
    DO_STRINGIFY(EXCEPTION_PRIV_INSTRUCTION)
    DO_STRINGIFY(EXCEPTION_SINGLE_STEP)
    DO_STRINGIFY(EXCEPTION_STACK_OVERFLOW)
  case DS2_EXCEPTION_UNCAUGHT_COM:
    return "0x800706BA (uncaught COM exception)";
  case DS2_EXCEPTION_UNCAUGHT_USER:
    return "0xE06D7363 (uncaught user exception)";
    DO_DEFAULT("unknown exception code", code)
  }
}
}
}
