
/*================================================================================
                                                                               
                      Header Name: mt9m111.h

General Description: Camera module mt9m111  interface head file
 
==================================================================================
                     Motorola Confidential Proprietary
                 Advanced Technology and Software Operations
               (c) Copyright Motorola 1999, All Rights Reserved
 
Revision History:
                            Modification     Tracking
Author                 Date          Number     Description of Changes
----------------   ------------    ----------   -------------------------
Ma Zhiqiang         6/30/2004                    Change for auto-detect

==================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _MT9M111_H_
#define _MT9M111_H_

#include "camera.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//          Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////

int camera_func_mt9m111_init(p_camera_context_t);
int camera_func_mt9m111_deinit(p_camera_context_t);
int camera_func_mt9m111_set_capture_format(p_camera_context_t);
int camera_func_mt9m111_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_mt9m111_stop_capture(p_camera_context_t);
int camera_func_mt9m111_pm_management(p_camera_context_t cam_ctx, int suspend);
int camera_func_mt9m111_docommand(p_camera_context_t cam_ctx, unsigned int cmd, void *param);

#endif /* _MT9M111_H_ */


