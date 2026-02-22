#include "ui_TranslationWidget.h"

#include "TranslationWidget.h"

#include <ranges>

#include "fnd/FindPair.h"
#include "fnd/ScopedCall.h"

#include "role.h"

using namespace HomeCompa::FliFaq;
using namespace HomeCompa;

namespace
{

constexpr auto REFERENCE_LANGUAGE   = "referenceLanguage";
constexpr auto TRANSLATION_LANGUAGE = "translationLanguage";

struct ModeSettings
{
	const int         languageRole;
	const int         questionRole;
	const int         answerRole;
	const char* const languageKey;
};

class Model final : public QAbstractListModel
{
public:
	struct Role
	{
		enum
		{
			Row = Qt::UserRole + 1,
			Text,
		};
	};

public:
	void SetData(const QString& data)
	{
		const ScopedCall resetGuard(
			[this] {
				beginResetModel();
			},
			[this] {
				endResetModel();
			}
		);
		m_data = data.split('\n');
	}

private: // QAbstractItemModel
	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : static_cast<int>(m_data.size());
	}

	QVariant data(const QModelIndex& index, const int role) const override
	{
		if (index.isValid())
		{
			assert(index.row() >= 0 && index.row() < rowCount());
			return role == Qt::DisplayRole ? m_data[index.row()] : QVariant {};
		}

		switch (role)
		{
			case Role::Row:
				return m_row;
			case Role::Text:
				return m_data.join('\n');
			default:
				break;
		}
		return assert(false && "unexpected role"), QVariant {};
	}

	bool setData(const QModelIndex& /*index*/, const QVariant& value, const int role) override
	{
		if (role != Role::Row)
			return false;

		if (const auto it = std::ranges::find_if(
				m_data,
				[str = QString("%%1").arg(value.toInt())](const QString& item) {
					return item.contains(str);
				}
			);
		    it != m_data.end())
		{
			m_row = static_cast<int>(std::distance(m_data.begin(), it));
			return true;
		}

		m_row = -1;
		return false;
	}

private:
	QStringList m_data;
	int         m_row { -1 };
};

class TranslationWidgetImpl // NOLINT(cppcoreguidelines-special-member-functions)
{
public:
	TranslationWidgetImpl(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget& ui, const ModeSettings& modeSettings)
		: m_self { self }
		, m_settings { settings }
		, m_model { model }
		, m_ui { ui }
		, m_modeSettings { modeSettings }
	{
		m_ui.answer->setModel(m_dataModel.get());
		m_ui.answer->verticalHeader()->setDefaultAlignment(Qt::AlignTop);
		QObject::connect(m_dataModel.get(), &QAbstractItemModel::dataChanged, [this] {
			OnDataChanged();
		});
		QObject::connect(m_dataModel.get(), &QAbstractItemModel::rowsInserted, [this] {
			OnDataChanged();
		});
		QObject::connect(m_dataModel.get(), &QAbstractItemModel::rowsMoved, [this] {
			OnDataChanged();
		});
		QObject::connect(m_dataModel.get(), &QAbstractItemModel::rowsRemoved, [this] {
			OnDataChanged();
		});
	}

	virtual ~TranslationWidgetImpl() = default;

public:
	virtual void SetCurrentIndex(const QModelIndex& index) = 0;

	virtual void OnDataChanged() = 0;

	virtual void SetRow(int)
	{
	}

protected:
	void Reset()
	{
		m_dataModel->SetData(m_model.data(m_currentIndex, m_modeSettings.answerRole).toString());
		m_ui.answer->resizeRowsToContents();
	}

protected:
	TranslationWidget&  m_self;
	ISettings&          m_settings;
	QAbstractItemModel& m_model;

	Ui::TranslationWidget& m_ui;
	const ModeSettings&    m_modeSettings;

	PropagateConstPtr<Model> m_dataModel;
	QPersistentModelIndex    m_currentIndex;
};

class TranslationWidgetCommon final : public TranslationWidgetImpl
{
public:
	static std::unique_ptr<TranslationWidgetImpl> Create(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget& ui, const ModeSettings& modeSettings)
	{
		return std::make_unique<TranslationWidgetCommon>(self, settings, model, ui, modeSettings);
	}

	TranslationWidgetCommon(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget& ui, const ModeSettings& modeSettings)
		: TranslationWidgetImpl(self, settings, model, ui, modeSettings)
	{
		if (const auto index = m_ui.language->findData(m_settings.Get(modeSettings.languageKey, QString())); index >= 0)
			m_ui.language->setCurrentIndex(index);

		QObject::connect(m_ui.language, &QComboBox::currentIndexChanged, [this] {
			m_settings.Set(m_modeSettings.languageKey, m_ui.language->currentData());
			OnLanguageChanged();
		});

		QObject::connect(m_ui.question, &QLineEdit::editingFinished, [this] {
			if (m_currentIndex.isValid())
				m_model.setData(m_currentIndex, m_ui.question->text(), m_modeSettings.questionRole);
		});

		QObject::connect(m_ui.answer->selectionModel(), &QItemSelectionModel::selectionChanged, [this] {
			emit m_self.RowChanged(m_ui.answer->currentIndex().row() + 1);
		});

		if (m_ui.language->count() > 0)
			OnLanguageChanged();
	}

private: // TranslationWidgetImpl
	void SetCurrentIndex(const QModelIndex& index) override
	{
		m_currentIndex = index;
		const QSignalBlocker questionSignalBlocker(m_ui.question), answerSignalBlocker(m_ui.answer);
		m_ui.question->setText(m_model.data(index, m_modeSettings.questionRole).toString());
		Reset();
	}

	void OnDataChanged() override
	{
		if (m_currentIndex.isValid())
			m_model.setData(m_currentIndex, static_cast<const QAbstractItemModel&>(*m_dataModel).data({}, Model::Role::Text), m_modeSettings.answerRole);
	}

private:
	void OnLanguageChanged() const
	{
		m_model.setData({}, m_ui.language->currentData(), m_modeSettings.languageRole);
		emit m_self.LanguageChanged();
	}
};

class TranslationWidgetTemplate final : public TranslationWidgetImpl
{
public:
	static std::unique_ptr<TranslationWidgetImpl> Create(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget& ui, const ModeSettings& modeSettings)
	{
		return std::make_unique<TranslationWidgetTemplate>(self, settings, model, ui, modeSettings);
	}

	TranslationWidgetTemplate(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget& ui, const ModeSettings& modeSettings)
		: TranslationWidgetImpl(self, settings, model, ui, modeSettings)
	{
		m_ui.question->setVisible(false);
		m_ui.answer->verticalHeader()->setVisible(false);

		QObject::connect(m_ui.language, &QComboBox::currentIndexChanged, [this] {
			OnLanguageChanged();
		});
	}

private: // TranslationWidgetImpl
	void SetCurrentIndex(const QModelIndex& index) override
	{
		m_currentIndex = index;
		const QSignalBlocker questionSignalBlocker(m_ui.question), answerSignalBlocker(m_ui.answer);
		if (const auto n = m_ui.language->findData(m_model.data(index, m_modeSettings.questionRole)); n >= 0)
			m_ui.language->setCurrentIndex(n);
		Reset();
	}

	void OnDataChanged() override
	{
		if (!m_currentIndex.isValid())
			return;

		OnLanguageChanged();
		m_model.setData(m_currentIndex, static_cast<const QAbstractItemModel&>(*m_dataModel).data({}, Model::Role::Text), m_modeSettings.answerRole);
	}

	void SetRow(const int row) override
	{
		auto& dataModel = static_cast<QAbstractItemModel&>(*m_dataModel);
		if (dataModel.setData({}, row, Model::Role::Row))
			m_ui.answer->setCurrentIndex(dataModel.index(dataModel.data({}, Model::Role::Row).toInt(), 0));
	}

private:
	void OnLanguageChanged() const
	{
		m_model.setData(m_currentIndex, m_ui.language->currentData(), m_modeSettings.languageRole);
	}
};

using TranslationWidgetCreator = std::unique_ptr<TranslationWidgetImpl> (*)(TranslationWidget& self, ISettings& settings, QAbstractItemModel& model, Ui::TranslationWidget&, const ModeSettings&);
constexpr std::pair<TranslationWidget::Mode, std::pair<TranslationWidgetCreator, ModeSettings>> MODE_SETTINGS[] {
	{    TranslationWidget::Mode::Template,                     { &TranslationWidgetTemplate::Create, { Role::TemplateQuestion, Role::TemplateQuestion, Role::TemplateAnswer, nullptr } } },
	{   TranslationWidget::Mode::Reference,         { &TranslationWidgetCommon::Create, { Role::ReferenceLanguage, Role::ReferenceQuestion, Role::ReferenceAnswer, REFERENCE_LANGUAGE } } },
	{ TranslationWidget::Mode::Translation, { &TranslationWidgetCommon::Create, { Role::TranslationLanguage, Role::TranslationQuestion, Role::TranslationAnswer, TRANSLATION_LANGUAGE } } },
};

} // namespace

class TranslationWidget::Impl
{
public:
	explicit Impl(TranslationWidget& self, std::shared_ptr<ISettings> settings, std::shared_ptr<QAbstractItemModel> model)
		: m_self { self }
		, m_settings { std::move(settings) }
		, m_model { std::move(model) }
	{
		m_ui.setupUi(&m_self);
	}

public:
	void SetMode(const Mode mode)
	{
		const auto& [creator, settings] = FindSecond(MODE_SETTINGS, mode);
		m_impl.reset(creator(m_self, *m_settings, *m_model, m_ui, settings));
	}

	void AddLanguage(const QString& language) const
	{
		m_ui.language->addItem(language, language);
	}

	void SetLanguage(const QString& language) const
	{
		if (const auto index = m_ui.language->findData(language); index >= 0)
			m_ui.language->setCurrentIndex(index);
	}

	void SetCurrentIndex(const QModelIndex& index)
	{
		if (index.isValid())
			m_impl->SetCurrentIndex(index);
	}

	void SetRow(const int row)
	{
		m_impl->SetRow(row);
	}

private:
	TranslationWidget&                                     m_self;
	PropagateConstPtr<ISettings, std::shared_ptr>          m_settings;
	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_model;
	PropagateConstPtr<TranslationWidgetImpl>               m_impl { std::unique_ptr<TranslationWidgetImpl> {} };

	Ui::TranslationWidget m_ui;
};

TranslationWidget::TranslationWidget(std::shared_ptr<ISettings> settings, std::shared_ptr<QAbstractItemModel> model, QWidget* parent)
	: QWidget(parent)
	, m_impl(*this, std::move(settings), std::move(model))
{
}

TranslationWidget::~TranslationWidget() = default;

void TranslationWidget::SetMode(const Mode mode)
{
	m_impl->SetMode(mode);
}

void TranslationWidget::AddLanguage(const QString& language) const
{
	m_impl->AddLanguage(language);
}

void TranslationWidget::SetLanguage(const QString& language) const
{
	m_impl->SetLanguage(language);
}

void TranslationWidget::SetCurrentIndex(const QModelIndex& index)
{
	m_impl->SetCurrentIndex(index);
}

void TranslationWidget::SetRow(const int row)
{
	m_impl->SetRow(row);
}
