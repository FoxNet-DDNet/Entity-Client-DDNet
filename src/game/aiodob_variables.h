// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// chillerbot-ux

MACRO_CONFIG_INT(ClFinishRename, ac_finish_rename, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change name to ac_finish_name if finish is near.")
MACRO_CONFIG_STR(ClFinishName, ac_finish_name, 16, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Change to this name if ac_finish_rename is active.")
MACRO_CONFIG_STR(ClAutoReplyMsg, ac_auto_reply_msg, 255, "I'm currently tabbed out", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when pinged in chat and ac_auto_reply is set to 1")
MACRO_CONFIG_INT(ClTabbedOutMsg, ac_tabbed_out_msg, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let others know when pinged in chat that you are tabbed out")

MACRO_CONFIG_INT(ClChillerbotHud, ac_chillerbot_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show enabled chillerbot components in hud")
MACRO_CONFIG_INT(ClChangeTileNotification, ac_change_tile_notification, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when leaving current tile type")
MACRO_CONFIG_INT(ClAlwaysReconnect, ac_always_reconnect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reconnect to last server after some time no matter what")
MACRO_CONFIG_INT(ClReconnectWhenEmpty, ac_reconnect_when_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reconnect to the current server when the last player leaves")


// warlist
MACRO_CONFIG_INT(ClWarList, ac_enable_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Display nameplate color based on chillerbot/warlist directory")
MACRO_CONFIG_INT(ClNameplatesWarReason, ac_nameplates_war_reason, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason in name plates")
MACRO_CONFIG_INT(ClWarListAutoReload, ac_war_list_auto_reload, 10, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reload warlist every x seconds 0=off")
MACRO_CONFIG_INT(ClSilentChatCommands, ac_silent_chat_commands, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Send a Chatmessage After Correctly Typing in a Chat Command (!help...)")

MACRO_CONFIG_COL(ClWarlistMessageColor, ac_warlist_message_color, 9633471, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client message color of warlist cmds")

MACRO_CONFIG_INT(ClWarlistConsoleColors, ac_warlist_console_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "colors in console for warlist")

MACRO_CONFIG_INT(ClMutedConsoleColor, ac_muted_console_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "colors in console for warlist")

// from tater client (credits to tater)

// Anti Latency Tools
MACRO_CONFIG_INT(ClFreezeUpdateFix, ac_freeze_update_fix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Will change your skin faster when you enter freeze. ")
MACRO_CONFIG_INT(ClRemoveAnti, ac_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Removes some amount of antiping & player prediction in freeze")
MACRO_CONFIG_INT(ClUnfreezeLagTicks, ac_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The biggest amount of prediction ticks that are removed")
MACRO_CONFIG_INT(ClUnfreezeLagDelayTicks, ac_remove_anti_delay_ticks, 25, 0, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks it takes to remove the maximum prediction after being frozen")

MACRO_CONFIG_INT(ClUnpredOthersInFreeze, ac_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont predict other players if you are frozen")
MACRO_CONFIG_INT(ClPredMarginInFreeze, ac_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "enable changing prediction margin while frozen")
MACRO_CONFIG_INT(ClPredMarginInFreezeAmount, ac_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set what your prediction margin while frozen should be")

MACRO_CONFIG_INT(ClShowOthersGhosts, ac_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghosts for other players in their unpredicted position")
MACRO_CONFIG_INT(ClSwapGhosts, ac_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show predicted players as ghost and normal players as unpredicted")
MACRO_CONFIG_INT(ClHideFrozenGhosts, ac_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide Ghosts of other players if they are frozen")

MACRO_CONFIG_INT(ClPredGhostsAlpha, ac_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of predicted ghosts (0-100)")
MACRO_CONFIG_INT(ClUnpredGhostsAlpha, ac_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of unpredicted ghosts (0-100)")
MACRO_CONFIG_INT(ClRenderGhostAsCircle, ac_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render Ghosts as circles instead of tee")

MACRO_CONFIG_INT(ClSendClientInfo, ac_send_client_info, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Send Info to Server That You Are Using Aiodob Client")

// Outline Variables
MACRO_CONFIG_INT(ClOutline, ac_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outlines")
MACRO_CONFIG_INT(ClOutlineEntities, ac_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show outlines in entities")
MACRO_CONFIG_INT(ClOutlineFreeze, ac_outline_freeze, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around freeze and deep")
MACRO_CONFIG_INT(ClOutlineUnFreeze, ac_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around unfreeze and undeep")
MACRO_CONFIG_INT(ClOutlineTele, ac_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around teleporters")
MACRO_CONFIG_INT(ClOutlineSolid, ac_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around hook and unhook")
MACRO_CONFIG_INT(ClOutlineWidth, ac_outline_width, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) Width of freeze outline")
MACRO_CONFIG_INT(ClOutlineAlpha, ac_outline_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(0-100) Outline alpha")
MACRO_CONFIG_INT(ClOutlineAlphaSolid, ac_outline_alpha_solid, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(0-100) Outline solids alpha")
MACRO_CONFIG_COL(ClOutlineColorSolid, ac_outline_color_solid, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Solid outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorFreeze, ac_outline_color_freeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorTele, ac_outline_color_tele, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tele outline color") // 0 0 0
MACRO_CONFIG_COL(ClOutlineColorUnfreeze, ac_outline_color_unfreeze, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Unfreeze outline color") // 0 0 0

// fast input

MACRO_CONFIG_INT(ClFastInput, ac_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction up to 20ms faster")
MACRO_CONFIG_INT(ClFastInputOthers, ac_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Do an extra 1 tick (20ms) for other tees with your fast inputs. (increases visual latency, makes dragging easier)")

// used in fast input code
	MACRO_CONFIG_INT(ClSmoothPredictionMargin, ac_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes prediction margin transition smooth, causes worse ping jitter adjustment (reverts a ddnet change)")
	MACRO_CONFIG_INT(ClOldMouseZoom, ac_old_mouse_zoom, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Does not zoom mouse based on camera to prevent precision loss at low zoom levels")


// Run on Join
MACRO_CONFIG_INT(ClRunOnJoinConsole, ac_run_on_join_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to use run on join in chat or console")
MACRO_CONFIG_INT(ClRunOnJoinDelay, ac_run_on_join_delay, 2, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick Delay before using run on join")
MACRO_CONFIG_STR(ClRunOnJoinMsg, ac_run_on_join_console_msg, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What it should run inside of the Console")

MACRO_CONFIG_INT(ClLimitMouseToScreen, ac_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundries")


// Frozen Tee Display
MACRO_CONFIG_INT(ClShowFrozenText, ac_frozen_tees_text, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show how many tees in your team are currently frozen. (0 - off, 1 - show alive, 2 - show frozen)")
MACRO_CONFIG_INT(ClShowFrozenHud, ac_frozen_tees_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show frozen tee HUD")
MACRO_CONFIG_INT(ClShowFrozenHudSkins, ac_frozen_tees_hud_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use ninja skin, or darkened skin for frozen tees on hud")

MACRO_CONFIG_INT(ClFrozenHudTeeSize, ac_frozen_tees_size, 15, 8, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees in frozen tee hud. (Default : 15)")
MACRO_CONFIG_INT(ClFrozenMaxRows, ac_frozen_tees_max_rows, 1, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of rows in frozen tee HUD display")
MACRO_CONFIG_INT(ClFrozenHudTeamOnly, ac_frozen_tees_only_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only render frozen tee HUD display while in team")

// Notify When Last
MACRO_CONFIG_INT(ClNotifyWhenLast, ac_last_notify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when you are last")
MACRO_CONFIG_STR(ClNotifyWhenLastText, ac_last_notify_text, 64, "Last!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text for last notify")
MACRO_CONFIG_COL(ClNotifyWhenLastColor, ac_last_notify_color, 29057, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color for last notify")



MACRO_CONFIG_INT(ClRenderCursorSpec, ac_cursor_in_spec, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(ClDoCursorSpecOpacity, ac_do_cursor_opacity, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow Cursor Opacity Change While Spectating?")
MACRO_CONFIG_INT(ClRenderCursorSpecOpacity, ac_cursor_opacity_in_spec, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Opacity of Curser when Spectating")

// Nameplate in Spec
MACRO_CONFIG_INT(ClRenderNameplateSpec, ac_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")

// Pingcircle next to name
MACRO_CONFIG_INT(ClPingNameCircle, ac_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle next to nameplate to indicate ping")

// Indicator Variables
MACRO_CONFIG_COL(ClIndicatorAlive, ac_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorFreeze, ac_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(ClIndicatorSaved, ac_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
MACRO_CONFIG_INT(ClIndicatorOffset, ac_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Offset of indicator position")
MACRO_CONFIG_INT(ClIndicatorOffsetMax, ac_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Max indicator offset for variable offset setting")
MACRO_CONFIG_INT(ClIndicatorVariableDistance, ac_indicator_variable_distance, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Indicator circles will be further away the further the tee is")
MACRO_CONFIG_INT(ClIndicatorMaxDistance, ac_indicator_variable_max_distance, 2500, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum tee distance for variable offset")
MACRO_CONFIG_INT(ClIndicatorRadius, ac_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) indicator circle size")
MACRO_CONFIG_INT(ClIndicatorOpacity, ac_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of indicator circles")
MACRO_CONFIG_INT(ClPlayerIndicator, ac_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show radial indicator of other tees")
MACRO_CONFIG_INT(ClPlayerIndicatorFreeze, ac_player_indicator_freeze, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show frozen tees in indicator")
MACRO_CONFIG_INT(ClIndicatorHideOnScreen, ac_indicator_hide_on_screen, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides the Indicator if Tee is on Screen (might be a lil buggy on bigger/smaller monitors")
MACRO_CONFIG_INT(ClIndicatorOnlyTeammates, ac_indicator_only_teammates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only shows teammates on the indicator")

MACRO_CONFIG_INT(ClIndicatorTeamOnly, ac_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(ClIndicatorTees, ac_indicator_tees, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")

// Bind Wheel
MACRO_CONFIG_INT(ClResetBindWheelMouse, ac_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Auto Verify
MACRO_CONFIG_INT(ClAutoVerify, ac_auto_verify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto verify")

// Skin Profiles
MACRO_CONFIG_INT(ClApplyProfileSkin, ac_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(ClApplyProfileName, ac_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(ClApplyProfileClan, ac_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(ClApplyProfileFlag, ac_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(ClApplyProfileColors, ac_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(ClApplyProfileEmote, ac_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")

// AAAAAAA
MACRO_CONFIG_INT(ClAmIFrozen, EEEfrz, 0, 0, 1, CFGFLAG_CLIENT, "")


// Aiodob Variables
MACRO_CONFIG_INT(ClAutoVoteOnKrxMessage, ac_autovote_krx_message, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically votes a person that send a krx message")


// Color Variables

	// Icon Color

	MACRO_CONFIG_COL(ClMutedColor, ac_muted_color, 12792139, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Muted Icon Color")

	// War / Team color (chillerbots Warlist)

	MACRO_CONFIG_COL(ClWarColor, ac_war_color, 16777123, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enemy Name Color")
	MACRO_CONFIG_COL(ClTeamColor, ac_team_color, 5504948, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Teammate Name Color")
	MACRO_CONFIG_COL(ClHelperColor, ac_helper_color, 2686902, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helper Name Color")

	// Misc Color
	MACRO_CONFIG_COL(ClAfkColor, ac_afk_color, 10951270, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Afk name color")
	MACRO_CONFIG_COL(ClSpecColor, ac_spec_color, 8936607, CFGFLAG_CLIENT | CFGFLAG_SAVE, "'(s)' color in scoreboard")

	// Friend / Foe Color
	MACRO_CONFIG_COL(ClFriendAfkColor, ac_friend_afk_color, 14694222, CFGFLAG_CLIENT | CFGFLAG_SAVE, "friend name color")

	MACRO_CONFIG_COL(ClFriendColor, ac_friend_color, 14745554, CFGFLAG_CLIENT | CFGFLAG_SAVE, "friend name color")
	MACRO_CONFIG_COL(ClFoeColor, ac_foe_color, 16777123, CFGFLAG_CLIENT | CFGFLAG_SAVE, "foe color (old does still work)")

	// Aiodob Menu color plates
	MACRO_CONFIG_COL(AiodobColor, Aiodob_color, 654311494, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Box Color in Aiodob Menu") // 160 70 175 228 hasalpha


// chatbubble / Menu

MACRO_CONFIG_INT(ClChatBubble, ac_chatbubble, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Chatbubble on or Off")
MACRO_CONFIG_INT(ClShowOthersInMenu, ac_show_others_in_menu , 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows The Settings Emote if Someones in The Menu")
MACRO_CONFIG_INT(ClShowOwnMenuToOthers, ac_show_self_in_menu, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show The Settings Emot to Others When In The Menu")


// misc




MACRO_CONFIG_INT(ClShowIdsChat, ac_show_ids_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Whether to Show Client IDs in Chat")

MACRO_CONFIG_INT(ClOldNameplateIds, ac_old_nameplate_ids, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render the old version of nameplate ids")

MACRO_CONFIG_INT(ClStrongWeakColorId, ac_strong_weak_color_id, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render ClientIds in Nameplate Same Color as Strong/Weak Hook Color")


MACRO_CONFIG_INT(ClShowAiodobPreview, ac_show_preview, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Chat Preview in Aiodob Settings Menu")

MACRO_CONFIG_INT(ClDoTeammateNameColor, ac_team_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Teammate Nameplate Color")
MACRO_CONFIG_INT(ClDoHelperNameColor, ac_helper_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helper Nameplate Color")
MACRO_CONFIG_INT(ClDoEnemyNameColor, ac_war_name_color, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enemy Nameplate Color")
MACRO_CONFIG_INT(ClDoAfkColors, ac_do_afk_colors, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Makes Names Darker in Scoreboard if Player is afk")

MACRO_CONFIG_INT(ClAutoClanWar, ac_auto_clan_war, 1, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Automatically displays a name in a color if you have a player as an enemy in the same clan")


MACRO_CONFIG_INT(ClScoreSpecPlayer, ac_do_score_spec_tee, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes The Tee in The Scoreboard to a Spectating Tee if The Player is Spectating")


MACRO_CONFIG_INT(ClDoWarListColorScore, ac_do_warlist_color_score, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors")


// friend name settings


MACRO_CONFIG_INT(ClDoFriendNameColor, ac_do_friend_name_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors")
MACRO_CONFIG_INT(ClDoFriendColorScore, ac_do_friend_color_score, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Friend name colors in scoreboard")
MACRO_CONFIG_INT(ClDoFriendColorInchat, ac_do_friend_color_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on friend color in chat")

// unused or shouldnt be used

// foe name color (pretty much useless after I got the warlist to work)

MACRO_CONFIG_INT(ClFoeNameColor, ac_foe_name_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Foe name colors")

MACRO_CONFIG_INT(ClShowSpecials, ac_specials, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Autocrashes the game if someone with cheats is detected")
MACRO_CONFIG_INT(ClFinishRenameDistance, ac_finish_rename_distance, 32, 32, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, " (dont change buggy) distance to where the game detects the finish line")


MACRO_CONFIG_INT(ClScoreSpecPrefix, ac_do_score_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle Spectator Prefix Next to Names in Scoreboard")
MACRO_CONFIG_INT(ClChatSpecPrefix, ac_do_chat_spec_prefix, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Spectator Prefix Next to Names in Chat")
MACRO_CONFIG_INT(ClChatEnemyPrefix, ac_do_chat_war_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Enemy Prefix  Next to Names in Chat")
MACRO_CONFIG_INT(ClChatTeammatePrefix, ac_do_chat_team_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Teammate Prefix Next to names in Chat")
MACRO_CONFIG_INT(ClChatHelperPrefix, ac_do_chat_helper_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Helper Prefix Next to names in Chat")

MACRO_CONFIG_INT(ClChatServerPrefix, ac_do_chat_server_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Server Prefix")
MACRO_CONFIG_INT(ClChatClientPrefix, ac_do_chat_client_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles Client Prefix")



// Prefixes

MACRO_CONFIG_STR(ClClientPrefix, ac_client_prefix, 24, "— ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Client (Echo) Message Prefix")
MACRO_CONFIG_STR(ClServerPrefix, ac_server_prefix, 24, "*** ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Server Message Prefix")
MACRO_CONFIG_STR(ClFriendPrefix, ac_friend_prefix, 8, "♥ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Friends in Chat (alt + num3 = ♥)")
MACRO_CONFIG_STR(ClSpecPrefix, ac_spec_prefix, 8, "(s) ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Specating People in Chat")
MACRO_CONFIG_STR(ClEnemyPrefix, ac_war_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Enemies in Chat (alt + num4 = ♦)")
MACRO_CONFIG_STR(ClTeammatePrefix, ac_team_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Teammates in Chat (alt + num4 = ♦)")
MACRO_CONFIG_STR(ClHelperPrefix, ac_helper_prefix, 8, "♦ ", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Show Next To Helpers in Chat (alt + num4 = ♦)")

// Warlist


MACRO_CONFIG_STR(ClAddHelperString, ac_addhelper_string, 20, "add_helper", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClRemoveHelperString, ac_delhelper_string, 20, "del_helper", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddTempWarString, ac_addtempwar_string, 20, "add_temp_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClRemoveTempWarString, ac_deltempwar_string, 20, "del_temp_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddMuteString, ac_addmute_string, 20, "add_mute", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClRemoveMuteString, ac_delmute_string, 20, "del_mute", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddWarString, ac_addwar_string, 20, "add_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClRemoveWarString, ac_delwar_string, 20, "del_war", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddTeamString, ac_addteam_string, 20, "add_team", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClRemoveTeamString, ac_delteam_string, 20, "del_team", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddClanWarString, ac_addclanwar_string, 20, "add_clanwar", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClDelClanWarString, ac_delclanwar_string, 20, "del_clanwar", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")

MACRO_CONFIG_STR(ClAddClanTeamString, ac_addclanteam_string, 20, "add_clanteam", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")
MACRO_CONFIG_STR(ClDelClanTeamString, ac_delclanteam_string, 20, "del_clanteam", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom War Command String")


MACRO_CONFIG_INT(ClSendDotsChat, ac_send_dots_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Doesnt send a message if it start with a '.'")
MACRO_CONFIG_INT(ClAutoAddOnNameChange, ac_auto_add_on_name_change, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically adds a player back to a list if their name changed")

MACRO_CONFIG_STR(ClAutoReplyMutedMsg, ac_auto_reply_muted_msg, 255, "I can't see your messages, you're muted", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply with then a muted player pings you")
MACRO_CONFIG_INT(ClHideEnemyChat, ac_hide_enemy_chat, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hides Chat Messages  of Enemies")
MACRO_CONFIG_INT(ClShowMutedInConsole, ac_show_muted_in_console, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows messages of muted people in the console")
MACRO_CONFIG_INT(ClReplyMuted, ac_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "replies to muted player")
MACRO_CONFIG_INT(ClMutedIconNameplate, ac_muted_icon_nameplate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows an Icon Next to Nameplates of Muted Players")
MACRO_CONFIG_INT(ClMutedIconScore, ac_muted_icon_Scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows an Icon Next to Nameplates of Muted Players ")

// Cloning People with chatcommands

MACRO_CONFIG_INT(ClCloningPerson, cloning_person, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dont edit this")

MACRO_CONFIG_INT(ClCloningPersonDummy, cloning_person_dummy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dont edit this")

MACRO_CONFIG_INT(ClCopyingSkin, copying_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dont edit this")

MACRO_CONFIG_INT(ClCopyingSkinDummy, copying_skin_dummy, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Dont edit this")

MACRO_CONFIG_INT(ClSavedCountry, saved_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_INT(ClSavedDummyCountry, saved_dummy_country, -1, -1, 1000, CFGFLAG_SAVE | CFGFLAG_CLIENT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_STR(ClSavedName, saved_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_STR(ClSavedClan, saved_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_STR(ClSavedDummyName, saved_dummy_name, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_STR(ClSavedDummyClan, saved_dummy_clan, 24, "Aiodob", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_INT(ClSavedPlayerUseCustomColor, saved_player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_COL(ClSavedPlayerColorBody, saved_player_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_COL(ClSavedPlayerColorFeet, saved_player_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_STR(ClSavedPlayerSkin, saved_player_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_INT(ClSavedDummyUseCustomColor, saved_dummy_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_COL(ClSavedDummyColorBody, saved_dummy_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_COL(ClSavedDummyColorFeet, saved_dummy_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")
MACRO_CONFIG_STR(ClSavedDummySkin, saved_dummy_skin, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Saved to restore for !delclone")

MACRO_CONFIG_INT(ClAutoJoinTest, ac_auto_join_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "test")

MACRO_CONFIG_INT(ClTest, ac_test, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "test")


MACRO_CONFIG_INT(ClAutoKillDebug, ac_auto_kill_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "test")


// In Spec Menu Prefixes

MACRO_CONFIG_INT(ClSpecMenuColors, ac_specmenu_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on colors in the spectate menu")
MACRO_CONFIG_INT(ClSpecMenuFriendPrefix, ac_specmenu_friend_prefix, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Friend Prefix Next to Names in The Spectate Menu")
MACRO_CONFIG_INT(ClSpecMenuPrefixes, ac_specmenu_prefixes, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turns on Prefixes Next to Names in The Spectate Menu")

// Notify on join

MACRO_CONFIG_INT(ClNotifyOnJoin, ac_auto_notify_on_join, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notifies you if a player joined")
MACRO_CONFIG_STR(ClAutoNotifyName, ac_auto_notify_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")
MACRO_CONFIG_STR(ClAutoNotifyMsg, ac_auto_notify_msg, 256, "Your Fav Person Has Joined!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What to Auto Whisper Them")
MACRO_CONFIG_INT(ClAutoNotifySound, ac_auto_notify_sound, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Plays a Sound When The Player Joins")

MACRO_CONFIG_INT(ClAutoJoinTeam, ac_auto_join_team, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Joins that players team if they join one")
MACRO_CONFIG_STR(ClAutoJoinTeamName, ac_auto_join_team_name, 16, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Name of Player to Whisper Something When They Join")

// Info

MACRO_CONFIG_INT(ClListsInfo, ac_lists_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Everytime You Join a Server You Will be Informed How many wars, teams, helpers and mutes are online on the current server")
MACRO_CONFIG_INT(ClEnabledInfo, ac_enabled_info, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Everytime You Join a Server You Will be Informed About Enabled Features")

// Freeze Kill


MACRO_CONFIG_INT(ClFreezeKill, ac_freeze_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillGrounded, ac_freeze_kill_grounded, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillIgnoreKillProt, ac_freeze_kill_ignore_kill_prot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
MACRO_CONFIG_INT(ClFreezeKillMultOnly, ac_freeze_kill_mult_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only Freeze Kills if Current Map is Multeasymap")
MACRO_CONFIG_INT(ClFreezeKillOnlyFullFrozen, ac_freeze_kill_full_frozen, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only Starts Timer When Full Frozen")
MACRO_CONFIG_INT(ClFreezeKillWaitMs, ac_freeze_kill_wait_ms, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Wait a Certain amount of time while frozen until you kill")
MACRO_CONFIG_INT(ClFreezeKillMs, ac_freeze_kill_ms, 1000, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "time to wait before you kill")
MACRO_CONFIG_INT(ClFreezeKillTeamClose, ac_freeze_kill_team_close, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Kill if a Teammate is Close enough")
MACRO_CONFIG_INT(ClFreezeKillTeamDistance, ac_freeze_kill_team_dist, 12, 0, 25, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance a Teammate Can be Away")
MACRO_CONFIG_INT(ClFreezeDontKillMoving, ac_freeze_dont_kill_moving, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Distance a Teammate Can be Away")

MACRO_CONFIG_INT(ClFreezeKillDebug, ac_freeze_kill_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "debug")

// Ui
MACRO_CONFIG_INT(ClCornerRoundness, ac_corner_roundness, 50, 0, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How round corners are in scrollable menus")
MACRO_CONFIG_INT(ClFpsSpoofer, ac_fps_spoofer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spoof Da Fps counter")
MACRO_CONFIG_INT(ClFpsSpoofPercentage, ac_fps_spoofer_percentage, 100, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fps Spoofer Percentage")
MACRO_CONFIG_INT(ClAidsPingDetection, ac_aids_ping_detection, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "shows ping range 76-77 in black (ping of a weirdo)")

// Might be Cheating

	// KoG Mode
	MACRO_CONFIG_INT(ClGoresMode, ac_gores_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "always have a gun in your hand :p")
	MACRO_CONFIG_INT(ClGoresModeDisableIfWeapons, ac_gores_mode_disable_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable Gores Mode When The Player Has Another Weapon")
	MACRO_CONFIG_INT(ClAutoEnableGoresMode, ac_auto_enable_gores_mode, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "enables it on Gores gametype and disables on others")
	MACRO_CONFIG_INT(ClDisableGoresOnShutdown, ac_disable_gores_on_shutdown, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "disables it on shutdown")
	MACRO_CONFIG_STR(ClGoresModeSaved, ac_gores_mode_saved, 128, "+fire", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont Edit This")

	// auto kill if player is sitting ontop of you
	MACRO_CONFIG_INT(ClAutoKill, ac_autokill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto kills if an enemy player is sitting on you while you are freezed")
	MACRO_CONFIG_INT(ClAutoKillIgnoreKillProt, ac_autokill_ignore_kill_prot, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Kill if Frozen")
	MACRO_CONFIG_INT(ClAutoKillMultOnly, ac_autokill_mult_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto kills if an enemy player is sitting on you while you are freezed")
	MACRO_CONFIG_INT(ClAutoKillWarOnly, ac_autokill_war_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto kills if an enemy player is sitting on you while you are freezed")
	MACRO_CONFIG_INT(ClAutoKillRangeX, ac_autokill_range_x, 35, 1, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "x coordinate range to autokill in ↔")
	MACRO_CONFIG_INT(ClAutoKillRangeY, ac_autokill_range_y, 10, 1, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "y coordinate range to autokill in ↕")

	//Dummy Controls +

	MACRO_CONFIG_INT(ClDummyHammerSpeed, ac_dummy_hammer_speed, 25, 0, 100, CFGFLAG_CLIENT, "Speed at which the dummy hammers")
	MACRO_CONFIG_INT(ClIdToHammer, ac_id_to_hammer, -1, -1, MAX_CLIENTS, CFGFLAG_CLIENT, "Speed at which the dummy hammers")

	// Tee

MACRO_CONFIG_INT(ClOwnTeeSkin, cle_own_tee_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders your own skin differently for yourself")
MACRO_CONFIG_INT(ClOwnTeeSkinCustomColor, cle_own_tee_custom_color, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders your own skin differently for yourself")
MACRO_CONFIG_STR(ClOwnTeeSkinName, cle_own_tee_skin_name, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Own Tee Skin Name")
MACRO_CONFIG_COL(ClOwnTeeColorBody, cle_own_tee_color_body, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player body color")
MACRO_CONFIG_COL(ClOwnTeeColorFeet, cle_own_tee_color_feet, 65408, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLLIGHT | CFGFLAG_INSENSITIVE, "Player feet color")


// Sweat mode

MACRO_CONFIG_INT(ClSweatMode, ac_sweat_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "sweat mode")
MACRO_CONFIG_INT(ClSweatModeSkin, ac_sweat_mode_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "sweat mode")
MACRO_CONFIG_INT(ClSweatModeOnlyOthers, ac_sweat_mode_only_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "sweat mode")
MACRO_CONFIG_STR(ClSweatModeSkinName, ac_sweat_mode_skin_name, 24, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Skin Everyone Should Have")


// Effects
MACRO_CONFIG_COL(ClSparkleColor, cle_sparkle_color, 14155743, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Sparkle effect Color")


MACRO_CONFIG_INT(ClSparkleEffect, cle_sparkle_effect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on Sparkle Effect Client Side")
MACRO_CONFIG_INT(ClSparkleEffectOthers, cle_sparkle_effect_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on Sparkle Effect Client Side")

MACRO_CONFIG_INT(ClSpecialEffect, cle_special_effect, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Doesnt do anything")

MACRO_CONFIG_INT(ClFreezeParticleSpin, cle_freeze_particle_spin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make Freeze Particles Spin Around The Player")


// 



MACRO_CONFIG_INT(ClRainbowSpeed, ac_rainbow_speed, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow hook client side")

MACRO_CONFIG_INT(ClRainbowHookOthers, ac_rainbow_hook_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow hook for others client side")

MACRO_CONFIG_INT(ClRainbowHook, ac_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow hook client side")

MACRO_CONFIG_INT(ClRainbowOthers, ac_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side for others")

MACRO_CONFIG_INT(ClRainbow, ac_rainbow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow mode (1: off, 2: rainbow, 3: pulse, 4: darkness)")

MACRO_CONFIG_INT(ClSmallSkins, cle_small_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Small tees")


// Not in Settings Menu

MACRO_CONFIG_INT(ClTeeSize, cle_tee_size, 0, -5640, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees")
MACRO_CONFIG_INT(ClTeeWalkRuntime, cle_tee_walkruntime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "weird animations?? walk/run -time")
MACRO_CONFIG_INT(ClTeeFeetInAir, cle_tee_feet_inair, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How far the feet are apart in the air")
MACRO_CONFIG_INT(ClTeeSitting, cle_tee_sitting, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What the tee looks like while sitting (spectating, afk)")
MACRO_CONFIG_INT(ClTeeFeetWalking, cle_tee_feet_walking, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Feet while walking")

// Hook

MACRO_CONFIG_INT(ClHookSizeX, cle_hook_size_y, 0, -2, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hook size x")
MACRO_CONFIG_INT(ClHookSizeY, cle_hook_size_x, 0, -2, 4360, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hook size y")

// Weapons

MACRO_CONFIG_INT(ClShowWeapons, cle_show_weapons, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Size")

MACRO_CONFIG_INT(ClWeaponSize, cle_weapon_size, 0, -2, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Size")
MACRO_CONFIG_INT(ClWeaponRot, cle_weapon_rot, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weapon Rotation?")
MACRO_CONFIG_INT(ClWeaponRotSize2, cle_weapon_rot_size2, 0, -1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Some size? i think when looking left")
MACRO_CONFIG_INT(ClWeaponRotSize3, cle_weapon_rot_size3, 0, -1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Some size? i think when looking right")

// Hammer

MACRO_CONFIG_INT(ClHammerDir, cle_hammer_dir, 0, -4800, 5200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hammer Direction")
MACRO_CONFIG_INT(ClHammerRot, cle_hammer_rot, 0, -4800, 5200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hammer Rotation")

// Gun

MACRO_CONFIG_INT(ClGunPosSitting, cle_gun_pos_sitting, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Position while sitting")
MACRO_CONFIG_INT(ClGunPos, cle_gun_pos, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Position")
MACRO_CONFIG_INT(ClGunReload, cle_gun_reload, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Gun Reload animation")
MACRO_CONFIG_INT(ClGunRecoil, cle_gun_recoil, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "how far the gun recoil animation goes")
	// Bullet
	MACRO_CONFIG_INT(ClBulletLifetime, cle_bullet_lifetime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bullet trail Lifetime")
	MACRO_CONFIG_INT(ClBulletSize, cle_bullet_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bullet trail Size")

// Grenade

MACRO_CONFIG_INT(ClGrenadeLifetime, cle_grenade_lifetime, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Grenade Lifetime")
MACRO_CONFIG_INT(ClGrenadeSize, cle_grenade_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Grenade  Size")

// Freeze Bar

MACRO_CONFIG_INT(ClFreezeBarWidth, cle_freezebar_width, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Width")
MACRO_CONFIG_INT(ClFreezeBarHeight, cle_freezebar_height, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Height")

MACRO_CONFIG_INT(ClFreezeBarX, cle_freezebar_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar X")
MACRO_CONFIG_INT(ClFreezeBarY, cle_freezebar_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Freeze Bar Y")

// unused

MACRO_CONFIG_INT(ClSnowflakeRandomSize, cle_snowflake_random_size, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflakes random size")
MACRO_CONFIG_INT(ClSnowflakeSize, cle_snowflake_size, 0, 5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflake Size")

// Double Jump

MACRO_CONFIG_INT(ClDjSprite, cle_dj_sprite, 0, -8, 130, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Sprites")

MACRO_CONFIG_INT(ClDjPosX, cle_dj_pos_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where Dj Spawns on The x Axis <->")
MACRO_CONFIG_INT(ClDjPosY, cle_dj_pos_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "where Dj Spawns on The y Axis")
MACRO_CONFIG_INT(ClDjLifespan, cle_dj_lifespan, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Lifespan")
MACRO_CONFIG_INT(ClDjSize, cle_dj_size, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Size")
MACRO_CONFIG_INT(ClDjGravity, cle_dj_gravity, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Gravity")
MACRO_CONFIG_INT(ClDjRotSpeed, cle_dj_rot_speed, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dj Sprite Gravity")

// snowflake

MACRO_CONFIG_INT(ClSnowflakeLifeSpan, cle_snowflake_lifespan, 0, -2, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Lifespan")
MACRO_CONFIG_INT(ClSnowflakeGravity, cle_snowflake_gravity, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Changes Snowflake Gravity")
MACRO_CONFIG_INT(ClSnowflakeCollision, cle_snowflake_collision, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Let Snowflakes Collide With Floor?")

MACRO_CONFIG_INT(ClSnowflakeX, cle_snowflake_x, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Where Snowflakes Spawn on The x Axis <->")
MACRO_CONFIG_INT(ClSnowflakeY, cle_snowflake_y, 0, -5000, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "where Snowflakes Spawn on The y Axis")


// auto reload sprites

MACRO_CONFIG_INT(ClAutoReloadSprite, cle_sprite_auto_reload, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "If Sprites Auto Reload")

// Weapon Sprites
MACRO_CONFIG_INT(ClGunSprite, cle_gun_sprite, 0, -26, 112, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Gun Uses")
MACRO_CONFIG_INT(ClHammerSprite, cle_hammer_sprite, 0, -41, 97, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Hammer Uses")
MACRO_CONFIG_INT(ClGrenadeSprite, cle_grenade_sprite, 0, -38, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Grenade Uses")
MACRO_CONFIG_INT(ClShotgunSprite, cle_shotgun_sprite, 0, -32, 106, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Shotgun Uses")
MACRO_CONFIG_INT(ClLaserSprite, cle_laser_sprite, 0, -47, 91, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Laser Uses")
MACRO_CONFIG_INT(ClNinjaSprite, cle_ninja_sprite, 0, -44, 94, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Ninja Uses")

// Cursor Sprites
MACRO_CONFIG_INT(ClGunCursorSprite, cle_gun_cursor_sprite, 0, -27, 111, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Gun Cursor Uses")
MACRO_CONFIG_INT(ClHammerCursorSprite, cle_hammer_cursor_sprite, 0, -42, 96, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Hammer Cursor Uses")
MACRO_CONFIG_INT(ClGrenadeCursorSprite, cle_grenade_cursor_sprite, 0, -39, 99, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Grenade Cursor Uses")
MACRO_CONFIG_INT(ClShotgunCursorSprite, cle_shotgun_cursor_sprite, 0, -33, 105, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Shotgun Uses")
MACRO_CONFIG_INT(ClLaserCursorSprite, cle_laser_cursor_sprite, 0, -48, 90, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Laser Cursor Uses")
MACRO_CONFIG_INT(ClNinjaCursorSprite, cle_ninja_cursor_sprite, 0, -45, 93, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite Ninja Cursor Uses")

// gun fire sprite
MACRO_CONFIG_INT(ClSpriteGunFire, cle_sprite_gun_fire, 0, -29, 106, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")
MACRO_CONFIG_INT(ClNoSpriteGunFire, cle_sprite_no_gun_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn Off The Gun Fire Sprite")

// hook sprites
MACRO_CONFIG_INT(ClHookChainSprite, cle_sprite_hook_chain, 0, -50, 88, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")
MACRO_CONFIG_INT(ClHookHeadSprite, cle_sprite_hook_head, 0, -51, 87, CFGFLAG_CLIENT | CFGFLAG_SAVE, "What Sprite The Gun Fire Uses")

MACRO_CONFIG_INT(SndFriendChat, snd_friend_chat, 0, 0, 1, CFGFLAG_SAVE | CFGFLAG_CLIENT, "Enable regular Chat Sound Only When a Friend Says Something")


// Custom Vairiables from My Server for the editor
// ignore these you cant do anything with them except if you make ur own version on a server


MACRO_CONFIG_INT(SvAutoHammer, sv_auto_hammer, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoGun, sv_auto_gun, 1, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoGrenade, sv_auto_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoLaser, sv_auto_laser, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoShotgun, sv_auto_shotgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoExplGun, sv_auto_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")
MACRO_CONFIG_INT(SvAutoFreezeGun, sv_auto_Freezegun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Test | auto enables guns")

MACRO_CONFIG_INT(SvExplGun, sv_explgun, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "enables the ''better version'' of the explosion gun")

MACRO_CONFIG_INT(SvFakeGrenade, sv_fake_grenade, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "Grenade doesnt explode")
MACRO_CONFIG_INT(SvDisableFreeze, sv_disable_freeze, 0, 0, 1, CFGFLAG_SERVER | CFGFLAG_GAME, "fake freeze?")

MACRO_CONFIG_INT(ClDisableFreeze, cl_disable_freeze, 0, 0, 1, CFGFLAG_CLIENT, "fake freeze?")
MACRO_CONFIG_INT(ClFakeGrenade, cl_fake_grenade, 0, 0, 1, CFGFLAG_CLIENT, "Grenade doesnt explode")



// Testing Variables For Menus

MACRO_CONFIG_INT(XPos1, x_pos1, 0, -5000, 50000, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(XPos2, x_pos2, 0, -5000, 50000, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(XPos3, x_pos3, 0, -5000, 50000, CFGFLAG_CLIENT, "")

MACRO_CONFIG_INT(YPos1, y_pos1, 0, -5000, 50000, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(YPos2, y_pos2, 0, -5000, 50000, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(YPos3, y_pos3, 0, -5000, 50000, CFGFLAG_CLIENT, "")

MACRO_CONFIG_STR(Text1, text1, 100, "Lorem ipsum", CFGFLAG_CLIENT, "")
MACRO_CONFIG_STR(Text2, text2, 100, "Lorem ipsum", CFGFLAG_CLIENT, "")
MACRO_CONFIG_STR(Text3, text3, 100, "Lorem ipsum", CFGFLAG_CLIENT, "")