#include <QStandardPaths>

#include <plog/Appenders/ConsoleAppender.h>

#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/LogConsoleFormatter.h"

#include "log.h"

#include "config/version.h"

using namespace HomeCompa;

namespace
{

constexpr auto APP_ID = "flicmp";

void go(const int /*argc*/, char* /*argv*/[])
{
}

} // namespace

int main(const int argc, char* argv[])
{
	Log::LoggingInitializer                          logging(QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID).toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);

	try
	{
		PLOGI << "start";
		go(argc, argv);
		return 0;
	}
	catch (const std::exception& ex)
	{
		PLOGE << ex.what();
	}
	catch (...)
	{
		PLOGE << "Unknown error";
	}

	return 1;
}
