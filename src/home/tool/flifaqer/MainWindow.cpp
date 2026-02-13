#include "ui_MainWindow.h"

#include "MainWindow.h"

#include "util/GeometryRestorable.h"

using namespace HomeCompa::FliFaq;

namespace
{

constexpr auto MAIN_WINDOW = "MainWindow";

}

class MainWindow::Impl final
	: Util::GeometryRestorable
	, Util::GeometryRestorableObserver
{
	NON_COPY_MOVABLE(Impl)

public:
	Impl(MainWindow& self, std::shared_ptr<ISettings> settings)
		: GeometryRestorable(*this, std::move(settings), MAIN_WINDOW)
		, GeometryRestorableObserver(self)
		, m_self { self }
	{
		m_ui.setupUi(&m_self);
		LoadGeometry();
	}

	~Impl() override
	{
		SaveGeometry();
	}

private:
	MainWindow& m_self;

	Ui::MainWindow m_ui;
};

MainWindow::MainWindow(std::shared_ptr<ISettings> settings, QWidget* parent)
	: QMainWindow(parent)
	, m_impl(*this, std::move(settings))
{
}

MainWindow::~MainWindow() = default;
