[GtkTemplate (ui = "/org/scintilla/Demo/tab.ui")]
class Demo.Tab : Gtk.Widget {
	public File? file;

	[GtkChild]
	private unowned Scintilla.View scintilla;

	static construct {
		set_layout_manager_type (typeof (Gtk.BinLayout));
	}

	public async void load_file (Cancellable? cancellable) {
		if (file == null) {
			return;
		}
		try {
			var input_stream = yield ((!) file).read_async ();
			while (true) {
				var bytes = yield input_stream.read_bytes_async (4096, Priority.DEFAULT, cancellable);
				if (bytes.get_size () == 0) {
					// EOF.
					break;
				}
				scintilla.append_text ((char[]) bytes.get_data ());
			}
			yield input_stream.close_async (Priority.DEFAULT, cancellable);
		} catch (Error err) {
			printerr ("%s", err.message);
		}
	}
}
