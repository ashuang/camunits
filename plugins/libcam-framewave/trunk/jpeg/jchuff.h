/*
 * jchuff.h
 *
 * Copyright (C) 1991-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains declarations for Huffman entropy encoding routines
 * that are shared between the sequential encoder (jchuff.c) and the
 * progressive encoder (jcphuff.c).  No other modules need to see these.
 */

/* The legal range of a DCT coefficient is
 *  -1024 .. +1023  for 8-bit data;
 * -16384 .. +16383 for 12-bit data.
 * Hence the magnitude should always fit in 10 or 14 bits respectively.
 */

#ifdef FWJ_HUFF
#include "jpegfw.h"
#endif

#if BITS_IN_JSAMPLE == 8
#define MAX_COEF_BITS 10
#else
#define MAX_COEF_BITS 14
#endif

/* Derived data constructed for each Huffman table */

typedef struct {
  unsigned int ehufco[256]; /* code for each symbol */
  char ehufsi[256];   /* length of code for each symbol */
  /* If no code has been allocated for a symbol S, ehufsi[S] contains 0 */
#ifdef FWJ_HUFF
  FwiEncodeHuffmanSpec* pHuffTbl;
#endif
} c_derived_tbl;


/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpegfw_make_c_derived_tbl jMkCDerived
#define jpegfw_gen_optimal_table  jGenOptTbl
#endif /* NEED_SHORT_EXTERNAL_NAMES */

/* Expand a Huffman table definition into the derived format */
EXTERN(void) jpegfw_make_c_derived_tbl
  JPP((j_compress_ptr cinfo, boolean isDC, int tblno,
       c_derived_tbl ** pdtbl));
#ifdef FWJ_HUFF
EXTERN(void) jpegfw_make_c_derived_tbl_intellib
  JPP((j_compress_ptr cinfo, boolean isDC, int tblno,
       c_derived_tbl ** pdtbl));
#endif

/* Generate an optimal table definition given the specified counts */
EXTERN(void) jpegfw_gen_optimal_table
  JPP((j_compress_ptr cinfo, JHUFF_TBL * htbl, long freq[]));
#ifdef FWJ_HUFF
EXTERN(void) jpegfw_gen_optimal_table_intellib
  JPP((j_compress_ptr cinfo, JHUFF_TBL * htbl, long freq[]));
#endif
