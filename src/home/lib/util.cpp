#include "util.h"

#include <QFileInfo>
#include <QRegularExpression>

#include "log.h"

namespace HomeCompa::FliLib
{

void Write(const QString& fileName, const QByteArray& data)
{
	QFile output(fileName);
	if (!output.open(QIODevice::WriteOnly))
	{
		PLOGE << "Cannot write to " << fileName;
		return;
	}

	const auto written = output.write(data);
	if (written == data.size())
		PLOGV << QFileInfo(fileName).fileName() << ": " << written << " bytes written";
	else
		PLOGE << QFileInfo(fileName).fileName() << ": " << written << " bytes written of a " << data.size();
}

QString& ReplaceTags(QString& str)
{
	static constexpr std::pair<const char*, const char*> tags[] {
		{    "br",    "br" },
        {    "hr",    "hr" },
        { "quote",     "q" },
        { "table", "table" },
        {    "tr",    "tr" },
        {    "th",    "th" },
        {    "td",    "td" },
	};

	str.replace("<p>&nbsp;</p>", "");

	auto strings = str.split('\n', Qt::SkipEmptyParts);
	erase_if(strings, [](const QString& item) {
		return item.simplified().isEmpty();
	});
	str = strings.join("<br/>");

	str.replace(QRegularExpression(R"(\[(\w)\])"), R"(<\1>)").replace(QRegularExpression(R"(\[(/\w)\])"), R"(<\1>)");
	for (const auto& [from, to] : tags)
		str.replace(QString("[%1]").arg(from), QString("<%1>").arg(to), Qt::CaseInsensitive).replace(QString("[/%1]").arg(from), QString("</%1>").arg(to), Qt::CaseInsensitive);

	str.replace(QRegularExpression(R"(\[img\](.*?)\[/img\])"), R"(<img src="\1"/>)");
	str.replace(QRegularExpression(R"(\[(URL|url)=(.*?)\](.*?)\[/(URL|url)\])"), R"(<a href="\2"/>\3</a>)");
	str.replace(QRegularExpression(R"(\[color=(.*?)\])"), R"(<font color="\1">)").replace("[/color]", "</font>");

	str.replace(QRegularExpression(R"(([^"])(https{0,1}:\/\/\S+?)([\s<]))"), R"(\1<a href="\2">\2</a>\3)");

	str.replace(QRegularExpression(R"(\[collapse collapsed title=(.*?)\])"), R"(<details><summary>\1</summary>)");
	str.replace(QRegularExpression(R"(\[/collapse])"), R"(</details>)");

	return str;
}

} // namespace HomeCompa::FliParser
