//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Target_Darwin_MachOProcess_h
#define __DebugServer2_Target_Darwin_MachOProcess_h

#include "DebugServer2/Host/Darwin/Mach.h"
#include "DebugServer2/Target/POSIX/Process.h"

namespace ds2 {
namespace Target {
namespace Darwin {

class MachOProcess : public ds2::Target::POSIX::Process {
protected:
  std::string _auxiliaryVector;
  Address _sharedLibraryInfoAddress;
  Host::Darwin::Mach _mach;

public:
  ErrorCode getAuxiliaryVector(std::string &auxv) override;
  uint64_t getAuxiliaryVectorValue(uint64_t type) override;

public:
  virtual ErrorCode getSharedLibraryInfoAddress(Address &address);
  virtual ErrorCode enumerateSharedLibraries(
      std::function<void(SharedLibraryInfo const &)> const &cb);

public:
  Host::Darwin::Mach &mach();

protected:
  ErrorCode updateInfo() override;
  virtual ErrorCode updateAuxiliaryVector();
};
}
}
}

#endif // !__DebugServer2_Target_POSIX_Process_h
