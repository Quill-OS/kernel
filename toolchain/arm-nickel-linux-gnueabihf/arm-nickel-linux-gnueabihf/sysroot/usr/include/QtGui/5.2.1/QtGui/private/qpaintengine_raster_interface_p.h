/****************************************************************************
** Copyright (C) 2012 ACCESS, CO., LTD
** All rights reserved.
****************************************************************************/

#ifndef QRASTERPAINTENGINEINTERFACE_H
#define QRASTERPAINTENGINEINTERFACE_H

#include "private/qfixed_p.h"
#include "qtextureglyphcache_p.h"
#include "qpaintengine_raster_p.h"

class QRasterPaintEngineInterface
{
public:
    virtual ~QRasterPaintEngineInterface() {}
    virtual int drawCachedGlyphs1(const glyph_t *glyphs, QRasterPaintEngineState *s, QFontEngine *fontEngine, const QFixedPoint *positions, const QTextureGlyphCache::Coord &c, int margin, int i) = 0;
    virtual int drawCachedGlyphs2(QRasterPaintEngineState *s, const QFixedPoint *positions, const QTextureGlyphCache::Coord &c, int margin, int i, const QFixed offs) = 0;
};

QT_BEGIN_NAMESPACE

Q_DECLARE_INTERFACE(QRasterPaintEngineInterface,
                    "com.access_company.qt.Plugin.QRasterPaintEngineInterface/1.0");

QT_END_NAMESPACE

#endif
