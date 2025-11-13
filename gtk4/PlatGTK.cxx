#include <gtk/gtk.h>

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>
#include <memory>

#include "ScintillaTypes.h"
#include "ScintillaMessages.h"
#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"

#include "ScintillaView.h"
#include "Internals.h"

namespace Scintilla::Internal {

Window::~Window() noexcept {}

void Window::InvalidateAll() {
	if (!wid) {
		return;
	}
	// This is worse than it looks like. Things also get invalidated
	// when the selection updates, and thus we may need to queue an
	// allocate and not only a redraw.
	if (SCINTILLA_IS_VIEW(wid)) {
		scintilla_view_queue_allocate_if_need_update_ui(SCINTILLA_VIEW(wid));
	}
	gtk_widget_queue_draw(GTK_WIDGET(wid));
}

void Window::InvalidateRectangle(PRectangle rc) {
	(void) rc;
	InvalidateAll();
}

void Window::Show(bool show) {
	g_debug("Window::Show()\n");
}

void Window::SetPositionRelative(PRectangle rc, const Window *relativeTo) {
	g_debug("Window::SetPositionRelative()\n");
}

PRectangle Window::GetClientPosition() const {
	if (!wid) {
		return PRectangle();
	}
	GtkWidget *widget = GTK_WIDGET(wid);
	int width = gtk_widget_get_width(widget);
	int height = gtk_widget_get_height(widget);
	if (SCINTILLA_IS_VIEW(widget)) {
		return PRectangle::FromInts(0, 0, width, height);
	}
	g_debug("Window::GetClientPosition()\n");
	return PRectangle();
}

void Window::SetCursor(Cursor curs) {
	if (curs == cursorLast) {
		return;
	}
	cursorLast = curs;
	const char *cursor_name;
	switch (curs) {
	case Cursor::invalid:
		cursor_name = "none";
		break;
	case Cursor::text:
		cursor_name = "text";
		break;
	case Cursor::arrow:
	default:
		cursor_name = "default";
		break;
	case Cursor::up:
		cursor_name = "grabbing";
		break;
	case Cursor::wait:
		cursor_name = "wait";
		break;
	case Cursor::horizontal:
		cursor_name = "col-resize";
		break;
	case Cursor::vertical:
		cursor_name = "row-resize";
		break;
	case Cursor::reverseArrow:
		cursor_name = "pointer";
		break;
	case Cursor::hand:
		cursor_name = "grab";
		break;
	}
	gtk_widget_set_cursor_from_name(GTK_WIDGET(wid), cursor_name);
}

void Window::Destroy() noexcept {
	g_debug("Window::Destroy()\n");
}

PRectangle Window::GetMonitorRect(Point pt) {
	// This is only used to position the autocomplete popover (see ScintillaBase::AutoCompleteStart).
	// We'd rather let GTK position it properly. Pretend there's a huge monitor, so things always
	// fit as far as Scintilla core is concrened.
	g_debug("Window::GetMonitorRect()\n");
	return PRectangle::FromInts(-5000, -5000, 5000, 5000);
}

Menu::Menu() noexcept : mid(nullptr) {}

void Menu::CreatePopUp() {
	g_debug("Menu::CreatePopUp()\n");
}

void Menu::Destroy() noexcept {
	g_debug("Menu::Destroy()\n");
}

void Menu::Show(Point pt, const Window &w) {
	g_debug("Menu::Show()\n");
}

unsigned int Platform::DoubleClickTime() {
	GtkSettings *settings = nullptr;
	if (scintilla_view_recent_instance) {
		settings = gtk_widget_get_settings(GTK_WIDGET(scintilla_view_recent_instance));
	}
	if (!settings) {
		settings = gtk_settings_get_default();
	}
	if (G_UNLIKELY(!settings)) {
		// The default for gtk-double-click-time.
		return 400;
	}
	guint double_click_time;
	g_object_get(settings, "gtk-double-click-time", &double_click_time, nullptr);
	return double_click_time;
}

static ColourRGBA sci_color_from_gdk_rgba(const GdkRGBA *color) {
	return ColourRGBA(color->red * maximumByte, color->green * maximumByte, color->blue * maximumByte, color->alpha * maximumByte);
}

ColourRGBA Platform::Chrome() {
	constexpr ColourRGBA fallback { 0xe0, 0xe0, 0xe0 };
	if (!scintilla_view_recent_instance) {
		return fallback;
	}
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	GtkStyleContext *style_context = gtk_widget_get_style_context(GTK_WIDGET(scintilla_view_recent_instance));
	GdkRGBA color;
	gboolean ok = gtk_style_context_lookup_color(style_context, "theme_bg_color", &color);
G_GNUC_END_IGNORE_DEPRECATIONS
	if (!ok) {
		return fallback;
	}
	return sci_color_from_gdk_rgba(&color);
}

ColourRGBA Platform::ChromeHighlight() {
	return white;
}

static PangoFontDescription *get_font_desc() {
	if (!scintilla_view_recent_instance) {
		return nullptr;
	}
	PangoContext *context = gtk_widget_get_pango_context(GTK_WIDGET(scintilla_view_recent_instance));
	return pango_context_get_font_description(context);
}

const char *Platform::DefaultFont() {
#if 0
	PangoFontDescription *desc = get_font_desc();
	if (!desc) {
		return "monospace";
	}
	return pango_font_description_get_family(desc);
#else
	return SCINTILLA_GTK4_DYNAMIC_DEFAULT_FONT;
#endif
}

int Platform::DefaultFontSize() {
	PangoFontDescription *desc = get_font_desc();
	if (!desc) {
		return 11;
	}
	int s;
	double size = pango_font_description_get_size(desc) / PANGO_SCALE;
#if 0
	if (pango_font_description_get_size_is_absolute(desc)) {
		s = ceil(size);
	} else {
		s = ceil(size * 96.0 / 72.0);
	}
#else
	s = ceil(size);
#endif
	return s;
}

void Platform::Assert(const char *c, const char *file, int line) noexcept {
	g_assertion_message_expr(G_LOG_DOMAIN, file, line, "", c);
#ifdef __GNUC__
	__builtin_unreachable();
#endif
}

void Platform::DebugPrintf(const char *format, ...) noexcept {
	va_list args;
	va_start(args, format);
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
	va_end(args);
}

}
