VERSION := $(shell readelf -n bazel-bin/envoy.stripped|grep "Build ID:" |cut -f2 -d:|tr -d ' ')
RELEASE := "false"
ifneq ($(TAGGED_VERSION),)
        VERSION := $(shell echo $(TAGGED_VERSION) | cut -c 2-)
        RELEASE := "true"
endif

.PHONY: docker-release
docker-release:
ifeq ($(RELEASE),"true")
	cd ci
	docker build -t soloio/envoy-gloo:$(VERSION) .
	docker push soloio/envoy-gloo:$(VERSION)
endif
