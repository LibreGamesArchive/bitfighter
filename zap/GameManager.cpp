//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "GameManager.h"

#include "DisplayManager.h"
#include "FontManager.h"
#include "ServerGame.h"
#include "SoundSystem.h"
#include "VideoSystem.h"
#include "Console.h"

#ifndef ZAP_DEDICATED
#  include "UIErrorMessage.h"
#  include "UIManager.h"
#  include "ClientGame.h"
#  include "ClientInfo.h"
#endif

#ifndef BF_NO_CONSOLE
#  include "ConsoleLogConsumer.h"
#endif

namespace Zap
{

// Declare statics
ServerGame *GameManager::mServerGame = NULL;

#ifndef ZAP_DEDICATED
   Vector<ClientGame *> GameManager::mClientGames;
#endif

GameManager::HostingModePhase GameManager::mHostingModePhase = GameManager::NotHosting;
Console *GameManager::gameConsole = NULL;    // For the moment, we'll just have one console for everything.  This may change later, but probably won't.

#ifndef BF_NO_CONSOLE
static ConsoleLogConsumer *ConsoleLog;
#endif

// Constructor
GameManager::GameManager()
{
   gameConsole = new Console();
}


// Destructor
GameManager::~GameManager()
{
#ifndef BF_NO_CONSOLE
   delete gameConsole;
   gameConsole = NULL;

   delete ConsoleLog;
#endif
}


void GameManager::initialize()
{
#ifndef BF_NO_CONSOLE
   gameConsole->initialize();
   ConsoleLog = new ConsoleLogConsumer(gameConsole);  // Logs to our in-game console, when available
#endif
}


// All levels loaded, we're ready to go
bool GameManager::hostGame()
{
   TNLAssert(mServerGame, "Need a ServerGame to host, silly!");

   if(!mServerGame->startHosting())
   {
      abortHosting_noLevels();
      return false;
   }

#ifndef ZAP_DEDICATED
   const Vector<ClientGame *> *clientGames = GameManager::getClientGames();

   for(S32 i = 0; i < clientGames->size(); i++)
   {
      clientGames->get(i)->getUIManager()->disableLevelLoadDisplay(true);
      clientGames->get(i)->joinLocalGame(mServerGame->getNetInterface());  // ...then we'll play, too!
   }
#endif

   return true;
}


// If we can't load any levels, here's the plan...
void GameManager::abortHosting_noLevels()
{
   if(mServerGame->isDedicated())
   {
      FolderManager *folderManager = mServerGame->getSettings()->getFolderManager();
      const char *levelDir = folderManager->getLevelDir().c_str();

      logprintf(LogConsumer::LogError,     "No levels found in folder %s.  Cannot host a game.", levelDir);
      logprintf(LogConsumer::ServerFilter, "No levels found in folder %s.  Cannot host a game.", levelDir);
   }

#ifndef ZAP_DEDICATED
   const Vector<ClientGame *> *clientGames = getClientGames();

   for(S32 i = 0; i < clientGames->size(); i++)    // <<=== Should probably only display this message on the clientGame that initiated hosting
   {
      UIManager *uiManager = clientGames->get(i)->getUIManager();

      ErrorMessageUserInterface *errUI = uiManager->getUI<ErrorMessageUserInterface>();

      FolderManager *folderManager = mServerGame->getSettings()->getFolderManager();
      string levelDir = folderManager->getLevelDir();

      errUI->reset();
      errUI->setTitle("HOUSTON, WE HAVE A PROBLEM");
      errUI->setMessage("No levels were loaded.  Cannot host a game.  "
         "Check the LevelDir parameter in your INI file, "
         "or your command-line parameters to make sure "
         "you have correctly specified a folder containing "
         "valid level files.\n\n"
         "Trying to load levels from folder:\n" +
         (levelDir == "" ? string("<<Unresolvable>>") : levelDir));

      errUI->setInstr("Press [[Esc]] to continue");

      uiManager->activate<ErrorMessageUserInterface>();
      uiManager->disableLevelLoadDisplay(false);
   }

   if(clientGames->size() == 0)
#endif
      shutdownBitfighter();      // Quit in an orderly fashion... this function will never return

   deleteServerGame();
}


ServerGame *GameManager::getServerGame()
{
   return mServerGame;
}


void GameManager::setServerGame(ServerGame *serverGame)
{
   TNLAssert(serverGame, "Expect a valid serverGame here!");
   TNLAssert(!mServerGame, "Already have a ServerGame!");

   mServerGame = serverGame;
}


void GameManager::deleteServerGame()
{
   // mServerGame might be NULL here; for example when quitting after losing a connection to the game server
   delete mServerGame;     // Kill the serverGame (leaving the clients running)
   mServerGame = NULL;
}


void GameManager::idleServerGame(U32 timeDelta)
{
   if(mServerGame)
      mServerGame->idle(timeDelta);
}


// Used by tests to reset state to factory settings
void GameManager::reset()
{
   deleteServerGame();
   deleteClientGames();
}


/////

#ifndef ZAP_DEDICATED

// Called when user quits/returns to editor when playing game
// Code seems rather brutal to me, but that's the harsh reality of life in space
void GameManager::localClientQuits(ClientGame *game)
{
   game->closeConnectionToGameServer();
   deleteServerGame();
}


const Vector<ClientGame *> *GameManager::getClientGames()
{
   return &mClientGames;
}


void GameManager::deleteClientGames()
{
   mClientGames.deleteAndClear();
}


void GameManager::deleteClientGame(S32 index)
{
   mClientGames.deleteAndErase(index);
}


void GameManager::addClientGame(ClientGame *clientGame)
{
   mClientGames.push_back(clientGame);
}
#endif


void GameManager::idleClientGames(U32 timeDelta)
{
#ifndef ZAP_DEDICATED
   for(S32 i = 0; i < mClientGames.size(); i++)
      mClientGames[i]->idle(timeDelta);
#endif
}

void GameManager::idle(U32 timeDelta)
{
   idleServerGame(timeDelta);
   idleClientGames(timeDelta);
}


void GameManager::setHostingModePhase(HostingModePhase phase)
{
   mHostingModePhase = phase;
}


GameManager::HostingModePhase GameManager::getHostingModePhase()
{
   return mHostingModePhase;
}


// Returns the first GameSettings object we can find.  This is a method of desperation.
GameSettings *GameManager::getAnyGameSettings()
{
#ifndef ZAP_DEDICATED
   if(getClientGames()->size() > 0)
      return getClientGames()->get(0)->getSettings();
#endif

   if(getServerGame())
      return getServerGame()->getSettings();

   TNLAssert(false, "Who am I, and why am I here?");     // Bonus points if you know who said this!

   return NULL;      // Should never happen
}


extern void exitToOs();

// Run when we're quitting the game, returning to the OS.  Saves settings and does some final cleanup to keep things orderly.
// There are currently only 6 ways to get here (i.e. 6 legitimate ways to exit Bitfighter): 
// 1) Hit escape during initial name entry screen
// 2) Hit escape from the main menu
// 3) Choose Quit from main menu
// 4) Host a game with no levels as a dedicated server
// 5) Admin issues a shutdown command to a remote dedicated server
// 6) Click the X on the window to close the game window   <=== NOTE: This scenario fails for me when running a dedicated server on windows.
//
// And two illigitimate ways
// 7) Lua panics!!
// 8) Video system fails to initialize
void GameManager::shutdownBitfighter()
{
   GameSettings *settings = NULL;

   // Avoid this function being called twice when we exit via methods 1-4 above
#ifndef ZAP_DEDICATED
   if(getClientGames()->size() == 0)
#endif
      exitToOs();

   // Grab a pointer to settings wherever we can.  Note that all Games (client or server) currently refer to the same settings object.
   // This is no longer quite true -- now they point to identical objects, but ones that can be configured independently for
   // testing purposes.
#ifndef ZAP_DEDICATED
   settings = getClientGames()->get(0)->getSettings();
   deleteClientGames();
#endif

   if(getServerGame())
   {
      settings = getServerGame()->getSettings();
      deleteServerGame();
   }


   TNLAssert(settings, "Should always have a value here!");

   EventManager::shutdown();
   LuaScriptRunner::shutdown();
   SoundSystem::shutdown();

   if(!settings->isDedicatedServer())
   {
#ifndef ZAP_DEDICATED
      Joystick::shutdownJoystick();

      // Save current window position if in windowed mode
      if(settings->getSetting<DisplayMode>(IniKey::WindowMode) == DISPLAY_MODE_WINDOWED)
         settings->setWindowPosition(VideoSystem::getWindowPositionX(), VideoSystem::getWindowPositionY());

      SDL_QuitSubSystem(SDL_INIT_VIDEO);

      FontManager::cleanup();
      RenderManager::shutdown();
#endif
   }

#ifndef BF_NO_CONSOLE
   // Avoids annoying shutdown crashes when logging is still trying to output to oglconsole
   ConsoleLog->setMsgTypes(LogConsumer::LogNone);
#endif

   settings->save();                                  // Write settings to bitfighter.ini

   delete settings;

   DisplayManager::cleanup();

   NetClassRep::logBitUsage();
   logprintf("Bye!");

   exitToOs();    // Do not pass Go
}


} 
