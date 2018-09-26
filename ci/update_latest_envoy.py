import subprocess,os

locations = os.path.join(os.path.dirname(os.path.realpath(__file__)),'..', 'bazel/repository_locations.bzl')

with open(locations,"rb") as loc:
    content = loc.read()
exec(content)

commit = subprocess.check_output("git ls-remote https://github.com/envoyproxy/envoy HEAD | cut -f1", shell=True)
commit = commit.strip()

currentcommit = REPOSITORY_LOCATIONS['envoy']['commit']

# encode makes this work with python3 too.
content = content.replace(currentcommit.encode("utf8"), commit)

with open(locations,"wb") as loc:
    loc.write(content)
