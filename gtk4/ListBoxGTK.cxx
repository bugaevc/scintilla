#include "ListBoxGTK.h"

namespace Scintilla::Internal {

void ListBoxGTK::Clear() noexcept {
	g_list_store_remove_all(list_store);
}

ListBox::ListBox() noexcept {}
ListBox::~ListBox() noexcept {}

ListBoxGTK::ListBoxGTK() noexcept {
	// FIXME: Fill the proper type here.
	list_store = g_list_store_new(G_TYPE_OBJECT);
	selection = gtk_single_selection_new(G_LIST_MODEL(list_store));
}

ListBoxGTK::~ListBoxGTK() {
	g_object_unref(selection);
}

int ListBoxGTK::Length() {
	return g_list_model_get_n_items(G_LIST_MODEL(list_store));
}

void ListBoxGTK::SetFont(const Font *font) {}

void ListBoxGTK::Create(Window &parent, int ctrlID, Point location, int lineHeight, bool unicodeMode, Scintilla::Technology) {}

void ListBoxGTK::SetAverageCharWidth(int width) {}

void ListBoxGTK::SetVisibleRows(int rows) {}

int ListBoxGTK::GetVisibleRows() const {
	return 1;
}

PRectangle ListBoxGTK::GetDesiredRect() {
	return PRectangle::FromInts(0, 0, 0, 0);
}

int ListBoxGTK::CaretFromEdge() {
	return 0;
}

void ListBoxGTK::Append(char *s, int type) {
	g_assert_not_reached();
}

void ListBoxGTK::Select(int n) {
	guint position;
	if (n == -1) {
		position = GTK_INVALID_LIST_POSITION;
	} else {
		position = n;
	}
	gtk_single_selection_set_selected(selection, position);
}

int ListBoxGTK::GetSelection() {
	guint position = gtk_single_selection_get_selected(selection);
	if (position == GTK_INVALID_LIST_POSITION) {
		return -1;
	}
	return position;
}

int ListBoxGTK::Find(const char *prefix) {
	return -1;
}

std::string ListBoxGTK::GetValue(int n) {
	return "";
}

void ListBoxGTK::RegisterImage(int type, const char *xpm_data) {}

void ListBoxGTK::RegisterRGBAImage(int type, int width, int height, const unsigned char *pixelsImage) {}

void ListBoxGTK::ClearRegisteredImages() {}

void ListBoxGTK::SetDelegate(IListBoxDelegate *lbDelegate) {
	this->delegate = lbDelegate;
}

void ListBoxGTK::SetList(const char* list, char separator, char typesep) {}

void ListBoxGTK::SetOptions(ListOptions options) {}

std::unique_ptr<ListBox> ListBox::Allocate() {
	return std::make_unique<ListBoxGTK>();
}


void ListBoxGTK::on_selected_changed(ListBoxGTK *lb) {
	if (!lb->delegate) {
		return;
	}
	ListBoxEvent event(ListBoxEvent::EventType::selectionChange);
	lb->delegate->ListNotify(&event);
}

void ListBoxGTK::on_activate(ListBoxGTK *lb) {
	if (!lb->delegate) {
		return;
	}
	ListBoxEvent event(ListBoxEvent::EventType::doubleClick);
	lb->delegate->ListNotify(&event);
}

}
