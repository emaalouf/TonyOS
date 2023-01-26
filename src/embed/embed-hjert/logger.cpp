#include <embed/logger.h>
#include <hjert-core/arch.h>
#include <karm-sys/chan.h>

namespace Embed {

void loggerLock() {}

void loggerUnlock() {}

Io::TextWriter<> &loggerOut() {
    return Hjert::Arch::loggerOut();
}

} // namespace Embed
