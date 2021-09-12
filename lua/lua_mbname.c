/* lua_mbname.c -- Lua binding to mbname API
 *
 * Copyright (c) 1994-2021 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Carnegie Mellon University
 *      Center for Technology Transfer and Enterprise Creation
 *      4615 Forbes Avenue
 *      Suite 302
 *      Pittsburgh, PA  15213
 *      (412) 268-7393, fax: (412) 268-7395
 *      innovation@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "imap/mboxname.h"

static int l_mbname_free (lua_State *L)
{
    mbname_t *mbname = luaL_checkudata(L, 1, "cyrus.mbname");

    mbname_free(&mbname);

    return 0;
}

static int _l_mbname_new (lua_State *L, mbname_t *mbname) {
    if (!mbname) {
        // XXX error check
        return 0;
    }

    lua_pushlightuserdata(L, mbname);

    if (luaL_newmetatable(L, "cyrus.mbname")) {
        lua_getglobal(L, "cyrus");
        lua_getfield(L, -1, "mbname");
        lua_setfield(L, -3, "__index");
        lua_pop(L, 1);

        lua_pushcfunction(L, l_mbname_free);
        lua_setfield(L, -2, "__gc");
    }

    lua_setmetatable(L, -2);

    return 1;
}

static int l_mbname_from_userid (lua_State *L) {
    const char *userid = luaL_checkstring(L, 1);
    return _l_mbname_new(L, mbname_from_userid(userid));
}

/* XXX mbname_from_localdom not exported?
static int l_mbname_from_localdom (lua_State *L) {
    const char *localpart = luaL_checkstring(L, 1);
    const char *domain = luaL_checkstring(L, 2);
    return _l_mbname_new(L, mbname_from_localdom(localpart, domain));
}
*/

static int l_mbname_from_intname (lua_State *L) {
    const char *intname = luaL_checkstring(L, 1);
    return _l_mbname_new(L, mbname_from_intname(intname));
}

static int l_mbname_userid (lua_State *L) {
    mbname_t *mbname = luaL_checkudata(L, 1, "cyrus.mbname");

    const char *userid = mbname_userid(mbname);
    if (!userid)
        return 0;

    lua_pushstring(L, userid);
    return 1;
}

static int l_mbname_intname (lua_State *L) {
    mbname_t *mbname = luaL_checkudata(L, 1, "cyrus.mbname");

    const char *intname = mbname_intname(mbname);
    if (!intname)
        return 0;

    lua_pushstring(L, intname);
    return 1;
}

static int l_mbname_domain (lua_State *L) {
    mbname_t *mbname = luaL_checkudata(L, 1, "cyrus.mbname");

    const char *domain = mbname_domain(mbname);
    if (!domain)
        return 0;

    lua_pushstring(L, domain);
    return 1;
}

static int l_mbname_localpart (lua_State *L) {
    mbname_t *mbname = luaL_checkudata(L, 1, "cyrus.mbname");

    const char *localpart = mbname_localpart(mbname);
    if (!localpart)
        return 0;

    lua_pushstring(L, localpart);
    return 1;
}

void l_mbname_register (lua_State *L) {
    static const struct luaL_Reg mbname_lib[] = {
        { "from_userid",   l_mbname_from_userid },
        /* { "from_localdom", l_mbname_from_localdom }, */
        { "from_intname",  l_mbname_from_intname },

        { "userid",    l_mbname_userid },
        { "intname",   l_mbname_intname },
        { "domain",    l_mbname_domain },
        { "localpart", l_mbname_localpart },

        { NULL, NULL }
    };

    lua_newtable(L);
    luaL_register(L, NULL, mbname_lib);
    lua_setfield(L, -2, "mbname");
}
