#!/bin/bash
for i in {1..20}; do
  make -C ifa test-ir > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "Failed on run $i"
    exit 1
  fi
done
echo "Success"
