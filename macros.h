/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2011 Emil Renner Berthing
 *
 * LEM is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * LEM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LEM.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Support gcc's __FUNCTION__ for people using other compilers */
#if !defined(__GNUC__) && !defined(__FUNCTION__)
# define __FUNCTION__ __func__ /* C99 */
#endif

#ifdef NDEBUG
#define lem_debug(...)
#else
#define lem_debug(fmt, ...)                                              \
        printf("%s (%s:%u): " fmt "\n",                                  \
	       __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__);         \
        fflush(stdout)
#endif

#if EV_MINPRI == EV_MAXPRI
# undef ev_priority
# undef ev_set_priority
# define ev_priority(pri)
# define ev_set_priority(ev, pri)
#endif

#if EV_MULTIPLICITY
# define EV_G lem_loop
# define EV_G_ EV_G,
#else
# define EV_G
# define EV_G_
#endif
