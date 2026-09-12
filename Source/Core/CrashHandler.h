#pragma once

#include <string>

namespace Freeking
{
	// Fatal-signal reporter: writes UserDir/last_crash.log (signal + map
	// context + native backtrace where available) so a native crash can be
	// diagnosed from the device instead of vanishing silently.
	void InstallCrashHandlers();
	void SetCrashContext(const std::string& context);
	void ClearCrashContext();
}
