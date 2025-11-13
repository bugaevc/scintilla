#include "ScintillaGTK.h"
#include "ScintillaView.h"
#include "Internals.h"
#include "Scintilla.h"

namespace Scintilla::Internal {

static ColourRGBA RemoveWhiteBackground(ColourRGBA source) {
	if (source.GetAlpha() != maximumByte) {
		// This already has some transparency, leave it alone.
		return source;
	}
	unsigned char m = std::min({ source.GetRed(), source.GetBlue(), source.GetGreen() });
	return ColourRGBA(source.GetRed() - m, source.GetGreen() - m, source.GetBlue() - m, maximumByte - m);
}

ScintillaGTK::ScintillaGTK(struct _ScintillaView *widget) {
	wMain = widget;
	view.bufferedDraw = false;
	trackLineWidth = true;
	foldAutomatic = static_cast<AutomaticFold>(
		static_cast<int>(AutomaticFold::Show) |
		static_cast<int>(AutomaticFold::Change) |
		static_cast<int>(AutomaticFold::Click)
	);
	for (Style &style : vs.styles) {
		style.back = RemoveWhiteBackground(style.back);
	}
}

inline ScintillaView *ScintillaGTK::widget() const {
	return SCINTILLA_VIEW(wMain.GetID());
}

void ScintillaGTK::ScrollTo(double adj_value) {
	Sci::Line newTopLine = adj_value / vs.lineHeight;
	ScintillaBase::ScrollTo(newTopLine, false);
}

void ScintillaGTK::Redraw() {
	Editor::Redraw();
	if (redrawPendingText) {
		gtk_widget_queue_allocate(GTK_WIDGET(widget()));
	}
}

void ScintillaGTK::SetVerticalScrollPos() {
	DwellEnd(true); // ???

	GtkScrollable *scrollable = GTK_SCROLLABLE(wMain.GetID());
	GtkAdjustment *adj = gtk_scrollable_get_vadjustment(scrollable);
	double min = topLine * vs.lineHeight;
	double max = (topLine + 0.7) * vs.lineHeight;
	double value = gtk_adjustment_get_value(adj);
	if (value < min) {
		gtk_adjustment_set_value(adj, min);
	} else if (value > max) {
		// Note: min, not max.
		gtk_adjustment_set_value(adj, min);
	}

	// Always queue an allocate, even if the value didn't actually change.
	// This gets rid of the needUpdateUI flag.
	gtk_widget_queue_allocate(GTK_WIDGET(widget()));
}

void ScintillaGTK::SetScrollBars() {
	Editor::SetScrollBars();
	// This is called when our size changes, hence:
	if (!in_measure) {
		gtk_widget_queue_resize(GTK_WIDGET(widget()));
	}
}

void ScintillaGTK::SetHorizontalScrollPos() {
	DwellEnd(true); // ???

	GtkScrollable *scrollable = GTK_SCROLLABLE(wMain.GetID());
	GtkAdjustment *adj = gtk_scrollable_get_hadjustment(scrollable);
	gtk_adjustment_set_value(adj, xOffset);

	// Always queue an allocate, even if the value didn't actually change.
	// This gets rid of the needUpdateUI flag.
	gtk_widget_queue_allocate(GTK_WIDGET(widget()));
}

bool ScintillaGTK::ModifyScrollBars(Sci::Line nMax, Sci::Line nPage) {
	bool modified = false;
	ScintillaView *view = widget();
	GtkAdjustment *hadj, *vadj;
	g_object_get(view,
		"hadjustment", &hadj,
		"vadjustment", &vadj,
		nullptr);

	double v_page_size = GetTextRectangle().Height();
	double v_page_increment = LinesToScroll() * vs.lineHeight;
	double v_upper = std::max(static_cast<double>((nMax + 1) * vs.lineHeight), v_page_size);
	if (gtk_adjustment_get_upper(vadj) != v_upper ||
			gtk_adjustment_get_page_size(vadj) != v_page_size ||
			gtk_adjustment_get_page_increment(vadj) != v_page_increment) {
		gtk_adjustment_configure(vadj, gtk_adjustment_get_value(vadj), 0, v_upper,
			v_page_size * 0.1, v_page_increment, v_page_size);
		modified = true;
	}

	double h_page_size = GetTextRectangle().Width();
	double h_step_increment = vs.styles[STYLE_DEFAULT].aveCharWidth;
	double upper;
	if (Wrapping()) {
		upper = h_page_size;
	} else {
		upper = std::max(static_cast<double>(scrollWidth), h_page_size);
	}
	if (gtk_adjustment_get_upper(hadj) != upper ||
			gtk_adjustment_get_page_size(hadj) != h_page_size ||
			gtk_adjustment_get_step_increment(hadj) != h_step_increment) {
		gtk_adjustment_configure(hadj, gtk_adjustment_get_value(hadj), 0, upper,
			h_step_increment, h_page_size * 0.9, h_page_size);
		modified = true;
	}

	return modified;
}

/*
Point ScintillaGTK::GetVisibleOriginInMain() const {
	ScintillaView *view = widget();
	GtkAdjustment *hadj, *vadj;
	g_object_get(view,
		"hadjustment", &hadj,
		"vadjustment", &vadj,
		nullptr);
	double x = gtk_adjustment_get_value(hadj);
	double y = gtk_adjustment_get_value(vadj);
	g_object_unref(hadj);
	g_object_unref(vadj);
	return Point(x, y);
}
*/

struct ScintillaSelectionText {
	SelectionText selection_text;
	unsigned rc { 1 };
};

static void scintilla_selection_text_unref(ScintillaSelectionText *st) noexcept {
	st->rc--;
	if (st->rc == 0) {
		delete st;
	}
}

static ScintillaSelectionText *scintilla_selection_text_ref(ScintillaSelectionText *st) noexcept {
	st->rc++;
	return st;
}

#define SCINTILLA_TYPE_SELECTION_TEXT scintilla_selection_text_get_type()
G_DEFINE_BOXED_TYPE(ScintillaSelectionText, scintilla_selection_text, scintilla_selection_text_ref, scintilla_selection_text_unref)

static void text_plain_serializer_finish(GObject *obj, GAsyncResult *result, gpointer data) noexcept {
	GOutputStream *stream = G_OUTPUT_STREAM(obj);
	GdkContentSerializer *serializer = GDK_CONTENT_SERIALIZER(data);
	GError *error = nullptr;

	bool ok = g_output_stream_write_all_finish(stream, result, nullptr, &error);
	if (ok) {
		gdk_content_serializer_return_success(serializer);
	} else {
		gdk_content_serializer_return_error(serializer, error);
	}
}

static void text_plain_serializer(GdkContentSerializer *serializer) noexcept {
	const GValue *value = gdk_content_serializer_get_value(serializer);
	ScintillaSelectionText *st = reinterpret_cast<ScintillaSelectionText *>(g_value_get_boxed(value));
	if (st == nullptr || st->selection_text.Empty()) {
		gdk_content_serializer_return_success(serializer);
		return;
	}
	GOutputStream *stream = gdk_content_serializer_get_output_stream(serializer);
	int io_priority = gdk_content_serializer_get_priority(serializer);
	GCancellable *cancellable = gdk_content_serializer_get_cancellable(serializer);

	g_output_stream_write_all_async(stream, st->selection_text.Data(), st->selection_text.Length(),
		io_priority, cancellable, text_plain_serializer_finish, serializer);
}

static void register_serializers() noexcept {
	static bool registered = false;
	if (registered) {
		return;
	}
	gdk_content_register_serializer(SCINTILLA_TYPE_SELECTION_TEXT, "text/plain;charset=utf-8",
		text_plain_serializer, nullptr, nullptr);
	registered = true;
}

void ScintillaGTK::ClaimSelection() {}

void ScintillaGTK::DoPaste(const GValue *value) noexcept {
	UndoGroup ug(pdoc);
	ClearSelection(multiPasteMode == MultiPaste::Each);

	if (G_VALUE_HOLDS(value, SCINTILLA_TYPE_SELECTION_TEXT)) {
		const ScintillaSelectionText *sc = reinterpret_cast<const ScintillaSelectionText *>(g_value_get_boxed(value));
		const SelectionText &selectedText = sc->selection_text;

		PasteShape pasteShape;
		if (selectedText.rectangular) {
			pasteShape = PasteShape::rectangular;
		} else if (selectedText.lineCopy) {
			pasteShape = PasteShape::line;
		} else {
			pasteShape = PasteShape::stream;
		}
		InsertPasteShape(selectedText.Data(), selectedText.Length(), pasteShape);
	} else if (G_VALUE_HOLDS(value, G_TYPE_STRING)) {
		const char *text = g_value_get_string(value);
		InsertPasteShape(text, strlen(text), PasteShape::stream);
	} else {
		g_return_if_reached();
	}
}

void ScintillaGTK::ReadReady(GObject *source_object, GAsyncResult *res, gpointer data) noexcept {
	GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);
	ScintillaView *view = SCINTILLA_VIEW(data);
	scintilla_view_recent_instance = view;
	ScintillaGTK &self = scintilla_view_get_editor(view);
	GError *error = nullptr;

	const GValue *value = gdk_clipboard_read_value_finish(clipboard, res, &error);
	if (G_UNLIKELY(error)) {
		g_warning("Failed to read from clipboard: %s", error->message);
		g_error_free(error);
		gtk_widget_error_bell(GTK_WIDGET(view));
		g_object_unref(view);
		return;
	}

	self.DoPaste(value);

	// See WndProc - Message::Paste
	if (self.caretSticky == CaretSticky::Off || self.caretSticky == CaretSticky::WhiteSpace) {
		self.SetLastXChosen();
	}
	self.EnsureCaretVisible();

	g_object_unref(view);
}

bool ScintillaGTK::CanPaste() {
	if (!ScintillaBase::CanPaste()) {
		return false;
	}
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(widget()));
	GdkContentFormats *formats = gdk_clipboard_get_formats(clipboard);
	return gdk_content_formats_contain_gtype(formats, SCINTILLA_TYPE_SELECTION_TEXT) ||
	       gdk_content_formats_contain_gtype(formats, G_TYPE_STRING);
}

void ScintillaGTK::Paste() {
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(widget()));
	Paste(clipboard);
}

void ScintillaGTK::Paste(GdkClipboard *clipboard) {
	GdkContentFormats *formats = gdk_clipboard_get_formats(clipboard);
	GType type;
	if (gdk_content_formats_contain_gtype(formats, SCINTILLA_TYPE_SELECTION_TEXT)) {
		type = SCINTILLA_TYPE_SELECTION_TEXT;
	} else if (gdk_content_formats_contain_gtype(formats, G_TYPE_STRING)) {
		type = G_TYPE_STRING;
	} else {
		g_warning("Cannot paste, no supported format");
		return;
	}

	if (gdk_clipboard_is_local(clipboard)) {
		// Try pasting synchronously.
		GdkContentProvider *provider = gdk_clipboard_get_content(clipboard);
		if (!provider) {
			goto paste_async;
		}
		GError *error = nullptr;
		GValue value = G_VALUE_INIT;
		g_value_init(&value, type);
		if (gdk_content_provider_get_value(provider, &value, &error)) {
			DoPaste(&value);
			g_value_unset(&value);
			return;
		} else if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
			g_value_unset(&value);
			g_error_free(error);
			goto paste_async;
		} else {
			g_value_unset(&value);
			g_warning("Failed to read from clipboard: %s", error->message);
			g_error_free(error);
			gtk_widget_error_bell(GTK_WIDGET(widget()));
			return;
		}
	} else {
	paste_async:
		GCancellable *cancellable = nullptr; // TODO
		gdk_clipboard_read_value_async(clipboard, type, G_PRIORITY_DEFAULT,
			cancellable, ReadReady, g_object_ref(widget()));
	}
}

void ScintillaGTK::Copy() {
	if (sel.Empty()) {
		return;
	}
	register_serializers();
	ScintillaSelectionText *st = new ScintillaSelectionText;
	CopySelectionRange(&st->selection_text);
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(widget()));
	gdk_clipboard_set(clipboard, SCINTILLA_TYPE_SELECTION_TEXT, st);
	scintilla_selection_text_unref(st);
}

void ScintillaGTK::CopyToClipboard(const SelectionText &selectedText) {
	register_serializers();
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(widget()));
	ScintillaSelectionText *st = new ScintillaSelectionText;
	st->selection_text.Copy(selectedText);
	gdk_clipboard_set(clipboard, SCINTILLA_TYPE_SELECTION_TEXT, st);
	scintilla_selection_text_unref(st);
}

void ScintillaGTK::NotifyChange() {}
void ScintillaGTK::NotifyParent(Scintilla::NotificationData scn) {
	scn.nmhdr.hwndFrom = widget();
	scn.nmhdr.idFrom = GetCtrlID();
	scintilla_view_sci_notify(widget(), &scn);
}

void ScintillaGTK::SetMouseCapture(bool b) {
	// We always have GtkEventControllerMotion, ...
	haveMouseCapture = b;
}
bool ScintillaGTK::HaveMouseCapture() {
	// ...but we need to return the set values for the various logic
	// inside Scintilla to work out.
	return haveMouseCapture;
}

std::string ScintillaGTK::UTF8FromEncoded(std::string_view encoded) const {
	if (IsUnicodeMode()) {
		return std::string(encoded);
	} else {
		abort();
		// return ConvertText(encoded.data(), encoded.length(), "UTF-8", CharacterSetID(), true);
	}
}

std::string ScintillaGTK::EncodedFromUTF8(std::string_view utf8) const {
	if (IsUnicodeMode()) {
		return std::string(utf8);
	} else {
		abort();
		// return ConvertText(utf8.data(), utf8.length(), CharacterSetID(), "UTF-8", true);
	}
}

sptr_t ScintillaGTK::WndProc(Message iMessage, uptr_t wParam, sptr_t lParam) {
	switch (iMessage) {
	case Message::GrabFocus:
		gtk_widget_grab_focus(GTK_WIDGET(widget()));
		return 0;
	default:
		break;
	}

	sptr_t res = ScintillaBase::WndProc(iMessage, wParam, lParam);

	const char *prop;
	switch (iMessage) {
	case Message::SetReadOnly:
		prop = "editable";
		scintilla_view_notify_editable(SCINTILLA_VIEW(widget()), wParam == 0);
		break;
	case Message::SetTabWidth:
		prop = "tab-width";
		break;
	case Message::SetSelEOLFilled:
		prop = "eol-filled";
		break;
	case Message::SetILexer:
		prop = "lexer-id";
		break;
/*
	case Message::SetSavePoint:
		prop = "modified";
*/
	case Message::EditToggleOvertype:
	case Message::SetOvertype:
		prop = "overtype";
		break;
	case Message::SetSearchFlags:
		prop = "search-flags";
		break;
	case Message::SetCaretStyle:
		prop = "caret-style";
		break;
	case Message::SetCaretSticky:
	case Message::ToggleCaretSticky:
		prop = "caret-sticky";
		break;
	case Message::SetDocPointer:
		prop = "document";
		break;
	case Message::SetWrapMode:
		prop = "wrap-mode";
		break;
	case Message::SetWrapVisualFlags:
		prop = "wrap-visual-flags";
		break;
	case Message::SetWrapVisualFlagsLocation:
		prop = "wrap-visual-flags-location";
		break;
	case Message::SetWrapIndentMode:
		prop = "wrap-indent-mode";
		break;
	case Message::SetWrapStartIndent:
		prop = "wrap-start-indent";
		break;
	case Message::EOLAnnotationSetVisible:
		prop = "eol-annotation-visible";
		break;
	case Message::SetAutomaticFold:
		prop = "automatic-fold";
		break;
	case Message::SetFoldFlags:
		prop = "fold-flags";
		break;
	case Message::SetMultipleSelection:
		prop = "multiple-selection";
		break;
	default:
		prop = nullptr;
		break;
	}
	if (prop) {
		g_object_notify(G_OBJECT(widget()), prop);
	}
	return res;
}

sptr_t ScintillaGTK::DefWndProc(Message iMessage, uptr_t wParam, sptr_t lParam) {
	return 0;
}

void ScintillaGTK::CreateCallTipWindow(PRectangle rc) {}
void ScintillaGTK::AddToPopUp(const char *label, int cmd, bool enabled) {}

ScintillaGTK::Ticker::~Ticker() {
	if (source_id) {
		g_source_remove(source_id);
	}
}

bool ScintillaGTK::FineTickerRunning(TickReason reason) {
	return tickers[static_cast<size_t>(reason)].source_id != 0;
}

gboolean ScintillaGTK::FineTickerCallback(ScintillaGTK::Ticker *ticker) {
	scintilla_view_recent_instance = ticker->sc->widget();
	TickReason reason = static_cast<TickReason>(ticker - ticker->sc->tickers);
	ticker->sc->TickFor(reason);
	return G_SOURCE_CONTINUE;
}

void ScintillaGTK::FineTickerStart(TickReason reason, int millis, int tolerance) {
	Ticker &ticker = tickers[static_cast<size_t>(reason)];
	if (ticker.source_id) {
		g_source_remove(ticker.source_id);
	}
	ticker.sc = this;
	ticker.source_id = g_timeout_add(millis, G_SOURCE_FUNC(FineTickerCallback), &ticker);
}

void ScintillaGTK::FineTickerCancel(TickReason reason) {
	Ticker &ticker = tickers[static_cast<size_t>(reason)];
	if (ticker.source_id) {
		g_source_remove(ticker.source_id);
		ticker.source_id = 0;
	}
}

}
