--mod-version:4

local core = require "core"
local image = require "libraries.image"
local RootView = require "core.rootview"
local config = require "core.config"
local View = require "core.view"
local common = require "core.common"
local Doc = require "core.doc"
local style = require "core.style"


config.plugins.imagepreview = common.merge({
  -- list of extensions to open with ImageView
  extensions = { "png", "jpeg", "jpg", "svg", "tga", "raw" }
}, config.plugins.imagepreview)

local ImageView = View:extend()

function ImageView:get_name()
  return common.basename(self.image_doc.abs_filename)
end

local ImageDoc = Doc:extend()

function ImageView:new(doc)
  ImageView.super.new(self, doc)
  self.image = image.load(doc.abs_filename)
  self.image_doc = doc
  self.canvas = canvas.new(self.image.width, self.image.height)
	self.canvas:set_pixels(self.image:save({ channels = 4 }), 0, 0, self.image.width, self.image.height)
end

function ImageView:draw()
  ImageView.super.draw(self)
  self:draw_background(style.background)
  renderer.draw_canvas(self.canvas, math.floor(self.position.x + style.padding.x), math.floor(self.position.y + style.padding.y))
end

local old_core_open_doc = core.open_doc
function core.open_doc(filename)
  local new_file = true
  local abs_filename
  if filename then
    -- normalize filename and set absolute filename then
    -- try to find existing doc for filename
    filename = core.root_project():normalize_path(filename)
    abs_filename = core.root_project():absolute_path(filename)
    new_file = not system.get_file_info(abs_filename)
    for _, doc in ipairs(core.docs) do
      if doc.abs_filename and abs_filename == doc.abs_filename then
        return doc
      end
    end
  end
  -- no existing doc for filename; create new
  for _, extension in ipairs(config.plugins.imagepreview.extensions) do
    if filename:find("%." .. extension .. "$") then
      local id = ImageDoc(filename, abs_filename, new_file)
      table.insert(core.docs, id)
      core.log_quiet(filename and "Opened doc \"%s\"" or "Opened new doc", filename)
      return id
    end
  end
  return old_core_open_doc(filename)
end


local old_rootview_open_doc = RootView.open_doc
function RootView:open_doc(doc)
  if not doc or getmetatable(doc) ~= ImageDoc then return old_rootview_open_doc(self, doc) end
  local node = self:get_active_node_default()
  for i, view in ipairs(node.views) do
    if view.doc == doc then
      node:set_active_view(node.views[i])
      return view
    end
  end
  local view = ImageView(doc)
  node:add_view(view)
  self.root_node:update_layout()
  return view
end
