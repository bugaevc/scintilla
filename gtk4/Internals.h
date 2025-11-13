struct _ScintillaView;
namespace Scintilla::Internal {
class ScintillaGTK;
}

void scintilla_view_sci_notify(_ScintillaView *view, void *scn) noexcept;

Scintilla::Internal::ScintillaGTK &scintilla_view_get_editor(_ScintillaView *view) noexcept;
void scintilla_view_queue_allocate_if_need_update_ui(_ScintillaView *view) noexcept;
void scintilla_view_notify_editable(_ScintillaView *view, bool editable) noexcept;

#if defined(__GNUC__)
// Apparently needed despite -fvisibility=hidden.
__attribute__((visibility("hidden")))
#endif
extern _ScintillaView *scintilla_view_recent_instance;

#define SCINTILLA_GTK4_DYNAMIC_DEFAULT_FONT "scintilla-gtk4-dynamic-default-font"
