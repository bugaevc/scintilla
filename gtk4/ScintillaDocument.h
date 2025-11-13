#pragma once

#include "ScintillaVisibility.h"
#include "ScintillaEnums.h"

G_BEGIN_DECLS

#if defined(__cplusplus)
/* See ILoader.h */
namespace Scintilla {
class IDocumentEditable;
}
using ScintillaDocument = Scintilla::IDocumentEditable;
#else
typedef struct _ScintillaDocument ScintillaDocument;
#endif

SCINTILLA_AVAILABLE_IN_ALL
GType scintilla_document_get_type(void);

#define SCINTILLA_TYPE_DOCUMENT scintilla_document_get_type()

SCINTILLA_AVAILABLE_IN_ALL
ScintillaDocument *scintilla_document_ref(ScintillaDocument *document);
SCINTILLA_AVAILABLE_IN_ALL
void scintilla_document_unref(ScintillaDocument *document);

SCINTILLA_AVAILABLE_IN_ALL
ScintillaDocument *scintilla_document_new(gsize bytes, ScintillaDocumentOption options);

G_END_DECLS
