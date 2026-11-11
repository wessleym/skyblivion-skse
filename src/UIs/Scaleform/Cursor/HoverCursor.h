#pragma once

//The hover cursor: while the mouse is over something clickable in any menu, the cursor menu shows a
//pointing hand (Interface\Skyblivion\SKYBCursorHand.swf) in place of its arrow.
//
//How a menu says "clickable": it does not have to. Every Scaleform movie the game loads gets one
//Mouse listener from here. On a mouse move it asks the movie which entity lies under the pointer
//(GFx's Mouse.getTopMostEntity, the call SkyUI's lists use) and climbs to the first one that takes
//button events. That is clickable when it has onPress or onRelease (or is a Button symbol) and
//neither enabled == false, disabled == true nor useHandCursor == false: Flash's own rule for the
//hand cursor, so a menu turns the hand off for an element (a modal shade, a tooltip-only row) the
//Flash way, with useHandCursor = false. Over several open menus the topmost one with something
//under the pointer decides.
//
//A menu that finds its targets with its own hit tests (the class picker's tiles, the map's markers,
//which the engine picks) calls skse.plugins.SKYBHoverCursor.hover(true) while the pointer is on one
//and hover(false) when it leaves; false hands the decision back to the rule above.
//
//The cursor menu (Interface\cursormenu.swf, the team's) moves its clip mc_Cursor to the mouse on
//every move. The hand is loaded beside it at load time; to show it, the two clips trade names, so
//the cursor menu's own script moves whichever is showing. No change to cursormenu.swf, no
//per-frame work: the listener runs only when the mouse moves, clicks or scrolls.
//
//Settings: Data\SKSE\Plugins\SKYBHoverCursor.ini ([HoverCursor] bEnabled, sExcludeMenus).
namespace HoverCursor {
	//SKSEPlugin_Load: the Scaleform callback (SKSE accepts them only while the plugin loads).
	void InstallScaleform();
	//Once the UI exists (kDataLoaded): the menu open/close sink that resets the cursor.
	void Register();
}
