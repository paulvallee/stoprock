# PlatformIO post-build step for the `emu` environment: merge the bootloader, partition
# table and app into the single flash image esp-emulator runs
# (.pio/build/emu/stoprock-emu.bin).
import os

Import("env")  # noqa: F821  (provided by PlatformIO)


def merge(source, target, env):
    build = env.subst("$BUILD_DIR")
    out = os.path.join(build, "stoprock-emu.bin")
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy"), "esptool.py")
    images = []
    for offset, path in env.get("FLASH_EXTRA_IMAGES", []):
        images += [offset, env.subst(path)]
    images += [env.subst("$ESP32_APP_OFFSET"), os.path.join(build, "firmware.bin")]
    cmd = ["$PYTHONEXE", esptool, "--chip", env.BoardConfig().get("build.mcu"), "merge_bin",
           "-o", out, "--flash_mode", "dio",
           "--flash_size", env.BoardConfig().get("upload.flash_size", "8MB")] + images
    env.Execute(" ".join(f'"{c}"' for c in cmd))
    print("merged image:", out)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge)  # noqa: F821
