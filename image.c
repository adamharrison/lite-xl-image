#ifdef LIBIMAGE_STANDALONE
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
#else
  #define LITE_XL_PLUGIN_ENTRYPOINT
  #include <lite_xl_plugin_api.h>
#endif

#include <errno.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

static int f_image_gc(lua_State* L) {
  lua_getfield(L, 1, "__data");
  stbi_image_free(lua_touserdata(L, -1));
  return 0;
}

static int f_image_create(lua_State* L, const char* bytes, int x, int y, int channels) {
  lua_newtable(L);
  luaL_setmetatable(L, "libimage");
  lua_pushinteger(L, x);
  lua_setfield(L, -2, "width");
  lua_pushinteger(L, y);
  lua_setfield(L, -2, "height");
  lua_pushinteger(L, channels);
  lua_setfield(L, -2, "channels");
  lua_pushlightuserdata(L, (void*)bytes);
  lua_setfield(L, -2, "__data");
  return 1;
}

static int f_image_new(lua_State* L) {
  size_t length;
  const char* buffer = luaL_checklstring(L, 1, &length);
  int x, y, channels;
  const char* bytes = stbi_load_from_memory(buffer, length, &x, &y, &channels, 0);
  return f_image_create(L, bytes, x, y, channels);
}

static int f_image_load(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int x, y, channels;
  const char* buffer = stbi_load(path, &x, &y, &channels, 0);
  return f_image_create(L, buffer, x, y, channels);
}

static void f_image_write_disk(void *context, void *data, int size) {
  FILE* f = (FILE*)context;
  fwrite(data, sizeof(char), size, f);
}

static void f_image_write_memory(void *context, void *data, int size) {
  luaL_Buffer* b = (luaL_Buffer*)context;
  luaL_addlstring(b, data, size);
}

static void f_image_write_callback(void *context, void *data, int size) {
  lua_State* L = (lua_State*)context;
  lua_pushvalue(L, -1);
  lua_pushlstring(L, data, size);
  lua_call(L, 1, 0);
}

static int f_image_save(lua_State* L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  int x, y, channels;
  lua_getfield(L, 1, "width");
  x = lua_tointeger(L, -1);
  lua_getfield(L, 1, "height");
  y = lua_tointeger(L, -1);
  lua_getfield(L, 1, "channels");
  channels = lua_tointeger(L, -1);
  lua_getfield(L, 1, "__data");
  const char* bytes = lua_touserdata(L, -1);
  lua_pop(L, 4);
  int options_table = -1;
  luaL_Buffer b;
  void* context = L;
  const char* format = "raw";
  
  if (lua_type(L, 3) == LUA_TTABLE)
    options_table = 3;
  else if (lua_type(L, 2) == LUA_TTABLE)
    options_table = 2;
  stbi_write_func* write_func = NULL;
  if (lua_type(L, 2) == LUA_TFUNCTION) 
    write_func = f_image_write_callback;
  else if (lua_type(L, 2) == LUA_TSTRING) {
    write_func = f_image_write_disk;
    size_t length;
    const char* path = luaL_checklstring(L, 2, &length);
    format = &path[length - 3];
    context = fopen(path, "wb");
  } else {
    write_func = f_image_write_memory;
    luaL_buffinit(L, &b);
    context = &b;
  }
  
  int quality = 50;
  int stride = x * channels;
  if (options_table != -1 && lua_type(L, options_table) == LUA_TTABLE) {
    lua_getfield(L, options_table, "quality");
    if (!lua_isnil(L, -1))
      quality = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, options_table, "stride");
    if (!lua_isnil(L, -1))
      stride = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, options_table, "format");
    if (!lua_isnil(L, -1))
      format = luaL_checkstring(L, -1);
    lua_pop(L, 1);
  }
  int status = 0;
  if (strcmp(format, "png") == 0)
    status = stbi_write_png_to_func(write_func, context, x, y, channels, bytes, stride);
  else if (strcmp(format, "jpg") == 0)
    status = stbi_write_jpg_to_func(write_func, context,x, y, channels, bytes, quality);
  else if (strcmp(format, "tga") == 0)
    status = stbi_write_tga_to_func(write_func, context,x, y, channels, bytes);
  else if (strcmp(format, "hdr") == 0)
    status = stbi_write_hdr_to_func(write_func, context, x, y, channels, (float*)bytes);
  else if (strcmp(format, "raw") == 0)
    write_func(context, (void*)bytes, channels * x * y);
  else {
    status = -1;
    lua_pushfstring(L, "unknown file format %s", format);
  }
  if (status != 0) {
    status = -1;
    lua_pushfstring(L, "unable to save image: %s", strerror(errno));
  }
  if (write_func == f_image_write_disk)
    fclose(context);
  else if (write_func == f_image_write_memory) {
    luaL_pushresult(&b);
    return 1;
  }
  return status == 0 ? 0 : lua_error(L);
}


static const luaL_Reg image_api[] = {
  { "__gc",          f_image_gc         },
  { "new",           f_image_new        },
  { "load",          f_image_load       },
  { "save",          f_image_save       },
  { NULL,            NULL               }
};

#ifndef IMAGE_VERSION
  #define IMAGE_VERSION "unknown"
#endif



#ifndef LIBIMAGE_STANDALONE
int luaopen_lite_xl_image(lua_State* L, void* XL) {
  lite_xl_plugin_init(XL);
#else
int luaopen_image(lua_State* L) {
#endif
  luaL_newmetatable(L, "libimage");
  luaL_setfuncs(L, image_api, 0);
  lua_pushliteral(L, IMAGE_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  return 1;
}

