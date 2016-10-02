#pragma once

#include <boost/endian/conversion.hpp>

#define ROUNDUP_POWEROFTWO(Val,Pow) (((Val) + ((1 << Pow)-1)) & ~((1 << Pow)-1))
