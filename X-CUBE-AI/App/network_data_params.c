/**
  ******************************************************************************
  * @file    network_data_params.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-09-14T00:42:20+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "network_data_params.h"


/**  Activations Section  ****************************************************/
ai_handle g_network_activations_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(NULL),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};




/**  Weights Section  ********************************************************/
AI_ALIGNED(32)
const ai_u64 s_network_weights_array_u64[21] = {
  0xe546d5f9eed80e7fU, 0xfa6d8172dd0bc5d5U, 0xa671e6e01623267fU, 0xf50c24ff15c27fU,
  0x44f050246cd61781U, 0xf7b1861f5dfc81f9U, 0x899f0c90b37fabcdU, 0x4700c1529e0a3e81U,
  0xffffe207U, 0xffffdccbffffd2cdU, 0x1913U, 0x294c00000000U,
  0xcf41513f904fb81U, 0xc00b0121fc1fde81U, 0xaafc77ecd95c0581U, 0xdc0307ef2d39f17fU,
  0x10200000462U, 0xfffffa6100000000U, 0x7fd2680d81101b35U, 0xffffff767fe3b5ecU,
  0x1f4fffffc6eU,
};


ai_handle g_network_weights_table[1 + 2] = {
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
  AI_HANDLE_PTR(s_network_weights_array_u64),
  AI_HANDLE_PTR(AI_MAGIC_MARKER),
};

