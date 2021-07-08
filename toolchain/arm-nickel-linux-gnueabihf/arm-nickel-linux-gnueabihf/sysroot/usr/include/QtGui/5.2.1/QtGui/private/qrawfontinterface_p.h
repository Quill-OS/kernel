/****************************************************************************
** Copyright (C) 2012 ACCESS, CO., LTD
** All rights reserved.
****************************************************************************/

#ifndef QRAWFONTINTERFACE_H
#define QRAWFONTINTERFACE_H

#include <QObject>
#include <QSharedData>
#include "qfont.h"
#include "qrawfont_p.h"

class QRawFontInterface
{
public:
    virtual ~QRawFontInterface() {}
    virtual bool hasVerticalGlyphs(QExplicitlySharedDataPointer<QRawFontPrivate> d) = 0;
    virtual quint32 substituteWithVerticalVariants(QExplicitlySharedDataPointer<QRawFontPrivate> d, quint32* glyphs, const unsigned length) = 0;
};

#endif
