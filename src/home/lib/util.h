#pragma once

#include <filesystem>
#include <format>
#include <string>

#include <QString>

#include "export/lib.h"

class QByteArray;

namespace HomeCompa::FliLib
{

LIB_EXPORT void     Write(const QString& fileName, const QByteArray& data);
LIB_EXPORT QString& ReplaceTags(QString& str);

}

template <>
struct std::formatter<QString> : std::formatter<std::string>
{
	auto format(const QString& obj, std::format_context& ctx) const
	{
		return std::formatter<std::string>::format(obj.toStdString(), ctx);
	}
};

template <>
struct std::formatter<std::filesystem::path> : std::formatter<std::string>
{
	auto format(const std::filesystem::path& obj, std::format_context& ctx) const
	{
		return std::formatter<std::string>::format(obj.string(), ctx);
	}
};
