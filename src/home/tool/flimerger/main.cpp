#include <ranges>
#include <set>
#include <unordered_set>

#include <QCommandLineParser>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

#include <plog/Appenders/ConsoleAppender.h>

#include "lib/UniqueFile.h"
#include "lib/book.h"
#include "logging/LogAppender.h"
#include "logging/init.h"
#include "util/BookUtil.h"
#include "util/LogConsoleFormatter.h"
#include "util/files.h"
#include "util/xml/Initializer.h"

#include "Constant.h"
#include "log.h"

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

struct Archive
{
	QString filePath;
	QString hashPath;
};

using Archives = std::vector<Archive>;

struct Settings
{
	QDir        outputDir;
	QStringList arguments;
	QString     logFileName;
};

void ProcessArchive(const Settings& settings, const Archive& archive, UniqueFileStorage& uniqueFileStorage)
{
	QFile file(archive.hashPath);
	if (!file.open(QIODevice::ReadOnly))
		throw std::invalid_argument(std::format("Cannot read from {}", archive.hashPath));

	Util::Remove::Books toRemove;

	const QFileInfo fileInfo(archive.filePath);

	HashParser::Parse(
		file,
		[&, idBook = 0LL](
#define HASH_PARSER_CALLBACK_ITEM(NAME) QString NAME,
			HASH_PARSER_CALLBACK_ITEMS_X_MACRO
#undef HASH_PARSER_CALLBACK_ITEM
				QString cover,
			QStringList images
		) mutable {
			decltype(UniqueFile::images) imageItems;
			std::ranges::transform(std::move(images) | std::views::as_rvalue, std::inserter(imageItems, imageItems.end()), [](QString&& hash) {
				return ImageItem { .hash = std::move(hash) };
			});
			auto split    = title.split(' ', Qt::SkipEmptyParts);
			auto hashText = id;

			if (!uniqueFileStorage.Add(
					std::move(id),
					UniqueFile {
						.title    = { std::make_move_iterator(split.begin()), std::make_move_iterator(split.end()) },
						.folder   = std::move(folder),
						.file     = file,
						.hashText = std::move(hashText),
						.cover    = { .hash = std::move(cover) },
						.images   = std::move(imageItems)
            }
				))
				toRemove.emplace_back(++idBook, fileInfo.fileName(), std::move(file));
		}
	);

	uniqueFileStorage.Save(fileInfo.completeBaseName(), false);

	const auto dstFilePath = settings.outputDir.filePath(fileInfo.fileName());
	if (!QFile::copy(fileInfo.filePath(), dstFilePath))
		throw std::invalid_argument(std::format("Cannot copy {} to {}", fileInfo.filePath(), dstFilePath));

	for (const char* imageFolder : { Global::COVERS, Global::IMAGES })
	{
		auto imageDir = fileInfo.dir();
		if (!imageDir.cd(imageFolder))
			continue;

		const auto imageArchiveFileSrc = imageDir.absoluteFilePath(fileInfo.completeBaseName()) + ".zip";
		if (!QFile::exists(imageArchiveFileSrc))
			continue;

		QDir dstDir(settings.outputDir.filePath(imageFolder));
		if (!dstDir.exists())
			dstDir.mkpath(".");

		const auto imageArchiveFileDst = dstDir.filePath(fileInfo.completeBaseName() + ".zip");
		if (!QFile::copy(imageArchiveFileSrc, imageArchiveFileDst))
			throw std::invalid_argument(std::format("Cannot copy {} to {}", imageArchiveFileSrc, imageArchiveFileDst));
	}

	if (toRemove.empty())
		return;

	auto allFiles = CollectBookFiles(toRemove, [] {
		return nullptr;
	});
	auto images   = Util::Remove::CollectImageFiles(allFiles, settings.outputDir.absolutePath(), [] {
        return nullptr;
    });
	std::ranges::move(std::move(images), std::inserter(allFiles, allFiles.end()));
	Util::Remove::RemoveFiles(allFiles, settings.outputDir.absolutePath());
}

void ProcessArchives(const Settings& settings, const Archives& archives, UniqueFileStorage& uniqueFileStorage)
{
	PLOGD << "Total file count calculation";
	const auto totalFileCount = std::accumulate(archives.cbegin(), archives.cend(), size_t {0}, [](const size_t init, const Archive& archive) {
		const Zip zip(archive.filePath);
		return init + zip.GetFileNameList().size();
	});

	PLOGI << "Total file count: " << totalFileCount;
	for (const auto& archive : archives)
		ProcessArchive(settings, archive, uniqueFileStorage);
}

Archives GetArchives(const Settings& settings)
{
	std::multimap<int, Archive> sorted;
	std::unordered_set<QString> uniqueFiles;
	const QRegularExpression    rx("^.*?fb2.*?([0-9]+).*?$");

	for (const auto& argument : settings.arguments)
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
				auto       fileName = item.fileName().toLower();
				const auto result   = !uniqueFiles.contains(fileName);
				if (result)
					uniqueFilesLocal.emplace_back(fileName);
				return result;
			}) | std::views::transform([&](const QFileInfo& item) {
				auto hashPath = hashFolder.filePath(item.completeBaseName()) + ".xml";
				return Archive { item.absoluteFilePath(), std::move(hashPath) };
			}) | std::views::filter([](const Archive& item) {
				return QFile::exists(item.hashPath);
			}),
			std::inserter(sorted, sorted.end()),
			[&](Archive archive) {
				const auto match = rx.match(QFileInfo(archive.filePath).fileName());
				return std::make_pair(match.hasMatch() ? match.captured(1).toInt() : 0, std::move(archive));
			}
		);

		std::ranges::move(std::move(uniqueFilesLocal), std::inserter(uniqueFiles, uniqueFiles.end()));
	}

	return std::move(sorted) | std::views::values | std::views::reverse | std::ranges::to<Archives>();
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
		settings.arguments = parser.positionalArguments();
	}
	else
	{
		parser.showHelp();
	}

	if (parser.isSet(FOLDER))
	{
		settings.outputDir = QDir { parser.value(FOLDER) };
	}
	else
	{
		parser.showHelp();
	}

	return settings;
}

void run(int argc, char* argv[])
{
	const QCoreApplication app(argc, argv); //-V821
	QCoreApplication::setApplicationName(APP_ID);
	QCoreApplication::setApplicationVersion(PRODUCT_VERSION);
	Util::XMLPlatformInitializer xmlPlatformInitializer;

	const auto settings = ProcessCommandLine(app);

	Log::LoggingInitializer                          logging(settings.logFileName.toStdWString());
	plog::ConsoleAppender<Util::LogConsoleFormatter> consoleAppender;
	Log::LogAppender                                 logConsoleAppender(&consoleAppender);
	PLOGI << QString("%1 started").arg(APP_ID);

	if (!settings.outputDir.exists() && !settings.outputDir.mkpath("."))
		throw std::ios_base::failure(std::format("Cannot create folder {}", settings.outputDir.path()));

	const auto        archives = GetArchives(settings);

	UniqueFileStorage uniqueFileStorage(settings.outputDir.absolutePath());

	ProcessArchives(settings, archives, uniqueFileStorage);
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
