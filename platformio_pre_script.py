import sys
import os
import subprocess
Import("env")
# Import("projenv")

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

def fixEnv(thisEnv):
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
    programRepo = programRepo.replace('git@github.com:', 'https://github.com/')
    # Same <version>+sha.<hash> shape as IOTSA_FULL_VERSION (see mkversionh.py):
    # tag-distance ("-N-gHASH") isn't used here either, both because it can't
    # work for a shallow lib_deps-style clone and because plain "git describe"
    # ignores lightweight tags (--tags is needed, and even then a repo with no
    # reachable "v*" tag at all has nothing to anchor to).
    programBaseVersion = commandOutput("git describe --tags --abbrev=0 --match 'v*'", projectDir)
    programSha = commandOutput("git rev-parse --short HEAD", projectDir)
    if programBaseVersion and programSha:
        programVersion = programBaseVersion + "+sha." + programSha
    elif programSha:
        print(f"platformio_pre_script: WARNING: no 'v*' tag found in {projectDir}, programVersion will have no base version", file=sys.stderr)
        programVersion = "unknown+sha." + programSha
    else:
        programVersion = "unknown"
    print(f"platformio_pre_script: programName: {programName}", file=sys.stderr)
    print(f"platformio_pre_script: programRepo: {programRepo}", file=sys.stderr)
    print(f"platformio_pre_script: programVersion: {programVersion}", file=sys.stderr)
    thisEnv.Append(CPPDEFINES=[
        ("IOTSA_CONFIG_PROGRAM_NAME", thisEnv.StringifyMacro(programName)),
        ("IOTSA_CONFIG_PROGRAM_REPO", thisEnv.StringifyMacro(programRepo)),
        ("IOTSA_CONFIG_PROGRAM_VERSION", thisEnv.StringifyMacro(programVersion)),
    ])
    # And change the firmware name
#    if programName:
#        thisEnv.Replace(PROGNAME=programName)

#for e in [env, projenv, DefaultEnvironment()]:
for e in [env, DefaultEnvironment()]:
    fixEnv(e)
# print("platformio_pre_script: env ", thisEnvName,": ", thisEnv.Dump(), file=sys.stderr)
