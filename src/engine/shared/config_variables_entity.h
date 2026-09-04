// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// Auto Reply
MACRO_CONFIG_STR(ClAutoReplyMsg, ec_auto_reply_msg, 255, "I'm currently tabbed out", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when pinged in chat and ec_auto_reply is set to 1")
MACRO_CONFIG_INT(ClTabbedOutMsg, ec_tabbed_out_msg, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let others know when pinged in chat that you are tabbed out")

MACRO_CONFIG_INT(ClNotifyOnMove, ec_notify_on_move, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notifies you when your player moves")

MACRO_CONFIG_COL(ClECMessageColor, ec_message_color, 9633471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color of warlist cmds (usually)")
MACRO_CONFIG_INT(ClMutedConsoleColor, ec_muted_console_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "colors in console for warlist")

// Anti Latency Tools
MACRO_CONFIG_INT(EcRemoveAnti, ec_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Removes some amount of antiping & player prediction in freeze")
MACRO_CONFIG_INT(EcUnfreezeLagTicks, ec_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The biggest amount of prediction ticks that are removed")
MACRO_CONFIG_INT(EcUnfreezeLagDelayTicks, ec_remove_anti_delay_ticks, 25, 5, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks it takes to remove the maximum prediction after being frozen")

MACRO_CONFIG_INT(EcUnpredOthersInFreeze, ec_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont predict other players if you are frozen")
MACRO_CONFIG_INT(EcPredMarginInFreeze, ec_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable changing prediction margin while frozen")
MACRO_CONFIG_INT(EcPredMarginInFreezeAmount, ec_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set what your prediction margin while frozen should be")

MACRO_CONFIG_INT(EcShowOthersGhosts, ec_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghosts for other players in their unpredicted position")
MACRO_CONFIG_INT(EcSwapGhosts, ec_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show predicted players as ghost and normal players as unpredicted")
MACRO_CONFIG_INT(EcHideFrozenGhosts, ec_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide Ghosts of other players if they are frozen")

MACRO_CONFIG_INT(EcPredGhostsAlpha, ec_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of predicted ghosts (0-100)")
MACRO_CONFIG_INT(EcUnpredGhostsAlpha, ec_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of unpredicted ghosts (0-100)")
MACRO_CONFIG_INT(EcRenderGhostAsCircle, ec_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render Ghosts as circles instead of tee")

// Outline Variables
MACRO_CONFIG_INT(ClOutline, ec_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outlines")
MACRO_CONFIG_INT(ClOutlineEntities, ec_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show outlines in entities")
MACRO_CONFIG_INT(ClOutlineFreeze, ec_outline_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around freeze and deep")
MACRO_CONFIG_INT(ClOutlineUnfreeze, ec_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around unfreeze and undeep")
MACRO_CONFIG_INT(ClOutlineSolid, ec_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around hook and unhook")
MACRO_CONFIG_INT(ClOutlineTele, ec_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around teleporters")
MACRO_CONFIG_INT(ClOutlineKill, ec_outline_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around kill")
MACRO_CONFIG_INT(ClOutlineWidthFreeze, ec_outline_width_freeze, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of freeze outline")
MACRO_CONFIG_INT(ClOutlineWidthUnfreeze, ec_outline_width_unfreeze, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of unfreeze outline")
MACRO_CONFIG_INT(ClOutlineWidthSolid, ec_outline_width_solid, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of solid outline")
MACRO_CONFIG_INT(ClOutlineWidthTele, ec_outline_width_tele, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of tele outline")
MACRO_CONFIG_INT(ClOutlineWidthKill, ec_outline_width_kill, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of kill outline")
MACRO_CONFIG_COL(ClOutlineColorFreeze, ec_outline_color_freeze, 2097152251, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Freeze outline color")
MACRO_CONFIG_COL(ClOutlineColorUnfreeze, ec_outline_color_unfreeze, 2099380187, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Unfreeze outline color")
MACRO_CONFIG_COL(ClOutlineColorSolid, ec_outline_color_solid, 2108620759, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Solid outline color")
MACRO_CONFIG_COL(ClOutlineColorTele, ec_outline_color_tele, 2113893167, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Tele outline color")
MACRO_CONFIG_COL(ClOutlineColorKill, ec_outline_color_kill, 2097217461, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Unfreeze outline color")

// Fast Input
MACRO_CONFIG_INT(EcFastInput, ec_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction before the next tick")
MACRO_CONFIG_INT(EcFastInputAmount, ec_fast_input_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many milliseconds fast input will apply")
MACRO_CONFIG_INT(EcFastInputOthers, ec_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply fast input to other tees")
MACRO_CONFIG_INT(EcFastInputMode, ec_fast_input_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fast input algorithm: 0 = classic, 1 = flux (caps other tees to one extrapolated tick and waits for server pre-input)")
MACRO_CONFIG_INT(EcFluxInputAmount, ec_flux_input_amount, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Flux prediction offset, in hundredths of a tick (100 = 1.00 ticks). Only used when ec_fast_input_mode is set to flux")

// Anti Ping Smoothing
MACRO_CONFIG_INT(EcAntiPingImproved, ec_antiping_improved, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Different antiping smoothing algorithm, not compatible with cl_antiping_smooth")
MACRO_CONFIG_INT(EcAntiPingNegativeBuffer, ec_antiping_negative_buffer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helps in Gores. Allows internal certainty value to be negative which causes more conservative prediction")
MACRO_CONFIG_INT(EcAntiPingStableDirection, ec_antiping_stable_direction, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predicts optimistically along the tees stable axis to reduce delay in gaining overall stability")
MACRO_CONFIG_INT(EcAntiPingUncertaintyScale, ec_antiping_uncertainty_scale, 150, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Determines uncertainty duration as a factor of ping, 100 = 1.0")

// Revert Variables
MACRO_CONFIG_INT(EcSmoothPredictionMargin, ec_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes prediction margin transition smooth, causes worse ping jitter adjustment (reverts a ddnet change)")
MACRO_CONFIG_INT(EcFrozenKatana, ec_frozen_katana, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show katana on frozen players (reverts a DDNet change)")

// War List
MACRO_CONFIG_INT(ClWarList, ec_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles war list visuals")
MACRO_CONFIG_INT(ClWarListShowClan, ec_warlist_show_clan_if_war, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in nameplate if there is a war")
MACRO_CONFIG_INT(ClWarListReason, ec_warlist_reason, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason")
MACRO_CONFIG_INT(ClWarListChat, ec_warlist_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in chat")
MACRO_CONFIG_INT(ClWarlistPrefixes, ec_warlist_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Warlist Prefix")
MACRO_CONFIG_INT(ClWarListScoreboard, ec_warlist_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in scoreboard")
MACRO_CONFIG_INT(ClWarListSpecMenu, ec_warlist_specmenu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in spectate menu")
MACRO_CONFIG_INT(ClWarListAllowDuplicates, ec_warlist_allow_duplicates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow duplicate war entries")

MACRO_CONFIG_INT(ClWarListIndicator, ec_warlist_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use warlist for indicator")
MACRO_CONFIG_INT(ClWarListIndicatorColors, ec_warlist_indicator_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show warlist colors instead of freeze colors")
MACRO_CONFIG_INT(ClWarListIndicatorAll, ec_warlist_indicator_all, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show all groups")
MACRO_CONFIG_INT(ClWarListIndicatorEnemy, ec_warlist_indicator_enemy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from the first group")
MACRO_CONFIG_INT(ClWarListIndicatorTeam, ec_warlist_indicator_team, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from second group")

MACRO_CONFIG_INT(ClWarListSwapNameReason, ec_warlist_swap_name_reason, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If a Player has a Reason, their Name will be switched with the Reason")
MACRO_CONFIG_INT(ClWarListColorJoinLeave, ec_warlist_color_join_leave, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color a players name in the join/leave message")

// Server Browser
MACRO_CONFIG_INT(ClWarlistBrowser, ec_warlist_browser, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show online warlist players in the server browser or not")
MACRO_CONFIG_INT(ClWarlistBrowserFlags, ec_warlist_browser_flags, 0, 0, 262144, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to hide wartypes in server browser")
MACRO_CONFIG_INT(ClWarlistAutoAddFlags, ec_warlist_auto_add_flags, 4, 0, 262144, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to disable wartypes getting auto added")
MACRO_CONFIG_INT(ClWarlistFrozenTeeFlags, ec_warlist_frozen_tee_flags, 0, 0, 262144, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to hide/show wartypes getting in frozen tee hud")

// Server Info
MACRO_CONFIG_INT(ClWarlistPrefixesServerInfo, ec_warlist_prefixes_server_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show warlist prefixes in the server info")

// Run on Join
MACRO_CONFIG_STR(ClExecuteOnConnect, ec_execute_on_connect, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Run a console command before connect")
MACRO_CONFIG_STR(ClRunOnJoinConsole, ec_run_on_join_console, 128, "onlineinfo", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What it should run inside of the Console")
MACRO_CONFIG_INT(ClRunOnJoinConsoleDelay, ec_run_on_join_delay, 2, 1, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick Delay before using run on join")

// Limit Mouse
MACRO_CONFIG_INT(ClLimitMouseToScreen, ec_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundaries")
MACRO_CONFIG_INT(ClScaleMouseDistance, ec_scale_mouse_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Improve mouse precision by scaling max distance to 1000")
MACRO_CONFIG_INT(ClUiMouseBorderTeleport, ec_ui_mouse_border_teleport, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Teleport Mouse from one border to the other if it goes off screen")

// Frozen Tee Display
MACRO_CONFIG_INT(ClShowFrozenText, ec_frozen_tees_text, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show how many tees in your team are currently frozen. (0 - off, 1 - show alive, 2 - show frozen)")
MACRO_CONFIG_INT(ClShowFrozenHud, ec_frozen_tees_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show frozen tee HUD")
MACRO_CONFIG_INT(ClShowFrozenHudSkins, ec_frozen_tees_hud_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use ninja skin, or darkened skin for frozen tees on hud")

MACRO_CONFIG_INT(ClFrozenHudTeeSize, ec_frozen_tees_size, 15, 8, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees in frozen tee hud. (Default : 15)")
MACRO_CONFIG_INT(ClFrozenMaxRows, ec_frozen_tees_max_rows, 1, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of rows in frozen tee HUD display")
MACRO_CONFIG_INT(ClFrozenHudTeamOnly, ec_frozen_tees_only_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only render frozen tee HUD display while in team")

// Notify When Last
MACRO_CONFIG_INT(ClNotifyWhenLast, ec_last_notify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when you are last")
MACRO_CONFIG_STR(ClNotifyWhenLastText, ec_last_notify_text, 64, "Last!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text for last notify")
MACRO_CONFIG_COL(ClNotifyWhenLastColor, ec_last_notify_color, 29057, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color for last notify")

// Spectate Cursor
MACRO_CONFIG_INT(ClCursorOpacitySpec, ec_cursor_opacity_spec, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of your cursor when spectating in freeview")

// Pingcircle next to name
MACRO_CONFIG_INT(ClPingNameCircle, ec_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle next to nameplate to indicate ping")

// Indicator Variables
MACRO_CONFIG_COL(ClIndicatorAlive, ec_indicator_alive, 5636044, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorFreeze, ec_indicator_freeze, 9725116, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorSaved, ec_indicator_dead, 3145695, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
MACRO_CONFIG_INT(ClIndicatorOffset, ec_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Offset of indicator position")
MACRO_CONFIG_INT(ClIndicatorOffsetMax, ec_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Max indicator offset for variable offset setting")
MACRO_CONFIG_INT(ClIndicatorVariableDistance, ec_indicator_variable_distance, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Indicator circles will be further away the further the tee is")
MACRO_CONFIG_INT(ClIndicatorMaxDistance, ec_indicator_variable_max_distance, 2500, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum tee distance for variable offset")
MACRO_CONFIG_INT(ClIndicatorRadius, ec_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) indicator circle size")
MACRO_CONFIG_INT(ClIndicatorOpacity, ec_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of indicator circles")
MACRO_CONFIG_INT(ClPlayerIndicator, ec_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show radial indicator of other tees")
MACRO_CONFIG_INT(ClPlayerIndicatorFreeze, ec_player_indicator_freeze, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show frozen tees in indicator")
MACRO_CONFIG_INT(ClIndicatorHideOnScreen, ec_indicator_hide_on_screen, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides the Indicator if Tee is on Screen (might be a lil buggy on bigger/smaller monitors")
MACRO_CONFIG_INT(ClIndicatorOnlyTeammates, ec_indicator_only_teammates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only shows teammates on the indicator")
MACRO_CONFIG_INT(ClIndicatorHideAfk, ec_indicator_hide_afk, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides AFK players on the indicator")

MACRO_CONFIG_INT(ClIndicatorTeamOnly, ec_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(ClIndicatorTees, ec_indicator_tees, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")

// Translate
MACRO_CONFIG_STR(EcTranslateBackend, ec_translate_backend, 32, "google", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate backends (libretranslate, deeplfree/deepl, google)")
MACRO_CONFIG_STR(EcTranslateTarget, ec_translate_target, 16, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate target language (must be 2 character ISO 639 code)")
MACRO_CONFIG_STR(EcTranslateEndpoint, ec_translate_endpoint, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, endpoint to use (must be https)")
MACRO_CONFIG_STR(EcTranslateKey, ec_translate_key, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, api key to use")
MACRO_CONFIG_INT(EcTranslateAuto, ec_translate_auto, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically translate messages, only some backends support this (FTApi does not, DeepL Free requires api key)")
MACRO_CONFIG_STR(EcTranslateLanguageBlacklist, ec_translate_language_blacklist, 256, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "comma seperated languages that get ignored while translating (e.g. 'en,de,ru'")
MACRO_CONFIG_STR(EcTranslateLanguageWhitelist, ec_translate_language_whitelist, 256, "ru,uk,tr,es,pl,mk,lt,bg,zh-CN", CFGFLAG_CLIENT | CFGFLAG_SAVE, "comma seperated languages that get translated, if not in this list, ignored (e.g. 'en,de,ru')")
MACRO_CONFIG_INT(EcTranslateLogErrors, ec_translate_log_errors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Log translation errors")
MACRO_CONFIG_INT(EcTranslateMinInterval, ec_translate_min_interval, -1, -1, 60000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Minimum time in milliseconds between translate requests, -1 uses the default of the backend (Google 1500)")

// Animations
MACRO_CONFIG_INT(ClAnimateWheelTime, ec_animate_wheel_time, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Duration of wheel animations, in milliseconds (0 == no animation, 1000 = 1 second)")

// Bind Wheel
MACRO_CONFIG_INT(ClResetBindWheelMouse, ec_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Profiles
MACRO_CONFIG_INT(ClProfileSkin, ec_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(ClProfileName, ec_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(ClProfileClan, ec_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(ClProfileFlag, ec_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(ClProfileColors, ec_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(ClProfileEmote, ec_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")
MACRO_CONFIG_INT(ClProfileOverwriteClanWithEmpty, ec_profile_overwrite_clan_with_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Overwrite clan name even if profile has an empty clan name")

// Font
MACRO_CONFIG_STR(ClCustomFont, ec_custom_font, 255, "DejaVu Sans", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom font face")

// EClient

// First Client Launch
MACRO_CONFIG_INT(ClFirstLaunch, ec_first_launch, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show welcome message indicating the first launch of the client")

// Misc Color
MACRO_CONFIG_COL(ClAfkColor, ec_afk_color, 10951270, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Afk name color")
MACRO_CONFIG_COL(ClSpecColor, ec_spec_color, 8936607, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spectate Prefix color")

// Friend
MACRO_CONFIG_COL(ClFriendColor, ec_friend_color, 65425, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend name color")
MACRO_CONFIG_INT(ClNameplateFriendColor, ec_nameplate_friend_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use friends name color as text color")
MACRO_CONFIG_INT(ClScoreboardFriendColor, ec_scoreboard_friend_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use friends name color as text color")
MACRO_CONFIG_INT(ClSpecMenuFriendColor, ec_specmenu_friend_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on colors in the spectate menu")
MACRO_CONFIG_INT(ClChatFriendColor, ec_chat_friend_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on colors in the spectate menu")

// chatbubble / Menu
MACRO_CONFIG_INT(ClChatBubble, ec_chatbubble, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Chatbubble on or Off")
MACRO_CONFIG_INT(ClShowOthersInMenu, ec_show_others_in_menu, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows The Settings Emote if Someones in The Menu")
MACRO_CONFIG_INT(ClSendMenuFlag, ec_send_menu_flag, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to send the in settings flag to the server or not")

// misc/Bind Chat
MACRO_CONFIG_INT(ClSendExclamation, ec_send_exclamation_mark, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send chat messages that begin with the prefix \"!\"")
MACRO_CONFIG_INT(ClSendDotsChat, ec_send_dots_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Never send a message if it starts with a '.'")
MACRO_CONFIG_INT(ClShowIdsChat, ec_show_ids_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to Show Client IDs in Chat")

MACRO_CONFIG_INT(ClDoAfkColors, ec_do_afk_colors, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Makes Names Darker in Scoreboard if Player is afk")
MACRO_CONFIG_INT(ClChatServerPrefix, ec_do_chat_server_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Server Prefix")
MACRO_CONFIG_INT(ClChatClientPrefix, ec_do_chat_client_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Client Prefix")
MACRO_CONFIG_INT(ClSpectatePrefix, ec_do_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Spectator Prefix Next to Names")

// Prefixes
MACRO_CONFIG_STR(ClClientPrefix, ec_client_prefix, 5, "— ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client (Echo) Message Prefix")
MACRO_CONFIG_STR(ClServerPrefix, ec_server_prefix, 5, "*** ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server Message Prefix")
MACRO_CONFIG_STR(ClWarlistPrefix, ec_warlist_prefix, 5, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warlist Prefix")
MACRO_CONFIG_STR(ClFriendPrefix, ec_friend_prefix, 5, "♥ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend Prefix (alt + num3 = ♥)")
MACRO_CONFIG_STR(ClSpecPrefix, ec_spec_prefix, 5, "• ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spectating Prefix (alt + num7 = •)")

// Mutes
MACRO_CONFIG_INT(ClReplyMuted, ec_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "replies to muted players if they ping you")
MACRO_CONFIG_INT(ClShowMutedInConsole, ec_show_muted_in_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows messages of muted people in the console")
MACRO_CONFIG_INT(ClHideEnemyChat, ec_hide_enemy_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides Chat Messages of Enemies")
MACRO_CONFIG_STR(ClAutoReplyMutedMsg, ec_auto_reply_muted_msg, 255, "I can't see your messages, you're muted", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply with then a muted player pings you")
MACRO_CONFIG_COL(ClMutedColor, ec_muted_color, 12792139, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Muted Icon Color")

// Saving/Restoring
MACRO_CONFIG_INT(ClSavedCountry, saved_country, -1, -1000, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedName, saved_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedClan, saved_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_INT(ClSavedPlayerUseCustomColor, saved_player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedPlayerColorBody, saved_player_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedPlayerColorFeet, saved_player_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedPlayerSkin, saved_player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")

MACRO_CONFIG_INT(ClSavedDummyCountry, saved_dummy_country, -1, -1000, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummyName, saved_dummy_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummyClan, saved_dummy_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_INT(ClSavedDummyUseCustomColor, saved_dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedDummyColorBody, saved_dummy_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedDummyColorFeet, saved_dummy_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummySkin, saved_dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")

// Discord
MACRO_CONFIG_INT(ClDiscordRPC, ec_discord_rpc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Discord Rpc (requires restart)")
// MACRO_CONFIG_INT(ClDiscordTimestamp, ec_discord_timestamp, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Discord Rpc Time Stamp")
MACRO_CONFIG_INT(ClDiscordMapStatus, ec_discord_map_status, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show What Map you're on")
MACRO_CONFIG_STR(ClDiscordOnlineStatus, ec_discord_online_status, 128, "Online", CFGFLAG_CLIENT | CFGFLAG_SAVE, "discord Online Status")
MACRO_CONFIG_STR(ClDiscordOfflineStatus, ec_discord_offline_status, 128, "Offline", CFGFLAG_CLIENT | CFGFLAG_SAVE, "discord Offline Status")

// Spec Menu Prefixes
MACRO_CONFIG_INT(ClSpecMenuPrefixes, ec_specmenu_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Prefixes Next to Names in The Spectate Menu")

// Chat Detection
MACRO_CONFIG_INT(ClDismissAdBots, ec_dismiss_adbots, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If an AdBot messages you it hides their message")

MACRO_CONFIG_INT(ClNotifyOnJoin, ec_auto_notify_on_join, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notifies you if a player joined")
MACRO_CONFIG_STR(ClAutoNotifyName, ec_auto_notify_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")
MACRO_CONFIG_STR(ClAutoNotifyMsg, ec_auto_notify_msg, 256, "Your Fav Person Has Joined!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Auto Whisper Them")

MACRO_CONFIG_INT(ClAutoJoinTeam, ec_auto_join_team, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Joins that players team if they join one")
MACRO_CONFIG_STR(ClAutoJoinTeamName, ec_auto_join_team_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")

MACRO_CONFIG_INT(ClAutoAddOnNameChange, ec_auto_add_on_name_change, 2, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically adds a player back to a list if their name changed")
MACRO_CONFIG_INT(ClAutoJoinTest, ec_auto_join_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto Join Test")

// Anti Spawn Block
MACRO_CONFIG_INT(ClAntiSpawnBlock, ec_anti_spawn_block, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Puts you in a team when you die and in team 0 when youre about to cross the start line")

MACRO_CONFIG_INT(ClAutoDummyConnect, ec_auto_dummy_connect, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically connect your dummy on join | 1 = switch to dummy on connect | 2 = dont switch to dummy on connect")

// Freeze Kill
MACRO_CONFIG_INT(ClFreezeKill, ec_freeze_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillBeforeStart, ec_freeze_kill_before_start, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether the feature works before having started the race")
//MACRO_CONFIG_INT(ClFreezeKillGrounded, ec_freeze_kill_grounded, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillIgnoreKillProt, ec_freeze_kill_ignore_kill_prot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ignore Kill protection when for auto freeze kill")
MACRO_CONFIG_INT(ClFreezeKillOnlyFullFrozen, ec_freeze_kill_full_frozen, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only Starts Timer When Full Frozen")
MACRO_CONFIG_INT(ClFreezeKillWaitMs, ec_freeze_kill_wait_ms, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wait a Certain amount of time while frozen until you kill")
MACRO_CONFIG_INT(ClFreezeKillMs, ec_freeze_kill_ms, 1000, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "time to wait before you kill")
MACRO_CONFIG_INT(ClFreezeKillTeamInView, ec_freeze_kill_team_in_view, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Kill if a Teammate is in your zoom range ")
MACRO_CONFIG_INT(ClFreezeKillNotMoving, ec_freeze_kill_not_moving, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance a Teammate Can be Away")

MACRO_CONFIG_INT(ClFreezeKillDebug, ec_freeze_kill_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "debug")

// Ui/Sound
MACRO_CONFIG_INT(SndFriendChat, snd_friend_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable regular Chat Sound Only When a Friend Says Something") // Aiodob Menu color plates

// Gores Mode
MACRO_CONFIG_INT(ClGoresMode, ec_gores_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically switches to gun, when firing it switches to hammer and then back")
MACRO_CONFIG_INT(ClGoresModeDisableIfWeapons, ec_gores_mode_disable_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable Gores Mode When The Player Has Another Weapon")
MACRO_CONFIG_INT(ClAutoEnableGoresMode, ec_gores_mode_auto_enable, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "enables it on Gores gametype and disables on others")

// Tee
MACRO_CONFIG_INT(ClOwnTeeSkin, ec_own_tee_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders your own skin differently for yourself")
MACRO_CONFIG_INT(ClOwnTeeSkinCustomColor, ec_own_tee_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders your own skin differently for yourself")
MACRO_CONFIG_STR(ClOwnTeeSkinName, ec_own_tee_skin_name, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Own Tee Skin Name")
MACRO_CONFIG_COL(ClOwnTeeColorBody, ec_own_tee_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player body color")
MACRO_CONFIG_COL(ClOwnTeeColorFeet, ec_own_tee_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player feet color")

// Sweat mode
MACRO_CONFIG_INT(ClSweatMode, ec_sweat_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "sweat mode")
MACRO_CONFIG_INT(ClSweatModeOnlyOthers, ec_sweat_mode_only_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only applies the skin to other players")
MACRO_CONFIG_INT(ClSweatModeSelfColor, ec_sweat_mode_self_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "whether to change your own color when sweatmode is turned on")
MACRO_CONFIG_STR(ClSweatModeSkinName, ec_sweat_mode_skin_name, 24, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Skin Everyone Should Have (leave empty to not change)")

// Visuals
MACRO_CONFIG_INT(ClEffectSpeed, ec_effect_speed, 50, 1, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How Fast The Particles Should Update")
MACRO_CONFIG_INT(ClEffectSpeedOverride, ec_effect_speed_override, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change the Speed to an \"ideal\" one after switching to a different effect")

MACRO_CONFIG_COL(ClEffectColor, ec_effect_color, 14155743, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Effect Color")
MACRO_CONFIG_INT(ClEffectColors, ec_effect_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Do Effect Color")

MACRO_CONFIG_INT(ClEffect, ec_effect, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show effect for own player")
MACRO_CONFIG_INT(ClEffectOthers, ec_effect_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show effects for others")
MACRO_CONFIG_INT(ClSmallSkins, ec_small_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Small tees")

// Serverside Rainbow
MACRO_CONFIG_INT(ClServerRainbow, ec_server_rainbow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enables Rainbow Mode Server Side! (Way Slower though)")

// TClient Rainbow
MACRO_CONFIG_INT(ClRainbowTees, ec_rainbow_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side")
MACRO_CONFIG_INT(ClRainbowHook, ec_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow hook")
MACRO_CONFIG_INT(ClRainbowWeapon, ec_rainbow_weapon, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow Weapons")

MACRO_CONFIG_INT(ClRainbowOthers, ec_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side for others")
MACRO_CONFIG_INT(ClRainbowMode, ec_rainbow_mode, 1, 1, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow mode (1: rainbow, 2: pulse, 3: darkness, 4: random)")
MACRO_CONFIG_INT(ClRainbowSpeed, ec_rainbow_speed, 25, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow speed as a percentage (50 = half speed, 200 = double speed)")

// Flags
MACRO_CONFIG_INT(ClHideSettingsTabs, ec_hide_settings_tabs, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to disable settings tabs")

// Silent Messages
MACRO_CONFIG_INT(ClSilentMessages, ec_silent_messages, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When sending a Silent Message to the chat it will still show what you typed")
MACRO_CONFIG_COL(ClSilentColor, ec_silent_color, 8924078, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color of warlist cmds")

// Nameplate
// MACRO_CONFIG_INT(ClRenderNameplateSpec, ec_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")
MACRO_CONFIG_INT(ClStrongWeakColorId, ec_strong_weak_color_id, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render ClientIds in Nameplate Same Color as Strong/Weak Hook Color")

// Inform Update
MACRO_CONFIG_INT(ClInformUpdate, ec_inform_update, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Inform you if a new version of E-Client is available")
MACRO_CONFIG_INT(EcUnreadNews, ec_unread_news, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether there is unread news")

// Chat
MACRO_CONFIG_INT(ClChatColorParsing, ec_chat_color_parsing, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allows you to use &00-&99 to chang the color of your text, &x marks the end of the color")
MACRO_CONFIG_INT(ClChatMath, ec_chat_math, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Suggest the result of a math expression ending with '=' in the chat, press Tab to insert it")

// Chat Bubbles
MACRO_CONFIG_INT(ClChatBubbles, ec_chat_bubbles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show Chatbubbles above players")

MACRO_CONFIG_INT(ClChatBubbleSize, ec_chat_bubble_size, 20, 15, 30, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the chat bubble")

MACRO_CONFIG_INT(ClChatBubbleShowTime, ec_chat_bubble_showtime, 200, 200, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long to show the bubble for")
MACRO_CONFIG_INT(ClChatBubbleFadeOut, ec_chat_bubble_fadeout, 35, 15, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long it fades out")
MACRO_CONFIG_INT(ClChatBubbleFadeIn, ec_chat_bubble_fadein, 15, 15, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "how long it fades in")

// MACRO_CONFIG_INT(ClChatBubblePushOut, ec_chat_bubble_push_out, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes sure chatbubbles dont overlap")

// Quick Actions
MACRO_CONFIG_INT(ClResetPlayerActionMouse, ec_reset_playeraction_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening the player actions menu")

// Status Bar
MACRO_CONFIG_INT(ClStatusBar, ec_statusbar, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable status bar")

MACRO_CONFIG_INT(ClStatusBar12HourClock, ec_statusbar_12_hour_clock, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use 12 hour clock in local time")
MACRO_CONFIG_INT(ClStatusBarLocalTimeSeconds, ec_statusbar_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show seconds in local time")
MACRO_CONFIG_INT(ClStatusBarHeight, ec_statusbar_height, 8, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Height of the status bar")

MACRO_CONFIG_COL(ClStatusBarColor, ec_statusbar_color, 3221225472, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background color")
MACRO_CONFIG_COL(ClStatusBarTextColor, ec_statusbar_text_color, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text color")
MACRO_CONFIG_INT(ClStatusBarAlpha, ec_statusbar_alpha, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background alpha")
MACRO_CONFIG_INT(ClStatusBarTextAlpha, ec_statusbar_text_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text alpha")

MACRO_CONFIG_INT(ClStatusBarLabels, ec_statusbar_labels, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show labels on status bar entries")
MACRO_CONFIG_STR(ClStatusBarScheme, ec_statusbar_scheme, 16, "ac pf r", CFGFLAG_CLIENT | CFGFLAG_SAVE, "The order in which to show status bar items")

MACRO_CONFIG_INT(ClInfoUrlType, ec_info_url_type, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Url info.cpp should use (0 = Github, 1 = entity)")

// Custom Communities
MACRO_CONFIG_STR(ClCustomCommunitiesUrl, ec_custom_communities_url, 256, "https://raw.githubusercontent.com/TaterClient/ddnet-custom-communities/refs/heads/main/custom-communities-ddnet-info.json", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL to fetch custom communities from (must be https), empty to disable")

// Bg Draw
MACRO_CONFIG_INT(EcBgDrawWidth, ec_bg_draw_width, 5, 1, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of background draw strokes")
MACRO_CONFIG_INT(EcBgDrawFadeTime, ec_bg_draw_fade_time, 0, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time until strokes disappear (0 = never)")
MACRO_CONFIG_INT(EcBgDrawMaxItems, ec_bg_draw_max_items, 128, 0, 2048, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of strokes")
MACRO_CONFIG_COL(EcBgDrawColor, ec_bg_draw_color, 11893936, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of background draw strokes")
MACRO_CONFIG_INT(EcBgDrawAutoSaveLoad, ec_bg_draw_auto_save_load, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically save and load background drawings")

// Timer
MACRO_CONFIG_INT(EcShowLocalTimeSeconds, ec_show_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show local time in seconds")

// Weapon in Freeze
MACRO_CONFIG_INT(ClRenderWeaponsInFreeze, ec_render_weapons_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders players' weapons while frozen")

// Map Overview
MACRO_CONFIG_INT(ClMapOverview, ec_map_overview, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable map overview")
MACRO_CONFIG_INT(ClMapOverviewSpectatingOnly, ec_map_overview_spectating_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable map overview only when spectating")
MACRO_CONFIG_INT(ClMapOverviewOpacity, ec_map_overview_opacity, 15, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the map overview")

// Volleyball
MACRO_CONFIG_INT(EcVolleyBallBetterBall, ec_volleyball_better_ball, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make frozen players in volleyball look more like volleyballs (0 = disabled, 1 = in volleyball maps, 2 = always)")
MACRO_CONFIG_STR(EcVolleyBallBetterBallSkin, ec_volleyball_better_ball_skin, 24, "Volleyball", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin to use for better volleyball ball")
MACRO_CONFIG_INT(EcVolleyBallSpinSpeed, ec_volleyball_spin_speed, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How fast the volleyball spins")

// Trails
MACRO_CONFIG_INT(EcTeeTrail, ec_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Tee trails")
MACRO_CONFIG_INT(EcTeeTrailOthers, ec_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tee trails for other players")
MACRO_CONFIG_INT(EcTeeTrailWidth, ec_tee_trail_width, 15, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail width")
MACRO_CONFIG_INT(EcTeeTrailLength, ec_tee_trail_length, 25, 5, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail length")
MACRO_CONFIG_INT(EcTeeTrailAlpha, ec_tee_trail_alpha, 80, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail alpha")
MACRO_CONFIG_COL(EcTeeTrailColor, ec_tee_trail_color, 16777, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color")
MACRO_CONFIG_INT(EcTeeTrailTaper, ec_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Taper tee trail over length")
MACRO_CONFIG_INT(EcTeeTrailFade, ec_tee_trail_fade, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade trail alpha over length")
MACRO_CONFIG_INT(EcTeeTrailColorMode, ec_tee_trail_color_mode, 1, 1, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color mode (1: Solid color, 2: Current Tee color, 3: Rainbow, 4: Color based on Tee speed, 5: Random)")

// Scoreboard Teams
MACRO_CONFIG_INT(ClScoreboardOutlineTeams, ec_scoreboard_outline_teams, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "outline teams in scoreboard")

// Team Colors
MACRO_CONFIG_INT(ClRevertTeamColors, ec_revert_team_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "revert team colors (github.com/ddnet/ddnet/pull/7129)") // https://github.com/ddnet/ddnet/pull/7129
MACRO_CONFIG_INT(ClRevertDoorDesign, ec_revert_door_design, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "revert door design (github.com/ddnet/ddnet/pull/9901)") // https://github.com/ddnet/ddnet/pull/9901

// Hookline
// MACRO_CONFIG_INT(EcRevertHookLine, ec_revert_hook_line, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Revert hook line to default behavior | 1=Accurate to Tiles | 2=Accurate to Tees")

MACRO_CONFIG_INT(ClColorFrozenTeeBody, ec_color_frozen_tee_body, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use skin colors for frozen tees")
MACRO_CONFIG_INT(ClColorFrozenTeeDarken, ec_color_frozen_tee_darken, 90, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes color of tees darker when in freeze (0-100)")
MACRO_CONFIG_INT(ClColorFrozenTeeFeet, ec_color_frozen_tee_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also use color for frozen tee feet")

MACRO_CONFIG_INT(ClWhiteFeet, ec_white_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render all feet as ec_white_feet_skin and fully white")
MACRO_CONFIG_STR(ClWhiteFeetSkin, ec_white_feet_skin, 24, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base skin for white feet")

// Balls
MACRO_CONFIG_STR(ClPhysicBallsSkin, ec_physic_balls_skin, 24, "volleyball", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base skin for physic balls")

// Moving Tiles
MACRO_CONFIG_INT(ClShowMovingTilesEntities, ec_show_moving_tiles_entities, 1, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show server-side moving tiles in entities | 1 = map design | 2 = use entities color | 3 = map design with entities color")
MACRO_CONFIG_INT(ClPredictMovingTiles, ec_predict_moving_tiles, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predict the moving tiles that have a predictable outcome on FoxNet servers (freeze, unfreeze and stoppers)")

MACRO_CONFIG_INT(ClHighProcessPriority, ec_high_process_priority, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set DDNets process priority to high")
MACRO_CONFIG_INT(ClDiscordNormalProcessPriority, ec_discord_normal_process_priority, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set Discord process priority to normal")

// Media Island
MACRO_CONFIG_INT(ClMediaIsland, ec_media_island, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show media island")
MACRO_CONFIG_COL(ClMediaIslandColor, ec_media_island_color, 2141323264 /* gray */, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of the media island background")
MACRO_CONFIG_INT(ClMediaIslandAnimation, ec_media_island_animation, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How fast the media island animation is (0 = disabled)")
MACRO_CONFIG_INT(ClMediaIslandVisualizer, ec_media_island_visualizer, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show media island visualizer")
MACRO_CONFIG_INT(ClMediaIslandVisualizerAlignment, ec_media_island_visualizer_alignment, 2, 1, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "1 = align bottom | 2 = align center")
MACRO_CONFIG_INT(ClMediaIslandVisualizerColorDynamic, ec_media_island_visualizer_color_dynamic, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable dynamic color for media island visualizer")
MACRO_CONFIG_COL(ClMediaIslandVisualizerColor, ec_media_island_visualizer_color, 10485663 /* blue-ish */, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the media island visualizer")

// Force 0.7 Skin
MACRO_CONFIG_INT(ClForceSevenSkin, ec_force_seven_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to force rendering the 0.7 skin on any server for yourself")

// Decouple mouse sensitivity
MACRO_CONFIG_INT(ClDecoupleMouseSens, ec_decouple_mouse_sens, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to decouple mouse sensitivity")
MACRO_CONFIG_INT(ClMouseSensXIngame, ec_mouse_sens_x_ingame, -1, -1, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse sensitivity for X axis when decoupled")
MACRO_CONFIG_INT(ClMouseSensYIngame, ec_mouse_sens_y_ingame, -1, -1, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse sensitivity for Y axis when decoupled")
MACRO_CONFIG_INT(ClMouseSensXUi, ec_mouse_sens_x_ui, -1, -1, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse sensitivity for X axis when decoupled")
MACRO_CONFIG_INT(ClMouseSensYUi, ec_mouse_sens_y_ui, -1, -1, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse sensitivity for Y axis when decoupled")

// Client Indicator
MACRO_CONFIG_INT(ClClientUsersBrowser, ec_client_users_browser, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show a list of people using EClient in the server browser")
MACRO_CONFIG_INT(ClClientUsersOnlineInfo, ec_client_users_online_info, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "list entity client users in the 'onlineinfo' command")
MACRO_CONFIG_INT(ClClientUsersScoreboard, ec_client_users_scoreboard, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show entity client users in the scoreboard")

MACRO_CONFIG_INT(ClUseUiColorOffline, ec_use_ui_color_offline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use the ui_color while not ingame")

// Statistics
MACRO_CONFIG_INT(ClStatisticsShowFps, ec_statistics_show_fps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show FPS in the statistics")
MACRO_CONFIG_INT(ClStatisticsShowPing, ec_statistics_show_ping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show Ping in the statistics")
MACRO_CONFIG_INT(ClStatisticsShowSnapRate, ec_statistics_show_snap_rate, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show Snap Rate in the statistics")

// Player HUD
// Timer
MACRO_CONFIG_INT(ClShowHudTimerStartedFlag, ec_showhud_timer_started_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show a flag if the timer has started")
MACRO_CONFIG_INT(ClShowhudPlayerCheckpoint, ec_showhud_player_checkpoint, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Player Checkpoint)")
MACRO_CONFIG_INT(ClShowhudPlayerCompact, ec_showhud_player_compact, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player Hud compact version")
MACRO_CONFIG_INT(ClCursorSize, ec_cursor_size, 100, 5, 500, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Cursor size in %")

// Finish Times
MACRO_CONFIG_INT(ClClientSideMapFinishes, ec_client_side_map_finishes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to show map finishes saved on the client in the server browser")

// Finish Times
MACRO_CONFIG_INT(ClSpectatorRadioShowDelay, ec_specradio_show_delay, 200, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long it takes until the spectator radio shows up fully, its usable before it shows up")

// Night Shift
MACRO_CONFIG_INT(ClNightShift, ec_night_shift, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warm the colors of the whole screen after dark")
MACRO_CONFIG_INT(ClNightShiftSchedule, ec_night_shift_schedule, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When night shift is active (0 = always, 1 = custom hours, 2 = sunset to sunrise)")
MACRO_CONFIG_INT(ClNightShiftTemperature, ec_night_shift_temperature, 4000, 1900, 6500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color temperature night shift warms the screen to, in kelvin (lower is warmer)")
MACRO_CONFIG_INT(ClNightShiftFrom, ec_night_shift_from, 1260, 0, 1439, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Minute of the day the custom night shift schedule turns on")
MACRO_CONFIG_INT(ClNightShiftTo, ec_night_shift_to, 420, 0, 1439, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Minute of the day the custom night shift schedule turns off")
MACRO_CONFIG_INT(ClNightShiftTransition, ec_night_shift_transition, 30, 0, 180, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many minutes night shift takes to fade in and out")
MACRO_CONFIG_INT(ClNightShiftLatitude, ec_night_shift_latitude, 0, -9000, 9000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Latitude in hundredths of a degree, used by the sunset to sunrise schedule")
MACRO_CONFIG_INT(ClNightShiftLongitude, ec_night_shift_longitude, 0, -18000, 18000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Longitude in hundredths of a degree, used by the sunset to sunrise schedule")

// Custom variables from my server for the editor
// You can use these if you get my server from https://github.com/FoxNet-DDNet/FoxNet

// Quads
MACRO_CONFIG_INT(SvMovingTiles, sv_moving_tiles, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to use moving tiles using quads or not")
MACRO_CONFIG_INT(SvTeleGrenade, sv_tele_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Whether to use tele grenade")

// Local Practice World
MACRO_CONFIG_INT(ClLocalPracticeGhost, ec_local_practice_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show your real tee on the server as a see-through ghost while practicing")
MACRO_CONFIG_INT(ClLocalPracticeGhostAlpha, ec_local_practice_ghost_alpha, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of your real tee while practicing")
MACRO_CONFIG_INT(ClLocalPracticeGhostFade, ec_local_practice_ghost_fade, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade the ghost out while it overlaps your practice tee")
MACRO_CONFIG_INT(ClLocalPracticeFreezeAim, ec_local_practice_freeze_aim, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep the aim your real tee had when practice started instead of sending your live aim")
MACRO_CONFIG_INT(ClLocalPracticeAlert, ec_local_practice_alert, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a notice when your real tee gets moved while practicing")
MACRO_CONFIG_INT(ClLocalPracticeAlertTime, ec_local_practice_alert_time, 5, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many seconds the moved notice stays on screen")
MACRO_CONFIG_COL(ClLocalPracticeAlertColor, ec_local_practice_alert_color, 1376160, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of the moved notice")
MACRO_CONFIG_INT(ClLocalPracticeExitOnMove, ec_local_practice_exit_on_move, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Leave the practice world as soon as your real tee gets moved")
MACRO_CONFIG_INT(ClLocalPracticeKillToReal, ec_local_practice_kill_to_real, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Killing in the practice world puts you back at your real tee instead of at a spawn tile")
