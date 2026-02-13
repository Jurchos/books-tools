#include "di_app.h"

#include "Hypodermic/Hypodermic.h"
#include "util/Settings.h"

#include "MainWindow.h"

#include "config/version.h"

namespace HomeCompa::FliFaq
{

void DiInit(Hypodermic::ContainerBuilder& builder, std::shared_ptr<Hypodermic::Container>& container)
{
	builder.registerType<MainWindow>().as<QMainWindow>();

	builder
		.registerInstanceFactory([](Hypodermic::ComponentContext&) {
			return std::make_shared<Settings>(COMPANY_ID, APP_ID);
		})
		.as<ISettings>()
		.singleInstance();

	container = builder.build();
}

}
