//
// Copyright (c) 2014, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#include "DebugServer2/BreakpointManager.h"
#include "DebugServer2/GDBRemote/DebugSessionImpl.h"
#include "DebugServer2/GDBRemote/PlatformSessionImpl.h"
#include "DebugServer2/GDBRemote/ProtocolHelpers.h"
#include "DebugServer2/GDBRemote/SlaveSessionImpl.h"
#include "DebugServer2/Host/Platform.h"
#include "DebugServer2/Host/QueueChannel.h"
#include "DebugServer2/Host/Socket.h"
#include "DebugServer2/SessionThread.h"
#include "DebugServer2/Utils/Log.h"
#include "DebugServer2/Utils/OptParse.h"
#include "DebugServer2/Utils/String.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <set>
#include <string>
#if !defined(OS_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

using ds2::Host::Socket;
using ds2::Host::QueueChannel;
using ds2::Host::Platform;
using ds2::BreakpointManager;
using ds2::GDBRemote::Session;
using ds2::GDBRemote::SessionDelegate;
using ds2::GDBRemote::PlatformSessionImpl;
using ds2::GDBRemote::DebugSessionImpl;
using ds2::GDBRemote::SlaveSessionImpl;

static std::string gDefaultPort = "12345";
static std::string gDefaultHost = "localhost";
static bool gKeepAlive = false;
static bool gGDBCompat = false;

#if !defined(OS_WIN32)
static int PlatformMain(char const *port, char const *host) {
  auto server = new Socket;

  if (!server->create()) {
    DS2LOG(Fatal, "cannot create server socket on port %s: %s", port,
           server->error().c_str());
  }

  if (!server->listen(port)) {
    DS2LOG(Fatal, "error: failed to listen: %s", server->error().c_str());
  }

  DS2LOG(Info, "listening on port %s", port);

  PlatformSessionImpl impl;

  do {
    // Platform mode implies that we are talking to an LLDB remote.
    Session session(ds2::GDBRemote::kCompatibilityModeLLDB);
    session.setDelegate(&impl);
    session.create(server->accept());

    while (session.receive(/*cooked=*/false))
      continue;
  } while (gKeepAlive);

  return EXIT_SUCCESS;
}
#endif

static int RunDebugServer(Socket *server, SessionDelegate *impl,
                          char const *port, char const *host, bool reverse) {
  if (reverse && !server->connect(host, port)) {
    DS2LOG(Fatal, "reverse connect failed: %s", server->error().c_str());
  }

  Session session(gGDBCompat ? ds2::GDBRemote::kCompatibilityModeGDB
                             : ds2::GDBRemote::kCompatibilityModeLLDB);
  auto qchannel = new QueueChannel(reverse ? server : server->accept());
  SessionThread thread(qchannel, &session);

  session.setDelegate(impl);
  session.create(qchannel);

  DS2LOG(Debug, "DEBUG SERVER STARTED");
  thread.start();
  while (session.receive(/*cooked=*/true))
    continue;
  DS2LOG(Debug, "DEBUG SERVER KILLED");

  return EXIT_SUCCESS;
}

static int DebugMain(ds2::StringCollection const &args,
                     ds2::EnvironmentBlock const &env, int attachPid,
                     char const *port, char const *host, bool reverse,
                     std::string const &namedPipePath) {
  auto server = new Socket;

  if (!server->create()) {
    DS2LOG(Fatal, "cannot create server socket on port %s: %s", port,
           server->error().c_str());
  }

  if (!reverse && !server->listen(port)) {
    DS2LOG(Fatal, "failed to listen: %s", server->error().c_str());
  }

  if (!reverse)
    DS2LOG(Info, "listening on port %d", server->port());

  if (!namedPipePath.empty()) {
    std::string portStr = ds2::ToString(server->port());
    FILE *namedPipe = fopen(namedPipePath.c_str(), "a");
    if (namedPipe == nullptr) {
      DS2LOG(Error, "unable to open %s: %s", namedPipePath.c_str(),
             strerror(errno));
    } else {
      // Write the null terminator to the file. This follows the llgs behavior.
      fwrite(portStr.c_str(), 1, portStr.length() + 1, namedPipe);
      fclose(namedPipe);
    }
  }

  do {
    DebugSessionImpl *impl;

    if (attachPid > 0)
      impl = new DebugSessionImpl(attachPid);
    else if (args.size() > 0)
      impl = new DebugSessionImpl(args, env);
    else
      impl = new DebugSessionImpl();

    return RunDebugServer(server, impl, port, host, reverse);

    delete impl;
  } while (gKeepAlive);

  return EXIT_SUCCESS;
}

#if !defined(OS_WIN32)
static int SlaveMain() {
  auto server = new Socket;

  if (!server->create())
    return EXIT_FAILURE;

  if (!server->listen(0))
    return EXIT_FAILURE;

  uint16_t port = server->port();

  pid_t pid = ::fork();
  if (pid < 0)
    return EXIT_FAILURE;

  if (pid == 0) {
    //
    // Let the slave have its own session
    // TODO maybe should be a command line switch?
    //
    setsid();

    //
    // When in slave mode, output is suppressed but
    // for standard error.
    //
    close(0);
    close(1);

    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);

    return RunDebugServer(server, new SlaveSessionImpl, nullptr, nullptr,
                          false);
  } else {
    //
    // Write to the standard output to let know our parent
    // where we're listening.
    //
    fprintf(stdout, "%u %d\n", port, pid);

    DS2LOG(Info, "listening on port %u pid %d", port, pid);
  }

  return EXIT_SUCCESS;
}
#endif

DS2_ATTRIBUTE_NORETURN static void ListProcesses() {
  printf("Processes running on %s:\n\n", Platform::GetHostName());
  printf("%s\n%s\n", "PID    USER       ARCH    NAME",
         "====== ========== ======= ============================");

  Platform::EnumerateProcesses(
      true, ds2::UserId(), [&](ds2::ProcessInfo const &info) {
        std::string pid = ds2::ToString(info.pid);

        std::string user;
        if (!Platform::GetUserName(info.realUid, user)) {
#if !defined(OS_WIN32)
          user = ds2::ToString(info.realUid);
#else
          user = "<NONE>";
#endif
        }

        std::string path = info.name;
        size_t lastsep;
#if defined(OS_WIN32)
        lastsep = path.rfind('\\');
#else
        lastsep = path.rfind('/');
#endif
        if (lastsep != std::string::npos) {
          path = path.substr(lastsep + 1);
        }

        printf("%-6s %-10.10s %-7.7s %s\n", pid.c_str(), user.c_str(),
               ds2::GetArchName(info.cpuType, info.cpuSubType), path.c_str());
      });

  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  ds2::OptParse opts;
  int idx;

  ds2::Host::Platform::Initialize();
#if !defined(OS_WIN32)
  ds2::SetLogColorsEnabled(isatty(fileno(stderr)));
#endif
  ds2::SetLogLevel(ds2::kLogLevelWarning);

  enum RunMode {
    kRunModeNormal,
#if !defined(OS_WIN32)
    kRunModePlatform,
    kRunModeSlave,
#endif
  };

  int attachPid = -1;
  std::string port;
  std::string host;
  std::string namedPipePath;
  RunMode mode = kRunModeNormal;
  bool reverse = false;

  if (argc < 2)
    opts.usageDie("first argument must be g[dbserver] or p[latform]");

  switch (argv[1][0]) {
  case 'g':
    mode = kRunModeNormal;
    break;
  case 'p':
    mode = kRunModePlatform;
    break;
  case 's':
    mode = kRunModeSlave;
    break;
  default:
    opts.usageDie("first argument must be g[dbserver] or p[latform]");
  }

  opts.addOption(ds2::OptParse::stringOption, "attach", 'a',
                 "attach to the name or PID specified");
  opts.addOption(ds2::OptParse::boolOption, "keep-alive", 'k',
                 "keep the server alive after the client disconnects");
  opts.addOption(ds2::OptParse::boolOption, "reverse-connect", 'R',
                 "connect back to the debugger at [HOST]:PORT");

  // Target debug options.
  opts.addOption(ds2::OptParse::vectorOption, "set-env", 'e',
                 "add an element to the environment before launch");
  opts.addOption(ds2::OptParse::vectorOption, "unset-env", 'E',
                 "remove an element from the environment before lauch");

  // Logging options.
  opts.addOption(ds2::OptParse::stringOption, "log-output", 'o',
                 "output log messages to the file specified");
  opts.addOption(ds2::OptParse::boolOption, "debug-remote", 'D',
                 "enable log for remote protocol packets");
  opts.addOption(ds2::OptParse::boolOption, "debug", 'd',
                 "enable debug log output");
  opts.addOption(ds2::OptParse::boolOption, "no-colors", 'n',
                 "disable colored output");

  // Non-debugserver options.
  opts.addOption(ds2::OptParse::boolOption, "list-processes", 'L',
                 "list processes debuggable by the current user");

  // llgs-compat options.
  opts.addOption(ds2::OptParse::boolOption, "gdb-compat", 'g',
                 "force ds2 to run in gdb compat mode");
  opts.addOption(ds2::OptParse::stringOption, "named-pipe", 'N',
                 "determine a port dynamically and write back to FIFO");
  opts.addOption(ds2::OptParse::boolOption, "native-regs", 'r',
                 "use native registers (no-op)", true);
  opts.addOption(ds2::OptParse::boolOption, "setsid", 'S',
                 "make ds2 run in its own session (no-op)", true);

  idx = opts.parse(argc, argv, host, port);

  if (!opts.getString("log-output").empty()) {
    FILE *stream = fopen(opts.getString("log-output").c_str(), "a");
    if (stream == nullptr) {
      DS2LOG(Error, "unable to open %s for writing: %s",
             opts.getString("log-output").c_str(), strerror(errno));
    } else {
#if !defined(OS_WIN32)
      //
      // Note(sas): When ds2 is spawned by the app, it will run with the
      // app's user/group ID, and will create its log file owned by the
      // app. By default, the permissions will be 0600 (rw-------) which
      // makes us unable to get the log files. chmod() them to be able to
      // access them.
      //
      fchmod(fileno(stream), 0644);
#endif
      ds2::SetLogColorsEnabled(false);
      ds2::SetLogOutputStream(stream);
      ds2::SetLogLevel(ds2::kLogLevelDebug);
    }
  }

  if (opts.getBool("debug-remote")) {
    ds2::SetLogLevel(ds2::kLogLevelPacket);
  } else if (opts.getBool("debug")) {
    ds2::SetLogLevel(ds2::kLogLevelDebug);
  }

  if (opts.getBool("no-colors")) {
    ds2::SetLogColorsEnabled(false);
  }

  gKeepAlive = opts.getBool("keep-alive");

  if (!opts.getString("attach").empty()) {
    attachPid = atoi(opts.getString("attach").c_str());
  }

  reverse = opts.getBool("reverse-connect");
  if (mode != kRunModeNormal && reverse) {
    opts.usageDie("reverse connection only supported in gdbserver mode");
  }

  if (opts.getBool("list-processes")) {
    ListProcesses();
  }

  if (mode == kRunModePlatform) {
    // The platform spawner should stay alive by default.
    gKeepAlive = true;
  }

  // This option forces ds2 to operate in lldb compatibilty mode. When not
  // specified, we assume we are talking to a GDB remote until we detect
  // otherwise.
  gGDBCompat = opts.getBool("gdb-compat");

  // This is used for llgs testing. We determine a port number dynamically and
  // write it back to the FIFO passed as argument for the test harness to use
  // it.
  namedPipePath = opts.getString("named-pipe");

  argc -= idx, argv += idx;

  ds2::StringCollection args(&argv[0], &argv[argc]);
  ds2::EnvironmentBlock env;

  Platform::GetCurrentEnvironment(env);

  for (auto const &e : opts.getVector("set-env")) {
    char const *arg = e.c_str();
    char const *equal = strchr(arg, '=');
    if (equal == nullptr || equal == arg)
      DS2LOG(Fatal, "invalid environment value: %s", arg);
    env[std::string(arg, equal)] = equal + 1;
  }

  for (auto const &e : opts.getVector("unset-env")) {
    env.erase(e);
  }

  // We need a process to attach to or a command line so we can launch it,
  // unless we are in lldb compat mode, which implies we can launch the debug
  // server without any of those two things, and wait for an "A" command that
  // specifies the command line to use to launch the inferior.
  if (mode == kRunModeNormal && gGDBCompat && args.empty() && attachPid < 0) {
    opts.usageDie("either a program or target PID is required");
  }

  if (mode != kRunModeNormal && argc != 0) {
    opts.usageDie("PROGRAM and ARGUMENTS only supported in gdbserver mode");
  }

  if (port.empty()) {
    // If we have a named pipe, set the port to 0 to indicate that we should
    // dynamically allocate it and write it back to the FIFO.
    port = namedPipePath.empty() ? gDefaultPort : "0";
  }
  if (host.empty()) {
    host = gDefaultHost;
  }

  switch (mode) {
  case kRunModeNormal:
    return DebugMain(args, env, attachPid, port.c_str(), host.c_str(), reverse,
                     namedPipePath);
#if !defined(OS_WIN32)
  case kRunModePlatform:
    return PlatformMain(port.c_str(), host.c_str());
  case kRunModeSlave:
    return SlaveMain();
#endif
  default:
    DS2BUG("invalid run mode: %d", mode);
  }
}
