#include <ranges>
#include <set>
#include <unordered_set>

#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>

#include <plog/Appenders/ConsoleAppender.h>

#include "lib/UniqueFile.h"
#include "lib/book.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/LogConsoleFormatter.h"
#include "util/files.h"
#include "util/xml/Initializer.h"

#include "log.h"
#include "zip.h"

#include "config/version.h"

using namespace HomeCompa;

template <>
struct std::formatter<QString> : std::formatter<std::string>
{
	auto format(const QString& obj, std::format_context& ctx) const
	{
		return std::formatter<std::string>::format(obj.toStdString(), ctx);
	}
};

namespace
{

constexpr auto APP_ID = "flimerger";

constexpr auto ARCHIVE_WILDCARD_OPTION_NAME = "archives";
constexpr auto FOLDER                       = "folder";

using Hash = std::unordered_multimap<QString, Section::Ptr>;

struct Archive
{
	QString filePath;
	QString hashPath;
};

using Archives = std::vector<Archive>;

struct Settings
{
	QDir     outputDir;
	Archives archives;
	size_t   totalFileCount { 0 };
	QString  logFileName;
};

void ProcessArchives(const Settings& /*settings*/)
{
}

void GetArchives(Settings& settings, const QStringList& arguments)
{
	std::unordered_set<QString> uniqueFiles;
	for (const auto& argument : arguments)
	{
		std::vector<QString> uniqueFilesLocal;

		auto splitted = argument.split(';');
		if (splitted.size() != 2)
			throw std::invalid_argument(std::format("{} must be archives_wildcard;hash_folder", argument));

		const auto wildCard = std::move(splitted.front());
		const QDir hashFolder(splitted.back());
		if (!hashFolder.exists())
			throw std::invalid_argument(std::format("hash folder {} not found", splitted.back()));

		std::ranges::transform(
			Util::ResolveWildcard(wildCard) | std::views::transform([&](const QString& item) {
				return QFileInfo(item);
			}) | std::views::filter([&](const QFileInfo& item) {
				return !uniqueFiles.contains(item.fileName().toLower());
			}),
			std::back_inserter(settings.archives),
			[&](const QFileInfo& fileInfo) {
				auto hashPath = hashFolder.filePath(fileInfo.completeBaseName()) + ".xml";
				if (!QFile::exists(hashPath))
					throw std::invalid_argument(std::format("hash file {} not found", hashPath));

				uniqueFilesLocal.emplace_back(fileInfo.fileName().toLower());
				return Archive { fileInfo.absoluteFilePath(), std::move(hashPath) };
			}
		);

		std::ranges::move(std::move(uniqueFilesLocal), std::inserter(uniqueFiles, uniqueFiles.end()));
	}

	PLOGD << "Total file count calculation";
	settings.totalFileCount = std::accumulate(settings.archives.cbegin(), settings.archives.cend(), settings.totalFileCount, [](const size_t init, const Archive& archive) {
		const Zip zip(archive.filePath);
		return init + zip.GetFileNameList().size();
	});
	PLOGI << "Total file count: " << settings.totalFileCount;
}

Settings ProcessCommandLine(const QCoreApplication& app)
{
	Settings settings;

	QCommandLineParser parser;
	parser.setApplicationDescription(QString("%1 recodes images").arg(APP_ID));
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument(ARCHIVE_WILDCARD_OPTION_NAME, "Input archives with hashes (required)");
	parser.addOptions({
		{ { "o", FOLDER }, "Output folder (required)", FOLDER },
	});

	const auto defaultLogPath = QString("%1/%2.%3.log").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation), COMPANY_ID, APP_ID);
	const auto logOption      = Log::LoggingInitializer::AddLogFileOption(parser, defaultLogPath);
	parser.process(app);

	settings.logFileName = parser.isSet(logOption) ? parser.value(logOption) : defaultLogPath;

	if (const auto& positionalArguments = parser.positionalArguments(); !positionalArguments.isEmpty())
	{
		GetArchives(settings, parser.positionalArguments());
	}
	else
	{
		PLOGE << "Specifying input archives is mandatory";
		parser.showHelp();
	}

	if (parser.isSet(FOLDER))
	{
		settings.outputDir = QDir { parser.value(FOLDER) };
	}
	else
	{
		PLOGE << "Specifying output folder is mandatory";
		parser.showHelp();
	}

	if (!settings.outputDir.exists() && !settings.outputDir.mkpath("."))
		throw std::ios_base::failure(std::format("Cannot create folder {}", settings.outputDir.path()));

	return settings;
}

void run(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv); //-V821
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	auto settings = ProcessCommandLine(app);

	Log::LoggingInitializer                          logging(settings.logFileName.toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);
	PLOGI << QString("%1 started").arg(APP_ID);

	UniqueFileStorage uniqueFileStorage(settings.outputDir.absolutePath());

	ProcessArchives(settings);
}

} // namespace

int main(const int argc, char* argv[])
{
	try
	{
		run(argc, argv);
		return 0;
	}
	catch (const std::exception& ex)
	{
		PLOGE << QString("%1 failed: %2").arg(APP_ID).arg(ex.what());
	}
	catch (...)
	{
		PLOGE << QString("%1 failed").arg(APP_ID);
	}
	return 1;
}
