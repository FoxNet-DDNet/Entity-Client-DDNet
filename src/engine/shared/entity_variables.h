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

MACRO_CONFIG_INT(ClChangeTileNotification, ec_change_tile_notification, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when leaving current tile type")

MACRO_CONFIG_COL(ClECMessageColor, ec_message_color, 9633471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color of warlist cmds (usually)")
MACRO_CONFIG_INT(ClMutedConsoleColor, ec_muted_console_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "colors in console for warlist")

// Anti Latency Tools
MACRO_CONFIG_INT(ClFreezeUpdateFix, ec_freeze_update_fix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Will change your skin faster when you enter freeze. ")
MACRO_CONFIG_INT(ClRemoveAnti, ec_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Removes some amount of antiping & player prediction in freeze")
MACRO_CONFIG_INT(ClUnfreezeLagTicks, ec_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The biggest amount of prediction ticks that are removed")
MACRO_CONFIG_INT(ClUnfreezeLagDelayTicks, ec_remove_anti_delay_ticks, 25, 0, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks it takes to remove the maximum prediction after being frozen")

MACRO_CONFIG_INT(ClUnpredOthersInFreeze, ec_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont predict other players if you are frozen")
MACRO_CONFIG_INT(ClPredMarginInFreeze, ec_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "enable changing prediction margin while frozen")
MACRO_CONFIG_INT(ClPredMarginInFreezeAmount, ec_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set what your prediction margin while frozen should be")

MACRO_CONFIG_INT(ClShowOthersGhosts, ec_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghosts for other players in their unpredicted position")
MACRO_CONFIG_INT(ClSwapGhosts, ec_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show predicted players as ghost and normal players as unpredicted")
MACRO_CONFIG_INT(ClHideFrozenGhosts, ec_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide Ghosts of other players if they are frozen")

MACRO_CONFIG_INT(ClPredGhostsAlpha, ec_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of predicted ghosts (0-100)")
MACRO_CONFIG_INT(ClUnpredGhostsAlpha, ec_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of unpredicted ghosts (0-100)")
MACRO_CONFIG_INT(ClRenderGhostAsCircle, ec_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render Ghosts as circles instead of tee")

// Outline Variables
MACRO_CONFIG_INT(ClOutline, ec_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outlines")
MACRO_CONFIG_INT(ClOutlineEntities, ec_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show outlines in entities")
MACRO_CONFIG_INT(ClOutlineFreeze, ec_outline_freeze, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around freeze and deep")
MACRO_CONFIG_INT(ClOutlineKill, ec_outline_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around kill")
MACRO_CONFIG_INT(ClOutlineUnFreeze, ec_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around unfreeze and undeep")
MACRO_CONFIG_INT(ClOutlineTele, ec_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around teleporters")
MACRO_CONFIG_INT(ClOutlineSolid, ec_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around hook and unhook")
MACRO_CONFIG_INT(ClOutlineWidth, ec_outline_width, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of freeze outline")
MACRO_CONFIG_INT(ClOutlineAlpha, ec_outline_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(0-100) Outline alpha")
MACRO_CONFIG_INT(ClOutlineAlphaSolid, ec_outline_alpha_solid, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(0-100) Outline solids alpha")
MACRO_CONFIG_COL(ClOutlineColorSolid, ec_outline_color_solid, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Solid outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorFreeze, ec_outline_color_freeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorTele, ec_outline_color_tele, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tele outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorUnfreeze, ec_outline_color_unfreeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Unfreeze outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorKill, ec_outline_color_kill, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Unfreeze outline color") // 0 0 0

MACRO_CONFIG_INT(ClFastInput, ec_fast_input, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction up to 20ms faster")
MACRO_CONFIG_INT(ClFastInputOthers, ec_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Do an extra 1 tick (20ms) for other tees with your fast inputs. (increases visual latency, makes dragging easier)")

MACRO_CONFIG_INT(ClAntiPingImproved, ec_antiping_improved, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Different antiping smoothing algorithm, not compatible with cl_antiping_smooth")
MACRO_CONFIG_INT(ClAntiPingNegativeBuffer, ec_antiping_negative_buffer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helps in Gores. Allows internal certainty value to be negative which causes more conservative prediction")
MACRO_CONFIG_INT(ClAntiPingStableDirection, ec_antiping_stable_direction, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predicts optimistically along the tees stable axis to reduce delay in gaining overall stability")
MACRO_CONFIG_INT(ClAntiPingUncertaintyScale, ec_antiping_uncertainty_scale, 150, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Determines uncertainty duration as a factor of ping, 100 = 1.0")

// used in fast input code
MACRO_CONFIG_INT(ClSmoothPredictionMargin, ec_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes prediction margin transition smooth, causes worse ping jitter adjustment (reverts a ddnet change)")

// War List
MACRO_CONFIG_INT(ClWarList, ec_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles war list visuals")
MACRO_CONFIG_INT(ClWarListShowClan, ec_warlist_show_clan_if_war, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in nameplate if there is a war")
MACRO_CONFIG_INT(ClWarListReason, ec_warlist_reason, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason")
MACRO_CONFIG_INT(ClWarListChat, ec_warlist_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in chat")
MACRO_CONFIG_INT(ClWarlistPrefixes, ec_warlist_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Warlist Prefix")
MACRO_CONFIG_INT(ClWarListScoreboard, ec_warlist_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in scoreboard")
MACRO_CONFIG_INT(ClWarListSpecMenu, ec_warlist_spec_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in spectate menu")
MACRO_CONFIG_INT(ClWarListAllowDuplicates, ec_warlist_allow_duplicates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow duplicate war entries")

MACRO_CONFIG_INT(ClWarListIndicator, ec_warlist_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use warlist for indicator")
MACRO_CONFIG_INT(ClWarListIndicatorColors, ec_warlist_indicator_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show warlist colors instead of freeze colors")
MACRO_CONFIG_INT(ClWarListIndicatorAll, ec_warlist_indicator_all, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show all groups")
MACRO_CONFIG_INT(ClWarListIndicatorEnemy, ec_warlist_indicator_enemy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from the first group")
MACRO_CONFIG_INT(ClWarListIndicatorTeam, ec_warlist_indicator_team, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from second group")

MACRO_CONFIG_INT(ClWarListSwapNameReason, ec_warlist_swap_name_reason, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If a Player has a Reason, their Name will be switched with the Reason")

// Run on Join
MACRO_CONFIG_INT(ClRunOnJoinConsole, ec_run_on_join_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use run on join")
MACRO_CONFIG_INT(ClRunOnJoinDelay, ec_run_on_join_delay, 2, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick Delay before using run on join")
MACRO_CONFIG_STR(ClRunOnJoinMsg, ec_run_on_join_console_msg, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What it should run inside of the Console")

MACRO_CONFIG_INT(ClLimitMouseToScreen, ec_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundries")
MACRO_CONFIG_INT(ClScaleMouseDistance, ec_scale_mouse_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Improve mouse precision by scaling max distance to 1000")

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
MACRO_CONFIG_INT(ClRenderCursorSpec, ec_cursor_in_spec, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(ClRenderCursorSpecOpacity, ec_cursor_opacity_in_spec, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Opacity of Curser when Spectating")

// Pingcircle next to name
MACRO_CONFIG_INT(ClPingNameCircle, ec_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle next to nameplate to indicate ping")

// Indicator Variables
MACRO_CONFIG_COL(ClIndicatorAlive, ec_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorFreeze, ec_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorSaved, ec_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
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

MACRO_CONFIG_INT(ClIndicatorTeamOnly, ec_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(ClIndicatorTees, ec_indicator_tees, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")

// Bind Wheel
MACRO_CONFIG_INT(ClResetBindWheelMouse, ec_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Skin Profiles
MACRO_CONFIG_INT(ClApplyProfileSkin, ec_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(ClApplyProfileName, ec_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(ClApplyProfileClan, ec_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(ClApplyProfileFlag, ec_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(ClApplyProfileColors, ec_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(ClApplyProfileEmote, ec_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")

// Font
MACRO_CONFIG_STR(ClCustomFont, ec_custom_font, 255, "DejaVu Sans", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom font face")

// E-Client

// First Client Launch
MACRO_CONFIG_INT(ClFirstLaunch, ec_first_launch, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show welcome message indicating the first launch of the client")

// Misc Color
MACRO_CONFIG_COL(ClAfkColor, ec_afk_color, 10951270, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Afk name color")
MACRO_CONFIG_COL(ClSpecColor, ec_spec_color, 8936607, CFGFLAG_CLIENT | CFGFLAG_SAVE, "• color in scoreboard")

// Friend
MACRO_CONFIG_COL(ClFriendColor, ec_friend_color, 14745554, CFGFLAG_CLIENT | CFGFLAG_SAVE, "friend name color")

// chatbubble / Menu
MACRO_CONFIG_INT(ClChatBubble, ec_chatbubble, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Chatbubble on or Off")
MACRO_CONFIG_INT(ClShowOthersInMenu, ec_show_others_in_menu , 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows The Settings Emote if Someones in The Menu")
MACRO_CONFIG_INT(ClSendMenuFlag, ec_send_menu_flag, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to send the in settings flag to the server or not")

// misc
MACRO_CONFIG_INT(ClSendDotsChat, ec_send_dots_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Never send a message if it starts with a '.'")
MACRO_CONFIG_INT(ClShowIdsChat, ec_show_ids_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to Show Client IDs in Chat")

MACRO_CONFIG_INT(ClDoAfkColors, ec_do_afk_colors, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Makes Names Darker in Scoreboard if Player is afk")

// friend name settings
MACRO_CONFIG_INT(ClDoFriendColors, ec_do_friend_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors")

// Prefix toggles
MACRO_CONFIG_INT(ClSpectatePrefix, ec_chat_spectate_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Spectator Prefix Next to Names")

MACRO_CONFIG_INT(ClChatServerPrefix, ec_do_chat_server_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Server Prefix")
MACRO_CONFIG_INT(ClChatClientPrefix, ec_do_chat_client_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Client Prefix")

// Prefixes
MACRO_CONFIG_STR(ClClientPrefix, ec_client_prefix, 8, "— ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client (Echo) Message Prefix")
MACRO_CONFIG_STR(ClServerPrefix, ec_server_prefix, 8, "*** ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server Message Prefix")
MACRO_CONFIG_STR(ClWarlistPrefix, ec_warlist_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Warlist Prefix")
MACRO_CONFIG_STR(ClFriendPrefix, ec_friend_prefix, 8, "♥ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Friend Prefix (alt + num3 = ♥)")
MACRO_CONFIG_STR(ClSpecPrefix, ec_spec_prefix, 8, "• ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spectating Prefix (alt + num7 = ♥)")

// Mutes
MACRO_CONFIG_INT(ClReplyMuted, ec_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "replies to muted players if they ping you")
MACRO_CONFIG_INT(ClShowMutedInConsole, ec_show_muted_in_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows messages of muted people in the console")
MACRO_CONFIG_INT(ClMutedIcon, ec_muted_icon, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows an Icon Next to Nameplates of Muted Players")
MACRO_CONFIG_INT(ClHideEnemyChat, ec_hide_enemy_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides Chat Messages of Enemies")
MACRO_CONFIG_STR(ClAutoReplyMutedMsg, ec_auto_reply_muted_msg, 255, "I can't see your messages, you're muted", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply with then a muted player pings you")
MACRO_CONFIG_COL(ClMutedColor, ec_muted_color, 12792139, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Muted Icon Color")

// Saving/Restoring
MACRO_CONFIG_INT(ClSavedCountry, saved_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedName, saved_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedClan, saved_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_INT(ClSavedPlayerUseCustomColor, saved_player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedPlayerColorBody, saved_player_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedPlayerColorFeet, saved_player_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedPlayerSkin, saved_player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")

MACRO_CONFIG_INT(ClSavedDummyCountry, saved_dummy_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummyName, saved_dummy_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummyClan, saved_dummy_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_INT(ClSavedDummyUseCustomColor, saved_dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedDummyColorBody, saved_dummy_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_COL(ClSavedDummyColorFeet, saved_dummy_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")
MACRO_CONFIG_STR(ClSavedDummySkin, saved_dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !restore")

// Discord
MACRO_CONFIG_INT(ClDiscordRPC, ec_discord_rpc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Discord Rpc (requires restart)")
//MACRO_CONFIG_INT(ClDiscordTimestamp, ec_discord_timestamp, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Discord Rpc Time Stamp")
MACRO_CONFIG_INT(ClDiscordMapStatus, ec_discord_map_status, 1,0,1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show What Map you're on")
MACRO_CONFIG_STR(ClDiscordOnlineStatus, ec_discord_online_status, 25, "Online", CFGFLAG_CLIENT | CFGFLAG_SAVE, "discord Online Status")
MACRO_CONFIG_STR(ClDiscordOfflineStatus, ec_discord_offline_status, 25, "Offline", CFGFLAG_CLIENT | CFGFLAG_SAVE, "discord Offline Status")

// Spec Menu Prefixes
MACRO_CONFIG_INT(ClSpecMenuFriendColor, ec_spec_menu_friend_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on colors in the spectate menu")
MACRO_CONFIG_INT(ClSpecMenuPrefixes, ec_specmenu_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Prefixes Next to Names in The Spectate Menu")

// Chat Detection
MACRO_CONFIG_INT(ClDismissAdBots, ec_dismiss_adbots, 1, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If an AdBot messages you it | 1 = hides their message | 2 = Info and Vote Kick Option | 3 = Info and Auto Vote Kick")

MACRO_CONFIG_INT(ClNotifyOnJoin, ec_auto_notify_on_join, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notifies you if a player joined")
MACRO_CONFIG_STR(ClAutoNotifyName, ec_auto_notify_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")
MACRO_CONFIG_STR(ClAutoNotifyMsg, ec_auto_notify_msg, 256, "Your Fav Person Has Joined!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Auto Whisper Them")

MACRO_CONFIG_INT(ClAutoJoinTeam, ec_auto_join_team, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Joins that players team if they join one")
MACRO_CONFIG_STR(ClAutoJoinTeamName, ec_auto_join_team_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")

MACRO_CONFIG_INT(ClAutoAddOnNameChange, ec_auto_add_on_name_change, 2, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically adds a player back to a list if their name changed")
MACRO_CONFIG_INT(ClAutoJoinTest, ec_auto_join_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto Join Test")

// Info
MACRO_CONFIG_INT(ClListsInfo, ec_lists_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Everytime You Join a Server You Will be Informed How many wars, teams, helpers and mutes are online on the current server")
MACRO_CONFIG_INT(ClEnabledInfo, ec_enabled_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Everytime You Join a Server You Will be Informed About Enabled Features")

// Anti Spawn Block
MACRO_CONFIG_INT(ClAntiSpawnBlock, ec_anti_spawn_block, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Puts you in a Team before enter Play Ground on Multeasymap")

// Freeze Kill
MACRO_CONFIG_INT(ClFreezeKill, ec_freeze_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillGrounded, ec_freeze_kill_grounded, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillIgnoreKillProt, ec_freeze_kill_ignore_kill_prot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillMultOnly, ec_freeze_kill_mult_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only Freeze Kills if Current Map is Multeasymap")
MACRO_CONFIG_INT(ClFreezeKillOnlyFullFrozen, ec_freeze_kill_full_frozen, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only Starts Timer When Full Frozen")
MACRO_CONFIG_INT(ClFreezeKillWaitMs, ec_freeze_kill_wait_ms, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wait a Certain amount of time while frozen until you kill")
MACRO_CONFIG_INT(ClFreezeKillMs, ec_freeze_kill_ms, 1000, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "time to wait before you kill")
MACRO_CONFIG_INT(ClFreezeKillTeamClose, ec_freeze_kill_team_close, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Kill if a Teammate is Close enough")
MACRO_CONFIG_INT(ClFreezeKillTeamDistance, ec_freeze_kill_team_dist, 12, 0, 25, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance a Teammate Can be Away")
MACRO_CONFIG_INT(ClFreezeDontKillMoving, ec_freeze_dont_kill_moving, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance a Teammate Can be Away")

MACRO_CONFIG_INT(ClFreezeKillDebug, ec_freeze_kill_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "debug")

// Ui/Sound

MACRO_CONFIG_INT(SndFriendChat, snd_friend_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable regular Chat Sound Only When a Friend Says Something")// Aiodob Menu color plates
MACRO_CONFIG_COL(ClScrollMenuColor, ec_scrollmenu_color, 654311494, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color in Entity Menu")
MACRO_CONFIG_INT(ClCornerRoundness, ec_corner_roundness, 70, 0, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How round corners are in scrollable menus")
MACRO_CONFIG_INT(ClFpsSpoofer, ec_fps_spoofer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spoof Da Fps counter")
MACRO_CONFIG_INT(ClFpsSpoofPercentage, ec_fps_spoofer_percentage, 100, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fps Spoofer Percentage")

// Gores Mode
MACRO_CONFIG_INT(ClGoresMode, ec_gores_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "always have a gun in your hand :p")
MACRO_CONFIG_STR(ClGoresModeKey, ec_gores_mode_key, 255, "mouse1", CFGFLAG_CLIENT | CFGFLAG_SAVE, "The key it binds to")
MACRO_CONFIG_INT(ClGoresModeDisableIfWeapons, ec_gores_mode_disable_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable Gores Mode When The Player Has Another Weapon")
MACRO_CONFIG_INT(ClAutoEnableGoresMode, ec_auto_enable_gores_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "enables it on Gores gametype and disables on others")
MACRO_CONFIG_INT(ClDisableGoresOnShutdown, ec_disable_gores_on_shutdown, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "disables it on shutdown")
MACRO_CONFIG_STR(ClGoresModeSaved, ec_gores_mode_saved, 128, "+fire", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Edit This")

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

MACRO_CONFIG_COL(ClEffectColor, ec_effect_color, 14155743, CFGFLAG_CLIENT | CFGFLAG_SAVE, "effect Color")
MACRO_CONFIG_INT(ClEffectColors, ec_effect_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "effect Color")

MACRO_CONFIG_INT(ClEffect, ec_effect, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on  Effect Client Side")
MACRO_CONFIG_INT(ClEffectOthers, ec_effect_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on Effect Client Side")
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

// Nameplates Chat Messages
//MACRO_CONFIG_INT(ClNameplateChatBox, ec_nameplate_chatbox, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a Chat Box with the message above the writers Tee")
//MACRO_CONFIG_INT(ClNameplateChatBoxFriends, ec_nameplate_chatbox_friends, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows The Chat Box for friends only")
//MACRO_CONFIG_INT(ClNameplateChatBoxSize, ec_nameplate_chatbox_size, 40, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a Chat Box with the message above the writers Tee")

// Flags
MACRO_CONFIG_INT(ClEClientSettingsTabs, ec_eclient_settings_tabs, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to disable settings tabs")

// Kill Counter
MACRO_CONFIG_INT(ClKillCounter, ec_self_murder_count, 0, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "I'f you wish to fake the ammount of deaths then this is the config to edit (it's named that way so its harder to find)")

// Bind Chat
MACRO_CONFIG_INT(ClSendExclamation, ec_send_exclamation_mark, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send chat messages that begin with the prefix \"!\"")

// Silent Messages
MACRO_CONFIG_INT(ClSilentMessages, ec_silent_messages, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When sending a Silent Message to the chat it will still show what you typed")
MACRO_CONFIG_COL(ClSilentColor, ec_silent_color, 8924078, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color of warlist cmds")

// Nameplate
MACRO_CONFIG_INT(ClRenderNameplateSpec, ec_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")
MACRO_CONFIG_INT(ClStrongWeakColorId, ec_strong_weak_color_id, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render ClientIds in Nameplate Same Color as Strong/Weak Hook Color")

// Inform Update
MACRO_CONFIG_INT(ClInformUpdate, ec_inform_update, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Inform you if a new version of E-Client is available")

// Chat 
MACRO_CONFIG_INT(ClChatColorParsing, ec_chat_color_parsing, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allows you to use &00-&99 to chang the color of your text, &x marks the end of the color")
MACRO_CONFIG_INT(ClAutoWhisper, ec_auto_whisper, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically adds \"/c \" to your message if the last one was a whisper")

// Client Detection
MACRO_CONFIG_INT(ClDetectOthers, ec_detect_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tries to detect other players using E-Client (can have false positives)")

// Custom Vairiables from My Server for the editor
// You can use these if you get my server from https://github.com/FoxNetw/FoxNet-DDNet

MACRO_CONFIG_INT(SvAutoHammer, fs_auto_hammer, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoGun, fs_auto_gun, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoGrenade, fs_auto_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoLaser, fs_auto_laser, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoShotgun, fs_auto_shotgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoExplGun, fs_auto_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")
MACRO_CONFIG_INT(SvAutoFreezeGun, fs_auto_freezegun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "auto enables guns")

MACRO_CONFIG_INT(SvExplGun, fs_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "enables the \"better version\" of the explosion gun")

MACRO_CONFIG_INT(SvFakeGrenade, fs_fake_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Grenade doesnt explode")
MACRO_CONFIG_INT(SvDisableFreeze, fs_disable_freeze, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "fake freeze?")

MACRO_CONFIG_INT(SvFinishXPAmount, fs_finish_xp_amount, 0, 0, 5000, CFGFLAG_SERVER | CFGFLAG_GAME, "How much xp a players should get for a map finish (0 = disabled)")
