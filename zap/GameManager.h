//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _GAME_MANAGER_H_
#define _GAME_MANAGER_H_

#include "tnlVector.h"
#include "Console.h"

using namespace TNL;

namespace Zap
{
   class GameSettings;
   class ServerGame;

#ifndef ZAP_DEDICATED
   class ClientGame;
#endif

// Singleton class for keeping track of various Game objects
class GameManager
{
public:
   enum HostingModePhase
   {
      NotHosting,
      LoadingLevels,
      DoneLoadingLevels,
      Hosting
   };

private:
   static ServerGame *mServerGame;
#ifndef ZAP_DEDICATED
   static Vector<ClientGame *> mClientGames;
#endif

   static HostingModePhase mHostingModePhase;

public:
   static ::Console *gameConsole;

   GameManager();
   virtual ~GameManager();

   static void initialize();
   static void shutdownBitfighter();

   static bool hostGame(); 
   static void abortHosting_noLevels();

   // ServerGame related
   static void setServerGame(ServerGame *serverGame);
   static ServerGame *getServerGame();
   static void deleteServerGame();
   static void idleServerGame(U32 timeDelta);

   static void reset();    // Only used by testing


   // ClientGame related
#ifndef ZAP_DEDICATED
   static const Vector<ClientGame *> *getClientGames();
   static void addClientGame(ClientGame *clientGame);
   static void deleteClientGame(S32 index);     // Delete specified game
   static void deleteClientGames();             // Delete all games

   static void localClientQuits(ClientGame *game);
#endif

   static void idleClientGames(U32 timeDelta);

   // Other
   static void idle(U32 timeDelta);
   static void setHostingModePhase(HostingModePhase);
   static HostingModePhase getHostingModePhase();

   static GameSettings *getAnyGameSettings();
};

} 

#endif 
