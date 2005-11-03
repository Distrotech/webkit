/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#include <kdom/Namespace.h>
#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGDocumentImpl.h"
#include "SVGGElementImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGUseElementImpl.h"
#include "SVGSymbolElementImpl.h"
#include "SVGAnimatedLengthImpl.h"
#include "SVGAnimatedStringImpl.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingStyle.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace KSVG;

SVGUseElementImpl::SVGUseElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentPtr *doc)
: SVGStyledElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl(), SVGTransformableImpl(), SVGURIReferenceImpl()
{
    m_x = m_y = m_width = m_height = 0;
}

SVGUseElementImpl::~SVGUseElementImpl()
{
    if(m_x)
        m_x->deref();
    if(m_y)
        m_y->deref();
    if(m_width)
        m_width->deref();
    if(m_height)
        m_height->deref();
}

SVGAnimatedLengthImpl *SVGUseElementImpl::x() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::y() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::width() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLengthImpl *SVGUseElementImpl::height() const
{
    return lazy_create<SVGAnimatedLengthImpl>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGUseElementImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    const KDOM::AtomicString& value = attr->value();
    
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else {
        if(SVGTestsImpl::parseAttribute(attr)) return;
        if(SVGLangSpaceImpl::parseAttribute(attr)) return;
        if(SVGExternalResourcesRequiredImpl::parseAttribute(attr)) return;
        if(SVGTransformableImpl::parseAttribute(attr)) return;
        if(SVGURIReferenceImpl::parseAttribute(attr)) return;

        SVGStyledElementImpl::parseAttribute(attr);
    }
}

void SVGUseElementImpl::closeRenderer()
{
    QString ref = KDOM::DOMString(href()->baseVal()).qstring();
    KDOM::DOMString targetId = SVGURIReferenceImpl::getTarget(ref);
    KDOM::ElementImpl *targetElement = ownerDocument()->getElementById(targetId.impl());
    SVGElementImpl *target = svg_dynamic_cast(targetElement);
    if (!target)
    {
        getDocument()->addForwardReference(this);
        return;
    }

    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _w = width()->baseVal()->value(), _h = height()->baseVal()->value();
    
    KDOM::DOMString wString = QString::number(_w);
    KDOM::DOMString hString = QString::number(_h);
    
    int exceptioncode;
    QString trans = QString::fromLatin1("translate(%1, %2)").arg(_x).arg(_y);
    if(target->hasTagName(SVGNames::symbolTag))
    {
        SVGElementImpl *dummy = new SVGSVGElementImpl(SVGNames::svgTag, docPtr());
        if(_w > 0)
            dummy->setAttribute(SVGNames::widthAttr, wString.impl());
        if(_h > 0)
            dummy->setAttribute(SVGNames::heightAttr, hString.impl());
        
        SVGSymbolElementImpl *symbol = static_cast<SVGSymbolElementImpl *>(target);
        if (symbol->hasAttribute(SVGNames::viewboxAttr)) {
            const KDOM::AtomicString& symbolViewBox = symbol->getAttribute(SVGNames::viewboxAttr);
            dummy->setAttribute(SVGNames::viewboxAttr, symbolViewBox);
        }
        target->cloneChildNodes(dummy);

        SVGElementImpl *dummy2 = new SVGDummyElementImpl(SVGNames::gTag, docPtr());
        dummy2->setAttribute(SVGNames::transformAttr, KDOM::DOMString(trans));
        
        appendChild(dummy2, exceptioncode);
        dummy2->appendChild(dummy, exceptioncode);
    }
    else if(target->hasTagName(SVGNames::svgTag))
    {
        SVGDummyElementImpl *dummy = new SVGDummyElementImpl(SVGNames::gTag, docPtr());
        dummy->setAttribute(SVGNames::transformAttr, KDOM::DOMString(trans));
        
        SVGElementImpl *root = static_cast<SVGElementImpl *>(target->cloneNode(true));
        if(hasAttribute(SVGNames::widthAttr))
            root->setAttribute(SVGNames::widthAttr, wString.impl());
            
        if(hasAttribute(SVGNames::heightAttr))
            root->setAttribute(SVGNames::heightAttr, hString.impl());
            
        appendChild(dummy, exceptioncode);
        dummy->appendChild(root, exceptioncode);
    }
    else
    {
        SVGDummyElementImpl *dummy = new SVGDummyElementImpl(SVGNames::gTag, docPtr());
        dummy->setAttribute(SVGNames::transformAttr, trans);
        
        KDOM::NodeImpl *root = target->cloneNode(true);
        
        appendChild(dummy, exceptioncode);
        dummy->appendChild(root, exceptioncode);
    }

    SVGElementImpl::closeRenderer();
}

bool SVGUseElementImpl::hasChildNodes() const
{
    return false;
}

KCanvasItem *SVGUseElementImpl::createCanvasItem(KCanvas *canvas, KRenderingStyle *style) const
{
    return KCanvasCreator::self()->createContainer(canvas, style);
}

// vim:ts=4:noet
