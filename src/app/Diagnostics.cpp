// SPDX-License-Identifier: MIT
#include "app/Diagnostics.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <typeinfo>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <dbghelp.h>
#  include <crtdbg.h>
#  include <new.h>
#endif

namespace st::diagnostics {
namespace {

QFile* gLogFile = nullptr;
QMutex gLogMutex;

// A second, plain-C handle on the same file, opened in append mode and used
// only by the crash path. The Qt handler formats with QString and QDateTime,
// both of which allocate; if the process is dying because the heap is corrupt
// or the stack is exhausted, allocating is exactly what must not happen. This
// handle writes from a stack buffer and nothing else.
//
// Once the crash path has written, the QFile's idea of its own position is
// stale — but by then the process is seconds from gone and nothing else logs.
std::FILE* gCrashFile = nullptr;

/// Append one line to the log without allocating. Safe from an exception
/// filter, a signal handler, and a stack overflow.
void crashLog(const char* format, ...)
{
    char    buffer[1024];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (written < 0)
        return;

    if (gCrashFile != nullptr) {
        std::fputs(buffer, gCrashFile);
        std::fputc('\n', gCrashFile);
        std::fflush(gCrashFile);
    }
    std::fputs(buffer, stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

[[nodiscard]] const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "debug";
    case QtInfoMsg:     return "info ";
    case QtWarningMsg:  return "warn ";
    case QtCriticalMsg: return "crit ";
    case QtFatalMsg:    return "FATAL";
    default:            return "?????";
    }
}

void handler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    QString       line  = QStringLiteral("%1 %2 %3").arg(stamp, QString::fromLatin1(levelName(type)), message);

    // Qt only fills in file/line for debug builds; include it when present
    // because it is exactly what pins down a warning's origin.
    if (context.file != nullptr && type != QtInfoMsg)
        line += QStringLiteral("   [%1:%2]").arg(QString::fromUtf8(context.file)).arg(context.line);

    {
        QMutexLocker locker(&gLogMutex);
        if (gLogFile != nullptr && gLogFile->isOpen()) {
            gLogFile->write(line.toUtf8());
            gLogFile->write("\n");
            // Flush every line. A crash must not take the last one with it —
            // that line is usually the whole answer.
            gLogFile->flush();
        }
    }

    // Deliberately not chaining to the previous handler: it would print every
    // line a second time. Qt aborts on QtFatalMsg by itself once this returns,
    // so nothing is lost by ending the chain here.
    std::fputs(line.toUtf8().constData(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

/// Last words for an uncaught C++ exception. Without this the process simply
/// disappears and the host reports "terminated abnormally", which says nothing.
void onTerminate()
{
    if (auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& e) {
            crashLog("TERMINATE: uncaught %s: %s", typeid(e).name(), e.what());
        } catch (...) {
            crashLog("TERMINATE: uncaught non-standard exception");
        }
    } else {
        crashLog("TERMINATE: called with no active exception");
    }
    std::abort();
}

/// abort() — from Q_ASSERT, qFatal(), a failed CRT check, or our own
/// onTerminate(). Without a handler this is one of the several ways the
/// process can vanish leaving nothing in the log.
void onAbortSignal(int)
{
    crashLog("ABORT: SIGABRT raised");
#if defined(_WIN32)
    // Nothing after this is reliable, but the backtrace usually still is.
    void* frames[48];
    const USHORT count = CaptureStackBackTrace(0, 48, frames, nullptr);
    for (USHORT i = 0; i < count; ++i)
        crashLog("  #%02u  %p", static_cast<unsigned>(i), frames[i]);
#endif
    std::_Exit(3);
}

#if defined(_WIN32)

/// Resolve one return address to "module+rva  symbol  file:line".
///
/// The module and RVA come from GetModuleHandleEx, which touches no heap and
/// always works; on its own that is enough to symbolise the frame afterwards
/// with `dumpbin`/WinDbg. dbghelp is then asked for a name as a bonus, because
/// a Debug build has the .pdb sitting right next to the .exe and a named frame
/// ends the guessing on the spot.
void describeFrame(unsigned index, void* address)
{
    char        moduleName[MAX_PATH] = "?";
    std::size_t rva                  = 0;

    HMODULE module = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCSTR>(address), &module)
        && module != nullptr) {
        char full[MAX_PATH] = "";
        if (GetModuleFileNameA(module, full, MAX_PATH) > 0) {
            const char* base = std::strrchr(full, '\\');
            std::snprintf(moduleName, sizeof moduleName, "%s", base != nullptr ? base + 1 : full);
        }
        rva = static_cast<std::size_t>(static_cast<const char*>(address)
                                       - reinterpret_cast<const char*>(module));
    }

    // SYMBOL_INFO is variable-length; give it room for the name inline so this
    // stays on the stack.
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 512] = {};
    auto* symbol        = reinterpret_cast<SYMBOL_INFO*>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen   = 511;

    DWORD64 displacement = 0;
    const bool named =
        SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(address), &displacement, symbol)
        != FALSE;

    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct  = sizeof(IMAGEHLP_LINE64);
    DWORD lineOffset   = 0;
    const bool located = SymGetLineFromAddr64(GetCurrentProcess(),
                                              reinterpret_cast<DWORD64>(address), &lineOffset,
                                              &line)
                         != FALSE;

    if (named && located) {
        crashLog("  #%02u  %s+0x%llX  %s+0x%llX  (%s:%lu)", index, moduleName,
                 static_cast<unsigned long long>(rva), symbol->Name,
                 static_cast<unsigned long long>(displacement), line.FileName, line.LineNumber);
    } else if (named) {
        crashLog("  #%02u  %s+0x%llX  %s+0x%llX", index, moduleName,
                 static_cast<unsigned long long>(rva), symbol->Name,
                 static_cast<unsigned long long>(displacement));
    } else {
        crashLog("  #%02u  %s+0x%llX  %p", index, moduleName,
                 static_cast<unsigned long long>(rva), address);
    }
}

void writeBacktrace()
{
    void*        frames[62];
    const USHORT count = CaptureStackBackTrace(0, 62, frames, nullptr);
    crashLog("CRASH: backtrace (%u frames, innermost first):", static_cast<unsigned>(count));
    for (USHORT i = 0; i < count; ++i)
        describeFrame(i, frames[i]);
}

/// Last words for an access violation and friends. A C++ terminate handler
/// never sees these — they are structured exceptions, and they are the most
/// likely way a program like this dies on Windows.
LONG WINAPI onStructuredException(EXCEPTION_POINTERS* info)
{
    // First line, no formatting of anything that could itself fault: if the
    // log ends here we still learn that the filter ran at all.
    crashLog("CRASH: unhandled structured exception");

    const DWORD code    = info->ExceptionRecord->ExceptionCode;
    const void* address = info->ExceptionRecord->ExceptionAddress;
    const char* name    = "structured exception";
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      name = "ACCESS_VIOLATION"; break;
    case EXCEPTION_STACK_OVERFLOW:        name = "STACK_OVERFLOW"; break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:   name = "ILLEGAL_INSTRUCTION"; break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    name = "INT_DIVIDE_BY_ZERO"; break;
    case EXCEPTION_PRIV_INSTRUCTION:      name = "PRIV_INSTRUCTION"; break;
    case EXCEPTION_IN_PAGE_ERROR:         name = "IN_PAGE_ERROR"; break;
    case 0xE06D7363:                      name = "C++ exception (MSVC)"; break;
    case 0xC0000409:                      name = "FAIL_FAST (stack/heap check)"; break;
    default: break;
    }

    crashLog("CRASH: %s (0x%08lX) at %p", name, static_cast<unsigned long>(code), address);
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR operation = info->ExceptionRecord->ExceptionInformation[0];
        const ULONG_PTR at        = info->ExceptionRecord->ExceptionInformation[1];
        crashLog("CRASH: %s address 0x%llX",
                 operation == 0 ? "read from" : (operation == 1 ? "write to" : "execute at"),
                 static_cast<unsigned long long>(at));
    }

    writeBacktrace();
    return EXCEPTION_EXECUTE_HANDLER;
}

/// The CRT calls this instead of returning an error when it is handed a bad
/// argument — and in a release CRT its default is to fail fast, which bypasses
/// the unhandled-exception filter entirely. That is one of the ways a process
/// dies leaving an empty log.
void onInvalidParameter(const wchar_t* expression, const wchar_t* function, const wchar_t* file,
                        unsigned int line, uintptr_t)
{
    crashLog("CRASH: CRT invalid parameter in %ls (%ls:%u): %ls",
             function != nullptr ? function : L"?", file != nullptr ? file : L"?", line,
             expression != nullptr ? expression : L"?");
    writeBacktrace();
    std::_Exit(4);
}

void onPureCall()
{
    crashLog("CRASH: pure virtual function call");
    writeBacktrace();
    std::_Exit(5);
}

#endif  // _WIN32

void onExit()
{
    crashLog("-- exiting cleanly");
}

} // namespace

void install()
{
    if (gLogFile != nullptr)
        return;

    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    QDir().mkpath(dir);

    const QString path = QDir(dir).filePath(QStringLiteral("schnellerTyp-e.log"));

    auto* file = new QFile(path);
    if (file->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        gLogFile = file;
    } else {
        delete file;
    }

    // Opened after the truncate above so it appends to the same file rather
    // than racing it for the first byte.
#if defined(_MSC_VER)
    if (fopen_s(&gCrashFile, path.toLocal8Bit().constData(), "a") != 0)
        gCrashFile = nullptr;
#else
    gCrashFile = std::fopen(path.toLocal8Bit().constData(), "a");
#endif

    qInstallMessageHandler(handler);
    std::set_terminate(onTerminate);
    std::signal(SIGABRT, onAbortSignal);
    std::atexit(onExit);

#if defined(_WIN32)
    // Recovering from a stack overflow needs stack, and by definition there is
    // none left. This reserves a slice below the guard page so the exception
    // filter — and therefore the backtrace that names the recursion — can still
    // run. Without it a stack overflow is indistinguishable from silence.
    // ULONG, not ULONG_PTR: the parameter is PULONG on both 32- and 64-bit.
    ULONG guarantee = 128 * 1024;
    SetThreadStackGuarantee(&guarantee);

    // Resolve symbols from the .pdb next to the .exe. Deferred loading keeps
    // startup cheap; the frames are only named when something actually breaks.
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);

    SetUnhandledExceptionFilter(onStructuredException);
    _set_invalid_parameter_handler(onInvalidParameter);
    _set_purecall_handler(onPureCall);
    // Do not pop the "this application has requested the Runtime to terminate"
    // dialog; the log is the channel we actually read.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#  if defined(_DEBUG)
    for (const int report : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
        _CrtSetReportMode(report, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(report, _CRTDBG_FILE_STDERR);
    }
#  endif
#endif

    qInfo("schnellerTyp-e %s starting", SCHNELLERTYPE_VERSION);
    if (gLogFile != nullptr)
        qInfo("log: %s", qUtf8Printable(gLogFile->fileName()));
}

QString logFilePath()
{
    QMutexLocker locker(&gLogMutex);
    return gLogFile != nullptr ? gLogFile->fileName() : QString();
}

void milestone(const QString& what)
{
    qInfo("-- %s", qUtf8Printable(what));
}

} // namespace st::diagnostics
