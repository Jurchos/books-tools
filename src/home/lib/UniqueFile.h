#pragma once
#include <set>

#include <QDateTime>
#include <QImage>
#include <QStringList>

#include "fnd/algorithm.h"

#include "export/lib.h"

namespace HomeCompa
{

struct LIB_EXPORT ImageItem
{
	QString    fileName;
	QByteArray body;
	QDateTime  dateTime;
	QString    hash;
	QImage     image;

	bool operator<(const ImageItem& rhs) const;
};

using ImageItems = std::vector<ImageItem>;

struct LIB_EXPORT UniqueFile
{
	enum class ImagesCompareResult
	{
		Equal,
		Inner,
		Outer,
		Varied,
	};

	std::set<QString>   title;
	QString             folder;
	QString             file;
	QString             hashText;
	QStringList         hashSections;
	ImageItem           cover;
	std::set<ImageItem> images;

	int order { 0 };

	bool                              operator<(const UniqueFile& rhs) const;
	QString                           GetTitle() const;
	[[nodiscard]] ImagesCompareResult CompareImages(const UniqueFile& rhs) const;
	void                              ClearImages();
};

class LIB_EXPORT UniqueFileStorage
{
	struct Dup
	{
		UniqueFile file;
		UniqueFile origin;
	};

public:
	explicit UniqueFileStorage(QString dstDir);

public:
	std::pair<ImageItem, std::set<ImageItem>> GetImages(UniqueFile& file);
	void                                      SetImages(const QString& hash, const QString& fileName, ImageItem cover, std::set<ImageItem> images);
	UniqueFile*                               Add(QString hash, UniqueFile file);
	std::pair<ImageItems, ImageItems>         GetNewImages();
	void                                      Save(const QString& folder, bool moveDuplicates);
	void                                      Skip(const QString& fileName);

private:
	const QString m_dstDir;
	std::mutex    m_guard;

	std::unordered_multimap<QString, UniqueFile> m_old;
	std::vector<Dup>                             m_dup;

	std::unordered_map<std::pair<QString, QString>, std::pair<QString, QString>, Util::PairHash<QString, QString>> m_skip;

	std::unordered_multimap<QString, std::pair<UniqueFile, std::vector<UniqueFile>>> m_new;

	const QString m_si;
};

struct HashParser
{
#define HASH_PARSER_CALLBACK_ITEMS_X_MACRO \
	HASH_PARSER_CALLBACK_ITEM(id)          \
	HASH_PARSER_CALLBACK_ITEM(folder)      \
	HASH_PARSER_CALLBACK_ITEM(file)        \
	HASH_PARSER_CALLBACK_ITEM(title)

	using Callback = std::function<void(
#define HASH_PARSER_CALLBACK_ITEM(NAME) QString NAME,
		HASH_PARSER_CALLBACK_ITEMS_X_MACRO
#undef HASH_PARSER_CALLBACK_ITEM
			QString cover,
		QStringList images
	)>;

	LIB_EXPORT static void Parse(QIODevice& input, Callback callback);
};

} // namespace HomeCompa
