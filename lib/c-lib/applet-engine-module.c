#include "applet-engine-module.h"
#include <gmodule.h>

struct _ValaPanelAppletEngineModule
{
	GTypeModule __parent__;
	char *filename;
	uint use_count;
	GModule *library;
	GType plugin_type;
};

G_DEFINE_TYPE(ValaPanelAppletEngineModule, vala_panel_applet_engine_module, G_TYPE_TYPE_MODULE)

static bool vala_panel_applet_engine_module_finalize(GTypeModule *type_module)
{
	ValaPanelAppletEngineModule *module = VALA_PANEL_APPLET_ENGINE_MODULE(type_module);

	g_free(module->filename);

	(*G_OBJECT_CLASS(vala_panel_applet_engine_module_parent_class)->finalize)(type_module);
}

static bool vala_panel_applet_engine_module_load(GTypeModule *type_module)
{
	ValaPanelAppletEngineModule *module = VALA_PANEL_APPLET_ENGINE_MODULE(type_module);
	PluginInitFunc init_func;
	gboolean make_resident = TRUE;
	gpointer foo;

	/* open the module */
	module->library = g_module_open(module->filename, G_MODULE_BIND_LOCAL);
	if (G_UNLIKELY(module->library == NULL))
	{
		g_critical("Failed to load module \"%s\": %s.", module->filename, g_module_error());
		return FALSE;
	}

	/* try to link the contruct function */
	if (g_module_symbol(module->library, "vala_panel_module_init", (gpointer)&init_func))
	{
		/* initialize the plugin */
		module->plugin_type = init_func(type_module, &make_resident);

		/* whether to make this plugin resident or not */
		if (make_resident)
			g_module_make_resident(module->library);
	}
	else
	{
		g_critical("Module \"%s\" lacks a plugin register function.", module->filename);

		panel_module_unload(type_module);

		return FALSE;
	}

	return TRUE;
}

void vala_panel_applet_engine_module_init(ValaPanelAppletEngineModule *self)
{
}
void vala_panel_applet_engine_module_class_init(ValaPanelAppletEngineModuleClass *klass)
{
}
