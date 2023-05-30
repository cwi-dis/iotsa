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

# If seems platformio does a chdir() to the base directory of the extra_script 
# before running the extra_script. So getcwd() is probably the right thing to do (somewhat surprisingly)
try:
    myDir = os.path.dirname(__file__)
except NameError:
    myDir = os.getcwd()
#print("platformio_pre_script: env: ", env.Dump(), file=sys.stderr)
# Create the version.h file
mkversionPath = os.path.join(myDir, "extras", "python", "mkversionh.py")
exec(open(mkversionPath).read())
# Get information on this specific program (target). Finding the project directory is difficult:
# when running with 'pio ci' the PROJECT_DIR points to the temporary project directory copy, which
# doesn't dontain the .git administration. Then, we revert to looking at the CWD or PWD environment variables.

projectDir = env["PROJECT_DIR"]
if not os.path.exists(os.path.join(projectDir, ".git")):
    attempt = None
    if 'PWD' in env["ENV"]: attempt = env["ENV"].get("PWD")
    if 'CWD' in env["ENV"]: attempt = env["ENV"].get("CWD")
    if attempt and os.path.exists(os.path.join(attempt, ".git")):
        print(f"platformio_pre_script: override projectDir for pio ci", file=sys.stderr)
        projectDir = attempt
print(f"platformio_pre_script: projectDir: {projectDir}", file=sys.stderr)
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
if not "BUILD_FLAGS" in env:
    env["BUILD_FLAGS"] = []
if programName:
    env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_NAME=\\"{programName}\\"')
if programRepo:
    env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_REPO=\\"{programRepo}\\"')
if programVersion:
    env["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_VERSION=\\"{programVersion}\\"')
# And change the firmware name
if programName:
    env.Replace(PROGNAME=programName)
