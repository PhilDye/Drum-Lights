# PlatformIO pre-build script: sets FIRMWARE_VERSION from git, so the TX can
# report which build it is running (the "fw" field in its state messages)
import subprocess

Import("env")


def git_version():
    try:
        return (
            subprocess.check_output(
                ["git", "describe", "--tags", "--always", "--dirty"],
                cwd=env["PROJECT_DIR"],
                stderr=subprocess.DEVNULL,
            )
            .decode()
            .strip()
        )
    except Exception:
        return "unknown"


env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(git_version()))])
