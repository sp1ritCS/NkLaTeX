#include <gtk/gtk.h>
#include <adwaita.h>
#include <gtksourceview/gtksource.h>
#include <librsvg/rsvg.h>
#include <zstd.h>
#include <zlib.h>

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "notekit_external.h"
#include "nkl_res.h"

#include "config.h"

// this should be in sys/mman.h, but for some reason isn't.
extern int memfd_create(const char *__name, unsigned int __flags);

enum {
	SIGNAL_EACTIVATE,
	NR_SIGNALS
};
static guint nk_ext_appl_signals[NR_SIGNALS];

typedef struct {
	GDBusObjectManagerServer* bus_mgr;
	NoteKitObjectSkeleton* skel;
	NoteKitExternal* ext;
} NkExtApplPrivate;

struct _NkExtApplClass {
	AdwApplicationClass parent_class;

	gpointer padding[4];
};

#define NOTEKIT_TYPE_APPLICATION (nk_ext_appl_get_type())
G_DECLARE_DERIVABLE_TYPE(NkExtAppl, nk_ext_appl, NOTEKIT, APPLICATION, AdwApplication)
G_DEFINE_TYPE_WITH_PRIVATE(NkExtAppl, nk_ext_appl, ADW_TYPE_APPLICATION)

static bool nk_ext_appl_eactivate(NoteKitExternal* nke, GDBusMethodInvocation* invoc, GVariant* widget, const gchar* path, gpointer user_data) {
	GVariant* copath = g_variant_new("s", g_strdup(path));
	g_signal_emit(user_data, nk_ext_appl_signals[SIGNAL_EACTIVATE], 0, g_variant_ref(widget), copath);
	note_kit_external_complete_activate(nke, invoc);
	return TRUE;
}

static gboolean nk_ext_appl_dbus_register(GApplication* app, GDBusConnection* con, const gchar* opath, GError** err) {
	NkExtAppl* self = NOTEKIT_APPLICATION(app);
	NkExtApplPrivate* priv = nk_ext_appl_get_instance_private(self);

	if (!G_APPLICATION_CLASS(nk_ext_appl_parent_class)->dbus_register(app, con, opath, err))
		return FALSE;

	priv->bus_mgr = g_dbus_object_manager_server_new("/com/github/blackhole89/NoteKit");
	priv->skel = note_kit_object_skeleton_new("/com/github/blackhole89/NoteKit/External");

	priv->ext = note_kit_external_skeleton_new();
	note_kit_object_skeleton_set_external(priv->skel, priv->ext);

	g_dbus_object_manager_server_export(priv->bus_mgr, G_DBUS_OBJECT_SKELETON(priv->skel));

	g_dbus_object_manager_server_set_connection(priv->bus_mgr, con);


	g_signal_connect(priv->ext, "handle-activate", G_CALLBACK(nk_ext_appl_eactivate), self);

	return TRUE;
}

static void nk_ext_appl_dbus_unregister(GApplication* app, GDBusConnection* con, const gchar* opath) {
	NkExtAppl* self = NOTEKIT_APPLICATION(app);
	NkExtApplPrivate* priv = nk_ext_appl_get_instance_private(self);

	g_clear_object(&priv->ext);
	g_clear_object(&priv->skel);
	g_clear_object(&priv->bus_mgr);

	G_APPLICATION_CLASS(nk_ext_appl_parent_class)->dbus_unregister(app, con, opath);
}


static void nk_ext_appl_init(NkExtAppl*) {}
static void nk_ext_appl_class_init(NkExtApplClass* class) {
	GApplicationClass* application_class = G_APPLICATION_CLASS(class);
	application_class->dbus_register = nk_ext_appl_dbus_register;
	application_class->dbus_unregister = nk_ext_appl_dbus_unregister;

	nk_ext_appl_signals[SIGNAL_EACTIVATE] = g_signal_new("eactivate", NOTEKIT_TYPE_APPLICATION, G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_VARIANT, G_TYPE_VARIANT);
}

AdwApplication* nk_ext_appl_new(void) {
	return g_object_new(NOTEKIT_TYPE_APPLICATION, "application-id", "arpa.sp1rit.NoteKit.NkLaTeX", NULL);
}

typedef struct {
	RsvgHandle** svg;
} NkLatexSvgAreaPrivate;

struct _NkLatexSvgAreaClass {
	GtkWidgetClass parent_class;

	gpointer padding[4];
};

#define NK_LATEX_TYPE_SVG_AREA (nk_latex_svg_area_get_type())
G_DECLARE_DERIVABLE_TYPE(NkLatexSvgArea, nk_latex_svg_area, NK_LATEX, SVG_AREA, GtkWidget)
G_DEFINE_TYPE_WITH_PRIVATE(NkLatexSvgArea, nk_latex_svg_area, GTK_TYPE_WIDGET)

static void nk_latex_svg_area_snapshot(GtkWidget* widget, GtkSnapshot* snapshot) {
	NkLatexSvgArea* self = NK_LATEX_SVG_AREA(widget);
	NkLatexSvgAreaPrivate* priv = nk_latex_svg_area_get_instance_private(self);

	g_return_if_fail(RSVG_HANDLE(*priv->svg));
	
	cairo_t* ctx;
	int width, height;
	
	width = gtk_widget_get_width(widget);
	height = gtk_widget_get_height(widget);
	ctx = gtk_snapshot_append_cairo(snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));
	
	RsvgRectangle rect;
	rsvg_handle_get_intrinsic_dimensions(*priv->svg, NULL, NULL, NULL, NULL, NULL, &rect);
	rect.width = width-2*rect.x;
	rect.height = height-2*rect.y;
	rsvg_handle_render_document(*priv->svg, ctx, &rect, NULL);

	cairo_destroy(ctx);
}

static void nk_latex_svg_area_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* min, int* nat, int*, int*) {
	NkLatexSvgArea* self = NK_LATEX_SVG_AREA(widget);
	NkLatexSvgAreaPrivate* priv = nk_latex_svg_area_get_instance_private(self);

	g_return_if_fail(RSVG_HANDLE(*priv->svg));
	
	RsvgRectangle rect;
	rsvg_handle_get_intrinsic_dimensions(RSVG_HANDLE(*priv->svg), NULL, NULL, NULL, NULL, NULL, &rect);

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		*min = 2*(rect.x + rect.width);

		double ar = (double)2*(rect.x + rect.width) / (double) (2*rect.y + rect.height);

		*nat = (ar*(double)for_size);
	} else {
		*min = 2*rect.y + rect.height;

		double ar = (double)(2*rect.y + rect.height) / (double) (2*(rect.x + rect.width));
		*nat = (ar*(double)for_size);
	}

	// TODO: figure out a way to set the widget to a maximum size relative to height

	if (*nat < *min) {
		*nat = *min;
	}
}

GtkSizeRequestMode nk_latex_svg_area_get_request_mode(GtkWidget*) {
	return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void nk_latex_svg_area_init(NkLatexSvgArea*) {}
static void nk_latex_svg_area_class_init(NkLatexSvgAreaClass* class) {
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
	widget_class->measure = nk_latex_svg_area_measure;
	widget_class->get_request_mode = nk_latex_svg_area_get_request_mode;
	widget_class->snapshot = nk_latex_svg_area_snapshot;
}

GtkWidget* nk_latex_svg_area_new(void) {
	return g_object_new(NK_LATEX_TYPE_SVG_AREA, NULL);
}

void nk_latex_svg_area_set_svg(NkLatexSvgArea* self, RsvgHandle** svg) {
	NkLatexSvgAreaPrivate* priv = nk_latex_svg_area_get_instance_private(self);
	g_return_if_fail(NK_LATEX_IS_SVG_AREA(self));
	g_return_if_fail(svg);

	priv->svg = svg;
}

typedef struct {
	GtkWidget* parent;
	GtkWidget* sister;
} NkLatexSizeContainerPrivate;

struct _NkLatexSizeContainerClass {
	AdwBinClass parent_class;

	gpointer padding[64];
};

#define NK_LATEX_TYPE_SIZE_CONTAINER (nk_latex_size_container_get_type())
G_DECLARE_DERIVABLE_TYPE(NkLatexSizeContainer, nk_latex_size_container, NK_LATEX, SIZE_CONTAINER, AdwBin)
G_DEFINE_TYPE_WITH_PRIVATE(NkLatexSizeContainer, nk_latex_size_container, ADW_TYPE_BIN)

static void nk_latex_size_container_measure(GtkWidget* widget, GtkOrientation orientation, int for_size, int* min, int* nat, int*, int*) {
	NkLatexSizeContainer* self = NK_LATEX_SIZE_CONTAINER(widget);
	NkLatexSizeContainerPrivate* priv = nk_latex_size_container_get_instance_private(self);

	g_return_if_fail(GTK_WIDGET(priv->parent));
	g_return_if_fail(GTK_WIDGET(priv->sister));

	if (orientation == GTK_ORIENTATION_HORIZONTAL) {
		int par_width = gtk_widget_get_allocated_width(priv->parent);
		int sis_min,sis_nat,sis_bl_min,sis_bl_nat;
		gtk_widget_measure(priv->sister, GTK_ORIENTATION_HORIZONTAL, for_size, &sis_min, &sis_nat, &sis_bl_min, &sis_bl_nat);
		*min = *nat = par_width - sis_nat;
	} else {
		*min = *nat = -1;
	}
}

GtkSizeRequestMode nk_latex_size_container_get_request_mode(GtkWidget*) {
	return GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT;
}

static void nk_latex_size_container_init(NkLatexSizeContainer*) {}
static void nk_latex_size_container_class_init(NkLatexSizeContainerClass* class) {
	GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(class);
	widget_class->measure = nk_latex_size_container_measure;
	widget_class->get_request_mode = nk_latex_size_container_get_request_mode;
}

GtkWidget* nk_latex_size_container_new(void) {
	return g_object_new(NK_LATEX_TYPE_SIZE_CONTAINER, NULL);
}

void nk_latex_size_container_set_widgets(NkLatexSizeContainer* self, GtkWidget* parent, GtkWidget* sister) {
	g_return_if_fail(NK_LATEX_IS_SIZE_CONTAINER(self));
	NkLatexSizeContainerPrivate* priv = nk_latex_size_container_get_instance_private(self);
	g_return_if_fail(GTK_WIDGET(parent));
	g_return_if_fail(GTK_WIDGET(sister));

	priv->parent = parent;
	priv->sister = sister;
}

enum {
	PANE_EDIT,
	PANE_RENDER
};

typedef struct LatexResultDataCb {
	int doc_fd;
	int svg_fd;
	GtkButton* btn;
	RsvgHandle** svg;
	GtkWidget* res_stack;
	NkLatexSvgArea* render;
	GtkLabel* error;
} LatexResultDataCb;
void latex_result_cb(GObject* src, GAsyncResult* res, LatexResultDataCb* user_data) {
	gtk_widget_set_sensitive(GTK_WIDGET(user_data->btn), TRUE);
	gtk_widget_set_visible(GTK_WIDGET(user_data->res_stack), TRUE);

	GError* err = NULL;
	if (!g_subprocess_wait_finish(G_SUBPROCESS(src), res, &err)) {
		g_critical("failed wating for latex: %s\n", err->message);
		g_error_free(err);
		return;
	}
	
	close(user_data->doc_fd);

	gint sign = g_subprocess_get_exit_status(G_SUBPROCESS(src));
	if (sign) {
		GInputStream* stderr_stream;
		GBytes* stderr_content;
		g_warning("compilation failed: scriped returned POSIX %d\n", sign);

		stderr_stream = g_subprocess_get_stderr_pipe(G_SUBPROCESS(src));
		GError* err = NULL;
		stderr_content = g_input_stream_read_bytes(stderr_stream, 65536, NULL, &err);
		if (err) {
			g_critical("failed reading stderr from compilation: %s\n", err->message);
			g_error_free(err);
			return;
		}

		gtk_label_set_text(user_data->error, (const char*)g_bytes_get_data(stderr_content, NULL));
		gtk_widget_set_visible(GTK_WIDGET(user_data->render), FALSE);
		
		g_bytes_unref(stderr_content);
		g_object_unref(stderr_stream);
		return;
	}
	
	off_t size = lseek(user_data->svg_fd, 0, SEEK_END);
	lseek(user_data->svg_fd, 0, SEEK_SET);
	char* buffer = malloc(size + 1);
	
	read(user_data->svg_fd, buffer, size);
	buffer[size] = 0x0;
	printf("svg: %s\n", buffer);
	
	const gchar* i = g_strdup_printf("/proc/self/fd/%d", user_data->svg_fd);
	RsvgHandle* handle;
	GError* perr = NULL;
	handle = rsvg_handle_new_from_file(i, &err);
	if (perr) {
		g_warning("unable to parse svg: %s\n", perr->message);
		g_error_free(perr);
	} else {
		g_object_unref(*user_data->svg);
		*user_data->svg = handle;
		//RsvgRectangle rect;
		//rsvg_handle_get_intrinsic_dimensions(handle, NULL, NULL, NULL, NULL, NULL, &rect);
		//gtk_drawing_area_set_content_width(user_data->render, 2*(rect.width+rect.x));
		//gtk_drawing_area_set_content_height(user_data->render, rect.height + rect.y);
		gtk_widget_set_visible(GTK_WIDGET(user_data->render), TRUE);
		gtk_widget_queue_draw(GTK_WIDGET(user_data->render));
	}
	
	g_free(buffer);
	g_object_unref(src);
	g_free(user_data);
}

static void updated_image_path(GObject* src, GAsyncResult* res, gpointer) {
	GError* err = NULL;
	if (!g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err)) {
		g_warning("Error %d failed updating edata path of image: %s\n", err->code, err->message);
		g_error_free(err);
		return;
	}
}

typedef struct NkActivateArgs {
	GVariant* file;
	GVariant* widget;
} NkActivateArgs;

typedef struct InsertNkeCbData {
	gchar* data;
	NkActivateArgs* args;
} InsertNkeCbData;
static void insert_nke_cb(GObject* src, GAsyncResult* res, InsertNkeCbData* user_data) {
	GError* err = NULL;
	GVariant* ret;
	ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
	if (err) {
		g_warning("Error %d failed sending image to NoteKit: %s\n", err->code, err->message);
		g_error_free(err);
		return;
	}
	g_variant_get(ret, "(@(ss))", &user_data->args->widget);

	const gchar* active_note;
	gchar* a_note_dir;
	gchar* a_note_name;
	const gchar* uuid;
	gchar* filepath;
	GFile* file;
	GFile* dir;

	g_variant_get(user_data->args->widget, "(ss)", &active_note, &uuid);
	a_note_dir = g_path_get_dirname(active_note);
	a_note_name = g_path_get_basename(active_note);
	//g_free(active_note);	
	
	gchar* sfx = g_strrstr(a_note_name, ".md");
	sfx[0] = 0x0;
	sfx[1] = 0x0;
	sfx[2] = 0x0;

	filepath = g_strdup_printf("%s/.%s/%s~%s.tex", a_note_dir, APPL_ID, a_note_name, uuid);
	g_free(a_note_dir);
	g_free(a_note_name);


	file = g_file_new_for_path(filepath);
	dir = g_file_get_parent(file);
	g_file_make_directory(dir, NULL, NULL);
	g_object_unref(dir);
	GError* rerr = NULL;
	g_file_replace_contents(file, user_data->data, strlen(user_data->data), NULL, false, G_FILE_CREATE_NONE, NULL, NULL, &rerr);
	if (rerr) {
		g_critical("failed saving document: %s\n", rerr->message);
		g_error_free(rerr);
	}
	user_data->args->file = g_variant_new("s", filepath);

	g_dbus_connection_call(G_DBUS_CONNECTION(src),
		"com.github.blackhole89.notekit",
		"/com/github/blackhole89/NoteKit/Notebook/1",
		"com.github.blackhole89.NoteKit.Notebook",
		"update_nke_edata",
		g_variant_new("(@(ss)s)",
			g_variant_ref(user_data->args->widget),
			filepath
		),
		NULL,
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		(GAsyncReadyCallback) updated_image_path,
		NULL
	);

	g_print("inserted image to notekit\n");
	g_free(user_data->data);
	g_free(user_data);
}

static void sent_msg_cb(GObject* src, GAsyncResult* res, gpointer) {
	GError* err;
	if (!g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err)) {
		g_warning("Error %d failed sending image to NoteKit: %s\n", err->code, err->message);
		g_error_free(err);
		return;
	}
	g_print("sent image to notekit\n");
}

typedef struct PBtnClickedData {
	GSettings* settings;
	GtkSourceBuffer* buf;
	AdwLeaflet* leaflet;
	GtkWidget* res;
	int* svg_fd;
	RsvgHandle** svg;
	GtkWidget* res_stack;
	NkLatexSvgArea* render;
	GtkLabel* error;
	guint8* pane_state;
	GDBusConnection* con;
	NkActivateArgs* args;
} PBtnClickedData;
static void pbtn_clicked(GtkButton* btn, PBtnClickedData* user_data) {
	if (*user_data->pane_state == PANE_EDIT) {
		GtkTextIter start,end;
		GString* packages;
		const gchar* custom_preamble;
		const char* input;
		const char* doc;

		gtk_widget_set_visible(user_data->res, TRUE);
		gtk_widget_set_visible(GTK_WIDGET(user_data->res_stack), FALSE);
		gtk_button_set_label(btn, "Export");
		gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
		adw_leaflet_navigate(user_data->leaflet, ADW_NAVIGATION_DIRECTION_FORWARD);
		*user_data->pane_state = PANE_RENDER;
		
		gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(user_data->buf), &start);
		gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(user_data->buf), &end);
		input = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(user_data->buf), &start, &end, FALSE);
		printf("goin to render: %s\n", input);

		packages = g_string_new("");
		if (g_settings_get_boolean(user_data->settings, "pkg-tikz"))
			g_string_append(packages, "\\usepackage{tikz}\n");
		if (g_settings_get_boolean(user_data->settings, "pkg-circuitikz"))
			g_string_append(packages, "\\usepackage{circuitikz}\n");
		if (g_settings_get_boolean(user_data->settings, "pkg-chemfig"))
			g_string_append(packages, "\\usepackage{chemfig}\n");
		if (g_settings_get_boolean(user_data->settings, "pkg-mhchem"))
			g_string_append(packages, "\\usepackage{mhchem}\n");

		custom_preamble = g_settings_get_string(user_data->settings, "custom-preamble");
		
		doc = g_strdup_printf(	"\\documentclass[10pt,dvisvgm]{article}\n"
					"\\usepackage{amsmath}\n"
					"\\usepackage{amssymb}\n"
					"\\usepackage[usenames]{color}\n"
					"\\usepackage{ifxetex}\n"
					"\n"
					"%% XeLaTeX compiler\n"
					"\\ifxetex\n"
					"\n"
					"    \\usepackage{fontspec}\n"
					"    \\usepackage{unicode-math}\n"
					"\n"
					"    %% Uncomment these lines for alternative fonts\n"
					"    %%\\setmainfont{FreeSerif}\n"
					"    %%\\setmathfont{FreeSerif}\n"
					"\n"
					"%% LaTeX compiler\n"
					"\\else\n"
					"\n"
					"    %% Uncomment this line for sans-serif maths font\n"
					"    %%\\everymath{\\mathsf{\\xdef\\mysf{\\mathgroup\\the\\mathgroup\\relax}}\\mysf}\n"
					"\n"
					"\\fi\n"
					"\n"
					"%s"
					"%s"
					"\n"
					"\\pagestyle{empty}\n"
					"\n"
					"\\begin{document}\n"
					"\\newsavebox{\\eqbox}\n"
					"\\newlength{\\width}\n"
					"\\newlength{\\height}\n"
					"\\newlength{\\depth}\n"
					"\\begin{lrbox}{\\eqbox}\n"
					"{$\\displaystyle\n"
					"	%s\n"
					"$}\n"
					"\\end{lrbox}\n"
					"\\settowidth {\\width}  {\\usebox{\\eqbox}}\n"
					"\\settoheight{\\height} {\\usebox{\\eqbox}}\n"
					"\\settodepth {\\depth}  {\\usebox{\\eqbox}}\n"
					"\\newwrite\\file\n"
					"\\immediate\\openout\\file=\\jobname.bsl\n"
					"\\immediate\\write\\file{Depth = \\the\\depth}\n"
					"\\immediate\\write\\file{Height = \\the\\height}\n"
					"\\addtolength{\\height} {\\depth}\n"
					"\\immediate\\write\\file{TotalHeight = \\the\\height}\n"
					"\\immediate\\write\\file{Width = \\the\\width}\n"
					"\\closeout\\file\n"
					"\\usebox{\\eqbox}\n"
					"\\end{document}\n"
					"\n", packages->str, custom_preamble, input);

		g_string_free(packages, TRUE);
		
		int fd = memfd_create("latex_doc.tex", 0);
		if (fd == -1) {
			char* err = strerror(errno);
			g_critical("failed creating memfd: %s\n", err);
			free(err);
			return;
		}
		ssize_t res = write(fd, doc, strlen(doc));
		if (res == -1) {
			char* err = strerror(errno);
			g_critical("failed writing to memfd: %s\n", err);
			free(err);
			return;
		}
		const gchar* doc_path = g_strdup_printf("/proc/%d/fd/%d\n", getpid(), fd);

		if (*user_data->svg_fd != 0)
			close(*user_data->svg_fd);
		*user_data->svg_fd = memfd_create("result.svg", 0);
		if (*user_data->svg_fd == -1) {
			char* err = strerror(errno);
			g_critical("failed creating memfd: %s\n", err);
			free(err);
			return;
		}
		const gchar* svg_path = g_strdup_printf("/proc/%d/fd/%d\n", getpid(), *user_data->svg_fd);

		const gchar* env = g_getenv("NK_LATEX_LATEX2SVG_LOCATION");
		GSubprocess* proc;		
		GError* err = NULL;
		if (env)
			proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &err, env, doc_path, svg_path, NULL);
		else
			proc = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &err, LATEX2SVG_LOCATION, doc_path, svg_path, NULL);
		if (err) {
			g_warning("Failed launching latex2svg: %s\n", err->message);
			g_error_free(err);
			return;
		}
		LatexResultDataCb* lres_d = g_new(LatexResultDataCb, 1);
		lres_d->doc_fd = fd;
		lres_d->svg_fd = *user_data->svg_fd;
		lres_d->btn = btn;
		lres_d->svg = user_data->svg;
		lres_d->res_stack = user_data->res_stack;
		lres_d->render = user_data->render;
		lres_d->error = user_data->error;
		g_subprocess_wait_async(proc, NULL, (GAsyncReadyCallback)latex_result_cb, lres_d);
	} else {
		char* tex;

		off_t size = lseek(*user_data->svg_fd, 0, SEEK_END);
		lseek(*user_data->svg_fd, 0, SEEK_SET);
		guint8* buffer = malloc(size);
	
		read(*user_data->svg_fd, buffer, size);

		guint8* data;
		/*size_t cbound = ZSTD_compressBound(size);
		data = g_new(guint8, cbound);
		size_t ret = ZSTD_compress(data, cbound, buffer, size, ZSTD_defaultCLevel());
		if (ZSTD_isError(ret))
			g_critical("failed compressing svg: %s\n", ZSTD_getErrorName(ret));*/
		unsigned long ret;
		unsigned long cbound = compressBound(size);
		guint32 fsize = (guint32)size;
		data = g_new(guint8, cbound+4);
		memcpy(data, &fsize, 4);
		int res = compress(&data[4], &ret, buffer, size);
		if (res != Z_OK)
			g_critical("faild compressing svg: Error %d\n", res);

		g_free(buffer);

		GVariantBuilder builder;
		g_variant_builder_init (&builder, G_VARIANT_TYPE("ay"));
		for (gsize l = 0; l < ret + 4; l++)
			g_variant_builder_add (&builder, "y", data[l]);

		GtkTextIter start,end;
		gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(user_data->buf), &start, &end);
		tex = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(user_data->buf), &start, &end, FALSE);
		
		if (user_data->args->widget != NULL) {
			const gchar* filepath;
			GFile* file;

			filepath = g_variant_get_string(user_data->args->file, NULL);
			file = g_file_new_for_path(filepath);

			GError* err = NULL;
			g_file_replace_contents(file, tex, strlen(tex), NULL, false, G_FILE_CREATE_NONE, NULL, NULL, &err);
			if (err) {
				g_critical("failed saving document: %s\n", err->message);
				g_error_free(err);
			}
			g_object_unref(file);
			//g_free(filepath);
			g_free(tex);

			g_dbus_connection_call(user_data->con,
				"com.github.blackhole89.notekit",
				"/com/github/blackhole89/NoteKit/Notebook/1",
				"com.github.blackhole89.NoteKit.Notebook",
				"update_nke",
				g_variant_new("(@(ss)@(usay))",
					g_variant_ref(user_data->args->widget),
					g_variant_new("(usay)", 2, "image/svg+xml", &builder)
				),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback) sent_msg_cb,
				NULL
			);
		} else {
			InsertNkeCbData* cb_data = g_new(InsertNkeCbData, 1);
			cb_data->args = user_data->args;
			cb_data->data = tex;

			g_dbus_connection_call(user_data->con,
				"com.github.blackhole89.notekit",
				"/com/github/blackhole89/NoteKit/Notebook/1",
				"com.github.blackhole89.NoteKit.Notebook",
				"insert_nke",
				g_variant_new("(@(usay)@(ss))",
					g_variant_new("(usay)", 2, "image/svg+xml", &builder),
					g_variant_new("(ss)",
						APPL_ID,
						""
					)
				),
				NULL,
				G_DBUS_CALL_FLAGS_NONE,
				-1,
				NULL,
				(GAsyncReadyCallback) insert_nke_cb,
				cb_data
			);
		}
		g_free(data);
	}
}
typedef struct GoEditPaneData {
	GtkButton* pbtn;
	AdwLeaflet* leaflet;
	guint8* pane_state;
} GoEditPaneData;
static void go_edit_pane(GtkWidget*, GoEditPaneData* user_data) {
	gtk_button_set_label(user_data->pbtn, "Render");
	adw_leaflet_navigate(user_data->leaflet, ADW_NAVIGATION_DIRECTION_BACK);
	*user_data->pane_state = PANE_EDIT;
}

static void save_preamble(GtkSourceBuffer* buf, GSettings* settings) {
	char* preamble;

	GtkTextIter start,end;
	gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(buf), &start, &end);
	preamble = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buf), &start, &end, FALSE);

	g_settings_set_string(settings, "custom-preamble", preamble);
	
	g_free(preamble);
}

typedef struct PreferencesWindowData {
	GtkWindow* parent;
	GSettings* settings;
} PreferencesWindowData;
static void preferences_window(GObject*, GVariant*, PreferencesWindowData* user_data) {
	GtkBuilder* bld;
	GtkWidget *win,*tikz,*circuitikz,*chemfig,*mhchem;
	GtkSourceBuffer* preamble;
	GtkSourceLanguageManager* lm;
	GtkSourceLanguage* tex;
	bld = gtk_builder_new_from_resource("/arpa/sp1rit/NoteKit/NkLaTeX/prefs.ui");
	win = GTK_WIDGET(gtk_builder_get_object(bld, "pref_win"));
	tikz = GTK_WIDGET(gtk_builder_get_object(bld, "pkg_tikz"));
	circuitikz = GTK_WIDGET(gtk_builder_get_object(bld, "pkg_circuitikz"));
	chemfig = GTK_WIDGET(gtk_builder_get_object(bld, "pkg_chemfig"));
	mhchem = GTK_WIDGET(gtk_builder_get_object(bld, "pkg_mhchem"));
	
	preamble = GTK_SOURCE_BUFFER(gtk_builder_get_object(bld, "preamble"));
	lm = gtk_source_language_manager_get_default();
	tex = gtk_source_language_manager_get_language(lm, "latex");
	gtk_source_buffer_set_language(preamble, tex);

	gchar* preamble_text = g_settings_get_string(user_data->settings, "custom-preamble");
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(preamble), preamble_text, -1);
	g_free(preamble_text);

	g_settings_bind(user_data->settings, "pkg-tikz", G_OBJECT(tikz), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(user_data->settings, "pkg-circuitikz", G_OBJECT(circuitikz), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(user_data->settings, "pkg-chemfig", G_OBJECT(chemfig), "active", G_SETTINGS_BIND_DEFAULT);
	g_settings_bind(user_data->settings, "pkg-mhchem", G_OBJECT(mhchem), "active", G_SETTINGS_BIND_DEFAULT);
	g_signal_connect(preamble, "changed", G_CALLBACK(save_preamble), user_data->settings);

	gtk_window_set_transient_for(GTK_WINDOW(win), user_data->parent);
	gtk_window_set_modal(GTK_WINDOW(win), TRUE);
	gtk_widget_show(win);

}

static void about_window(GObject*, GVariant*, gpointer user_data) {
	GtkWidget* diag = gtk_about_dialog_new();
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(diag), "NkLaTeX");
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(diag), APPL_ID);
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(diag), "Latex typesetting utility for NoteKit");
	gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(diag), GTK_LICENSE_AGPL_3_0);
	gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(diag), TRUE);
	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(diag), VERSION);
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(diag), "Copyright (c) 2021 Florian \"sp1rit\" and contributors");

	gtk_window_set_transient_for(GTK_WINDOW(diag), GTK_WINDOW(user_data));
	gtk_window_set_modal(GTK_WINDOW(diag), TRUE);
	gtk_widget_show(diag);
}

typedef struct DestroyData {
	NkActivateArgs* args;

	RsvgHandle** svg;
	int* svg_fd;
	guint8* pane_state;

	PBtnClickedData* pbtn_d;
	GoEditPaneData* epane_d;
	PreferencesWindowData* pref_d;
} DestroyData;

static void destroy(GtkWidget*, DestroyData* user_data) {
	if (user_data->args->widget)
		g_variant_unref(user_data->args->widget);
	if (user_data->args->file)
		g_variant_unref(user_data->args->file);
	g_free(user_data->args);

	g_object_unref(*user_data->svg);
	g_free(user_data->svg);
	close(*user_data->svg_fd);
	g_free(user_data->svg_fd);
	g_free(user_data->pane_state);

	g_free(user_data->pbtn_d);
	g_free(user_data->epane_d);
	g_free(user_data->pref_d);

	g_free(user_data);
}

static void activate(GtkApplication* app, NkActivateArgs* args) {
	GtkWidget* window;
	GtkBuilder* bld;
	GtkWidget *inner,*pbtn,*bbtn,*leaflet,*cont,*res,*result_stack,*render,*error_view,*error,*spinner;
	GtkSourceBuffer* buf;
	GtkSourceLanguageManager* lm;
	GtkSourceLanguage* tex;

	RsvgHandle** svg = g_new(RsvgHandle*, 1);
	int* svg_fd = g_new(int, 1);
	*svg_fd = 0;
	guint8* pane_state = g_new(guint8, 1);
	*pane_state = PANE_EDIT;

	*svg = rsvg_handle_new_from_data((guint8*)"<svg></svg>", 13, NULL);
	
	window = adw_application_window_new(app);
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
	gtk_window_set_title(GTK_WINDOW(window), "NoteKit - LaTeX");
	if (DEBUG_MODE)
		gtk_widget_add_css_class(window, "devel");
	
	g_type_ensure(NK_LATEX_TYPE_SVG_AREA);
	g_type_ensure(NK_LATEX_TYPE_SIZE_CONTAINER);
	bld = gtk_builder_new_from_resource("/arpa/sp1rit/NoteKit/NkLaTeX/nklatex.ui");
	inner = GTK_WIDGET(gtk_builder_get_object(bld, "inner"));
	pbtn = GTK_WIDGET(gtk_builder_get_object(bld, "pbtn"));
	bbtn = GTK_WIDGET(gtk_builder_get_object(bld, "bbtn"));
	leaflet = GTK_WIDGET(gtk_builder_get_object(bld, "leaflet"));
	cont = GTK_WIDGET(gtk_builder_get_object(bld, "scont"));
	res = GTK_WIDGET(gtk_builder_get_object(bld, "res"));
	result_stack = GTK_WIDGET(gtk_builder_get_object(bld, "result_stack"));
	render = GTK_WIDGET(gtk_builder_get_object(bld, "render"));
	error_view = GTK_WIDGET(gtk_builder_get_object(bld, "error_view"));
	error = GTK_WIDGET(gtk_builder_get_object(bld, "error"));
	spinner = GTK_WIDGET(gtk_builder_get_object(bld, "spinner"));
	
	buf = GTK_SOURCE_BUFFER(gtk_builder_get_object(bld, "buffer"));
	lm = gtk_source_language_manager_get_default();
	tex = gtk_source_language_manager_get_language(lm, "latex");
	gtk_source_buffer_set_language(buf, tex);
	
	PBtnClickedData* pbtn_d = g_new(PBtnClickedData, 1);
	pbtn_d->settings = G_SETTINGS(g_object_get_data(G_OBJECT(app), "settings"));
	pbtn_d->buf = buf;
	pbtn_d->leaflet = ADW_LEAFLET(leaflet);
	pbtn_d->res = res;
	pbtn_d->svg_fd = svg_fd;
	pbtn_d->svg = svg;
	pbtn_d->res_stack = GTK_WIDGET(result_stack);
	pbtn_d->render = NK_LATEX_SVG_AREA(render);
	pbtn_d->error = GTK_LABEL(error);
	pbtn_d->pane_state = pane_state;
	pbtn_d->con = g_application_get_dbus_connection(G_APPLICATION(app));
	pbtn_d->args = args;
	g_signal_connect(pbtn, "clicked", G_CALLBACK(pbtn_clicked), pbtn_d);

	GoEditPaneData* epane_d = g_new(GoEditPaneData, 1);
	epane_d->pbtn = GTK_BUTTON(pbtn);
	epane_d->leaflet = ADW_LEAFLET(leaflet);
	epane_d->pane_state = pane_state;
	g_signal_connect(bbtn, "clicked", G_CALLBACK(go_edit_pane), epane_d);
	g_signal_connect(buf, "changed", G_CALLBACK(go_edit_pane), epane_d);

	g_object_bind_property(G_OBJECT(result_stack), "visible", G_OBJECT(spinner), "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
	g_object_bind_property(G_OBJECT(result_stack), "visible", G_OBJECT(spinner), "spinning", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
	g_object_bind_property(G_OBJECT(render), "visible", G_OBJECT(error_view), "visible", G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

	nk_latex_svg_area_set_svg(NK_LATEX_SVG_AREA(render), svg);
	nk_latex_size_container_set_widgets(NK_LATEX_SIZE_CONTAINER(cont), window, res);

	PreferencesWindowData* pref_d = g_new(PreferencesWindowData, 1);
	pref_d->parent = GTK_WINDOW(window);
	pref_d->settings = G_SETTINGS(g_object_get_data(G_OBJECT(app), "settings"));
	GSimpleAction* prefs = g_simple_action_new("preferences", NULL);
	g_signal_connect(prefs, "activate", G_CALLBACK(preferences_window), pref_d);
	g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(prefs));
	GSimpleAction* about = g_simple_action_new("about", NULL);
	g_signal_connect(about, "activate", G_CALLBACK(about_window), window);
	g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(about));

	if (args->file != NULL) {
		GFile* file = g_file_new_for_path(g_variant_get_string(args->file, NULL));
		GError* err = NULL;
		GBytes* data = g_file_load_bytes(file, NULL, NULL, &err);
		if (err) {
			g_warning("error loading data: %s\n", err->message);
			g_error_free(err);
		} else {
			gsize len;
			// TODO: length calculations are wrong on multi-line files
			gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buf), g_bytes_get_data(data, &len), -1);
			g_bytes_unref(data);
		}
		g_object_unref(file);
	}

	DestroyData* destroy_d = g_new(DestroyData, 1);
	destroy_d->args = args;
	destroy_d->svg = svg;
	destroy_d->svg_fd = svg_fd;
	destroy_d->pane_state = pane_state;
	destroy_d->pbtn_d = pbtn_d;
	destroy_d->epane_d = epane_d;
	destroy_d->pref_d = pref_d;
	g_signal_connect(window, "destroy", G_CALLBACK(destroy), destroy_d);

	adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), inner);
	gtk_widget_show(window);
}

static void nk_activate(GtkApplication* app, GVariant* widget, GVariant* path, gpointer) {
	NkActivateArgs* args = g_new(NkActivateArgs, 1);
	args->file = g_variant_ref(path);
	args->widget = widget;
	
	activate(app, args);
}

static void user_activate(GtkApplication* app) {
	NkActivateArgs* args = g_new(NkActivateArgs, 1);
	args->file = NULL;
	args->widget = NULL;
	activate(app, args);
}

int main(int argc, char** argv) {
	AdwApplication* app;
	GSettings* settings;
	int status;
	
	gtk_source_init();
	g_resources_register(nkl_resource_get_resource());
	app = nk_ext_appl_new();

	settings = g_settings_new(APPL_ID);
	g_object_set_data(G_OBJECT(app), "settings", settings);

	g_signal_connect(app, "activate", G_CALLBACK(user_activate), NULL);
	g_signal_connect(app, "eactivate", G_CALLBACK(nk_activate), NULL);

	status = g_application_run(G_APPLICATION(app), argc, argv);

	g_object_unref(app);

	return status;
}
