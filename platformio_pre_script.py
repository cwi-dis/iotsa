import sys
import os
Import("env")

print("platformio_pre_script: env: ", env.Dump(), file=sys.stderr)
projectDir = env["PROJECT_DIR"]
mkversionPath = os.path.join(projectDir, "extras", "python", "mkversionh.py")
exec(open(mkversionPath).read())
