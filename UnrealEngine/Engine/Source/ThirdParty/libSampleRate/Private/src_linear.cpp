/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/libsndfile/libsamplerate/blob/master/COPYING
*/

#ifndef LIBSAMPLERATE_WITHOUT_LINEAR

#include "CoreMinimal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "src_config.h"
#include "common.h"

static enum SRC_ERR linear_vari_process (SRC_STATE *state, SRC_DATA *data) ;
static void linear_reset (SRC_STATE *state) ;
static enum SRC_ERR linear_copy (SRC_STATE *from, SRC_STATE *to) ;

/*========================================================================================
*/

#define	LINEAR_MAGIC_MARKER	MAKE_MAGIC ('l', 'i', 'n', 'e', 'a', 'r')

#define	SRC_DEBUG	0

typedef struct
{	int		linear_magic_marker ;
	int		dirty ;
	long	in_count, in_used ;
	long	out_count, out_gen ;
	float	last_value [] ;
} LINEAR_DATA ;

/*----------------------------------------------------------------------------------------
*/

static enum SRC_ERR
linear_vari_process (SRC_STATE *state, SRC_DATA *data)
{	LINEAR_DATA *priv ;
	double		src_ratio, input_index, rem ;
	int			ch ;

	if (data->input_frames <= 0)
		return SRC_ERR_NO_ERROR ;

	if (state->private_data == NULL)
		return SRC_ERR_NO_PRIVATE ;

	priv = (LINEAR_DATA*) state->private_data ;

	if (!priv->dirty)
	{	/* If we have just been reset, set the last_value data. */
		for (ch = 0 ; ch < state->channels ; ch++)
			priv->last_value [ch] = data->data_in [ch] ;
		priv->dirty = true ;
		} ;

	priv->in_count = data->input_frames * state->channels ;
	priv->out_count = data->output_frames * state->channels ;
	priv->in_used = priv->out_gen = 0 ;

	src_ratio = state->last_ratio ;

	if (is_bad_src_ratio (src_ratio))
		return SRC_ERR_BAD_INTERNAL_STATE ;

	input_index = state->last_position ;

	/* Calculate samples before first sample in input array. */
	while (input_index < 1.0 && priv->out_gen < priv->out_count)
	{
		if (priv->in_used + state->channels * (1.0 + input_index) > priv->in_count)
			break ;

		if (priv->out_count > 0 && fabs (state->last_ratio - data->src_ratio) > SRC_MIN_RATIO_DIFF)
			src_ratio = state->last_ratio + priv->out_gen * (data->src_ratio - state->last_ratio) / priv->out_count ;

		for (ch = 0 ; ch < state->channels ; ch++)
		{	data->data_out [priv->out_gen] = (float) (priv->last_value [ch] + input_index *
										(data->data_in [ch] - priv->last_value [ch])) ;
			priv->out_gen ++ ;
			} ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		} ;

	rem = fmod_one (input_index) ;
	priv->in_used += state->channels * lrint (input_index - rem) ;
	input_index = rem ;

	/* Main processing loop. */
	while (priv->out_gen < priv->out_count && priv->in_used + state->channels * input_index < priv->in_count)
	{
		if (priv->out_count > 0 && fabs (state->last_ratio - data->src_ratio) > SRC_MIN_RATIO_DIFF)
			src_ratio = state->last_ratio + priv->out_gen * (data->src_ratio - state->last_ratio) / priv->out_count ;

		if (SRC_DEBUG && priv->in_used < state->channels && input_index < 1.0)
		{	printf ("Whoops!!!!   in_used : %ld     channels : %d     input_index : %f\n", priv->in_used, state->channels, input_index) ;
			exit (1) ;
			} ;

		for (ch = 0 ; ch < state->channels ; ch++)
		{	data->data_out [priv->out_gen] = (float) (data->data_in [priv->in_used - state->channels + ch] + input_index *
						(data->data_in [priv->in_used + ch] - data->data_in [priv->in_used - state->channels + ch])) ;
			priv->out_gen ++ ;
			} ;

		/* Figure out the next index. */
		input_index += 1.0 / src_ratio ;
		rem = fmod_one (input_index) ;

		priv->in_used += state->channels * lrint (input_index - rem) ;
		input_index = rem ;
		} ;

	if (priv->in_used > priv->in_count)
	{	input_index += (priv->in_used - priv->in_count) / state->channels ;
		priv->in_used = priv->in_count ;
		} ;

	state->last_position = input_index ;

	if (priv->in_used > 0)
		for (ch = 0 ; ch < state->channels ; ch++)
			priv->last_value [ch] = data->data_in [priv->in_used - state->channels + ch] ;

	/* Save current ratio rather then target ratio. */
	state->last_ratio = src_ratio ;

	data->input_frames_used = priv->in_used / state->channels ;
	data->output_frames_gen = priv->out_gen / state->channels ;

	return SRC_ERR_NO_ERROR ;
} /* linear_vari_process */

/*------------------------------------------------------------------------------
*/

const char*
linear_get_name (int src_enum)
{
	if (src_enum == SRC_LINEAR)
		return "Linear Interpolator" ;

	return NULL ;
} /* linear_get_name */

const char*
linear_get_description (int src_enum)
{
	if (src_enum == SRC_LINEAR)
		return "Linear interpolator, very fast, poor quality." ;

	return NULL ;
} /* linear_get_descrition */

enum SRC_ERR
linear_set_converter (SRC_STATE *state, int src_enum)
{	LINEAR_DATA *priv = NULL ;

	if (src_enum != SRC_LINEAR)
		return SRC_ERR_BAD_CONVERTER ;

	if (state->private_data != NULL)
	{	FMemory::Free (state->private_data) ;
		state->private_data = NULL ;
		} ;

	if (state->private_data == NULL)
	{	
		priv = (LINEAR_DATA*)FMemory::Malloc(sizeof(LINEAR_DATA) + state->channels * sizeof(float));
		state->private_data = priv ;
		} ;

	if (priv == NULL)
		return SRC_ERR_MALLOC_FAILED ;

	priv->linear_magic_marker = LINEAR_MAGIC_MARKER ;

	state->const_process = linear_vari_process ;
	state->vari_process = linear_vari_process ;
	state->reset = linear_reset ;
	state->copy = linear_copy ;

	linear_reset (state) ;

	return SRC_ERR_NO_ERROR ;
} /* linear_set_converter */

/*===================================================================================
*/

static void
linear_reset (SRC_STATE *state)
{	LINEAR_DATA *priv = NULL ;

	priv = (LINEAR_DATA*) state->private_data ;
	if (priv == NULL)
		return ;

	priv->dirty = false ;
	FMemory::Memset(priv->last_value, 0, sizeof (priv->last_value [0]) * state->channels) ;

	return ;
} /* linear_reset */

static enum SRC_ERR
linear_copy (SRC_STATE *from, SRC_STATE *to)
{
	if (from->private_data == NULL)
		return SRC_ERR_NO_PRIVATE ;

	LINEAR_DATA *to_priv = NULL ;
	LINEAR_DATA* from_priv = (LINEAR_DATA*) from->private_data ;
	size_t private_size = sizeof (*to_priv) + from->channels * sizeof (float) ;

	to_priv = (LINEAR_DATA*)FMemory::Malloc(private_size);
	if (to_priv == NULL)
		return SRC_ERR_MALLOC_FAILED ;

	FMemory::Memcpy (to_priv, from_priv, private_size) ;
	to->private_data = to_priv ;

	return SRC_ERR_NO_ERROR ;
} /* linear_copy */

#endif // LIBSAMPLERATE_WITHOUT_LINEAR
