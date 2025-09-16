// This file can be included several times.

#ifndef CONFIG_DOMAIN
#error "CONFIG_DOMAIN macro not defined"
#define CONFIG_DOMAIN(Name, ConfigPath, HasVars) ;
#endif

CONFIG_DOMAIN(DDNET, "settings_ddnet.cfg", true)
CONFIG_DOMAIN(ENTITY, "settings_entity.cfg", true)
// E-Client
CONFIG_DOMAIN(ENTITYBINDHWEEL, "entity_bindwheel.cfg", false)
CONFIG_DOMAIN(ENTITYQUICKACTIONS, "entity_quickactions.cfg", false)
// T-Client
CONFIG_DOMAIN(TCLIENTPROFILES, "tclient_profiles.cfg", false)
CONFIG_DOMAIN(TCLIENTCHATBINDS, "tclient_chatbinds.cfg", false)
CONFIG_DOMAIN(TCLIENTWARLIST, "tclient_warlist.cfg", false)
