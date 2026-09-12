#include "CrashHandler.h"
#include "Paths.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <csignal>

#if defined(__ANDROID__)
#include <unwind.h>
#include <dlfcn.h>
#include <unistd.h>
#elif !defined(_WIN32)
#include <execinfo.h>
#include <unistd.h>
#endif

namespace Freeking
{
	namespace
	{
		// Async-signal-safe context: plain char buffer, never heap-touched
		// from the handler.
		char g_context[256] = { 0 };
		std::atomic<bool> g_installed(false);
		std::atomic<bool> g_crashing(false);

		const char* SignalName(int sig)
		{
			switch (sig)
			{
			case SIGSEGV: return "SIGSEGV";
			case SIGABRT: return "SIGABRT";
			case SIGILL: return "SIGILL";
			case SIGFPE: return "SIGFPE";
#ifdef SIGBUS
			case SIGBUS: return "SIGBUS";
#endif
			default: return "UNKNOWN";
			}
		}

#if defined(__ANDROID__)
		struct BacktraceState
		{
			FILE* file;
			int depth;
		};

		_Unwind_Reason_Code UnwindCallback(_Unwind_Context* context, void* arg)
		{
			auto* state = static_cast<BacktraceState*>(arg);
			if (state->depth >= 32)
			{
				return _URC_END_OF_STACK;
			}

			uintptr_t pc = _Unwind_GetIP(context);
			if (pc != 0)
			{
				Dl_info info;
				const char* sym = "?";
				uintptr_t offset = pc;
				if (dladdr(reinterpret_cast<void*>(pc), &info) != 0)
				{
					if (info.dli_sname != nullptr)
					{
						sym = info.dli_sname;
					}
					offset = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
				}

				std::fprintf(state->file, "#%02d pc %08x %s\n",
					state->depth, static_cast<unsigned>(offset), sym);
				state->depth++;
			}

			return _URC_NO_REASON;
		}
#endif

		void WriteCrashLog(FILE* file, int sig)
		{
			std::fprintf(file, "FATAL SIGNAL: %s (%d)\n", SignalName(sig), sig);
			if (g_context[0] != '\0')
			{
				std::fprintf(file, "Context: %s\n", g_context);
			}

#if defined(__ANDROID__)
			BacktraceState state{ file, 0 };
			_Unwind_Backtrace(UnwindCallback, &state);
#elif !defined(_WIN32)
			void* frames[32];
			int count = backtrace(frames, 32);
			backtrace_symbols_fd(frames, count, fileno(file));
#else
			std::fprintf(file, "(native stack trace unavailable on this platform)\n");
#endif
		}

		void CrashSignalHandler(int sig)
		{
			// Only the first fatal signal reports; recursive faults fall
			// through to the default disposition below.
			if (g_crashing.exchange(true))
			{
				std::signal(sig, SIG_DFL);
				std::raise(sig);
				return;
			}

			auto path = Paths::UserDir() / "last_crash.log";
			if (FILE* file = std::fopen(path.string().c_str(), "w"))
			{
				WriteCrashLog(file, sig);
				std::fclose(file);
			}

			// Also mirror to stderr (logcat on Android).
			WriteCrashLog(stderr, sig);

			std::signal(sig, SIG_DFL);
			std::raise(sig);
		}
	}

	void InstallCrashHandlers()
	{
		if (g_installed.exchange(true))
		{
			return;
		}

		std::signal(SIGSEGV, CrashSignalHandler);
		std::signal(SIGABRT, CrashSignalHandler);
		std::signal(SIGILL, CrashSignalHandler);
		std::signal(SIGFPE, CrashSignalHandler);
#ifdef SIGBUS
		std::signal(SIGBUS, CrashSignalHandler);
#endif
	}

	void SetCrashContext(const std::string& context)
	{
		std::strncpy(g_context, context.c_str(), sizeof(g_context) - 1);
		g_context[sizeof(g_context) - 1] = '\0';
	}

	void ClearCrashContext()
	{
		g_context[0] = '\0';
	}
}
