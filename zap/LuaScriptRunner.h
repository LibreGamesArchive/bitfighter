//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _LUA_SCRIPT_RUNNER_H
#define _LUA_SCRIPT_RUNNER_H

#include "LuaBase.h"          // Parent class

#include "EventManager.h"
#include "LuaWrapper.h"

#include "tnlVector.h"

#include <deque>
#include <string>

using namespace std;
using namespace TNL;


#define ARRAYDEF(...) __VA_ARGS__                  // Wrap inline array definitions so they don't confuse the preprocessor   


namespace Zap
{

class BfObject;
class DatabaseObject;
class Game;
class Level;
class LuaPlayerInfo;
class Rect;
class Ship;
class MenuItem;

////////////////////////////////////////
////////////////////////////////////////


#define ROBOT_HELPER_FUNCTIONS_KEY    "robot_helper_functions"
#define LEVELGEN_HELPER_FUNCTIONS_KEY "levelgen_helper_functions"
#define SCRIPT_TIMER_KEY "script_timer"

class LuaScriptRunner
{

private:
   static deque<string> mCachedScripts;

   static string mScriptingDir;

   void setLuaArgs(const Vector<string> &args);
   static void setModulePath();

   static void loadCompileSaveHelper(const string &scriptName, const char *registryKey);
   static void loadCompileRunHelper(const string &scriptName);
   static void loadCompileSaveScript(const char *filename, const char *registryKey);
   static void loadCompileScript(const char *filename);

   void pushStackTracer();      // Put error handler function onto the stack

   static void setEnums(lua_State *L);                       // Set a whole slew of enum values that we want the scripts to have access to
   static void setGlobalObjectArrays(lua_State *L);          // And some objects
   static void logErrorHandler(const char *msg, const char *prefix);

protected:
   enum ScriptType {
      ScriptTypeLevelgen,
      ScriptTypeRobot,
      ScriptTypeEditorPlugin,
      ScriptTypeConsole,
      MaxScriptTypes,
      ScriptTypeInvalid,
   };

   Game *mLuaGame;               // Pointer to our current Game object, which could be ServerGame or
                                 // ClientGame depending on where the script is called from
   Game *getLuaGame() const;     // Use this to access mLuaGame

   Level *mLevel;                // Pointer to our current level

   static lua_State *L;          // Main Lua state variable
   string mScriptName;           // Fully qualified script name, with path and everything
   Vector<string> mScriptArgs;   // List of arguments passed to the script

   string mScriptId;             // Unique id for this script
   ScriptType mScriptType;

   bool mSubscriptions[EventManager::EventTypes];  // Keep track of which events we're subscribed to for rapid unsubscription upon death or destruction

   // Sub-classes that override this should still call this with Parent::prepareEnvironment()
   virtual bool prepareEnvironment();

   static int luaPanicked(lua_State *L);  // Handle a total freakout by Lua
   static void registerClasses();
   void setEnvironment();                 // Sets the environment for the function on the top of the stack to that associated with name

   bool loadCompileRunEnvironmentScript(const string &scriptName);

   static void deleteScript(const char *name);  // Remove saved script from the Lua registry

   static void registerLooseFunctions(lua_State *L);     // Register some functions not associated with a particular class

   static S32 findObjectById(lua_State *L, const Vector<DatabaseObject *> *objects);


// Sets a var in the script's environment to give access to the caller's "this" obj, with the var name "name".
// Basically sets the "bot", "levelgen", and "plugin" vars.
template <class T>
void setSelf(lua_State *L, T *self, const char *name)
{
   lua_getfield(L, LUA_REGISTRYINDEX, self->getScriptId()); // Put script's env table onto the stack  -- env_table
                                                         
   lua_pushstring(L, name);                                 //                                        -- env_table, name
   luaW_push(L, self);                                      //                                        -- env_table, name, *this
   lua_rawset(L, -3);                                       // env_table[name] = *this                -- env_table
                                                                                                    
   lua_pop(L, -1);                                          // Cleanup                                -- <<empty stack>>

   TNLAssert(lua_gettop(L) == 0 || dumpStack(L), "Stack not cleared!");
}


protected:
   virtual void killScript();

public:
   LuaScriptRunner();               // Constructor
   virtual ~LuaScriptRunner();      // Destructor

   static void clearScriptCache();

   virtual const char *getErrorMessagePrefix();

   static lua_State *getL();
   static const char *getScriptId(lua_State *L);

   static bool startLua(const string &scriptingDir);  // Create L
   static void shutdown();                            // Delete L

   static bool configureNewLuaInstance(lua_State *L); // Prepare a new Lua environment for use

   bool runString(const string &code);
   bool runMain();                                    // Run a script's main() function
   bool runMain(const Vector<string> &args);          // Run a script's main() function, putting args into Lua's arg table

   bool loadScript(bool cacheScript);  // Loads script from file into a Lua chunk, then runs it
   bool runScript(bool cacheScript);   // Load the script, execute the chunk to get it in memory, then run its main() function

   bool runFunction(const char *function, S32 returnValues);
   void handleError(const string &message);


   const char *getScriptId();
   static bool loadFunction(lua_State *L, const char *scriptId, const char *functionName);
   bool loadAndRunGlobalFunction(lua_State *L, const char *key, ScriptContext context);

   void logError(const char *format, ...);

   S32 doSubscribe(lua_State *L, ScriptContext context);
   S32 doUnsubscribe(lua_State *L);


   // Consolidate code from bots and levelgens -- this tickTimer works for both!
   template <class T>
   void tickTimer(U32 deltaT)          
   {
      TNLAssert(lua_gettop(L) == 0 || dumpStack(L), "Stack dirty!");
      clearStack(L);

      luaW_push<T>(L, static_cast<T *>(this));           // -- this
      lua_pushnumber(L, deltaT);                         // -- this, deltaT

      // Note that we don't care if this generates an error... if it does the error handler will
      // print a nice message, then call killScript().
      runFunction("_tickTimer", 0);
   }


   //// Lua interface
   LUAW_DECLARE_ABSTRACT_CLASS(LuaScriptRunner);

   static const char *luaClassName;
   static const LuaFunctionProfile functionArgs[];
   static const luaL_reg luaMethods[];              // Only used for non-static lua methods

   // Non-static methods
   S32 lua_pointCanSeePoint(lua_State *L);

   S32 lua_findAllObjects(lua_State *L);
   S32 lua_findAllObjectsInArea(lua_State *L);
   S32 lua_findObjectById(lua_State *L);

   S32 lua_addItem(lua_State *L);

   S32 lua_getBotScripts(lua_State *L);
   S32 lua_getGameInfo(lua_State *L);
   S32 lua_getPlayerCount(lua_State *L);

   S32 lua_subscribe(lua_State *L);
   S32 lua_unsubscribe(lua_State *L);
};


////////////////////////////////////////
////////////////////////////////////////
//
// Some ugly macro defs that will make our Lua classes sleek and beautiful
//
////////////////////////////////////////
////////////////////////////////////////
//
// See discussion of this code here:
// http://stackoverflow.com/questions/11413663/reducing-code-repetition-in-c
//
// Starting with a definition like the following:
/*
 #define LUA_METHODS(CLASS, METHOD) \
    METHOD(CLASS, addDest,    ARRAYDEF({{ PT,  END }}), 1 ) \
    METHOD(CLASS, delDest,    ARRAYDEF({{ INT, END }}), 1 ) \
    METHOD(CLASS, clearDests, ARRAYDEF({{      END }}), 1 ) \
*/

#define LUA_METHOD_ITEM(class_, name, b, c) \
{ #name, luaW_doMethod<class_, &class_::lua_## name > },


#define GENERATE_LUA_METHODS_TABLE(class_, table_) \
const luaL_reg class_::luaMethods[] =              \
{                                                  \
   table_(class_, LUA_METHOD_ITEM)                 \
   { NULL, NULL }                                  \
}

// Generates something like the following:
// const luaL_reg Teleporter::luaMethods[] =
// {
//       { "addDest",    luaW_doMethod<Teleporter, &Teleporter::lua_addDest >    }
//       { "delDest",    luaW_doMethod<Teleporter, &Teleporter::lua_delDest >    }
//       { "clearDests", luaW_doMethod<Teleporter, &Teleporter::lua_clearDests > }
//       { NULL, NULL }
// };


////////////////////////////////////////

 #define LUA_FUNARGS_ITEM(class_, name, profiles, profileCount) \
{ #name, {profiles, profileCount } },
 

#define GENERATE_LUA_FUNARGS_TABLE(class_, table_)  \
using namespace LuaArgs;                            \
const LuaFunctionProfile class_::functionArgs[] =   \
{                                                   \
   table_(class_, LUA_FUNARGS_ITEM)                 \
   { NULL, {{{ }}, 0 } }                            \
}                                                   \

// Generates something like the following (without the comment block, of course!):
// const LuaFunctionProfile Teleporter::functionArgs[] =
//    |---------------- LuaFunctionProfile ------------------|     
//    |- Function name -|-------- LuaFunctionArgList --------|
//    |                 |-argList -|- # elements in argList -|  
// {
//    { "addDest",    {{{ PT,  END }},         1             } },
//    { "delDest",    {{{ INT, END }},         1             } },
//    { "clearDests", {{{      END }},         1             } },
//    { NULL, {{{ }}, 0 } }
// };

////////////////////////////////////////
////////////////////////////////////////

};

#endif
