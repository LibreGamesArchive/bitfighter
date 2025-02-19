//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "GameSettings.h"
#include "LevelSource.h"

#include "BanList.h"
#include "DisplayManager.h"
#include "IniFile.h"
#include "game.h"
#include "SharedConstants.h"  // For MAX_PLAYERS
#include "stringUtils.h"      // For itos

#include "tnlTypes.h"         // For TNL_OS_WIN32 def
#include "tnlLog.h"           // For logprintf

#include "version.h"

#ifdef TNL_OS_WIN32 
#  include <windows.h>        // For ARRAYSIZE def
#endif

#include <stdio.h>

using namespace std;
using namespace CmdLineParams;

namespace Zap
{

////////////////////////////////////////
////////////////////////////////////////


enum ParamRequirements {
   NO_PARAMETERS,      
   ONE_OPTIONAL,
   ONE_REQUIRED,
   TWO_REQUIRED,
   FOUR_REQUIRED,
   ALL_REMAINING
};


struct ParamInfo {
   string paramName;
   ParamRequirements argsRequired;
   CmdLineParams::ParamId paramId;
   S32 docLevel;     
   string paramString;
   string helpString;
   const char *errorMsg;
};


ParamInfo paramDefs[] = {   
// Parameter               Args required   ParamId           Doc. tier  Args                  Help string            Error message (not needed for NO_PARAMETERS)

// Player-oriented options
{ "name",                  ONE_REQUIRED,   LOGIN_NAME,            0, "<string>",    "Specify your username",                                                                   "You must enter a nickname with the -name option" },
{ "password",              ONE_REQUIRED,   LOGIN_PASSWORD,        0, "<string>",    "Specify your password",                                                                   "You must enter a password with the -password option" },
{ "window",                NO_PARAMETERS,  WINDOW_MODE,           0, "",            "Start in windowed mode",                                       "" },
{ "fullscreen",            NO_PARAMETERS,  FULLSCREEN_MODE,       0, "",            "Start in fullscreen mode (no stretching)",                     "" },
{ "fullscreen-stretch",    NO_PARAMETERS,  FULLSCREEN_STRETCH,    0, "",            "Start in fullscreen mode (gaphics stretched to fill monitor)", "" },
{ "winpos",                TWO_REQUIRED,   WINDOW_POS,            0, "<int> <int>", "Specify x,y location of game window (note that this is the position of the UL corner of the game canvas, and does not account for the window frame)", "You must specify the x and y position of the window with the -winpos option" },
{ "winwidth",              ONE_REQUIRED,   WINDOW_WIDTH,          0, "<int>",       "Specify width of game window. Height will be set automatically. Note that the specified width is the width of the game canvas itself, and does not take account of window borders. Therefore, the entire window width will exceed the size specified slightly.", "You must specify the width of the game window with the -winwidth option" },
{ "usestick",              ONE_REQUIRED,   USE_STICK,             0, "<int>",       "Specify which joystick or other input device to use. Default is 1.", "You must specify the joystick you want to use with the -usestick option" },
{ "nomusic",               NO_PARAMETERS,  NO_MUSIC,              0, "",            "Disable music for this session only", "" },
{ "master",                ONE_REQUIRED,   MASTER_ADDRESS,        0, "<address>",   "Use master server (game finder) at specified address",                                   "You must specify a master server address with -master option" },

// Options for hosting
{ "dedicated",             NO_PARAMETERS,  DEDICATED,             1, "",          "Run as a dedicated game server (i.e. no game window, console mode)",                     "" },
{ "hostondedicated",       NO_PARAMETERS,  HOST_ON_DEDICATED,     1, "",          "Run as a dedicated game server using host's maps and settings" ,                         "" },
{ "serverpassword",        ONE_REQUIRED,   SERVER_PASSWORD,       1, "<string>",  "Specify a server password (players will need to know this to connect to your server)",    "You must enter a password with the -serverpassword option" },
{ "ownerpassword",         ONE_REQUIRED,   OWNER_PASSWORD,        1, "<string>",  "Specify an owner password (allowing those with the password to have all admin priveleges and power over admins) when you host a game or run a dedicated server", "You must specify an owner password with the -ownerpassword option" },
{ "adminpassword",         ONE_REQUIRED,   ADMIN_PASSWORD,        1, "<string>",  "Specify an admin password (allowing those with the password to kick players and change their teams) when you host a game or run a dedicated server", "You must specify an admin password with the -adminpassword option" },
{ "noadminpassword",       NO_PARAMETERS,  NO_ADMIN_PASSWORD,     1, "",          "Overrides admin password specified in the INI (or cmd line), and will not allow anyone to have admin permissions", "" },
{ "levelchangepassword",   ONE_REQUIRED,   LEVEL_CHANGE_PASSWORD, 1, "<string>",  "Specify the password required for players to be able to change levels on your server when you host a game or run a dedicated server", "You must specify an level-change password with the -levelchangepassword option" },
{ "nolevelchangepassword", NO_PARAMETERS,  NO_LEVEL_CHANGE_PASSWORD, 1, "",       "Overrides level change password specified in the INI (or cmd line), and will allow any player to change levels", "" },
{ "hostname",              ONE_REQUIRED,   HOST_NAME,             1, "<string>",  "Set the name that will appear in the server browser when searching for servers", "You must specify a server name with the -hostname option" },
{ "hostdescr",             ONE_REQUIRED,   HOST_DESCRIPTION,      1, "<string>",  "Set a brief description of the server, which will be visible when players browse for game servers. Use double quotes (\") for descriptions containing spaces.", "You must specify a description (use quotes) with the -hostdescr option" },
{ "maxplayers",            ONE_REQUIRED,   MAX_PLAYERS_PARAM,     1, "<int>",     "Max players allowed in a game (default is 128)", "You must specify the max number of players on your server with the -maxplayers option" }, 
{ "hostaddr",              ONE_REQUIRED,   HOST_ADDRESS,          1, "<address>", "Specify host address for the server to listen to when hosting",                        "You must specify a host address for the host to listen on (e.g. IP:Any:28000 or IP:192.169.1.100:5500)" },

// Specifying levels
{ "levels",                ALL_REMAINING,  LEVEL_LIST,            2, "<level 1> [level 2]...", "Specify the levels to play. Note that all remaining items on the command line will be interpreted as levels, so this must be the last parameter.", "You must specify one or more levels to load with the -levels option" },
{ "playlist",              ALL_REMAINING,  USE_FILE,              2, "<path>", "Specify the location of a playlist of levels, which is a text file that specifies what levels the user want to be played. you must specify its relative location (relative to the Bitfighter binaries) with the -playlist option"},

// Specifying folders
{ "rootdatadir",           ONE_REQUIRED,   ROOT_DATA_DIR,         3, "<path>",                "Equivalent to setting the -inidir, -logdir, -robotdir, -screenshotdir, and -leveldir parameters. The application will automatially append \"/robots\", \"/screenshots\", and \"/levels\" to path as appropriate.", "You must specify the root data folder with the -rootdatadir option" },
{ "leveldir",              ONE_REQUIRED,   LEVEL_DIR,             2, "<folder or subfolder>", "Load all levels in specified system folder, or a subfolder under the levels folder. Levels will be loaded in alphabetical order by level-file name. Admins can create custom level lists by copying selected levels into folders or subfolders, and rename the files to get them to load in the proper order.", "You must specify a levels subfolder with the -leveldir option" },
{ "logdir",                ONE_REQUIRED,   LOG_DIR,               3, "<path>",                "Folder where logfiles will be written",                                                    "You must specify your log folder with the -logdir option" },
{ "scriptsdir",            ONE_REQUIRED,   SCRIPTS_DIR,           3, "<path>",                "Folder where Lua helper scripts are stored",                                               "You must specify the folder where your Lua scripts are stored with the -scriptsdir option" },
{ "robotdir",              ONE_REQUIRED,   ROBOT_DIR,             3, "<path>",                "Folder where robot scripts are stored",                                                    "You must specify the robots folder with the -robotdir option" },
{ "screenshotdir",         ONE_REQUIRED,   SCREENSHOT_DIR,        3, "<path>",                "Folder where screenshots are stored",                                                      "You must specify your screenshots folder with the -screenshotdir option" },
{ "sfxdir",                ONE_REQUIRED,   SFX_DIR,               3, "<path>",                "Folder where user sounds are stored; will overwrite defaults; can specify multiple times", "You must specify your sounds folder with the -sfxdir option"},
{ "musicdir",              ONE_REQUIRED,   MUSIC_DIR,             3, "<path>",                "Folder where game music stored",                                                           "You must specify your music folder with the -musicdir option" },
{ "plugindir",             ONE_REQUIRED,   PLUGIN_DIR,            3, "<path>",                "Folder where editor plugins are stored",                                                   "You must specify your plugins folder with the -plugindir option" },
{ "fontsdir",              ONE_REQUIRED,   FONTS_DIR,             3, "<path>",                "Folder where fonts are stored; will overwrite defaults; can specify multiple times",       "You must specify your fonts folder with the -fontsdir option" },
{ "recorddir",             ONE_REQUIRED,   RECORD_DIR,            3, "<path>",                "Folder where recording gameplay are stored",                                               "You must specify your recorded gameplay folder with the -recorddir option" },

// Developer-oriented options
{ "loss",                  ONE_REQUIRED,   SIMULATED_LOSS,        4, "<float>",   "Simulate the specified amount of packet loss, from 0 (no loss) to 1 (all packets lost) Note: Client only!", "You must specify a loss rate between 0 and 1 with the -loss option" },
{ "lag",                   ONE_REQUIRED,   SIMULATED_LAG,         4, "<int>",     "Simulate the specified amount of server lag (in milliseconds) Note: Client only!",                          "You must specify a lag (in ms) with the -lag option" },
{ "stutter",               ONE_REQUIRED,   SIMULATED_STUTTER,     4, "<int>",     "Simulate VPS CPU stutter (in milliseconds/second) Note: Server only!",                                      "You must specify a value (in ms) with the -stutter option.  Values clamped to 0-1000" },
{ "forceupdate",           NO_PARAMETERS,  FORCE_UPDATE,          4, "",          "Trick game into thinking it needs to update",                                            "" },

// Also, see the directives section below!

};


// The following are commands and directives that can be specified on the cmd line, that are used to do something other than play the game
struct DirectiveInfo {
   string paramName;
   ParamRequirements argsRequired;
   CmdLineParams::ParamId paramId;
   S32 docLevel;     
   void (*cmdCallback)(GameSettings *settings, const Vector<string> &args);
   string paramString;
   string helpString;
   const char *errorMsg;
};


DirectiveInfo directiveDefs[] = {   

// Advanced server management options
{ "getres",  FOUR_REQUIRED,  SEND_RESOURCE, 5, GameSettings::getRes,    "<server address> <admin password> <resource name> <LEVEL|LEVELGEN|BOT>", "Send a resource to a remote server. Address must be specified in the form IP:nnn.nnn.nnn.nnn:port. The server must be running, have an admin password set, and have resource management enabled ([Host] section in the bitfighter.ini file).", "Usage: bitfighter getres <server address> <admin password> <resource name> <LEVEL|LEVELGEN|BOT>" },
{ "sendres", FOUR_REQUIRED,  GET_RESOURCE,  5, GameSettings::sendRes,   "<server address> <admin password> <resource name> <LEVEL|LEVELGEN|BOT>", "Retrieve a resource from a remote server, with same requirements as -sendres.",                                                                                                                                                                "Usage: bitfighter sendres <server address> <admin password> <resource name> <LEVEL|LEVELGEN|BOT>" },

// Other commands
{ "rules",   NO_PARAMETERS,  SHOW_RULES,        6, GameSettings::showRules,      "",  "Print a list of \"rules of the game\" and other possibly useful data", "" },
{ "help",    NO_PARAMETERS,  HELP,              6, GameSettings::showHelp,       "",  "Display this message", "" },
{ "version", NO_PARAMETERS,  VERSION,           6, GameSettings::showVersion,    "",  "Print version information", "" },

};

// These correspond to tier above
const char *helpTitles[] = {
   "Player-oriented options:",
   "Options for hosting:",
   "Specifying levels:",
   "Specifying folders:\n\nAll of the following options can be specified with either a relative or absolute path. They are primarily intended to make installation on certain Linux platforms more flexible; they are not meant for daily use by average users.\nIn most cases, -rootdatadir is the only parameter in this section you will need.",
   "Developer-oriented options:",
   "Advanced server management commands:",
   "Other commands:",
};


////////////////////////////////////////
////////////////////////////////////////
// Define statics
GameSettings *GameSettings::staticSelf = NULL;

// List of controllers we found attached to this machine.  This contains the
// Controller index as the key, the name as the value
map<S32,string> GameSettings::DetectedControllerList;

S32 GameSettings::UseControllerIndex = -1;

CIniFile GameSettings::iniFile("dummy");                 // Our INI file.  Real filename will be supplied later.
CIniFile GameSettings::userPrefs("dummy");               // Our INI file.  Real filename will be supplied later.

string GameSettings::mExecutablePath = "bitfighter";     // Default executable name, will be overwritten

// Constructor
GameSettings::GameSettings()
{
   mFolderManager = new FolderManager();        // Cleaned up in destructor

   mBanList = new BanList(getFolderManager()->getIniDir());
   mLoadoutPresets.resize(LoadoutPresetCount);   // Make sure we have the right number of slots available

   staticSelf = this;
}


// Destructor -- this will only get run when Bitfighter is shutting down
GameSettings::~GameSettings()
{
   delete mBanList;
   if(mFolderManager)
   {
      delete mFolderManager;
      mFolderManager = NULL;
   }
}


// Helpers for init functions below
static const string *choose(const string &firstChoice, const string &secondChoice)
{
   return firstChoice != "" ? &firstChoice : &secondChoice;
}


static const string *choose(const string &firstChoice, const string &secondChoice, const string &thirdChoice)
{
   return choose(firstChoice, *choose(secondChoice, thirdChoice));
}


GameSettings *GameSettings::get()
{
   TNLAssert(staticSelf, "Static access is NULL!");
   return staticSelf;
}


void GameSettings::setExecutablePath(const string &executablePath)
{
   mExecutablePath = executablePath;
}


string GameSettings::getHostName() const
{
   return mHostName;
}


void GameSettings::setHostName(const string &serverName, bool updateINI) 
{ 
   mHostName = serverName; 

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::ServerName, serverName);
}


string GameSettings::getHostDescr() const
{
   return mHostDescr;
}


void GameSettings::setHostDescr(const string &serverDescription, bool updateINI) 
{ 
   mHostDescr = serverDescription;
   
   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::ServerDescription, serverDescription);
}


string GameSettings::getWelcomeMessage() const
{
   return mWelcomeMessage;
}


void GameSettings::setWelcomeMessage(const string &welcomeMessage, bool updateINI) 
{ 
   mWelcomeMessage = welcomeMessage;

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::WelcomeMessage, welcomeMessage);
}


string GameSettings::getServerPassword() const
{
   return mServerPassword;
}


void GameSettings::setServerPassword(const string &serverPassword, bool updateINI) 
{ 
   mServerPassword = serverPassword; 

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::ServerPassword, serverPassword);
}


string GameSettings::getOwnerPassword() const
{
   return mOwnerPassword;
}


void GameSettings::setOwnerPassword(const string &ownerPassword, bool updateINI)
{
   mOwnerPassword = ownerPassword;

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::OwnerPassword, ownerPassword);
}


string GameSettings::getAdminPassword() const
{
   return mAdminPassword;
}


void GameSettings::setAdminPassword(const string &adminPassword, bool updateINI) 
{ 
   mAdminPassword = adminPassword; 

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::AdminPassword, adminPassword);
}


string GameSettings::getLevelChangePassword() const
{
   return mLevelChangePassword;
}


void GameSettings::setLevelChangePassword(const string &levelChangePassword, bool updateINI) 
{ 
   mLevelChangePassword = levelChangePassword;     // Update our working copy

   if(updateINI)
      mIniSettings.mSettings.setVal(IniKey::LevelChangePassword, levelChangePassword);        
}


string GameSettings::getCmdLineParamString(ParamId paramId)
{
   return mCmdLineParams[paramId].size() > 0 ? mCmdLineParams[paramId].get(0) : "";
}


Vector<string> GameSettings::getCmdLineParamStringList(ParamId paramId)
{
   return mCmdLineParams[paramId];
}


U32 GameSettings::getCmdLineParamU32(ParamId paramId)
{
   return mCmdLineParams[paramId].size() > 0 ? U32(atoi(mCmdLineParams[paramId].get(0).c_str())) : 0;
}


F32 GameSettings::getCmdLineParamF32(ParamId paramId)
{
   return mCmdLineParams[paramId].size() > 0 ? (F32)Zap::stof(mCmdLineParams[paramId].get(0)) : 0;
}


bool GameSettings::isCmdLineParamSpecified(ParamId paramId) const
{
   return mCmdLineParams[paramId].size() > 0;
}


FolderManager *GameSettings::getFolderManager() const
{
   return mFolderManager;
}


FolderManager GameSettings::getCmdLineFolderManager()
{
    return FolderManager(getCmdLineParamString(LEVEL_DIR), 
                         getCmdLineParamString(ROBOT_DIR), 
                         getCmdLineParamStringList(SFX_DIR),
                         getCmdLineParamString(MUSIC_DIR),
                         getCmdLineParamString(INI_DIR),
                         getCmdLineParamString(LOG_DIR),
                         getCmdLineParamString(SCREENSHOT_DIR),
                         getCmdLineParamString(SCRIPTS_DIR),
                         getCmdLineParamString(ROOT_DATA_DIR),
                         getCmdLineParamStringList(PLUGIN_DIR),
                         getCmdLineParamStringList(FONTS_DIR),
                         getCmdLineParamString(RECORD_DIR));
}


BanList *GameSettings::getBanList() const
{
   return mBanList;
}


Vector<string> GameSettings::getPlaylists() const
{
   string levelDir = getFolderManager()->getLevelDir();
   return getFolderManager()->findAllPlaylistsInFolder(levelDir);
}


// Figure out where all our folders are
void GameSettings::resolveDirs()
{
   getFolderManager()->resolveDirs(this);        // Resolve all folders except for levels folder, which is resolved later
}


string GameSettings::getHostAddress()
{
   // Try cmd line first
   string cmdLineHostAddr = getCmdLineParamString(HOST_ADDRESS);

   if(cmdLineHostAddr != "")
      return cmdLineHostAddr;

   // Then look in the INI
   string addr = mIniSettings.mSettings.getVal<string>(IniKey::ServerAddress);

   if(addr != "")
      return addr;

   // Fall back to default, which is what we usually want anyway!
   return "IP:Any:" + itos(DEFAULT_GAME_PORT);
}


U32 GameSettings::getMaxPlayers()
{
   U32 maxplayers = getCmdLineParamU32(MAX_PLAYERS_PARAM);     // Command line value

   if(maxplayers == 0)
      maxplayers = mIniSettings.mSettings.getVal<U32>(IniKey::MaxPlayers);

   if(maxplayers > MAX_PLAYERS)
      maxplayers = MAX_PLAYERS;

   return maxplayers;
}


// Write all our settings to bitfighter.ini
void GameSettings::save()
{
   //   BanList *bl = settings->getBanList();
   //   bl->writeToFile();      // Writes ban list back to file XXX enable this when admin functionality is built in

   //saveWindowMode(&iniFile, &mIniSettings);
   //setSetting("WindowMode", cmdLineDisplayMode);
      //ini->SetValue("Settings",  "WindowMode", displayModeToString(iniSettings->displayMode));;
   saveSettingsToINI(&iniFile, this);        // Writes settings to iniFile, then writes it to disk
}


IniSettings *GameSettings::getIniSettings()
{
   return &mIniSettings;
}


string GameSettings::getDefaultName()
{
   return getSetting<string>(IniKey::DefaultName);
}


bool GameSettings::getForceUpdate() const
{
   return isCmdLineParamSpecified(FORCE_UPDATE);
}


string GameSettings::getPlayerName() const
{
   return mPlayerName;
}


void GameSettings::setQueryServerSortColumn(S32 column, bool ascending)
{
   mIniSettings.mSettings.setVal(IniKey::QueryServerSortColumn, column);
   mIniSettings.mSettings.setVal(IniKey::QueryServerSortAscending, ascending ? Yes : No);
}


S32  GameSettings::getQueryServerSortColumn()    const { return mIniSettings.mSettings.getVal<S32>(IniKey::QueryServerSortColumn);      }
bool GameSettings::getQueryServerSortAscending() const { return mIniSettings.mSettings.getVal<YesNo>(IniKey::QueryServerSortAscending); }


void GameSettings::setWindowPosition(S32 x, S32 y)
{
   setSetting(IniKey::WindowXPos, x);
   setSetting(IniKey::WindowYPos, y);
}

S32 GameSettings::getWindowPositionX() const { return mIniSettings.mSettings.getVal<S32>(IniKey::WindowXPos); }
S32 GameSettings::getWindowPositionY() const { return mIniSettings.mSettings.getVal<S32>(IniKey::WindowYPos); }


void GameSettings::setWindowSizeFactor(F32 scalingFactor)
{
   setSetting(IniKey::WindowScalingFactor, scalingFactor);
}

F32 GameSettings::getWindowSizeFactor() const { return mIniSettings.mSettings.getVal<F32>(IniKey::WindowScalingFactor); }


F32 GameSettings::getMusicVolume() const
{
   if(isCmdLineParamSpecified(NO_MUSIC))
      return 0;

   return mIniSettings.mSettings.getVal<F32>(IniKey::MusicVolume);
}


// User has entered name and password, and has clicked Ok.  That's the only way to get here.
// Do not call this function directly -- you probably want ClientGame::userEnteredLoginCredentials(), which will call this.
void GameSettings::setLoginCredentials(const string &name, const string &password, bool savePassword)
{
   mPlayerName = name;
   mPlayerPassword = password;

   if(savePassword)
      mIniSettings.mSettings.setVal(IniKey::Password, password);

   mIniSettings.mSettings.setVal(IniKey::LastName, name);
   
   iniFile.writeFile();
}


// User name has been corrected by master server (usually changing only capitalization and such
void GameSettings::updatePlayerName(const string &name)
{
   mPlayerName = name;

   if(!mPlayerNameSpecifiedOnCmdLine)
   {
      mIniSettings.mSettings.setVal(IniKey::LastName, name);      // Save new name to the INI
      iniFile.writeFile();
   }
}


// Forums password
string GameSettings::getPlayerPassword() const
{
   return mPlayerPassword;
}


void GameSettings::setAutologin(bool autologin)
{
   if(autologin)
   {
      mIniSettings.mSettings.setVal(IniKey::Nickname, mIniSettings.mSettings.getVal<string>(IniKey::LastName));
      mIniSettings.mSettings.setVal(IniKey::Password, mIniSettings.mSettings.getVal<string>(IniKey::LastPassword));
   }
   else
   {
      mIniSettings.mSettings.setVal(IniKey::Nickname, string());
      mIniSettings.mSettings.setVal(IniKey::Password, string());
   }
}


bool GameSettings::isDedicatedServer() const
{
   return isCmdLineParamSpecified(DEDICATED) || isCmdLineParamSpecified(HOST_ON_DEDICATED);
}


string GameSettings::getLevelDir(SettingSource source)
{
   if(source == CMD_LINE)
      return getCmdLineParamString(LEVEL_DIR);
   else
      return mIniSettings.mSettings.getVal<string>(IniKey::LevelDir);
}


// Returns the string passed to the commandline option -useplaylist
// Will return the path if using the param, and "" if you aren't
string GameSettings::getPlaylistFile()
{
	return getCmdLineParamString(USE_FILE);
}


// A system of finding out if you are using a playlist
bool GameSettings::isUsingPlaylist()
{
	return getPlaylistFile() != "";
}


// Returns the location of the playlist or the the directory of the levels, depending on if you are using a playlist.
// Used in the constructors for each object, as each object either takes a playlist file of a folder.
string GameSettings::getLevelLoc()
{
	if(isUsingPlaylist())
		return getPlaylistFile();

   return getFolderManager()->getLevelDir();
}


// Returns a pointer to the desired LevelSource, depending on if you are using a playlist file or not.
// Note that game CAN be NULL here.
LevelSource *GameSettings::chooseLevelSource(Game *game)
{
	if(game->getPlaylist() != "")
   {
      string levelDir = getFolderManager()->getLevelDir();

      string playlist = checkName(game->getPlaylist(), levelDir, ".playlist");

      // Create a list of levels for hosting a game from a file, but does not read the files or do any validation of them
      Vector<string> list = PlaylistLevelSource::findAllFilesInPlaylist(playlist, levelDir);
		return new PlaylistLevelSource(list, levelDir, this);
   }

   return new FolderLevelSource(getLevelList(), getFolderManager()->getLevelDir());
}


LoadoutTracker GameSettings::getLoadoutPreset(S32 index) 
{ 
   TNLAssert(index >= 0 && index < mLoadoutPresets.size(), "Preset index out of range!") ;
   return mLoadoutPresets[index]; 
}


// Caller is responsible for bounds checking index...
void GameSettings::setLoadoutPreset(const LoadoutTracker *preset, S32 index) 
{
   mLoadoutPresets[index] = *preset;
}


void GameSettings::addConfigurationError(const string &errorMessage)
{
   mConfigurationErrors.push_back(errorMessage);
}


Vector<string> GameSettings::getConfigurationErrors() const
{
   return mConfigurationErrors;
}


void GameSettings::saveLevelChangePassword(const string &serverName, const string &password)
{
   iniFile.setValue("SavedLevelChangePasswords", serverName, password, true);
   iniFile.writeFile();
}


void GameSettings::saveAdminPassword(const string &serverName, const string &password)
{
   iniFile.setValue("SavedAdminPasswords", serverName, password, true);
   iniFile.writeFile();
}


void GameSettings::saveOwnerPassword(const string &serverName, const string &password)
{
   iniFile.setValue("SavedOwnerPasswords", serverName, password, true);
   iniFile.writeFile();
}


void GameSettings::forgetLevelChangePassword(const string &serverName)
{
   iniFile.deleteKey("SavedLevelChangePasswords", serverName);
   iniFile.writeFile();
}


void GameSettings::forgetAdminPassword(const string &serverName)
{
   iniFile.deleteKey("SavedAdminPasswords", serverName);
   iniFile.writeFile();
}


void GameSettings::forgetOwnerPassword(const string &serverName)
{
   iniFile.deleteKey("SavedOwnerPasswords", serverName);
   iniFile.writeFile();
}


Vector<string> *GameSettings::getLevelSkipList()
{
   return &mLevelSkipList;
}


InputCodeManager *GameSettings::getInputCodeManager()
{ 
   return &mInputCodeManager;
}


Vector<string> *GameSettings::getSpecifiedLevels()
{
   return &mCmdLineParams[CmdLineParams::LEVEL_LIST];
}


// This is the generic way to get a list of levels we'll be playing with, the one used in the ordinary course of events
Vector<string> GameSettings::getLevelList()
{
   return getLevelList(getFolderManager()->getLevelDir(), false);
}


// See what levels we can find in levelFolder; mainly used when remote user is changing level folder, and we want to see if it's valid
Vector<string> GameSettings::getLevelList(const string &levelFolder)
{
   return getLevelList(levelFolder, true);
}


// Create a list of levels for hosting a game, but does not read the files or do any validation of them
Vector<string> GameSettings::getLevelList(const string &levelDir, bool ignoreCmdLine)
{
   Vector<string> levelList;

   // If user specified a list of levels on the command line, use those, unless ignoreCmdLine was set to true
   if(!ignoreCmdLine && mCmdLineParams[CmdLineParams::LEVEL_LIST].size() > 0)
      levelList = mCmdLineParams[CmdLineParams::LEVEL_LIST];
   else  // Build our level list by looking at the filesystem 
      levelList = LevelSource::findAllLevelFilesInFolder(levelDir);


   // Now, remove any levels listed in the skip list from levelList.  Not foolproof!
   for(S32 i = 0; i < levelList.size(); i++)
   {
      // Make sure we have the right extension
      string filename_i = lcase(levelList[i]);
      if(filename_i.find(".level") == string::npos)
         filename_i += ".level";

      for(S32 j = 0; j < mLevelSkipList.size(); j++)
         if(filename_i == mLevelSkipList[j])
         {
            logprintf(LogConsumer::ServerFilter, "Loader skipping level %s listed in LevelSkipList (see INI file)", levelList[i].c_str());
            levelList.erase(i);
            i--;
            break;
         }
   }

   return levelList;
}


extern void exitToOs(S32 errcode);

static void parameterError(const char *errorMsg)
{
   printf("%s\n", errorMsg);
   exitToOs(1);
}


// Fills params Vector with the requisite number of param arguments.  Returns new position along
// the tokens in cmd line where we should continue parsing.
static S32 parseParams(ParamRequirements argsRequired, const S32 paramPtr, const S32 argPtr,
                     const S32 argc, const Vector<string> &argv, const char *errorMsg, Vector<string> &params)
{
   // Assume "args" starting with "-" are actually subsequent params
   bool hasAdditionalArg =                         (argPtr <= argc - 1 && argv[argPtr + 0][0] != '-');
   bool has2AdditionalArgs = hasAdditionalArg   && (argPtr <= argc - 2 && argv[argPtr + 1][0] != '-');
   bool has3AdditionalArgs = has2AdditionalArgs && (argPtr <= argc - 3 && argv[argPtr + 2][0] != '-');
   bool has4AdditionalArgs = has3AdditionalArgs && (argPtr <= argc - 4 && argv[argPtr + 3][0] != '-');

   if(argsRequired == NO_PARAMETERS)
   {
      params.push_back("true");     // Just so we know we encountered this param
      return argPtr;
   }
   else if(argsRequired == ONE_OPTIONAL)
   {
      if(hasAdditionalArg)
      {
         params.push_back(argv[argPtr]);
         return argPtr + 1;
      }
   }
   else if(argsRequired == ONE_REQUIRED)
   {
      if(!hasAdditionalArg)
         parameterError(errorMsg);

      params.push_back(argv[argPtr]);
      return argPtr + 1;
   }
   else if(argsRequired == TWO_REQUIRED)
   {
      if(!has2AdditionalArgs)
         parameterError(errorMsg);

      params.push_back(argv[argPtr]);
      params.push_back(argv[argPtr + 1]);
      return argPtr + 2;
   }
   else if(argsRequired == FOUR_REQUIRED)
   {
      if(!has4AdditionalArgs)
         parameterError(errorMsg);

      params.push_back(argv[argPtr]);
      params.push_back(argv[argPtr + 1]);
      params.push_back(argv[argPtr + 2]);
      params.push_back(argv[argPtr + 3]);
      return argPtr + 4;
   }
   else if(argsRequired == ALL_REMAINING)
   {
      if(!hasAdditionalArg)
         parameterError(errorMsg);

      for(S32 j = argPtr; j < argc; j++)
         params.push_back(argv[j]);

      return argc;
   }

   TNLAssert(false, "Unhandled argsRequired value!");
   return 0;
}



void GameSettings::readCmdLineParams(const Vector<string> &argv)
{
   S32 argc = argv.size();
   S32 argPtr = 0;

   while(argPtr < argc)
   {
      bool found = false;

      string arg = argv[argPtr];

      // Hack to turn double dashes into single dashes to be Linux-friendly
      if(arg.substr(0, 2) == "--")
         arg = arg.substr(1);

      argPtr++;      // Advance argPtr to location of first parameter argument

      // Mac adds on a 'Process Serial Number' to every application launched from a .app bundle
      // we should just ignore it and not exit the game
#ifdef TNL_OS_MAC_OSX
      if(arg.find("-psn") != string::npos)
      {
         printf("Ignoring cmd line parameter: %s\n", arg.c_str());
         continue;
      }
#endif

      // Scan through the possible parameters for a match to what is on the command line
      // This will fill out mCmdLineParams with any parameters found for the specific param id
      for(U32 i = 0; i < ARRAYSIZE(paramDefs); i++)
      {
         if(arg == "-" + paramDefs[i].paramName)
         {
            argPtr = parseParams(paramDefs[i].argsRequired, i, argPtr, argc, argv, paramDefs[i].errorMsg, mCmdLineParams[paramDefs[i].paramId]);

            found = true;
            break;
         }
      }

      // Didn't find a matching parameter... let's try the command directives
      if(!found)
      {
         for(U32 i = 0; i < ARRAYSIZE(directiveDefs); i++)
         {
            if(arg == "-" + directiveDefs[i].paramName)
            {
               argPtr = parseParams(directiveDefs[i].argsRequired, i, argPtr, argc, argv, directiveDefs[i].errorMsg, mCmdLineParams[directiveDefs[i].paramId]);

               found = true;
               break;
            }
         }
      }

      if(!found)
      {
         printf("Unknown cmd line parameter found: %s\n", arg.c_str());
         exit(1);
      }
   }

   
#ifdef ZAP_DEDICATED
   // Override some settings if we're compiling ZAP_DEDICATED
   mCmdLineParams[DEDICATED].push_back("true");
#endif
}


// If any directives were specified on the cmd line, run them
void GameSettings::runCmdLineDirectives()
{
   for(S32 i = 0; i < S32(ARRAYSIZE(directiveDefs)); i++)
   {
      if(mCmdLineParams[directiveDefs[i].paramId].size() > 0)
      {
         directiveDefs[i].cmdCallback(this, mCmdLineParams[directiveDefs[i].paramId]);    // Run the command
         exitToOs(0);                                                                     // Exit the game (in case the command itself doesn't)
      }
   }
}


// Now integrate INI settings with those from the command line and process them
// Should be run after INI and cmd line params have been read
void GameSettings::onFinishedLoading()
{
   string masterAddressList, cmdLineVal;
   Settings<IniKey::SettingsItem> &settings = mIniSettings.mSettings;

   // Some parameters can be specified both on the cmd line and in the INI... in those cases,
   // the cmd line version takes precedence.
   //
   // No command-line parameter should write to the INI unless otherwise specified
   //
   //                                First choice (cmdLine)             Second choice (INI)                  Third choice (fallback)
   mServerPassword         = *choose( getCmdLineParamString(SERVER_PASSWORD),       settings.getVal<string>(IniKey::ServerPassword) );

   mOwnerPassword          = *choose( getCmdLineParamString(OWNER_PASSWORD),        settings.getVal<string>(IniKey::OwnerPassword) );

   // Admin and level change passwords have special overrides that force them to be blank... handle those below
   if(isCmdLineParamSpecified(NO_ADMIN_PASSWORD))
      mAdminPassword = "";
   else
      mAdminPassword       = *choose( getCmdLineParamString(ADMIN_PASSWORD),        settings.getVal<string>(IniKey::AdminPassword) );

   if(isCmdLineParamSpecified(NO_LEVEL_CHANGE_PASSWORD))
      mLevelChangePassword = "";
   else
      mLevelChangePassword = *choose( getCmdLineParamString(LEVEL_CHANGE_PASSWORD), settings.getVal<string>(IniKey::LevelChangePassword) );


   mHostName               = *choose( getCmdLineParamString(HOST_NAME),             settings.getVal<string>(IniKey::ServerName) );
   mHostDescr              = *choose( getCmdLineParamString(HOST_DESCRIPTION),      settings.getVal<string>(IniKey::ServerDescription) );
   mWelcomeMessage         = mIniSettings.mSettings.getVal<string>(IniKey::WelcomeMessage);


   cmdLineVal = getCmdLineParamString(LOGIN_NAME);
   mPlayerNameSpecifiedOnCmdLine = (cmdLineVal!= "");

   //                                 Cmd Line value                    User must set manually in INI            Saved in INI based on last entry       
   mPlayerName             = *choose( cmdLineVal,                       getSetting<string>(IniKey::Nickname),    getSetting<string>(IniKey::LastName));
   mPlayerPassword         = *choose( getCmdLineParamString(LOGIN_PASSWORD), getSetting<string>(IniKey::Password), getSetting<string>(IniKey::LastPassword));

   cmdLineVal = getCmdLineParamString(MASTER_ADDRESS);
   mMasterServerSpecifiedOnCmdLine = (cmdLineVal != "");

   masterAddressList       = *choose( getCmdLineParamString(MASTER_ADDRESS), getSetting<string>(IniKey::MasterServerAddressList) );    // The INI will always have a value

   parseString(masterAddressList, mMasterServerList, ',');        // Move the list of master servers into mMasterServerList

   getFolderManager()->resolveLevelDir(this);                     // Figure out where the heck our levels are stored

   // If there is nothing in the INI, write a good default to the INI
   if(mIniSettings.mSettings.getVal<string>(IniKey::LevelDir) == "")    
      mIniSettings.mSettings.setVal(IniKey::LevelDir, getFolderManager()->getLevelDir());

   // Now we turn to the size and position of the game window
   // First, figure out what display mode to start in...
   DisplayMode cmdLineDisplayMode = resolveCmdLineSpecifiedDisplayMode();

   S32 xpos = S32_MIN, ypos = S32_MIN;

   // ...and where the window should be...
   if(mCmdLineParams[WINDOW_POS].size() > 0)
   {
      xpos = atoi(mCmdLineParams[WINDOW_POS].get(0).c_str());
      ypos = atoi(mCmdLineParams[WINDOW_POS].get(1).c_str());
   }

   // ... and finally, the window width (which in turns determines its height because the aspect ratio is fixed at 4:3)
   U32 winWidth = getCmdLineParamU32(WINDOW_WIDTH);

   // In all of these cases, if something was specified on the cmd line, write the result directly to the INI, clobbering whatever was there.
   // When we need the value, we'll get it from the INI.
   if(cmdLineDisplayMode != DISPLAY_MODE_UNKNOWN)
      setSetting(IniKey::WindowMode, cmdLineDisplayMode);

   if(xpos != S32_MIN)
      setWindowPosition(xpos, ypos);

   if(winWidth > 0)
   {
      F32 scalingFactor = max((F32) winWidth / (F32) DisplayManager::getScreenInfo()->getGameCanvasWidth(), DisplayManager::getScreenInfo()->getMinScalingFactor());
      setWindowSizeFactor(scalingFactor);
   }

#ifndef ZAP_DEDICATED
   U32 stick = getCmdLineParamU32(USE_STICK);
   if(stick > 0)
      UseControllerIndex = stick - 1;
#endif
}


// We need to show the name entry screen unless user has specified a nickname via the cmd line or the INI file
bool GameSettings::shouldShowNameEntryScreenOnStartup()
{
   return getCmdLineParamString(LOGIN_NAME) == "" && getSetting<string>(IniKey::Nickname) == "";
}


Vector<string> *GameSettings::getMasterServerList()
{
   return &mMasterServerList;
}


void GameSettings::saveMasterAddressListInIniUnlessItCameFromCmdLine()
{
   // If we got the master from the cmd line, or we only have one address, we have nothing to do
   if(mMasterServerSpecifiedOnCmdLine || mMasterServerList.size() < 2)
      return;

   // Otherwise write the master list to the INI file in their new order; the most recently successful address will now be first
   setSetting(IniKey::MasterServerAddressList, listToString(mMasterServerList, ","));
}


// Tries to figure out what display mode was specified on the cmd line, if any
DisplayMode GameSettings::resolveCmdLineSpecifiedDisplayMode() const
{
   if(isCmdLineParamSpecified(WINDOW_MODE))
      return DISPLAY_MODE_WINDOWED;

   if(isCmdLineParamSpecified(FULLSCREEN_MODE))
      return DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED;

   if(isCmdLineParamSpecified(FULLSCREEN_STRETCH))
      return DISPLAY_MODE_FULL_SCREEN_STRETCHED;

   return DISPLAY_MODE_UNKNOWN;
}


////////////////////////////////////////
////////////////////////////////////////
// Handle -setres and -getres commands

extern void transferResource(GameSettings *settings, const string &addr, const string &pw, const string &fileName, const string &resourceType, bool sending);

void GameSettings::getRes(GameSettings *settings, const Vector<string> &words)
{
   transferResource(settings, words[0], words[1], words[2], words[3], false);
}


void GameSettings::sendRes(GameSettings *settings, const Vector<string> &words)
{
   transferResource(settings, words[0], words[1], words[2], words[3], true);
}


////////////////////////////////////////
////////////////////////////////////////
// Dump rules with the -rules option

extern bool writeToConsole();
extern void printRules();

void GameSettings::showRules(GameSettings *settings, const Vector<string> &words)
{
   writeToConsole();
   printRules();
   exitToOs(0);
}


////////////////////////////////////////
////////////////////////////////////////
// Print help message with -help

static const S32 MAX_HELP_LINE_LEN = 110;


static string makeParamStr(const string &paramName, const string &paramString)
{
   return paramName + (paramString == "" ? "" : " ") + paramString;
}


static string makePad(U32 len)
{
   string padding = "";

   for(U32 i = 0; i < len; i++)
      padding += " ";

   return padding;
}


static std::size_t chunkStart;  // string::length returns size_t type which might be U64 for 64 bit systems.
static string chunkText;

// Return a chunk of text starting at start, with a max of len chars
// Used for word-wrapping help text
static string getChunk(U32 len)
{
   if(chunkStart >= chunkText.length())
      return "";

   // Advance chunkStart to position of first non-space; avoids leading spaces
   chunkStart += chunkText.substr(chunkStart, len + 1).find_first_not_of(' ');
               
   // Create a chunk of text, with the max length we have room for
   string chunk = chunkText.substr(chunkStart, len + 1);
               
   if(chunk.length() >= len)                                // If chunk would fill a full line...
      chunk = chunk.substr(0, chunk.find_last_of(' '));     // ...lop chunk off at last space

   chunkStart += chunk.length();

   return chunk;
}


static void resetChunker(const string &text)
{
   chunkText = trim(text);
   chunkStart = 0;
}


static void printHelpHeader(const S32 section, const bool firstInSection)
{
   if(firstInSection)     // First item in this docLevel... print the section header
   {
      printf("\n");
      string title = helpTitles[section];

      while(title.length())
      {
         printf("\n");

         // Have to use string::size_type here because U32 and string::npos don't
         // compare well on x86_64 machines
         string::size_type firstCR = title.find_first_of('\n');     // Grab a line of the title
         resetChunker(title.substr(0, firstCR));

         if(firstCR != string::npos)
            title = title.substr(firstCR + 1);
         else
            title = "";

         while(true)
         {
            string chunk = getChunk(MAX_HELP_LINE_LEN);

            if(!chunk.length())
               break;

            printf("%s\n", chunk.c_str());
         }
      }
   }
}


static void printHelpEntry(const string &paramName, const string &paramString, const string &helpString, S32 maxSize)
{
   string paramStr = makeParamStr(paramName, paramString);
   U32 paddingLen  = maxSize - (U32)paramStr.length();

   U32 wrapWidth = MAX_HELP_LINE_LEN - maxSize;

   resetChunker(trim(helpString));

   bool first = true;

   while(true)
   {
      string chunk = getChunk(wrapWidth);

      if(!chunk.length())
         break;

      if(first)
      {
         printf("\t-%s%s -- %s\n", paramStr.c_str(), makePad(paddingLen).c_str(),  chunk.c_str());
         first = false;
      }
      else
         printf("\t%s %s\n",                         makePad(maxSize + 4).c_str(), chunk.c_str());
   }
}


void GameSettings::showHelp(GameSettings *settings, const Vector<string> &words)
{
   // Header
   printf("Usage: %s [OPTIONS]\n\nRun %s, a 2-D multiplayer space-combat game.\n\nOptions:\n\n"
          "The following options can be specified on the command-line.",
          mExecutablePath.c_str(), ZAP_GAME_NAME);

   for(S32 i = 0; i < S32(ARRAYSIZE(helpTitles)); i++)
   {
      // Make an initial sweep through to check on the sizes of things, to ensure we get the indention right
      // This first chunk just determies the longest command to figure out how much padding is needed.
      U32 maxSize = 0;

      for(S32 j = 0; j < S32(ARRAYSIZE(paramDefs)); j++)
         if(paramDefs[j].docLevel == i)
         {
            U32 len = (U32) makeParamStr(paramDefs[j].paramName, paramDefs[j].paramString).length();

            if(len > maxSize)
               maxSize = len;
         }

      for(S32 j = 0; j < S32(ARRAYSIZE(directiveDefs)); j++)
         if(directiveDefs[j].docLevel == i)
         {
            U32 len = (U32)makeParamStr(directiveDefs[j].paramName, directiveDefs[j].paramString).length();

            if(len > maxSize)
               maxSize = len;
         }

      bool firstInSection = true;

      for(S32 j = 0; j < S32(ARRAYSIZE(paramDefs)); j++)
      {
         if(paramDefs[j].docLevel != i)
            continue;

         printHelpHeader(i, firstInSection);
         printHelpEntry(paramDefs[j].paramName, paramDefs[j].paramString, paramDefs[j].helpString, maxSize);

         firstInSection = false;
      }

      for(S32 j = 0; j < S32(ARRAYSIZE(directiveDefs)); j++)
      {
         if(directiveDefs[j].docLevel != i)
            continue;

         printHelpHeader(i, firstInSection);
         printHelpEntry(directiveDefs[j].paramName, directiveDefs[j].paramString, directiveDefs[j].helpString, maxSize);

         firstInSection = false;
      }
   }

   // Add some final notes...
   printf("\n\nNotes:\n\
   \t<param> denotes a required parameter\n\
   \t[param] denotes an optional parameter\n\
   \taddress is an address in the form ip address:port. (e.g. 192.168.1.55:25955)\n\
   \tstring means a parameter consisting of some combination of letters and numbers (e.g. Grambol_22).\n\
   \t   In many cases, spaces can be included by enclosing entire string in double quotes (\"Solid Gold Levels\").\n\
   \tinteger means an integer number must be specified (e.g. 4)\n\
   \tfloat means a floating point number must be specified (e.g. 3.5)\n");

   exitToOs(0);
}


void GameSettings::showVersion(GameSettings *settings, const Vector<string> &words)
{
#ifdef TNL_DEBUG
   printf("%s %s\nBuild: %d\nClient-Server protocol: %d\nMaster protocol: %d\n",
         ZAP_GAME_NAME, ZAP_GAME_RELEASE, BUILD_VERSION, CS_PROTOCOL_VERSION, MASTER_PROTOCOL_VERSION);
#else
   printf("%s %s\n", ZAP_GAME_NAME, ZAP_GAME_RELEASE);
#endif
}


Color GameSettings::getWallFillColor() const
{
   return mIniSettings.mSettings.getVal<Color>(IniKey::WallFillColor);
}


Color GameSettings::getWallOutlineColor() const
{
   return mIniSettings.mSettings.getVal<Color>(IniKey::WallOutlineColor);
}


// Accessor methods
U32 GameSettings::getSimulatedStutter()
{
   return getCmdLineParamU32(SIMULATED_STUTTER);
}


F32 GameSettings::getSimulatedLoss()
{
   return getCmdLineParamF32(SIMULATED_LOSS);
}


U32 GameSettings::getSimulatedLag()
{
   return min(getCmdLineParamU32(SIMULATED_LAG), (U32)1000);
}


// static method
void GameSettings::saveServerPassword(const string &serverName, const string &password)
{
   iniFile.setValue("SavedServerPasswords", serverName, password, true);
}


string GameSettings::getServerPassword(const string &serverName)
{
   return iniFile.getValue("SavedServerPasswords", serverName);
}


// static method
void GameSettings::deleteServerPassword(const string &serverName)
{
   iniFile.deleteKey("SavedServerPasswords", serverName);
}


bool GameSettings::isLevelOnSkipList(const string &filename) const
{
   for(S32 i = 0; i < mLevelSkipList.size(); i++)
      if(mLevelSkipList[i] == filename)    // Already on our list!
         return true;

   return false;
}


void GameSettings::addLevelToSkipList(const string &filename)
{
   mLevelSkipList.push_back(filename);
   saveSkipList();
}


void GameSettings::removeLevelFromSkipList(const string &filename)
{
   for(S32 i = 0; i < mLevelSkipList.size(); i++)
      if(mLevelSkipList[i] == filename)
      {
         mLevelSkipList.erase(i);
         saveSkipList();
         break;
      }
}


// Do we still need to do this at this point?  This will get done when INI is saved through regular channels...
void GameSettings::saveSkipList() const
{
   writeSkipList(&iniFile, &mLevelSkipList);  // Write skipped levels to INI
   iniFile.writeFile();                       // Save new INI settings to disk
}


InputMode GameSettings::getInputMode()
{
   return getInputCodeManager()->getInputMode();
}


void GameSettings::setShowingInGameHelp(bool show)
{
   mIniSettings.mSettings.setVal(IniKey::ShowInGameHelp, show ? Yes : No);
}


bool GameSettings::getShowingInGameHelp() const
{
   return mIniSettings.mSettings.getVal<YesNo>(IniKey::ShowInGameHelp);
}


bool GameSettings::getEditorShowConnectionsToMaster() const
{
   return mIniSettings.mSettings.getVal<YesNo>(IniKey::ShowConnectionsToMaster);
}


void GameSettings::setEditorShowConnectionsToMaster(bool show)
{
   mIniSettings.mSettings.setVal(IniKey::ShowConnectionsToMaster, show ? Yes : No);
}


// Returns pointer to inserted item
const UserSettings *GameSettings::addUserSettings(const UserSettings &userSettings)
{
   // Man oh man, is this ugly and confusing!
   return &mUserSettings.insert(pair<string,UserSettings>(userSettings.name, userSettings)).first->second;
}


// Retrieve settings for named user, create empty settings if we don't already have some
const UserSettings *GameSettings::getUserSettings(const string &name)
{
   UserSettingsMap::iterator i = mUserSettings.find(name);

   // If the settings don't already exist, create them here
   if(i == mUserSettings.end())    
   {
      UserSettings newSettings;
      newSettings.name = name;

      return addUserSettings(newSettings);
   }

   return &i->second;
}


void GameSettings::setIniSetting(const string &section, const string &key, const string &value)
{
   iniFile.setValue(section, key, value, true);
}

};

