/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include "dmapi_private.h"


int
dm_get_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	u_int		*nelemp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_region(tdp->td_bdp, tdp->td_right,
		nelem, regbufp, nelemp);

	dm_app_put_tdp(tdp);
	return(error);
}



int
dm_set_region(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_region_t	*regbufp,
	dm_boolean_t	*exactflagp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	int 		error;

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_REG,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->set_region(tdp->td_bdp, tdp->td_right,
		nelem, regbufp, exactflagp);

	dm_app_put_tdp(tdp);
	return(error);
}
