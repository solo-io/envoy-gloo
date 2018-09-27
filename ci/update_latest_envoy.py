import subprocess,os

locations = os.path.join(os.path.dirname(os.path.realpath(__file__)),'..', 'bazel/repository_locations.bzl')

with open(locations,"rb") as loc:
    content = loc.read()
exec(content)

ls_remote_args = "git ls-remote https://github.com/envoyproxy/envoy HEAD"
ls_remote_output = subprocess.check_output(ls_remote_args, shell=True)
commit_hash = ls_remote_output.split()[0]

current_commit_hash = REPOSITORY_LOCATIONS['envoy']['commit']

# encode makes this work with python3 too.
content = content.replace(current_commit_hash.encode("utf8"), commit_hash)

with open(locations,"wb") as loc:
    loc.write(content)
