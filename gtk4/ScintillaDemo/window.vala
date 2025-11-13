[GtkTemplate (ui = "/org/scintilla/Demo/window.ui")]
class Demo.Window : Gtk.ApplicationWindow {
	public Window (Gtk.Application app) {
		Object (application: app, show_menubar: true);
	}

	private const ActionEntry action_entries[] = {
		{ "save-file", save_file_cb },
		{ "save-file-as", save_file_as_cb }
	};

	construct {
		add_action_entries (action_entries, this);
	}

	public void open_new_document () {
		var tab = new Tab ();
		var index = notebook.append_page (tab, null);
		unowned var page = notebook.get_page (tab);
		page.reorderable = page.detachable = true;
		notebook.page = index;
	}

	public void open (File[] files, string hint) {
		int last_added_page = -1;
		foreach (unowned var file in files) {
			var tab = new Tab () {
				file = file
			};
			tab.load_file.begin (null);
			last_added_page = notebook.append_page (tab, null);
			unowned var page = notebook.get_page (tab);
			page.reorderable = page.detachable = true;
		}
		if (last_added_page != -1) {
			notebook.page = last_added_page;
		}
	}

	[GtkChild]
	private unowned Gtk.Notebook notebook;

	[GtkCallback]
	private unowned Gtk.Notebook? create_window_cb (Gtk.Widget page) {
		var window = new Window (this.application);
		window.present ();
		return window.notebook;
	}

	private void save_file_cb () {
		print ("save-file-cb\n");
	}
	private void save_file_as_cb () {
		print ("save-file-as_cb\n");
	}
}
