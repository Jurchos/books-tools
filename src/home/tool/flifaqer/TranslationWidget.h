#pragma once

#include <QWidget>

#include "fnd/NonCopyMovable.h"
#include "fnd/memory.h"

namespace HomeCompa::FliFaq
{

class TranslationWidget final : public QWidget
{
	Q_OBJECT
	NON_COPY_MOVABLE(TranslationWidget)

public:
	TranslationWidget(QWidget* parent = nullptr);
	~TranslationWidget() override;

private:
	class Impl;
	PropagateConstPtr<Impl> m_impl;
};

}
