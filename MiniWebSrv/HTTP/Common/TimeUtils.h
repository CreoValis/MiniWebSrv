#pragma once

#include <time.h>

namespace UD
{

namespace TimeUtils
{

inline bool GMTime(const time_t *Timestamp, tm *TM)
{
#ifdef _MSC_VER
	return gmtime_s(TM,Timestamp)==0;
#else
	return gmtime_r(Timestamp,TM)!=nullptr;
#endif
}

inline time_t TimeGM(tm *TM)
{
#ifdef _MSC_VER
	return _mkgmtime(TM);
#else
	return timegm(TM);
#endif
}

}; //TimeUtils

}; //UD
