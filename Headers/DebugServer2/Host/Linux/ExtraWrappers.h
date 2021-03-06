//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Host_Linux_ExtraWrappers_h
#define __DebugServer2_Host_Linux_ExtraWrappers_h

#include "DebugServer2/Base.h"
#include "DebugServer2/Utils/CompilerSupport.h"

#include <asm/unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <unistd.h>

// Required structs for PTrace GETREGSET with NT_X86_STATE
// These structs are not made available by the system headers
struct YMMHighVector {
  uint8_t value[16];
};

struct xstate_hdr {
  uint64_t mask;
  uint64_t reserved1[2];
  uint64_t reserved2[5];
} DS2_ATTRIBUTE_PACKED;

#if defined(ARCH_X86)
struct xfpregs_struct {
  user_fpxregs_struct fpregs;
  xstate_hdr header;
  YMMHighVector ymmh[8];
} DS2_ATTRIBUTE_PACKED DS2_ATTRIBUTE_ALIGNED(64);
#elif defined(ARCH_X86_64)
struct xfpregs_struct {
  user_fpregs_struct fpregs;
  xstate_hdr header;
  YMMHighVector ymmh[16];
} DS2_ATTRIBUTE_PACKED DS2_ATTRIBUTE_ALIGNED(64);
#endif

#if !defined(PLATFORM_ANDROID)
// Android headers do have a wrapper for `gettid`, unlike glibc.
static inline pid_t gettid() { return ::syscall(SYS_gettid); }
#endif

static inline int tgkill(pid_t pid, pid_t tid, int signo) {
  return ::syscall(SYS_tgkill, pid, tid, signo);
}

static inline int tkill(pid_t tid, int signo) {
  return ::syscall(SYS_tkill, tid, signo);
}

#endif // !__DebugServer2_Host_Linux_ExtraWrappers_h
