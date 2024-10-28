# Release Policy

Goal: Ensure that we can update gloo-envoy with upstream security features, including older Gloo releases, without running into deprecated features requiring Gloo updates.

We aim to have gloo-envoy release match corresponding gloo releases.
To make sure that we can backport security fixes without breaking changes, 
we will follow the following release paradigm:

envoy-gloo releases match gloo releases with the major/minor version number.
i.e. envoy-gloo 1.4 is meant to work with gloo 1.4

The current release may depend on envoy master. Once we release gloo, that release must not update to an envoy 
newer than the next envoy release. Using a newer envoy runs the risk of having deprecated features
removed, which will break behavior.

Example:
```
--- time --->
gloo releases:
1.2*********1.3*********1.4*********
envoy releases:
******1.13*********1.14*********1.15
```


envoy-gloo that is shipped with gloo 1.2 must not use envoy newer than 1.13.*
envoy-gloo that is shipped with gloo 1.3 must not use envoy newer than 1.14.*
envoy-gloo that is shipped with gloo 1.4 must not use envoy newer than 1.15.*
