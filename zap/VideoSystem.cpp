//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "VideoSystem.h"

#include "GameManager.h"
#include "ClientGame.h"
#include "IniFile.h"
#include "Console.h"
#include "DisplayManager.h"
#include "UI.h"
#include "version.h"
#include "FontManager.h"
#include "UIManager.h"
#include "RenderUtils.h"

#include "stringUtils.h"

#include "tnlLog.h"

#include <cmath>

namespace Zap
{

VideoSystem::VideoSystem()
{
   // Do nothing

}

VideoSystem::~VideoSystem()
{
   // Do nothing
}


extern string getInstalledDataDir();

static string WINDOW_TITLE = "Bitfighter " + string(ZAP_GAME_RELEASE);

// Returns true if everything went ok, false otherwise
bool VideoSystem::init()
{
   // Make sure "SDL_Init(0)" was done before calling this function

   // First, initialize SDL's video subsystem
   if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
   {
      // Failed, exit
      logprintf(LogConsumer::LogFatalError, "SDL Video initialization failed: %s", SDL_GetError());
      return false;
   }


   // Now, we want to setup our requested
   // window attributes for our OpenGL window.
   // Note on SDL_GL_RED/GREEN/BLUE/ALPHA_SIZE: On windows, it is better to not set them at all, or risk going extremely slow software rendering including if your desktop graphics set to 16 bit color.
   //SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8 );
   //SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8 );
   //SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8 );
   //SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8 );
   SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );  // depth used in editor to display spybug visible area non-overlap
   SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );


   // Get information about the current desktop video settings and initialize
   // our ScreenInfo class with with current width and height

   SDL_DisplayMode mode;

   SDL_GetCurrentDisplayMode(0, &mode);  // We only have one display..  for now

   DisplayManager::getScreenInfo()->init(mode.w, mode.h);

   S32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
   // Fake fullscreen might not be needed with SDL2 - I think it does the fast switching
   // on platforms that support it
   //   if(gClientGame->getSettings()->getIniSettings()->useFakeFullscreen)  <== don't use gClientGame here, please!
   //      flags |= SDL_WINDOW_BORDERLESS;

#ifdef TNL_OS_MOBILE
   // Force fullscreen on mobile
   flags |= SDL_WINDOW_FULLSCREEN;

#  ifdef TNL_OS_IOS
   // This hint should be only for phones, tablets could rotate freely
   SDL_SetHint("SDL_IOS_ORIENTATIONS","LandscapeLeft LandscapeRight");
#  endif
#endif

#ifdef TNL_OS_MAC_OSX
   // This hint is to workaround this bug: https://bugzilla.libsdl.org/show_bug.cgi?id=1840
   SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#endif

#ifdef BF_USE_GLES
   SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles");
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(BF_USE_GLES2)
   SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

   // SDL 2.0 lets us create the window first, only once
   DisplayManager::getScreenInfo()->sdlWindow = SDL_CreateWindow(WINDOW_TITLE.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
         DisplayManager::getScreenInfo()->getWindowWidth(), DisplayManager::getScreenInfo()->getWindowHeight(), flags);

   if(!DisplayManager::getScreenInfo()->sdlWindow)
   {
      logprintf(LogConsumer::LogFatalError, "SDL window creation failed: %s", SDL_GetError());
      return false;
   }

   // Create our OpenGL context; save it in case we ever need it
   SDL_GLContext context = SDL_GL_CreateContext(DisplayManager::getScreenInfo()->sdlWindow);
   DisplayManager::getScreenInfo()->sdlGlContext = &context;


   // Set the window icon -- note that the icon must be a 32x32 bmp, and SDL will
   // downscale it to 16x16 with no interpolation.  Therefore, it's best to start
   // with a finely crafted 16x16 icon, then scale it up to 32x32 with no interpolation.
   // It will look crappy at 32x32, but good at 16x16, and that's all that really matters.

   // Save bmp as a 32 bit XRGB bmp file (Gimp can do it!)
   string iconPath = getInstalledDataDir() + getFileSeparator() + "bficon.bmp";
   SDL_Surface *icon = SDL_LoadBMP(iconPath.c_str());

   // OSX handles icons better with its native .icns file
#ifndef TNL_OS_MAC_OSX
   if(icon != NULL)
      SDL_SetWindowIcon(DisplayManager::getScreenInfo()->sdlWindow, icon);
#endif

   if(icon != NULL)
      SDL_FreeSurface(icon);

   // We will set the resolution, position, and flags in actualizeScreenMode()

   return true;
}


void VideoSystem::setWindowPosition(S32 left, S32 top)
{
   SDL_SetWindowPosition(DisplayManager::getScreenInfo()->sdlWindow, left, top);
}


S32 VideoSystem::getWindowPositionCoord(bool getX)
{
   S32 x, y;
   SDL_GetWindowPosition(DisplayManager::getScreenInfo()->sdlWindow, &x, &y);

   return getX ? x : y;
}


S32 VideoSystem::getWindowPositionX()
{
   return getWindowPositionCoord(true);
}


S32 VideoSystem::getWindowPositionY()
{
   return getWindowPositionCoord(false);
}


// Actually put us in windowed or full screen mode.  Pass true the first time this is used, false subsequently.
// This has the unfortunate side-effect of triggering a mouse move event.
void VideoSystem::actualizeScreenMode(GameSettings *settings, bool changingInterfaces, bool currentUIUsesEditorScreenMode)
{
   DisplayMode displayMode = settings->getSetting<DisplayMode>(IniKey::WindowMode);

   DisplayManager::getScreenInfo()->resetGameCanvasSize();     // Set GameCanvasSize vars back to their default values
   DisplayManager::getScreenInfo()->setActualized();


   // If old display mode is windowed or current is windowed but we change interfaces,
   // save the window position
   if(settings->getIniSettings()->oldDisplayMode == DISPLAY_MODE_WINDOWED ||
         (changingInterfaces && displayMode == DISPLAY_MODE_WINDOWED))
   {
      settings->setWindowPosition(VideoSystem::getWindowPositionX(), VideoSystem::getWindowPositionY());
   }

   // When we're in the editor, let's take advantage of the entire screen unstretched
   // We might want to disallow this when we're in split screen mode?
   if(currentUIUsesEditorScreenMode &&
         (displayMode == DISPLAY_MODE_FULL_SCREEN_STRETCHED || displayMode == DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED))
   {
      // Smaller values give bigger magnification; makes small things easier to see on full screen
      F32 magFactor = 0.85f;

      // For screens smaller than normal, we need to readjust magFactor to make sure we get the full canvas height crammed onto
      // the screen; otherwise our dock will break.  Since this mode is only used in the editor, we don't really care about
      // screen width; tall skinny screens will work just fine.
      magFactor = max(magFactor, (F32)DisplayManager::getScreenInfo()->getGameCanvasHeight() / (F32)DisplayManager::getScreenInfo()->getPhysicalScreenHeight());

      DisplayManager::getScreenInfo()->setGameCanvasSize(S32(DisplayManager::getScreenInfo()->getPhysicalScreenWidth() * magFactor), S32(DisplayManager::getScreenInfo()->getPhysicalScreenHeight() * magFactor));

      displayMode = DISPLAY_MODE_FULL_SCREEN_STRETCHED;
   }


   // Set up video/window flags amd parameters and get ready to change the window
   S32 sdlWindowWidth, sdlWindowHeight;
   F64 orthoLeft, orthoRight, orthoTop, orthoBottom;

   getWindowParameters(settings, displayMode, sdlWindowWidth, sdlWindowHeight, orthoLeft, orthoRight, orthoTop, orthoBottom);

   // Change video modes based on selected display mode
   // Note:  going into fullscreen you have to do in order:
   //  - SDL_SetWindowSize()
   //  - SDL_SetWindowFullscreen()
   //
   // However, coming out of fullscreen mode you must do the reverse
   switch (displayMode)
   {
   case DISPLAY_MODE_FULL_SCREEN_STRETCHED:
      SDL_SetWindowSize(DisplayManager::getScreenInfo()->sdlWindow, sdlWindowWidth, sdlWindowHeight);
      SDL_SetWindowFullscreen(DisplayManager::getScreenInfo()->sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);

      break;

   case DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED:
      SDL_SetWindowSize(DisplayManager::getScreenInfo()->sdlWindow, sdlWindowWidth, sdlWindowHeight);
      SDL_SetWindowFullscreen(DisplayManager::getScreenInfo()->sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);

      break;

   case DISPLAY_MODE_WINDOWED:
   default:
      // Reverse order, leave fullscreen before setting size
      SDL_SetWindowFullscreen(DisplayManager::getScreenInfo()->sdlWindow, 0);
      SDL_SetWindowSize(DisplayManager::getScreenInfo()->sdlWindow, sdlWindowWidth, sdlWindowHeight);
      break;
   }

   if(settings->getSetting<YesNo>(IniKey::DisableScreenSaver))
      SDL_DisableScreenSaver();
   else
      SDL_EnableScreenSaver();

   // Flush window events because SDL_SetWindowSize triggers a SDL_WINDOWEVENT_RESIZED 
   // event (which in turn triggers another SDL_SetWindowSize)
   SDL_FlushEvent(SDL_WINDOWEVENT);

   SDL_GL_SetSwapInterval(settings->getSetting<YesNo>(IniKey::Vsync) ? 1 : 0);

   // Now save the new window dimensions in ScreenInfo
   DisplayManager::getScreenInfo()->setWindowSize(sdlWindowWidth, sdlWindowHeight);

   mGL->glClearColor(0, 0, 0, 0);

   mGL->glViewport(0, 0, sdlWindowWidth, sdlWindowHeight);

   mGL->glMatrixMode(GLOPT::Projection);
   mGL->glLoadIdentity();

   // The best understanding I can get for glOrtho is that these are the coordinates you want to appear at the four corners of the
   // physical screen. If you want a "black border" down one side of the screen, you need to make left negative, so that 0 would
   // appear some distance in from the left edge of the physical screen.  The same applies to the other coordinates as well.
   mGL->glOrtho(orthoLeft, orthoRight, orthoBottom, orthoTop, 0, 1);

   mGL->glMatrixMode(GLOPT::Modelview);
   mGL->glLoadIdentity();

   // Do the scissoring
   if(displayMode == DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED)
   {
      mGL->glScissor(DisplayManager::getScreenInfo()->getHorizPhysicalMargin(),    // x
                DisplayManager::getScreenInfo()->getVertPhysicalMargin(),     // y
                DisplayManager::getScreenInfo()->getDrawAreaWidth(),          // width
                DisplayManager::getScreenInfo()->getDrawAreaHeight());        // height
   }
   else
   {
      // Enabling scissor appears to fix crashing problem switching screen mode
      // in linux and "Mobile 945GME Express Integrated Graphics Controller",
      // probably due to lines and points was not being clipped,
      // causing some lines to wrap around the screen, or by writing other
      // parts of RAM that can crash Bitfighter, graphics driver, or the entire computer.
      // This is probably a bug in the Linux Intel graphics driver.
      mGL->glScissor(0, 0, DisplayManager::getScreenInfo()->getWindowWidth(), DisplayManager::getScreenInfo()->getWindowHeight());
   }

   mGL->glEnable(GLOPT::ScissorTest);    // Turn on clipping

   mGL->setDefaultBlendFunction();
   mGL->glLineWidth(RenderUtils::DEFAULT_LINE_WIDTH);

   // Enable Line smoothing everywhere!  Make sure to disable temporarily for filled polygons and such
   if(settings->getSetting<YesNo>(IniKey::LineSmoothing))
   {
      mGL->glEnable(GLOPT::LineSmooth);
      //glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
   }

   mGL->glEnable(GLOPT::Blend);

   // Now set the window position
   if(displayMode == DISPLAY_MODE_WINDOWED)
   {
      // Sometimes it happens to be (0,0) hiding the top title bar preventing ability to
      // move the window, in this case we are not moving it unless it is not (0,0).
      // Note that ini config file will default to (0,0).
      if(settings->getWindowPositionX() != 0 || settings->getWindowPositionY() != 0)
         setWindowPosition(settings->getWindowPositionX(), settings->getWindowPositionY());
   }
   else
      setWindowPosition(0, 0);

   // Notify all active UIs that the screen has changed mode.  This will likely need some work to not do something
   // horrible in split-screen mode.
   const Vector<ClientGame *> *clientGames = GameManager::getClientGames();
   for(S32 i = 0; i < clientGames->size(); i++)
      if(clientGames->get(i)->getUIManager()->getCurrentUI())
         clientGames->get(i)->getUIManager()->getCurrentUI()->onDisplayModeChange();

   // Re-initialize our fonts because OpenGL textures can be lost upon screen change
   FontManager::reinitialize(settings);

   // This needs to happen after font re-initialization because I think fontstash interferes
   // with the oglconsole font somehow...
   GameManager::gameConsole->onScreenModeChanged();
}


void VideoSystem::getWindowParameters(GameSettings *settings, DisplayMode displayMode, 
                                      S32 &sdlWindowWidth, S32 &sdlWindowHeight, F64 &orthoLeft, F64 &orthoRight, F64 &orthoTop, F64 &orthoBottom)
{
   // Set up variables according to display mode
   switch(displayMode)
   {
      case DISPLAY_MODE_FULL_SCREEN_STRETCHED:
         sdlWindowWidth  = DisplayManager::getScreenInfo()->getPhysicalScreenWidth();
         sdlWindowHeight = DisplayManager::getScreenInfo()->getPhysicalScreenHeight();
         orthoLeft   = 0;
         orthoRight  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
         orthoBottom = DisplayManager::getScreenInfo()->getGameCanvasHeight();
         orthoTop    = 0;
         break;

      case DISPLAY_MODE_FULL_SCREEN_UNSTRETCHED:
         sdlWindowWidth  = DisplayManager::getScreenInfo()->getPhysicalScreenWidth();
         sdlWindowHeight = DisplayManager::getScreenInfo()->getPhysicalScreenHeight();
         orthoLeft   = -1 * DisplayManager::getScreenInfo()->getHorizDrawMargin();
         orthoRight  = DisplayManager::getScreenInfo()->getGameCanvasWidth() + DisplayManager::getScreenInfo()->getHorizDrawMargin();
         orthoBottom = DisplayManager::getScreenInfo()->getGameCanvasHeight() + DisplayManager::getScreenInfo()->getVertDrawMargin();
         orthoTop    = -1 * DisplayManager::getScreenInfo()->getVertDrawMargin();
         break;

      case DISPLAY_MODE_WINDOWED:
      default:  //  Fall through OK
         sdlWindowWidth  = (S32) floor((F32)DisplayManager::getScreenInfo()->getGameCanvasWidth()  * settings->getWindowSizeFactor() + 0.5f);
         sdlWindowHeight = (S32) floor((F32)DisplayManager::getScreenInfo()->getGameCanvasHeight() * settings->getWindowSizeFactor() + 0.5f);
         orthoLeft   = 0;
         orthoRight  = DisplayManager::getScreenInfo()->getGameCanvasWidth();
         orthoBottom = DisplayManager::getScreenInfo()->getGameCanvasHeight();
         orthoTop    = 0;
         break;
   }
}

} /* namespace Zap */
