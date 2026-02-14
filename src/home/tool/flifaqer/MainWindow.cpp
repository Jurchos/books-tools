#include "ui_MainWindow.h"

#include "MainWindow.h"

#include "util/GeometryRestorable.h"

#include "di_app.h"
#include "role.h"

#include "config/version.h"

using namespace HomeCompa::FliFaq;

namespace
{

constexpr auto MAIN_WINDOW = "MainWindow";

constexpr auto LANGUAGE = "language";

constexpr auto FONT_SIZE_KEY = "ui/Font/pointSizeF";

constexpr auto ADD = QT_TRANSLATE_NOOP("flifaqer", "Add");
constexpr auto REMOVE = QT_TRANSLATE_NOOP("flifaqer", "Remove");

}

class MainWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(MainWindow& self, std::shared_ptr<ISettings> settings, std::shared_ptr<QAbstractItemModel> model, std::shared_ptr<TranslationWidget> reference, std::shared_ptr<TranslationWidget> translation)
		: GeometryRestorable(*this, settings, MAIN_WINDOW)
		, GeometryRestorableObserver(self)
		, m_self { self }
		, m_settings { std::move(settings) }
		, m_model { std::move(model) }
		, m_reference { std::move(reference) }
		, m_translation { std::move(translation) }
	{
		m_ui.setupUi(&m_self);
		m_ui.navigatorView->setModel(m_model.get());
		m_ui.referenceLayout->addWidget(m_reference.get());
		m_ui.translationLayout->addWidget(m_translation.get());

		for (const auto& language : m_model->data({}, Role::LanguageList).toStringList())
		{
			m_ui.language->addItem(language, language);
			m_reference->AddLanguage(language);
			m_translation->AddLanguage(language);
		}

		m_reference->SetMode(TranslationWidget::Mode::Reference);
		m_translation->SetMode(TranslationWidget::Mode::Translation);

		if (const auto index = m_ui.language->findData(m_settings->Get(LANGUAGE, QString())); index >= 0)
			m_ui.language->setCurrentIndex(index);

		connect(m_ui.language, &QComboBox::currentIndexChanged, [this] {
			m_model->setData({}, m_ui.language->currentData().toString(), Role::Language);
			m_settings->Set(LANGUAGE, m_ui.language->currentData());
		});

		if (m_ui.language->count() > 0)
			m_model->setData({}, m_ui.language->currentData().toString(), Role::Language);

		connect(m_ui.navigatorView, &QWidget::customContextMenuRequested, [this](const QPoint& pos) {
			OnNavigationViewContextMenuRequested(pos);
		});

		connect(m_ui.navigatorView->selectionModel(), &QItemSelectionModel::currentChanged, [this](const QModelIndex& index) {
			m_reference->SetCurrentIndex(index);
			m_translation->SetCurrentIndex(index);
		});
		connect(m_reference.get(), &TranslationWidget::LanguageChanged, [this] {
			m_reference->SetCurrentIndex(m_ui.navigatorView->currentIndex());
		});
		connect(m_translation.get(), &TranslationWidget::LanguageChanged, [this] {
			m_translation->SetCurrentIndex(m_ui.navigatorView->currentIndex());
		});

		connect(m_ui.actionSave, &QAction::triggered, [this] {
			m_model->setData({}, {}, Role::Save);
		});
		connect(m_ui.actionExport, &QAction::triggered, [this] {
			m_model->setData({}, {}, Role::Export);
		});

		const auto incrementFontSize = [&](const int value) {
			const auto fontSize = m_settings->Get(FONT_SIZE_KEY, 10);
			m_settings->Set(FONT_SIZE_KEY, fontSize + value);
		};
		connect(m_ui.actionFontSizeUp, &QAction::triggered, &m_self, [=] {
			incrementFontSize(1);
		});
		connect(m_ui.actionFontSizeDown, &QAction::triggered, &m_self, [=] {
			incrementFontSize(-1);
		});

		m_self.setWindowTitle(QString("%1 %2").arg(APP_ID, PRODUCT_VERSION));
		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

private:
	void OnNavigationViewContextMenuRequested(const QPoint& pos)
	{
		const auto index = m_ui.navigatorView->indexAt(pos);

		QMenu menu;
		menu.setFont(m_self.font());
		menu.addAction(QCoreApplication::translate(APP_ID, ADD), [&] {
			m_model->insertRow(m_model->rowCount(index), index);
			if (index.isValid())
				m_ui.navigatorView->expand(index);
			m_ui.navigatorView->setCurrentIndex(m_model->index(m_model->rowCount(index) - 1, 0, index));
		});
		menu.addAction(
				QCoreApplication::translate(APP_ID, REMOVE),
				[&] {
					m_model->removeRow(index.row(), index.parent());
				}
		)->setEnabled(index.isValid());
		menu.exec(QCursor::pos());
	}

private:
	MainWindow&                m_self;
	std::shared_ptr<ISettings> m_settings;

	PropagateConstPtr<QAbstractItemModel, std::shared_ptr> m_model;
	PropagateConstPtr<TranslationWidget, std::shared_ptr>  m_reference;
	PropagateConstPtr<TranslationWidget, std::shared_ptr>  m_translation;

	Ui::MainWindow m_ui;
};

MainWindow::MainWindow(
	std::shared_ptr<ISettings>          settings,
	std::shared_ptr<QAbstractItemModel> model,
	std::shared_ptr<TranslationWidget>  reference,
	std::shared_ptr<TranslationWidget>  translation,
	QWidget*                            parent
)
	: QMainWindow(parent)
	, m_impl(*this, std::move(settings), std::move(model), std::move(reference), std::move(translation))
{
}

MainWindow::~MainWindow() = default;
