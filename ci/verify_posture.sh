#!/bin/bash


ALL_EXTENSIONS=$(bazel query "filter(//source, deps(@envoy_gloo//:envoy_gloo_all_filters_lib,1))" --output label 2> /dev/null|grep -v '@envoy//' | cut -d: -f1|sort)

# check if arg $1 is verify:

function do_verify () {
for EXTENSION in $ALL_EXTENSIONS; do
 EXTENSION=${EXTENSION##*source/extensions/}
 if grep -q $EXTENSION $MD_FILE; then
    echo "Found $EXTENSION in $MD_FILE"
  else
    echo "Did not find $EXTENSION in $MD_FILE, please add it."
    echo "You can use \"$0 template\" command to get you started."
    exit 1
  fi
done
}

MD_FILE=$(bazel info workspace)/security_posture.yaml

function do_template () {
for EXTENSION in $ALL_EXTENSIONS; do
  EXTENSION=${EXTENSION##*source/extensions/}
  if grep -q $EXTENSION $MD_FILE; then
    echo "Found $EXTENSION in $MD_FILE skipping"
  else
    echo "Did not find $EXTENSION in $MD_FILE, adding"
    echo "- name: $EXTENSION" >> $MD_FILE
    echo "  security_posture: unknown" >> $MD_FILE
  fi
done
}

case $1 in

  verify)
    do_verify
    ;;

  template)
    do_template
    ;;

  *)
    echo "unknown command"
    exit 1
    ;;
esac
