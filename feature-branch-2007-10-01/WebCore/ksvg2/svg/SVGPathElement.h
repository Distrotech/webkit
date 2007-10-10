/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGPathElement_h
#define SVGPathElement_h

#if ENABLE(SVG)

#include "SVGAnimatedPathData.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
//#include "SVGParserUtilities.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore
{
    class SVGPathSeg;
    class SVGPathSegArcAbs;
    class SVGPathSegArcRel;
    class SVGPathSegClosePath;
    class SVGPathSegLinetoAbs;
    class SVGPathSegLinetoRel;
    class SVGPathSegMovetoAbs;
    class SVGPathSegMovetoRel;
    class SVGPathSegCurvetoCubicAbs;
    class SVGPathSegCurvetoCubicRel;
    class SVGPathSegLinetoVerticalAbs;
    class SVGPathSegLinetoVerticalRel;
    class SVGPathSegLinetoHorizontalAbs;
    class SVGPathSegLinetoHorizontalRel;
    class SVGPathSegCurvetoQuadraticAbs;
    class SVGPathSegCurvetoQuadraticRel;
    class SVGPathSegCurvetoCubicSmoothAbs;
    class SVGPathSegCurvetoCubicSmoothRel;
    class SVGPathSegCurvetoQuadraticSmoothAbs;
    class SVGPathSegCurvetoQuadraticSmoothRel;
    class SVGPathElement : public SVGStyledTransformableElement,
                           public SVGTests,
                           public SVGLangSpace,
                           public SVGExternalResourcesRequired,
                           public SVGAnimatedPathData
    {
    public:
        SVGPathElement(const QualifiedName&, Document*);
        virtual ~SVGPathElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }
        double getTotalLength();
        FloatPoint getPointAtLength(double distance);
        unsigned long getPathSegAtLength(double distance);

        static SVGPathSegClosePath* createSVGPathSegClosePath();
        static SVGPathSegMovetoAbs* createSVGPathSegMovetoAbs(double x, double y);
        static SVGPathSegMovetoRel* createSVGPathSegMovetoRel(double x, double y);
        static SVGPathSegLinetoAbs* createSVGPathSegLinetoAbs(double x, double y);
        static SVGPathSegLinetoRel* createSVGPathSegLinetoRel(double x, double y);
        static SVGPathSegCurvetoCubicAbs* createSVGPathSegCurvetoCubicAbs(double x, double y, double x1, double y1, double x2, double y2);
        static SVGPathSegCurvetoCubicRel* createSVGPathSegCurvetoCubicRel(double x, double y, double x1, double y1, double x2, double y2);
        static SVGPathSegCurvetoQuadraticAbs* createSVGPathSegCurvetoQuadraticAbs(double x, double y, double x1, double y1);
        static SVGPathSegCurvetoQuadraticRel* createSVGPathSegCurvetoQuadraticRel(double x, double y, double x1, double y1);
        static SVGPathSegArcAbs* createSVGPathSegArcAbs(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag);
        static SVGPathSegArcRel* createSVGPathSegArcRel(double x, double y, double r1, double r2, double angle, bool largeArcFlag, bool sweepFlag);
        static SVGPathSegLinetoHorizontalAbs* createSVGPathSegLinetoHorizontalAbs(double x);
        static SVGPathSegLinetoHorizontalRel* createSVGPathSegLinetoHorizontalRel(double x);
        static SVGPathSegLinetoVerticalAbs* createSVGPathSegLinetoVerticalAbs(double y);
        static SVGPathSegLinetoVerticalRel* createSVGPathSegLinetoVerticalRel(double y);
        static SVGPathSegCurvetoCubicSmoothAbs* createSVGPathSegCurvetoCubicSmoothAbs(double x, double y, double x2, double y2);
        static SVGPathSegCurvetoCubicSmoothRel* createSVGPathSegCurvetoCubicSmoothRel(double x, double y, double x2, double y2);
        static SVGPathSegCurvetoQuadraticSmoothAbs* createSVGPathSegCurvetoQuadraticSmoothAbs(double x, double y);
        static SVGPathSegCurvetoQuadraticSmoothRel* createSVGPathSegCurvetoQuadraticSmoothRel(double x, double y);

        // Derived from: 'SVGAnimatedPathData'
        virtual SVGPathSegList* pathSegList() const;
        virtual SVGPathSegList* normalizedPathSegList() const;
        virtual SVGPathSegList* animatedPathSegList() const;
        virtual SVGPathSegList* animatedNormalizedPathSegList() const;

        virtual void parseMappedAttribute(MappedAttribute* attr);
        virtual void notifyAttributeChange() const;

        virtual Path toPathData() const;

        virtual bool supportsMarkers() const { return true; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        mutable RefPtr<SVGPathSegList> m_pathSegList;

        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGPathElement, double, double, PathLength, pathLength)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
