#!/usr/bin/env python3
import os
from PIL import Image

def image_to_lvgl_c_array(image_path, var_name):
    img = Image.open(image_path).convert('RGBA')
    width, height = img.size
    pixels = img.load()
    
    lines = []
    lines.append(f"LV_ATTRIBUTE_MEM_ALIGN")
    lines.append(f"static const uint8_t {var_name}_map[{width} * {height} * 4] = {{")
    
    pixel_values = []
    for y in range(height):
        row = []
        for x in range(width):
            r, g, b, a = pixels[x, y]
            row.append(f"0x{b:02X},0x{g:02X},0x{r:02X},0x{a:02X}")
        pixel_values.append("    " + ",".join(row) + ",")
    
    lines.extend(pixel_values)
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_image_dsc_t {var_name} = {{")
    lines.append("    .header = {")
    lines.append("        .magic = LV_IMAGE_HEADER_MAGIC,")
    lines.append("        .cf = LV_COLOR_FORMAT_ARGB8888,")
    lines.append(f"        .w = {width},")
    lines.append(f"        .h = {height},")
    lines.append("    },")
    lines.append(f"    .data_size = {width} * {height} * 4,")
    lines.append(f"    .data = {var_name}_map,")
    lines.append("};")
    
    return "\n".join(lines), width, height

def main():
    assets_dir = "assets"
    output_file = "generated_images.txt"
    
    eye_files = [
        "eye_left_normal", "eye_right_normal",
        "eye_left_big", "eye_right_big",
        "eye_left_happy", "eye_right_happy",
        "eye_left_think", "eye_right_think",
        "eye_left_shy", "eye_right_shy",
        "eye_left_sad", "eye_right_sad",
        "eye_left_close", "eye_right_close",
        "eye_left_big_big", "eye_right_big_big",
        "eye_left_wink", "eye_right_wink",
    ]
    
    mouth_files = [
        "mouth_close", "mouth_smile", "mouth_small_smile",
        "mouth_laugh", "mouth_open1", "mouth_open2",
        "mouth_o", "mouth_down", "mouth_relax", "mouth_surprise"
    ]
    
    blush_files = ["blush"]
    
    bg_files = ["bg"]
    
    all_files = eye_files + mouth_files + blush_files + bg_files
    
    results = []
    defines = []
    defined_macros = set()
    
    for name in all_files:
        image_path = os.path.join(assets_dir, f"{name}.png")
        if os.path.exists(image_path):
            print(f"Processing: {name}.png")
            code, w, h = image_to_lvgl_c_array(image_path, name)
            results.append(code)
            
            if name.startswith("eye"):
                if "EYE_W" not in defined_macros:
                    defines.append(f"#define EYE_W {w}")
                    defines.append(f"#define EYE_H {h}")
                    defined_macros.add("EYE_W")
                    defined_macros.add("EYE_H")
            elif name.startswith("mouth"):
                if "MOUTH_W" not in defined_macros:
                    defines.append(f"#define MOUTH_W {w}")
                    defines.append(f"#define MOUTH_H {h}")
                    defined_macros.add("MOUTH_W")
                    defined_macros.add("MOUTH_H")
            elif name == "blush":
                if "BLUSH_W" not in defined_macros:
                    defines.append(f"#define BLUSH_W {w}")
                    defines.append(f"#define BLUSH_H {h}")
                    defined_macros.add("BLUSH_W")
                    defined_macros.add("BLUSH_H")
        else:
            print(f"Warning: {image_path} not found")
    
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("// Auto-generated LVGL image data\n")
        f.write("// Generated from PNG files in assets/\n\n")
        f.write("\n".join(defines) + "\n\n")
        f.write("\n\n".join(results))
    
    print(f"\nGenerated {output_file}")

if __name__ == "__main__":
    main()
