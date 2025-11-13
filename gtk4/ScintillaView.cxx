#include "ScintillaView.h"
#include "ScintillaGTK.h"
#include "SurfaceGTK.h"
#include "Scintilla.h"
#include "scintilla-marshal.h"
#include "Internals.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

using namespace Scintilla;
using namespace Scintilla::Internal;

struct _ScintillaView {
	GtkWidget parent_instance;

	ScintillaGTK editor;
	GtkIMContext *im_context;
	GtkEventController *key_controller;
	GtkWidget *context_menu;
	char *im_module;
	GtkAdjustment *adj[2];
	GtkScrollablePolicy policy[2];
	guint32 cursor_hidden_at_timestamp;
	size_t preedit_len;
	int preedit_cursor_pos;
	bool cursor_hidden : 1;
	bool need_im_reset : 1;

	static gboolean scrollable_get_border(GtkScrollable *scrollable, GtkBorder *border) noexcept;

#if GTK_CHECK_VERSION(4, 14, 0)
	static GBytes *accessible_get_contents(GtkAccessibleText *, unsigned start, unsigned end) noexcept;
	static GBytes *accessible_get_contents_at(GtkAccessibleText *, unsigned offset, GtkAccessibleTextGranularity, unsigned *start, unsigned *end) noexcept;
	static unsigned accessible_get_caret_position(GtkAccessibleText *) noexcept;
	static gboolean accessible_get_selection(GtkAccessibleText *, gsize *n_ranges, GtkAccessibleTextRange **ranges) noexcept;
	// static gboolean accessible_get_attributes(GtkAccessibleText *, unsigned offset, gsize *n_ranges, GtkAccessibleTextRange **ranges, char ***attribute_names, char ***attribute_values) noexcept;
	// static void accessible_get_default_attributes(GtkAccessibleText *, char ***attribute_names, char ***attribute_values) noexcept;
#endif
#if GTK_CHECK_VERSION(4, 16, 0)
	static gboolean accessible_get_extents(GtkAccessibleText *, unsigned start, unsigned end, graphene_rect_t *extents) noexcept;
	static gboolean accessible_get_offset(GtkAccessibleText *, const graphene_point_t *point, unsigned *offset) noexcept;
#endif
#if GTK_CHECK_VERSION(4, 22, 0)
	static gboolean accessible_set_caret_position(GtkAccessibleText *, unsigned offset) noexcept;
	static gboolean accessible_set_selection(GtkAccessibleText *, gsize i, GtkAccessibleTextRange *range) noexcept;
#endif

	static void dispose(GObject *object) noexcept;
	static void finalize(GObject *object) noexcept;
	static void realize(GtkWidget *widget) noexcept;
	static void unrealize(GtkWidget *widget) noexcept;
	static void measure(GtkWidget *widget, GtkOrientation, int, int *, int *, int *, int *) noexcept;
	static void size_allocate(GtkWidget *widget, int, int, int) noexcept;
	static void snapshot(GtkWidget *widget, GtkSnapshot *snapshot) noexcept;
	static void css_changed(GtkWidget *widget, GtkCssStyleChange *) noexcept;

	static int insert_characters(ScintillaView *, const char *, Scintilla::CharacterSource) noexcept;
	static void hide_cursor(ScintillaView *) noexcept;
	static void unhide_cursor(ScintillaView *) noexcept;
	static void queue_allocate_if_need_update_ui(ScintillaView *) noexcept;

	static void adjustment_value_changed(ScintillaView *, GtkAdjustment *) noexcept;

	static gboolean key_pressed(ScintillaView *, guint keyval, guint keycode, GdkModifierType state) noexcept;

	static void im_commit(ScintillaView *, const char *) noexcept;
	static void im_preedit_start(ScintillaView *) noexcept;
	static void im_preedit_changed(ScintillaView *) noexcept;
	static gboolean im_retrieve_surrounding(ScintillaView *) noexcept;
	static gboolean im_delete_surrounding(ScintillaView *, int offset, int n_chars) noexcept;
	static void reset_im_context_if_needed(ScintillaView *) noexcept;

	static void motion(ScintillaView *, double x, double y, GtkEventControllerMotion *) noexcept;
	static void pressed(ScintillaView *, int n_press, double x, double y, GtkEventController *) noexcept;
	static void released(ScintillaView *, int n_press, double x, double y, GtkEventController *) noexcept;
	static void focus_enter(ScintillaView *) noexcept;
	static void focus_leave(ScintillaView *) noexcept;
	static gboolean event(ScintillaView *, GdkEvent *) noexcept;

	static gboolean modify_attempt(ScintillaView *) noexcept;
	bool is_potentially_editable() noexcept;

	static void activate_text_undo(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_text_redo(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_clipboard_cut(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_clipboard_copy(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_clipboard_paste(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_selection_delete(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_selection_select_all(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_text_clear(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;
	static void activate_menu_popup(GtkWidget *, const char *action_name, GVariant *parameter) noexcept;

	static void open_context_menu(ScintillaView *, double x, double y) noexcept;
};

ScintillaView *scintilla_view_recent_instance;

static void scintilla_view_scrollable_interface_init(GtkScrollableInterface *iface) noexcept;
#if GTK_CHECK_VERSION(4, 14, 0)
static void scintilla_view_accessible_text_interface_init(GtkAccessibleTextInterface *iface) noexcept;
static void implement_accessible_text(GType type) noexcept {
	GInterfaceInfo info {
		reinterpret_cast<GInterfaceInitFunc>(scintilla_view_accessible_text_interface_init),
		nullptr, nullptr
	};
	g_type_add_interface_static(type, GTK_TYPE_ACCESSIBLE_TEXT, &info);
}
#else
static void implement_accessible_text(GType) noexcept { }
#endif

G_DEFINE_FINAL_TYPE_WITH_CODE(ScintillaView, scintilla_view, GTK_TYPE_WIDGET,
	G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, scintilla_view_scrollable_interface_init)
	implement_accessible_text(g_define_type_id);
)


enum {
	PROP_0,

	// Our own props
	PROP_IM_MODULE,
	PROP_INPUT_HINTS,
	PROP_INPUT_PURPOSE,

	PROP_EDITABLE,
	PROP_TAB_WIDTH,
	PROP_EOL_FILLED,
	PROP_LEXER_ID,
	PROP_MODIFIED,
	PROP_OVERTYPE,
	PROP_SEARCH_FLAGS,
	PROP_CARET_STYLE,
	PROP_CARET_STICKY,
	PROP_DOCUMENT,
	PROP_WRAP_MODE,
	PROP_WRAP_VISUAL_FLAGS,
	PROP_WRAP_VISUAL_FLAGS_LOCATION,
	PROP_WRAP_INDENT_MODE,
	PROP_WRAP_START_INDENT,
	PROP_EOL_ANNOTATION_VISIBLE,
	PROP_AUTOMATIC_FOLD,
	PROP_FOLD_FLAGS,
	PROP_MULTIPLE_SELECTION,

	N_PROPS,

	// GtkScrollable props
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

static GParamSpec *props[N_PROPS];

enum {
	SIGNAL_SCI_NOTIFY,
	SIGNAL_MACRO_RECORD,
	SIGNAL_MODIFY_ATTEMPT,
	N_SIGNALS
};

static guint signals[N_SIGNALS];

gboolean ScintillaView::scrollable_get_border(GtkScrollable *scrollable, GtkBorder *border) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(scrollable);

	border->left = view->editor.vs.fixedColumnWidth;
	border->right = view->editor.vs.rightMarginWidth;
	border->top = border->bottom = 0;
	return TRUE;
}

static void scintilla_view_scrollable_interface_init(GtkScrollableInterface *iface) noexcept {
	iface->get_border = ScintillaView::scrollable_get_border;
}

#if GTK_CHECK_VERSION(4, 14, 0)
static void scintilla_view_accessible_text_interface_init(GtkAccessibleTextInterface *iface) noexcept {
	iface->get_contents = ScintillaView::accessible_get_contents;
	iface->get_contents_at = ScintillaView::accessible_get_contents_at;
	iface->get_caret_position = ScintillaView::accessible_get_caret_position;
	iface->get_selection = ScintillaView::accessible_get_selection;
	// iface->get_attributes = ScintillaView::accessible_get_attributes;
	// iface->get_default_attributes = ScintillaView::accessible_get_default_attributes;
#if GTK_CHECK_VERSION(4, 16, 0)
	iface->get_extents = ScintillaView::accessible_get_extents;
	iface->get_offset = ScintillaView::accessible_get_offset;
#endif
#if GTK_CHECK_VERSION(4, 22, 0)
	iface->set_caret_position = ScintillaView::accessible_set_caret_position;
	iface->set_selection = ScintillaView::accessible_set_selection;
#endif
}
#endif

static void scintilla_view_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(object);
	switch (prop_id) {
	case PROP_HADJUSTMENT:
		g_value_set_object(value, view->adj[GTK_ORIENTATION_HORIZONTAL]);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object(value, view->adj[GTK_ORIENTATION_VERTICAL]);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum(value, view->policy[GTK_ORIENTATION_HORIZONTAL]);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum(value, view->policy[GTK_ORIENTATION_VERTICAL]);
		break;
	case PROP_IM_MODULE:
		g_value_set_string(value, view->im_module);
		break;
	case PROP_INPUT_HINTS:
		g_object_get_property(G_OBJECT(view->im_context), "input-hints", value);
		break;
	case PROP_INPUT_PURPOSE:
		g_object_get_property(G_OBJECT(view->im_context), "input-purpose", value);
		break;
	case PROP_EDITABLE:
		g_value_set_boolean(value, scintilla_view_get_editable(view));
		break;
	case PROP_TAB_WIDTH:
		g_value_set_int(value, scintilla_view_get_tab_width(view));
		break;
	case PROP_EOL_FILLED:
		g_value_set_boolean(value, scintilla_view_get_eol_filled(view));
		break;
	case PROP_LEXER_ID:
		g_value_set_int(value, scintilla_view_get_lexer_id(view));
		break;
	case PROP_MODIFIED:
		g_value_set_boolean(value, scintilla_view_get_modified(view));
		break;
	case PROP_OVERTYPE:
		g_value_set_boolean(value, scintilla_view_get_overtype(view));
		break;
	case PROP_SEARCH_FLAGS:
		g_value_set_flags(value, scintilla_view_get_search_flags(view));
		break;
	case PROP_CARET_STYLE:
		g_value_set_flags(value, scintilla_view_get_caret_style(view));
		break;
	case PROP_CARET_STICKY:
		g_value_set_enum(value, scintilla_view_get_caret_sticky(view));
		break;
	case PROP_DOCUMENT:
		g_value_set_boxed(value, scintilla_view_get_document(view));
		break;
	case PROP_WRAP_MODE:
		g_value_set_enum(value, scintilla_view_get_wrap_mode(view));
		break;
	case PROP_WRAP_VISUAL_FLAGS:
		g_value_set_flags(value, scintilla_view_get_wrap_visual_flags(view));
		break;
	case PROP_WRAP_VISUAL_FLAGS_LOCATION:
		g_value_set_flags(value, scintilla_view_get_wrap_visual_flags_location(view));
		break;
	case PROP_WRAP_INDENT_MODE:
		g_value_set_enum(value, scintilla_view_get_wrap_indent_mode(view));
		break;
	case PROP_WRAP_START_INDENT:
		g_value_set_int(value, scintilla_view_get_wrap_start_indent(view));
		break;
	case PROP_EOL_ANNOTATION_VISIBLE:
		g_value_set_enum(value, scintilla_view_eol_annotation_get_visible(view));
		break;
	case PROP_AUTOMATIC_FOLD:
		g_value_set_flags(value, scintilla_view_get_automatic_fold(view));
		break;
	case PROP_FOLD_FLAGS:
		g_value_set_flags(value, scintilla_view_get_fold_flags(view));
		break;
	case PROP_MULTIPLE_SELECTION:
		g_value_set_boolean(value, scintilla_view_get_multiple_selection(view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

void ScintillaView::adjustment_value_changed(ScintillaView *view, GtkAdjustment *adj) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	g_return_if_fail(GTK_IS_ADJUSTMENT(adj));
	g_return_if_fail(adj == view->adj[0] || adj == view->adj[1]);
	scintilla_view_recent_instance = view;

	double value = gtk_adjustment_get_value(adj);

	if (adj == view->adj[GTK_ORIENTATION_HORIZONTAL]) {
		view->editor.HorizontalScrollTo(value);
	} else {
		view->editor.ScrollTo(value);
	}

	gtk_widget_queue_allocate(GTK_WIDGET(view));
}

static void scintilla_view_set_adjustment(ScintillaView *view, GtkAdjustment *adj, GtkOrientation orientation) noexcept {
	if (adj == nullptr) {
		adj = gtk_adjustment_new(0, 0, 0, 0, 0, 0);
	} else if (view->adj[orientation] == adj) {
		return;
	}
	gtk_widget_queue_allocate(GTK_WIDGET(view));
	if (view->adj[orientation]) {
		g_signal_handlers_disconnect_by_func(view->adj[orientation], reinterpret_cast<void *>(ScintillaView::adjustment_value_changed), view);
		g_object_unref(view->adj[orientation]);
	}
	view->adj[orientation] = g_object_ref_sink(adj);
	g_signal_connect_object(adj, "value-changed", G_CALLBACK(ScintillaView::adjustment_value_changed), view, G_CONNECT_SWAPPED);
	if (orientation == GTK_ORIENTATION_VERTICAL) {
		g_object_notify(G_OBJECT(view), "vadjustment");
	} else {
		g_object_notify(G_OBJECT(view), "hadjustment");
	}
}

static void scintilla_view_set_policy(ScintillaView *view, GtkScrollablePolicy policy, GtkOrientation orientation) noexcept {
	g_print("TODO: scintilla_view_set_policy\n");
}

static void scintilla_view_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(object);
	scintilla_view_recent_instance = view;

	switch (prop_id) {
	case PROP_HADJUSTMENT:
		scintilla_view_set_adjustment(view, GTK_ADJUSTMENT(g_value_get_object(value)), GTK_ORIENTATION_HORIZONTAL);
		break;
	case PROP_VADJUSTMENT:
		scintilla_view_set_adjustment(view, GTK_ADJUSTMENT(g_value_get_object(value)), GTK_ORIENTATION_VERTICAL);
		break;
	case PROP_HSCROLL_POLICY:
		scintilla_view_set_policy(view, static_cast<GtkScrollablePolicy>(g_value_get_enum(value)), GTK_ORIENTATION_HORIZONTAL);
		break;
	case PROP_VSCROLL_POLICY:
		scintilla_view_set_policy(view, static_cast<GtkScrollablePolicy>(g_value_get_enum(value)), GTK_ORIENTATION_VERTICAL);
		break;
	case PROP_IM_MODULE:
		g_free(view->im_module);
		view->im_module = g_value_dup_string(value);
		gtk_im_multicontext_set_context_id(GTK_IM_MULTICONTEXT(view->im_context), view->im_module);
		g_object_notify_by_pspec(object, pspec);
		break;
	case PROP_INPUT_HINTS:
		scintilla_view_set_input_hints(view, static_cast<GtkInputHints>(g_value_get_flags(value)));
		break;
	case PROP_INPUT_PURPOSE:
		scintilla_view_set_input_purpose(view, static_cast<GtkInputPurpose>(g_value_get_enum(value)));
		break;
	case PROP_EDITABLE:
		scintilla_view_set_editable(view, g_value_get_boolean(value));
		break;
	case PROP_TAB_WIDTH:
		scintilla_view_set_tab_width(view, g_value_get_int(value));
		break;
	case PROP_EOL_FILLED:
		scintilla_view_set_eol_filled(view, g_value_get_boolean(value));
		break;
	case PROP_OVERTYPE:
		scintilla_view_set_overtype(view, g_value_get_boolean(value));
		break;
	case PROP_SEARCH_FLAGS:
		scintilla_view_set_search_flags(view, static_cast<ScintillaFindOption>(g_value_get_flags(value)));
		break;
	case PROP_CARET_STYLE:
		scintilla_view_set_caret_style(view, static_cast<ScintillaCaretStyle>(g_value_get_flags(value)));
		break;
	case PROP_CARET_STICKY:
		scintilla_view_set_caret_sticky(view, static_cast<ScintillaCaretSticky>(g_value_get_enum(value)));
		break;
	case PROP_DOCUMENT:
		scintilla_view_set_document(view, reinterpret_cast<ScintillaDocument *>(g_value_get_boxed(value)));
		break;
	case PROP_WRAP_MODE:
		scintilla_view_set_wrap_mode(view, static_cast<ScintillaWrap>(g_value_get_enum(value)));
		break;
	case PROP_WRAP_VISUAL_FLAGS:
		scintilla_view_set_wrap_visual_flags(view, static_cast<ScintillaWrapVisualFlag>(g_value_get_flags(value)));
		break;
	case PROP_WRAP_VISUAL_FLAGS_LOCATION:
		scintilla_view_set_wrap_visual_flags_location(view, static_cast<ScintillaWrapVisualLocation>(g_value_get_flags(value)));
		break;
	case PROP_WRAP_INDENT_MODE:
		scintilla_view_set_wrap_indent_mode(view, static_cast<ScintillaWrapIndentMode>(g_value_get_enum(value)));
		break;
	case PROP_WRAP_START_INDENT:
		scintilla_view_set_wrap_start_indent(view, g_value_get_int(value));
		break;
	case PROP_EOL_ANNOTATION_VISIBLE:
		scintilla_view_eol_annotation_set_visible(view, static_cast<ScintillaEOLAnnotationVisible>(g_value_get_enum(value)));
		break;
	case PROP_AUTOMATIC_FOLD:
		scintilla_view_set_automatic_fold(view, static_cast<ScintillaAutomaticFold>(g_value_get_flags(value)));
		break;
	case PROP_FOLD_FLAGS:
		scintilla_view_set_fold_flags(view, static_cast<ScintillaFoldFlag>(g_value_get_flags(value)));
		break;
	case PROP_MULTIPLE_SELECTION:
		scintilla_view_set_multiple_selection(view, g_value_get_boolean(value));
		break;
	case PROP_LEXER_ID:
	case PROP_MODIFIED:
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

void ScintillaView::realize(GtkWidget *widget) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	GTK_WIDGET_CLASS(scintilla_view_parent_class)->realize(widget);
	gtk_im_context_set_client_widget(view->im_context, widget);
	if (gtk_widget_is_focus(widget) && view->is_potentially_editable()) {
		view->need_im_reset = true;
		gtk_im_context_focus_in(view->im_context);
	}
}

void ScintillaView::unrealize(GtkWidget *widget) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	gtk_im_context_set_client_widget(view->im_context, nullptr);
	GTK_WIDGET_CLASS(scintilla_view_parent_class)->unrealize(widget);
}

void scintilla_view_notify_editable(ScintillaView *view, bool editable) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));

	gtk_accessible_update_property(GTK_ACCESSIBLE(view),
		GTK_ACCESSIBLE_PROPERTY_READ_ONLY, static_cast<gboolean>(!editable),
		-1);

	if (gtk_widget_is_focus(GTK_WIDGET(view))) {
		view->need_im_reset = true;
		if (editable) {
			gtk_im_context_focus_in(view->im_context);
		} else {
			gtk_im_context_focus_out(view->im_context);
		}
	}
	gtk_event_controller_key_set_im_context(GTK_EVENT_CONTROLLER_KEY(view->key_controller), editable ? view->im_context : nullptr);
}

void ScintillaView::measure(GtkWidget *widget, GtkOrientation orientation, int for_size, int *minimum, int *natural, int *minimum_baseline, int *natural_baseline) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.in_measure = true;
	view->editor.NotifyUpdateUI();
	view->editor.RefreshStyleData();
	view->editor.in_measure = false;

	(void) for_size;
	*minimum_baseline = *natural_baseline = -1;
	// TODO: This could be better perhaps?
	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		*minimum = *natural = view->editor.scrollWidth;
	} else {
		*minimum = *natural = view->editor.pcs->LinesDisplayed() * view->editor.vs.lineHeight;
	}
}

void ScintillaView::size_allocate(GtkWidget *widget, int width, int height, int baseline) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	if (view->context_menu) {
		gtk_popover_present(GTK_POPOVER(view->context_menu));
	}

	(void) baseline;
	PRectangle rect = PRectangle::FromInts(0, 0, width, height);

	// Explicitly do the preparations that would cause Paint() to abandon
	// painting.
	do {
		view->editor.ChangeScrollBars();
		view->editor.StyleAreaBounded(rect, false);
		view->editor.RefreshStyleData();
		view->editor.WrapLines(Editor::WrapScope::wsVisible);
	} while (view->editor.NotifyUpdateUI());
}

void scintilla_view_queue_allocate_if_need_update_ui(ScintillaView *view) noexcept {
	ScintillaView::queue_allocate_if_need_update_ui(view);
}

void ScintillaView::queue_allocate_if_need_update_ui(ScintillaView *view) noexcept {
	if (view->editor.needUpdateUI != Update::None) {
		gtk_widget_queue_allocate(GTK_WIDGET(view));
	}
}

void ScintillaView::snapshot(GtkWidget *widget, GtkSnapshot *snapshot) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	if (view->editor.NotifyUpdateUI()) {
		g_warning("needUpdateUI set in snapshot");
		gtk_widget_queue_resize(widget);
	}

	double width = gtk_widget_get_width(widget);
	double height = gtk_widget_get_height(widget);
	PRectangle rect = PRectangle::FromInts(0, 0, width, height);

	SurfaceGTK surface;
	surface.snapshot = g_object_ref(snapshot);
	surface.widget = widget;
	surface.size = { static_cast<float>(width), static_cast<float>(height) };

	view->editor.paintState = Editor::PaintState::painting;
	view->editor.Paint(&surface, rect);

	if (view->editor.paintState == Editor::PaintState::abandoned) {
		g_warning("abandoned paint");
		gtk_widget_queue_resize(widget);
	}

	view->editor.paintState = Editor::PaintState::notPainting;
}

void ScintillaView::css_changed(GtkWidget *widget, GtkCssStyleChange *change) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	GTK_WIDGET_CLASS(scintilla_view_parent_class)->css_changed(widget, change);
	view->editor.InvalidateStyleData();
}

static KeyMod translate_modifiers(GdkModifierType modifiers) {
	KeyMod res = KeyMod::Norm;
	if (modifiers & GDK_SHIFT_MASK) {
		res = res | KeyMod::Shift;
	}
	if (modifiers & GDK_CONTROL_MASK) {
		res = res | KeyMod::Ctrl;
	}
	if (modifiers & GDK_ALT_MASK) {
		res = res | KeyMod::Alt;
	}
	if (modifiers & GDK_SUPER_MASK) {
		res = res | KeyMod::Super;
	}
	if (modifiers & GDK_META_MASK) {
		res = res | KeyMod::Meta;
	}
	return res;
}

gboolean ScintillaView::key_pressed(ScintillaView *view, guint keyval, guint keycode, GdkModifierType state) noexcept {
	scintilla_view_recent_instance = view;

	hide_cursor(view);

	// gtk_im_context_filter_keypress already invoked by GtkEventControllerKey.

	switch (keyval) {
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
	case GDK_KEY_ISO_Enter:
	case GDK_KEY_Escape:
		reset_im_context_if_needed(view);
		break;
	}

	Scintilla::Keys sci_keys;

	static_assert(GDK_KEY_0 == '0', "GDK_KEY_0 is not '0'");
	static_assert(GDK_KEY_A == 'A', "GDK_KEY_A is not 'A'");
	static_assert(GDK_KEY_a == 'a', "GDK_KEY_a is not 'a'");
	// Pass everything in the "plain ASCII" range as is, since GDK_KEY constants
	// match the ASCII encodings.
	if (GDK_KEY_space <= keyval && keyval <= GDK_KEY_asciitilde) {
		sci_keys = static_cast<Scintilla::Keys>(keyval);
		// Scintilla expects upper case letters.
		if (GDK_KEY_a <= keyval && keyval <= GDK_KEY_z) {
			sci_keys = static_cast<Scintilla::Keys>(keyval + GDK_KEY_A - GDK_KEY_a);
		}
		goto sci_keys_set;
	}

	switch (keyval) {
	case GDK_KEY_ISO_Left_Tab:
	case GDK_KEY_Tab:
	case GDK_KEY_KP_Tab:
		sci_keys = Scintilla::Keys::Tab;
		break;
	case GDK_KEY_KP_Down:
	case GDK_KEY_Down:
	case GDK_KEY_ISO_Move_Line_Down:
		sci_keys = Scintilla::Keys::Down;
		break;
	case GDK_KEY_KP_Up:
	case GDK_KEY_Up:
	case GDK_KEY_ISO_Move_Line_Up:
		sci_keys = Scintilla::Keys::Up;
		break;
	case GDK_KEY_KP_Left:
	case GDK_KEY_Left:
		sci_keys = Scintilla::Keys::Left;
		break;
	case GDK_KEY_KP_Right:
	case GDK_KEY_Right:
		sci_keys = Scintilla::Keys::Right;
		break;
	case GDK_KEY_KP_Home:
	case GDK_KEY_Home:
		sci_keys = Scintilla::Keys::Home;
		break;
	case GDK_KEY_KP_End:
	case GDK_KEY_End:
		sci_keys = Scintilla::Keys::End;
		break;
	case GDK_KEY_KP_Page_Up:
	case GDK_KEY_Page_Up:
		sci_keys = Scintilla::Keys::Prior;
		break;
	case GDK_KEY_KP_Page_Down:
	case GDK_KEY_Page_Down:
		sci_keys = Scintilla::Keys::Next;
		break;
	case GDK_KEY_KP_Delete:
	case GDK_KEY_Delete:
		sci_keys = Scintilla::Keys::Delete;
		break;
	case GDK_KEY_KP_Insert:
	case GDK_KEY_Insert:
		sci_keys = Scintilla::Keys::Insert;
		break;
	case GDK_KEY_KP_Enter:
	case GDK_KEY_Return:
	case GDK_KEY_ISO_Enter:
		sci_keys = Scintilla::Keys::Return;
		break;
	case GDK_KEY_Escape:
		sci_keys = Scintilla::Keys::Escape;
		break;
	case GDK_KEY_BackSpace:
		sci_keys = Scintilla::Keys::Back;
		break;
	case GDK_KEY_KP_Add:
		sci_keys = Scintilla::Keys::Add;
		break;
	case GDK_KEY_KP_Subtract:
		sci_keys = Scintilla::Keys::Subtract;
		break;
	case GDK_KEY_KP_Divide:
		sci_keys = Scintilla::Keys::Divide;
		break;
	case GDK_KEY_Super_L:
		sci_keys = Scintilla::Keys::Win;
		break;
	case GDK_KEY_Super_R:
		sci_keys = Scintilla::Keys::RWin;
		break;
	case GDK_KEY_Menu:
		sci_keys = Scintilla::Keys::Menu;
		break;
	default:
		return false;
	}

sci_keys_set:
	bool consumed = false;
	bool added = view->editor.KeyDownWithModifiers(sci_keys, translate_modifiers(state), &consumed) != 0;
	// ???
	return consumed || added;
}

void ScintillaView::motion(ScintillaView *view, double x, double y, GtkEventControllerMotion *motion_controller) noexcept {
	scintilla_view_recent_instance = view;

	GtkEventController *event_controller = GTK_EVENT_CONTROLLER(motion_controller);
	if (view->cursor_hidden) {
		GdkDevice *device = gtk_event_controller_get_current_event_device(event_controller);
		guint32 ts = gdk_device_get_timestamp(device);
		if (ts != view->cursor_hidden_at_timestamp) {
			unhide_cursor(view);
		}
	}
	GdkModifierType modifiers = gtk_event_controller_get_current_event_state(event_controller);
	auto time = gtk_event_controller_get_current_event_time(event_controller);
	Point pt { x, y };
	view->editor.ButtonMoveWithModifiers(pt, time, translate_modifiers(modifiers));
}

void ScintillaView::pressed(ScintillaView *view, int n_press, double x, double y, GtkEventController *controller) noexcept {
	scintilla_view_recent_instance = view;
	GtkWidget *widget = GTK_WIDGET(view);

	unhide_cursor(view);
	if (gtk_widget_get_focus_on_click(widget)) {
		gtk_widget_grab_focus(widget);
	}
	GdkModifierType modifiers = gtk_event_controller_get_current_event_state(controller);
	auto sci_modifiers = translate_modifiers(modifiers);
	auto time = gtk_event_controller_get_current_event_time(controller);
	Point pt { x, y };

	view->need_im_reset = true;
	reset_im_context_if_needed(view);

	GdkEvent *event = gtk_event_controller_get_current_event(controller);
	if (gdk_event_triggers_context_menu(event) && view->editor.ShouldDisplayPopup(pt)) {
		gtk_gesture_set_state(GTK_GESTURE(controller), GTK_EVENT_SEQUENCE_CLAIMED);
		// view->editor.ContextMenu(pt);
		open_context_menu(view, x, y);
		return;
	}

	auto button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(controller));
	switch (button) {
	case GDK_BUTTON_PRIMARY:
		view->editor.ButtonDownWithModifiers(pt, time, sci_modifiers);
		break;
	case GDK_BUTTON_SECONDARY:
		view->editor.RightButtonDownWithModifiers(pt, time, sci_modifiers);
		break;
	case GDK_BUTTON_MIDDLE: {
			if (n_press != 1) {
				break;
			}
			GtkSettings *settings = gtk_widget_get_settings(widget);
			gboolean enable_primary_paste;
			g_object_get(settings, "gtk-enable-primary-paste", &enable_primary_paste, nullptr);
			if (!enable_primary_paste) {
				break;
			}
			// TODO: check editable / CanPaste()?
			view->editor.Paste(gtk_widget_get_primary_clipboard(widget));
			break;
		}
	}
}

bool ScintillaView::is_potentially_editable() noexcept {
	bool editable = editor.WndProc(Message::GetReadOnly, 0, 0) == 0;
	if (editable) {
		return true;
	}
	return g_signal_has_handler_pending(this, signals[SIGNAL_MODIFY_ATTEMPT], 0, FALSE);
}

void ScintillaView::released(ScintillaView *view, int n_press, double x, double y, GtkEventController *controller) noexcept {
	scintilla_view_recent_instance = view;

	unhide_cursor(view);
	GdkModifierType modifiers = gtk_event_controller_get_current_event_state(controller);
	auto sci_modifiers = translate_modifiers(modifiers);
	auto time = gtk_event_controller_get_current_event_time(controller);
	Point pt { x, y };
	GdkEvent *event = gtk_event_controller_get_current_event(controller);
	auto button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(controller));
	switch (button) {
	case GDK_BUTTON_PRIMARY:
		view->editor.ButtonUpWithModifiers(pt, time, sci_modifiers);
#if GTK_CHECK_VERSION(4, 14, 0)
		if (!view->editor.PointInSelMargin(pt) && view->is_potentially_editable()) {
			gtk_im_context_activate_osk(view->im_context, event);
		}
#endif
		break;
	case GDK_BUTTON_SECONDARY:
		// view->editor.RightButtonUpWithModifiers(pt, time, sci_modifiers);
		break;
	}
}

void ScintillaView::open_context_menu(ScintillaView *view, double x, double y) noexcept {
	if (!view->context_menu) {
		GMenu *model = g_menu_new();

		GMenu *section = g_menu_new();
		GMenuItem *item;

		item = g_menu_item_new(_("Cu_t"), "clipboard.cut");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-cut-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);
		item = g_menu_item_new(_("_Copy"), "clipboard.copy");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-copy-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);
		item = g_menu_item_new(_("_Paste"), "clipboard.paste");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-paste-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);
		item = g_menu_item_new(_("_Delete"), "selection.delete");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-delete-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);

		g_menu_append_section(model, nullptr, G_MENU_MODEL(section));
		g_object_unref(section);
		section = g_menu_new();

		item = g_menu_item_new(_("_Undo"), "text.undo");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-undo-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);
		item = g_menu_item_new(_("_Redo"), "text.redo");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-redo-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);

		g_menu_append_section(model, nullptr, G_MENU_MODEL(section));
		g_object_unref(section);
		section = g_menu_new();

		item = g_menu_item_new(_("Select _All"), "selection.select-all");
		g_menu_item_set_attribute(item, "touch-icon", "s", "edit-select-all-symbolic");
		g_menu_append_item(section, item);
		g_object_unref(item);

		g_menu_append_section(model, nullptr, G_MENU_MODEL(section));
		g_object_unref(section);

		view->context_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(model));
		g_object_unref(model);

		gtk_widget_set_parent(view->context_menu, GTK_WIDGET(view));
		gtk_widget_set_halign(view->context_menu, GTK_ALIGN_START);
		gtk_popover_set_position(GTK_POPOVER(view->context_menu), GTK_POS_BOTTOM);
		gtk_popover_set_has_arrow(GTK_POPOVER(view->context_menu), FALSE);

		gtk_accessible_update_property(GTK_ACCESSIBLE(view->context_menu),
			GTK_ACCESSIBLE_PROPERTY_LABEL, _("Context menu"),
			-1);
	}

	if (x != -1 && y != -1) {
		GdkRectangle point {
			static_cast<int>(round(x)),
			static_cast<int>(round(y)),
			1, 1
		};
		gtk_popover_set_pointing_to(GTK_POPOVER(view->context_menu), &point);
	} else {
		gtk_popover_set_pointing_to(GTK_POPOVER(view->context_menu), nullptr);
	}

	gtk_popover_popup(GTK_POPOVER(view->context_menu));
}

void ScintillaView::focus_enter(ScintillaView *view) noexcept {
	scintilla_view_recent_instance = view;

	view->editor.SetFocusState(true);
	view->need_im_reset = true;
	gtk_im_context_focus_in(view->im_context);
}

void ScintillaView::focus_leave(ScintillaView *view) noexcept {
	scintilla_view_recent_instance = view;

	view->editor.SetFocusState(false);
	view->need_im_reset = true;
	gtk_im_context_focus_out(view->im_context);
}

void ScintillaView::hide_cursor(ScintillaView *view) noexcept {
	if (view->cursor_hidden) {
		return;
	}
	gtk_widget_set_cursor_from_name(GTK_WIDGET(view), "none");

	view->cursor_hidden = true;
	GdkDisplay *display = gtk_widget_get_display(GTK_WIDGET(view));
	GdkSeat *seat = gdk_display_get_default_seat(display);
	GdkDevice *device = gdk_seat_get_pointer(seat);
	view->cursor_hidden_at_timestamp = gdk_device_get_timestamp(device);
}

void ScintillaView::unhide_cursor(ScintillaView *view) noexcept {
	if (!view->cursor_hidden) {
		return;
	}
	view->cursor_hidden = false;
	gtk_widget_set_cursor_from_name(GTK_WIDGET(view), "text");
}

int ScintillaView::insert_characters(ScintillaView *view, const char *str, Scintilla::CharacterSource charSource) noexcept {
	scintilla_view_recent_instance = view;

	int n_chars = 0;
	while (*str) {
		n_chars++;
		const char *next = g_utf8_next_char(str);
		std::string_view sv { str, static_cast<size_t>(next - str) };
		if (view->editor.IsUnicodeMode()) {
			view->editor.InsertCharacter(sv, charSource);
		} else {
			// Convert to the target encoding...
			g_abort();
		}
		str = next;
	}
	return n_chars;
}

void ScintillaView::reset_im_context_if_needed(ScintillaView *view) noexcept {
	if (view->need_im_reset) {
		gtk_im_context_reset(view->im_context);
		view->need_im_reset = false;
	}
}

void ScintillaView::im_commit(ScintillaView *view, const char *str) noexcept {
	if (view->editor.pdoc->TentativeActive()) {
		view->editor.pdoc->TentativeUndo();
	}
	insert_characters(view, str, CharacterSource::ImeResult);
}

void ScintillaView::im_preedit_start(ScintillaView *view) noexcept {
	scintilla_view_recent_instance = view;

	g_debug("ScintillaView::im_preedit_start\n");
	// Delete any selection. Figure out the most correct way to do that;
	// what does Scintilla itself do when a key is pressed?
	// document->DeleteChars()?

	view->editor.ClearBeforeTentativeStart();
	view->preedit_len = view->preedit_cursor_pos = 0;
}

void ScintillaView::im_preedit_changed(ScintillaView *view) noexcept {
	scintilla_view_recent_instance = view;

	g_debug("ScintillaView::im_preedit_changed\n");
	if (view->editor.pdoc->IsReadOnly() || view->editor.SelectionContainsProtected()) {
		view->need_im_reset = true;
		reset_im_context_if_needed(view);
		return;
	}

	hide_cursor(view);

	bool initial_compose = !view->editor.pdoc->TentativeActive();
	if (initial_compose) {
		view->editor.ClearBeforeTentativeStart();
	} else {
		view->editor.pdoc->TentativeUndo();
	}

	char *preedit_string = nullptr;
	int cursor_pos = 0;
	gtk_im_context_get_preedit_string(view->im_context, &preedit_string, nullptr, &cursor_pos);
	if (preedit_string == nullptr) {
		view->preedit_len = view->preedit_cursor_pos = 0;
		return;
	}

	view->editor.pdoc->TentativeStart(); // TentativeActive() from now on

	int n_chars = insert_characters(view, preedit_string, CharacterSource::TentativeInput);
	g_free(preedit_string);
	view->preedit_len = n_chars;
	view->preedit_cursor_pos = cursor_pos;
	// DrawImeIndicator?
	auto sci_current_pos = view->editor.CurrentPosition();
	const Sci::Position sci_cursor_pos = view->editor.pdoc->GetRelativePosition(sci_current_pos, cursor_pos - n_chars);
	view->editor.MoveImeCarets(sci_cursor_pos - sci_current_pos);
}

gboolean ScintillaView::im_retrieve_surrounding(ScintillaView *view) noexcept {
	scintilla_view_recent_instance = view;

	Sci::Position current_pos = view->editor.CurrentPosition();
	int line = view->editor.pdoc->LineFromPosition(current_pos);
	Sci::Position line_start = view->editor.pdoc->LineStart(line);
	Sci::Position line_end = view->editor.pdoc->LineEnd(line);
	auto sel_start = view->editor.SelectionStart();
	auto sel_end = view->editor.SelectionEnd();

	int cursor_index = current_pos - line_start;
	int anchor_index;
	if (!sel_start.IsValid() && !sel_end.IsValid()) {
		anchor_index = cursor_index;
	} else if (!sel_start.IsValid()) {
		anchor_index = sel_end.Position() - line_start;
	} else if (!sel_end.IsValid()) {
		anchor_index = sel_start.Position() - line_start;
	} else if (sel_start.Position() == current_pos) {
		anchor_index = sel_end.Position() - line_start;
	} else {
		anchor_index = sel_start.Position() - line_start;
	}

	if (view->editor.IsUnicodeMode()) {
		std::string text = view->editor.RangeText(line_start, line_end);
		text.erase(cursor_index - view->preedit_cursor_pos, view->preedit_len);
		cursor_index -= view->preedit_cursor_pos;
		if (anchor_index >= cursor_index) {
			anchor_index = std::max(cursor_index, anchor_index - view->preedit_cursor_pos);
		}
#if GTK_CHECK_VERSION(4, 2, 0)
		gtk_im_context_set_surrounding_with_selection(view->im_context, text.c_str(), text.size(), cursor_index, anchor_index);
#else
		gtk_im_context_set_surrounding(view->im_context, text.c_str(), text.size(), cursor_index);
		(void) anchor_index;
#endif
		return TRUE;
	} else {
		// TODO
		g_abort();
	}

	return FALSE;
}

gboolean ScintillaView::im_delete_surrounding(ScintillaView *view, int offset, int n_chars) noexcept {
	scintilla_view_recent_instance = view;

	Sci::Position current_pos = view->editor.CurrentPosition();
	Sci::Position start = view->editor.pdoc->GetRelativePosition(current_pos, offset);
	if (start == INVALID_POSITION) {
		return FALSE;
	}
	Sci::Position end = view->editor.pdoc->GetRelativePosition(start, n_chars);
	if (end == INVALID_POSITION) {
		return FALSE;
	}
	// TODO: isn't end - start same as n_chars?
	return view->editor.pdoc->DeleteChars(start, end - start);
}

gboolean ScintillaView::modify_attempt(ScintillaView *view) noexcept {
	gtk_widget_error_bell(GTK_WIDGET(view));
	return TRUE;
}

void scintilla_view_sci_notify(ScintillaView *view, void *scn) noexcept {
	// TODO: get rid of this maybe
	g_signal_emit(view, signals[SIGNAL_SCI_NOTIFY], 0, scn);

	const NotificationData *n = reinterpret_cast<const NotificationData *>(scn);
	switch (n->nmhdr.code) {
	case Notification::MacroRecord:
		g_signal_emit(view, signals[SIGNAL_MACRO_RECORD], 0, static_cast<guint>(n->message), static_cast<guint64>(n->wParam), static_cast<gint64>(n->lParam));
		break;
	case Notification::SavePointReached:
	case Notification::SavePointLeft:
		g_object_notify_by_pspec(G_OBJECT(view), props[PROP_MODIFIED]);
		break;
	case Notification::ModifyAttemptRO: {
			gboolean handled = FALSE;
			g_signal_emit(view, signals[SIGNAL_MODIFY_ATTEMPT], 0, &handled);
			break;
		}
#if GTK_CHECK_VERSION(4, 14, 0)
	case Notification::Modified:
		if (FlagSet(n->modificationType, ModificationFlags::InsertText)) {
			// TODO: here and elsewhere, CharacterOffsetFromByteOffset
			unsigned start = n->position;
			unsigned end = n->position + n->length;
			gtk_accessible_text_update_contents(GTK_ACCESSIBLE_TEXT(view), GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_INSERT, start, end);
		}
		if (FlagSet(n->modificationType, ModificationFlags::BeforeDelete)) {
			unsigned start = n->position;
			unsigned end = n->position + n->length;
			gtk_accessible_text_update_contents(GTK_ACCESSIBLE_TEXT(view), GTK_ACCESSIBLE_TEXT_CONTENT_CHANGE_REMOVE, start, end);
		}
		break;
	case Notification::UpdateUI:
		if (FlagSet(n->updated, Update::Selection)) {
			// TODO: need granular tracking of what has changed and what hasn't.
			gtk_accessible_text_update_caret_position(GTK_ACCESSIBLE_TEXT(view));
			gtk_accessible_text_update_selection_bound(GTK_ACCESSIBLE_TEXT(view));
		}
		break;
#endif
	default:
		break;
	}
}

/**
 * scintilla_view_get_document:
 * @view: A Scintilla view
 *
 * Returns: (transfer none) (not nullable): The document
 */
ScintillaDocument *scintilla_view_get_document(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), nullptr);
	scintilla_view_recent_instance = view;

	return reinterpret_cast<ScintillaDocument *>(view->editor.WndProc(Message::GetDocPointer, 0, 0));
}

/**
 * scintilla_view_set_document:
 * @view: A Scintilla view
 * @document: (nullable) (transfer none): A document
 */
void scintilla_view_set_document(ScintillaView *view, ScintillaDocument *document) noexcept {
	g_return_if_fail(SCINTILLA_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetDocPointer, 0, reinterpret_cast<sptr_t>(document));
}

/**
 * scintilla_view_send_message:
 * @view: A Scintilla view
 * @msg: Message code
 * @wParam: First message argument
 * @lParam: Second message argument
 *
 * Sends a [Win32-style message](https://learn.microsoft.com/en-us/windows/win32/winmsg/about-messages-and-message-queues)
 * to the view. This is available on all platforms (not just Win32),
 * and is a generic funnel allowing one to invoke just about any
 * method of the Scintilla API.
 *
 * You can use this method when some Scintilla functionality is not
 * (yet) exposed as a convenient `ScintillaView` method.
 *
 * Returns: window procedure return value.
 */
gintptr scintilla_view_send_message(ScintillaView *view, unsigned int msg, guintptr wParam, gintptr lParam) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(static_cast<Message>(msg), wParam, lParam);
}

/**
 * scintilla_view_get_editable:
 * @view: A Scintilla view
 *
 * Returns whether the text in the view is editable by the user.
 *
 * Returns: `TRUE` if editable
 */
gboolean scintilla_view_get_editable(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), false);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::GetReadOnly, 0, 0) == 0;
}

/**
 * scintilla_view_set_editable:
 * @view: A Scintilla view
 * @editable: `TRUE` if the user is allowed to edit the text
 *
 * Sets whether the text in the view is editable by the user.
 */
void scintilla_view_set_editable(ScintillaView *view, gboolean editable) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetReadOnly, editable ? 0 : 1, 0);
}

/**
 * scintilla_view_get_text_length:
 * @view: A Scintilla view
 *
 * Returns: Current length of text in the view.
 */
gsize scintilla_view_get_text_length(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	// Same as Message::GetLength
	return static_cast<gsize>(view->editor.WndProc(Message::GetTextLength, 0, 0));
}

/**
 * scintilla_view_get_text:
 * @view: A Scintilla view
 * @buffer: (optional) (out caller-allocates) (array length=count) (element-type gchar): a buffer to read the text into
 * @count: (in): the size of `buffer`, including the zero terminator
 *
 * Copies the full text from the view into the specified buffer. The text is truncated
 * if the `count` is smaller than the text length. The buffer is always zero-terminated.
 *
 * ::: note
 *     The corresponding `SC_GETTEXT` message doesn't count the terminating zero byte
 *     towards its `length` argument (in `wParam`), whereas this method does.
 *
 * Returns: The full length of the text, same as returned by [method@View.get_text_length].
 */
gsize scintilla_view_get_text(ScintillaView *view, char *buffer, gsize count) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	g_return_val_if_fail(buffer == nullptr || count > 0, 0);
	scintilla_view_recent_instance = view;

	return static_cast<gsize>(view->editor.WndProc(Message::GetText, count + 1, reinterpret_cast<uptr_t>(buffer)));
}

/**
 * scintilla_view_set_text:
 * @view: A Scintilla view
 * @text: new text
 *
 * Sets the text of the view.
 */
void scintilla_view_set_text(ScintillaView *view, const char *text) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	g_return_if_fail(text);
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetText, 0, reinterpret_cast<uptr_t>(text));
}

/**
 * scintilla_view_get_character_pointer:
 * @view: A Scintilla view
 *
 * Returns: (transfer none): Read-only character pointer
 */
const char *scintilla_view_get_character_pointer(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), nullptr);
	scintilla_view_recent_instance = view;

	return reinterpret_cast<const char *>(view->editor.WndProc(Message::GetCharacterPointer, 0, 0));
}

/**
 * scintilla_view_get_range_pointer:
 * @view: A Scintilla view
 * @start: Starting position of the range
 * @length: (in): Length of the range
 *
 * Returns: (transfer none) (array length=length) (element-type gchar): Read-only character buffer
 */
const char *scintilla_view_get_range_pointer(ScintillaView *view, gsize start, gsize length) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), nullptr);
	scintilla_view_recent_instance = view;

	return reinterpret_cast<const char *>(view->editor.WndProc(Message::GetRangePointer, start, length));
}

gsize scintilla_view_get_gap_position(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::GetGapPosition, 0, 0);
}

/**
 * scintilla_view_set_save_point:
 * @view: A Scintilla view
 */
void scintilla_view_set_save_point(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetSavePoint, 0, 0);
}

/**
 * scintilla_view_get_modified:
 * @view: A Scintilla view
 *
 * Returns: Whether the text has been modified since last save point.
 */
gboolean scintilla_view_get_modified(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), false);
	scintilla_view_recent_instance = view;

	return !!view->editor.WndProc(Message::GetModify, 0, 0);
}

gsize scintilla_view_count_characters(ScintillaView *view, gsize start, gsize end) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	g_return_val_if_fail(end >= start, 0);
	scintilla_view_recent_instance = view;

	return static_cast<gsize>(view->editor.WndProc(Message::CountCharacters, start, end));
}

/**
 * scintilla_view_line_length:
 * @view: A Scintilla view
 * @line: index of line
 */
gsize scintilla_view_line_length(ScintillaView *view, gsize line) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return static_cast<gsize>(view->editor.WndProc(Message::LineLength, 0, 0));
}

/**
 * scintilla_view_get_line:
 * @view: A Scintilla view
 * @line: index of line
 * @buffer: (optional) (out caller-allocates) (array zero-terminated=0) (element-type gchar): a buffer to read the line into
 *
 * Returns: the length of the line.
 */
gsize scintilla_view_get_line(ScintillaView *view, gsize line, char *buffer) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return static_cast<gsize>(view->editor.WndProc(Message::GetLine, line, reinterpret_cast<sptr_t>(buffer)));
}

/**
 * scintilla_view_get_lines_count:
 * @view: A Scintilla view
 *
 * Returns the total number of lines in the current document.
 */
gsize scintilla_view_get_lines_count(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return static_cast<int>(view->editor.WndProc(Message::GetLineCount, 0, 0));
}

ScintillaFindOption scintilla_view_get_search_flags(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_FIND_NONE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaFindOption>(view->editor.WndProc(Message::GetSearchFlags, 0, 0));
}

void scintilla_view_set_search_flags(ScintillaView *view, ScintillaFindOption flags) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetSearchFlags, static_cast<uptr_t>(flags), 0);
}


/**
 * scintilla_view_insert_text:
 * @view: A Scintilla view
 * @position: A position in the buffer, or -1 to use the current position
 * @text: (nullable): The text to insert
 */
void scintilla_view_insert_text(ScintillaView *view, gssize position, const char *text) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	g_return_if_fail(position >= -1);
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::InsertText, position, reinterpret_cast<sptr_t>(text));
}

/**
 * scintilla_view_append_text:
 * @view: A Scintilla view
 * @text: (array length=length) (element-type gchar): characters to insert
 * @length: length of `text`
 */
void scintilla_view_append_text(ScintillaView *view, const char *text, gsize length) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	g_return_if_fail(text);
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::AppendText, length, reinterpret_cast<sptr_t>(text));
}

/**
 * scintilla_view_clear_all:
 * @view: A Scintilla view
 */
void scintilla_view_clear_all(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::ClearAll, 0, 0);
}

/**
 * scintilla_view_delete_range:
 * @view: A Scintilla view
 * @position: Position to delete from
 * @length: Number of characters to delete
 */
void scintilla_view_delete_range(ScintillaView *view, gsize position, gsize length) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::DeleteRange, position, length);
}

int scintilla_view_get_tab_width(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return static_cast<int>(view->editor.WndProc(Message::GetTabWidth, 0, 0));
}

void scintilla_view_set_tab_width(ScintillaView *view, int tab_width) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	g_return_if_fail(tab_width > 0);
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetTabWidth, tab_width, 0);
}

gboolean scintilla_view_get_eol_filled(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), false);
	scintilla_view_recent_instance = view;

	return !!view->editor.WndProc(Message::GetSelEOLFilled, 0, 0);
}

void scintilla_view_set_eol_filled(ScintillaView *view, gboolean eol_filled) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetSelEOLFilled, eol_filled, 0);
}

ScintillaCaretStyle scintilla_view_get_caret_style(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_CARET_STYLE_LINE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaCaretStyle>(view->editor.WndProc(Message::GetCaretStyle, 0, 0));
}

void scintilla_view_set_caret_style(ScintillaView *view, ScintillaCaretStyle style) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	auto line_or_block = SCINTILLA_CARET_STYLE_LINE | SCINTILLA_CARET_STYLE_BLOCK;
	g_return_if_fail((style & line_or_block) != line_or_block);
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetCaretStyle, static_cast<uptr_t>(style), 0);
}

ScintillaCaretSticky scintilla_view_get_caret_sticky(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_CARET_STICKY_OFF);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaCaretSticky>(view->editor.WndProc(Message::GetCaretSticky, 0, 0));
}

void scintilla_view_set_caret_sticky(ScintillaView *view, ScintillaCaretSticky sticky) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetCaretSticky, static_cast<uptr_t>(sticky), 0);
}

void scintilla_view_toggle_caret_sticky(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::ToggleCaretSticky, 0, 0);
}

ScintillaWrap scintilla_view_get_wrap_mode(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_WRAP_NONE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaWrap>(view->editor.WndProc(Message::GetWrapMode, 0, 0));
}

void scintilla_view_set_wrap_mode(ScintillaView *view, ScintillaWrap wrap_mode) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetWrapMode, static_cast<uptr_t>(wrap_mode), 0);
}

ScintillaWrapVisualFlag scintilla_view_get_wrap_visual_flags(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_WRAP_VISUAL_NONE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaWrapVisualFlag>(view->editor.WndProc(Message::GetWrapVisualFlags, 0, 0));
}

void scintilla_view_set_wrap_visual_flags(ScintillaView *view, ScintillaWrapVisualFlag wrap_visual_flags) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetWrapVisualFlags, static_cast<uptr_t>(wrap_visual_flags), 0);
}

ScintillaWrapVisualLocation scintilla_view_get_wrap_visual_flags_location(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_WRAP_VISUAL_LOCATION_DEFAULT);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaWrapVisualLocation>(view->editor.WndProc(Message::GetWrapVisualFlagsLocation, 0, 0));
}

void scintilla_view_set_wrap_visual_flags_location(ScintillaView *view, ScintillaWrapVisualLocation wrap_visual_flags_location) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetWrapVisualFlagsLocation, static_cast<uptr_t>(wrap_visual_flags_location), 0);
}

ScintillaWrapIndentMode scintilla_view_get_wrap_indent_mode(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_WRAP_INDENT_MODE_FIXED);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaWrapIndentMode>(view->editor.WndProc(Message::GetWrapIndentMode, 0, 0));
}

void scintilla_view_set_wrap_indent_mode(ScintillaView *view, ScintillaWrapIndentMode wrap_indent_mode) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetWrapIndentMode, static_cast<uptr_t>(wrap_indent_mode), 0);
}

int scintilla_view_get_wrap_start_indent(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::GetWrapStartIndent, 0, 0);
}

void scintilla_view_set_wrap_start_indent(ScintillaView *view, int indent) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetWrapStartIndent, indent, 0);
}

gboolean scintilla_view_get_overtype(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), false);
	scintilla_view_recent_instance = view;

	return !!view->editor.WndProc(Message::GetOvertype, 0, 0);
}

void scintilla_view_set_overtype(ScintillaView *view, gboolean overtype) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetOvertype, overtype, 0);
}

void scintilla_view_start_record(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::StartRecord, 0, 0);
}

void scintilla_view_stop_record(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::StopRecord, 0, 0);
}

/**
 * scintilla_view_get_lexer_id:
 * @view: A Scintilla view
 *
 * Returns the ID of the current lexer, as returned from its `GetIdentifier()` method.
 */
int scintilla_view_get_lexer_id(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::GetLexer, 0, 0);
}

/**
 * ILexer5: (foreign) (skip)
 */

/**
 * scintilla_view_set_lexer:
 * @view: A Scintilla view
 * @lexer: (type gpointer) (transfer full): A lexer instance, as created by `CreateLexer()` in Lexilla.
 */
void scintilla_view_set_lexer(ScintillaView *view, ILexer5 *lexer) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetILexer, 0, reinterpret_cast<sptr_t>(lexer));
}

/**
 * scintilla_view_eol_annotation_set_text:
 * @view: A Scintilla view
 * @line:
 * @text: (nullable):
 */
void scintilla_view_eol_annotation_set_text(ScintillaView *view, gsize line, const char *text) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::EOLAnnotationSetText, line, reinterpret_cast<sptr_t>(text));
}

/**
 * scintilla_view_eol_annotation_get_text:
 * @view: A Scintilla view
 * @line:
 * @buffer: (out caller-allocates) (array) (element-type gchar):
 */
gsize scintilla_view_eol_annotation_get_text(ScintillaView *view, gsize line, char *buffer) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), 0);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::EOLAnnotationGetText, line, reinterpret_cast<sptr_t>(buffer));
}

void scintilla_view_eol_annotation_clear_all(ScintillaView *view) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::EOLAnnotationClearAll, 0, 0);
}

/**
 * scintilla_view_eol_annotation_set_visible: (set-property eol-annotation-visible)
 */
void scintilla_view_eol_annotation_set_visible(ScintillaView *view, ScintillaEOLAnnotationVisible visible) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::EOLAnnotationSetVisible, static_cast<uptr_t>(visible), 0);
}

/**
 * scintilla_view_eol_annotation_get_visible: (get-property eol-annotation-visible)
 */
ScintillaEOLAnnotationVisible scintilla_view_eol_annotation_get_visible(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_EOL_ANNOTATION_VISIBLE_HIDDEN);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaEOLAnnotationVisible>(view->editor.WndProc(Message::EOLAnnotationGetVisible, 0, 0));
}

ScintillaAutomaticFold scintilla_view_get_automatic_fold(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_AUTOMATIC_FOLD_NONE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaAutomaticFold>(view->editor.WndProc(Message::GetAutomaticFold, 0, 0));
}

void scintilla_view_set_automatic_fold(ScintillaView *view, ScintillaAutomaticFold fold) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetAutomaticFold, static_cast<uptr_t>(fold), 0);
}

ScintillaFoldFlag scintilla_view_get_fold_flags(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), SCINTILLA_FOLD_NONE);
	scintilla_view_recent_instance = view;

	return static_cast<ScintillaFoldFlag>(view->editor.foldFlags);
}

void scintilla_view_set_fold_flags(ScintillaView *view, ScintillaFoldFlag flags) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetFoldFlags, static_cast<uptr_t>(flags), 0);
}

gboolean scintilla_view_get_multiple_selection(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), FALSE);
	scintilla_view_recent_instance = view;

	return view->editor.WndProc(Message::GetMultipleSelection, 0, 0);
}

void scintilla_view_set_multiple_selection(ScintillaView *view, gboolean multiple_selection) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	view->editor.WndProc(Message::SetMultipleSelection, multiple_selection, 0);
}

void ScintillaView::activate_text_undo(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.Undo();
}

void ScintillaView::activate_text_redo(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.Redo();
}

void ScintillaView::activate_clipboard_cut(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.Cut();
}

void ScintillaView::activate_clipboard_copy(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.Copy();
}

void ScintillaView::activate_clipboard_paste(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.Paste();
}

void ScintillaView::activate_selection_delete(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	// See Message::Clear:
	view->editor.Clear();
	view->editor.SetLastXChosen();
	view->editor.EnsureCaretVisible();
}

void ScintillaView::activate_selection_select_all(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.SelectAll();
}

void ScintillaView::activate_text_clear(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	view->editor.ClearAll();
}

void ScintillaView::activate_menu_popup(GtkWidget *widget, const char *action_name, GVariant *parameter) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(widget);
	scintilla_view_recent_instance = view;

	open_context_menu(view, -1, -1);
}

void ScintillaView::dispose(GObject *object) noexcept {
	scintilla_view_recent_instance = nullptr;
	ScintillaView *view = SCINTILLA_VIEW(object);
	g_clear_object(&view->adj[GTK_ORIENTATION_HORIZONTAL]);
	g_clear_object(&view->adj[GTK_ORIENTATION_VERTICAL]);
	g_clear_object(&view->im_context);
	g_clear_pointer(&view->context_menu, gtk_widget_unparent);
	try {
		view->editor.Finalise();
	} catch (...) {
		g_critical("Editor::Finalise threw an exception");
	}
	G_OBJECT_CLASS(scintilla_view_parent_class)->dispose(object);
}

void ScintillaView::finalize(GObject *object) noexcept {
	scintilla_view_recent_instance = nullptr;
	ScintillaView *view = SCINTILLA_VIEW(object);
	try {
		view->editor.~ScintillaGTK();
	} catch (...) {
		g_critical("Editor destructor threw an exception");
	}
	G_OBJECT_CLASS(scintilla_view_parent_class)->finalize(object);
}

static void scintilla_view_class_init(ScintillaViewClass *klass) /* noexcept */ {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->dispose = ScintillaView::dispose;
	object_class->finalize = ScintillaView::finalize;
	object_class->get_property = scintilla_view_get_property;
	object_class->set_property = scintilla_view_set_property;

	widget_class->realize = ScintillaView::realize;
	widget_class->unrealize = ScintillaView::unrealize;
	widget_class->measure = ScintillaView::measure;
	widget_class->size_allocate = ScintillaView::size_allocate;
	widget_class->snapshot = ScintillaView::snapshot;
	widget_class->css_changed = ScintillaView::css_changed;

	props[PROP_IM_MODULE] = g_param_spec_string("im-module", nullptr, nullptr, nullptr,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_INPUT_HINTS] = g_param_spec_flags("input-hints", nullptr, nullptr,
		GTK_TYPE_INPUT_HINTS, GTK_INPUT_HINT_NONE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_INPUT_PURPOSE] = g_param_spec_enum("input-purpose", nullptr, nullptr,
		GTK_TYPE_INPUT_PURPOSE, GTK_INPUT_PURPOSE_FREE_FORM,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_EDITABLE] = g_param_spec_boolean("editable", nullptr, nullptr, TRUE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_TAB_WIDTH] = g_param_spec_int("tab-width", nullptr, nullptr, 1, G_MAXINT, 8,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_EOL_FILLED] = g_param_spec_boolean("eol-filled", nullptr, nullptr, FALSE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_LEXER_ID] = g_param_spec_int("lexer-id", nullptr, nullptr, 0, G_MAXINT, 0,
		GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_MODIFIED] = g_param_spec_boolean("modified", nullptr, nullptr, FALSE,
		GParamFlags(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_OVERTYPE] = g_param_spec_boolean("overtype", nullptr, nullptr, FALSE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_SEARCH_FLAGS] = g_param_spec_flags("search-flags", nullptr, nullptr,
		SCINTILLA_TYPE_FIND_OPTION, SCINTILLA_FIND_NONE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_CARET_STYLE] = g_param_spec_flags("caret-style", nullptr, nullptr,
		SCINTILLA_TYPE_CARET_STYLE, SCINTILLA_CARET_STYLE_LINE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_CARET_STICKY] = g_param_spec_enum("caret-sticky", nullptr, nullptr,
		SCINTILLA_TYPE_CARET_STICKY, SCINTILLA_CARET_STICKY_OFF,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_DOCUMENT] = g_param_spec_boxed("document", nullptr, nullptr, SCINTILLA_TYPE_DOCUMENT,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_WRAP_MODE] = g_param_spec_enum("wrap-mode", nullptr, nullptr,
		SCINTILLA_TYPE_WRAP, SCINTILLA_WRAP_NONE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_WRAP_VISUAL_FLAGS] = g_param_spec_flags("wrap-visual-flags", nullptr, nullptr,
		SCINTILLA_TYPE_WRAP_VISUAL_FLAG, SCINTILLA_WRAP_VISUAL_NONE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_WRAP_VISUAL_FLAGS_LOCATION] = g_param_spec_flags("wrap-visual-flags-location", nullptr, nullptr,
		SCINTILLA_TYPE_WRAP_VISUAL_LOCATION, SCINTILLA_WRAP_VISUAL_LOCATION_DEFAULT,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_WRAP_INDENT_MODE] = g_param_spec_enum("wrap-indent-mode", nullptr, nullptr,
		SCINTILLA_TYPE_WRAP_INDENT_MODE, SCINTILLA_WRAP_INDENT_MODE_FIXED,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_WRAP_START_INDENT] = g_param_spec_int("wrap-start-indent", nullptr, nullptr, G_MININT, G_MAXINT, 0,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	/**
	 * ScintillaView:eol-annotation-visible: (getter eol_annotation_get_visible) (setter eol_annotation_set_visible)
	 */
	props[PROP_EOL_ANNOTATION_VISIBLE] = g_param_spec_enum("eol-annotation-visible", nullptr, nullptr,
		SCINTILLA_TYPE_EOL_ANNOTATION_VISIBLE, SCINTILLA_EOL_ANNOTATION_VISIBLE_HIDDEN,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_AUTOMATIC_FOLD] = g_param_spec_flags("automatic-fold", nullptr, nullptr,
		SCINTILLA_TYPE_AUTOMATIC_FOLD, SCINTILLA_AUTOMATIC_FOLD_SHOW | SCINTILLA_AUTOMATIC_FOLD_CHANGE | SCINTILLA_AUTOMATIC_FOLD_CLICK,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_FOLD_FLAGS] = g_param_spec_flags("fold-flags", nullptr, nullptr,
		SCINTILLA_TYPE_FOLD_FLAG, SCINTILLA_FOLD_NONE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));
	props[PROP_MULTIPLE_SELECTION] = g_param_spec_boolean("multiple-selection", nullptr, nullptr, FALSE,
		GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY));

	g_object_class_install_properties(object_class, N_PROPS, props);

	g_object_class_override_property(object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property(object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property(object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property(object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	signals[SIGNAL_SCI_NOTIFY] = g_signal_new("sci-notify",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
		scintilla_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		SCINTILLA_TYPE_NOTIFICATION | G_SIGNAL_TYPE_STATIC_SCOPE);
	g_signal_set_va_marshaller(signals[SIGNAL_SCI_NOTIFY], G_OBJECT_CLASS_TYPE(object_class),
		scintilla_marshal_VOID__BOXEDv);

	/**
	 * ScintillaView::macro-record:
	 * @self: A Scintilla view
	 * @message: Message code
	 * @wParam: First message argument
	 * @lParam: Second message argument
	 *
	 * Emitted when a message is recorded during macro recording. The message
	 * can be replayed back using [method@ScintillaView.send_message].
	 */
	signals[SIGNAL_MACRO_RECORD] = g_signal_new("macro-record",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_LAST, 0, nullptr, nullptr,
		scintilla_marshal_VOID__UINT_UINT64_INT64,
		G_TYPE_NONE, 3,
		G_TYPE_UINT, G_TYPE_UINT64, G_TYPE_INT64);
	g_signal_set_va_marshaller(signals[SIGNAL_MACRO_RECORD], G_OBJECT_CLASS_TYPE(object_class),
		scintilla_marshal_VOID__UINT_UINT64_INT64v);

	signals[SIGNAL_MODIFY_ATTEMPT] = g_signal_new_class_handler("modify-attempt",
		G_OBJECT_CLASS_TYPE(object_class),
		G_SIGNAL_RUN_LAST, G_CALLBACK(ScintillaView::modify_attempt),
		g_signal_accumulator_true_handled, nullptr,
		scintilla_marshal_BOOLEAN__VOID,
		G_TYPE_BOOLEAN, 0);
	g_signal_set_va_marshaller(signals[SIGNAL_MODIFY_ATTEMPT], G_OBJECT_CLASS_TYPE(object_class),
		scintilla_marshal_BOOLEAN__VOIDv);

	gtk_widget_class_set_css_name(widget_class, "scintilla");

	/**
	 * ScintillaView|clipboard.cut:
	 *
	 * Copies selected text to the clipboard and deletes it from the widget.
	 */
	gtk_widget_class_install_action(widget_class, "clipboard.cut", nullptr, ScintillaView::activate_clipboard_cut);

	/**
	 * ScintillaView|clipboard.copy:
	 *
	 * Copies selected text to the clipboard.
	 */
	gtk_widget_class_install_action(widget_class, "clipboard.copy", nullptr, ScintillaView::activate_clipboard_copy);

	/**
	 * ScintillaView|clipboard.paste:
	 *
	 * Inserts the contents of the clipboard into the widget.
	 */
	gtk_widget_class_install_action(widget_class, "clipboard.paste", nullptr, ScintillaView::activate_clipboard_paste);

	/**
	 * ScintillaView|selection.delete:
	 *
	 * Deletes the current selection.
	 */
	gtk_widget_class_install_action(widget_class, "selection.delete", nullptr, ScintillaView::activate_selection_delete);

	/**
	 * ScintillaView|selection.select-all:
	 *
	 * Selects all text.
	 */
	gtk_widget_class_install_action(widget_class, "selection.select-all", nullptr, ScintillaView::activate_selection_select_all);

	/**
	 * ScintillaView|text.clear:
	 *
	 * Deletes all the text.
	 */
	gtk_widget_class_install_action(widget_class, "text.clear", nullptr, ScintillaView::activate_text_clear);

	/**
	 * ScintillaView|text.undo:
	 */
	gtk_widget_class_install_action(widget_class, "text.undo", nullptr, ScintillaView::activate_text_undo);

	/**
	 * ScintillaView|text.redo:
	 */
	gtk_widget_class_install_action(widget_class, "text.undo", nullptr, ScintillaView::activate_text_redo);

	/**
	 * ScintillaView|menu.popup:
	 *
	 * Opens the context menu.
	 */
	gtk_widget_class_install_action(widget_class, "menu.popup", nullptr, ScintillaView::activate_menu_popup);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Cut, GdkModifierType(0), "clipboard.cut", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Copy, GdkModifierType(0), "clipboard.copy", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Paste, GdkModifierType(0), "clipboard.paste", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Undo, GdkModifierType(0), "text.undo", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Redo, GdkModifierType(0), "text.redo", nullptr);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Delete, GDK_SHIFT_MASK, "clipboard.cut", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Insert, GDK_SHIFT_MASK, "clipboard.paste", nullptr);
#ifndef __APPLE__
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Insert, GDK_CONTROL_MASK, "clipboard.copy", nullptr);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_x, GDK_CONTROL_MASK, "clipboard.cut", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_c, GDK_CONTROL_MASK, "clipboard.copy", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_v, GDK_CONTROL_MASK, "clipboard.paste", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_a, GDK_CONTROL_MASK, "selection.select-all", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_z, GDK_CONTROL_MASK, "text.undo", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_y, GDK_CONTROL_MASK, "text.redo", nullptr);
#else
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Insert, GDK_META_MASK, "clipboard.copy", nullptr);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_x, GDK_META_MASK, "clipboard.cut", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_c, GDK_META_MASK, "clipboard.copy", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_v, GDK_META_MASK, "clipboard.paste", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_a, GDK_META_MASK, "selection.select-all", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_z, GDK_META_MASK, "text.undo", nullptr);
#endif
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_z, GdkModifierType(GDK_SHIFT_MASK | GDK_CONTROL_MASK), "text.redo", nullptr);

	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Menu, GdkModifierType(0), "menu.popup", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_F10, GDK_SHIFT_MASK, "menu.popup", nullptr);
	gtk_widget_class_add_binding_action(widget_class, GDK_KEY_Clear, GdkModifierType(0), "text.clear", nullptr);

	gtk_widget_class_set_accessible_role(widget_class, GTK_ACCESSIBLE_ROLE_TEXT_BOX);
}

static void scintilla_view_init(ScintillaView *view) /* noexcept */ {
	GtkWidget *widget = GTK_WIDGET(view);
	scintilla_view_recent_instance = view;

	new(&view->editor) ScintillaGTK(view);

	gtk_widget_set_focusable(widget, TRUE);
	gtk_accessible_update_property(GTK_ACCESSIBLE(view),
		GTK_ACCESSIBLE_PROPERTY_MULTI_LINE, TRUE,
		-1);
	gtk_widget_add_css_class(widget, "view");

	view->im_context = gtk_im_multicontext_new();
	g_signal_connect_swapped(view->im_context, "preedit-start", G_CALLBACK(ScintillaView::im_preedit_start), view);
	g_signal_connect_swapped(view->im_context, "preedit-changed", G_CALLBACK(ScintillaView::im_preedit_changed), view);
	g_signal_connect_swapped(view->im_context, "retrieve-surrounding", G_CALLBACK(ScintillaView::im_retrieve_surrounding), view);
	g_signal_connect_swapped(view->im_context, "delete-surrounding", G_CALLBACK(ScintillaView::im_delete_surrounding), view);
	g_signal_connect_swapped(view->im_context, "commit", G_CALLBACK(ScintillaView::im_commit), view);

	view->key_controller = gtk_event_controller_key_new();
#if GTK_CHECK_VERSION(4, 8, 0)
	gtk_event_controller_set_static_name(view->key_controller, "scintilla-key-controller");
#endif
	g_signal_connect_swapped(view->key_controller, "key-pressed", G_CALLBACK(ScintillaView::key_pressed), view);
	// g_signal_connect_swapped(view->key_controller, "im-update", G_CALLBACK(ScintillaView::im_update), view);
	gtk_event_controller_key_set_im_context(GTK_EVENT_CONTROLLER_KEY(view->key_controller), view->im_context);
	gtk_widget_add_controller(widget, view->key_controller);

	GtkEventController *motion_controller = gtk_event_controller_motion_new();
#if GTK_CHECK_VERSION(4, 8, 0)
	gtk_event_controller_set_static_name(motion_controller, "scintilla-motion-controller");
#endif
	g_signal_connect_swapped(motion_controller, "motion", G_CALLBACK(ScintillaView::motion), view);
	gtk_widget_add_controller(widget, motion_controller);

	GtkGesture *gesture_click = gtk_gesture_click_new();
#if GTK_CHECK_VERSION(4, 8, 0)
	gtk_event_controller_set_static_name(GTK_EVENT_CONTROLLER(gesture_click), "scintilla-click");
#endif
	gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture_click), 0);
	g_signal_connect_swapped(gesture_click, "pressed", G_CALLBACK(ScintillaView::pressed), view);
	g_signal_connect_swapped(gesture_click, "released", G_CALLBACK(ScintillaView::released), view);
	gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(gesture_click));

	GtkEventController *focus_controller = gtk_event_controller_focus_new();
#if GTK_CHECK_VERSION(4, 8, 0)
	gtk_event_controller_set_static_name(focus_controller, "scintilla-focus");
#endif
	g_signal_connect_swapped(focus_controller, "enter", G_CALLBACK(ScintillaView::focus_enter), view);
	g_signal_connect_swapped(focus_controller, "leave", G_CALLBACK(ScintillaView::focus_leave), view);
	gtk_widget_add_controller(widget, focus_controller);

	gtk_widget_set_cursor_from_name(GTK_WIDGET(view), "text");
}

/**
 * scintilla_view_new:
 *
 * Creates a new Scintilla view.
 *
 * Returns: (transfer none): a new Scintilla view
 */
GtkWidget *scintilla_view_new(void) noexcept {
    return GTK_WIDGET(g_object_new(SCINTILLA_TYPE_VIEW, NULL));
}

ScintillaGTK &scintilla_view_get_editor(_ScintillaView *view) noexcept {
	return view->editor;
}

GtkInputHints scintilla_view_get_input_hints(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), GTK_INPUT_HINT_NONE);
	scintilla_view_recent_instance = view;

	GtkInputHints hints;
	g_object_get(G_OBJECT(view->im_context), "input-hints", &hints, nullptr);
	return hints;
}

void scintilla_view_set_input_hints(ScintillaView *view, GtkInputHints hints) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	if (scintilla_view_get_input_hints(view) == hints) {
		return;
	}
	g_object_set(G_OBJECT(view->im_context), "input-hints", hints, nullptr);
	g_object_notify_by_pspec(G_OBJECT(view), props[PROP_INPUT_HINTS]);
}

GtkInputPurpose scintilla_view_get_input_purpose(ScintillaView *view) noexcept {
	g_return_val_if_fail(SCINTILLA_IS_VIEW(view), GTK_INPUT_PURPOSE_FREE_FORM);
	scintilla_view_recent_instance = view;

	GtkInputPurpose purpose;
	g_object_get(G_OBJECT(view->im_context), "input-purpose", &purpose, nullptr);
	return purpose;
}

void scintilla_view_set_input_purpose(ScintillaView *view, GtkInputPurpose purpose) noexcept {
	g_return_if_fail(SCINTILLA_IS_VIEW(view));
	scintilla_view_recent_instance = view;

	if (scintilla_view_get_input_purpose(view) == purpose) {
		return;
	}
	g_object_set(G_OBJECT(view->im_context), "input-purpose", purpose, nullptr);
	g_object_notify_by_pspec(G_OBJECT(view), props[PROP_INPUT_PURPOSE]);
}

#if GTK_CHECK_VERSION(4, 14, 0)
GBytes *ScintillaView::accessible_get_contents(GtkAccessibleText *accessible, unsigned start, unsigned end) noexcept {
	g_return_val_if_fail(start <= end, nullptr);
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	Sci::Position cpMin = start, cpMax = end;
	gsize len;
	if (end != G_MAXUINT) {
		len = end - start;
	} else {
		len = view->editor.WndProc(Message::GetTextLength, 0, 0);
		cpMax = cpMin + len;
	}
	char *buffer = reinterpret_cast<char *>(g_malloc(len + 1));
	auto len2 = view->editor.GetTextRange(buffer, cpMin, cpMax);
	g_assert(len == static_cast<gsize>(len2));
	return g_bytes_new_take(buffer, len + 1);
}

GBytes *ScintillaView::accessible_get_contents_at(GtkAccessibleText *accessible, unsigned offset, GtkAccessibleTextGranularity granularity, unsigned *start, unsigned *end) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

#ifdef DEBUG
	GEnumClass *enum_class = G_ENUM_CLASS(g_type_class_ref(GTK_TYPE_ACCESSIBLE_TEXT_GRANULARITY));
	const char *enum_nick = g_enum_get_value(enum_class, granularity)->value_nick;
	g_type_class_unref(enum_class);
	g_debug("accessible_get_contents_at %u %s", offset, enum_nick);
#endif

	Sci::Position cpMin, cpMax;

	switch (granularity) {
	case GTK_ACCESSIBLE_TEXT_GRANULARITY_CHARACTER: {
			cpMin = offset;
			cpMax = view->editor.WndProc(Message::PositionAfter, offset, 0);
			break;
		}
	case GTK_ACCESSIBLE_TEXT_GRANULARITY_PARAGRAPH:
	case GTK_ACCESSIBLE_TEXT_GRANULARITY_SENTENCE: {
			g_debug("Unimplemented granularity, falling back to line");
			G_GNUC_FALLTHROUGH;
		}
	case GTK_ACCESSIBLE_TEXT_GRANULARITY_LINE: {
			int line = view->editor.pdoc->LineFromPosition(offset);
			cpMin = view->editor.pdoc->LineStart(line);
			cpMax = view->editor.pdoc->LineEnd(line);
			break;
		}
	case GTK_ACCESSIBLE_TEXT_GRANULARITY_WORD: {
			cpMin = view->editor.WndProc(Message::WordStartPosition, offset, 1);
			cpMax = view->editor.WndProc(Message::WordEndPosition, offset, 1);
			break;
		}
	default:
		*start = 0;
		*end = 0;
		return nullptr;
	}

	*start = cpMin;
	*end = cpMax;

	char *buffer = reinterpret_cast<char *>(g_malloc(cpMax - cpMin + 1));
	auto len = view->editor.GetTextRange(buffer, cpMin, cpMax);
	g_assert(len == cpMax - cpMin);
	return g_bytes_new_take(buffer, len + 1);
}

unsigned ScintillaView::accessible_get_caret_position(GtkAccessibleText *accessible) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	return view->editor.CurrentPosition();
}

gboolean ScintillaView::accessible_get_selection(GtkAccessibleText *accessible, gsize *n_ranges, GtkAccessibleTextRange **ranges) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	const Selection &sel = view->editor.sel;
	*n_ranges = sel.Count();
	if (*n_ranges == 0) {
		return false;
	}
	if (ranges) {
		*ranges = g_new(GtkAccessibleTextRange, *n_ranges);
		for (size_t i = 0; i < *n_ranges; i++) {
			const SelectionRange &sci_range = sel.Range(i);
			GtkAccessibleTextRange range {
				static_cast<gsize>(sci_range.Start().Position()),
				static_cast<gsize>(sci_range.Length())
			};
			(*ranges)[i] = range;
		}
	}
	return TRUE;
}
#endif
#if GTK_CHECK_VERSION(4, 16, 0)
gboolean ScintillaView::accessible_get_extents(GtkAccessibleText *accessible, unsigned start, unsigned end, graphene_rect_t *extents) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	PRectangle rc = view->editor.RectangleFromRange({ start, end }, 0);
	graphene_rect_t rect {
		{ static_cast<float>(rc.left), static_cast<float>(rc.top) },
		{ static_cast<float>(rc.Width()), static_cast<float>(rc.Height()) }
	};
	*extents = rect;
	return TRUE;
}

gboolean ScintillaView::accessible_get_offset(GtkAccessibleText *accessible, const graphene_point_t *point, unsigned *offset) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	Sci::Position pos = view->editor.PositionFromLocation(Point { point->x, point->y }, true);
	if (pos == Sci::invalidPosition) {
		return false;
	}
	*offset = pos;
	return TRUE;
}
#endif
#if GTK_CHECK_VERSION(4, 22, 0)
gboolean ScintillaView::accessible_set_caret_position(GtkAccessibleText *accessible, unsigned offset) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	view->editor.MovePositionTo(offset);
	return TRUE;
}

gboolean ScintillaView::accessible_set_selection(GtkAccessibleText *, gsize i, GtkAccessibleTextRange *range) noexcept {
	ScintillaView *view = SCINTILLA_VIEW(accessible);
	scintilla_view_recent_instance = view;

	if (i != 0) {
		// TODO
		return FALSE;
	}

	view->editor.SetSelection(range->start, range->start + range->length);
	return TRUE;
}
#endif

// GObject boxed type plumbing for SCNotification. Note that these should not
// normally get called in normal usage because of G_SIGNAL_TYPE_STATIC_SCOPE,
// but are required to register a proper boxed type as opposed to a pointer type.

static void scintilla_notification_free(SCNotification *scn) noexcept {
#if GLIB_CHECK_VERSION(2, 76, 0)
	g_free_sized(scn, sizeof(SCNotification));
#else
	g_free(scn);
#endif
}

static SCNotification *scintilla_notification_copy(SCNotification *scn) noexcept {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	// We know sizeof(SCNotification) doesn't overflow guint.
	return reinterpret_cast<SCNotification *>(g_memdup(scn, sizeof(SCNotification)));
G_GNUC_END_IGNORE_DEPRECATIONS
}

G_DEFINE_BOXED_TYPE(SCNotification, scintilla_notification, scintilla_notification_copy, scintilla_notification_free)
