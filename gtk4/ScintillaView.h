#pragma once

#include <gtk/gtk.h>
#include "ScintillaVisibility.h"
#include "ScintillaEnums.h"
#include "ScintillaDocument.h"

G_BEGIN_DECLS

/* See Lexilla.h */
#if defined(__cplusplus)
namespace Scintilla {
class ILexer5;
}
using Scintilla::ILexer5;
#else
#ifndef __GI_SCANNER__
typedef void ILexer5;
#else
/* GI scanner doesn't quite like 'typedef void' */
typedef struct ILexer5 ILexer5;
#endif
#endif

SCINTILLA_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE(ScintillaView, scintilla_view, SCINTILLA, VIEW, GtkWidget)

#define SCINTILLA_TYPE_VIEW scintilla_view_get_type()

SCINTILLA_AVAILABLE_IN_ALL
GtkWidget *scintilla_view_new(void) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
GtkInputHints scintilla_view_get_input_hints(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_input_hints(ScintillaView *view, GtkInputHints hints) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
GtkInputPurpose scintilla_view_get_input_purpose(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_input_purpose(ScintillaView *view, GtkInputPurpose purpose) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gboolean scintilla_view_get_editable(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_editable(ScintillaView *view, gboolean editable) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_get_text_length(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_get_text(ScintillaView *view, char *buffer, gsize count) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_text(ScintillaView *view, const char *text) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
const char *scintilla_view_get_character_pointer(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
const char *scintilla_view_get_range_pointer(ScintillaView *view, gsize start, gsize length) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_get_gap_position(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_save_point(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
gboolean scintilla_view_get_modified(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_count_characters(ScintillaView *view, gsize start, gsize end) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_line_length(ScintillaView *view, gsize line) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_get_line(ScintillaView *view, gsize line, char *buffer) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_get_lines_count(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaFindOption scintilla_view_get_search_flags(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_search_flags(ScintillaView *view, ScintillaFindOption flags) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_insert_text(ScintillaView *view, gssize position, const char *text) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_append_text(ScintillaView *view, const char *text, gsize length) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_clear_all(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_delete_range(ScintillaView *view, gsize position, gsize length) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
int scintilla_view_get_tab_width(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_tab_width(ScintillaView *view, int tab_width) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gboolean scintilla_view_get_eol_filled(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_eol_filled(ScintillaView *view, gboolean eol_filled) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaCaretStyle scintilla_view_get_caret_style(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_caret_style(ScintillaView *view, ScintillaCaretStyle style) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaCaretSticky scintilla_view_get_caret_sticky(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_caret_sticky(ScintillaView *view, ScintillaCaretSticky sticky) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_toggle_caret_sticky(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaWrap scintilla_view_get_wrap_mode(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_wrap_mode(ScintillaView *view, ScintillaWrap wrap_mode) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
ScintillaWrapVisualFlag scintilla_view_get_wrap_visual_flags(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_wrap_visual_flags(ScintillaView *view, ScintillaWrapVisualFlag wrap_visual_flags) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
ScintillaWrapVisualLocation scintilla_view_get_wrap_visual_flags_location(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_wrap_visual_flags_location(ScintillaView *view, ScintillaWrapVisualLocation wrap_visual_flags_location) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
ScintillaWrapIndentMode scintilla_view_get_wrap_indent_mode(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_wrap_indent_mode(ScintillaView *view, ScintillaWrapIndentMode wrap_indent_mode) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
int scintilla_view_get_wrap_start_indent(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_wrap_start_indent(ScintillaView *view, int indent) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gboolean scintilla_view_get_overtype(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_overtype(ScintillaView *view, gboolean overtype) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_start_record(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_stop_record(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
int scintilla_view_get_lexer_id(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_lexer(ScintillaView *view, ILexer5 *lexer) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaDocument *scintilla_view_get_document(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_document(ScintillaView *view, ScintillaDocument *document) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_eol_annotation_set_text(ScintillaView *view, gsize line, const char *text) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
gsize scintilla_view_eol_annotation_get_text(ScintillaView *view, gsize line, char *buffer) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_eol_annotation_clear_all(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_eol_annotation_set_visible(ScintillaView *view, ScintillaEOLAnnotationVisible visible) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
ScintillaEOLAnnotationVisible scintilla_view_eol_annotation_get_visible(ScintillaView *view) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaAutomaticFold scintilla_view_get_automatic_fold(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_automatic_fold(ScintillaView *view, ScintillaAutomaticFold fold) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
ScintillaFoldFlag scintilla_view_get_fold_flags(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_fold_flags(ScintillaView *view, ScintillaFoldFlag flags) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gboolean scintilla_view_get_multiple_selection(ScintillaView *view) SCINTILLA_NOEXCEPT;
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_view_set_multiple_selection(ScintillaView *view, gboolean multiple_selection) SCINTILLA_NOEXCEPT;

SCINTILLA_AVAILABLE_IN_ALL
gintptr scintilla_view_send_message(ScintillaView *view, unsigned int msg, guintptr wParam, gintptr lParam) SCINTILLA_NOEXCEPT;

/* Declared in <Scintilla.h> */
typedef struct SCNotification SCNotification;
SCINTILLA_AVAILABLE_IN_ALL
GType scintilla_notification_get_type(void);
#define SCINTILLA_TYPE_NOTIFICATION scintilla_notification_get_type()

G_END_DECLS
