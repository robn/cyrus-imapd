/* cyr_lua.c -- run a Lua program inside Cyrus
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

#include <stdio.h>
#include <sysexits.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "global.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"


static int l_db_close (lua_State *L)
{
    struct db *db = luaL_checkudata(L, 1, "cyrus.db");

    cyrusdb_close(db);

    return 0;
}

static int l_db_open (lua_State *L) {
    const char *filename = luaL_checkstring(L, 1);
    const char *backend =  luaL_checkstring(L, 2);

    static struct db *db;

    int r = cyrusdb_open(backend, filename, 0, &db);
    if(r != CYRUSDB_OK) {
        // XXX return something that makes sense
        return 0;
    }

    lua_pushlightuserdata(L, db);

    if (luaL_newmetatable(L, "cyrus.db")) {
        lua_getglobal(L, "cyrus");
        lua_getfield(L, -1, "db");
        lua_setfield(L, -3, "__index");
        lua_pop(L, 1);

        lua_pushcfunction(L, l_db_close);
        lua_setfield(L, -2, "__gc");
    }

    lua_setmetatable(L, -2);

    return 1;
}

static int l_db_fetch (lua_State *L)
{
    struct db *db = luaL_checkudata(L, 1, "cyrus.db");
    const char *key = luaL_checkstring(L, 2);

    const char *res;
    size_t reslen;

    int r = cyrusdb_fetch(db, key, strlen(key), &res, &reslen, NULL);
    if (r == CYRUSDB_NOTFOUND)
      return 0;

    // XXX error checks

    lua_pushlstring(L, res, reslen);
    return 1;
}

static int l_db_store (lua_State *L)
{
    struct db *db = luaL_checkudata(L, 1, "cyrus.db");
    const char *key = luaL_checkstring(L, 2);
    const char *val = luaL_checkstring(L, 3);

    int r = cyrusdb_store(db, key, strlen(key), val, strlen(val), NULL);

    // XXX error checks

    return 0;
}

static int l_db_delete (lua_State *L)
{
    struct db *db = luaL_checkudata(L, 1, "cyrus.db");
    const char *key = luaL_checkstring(L, 2);

    int r = cyrusdb_delete(db, key, strlen(key), NULL, 1);

    // XXX error checks

    return 0;
}

static int l_db_pairs_next (lua_State *L) {
    struct db *db = lua_touserdata(L, lua_upvalueindex(1));
    const char *key = lua_tostring(L, lua_upvalueindex(2));
    size_t keylen = key ? strlen(key) : 0;
    const char *val;
    size_t vallen;

    int r = cyrusdb_fetchnext(db, key, keylen, &key, &keylen, &val, &vallen, NULL);
    /* XXX error checks */

    if (r == CYRUSDB_NOTFOUND) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushlstring(L, key, keylen);
    lua_pushvalue(L, -1);

    lua_replace(L, lua_upvalueindex(2));

    lua_pushlstring(L, val, vallen);

    return 2;
}

static int l_db_pairs (lua_State *L) {
    struct db *db = luaL_checkudata(L, 1, "cyrus.db");

    lua_pushlightuserdata(L, db);
    lua_pushnil(L);
    lua_pushcclosure(L, l_db_pairs_next, 3);

    return 1;
}

static void l_db_register (lua_State *L) {
    static const struct luaL_Reg cyrusdb_lib[] = {
        { "open",   l_db_open   },
        { "close",  l_db_close  },
        { "fetch",  l_db_fetch  },
        { "store",  l_db_store  },
        { "delete", l_db_delete },
        { "pairs",  l_db_pairs  },
        { NULL, NULL }
    };

    lua_newtable(L);
    luaL_register(L, NULL, cyrusdb_lib);
    lua_setfield(L, -2, "db");
}

int main (int argc, char *argv[])
{
    int opt;
    char *alt_config = NULL;
    char *program_text = NULL;
    const char *luafile;

    while ((opt = getopt(argc, argv, "C:e:")) != EOF) {
        switch (opt) {
        case 'C': /* alt config file */
            alt_config = optarg;
            break;

        case 'e': /* program text direct on command line */
            program_text = optarg;
            break;
        }
    }

    if ((argc - optind) < 1 && !program_text) {
        fprintf(stderr, "Usage: %s [-C altconfig] <program.lua>\n", argv[0]);
        fprintf(stderr, "       %s [-C altconfig] -e 'program text'\n", argv[0]);
        exit(EX_USAGE);
    }

    if (!program_text)
        luafile = argv[optind];

    cyrus_init(alt_config, "cyr_lua", 0, 0);

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_newtable(L);
    l_db_register(L);
    lua_setglobal(L, "cyrus");

    if (program_text) {
        if (luaL_loadbuffer(L, program_text, strlen(program_text), "-e")) {
            fprintf(stderr, "%s\n", lua_tostring(L, -1));
            exit(EX_DATAERR);
        }
    }

    else if (luaL_loadfile(L, luafile)) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        exit(EX_DATAERR);
    }

    if (lua_pcall(L, 0, 0, 0)) {
        fprintf(stderr, "%s\n", lua_tostring(L, -1));
        /* XXX clean shutdown? */
        exit(EX_DATAERR);
    }

    lua_close(L);

    cyrus_done();

    return 0;
}
