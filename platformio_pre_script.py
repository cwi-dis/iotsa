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
# Create the version.h file
mkversionPath = os.path.join(myDir, "extras", "python", "mkversionh.py")
exec(open(mkversionPath).read())

def fixEnv(thisEnv, thisEnvName):
    # Get information on this specific program (target). Finding the project directory is difficult:
    # when running with 'pio ci' the PROJECT_DIR points to the temporary project directory copy, which
    # doesn't dontain the .git administration. Then, we revert to looking at the CWD or PWD environment variables.

    projectDir = thisEnv["PROJECT_DIR"]
    if not os.path.exists(os.path.join(projectDir, ".git")):
        attempt = None
        if 'PWD' in thisEnv["ENV"]: attempt = thisEnv["ENV"].get("PWD")
        if 'CWD' in thisEnv["ENV"]: attempt = thisEnv["ENV"].get("CWD")
        if attempt and os.path.exists(os.path.join(attempt, ".git")):
            print(f"platformio_pre_script: override projectDir for pio ci", file=sys.stderr)
            projectDir = attempt
    print(f"platformio_pre_script: projectDir: {projectDir}", file=sys.stderr)
    programName = thisEnv['PIOENV']
    # This happens with pio ci: the platformio.ini doesn't have the program name.
    # Get it from the github actions matrix environment variable.
    if "IOTSA_CONFIG_PROGRAM_NAME" in thisEnv["ENV"]:
        programName = thisEnv["ENV"].get("IOTSA_CONFIG_PROGRAM_NAME")
    programRepo = commandOutput("git config --get remote.origin.url", projectDir)
    if programRepo.endswith('.git'):
        programRepo = programRepo[:-4]
    programRepo = programRepo.replace('ssh://git@github.com/', 'https://github.com/')
    programVersion = commandOutput("git describe --always --match 'v*'", projectDir)
    print(f"platformio_pre_script: programName: {programName}", file=sys.stderr)
    print(f"platformio_pre_script: programRepo: {programRepo}", file=sys.stderr)
    print(f"platformio_pre_script: programVersion: {programVersion}", file=sys.stderr)
    # Add to the build_flags
    if not "BUILD_FLAGS" in thisEnv:
        thisEnv["BUILD_FLAGS"] = []
    if programName:
        thisEnv["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_NAME=\\"{programName}\\"')
    if programRepo:
        thisEnv["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_REPO=\\"{programRepo}\\"')
    if programVersion:
        thisEnv["BUILD_FLAGS"].append(f'-DIOTSA_CONFIG_PROGRAM_VERSION=\\"{programVersion}\\"')
    # And change the firmware name
    if programName:
        thisEnv.Replace(PROGNAME=programName)

    print("platformio_pre_script: env ", thisEnvName,": ", thisEnv.Dump(), file=sys.stderr)

fixEnv(env, "env")
defaultEnv = DefaultEnvironment()
if not defaultEnv is env:
    fixEnv(defaultEnv, "DefaultEnvironment")