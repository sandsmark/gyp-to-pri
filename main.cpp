// gyp-to-pri
// Copyright (C) 2016  The Magma Company AS
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QJsonDocument>
#include <QFileInfo>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

int main(int argc, char *argv[])
{
    if (argc < 2) {
        qWarning() << "Usage:" << argv[0] << "<folder/file.gyp>";
        qWarning() << "WARNING: This will potentially OVERWRITE existing .pro/.pri files.";
        return 1;
    }

    QFile gypFile(argv[1]);
    if (!gypFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Unable to open file" << gypFile.fileName();
        return 1;
    }

    QByteArray fileContents = gypFile.readAll();
    if (fileContents.isEmpty()) {
        qWarning() << gypFile.fileName() << "is empty";
        return 1;
    }

    qDebug() << "Parsing" << gypFile.fileName();

    // Strip out the comments which QJsonDocument barfs on
    QList<QByteArray> lines = fileContents.split('\n');
    fileContents.clear();
    foreach(QByteArray line, lines) {
        line = line.trimmed();
        if (line.startsWith('#')) {
            continue;
        }
        if (line.contains('#')) {
            line = line.split('#').first();
        }

        fileContents.append(line);
    }

    // Ugly hacks to turn the quasi-JSON that gyp uses into something
    // QJsonDocument understands
    fileContents.replace("\"", "\\\"");
    fileContents.replace('\'', '"');
    QString jsonString = QString::fromUtf8(fileContents);
    jsonString.replace(QRegularExpression(",\\s*]"), "]");
    jsonString.replace(QRegularExpression(",\\s*}"), "}");


    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);

    if (jsonDoc.isEmpty()) {
        qWarning() << gypFile.fileName() << "doesn't contain any valid JSON:" << error.errorString();
        return 1;
    }
    if (!jsonDoc.isObject()) {
        qWarning() << "Invalid gyp";
        return 1;
    }
    QJsonValue targetsValue = jsonDoc.object().value("targets");
    if (targetsValue.isNull() || !targetsValue.isArray()) {
        qWarning() << "invalid or missing 'targets'";
        return 1;
    }
    QJsonArray targets = targetsValue.toArray();
    QStringList targetList;
    foreach(const QJsonValue &value, targets) {
        if (!value.isObject()) {
            qWarning() << "invalid structure";
            continue;
        }
        QJsonObject target = value.toObject();
        QString targetName = target.value("target_name").toString();
        if (targetName.isEmpty()) {
            qWarning() << "target missing name";
            continue;
        }
        if (targetName.contains("/")) {
            qWarning() << "invalid target name:" << targetName;
            continue;
        }
        if (targetName.contains("test")) {
            qDebug() << "Skipping potential test target:" << targetName;
            continue;
        }
        QJsonArray sourceArray = target.value("sources").toArray();
        if (sourceArray.isEmpty()) {
            qWarning() << "sources list missing or invalid";
            continue;
        }
        QStringList cppFiles;
        QStringList headerFiles;
        foreach(const QJsonValue &sourceValue, sourceArray) {
            QString sourceFile = sourceValue.toString();
            if (sourceFile.isEmpty()) {
                qWarning() << "empty or invalid source";
                continue;
            }
            if (sourceFile.endsWith(".h")) {
                headerFiles.append(sourceFile);
            } else {
                cppFiles.append(sourceFile);
            }
        }
        QFile priFile(targetName + ".pri");
        priFile.open(QIODevice::WriteOnly);
        targetList.append(priFile.fileName());

        priFile.write("HEADERS += \\\n");
        foreach(const QString headerFile, headerFiles) {
            priFile.write("    $$SOURCE_DIR/" + headerFile.toUtf8() + " \\\n");
        }

        priFile.write("\n");

        priFile.write("SOURCES += \\\n");
        foreach(const QString cppFile, cppFiles) {
            priFile.write("    $$SOURCE_DIR/" + cppFile.toUtf8() + " \\\n");
        }
    }

    QFileInfo gypfileInfo(gypFile);
    QFile proFile(gypfileInfo.baseName().toUtf8() + ".pro");
    if (!proFile.open(QIODevice::WriteOnly)) {
        qWarning() << "error when opening .pro-file for writing";
        return 1;
    }

    proFile.write("# Automatically generated by gyp-to-pri, do not edit.\n"
            "# Put custom options in config.pri.\n\n");
    proFile.write("TARGET = " + gypfileInfo.baseName().toUtf8() + "\n\n");
    proFile.write("include(config.pri)\n\n");
    proFile.write("SOURCE_DIR = $$PWD/" + gypfileInfo.path().toUtf8() + "\n\n");

    QJsonObject targetDefaults = jsonDoc.object().value("target_defaults").toObject();
    if (!targetDefaults.isEmpty()) {
        QJsonArray includePaths = targetDefaults.value("include_dirs").toArray();
        if (!includePaths.isEmpty()) {
            proFile.write("INCLUDEPATH += \\\n");
            foreach(const QJsonValue &includePath, includePaths) {
                QByteArray path = includePath.toString().toUtf8();
                if (path.isEmpty() || path == "<(DEPTH)") {
                    continue;
                }
                proFile.write("    $$SOURCE_DIR/" + includePath.toString().toUtf8() + " \\\n");
            }
            proFile.write("\n");
        }
    }

    foreach(const QString &targetName, targetList) {
        proFile.write("include(" + targetName.toUtf8() + ")\n");
    }


    // Create config.pri if it doesn't exist
    QFile configFile("config.pri");
    if (!configFile.exists() && configFile.open(QIODevice::WriteOnly)) {
        configFile.write("TEMPLATE = lib\n");
        configFile.write("CONFIG += c++11 static\n");
        qDebug() << "created config.pri";
    }

    qDebug() << "Created" << proFile.fileName() << targetList;

    qDebug() << "Remember to edit config.pri to adjust the build to your satisfaction guaranteed.";

    return 0;
}
