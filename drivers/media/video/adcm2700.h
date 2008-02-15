
/*================================================================================
                                                                               
                      Header Name: adcm2700.h

General Description: Camera module adcm2700  interface head file
 
==================================================================================
                     Motorola Confidential Proprietary
                 Advanced Technology and Software Operations
               (c) Copyright Motorola 1999, All Rights Reserved
 
Revision History:
                            Modification     Tracking
Author                 Date          Number     Description of Changes
----------------   ------------    ----------   -------------------------
wangfei(w20239)      12/15/2003     LIBdd35749    Created   

==================================================================================
                                 INCLUDE FILES
==================================================================================*/

#ifndef _ADCM2700_H_
#define _ADCM2700_H_

#include "camera.h"

//////////////////////////////////////////////////////////////////////////////////////
//
//          Prototypes
//
//////////////////////////////////////////////////////////////////////////////////////

int camera_func_adcm2700_init(p_camera_context_t);
int camera_func_adcm2700_deinit(p_camera_context_t);
int camera_func_adcm2700_set_capture_format(p_camera_context_t);
int camera_func_adcm2700_start_capture(p_camera_context_t, unsigned int frames);
int camera_func_adcm2700_stop_capture(p_camera_context_t);

#endif /* _ADCM2700_H_ */


