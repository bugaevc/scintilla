#include "ScintillaDocument.h"
#include "ILoader.h"
#include "Internals.h"
#include "ScintillaView.h"
#include "ScintillaGTK.h"

using namespace Scintilla;
using namespace Scintilla::Internal;

/**
 * ScintillaDocument: (ref-func scintilla_document_ref) (unref-func scintilla_document_unref)
 */
G_DEFINE_BOXED_TYPE(ScintillaDocument, scintilla_document, scintilla_document_ref, scintilla_document_unref)

void scintilla_document_unref(ScintillaDocument *document) {
	document->Release();
}

ScintillaDocument *scintilla_document_ref(ScintillaDocument *document) {
	document->AddRef();
	return document;
}

/**
 * scintilla_document_new:
 *
 * Returns: (nullable) (transfer full): a new document
 */
ScintillaDocument *scintilla_document_new(gsize bytes, ScintillaDocumentOption options) {
	if (scintilla_view_recent_instance) {
		guintptr doc = scintilla_view_send_message(scintilla_view_recent_instance,
			static_cast<unsigned>(Message::CreateDocument), static_cast<guintptr>(bytes), static_cast<gintptr>(options));
		return reinterpret_cast<ScintillaDocument *>(doc);
	} else {
		/* We have no view instance to start off, so do what it does manually. */
		Document *doc = new Document(static_cast<DocumentOption>(options));
		doc->AddRef();
		doc->Allocate(bytes);
		return doc->AsDocumentEditable();
	}
}
