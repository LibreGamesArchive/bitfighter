//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "UIEditor.h"

#include "UIEditorInstructions.h"
#include "UIEditorMenu.h"
#include "UIErrorMessage.h"
#include "UIGameParameters.h"
#include "UIManager.h"
#include "UINameEntry.h"         // For LevelnameEntryUI
#include "UIQuickMenu.h"         // For access to menu methods such as setObject
#include "UITeamDefMenu.h"

#include "barrier.h"             // For DEFAULT_BARRIER_WIDTH
#include "ClientGame.h"  
#include "Colors.h"
#include "config.h"
#include "CoreGame.h"            // For CoreItem def
#include "Cursor.h"              // For various editor cursor
#include "EditorTeam.h"
#include "EditorWorkUnit.h"
#include "EngineeredItem.h"      // For Turret properties
#include "FileList.h"
#include "goalZone.h"
#include "Intervals.h"
#include "LevelSource.h"
#include "LineItem.h"
#include "loadoutZone.h"         // For LoadoutZone def
#include "NexusGame.h"           // For NexusZone def
#include "PickupItem.h"          // For RepairItem
#include "PolyWall.h"
#include "projectile.h"
#include "ship.h"
#include "soccerGame.h"          // For Soccer ball radius
#include "Spawn.h"
#include "speedZone.h"           // For Speedzone def
#include "Teleporter.h"          // For Teleporter def
#include "TextItem.h"            // For MAX_TEXTITEM_LEN and MAX_TEXT_SIZE
#include "VertexStylesEnum.h"
#include "WallItem.h"

#include "DisplayManager.h"
#include "GameManager.h"
#include "GameObjectRender.h"
#include "LevelDatabaseUploadThread.h"
#include "luaLevelGenerator.h"
#include "SystemFunctions.h"
#include "VideoSystem.h"

#include "FontManager.h"
#include "GeomUtils.h"
#include "RenderUtils.h"
#include "ScreenShooter.h"
#include "stringUtils.h"

#include <cmath>
#include <set>
#include "MathUtils.h"


namespace Zap
{

// Dock widths in pixels
const S32 ITEMS_DOCK_WIDTH = 50;
//const S32 PLUGINS_DOCK_WIDTH = 150;
const U32 PLUGIN_LINE_SPACING = 20;

const F32 MIN_SCALE = .02f;         // Most zoomed-out scale
const F32 MAX_SCALE = 10.0f;        // Most zoomed-in scale
const F32 STARTING_SCALE = 0.5;

static Level *mLoadTarget;

const string EditorUserInterface::UnnamedFile = "unnamed_file";      // When a file has no name, this is its name!


S32 QSORT_CALLBACK pluginInfoSort(PluginInfo *a, PluginInfo *b)
{
   return stricmp((a)->prettyName.c_str(), (b)->prettyName.c_str());
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
PluginInfo::PluginInfo(string prettyName, string fileName, string description, string requestedBinding) :
   prettyName(prettyName), fileName(fileName), description(description), requestedBinding(requestedBinding)
{
   bindingCollision = false;
}


////////////////////////////////////////
////////////////////////////////////////


// Constructor
EditorUserInterface::EditorUserInterface(ClientGame *game, UIManager *uiManager) :
   Parent(game, uiManager),
   mChatMessageDisplayer(game, 5, true, 500, 12, 3)     // msgCount, topDown, wrapWidth, fontSize, fontGap
{
   mWasTesting = false;
   mIgnoreMouseInput = false;

   clearSnapEnvironment();
   mCurrentScale = STARTING_SCALE;

   mHitItem = NULL;
   mNewItem = NULL;
   mDockItemHit = NULL;
   mDockWidth = ITEMS_DOCK_WIDTH;
   mDockMode = DOCKMODE_ITEMS;
   mDockPluginScrollOffset = 0;
   mCurrentTeam = 0;

   mPointOfRotation = NULL;

   mHitVertex = NONE;
   mEdgeHit = NONE;

   mAutoScrollWithMouse = false;
   mAutoScrollWithMouseReady = false;

   mAddingVertex = false;

   mPreviewMode = false;
   mNormalizedScreenshotMode = false;

   mSaveMsgTimer.setPeriod(FIVE_SECONDS);

   mGridSize = mGameSettings->getSetting<U32>(IniKey::GridSize);

   mQuitLocked = false;
   mVertexEditMode = true;
   mDraggingObjects = false;
}


Level *EditorUserInterface::getLevel() const
{
   return mLevel.get();
}


F32 EditorUserInterface::getGridSize() const
{
   return (F32)mGridSize;
}


void EditorUserInterface::geomChanged(BfObject *obj)
{
   obj->onGeomChanged();

   if(isWallType(obj->getObjectTypeNumber()))
      rebuildWallGeometry(mLevel.get());
}


void EditorUserInterface::setLevel(const boost::shared_ptr<Level> &level)
{
   TNLAssert(level.get(), "Level should not be NULL!");
   mLevel = level;

   //getGame()->setLevel(level);
   mUndoManager.setLevel(level, this);

   // Do some special preparations for walls/polywalls -- the editor needs to know about wall edges and such
   // for hit detection and mounting turrets/ffs
   //const Vector<DatabaseObject *> *walls = level->findObjects_fast(WallItemTypeNumber);

   //for(S32 i = 0; i < walls->size(); i++)
   //{
   //   const WallItem *wall = static_cast<WallItem *>(walls->get(i));
   //   //addToEditor(wall);
   //   //wa
   //}

   // Tell mDockItems to use the same team info as we use for the regular items
   mDockItems.setTeamManager(mLevel->getTeamManager());
}


// Really quitting... no going back!
void EditorUserInterface::onQuitted()
{
   cleanUp();
   mLevel.reset();    // reset mLevel
}


void EditorUserInterface::onClientConnectedToMaster(StringTableEntry playerNick)
{
   if(!mGameSettings->getEditorShowConnectionsToMaster())
      return;

   string message = string(playerNick.getString()) + " connected to the master server";
   mChatMessageDisplayer.onChatMessageReceived(Colors::green50, message);
}


// Re-read any settings set in the Editor Options menu
void EditorUserInterface::updateSettings()
{
   mGridSize = mGameSettings->getSetting<U32>(IniKey::GridSize);
}


void EditorUserInterface::addDockObject(BfObject *object, F32 xPos, F32 yPos)
{
   object->prepareForDock(Point(xPos, yPos), mCurrentTeam);  // Prepare object   
   mDockItems.addToDatabase(object);
}


void EditorUserInterface::populateDock()
{
   // Out with the old
   mDockItems.clearAllObjects();
   GameTypeId gameTypeId = mLevel->getGameType()->getGameTypeId();

   F32 xPos = (F32)DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin - ITEMS_DOCK_WIDTH / 2;
   F32 yPos = 35;
   const F32 spacer = 35;

   addDockObject(new RepairItem(), xPos, yPos);
   //addDockObject(new ItemEnergy(), xPos + 10, yPos);
   yPos += spacer;

   addDockObject(new Spawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new ForceFieldProjector(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Turret(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Mortar(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Teleporter(), xPos, yPos);
   yPos += spacer;

   addDockObject(new SpeedZone(), xPos, yPos);
   yPos += spacer;

   addDockObject(new TextItem(), xPos, yPos);
   yPos += spacer;

   if(gameTypeId == SoccerGame)
      addDockObject(new SoccerBallItem(), xPos, yPos);
   else if(gameTypeId == CoreGame)
      addDockObject(new CoreItem(), xPos, yPos);
   else
      addDockObject(new FlagItem(), xPos, yPos);
   yPos += spacer;

   addDockObject(new FlagSpawn(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Mine(), xPos - 10, yPos);
   addDockObject(new SpyBug(), xPos + 10, yPos);
   yPos += spacer;

   // These two will share a line
   addDockObject(new Asteroid(), xPos - 10, yPos);
   addDockObject(new AsteroidSpawn(), xPos + 10, yPos);
   yPos += spacer;

   //addDockObject(new CircleSpawn(), xPos - 10, yPos);
   //   addDockObject(new Core(), xPos /*+ 10*/, yPos);
   //   yPos += spacer;


   // These two will share a line
   addDockObject(new TestItem(), xPos - 10, yPos);
   addDockObject(new ResourceItem(), xPos + 10, yPos);
   yPos += 25;


   addDockObject(new LoadoutZone(), xPos, yPos);
   yPos += 25;

   if(gameTypeId == NexusGame)
   {
      addDockObject(new NexusZone(), xPos, yPos);
      yPos += 25;
   }
   else
   {
      addDockObject(new GoalZone(), xPos, yPos);
      yPos += 25;
   }

   addDockObject(new PolyWall(), xPos, yPos);
   yPos += spacer;

   addDockObject(new Zone(), xPos, yPos);
   yPos += spacer;

}


// Destructor -- unwind things in an orderly fashion.  Note that mLevelGenDatabase will clear itself as the referenced object is deleted.
EditorUserInterface::~EditorUserInterface()
{
   mClipboard.clear();

   delete mNewItem.getPointer();
   delete mPointOfRotation;
}


void EditorUserInterface::clearSnapEnvironment()
{
   mSnapObject = NULL;
   mSnapVertexIndex = NONE;
}


void EditorUserInterface::undo(bool addToRedoStack)
{
   if(!mUndoManager.undoAvailable())
      return;

   clearSnapEnvironment();

   mUndoManager.undo();


   rebuildEverything(getLevel());    // Well, rebuild segments from walls at least

   onSelectionChanged();

   validateLevel();
}


void EditorUserInterface::redo()
{
   if(!mUndoManager.redoAvailable())
      return;

   clearSnapEnvironment();

   mUndoManager.redo();
   rebuildEverything(getLevel());    // Well, rebuild segments from walls at least

   onSelectionChanged();

   validateLevel();
}


void EditorUserInterface::rebuildWallGeometry(Level *level)
{
   level->buildWallEdgeGeometry(mWallEdgePoints);     // Populates mWallEdgePoints
   rebuildSelectionOutline();

   level->snapAllEngineeredItems(false);
}


void EditorUserInterface::rebuildEverything(Level *level)
{
   rebuildWallGeometry(level);

   // If we're rebuilding items in our levelgen database, no need to save anything!
   if(level != &mLevelGenDatabase)
      autoSave();
}


void EditorUserInterface::setLevelFileName(const string &name)
{
   mEditFileName = name;
   if(name != "" && name.find('.') == string::npos)      // Append extension, if one is needed
      mEditFileName = name + ".level";
}


void EditorUserInterface::cleanUp()
{
   getGame()->resetRatings();       // Move to mLevel?

   mUndoManager.clearAll();         // Clear up a little memory
   mDockItems.clearAllObjects();    // Free a little more -- dock will be rebuilt when editor restarts

   mLoadTarget = getLevel();

   mRobotLines.clear();             // Clear our special Robot lines

   mLevel->clearTeams();

   clearSnapEnvironment();

   mAddingVertex = false;
   clearLevelGenItems();
   mGameTypeArgs.clear();
}


// Loads a level
void EditorUserInterface::loadLevel()
{
   string filename = getLevelFileName();
   TNLAssert(filename != "", "Need file name here!");

   // Only clean up if we've got a level to clean up!
   if(mLevel)
      cleanUp();

   FolderManager *folderManager = mGameSettings->getFolderManager();
   string fileName = joindir(folderManager->getLevelDir(), filename).c_str();


   // Process level file --> returns true if file found and loaded, false if not (assume it's a new level)

   Level *level = new Level();
   bool isNewLevel = !level->loadLevelFromFile(fileName);   // load returns true if fileName exists, false if not

   setLevel(boost::shared_ptr<Level>(level));

   mLoadTarget = level;

   // Make sure all our ForceFieldProjectors have dummy forcefields for rendering purposes
   const Vector<DatabaseObject *> *objects = level->findObjects_fast();

   for(S32 i = 0; i < objects->size(); i++)
      static_cast<BfObject *>(objects->get(i))->onAddedToEditor();

   TNLAssert(mLevel->getGameType(), "Level should have GameType!");
   TNLAssert(mLevel->getTeamCount() > 0, "Level should have at least one team!");

   if(isNewLevel)
      mLevel->getGameType()->setLevelCredits(getGame()->getClientInfo()->getName());  // Set default author
   else
   {
      // Loaded a level!
      validateTeams();                 // Make sure every item has a valid team
      validateLevel();                 // Check level for errors (like too few spawns)
   }

   //// If we have a level in the database, let's ping the database to make sure it's really still there
   //if(game->isLevelInDatabase() && game->getConnectionToMaster())
   //{
   //   game->getConnectionToMaster()->c2mRequestLevelRating(getLevelDatabaseId());
   //}

   clearSelection(mLoadTarget);        // Nothing starts selected
   populateDock();                     // Add game-specific items to the dock

   mUndoManager.clearAll();

   // Bulk-process new items, walls first
   rebuildWallGeometry(mLevel.get());

   // Snap all engineered items to the closest wall, if one is found
   mLoadTarget->snapAllEngineeredItems(false);
}


void EditorUserInterface::clearLevelGenItems()
{
   mLevelGenDatabase.clearAllObjects();
}


void EditorUserInterface::copyScriptItemsToEditor()
{
   if(mLevelGenDatabase.getObjectCount() == 0)
      return;

   // Duplicate EditorObject pointer list to avoid unsynchronized loop removal
   Vector<DatabaseObject *> tempList(*mLevelGenDatabase.findObjects_fast());

   mUndoManager.startTransaction();

   // We can't call addToEditor immediately because it calls addToGame which will trigger
   // an assert since the levelGen items are already added to the game.  We must therefore
   // remove them from the game first
   for(S32 i = 0; i < tempList.size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(tempList[i]);

      obj->removeFromGame(false);     // False ==> remove, but do not delete object
      addToEditor(obj);
      mUndoManager.saveAction(ActionCreate, obj);
   }

   mUndoManager.endTransaction();

   mLevelGenDatabase.clearAllObjects();   // This will delete objects... is that what we want?

   rebuildEverything(getLevel());
}


void EditorUserInterface::addToEditor(BfObject *obj)
{
   obj->addToDatabase(mLevel.get());
   obj->onAddedToEditor();
   geomChanged(obj);                   // Easy way to get PolyWalls to build themselves after being dragged from the dock
}


// User has pressed Ctrl+K -- run the levelgen script and insert any resulting items into the editor in a separate database
void EditorUserInterface::runLevelGenScript()
{
   GameType *gameType = mLevel->getGameType();
   string scriptName = gameType->getScriptName();

   if(scriptName == "")      // No script included!!
      return;

   const Vector<string> *scriptArgs = gameType->getScriptArgs();  // As entered on game params menu

   clearLevelGenItems();                                          // Clear out any items from the last run

   logprintf(LogConsumer::ConsoleMsg, "Running script %s", gameType->getScriptLine().c_str());
   runScript(&mLevelGenDatabase, mGameSettings->getFolderManager(), scriptName, *scriptArgs);
}


// game is an unused parameter needed to make the method fit the signature of the callbacks used by UIMenus
static void openConsole(ClientGame *game)
{
   if(GameManager::gameConsole->isOk())
   {
      GameManager::gameConsole->show();
      return;
   }
   // else show error message  <== TODO DO ThiS!
}


// Runs an arbitrary lua script.  Command is first item in cmdAndArgs, subsequent items are the args, if any
void EditorUserInterface::runScript(Level *level, const FolderManager *folderManager,
   const string &scriptName, const Vector<string> &args)
{
   string name = folderManager->findLevelGenScript(scriptName);  // Find full name of levelgen script

   if(name == "")
   {
      logprintf(LogConsumer::ConsoleMsg, "Could not find script %s; looked in folders: %s",
         scriptName.c_str(), concatenate(folderManager->getScriptFolderList()).c_str());
      return;
   }

   // Load the items
   LuaLevelGenerator levelGen(getGame(), name, args, level);

   // Error reporting handled within -- we won't cache these scripts for easier development   
   bool error = !levelGen.runScript(false);

   if(error)
   {
      ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle("SCRIPT ERROR");

#ifndef BF_NO_CONSOLE
      ui->setMessage("The levelgen script you ran encountered an error.\n\n"
         "See the console (press [[/]]) or the logfile for details.");
#else
      ui->setMessage("The levelgen script you ran encountered an error.\n\n"
         "See the logfile for details.");
#endif

      ui->setInstr("Press [[Esc]] to return to the editor");

      ui->registerKey(KEY_SLASH, &openConsole);
      getUIManager()->activate(ui);
   }

   // Even if we had an error, continue on so we can process what does work -- this will make it more consistent with how the script will 
   // perform in-game.  Though perhaps we should show an error to the user...


   // Process new items that need it (walls need processing so that they can render properly).
   // Items that need no extra processing will be kept as-is.
   fillVector.clear();
   level->findObjects((TestFunc)isWallType, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

      if(obj->getVertCount() < 2)      // Invalid item; delete  --> aren't 1 point walls already excluded, making this check redundant?
         level->removeFromDatabase(obj, true);

      if(obj->getObjectTypeNumber() == WallItemTypeNumber)
         static_cast<WallItem *>(obj)->computeExtendedEndPoints();
   }

   // Also find any teleporters and make sure their destinations are in order.  Teleporters with no dests will be deleted.
   // Those with multiple dests will be broken down into multiple single dest versions.
   fillVector.clear();
   level->findObjects(TeleporterTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Teleporter *teleporter = static_cast<Teleporter *>(fillVector[i]);
      if(teleporter->getDestCount() == 0)
         level->removeFromDatabase(teleporter, true);
      else
      {
         for(S32 i = 1; i < teleporter->getDestCount(); i++)
         {
            Teleporter *newTel = new Teleporter;
            newTel->setPos(teleporter->getPos());
            newTel->setEndpoint(teleporter->getDest(i));
            newTel->addDest(teleporter->getDest(i));

            //newTel->addToGame(getGame(), level);
            newTel->addToDatabase(mLevel.get());
         }

         // Delete any destinations past the first one
         for(S32 i = 1; i < teleporter->getDestCount(); i++)
            teleporter->delDest(i);
      }
   }

   rebuildEverything(level);
}


void EditorUserInterface::showPluginError(const string &msg)
{
   Vector<string> messages;
   messages.push_back("Problem With Plugin");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("This plugin encountered an error " + msg + ".\n"
      "It has probably been misconfigured.\n\n"

#ifndef BF_NO_CONSOLE
      "See the Bitfighter logfile or console ([[/]]) for details.");
#else
      "See the Bitfighter logfile for details.");
#endif

   mMessageBoxQueue.push_back(messages);
}


// Try to create some sort of unique-ish signature for the plugin
string EditorUserInterface::getPluginSignature()
{
   string key = mPluginRunner->getScriptName();

   if(mPluginMenu)
      for(S32 i = 0; i < mPluginMenu->getMenuItemCount(); i++)
      {
         MenuItem *menuItem = mPluginMenu->getMenuItem(i);
         key += itos(menuItem->getItemType()) + "-";
      }

   return key;
}


void EditorUserInterface::runPlugin(const FolderManager *folderManager, const string &scriptName, const Vector<string> &args)
{
   string fullName = folderManager->findPlugin(scriptName);     // Find full name of plugin script

   if(fullName == "")
   {
      showCouldNotFindScriptMessage(scriptName);
      return;
   }

   // Create new plugin, will be deleted by boost
   EditorPlugin *plugin = new EditorPlugin(fullName, args, mLoadTarget, getGame());

   mPluginRunner = boost::shared_ptr<EditorPlugin>(plugin);

   // Loads the script and runs it to get everything loaded into memory.  Does not run main().
   // We won't cache scripts here because the performance impact should be relatively small, and it will
   // make it easier to develop them.  If circumstances change, we might want to start caching.
   if(!mPluginRunner->prepareEnvironment() || !mPluginRunner->loadScript(false))
   {
      showPluginError("during loading");
      mPluginRunner.reset();
      return;
   }

   string title;
   Vector<boost::shared_ptr<MenuItem> > menuItems;

   bool error = plugin->runGetArgsMenu(title, menuItems);   // Fills menuItems

   if(error)
   {
      showPluginError("configuring its options menu.");
      mPluginRunner.reset();
      return;
   }

   if(menuItems.size() == 0)
   {
      onPluginExecuted(Vector<string>());        // No menu items?  Let's run the script directly!
      mPluginRunner.reset();
      return;
   }

   // There are menu items!
   // Build a menu from the menuItems returned by the plugin
   mPluginMenu.reset(new PluginMenuUI(getGame(), getUIManager(), title));  // Use smart pointer for auto cleanup

   for(S32 i = 0; i < menuItems.size(); i++)
      mPluginMenu->addWrappedMenuItem(menuItems[i]);

   mPluginMenu->addSaveAndQuitMenuItem("Run plugin", "Saves values and runs plugin");

   mPluginMenu->setMenuCenterPoint(Point(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2,
      DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2));

   // Restore previous values, if available
   string key = getPluginSignature();

   if(mPluginMenuValues.count(key) == 1)    // i.e. the key exists; use count to avoid creating new entry if it does not exist
      for(S32 i = 0; i < mPluginMenuValues[key].size(); i++)
         mPluginMenu->getMenuItem(i)->setValue(mPluginMenuValues[key].get(i));

   getUIManager()->activate(mPluginMenu.get());
}


void EditorUserInterface::onPluginExecuted(const Vector<string> &args)
{
   TNLAssert(mPluginRunner, "NULL PluginRunner!");

   // Save menu values for next time -- using a key that includes both the script name and the type of menu items
   // provides some protection against the script being changed while Bitfighter is running.  Probably not realy
   // necessary, but we can afford it here.
   string key = getPluginSignature();

   mPluginMenuValues[key] = args;

   if(!mPluginRunner->runMain(args))
      setSaveMessage("Plugin Error: press [/] for details", false);

   rebuildEverything(getLevel());
   findSnapVertex();

   mPluginRunner.reset();
}


void EditorUserInterface::showCouldNotFindScriptMessage(const string &scriptName)
{
   Vector<string> pluginDirs = mGameSettings->getFolderManager()->getPluginDirs();

   Vector<string> messages;
   messages.push_back("Plugin not Found");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("Could not find the plugin called " + scriptName + "\n"
      "I looked in these folders: " + listToString(pluginDirs, "; ") + ".\n\n"
      "You likely have a typo in the [EditorPlugins] section of your INI file.");

   mMessageBoxQueue.push_back(messages);
}


void EditorUserInterface::showUploadErrorMessage(S32 errorCode, const string &errorBody)
{
   Vector<string> messages;
   messages.push_back("Error Uploading Level");
   messages.push_back("Press [[Esc]] to return to the editor");

   messages.push_back("Error uploading level.\n\nServer responded with error code " + itos(errorCode) + "." +
      (errorBody != "" ? "\n\n\"" + errorBody + "\"" : ""));

   mMessageBoxQueue.push_back(messages);
}


static bool TeamListToString(string &output, Vector<bool> teamVector)
{
   string teamList;
   bool hasError = false;
   char buf[16];

   // Make sure each team has a spawn point
   for(S32 i = 0; i < (S32)teamVector.size(); i++)
      if(!teamVector[i])
      {
         dSprintf(buf, sizeof(buf), "%d", i + 1);

         if(!hasError)     // This is our first error
         {
            output = "team ";
            teamList = buf;
         }
         else
         {
            output = "teams ";
            teamList += ", ";
            teamList += buf;
         }
         hasError = true;
      }
   if(hasError)
   {
      output += teamList;
      return true;
   }
   return false;
}


static bool hasTeamFlags(GridDatabase *database)
{
   const Vector<DatabaseObject *> *flags = database->findObjects_fast(FlagTypeNumber);

   for(S32 i = 0; i < flags->size(); i++)
      if(static_cast<FlagItem *>(flags->get(i))->getTeam() > TEAM_NEUTRAL)
         return true;

   return false;
}


static void setTeam(BfObject *obj, S32 team)
{
   obj->setTeam(team);

   // Set default health based on team for display purposes
   if(isEngineeredType(obj->getObjectTypeNumber()))
      static_cast<EngineeredItem *>(obj)->setHealth((team == TEAM_NEUTRAL) ? 0.0f : 1.0f);
}


static bool hasTeamSpawns(GridDatabase *database)
{
   fillVector.clear();
   database->findObjects(FlagSpawnTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
      if(dynamic_cast<FlagSpawn *>(fillVector[i])->getTeam() >= 0)
         return true;

   return false;
}


void EditorUserInterface::validateLevel()
{
   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   bool foundNeutralSpawn = false;

   Vector<bool> foundSpawn;

   string teamList;

   // First, catalog items in level
   S32 teamCount = getTeamCount();
   foundSpawn.resize(teamCount);

   for(S32 i = 0; i < teamCount; i++)      // Initialize vector
      foundSpawn[i] = false;

   Level *level = getLevel();

   fillVector.clear();
   level->findObjects(ShipSpawnTypeNumber, fillVector);

   for(S32 i = 0; i < fillVector.size(); i++)
   {
      Spawn *spawn = static_cast<Spawn *>(fillVector[i]);
      const S32 team = spawn->getTeam();

      if(team == TEAM_NEUTRAL)
         foundNeutralSpawn = true;
      else if(team > TEAM_NEUTRAL && team < teamCount)
         foundSpawn[team] = true;
   }

   bool foundSoccerBall = level->hasObjectOfType(SoccerBallItemTypeNumber);
   bool foundNexus = level->hasObjectOfType(NexusTypeNumber);
   bool foundFlags = level->hasObjectOfType(FlagTypeNumber);

   bool foundTeamFlags = hasTeamFlags(level);
   bool foundTeamFlagSpawns = hasTeamSpawns(level);

   // "Unversal errors" -- levelgens can't (yet) change gametype

   GameType *gameType = mLevel->getGameType();
   GameTypeId gameTypeId = mLevel->getGameType()->getGameTypeId();

   // Check for soccer ball in a a game other than SoccerGameType. Doesn't crash no more.
   if(foundSoccerBall && gameTypeId != SoccerGame)
      mLevelWarnings.push_back("WARNING: Soccer ball can only be used in soccer game.");

   // Check for the nexus object in a non-hunter game. Does not affect gameplay in non-hunter game.
   if(foundNexus && gameTypeId != NexusGame)
      mLevelWarnings.push_back("WARNING: Nexus object can only be used in Nexus game.");

   // Check for missing nexus object in a hunter game.  This cause mucho dolor!
   if(!foundNexus && gameTypeId == NexusGame)
      mLevelErrorMsgs.push_back("ERROR: Nexus game must have a Nexus.");

   if(foundFlags && !gameType->isFlagGame())
      mLevelWarnings.push_back("WARNING: This game type does not use flags.");

   // Check for team flag spawns on games with no team flags
   if(foundTeamFlagSpawns && !foundTeamFlags)
      mLevelWarnings.push_back("WARNING: Found team flag spawns but no team flags.");

   // Errors that may be corrected by levelgen -- script could add spawns
   // Neutral spawns work for all; if there's one, then that will satisfy our need for spawns for all teams
   if(mLevel->getGameType()->getScriptName() == "" && !foundNeutralSpawn)
   {
      if(TeamListToString(teamList, foundSpawn))     // Compose error message
         mLevelErrorMsgs.push_back("ERROR: Need spawn point for " + teamList);
   }

   if(gameTypeId == CoreGame)
   {
      for(S32 i = 0; i < teamCount; i++)      // Initialize vector
         foundSpawn[i] = false;

      fillVector.clear();
      level->findObjects(CoreTypeNumber, fillVector);
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         CoreItem *core = static_cast<CoreItem *>(fillVector[i]);
         const S32 team = core->getTeam();
         if(U32(team)< U32(foundSpawn.size()))
            foundSpawn[team] = true;
      }
      if(TeamListToString(teamList, foundSpawn))     // Compose error message
         mLevelErrorMsgs.push_back("ERROR: Need Core for " + teamList);
   }
}


void EditorUserInterface::validateTeams()
{
   validateTeams(getLevel()->findObjects_fast());
}


// Check that each item has a valid team  (fixes any problems it finds)
void EditorUserInterface::validateTeams(const Vector<DatabaseObject *> *dbObjects)
{
   S32 teams = getTeamCount();

   for(S32 i = 0; i < dbObjects->size(); i++)
   {
      BfObject *obj = dynamic_cast<BfObject *>(dbObjects->get(i));
      S32 team = obj->getTeam();

      if(obj->hasTeam() && ((team >= 0 && team < teams) || team == TEAM_NEUTRAL || team == TEAM_HOSTILE))
         continue;      // This one's OK

      if(team == TEAM_NEUTRAL && obj->canBeNeutral())
         continue;      // This one too

      if(team == TEAM_HOSTILE && obj->canBeHostile())
         continue;      // This one too

      if(obj->hasTeam())
         setTeam(obj, 0);              // We know there's at least one team, so there will always be a team 0
      else if(obj->canBeHostile() && !obj->canBeNeutral())
         setTeam(obj, TEAM_HOSTILE);
      else
         setTeam(obj, TEAM_NEUTRAL);   // We won't consider the case where hasTeam == canBeNeutral == canBeHostile == false
   }
}


// Search through editor objects, to make sure everything still has a valid team.  If not, we'll assign it a default one.
// Note that neutral/hostile items are on team -1/-2, and will be unaffected by this loop or by the number of teams we have.
void EditorUserInterface::teamsHaveChanged()
{
   bool teamsChanged = false;

   if(getTeamCount() != mOldTeams.size())     // Number of teams has changed
      teamsChanged = true;
   else
      for(S32 i = 0; i < getTeamCount(); i++)
      {
         const AbstractTeam *team = getTeam(i);

         if(mOldTeams[i].getColor() != team->getColor() ||          // Color(s) or
            mOldTeams[i].getName() != team->getName().getString()) // names(s) have changed
         {
            teamsChanged = true;
            break;
         }
      }

   if(!teamsChanged)       // Nothing changed, we're done here
      return;

   validateTeams();

   validateTeams(mDockItems.findObjects_fast());

   validateLevel();          // Revalidate level -- if teams have changed, requirements for spawns have too
   //markLevelPermanentlyDirty();  //<<-- TODO: Remove when team saving is part of mUndoManager
   autoSave();
}


string EditorUserInterface::getLevelFileName() const
{
   return mEditFileName != "" ? mEditFileName : UnnamedFile;
}


void EditorUserInterface::onSelectionChanged()
{
   rebuildSelectionOutline();
   clearPointOfRotation();
}


void EditorUserInterface::clearPointOfRotation()
{
   delete mPointOfRotation;
   mPointOfRotation = NULL;
}


template <class T>
static void addSelectedSegmentsToList(const Vector<DatabaseObject *> *walls,
   bool wholeWallOnly,
   Vector<const WallSegment *> &segments)
{
   for(S32 i = 0; i < walls->size(); i++)
   {
      WallItem *wall = static_cast<WallItem *>(walls->get(i));

      if(wall->isSelected() || (!wholeWallOnly && wall->anyVertsSelected()))
      {
         const Vector<WallSegment *> wallsegs = static_cast<BarrierX *>(wall)->getSegments();

         for(S32 j = 0; j < wallsegs.size(); j++)
            segments.push_back(wallsegs[j]);
      }
   }
}

// Declare these specific implementations of the above template so here so project will link
template void addSelectedSegmentsToList<WallItem>(const Vector<DatabaseObject *> *walls, bool, Vector<const WallSegment *> &segments);
template void addSelectedSegmentsToList<PolyWall>(const Vector<DatabaseObject *> *walls, bool, Vector<const WallSegment *> &segments);


// Return a list of all selected segments
static Vector<WallSegment const *> getSelectedWallsAndPolywallSegments(const Level *level)
{
   Vector<WallSegment const *> segments;

   const Vector<DatabaseObject *> *walls = level->findObjects_fast(WallItemTypeNumber);
   const Vector<DatabaseObject *> *polywalls = level->findObjects_fast(PolyWallTypeNumber);

   addSelectedSegmentsToList<WallItem>(walls, false, segments);
   addSelectedSegmentsToList<PolyWall>(polywalls, false, segments);

   return segments;
}


static Vector<WallSegment const *> getSelectedWallSegments(const Level *level)
{
   Vector<WallSegment const *> segments;

   const Vector<DatabaseObject *> *walls = level->findObjects_fast(WallItemTypeNumber);

   addSelectedSegmentsToList<WallItem>(walls, true, segments);

   return segments;
}


static Vector<WallSegment const *> getSelectedWallSegmentsMovingVertices(const Level *level)
{
   Vector<WallSegment const *> segments;

   const Vector<DatabaseObject *> *walls = level->findObjects_fast(WallItemTypeNumber);
   addSelectedSegmentsToList<WallItem>(walls, false, segments);

   const Vector<DatabaseObject *> *polywalls = level->findObjects_fast(PolyWallTypeNumber);

   addSelectedSegmentsToList<WallItem>(polywalls, false, segments);


   return segments;
}



// Rebuilds outline of selected walls
void EditorUserInterface::rebuildSelectionOutline()
{
   Vector<WallSegment const *> segments;

   segments = getSelectedWallSegments(mLevel.get());
   WallEdgeManager::clipAllWallEdges(segments, mSelectedWallEdgePointsWholeWalls);        // Populate mSelectedWallEdgePoints from segments

   segments = getSelectedWallSegmentsMovingVertices(mLevel.get());
   WallEdgeManager::clipAllWallEdges(segments, mSelectedWallEdgePointsDraggedVertices);   // Populate mSelectedWallEdgePoints from segments
}


void EditorUserInterface::onBeforeRunScriptFromConsole()
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   // Use selection as a marker -- will have to change in future
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(true);
   }
}


void EditorUserInterface::onAfterRunScriptFromConsole()
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   // Since all our original objects were marked as selected before the script was run, and since the objects generated by
   // the script are not selected, if we invert the selection, our script items will now be selected.
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(!obj->isSelected());
   }

   rebuildEverything(getLevel());
   onSelectionChanged();
}


void EditorUserInterface::onActivate()
{
   mDelayedUnselectObject = NULL;
   FolderManager *folderManager = mGameSettings->getFolderManager();

   // Check if we have a level name:
   if(getLevelFileName() == UnnamedFile)           // We need to take a detour to get a level name
   {
      if(folderManager->getLevelDir().empty())     // Never did resolve a leveldir... no editing for you!
      {
         getUIManager()->reactivatePrevUI();       // Must come before the error msg, so it will become the previous UI when that one exits

         ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();
         ui->reset();
         ui->setTitle("HOUSTON, WE HAVE A PROBLEM");
         ui->setMessage("No valid level folder was found, so I cannot start the level editor.\n\n"
            "Check the LevelDir parameter in your INI file, or your command-line parameters to make sure"
            "you have correctly specified a valid folder.");

         getUIManager()->activate(ui);

         return;
      }

      // Don't save this menu (false, below).  That way, if the user escapes out, and is returned to the "previous"
      // UI, they will get back to where they were before (prob. the main menu system), not back to here.
      getUIManager()->activate<LevelNameEntryUserInterface>(false);

      return;
   }

   mLevelErrorMsgs.clear();
   mLevelWarnings.clear();

   mSaveMsgTimer.clear();

   mGameTypeArgs.clear();
   mChatMessageDisplayer.reset();

   onActivateReactivate();

   loadLevel();
   setCurrentTeam(0);

   // Reset display parameters...
   mDragSelecting = false;

   mCreatingPoly = false;
   mCreatingPolyline = false;
   mDraggingDockItem = NULL;
   mCurrentTeam = 0;
   mPreviewMode = false;
   mDragCopying = false;
   mJustInsertedVertex = false;
   mShowAllIds = false;

   VideoSystem::actualizeScreenMode(mGameSettings, true, usesEditorScreenMode());

   centerView();
   findPlugins();
}


void EditorUserInterface::renderMasterStatus(const MasterServerConnection *connectionToMaster) const
{
   // Do nothing, don't render this in editor 
}


bool EditorUserInterface::usesEditorScreenMode() const
{
   return true;
}


// Stuff to do when activating or reactivating
void EditorUserInterface::onActivateReactivate()
{
   mDraggingObjects = false;
   mUp = mDown = mLeft = mRight = mIn = mOut = false;
   mDockItemHit = NULL;
   mSnapContext = GRID_SNAPPING | OBJECT_SNAPPING;    // By default, we'll snap to the grid and other objects

   Cursor::enableCursor();
}


void EditorUserInterface::onReactivate()     // Run when user re-enters the editor after testing, among other things
{
   onActivateReactivate();

   if(mWasTesting)
   {
      mWasTesting = false;
      mSaveMsgTimer.clear();

      remove("editor.tmp");         // Delete temp file
      //getGame()->setLevel(mLevel.get());  // Point the game at our old level
   }

   if(mCurrentTeam >= getTeamCount())
      mCurrentTeam = 0;

   if(UserInterface::getUIManager()->getPrevUI()->usesEditorScreenMode() != usesEditorScreenMode())
      VideoSystem::actualizeScreenMode(mGameSettings, true, usesEditorScreenMode());

   //TNLAssert(getGame()->getLevel() == mLevel.get(), "I want my level back!!");
}


S32 EditorUserInterface::getTeamCount() const
{
   return mLevel->getTeamCount();
}


const AbstractTeam *EditorUserInterface::getTeam(S32 teamId)
{
   return mLevel->getTeam(teamId);
}


void EditorUserInterface::clearTeams()
{
   mLevel->clearTeams();
}


bool EditorUserInterface::getNeedToSave() const
{
   return mUndoManager.needToSave();
}


//void EditorUserInterface::addTeam(const TeamInfo &teamInfo)
//{
//   mLevel->addTeam(teamInfo);
//}


void EditorUserInterface::addTeam(EditorTeam *team, S32 teamIndex)
{
   mLevel->addTeam(team, teamIndex);
}


void EditorUserInterface::removeTeam(S32 teamIndex)
{
   mLevel->removeTeam(teamIndex);
}


Point EditorUserInterface::convertCanvasToLevelCoord(const Point &p) const
{
   return (p - mCurrentOffset) / mCurrentScale;
}


Point EditorUserInterface::convertLevelToCanvasCoord(const Point &p, bool convert) const
{
   return convert ? p * mCurrentScale + mCurrentOffset : p;
}


// Called when we shift between windowed and fullscreen mode, after change is made
void EditorUserInterface::onDisplayModeChange()
{
   static S32 previousXSize = -1;
   static S32 previousYSize = -1;

   if(previousXSize != DisplayManager::getScreenInfo()->getGameCanvasWidth() ||
      previousYSize != DisplayManager::getScreenInfo()->getGameCanvasHeight())
   {
      // Recenter canvas -- note that canvasWidth may change during displayMode change
      mCurrentOffset.set(mCurrentOffset.x - previousXSize / 2 + DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2,
         mCurrentOffset.y - previousYSize / 2 + DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2);
   }

   // Need to populate the dock here because dock items are tied to a particular screen x,y; 
   // maybe it would be better to give them a dock x,y instead?  
   // mLevel might be NULL here when we first start up the editor.
   if(mLevel && mLevel->getGameType())
      populateDock();               // If game type has changed, items on dock will change

   previousXSize = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   previousYSize = DisplayManager::getScreenInfo()->getGameCanvasHeight();
}


// We'll have 4 possibilities to consider here: 1) GridSnapping; 2) Constrained Movement; 3) Both; 4) Neither
// p must be a level coordinate.  
Point EditorUserInterface::snapPointConstrainedOrLevelGrid(Point const &p) const
{
   // Are we doing any snapping at all?  If not, there's nothing to do.  Case 4) Neither
   if(!(mSnapContext & (GRID_SNAPPING | CONSTRAINED_MOVEMENT)))
      return p;

   // First, find a snap point based on our grid
   F32 factor = (showMinorGridLines() ? 0.1f : 0.5f) * mGridSize;    // Tenths or halves -- major gridlines are gridsize pixels apart

   // Are we only interested in grid snapping? If so, return our point!  Case 1) GridSnapping
   if(!(mSnapContext & CONSTRAINED_MOVEMENT))                        
      return snapPointToLevelGrid(p, factor);

   // Some stuff we'll need for the following cases
   const F32 angles[] = { degreesToRadians(0),   degreesToRadians(15),  degreesToRadians(30), 
                          degreesToRadians(45),  degreesToRadians(60),  degreesToRadians(75), 
                          degreesToRadians(90),  degreesToRadians(105), degreesToRadians(120), 
                          degreesToRadians(135), degreesToRadians(150), degreesToRadians(165) }; 


   // Case 2) Constrained Movement
   if(!(mSnapContext & GRID_SNAPPING))    
   {
      Vector<Point> candidates(ARRAYSIZE(angles));
      for(S32 i = 0; i < ARRAYSIZE(angles); i++)
         candidates.push_back(pointOnLine(p, mMoveOrigin, Point(cos(angles[i]), sin(angles[i]))));

      return candidates[findClosestPoint(p, candidates)];
   }


   // Here we do the more complex grid snapping PLUS constrained movement  Case 3) Both

   // These are the four corners of the "snap box" we're in
   F32 x1 = floor(p.x / factor) * factor;    F32 x2 = ceil (p.x / factor) * factor;
   F32 y1 = ceil (p.y / factor) * factor;    F32 y2 = floor(p.y / factor) * factor;

   // These are the four corners of a more outer "snap box" we're in
   F32 x3 = floor(p.x / factor) * factor - factor;    F32 x4 = ceil (p.x / factor) * factor + factor;
   F32 y3 = ceil (p.y / factor) * factor + factor;    F32 y4 = floor(p.y / factor) * factor - factor;
  

   Point lr = Point(x2, y1);    Point ll = Point(x1, y1);
   Point ul = Point(x1, y2);    Point ur = Point(x2, y2);

   Point lr2 = Point(x4, y3);    Point ll2 = Point(x3, y3);
   Point ul2 = Point(x3, y4);    Point ur2 = Point(x4, y4);

   // Render some points -- uncomment and call from render() to see these
   //Vector<Point> ps = Vector<Point>();
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ll - Point(1000,0))); ps.push_back(convertLevelToCanvasCoord(lr + Point(1000,0))); RenderUtils::drawLine(&ps, Colors::cyan);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(lr - Point(0,1000))); ps.push_back(convertLevelToCanvasCoord(ur + Point(0,1000))); RenderUtils::drawLine(&ps, Colors::cyan);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ur + Point(1000,0))); ps.push_back(convertLevelToCanvasCoord(ul - Point(1000,0))); RenderUtils::drawLine(&ps, Colors::cyan);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ll - Point(0,1000))); ps.push_back(convertLevelToCanvasCoord(ul + Point(0,1000))); RenderUtils::drawLine(&ps, Colors::cyan);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ll2 - Point(1000,0))); ps.push_back(convertLevelToCanvasCoord(lr2 + Point(1000,0))); RenderUtils::drawLine(&ps, Colors::red);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(lr2 - Point(0,1000))); ps.push_back(convertLevelToCanvasCoord(ur2 + Point(0,1000))); RenderUtils::drawLine(&ps, Colors::green);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ur2 + Point(1000,0))); ps.push_back(convertLevelToCanvasCoord(ul2 - Point(1000,0))); RenderUtils::drawLine(&ps, Colors::orange50);
   //ps.clear();ps.push_back(convertLevelToCanvasCoord(ll2 - Point(0,1000))); ps.push_back(convertLevelToCanvasCoord(ul2 + Point(0,1000))); RenderUtils::drawLine(&ps, Colors::blue);


   const S32 len = 1000000;
   Point intersection;     // reusable container

   Vector<Point> candidates(ARRAYSIZE(angles) * 8);

   for(S32 i = 0; i < ARRAYSIZE(angles); i++)
   {
      Point offset = Point(cos(angles[i]), sin(angles[i])) * len;

      Point horiz = Point(len, 0);
      Point vert  = Point(0, len);

      // Inner box
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, lr + horiz, ll - horiz, intersection))      // Bottom
         candidates.push_back(intersection);
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, ul - horiz, ur + horiz, intersection))      // Top
         candidates.push_back(intersection);
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, lr - vert,  ur + vert,  intersection))      // Right
         candidates.push_back(intersection);                                       
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, ul + vert,  ll - vert,  intersection))      // Left
         candidates.push_back(intersection); 

      // Outer box
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, lr2 + horiz, ll2 - horiz, intersection))      // Bottom
         candidates.push_back(intersection);
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, ul2 - horiz, ur2 + horiz, intersection))      // Top
         candidates.push_back(intersection);
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, lr2 - vert,  ur2 + vert,  intersection))      // Right
         candidates.push_back(intersection);
      if(findIntersection(mMoveOrigin - offset, mMoveOrigin + offset, ul2 + vert,  ll2 - vert,  intersection))      // Left
         candidates.push_back(intersection);
   }

   if(candidates.size() == 0)
      return (mSnapContext & GRID_SNAPPING) ? snapPointToLevelGrid(p, factor) : p;

   logprintf("candidates %d", candidates.size()); //{P{P

   return candidates[findClosestPoint(p, candidates)];
}


Point EditorUserInterface::snapPointToLevelGrid(Point const &p, F32 factor) const
{
   return Point(floor(p.x / factor + 0.5) * factor, floor(p.y / factor + 0.5) * factor);
}


// p should be a level coordinate
Point EditorUserInterface::snapPoint(const Point &p, bool snapWhileOnDock) const
{
   if(mouseOnDock() && !snapWhileOnDock)
      return p;      // No snapping!

   Point snapPoint(p);     // Make a working copy

   if(mDraggingObjects)
   {
      // Turrets & forcefields: Snap to a wall edge as first (and only) choice, regardless of whether snapping is on or off
      if(isEngineeredType(mSnapObject->getObjectTypeNumber()))
         return snapPointConstrainedOrLevelGrid(p);
   }

   F32 minDist = 255 / mCurrentScale;    // 255 just seems to work well, not related to gridsize; only has an impact when grid is off

   // Only snap to grid when grid snapping or constrained movement is enabled; lowest priority snaps go first
   if(mSnapContext & (GRID_SNAPPING | CONSTRAINED_MOVEMENT))      
   {
      snapPoint = snapPointConstrainedOrLevelGrid(p);
      minDist = snapPoint.distSquared(p);
   }


   if(mSnapContext & OBJECT_SNAPPING)
   {
      snapPoint = snapToObjects(p, snapPoint, minDist);
      //minDist = snapPoint.distSquared(p);
   }

   return snapPoint;
}


// we'll make a local copy of closest
Point EditorUserInterface::snapToObjects(const Point &mousePos, Point closest, F32 minDist) const
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   // Now look for other things we might want to snap to
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      // Don't snap to selected items or items with selected verts (keeps us from snapping to ourselves, which is usually trouble)
      if(obj->isSelected() || obj->anyVertsSelected())
         continue;

      for(S32 j = 0; j < obj->getVertCount(); j++)
      {
         F32 dist = obj->getVert(j).distSquared(closest);
         if(dist < minDist)
         {
            minDist = dist;
            closest.set(obj->getVert(j));
         }
      }
   }

   // Search for a corner to snap to - by using wall edges, we'll also look for intersections between segments.  Sets closest.
   if(getSnapToWallCorners())
      closest = checkCornersForSnap(mousePos, mLevel->getWallEdgeDatabase()->findObjects_fast(), minDist, closest);

   return closest;
}


void EditorUserInterface::markSelectedObjectsAsUnsnapped2(const Vector<DatabaseObject *> *objList)
{
   for(S32 i = 0; i < objList->size(); i++)
   {
      // Only engineered items can be snapped
      if(!isEngineeredType(objList->get(i)->getObjectTypeNumber()))
         continue;

      EngineeredItem *obj = static_cast<EngineeredItem *>(objList->get(i));

      // Unselected items will remain unaffected
      if(!obj->isSelected())
         continue;

      // Snapped items whose mounts are selected (and hence also being dragged) will remain snapped
      if(obj->isSnapped() && obj->getMountSegment()->isSelected())
         continue;

      // Everything else is free to snap to something else!
      obj->setSnapped(false);
   }
}


bool EditorUserInterface::getSnapToWallCorners() const
{
   // Allow snapping to wall corners when we're dragging items.  Disallow for all wall types other than PolyWall
   return mSnapContext != 0 && mDraggingObjects &&                  // When all snapping flags are off, mSnapContext will be 0
      (mSnapObject->getObjectTypeNumber() == PolyWallTypeNumber ||  // Allow PolyWall
       mSnapObject->getObjectTypeNumber() == WallItemTypeNumber ||  // Allow WallItem
       !isWallType(mSnapObject->getObjectTypeNumber()));            // Disallow other Wall-related parts (would
   // these even ever appear as a snap object?)
}


// Sets snapPoint
static bool checkPoint(const Point &clickPoint, const Point &point, F32 &minDist, Point &snapPoint)
{
   F32 dist = point.distSquared(clickPoint);
   if(dist < minDist)
   {
      minDist = dist;
      snapPoint = point;
      return true;
   }

   return false;
}


// Pass snapPoint in a way that creates local copy we can modify
Point EditorUserInterface::checkCornersForSnap(const Point &clickPoint, const Vector<DatabaseObject *> *edges,
                                               F32 &minDist, Point snapPoint) const
{
   const Point *vert;

   for(S32 i = 0; i < edges->size(); i++)
      for(S32 j = 0; j < 1; j++)
      {
         WallEdge *edge = static_cast<WallEdge *>(edges->get(i));
         vert = (j == 0) ? edge->getStart() : edge->getEnd();

         if(checkPoint(clickPoint, *vert, minDist, snapPoint))
            return snapPoint;
      }

   return snapPoint;
}


////////////////////////////////////
////////////////////////////////////
// Rendering routines


bool EditorUserInterface::showMinorGridLines() const
{
   return mCurrentScale >= .5;
}

static S32 QSORT_CALLBACK sortByTeam(DatabaseObject **a, DatabaseObject **b)
{
   TNLAssert(dynamic_cast<BfObject *>(*a), "Not a BfObject");
   TNLAssert(dynamic_cast<BfObject *>(*b), "Not a BfObject");
   return ((BfObject *)(*b))->getTeam() - ((BfObject *)(*a))->getTeam();
}


void EditorUserInterface::renderTurretAndSpyBugRanges(GridDatabase *editorDb) const
{
   fillVector = *editorDb->findObjects_fast(SpyBugTypeNumber);  // This will actually copy vector of pointers to fillVector
   // so we can sort by team, this is still faster then findObjects.
   if(fillVector.size() != 0)
   {
      // Use Z Buffer to make use of not drawing overlap visible area of same team SpyBug, but does overlap different team
      fillVector.sort(sortByTeam); // Need to sort by team, or else won't properly combine the colors.
      mGL->glClear(GLOPT::DepthBufferBit);
      mGL->glEnable(GLOPT::DepthTest);
      mGL->glEnable(GLOPT::DepthWritemask);
      mGL->glDepthFunc(GLOPT::Less);
      mGL->glPushMatrix();
      mGL->glTranslate(0, 0, -0.95f);

      // This blending works like this, source(SRC) * GL_ONE_MINUS_DST_COLOR + destination(DST) * GL_ONE
      mGL->glBlendFunc(GLOPT::OneMinusDstColor, GLOPT::One);

      S32 prevTeam = -10;

      // Draw spybug visibility ranges first, underneath everything else
      for(S32 i = 0; i < fillVector.size(); i++)
      {
         BfObject *editorObj = dynamic_cast<BfObject *>(fillVector[i]);

         if(i != 0 && editorObj->getTeam() != prevTeam)
            mGL->glTranslate(0, 0, 0.05f);
         prevTeam = editorObj->getTeam();

         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         GameObjectRender::renderSpyBugVisibleRange(pos, editorObj->getColor(), mCurrentScale);
      }

      mGL->setDefaultBlendFunction();

      mGL->glPopMatrix();
      mGL->glDisable(GLOPT::DepthWritemask);
      mGL->glDisable(GLOPT::DepthTest);
   }

   // Next draw turret firing ranges for selected or highlighted turrets only
   fillVector.clear();

   editorDb->findObjects(TurretTypeNumber, fillVector);
   for(S32 i = 0; i < fillVector.size(); i++)
   {
      BfObject *editorObj = dynamic_cast<BfObject *>(fillVector[i]);
      if(editorObj->isSelected() || editorObj->isLitUp())
      {
         Point pos = editorObj->getPos();
         pos *= mCurrentScale;
         pos += mCurrentOffset;
         GameObjectRender::renderTurretFiringRange(pos, editorObj->getColor(), mCurrentScale);
      }
   }
}


S32 getDockHeight()
{
   return DisplayManager::getScreenInfo()->getGameCanvasHeight() - 2 * EditorUserInterface::vertMargin;
}


void EditorUserInterface::renderDock() const
{
   // Render item dock down RHS of screen
   const S32 canvasWidth = DisplayManager::getScreenInfo()->getGameCanvasWidth();
   const S32 canvasHeight = DisplayManager::getScreenInfo()->getGameCanvasHeight();

   Color fillColor;

   switch(mDockMode)
   {
   case DOCKMODE_ITEMS:
      fillColor = Colors::red30;
      break;

   case DOCKMODE_PLUGINS:
      fillColor = Colors::blue40;
      break;
   }

   S32 dockHeight = getDockHeight();

   RenderUtils::drawFilledFancyBox(canvasWidth - mDockWidth - horizMargin, canvasHeight - vertMargin - dockHeight,
      canvasWidth - horizMargin, canvasHeight - vertMargin,
      8, fillColor, .7f, (mouseOnDock() ? Colors::yellow : Colors::white));

   switch(mDockMode)
   {
   case DOCKMODE_ITEMS:
      renderDockItems();
      break;

   case DOCKMODE_PLUGINS:
      renderDockPlugins();
      break;
   }
}


const S32 PANEL_TEXT_SIZE = 10;
const S32 PANEL_SPACING = S32(PANEL_TEXT_SIZE * 1.3);

static S32 PanelBottom, PanelTop, PanelLeft, PanelRight, PanelInnerMargin;

void EditorUserInterface::renderInfoPanel() const
{
   // Recalc dimensions in case screen mode changed
   PanelBottom = DisplayManager::getScreenInfo()->getGameCanvasHeight() - EditorUserInterface::vertMargin;
   PanelTop = PanelBottom - (4 * PANEL_SPACING + 9);
   PanelLeft = EditorUserInterface::horizMargin;
   PanelRight = PanelLeft + 180;      // left + width
   PanelInnerMargin = 4;

   RenderUtils::drawFilledFancyBox(PanelLeft, PanelTop, PanelRight, PanelBottom, 6, Colors::richGreen, .7f, Colors::white);


   // Draw coordinates on panel -- if we're moving an item, show the coords of the snap vertex, otherwise show the coords of the
   // snapped mouse position
   Point pos;

   if(mSnapObject)
      pos = mSnapObject->getVert(mSnapVertexIndex);
   else
      pos = snapPoint(convertCanvasToLevelCoord(mMousePos));


   mGL->glColor(Colors::white);
   renderPanelInfoLine(1, "Cursor X,Y: %2.1f,%2.1f", pos.x, pos.y);

   // And scale
   renderPanelInfoLine(2, "Zoom Scale: %2.2f", mCurrentScale);

   // Show number of teams
   renderPanelInfoLine(3, "Team Count: %d", getTeamCount());

   bool needToSave = mUndoManager.needToSave();

   // Color level name by whether it needs to be saved or not
   mGL->glColor(needToSave ? Colors::red : Colors::green);

   // Filename without extension
   string filename = getLevelFileName();
   renderPanelInfoLine(4, "Filename: %s%s", needToSave ? "*" : "", filename.substr(0, filename.find_last_of('.')).c_str());
}


void EditorUserInterface::renderPanelInfoLine(S32 line, const char *format, ...) const
{
   const S32 xpos = horizMargin + PanelInnerMargin;

   va_list args;
   static char text[512];  // reusable buffer

   va_start(args, format);
   vsnprintf(text, sizeof(text), format, args);
   va_end(args);

   RenderUtils::drawString(xpos, DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - PANEL_TEXT_SIZE -
      line * PANEL_SPACING + 6, PANEL_TEXT_SIZE, text);
}


// Helper to render attributes in a colorful and lady-like fashion
void EditorUserInterface::renderAttribText(S32 xpos, S32 ypos, S32 textsize,
   const Color &keyColor, const Color &valColor,
   const Vector<string> &keys, const Vector<string> &vals)
{
   TNLAssert(keys.size() == vals.size(), "Expected equal number of keys and values!");
   for(S32 i = 0; i < keys.size(); i++)
   {
      mGL->glColor(keyColor);
      xpos += RenderUtils::drawStringAndGetWidth(xpos, ypos, textsize, keys[i].c_str());
      xpos += RenderUtils::drawStringAndGetWidth(xpos, ypos, textsize, ": ");

      mGL->glColor(valColor);
      xpos += RenderUtils::drawStringAndGetWidth(xpos, ypos, textsize, vals[i].c_str());
      if(i < keys.size() - 1)
         xpos += RenderUtils::drawStringAndGetWidth(xpos, ypos, textsize, "; ");
   }
}


// Shows selected item attributes, or, if we're hovering over dock item, shows dock item info string
void EditorUserInterface::renderItemInfoPanel() const
{
   string itemName;     // All intialized to ""

   S32 hitCount = 0;
   bool multipleKindsOfObjectsSelected = false;

   static Vector<string> keys, values;       // Reusable containers
   keys.clear();
   values.clear();

   const char *instructs = "";

   S32 xpos = PanelRight + 9;
   S32 ypos = PanelBottom - PANEL_TEXT_SIZE - PANEL_SPACING + 6;
   S32 upperLineTextSize = 14;

   // Render information when hovering over a dock item
   if(mDockItemHit)
   {
      itemName = mDockItemHit->getOnScreenName();

      mGL->glColor(Colors::green);
      RenderUtils::drawString(xpos, ypos, 12, mDockItemHit->getEditorHelpString());

      ypos -= S32(upperLineTextSize * 1.3);

      mGL->glColor(Colors::white);
      RenderUtils::drawString(xpos, ypos, upperLineTextSize, itemName.c_str());
   }

   // Handle everything else
   else
   {
      // Cycle through all our objects to find the selected ones
      const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected())
         {
            if(hitCount == 0)       // This is the first object we've hit
            {
               itemName = obj->getOnScreenName();
               obj->fillAttributesVectors(keys, values);
               instructs = obj->getInstructionMsg(keys.size());      // Various objects have different instructions

               S32 id = obj->getUserAssignedId();
               keys.push_back("Id");
               values.push_back(id > 0 ? itos(id) : "Unassigned");
            }
            else                    // Second or subsequent selected object found
            {
               if(multipleKindsOfObjectsSelected || itemName != obj->getOnScreenName())    // Different type of object
               {
                  itemName = "Multiple object types selected";
                  multipleKindsOfObjectsSelected = true;
               }
            }

            hitCount++;
         }  // end if obj is selected
      }

      /////
      // Now render the info we collected above

      if(hitCount == 1)
      {
         mGL->glColor(Colors::yellow);
         S32 w = RenderUtils::drawStringAndGetWidth(xpos, ypos, PANEL_TEXT_SIZE, instructs);
         if(w > 0)
            w += RenderUtils::drawStringAndGetWidth(xpos + w, ypos, PANEL_TEXT_SIZE, "; ");
         RenderUtils::drawString(xpos + w, ypos, PANEL_TEXT_SIZE, "[#] to edit Id");

         renderAttribText(xpos, ypos - PANEL_SPACING, PANEL_TEXT_SIZE, Colors::cyan, Colors::white, keys, values);
      }

      ypos -= PANEL_SPACING + S32(upperLineTextSize * 1.3);
      if(hitCount > 0)
      {
         if(!multipleKindsOfObjectsSelected)
            itemName = (mDraggingObjects ? "Dragging " : "Selected ") + itemName;

         if(hitCount > 1)
            itemName += " (" + itos(hitCount) + ")";

         mGL->glColor(Colors::yellow);
         RenderUtils::drawString(xpos, ypos, upperLineTextSize, itemName.c_str());
      }

      ypos -= S32(upperLineTextSize * 1.3);
      if(mInfoMsg != "")
      {
         mGL->glColor(Colors::white);
         RenderUtils::drawString(xpos, ypos, upperLineTextSize, mInfoMsg.c_str());
      }
   }
}


void EditorUserInterface::renderReferenceShip() const
{
   // Render ship at cursor to show scale
   static F32 thrusts[4] = {1, 0, 0, 0};

   mGL->glPushMatrix();
   mGL->glTranslate(mMousePos);
   mGL->glScale(mCurrentScale);
   mGL->glRotate(90);
   GameObjectRender::renderShip(ShipShape::Normal, Colors::red, 1, thrusts, 1, 5, 0, false, false, false, false);
   mGL->glRotate(-90);

   // Draw collision circle
   const F32 spaceAngle = 0.0278f * FloatTau;
   mGL->glColor(Colors::green, 0.35f);
   mGL->glLineWidth(RenderUtils::LINE_WIDTH_1);
   RenderUtils::drawDashedCircle(Point(0, 0), (F32)Ship::CollisionRadius, 10, spaceAngle, 0);
   mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH);

   // And show how far it can see
   const S32 horizDist = Game::PLAYER_VISUAL_DISTANCE_HORIZONTAL;
   const S32 vertDist = Game::PLAYER_VISUAL_DISTANCE_VERTICAL;

   mGL->glColor(Colors::paleBlue, 0.35f);
   RenderUtils::drawFilledRect(-horizDist, -vertDist, horizDist, vertDist);

   mGL->glPopMatrix();
}


static F32 getRenderingAlpha(bool isScriptItem)
{
   return isScriptItem ? .4f : 1;     // Script items will appear somewhat translucent
}


// Master render function
void EditorUserInterface::render() const
{
   GridDatabase *editorDb = getLevel();

   // Render bottom-most layer of our display
   if(mPreviewMode)
      renderTurretAndSpyBugRanges(editorDb);    // Render range of all turrets and spybugs in editorDb
   else
      GameObjectRender::renderGrid(mCurrentScale, mCurrentOffset, convertLevelToCanvasCoord(Point(0, 0)),
                                   (F32)mGridSize, mSnapContext & GRID_SNAPPING, showMinorGridLines());

   // == Draw rays when movement is constrained ==
   if(mDraggingObjects && mSnapContext & CONSTRAINED_MOVEMENT)
      GameObjectRender::renderConstrainedDraggingLines(convertLevelToCanvasCoord(mMoveOrigin));

   mGL->glPushMatrix();
   mGL->glTranslate(getCurrentOffset());
   mGL->glScale(getCurrentScale());

   // mSnapDelta only gets recalculated during a dragging event -- if an item is no longer being dragged, we
   // don't want to use the now stale value in mSnapDelta, but rather (0,0) to reflect the rather obvoius fact
   // that walls that are not being dragged should be rendered in place.
   static Point delta = mDraggingObjects ? mSnapDelta : Point(0, 0);

   // == Render walls and polyWalls ==
   // Shadows only drawn under walls that are being dragged.  No dragging, no shadows.
   if(mDraggingObjects)
      GameObjectRender::renderShadowWalls(mSelectedObjectsForDragging);

   renderWallsAndPolywalls(&mLevelGenDatabase, delta, false, true);
   renderWallsAndPolywalls(editorDb, delta, false, false);

   // == Normal, unselected items ==
   // Draw map items (teleporters, etc.) that are not being dragged, and won't have any text labels  (below the dock)
   renderObjects(editorDb, RENDER_UNSELECTED_NONWALLS, false);             // Render our normal objects
   renderObjects(&mLevelGenDatabase, RENDER_UNSELECTED_NONWALLS, true);    // Render any levelgen objects being overlaid

   // == Selected items ==
   // Draw map items (teleporters, etc.) that are are selected and/or lit up, so label is readable (still below the dock)
   // Do this as a separate operation to ensure that these are drawn on top of those drawn above.
   // We do render polywalls here because this is what draws the highlighted outline when the polywall is selected.
   renderObjects(editorDb, RENDER_SELECTED_NONWALLS, false);               // Render selected objects 

   renderWallsAndPolywalls(editorDb, delta, true, false);

   // == Draw geomPolyLine features under construction ==
   if(mCreatingPoly || mCreatingPolyline)
      renderObjectsUnderConstruction();

   // Since we're not constructing a barrier, if there are any barriers or lineItems selected, 
   // get the width for display at bottom of dock
   else
   {
      fillVector.clear();
      editorDb->findObjects((TestFunc)isLineItemType, fillVector);

      for(S32 i = 0; i < fillVector.size(); i++)
      {
         LineItem *obj = dynamic_cast<LineItem *>(fillVector[i]);   // Walls are a subclass of LineItem, so this will work for both

         if(obj && (obj->isSelected() || (obj->isLitUp() && obj->isVertexLitUp(NONE))))
            break;
      }
   }

   // Render our snap vertex as a hollow magenta box
   if(mVertexEditMode &&                                                                        // Must be in vertex-edit mode
      !mPreviewMode && mSnapObject && mSnapObject->isSelected() && mSnapVertexIndex != NONE &&  // ...but not in preview mode...
      mSnapObject->getGeomType() != geomPoint &&                                                // ...and not on point objects...
      !mSnapObject->isVertexLitUp(mSnapVertexIndex) &&                                          // ...or lit-up vertices...
      !mSnapObject->vertSelected(mSnapVertexIndex))                                             // ...or selected vertices
   {
      GameObjectRender::renderVertex(SnappingVertex, mSnapObject->getVert(mSnapVertexIndex), NO_NUMBER, mCurrentScale/*, alpha*/);
   }

   if(mShowAllIds)
      renderObjectIds(editorDb);

   mGL->glPopMatrix();

   if(!mNormalizedScreenshotMode)
   {
      if(mPreviewMode)
         renderReferenceShip();
      else
      {
         // The following items are hidden in preview mode:
         renderDock();

         renderInfoPanel();
         renderItemInfoPanel();

         if(mouseOnDock() && mDockItemHit)
            mDockItemHit->setLitUp(true);       // Will trigger a selection highlight to appear around dock item
      }
   }

   renderDragSelectBox();

   if(mAutoScrollWithMouse)
   {
      mGL->glColor(Colors::white);
      GameObjectRender::drawFourArrows(mScrollWithMouseLocation);
   }

   if(!mNormalizedScreenshotMode)
   {
      renderSaveMessage();
      renderWarnings();
      renderLingeringMessage();

      // anchorPos, helperVisible, helperFadeIn, composingMessage, anouncementActive, alpha
      mChatMessageDisplayer.render(messageMargin, 0, false, false, 1);
   }

   renderConsole();        // Rendered last, so it's always on top
}


void EditorUserInterface::renderObjectIds(GridDatabase *database) const
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   Point offset(50, 30);

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->getUserAssignedId() > 0)
         GameObjectRender::renderNumberInBox(obj->getCentroid(), obj->getUserAssignedId(), mCurrentScale);
   }
}


void EditorUserInterface::setColor(bool isSelected, bool isLitUp, bool isScriptItem)
{
   F32 alpha = isScriptItem ? .6f : 1;     // So script items will appear somewhat translucent

   if(isSelected)
      mGL->glColor(Colors::EDITOR_SELECT_COLOR, alpha);       // yellow
   else if(isLitUp)
      mGL->glColor(Colors::EDITOR_HIGHLIGHT_COLOR, alpha);    // white
   else  // Normal
      mGL->glColor(Colors::EDITOR_PLAIN_COLOR, alpha);
}


// Render objects in the specified database
void EditorUserInterface::renderObjects(const GridDatabase *database, RenderModes renderMode, bool isLevelgenOverlay) const
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   bool wantSelected = (renderMode == RENDER_SELECTED_NONWALLS || renderMode == RENDER_SELECTED_WALLS);
   bool wantWalls = (renderMode == RENDER_UNSELECTED_WALLS || renderMode == RENDER_SELECTED_WALLS);

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      bool isSelected = obj->isSelected() || obj->isLitUp();
      bool isWall = isWallType(obj->getObjectTypeNumber());

      if(isSelected != wantSelected || isWall != wantWalls)
         continue;

      // Items are rendered in index order, so those with a higher index get drawn later, and hence, on top
      setColor(obj->isSelected(), obj->isLitUp(), isLevelgenOverlay);

      if(mPreviewMode)
      {
         if(!isWall)
            obj->render();
      }
      else
      {
         obj->renderEditor(mCurrentScale, getSnapToWallCorners(), mVertexEditMode);
         obj->renderAndLabelHighlightedVertices(mCurrentScale);
      }
   }
}


// Render walls (both normal walls and polywalls, outlines and fills) and centerlines
void EditorUserInterface::renderWallsAndPolywalls(const GridDatabase *database, const Point &offset,
   bool drawSelected, bool isLevelGenDatabase) const
{
   // Guarantee walls are a standard color for editor screenshot uploads to the level database
   const Color &fillColor = mNormalizedScreenshotMode ? Colors::DefaultWallFillColor :
      mPreviewMode ? mGameSettings->getWallFillColor() :
      Colors::EDITOR_WALL_FILL_COLOR;

   const Color &outlineColor = mNormalizedScreenshotMode ? Colors::DefaultWallOutlineColor :
      mGameSettings->getWallOutlineColor();

   GameObjectRender::renderWalls(mLevel->findObjects_fast(WallItemTypeNumber),
      mLevel->findObjects_fast(PolyWallTypeNumber),
      mWallEdgePoints,
      mSelectedWallEdgePointsWholeWalls,
      mSelectedWallEdgePointsDraggedVertices,
      outlineColor,
      fillColor,
      mCurrentScale,
      mDraggingObjects, // <== bool
      drawSelected,
      offset,
      mPreviewMode,
      getSnapToWallCorners(),
      getRenderingAlpha(isLevelGenDatabase));


   // Render walls as ordinary objects; this will draw wall centerlines
   if(!isLevelGenDatabase)
      renderObjects(database, drawSelected ? RENDER_SELECTED_WALLS : RENDER_UNSELECTED_WALLS, false);
}


void EditorUserInterface::renderObjectsUnderConstruction() const
{
   // Add a vert (and deleted it later) to help show what this item would look like if the user placed the vert in the current location
   mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
   mGL->glLineWidth(RenderUtils::LINE_WIDTH_3);

   if(mCreatingPoly) // Wall
      mGL->glColor(Colors::EDITOR_SELECT_COLOR);
   else              // LineItem --> Caution! we're rendering an object that doesn't exist yet; its game is NULL
      mGL->glColor(mLevel->getTeamColor(mCurrentTeam));

   RenderUtils::drawLine(mNewItem->getOutline());

   mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH);

   for(S32 j = mNewItem->getVertCount() - 1; j >= 0; j--)      // Go in reverse order so that placed vertices are drawn atop unplaced ones
   {
      Point v = mNewItem->getVert(j);

      // Draw vertices
      if(j == mNewItem->getVertCount() - 1)                    // This is our most current vertex
         GameObjectRender::renderVertex(HighlightedVertex, v, NO_NUMBER, mCurrentScale);
      else
         GameObjectRender::renderVertex(SelectedItemVertex, v, j, mCurrentScale);
   }
   mNewItem->deleteVert(mNewItem->getVertCount() - 1);
}


// Draw box for selecting items
void EditorUserInterface::renderDragSelectBox() const
{
   if(!mDragSelecting)
      return;

   mGL->glColor(Colors::white);
   Point downPos = convertLevelToCanvasCoord(mMouseDownPos);
   RenderUtils::drawHollowRect(downPos, mMousePos);
}


static const S32 DOCK_LABEL_SIZE = 9;      // Size to label items on the dock

void EditorUserInterface::renderDockItem(const BfObject *object, const Color &color, F32 currentScale, S32 snapVertexIndex)
{
   mGL->glColor(Colors::EDITOR_PLAIN_COLOR);

   object->renderDock(color);
   renderDockItemLabel(object->getDockLabelPos(), object->getOnDockName());

   if(object->isLitUp())
      object->highlightDockItem();
}


void EditorUserInterface::renderDockItems() const
{
   for(S32 i = 0; i < mDockItems.getObjectCount(); i++)
   {
      BfObject *bfObject = (BfObject *)mDockItems.getObjectByIndex(i);
      renderDockItem(bfObject, mLevel->getTeamColor(mCurrentTeam), mCurrentScale, mSnapVertexIndex);
      bfObject->setLitUp(false);    // TODO -- This should not be in render method
   }
}


void EditorUserInterface::renderDockItemLabel(const Point &pos, const char *label)
{
   F32 xpos = pos.x;
   F32 ypos = pos.y - DOCK_LABEL_SIZE / 2;
   mGL->glColor(Colors::white);
   RenderUtils::drawStringc(xpos, ypos + (F32)DOCK_LABEL_SIZE, (F32)DOCK_LABEL_SIZE, label);
}


void EditorUserInterface::renderDockPlugins() const
{
   S32 hoveredPlugin = mouseOnDock() ? findHitPlugin() : -1;
   S32 maxPlugins = getDockHeight() / PLUGIN_LINE_SPACING;
   for(S32 i = mDockPluginScrollOffset; i < mPluginInfos.size() && (i - mDockPluginScrollOffset) < maxPlugins; i++)
   {
      if(hoveredPlugin == i)
      {
         S32 x = DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin;
         F32 y = 1.5f * vertMargin + PLUGIN_LINE_SPACING * (i - mDockPluginScrollOffset);

         RenderUtils::drawHollowRect(x + horizMargin / 3, y, x + mDockWidth - horizMargin / 3, y + PLUGIN_LINE_SPACING, Colors::white);
      }

      mGL->glColor(Colors::white);

      S32 y = (S32)(1.5 * vertMargin + PLUGIN_LINE_SPACING * (i - mDockPluginScrollOffset + 0.33));
      RenderUtils::drawString((S32)(DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin / 2), y, DOCK_LABEL_SIZE, mPluginInfos[i].prettyName.c_str());

      S32 bindingWidth = RenderUtils::getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
      RenderUtils::drawString((S32)(DisplayManager::getScreenInfo()->getGameCanvasWidth() - bindingWidth - horizMargin * 1.5), y, DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
   }
}


void EditorUserInterface::renderSaveMessage() const
{
   if(mSaveMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(mSaveMsgTimer.getCurrent() < (U32)ONE_SECOND)
         alpha = (F32)mSaveMsgTimer.getCurrent() / 1000;

      const S32 textsize = 25;
      const S32 len = RenderUtils::getStringWidth(textsize, mSaveMsg.c_str()) + 20;
      const S32 inset = min((DisplayManager::getScreenInfo()->getGameCanvasWidth() - len) / 2, 200);
      const S32 boxTop = 515;
      const S32 boxBottom = 555;
      const S32 cornerInset = 10;

      // Fill
      mGL->glColor(Colors::black, alpha * 0.80f);
      RenderUtils::drawFancyBox(inset, boxTop, DisplayManager::getScreenInfo()->getGameCanvasWidth() - inset, boxBottom, cornerInset, GLOPT::TriangleFan);

      // Border
      mGL->glColor(Colors::blue, alpha);
      RenderUtils::drawFancyBox(inset, boxTop, DisplayManager::getScreenInfo()->getGameCanvasWidth() - inset, boxBottom, cornerInset, GLOPT::LineLoop);

      mGL->glColor(mSaveMsgColor, alpha);
      RenderUtils::drawCenteredString(520, textsize, mSaveMsg.c_str());
   }
}


static const S32 WARN_MESSAGE_FADE_TIME = 500;

void EditorUserInterface::renderWarnings() const
{
   FontManager::pushFontContext(EditorWarningContext);

   if(mWarnMsgTimer.getCurrent())
   {
      F32 alpha = 1.0;
      if(mWarnMsgTimer.getCurrent() < WARN_MESSAGE_FADE_TIME)
         alpha = (F32)mWarnMsgTimer.getCurrent() / WARN_MESSAGE_FADE_TIME;

      mGL->glColor(mWarnMsgColor, alpha);
      RenderUtils::drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() / 4, 25, mWarnMsg1.c_str());
      RenderUtils::drawCenteredString(DisplayManager::getScreenInfo()->getGameCanvasHeight() / 4 + 30, 25, mWarnMsg2.c_str());
   }

   if(mLevelErrorMsgs.size() || mLevelWarnings.size())
   {
      S32 ypos = vertMargin + 20;

      mGL->glColor(Colors::ErrorMessageTextColor);

      for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
      {
         RenderUtils::drawCenteredString(ypos, 20, mLevelErrorMsgs[i].c_str());
         ypos += 25;
      }

      mGL->glColor(Colors::yellow);

      for(S32 i = 0; i < mLevelWarnings.size(); i++)
      {
         RenderUtils::drawCenteredString(ypos, 20, mLevelWarnings[i].c_str());
         ypos += 25;
      }
   }

   FontManager::popFontContext();
}


void EditorUserInterface::renderLingeringMessage() const
{
   mLingeringMessage.render(horizMargin, vertMargin + mLingeringMessage.getHeight(), AlignmentLeft);
}


////////////////////////////////////////
////////////////////////////////////////
/*
1. User creates wall by drawing line
2. Each line segment is converted to a series of endpoints, who's location is adjusted to improve rendering (extended)
3. Those endpoints are used to generate a series of WallSegment objects, each with 4 corners
4. Those corners are used to generate a series of edges on each WallSegment.  Initially, each segment has 4 corners
and 4 edges.
5. Segments are intsersected with one another, punching "holes", and creating a series of shorter edges that represent
the dark blue outlines seen in the game and in the editor.

If wall shape or location is changed steps 1-5 need to be repeated
If intersecting wall is changed, only steps 4 and 5 need to be repeated
If wall thickness is changed, steps 3-5 need to be repeated
*/


// Mark all objects in database as unselected
void EditorUserInterface::clearSelection(GridDatabase *database)
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->unselect();
   }
}


// Mark everything as selected
void EditorUserInterface::selectAll(GridDatabase *database)
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      obj->setSelected(true);
   }

   onSelectionChanged();
}


bool EditorUserInterface::anyItemsSelected(const GridDatabase *database) const
{
   const Vector<DatabaseObject *> *objList = database->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->isSelected())
         return true;
   }

   return false;
}


// Copy selection to the clipboard
void EditorUserInterface::copySelection()
{
   GridDatabase *database = getLevel();

   if(!anyItemsSelected(database))
      return;

   mClipboard.clear();

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         BfObject *objcopy = obj->copy();
         mClipboard.push_back(boost::shared_ptr<BfObject>(objcopy));
      }
   }
}


// Paste items on the clipboard
void EditorUserInterface::pasteSelection()
{
   if(mDraggingObjects)       // Pasting while dragging can cause crashes!!
      return;

   S32 objCount = mClipboard.size();

   if(objCount == 0)         // Nothing on clipboard, nothing to do
      return;

   Point pastePos = snapPoint(convertCanvasToLevelCoord(mMousePos));

   Point firstPoint = mClipboard[0]->getVert(0);

   Point offsetFromFirstPoint;
   Vector<BfObject *> copiedObjects(objCount);     // Reserve some space

   mUndoManager.startTransaction();

   for(S32 i = 0; i < objCount; i++)
   {
      offsetFromFirstPoint = firstPoint - mClipboard[i]->getVert(0);

      BfObject *newObject = mClipboard[i]->newCopy();
      newObject->moveTo(pastePos - offsetFromFirstPoint);
      geomChanged(newObject);

      copiedObjects.push_back(newObject);

      mUndoManager.saveAction(ActionCreate, newObject);
   }

   mUndoManager.endTransaction();

   getLevel()->addToDatabase(copiedObjects);

   for(S32 i = 0; i < copiedObjects.size(); i++)
      copiedObjects[i]->onAddedToEditor();

   // Rebuild wall outlines so engineered items will have something to snap to
   rebuildWallGeometry(mLevel.get());

   getLevel()->snapAllEngineeredItems(false);  // True would work?

   doneAddingObjects(copiedObjects);

   autoSave();
}


// Expand or contract selection by scale (i.e. resize)
void EditorUserInterface::scaleSelection(F32 scale)
{
   Level *level = getLevel();

   if(!anyItemsSelected(level) || scale < .01 || scale == 1)    // Apply some sanity checks; limits here are arbitrary
      return;

   // Find center of selection
   Point min, max;
   level->computeSelectionMinMax(min, max);
   Point ctr = (min + max) * 0.5;

   bool modifiedWalls = false;
   mLevel->beginBatchGeomUpdate();
   mUndoManager.startTransaction();

   const Vector<DatabaseObject *> *objList = level->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mUndoManager.saveChangeAction_before(obj);

         obj->scale(ctr, scale);
         geomChanged(obj);

         mUndoManager.saveChangeAction_after(obj);

         if(isWallType(obj->getObjectTypeNumber()))
            modifiedWalls = true;
      }
   }

   mUndoManager.endTransaction();

   Vector<WallSegment const *> segments = getSelectedWallsAndPolywallSegments(mLevel.get());

   mLevel->endBatchGeomUpdate(mLevel.get(), segments, mWallEdgePoints, modifiedWalls);
   rebuildSelectionOutline();

   autoSave();
}


bool EditorUserInterface::canRotate() const
{
   return !mDraggingObjects && anyItemsSelected(getLevel());
}


static const Point ORIGIN;

// Rotate selected objects around their center point by angle
void EditorUserInterface::rotateSelection(F32 angle, bool useOrigin)
{
   if(!canRotate())
      return;

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   if(useOrigin)
      clearPointOfRotation();

   else if(!mPointOfRotation)
   {
      mPointOfRotation = new Point();
      Vector<Point> centroidList;

      // Add all object centroids to a set for de-duplication.  We'll get the centroid of the set.
      set<Point> centroidSet;
      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected())
            centroidList.push_back(obj->getCentroid());
      }

      mPointOfRotation->set(findCentroid(centroidList, true));
   }

   const Point *center = useOrigin ? &ORIGIN : mPointOfRotation;

   mUndoManager.startTransaction();

   // Now do the actual rotation
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mUndoManager.saveChangeAction_before(obj);

         obj->rotateAboutPoint(*center, angle);
         geomChanged(obj);

         mUndoManager.saveChangeAction_after(obj);
      }
   }

   mUndoManager.endTransaction();

   autoSave();
}


void EditorUserInterface::setSelectionId(S32 id)
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())               // Should only be one
      {
         if(obj->getUserAssignedId() != id)     // Did the id actually change?
         {
            mUndoManager.saveChangeAction_before(obj);
            obj->setUserAssignedId(id, true);
            mUndoManager.saveChangeAction_after(obj);
         }
         break;
      }
   }
}


// Set the team affiliation of any selected items
void EditorUserInterface::setCurrentTeam(S32 currentTeam)
{
   if(currentTeam >= getTeamCount())
   {
      char msg[255];

      if(getTeamCount() == 1)
         dSprintf(msg, sizeof(msg), "Only 1 team has been configured.");
      else
         dSprintf(msg, sizeof(msg), "Only %d teams have been configured.", getTeamCount());

      setWarnMessage(msg, "Hit [F2] to configure teams.");

      return;
   }

   clearWarnMessage();
   mCurrentTeam = currentTeam;
   bool anyChanged = false;


   // Update all dock items to reflect new current team
   for(S32 i = 0; i < mDockItems.getObjectCount(); i++)
   {
      BfObject *bfObject = (BfObject *)mDockItems.getObjectByIndex(i);

      if(!bfObject->hasTeam())
         continue;

      if(currentTeam == TEAM_NEUTRAL && !bfObject->canBeNeutral())
         continue;

      if(currentTeam == TEAM_HOSTILE && !bfObject->canBeHostile())
         continue;

      setTeam(bfObject, currentTeam);
   }


   mUndoManager.startTransaction();

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mUndoManager.saveChangeAction_before(obj);

         if(!obj->hasTeam())
            continue;

         if(currentTeam == TEAM_NEUTRAL && !obj->canBeNeutral())
            continue;

         if(currentTeam == TEAM_HOSTILE && !obj->canBeHostile())
            continue;

         setTeam(obj, currentTeam);

         mUndoManager.saveChangeAction_after(obj);

         anyChanged = true;
      }
   }

   mUndoManager.endTransaction();

   // Overwrite any warnings set above.  If we have a group of items selected, it makes no sense to show a
   // warning if one of those items has the team set improperly.  The warnings are more appropriate if only
   // one item is selected, or none of the items are given a valid team setting.

   if(anyChanged)
   {
      clearWarnMessage();
      validateLevel();
      autoSave();
   }
}


void EditorUserInterface::flipSelectionHorizontal()
{
   Point min, max;
   getLevel()->computeSelectionMinMax(min, max);
   F32 centerX = (min.x + max.x) / 2;

   flipSelection(centerX, true);
}


void EditorUserInterface::flipSelectionVertical()
{
   Point min, max;
   getLevel()->computeSelectionMinMax(min, max);
   F32 centerY = (min.y + max.y) / 2;

   flipSelection(centerY, false);
}


void EditorUserInterface::flipSelection(F32 center, bool isHoriz)
{
   if(!canRotate())
      return;

   Level *level = getLevel();

   Point min, max;
   level->computeSelectionMinMax(min, max);

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   bool modifiedWalls = false;

   level->beginBatchGeomUpdate();
   mUndoManager.startTransaction();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mUndoManager.saveChangeAction_before(obj);

         obj->flip(center, isHoriz);
         geomChanged(obj);

         mUndoManager.saveChangeAction_after(obj);

         if(isWallType(obj->getObjectTypeNumber()))
            modifiedWalls = true;
      }
   }

   mUndoManager.endTransaction();

   Vector<WallSegment const *> segments = getSelectedWallsAndPolywallSegments(mLevel.get());

   mLevel->endBatchGeomUpdate(mLevel.get(), segments, mWallEdgePoints, modifiedWalls);
   rebuildSelectionOutline();

   autoSave();
}


static const S32 POINT_HIT_RADIUS = 9;
static const S32 EDGE_HIT_RADIUS = 6;

void EditorUserInterface::findHitItemAndEdge()
{
   mHitItem = NULL;
   mEdgeHit = NONE;
   mHitVertex = NONE;

   // Make hit rectangle larger than 1x1 -- when we consider point items, we need to make sure that we grab the item even when we're not right
   // on top of it, as the point item's hit target is much larger than the item itself.  50 is a guess that seems to work well.
   // Note that this is only used for requesting a candidate list from the database, actual hit detection is more precise.
   const Rect cursorRect((mMousePos - mCurrentOffset) / mCurrentScale, 50);

   fillVector.clear();
   GridDatabase *editorDb = getLevel();
   editorDb->findObjects((TestFunc)isAnyObjectType, fillVector, cursorRect);

   Point mouse = convertCanvasToLevelCoord(mMousePos);      // Figure out where the mouse is in level coords

   // Do this in two passes -- the first we only consider selected items, the second pass will consider all targets.
   // This will give priority to hitting vertices of selected items.
   for(S32 firstPass = 1; firstPass >= 0; firstPass--)     // firstPass will be true the first time through, false the second time
      for(S32 i = fillVector.size() - 1; i >= 0; i--)      // Go in reverse order to prioritize items drawn on top
      {
         BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

         TNLAssert(obj, "Expected a BfObject!");

         if(firstPass == (!obj->isSelected() && !obj->anyVertsSelected()))  // First pass is for selected items only
            continue;                                                       // Second pass only for unselected items

         if(checkForVertexHit(obj) || checkForEdgeHit(mouse, obj))
            return;
      }

   // We've already checked for wall vertices; now we'll check for hits in the interior of walls
   fillVector2.clear();

   mLevel->findObjects(isWallType, fillVector2, cursorRect);

   for(S32 i = 0; i < fillVector2.size(); i++)
      if(overlaps(mouse, static_cast<BfObject *>(fillVector2[i])))
         return;

   // If we're still here, it means we didn't find anything yet.  Make one more pass, and see if we're in any polys.
   // This time we'll loop forward, though I don't think it really matters.
   for(S32 i = 0; i < fillVector.size(); i++)
      if(overlaps(mouse, static_cast<BfObject *>(fillVector[i])))
         return;
}


// Vertex is weird because we don't always do thing in level coordinates -- some of our hit computation is based on
// absolute screen coordinates; some things, like wall vertices, are the same size at every zoom scale.  
bool EditorUserInterface::checkForVertexHit(BfObject *object)
{
   F32 radius = object->getEditorRadius(mCurrentScale);

   for(S32 i = object->getVertCount() - 1; i >= 0; i--)
   {
      // p represents pixels from mouse to obj->getVert(j), at any zoom
      Point p = mMousePos - mCurrentOffset - (object->getVert(i) + object->getEditorSelectionOffset(mCurrentScale)) * mCurrentScale;

      if(fabs(p.x) < radius && fabs(p.y) < radius)
      {
         mHitItem = object;
         mHitVertex = i;
         return true;
      }
   }

   return false;
}


bool EditorUserInterface::checkForEdgeHit(const Point &point, BfObject *object)
{
   // Points have no edges, and walls are checked via another mechanism
   if(object->getGeomType() == geomPoint)
      return false;

   const Vector<Point> &verts = *object->getEditorHitPoly();
   TNLAssert(verts.size() > 0, "Empty vertex problem -- if debugging, check what kind of object 'object' is, and see "
      "if you can figure out why it has no verts");
   if(verts.size() == 0)
      return false;

   bool loop = (object->getGeomType() == geomPolygon);

   Point closest;

   S32 j_prev = loop ? (verts.size() - 1) : 0;

   for(S32 j = loop ? 0 : 1; j < verts.size(); j++)
   {
      if(findNormalPoint(point, verts[j_prev], verts[j], closest))
      {
         F32 distance = (point - closest).len();
         if(distance < EDGE_HIT_RADIUS / mCurrentScale)
         {
            mHitItem = object;
            mEdgeHit = j_prev;

            return true;
         }
      }
      j_prev = j;
   }

   return false;
}


// Returns true if point overlaps object
bool EditorUserInterface::overlaps(const Point &point, BfObject *object)
{
   if(object->overlapsPoint(point))
   {
      mHitItem = object;
      return true;
   }

   return false;
}


// Sets mDockItemHit
void EditorUserInterface::findHitItemOnDock()
{
   mDockItemHit = NULL;

   for(S32 i = 0; i < mDockItems.getObjectCount(); i++)
   {
      BfObject *bfObject = (BfObject *)mDockItems.getObjectByIndex(i);

      Point pos = bfObject->getPos();

      if(fabs(mMousePos.x - pos.x) < POINT_HIT_RADIUS && fabs(mMousePos.y - pos.y) < POINT_HIT_RADIUS)
      {
         mDockItemHit = bfObject;
         return;
      }
   }

   // Now check for polygon interior hits
   for(S32 i = 0; i < mDockItems.getObjectCount(); i++)
   {
      BfObject *bfObject = (BfObject *)mDockItems.getObjectByIndex(i);

      if(bfObject->getGeomType() == geomPolygon)
      {
         Vector<Point> verts;
         for(S32 j = 0; j < bfObject->getVertCount(); j++)
            verts.push_back(bfObject->getVert(j));

         if(polygonContainsPoint(verts.address(), verts.size(), mMousePos))
         {
            mDockItemHit = bfObject;
            return;
         }
      }
   }

   return;
}


S32 EditorUserInterface::findHitPlugin() const
{
   S32 i;
   for(i = 0; i < mPluginInfos.size(); i++)
      if(mMousePos.y > 1.5 * vertMargin + PLUGIN_LINE_SPACING * i &&
         mMousePos.y < 1.5 * vertMargin + PLUGIN_LINE_SPACING * (i + 1))
         return i + mDockPluginScrollOffset;

   return -1;
}


void EditorUserInterface::onMouseMoved()
{
   Parent::onMouseMoved();

   if(mIgnoreMouseInput)  // Needed to avoid freezing effect from too many mouseMoved events without a render in between
      return;

   mIgnoreMouseInput = true;

   setMousePos();

   // If any button is down, we consider ourselves to be in drag mode.
   // Doing this with MOUSE_RIGHT allows you to drag a vertex you just placed by holding the right-mouse button.
   if(InputCodeManager::getState(MOUSE_LEFT) || InputCodeManager::getState(MOUSE_RIGHT) || InputCodeManager::getState(MOUSE_MIDDLE))
   {
      onMouseDragged();
      return;
   }

   if(mCreatingPoly || mCreatingPolyline)
      return;

   // Turn off highlight on selected item -- will be turned back on for this object or another below
   if(mHitItem)
      mHitItem->setLitUp(false);

   findHitItemAndEdge();      //  Sets mHitItem, mHitVertex, and mEdgeHit
   findHitItemOnDock();

   bool spaceDown = InputCodeManager::getState(KEY_SPACE);

   // Highlight currently selected item
   if(mHitItem)
      mHitItem->setLitUp(true);

   if(mVertexEditMode)
   {
      // We hit a vertex that wasn't already selected
      if(!spaceDown && mHitItem && mHitVertex != NONE && !mHitItem->vertSelected(mHitVertex))
         mHitItem->setVertexLitUp(mHitVertex);

      findSnapVertex();
   }

   Cursor::enableCursor();
}


// Stores the current mouse position in mMousePos
void EditorUserInterface::setMousePos()
{
   mMousePos.set(DisplayManager::getScreenInfo()->getMousePos());
}


// onDrag
void EditorUserInterface::onMouseDragged()
{
   if(InputCodeManager::getState(MOUSE_MIDDLE) && mMousePos != mScrollWithMouseLocation)
   {
      mCurrentOffset += mMousePos - mScrollWithMouseLocation;
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = false;

      return;
   }

   if(mCreatingPoly || mCreatingPolyline || mDragSelecting)
      return;

   if(mDraggingDockItem.isValid())      // We just started dragging an item off the dock
      startDraggingDockItem();

   findSnapVertex();                               // Sets mSnapObject and mSnapVertexIndex
   if(!mSnapObject || mSnapVertexIndex == NONE)    // If we've just started dragging a dock item, this will be it
      return;

   mDelayedUnselectObject = NULL;

   if(!mDraggingObjects)
      onMouseDragged_startDragging();

   SDL_SetCursor(Cursor::getSpray());

   Point lastSnapDelta = mSnapDelta;
   // The thinking here is that for large items -- walls, polygons, etc., we may grab an item far from its snap vertex, and we
   // want to factor that offset into our calculations.  For point items (and vertices), we don't really care about any slop
   // in the selection, and we just want the damn thing where we put it.
   Point p;

   if(mSnapObject->getGeomType() == geomPoint || (mHitItem && mHitItem->anyVertsSelected()))
      p = snapPoint(convertCanvasToLevelCoord(mMousePos));
   else  // larger items
      p = snapPoint(convertCanvasToLevelCoord(mMousePos) + mMoveOrigin - mMouseDownPos);

   mSnapDelta = p - mMoveOrigin;

   // Nudge all selected objects by incremental move amount
   translateSelectedItems(mSnapDelta, lastSnapDelta);

   // Snap all selected engr. objects if possible
   snapSelectedEngineeredItems(mSnapDelta);
}


// onStartDragging
void EditorUserInterface::onMouseDragged_startDragging()
{
   mMoveOrigin = mSnapObject->getVert(mSnapVertexIndex);
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   mSelectedObjectsForDragging.clear();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected() || obj->anyVertsSelected())
         mSelectedObjectsForDragging.push_back(obj->clone());
   }

#ifdef TNL_OS_MAC_OSX 
   bool ctrlDown = InputCodeManager::getState(KEY_META);
#else
   bool ctrlDown = InputCodeManager::getState(KEY_CTRL);
#endif

   if(ctrlDown)     // Ctrl+Drag ==> copy and drag (except for Mac)
      onMouseDragged_copyAndDrag(objList);

   onSelectionChanged();
   mDraggingObjects = true;
   mSnapDelta.set(0, 0);

   markSelectedObjectsAsUnsnapped2(objList);
}


// Copy objects and start dragging the copies
void EditorUserInterface::onMouseDragged_copyAndDrag(const Vector<DatabaseObject *> *objList)
{
   Vector<BfObject *> copiedObjects;

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         BfObject *newObject = obj->newCopy();
         newObject->setSelected(true);

         copiedObjects.push_back(newObject);

         // Make mHitItem be the new copy of the old mHitItem
         if(mHitItem == obj)
            mHitItem = newObject;

         if(mSnapObject == obj)
            mSnapObject = newObject;
      }
   }

   mDragCopying = true;

   // Now mark source objects as unselected
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      obj->setSelected(false);
      obj->setLitUp(false);
   }

   for(S32 i = 0; i < copiedObjects.size(); i++)
      addToEditor(copiedObjects[i]);

   //onSelectionChanged();  should we call this??
   rebuildWallGeometry(mLevel.get());
   clearPointOfRotation();
}


void EditorUserInterface::translateSelectedItems(const Point &offset, const Point &lastOffset)
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   S32 k = 0;
   for(S32 i = 0; i < objList->size(); i++)  // k changes in this loop
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected() || obj->anyVertsSelected())
      {
         Point newVert;    // Reusable container

         Point dragOffset = (mSelectedObjectsForDragging[k]->getVert(0) - obj->getVert(0)) + offset;

         for(S32 j = obj->getVertCount() - 1; j >= 0; j--)
         {
            if(obj->isSelected())            // ==> Dragging whole object
            {
               newVert = obj->getVert(j) + dragOffset;

               obj->setVert(newVert, j);

               obj->onItemDragging();        // Let the item know it's being dragged
            }
            else if(obj->vertSelected(j))    // ==> Dragging individual vertex
            {
               // Pos of vert at last tick + Offset from last tick
               newVert = obj->getVert(j) + (offset - lastOffset);

               obj->setVert(newVert, j);
               obj->onGeomChanging();        // Because, well, the geom is changing

               if(isWallType(obj->getObjectTypeNumber()))
                  rebuildSelectionOutline();
            }
         }

         k++;
      }
   }
}


Point EditorUserInterface::snapToConstrainedLine(const Point &point) const
{
   Point mousePos = convertCanvasToLevelCoord(mMousePos);     

   Vector<Point> candidates(12);
   candidates.push_back(Point(point.x, mMoveOrigin.y));                  // Horizontal
   candidates.push_back(Point(mMoveOrigin.x, point.y));                  // Vertical
   candidates.push_back(pointOnLine(point, mMoveOrigin, Point(10,10)));  // Diagonal going up and to the right
   candidates.push_back(pointOnLine(point, mMoveOrigin, Point(10,-10))); // Diagonal going up and to the left

   // Add some other multiples of 15 deg
   S32 angles[] = { 15, 30, 60, 75, 105, 120, 150, 165 };
   for(S32 i = 0; i < ARRAYSIZE(angles); i++)
      candidates.push_back(pointOnLine(point, mMoveOrigin, Point(100 * cos(angles[i] * FloatPi / 180), 100 * sin(angles[i] * FloatPi / 180)))); 

   F32 dist = mousePos.distSquared(candidates[0]);
   S32 closest = 0;

   for(S32 i = 1; i < candidates.size(); i++)
   {
      F32 d = mousePos.distSquared(candidates[i]);
      if(d < dist)
      {
         closest = i;
         dist = d;
      }
   }

   return candidates[closest];
}


void EditorUserInterface::snapSelectedEngineeredItems(const Point &cumulativeOffset)
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      if(isEngineeredType(objList->get(i)->getObjectTypeNumber()))
      {
         EngineeredItem *engrObj = static_cast<EngineeredItem *>(objList->get(i));

         // Do not snap objects that are being dragged if their mounted wall is also being dragged
         if(mDraggingObjects && engrObj->isSelected() && engrObj->isSnapped() && engrObj->getMountSegment()->isSelected())
            continue;

         // Only try to mount any items that are both 1) selected and 2) marked as wanting to snap
         S32 j = 0;
         if(engrObj->isSelected())
         {
            engrObj->mountToWall(snapPointConstrainedOrLevelGrid(mSelectedObjectsForDragging[j]->getVert(0) + cumulativeOffset),
               getLevel(), mLevel->getWallEdgeDatabase());
            j++;
         }
      }
   }
}


BfObject *EditorUserInterface::copyDockItem(BfObject *source)
{
   // Instantiate object so we are essentially dragging a non-dock item
   BfObject *newObject = source->newCopy();
   newObject->newObjectFromDock((F32)mGridSize);     // Do things particular to creating an object that came from dock

   return newObject;
}


// User just dragged an item off the dock
void EditorUserInterface::startDraggingDockItem()
{
   BfObject *item(copyDockItem(mDraggingDockItem));   // This is a new object, will need to be deleted somewhere, somehow

   // Offset lets us drag an item out from the dock by an amount offset from the 0th vertex.  This makes placement seem more natural.
   Point pos = convertCanvasToLevelCoord(mMousePos) - item->getInitialPlacementOffset(mGridSize);
   item->moveTo(pos);

   addToEditor(item);

   // Create an undo action to remove this item
   mUndoManager.saveCreateActionAndMergeWithNextUndoState();

   mDraggingDockItem = NULL;     // Because now we're dragging a real item

   doneAddingObjects(item);


   // Because we sometimes have trouble finding an item when we drag it off the dock, after it's been sorted,
   // we'll manually set mHitItem based on the selected item, which will always be the one we just added.
   // TODO: Still needed?

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   mEdgeHit = NONE;
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         mHitItem = obj;
         break;
      }
   }
}


void EditorUserInterface::doneAddingObjects(S32 serialNumber)
{
   doneAddingObjects(mLevel->findObjBySerialNumber(serialNumber));
}


void EditorUserInterface::doneAddingObjects(const Vector<S32> &serialNumbers)
{
   Vector<BfObject *> bfObjects(serialNumbers.size());

   for(S32 i = 0; i < serialNumbers.size(); i++)
      bfObjects.push_back(mLevel->findObjBySerialNumber(serialNumbers[i]));

   doneChangingGeoms(bfObjects);
}


void EditorUserInterface::doneAddingObjects(BfObject *bfObject)
{
   Vector<BfObject *> bfObjects;
   bfObjects.push_back(bfObject);

   doneAddingObjects(bfObjects);
}


void EditorUserInterface::doneChangingGeoms(const Vector<S32> &serialNumbers)
{
   Vector<BfObject *> bfObjects(serialNumbers.size());

   for(S32 i = 0; i < serialNumbers.size(); i++)
      bfObjects.push_back(mLevel->findObjBySerialNumber(serialNumbers[i]));

   doneChangingGeoms(bfObjects);
}


void EditorUserInterface::doneAddingObjects(const Vector<BfObject *> &bfObjects)
{
   clearSelection(getLevel());   // No items are selected...

   for(S32 i = 0; i < bfObjects.size(); i++)
   {
      BfObject *obj = mLevel->findObjBySerialNumber(bfObjects[i]->getSerialNumber());
      obj->setSelected(true);  // ...except for the new ones;
   }

   onSelectionChanged();
   validateLevel();              // Check level for errors
}


void EditorUserInterface::doneChangingGeoms(S32 serialNumber)
{
   doneChangingGeoms(mLevel->findObjBySerialNumber(serialNumber));
}


void EditorUserInterface::doneChangingGeoms(BfObject *bfObject)
{
   Vector<BfObject *> bfObjects;
   bfObjects.push_back(bfObject);

   doneChangingGeoms(bfObjects);
}


void EditorUserInterface::doneChangingGeoms(const Vector<BfObject *> &bfObjects)
{
   rebuildEverything(getLevel());
}


// Sets mSnapObject and mSnapVertexIndex based on the vertex closest to the cursor that is part of the selected set
// What we really want is the closest vertex in the closest feature
void EditorUserInterface::findSnapVertex()
{
   F32 closestDist = F32_MAX;

   if(mDraggingObjects)    // Don't change snap vertex once we're dragging
      return;

   clearSnapEnvironment();

   Point mouseLevelCoord = convertCanvasToLevelCoord(mMousePos);

   // If we have a hit item, and it's selected, find the closest vertex in the item
   if(mHitItem.isValid() && mHitItem->isSelected())
   {
      // If we've hit an edge, restrict our search to the two verts that make up that edge
      if(mEdgeHit != NONE)
      {
         mSnapObject = mHitItem;     // Regardless of vertex, this is our hit item
         S32 v1 = mEdgeHit;
         S32 v2 = mEdgeHit + 1;

         // Handle special case of looping item
         if(mEdgeHit == mHitItem->getVertCount() - 1)
            v2 = 0;

         // Find closer vertex: v1 or v2
         mSnapVertexIndex = (mHitItem->getVert(v1).distSquared(mouseLevelCoord) <
            mHitItem->getVert(v2).distSquared(mouseLevelCoord)) ? v1 : v2;
         return;
      }

      // Didn't hit an edge... find the closest vertex anywhere in the item
      for(S32 j = 0; j < mHitItem->getVertCount(); j++)
      {
         F32 dist = mHitItem->getVert(j).distSquared(mouseLevelCoord);

         if(dist < closestDist)
         {
            closestDist = dist;
            mSnapObject = mHitItem;
            mSnapVertexIndex = j;
         }
      }

      return;
   }

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   // Otherwise, we don't have a selected hitItem -- look for a selected vertex
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      for(S32 j = 0; j < obj->getVertCount(); j++)
      {
         F32 dist = obj->getVert(j).distSquared(mouseLevelCoord);

         if(obj->vertSelected(j) && dist < closestDist)
         {
            closestDist = dist;
            mSnapObject = obj;
            mSnapVertexIndex = j;
         }
      }
   }
}


// Delete selected items (true = items only, false = items & vertices)
void EditorUserInterface::deleteSelection(bool objectsOnly)
{
   if(mDraggingObjects)     // No deleting while we're dragging, please...
      return;

   if(!anythingSelected())  // Nothing to delete
      return;

   mUndoManager.startTransaction();

   bool deletedWall = false;
   bool deletedAny = false;

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = objList->size() - 1; i >= 0; i--)  // Reverse to avoid having to have i-- in middle of loop
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         // Since indices change as items are deleted, this will keep incorrect items from being deleted
         if(obj->isLitUp())
            mHitItem = NULL;

         mUndoManager.saveAction(Editor::ActionDelete, obj);

         if(isWallType(obj->getObjectTypeNumber()))
            deletedWall = true;

         deleteItem(i, true);
         deletedAny = true;
      }
      else if(!objectsOnly)      // Deleted only selected vertices
      {
         bool geomChanged = false;

         BfObject *origObj = obj->clone();      // Will be cleaned up by UndoManager

         // Backwards!  Since we could be deleting multiple at once
         for(S32 j = obj->getVertCount() - 1; j > -1; j--)
         {
            if(obj->vertSelected(j))
            {
               obj->deleteVert(j);

               geomChanged = true;
               clearSnapEnvironment();
            }
         }

         // Check if item has too few vertices left to be viable
         if(obj->getVertCount() < obj->getMinVertCount())
         {
            mUndoManager.saveAction(Editor::ActionDelete, origObj);
            if(isWallType(obj->getObjectTypeNumber()))
               deletedWall = true;

            deleteItem(i, true);
         }
         else if(geomChanged)
         {
            mUndoManager.saveAction(Editor::ActionChange, origObj, obj);
            obj->onGeomChanged();
         }

      }  // else if(!objectsOnly) 
   }  // for


   mUndoManager.endTransaction();

   if(deletedWall)
      rebuildWallGeometry(mLevel.get());


   if(deletedAny)
   {
      autoSave();
      doneDeletingObjects();
   }
}


// Increase selected wall thickness by amt
void EditorUserInterface::changeBarrierWidth(S32 amt)
{
   fillVector2.clear();    // fillVector gets modified in some child function, so use our secondary reusable container
   getLevel()->findObjects((TestFunc)isWallItemType, fillVector2);

   mUndoManager.startTransaction();

   for(S32 i = 0; i < fillVector2.size(); i++)
   {
      WallItem *obj = static_cast<WallItem *>(fillVector2[i]);

      if(obj->isSelected())
      {
         mUndoManager.saveChangeAction_before(obj);
         obj->changeWidth(amt);
         mUndoManager.saveChangeAction_after(obj);
      }
   }

   mUndoManager.endTransaction(EditorUndoManager::ChangeIdMergeWalls);
}


// Split wall/barrier on currently selected vertex/vertices
// Or, if entire wall is selected, split on snapping vertex -- this seems a natural way to do it
void EditorUserInterface::splitBarrier()
{
   bool split = false;

   GridDatabase *database = getLevel();

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   mUndoManager.startTransaction();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getGeomType() == geomPolyLine)
         for(S32 j = 1; j < obj->getVertCount() - 1; j++)     // Can't split on end vertices!
            if(obj->vertSelected(j))
            {
               doSplit(obj, j);

               split = true;
               goto done2;       // Yes, gotos are naughty, but they just feel so good...
            }
   }

   // If we didn't find a suitable selected vertex to split on, look for a selected line with a magenta vertex
   if(!split && mSnapObject && mSnapObject->getGeomType() == geomPolyLine && mSnapObject->isSelected() &&
      mSnapVertexIndex != NONE && mSnapVertexIndex != 0 && mSnapVertexIndex != mSnapObject->getVertCount() - 1)
   {
      doSplit(mSnapObject, mSnapVertexIndex);
      split = true;
   }

done2:
   mUndoManager.endTransaction();

   if(split)
   {
      clearSelection(database);
      autoSave();
   }
}


// Split wall or line -- will probably crash on other geom types
void EditorUserInterface::doSplit(BfObject *object, S32 vertex)
{
   TNLAssert(mUndoManager.inTransaction(), "Should have opened an undo transaction before calling this!");

   BfObject *newObj = object->newCopy();    // Copy the attributes
   newObj->clearVerts();                    // Wipe out the geometry

   // Note that it would be more efficient to start at the end and work backwards, but the direction of our numbering would be
   // reversed in the new object compared to what it was.  This isn't important at the moment, but it just seems wrong from a
   // geographic POV.  Which I have.

   mUndoManager.saveChangeAction_before(object);
   for(S32 i = vertex; i < object->getVertCount(); i++)
   {
      newObj->addVert(object->getVert(i), true);      // true: If wall already has more than max number of points, let children have more as well
      if(i != vertex)               // i.e. if this isn't the first iteration
      {
         object->deleteVert(i);     // Don't delete first vertex -- we need it to remain as final vertex of old feature
         i--;
      }
   }

   addToEditor(newObj);     // Needs to happen before onGeomChanged, so mGame will not be NULL

   // Tell the new segments that they have new geometry
   object->onGeomChanged();
   newObj->onGeomChanged();

   rebuildWallGeometry(mLevel.get());

   mUndoManager.saveChangeAction_after(object);
   mUndoManager.saveAction(ActionCreate, newObj);
}


// Join two or more sections of wall that have coincident end points.  Will ignore invalid join attempts.
// Will also merge two or more overlapping polygons.
void EditorUserInterface::joinBarrier()
{
   GridDatabase *database = getLevel();

   BfObject *joinedObj = NULL;

   mUndoManager.startTransaction();

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size() - 1; i++)
   {
      BfObject *obj_i = static_cast<BfObject *>(objList->get(i));

      // Will work for both lines and walls, or any future polylines
      if((obj_i->getGeomType() == geomPolyLine) && obj_i->isSelected())
      {
         joinedObj = doMergeLines(obj_i, i);
         if(joinedObj == NULL)
         {
            mUndoManager.rollbackTransaction();
            return;
         }

         break;
      }
      else if(obj_i->getGeomType() == geomPolygon && obj_i->isSelected())
      {
         joinedObj = doMergePolygons(obj_i, i);
         if(joinedObj == NULL)
         {
            mUndoManager.rollbackTransaction();
            return;
         }

         break;
      }
   }

   // We had a successful merger
   clearSelection(database);
   autoSave();
   joinedObj->onGeomChanged();
   joinedObj->setSelected(true);

   onSelectionChanged();
   rebuildWallGeometry(mLevel.get());

   mUndoManager.endTransaction();
}


BfObject *EditorUserInterface::doMergePolygons(BfObject *firstItem, S32 firstItemIndex)
{
   TNLAssert(mUndoManager.inTransaction(), "Should be in an undo transaction here!");

   Vector<const Vector<Point> *> inputPolygons;
   Vector<Vector<Point> > outputPolygons;
   Vector<S32> deleteList;

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   inputPolygons.push_back(firstItem->getOutline());

   // Make sure all our polys are wound the same direction as the first
   bool cw = isWoundClockwise(*firstItem->getOutline());

   for(S32 i = firstItemIndex + 1; i < objList->size(); i++)   // Compare against remaining objects
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->getObjectTypeNumber() == firstItem->getObjectTypeNumber() && obj->isSelected())
      {
         // Reverse winding in place -- if merge succeeds, the poly will be deleted, and if it fails we'll revert it
         if(isWoundClockwise(*obj->getOutline()) != cw)
         {
            mUndoManager.saveChangeAction_before(obj);
            obj->reverseWinding();
            mUndoManager.saveChangeAction_after(obj);
         }

         inputPolygons.push_back(obj->getOutline());
         deleteList.push_back(i);
      }
   }

   if(!mergePolys(inputPolygons, outputPolygons) || outputPolygons.size() != 1)
      return NULL;

   mUndoManager.saveChangeAction_before(firstItem);

   // Clear out the polygon, back to front
   while(firstItem->getVertCount() > 0)
      firstItem->deleteVert(firstItem->getVertCount() - 1);

   // Add the new points
   for(S32 i = 0; i < outputPolygons[0].size(); i++)
   {
      bool ok = firstItem->addVert(outputPolygons[0][i], true);
      TNLAssert(ok, "Should always return true!");
   }

   mUndoManager.saveChangeAction_after(firstItem);

   // Delete the constituent parts; work backwards to avoid queering the deleteList indices
   for(S32 i = deleteList.size() - 1; i >= 0; i--)
   {
      mUndoManager.saveAction(ActionDelete, static_cast<BfObject *>(objList->get(deleteList[i])));
      deleteItem(deleteList[i]);
   }

   return firstItem;
}


// First vertices are the same  1 2 3 | 1 4 5
static void merge_1_2_3__1_4_5(BfObject *firstItem, BfObject *mergingWith)
{
   // Skip first vertex, because it would be a dupe
   for(S32 a = 1; a < mergingWith->getVertCount(); a++)
      firstItem->addVertFront(mergingWith->getVert(a));
}


// First vertex conincides with final vertex 3 2 1 | 5 4 3
static void merge_3_2_1__5_4_3(BfObject *firstItem, BfObject *mergingWith)
{
   for(S32 a = mergingWith->getVertCount() - 2; a >= 0; a--)
      firstItem->addVertFront(mergingWith->getVert(a));
}


// Last vertex conincides with first 1 2 3 | 3 4 5
static void merge_1_2_3__3_4_5(BfObject *firstItem, BfObject *mergingWith)
{
   // Skip first vertex, because it would be a dupe
   for(S32 a = 1; a < mergingWith->getVertCount(); a++)
      firstItem->addVert(mergingWith->getVert(a));
}


// Last vertices coincide  1 2 3 | 5 4 3
static void merge_1_2_3__5_4_3(BfObject *firstItem, BfObject *mergingWith)
{
   for(S32 j = mergingWith->getVertCount() - 2; j >= 0; j--)
      firstItem->addVert(mergingWith->getVert(j));
}


BfObject *EditorUserInterface::doMergeLines(BfObject *firstItem, BfObject *mergingWith, S32 mergingWithIndex,
   void(*mergeFunction)(BfObject *, BfObject *))
{
   BfObject *joinedObj = firstItem;

   mUndoManager.saveChangeAction_before(firstItem);

   mergeFunction(firstItem, mergingWith);

   mUndoManager.saveChangeAction_after(firstItem);
   mUndoManager.saveAction(ActionDelete, mergingWith);

   deleteItem(mergingWithIndex);    // Deletes mergingWith, but slightly more efficiently since we already know the index

   return joinedObj;
}


BfObject *EditorUserInterface::doMergeLines(BfObject *firstItem, S32 firstItemIndex)
{
   TNLAssert(mUndoManager.inTransaction(), "Should have opened an undo transaction before calling this!");

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();
   BfObject *joinedObj = NULL;

   for(S32 i = firstItemIndex + 1; i < objList->size(); i++)              // Compare against remaining objects
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->getObjectTypeNumber() == firstItem->getObjectTypeNumber() && obj->isSelected())
      {
         // Don't join if resulting object would be too big!
         if(firstItem->getVertCount() + obj->getVertCount() > Geometry::MAX_POLY_POINTS)
            continue;

         if(firstItem->getVert(0).distSquared(obj->getVert(0)) < .0001)   // First vertices are the same  1 2 3 | 1 4 5
         {
            joinedObj = doMergeLines(firstItem, obj, i, merge_1_2_3__1_4_5);
            i--;
         }

         // First vertex conincides with final vertex 3 2 1 | 5 4 3
         else if(firstItem->getVert(0).distSquared(obj->getVert(obj->getVertCount() - 1)) < .0001)
         {
            joinedObj = doMergeLines(firstItem, obj, i, merge_3_2_1__5_4_3);
            i--;
         }

         // Last vertex conincides with first 1 2 3 | 3 4 5
         else if(firstItem->getVert(firstItem->getVertCount() - 1).distSquared(obj->getVert(0)) < .0001)
         {
            joinedObj = doMergeLines(firstItem, obj, i, merge_1_2_3__3_4_5);
            i--;
         }

         // Last vertices coincide  1 2 3 | 5 4 3
         else if(firstItem->getVert(firstItem->getVertCount() - 1).distSquared(obj->getVert(obj->getVertCount() - 1)) < .0001)
         {
            joinedObj = doMergeLines(firstItem, obj, i, merge_1_2_3__5_4_3);
            i--;
         }
      }
   }

   return joinedObj;
}


// batchMode defaults to false
void EditorUserInterface::deleteItem(S32 itemIndex, bool batchMode)
{
   mLevel->removeFromDatabase(itemIndex, true);    // true ==> object will be deleted

   if(!batchMode)
      doneDeletingObjects();
}


// After deleting a bunch of items, clean up
void EditorUserInterface::doneDeleteingWalls()
{
   rebuildWallGeometry(mLevel.get());
}


void EditorUserInterface::doneDeletingObjects()
{
   // Reset a bunch of things
   clearSnapEnvironment();
   validateLevel();
   onMouseMoved();   // Reset cursor  
}


// Only called when user presses a hotkey to insert an item -- may crash if item to be inserted is not currently on the dock!
void EditorUserInterface::insertNewItem(U8 itemTypeNumber)
{
   if(mDraggingObjects)     // No inserting when items are being dragged!
      return;

   BfObject *newObject = NULL;

   // Find a dockItem to copy
   for(S32 i = 0; i < mDockItems.getObjectCount(); i++)
   {
      BfObject *bfObject = (BfObject *)mDockItems.getObjectByIndex(i);

      if(bfObject->getObjectTypeNumber() == itemTypeNumber)
      {
         newObject = copyDockItem(bfObject);
         break;
      }
   }

   // May occur if requested item is not currently on the dock
   TNLAssert(newObject, "Couldn't create object in insertNewItem()");
   if(!newObject)
      return;

   newObject->moveTo(snapPoint(convertCanvasToLevelCoord(mMousePos)));
   addToEditor(newObject);
   mUndoManager.saveAction(Editor::ActionCreate, newObject);

   doneAddingObjects(newObject);

   autoSave();
}


void EditorUserInterface::centerView(bool isScreenshot)
{
   Rect extents = getLevel()->getExtents();
   Rect levelgenDbExtents = mLevelGenDatabase.getExtents();

   if(levelgenDbExtents.getWidth() > 0 || levelgenDbExtents.getHeight() > 0)
      extents.unionRect(levelgenDbExtents);

   // If we have nothing, or maybe only one point object in our level
   if(extents.getWidth() < 1 && extents.getHeight() < 1)    // e.g. a single point item
   {
      mCurrentScale = STARTING_SCALE;
      setDisplayCenter(extents.getCenter());
   }
   else
   {
      if(isScreenshot)
      {
         // Expand just slightly so we don't clip edges
         extents.expand(Point(2, 2));
         setDisplayExtents(extents, 1.0f);
      }
      else
         setDisplayExtents(extents, 1.3f);
   }
}


F32 EditorUserInterface::getCurrentScale() const
{
   return mCurrentScale;
}


Point EditorUserInterface::getCurrentOffset() const
{
   return mCurrentOffset;
}


// Positive amounts are zooming in, negative are zooming out
void EditorUserInterface::zoom(F32 zoomAmount)
{
   Point mouseLevelPoint = convertCanvasToLevelCoord(mMousePos);

   setDisplayScale(mCurrentScale * (1 + zoomAmount));

   Point newMousePoint = convertLevelToCanvasCoord(mouseLevelPoint);

   mCurrentOffset += mMousePos - newMousePoint;
}


void EditorUserInterface::setDisplayExtents(const Rect &extents, F32 backoffFact)
{
   F32 scale = min(DisplayManager::getScreenInfo()->getGameCanvasWidth() / extents.getWidth(),
      DisplayManager::getScreenInfo()->getGameCanvasHeight() / extents.getHeight());

   scale /= backoffFact;

   setDisplayScale(scale);
   setDisplayCenter(extents.getCenter());
}


Rect EditorUserInterface::getDisplayExtents() const
{
   // mCurrentOffset is the UL corner of our screen... just what we need for the bounding box
   Point lr = Point(DisplayManager::getScreenInfo()->getGameCanvasWidth(),
      DisplayManager::getScreenInfo()->getGameCanvasHeight()) - mCurrentOffset;

   F32 mult = 1 / mCurrentScale;

   return Rect(-mCurrentOffset * mult, lr * mult);
}


// cenx and ceny are the desired center of the display; mCurrentOffset is the UL corner
void EditorUserInterface::setDisplayCenter(const Point &center)
{
   mCurrentOffset.set(DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2 - mCurrentScale * center.x,
      DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2 - mCurrentScale * center.y);
}


// We will need to recenter the display after changing the scale.  Higher scales are more zoomed in.
void EditorUserInterface::setDisplayScale(F32 scale)
{
   Point center = getDisplayCenter();

   mCurrentScale = scale;

   if(mCurrentScale < MIN_SCALE)
      mCurrentScale = MIN_SCALE;
   else if(mCurrentScale > MAX_SCALE)
      mCurrentScale = MAX_SCALE;

   setDisplayCenter(center);
}


Point EditorUserInterface::getDisplayCenter() const
{
   F32 mult = 1 / mCurrentScale;

   return Point(((DisplayManager::getScreenInfo()->getGameCanvasWidth() / 2) - mCurrentOffset.x),
      ((DisplayManager::getScreenInfo()->getGameCanvasHeight() / 2) - mCurrentOffset.y)) * mult;
}


void EditorUserInterface::onTextInput(char ascii)
{
   // Pass the key on to the console for processing
   if(GameManager::gameConsole->onKeyDown(ascii))
      return;
}


// Handle key presses
bool EditorUserInterface::onKeyDown(InputCode inputCode)
{
   if(Parent::onKeyDown(inputCode))
      return true;

   if(GameManager::gameConsole->onKeyDown(inputCode))      // Pass the key on to the console for processing
      return true;

   // If console is open, then we want to capture text, so return false
   if(GameManager::gameConsole->isVisible())
      return false;

   string inputString = InputCodeManager::getCurrentInputString(inputCode);

   return handleKeyPress(inputCode, inputString);
}


bool EditorUserInterface::handleKeyPress(InputCode inputCode, const string &inputString)
{
   if(inputCode == KEY_ENTER || inputCode == KEY_KEYPAD_ENTER)       // Enter - Edit props
      startAttributeEditor();

   // Mouse wheel scrolls the plugin list, or zooms in and out

   else if(inputCode == MOUSE_WHEEL_UP)
   {
      if(mDockMode == DOCKMODE_PLUGINS && mouseOnDock())
      {
         if(mDockPluginScrollOffset > 0)
            mDockPluginScrollOffset -= 1;
      }
      else
         zoom(0.2f);
   }
   else if(inputCode == MOUSE_WHEEL_DOWN)
   {
      if(mDockMode == DOCKMODE_PLUGINS && mouseOnDock())
      {
         if(mDockPluginScrollOffset < (S32)(mPluginInfos.size() - getDockHeight() / PLUGIN_LINE_SPACING))
            mDockPluginScrollOffset += 1;
      }
      else
         zoom(-0.2f);
   }
   else if(inputCode == MOUSE_MIDDLE)     // Click wheel to drag
   {
      mScrollWithMouseLocation = mMousePos;
      mAutoScrollWithMouseReady = !mAutoScrollWithMouse; // Ready to scroll when button is released
      mAutoScrollWithMouse = false;  // turn off in case we were already auto scrolling.
   }

   // Regular key handling from here on down
   else if(InputCodeManager::checkModifier(KEY_SHIFT) && inputCode == KEY_0)  // Shift-0 -> Set team to hostile
      setCurrentTeam(-2);
   else if(inputCode >= KEY_0 && inputCode <= KEY_9 && InputCodeManager::checkModifier(KEY_NONE))  // Change team affiliation of selection with 0-9 keys
   {
      setCurrentTeam((inputCode - KEY_0) - 1);
      return true;
   }

#ifdef TNL_OS_MAC_OSX 
   // Ctrl-left click is same as right click for Mac users
   else if(inputCode == MOUSE_RIGHT || (inputCode == MOUSE_LEFT && InputCodeManager::checkModifier(KEY_CTRL)))
#else
   else if(inputCode == MOUSE_RIGHT)
#endif
      onMouseClicked_right();

   else if(inputCode == MOUSE_LEFT)
      onMouseClicked_left();

   // Neither mouse button, let's try some keys
   else if(inputString == "D" || inputString == "Shift+D")                                  // Pan right
      mRight = true;
   else if(inputString == "Right Arrow")  // Pan right
      mRight = true;
   else if(inputString == getEditorBindingString(BINDING_FLIP_HORIZ))         // Flip horizontal
      flipSelectionHorizontal();
   else if(inputString == getEditorBindingString(BINDING_PASTE_SELECTION))    // Paste selection
      pasteSelection();
   else if(inputString == getEditorBindingString(BINDING_FLIP_VERTICAL))      // Flip vertical
      flipSelectionVertical();
   else if(inputString == "/" || inputString == "Keypad /")
      openConsole(NULL);
   else if(inputString == getEditorBindingString(BINDING_RELOAD_LEVEL))       // Reload level
   {
      loadLevel();
      setSaveMessage("Reloaded " + getLevelFileName(), true);
   }
   else if(inputString == getEditorBindingString(BINDING_REDO_ACTION))        // Redo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         redo();
   }
   else if(inputString == getEditorBindingString(BINDING_UNDO_ACTION))        // Undo
   {
      if(!mCreatingPolyline && !mCreatingPoly && !mDraggingObjects && !mDraggingDockItem)
         undo(true);
   }
   else if(inputString == getEditorBindingString(BINDING_RESET_VIEW))         // Reset veiw
      centerView();
   else if(inputString == getEditorBindingString(BINDING_LVLGEN_SCRIPT))      // Run levelgen script, or clear last results
   {
      // Ctrl+R is a toggle -- we either add items or clear them
      if(mLevelGenDatabase.getObjectCount() == 0)
         runLevelGenScript();
      else
         clearLevelGenItems();
   }
   else if(inputString == "Shift+1" || inputString == "Shift+3")  // '!' or '#'
      startSimpleTextEntryMenu(SimpleTextEntryID);
   else if(inputString == "Ctrl+Shift+3")    // i.e. ctrl-#
      mShowAllIds = !mShowAllIds;
   else if(inputString == getEditorBindingString(BINDING_ROTATE_CENTROID))    // Spin by arbitrary amount
   {
      if(canRotate())
         startSimpleTextEntryMenu(SimpleTextEntryRotateCentroid);
   }
   else if(inputString == getEditorBindingString(BINDING_ROTATE_ORIGIN))      // Rotate by arbitrary amount
      startSimpleTextEntryMenu(SimpleTextEntryRotateOrigin);
   else if(inputString == getEditorBindingString(BINDING_SPIN_CCW))           // Spin CCW
      rotateSelection(-15.f, false);
   else if(inputString == getEditorBindingString(BINDING_SPIN_CW))            // Spin CW
      rotateSelection(15.f, false);
   else if(inputString == getEditorBindingString(BINDING_ROTATE_CCW_ORIGIN))  // Rotate CCW about origin
      rotateSelection(-15.f, true);
   else if(inputString == getEditorBindingString(BINDING_ROTATE_CW_ORIGIN))   // Rotate CW about origin
      rotateSelection(15.f, true);

   else if(inputString == getEditorBindingString(BINDING_INSERT_GEN_ITEMS))   // Insert items generated with script into editor
      copyScriptItemsToEditor();

   else if(inputString == "Up Arrow" || inputString == "W" || inputString == "Shift+W")     // W or Up - Pan up
      mUp = true;
   else if(inputString == "Ctrl+Up Arrow")      // Zoom in
      mIn = true;
   else if(inputString == "Ctrl+Down Arrow")    // Zoom out
      mOut = true;
   else if(inputString == "Down Arrow")         // Pan down
      mDown = true;
   else if(inputString == getEditorBindingString(BINDING_SAVE_LEVEL))          // Save
      saveLevel(true, true);
   else if(inputString == "S" || inputString == "Shift+S")                                    // Pan down
      mDown = true;
   else if(inputString == "Left Arrow" || inputString == "A" || inputString == "Shift+A")    // Left or A - Pan left
      mLeft = true;
   else if(inputString == "Shift+=" || inputString == "Shift+Keypad +")                      // Shifted - Increase barrier width by 1
      changeBarrierWidth(1);
   else if(inputString == "=" || inputString == "Keypad +")                                  // Unshifted + --> by 5
      changeBarrierWidth(5);
   else if(inputString == "Shift+-" || inputString == "Shift+Keypad -")                      // Shifted - Decrease barrier width by 1
      changeBarrierWidth(-1);
   else if(inputString == "-" || inputString == "Keypad -")                                  // Unshifted --> by 5
      changeBarrierWidth(-5);
   else if(inputString == getEditorBindingString(BINDING_ZOOM_IN))                // Zoom In
      mIn = true;
   else if(inputString == "\\")                                                              // Split barrier on selected vertex               
      splitBarrier();
   else if(inputString == getEditorBindingString(BINDING_JOIN_SELECTION))         // Join selected barrier segments or polygons
      joinBarrier();
   else if(inputString == getEditorBindingString(BINDING_SELECT_EVERYTHING))      // Select everything
      selectAll(getLevel());
   else if(inputString == getEditorBindingString(BINDING_RESIZE_SELECTION))       // Resize selection
      startSimpleTextEntryMenu(SimpleTextEntryScale);
   else if(inputString == getEditorBindingString(BINDING_CUT_SELECTION))          // Cut selection
   {
      copySelection();
      deleteSelection(true);
   }
   else if(inputString == getEditorBindingString(BINDING_COPY_SELECTION))         // Copy selection to clipboard
      copySelection();
   else if(inputString == getEditorBindingString(BINDING_ZOOM_OUT))               // Zoom out
      mOut = true;
   else if(inputString == getEditorBindingString(BINDING_LEVEL_PARAM_EDITOR))     // Level Parameter Editor
   {
      getUIManager()->activate<GameParamUserInterface>();
      playBoop();
   }
   else if(inputString == getEditorBindingString(BINDING_TEAM_EDITOR))            // Team Editor Menu
   {
      getUIManager()->activate<TeamDefUserInterface>();
      playBoop();
   }
   else if(inputString == getEditorBindingString(BINDING_PLACE_TELEPORTER))       // Teleporter
      insertNewItem(TeleporterTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_SPEEDZONE))        // SpeedZone
      insertNewItem(SpeedZoneTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_SPAWN))            // Spawn
      insertNewItem(ShipSpawnTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_SPYBUG))           // Spybug
      insertNewItem(SpyBugTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_REPAIR))           // Repair
      insertNewItem(RepairItemTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_TURRET))           // Turret
      insertNewItem(TurretTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_MINE))             // Mine
      insertNewItem(MineTypeNumber);
   else if(inputString == getEditorBindingString(BINDING_PLACE_FORCEFIELD))       // Forcefield
      insertNewItem(ForceFieldProjectorTypeNumber);
   else if(inputString == "Backspace" || inputString == "Del" || inputString == "Keypad .")  // Keypad . is the keypad's del key
      deleteSelection(false);
   else if(checkInputCode(BINDING_HELP, inputString))                                        // Turn on help screen
   {
      getUIManager()->activate<EditorInstructionsUserInterface>();
      playBoop();
   }
   else if(inputCode == KEY_ESCAPE)          // Activate the menu
   {
      playBoop();
      getUIManager()->activate<EditorMenuUserInterface>();
   }
   else if(inputCode == getEditorBindingInputCode(TURN_ON_CONSTRAINED_MOVEMENT_CODE)) 
      mSnapContext |= (CONSTRAINED_MOVEMENT);
   else if(inputCode == getEditorBindingInputCode(BINDING_NO_GRID_SNAPPING))          // Disable grid snapping (Space)
      mSnapContext &= ~GRID_SNAPPING;
   else if(inputString == getEditorBindingString(BINDING_PREVIEW_MODE))                // Turn on preview mode
      mPreviewMode = true;       
   else if(inputString == getEditorBindingString(BINDING_DOCKMODE_ITEMS))              //  Toggle dockmode Items
   {
      if(mDockMode == DOCKMODE_ITEMS)
      {
         mDockMode = DOCKMODE_PLUGINS;
         mDockWidth = findPluginDockWidth();
      }
      else
      {
         mDockMode = DOCKMODE_ITEMS;
         mDockWidth = ITEMS_DOCK_WIDTH;
      }
   }
   else if(checkPluginKeyBindings(inputString))
   {
      // Do nothing
   }
   else if(inputString == getEditorBindingString(BINDING_TOGGLE_EDIT_MODE))
   {
      mVertexEditMode = !mVertexEditMode;
   }
   else
      return false;

   // A key was handled
   return true;
}


void EditorUserInterface::onMouseClicked_left()
{
   if(InputCodeManager::getState(MOUSE_RIGHT))  // Prevent weirdness
      return;

   bool spaceDown = InputCodeManager::getState(KEY_SPACE);

   mDraggingDockItem = NULL;
   setMousePos();
   mJustInsertedVertex = false;

   if(mCreatingPoly || mCreatingPolyline)       // Save any polygon/polyline we might be creating
   {
      TNLAssert(mNewItem.isValid(), "Should have an item here!");

      if(mNewItem->getVertCount() >= 2)
      {
         addToEditor(mNewItem);
         mUndoManager.saveAction(ActionCreate, mNewItem);      // mNewItem gets copied
      }
      else
      {
         // Not enough points... delete the object under construction
         delete mNewItem.getPointer();
      }

      mNewItem = NULL;

      mCreatingPoly = false;
      mCreatingPolyline = false;
   }

   mMouseDownPos = convertCanvasToLevelCoord(mMousePos);

   if(mouseOnDock())    // On the dock?  Did we hit something to start dragging off the dock?
   {
      switch(mDockMode)
      {
      case DOCKMODE_ITEMS:
         clearSelection(getLevel());
         mDraggingDockItem = mDockItemHit;      // Could be NULL

         if(mDraggingDockItem)
            SDL_SetCursor(Cursor::getSpray());
         break;

      case DOCKMODE_PLUGINS:
         S32 hitPlugin = findHitPlugin();

         if(hitPlugin >= 0 && hitPlugin < mPluginInfos.size())
            runPlugin(mGameSettings->getFolderManager(), mPluginInfos[hitPlugin].fileName, Vector<string>());

         break;
      }
   }
   else                 // Mouse is not on dock
   {
      mDraggingDockItem = NULL;
      SDL_SetCursor(Cursor::getDefault());

      // rules for mouse down:
      // if the click has no shift- modifier, then
      //   if the click was on something that was selected
      //     do nothing
      //   else
      //     clear the selection
      //     add what was clicked to the selection
      //  else
      //    toggle the selection of what was clicked
      //
      // Also... if we are unselecting something, don't make that unselection take effect until mouse-up, in case
      // it is the beginning of a drag; that way we don't unselect when we mean to drag when shift is down
      if(InputCodeManager::checkModifier(KEY_SHIFT))  // ==> Shift key is down
      {
         // Check for vertices
         if(mVertexEditMode && !spaceDown && mHitItem && mHitVertex != NONE && mHitItem->getGeomType() != geomPoint)
         {
            if(mHitItem->vertSelected(mHitVertex))
            {
               // These will be unselected when the mouse is released, unless we are initiating a drag event
               mDelayedUnselectObject = mHitItem;
               mDelayedUnselectVertex = mHitVertex;
            }
            else
               mHitItem->aselectVert(mHitVertex);
         }
         else if(mHitItem)    // Item level
         {
            // Unselecting an item
            if(mHitItem->isSelected())
            {
               mDelayedUnselectObject = mHitItem;
               mDelayedUnselectVertex = NONE;
            }
            else
               mHitItem->setSelected(true);

            onSelectionChanged();
         }
         else
            mDragSelecting = true;
      }
      else                                            // ==> Shift key is NOT down
      {

         // If we hit a vertex of an already selected item --> now we can move that vertex w/o losing our selection.
         // Note that in the case of a point item, we want to skip this step, as we don't select individual vertices.
         if(mVertexEditMode && !spaceDown && mHitVertex != NONE && mHitItem && mHitItem->isSelected() && mHitItem->getGeomType() != geomPoint)
         {
            clearSelection(getLevel());
            mHitItem->selectVert(mHitVertex);
            onSelectionChanged();
         }

         if(mHitItem && mHitItem->isSelected())    // Hit an already selected item
         {
            // Do nothing so user can drag a group of items that's already been selected
         }
         else if(mHitItem && mHitItem->getGeomType() == geomPoint)  // Hit a point item
         {
            clearSelection(getLevel());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else if(mVertexEditMode && !spaceDown && mHitVertex != NONE && (mHitItem && !mHitItem->isSelected()))      // Hit a vertex of an unselected item
         {        // (braces required)
            if(!(mHitItem->vertSelected(mHitVertex)))
            {
               clearSelection(getLevel());
               mHitItem->selectVert(mHitVertex);
               onSelectionChanged();
            }
         }
         else if(mHitItem)                                                          // Hit a non-point item, but not a vertex
         {
            clearSelection(getLevel());
            mHitItem->setSelected(true);
            onSelectionChanged();
         }
         else     // Clicked off in space.  Starting to draw a bounding rectangle?
         {
            mDragSelecting = true;
            clearSelection(getLevel());
            onSelectionChanged();
         }
      }
   }     // end mouse not on dock block, doc

   findSnapVertex();     // Update snap vertex in the event an item was selected
}


void EditorUserInterface::onMouseClicked_right()
{
   if(InputCodeManager::getState(MOUSE_LEFT) && !InputCodeManager::checkModifier(KEY_CTRL))  // Prevent weirdness
      return;

   setMousePos();

   if(mCreatingPoly || mCreatingPolyline)
   {
      if(mNewItem->getVertCount() < Geometry::MAX_POLY_POINTS)    // Limit number of points in a polygon/polyline
      {
         mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
         mNewItem->onGeomChanging();
      }

      return;
   }

   clearSelection(getLevel());   // Unselect anything currently selected
   onSelectionChanged();

   // Can only add new vertices by clicking on item's edge, not it's interior (for polygons, that is)
   if(mEdgeHit != NONE && mHitItem && (mHitItem->getGeomType() == geomPolyLine || mHitItem->getGeomType() >= geomPolygon))
   {
      if(mHitItem->getVertCount() >= Geometry::MAX_POLY_POINTS)     // Polygon full -- can't add more
         return;

      Point newVertex = snapPoint(convertCanvasToLevelCoord(mMousePos));   // adding vertex w/ right-mouse

      mAddingVertex = true;

      // Insert an extra vertex at the mouse clicked point, and then select it
      mHitItem->insertVert(newVertex, mEdgeHit + 1);
      mHitItem->selectVert(mEdgeHit + 1);
      mJustInsertedVertex = true;

      // Alert the item that its geometry is changing -- needed by polygons so they can recompute their fill
      mHitItem->onGeomChanging();

      // The user might just insert a vertex and be done; in that case we'll need to rebuild the wall outlines to account
      // for the new vertex.  If the user continues to drag the vertex to a new location, this will be wasted effort...
      mHitItem->onGeomChanged();

      mMouseDownPos = newVertex;
   }
   else     // Start creating a new poly or new polyline (tilde key + right-click ==> start polyline)
   {
      if(InputCodeManager::getState(KEY_BACKQUOTE))   // Tilde  
      {
         mCreatingPolyline = true;
         mNewItem = new LineItem();
      }
      else
      {
         mCreatingPoly = true;
         mNewItem = new WallItem();
      }

      mNewItem->initializeEditor();
      setTeam(mNewItem, mCurrentTeam);
      mNewItem->addVert(snapPoint(convertCanvasToLevelCoord(mMousePos)));
   }
}


// Returns true if key was handled, false if not
bool EditorUserInterface::checkPluginKeyBindings(string inputString)
{
   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      if(mPluginInfos[i].binding != "" && inputString == mPluginInfos[i].binding)
      {
         runPlugin(mGameSettings->getFolderManager(), mPluginInfos[i].fileName, Vector<string>());
         return true;
      }
   }

   return false;
}


static void simpleTextEntryMenuCallback(ClientGame *game, U32 unused)
{
   SimpleTextEntryMenuUI *ui = dynamic_cast<SimpleTextEntryMenuUI *>(game->getUIManager()->getCurrentUI());
   TNLAssert(ui, "Unexpected UI here -- expected a SimpleTextEntryMenuUI!");

   ui->doneEditing();
   ui->getUIManager()->reactivatePrevUI();
}


void idEntryCallback(TextEntryMenuItem *menuItem, const string &text, BfObject *object)
{
   TNLAssert(object, "Expected an object here!");

   // Check for duplicate IDs
   S32 id = atoi(text.c_str());
   bool duplicateFound = false;

   if(id != 0)
   {
      const Vector<DatabaseObject *> *objList = object->getDatabase()->findObjects_fast();

      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->getUserAssignedId() == id && obj != object)
         {
            duplicateFound = true;
            break;
         }
      }
   }


   // Show a message if we've detected a duplicate ID is being entered
   if(duplicateFound)
   {
      menuItem->setHasError(true);
      menuItem->setHelp("ERROR: Duplicate ID detected!");
   }
   else
   {
      menuItem->setHasError(false);
      menuItem->setHelp("");
   }
}


void EditorUserInterface::startSimpleTextEntryMenu(SimpleTextEntryType entryType)
{
   // No items selected?  Abort!
   if(!anyItemsSelected(getLevel()))
      return;

   // Are items being dragged?  If so, abort!
   if(mDraggingObjects)
      return;

   string menuTitle = "Some Interesting Title";
   string menuItemTitle = "Another Interesting Title";
   string lineValue = "";

   LineEditorFilter filter = numericFilter;
   void(*callback)(TextEntryMenuItem *, const string &, BfObject *) = NULL;  // Our input callback; triggers on input change

   // Find first selected item, and work with that
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   S32 selectedIndex = NONE;
   BfObject *selectedObject = NULL;
   BfObject *obj = NULL;
   for(S32 i = 0; i < objList->size(); i++)
   {
      obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
      {
         selectedIndex = i;
         selectedObject = obj;
         break;
      }
   }


   // Adjust our UI depending on which type was requested
   switch(entryType)
   {
   case SimpleTextEntryID:
   {
      menuTitle = "Add Item ID";
      menuItemTitle = "ID:";
      filter = digitsOnlyFilter;
      callback = idEntryCallback;

      // We need to assure that we only assign an ID to ONE object
      // Unselect all objects but our first selected one
      for(S32 i = 0; i < objList->size(); i++)
         if(i != selectedIndex)
            static_cast<BfObject *>(objList->get(i))->setSelected(false);

      onSelectionChanged();

      S32 currentId = selectedObject->getUserAssignedId();  // selectedObject should never be NULL here

      lineValue = currentId <= 0 ? "" : itos(currentId);

      break;
   }
   case SimpleTextEntryRotateOrigin:
      menuTitle = "Rotate object(s) about (0,0)";
      menuItemTitle = "Angle:";
      break;

   case SimpleTextEntryRotateCentroid:
      menuTitle = "Spin object(s)";
      menuItemTitle = "Angle:";
      break;

   case SimpleTextEntryScale:
      menuTitle = "Resize";
      menuItemTitle = "Resize Factor:";
      break;

   default:
      break;
   }

   // Create our menu item
   SimpleTextEntryMenuItem *menuItem = new SimpleTextEntryMenuItem(menuItemTitle, U32_MAX_DIGITS, simpleTextEntryMenuCallback);
   menuItem->getLineEditor()->setFilter(filter);

   if(lineValue != "")   // This object has an ID already
      menuItem->getLineEditor()->setString(lineValue);

   if(callback != NULL)  // Add a callback for IDs to check for duplicates
      menuItem->setTextEditedCallback(callback);

   // Create our menu, use scoped_ptr since we only need once instance of this menu
   mSimpleTextEntryMenu.reset(new SimpleTextEntryMenuUI(getGame(), getUIManager(), menuTitle, entryType));
   mSimpleTextEntryMenu->addMenuItem(menuItem);                // addMenuItem wraps the menu item in a smart pointer
   mSimpleTextEntryMenu->setAssociatedObject(selectedObject);  // Add our object for usage in the menu item callback

   getUIManager()->activate(mSimpleTextEntryMenu.get());
}


void EditorUserInterface::doneWithSimpleTextEntryMenu(SimpleTextEntryMenuUI *menu, S32 data)
{
   SimpleTextEntryType entryType = (SimpleTextEntryType)data;

   string value = menu->getMenuItem(0)->getValue();

   switch(entryType)
   {
   case SimpleTextEntryID:
      setSelectionId(atoi(value.c_str()));
      break;

   case SimpleTextEntryRotateOrigin:
   {
      F32 angle = (F32)Zap::stof(value);
      rotateSelection(-angle, true);       // Positive angle should rotate CW, negative makes that happen
      break;
   }

   case SimpleTextEntryRotateCentroid:
   {
      F32 angle = (F32)Zap::stof(value);
      rotateSelection(-angle, false);      // Positive angle should rotate CW, negative makes that happen
      break;
   }

   case SimpleTextEntryScale:
      scaleSelection((F32)Zap::stof(value));
      break;

   default:
      break;
   }
}


void EditorUserInterface::startAttributeEditor()
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj_i = static_cast<BfObject *>(objList->get(i));

      if(obj_i->isSelected())
      {
         // Force item i to be the one and only selected item type.  This will clear up some problems that might otherwise
         // occur if you had different item types selected while you were editing attributes.   If you have multiple
         // items selected, all will end up with the same values, which only make sense if they are the same kind
         // of object.  So after this runs, there may be multiple items selected, but they'll all  be the same type.
         for(S32 j = 0; j < objList->size(); j++)
         {
            BfObject *obj_j = static_cast<BfObject *>(objList->get(j));

            if(obj_j->isSelected() && obj_j->getObjectTypeNumber() != obj_i->getObjectTypeNumber())
               obj_j->unselect();
         }

         // Activate the attribute editor if there is one
         EditorAttributeMenuUI *menu = getUIManager()->getUI<EditorAttributeMenuUI>();

         bool ok = menu->startEditingAttrs(obj_i);
         if(ok)
            getUIManager()->activate(menu);

         return;
      }
   }
}


// Gets run when user exits special-item editing mode, called from attribute editors
void EditorUserInterface::doneEditingAttributes(EditorAttributeMenuUI *editor, BfObject *object)
{
   object->onAttrsChanged();

   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   mUndoManager.startTransaction();

   // Find any other selected items of the same type of the item we just edited, and update their attributes too
   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj != object && obj->isSelected() && obj->getObjectTypeNumber() == object->getObjectTypeNumber())
      {
         mUndoManager.saveChangeAction_before(obj);

         editor->doneEditingAttrs(obj);  // Transfer attributes from editor to object
         obj->onAttrsChanged();          // And notify the object that its attributes have changed

         mUndoManager.saveChangeAction_after(obj);
      }
   }

   mUndoManager.endTransaction();
}


void EditorUserInterface::onKeyUp(InputCode inputCode)
{
   switch(inputCode)
   {
   case KEY_UP:
      mIn = false;
      mUp = false;
      break;
   case KEY_W:
      mUp = false;
      break;
   case KEY_DOWN:
      mOut = false;
      mDown = false;
      break;
   case KEY_S:
      mDown = false;
      break;
   case KEY_LEFT:
   case KEY_A:
      mLeft = false;
      break;
   case KEY_RIGHT:
   case KEY_D:
      mRight = false;
      break;
   case KEY_E:
      mIn = false;
      break;
   case KEY_C:
      mOut = false;
      break;
   case KEY_TAB:
      mPreviewMode = false;
      break;
   case MOUSE_MIDDLE:
      mAutoScrollWithMouse = mAutoScrollWithMouseReady;
      break;

   case MOUSE_LEFT:
   case MOUSE_RIGHT:
      onMouseUp();
      break;

   default:
      // These aren't constant expressions so can't be put in the case structure above.
      if(inputCode == getEditorBindingInputCode(BINDING_NO_GRID_SNAPPING)) 
         mSnapContext |= GRID_SNAPPING;            // Turn grid snapping back on
      else if(inputCode == getEditorBindingInputCode(TURN_ON_CONSTRAINED_MOVEMENT_CODE))
         mSnapContext &= ~(CONSTRAINED_MOVEMENT);  // Turn object snapping back on 
      break;
   }     // end case
}


void EditorUserInterface::onMouseUp()
{
   if(mDelayedUnselectObject != NULL)
   {
      if(mDelayedUnselectVertex != NONE)
         mDelayedUnselectObject->unselectVert(mDelayedUnselectVertex);
      else
      {
         mDelayedUnselectObject->setSelected(false);
         onSelectionChanged();
      }

      mDelayedUnselectObject = NULL;
   }

   setMousePos();

   if(mDragSelecting)      // We were drawing a rubberband selection box
   {
      Rect r(convertCanvasToLevelCoord(mMousePos), mMouseDownPos);

      fillVector.clear();

      getLevel()->findObjects(fillVector);


      for(S32 i = 0; i < fillVector.size(); i++)
      {
         BfObject *obj = dynamic_cast<BfObject *>(fillVector[i]);

         // Make sure that all vertices of an item are inside the selection box; basically means that the entire 
         // item needs to be surrounded to be included in the selection
         S32 j;

         for(j = 0; j < obj->getVertCount(); j++)
            if(!r.contains(obj->getVert(j)))
               break;

         if(j == obj->getVertCount())
            obj->setSelected(true);
      }

      mDragSelecting = false;
      onSelectionChanged();
   }

   // We were dragging and dropping.  Could have been a move or a delete (by dragging to dock).
   else if(mDraggingObjects || mAddingVertex)
   {
      if(mAddingVertex)
         mAddingVertex = false;

      onFinishedDragging();
   }
}


// Called when user has been dragging an object and then releases it
void EditorUserInterface::onFinishedDragging()
{
   mDraggingObjects = false;
   SDL_SetCursor(Cursor::getDefault());

   // Dragged item off the dock, then back on  ==> nothing changed, do nothing
   if(mouseOnDock() && mDraggingDockItem != NULL)
      return;

   // Mouse is over the dock and we dragged something to the dock (probably a delete)
   if(mouseOnDock() && !mDraggingDockItem)
   {
      onFinishedDragging_droppedItemOnDock();
      return;
   }

   // Mouse not on dock, we are either:
   // 1. dragging from the dock;
   // 2. moving something; or
   // 3. we moved something to the dock and nothing was deleted, e.g. when dragging a vertex
   // need to save an undo state if anything changed
   if(mDraggingDockItem.isNull())    // Not dragging from dock - user is moving object around screen, or dragging vertex to dock
      onFinishedDragging_movingObject();
}


void EditorUserInterface::onFinishedDragging_droppedItemOnDock()     // Delete type action
{
   // Only delete items in normal dock mode
   if(mDockMode == DOCKMODE_ITEMS)
   {
      const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();
      bool deletedSomething = false, deletedWall = false;

      // Move objects back to their starting location before deleting, so when undeleting, 
      // objects return the way users will expect
      translateSelectedItems(Point(0, 0), mSnapDelta);

      mUndoManager.startTransaction();

      for(S32 i = 0; i < objList->size(); i++)    //  Delete all selected items
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected())
         {
            if(isWallType(obj->getObjectTypeNumber()))
               deletedWall = true;

            mUndoManager.saveAction(ActionDelete, obj);

            deleteItem(i, true);
            i--;
            deletedSomething = true;
         }
      }

      mUndoManager.endTransaction();

      // We deleted something, do some clean up and our job is done
      if(deletedSomething)
      {
         if(deletedWall)
            doneDeleteingWalls();

         doneDeletingObjects();

         return;
      }
   }
}


void EditorUserInterface::onFinishedDragging_movingObject()
{
   // If our snap vertex has moved then all selected items have moved
   bool itemsMoved = mDragCopying || (mSnapObject.isValid() && mSnapObject->getVert(mSnapVertexIndex) != mMoveOrigin);

   if(itemsMoved)    // Move consumated... update any moved items, and save our autosave
   {
      bool wallMoved = false;

      mUndoManager.startTransaction();

      const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

      S32 j = 0;
      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         if(obj->isSelected() || objList->get(i)->anyVertsSelected())
         {
            obj->onGeomChanged();
            mUndoManager.saveAction(ActionChange, mSelectedObjectsForDragging[j], obj);
            j++;
         }

         if(isWallType(obj->getObjectTypeNumber()) && (obj->isSelected() || obj->anyVertsSelected()))      // Wall or polywall
            wallMoved = true;
      }

      if(wallMoved)
         rebuildWallGeometry(mLevel.get());

      mUndoManager.endTransaction();

      autoSave();

      mDragCopying = false;

      return;
   }

   else if(mJustInsertedVertex)
      mJustInsertedVertex = false;

   // else we ended up doing nothing
}


bool EditorUserInterface::mouseOnDock() const
{
   return (mMousePos.x >= DisplayManager::getScreenInfo()->getGameCanvasWidth() - mDockWidth - horizMargin &&
      mMousePos.x <= DisplayManager::getScreenInfo()->getGameCanvasWidth() - horizMargin &&
      mMousePos.y >= DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin - getDockHeight() &&
      mMousePos.y <= DisplayManager::getScreenInfo()->getGameCanvasHeight() - vertMargin);
}


S32 EditorUserInterface::getItemSelectedCount()
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   S32 count = 0;

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(obj->isSelected())
         count++;
   }

   return count;
}


bool EditorUserInterface::anythingSelected() const
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));
      if(obj->isSelected() || obj->anyVertsSelected())
         return true;
   }

   return false;
}


void EditorUserInterface::idle(U32 timeDelta)
{
   Parent::idle(timeDelta);

   mIgnoreMouseInput = false;    // Avoid freezing effect from too many mouseMoved events without a render in between (sam)

   F32 pixelsToScroll = timeDelta * (InputCodeManager::getState(KEY_SHIFT) ? 1.0f : 0.5f);    // Double speed when shift held down

   if(mLeft && !mRight)
      mCurrentOffset.x += pixelsToScroll;
   else if(mRight && !mLeft)
      mCurrentOffset.x -= pixelsToScroll;
   if(mUp && !mDown)
      mCurrentOffset.y += pixelsToScroll;
   else if(mDown && !mUp)
      mCurrentOffset.y -= pixelsToScroll;

   if(mAutoScrollWithMouse)
   {
      mCurrentOffset += (mScrollWithMouseLocation - mMousePos) * pixelsToScroll / 128.f;
      onMouseMoved();  // Prevents skippy problem while dragging something
   }

   if(mIn && !mOut)
      zoom(timeDelta * 0.002f);
   else if(mOut && !mIn)
      zoom(timeDelta * -0.002f);

   mSaveMsgTimer.update(timeDelta);
   mWarnMsgTimer.update(timeDelta);

   // Process the messageBoxQueue
   if(mMessageBoxQueue.size() > 0)
   {
      ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

      ui->reset();
      ui->setTitle(mMessageBoxQueue[0][0]);
      ui->setInstr(mMessageBoxQueue[0][1]);
      ui->setMessage(mMessageBoxQueue[0][2]);
      getUIManager()->activate(ui);

      mMessageBoxQueue.erase(0);
   }

   if(mLingeringMessageQueue != "")
   {
      setLingeringMessage(mLingeringMessageQueue);
      mLingeringMessageQueue = "";
   }

   mInfoMsg = getInfoMsg();

   mChatMessageDisplayer.idle(timeDelta, false);
}


string EditorUserInterface::getInfoMsg() const
{
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 i = 0; i < objList->size(); i++)
   {
      BfObject *obj = static_cast<BfObject *>(objList->get(i));

      if(!obj->isSelected() && obj->isLitUp() && !mouseOnDock())
         return string("Hover: ") + obj->getOnScreenName();
   }

   return "";
}


// This may seem redundant, but... this gets around errors stemming from trying to run setLingeringMessage() from
// the LevelDatabaseUploadThread::run() method.  It seems there are some concurrency issues... blech.
void EditorUserInterface::queueSetLingeringMessage(const string &msg)
{
   mLingeringMessageQueue = msg;
}


void EditorUserInterface::setLingeringMessage(const string &msg)
{
   mLingeringMessage.setSymbolsFromString(msg, NULL, HelpContext, 12, &Colors::red);
}


void EditorUserInterface::clearLingeringMessage()
{
   mLingeringMessage.clear();
}


void EditorUserInterface::setSaveMessage(const string &msg, bool savedOK)
{
   mSaveMsg = msg;
   mSaveMsgTimer.reset();
   mSaveMsgColor = (savedOK ? Colors::green : Colors::red);
}


void EditorUserInterface::clearSaveMessage()
{
   mSaveMsgTimer.clear();
}


void EditorUserInterface::setWarnMessage(const string &msg1, const string &msg2)
{
   mWarnMsg1 = msg1;
   mWarnMsg2 = msg2;
   mWarnMsgTimer.reset(FOUR_SECONDS);    // Display for 4 seconds
   mWarnMsgColor = Colors::ErrorMessageTextColor;
}


void EditorUserInterface::clearWarnMessage()
{
   if(mWarnMsgTimer.getCurrent() > WARN_MESSAGE_FADE_TIME)
      mWarnMsgTimer.reset(WARN_MESSAGE_FADE_TIME);
}


void EditorUserInterface::autoSave()
{
   doSaveLevel("auto.save", false);
}


bool EditorUserInterface::saveLevel(bool showFailMessages, bool showSuccessMessages)
{
   string filename = getLevelFileName();
   TNLAssert(filename != "", "Need file name here!");

   if(!doSaveLevel(filename, showFailMessages))
      return false;

   mUndoManager.levelSaved();

   if(showSuccessMessages)
      setSaveMessage("Saved " + getLevelFileName(), true);

   return true;
}


void EditorUserInterface::lockQuit(const string &message)
{
   mQuitLocked = true;
   mQuitLockedMessage = message;
}


void EditorUserInterface::unlockQuit()
{
   mQuitLocked = false;
   getUIManager()->getUI<EditorMenuUserInterface>()->unlockQuit();
}


bool EditorUserInterface::isQuitLocked()
{
   return mQuitLocked;
}


string EditorUserInterface::getQuitLockedMessage()
{
   return mQuitLockedMessage;
}


string EditorUserInterface::getLevelText() const
{
   string result = "";

   // Write out basic game parameters, including gameType info
   result += mLevel->toLevelCode();    // Note that this toLevelCode appends a newline char; most don't

   // Next come the robots
   for(S32 i = 0; i < mRobotLines.size(); i++)
      result += mRobotLines[i] + "\n";

   // Write out all level items (do two passes; walls first, non-walls next, so turrets & forcefields have something to grab onto)
   const Vector<DatabaseObject *> *objList = getLevel()->findObjects_fast();

   for(S32 j = 0; j < 2; j++)
   {
      for(S32 i = 0; i < objList->size(); i++)
      {
         BfObject *obj = static_cast<BfObject *>(objList->get(i));

         // Writing wall items on first pass, non-wall items next -- that will make sure mountable items have something to grab onto
         if((j == 0 && isWallType(obj->getObjectTypeNumber())) || (j == 1 && ! isWallType(obj->getObjectTypeNumber())))
            result += obj->toLevelCode() + "\n";
      }
   }

   return result;
}


const Vector<PluginInfo> *EditorUserInterface::getPluginInfos() const
{
   return &mPluginInfos;
}


void EditorUserInterface::clearRobotLines()
{
   mRobotLines.clear();
}


void EditorUserInterface::addRobotLine(const string &robotLine)
{
   mRobotLines.push_back(robotLine);
}


// Returns true if successful, false otherwise
bool EditorUserInterface::doSaveLevel(const string &saveName, bool showFailMessages)
{
   try
   {
      FolderManager *folderManager = mGameSettings->getFolderManager();

      string fileName = joindir(folderManager->getLevelDir(), saveName);
      if(!writeFile(fileName, getLevelText()))
         throw(SaveException("Could not open file for writing"));
   }
   catch(SaveException &e)
   {
      if(showFailMessages)
         setSaveMessage("Error Saving: " + string(e.what()), false);
      return false;
   }

   return true;      // Saved OK
}


// We need some local hook into the testLevelStart() below.  Ugly but apparently necessary.
void testLevelStart_local(ClientGame *game)
{
   game->getUIManager()->getUI<EditorUserInterface>()->testLevelStart();
}


void EditorUserInterface::testLevel()
{
   bool gameTypeError = !mLevel->getGameType(); // Not sure this could really happen anymore...  TODO: Make sure we always have a valid gametype

   // With all the map loading error fixes, game should never crash!
   validateLevel();
   if(mLevelErrorMsgs.size() || mLevelWarnings.size() || gameTypeError)
   {
      showLevelHasErrorMessage(gameTypeError);
      return;
   }

   testLevelStart();
}


void EditorUserInterface::showLevelHasErrorMessage(bool gameTypeError)
{
   ErrorMessageUserInterface *ui = getUIManager()->getUI<ErrorMessageUserInterface>();

   ui->reset();
   ui->setTitle("LEVEL HAS PROBLEMS");
   ui->setRenderUnderlyingUi(false);      // Use black background... it's comforting

   string msg = "";

   for(S32 i = 0; i < mLevelErrorMsgs.size(); i++)
      msg += mLevelErrorMsgs[i] + "\n";

   for(S32 i = 0; i < mLevelWarnings.size(); i++)
      msg += mLevelWarnings[i] + "\n";

   if(gameTypeError)
   {
      msg += "ERROR: GameType is invalid.\n";
      msg += "(Fix in Level Parameters screen [[GameParameterEditor]])";
   }

   ui->setMessage(msg);
   ui->setInstr("Press [[Y]] to start,  [[Esc]] to cancel");
   ui->registerKey(KEY_Y, testLevelStart_local);      // testLevelStart_local() just calls testLevelStart() below
   getUIManager()->activate(ui);
}


void EditorUserInterface::testLevelStart()
{
   Cursor::disableCursor();                           // Turn off cursor

   if(!doSaveLevel(LevelSource::TestFileName, true))
   {
      getUIManager()->reactivatePrevUI();             // Saving failed, can't test, reactivate editor
      return;
   }

   // Level saved OK

   mWasTesting = true;

   Vector<string> levelList;
   levelList.push_back(LevelSource::TestFileName);

   LevelSourcePtr levelSource = LevelSourcePtr(
      new FolderLevelSource(levelList, mGameSettings->getFolderManager()->getLevelDir()));

   initHosting(getGame()->getSettingsPtr(), levelSource, true, false);
}


void EditorUserInterface::createNormalizedScreenshot(ClientGame* game)
{
   mPreviewMode = true;
   mNormalizedScreenshotMode = true;

   mGL->glClear(GLOPT::ColorBufferBit);
   centerView(true);

   render();
#ifndef BF_NO_SCREENSHOTS
   ScreenShooter::saveScreenshot(game->getUIManager(), mGameSettings, LevelDatabaseUploadThread::UploadScreenshotFilename);
#endif
   mPreviewMode = false;
   mNormalizedScreenshotMode = false;
   centerView(false);
}


void EditorUserInterface::findPlugins()
{
   FileList fileList;

   mPluginInfos.clear();
   Vector<string> folders = mGameSettings->getFolderManager()->getPluginDirs();
   map<string, string> plugins;
   string extension = ".lua";

   for(S32 i = 0; i < folders.size(); i++)
      fileList.addFilesFromFolder(folders[i], &extension, 1);

   // Reference to original
   Vector<PluginBinding> &bindings = mGameSettings->getIniSettings()->pluginBindings;

   // Check for binding collision in INI.  If one is detected, set its key to empty
   for(S32 i = 0; i < bindings.size(); i++)
   {
      for(S32 j = 0; j < i; j++)  // Efficiency!
      {
         if(bindings[i].key == bindings[j].key)
         {
            bindings[i].key = "";
            break;
         }
      }
   }

   // Loop through all of our detected plugins
   for(; !fileList.isLast(); fileList.nextFile())
   {
      // Try to find the title
      string title;
      Vector<boost::shared_ptr<MenuItem> > menuItems;  // Unused

      EditorPlugin plugin(fileList.getCurrentFullFilename(), Vector<string>(), mLoadTarget, getGame());

      if(plugin.prepareEnvironment() && plugin.loadScript(false))
         plugin.runGetArgsMenu(title, menuItems);

      // If the title is blank or couldn't be found, use the file name
      if(title == "")
         title = fileList.getCurrentFilename();

      PluginInfo info(title, fileList.getCurrentFilename(), plugin.getDescription(), plugin.getRequestedBinding());

      // Check for a binding from the INI, if it exists set it for this plugin
      for(S32 j = 0; j < bindings.size(); j++)
      {
         if(bindings[j].script == fileList.getCurrentFilename())
         {
            info.binding = bindings[j].key;
            break;
         }
      }

      // If no binding is configured, and the plugin specifies a requested binding
      // Use the requested binding if it is not currently in use
      if(info.binding == "" && info.requestedBinding != "")
      {
         bool bindingCollision = false;

         // Determine if this requested binding is already in use by a binding
         // in the INI
         for(S32 j = 0; j < bindings.size(); j++)
         {
            if(bindings[j].key == info.requestedBinding)
            {
               bindingCollision = true;
               break;
            }
         }

         // Determine if this requested binding is already in use by a previously
         // loaded plugin
         for(S32 j = 0; j < mPluginInfos.size(); j++)
         {
            if(mPluginInfos[j].binding == info.requestedBinding)
            {
               bindingCollision = true;
               break;
            }
         }

         info.bindingCollision = bindingCollision;

         // Available!  Set our binding to the requested one
         if(!bindingCollision)
            info.binding = info.requestedBinding;
      }

      mPluginInfos.push_back(info);
   }

   mPluginInfos.sort(pluginInfoSort);

   // Now update all the bindings in the INI
   bindings.clear();

   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      PluginInfo info = mPluginInfos[i];

      // Only write out valid ones
      if(info.binding == "" || info.bindingCollision)
         continue;

      PluginBinding binding;
      binding.key = info.binding;
      binding.script = info.fileName;
      binding.help = info.description;

      bindings.push_back(binding);
   }
}


U32 EditorUserInterface::findPluginDockWidth()
{
   U32 maxNameWidth = 0;
   U32 maxBindingWidth = 0;
   for(S32 i = 0; i < mPluginInfos.size(); i++)
   {
      U32 nameWidth = RenderUtils::getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].prettyName.c_str());
      U32 bindingWidth = RenderUtils::getStringWidth(DOCK_LABEL_SIZE, mPluginInfos[i].binding.c_str());
      maxNameWidth = max(maxNameWidth, nameWidth);
      maxBindingWidth = max(maxBindingWidth, bindingWidth);
   }
   return maxNameWidth + maxBindingWidth + 2 * horizMargin;
}

};
