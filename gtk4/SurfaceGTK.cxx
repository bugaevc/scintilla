#include "SurfaceGTK.h"
#include "Internals.h"

namespace Scintilla::Internal {

int SurfaceGTK::SupportsFeature(Scintilla::Supports feature) noexcept {
#define SUPPORTS(F) if (feature == Supports::F) { return 1; }

	SUPPORTS(LineDrawsFinal)
	SUPPORTS(PixelDivisions)
	SUPPORTS(FractionalStrokeWidth)
	SUPPORTS(TranslucentStroke)
	SUPPORTS(PixelModification)
	// SUPPORTS(ThreadSafeMeasureWidths)

	return false;
#undef SUPPORTS
}

SurfaceGTK::~SurfaceGTK() {
	if (snapshot) {
		g_object_unref(snapshot);
	}
	if (render_node) {
		gsk_render_node_unref(render_node);
	}
}

GskRenderNode *SurfaceGTK::EnsureRenderNode() {
	if (render_node) {
		return render_node;
	}
	g_assert(snapshot);
	render_node = gtk_snapshot_free_to_node(snapshot);
	snapshot = nullptr;
	return render_node;
}

std::unique_ptr<Surface> Surface::Allocate(Technology) {
	return std::make_unique<SurfaceGTK>();
}

void SurfaceGTK::Init(WindowID wid) {
	widget = GTK_WIDGET(wid);
}

void SurfaceGTK::Init(SurfaceID sid, WindowID wid) {
	// ???
	widget = GTK_WIDGET(wid);
}

PangoFontDescription *SurfaceGTK::GetFontDescription(const Font *font) {
	if (font) {
		return static_cast<const FontPango *>(font)->desc;
	}
	return nullptr;
}

static inline graphene_rect_t PRectangleToGraphene(const PRectangle &rc) noexcept {
	graphene_rect_t rect {
		{ static_cast<float>(rc.left), static_cast<float>(rc.top) },
		{ static_cast<float>(rc.Width()), static_cast<float>(rc.Height()) }
	};
	return rect;
}

static inline GdkRGBA ColourRGBAToGDK(ColourRGBA c) noexcept {
	GdkRGBA color {
		c.GetRedComponent(),
		c.GetGreenComponent(),
		c.GetBlueComponent(),
		c.GetAlphaComponent()
	};
	return color;
}

std::unique_ptr<Surface> SurfaceGTK::AllocatePixMap(int width, int height) {
	std::unique_ptr<SurfaceGTK> surface = std::make_unique<SurfaceGTK>();
	surface->snapshot = gtk_snapshot_new();
	surface->size = { static_cast<float>(width), static_cast<float>(height) };
	return surface;
}

bool SurfaceGTK::Initialised() {
	return snapshot != nullptr;
}

void SurfaceGTK::SetClip(PRectangle rc) {
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	gtk_snapshot_push_clip(snapshot, &bounds);
}

void SurfaceGTK::PopClip() {
	gtk_snapshot_pop(snapshot);
}

int SurfaceGTK::LogPixelsY() {
	return 72;
}


int SurfaceGTK::PixelDivisions() {
	if (widget == nullptr) {
		return 1;
	}
	return gtk_widget_get_scale_factor(widget);
}

int SurfaceGTK::DeviceHeightFont(int points) {
	const int logPix = LogPixelsY();
	return (points * logPix + logPix / 2) / 72;
}

void SurfaceGTK::LineDraw(Point start, Point end, Stroke stroke) {
	GdkRGBA color = ColourRGBAToGDK(stroke.colour);
#if GTK_CHECK_VERSION(4, 14, 0)
	// Unfortunately, we have to re-create the path each frame.
	GskPathBuilder *path_builder = gsk_path_builder_new();
	gsk_path_builder_move_to(path_builder, start.x, start.y);
	gsk_path_builder_line_to(path_builder, end.x, end.y);
	GskPath *path = gsk_path_builder_free_to_path(path_builder);
	GskStroke *gsk_stroke = gsk_stroke_new(stroke.width);
	gtk_snapshot_append_stroke(snapshot, path, gsk_stroke, &color);
	gsk_stroke_free(gsk_stroke);
	gsk_path_unref(path);
#else
	graphene_rect_t bounds;
	graphene_rect_init(&bounds, start.x, start.y, end.x - start.x, end.y - start.y);
	// Note: the rect is normalized by the above call.
	cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);
	gdk_cairo_set_source_rgba(cr, &color);
	cairo_set_line_width(cr, stroke.width);
	cairo_move_to(cr, start.x, start.y);
	cairo_line_to(cr, end.x, end.y);
	cairo_stroke(cr);
	cairo_destroy(cr);
#endif
}

void SurfaceGTK::PolyLine(const Point *pts, size_t npts, Stroke stroke) {
	g_warning("SurfaceGTK::PolyLine unimplemented");
}

void SurfaceGTK::Polygon(const Point *pts, size_t npts, FillStroke fill_stroke) {
	g_warning("SurfaceGTK::Polygon unimplemented");
}

void SurfaceGTK::RectangleDraw(PRectangle rc, FillStroke fillStroke) {
	FillRectangle(rc.Inset(fillStroke.stroke.width), fillStroke.fill.colour);
	RectangleFrame(rc, fillStroke.stroke);
}

void SurfaceGTK::RectangleFrame(PRectangle rc, Stroke stroke) {
	GskRoundedRect outline;
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	gsk_rounded_rect_init_from_rect(&outline, &bounds, 0);
	GdkRGBA border_color = ColourRGBAToGDK(stroke.colour);
	GdkRGBA border_colors[4] { border_color, border_color, border_color, border_color };
	float border_widths[4] {
		static_cast<float>(stroke.width),
		static_cast<float>(stroke.width),
		static_cast<float>(stroke.width),
		static_cast<float>(stroke.width)
	};
	gtk_snapshot_append_border(snapshot, &outline, border_widths, border_colors);
}

void SurfaceGTK::RoundedRectangle(PRectangle rc, FillStroke fillStroke) {
	GskRoundedRect outline;
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	gsk_rounded_rect_init_from_rect(&outline, &bounds, 8);

	gtk_snapshot_push_rounded_clip(snapshot, &outline);
	GdkRGBA fill_color = ColourRGBAToGDK(fillStroke.fill.colour);
	gtk_snapshot_append_color(snapshot, &fill_color, &bounds);
	gtk_snapshot_pop(snapshot);

	GdkRGBA border_color = ColourRGBAToGDK(fillStroke.stroke.colour);
	GdkRGBA border_colors[4] { border_color, border_color, border_color, border_color };
	float border_width = fillStroke.stroke.width;
	float border_widths[4] { border_width, border_width, border_width, border_width };
	gtk_snapshot_append_border(snapshot, &outline, border_widths, border_colors);
}

void SurfaceGTK::AlphaRectangle(PRectangle rc, XYPOSITION cornerSize, FillStroke fill_stroke) {
	g_warning("SurfaceGTK::AlphaRectangle unimplemented");
}

void SurfaceGTK::GradientRectangle(PRectangle rc, const std::vector<ColourStop> &stops, GradientOptions options)
{
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	graphene_point_t start_point, end_point;
	switch (options) {
	case GradientOptions::topToBottom:
		graphene_rect_get_top_left(&bounds, &start_point);
		graphene_rect_get_bottom_left(&bounds, &end_point);
		break;
	case GradientOptions::leftToRight:
		graphene_rect_get_top_left(&bounds, &start_point);
		graphene_rect_get_top_right(&bounds, &end_point);
		break;
	default:
		g_return_if_reached();
	}
	GskColorStop *gsk_stops = g_newa(GskColorStop, stops.size());
	for (size_t i = 0; i < stops.size(); i++) {
		gsk_stops[i].offset = stops[i].position;
		gsk_stops[i].color = ColourRGBAToGDK(stops[i].colour);
	}
	gtk_snapshot_append_linear_gradient(snapshot, &bounds, &start_point, &end_point, gsk_stops, stops.size());
}

void SurfaceGTK::DrawRGBAImage(PRectangle rc, int width, int height, const unsigned char *pixelsImage) {
	g_warning("SurfaceGTK::DrawRGBAImage unimplemented");
}

void SurfaceGTK::Ellipse(PRectangle rc, FillStroke fillStroke) {
	GskRoundedRect outline;
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	graphene_size_t corner_radius {
		static_cast<float>(bounds.size.width / 2.0),
		static_cast<float>(bounds.size.height / 2.0)
	};
	gsk_rounded_rect_init(&outline, &bounds, &corner_radius, &corner_radius, &corner_radius, &corner_radius);

	gtk_snapshot_push_rounded_clip(snapshot, &outline);
	GdkRGBA fill_color = ColourRGBAToGDK(fillStroke.fill.colour);
	gtk_snapshot_append_color(snapshot, &fill_color, &bounds);
	gtk_snapshot_pop(snapshot);

	GdkRGBA border_color = ColourRGBAToGDK(fillStroke.stroke.colour);
	GdkRGBA border_colors[4] { border_color, border_color, border_color, border_color };
	float border_width = fillStroke.stroke.width;
	float border_widths[4] { border_width, border_width, border_width, border_width };
	gtk_snapshot_append_border(snapshot, &outline, border_widths, border_colors);
}

void SurfaceGTK::Stadium(PRectangle rc, FillStroke fillStroke, Ends ends) {
	g_warning("SurfaceGTK::Stadium unimplemented");
}

XYPOSITION SurfaceGTK::Ascent(const Font *font) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoFontMetrics *metrics = pango_context_get_metrics(context, desc, nullptr);
	XYPOSITION res = (double) pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	return std::ceil(res);
}

XYPOSITION SurfaceGTK::Descent(const Font *font) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoFontMetrics *metrics = pango_context_get_metrics(context, desc, nullptr);
	XYPOSITION res = (double) pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	return std::ceil(res);
}

XYPOSITION SurfaceGTK::Height(const Font *font) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoFontMetrics *metrics = pango_context_get_metrics(context, desc, nullptr);
	XYPOSITION res = (double) pango_font_metrics_get_height(metrics) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	return res;
}

XYPOSITION SurfaceGTK::AverageCharWidth(const Font *font) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoFontMetrics *metrics = pango_context_get_metrics(context, desc, nullptr);
	XYPOSITION res = (double) pango_font_metrics_get_approximate_char_width(metrics) / PANGO_SCALE;
	pango_font_metrics_unref(metrics);
	return res;
}

XYPOSITION SurfaceGTK::InternalLeading(const Font *) {
	return 0;
}

XYPOSITION SurfaceGTK::WidthText(const Font *font, std::string_view text) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	// TODO: not UTF-8?
	pango_layout_set_text(layout, text.data(), text.length());
	int width;
	pango_layout_get_size(layout, &width, nullptr);
	g_object_unref(layout);
	return PANGO_PIXELS_CEIL(width);
}

XYPOSITION SurfaceGTK::WidthTextUTF8(const Font *font, std::string_view text) {
	g_print("SurfaceGTK::WidthTextUTF8\n");
	return WidthText(font, text);
}

void SurfaceGTK::MeasureWidths(const Font *font, std::string_view text, XYPOSITION *positions) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	// TODO: not UTF-8?
	pango_layout_set_text(layout, text.data(), text.length());
	PangoLayoutIter *iter = pango_layout_get_iter(layout);

	// positions[i] is the right edge of text[:i]. For monospace font,
	// positions[i] = monospaceCharacterWidth * (i+1);

	size_t index = 0;
	while (true) {
		PangoRectangle logical_rect;
		pango_layout_iter_get_char_extents(iter, &logical_rect);
		XYPOSITION position = PANGO_PIXELS_CEIL(logical_rect.x + logical_rect.width);
		gboolean more_chars = pango_layout_iter_next_char(iter);
		size_t next_index = more_chars ? pango_layout_iter_get_index(iter) : text.length();
		for (; index < next_index; index++) {
			positions[index] = position;
		}
		if (!more_chars) {
			break;
		}
	}

	pango_layout_iter_free(iter);
	g_object_unref(layout);
}

void SurfaceGTK::MeasureWidthsUTF8(const Font *font, std::string_view text, XYPOSITION *positions) {
	MeasureWidths(font, text, positions);
}

void SurfaceGTK::DrawTextTransparent(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore) {
	PangoContext *context = gtk_widget_get_pango_context(widget);
	PangoFontDescription *desc = GetFontDescription(font);
	PangoLayout *layout = pango_layout_new(context);
	pango_layout_set_font_description(layout, desc);
	// TODO: not UTF-8?
	pango_layout_set_text(layout, text.data(), text.length());
	GdkRGBA color = ColourRGBAToGDK(fore);
	gtk_snapshot_save(snapshot);
	graphene_point_t origin { (float) rc.left, (float) rc.top };
	gtk_snapshot_translate(snapshot, &origin);
	gtk_snapshot_append_layout(snapshot, layout, &color);
	gtk_snapshot_restore(snapshot);
	g_object_unref(layout);
}

void SurfaceGTK::DrawTextTransparentUTF8(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore) {
	DrawTextTransparent(rc, font, ybase, text, fore);
}

void SurfaceGTK::DrawTextNoClip(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore, ColourRGBA back) {
	FillRectangle(rc, back);
	DrawTextTransparent(rc, font, ybase, text, fore);
}

void SurfaceGTK::DrawTextNoClipUTF8(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore, ColourRGBA back) {
	DrawTextNoClip(rc, font, ybase, text, fore, back);
}

void SurfaceGTK::DrawTextClipped(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore, ColourRGBA back) {
	FillRectangle(rc, back);
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	gtk_snapshot_push_clip(snapshot, &bounds);
	DrawTextTransparent(rc, font, ybase, text, fore);
	gtk_snapshot_pop(snapshot);
}

void SurfaceGTK::DrawTextClippedUTF8(PRectangle rc, const Font *font, XYPOSITION ybase, std::string_view text, ColourRGBA fore, ColourRGBA back) {
	DrawTextClipped(rc, font, ybase, text, fore, back);
}

std::unique_ptr<IScreenLineLayout> SurfaceGTK::Layout(const IScreenLine *screenLine) {
	return nullptr;
}

void SurfaceGTK::FillRectangle(PRectangle rc, Fill fill) {
	g_assert(snapshot);
	if (fill.colour.GetAlpha() == 0) {
		return;
	}
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	GdkRGBA color = ColourRGBAToGDK(fill.colour);
	gtk_snapshot_append_color(snapshot, &color, &bounds);
}

void SurfaceGTK::FillRectangleAligned(PRectangle rc, Fill fill) {
	FillRectangle(PixelAlign(rc, PixelDivisions()), fill);
}

void SurfaceGTK::FillRectangle(PRectangle rc, Surface &surfacePattern) {
	g_assert(snapshot);

	SurfaceGTK &surfaceSourceGTK = static_cast<SurfaceGTK &>(surfacePattern);
	graphene_rect_t bounds = PRectangleToGraphene(rc);
	graphene_rect_t child_bounds {
		{ 0, 0 },
		surfaceSourceGTK.size
	};

	gtk_snapshot_push_repeat(snapshot, &bounds, &child_bounds);
	gtk_snapshot_append_node(snapshot, surfaceSourceGTK.EnsureRenderNode());
	gtk_snapshot_pop(snapshot);
}

void SurfaceGTK::Copy(PRectangle rc, Point from, Surface &surfaceSource) {
	g_assert(snapshot);

	graphene_rect_t bounds = PRectangleToGraphene(rc);
	gtk_snapshot_push_clip(snapshot, &bounds);
	gtk_snapshot_save(snapshot);

	graphene_point_t point {
		(float) (rc.left - from.x),
		(float) (rc.top - from.y)
	};
	gtk_snapshot_translate(snapshot, &point);

	SurfaceGTK &surfaceSourceGTK = static_cast<SurfaceGTK &>(surfaceSource);
	gtk_snapshot_append_node(snapshot, surfaceSourceGTK.EnsureRenderNode());

	gtk_snapshot_restore(snapshot);
	gtk_snapshot_pop(snapshot);
}

static inline PangoStretch FontStretchToPango(FontStretch stretch) noexcept {
	switch (stretch) {
	case FontStretch::UltraCondensed:
		return PANGO_STRETCH_ULTRA_CONDENSED;
	case FontStretch::ExtraCondensed:
		return PANGO_STRETCH_EXTRA_CONDENSED;
	case FontStretch::Condensed:
		return PANGO_STRETCH_CONDENSED;
	case FontStretch::SemiCondensed:
		return PANGO_STRETCH_SEMI_CONDENSED;
	case FontStretch::Normal:
		return PANGO_STRETCH_NORMAL;
	case FontStretch::SemiExpanded:
		return PANGO_STRETCH_SEMI_EXPANDED;
	case FontStretch::Expanded:
		return PANGO_STRETCH_EXPANDED;
	case FontStretch::ExtraExpanded:
		return PANGO_STRETCH_EXTRA_EXPANDED;
	case FontStretch::UltraExpanded:
		return PANGO_STRETCH_ULTRA_EXPANDED;
	default:
		return PANGO_STRETCH_NORMAL;
	}
}


std::shared_ptr<Font> Font::Allocate(const FontParameters &fp) {
	PangoFontDescription *desc = nullptr;

	if (strcmp(fp.faceName, SCINTILLA_GTK4_DYNAMIC_DEFAULT_FONT)) {
		desc = pango_font_description_new();
		pango_font_description_set_family(desc, fp.faceName);
		pango_font_description_set_style(desc, fp.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
		pango_font_description_set_size(desc, fp.size * PANGO_SCALE);
		pango_font_description_set_weight(desc, static_cast<PangoWeight>(fp.weight));
		pango_font_description_set_stretch(desc, FontStretchToPango(fp.stretch));
	}

	return std::make_shared<FontPango>(desc);
}

FontPango::~FontPango() {
	pango_font_description_free(desc);
}

}
