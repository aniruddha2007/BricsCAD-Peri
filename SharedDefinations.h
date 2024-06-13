#pragma once

//Pi values for calculating the pre defined angles
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif
#ifndef M_PI_4
#define M_PI_4 0.78539816339744830962
#endif
#ifndef M_3PI_2
#define M_3PI_2 4.71238898038468985769
#endif
// Undefine the conflicting macros
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif