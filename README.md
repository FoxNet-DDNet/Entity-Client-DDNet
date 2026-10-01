![gui_logo](https://github.com/user-attachments/assets/76f93b08-3efa-40a8-96b9-b64b17c14b3f)

### Feature highlights
- Scrollable Chat
- Select and copy text directly from chat
- Optional process priority controls for DDNet and Discord
- Moving tiles in entities, with prediction on compatible FoxNet servers
- Local practice world with all important practice commands
- Customizable HUD layout, media island, night shift

### To other Devs
Any of my sloppy code can be stolen, I don't mind, It's open source for a reason.\
If you do steal my code, you don't have to but I would appreciate if I get mentioned somewhere c: (Changelog, header files, etc.)


### Scripting

Entity Client supports the [ChaiScript](https://chaiscript.com/) language for simple tasks

Add scripts to your config directory, then run them from the client console with `chai <scriptname> [args]`.

> [!CAUTION]
> There are no runtime restrictions, you can easily `while (true) {}` yourself or run out of memory, be careful!

<details>

<summary>chai script capabilities</summary>

```js
var a // Declare a variable
a = 1 // Set it
var b = 2 // Do both at once
var c = "strings"
var d = ["lists", 2] // not strongly typed
// var e, f = d // no list deconstruction
print(d[0] + to_string(d[1])) // explicit to_string required for string concat
var bass = "ba" + "s" + "s"
var ass = bass.substr(1, -1) // both indices required, use -1 for end
if (a == b) { // brackets required
	print("this will never happen") // output
} else if (c == "strings") { // string comparison
	exec("echo hello world") // run console stuff
}
var current_game_mode = state("game_mode") // Get the current game mode, all states you can get are listed below
def myfunc(a, b, c) { // yeah it uses def for function definition idk
	print(a, b, c)
	if (a == b) { return "early" }
	c // last statement returns like in rust
}
print(myfunc(1, 2, 3)) // prints "early"
for (var i = 0; i < 10; i = i+1) { // for loops (c style)
	print(i) // auto converts to string, will throw if it cant
}
return "top level return"
```

Here is a list of states which are available:

| Return type | Call | Description |
| --- | -- | --- |
| `string` | `to_lower(<string>)` | Converts the input string to lowercase. |
| `string` | `to_upper(<string>)` | Converts the input string to uppercase. |
| `int` | `state("client_id")` | Returns the current client ID. |
| `int` | `state("dummy_id")` | Returns the dummy client ID if it's connected, else bogus data. |
| `string` | `state("game_mode")` | Returns the current game mode name (e.g., 'DM', 'TDM', 'CTF'). |
| `bool` | `state("game_mode_pvp")` | Whether the current mode is PvP. |
| `bool` | `state("game_mode_race")` | Whether the current mode is a race mode. |
| `bool` | `state("eye_wheel_allowed")` | Whether the 'eye wheel' feature is allowed on this server. |
| `bool` | `state("zoom_allowed")` | Whether camera zoom is allowed. |
| `bool` | `state("dummy_allowed")` | Whether using a dummy client is allowed. |
| `bool` | `state("dummy_connected")` | Whether the dummy client is currently connected. |
| `bool` | `state("local_practice")` | Whether the local practice world is active. |
| `bool` | `state("rcon_authed")` | Whether the client is authenticated with RCON (admin access). |
| `int` | `state("team")` | The player's current team number. |
| `int` | `state("ddnet_team")` | The player's DDNet team number. |
| `string` | `state("map")` | The name of the current or connecting map. |
| `string` | `state("server_ip")` | The IP address of the connected or connecting server. |
| `int` | `state("players_connected")` | Number of currently connected players. |
| `int` | `state("players_cap")` | Maximum number of players the server supports. |
| `string` | `state("server_name")` | The server's name. |
| `string` | `state("community")` | The server's community identifier. |
| `string` | `state("location")` | The player's approximate map location ('NW', 'C', 'SE', etc.). |
| `string` | `state("state")` | The client's connection state (e.g., 'online', 'offline', 'loading', 'demo'). |
| `bool` | `state("in_freeze")` | Whether the local tee is frozen. |
| `bool` | `state("server_passworded")` | Whether the server requires a password. |
| `int` | `state("id", string Name)` | Finds and returns a client ID by player name (exact or case-insensitive match). |
| `string` | `state("name", int Id)` | Returns the name of a player given their client ID. |
| `string` | `state("clan", int Id)` | Returns the clan name of a player given their client ID. |
| `string` | `state("player_name")` | Returns the value of config player_name. |
| `string` | `state("dummy_name")` | Returns the value of config dummy_name. |
| `bool` | `client_info("exists", int Id)` | Whether the ID exists. |
| `int` | `client_info("team", int Id)` | Team of ID. |
| `int` | `client_info("ddnet_team", int Id)` | DDRace team of ID. |
| `string` | `client_info("name", int Id)` | Returns the name of a player given their client ID. |
| `string` | `client_info("clan", int Id)` | Returns the clan name of a player given their client ID. |
| `string` | `client_info("skin_name", int Id)` | Returns the skin name of a player given their client ID. |
| `bool` | `client_info("skin_custom_color", int Id)` | Whether the player uses custom skin colors. |
| `int` | `client_info("skin_color_feet", int Id)` | Returns the feet color of a player given their client ID. |
| `int` | `client_info("skin_color_body", int Id)` | Returns the body color of a player given their client ID. |
| `bool` | `client_info("afk", int Id)` | Returns whether the player is AFK. |
| `bool` | `client_info("friend", int Id)` | Returns whether ID is a friend. |
| `bool` | `client_info("foe", int Id)` | Returns whether ID is a foe. |
| `int` | `client_info("warlist_type", int Id)` | Returns the warlist type if they have an entry. |
| `string` | `client_info("warlist_type_name", int Id)` | Returns the warlist type name if they have an entry. |
| `bool` | `client_info("muted", int Id)` | Returns whether the ID is muted |
| `int` | `client_info("auth_level", int Id)` | Returns IDs auth level |

```js
var what = include("thatscript.chai") // load another script through the game's storage paths
print(what) // prints "top level return"
if (!file_exists("file")) { // check whether a file exists in the game's storage paths
	throw("why doesn't this file exist")
}
```

There is also `math` and `re` modules

```js
import("math")
math.pi
math.e
math.pow(1, 2)
math.sqrt(3)
math.sin(1)
math.cos(1)
math.tan(1)
math.asin(1)
math.acos(1)
math.atan(1)
math.atan2(1, 1)
math.log(1)
math.log10(1)
math.log2(1)
math.ceil(1)
math.floor(1)
math.round(1)
math.abs(1)
math.clamp(1, 1, 1)
math.min(1, 1)
math.max(1, 1)
math.random(1, 10)
```

```js
import("re")

if(re.test(re.compile(".+?ello.+?"), "hello")) { // re.test(r, string)
	print("hi")
}
re.match(re.compile("\\d"), "h3ll0", false, fun[](str, match, group) { // re.match(r, string, global, callback)
	print("not global: " + to_string(match) + " " + str)
})
re.match(re.compile("\\d"), "h3ll0", true, fun[](str, match, group) {
	print("global: " + to_string(match) + " " + str)
})
re.match(re.compile("(h3)l(l0)"), "h3ll0", false, fun[](str, match, group) {
	print("groups: " + to_string(match) + " " + to_string(group) + " " + str)
})
print(re.replace(re.compile("\\d"), "h3ll0", true, fun[](str, match, group) { // re.replace(r, string, global, callback)
	if (str == "3") {
		return "e"
	} else if (str == "0") {
		return "o"
	}
	return str
}))
```

</details>
<details open>

<summary>Setting Pages</summary>

### Main Settings
<img width="1920" height="4471" alt="image" src="https://github.com/user-attachments/assets/e1ed0522-cf8c-4eba-ad3d-fa8e890c8eec" />

#
### Warlist
<img width="1920" height="1080" alt="menu_2026-07-12_13-44-18" src="https://github.com/user-attachments/assets/4a8138c0-817b-452e-b29e-dcdd96643ce6" />

#
### Status bar
<img width="1920" height="1080" alt="menu_2026-07-12_13-12-30" src="https://github.com/user-attachments/assets/26d4ec2f-2e23-4c78-98e5-c96bde360418" />

#
### Bindwheel
<img width="1920" height="1080" alt="menu_2026-07-12_13-12-32" src="https://github.com/user-attachments/assets/942384fe-32a7-44c1-af8b-e54a5ccf01f9" />

#
### Player actions
<img width="1920" height="1080" alt="menu_2026-07-12_13-12-33" src="https://github.com/user-attachments/assets/46436731-b867-4e71-a496-eaaf2b9b0bf7" />

#
### Info
<img width="1920" height="1080" alt="menu_2026-07-12_13-45-47" src="https://github.com/user-attachments/assets/09b589d4-3472-4ddd-98bd-04e0d5d7f7b9" />


</details>

<details>

<summary>Client command list</summary>

| Command | Purpose |
| --- | --- |
| `calc <expression>` | Evaluate an expression. |
| `votekick <name> [reason]` | Start a kick vote against a player. |
| `onlineinfo` / `playerinfo <name>` | Show player and warlist information. |
| `saveskin` / `restoreskin` | Save or restore your player appearance and identity. |
| `view_link <url>` | Open a URL in the browser. |
| `reply_last [message]` | Reply to the last ping. |
| `specid <id>` | Spectate a client ID. |
| `dump_votes [offset]` | List 20 votes, optionally starting at an offset. |
| `playtime <amount>` / `death_counter <amount>` | Set the local counters. |
| `server_rainbow_speed [speed]`, `server_rainbow_both_players [0|1]` | Control server rainbow speed and whether it affects both players. |
| `server_rainbow_sat [saturation] [dummy]`, `server_rainbow_lht [lightness] [dummy]` | Set server rainbow saturation and lightness. |
| `server_rainbow_body [0|1] [dummy]`, `server_rainbow_feet [0|1] [dummy]` | Toggle server rainbow for body or feet. |
| `war_name <group> <name> [reason]`, `war_clan <group> <clan> [reason]` | Add a warlist name or clan. |
| `remove_war_name <group> <name>`, `remove_war_clan <group> <clan>` | Remove a warlist name or clan. |
| `addmute <temporary: 0|1> <name>` / `delmute <name>` | Add or remove a muted name. |
| `bindchat <name> <command>`, `unbindchat <name> <command>`, `bindchats [name]` | Manage chat command binds; `bindchatdefaults` restores the defaults. |
| `+bindwheel`, `add_bindwheel <name> <command>`, `remove_bindwheel <name> <command>` | Open and edit the bindwheel. |
| `+playeractions`, `add_playeraction <name> <command>`, `remove_playeraction <name> <command>` | Open and edit player actions. |
| `hud_editor`, `hud_list`, `hud_reset [element]` | Edit, inspect, or reset HUD placement. |
| `hud_move <element> <x> <y>`, `hud_scale <element> <scale>`, `hud_anchor <element> <anchor>` | Position and scale HUD elements. |
| `+bg_draw`, `+bg_draw_erase`, `bg_draw_reset`, `bg_draw_save [filename]`, `bg_draw_load [filename]` | Draw on the game background and manage drawings. |
| `translate [name]`, `translate_id <id>` | Translate the last message from a player. |
| `add_map_finish <name> <map> <community>` | Save a map finish entry for the server browser. |
| `physic_ball_new [size]`, `physic_ball_new_cursor [size]`, `physic_balls_reset` | Add or clear local physics balls. |
| `add_mod_action <name> <command>`, `add_mod_client_action <name> <command>` | Add moderation menu actions. |
| `set_input <text>`, `say_queued <message>` | Prefill chat or queue a chat message. |
| `webhook_command <command>` | Send a command to the configured webhook (`ec_webhook_url`). |
| `local_practice`, `local_practice_cmd <command>` | Toggle the local practice world or run one of its commands from the console. |
| `+specpause` | Open the spectator pause radio. |
| `chai <file> [args]` | Run a ChaiScript file. |

While local practice is active, enter `/practicecmdlist` in chat to see its commands. Examples include `/tpcursor`, `/tpxy <x> <y>`, `/rescue`, `/unfreeze`, `/weapons`, and `/exit`. These act in the local practice world.

Default chat binds use `.` or `!` prefixes: for example, `.tempwar <name>`, `.temphelper <name>`, and `.tempmute <name>`. They expand to the registered warlist and mute commands. Run `bindchatdefaults` to add the defaults if they are missing.
</details>

<details>

<summary>Config List</summary>

```
ec_auto_reply_msg
ec_tabbed_out_msg

ec_notify_on_move

ec_message_color
ec_muted_console_color

ec_remove_anti
ec_remove_anti_ticks
ec_remove_anti_delay_ticks

ec_unpred_others_in_freeze
ec_pred_margin_in_freeze
ec_pred_margin_in_freeze_amount

ec_show_others_ghosts
ec_swap_ghosts
ec_hide_frozen_ghosts

ec_pred_ghosts_alpha
ec_unpred_ghosts_alpha
ec_render_ghost_as_circle

ec_outline
ec_outline_in_entities
ec_outline_freeze
ec_outline_unfreeze
ec_outline_solid
ec_outline_tele
ec_outline_kill
ec_outline_width_freeze
ec_outline_width_unfreeze
ec_outline_width_solid
ec_outline_width_tele
ec_outline_width_kill
ec_outline_color_freeze
ec_outline_color_unfreeze
ec_outline_color_solid
ec_outline_color_tele
ec_outline_color_kill

ec_fast_input
ec_fast_input_amount
ec_fast_input_others
ec_fast_input_mode
ec_flux_input_amount

ec_antiping_improved
ec_antiping_negative_buffer
ec_antiping_stable_direction
ec_antiping_uncertainty_scale

ec_prediction_margin_smooth
ec_frozen_katana

ec_warlist
ec_warlist_show_clan_if_war
ec_warlist_reason
ec_warlist_chat
ec_warlist_prefixes
ec_warlist_scoreboard
ec_warlist_specmenu
ec_warlist_allow_duplicates

ec_warlist_indicator
ec_warlist_indicator_colors
ec_warlist_indicator_all
ec_warlist_indicator_enemy
ec_warlist_indicator_team

ec_warlist_swap_name_reason
ec_warlist_color_join_leave

ec_warlist_browser
ec_warlist_browser_flags
ec_warlist_auto_add_flags
ec_warlist_frozen_tee_flags

ec_warlist_prefixes_server_info

ec_execute_on_connect
ec_run_on_join_console
ec_run_on_join_delay

ec_limit_mouse_to_screen
ec_scale_mouse_distance
ec_ui_mouse_border_teleport

ec_frozen_tees_text
ec_frozen_tees_hud
ec_frozen_tees_hud_skins

ec_frozen_tees_size
ec_frozen_tees_max_rows
ec_frozen_tees_only_inteam

ec_last_notify
ec_last_notify_text
ec_last_notify_color

ec_cursor_opacity_spec

ec_nameplate_ping_circle

ec_indicator_alive
ec_indicator_freeze
ec_indicator_dead
ec_indicator_offset
ec_indicator_offset_max
ec_indicator_variable_distance
ec_indicator_variable_max_distance
ec_indicator_radius
ec_indicator_opacity
ec_player_indicator
ec_player_indicator_freeze
ec_indicator_hide_on_screen
ec_indicator_only_teammates
ec_indicator_hide_afk

ec_indicator_inteam
ec_indicator_tees

ec_translate_backend
ec_translate_target
ec_translate_endpoint
ec_translate_key
ec_translate_auto
ec_translate_language_blacklist
ec_translate_language_whitelist
ec_translate_log_errors
ec_translate_min_interval

ec_animate_wheel_time

ec_reset_bindwheel_mouse

ec_profile_skin
ec_profile_name
ec_profile_clan
ec_profile_flag
ec_profile_colors
ec_profile_emote
ec_profile_overwrite_clan_with_empty

ec_custom_font

ec_afk_color
ec_spec_color

ec_friend_color
ec_nameplate_friend_color
ec_scoreboard_friend_color
ec_specmenu_friend_color
ec_chat_friend_color

ec_chatbubble
ec_show_others_in_menu
ec_send_menu_flag

ec_send_exclamation_mark
ec_send_dots_chat
ec_show_ids_chat

ec_do_afk_colors
ec_do_chat_server_prefix
ec_do_chat_client_prefix
ec_do_spec_prefix

ec_client_prefix
ec_server_prefix
ec_warlist_prefix
ec_friend_prefix
ec_spec_prefix

ec_reply_muted
ec_show_muted_in_console
ec_hide_enemy_chat
ec_auto_reply_muted_msg
ec_muted_color

ec_discord_rpc
ec_discord_map_status
ec_discord_online_status
ec_discord_offline_status

ec_specmenu_prefixes

ec_dismiss_adbots

ec_auto_notify_on_join
ec_auto_notify_name
ec_auto_notify_msg

ec_auto_join_team
ec_auto_join_team_name

ec_auto_add_on_name_change
ec_auto_join_test

ec_anti_spawn_block
ec_auto_dummy_connect

ec_freeze_kill
ec_freeze_kill_before_start
ec_freeze_kill_ignore_kill_prot
ec_freeze_kill_full_frozen
ec_freeze_kill_wait_ms
ec_freeze_kill_ms
ec_freeze_kill_team_in_view
ec_freeze_kill_not_moving
ec_freeze_kill_debug

snd_friend_chat

ec_gores_mode
ec_gores_mode_disable_weapons
ec_gores_mode_auto_enable

ec_own_tee_skin
ec_own_tee_custom_color
ec_own_tee_skin_name
ec_own_tee_color_body
ec_own_tee_color_feet

ec_sweat_mode
ec_sweat_mode_only_others
ec_sweat_mode_self_color
ec_sweat_mode_skin_name

ec_effect_speed
ec_effect_speed_override

ec_effect_color
ec_effect_colors

ec_effect
ec_effect_others
ec_small_skins

ec_server_rainbow

ec_rainbow_tees
ec_rainbow_hook
ec_rainbow_weapon

ec_rainbow_others
ec_rainbow_mode
ec_rainbow_speed

ec_hide_settings_tabs

ec_silent_messages
ec_silent_color

ec_strong_weak_color_id

ec_inform_update
ec_unread_news

ec_chat_color_parsing
ec_chat_math

ec_chat_bubbles

ec_chat_bubble_size

ec_chat_bubble_showtime
ec_chat_bubble_fadeout
ec_chat_bubble_fadein

ec_reset_playeraction_mouse

ec_statusbar
ec_statusbar_12_hour_clock
ec_statusbar_local_time_seconds
ec_statusbar_height
ec_statusbar_color
ec_statusbar_text_color
ec_statusbar_alpha
ec_statusbar_text_alpha
ec_statusbar_labels
ec_statusbar_scheme

ec_info_url_type

ec_custom_communities_url

ec_bg_draw_width
ec_bg_draw_fade_time
ec_bg_draw_max_items
ec_bg_draw_color
ec_bg_draw_auto_save_load

ec_show_local_time_seconds

ec_render_weapons_in_freeze

ec_map_overview
ec_map_overview_spectating_only
ec_map_overview_opacity

ec_volleyball_better_ball
ec_volleyball_better_ball_skin
ec_volleyball_spin_speed

ec_tee_trail
ec_tee_trail_others
ec_tee_trail_width
ec_tee_trail_length
ec_tee_trail_alpha
ec_tee_trail_color
ec_tee_trail_taper
ec_tee_trail_fade
ec_tee_trail_color_mode

ec_scoreboard_outline_teams

ec_revert_team_colors
ec_revert_door_design

ec_color_frozen_tee_body
ec_color_frozen_tee_darken
ec_color_frozen_tee_feet

ec_white_feet
ec_white_feet_skin

ec_physic_balls_skin

ec_show_moving_tiles_entities
ec_predict_moving_tiles

ec_high_process_priority
ec_discord_normal_process_priority

ec_media_island
ec_media_island_color
ec_media_island_animation
ec_media_island_visualizer
ec_media_island_visualizer_alignment
ec_media_island_visualizer_color_dynamic
ec_media_island_visualizer_color

ec_force_seven_skin

ec_decouple_mouse_sens
ec_mouse_sens_x_ingame
ec_mouse_sens_y_ingame
ec_mouse_sens_x_ui
ec_mouse_sens_y_ui

ec_client_users_browser
ec_client_users_online_info
ec_client_users_scoreboard

ec_use_ui_color_offline

ec_statistics_show_fps
ec_statistics_show_ping
ec_statistics_show_snap_rate

ec_showhud_timer_started_flag
ec_showhud_player_checkpoint
ec_showhud_player_compact
ec_cursor_size

ec_client_side_map_finishes

ec_specradio_show_delay

ec_night_shift
ec_night_shift_schedule
ec_night_shift_temperature
ec_night_shift_from
ec_night_shift_to
ec_night_shift_transition
ec_night_shift_latitude
ec_night_shift_longitude

ec_webhook_url

ec_local_practice_ghost
ec_local_practice_ghost_alpha
ec_local_practice_ghost_fade
ec_local_practice_freeze_aim
ec_local_practice_alert
ec_local_practice_alert_time
ec_local_practice_alert_color
ec_local_practice_exit_on_move
ec_local_practice_kill_to_real
```
</details>

