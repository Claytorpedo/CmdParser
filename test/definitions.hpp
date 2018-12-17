#pragma once
#ifndef INCLUDE_TEST_DEFINITIONS_HPP
#define INCLUDE_TEST_DEFINITIONS_HPP

#include "catch.hpp"

#include "../CmdParser/CmdParser.hpp"

#define ApproxEps(x) Approx(x).margin(0.00001)

#endif // INCLUDE_TEST_DEFINITIONS_HPP
