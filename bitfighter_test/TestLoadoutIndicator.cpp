//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "LoadoutIndicator.h"
#include "ClientGame.h"
#include "TestUtils.h"
#include "gtest/gtest.h"

namespace Zap
{

TEST(LoadoutIndicatorTest, RenderWidth)
{
   GamePair pair;
   ClientGame *game = pair.getClient(0);

   UI::LoadoutIndicator indicator;
   indicator.newLoadoutHasArrived(LoadoutTracker("Turbo,Shield,Triple,Mine,Bouncer"));     // Sets the loadout

   // Make sure the calculated width matches the rendered width
   ASSERT_EQ(indicator.render(game), indicator.getWidth());
}
	
};
