import subprocess

Import("env")

def after_build(source, target, env):
    # Your custom script or commands to run after the build
    print("Running custom script after build")
    print(source)
    print(target) 
    print(env)
    code = subprocess.call(['scp', '.pio/build/esp32-s3-devkitm-1/firmware.bin', 'zzzorgo@home-r:/usr/share/nginx/html/esp32/firmware.bin'])
    print(code)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", after_build)
