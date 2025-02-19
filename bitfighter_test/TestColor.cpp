//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#include "Colors.h"

#include "gtest/gtest.h"

namespace Zap
{

TEST(ColorTest, IniParsing)
{
   // Valid inputs
   EXPECT_EQ(Colors::red, Color::iniValToColor("#ff0000"));
   EXPECT_EQ(Colors::red, Color::iniValToColor("1 0 0"));

   EXPECT_EQ(Colors::yellow, Color::iniValToColor("#ffff00"));
   EXPECT_EQ(Colors::yellow, Color::iniValToColor("1 1 0"));

   // Invalid inputs -- white is the "default" color
   EXPECT_EQ(Colors::white, Color::iniValToColor("hello charlie!"));
   EXPECT_EQ(Colors::white, Color::iniValToColor("1 0"));

   // Quasi invalid inputs
   // Comments below indicate how colors currently work, not necessarily how they _should_ work
   EXPECT_TRUE(Colors::yellow  == Color::iniValToColor("2 1 0"));       // Numbers higher than 1 are clamped to 1!
   EXPECT_TRUE(Colors::magenta == Color::iniValToColor("#ff00ff0"));    // Long strings get truncated
   EXPECT_TRUE(Color("ff00f0") == Color::iniValToColor("#ff00f"));      // Short strings get padded with trailing 0s
   EXPECT_TRUE(Colors::white   == Color::iniValToColor("0 1"));         // Two components... no good!
   EXPECT_TRUE(Colors::green   == Color::iniValToColor("0 1 0 1"));     // Extra parameters are dropped!


}
	
};
