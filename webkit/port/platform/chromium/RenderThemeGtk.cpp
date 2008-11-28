/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderThemeGtk.h"

#include "AffineTransform.h"
#include "ChromiumBridge.h"
#include "CSSValueKeywords.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "RenderObject.h"
#include "ScrollbarTheme.h"
#include "gtkdrawing.h"
#include "GdkSkia.h"

#include <gdk/gdk.h>

namespace {

enum PaddingType {
    TopPadding,
    RightPadding,
    BottomPadding,
    LeftPadding
};

const int kStyledMenuListInternalPadding[4] = { 1, 4, 1, 4 };

// The default variable-width font size.  We use this as the default font
// size for the "system font", and as a base size (which we then shrink) for
// form control fonts.
float DefaultFontSize = 16.0;

}  // namespace

namespace WebCore {

static Color makeColor(const GdkColor& c)
{
  return Color(makeRGB(c.red >> 8, c.green >> 8, c.blue >> 8));
}

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// TODO(ojan): Fix this!
// The only case where we know we don't match IE is for ANSI encodings. IE uses
// MS Shell Dlg there, which we render incorrectly at certain pixel sizes
// (e.g. 15px). So, for now we just use Arial.
static const char* defaultGUIFont(Document* document)
{
    return "Arial";
}

// Converts points to pixels.  One point is 1/72 of an inch.
static float pointsToPixels(float points)
{
    static float pixelsPerInch = 0.0f;
    if (!pixelsPerInch) {
        GdkScreen* screen = gdk_screen_get_default();
        // TODO(deanm):  I'm getting floating point values of ~75 and ~100,
        // and it's making my fonts look all wrong.  Figure this out.
        if (0 && screen) {
            pixelsPerInch = gdk_screen_get_resolution(screen);
        } else {
            // Match the default we set on Windows.
            pixelsPerInch = 96.0f;
        }
    }

    static const float POINTS_PER_INCH = 72.0f;
    return points / POINTS_PER_INCH * pixelsPerInch;
}

static void setSizeIfAuto(RenderStyle* style, const IntSize& size)
{
    if (style->width().isIntrinsicOrAuto())
        style->setWidth(Length(size.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(size.height(), Fixed));
}

static bool supportsFocus(ControlPart appearance)
{
    switch (appearance) {
        case PushButtonPart:
        case ButtonPart:
        case TextFieldPart:
        case TextAreaPart:
        case SearchFieldPart:
        case MenulistPart:
        case RadioPart:
        case CheckboxPart:
            return true;
        default:
            return false;
    }
}

static GtkTextDirection gtkTextDirection(TextDirection direction)
{
    switch (direction) {
    case RTL:
        return GTK_TEXT_DIR_RTL;
    case LTR:
        return GTK_TEXT_DIR_LTR;
    default:
        return GTK_TEXT_DIR_NONE;
    }
}
static void setMozState(RenderTheme* theme, GtkWidgetState* state, RenderObject* o)
{
    state->active = theme->isPressed(o);
    state->focused = theme->isFocused(o);
    state->inHover = theme->isHovered(o);
    // FIXME: Disabled does not always give the correct appearance for ReadOnly
    state->disabled = !theme->isEnabled(o) || theme->isReadOnlyControl(o);
    state->isDefault = false;
    state->canDefault = false;
    state->depressed = false;
}

static bool paintMozWidget(RenderTheme* theme, GtkThemeWidgetType type, RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    // Painting is disabled so just claim to have succeeded
    if (i.context->paintingDisabled())
        return false;

    GtkWidgetState mozState;
    setMozState(theme, &mozState, o);

    int flags;

    // We might want to make setting flags the caller's job at some point rather than doing it here.
    switch (type) {
        case MOZ_GTK_BUTTON:
            flags = GTK_RELIEF_NORMAL;
            break;
        case MOZ_GTK_CHECKBUTTON:
        case MOZ_GTK_RADIOBUTTON:
            flags = theme->isChecked(o);
            break;
        default:
            flags = 0;
            break;
    }

    PlatformContextSkia* pcs = i.context->platformContext();
    SkCanvas* canvas = pcs->canvas();
    if (!canvas)
        return false;

    GdkRectangle gdkRect;
    gdkRect.x = rect.x();
    gdkRect.y = rect.y();
    gdkRect.width = rect.width();
    gdkRect.height = rect.height();

    // getTotalClip returns the currently set clip region in device coordinates,
    // so we have to apply the current transform (actually we only support translations)
    // to get the page coordinates that our gtk widget rendering expects.
    // We invert it because we want to map from device coordinates to page coordinates.
    const SkIRect clip_region = canvas->getTotalClip().getBounds();
    AffineTransform ctm = i.context->getCTM().inverse();
    IntPoint pos = ctm.mapPoint(IntPoint(SkScalarRound(clip_region.fLeft), SkScalarRound(clip_region.fTop)));
    GdkRectangle gdkClipRect;
    gdkClipRect.x = pos.x();
    gdkClipRect.y = pos.y();
    gdkClipRect.width = SkScalarRound(clip_region.width());
    gdkClipRect.height = SkScalarRound(clip_region.height());

    GtkTextDirection direction = gtkTextDirection(o->style()->direction());

    return moz_gtk_widget_paint(type, pcs->gdk_skia(), &gdkRect, &gdkClipRect, &mozState, flags, direction) != MOZ_GTK_SUCCESS;
}

static void gtkStyleSetCallback(GtkWidget* widget, GtkStyle* previous, RenderTheme* renderTheme)
{
    // FIXME: Make sure this function doesn't get called many times for a single GTK+ style change signal.
    renderTheme->platformColorsDidChange();
}


// Implement WebCore::theme() for getting the global RenderTheme.
RenderTheme* theme()
{
    static RenderThemeGtk gtkTheme;
    return &gtkTheme;
}

RenderThemeGtk::RenderThemeGtk()
    : m_gtkWindow(0)
    , m_gtkContainer(0)
    , m_gtkEntry(0)
    , m_gtkTreeView(0)
{
}

bool RenderThemeGtk::supportsFocusRing(const RenderStyle* style) const
{
    return supportsFocus(style->appearance());
}

Color RenderThemeGtk::platformActiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->base[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::platformInactiveSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->base[GTK_STATE_ACTIVE]);
}

Color RenderThemeGtk::platformActiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->text[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::platformInactiveSelectionForegroundColor() const
{
    GtkWidget* widget = gtkEntry();
    return makeColor(widget->style->text[GTK_STATE_ACTIVE]);
}

double RenderThemeGtk::caretBlinkFrequency() const
{
    // Disable the blinking caret in layout test mode, as it introduces
    // a race condition for the pixel tests. http://b/1198440
    if (ChromiumBridge::layoutTestMode()) {
        // TODO(port): We need to disable this under linux, but returning 0
        // (like Windows does) sends gtk into an infinite expose loop. Do
        // something about this later.
    }

    GtkSettings* settings = gtk_settings_get_default();

    gboolean shouldBlink;
    gint time;

    g_object_get(settings, "gtk-cursor-blink", &shouldBlink, "gtk-cursor-blink-time", &time, NULL);

    if (!shouldBlink)
        return 0;

    return time / 2000.;
}

void RenderThemeGtk::systemFont(int propId, Document* document, FontDescription& fontDescription) const
{
    const char* faceName = 0;
    float fontSize = 0;
    // TODO(mmoss) see also webkit/port/rendering/RenderThemeWin.cpp
    switch (propId) {
        case CSSValueMenu:
        case CSSValueStatusBar:
        case CSSValueSmallCaption:
            // triggered by LayoutTests/fast/css/css2-system-fonts.html
            notImplemented();
            break;
        case CSSValueWebkitMiniControl:
        case CSSValueWebkitSmallControl:
        case CSSValueWebkitControl:
            faceName = defaultGUIFont(document);
            // Why 2 points smaller?  Because that's what Gecko does.
            fontSize = DefaultFontSize - pointsToPixels(2);
            break;
        default:
            faceName = defaultGUIFont(document);
            fontSize = DefaultFontSize;
    }

    // Only update if the size makes sense.
    if (fontSize > 0) {
        fontDescription.firstFamily().setFamily(faceName);
        fontDescription.setSpecifiedSize(fontSize);
        fontDescription.setIsAbsoluteSize(true);
        fontDescription.setGenericFamily(FontDescription::NoFamily);
        fontDescription.setWeight(FontWeightNormal);
        fontDescription.setItalic(false);
    }
}

int RenderThemeGtk::minimumMenuListSize(RenderStyle* style) const
{
    return 0;
}

bool RenderThemeGtk::paintCheckbox(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKBUTTON, o, i, rect);
}

void RenderThemeGtk::setCheckboxSize(RenderStyle* style) const
{
    // If the width and height are both specified, then we have nothing to do.
    if (!style->width().isIntrinsicOrAuto() && !style->height().isAuto())
        return;

    // FIXME:  A hard-coded size of 13 is used.  This is wrong but necessary for now.  It matches Firefox.
    // At different DPI settings on Windows, querying the theme gives you a larger size that accounts for
    // the higher DPI.  Until our entire engine honors a DPI setting other than 96, we can't rely on the theme's
    // metrics.
    const IntSize size(13, 13);
    setSizeIfAuto(style, size);
}

bool RenderThemeGtk::paintRadio(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_RADIOBUTTON, o, i, rect);
}

void RenderThemeGtk::setRadioSize(RenderStyle* style) const
{
    // Use same sizing for radio box as checkbox.
    setCheckboxSize(style);
}

bool RenderThemeGtk::paintButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_BUTTON, o, i, rect);
}

bool RenderThemeGtk::paintTextField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_ENTRY, o, i, rect);
}

bool RenderThemeGtk::paintTextArea(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintTextField(o, i, r);
}

bool RenderThemeGtk::paintSearchField(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintTextField(o, i, rect);
}

bool RenderThemeGtk::paintSearchFieldResultsDecoration(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
}

bool RenderThemeGtk::paintSearchFieldResultsButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_DROPDOWN_ARROW, o, i, rect);
}

bool RenderThemeGtk::paintSearchFieldCancelButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_CHECKMENUITEM, o, i, rect);
}

void RenderThemeGtk::adjustMenuListStyle(CSSStyleSelector* selector, RenderStyle* style, WebCore::Element* e) const
{
    // Height is locked to auto on all browsers.
    style->setLineHeight(RenderStyle::initialLineHeight());
}

bool RenderThemeGtk::paintMenuList(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& rect)
{
    return paintMozWidget(this, MOZ_GTK_DROPDOWN, o, i, rect);
}

void RenderThemeGtk::adjustMenuListButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustMenuListStyle(selector, style, e);
}

// Used to paint styled menulists (i.e. with a non-default border)
bool RenderThemeGtk::paintMenuListButton(RenderObject* o, const RenderObject::PaintInfo& i, const IntRect& r)
{
    return paintMenuList(o, i, r);
}

int RenderThemeGtk::popupInternalPaddingLeft(RenderStyle* style) const
{
    return menuListInternalPadding(style, LeftPadding);
}

int RenderThemeGtk::popupInternalPaddingRight(RenderStyle* style) const
{
    return menuListInternalPadding(style, RightPadding);
}

int RenderThemeGtk::popupInternalPaddingTop(RenderStyle* style) const
{
    return menuListInternalPadding(style, TopPadding);
}

int RenderThemeGtk::popupInternalPaddingBottom(RenderStyle* style) const
{
    return menuListInternalPadding(style, BottomPadding);
}

void RenderThemeGtk::adjustButtonInnerStyle(RenderStyle* style) const
{
    // This inner padding matches Firefox.
    style->setPaddingTop(Length(1, Fixed));
    style->setPaddingRight(Length(3, Fixed));
    style->setPaddingBottom(Length(1, Fixed));
    style->setPaddingLeft(Length(3, Fixed));
}

void RenderThemeGtk::adjustSearchFieldStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustTextFieldStyle(selector, style, e);
}

void RenderThemeGtk::adjustSearchFieldCancelButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

void RenderThemeGtk::adjustSearchFieldResultsDecorationStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    style->resetBorder();
    style->resetPadding();

    // FIXME: This should not be hard-coded.
    IntSize size = IntSize(14, 14);
    style->setWidth(Length(size.width(), Fixed));
    style->setHeight(Length(size.height(), Fixed));
}

void RenderThemeGtk::adjustSearchFieldResultsButtonStyle(CSSStyleSelector* selector, RenderStyle* style, Element* e) const
{
    adjustSearchFieldCancelButtonStyle(selector, style, e);
}

bool RenderThemeGtk::controlSupportsTints(const RenderObject* o) const
{
    return isEnabled(o);
}

Color RenderThemeGtk::activeListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->base[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::activeListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->text[GTK_STATE_SELECTED]);
}

Color RenderThemeGtk::inactiveListBoxSelectionBackgroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->base[GTK_STATE_ACTIVE]);
}

Color RenderThemeGtk::inactiveListBoxSelectionForegroundColor() const
{
    GtkWidget* widget = gtkTreeView();
    return makeColor(widget->style->text[GTK_STATE_ACTIVE]);
}

GtkWidget* RenderThemeGtk::gtkEntry() const
{
    if (m_gtkEntry)
        return m_gtkEntry;

    m_gtkEntry = gtk_entry_new();
    g_signal_connect(m_gtkEntry, "style-set", G_CALLBACK(gtkStyleSetCallback), theme());
    gtk_container_add(gtkContainer(), m_gtkEntry);
    gtk_widget_realize(m_gtkEntry);

    return m_gtkEntry;
}

GtkWidget* RenderThemeGtk::gtkTreeView() const
{
    if (m_gtkTreeView)
        return m_gtkTreeView;

    m_gtkTreeView = gtk_tree_view_new();
    g_signal_connect(m_gtkTreeView, "style-set", G_CALLBACK(gtkStyleSetCallback), theme());
    gtk_container_add(gtkContainer(), m_gtkTreeView);
    gtk_widget_realize(m_gtkTreeView);

    return m_gtkTreeView;
}

GtkContainer* RenderThemeGtk::gtkContainer() const
{
    if (m_gtkContainer)
        return m_gtkContainer;

    m_gtkWindow = gtk_window_new(GTK_WINDOW_POPUP);
    m_gtkContainer = GTK_CONTAINER(gtk_fixed_new());
    gtk_container_add(GTK_CONTAINER(m_gtkWindow), GTK_WIDGET(m_gtkContainer));
    gtk_widget_realize(m_gtkWindow);

    return m_gtkContainer;
}

int RenderThemeGtk::menuListInternalPadding(RenderStyle* style, int paddingType) const
{
    // This internal padding is in addition to the user-supplied padding.
    // Matches the FF behavior.
    int padding = kStyledMenuListInternalPadding[paddingType];

    // Reserve the space for right arrow here. The rest of the padding is
    // set by adjustMenuListStyle, since PopMenuWin.cpp uses the padding from
    // RenderMenuList to lay out the individual items in the popup.
    // If the MenuList actually has appearance "NoAppearance", then that means
    // we don't draw a button, so don't reserve space for it.
    const int bar_type = style->direction() == LTR ? RightPadding : LeftPadding;
    if (paddingType == bar_type && style->appearance() != NoControlPart)
        padding += ScrollbarTheme::nativeTheme()->scrollbarThickness();

    return padding;
}

}  // namespace WebCore
