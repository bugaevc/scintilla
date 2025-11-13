#pragma once

#include <gtk/gtk.h>

#include <string>
#include <memory>
#include <vector>
#include <optional>

#include "ScintillaTypes.h"
#include "Geometry.h"
#include "Platform.h"

namespace Scintilla::Internal {

class ListBoxGTK final : public ListBox {
	IListBoxDelegate *delegate { nullptr };
	GtkScrolledWindow *scrolled_window;
	GtkListView *list_view;
	GListStore *list_store;
	GtkSingleSelection *selection;

	void Clear() noexcept override;
	int Length() override;
	void SetFont(const Font *font) override;
	void Create(Window &parent, int ctrlID, Point location, int lineHeight, bool unicodeMode, Scintilla::Technology) override;
	void SetAverageCharWidth(int width) override;
	void SetVisibleRows(int rows) override;
	int GetVisibleRows() const override;
	PRectangle GetDesiredRect() override;
	int CaretFromEdge() override;
	void Append(char *s, int type = -1) override;
	void Select(int n) override;
	int GetSelection() override;
	int Find(const char *prefix) override;
	std::string GetValue(int n) override;
	void RegisterImage(int type, const char *xpm_data) override;
	void RegisterRGBAImage(int type, int width, int height, const unsigned char *pixelsImage) override;
	void ClearRegisteredImages() override;
	void SetDelegate(IListBoxDelegate *lbDelegate) override;
	void SetList(const char* list, char separator, char typesep) override;
	void SetOptions(ListOptions) override;

	static void on_selected_changed(ListBoxGTK *);
	static void on_activate(ListBoxGTK *);

public:
	ListBoxGTK() noexcept;
	~ListBoxGTK() noexcept override;
};

}
