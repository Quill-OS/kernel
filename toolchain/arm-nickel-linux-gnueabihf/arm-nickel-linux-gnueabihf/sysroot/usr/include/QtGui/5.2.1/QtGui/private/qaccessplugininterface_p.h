/****************************************************************************
** Copyright (C) 2012 ACCESS, CO., LTD
** All rights reserved.
****************************************************************************/

#ifndef QACCESSPLUGININTERFACE_H
#define QACCESSPLUGININTERFACE_H

/* undef for X11 */
#undef Unsorted
#undef Bool

#include <QPluginLoader>
#include <QDir>
#include <QCoreApplication>

class QRawFontInterface;
class QRasterPaintEngineInterface;
class QFontEngineInterface;

class ACCESSPluginInterface
{

public:
    virtual ~ACCESSPluginInterface() = 0;
    virtual QRawFontInterface * rawFontPlugin() = 0;
    virtual QRasterPaintEngineInterface * rasterFontPlugin() = 0;
    virtual QFontEngineInterface * fontEnginePlugin() = 0;
};

inline ACCESSPluginInterface::~ACCESSPluginInterface() { }

QT_BEGIN_NAMESPACE

class ACCESSPlugin
{
    QStringList list;
    QDir pluginsDir;
    int i;
public:
    ACCESSPlugin() : i(-1) {
        pluginsDir = QDir(QCoreApplication::instance()->applicationDirPath());
#if defined(Q_OS_MAC)
        pluginsDir.cdUp();
        if (!pluginsDir.cd(QStringLiteral("Frameworks"))) {
            pluginsDir = QDir(QCoreApplication::instance()->applicationDirPath());
            pluginsDir.cd(QStringLiteral("plugins"));
        }
#else
        pluginsDir.cd(QStringLiteral("plugins"));
#endif
        list = pluginsDir.entryList(QDir::Files);
    }
    QObject * next() {
        i++;
        if (i >= list.size())
            return NULL;
        QPluginLoader pluginLoader(pluginsDir.absoluteFilePath(list.at(i)));
        return pluginLoader.instance();
    }
};


Q_DECLARE_INTERFACE(ACCESSPluginInterface,
                    "com.access_company.qt.Plugin.ACCESSPluginInterface/1.0");

QT_END_NAMESPACE

#endif
