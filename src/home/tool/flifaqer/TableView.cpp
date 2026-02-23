#include "TableView.h"

using namespace HomeCompa::FliFaq;
using namespace HomeCompa;

TableView::TableView(QWidget* parent)
	: QTableView(parent)
{
}

void TableView::mouseDoubleClickEvent(QMouseEvent* event)
{
	emit mouseDoubleClicked(event);
}
