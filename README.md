Foxs turn on copying files from other clients and making a *semi* All In One client, which will be called... Aiodob.

If anyones got a better name make an Issue please. I want something sick, nothing like.. idk.. THE Client or sum

## How to use the Client

### Steam
go to manage > browse local files and find the ddnet folder, then replace it with mine, if you dont want to replace the whole folder you can only add the .exe and in the data directory you have to add my aiodob folder, it conains images that are important
And as a side note you might wanna turn off auto updates
### Windows
download the Game.zip and extract it to somewhere. Create a *Shortcut* and place *the shortcut* on your desktop and you're done!

### Windows protected your PC?

I mean the source is literally right here.

press on More Info and run anyway :p

### Different OS?
If on a different operating system you have to build the Client yourself.
steps on how to do that are on the official [ddnet github](https://github.com/ddnet/ddnet/)

## Why This Change?

### Images.

I've looked into it for about 10 minutes and already figured that I wont be able to make this happen with just an exe. Sorry but not sorry :p 

If anyones willing to help me with this, please feel free to

### further explanation: 

Basically the new images need to be loaded somehow and I cant just embed them into the .exe, cause I'm also too stupid for that
I thought about autoupadtes and it downloading it from my servers but I have no clue how that would work.
If anyone can help me with this Issue then please feel free to, I wont pay or anything, this Client will be finished soon, so there wont be as many updates anymore





## ---Client will include---

### -Parts of Tater Client-
- Bind Wheel
- Skin Profiles
- AutoVerify
- Player Indicator
- Ping Circle next to names
- Nameplates in spec ig?
- Render Cursor in Spec - with an opacity config
- Run on Join in Console

### -Parts of Chillerbot-
- War List - its almost like the normal chillerbot except that the commands are also in the console
- silent chat commands if turned on or when using "." as the prefix
- ~~Skin Stealer~~ broke it and lazy
- Camp - might be bannable I have no clue tbh
- Finish Rename
- Auto reply - When alt tabbed

### -Aiodob Client-
- disable chat bubble
- customizeable in chat Prefixes (default: war = "♦ " , team = "♦ " , Friend = "♥ " , spec = "(s) "
- Friend name color in chat and scorebaord
- (s) next to names of people spectating in scoreboard and chat
- show tee sitting in scoreboard if the player is spectating
- darker names of people who are afk in scoreboard
- Show when others are in a menu
- Friend/Foe/Team/War/Helper nameplate colors
- strong/weak hook color for cl_nameplate_ids
- In Spectate Menu Prefixes and Name Colors 
- Custom Chat Commands - add your own command that the warlist recognizes (eg. !addteeeem qxdFox)
- Fps spoofer ig?
- Very pro looking menu trust
- Prediction support for my servers explosion gun
- Customization of alot of sprites - but like in the most useless way possible.
- automatically adds people back to lists if they changed their name


## Screenshots

![Usefull Settings](https://github.com/user-attachments/assets/831914d2-2305-4ab9-9aed-03c99bc664dc)
![Bindwheel](https://github.com/user-attachments/assets/ca857ca5-439a-414e-a5c2-935bc52f5845)
![Visual Sound Settings](https://github.com/user-attachments/assets/d98a00bb-badf-4f03-811b-d669657492e3)



# Command List:
```
ac_auto_add_on_name_change
ac_auto_notify_on_join
ac_chatbubble
ac_client_msg_prefix
ac_server_msg_prefix
ac_friend_prefix
ac_spec_prefix
ac_team_prefix
ac_war_prefix
ac_addmute_string
ac_addwar_string
ac_addteam_string
ac_dont_send_dots_chat
ac_delmute_string
ac_delwar_string
ac_delteam_string
ac_muted_icon_nameplate
ac_muted_icon_Scoreboard
ac_specmenu_colors
ac_specmenu_friend_prefix
ac_specmenu_prefixes
ac_do_afk_colors
ac_do_scoreboard_specmark
ac_do_scoreboard_spec_tee
ac_do_chat_server_prefix
ac_do_chat_client_prefix
ac_do_chat_spec_prefix
ac_do_chat_team_prefix
ac_do_chat_helper_prefix
ac_do_chat_war_prefix
ac_do_friend_name_color
ac_do_friend_color_scoreboard
ac_do_friend_color_chat
ac_do_score_spec_tee
ac_do_score_spec_prefix
ac_do_warlist_color_score
ac_fps_spoofer
ac_fps_spoofer_margin
ac_send_dots_chat
ac_show_others_in_menu
ac_show_ids_chat
ac_strong_weak_color_id
ac_team_name_color
ac_war_name_color

cb_finish_rename
cb_finish_name
cb_camp_hack
cb_auto_reply
cb_auto_reply_msg
cb_tabbed_out_msg
cb_chillerbot_hud
cb_change_tile_notification
cb_enable_warlist
cb_war_list_advanced
cb_nameplates_war_reason
cb_war_list_auto_relaod
cb_silent_chat_commands

tc_run_on_join_console
tc_run_on_join_console_msg
tc_run_on_join_delay
tc_cursor_in_spec
tc_do_cursor_opacity
tc_cursor_opacity_in_spec
tc_nameplate_ping_circle
tc_indicator_alive
tc_indicator_freeze
tc_indicator_dead
tc_indicatot_offset
tc_indicator_offset_max
tc_indicator_variable_distance
tc_indicator_variable_max_distance
tc_indicator_radius
tc_indicator_opacity
tc_player_indicator
tc_player_indicator_freeze
tc_indicator_inteam
tc_indicator_tees
tc_reset_bindwheel_mouse
tc_rainbow_others
tc_rainbow_mode
tc_auto_verify
tc_profile_skin
tc_profile_name
tc_profile_clan
tc_profile_flag
tc_profile_colors
tc_profile_emote

cle_small_skins
cle_tee_size
cle_tee_walkruntime
cle_tee_feet_inair
cle_tee_sitting
cle_tee_feet_walking
cle_hook_size_x
cle_hook_size_y
cle_show_weapons
cle_weapon_size
cle_weapon_rot
cle_weapon_rot_size2
cle_weapon_rot_size3
cle_hammer_dir
cle_hammer_rot
cle_gun_pos_sitting
cle_gun_pos
cle_gun_reload
cle_gun_recoil
cle_bullet_lifetime
cle_bullet_size
cle_grenade_lifetime
cle_grenade_size
cle_freezebar_width
cle_freezebar_height
cle_freezebar_x
cle_freezebar_y
cle_dj_sprite
cle_dj_pos_x
cle_dj_pos_y
cle_dj_lifespan
cle_dj_size
cle_dj_gravity
cle_dj_rot_speed
cle_snowflake_lifespan
cle_snowflake_gravity
cle_snowflake_collision
cle_snowflake_x
cle_snowflake_y
cle_sprite_auto_reload
cle_gun_sprite
cle_hammer_sprite
cle_grenade_sprite
cle_laser_sprite
cle_shotgun_sprite
cle_ninja_sprite
cle_gun_cursor_sprite
cle_hammer_cursor_sprite
cle_grenade_cursor_sprite
cle_laser_cursor_sprite
cle_shotgun_cursor_sprite
cle_ninja_cursor_sprite
cle_sprite_gun_fire
cle_spritr_hook_chain
cle_sprite_hook_head
```





To Do:
- ~~Add a few neat little features i found in a small client c:~~
- ~~try to get silent commands to     work~~ **I love you Chillerbot :D**
- ~~add Client/Server Msg Prefix functionality~~
- ~~add Helper List~~
- ~~show mutes in console~~
- work on the visuals tab in settings more
- ~~add !skin command (clone wont come I dont think I dont like it~~
- ~~add auto reply to muted players~~
- ~~when friend joines ping you or make name be in a color~~ - makes a sound and sends an echo
- ~~toggle to turn off sending a message that starts with "." (currently does that so you cant misspell commands)~~

Maybe:
- ~~do chat message only when a friended tee says something~~
- multiple replies for tabbed out reply and muted reply
- show in scoreboard when someones in a menu
- show when someones looking at the scoreboard
