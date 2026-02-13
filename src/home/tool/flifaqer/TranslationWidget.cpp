#include "ui_TranslationWidget.h"

#include "TranslationWidget.h"

using namespace HomeCompa::FliFaq;

class TranslationWidget::Impl
{
public:
	explicit Impl(TranslationWidget& self)
		: m_self { self }
	{
		m_ui.setupUi(&m_self);
	}

private:
	TranslationWidget& m_self;

	Ui::TranslationWidget m_ui;
};

TranslationWidget::TranslationWidget(QWidget* parent)
	: QWidget(parent)
	, m_impl(*this)
{
}

TranslationWidget::~TranslationWidget() = default;
