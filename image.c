#include <stdint.h>
#ifdef LIBIMAGE_STANDALONE
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
#else
  #define LITE_XL_PLUGIN_ENTRYPOINT
  #include "lib/lite-xl/resources/include/lite_xl_plugin_api.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb/stb_image_write.h"
#define NANOSVG_IMPLEMENTATION
#include "lib/nanosvg/src/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "lib/nanosvg/src/nanosvgrast.h"

static int f_image_gc(lua_State* L) {
  lua_getfield(L, 1, "__data");
  free(lua_touserdata(L, -1));
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

static char* f_rasterize(const char* bytes, int* x, int* y, int* channels, int copy) {
  char* buffer = copy ? strdup(bytes) : (char*)bytes;
  NSVGimage* image = nsvgParse(buffer, "px", 96);
  if (!image)
    return NULL;
  NSVGrasterizer* rast = nsvgCreateRasterizer();
  float scale = 1.0f;
  if (*x == -1 && *y == -1) {
    *x = image->width;
    *y = image->height;
  } else {
    scale = *x / (float)image->width;
  }
  *channels = 4;
  char* raster_buffer = malloc(*x * *y * *channels);
  nsvgRasterize(rast, image, 0, 0, scale, raster_buffer, *x, *y, *x * *channels);
  nsvgDelete(image);
  nsvgDeleteRasterizer(rast);
  if (copy)
    free(buffer);
  return raster_buffer;
}

static int f_image_new(lua_State* L) {
  size_t length;
  const char* buffer = luaL_checklstring(L, 1, &length);
  // first non-whitespace cahracter should be <. If so, it's likely an XML document.
  int is_svg = 0;
  for (int i = 0; i < length; ++i) {
    if (isspace(buffer[i]))
      continue;
    if (buffer[i] == '<')
      is_svg = 1;
    break;
  }
  int x = -1, y = -1, channels;
  char* bytes;
  if (is_svg) {
    if (lua_gettop(L) >= 2) {
      x = luaL_checkinteger(L, 2);
      y = luaL_checkinteger(L, 3);
    }
    bytes = f_rasterize(buffer, &x, &y, &channels, 1);
  } else 
    bytes = stbi_load_from_memory(buffer, length, &x, &y, &channels, 0);
  return f_image_create(L, bytes, x, y, channels);
}

static int f_image_load(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int x = -1, y = -1, channels;
  char* bytes;
  if (strstr(path, ".svg")) {
    FILE* file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    char* contents = malloc(length + 1);
    fseek(file, 0, SEEK_SET);
    if (fread(contents, sizeof(char), length, file) != length) {
      fclose(file);
      free(contents);
      return luaL_error(L, "error reading file");
    }
    contents[length] = 0;
    fclose(file);
    if (lua_gettop(L) >= 2) {
      x = luaL_checkinteger(L, 2);
      y = luaL_checkinteger(L, 3);
    }
    bytes = f_rasterize(contents, &x, &y, &channels, 0);
    free(contents);
  } else
    bytes = stbi_load(path, &x, &y, &channels, 0);
  return f_image_create(L, bytes, x, y, channels);
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
    if (!context)
      return luaL_error(L, "can't open file: %s", strerror(errno));
  } else {
    write_func = f_image_write_memory;
    luaL_buffinit(L, &b);
    context = &b;
  }
  
  int quality = 50;
  int target_channels = channels;
  int stride = -1;
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
    lua_getfield(L, options_table, "channels");
    if (!lua_isnil(L, -1))
      target_channels = luaL_checkinteger(L, -1);
    lua_pop(L, 1);
  }
  if (target_channels != channels) {
    char* target_bytes = malloc(target_channels * x * y);
    uint8_t byte;
    int target = 0;
    int source = 0;
    int lowest_channels = channels < target_channels ? channels : target_channels;
    for (int j = 0; j < y; ++j) {
      for (int i = 0; i < x; ++i) {
        for (int c = 0; c < lowest_channels; ++c)
          target_bytes[target++] = bytes[source++];  
        for (int c = channels; c < target_channels; ++c)
          target_bytes[target++] = 0xFF;
        if (target_channels < channels)
          source += (channels - target_channels);
      }
    }
    bytes = target_bytes;
  }
  if (stride == -1)
    stride = target_channels * x;
  int status = -1;
  if (strcmp(format, "png") == 0)
    status = stbi_write_png_to_func(write_func, context, x, y, target_channels, bytes, stride);
  else if (strcmp(format, "jpg") == 0)
    status = stbi_write_jpg_to_func(write_func, context, x, y, target_channels, bytes, quality);
  else if (strcmp(format, "tga") == 0)
    status = stbi_write_tga_to_func(write_func, context, x, y, target_channels, bytes);
  else if (strcmp(format, "hdr") == 0)
    status = stbi_write_hdr_to_func(write_func, context, x, y, target_channels, (float*)bytes);
  else if (strcmp(format, "raw") == 0)
    write_func(context, (void*)bytes, target_channels * x * y);
  else if (strcmp(format, "svg") == 0) {
    status = 0;
    lua_pushfstring(L, "writing to svg unsupported");
  } else {
    status = 0;
    lua_pushfstring(L, "unknown file format %s", format);
  }
  if (target_channels != channels)
    free((char*)bytes);
  if (status)
    lua_pushfstring(L, "unable to save image: %s", strerror(errno));
  if (write_func == f_image_write_disk)
    fclose(context);
  else if (write_func == f_image_write_memory) {
    luaL_pushresult(&b);
    return 1;
  }
  return status == 0 ? lua_error(L) : 0;
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

