## image

A simple image loading and saving library, designed to be used with `canvas` in lite-xl 3.0, using `stb_image` and `stb_image_write`.

### Usage

```lua
local image = require "libraries.image"

local i = image.load("myimage.png")
i = image.new(io.open("myimage.png", "rb"):read("*all"))
print(i.width)
print(i.height)
print(#i:save())

i:save("myimage.jpg")
i:save("myimage.jpg", { quality = 100 })
local bytes = i:save()
print(#bytes)
local jpeg_bytes = i:save({ format = "jpg" })
print(#jpeg_bytes)
i:save(function(bytes)
  print(#bytes)
end, { format = "jpg" })
```
