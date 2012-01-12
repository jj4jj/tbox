/*!The Treasure Box Library
 * 
 * TBox is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * TBox is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with TBox; 
 * If not, see <a href="http://www.gnu.org/licenses/"> http://www.gnu.org/licenses/</a>
 * 
 * Copyright (C) 2009 - 2011, ruki All rights reserved.
 *
 * \author		ruki
 * \file		select.c
 *
 */
/* /////////////////////////////////////////////////////////
 * includes
 */
#include <sys/select.h>

/* /////////////////////////////////////////////////////////
 * types
 */

// the select reactor type
typedef struct __tb_aiop_reactor_select_t
{
	// the reactor base
	tb_aiop_reactor_t 		base;

	// the objects hash
	tb_hash_t* 				hash;

	// the fd max
	tb_size_t 				sfdm;

	// the select fds
	fd_set 					rfdi;
	fd_set 					wfdi;
	fd_set 					efdi;

	fd_set 					rfdo;
	fd_set 					wfdo;
	fd_set 					efdo;
	
}tb_aiop_reactor_select_t;

/* /////////////////////////////////////////////////////////
 * implemention
 */
static tb_bool_t tb_aiop_reactor_select_addo(tb_aiop_reactor_t* reactor, tb_handle_t handle, tb_size_t etype)
{
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->hash && reactor->aiop, TB_FALSE);

	// fd
	tb_long_t fd = ((tb_long_t)handle) - 1;
	tb_assert_and_check_return_val(fd >= 0, TB_FALSE);
	tb_assert_and_check_return_val(tb_hash_size(rtor->hash) < FD_SETSIZE, TB_FALSE);

	// update fd max
	if (fd > rtor->sfdm) rtor->sfdm = fd;
	
	// init fds
	fd_set* prfds = (etype & TB_AIOO_ETYPE_READ || etype & TB_AIOO_ETYPE_ACPT)? &rtor->rfdi : TB_NULL;
	fd_set* pwfds = (etype & TB_AIOO_ETYPE_WRIT || etype & TB_AIOO_ETYPE_CONN)? &rtor->wfdi : TB_NULL;
	if (prfds) FD_SET(fd, prfds);
	if (pwfds) FD_SET(fd, pwfds);
	FD_SET(fd, &rtor->efdi);

	// init obj
	tb_aioo_t o;
	tb_aioo_seto(&o, handle, reactor->aiop->type, etype);

	// add obj
	tb_hash_set(rtor->hash, fd, &o);
	
	// ok
	return TB_TRUE;
}
static tb_bool_t tb_aiop_reactor_select_seto(tb_aiop_reactor_t* reactor, tb_handle_t handle, tb_size_t etype)
{
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->hash && reactor->aiop, TB_FALSE);

	// fd
	tb_long_t fd = ((tb_long_t)handle) - 1;
	tb_assert_and_check_return_val(fd >= 0, TB_FALSE);

	// get obj
	tb_aioo_t* o = tb_hash_get(rtor->hash, fd);
	tb_assert_and_check_return_val(o, TB_FALSE);

	// set fds
	fd_set* prfds = (etype & TB_AIOO_ETYPE_READ || etype & TB_AIOO_ETYPE_ACPT)? &rtor->rfdi : TB_NULL;
	fd_set* pwfds = (etype & TB_AIOO_ETYPE_WRIT || etype & TB_AIOO_ETYPE_CONN)? &rtor->wfdi : TB_NULL;
	if (prfds) FD_SET(fd, prfds); else FD_CLR(fd, prfds);
	if (pwfds) FD_SET(fd, pwfds); else FD_CLR(fd, pwfds);
	if (prfds || pwfds) FD_SET(fd, &rtor->efdi); else FD_CLR(fd, &rtor->efdi);

	// set obj
	tb_aioo_seto(o, handle, reactor->aiop->type, etype);

	// ok
	return TB_TRUE;
}
static tb_bool_t tb_aiop_reactor_select_delo(tb_aiop_reactor_t* reactor, tb_handle_t handle)
{
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->hash, TB_FALSE);

	// fd
	tb_long_t fd = ((tb_long_t)handle) - 1;
	tb_assert_and_check_return_val(fd >= 0, TB_FALSE);

	// del fds
	FD_CLR(fd, &rtor->rfdi);
	FD_CLR(fd, &rtor->wfdi);
	FD_CLR(fd, &rtor->efdi);

	// del obj
	tb_hash_del(rtor->hash, fd);
	
	// ok
	return TB_TRUE;
}
static tb_long_t tb_aiop_reactor_select_wait(tb_aiop_reactor_t* reactor, tb_long_t timeout)
{	
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	tb_assert_and_check_return_val(rtor && rtor->hash, -1);

	// init time
	struct timeval t = {0};
	if (timeout > 0)
	{
		t.tv_sec = timeout / 1000;
		t.tv_usec = (timeout % 1000) * 1000;
	}

	// init fdo
	tb_memcpy(&rtor->rfdo, &rtor->rfdi, sizeof(fd_set));
	tb_memcpy(&rtor->wfdo, &rtor->wfdi, sizeof(fd_set));
	tb_memcpy(&rtor->efdo, &rtor->efdi, sizeof(fd_set));

	// wait
	tb_long_t sfdn = select(rtor->sfdm + 1, &rtor->rfdo, &rtor->wfdo, &rtor->efdo, timeout >= 0? &t : TB_NULL);
	tb_assert_and_check_return_val(sfdn >= 0, -1);

	// timeout?
	tb_check_return_val(sfdn, 0);

	// ok
	return sfdn;
}
static tb_void_t tb_aiop_reactor_select_sync(tb_aiop_reactor_t* reactor, tb_size_t evtn)
{	
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	tb_assert_and_check_return(rtor && rtor->hash && reactor->aiop && reactor->aiop->objs);

	// sync
	tb_size_t i = 0;
	tb_size_t itor = tb_hash_itor_head(rtor->hash);
	tb_size_t tail = tb_hash_itor_tail(rtor->hash);
	for (; itor != tail; itor = tb_hash_itor_next(rtor->hash, itor))
	{
		tb_hash_item_t* item = tb_hash_itor_at(rtor->hash, itor);
		if (item)
		{
			tb_long_t 		fd = (tb_long_t)item->name;
			tb_aioo_t* 	o = (tb_aioo_t*)item->data;
			if (fd >= 0 && o)
			{
				tb_long_t e = 0;
				if (FD_ISSET(fd, &rtor->rfdo)) 
				{
					e |= TB_AIOO_ETYPE_READ;
					if (o->etype & TB_AIOO_ETYPE_ACPT) e |= TB_AIOO_ETYPE_ACPT;
				}
				if (FD_ISSET(fd, &rtor->wfdo)) 
				{
					e |= TB_AIOO_ETYPE_WRIT;
					if (o->etype & TB_AIOO_ETYPE_CONN) e |= TB_AIOO_ETYPE_CONN;
				}
				if (FD_ISSET(fd, &rtor->efdo) && !(e & (TB_AIOO_ETYPE_READ | TB_AIOO_ETYPE_WRIT))) 
					e |= TB_AIOO_ETYPE_READ | TB_AIOO_ETYPE_WRIT;
					
				// add object
				if (e && i < reactor->aiop->objn) reactor->aiop->objs[i++] = *o;
			}
		}
	}
}
static tb_void_t tb_aiop_reactor_select_exit(tb_aiop_reactor_t* reactor)
{
	tb_aiop_reactor_select_t* rtor = (tb_aiop_reactor_select_t*)reactor;
	if (rtor)
	{
		// free fds
		FD_ZERO(&rtor->rfdi);
		FD_ZERO(&rtor->wfdi);
		FD_ZERO(&rtor->efdi);
		FD_ZERO(&rtor->rfdo);
		FD_ZERO(&rtor->wfdo);
		FD_ZERO(&rtor->efdo);

		// exit hash
		if (rtor->hash) tb_hash_exit(rtor->hash);

		// free it
		tb_free(rtor);
	}
}
static tb_aiop_reactor_t* tb_aiop_reactor_select_init(tb_aiop_t* aiop)
{
	// check
	tb_assert_and_check_return_val(aiop && aiop->maxn, TB_NULL);
	tb_assert_and_check_return_val(aiop->type == TB_AIOO_OTYPE_FILE || aiop->type == TB_AIOO_OTYPE_SOCK, TB_NULL);

	// alloc reactor
	tb_aiop_reactor_select_t* rtor = tb_calloc(1, sizeof(tb_aiop_reactor_select_t));
	tb_assert_and_check_return_val(rtor, TB_NULL);

	// init base
	rtor->base.aiop = aiop;
	rtor->base.exit = tb_aiop_reactor_select_exit;
	rtor->base.addo = tb_aiop_reactor_select_addo;
	rtor->base.seto = tb_aiop_reactor_select_seto;
	rtor->base.delo = tb_aiop_reactor_select_delo;
	rtor->base.wait = tb_aiop_reactor_select_wait;
	rtor->base.sync = tb_aiop_reactor_select_sync;

	// init hash
	rtor->hash = tb_hash_init(tb_align8(tb_int32_sqrt(aiop->maxn) + 1), tb_item_func_int(), tb_item_func_ifm(sizeof(tb_aioo_t), TB_NULL, TB_NULL));
	tb_assert_and_check_goto(rtor->hash, fail);

	// init fds
	FD_ZERO(&rtor->rfdi);
	FD_ZERO(&rtor->wfdi);
	FD_ZERO(&rtor->efdi);
	FD_ZERO(&rtor->rfdo);
	FD_ZERO(&rtor->wfdo);
	FD_ZERO(&rtor->efdo);

	// ok
	return (tb_aiop_reactor_t*)rtor;

fail:
	if (rtor) tb_aiop_reactor_select_exit(rtor);
	return TB_NULL;
}
