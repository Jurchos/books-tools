#include "model.h"

#include <ranges>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "fnd/ScopedCall.h"
#include "fnd/algorithm.h"

#include "util/StrUtil.h"

#include "Constant.h"
#include "di_app.h"
#include "log.h"
#include "role.h"

using namespace HomeCompa::FliFaq;
using namespace HomeCompa;

namespace
{

constexpr auto LANGUAGE = "language";
constexpr auto ITEMS    = "x";
constexpr auto QUESTION = "q";
constexpr auto ANSWER   = "t";

constexpr QChar STRING_SEPARATOR = '\n';

constexpr auto NEW_ITEM = QT_TRANSLATE_NOOP("flifaqer", "New question");

class String
{
	static QString EMPTY;

public:
	bool Set(const QString& key, QString value)
	{
		auto& currentValue = m_data[key];
		return Util::Set(currentValue, std::move(value));
	}

	const QString& Get(const QString& key) const
	{
		const auto it = m_data.find(key);
		return it != m_data.end() ? it->second : EMPTY;
	}

private:
	std::unordered_map<QString, QString> m_data;
};

QString String::EMPTY;

struct Item;
using Items = std::vector<std::shared_ptr<Item>>;

struct Item
{
	Item*  parent { nullptr };
	int    row { -1 };
	String question;
	String answer;

	Items children;
};

using File  = std::pair<QString, QString>;
using Files = std::vector<File>;

void ParseItems(const QString& language, const QJsonArray& jsonArray, const std::shared_ptr<Item>& parent)
{
	for (const auto [jsonValue, row] : std::views::zip(jsonArray, std::views::iota(0)))
	{
		const auto obj   = jsonValue.toObject();
		auto&      child = row < static_cast<int>(parent->children.size()) ? parent->children[row] : parent->children.emplace_back(std::make_shared<Item>(parent.get(), row));
		child->question.Set(language, obj.value(QUESTION).toString());
		const auto answer = obj.value(ANSWER).toArray();
		child->answer.Set(
			language,
			(answer | std::views::transform([](const auto& item) {
				 return item.toString();
			 })
		     | std::ranges::to<QStringList>())
				.join(STRING_SEPARATOR)
		);
		ParseItems(language, obj.value(ITEMS).toArray(), child);
	}
}

File ParseFile(QString file, const std::shared_ptr<Item>& root)
{
	QFile stream(file);
	if (!stream.open(QIODevice::ReadOnly))
		throw std::invalid_argument(std::format("cannot open {}", file));

	QJsonParseError jsonParseError;
	const auto      doc = QJsonDocument::fromJson(stream.readAll(), &jsonParseError);
	if (jsonParseError.error != QJsonParseError::NoError)
		throw std::invalid_argument(std::format("cannot parse {}: {}", file, jsonParseError.errorString()));

	if (!doc.isObject())
		throw std::invalid_argument(std::format("document must be an object {}", file));

	const auto obj = doc.object();

	auto language = obj.value(LANGUAGE).toString();
	if (language.isEmpty())
		throw std::invalid_argument(std::format("document language must be specified {}", file));

	ParseItems(language, obj.value(ITEMS).toArray(), root);

	return std::make_pair(std::move(language), std::move(file));
}

auto CreateModelData(const ISettings& settings)
{
	auto        result = std::make_pair(Files {}, std::make_shared<Item>());
	auto&       files  = result.first;
	const auto& root   = result.second;

	std::ranges::transform(settings.Get(Constant::INPUT_FILES).toStringList(), std::back_inserter(files), [&](auto&& item) {
		return ParseFile(std::forward<QString>(item), root);
	});

	return result;
}

QJsonArray SaveImpl(const QString& language, const Item& item)
{
	QJsonArray jsonArray;
	for (const auto& child : item.children)
	{
		QJsonArray answer;
		for (const auto& str : child->answer.Get(language).split(STRING_SEPARATOR))
			answer.append(str);

		QJsonObject obj {
			{ QUESTION, child->question.Get(language) },
			{   ANSWER,             std::move(answer) },
		};

		if (!child->children.empty())
			obj.insert(ITEMS, SaveImpl(language, *child));

		jsonArray.append(std::move(obj));
	}
	return jsonArray;
}

class ModelImpl final : public QAbstractItemModel
{
public:
	static std::unique_ptr<QAbstractItemModel> Create(const ISettings& settings)
	{
		auto [files, root] = CreateModelData(settings);
		return std::make_unique<ModelImpl>(std::move(files), std::move(root));
	}

	ModelImpl(Files files, std::shared_ptr<Item> root)
		: m_files { std::move(files) }
		, m_root { std::move(root) }
	{
	}

private: // QAbstractItemModel
	[[nodiscard]] QModelIndex index(const int row, const int column, const QModelIndex& parent) const override
	{
		if (!hasIndex(row, column, parent))
			return {};

		const auto* parentItem = parent.isValid() ? static_cast<Item*>(parent.internalPointer()) : m_root.get();
		return row < 0 || row >= static_cast<int>(parentItem->children.size()) ? QModelIndex() : createIndex(row, column, parentItem->children[static_cast<size_t>(row)].get());
	}

	[[nodiscard]] QModelIndex parent(const QModelIndex& index) const override
	{
		if (!index.isValid())
			return {};

		const auto* childItem  = static_cast<Item*>(index.internalPointer());
		const auto* parentItem = childItem->parent;

		return parentItem != m_root.get() ? createIndex(parentItem->row, 0, parentItem) : QModelIndex();
	}

	[[nodiscard]] int rowCount(const QModelIndex& parent) const override
	{
		const auto* parentItem = parent.isValid() ? static_cast<Item*>(parent.internalPointer()) : m_root.get();
		return static_cast<int>(parentItem->children.size());
	}

	[[nodiscard]] int columnCount(const QModelIndex&) const override
	{
		return 1;
	}

	[[nodiscard]] QVariant data(const QModelIndex& index, const int role) const override
	{
		return index.isValid() ? GetDataImpl(index, role) : GetDataImpl(role);
	}

	bool setData(const QModelIndex& index, const QVariant& value, const int role) override
	{
		return index.isValid() ? SetDataImpl(index, value, role) : SetDataImpl(value, role);
	}

	bool insertRows(const int row, const int count, const QModelIndex& parent) override
	{
		if (row < 0 || row > rowCount(parent))
			return false;

		const ScopedCall insertGuard(
			[&] {
				beginInsertRows(parent, row, row + count - 1);
			},
			[this] {
				endInsertRows();
			}
		);

		auto* parentItem = parent.isValid() ? static_cast<Item*>(parent.internalPointer()) : m_root.get();
		parentItem->children.insert_range(std::next(parentItem->children.begin(), row), std::views::iota(row, row + count) | std::views::transform([&](const int n) {
																							auto item = std::make_shared<Item>(parentItem, n);
																							for (const auto& language : m_files | std::views::keys)
																								item->question.Set(language, QCoreApplication::translate(APP_ID, NEW_ITEM));
																							return item;
																						}));
		for (const auto& item : parentItem->children | std::views::drop(row + count))
			item->row += count;

		return true;
	}

	bool removeRows(const int row, const int count, const QModelIndex& parent) override
	{
		if (row < 0 || row > rowCount(parent))
			return false;

		const ScopedCall insertGuard(
			[&] {
				beginRemoveRows(parent, row, row + count - 1);
			},
			[this] {
				endRemoveRows();
			}
		);

		auto* parentItem = parent.isValid() ? static_cast<Item*>(parent.internalPointer()) : m_root.get();
		parentItem->children.erase(std::next(parentItem->children.begin(), row), std::next(parentItem->children.begin(), row + count));
		for (const auto& item : parentItem->children | std::views::drop(row))
			item->row -= count;

		return true;
	}

private:
	[[nodiscard]] QVariant GetDataImpl(const int role) const
	{
		switch (role)
		{
			case Role::LanguageList:
				return m_files | std::views::keys | std::ranges::to<QStringList>();

			default:
				break;
		}

		return assert(false && "unexpected role"), QVariant {};
	}

	[[nodiscard]] QVariant GetDataImpl(const QModelIndex& index, const int role) const
	{
		const auto* item = static_cast<Item*>(index.internalPointer());
		assert(item);

		switch (role)
		{
			case Qt::DisplayRole:
				return item->question.Get(m_language);

			case Role::ReferenceQuestion:
				return item->question.Get(m_referenceLanguage);

			case Role::ReferenceAnswer:
				return item->answer.Get(m_referenceLanguage);

			case Role::TranslationQuestion:
				return item->question.Get(m_translationLanguage);

			case Role::TranslationAnswer:
				return item->answer.Get(m_translationLanguage);

			default:
				break;
		}
		return {};
	}

	bool SetDataImpl(const QVariant& value, const int role)
	{
		switch (role)
		{
			case Role::Language:
				return Util::Set(
					m_language,
					value.toString(),
					[this] {
						beginResetModel();
					},
					[this] {
						endResetModel();
					}
				);

			case Role::ReferenceLanguage:
				return Util::Set(m_referenceLanguage, value.toString());

			case Role::TranslationLanguage:
				return Util::Set(m_translationLanguage, value.toString());

			case Role::Save:
				try
				{
					Save();
					return true;
				}
				catch (const std::exception& ex)
				{
					PLOGE << ex.what();
				}
				catch (...)
				{
					PLOGE << "Unknown save error";
				}
				return false;

			default:
				break;
		}

		return assert(false && "unexpected role"), false;
	}

	bool SetDataImpl(const QModelIndex& index, const QVariant& value, const int role)
	{
		auto* item = static_cast<Item*>(index.internalPointer());
		assert(item);

		switch (role)
		{
			case Role::ReferenceQuestion:
				if (item->question.Set(m_referenceLanguage, value.toString()))
				{
					emit dataChanged(index, index, { Qt::DisplayRole });
					return true;
				}
				return false;

			case Role::ReferenceAnswer:
				return item->answer.Set(m_referenceLanguage, value.toString());

			case Role::TranslationQuestion:
				if (item->question.Set(m_translationLanguage, value.toString()))
				{
					emit dataChanged(index, index, { Qt::DisplayRole });
					return true;
				}
				return false;

			case Role::TranslationAnswer:
				return item->answer.Set(m_translationLanguage, value.toString());

			default:
				break;
		}

		return QAbstractItemModel::setData(index, value, role);
	}

	void Save() const
	{
		for (const auto& [language, file] : m_files)
			Save(language, file);
	}

	void Save(const QString& language, const QString& file) const
	{
		QJsonObject obj {
			{ LANGUAGE, language },
			{ ITEMS, SaveImpl(language, *m_root) },
		};

		QFile stream(file);
		if (!stream.open(QIODevice::WriteOnly))
			throw std::runtime_error(std::format("cannot write to {}", file));

		stream.write(QJsonDocument(obj).toJson());
	}

private:
	const Files                 m_files;
	const std::shared_ptr<Item> m_root;

	QString m_language, m_referenceLanguage, m_translationLanguage;
};

} // namespace

Model::Model(const std::shared_ptr<const ISettings>& settings, QObject* parent)
	: QIdentityProxyModel(parent)
	, m_source { ModelImpl::Create(*settings) }
{
	QIdentityProxyModel::setSourceModel(m_source.get());
}

Model::~Model() = default;
