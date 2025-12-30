--mod-version:4

local core = require "core"
local image = require "libraries.image"
local RootView = require "core.rootview"
local config = require "core.config"
local command = require "core.command"
local keymap = require "core.keymap"
local View = require "core.view"
local common = require "core.common"
local Object = require "core.object"
local style = require "core.style"


config.plugins.imagepreview = common.merge({
  -- list of extensions to open with ImageView
  extensions = { "png", "jpeg", "jpg", "svg", "tga", "raw" }
}, config.plugins.imagepreview)

local ImageView = View:extend()

function ImageView:get_name()
  return string.format("[%dx%d] %s", self.image.image.width, self.image.image.height, common.basename(self.image.abs_filename))
end

function ImageView:new(image)
  ImageView.super.new(self, image)
  self.image = image
  self.canvas = canvas.new(self.image.image.width, self.image.image.height)
	self.canvas:set_pixels(self.image.image:save({ channels = 4 }), 0, 0, self.image.image.width, self.image.image.height)
end

function ImageView:draw()
  ImageView.super.draw(self)
  self:draw_background(style.background)
  renderer.draw_canvas(self.canvas, math.floor(self.position.x + (self.size.x - self.image.image.width) / 2), math.floor(self.position.y + (self.size.y - self.image.image.height) / 2))
end

local old_core_open_doc = core.open_doc
core.images = setmetatable({}, { __mode = "v" })
function core.open_doc(filename)
  -- no existing doc for filename; create new
  for _, extension in ipairs(config.plugins.imagepreview.extensions) do
    if filename:find("%." .. extension .. "$") then
      local abs_filename = core.root_project():absolute_path(filename)
      if not core.images[abs_filename] then
        core.images[abs_filename] = { abs_filename = abs_filename, image = image.load(abs_filename) }
        core.log_quiet("Opened image \"%s\"", common.basename(abs_filename))
      end
      return core.images[abs_filename]
    end
  end
  return old_core_open_doc(filename)
end


local old_rootview_open_doc = RootView.open_doc
function RootView:open_doc(doc)
  if not doc or not doc.image then return old_rootview_open_doc(self, doc) end
  local node = self:get_active_node_default()
  for i, view in ipairs(node.views) do
    if view.image == doc then
      node:set_active_view(node.views[i])
      return view
    end
  end
  local view = ImageView(doc)
  node:add_view(view)
  self.root_node:update_layout()
  return view
end

command.add(ImageView, {
  ["imageview:save-as"] = function(iv) 
    iv.root_view.command_view:enter("Save As", {
      text = core.root_project():normalize_path(iv.image.abs_filename),
      submit = function(filename)
        iv.image.image:save(common.home_expand(filename))
        iv.image.abs_filename = core.root_project():absolute_path(filename)
      end,
      suggest = function(text)
        return common.home_encode_list(common.path_suggest(common.home_expand(text)))
      end
    })
  end
})

keymap.add {
  ["ctrl+shift+s"] = "imageview:save-as"
}
