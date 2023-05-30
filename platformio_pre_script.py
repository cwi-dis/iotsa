import sys
import os
import subprocess
Import("env")

def commandOutput(command, baseDir):
    cmd = subprocess.Popen(
        command,
        shell=True,
        cwd=baseDir,
        stdout=subprocess.PIPE,
        universal_newlines=True,
    )
    rv = cmd.stdout.read().strip()
    return rv

#print("platformio_pre_script: env: ", env.Dump(), file=sys.stderr)
# Create the version.h file
projectDir = env["PROJECT_DIR"]
mkversionPath = os.path.join(projectDir, "extras", "python", "mkversionh.py")
exec(open(mkversionPath).read())
# Get information on this specific program (target)
programName = env['PIOENV']
programRepo = commandOutput("git config --get remote.origin.url", projectDir)
if programRepo.endswith('.git'):
    programRepo = programRepo[:-4]
programRepo = programRepo.replace('ssh://git@github.com/', 'https://github.com/')
programVersion = commandOutput("git describe --always --match 'v*'", projectDir)
print(f"platformio_pre_script: programName: {programName}", file=sys.stderr)
print(f"platformio_pre_script: programRepo: {programRepo}", file=sys.stderr)
print(f"platformio_pre_script: programVersion: {programVersion}", file=sys.stderr)
# Add to the build_flags
env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_NAME=\\"{programName}\\"')
env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_REPO=\\"{programRepo}\\"')
env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_VERSION=\\"{programVersion}\\"')
