#pragma once

#include "qnamespace.h"

namespace HomeCompa::FliFaq
{

struct Role
{
	enum
	{
		AddFile = Qt::UserRole,
		AddTemplate,
		QuestionTypeList,
		LanguageList,
		Language,
		ReferenceLanguage,
		ReferenceQuestion,
		ReferenceAnswer,
		TranslationLanguage,
		TranslationQuestion,
		TranslationAnswer,
		TemplateQuestion,
		TemplateAnswer,
		Macro,
		Save,
		Export,
		Validate,
	};
};

}
