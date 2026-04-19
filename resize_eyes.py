#!/usr/bin/env python3
import os
from PIL import Image

def resize_images():
    assets_dir = "assets"
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
    
    new_size = (60, 60)
    
    for name in eye_files:
        image_path = os.path.join(assets_dir, f"{name}.png")
        if os.path.exists(image_path):
            print(f"Resizing: {name}.png")
            img = Image.open(image_path).convert('RGBA')
            resized_img = img.resize(new_size, Image.LANCZOS)
            resized_img.save(image_path)
            print(f"  Resized to: {resized_img.width}x{resized_img.height}")
        else:
            print(f"Warning: {image_path} not found")

if __name__ == "__main__":
    resize_images()