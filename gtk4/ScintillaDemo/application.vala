class Demo.Application : Gtk.Application {
	public Application () {
		Object (
			application_id: "org.scintilla.Demo",
			flags: ApplicationFlags.HANDLES_OPEN
		);
	}

	public override void activate () {
		base.activate ();
		var window = get_active_window ();
		if (window == null) {
			window = new Window (this);
			((Window) window).open_new_document ();
		}
		((!) window).present ();
	}

	public override void open (File[] files, string hint) {
		var window = get_active_window () as Window ?? new Window (this);
		window.open (files, hint);
		window.present ();
	}

	private const ActionEntry action_entries[] = {
		{ "new-file", new_file_cb },
		{ "open-file", open_file_cb },
		{ "quit", quit_cb },
		{ "about", about_cb },
		{ "new-window", new_window_cb }
	};

	construct {
		add_action_entries (action_entries, this);
		set_accels_for_action ("app.new-file", { "<Primary>n" });
		set_accels_for_action ("app.open-file", { "<Primary>o" });
		set_accels_for_action ("win.save-file", { "<Primary>s" });
		set_accels_for_action ("win.save-file-as", { "<Primary><Shift>s" });
		set_accels_for_action ("app.quit", { "<Primary>q" });
		set_accels_for_action ("app.new-window", { "<Primary><Shift>n" });
	}

	private void new_file_cb () {
		var window = get_active_window () as Window ?? new Window (this);
		window.open_new_document ();
		window.present ();
	}

	private void open_file_cb () {
		open_file_with_dialog.begin (null);
	}

	private async void open_file_with_dialog (Cancellable? cancellable) {
		var window = get_active_window () as Window;
		var dialog = new Gtk.FileDialog ();
		ListModel files_list;
		try {
			files_list = (!) yield dialog.open_multiple (window, cancellable);
		} catch (Gtk.DialogError error) {
			if (error is Gtk.DialogError.CANCELLED || error is Gtk.DialogError.DISMISSED) {
				// Okay...
				return;
			}
			warning ("Failed to open file dialog: %s", error.message);
			return;
		} catch (Error error) {
			warning ("Failed to open file dialog: %s", error.message);
			return;
		}
		if (window == null) {
			window = new Window (this);
		}
		uint n_files = files_list.get_n_items ();
		File[] files = new File[n_files];
		for (uint position = 0; position < n_files; position++) {
			files[position] = (File) files_list.get_object (position);
		}
		((!) window).open (files, "");
		((!) window).present ();
	}

	private void quit_cb () {
		quit ();
	}

	private void about_cb () {
		Gtk.show_about_dialog (get_active_window (),
			program_name: _("Scintilla Demo"),
			website: "https://www.scintilla.org",
			website_label: _("Scintilla website")
		);
	}

	private void new_window_cb () {
		var window = new Window (this);
		window.open_new_document ();
		window.present ();
	}
}
