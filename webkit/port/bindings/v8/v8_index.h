// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INDEX_H__
#define V8_INDEX_H__

#include <v8.h>
#include "PlatformString.h"  // for WebCore::String

namespace WebCore {

class Node;
class XMLHttpRequest;

typedef v8::Persistent<v8::FunctionTemplate> (*FunctionTemplateFactory)();


#define NODE_WRAPPER_TYPES(V)                                           \
  V(ATTR, Attr)                                                         \
  V(CHARACTERDATA, CharacterData)                                       \
  V(CDATASECTION, CDATASection)                                         \
  V(COMMENT, Comment)                                                   \
  V(DOCUMENT, Document)                                                 \
  V(DOCUMENTFRAGMENT, DocumentFragment)                                 \
  V(DOCUMENTTYPE, DocumentType)                                         \
  V(ELEMENT, Element)                                                   \
  V(ENTITY, Entity)                                                     \
  V(ENTITYREFERENCE, EntityReference)                                   \
  V(EVENTTARGETNODE, EventTargetNode)                                   \
  V(HTMLDOCUMENT, HTMLDocument)                                         \
  V(NODE, Node)                                                         \
  V(NOTATION, Notation)                                                 \
  V(PROCESSINGINSTRUCTION, ProcessingInstruction)                       \
  V(TEXT, Text)

#define HTMLELEMENT_TYPES(V)                                            \
  V(HTMLANCHORELEMENT, HTMLAnchorElement)                               \
  V(HTMLAPPLETELEMENT, HTMLAppletElement)                               \
  V(HTMLAREAELEMENT, HTMLAreaElement)                                   \
  V(HTMLBASEELEMENT, HTMLBaseElement)                                   \
  V(HTMLBASEFONTELEMENT, HTMLBaseFontElement)                           \
  V(HTMLBLOCKQUOTEELEMENT, HTMLBlockquoteElement)                       \
  V(HTMLBODYELEMENT, HTMLBodyElement)                                   \
  V(HTMLBRELEMENT, HTMLBRElement)                                       \
  V(HTMLBUTTONELEMENT, HTMLButtonElement)                               \
  V(HTMLCANVASELEMENT, HTMLCanvasElement)                               \
  V(HTMLDIRECTORYELEMENT, HTMLDirectoryElement)                         \
  V(HTMLDIVELEMENT, HTMLDivElement)                                     \
  V(HTMLDLISTELEMENT, HTMLDListElement)                                 \
  V(HTMLEMBEDELEMENT, HTMLEmbedElement)                                 \
  V(HTMLFIELDSETELEMENT, HTMLFieldSetElement)                           \
  V(HTMLFONTELEMENT, HTMLFontElement)                                   \
  V(HTMLFORMELEMENT, HTMLFormElement)                                   \
  V(HTMLFRAMEELEMENT, HTMLFrameElement)                                 \
  V(HTMLFRAMESETELEMENT, HTMLFrameSetElement)                           \
  V(HTMLHEADINGELEMENT, HTMLHeadingElement)                             \
  V(HTMLHEADELEMENT, HTMLHeadElement)                                   \
  V(HTMLHRELEMENT, HTMLHRElement)                                       \
  V(HTMLHTMLELEMENT, HTMLHtmlElement)                                   \
  V(HTMLIFRAMEELEMENT, HTMLIFrameElement)                               \
  V(HTMLIMAGEELEMENT, HTMLImageElement)                                 \
  V(HTMLINPUTELEMENT, HTMLInputElement)                                 \
  V(HTMLSELECTIONINPUTELEMENT, HTMLSelectionInputElement)               \
  V(HTMLISINDEXELEMENT, HTMLIsIndexElement)                             \
  V(HTMLLABELELEMENT, HTMLLabelElement)                                 \
  V(HTMLLEGENDELEMENT, HTMLLegendElement)                               \
  V(HTMLLIELEMENT, HTMLLIElement)                                       \
  V(HTMLLINKELEMENT, HTMLLinkElement)                                   \
  V(HTMLMAPELEMENT, HTMLMapElement)                                     \
  V(HTMLMARQUEEELEMENT, HTMLMarqueeElement)                             \
  V(HTMLMENUELEMENT, HTMLMenuElement)                                   \
  V(HTMLMETAELEMENT, HTMLMetaElement)                                   \
  V(HTMLMODELEMENT, HTMLModElement)                                     \
  V(HTMLOBJECTELEMENT, HTMLObjectElement)                               \
  V(HTMLOLISTELEMENT, HTMLOListElement)                                 \
  V(HTMLOPTGROUPELEMENT, HTMLOptGroupElement)                           \
  V(HTMLOPTIONELEMENT, HTMLOptionElement)                               \
  V(HTMLPARAGRAPHELEMENT, HTMLParagraphElement)                         \
  V(HTMLPARAMELEMENT, HTMLParamElement)                                 \
  V(HTMLPREELEMENT, HTMLPreElement)                                     \
  V(HTMLQUOTEELEMENT, HTMLQuoteElement)                                 \
  V(HTMLSCRIPTELEMENT, HTMLScriptElement)                               \
  V(HTMLSELECTELEMENT, HTMLSelectElement)                               \
  V(HTMLSTYLEELEMENT, HTMLStyleElement)                                 \
  V(HTMLTABLECAPTIONELEMENT, HTMLTableCaptionElement)                   \
  V(HTMLTABLECOLELEMENT, HTMLTableColElement)                           \
  V(HTMLTABLEELEMENT, HTMLTableElement)                                 \
  V(HTMLTABLESECTIONELEMENT, HTMLTableSectionElement)                   \
  V(HTMLTABLECELLELEMENT, HTMLTableCellElement)                         \
  V(HTMLTABLEROWELEMENT, HTMLTableRowElement)                           \
  V(HTMLTEXTAREAELEMENT, HTMLTextAreaElement)                           \
  V(HTMLTITLEELEMENT, HTMLTitleElement)                                 \
  V(HTMLULISTELEMENT, HTMLUListElement)                                 \
  V(HTMLELEMENT, HTMLElement)

#if ENABLE(SVG_ANIMATION)
#define SVG_ANIMATION_ELEMENT_TYPES(V)                                  \
  V(SVGANIMATECOLORELEMENT, SVGAnimateColorElement)                     \
  V(SVGANIMATEELEMENT, SVGAnimateElement)                               \
  V(SVGANIMATETRANSFORMELEMENT, SVGAnimateTransformElement)             \
  V(SVGANIMATIONELEMENT, SVGAnimationElement)                           \
  V(SVGSETELEMENT, SVGSetElement)
#else
#define SVG_ANIMATION_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FILTERS)
#define SVG_FILTERS_ELEMENT_TYPES(V)                                     \
  V(SVGCOMPONENTTRANSFERFUNCTIONELEMENT, SVGComponentTransferFunctionElement)\
  V(SVGFEBLENDELEMENT, SVGFEBlendElement)                               \
  V(SVGFECOLORMATRIXELEMENT, SVGFEColorMatrixElement)                   \
  V(SVGFECOMPONENTTRANSFERELEMENT, SVGFEComponentTransferElement)       \
  V(SVGFECOMPOSITEELEMENT, SVGFECompositeElement)                       \
  V(SVGFEDIFFUSELIGHTINGELEMENT, SVGFEDiffuseLightingElement)           \
  V(SVGFEDISPLACEMENTMAPELEMENT, SVGFEDisplacementMapElement)           \
  V(SVGFEDISTANTLIGHTELEMENT, SVGFEDistantLightElement)                 \
  V(SVGFEFLOODELEMENT, SVGFEFloodElement)                               \
  V(SVGFEFUNCAELEMENT, SVGFEFuncAElement)                               \
  V(SVGFEFUNCBELEMENT, SVGFEFuncBElement)                               \
  V(SVGFEFUNCGELEMENT, SVGFEFuncGElement)                               \
  V(SVGFEFUNCRELEMENT, SVGFEFuncRElement)                               \
  V(SVGFEGAUSSIANBLURELEMENT, SVGFEGaussianBlurElement)                 \
  V(SVGFEIMAGEELEMENT, SVGFEImageElement)                               \
  V(SVGFEMERGEELEMENT, SVGFEMergeElement)                               \
  V(SVGFEMERGENODEELEMENT, SVGFEMergeNodeElement)                       \
  V(SVGFEOFFSETELEMENT, SVGFEOffsetElement)                             \
  V(SVGFEPOINTLIGHTELEMENT, SVGFEPointLightElement)                     \
  V(SVGFESPECULARLIGHTINGELEMENT, SVGFESpecularLightingElement)         \
  V(SVGFESPOTLIGHTELEMENT, SVGFESpotLightElement)                       \
  V(SVGFETILEELEMENT, SVGFETileElement)                                 \
  V(SVGFETURBULENCEELEMENT, SVGFETurbulenceElement)                     \
  V(SVGFILTERELEMENT, SVGFilterElement)
#else
#define SVG_FILTERS_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FONTS)
#define SVG_FONTS_ELEMENT_TYPES(V)                                      \
  V(SVGDEFINITIONSRCELEMENT, SVGDefinitionSrcElement)                   \
  V(SVGFONTFACEELEMENT, SVGFontFaceElement)                             \
  V(SVGFONTFACEFORMATELEMENT, SVGFontFaceFormatElement)                 \
  V(SVGFONTFACENAMEELEMENT, SVGFontFaceNameElement)                     \
  V(SVGFONTFACESRCELEMENT, SVGFontFaceSrcElement)                       \
  V(SVGFONTFACEURIELEMENT, SVGFontFaceUriElement)
#else
#define SVG_FONTS_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_FOREIGN_OBJECT)
#define SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)                             \
  V(SVGFOREIGNOBJECTELEMENT, SVGForeignObjectElement)
#else
#define SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG_USE)
#define SVG_USE_ELEMENT_TYPES(V)                                        \
  V(SVGUSEELEMENT, SVGUseElement)
#else
#define SVG_USE_ELEMENT_TYPES(V)
#endif

#if ENABLE(SVG)
#define SVGELEMENT_TYPES(V)                                             \
  SVG_ANIMATION_ELEMENT_TYPES(V)                                        \
  SVG_FILTERS_ELEMENT_TYPES(V)                                          \
  SVG_FONTS_ELEMENT_TYPES(V)                                            \
  SVG_FOREIGN_OBJECT_ELEMENT_TYPES(V)                                   \
  SVG_USE_ELEMENT_TYPES(V)                                              \
  V(SVGAELEMENT, SVGAElement)                                           \
  V(SVGALTGLYPHELEMENT, SVGAltGlyphElement)                             \
  V(SVGCIRCLEELEMENT, SVGCircleElement)                                 \
  V(SVGCLIPPATHELEMENT, SVGClipPathElement)                             \
  V(SVGCURSORELEMENT, SVGCursorElement)                                 \
  V(SVGDEFSELEMENT, SVGDefsElement)                                     \
  V(SVGDESCELEMENT, SVGDescElement)                                     \
  V(SVGELLIPSEELEMENT, SVGEllipseElement)                               \
  V(SVGGELEMENT, SVGGElement)                                           \
  V(SVGGLYPHELEMENT, SVGGlyphElement)                                           \
  V(SVGGRADIENTELEMENT, SVGGradientElement)                             \
  V(SVGIMAGEELEMENT, SVGImageElement)                                   \
  V(SVGLINEARGRADIENTELEMENT, SVGLinearGradientElement)                 \
  V(SVGLINEELEMENT, SVGLineElement)                                     \
  V(SVGMARKERELEMENT, SVGMarkerElement)                                 \
  V(SVGMASKELEMENT, SVGMaskElement)                                     \
  V(SVGMETADATAELEMENT, SVGMetadataElement)                             \
  V(SVGPATHELEMENT, SVGPathElement)                                     \
  V(SVGPATTERNELEMENT, SVGPatternElement)                               \
  V(SVGPOLYGONELEMENT, SVGPolygonElement)                               \
  V(SVGPOLYLINEELEMENT, SVGPolylineElement)                             \
  V(SVGRADIALGRADIENTELEMENT, SVGRadialGradientElement)                 \
  V(SVGRECTELEMENT, SVGRectElement)                                     \
  V(SVGSCRIPTELEMENT, SVGScriptElement)                                 \
  V(SVGSTOPELEMENT, SVGStopElement)                                     \
  V(SVGSTYLEELEMENT, SVGStyleElement)                                   \
  V(SVGSVGELEMENT, SVGSVGElement)                                       \
  V(SVGSWITCHELEMENT, SVGSwitchElement)                                 \
  V(SVGSYMBOLELEMENT, SVGSymbolElement)                                 \
  V(SVGTEXTCONTENTELEMENT, SVGTextContentElement)                       \
  V(SVGTEXTELEMENT, SVGTextElement)                                     \
  V(SVGTEXTPATHELEMENT, SVGTextPathElement)                             \
  V(SVGTEXTPOSITIONINGELEMENT, SVGTextPositioningElement)               \
  V(SVGTITLEELEMENT, SVGTitleElement)                                   \
  V(SVGTREFELEMENT, SVGTRefElement)                                     \
  V(SVGTSPANELEMENT, SVGTSpanElement)                                   \
  V(SVGVIEWELEMENT, SVGViewElement)                                     \
  V(SVGELEMENT, SVGElement)
#endif


#define NONNODE_WRAPPER_TYPES(V)                                        \
  V(BARINFO, BarInfo)                                                   \
  V(CANVASGRADIENT, CanvasGradient)                                     \
  V(CANVASPATTERN, CanvasPattern)                                       \
  V(CANVASPIXELARRAY, CanvasPixelArray)                                 \
  V(CANVASRENDERINGCONTEXT2D, CanvasRenderingContext2D)                 \
  V(CLIPBOARD, Clipboard)                                               \
  V(CONSOLE, Console)                                                   \
  V(COUNTER, Counter)                                                   \
  V(CSSCHARSETRULE, CSSCharsetRule)                                     \
  V(CSSFONTFACERULE, CSSFontFaceRule)                                   \
  V(CSSIMPORTRULE, CSSImportRule)                                       \
  V(CSSMEDIARULE, CSSMediaRule)                                         \
  V(CSSPAGERULE, CSSPageRule)                                           \
  V(CSSPRIMITIVEVALUE, CSSPrimitiveValue)                               \
  V(CSSRULE, CSSRule)                                                   \
  V(CSSRULELIST, CSSRuleList)                                           \
  V(CSSSTYLEDECLARATION, CSSStyleDeclaration)                           \
  V(CSSSTYLERULE, CSSStyleRule)                                         \
  V(CSSSTYLESHEET, CSSStyleSheet)                                       \
  V(CSSVALUE, CSSValue)                                                 \
  V(CSSVALUELIST, CSSValueList)                                         \
  V(CSSVARIABLESDECLARATION, CSSVariablesDeclaration)                   \
  V(CSSVARIABLESRULE, CSSVariablesRule)                                 \
  V(DOMCOREEXCEPTION, DOMCoreException)                                 \
  V(DOMIMPLEMENTATION, DOMImplementation)                               \
  V(DOMPARSER, DOMParser)                                               \
  V(DOMSELECTION, DOMSelection)                                         \
  V(DOMWINDOW, DOMWindow)                                               \
  V(EVENT, Event)                                                       \
  V(EVENTEXCEPTION, EventException)                                     \
  V(FILE, File)                                                         \
  V(FILELIST, FileList)                                                 \
  V(HISTORY, History)                                                   \
  V(UNDETECTABLEHTMLCOLLECTION, UndetectableHTMLCollection)             \
  V(HTMLCOLLECTION, HTMLCollection)                                     \
  V(HTMLOPTIONSCOLLECTION, HTMLOptionsCollection)                       \
  V(IMAGEDATA, ImageData)                                               \
  V(INSPECTORCONTROLLER, InspectorController)                           \
  V(KEYBOARDEVENT, KeyboardEvent)                                       \
  V(LOCATION, Location)                                                 \
  V(MEDIALIST, MediaList)                                               \
  V(MESSAGEEVENT, MessageEvent)                                         \
  V(MIMETYPE, MimeType)                                                 \
  V(MIMETYPEARRAY, MimeTypeArray)                                       \
  V(MOUSEEVENT, MouseEvent)                                             \
  V(MUTATIONEVENT, MutationEvent)                                       \
  V(NAMEDNODEMAP, NamedNodeMap)                                         \
  V(NAVIGATOR, Navigator)                                               \
  V(NODEFILTER, NodeFilter)                                             \
  V(NODEITERATOR, NodeIterator)                                         \
  V(NODELIST, NodeList)                                                 \
  V(NSRESOLVER, NSResolver)                                             \
  V(OVERFLOWEVENT, OverflowEvent)                                       \
  V(PLUGIN, Plugin)                                                     \
  V(PLUGINARRAY, PluginArray)                                           \
  V(PROGRESSEVENT, ProgressEvent)                                       \
  V(RANGE, Range)                                                       \
  V(RANGEEXCEPTION, RangeException)                                     \
  V(RECT, Rect)                                                         \
  V(RGBCOLOR, RGBColor)                                                 \
  V(SCREEN, Screen)                                                     \
  V(STYLESHEET, StyleSheet)                                             \
  V(STYLESHEETLIST, StyleSheetList)                                     \
  V(TEXTEVENT, TextEvent)                                               \
  V(TEXTMETRICS, TextMetrics)                                           \
  V(TREEWALKER, TreeWalker)                                             \
  V(UIEVENT, UIEvent)                                                   \
  V(WEBKITANIMATIONEVENT, WebKitAnimationEvent)                         \
  V(WEBKITCSSKEYFRAMERULE, WebKitCSSKeyframeRule)                       \
  V(WEBKITCSSKEYFRAMESRULE, WebKitCSSKeyframesRule)                     \
  V(WEBKITCSSTRANSFORMVALUE, WebKitCSSTransformValue)                   \
  V(WEBKITTRANSITIONEVENT, WebKitTransitionEvent)                       \
  V(WHEELEVENT, WheelEvent)                                             \
  V(XMLHTTPREQUEST, XMLHttpRequest)                                     \
  V(XMLHTTPREQUESTUPLOAD, XMLHttpRequestUpload)                         \
  V(XMLHTTPREQUESTEXCEPTION, XMLHttpRequestException)                   \
  V(XMLHTTPREQUESTPROGRESSEVENT, XMLHttpRequestProgressEvent)           \
  V(XMLSERIALIZER, XMLSerializer)                                       \
  V(XPATHEVALUATOR, XPathEvaluator)                                     \
  V(XPATHEXCEPTION, XPathException)                                     \
  V(XPATHEXPRESSION, XPathExpression)                                   \
  V(XPATHNSRESOLVER, XPathNSResolver)                                   \
  V(XPATHRESULT, XPathResult)                                           \
  V(XSLTPROCESSOR, XSLTProcessor)

#if ENABLE(SVG)
#define SVGNODE_WRAPPER_TYPES(V)                                        \
  V(SVGDOCUMENT, SVGDocument)

#define SVGNONNODE_WRAPPER_TYPES(V)                                     \
  V(SVGANGLE, SVGAngle)                                                 \
  V(SVGANIMATEDANGLE, SVGAnimatedAngle)                                 \
  V(SVGANIMATEDBOOLEAN, SVGAnimatedBoolean)                             \
  V(SVGANIMATEDENUMERATION, SVGAnimatedEnumeration)                     \
  V(SVGANIMATEDINTEGER, SVGAnimatedInteger)                             \
  V(SVGANIMATEDLENGTH, SVGAnimatedLength)                               \
  V(SVGANIMATEDLENGTHLIST, SVGAnimatedLengthList)                       \
  V(SVGANIMATEDNUMBER, SVGAnimatedNumber)                               \
  V(SVGANIMATEDNUMBERLIST, SVGAnimatedNumberList)                       \
  V(SVGANIMATEDPOINTS, SVGAnimatedPoints)                               \
  V(SVGANIMATEDPRESERVEASPECTRATIO, SVGAnimatedPreserveAspectRatio)     \
  V(SVGANIMATEDRECT, SVGAnimatedRect)                                   \
  V(SVGANIMATEDSTRING, SVGAnimatedString)                               \
  V(SVGANIMATEDTRANSFORMLIST, SVGAnimatedTransformList)                 \
  V(SVGCOLOR, SVGColor)                                                 \
  V(SVGELEMENTINSTANCE, SVGElementInstance)                             \
  V(SVGELEMENTINSTANCELIST, SVGElementInstanceList)                     \
  V(SVGEXCEPTION, SVGException)                                         \
  V(SVGLENGTH, SVGLength)                                               \
  V(SVGLENGTHLIST, SVGLengthList)                                       \
  V(SVGMATRIX, SVGMatrix)                                               \
  V(SVGNUMBER, SVGNumber)                                               \
  V(SVGNUMBERLIST, SVGNumberList)                                       \
  V(SVGPAINT, SVGPaint)                                                 \
  V(SVGPATHSEG, SVGPathSeg)                                             \
  V(SVGPATHSEGARCABS, SVGPathSegArcAbs)                                 \
  V(SVGPATHSEGARCREL, SVGPathSegArcRel)                                 \
  V(SVGPATHSEGCLOSEPATH, SVGPathSegClosePath)                           \
  V(SVGPATHSEGCURVETOCUBICABS, SVGPathSegCurvetoCubicAbs)               \
  V(SVGPATHSEGCURVETOCUBICREL, SVGPathSegCurvetoCubicRel)               \
  V(SVGPATHSEGCURVETOCUBICSMOOTHABS, SVGPathSegCurvetoCubicSmoothAbs)   \
  V(SVGPATHSEGCURVETOCUBICSMOOTHREL, SVGPathSegCurvetoCubicSmoothRel)   \
  V(SVGPATHSEGCURVETOQUADRATICABS, SVGPathSegCurvetoQuadraticAbs)       \
  V(SVGPATHSEGCURVETOQUADRATICREL, SVGPathSegCurvetoQuadraticRel)       \
  V(SVGPATHSEGCURVETOQUADRATICSMOOTHABS, SVGPathSegCurvetoQuadraticSmoothAbs)\
  V(SVGPATHSEGCURVETOQUADRATICSMOOTHREL, SVGPathSegCurvetoQuadraticSmoothRel)\
  V(SVGPATHSEGLINETOABS, SVGPathSegLinetoAbs)                           \
  V(SVGPATHSEGLINETOHORIZONTALABS, SVGPathSegLinetoHorizontalAbs)       \
  V(SVGPATHSEGLINETOHORIZONTALREL, SVGPathSegLinetoHorizontalRel)       \
  V(SVGPATHSEGLINETOREL, SVGPathSegLinetoRel)                           \
  V(SVGPATHSEGLINETOVERTICALABS, SVGPathSegLinetoVerticalAbs)           \
  V(SVGPATHSEGLINETOVERTICALREL, SVGPathSegLinetoVerticalRel)           \
  V(SVGPATHSEGLIST, SVGPathSegList)                                     \
  V(SVGPATHSEGMOVETOABS, SVGPathSegMovetoAbs)                           \
  V(SVGPATHSEGMOVETOREL, SVGPathSegMovetoRel)                           \
  V(SVGPOINT, SVGPoint)                                                 \
  V(SVGPOINTLIST, SVGPointList)                                         \
  V(SVGPRESERVEASPECTRATIO, SVGPreserveAspectRatio)                     \
  V(SVGRECT, SVGRect)                                                   \
  V(SVGRENDERINGINTENT, SVGRenderingIntent)                             \
  V(SVGSTRINGLIST, SVGStringList)                                       \
  V(SVGTRANSFORM, SVGTransform)                                         \
  V(SVGTRANSFORMLIST, SVGTransformList)                                 \
  V(SVGUNITTYPES, SVGUnitTypes)                                         \
  V(SVGURIREFERENCE, SVGURIReference)                                   \
  V(SVGZOOMEVENT, SVGZoomEvent)
#endif

// EVENTTARGET, EVENTLISTENER, and NPOBJECT do not have V8 wrappers.
#define NO_WRAPPER_TYPES(V)                                             \
  V(EVENTTARGET, EventTarget)                                           \
  V(EVENTLISTENER, EventListener)                                       \
  V(NPOBJECT, NPObject)

#if ENABLE(SVG)
#define WRAPPER_TYPES(V)                                \
    NODE_WRAPPER_TYPES(V)                               \
    HTMLELEMENT_TYPES(V)                                \
    NONNODE_WRAPPER_TYPES(V)                            \
    SVGNODE_WRAPPER_TYPES(V)                            \
    SVGELEMENT_TYPES(V)                                 \
    SVGNONNODE_WRAPPER_TYPES(V)
#else
#define WRAPPER_TYPES(V)                                \
    NODE_WRAPPER_TYPES(V)                               \
    HTMLELEMENT_TYPES(V)                                \
    NONNODE_WRAPPER_TYPES(V)
#endif

#define ALL_WRAPPER_TYPES(V)                            \
  WRAPPER_TYPES(V)                                      \
  NO_WRAPPER_TYPES(V)

class V8ClassIndex {
 public:
  // Type must start at non-negative numbers. See ToInt, FromInt.
  enum V8WrapperType {
    INVALID_CLASS_INDEX = 0,
#define DEFINE_ENUM(name, type) name,
ALL_WRAPPER_TYPES(DEFINE_ENUM)
#undef DEFINE_ENUM
    CLASSINDEX_END,
  };

  static int ToInt(V8WrapperType type) { return static_cast<int>(type); }

  static V8WrapperType FromInt(int v) {
    ASSERT(INVALID_CLASS_INDEX <= v && v < CLASSINDEX_END);
    return static_cast<V8WrapperType>(v);
  }

  static FunctionTemplateFactory GetFactory(V8WrapperType type);
  // Returns a field to be used as cache for the template for the given type
  static v8::Persistent<v8::FunctionTemplate>* GetCache(V8WrapperType type);
};
}

#endif  // V8_INDEX_H__

