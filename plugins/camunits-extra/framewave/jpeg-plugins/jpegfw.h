
/*
//
//             INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//     Copyright (c) 2001-2006 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __JPEGFW_H__
#define __JPEGFW_H__

#include "jinclude.h"
#include "jpeglib.h"

#ifndef __FWJ_H__
#include "fwJPEG.h"
#endif


/* uncomment it for FW v1.1 beta */
//#define FW_11_BETA


//#define FW_VER 11
/* uncomment it to use new functions in fwJP v 2.0 */
#define FW_VER 20

/* We have changed some names after fwJP v1.1 Beta was released */
#ifdef FW_11_BETA

#define FwiEncodeHuffmanState                   FwiEncoderHuffmanState
#define FwiDecodeHuffmanState                   FwiDecoderHuffmanState
#define FwiEncodeHuffmanSpec                    FwiEncoderHuffmanSpec
#define FwiDecodeHuffmanSpec                    FwiDecoderHuffmanSpec

#define fwiQuantFwdTableInit_JPEG_8u16u         fwiQuantFwdSpecInit_JPEG_8u16u
#define fwiEncodeHuffmanRawTableInit_JPEG_8u    fwiEncoderHuffmanRawSpecInit_JPEG_8u

#define fwiDecodeHuffmanSpecGetBufSize_JPEG_8u  fwiDecoderHuffmanSpecGetBufSize_JPEG_8u
#define fwiDecodeHuffmanSpecInit_JPEG_8u        fwiDecoderHuffmanSpecInit_JPEG_8u
#define fwiDecodeHuffmanStateGetBufSize_JPEG_8u fwiDecoderHuffmanStateGetBufSize_JPEG_8u
#define fwiDecodeHuffmanStateInit_JPEG_8u       fwiDecoderHuffmanStateInit_JPEG_8u
#define fwiDecodeHuffman8x8_JPEG_1u16s_C1       fwiDecoderHuffman8x8_JPEG_1u16s_C1
#define fwiEncodeHuffmanSpecInit_JPEG_8u        fwiEncoderHuffmanSpecInit_JPEG_8u
#define fwiEncodeHuffmanSpecGetBufSize_JPEG_8u  fwiEncoderHuffmanSpecGetBufSize_JPEG_8u
#define fwiEncodeHuffmanStateGetBufSize_JPEG_8u fwiEncoderHuffmanStateGetBufSize_JPEG_8u
#define fwiEncodeHuffmanStateInit_JPEG_8u       fwiEncoderHuffmanStateInit_JPEG_8u
#define fwiEncodeHuffman8x8_JPEG_16s1u_C1       fwiEncoderHuffman8x8_JPEG_16s1u_C1

#endif


/* Wrappers for Intel JPEG primitives */

/* encoder color conversion */
METHODDEF(void)
rgb_ycc_convert_intellib(
  j_compress_ptr cinfo,
  JSAMPARRAY     input_buf,
  JSAMPIMAGE     output_buf,
  JDIMENSION     output_row,
  int            num_rows);

METHODDEF(void)
rgb_gray_convert_intellib(
  j_compress_ptr cinfo,
  JSAMPARRAY     input_buf,
  JSAMPIMAGE     output_buf,
  JDIMENSION     output_row,
  int            num_rows);

METHODDEF(void)
cmyk_ycck_convert_intellib(
  j_compress_ptr cinfo,
  JSAMPARRAY     input_buf,
  JSAMPIMAGE     output_buf,
  JDIMENSION     output_row,
  int            num_rows);


/* forward DCT */
METHODDEF(void)
forward_DCT_intellib(
  j_compress_ptr       cinfo,
  jpegfw_component_info* compptr,
  JSAMPARRAY           sample_data,
  JBLOCKROW            coef_blocks,
  JDIMENSION           start_row,
  JDIMENSION           start_col,
  JDIMENSION           num_blocks);

/* inverse DCT */
GLOBAL(void)
jpegfw_idct_islow_intellib(
  j_decompress_ptr     cinfo,
  jpegfw_component_info* compptr,
  JCOEFPTR             coef_block,
  JSAMPARRAY           output_buf,
  JDIMENSION           output_col);


LOCAL(void)
std_huff_tables_intellib(j_compress_ptr cinfo);


METHODDEF(void)
ycc_rgb_convert_intellib(
  j_decompress_ptr cinfo,
  JSAMPIMAGE       input_buf,
  JDIMENSION       input_row,
  JSAMPARRAY       output_buf,
  int              num_rows);

METHODDEF(void)
ycck_cmyk_convert_intellib(
  j_decompress_ptr cinfo,
  JSAMPIMAGE       input_buf,
  JDIMENSION       input_row,
  JSAMPARRAY       output_buf,
  int              num_rows);


METHODDEF(void)
h2v1_downsample_intellib(
  j_compress_ptr       cinfo,
  jpegfw_component_info* compptr,
  JSAMPARRAY           input_data,
  JSAMPARRAY           output_data);


METHODDEF(boolean)
empty_output_buffer_intellib (j_compress_ptr cinfo);


LOCAL(void)
htest_one_block_intellib(
  j_compress_ptr cinfo,
  JCOEFPTR       block,
  int            last_dc_val,
  long           dc_counts[],
  long           ac_counts[]);

GLOBAL(void)
jpegfw_gen_optimal_table_intellib(
  j_compress_ptr cinfo,
  JHUFF_TBL*     htbl,
  long           freq[]);


METHODDEF(boolean)
fill_input_buffer_intellib (j_decompress_ptr cinfo);


#endif /* __JPEGFW_H__ */
