//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _INPUTCODE_H_
#define _INPUTCODE_H_

#include "InputModeEnum.h"
#include "InputCodeEnum.h"

#include "tnlVector.h"

#include <string>

// Until we move completely to SDL2
typedef S32 SDL_Keycode;

using namespace std;

namespace Zap
{

////////////////////////////////////////
////////////////////////////////////////

#define UNKNOWN_KEY_NAME "Unknown Key"

// Note that the BindingSet member name referenced below doesn't actually appear anywhere else... it could be any aribtrary and unique token

/*-----------------------------------------------------BINDING_TABLE----------------------------------------------------*/
/*                                                            BindingSet         Def. kb           Def. js              */
/*            Enum                      Name in INI           member name        binding           binding              */
#define BINDING_TABLE                                                                                                    \
   BINDING( BINDING_SELWEAP1,           "SelWeapon1",         inputSELWEAP1,     KEY_1,            KEY_1               ) \
   BINDING( BINDING_SELWEAP2,           "SelWeapon2",         inputSELWEAP2,     KEY_2,            KEY_2               ) \
   BINDING( BINDING_SELWEAP3,           "SelWeapon3",         inputSELWEAP3,     KEY_3,            KEY_3               ) \
   BINDING( BINDING_ADVWEAP,            "SelNextWeapon",      inputADVWEAP,      KEY_E,            BUTTON_1            ) \
   BINDING( BINDING_ADVWEAP2,           "SelNextWeapon2",     inputADVWEAP2,     MOUSE_WHEEL_UP,   MOUSE_WHEEL_UP      ) \
   BINDING( BINDING_PREVWEAP,           "SelPrevWeapon",      inputPREVWEAP,     MOUSE_WHEEL_DOWN, MOUSE_WHEEL_DOWN    ) \
   BINDING( BINDING_CMDRMAP,            "ShowCmdrMap",        inputCMDRMAP,      KEY_C,            BUTTON_2            ) \
   BINDING( BINDING_TEAMCHAT,           "TeamChat",           inputTEAMCHAT,     KEY_T,            KEY_T               ) \
   BINDING( BINDING_GLOBCHAT,           "GlobalChat",         inputGLOBCHAT,     KEY_G,            KEY_G               ) \
   BINDING( BINDING_QUICKCHAT,          "QuickChat",          inputQUICKCHAT,    KEY_V,            BUTTON_3            ) \
   BINDING( BINDING_CMDCHAT,            "Command",            inputCMDCHAT,      KEY_SLASH,        KEY_SLASH           ) \
   BINDING( BINDING_LOADOUT,            "ShowLoadoutMenu",    inputLOADOUT,      KEY_Z,            BUTTON_4            ) \
   BINDING( BINDING_MOD1,               "ActivateModule1",    inputMOD1,         KEY_SPACE,        BUTTON_TRIGGER_LEFT ) \
   BINDING( BINDING_MOD2,               "ActivateModule2",    inputMOD2,         MOUSE_RIGHT,      BUTTON_6            ) \
   BINDING( BINDING_FIRE,               "Fire",               inputFIRE,         MOUSE_LEFT,       MOUSE_LEFT          ) \
   BINDING( BINDING_DROPITEM,           "DropItem",           inputDROPITEM,     KEY_B,            KEY_B               ) \
   BINDING( BINDING_TOGVOICE,           "VoiceChat",          inputTOGVOICE,     KEY_R,            KEY_R               ) \
   BINDING( BINDING_UP,                 "ShipUp",             inputUP,           KEY_W,            KEY_UP              ) \
   BINDING( BINDING_DOWN,               "ShipDown",           inputDOWN,         KEY_S,            KEY_DOWN            ) \
   BINDING( BINDING_LEFT,               "ShipLeft",           inputLEFT,         KEY_A,            KEY_LEFT            ) \
   BINDING( BINDING_RIGHT,              "ShipRight",          inputRIGHT,        KEY_D,            KEY_RIGHT           ) \
   BINDING( BINDING_SCRBRD,             "ShowScoreboard",     inputSCRBRD,       KEY_TAB,          BUTTON_5            ) \
   BINDING( BINDING_MISSION,            "Mission",            inputMISSION,      KEY_F2,           KEY_F2              ) \
   BINDING( BINDING_TOGGLE_RATING,      "ToggleRating",       inputTOGGLERATING, KEY_EQUALS,       KEY_EQUALS          ) \
   BINDING( BINDING_FPS,                "FPS",                keyFPS,            KEY_F6,           KEY_F6              ) \
   BINDING( BINDING_LOAD_PRESET_1,      "LoadLoadoutPreset1", keyLoadPreset1,    KEY_ALT_1,        KEY_ALT_1           ) \
   BINDING( BINDING_LOAD_PRESET_2,      "LoadLoadoutPreset2", keyLoadPreset2,    KEY_ALT_2,        KEY_ALT_2           ) \
   BINDING( BINDING_LOAD_PRESET_3,      "LoadLoadoutPreset3", keyLoadPreset3,    KEY_ALT_3,        KEY_ALT_3           ) \
   BINDING( BINDING_SAVE_PRESET_1,      "SaveLoadoutPreset1", keySavePreset1,    KEY_CTRL_1,       KEY_CTRL_1          ) \
   BINDING( BINDING_SAVE_PRESET_2,      "SaveLoadoutPreset2", keySavePreset2,    KEY_CTRL_2,       KEY_CTRL_2          ) \
   BINDING( BINDING_SAVE_PRESET_3,      "SaveLoadoutPreset3", keySavePreset3,    KEY_CTRL_3,       KEY_CTRL_3          ) \
/*----------------------------------------------------------------------------------------------------------------------*/


/*----------------------------------------EDITOR_BINDING_TABLE----------------------------------------------*/
/*                                                                  BindingSet           Def. kb            */
/*                       Enum                  Name in INI          member name          binding            */
#define EDITOR_BINDING_TABLE                                                                                 \
   EDITOR_BINDING( BINDING_FLIP_HORIZ,        "FlipItemHorizontal", keyFlipItemHoriz,   "H"                ) \
   EDITOR_BINDING( BINDING_PASTE_SELECTION,   "PasteSelection",     keyPasteSelection,  "Ctrl+V"           ) \
   EDITOR_BINDING( BINDING_FLIP_VERTICAL,     "FlipItemVertical",   keyFlipItemVertl,   "V"                ) \
   EDITOR_BINDING( BINDING_RELOAD_LEVEL,      "ReloadLevel",        keyReloadLevel,     "Ctrl+Alt+Shift+L" ) \
   EDITOR_BINDING( BINDING_REDO_ACTION,       "RedoAction",         keyRedoAction,      "Ctrl+Shift+Z"     ) \
   EDITOR_BINDING( BINDING_UNDO_ACTION,       "UndoAction",         keyUndoAction,      "Ctrl+Z"           ) \
   EDITOR_BINDING( BINDING_RESET_VIEW,        "ResetView",          keyResetView,       "Z"                ) \
   EDITOR_BINDING( BINDING_LVLGEN_SCRIPT,     "RunLevelgenScript",  keyRunLvlgenScript, "Ctrl+K"           ) \
   EDITOR_BINDING( BINDING_ROTATE_CENTROID,   "RotateCentroid",     keyRotateCentroid,  "Alt+R"            ) \
   EDITOR_BINDING( BINDING_ROTATE_ORIGIN,     "RotateOrigin",       keyRotateOrigin,    "Ctrl+Alt+R"       ) \
   EDITOR_BINDING( BINDING_SPIN_CCW,          "RotateSpinCCW",      keyRotateSpinCCW,   "R"                ) \
   EDITOR_BINDING( BINDING_SPIN_CW,           "RotateSpinCW",       keyRotateSpinCW,    "Shift+R"          ) \
   EDITOR_BINDING( BINDING_ROTATE_CCW_ORIGIN, "RotateCCWOrigin",    keyRotateCCWOrigin, "Ctrl+R"           ) \
   EDITOR_BINDING( BINDING_ROTATE_CW_ORIGIN,  "RotateCWOrigin",     keyRotateCWOrigin,  "Ctrl+Shift+R"     ) \
   EDITOR_BINDING( BINDING_INSERT_GEN_ITEMS,  "InsertGenItems",     keyInsertGenItems,  "Ctrl+I"           ) \
   EDITOR_BINDING( BINDING_SAVE_LEVEL,        "SaveLevel",          keySaveLevel,       "Ctrl+S"           ) \
   EDITOR_BINDING( BINDING_ZOOM_IN,           "ZoomIn",             keyZoomIn,          "E"                ) \
   EDITOR_BINDING( BINDING_ZOOM_OUT,          "ZoomOut",            keyZoomOut,         "C"                ) \
   EDITOR_BINDING( BINDING_JOIN_SELECTION,    "JoinSelection",      keyJoinSelection,   "J"                ) \
   EDITOR_BINDING( BINDING_SELECT_EVERYTHING, "SelectEverything",   keySelectAll,       "Ctrl+A"           ) \
   EDITOR_BINDING( BINDING_RESIZE_SELECTION,  "ResizeSelection",    keyResizeSelection, "Ctrl+Shift+X"     ) \
   EDITOR_BINDING( BINDING_CUT_SELECTION,     "CutSelection",       keyCutSelection,    "Ctrl+X"           ) \
   EDITOR_BINDING( BINDING_COPY_SELECTION,    "CopySelection",      keyCopySelection,   "Ctrl+C"           ) \
   EDITOR_BINDING( BINDING_LEVEL_PARAM_EDITOR,"GameParameterEditor",keyGameParamEditor, "F3"               ) \
   EDITOR_BINDING( BINDING_TEAM_EDITOR,       "TeamEditor",         keyTeamEditor,      "F2"               ) \
   EDITOR_BINDING( BINDING_PLACE_TELEPORTER,  "PlaceNewTeleporter", keyPlaceTeleporter, "T"                ) \
   EDITOR_BINDING( BINDING_PLACE_SPEEDZONE,   "PlaceNewSpeedZone",  keyPlaceSpeedZone,  "P"                ) \
   EDITOR_BINDING( BINDING_PLACE_SPAWN,       "PlaceNewSpawn",      keyPlaceSpawn,      "G"                ) \
   EDITOR_BINDING( BINDING_PLACE_SPYBUG,      "PlaceNewSpybug",     keyPlaceSpybug,     "Ctrl+Shift+B"     ) \
   EDITOR_BINDING( BINDING_PLACE_REPAIR,      "PlaceNewRepair",     keyPlaceRepair,     "B"                ) \
   EDITOR_BINDING( BINDING_PLACE_TURRET,      "PlaceNewTurret",     keyPlaceTurret,     "Y"                ) \
   EDITOR_BINDING( BINDING_PLACE_MINE,        "PlaceNewMine",       keyPlaceMine,       "M"                ) \
   EDITOR_BINDING( BINDING_PLACE_FORCEFIELD,  "PlaceNewForcefield", keyPlaceForcefield, "F"                ) \
/*   EDITOR_BINDING( BINDING_NO_SNAPPING,       "NoSnapping",         keyNoSnapping,      "Shift+Space"      ) */\
/*   EDITOR_BINDING( BINDING_NO_GRID_SNAPPING,  "NoGridSnapping",     keyNoGridSnapping,  "Space"            ) */\
   EDITOR_BINDING( BINDING_PREVIEW_MODE,      "PreviewMode",        keyPreviewMode,     "Tab"              ) \
   EDITOR_BINDING( BINDING_DOCKMODE_ITEMS,    "DockmodeItems",      keyDockmodeItems,   "F4"               ) \
   EDITOR_BINDING( BINDING_TOGGLE_EDIT_MODE,  "ToggleEditMode",     keyToggleEditMode,  "Insert"           ) \
/*----------------------------------------------------------------------------------------------------------*/


/*------------------------------------EDITOR_BINDING_KEYCODE_TABLE-----------------------------------------------------------*/
/*                                                                                 BindingSet                      Def.      */
/*                       Enum                          Name in INI                 member name                     binding   */
#define EDITOR_BINDING_KEYCODE_TABLE                                                                                          \
   EDITOR_BINDING( BINDING_NO_GRID_SNAPPING,          "DisableGridSnapping",       inputDisableGridSnapping,       KEY_SPACE) \
   EDITOR_BINDING( TURN_ON_CONSTRAINED_MOVEMENT_CODE, "EnableConstrainedMovement", inputEnableConstrainedMovement, KEY_SHIFT) \
/*---------------------------------------------------------------------------------------------------------------------------*/


/*---------------------------------------SPECIAL_BINDING_TABLE----------------------------------------------*/
/*                                                              BindingSet          Def. kb    Def. js      */
/*                       Enum                Name in INI        member name         binding    binding      */
#define SPECIAL_BINDING_TABLE                                                                               \
   SPECIAL_BINDING( BINDING_SCREENSHOT_1,   "Screenshot_1",     keyScreenshot1,    "PrntScrn", "PrntScrn")  \
   SPECIAL_BINDING( BINDING_SCREENSHOT_2,   "Screenshot_2",     keyScreenshot2,    "Ctrl+Q",   "Ctrl+Q"  )  \
   SPECIAL_BINDING( BINDING_HELP,           "Help",             keyHELP,           "F1",       "F1"      )  \
   SPECIAL_BINDING( BINDING_LOBBYCHAT,      "OutOfGameChat",    keyLOBBYCHAT,      "F5",       "F5"      )  \
   SPECIAL_BINDING( BINDING_DIAG,           "Diagnostics",      keyDIAG,           "F7",       "F7"      )  \

/*---------------------------------------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------------------------------------*/
   

enum BindingNameEnum {
#define BINDING(enumName, b, c, d, e) enumName,
    BINDING_TABLE
#undef BINDING
    BINDING_DEFINEABLE_KEY_COUNT
};


enum EditorBindingNameEnum {
#define EDITOR_BINDING(editorEnumName, b, c, d) editorEnumName,
    EDITOR_BINDING_TABLE
#undef EDITOR_BINDING
    EDITOR_BINDING_DEFINEABLE_KEY_COUNT
};


enum EditorBindingCodeEnum {
#define EDITOR_BINDING(enumName, b, c, d) enumName,
   EDITOR_BINDING_KEYCODE_TABLE
#undef EDITOR_BINDING
    EDITOR_BINDING_DEFINEABLE_CODE_COUNT
};


enum SpecialBindingNameEnum {
#define SPECIAL_BINDING(specialEnumName, b, c, d, e) specialEnumName,
    SPECIAL_BINDING_TABLE
#undef SPECIAL_BINDING
    SPECIAL_BINDING_DEFINEABLE_KEY_COUNT
};


////////////////////////////////////////
////////////////////////////////////////

struct BindingSet
{
   BindingSet();     // Constructor
   virtual ~BindingSet();

   bool hasKeypad() const;

   InputCode getBinding(BindingNameEnum bindingName) const;
   void setBinding(BindingNameEnum bindingName, InputCode key);

   // Create a sequence of member variables from memberName column of the BINDING_TABLE above
#define BINDING(a, b, memberName, d, e) InputCode memberName;
    BINDING_TABLE
#undef BINDING
};


////////////////////////////////////////
////////////////////////////////////////

struct EditorBindingSet
{
   EditorBindingSet();     // Constructor
   virtual ~EditorBindingSet();

   string    getBinding(EditorBindingNameEnum bindingName) const;
   InputCode getBinding(EditorBindingCodeEnum bindingName) const;
   void setBinding(EditorBindingNameEnum bindingName, const string &key);


   // Generate some member variables
#define EDITOR_BINDING(a, b, memberName, d) string memberName;
   EDITOR_BINDING_TABLE
#undef EDITOR_BINDING

#define EDITOR_BINDING(a, b, memberName, d) InputCode memberName;
      EDITOR_BINDING_KEYCODE_TABLE
#undef EDITOR_BINDING
};


////////////////////////////////////////
////////////////////////////////////////

struct SpecialBindingSet
{
   SpecialBindingSet();     // Constructor
   virtual ~SpecialBindingSet();

   string getBinding(SpecialBindingNameEnum bindingName) const;
   void setBinding(SpecialBindingNameEnum bindingName, const string &key);

#define SPECIAL_BINDING(a, b, memberName, d, e) string memberName;
   SPECIAL_BINDING_TABLE
#undef SPECIAL_BINDING
};


////////////////////////////////////////
////////////////////////////////////////

class InputCodeManager
{
private:
   bool mBindingsHaveKeypadEntry;
   InputMode mInputMode;             // Joystick or Keyboard

   Vector<BindingSet> mBindingSets;

   EditorBindingSet mEditorBindingSet;

   Vector<SpecialBindingSet> mSpecialBindingSets;

public:
   enum JoystickJoysticks {
      JOYSTICK_DPAD,
      JOYSTICK_STICK_1,
      JOYSTICK_STICK_2
   };

   InputCodeManager();     // Constructor
   virtual ~InputCodeManager();

   static const char *inputCodeToString(InputCode inputCode);
   static const char *inputCodeToPrintableChar(InputCode inputCode);

   static InputCode stringToInputCode(const char *inputName);

   static void setState(InputCode inputCode, bool state);   // Set key state (t=down, f=up)
   static bool getState(InputCode inputCode);               // Return current key state (t=down, f=up)
   static void resetStates();                               // Initialize key states
   static void dumpInputCodeStates();                       // Log key states for testing
   static void initializeKeyNames();

   //static S32 getBindingCount();
   static string getBindingName(BindingNameEnum binding);
   static string getEditorBindingName(EditorBindingNameEnum binding);
   static string getSpecialBindingName(SpecialBindingNameEnum binding);
   
   InputCode getKeyBoundToBindingCodeName(const string &name) const;
   string getEditorKeyBoundToBindingCodeName(const string &name) const;
   string getSpecialKeyBoundToBindingCodeName(const string &name) const;

   // Some converters
   InputCode filterInputCode(InputCode inputCode);    // Calls filters below

#ifndef ZAP_DEDICATED
   static InputCode sdlControllerButtonToInputCode(U8 button);
#endif

   static InputCode convertJoystickToKeyboard(InputCode inputCode);
private:
   static InputCode convertNumPadToNum(InputCode inputCode);
   bool checkIfBindingsHaveKeypad(InputMode inputMode) const;

public:
   static string getCurrentInputString(InputCode inputCode);

   static bool checkModifier(InputCode mod1);            
   static bool checkModifier(InputCode mod1, InputCode mod2);            
   static bool checkModifier(InputCode mod1, InputCode mod2, InputCode mod3);

   static bool isValidInputString(const string &inputString);
   static string normalizeInputString(const string &inputString);


   void setInputMode(InputMode inputMode);
   InputMode getInputMode()    const;
   string getInputModeString() const;  // Returns display-friendly mode designator like "Keyboard" or "Joystick 1"

   #ifndef ZAP_DEDICATED
      static InputCode sdlKeyToInputCode(SDL_Keycode key);        // Convert SDL keys to InputCode
      static SDL_Keycode inputCodeToSDLKey(InputCode inputCode);  // Take a InputCode and return the SDL equivalent
   #endif

   static char keyToAscii(int unicode, InputCode inputCode);   // Return a printable ascii char, if possible
   static bool isControllerButton(InputCode inputCode);        // Does inputCode represent a controller button?
   static bool isKeypadKey(InputCode inputCode);               // Is inputCode on the numeric keypad?
   static bool isMouseAction(InputCode inputCode);             // Is inputCode related to the mouse?
   static bool isKeyboardKey(InputCode inputCode);             // Is inputCode a key on the keyboard?
   static bool isCtrlKey(InputCode inputCode);                 // Is inputCode modified with ctrl (e.g. KEY_CTRL_M)?
   static bool isAltKey(InputCode inputCode);                  // Is inputCode modified with alt (e.g. KEY_ALT_1)?
   static bool isModifier(InputCode inputCode);                // Is inputCode a modifier key?
   static bool isModified(InputCode inputCode);                // Does inputCode have a modifier attached to it?

   // For dealing with special cases, such as Ctrl-M
   static InputCode getModifier(InputCode inputCode);
   static string getModifierString(InputCode inputCode);

   static const Vector<string> *getModifierNames();

   static InputCode getBaseKeySpecialSequence(InputCode inputCode);
   static string getBaseKeyString(InputCode inputCode);

   static S16 inputCodeToControllerButton(InputCode inputCode);

   // For use in the INI file comments section
   static Vector<string> getValidKeyCodes(S32 width);
   static string getValidModifiers();
   static pair<string,string> getExamplesOfModifiedKeys();

   InputCode getBinding(BindingNameEnum bindingName) const; 
   InputCode getBinding(BindingNameEnum bindingName, InputMode inputMode) const;
   void setBinding(BindingNameEnum bindingName, InputCode key);
   void setBinding(BindingNameEnum bindingName, InputMode inputMode, InputCode key);

   string getEditorBinding(EditorBindingNameEnum bindingName) const;
   InputCode getEditorBinding(EditorBindingCodeEnum bindingName) const;
   void setEditorBinding(EditorBindingNameEnum bindingName, const string &inputString);

   string getSpecialBinding(SpecialBindingNameEnum bindingName) const; 
   string getSpecialBinding(SpecialBindingNameEnum bindingName, InputMode inputMode) const;

   void setSpecialBinding(SpecialBindingNameEnum bindingName, InputMode inputMode, const string &inputString);
};


////////////////////////////////////////
////////////////////////////////////////


};     // namespace Zap

#endif
