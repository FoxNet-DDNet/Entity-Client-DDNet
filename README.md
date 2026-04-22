
![gui_logo](https://github.com/user-attachments/assets/76f93b08-3efa-40a8-96b9-b64b17c14b3f)

### Newest Features:
- Scrollable Chat
- Select and copy text directly from chat
- Change ddnets process priority to high and lower discords process priority (fixed lag caused by discord when in a call/streaming)
- Moving tiles in entities

### To other Devs
Any of my sloppy code can be stolen, I don't mind, It's open source for a reason.\
If you do steal my code, you don't have to but I would appreciate if I get mentioned somewhere c: (Changelog, header files, etc.)


### Scripting

Entity Client supports the [ChaiScript](https://chaiscript.com/) language for simple tasks

Add scripts to your config dir then run them with `chai [scriptname] [args]`

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
| `int` | `client_info("skin_custom_color", int Id)` | Returns the custom color of a player given their client ID. 
| `int` | `client_info("skin_color_feet", int Id)` | Returns the feet color of a player given their client ID. |
| `int` | `client_info("skin_color_body", int Id)` | Returns the body color of a player given their client ID. |
| `bool` | `client_info("afk", int Id)` | Returns the skin body color of a player given their client ID. |
| `bool` | `client_info("friend", int Id)` | Returns whether ID is a friend. |
| `bool` | `client_info("foe", int Id)` | Returns whether ID is a foe. |
| `int` | `client_info("warlist_type", int Id)` | Returns the warlist type if they have an entry. |
| `string` | `client_info("warlist_type_name", int Id)` | Returns the warlist type name if they have an entry. |
| `bool` | `client_info("muted", int Id)` | Returns whether the ID is muted |
| `int` | `client_info("auth_level", int Id)` | Returns IDs auth level |

```js
var what = include("thatscript.chai") // you can include other scripts, they use absolute paths from config dir
print(what) // prints "top level return"
if (!file_exists("file")) { // check if a file exists, also absolute from config dir
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
<img width="1668" height="1719" alt="Settings" src="https://github.com/user-attachments/assets/56762cfe-c360-4fd0-853a-919c9cf19214" />

#
### Visuals
<img width="1668" height="1702" alt="Visuals" src="https://github.com/user-attachments/assets/cb677e97-71fc-4f78-87a0-64e4f991f1ff" />

#
### Warlist
<img width="1668" height="987" alt="Warlist" src="https://github.com/user-attachments/assets/e512dedf-7b94-4e11-9de1-86353bbb6c5f" />

#
### Status bar
<img width="1668" height="987" alt="Status bar" src="https://github.com/user-attachments/assets/f39b763b-4391-4e85-89b7-1abb5a6fbe6f" />

#
### Bindwheel
<img width="1668" height="987" alt="Bindwheel" src="https://github.com/user-attachments/assets/22346c8a-e8a0-4e2e-bb8a-b32c41970219" />

#
### Quickactions
<img width="1668" height="987" alt="Quick actions" src="https://github.com/user-attachments/assets/b9796271-3535-4960-a306-5325033d467d" />

#
### Info
<img width="1667" height="987" alt="Info" src="https://github.com/user-attachments/assets/279e9cf8-4574-4ca5-974b-733ed8466e4d" />

</details>

<details>

<summary>Command List</summary>

```
votekick "<Name> <Reason>"

onlineinfo
saveskin
playerinfo "<Name>"

addtempwar "<Name>"
deltempwar "<Name>"
addtemphelper "<Name>"
deltemphelper "<Name>"
addtempmute "<Name>"
deltempmute "<Name>"

saveskin
restoreskin

view_link <url>

server_rainbow_speed "<speed>"
server_rainbow_both_players "<0 | 1>"
server_rainbow_sat <Saturation> <0 | 1 (Dummy)>
server_rainbow_lht <Lightness> <0 | 1 (Dummy)>

server_rainbow_body <0 | 1> <0 | 1 (Dummy)>
server_rainbow_feet <0 | 1> <0 | 1 (Dummy)>

reply_last <?Message>
specid <id>
```
</details>

<details>

<summary>Config List</summary>

```
ec_auto_reply_msg
ec_tabbed_out_msg
ec_notify_on_move
ec_message_color
ec_muted_console_color

ec_freeze_update_fix
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
ec_fast_input_others

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
ec_warlist_spec_menu
ec_warlist_allow_duplicates
ec_warlist_indicator
ec_warlist_indicator_colors
ec_warlist_indicator_all
ec_warlist_indicator_enemy
ec_warlist_indicator_team
ec_warlist_swap_name_reason

ec_warlist_browser
ec_warlist_browser_flags

ec_run_on_join_console
ec_run_on_join_delay
ec_run_on_join_console_msg

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

ec_cursor_in_spec

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
ec_indicator_inteam
ec_indicator_tees

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

ec_spec_menu_friend_color
ec_specmenu_prefixes

ec_dismiss_adbots
ec_auto_notify_on_join
ec_auto_notify_name
ec_auto_notify_msg
ec_auto_join_team
ec_auto_join_team_name
ec_auto_add_on_name_change

ec_list_warlist_info
ec_enabled_info

ec_anti_spawn_block
ec_freeze_kill
ec_freeze_kill_grounded
ec_freeze_kill_ignore_kill_prot
ec_freeze_kill_mult_only
ec_freeze_kill_full_frozen
ec_freeze_kill_wait_ms
ec_freeze_kill_ms
ec_freeze_kill_team_close
ec_freeze_kill_team_dist
ec_freeze_dont_kill_moving

snd_friend_chat
ec_scrollmenu_color

ec_gores_mode
ec_gores_mode_key
ec_gores_mode_disable_weapons
ec_gores_mode_auto_enable
ec_gores_mode_disable_shutdown
ec_gores_mode_saved

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

ec_revert_team_colors

ec_eclient_settings_tabs

ec_silent_messages
ec_silent_color

ec_strong_weak_color_id

ec_inform_update

ec_chat_color_parsing

ec_chat_bubbles
ec_chat_bubble_size
ec_chat_bubble_showtime
ec_chat_bubble_fadeout
ec_chat_bubble_fadein

ec_reset_quickaction_mouse

ec_statusbar
ec_statusbar_12_hour_clock
tc_statusbar_local_time_seconds
ec_statusbar_height
ec_statusbar_color
ec_statusbar_text_color
ec_statusbar_alpha
ec_statusbar_text_alpha
ec_statusbar_labels
ec_statusbar_scheme
```
</details>

