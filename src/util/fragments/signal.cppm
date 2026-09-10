module;
#include <csignal>
export module Util:Signal;

export namespace Util {
enum Signal {
    SigAbrt = SIGABRT,
    SigFpe  = SIGFPE,
    SigIll  = SIGILL,
    SigInt  = SIGINT,
    SigSegv = SIGSEGV,
    SigTerm = SIGTERM,
};
} // namespace Util
