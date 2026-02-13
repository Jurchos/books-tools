#pragma once

#include <QMainWindow>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

#include "util/ISettings.h"

namespace HomeCompa::FliFaq
{

class MainWindow final : public QMainWindow
{
	Q_OBJECT
	NON_COPY_MOVABLE(MainWindow)

public:
	explicit MainWindow(std::shared_ptr<ISettings> settings, QWidget* parent = nullptr);
	~MainWindow() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
