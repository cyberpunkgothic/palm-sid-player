//
// /home/ms/source/sidplay/libsidplay/include/RCS/mytypes.h,v
//

#ifndef SIDPLAY1_MYTYPES_H
#define SIDPLAY1_MYTYPES_H


#include "compconf.h"

// A ``bool'' type for compilers that don't (yet) support one.
#if !defined(SID_HAVE_BOOL)
  typedef int bool;

  #if defined(true) || defined(false)
    #error Better check include file ``mytypes.h''.
    #undef true
    #undef false
  #endif
  #define true 1
  #define false 0
#endif


// Wanted: 8-bit signed/unsigned.
typedef signed char sbyte;
typedef unsigned char ubyte;

// Wanted: 16-bit signed/unsigned.
typedef signed short int sword;
typedef unsigned short int uword;

// Wanted: 32-bit signed/unsigned.
typedef signed long int sdword;
typedef unsigned long int udword;


// Some common type shortcuts.
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long int ulong;


#if defined(SID_FPUFILTER)
  typedef float filterfloat;
#else
  // sid-player: fixed point in an int (the old "fixed" class in fixpoint.h no longer
  // compiles with modern C++). Two scales, see waveCalcFilter in 6581_.cpp:
  //   filter state (filtLow, filtRef): FILTER_BITS fraction bits
  //   coefficients (filterDy, filterResDy and their tables, all <= 2.0): COEF_BITS
  typedef sdword filterfloat;
  #ifndef FILTER_BITS
  #define FILTER_BITS 16
  #endif
  #ifndef COEF_BITS
  #define COEF_BITS 29
  #endif
  #define FILTER_ONE (1L << FILTER_BITS)
  #define COEF_ONE (1L << COEF_BITS)
#endif


typedef void (*ptr2func)();


#endif  /* SIDPLAY1_MYTYPES_H */
